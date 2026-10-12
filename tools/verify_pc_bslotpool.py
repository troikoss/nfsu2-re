#!/usr/bin/env python3
"""Compare the recovered bWare slot pool with original x86 over random operation sequences."""
import hashlib
import json
import random
import re
import struct
import subprocess
import pefile
from unicorn import Uc, UC_ARCH_X86, UC_MODE_32, UC_HOOK_BLOCK
from unicorn.x86_const import *
from pc_coff import link_objects, read_object
from probe_pc_race_reward import ROOT, EXE

BUILD = ROOT / 'build/analysis/bslotpool'
HEAP, HEAP_SIZE, STACK, STOP = 0x2000000, 0x80000, 0x2100000 - 0x100, 0x2100000
BMALLOC, BFREE = 0x440390, 0x440540
SAVED = (UC_X86_REG_EBX, UC_X86_REG_ESI, UC_X86_REG_EDI, UC_X86_REG_EBP)
THISCALL = {'InitialiseFreeList', 'Expand', 'FindUsedSlot', 'Allocate', 'Delete'}
CLEANUP = dict(InitialiseFreeList=0, Expand=4, FindUsedSlot=4, Allocate=0, Delete=4)


def main():
    manifest = json.loads((ROOT / 'config/SPEED2_EXE_1_2/bslotpool.json').read_text())
    raw = EXE.read_bytes()
    assert hashlib.sha256(raw).hexdigest() == manifest['exe_sha256']
    image = pefile.PE(data=raw).get_memory_mapped_image()
    for f in manifest['functions']:
        assert hashlib.sha256(image[f['address'] - 0x400000:f['address'] - 0x400000 + f['size']]).hexdigest() == f['sha256']
    BUILD.mkdir(parents=True, exist_ok=True)
    obj = BUILD / 'bslotpool.obj'
    subprocess.run([str(ROOT / 'build/tools/wibo'), 'build/compilers/Win32/7.1/cl.exe', '/nologo', '/c', '/Ox', '/Ob0',
                    '/Z7', '/EHsc', '/Fo' + str(obj.relative_to(ROOT)), 'src/Speed/Indep/Libs/bWare/bSlotPool.cpp'],
                   cwd=ROOT, check=True)
    globals_ = {name: int(address, 16) for name, address in re.findall(r'^(\w+) = (0x[0-9a-f]+);',
                (ROOT / 'src/Platform/Native/OriginalSymbols.ld').read_text(), re.M)}
    externals = {'?bMalloc@@YAPAXHPBDHH@Z': BMALLOC, '?bFreeMemory@@YAXPAX@Z': BFREE}
    for symbol in read_object(obj)['symbols'].values():
        match = re.match(r'^\?(\w+)@@', symbol['name'])
        if symbol['section'] == 0 and match and match[1] in globals_:
            externals[symbol['name']] = globals_[match[1]]
    linked = link_objects([obj], externals)
    entries = {}
    for f in manifest['functions']:
        found = [a for s, a in linked['exports'].items() if s.startswith('?' + f['method'] + '@')]
        assert len(found) == 1, (f['method'], found)
        entries[f['method']] = (f['address'], found[0])
    machines, cursors, traces = [], [0, 0], [[], []]
    for side in (0, 1):
        uc = Uc(UC_ARCH_X86, UC_MODE_32)
        uc.mem_map(0x400000, (len(image) + 4095) & ~4095)
        uc.mem_write(0x400000, image)
        uc.mem_map(HEAP, 0x100000)
        uc.mem_map(linked['address'], linked['allocation'])
        for address, body in linked['blocks']:
            uc.mem_write(address, body)
        allowed = linked['ranges'] if side else [(f['address'], f['address'] + f['size']) for f in manifest['functions']]

        # bWare heap (platform seam): deterministic bump allocation filled with
        # 0xa5 so reads of uninitialized slots are visible; frees are traced.
        def hook(machine, address, size, user, side=side, allowed=allowed):
            if address in (BMALLOC, BFREE):
                sp = machine.reg_read(UC_X86_REG_ESP)
                if address == BMALLOC:
                    length, a, b, flags = struct.unpack('<iIii', machine.mem_read(sp + 4, 16))
                    pointer = cursors[side]
                    cursors[side] += (max(length, 1) + 15) & ~15
                    assert cursors[side] < HEAP + HEAP_SIZE
                    machine.mem_write(pointer, b'\xa5' * max(length, 1))
                    traces[side].append(('malloc', length, a, b, flags, pointer))
                    machine.reg_write(UC_X86_REG_EAX, pointer)
                else:
                    traces[side].append(('free', struct.unpack('<I', machine.mem_read(sp + 4, 4))[0]))
                machine.reg_write(UC_X86_REG_ECX, 0xeeeeeeee)
                machine.reg_write(UC_X86_REG_EDX, 0xffffffff)
                machine.reg_write(UC_X86_REG_EIP, struct.unpack('<I', machine.mem_read(sp, 4))[0])
                machine.reg_write(UC_X86_REG_ESP, sp + 4)
                return
            assert any(a <= address and address + size <= b for a, b in allowed), hex(address)
        uc.hook_add(UC_HOOK_BLOCK, hook)
        uc.mem_map(STOP, 0x1000)
        machines.append(uc)
    counts = dict.fromkeys(entries, 0)

    def call(method, args, receiver=0):
        results = []
        for side, uc in enumerate(machines):
            if method in THISCALL:
                uc.mem_write(STACK, struct.pack('<' + 'I' * (1 + len(args)), STOP, *[a & 0xffffffff for a in args]))
                expected = STACK + 4 + CLEANUP[method]
            else:
                uc.mem_write(STACK, struct.pack('<' + 'I' * (1 + len(args)), STOP, *[a & 0xffffffff for a in args]))
                expected = STACK + 4
            uc.reg_write(UC_X86_REG_ESP, STACK)
            uc.reg_write(UC_X86_REG_ECX, receiver)
            for reg in SAVED:
                uc.reg_write(reg, 0x12345678)
            uc.emu_start(entries[method][side], STOP, count=2000000)
            assert uc.reg_read(UC_X86_REG_EIP) == STOP and uc.reg_read(UC_X86_REG_ESP) == expected, method
            assert all(uc.reg_read(reg) == 0x12345678 for reg in SAVED)
            results.append((uc.reg_read(UC_X86_REG_EAX), bytes(uc.mem_read(HEAP, cursors[side] - HEAP)),
                            bytes(uc.mem_read(0x828530, 12)), bytes(uc.mem_read(0x8284f0, 8))))
        assert traces[0] == traces[1], (method, traces)
        assert cursors[0] == cursors[1]
        mask = 0xff if method == 'bIsSlotPoolFull' else 0xffffffff
        assert results[0][0] & mask == results[1][0] & mask, (method, counts[method])
        assert results[0][1:] == results[1][1:], (method, counts[method])
        counts[method] += 1
        return results[0][0] & mask

    rng = random.Random(0x440d40)
    for trial in range(300):
        for side, uc in enumerate(machines):
            uc.mem_write(HEAP, b'\0' * 0x80000)
            cursors[side] = HEAP + 0x1000
            traces[side] = []
            enabled = trial % 2
            uc.mem_write(0x828530, struct.pack('<III', enabled, 0x828534, 0x828534))
            uc.mem_write(0x8284f0, struct.pack('<Ii', 0, 0))
        pools, live, deletable = [], {}, []
        enabled = trial % 2
        for step in range(60):
            op = rng.randrange(10)
            if op == 0 or not pools:
                creator = rng.choice(('bNewSlotPool', 'bNewSlotPoolBlock'))
                # Slots hold a free-list pointer: blocks need sizes of at least four bytes
                # in multiples of four; bNewSlotPool rounds its size up itself.
                sizes = (1, 4, 6, 0xc, 0x40, 0x70) if creator == 'bNewSlotPool' else (4, 8, 0xc, 0x40, 0x70)
                size, count = rng.choice(sizes), rng.choice((0, 1, 2, 3, 8, 16))
                pool = call(creator, (size, count, HEAP + 0x10, rng.choice((0, 0x40))))
                if pool:
                    pools.append(pool)
                    live[pool] = []
                    if creator == 'bNewSlotPool' or not enabled:
                        deletable.append(pool)
                    if rng.randrange(3) == 0:
                        flags = rng.choice((0, 1, 2, 3, 8, 0xb))
                        for uc in machines:
                            uc.mem_write(pool + 0x14, struct.pack('<i', flags))
            elif op in (1, 2, 3):
                pool = rng.choice(pools)
                method = rng.choice(('bOMalloc', 'bSlotPoolAllocate', 'Allocate'))
                slot = call(method, () if method == 'Allocate' else (pool,), pool)
                if slot:
                    live[pool].append(slot)
            elif op == 4:
                pool = rng.choice(pools)
                if live[pool]:
                    slot = live[pool].pop(rng.randrange(len(live[pool])))
                    call('bFree', (pool, slot))
            elif op == 5:
                call('bIsSlotPoolFull', (rng.choice(pools),))
            elif op == 6:
                pool = rng.choice(pools)
                call('Expand', (rng.choice((1, 2, 5)),), pool)
            elif op == 7:
                pool = rng.choice(pools)
                call('FindUsedSlot', (rng.choice((0, 1, 3)),), pool)
            elif op == 8:
                node = call('bNewListNode', ())
                if node and rng.randrange(2):
                    if rng.randrange(4) == 0:
                        for uc in machines:
                            uc.mem_write(0x8284f4, struct.pack('<i', 1))
                    call('bDeleteListNode', (node,))
            elif op == 9 and trial % 3 == 0 and deletable:
                # Registered pools only: unregistered blocks have no list links.
                pool = deletable.pop(rng.randrange(len(deletable)))
                pools.remove(pool)
                call('Delete', (pool,), 0x828530)
    report = dict(executable_sha256=manifest['exe_sha256'], bit_identical_cases=counts, byte_matching='not established',
                  controlled_interfaces=['bWare heap 0x440390/0x440540: hooked deterministic allocation (0xa5 fill) and traced frees'],
                  limitations=['Random sequences of 60 operations over 300 trials; well-formed pointers and block slot sizes (multiples of four) only.'])
    (BUILD / 'verification.json').write_text(json.dumps(report, indent=2) + '\n')
    print(json.dumps(report, indent=2))


if __name__ == '__main__':
    main()
