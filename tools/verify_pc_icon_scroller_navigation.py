#!/usr/bin/env python3
"""Compare original/rebuilt scroller list navigation and selected-icon dispatch."""
import argparse
import hashlib
import json
import os
import random
import struct
import subprocess

import pefile
from unicorn import Uc, UC_ARCH_X86, UC_MODE_32, UC_HOOK_BLOCK
from unicorn.x86_const import (UC_X86_REG_EAX, UC_X86_REG_ECX, UC_X86_REG_ESP, UC_X86_REG_EIP,
                              UC_X86_REG_EBX, UC_X86_REG_ESI, UC_X86_REG_EDI, UC_X86_REG_EBP)
from probe_pc_race_reward import ROOT, EXE
from pc_coff import link_objects

BUILD = ROOT / 'build/analysis/icon-scroller-navigation'
FUNCTIONS = {'AtFirst': (0x5276d0, 42), 'AtLast': (0x527700, 42),
             'Next': (0x527730, 21), 'Previous': (0x527750, 21),
             'StepNoWrap': (0x51f9f0, 194), 'StepWrap': (0x51fac0, 167),
             'IndexOf': (0x51f820, 71), 'Dispatch': (0x50d980, 50)}
ARENA, STACK, STOP = 0x2000000, 0x200f000, 0x2100000
SELECT, REACT = STOP + 0x100, STOP + 0x110
REGISTERS = (UC_X86_REG_EBX, UC_X86_REG_ESI, UC_X86_REG_EDI, UC_X86_REG_EBP)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source', default='src/Speed/Indep/Src/Frontend/IconScrollerNavigation.cpp')
    args = parser.parse_args()
    raw = EXE.read_bytes()
    assert hashlib.sha256(raw).hexdigest() == 'f9dd86c054878ce6276beb07c1fd61874f7a1e4bf1f241b084c65b73e24168a7'
    image = pefile.PE(data=raw).get_memory_mapped_image()
    BUILD.mkdir(parents=True, exist_ok=True)
    obj = BUILD / 'IconScrollerNavigation.obj'
    subprocess.run([str(ROOT / 'build/tools/wibo'), 'build/compilers/Win32/7.1/cl.exe', '/nologo', '/c',
                    '/Ox', '/Ob0', '/Z7', '/EHsc', '/Fo' + str(obj.relative_to(ROOT)),
                    '/Isrc/Speed/Indep/Src/Frontend', os.path.relpath(args.source, ROOT)], cwd=ROOT, check=True)
    linked = link_objects([obj], {})
    entries = {name: next(a for n, a in linked['exports'].items() if n.startswith('?' + name + '@'))
               for name in FUNCTIONS}
    state, machines = {}, []
    for rebuilt in (False, True):
        uc = Uc(UC_ARCH_X86, UC_MODE_32)
        uc.mem_map(0x400000, (len(image) + 4095) & ~4095)
        uc.mem_write(0x400000, image)
        uc.mem_map(ARENA, 0x10000)
        uc.mem_map(STOP, 0x1000)
        uc.mem_map(linked['address'], linked['allocation'])
        for address, body in linked['blocks']:
            uc.mem_write(address, body)
        ranges = linked['ranges'] if rebuilt else [(a, a + n) for a, n in FUNCTIONS.values()]

        def block(uc, address, size, user, ranges=ranges):
            if address == STOP:
                return
            if address in (SELECT, REACT):
                argc = 1 if address == SELECT else 5
                sp, this = uc.reg_read(UC_X86_REG_ESP), uc.reg_read(UC_X86_REG_ECX)
                words = struct.unpack('<' + 'I' * (argc + 1), uc.mem_read(sp, (argc + 1) * 4))
                state['trace'].append((address, this, words[1:]))
                if address == SELECT:
                    state['trace'].append(('before_selection', bytes(uc.mem_read(this + 0x2c, 12)).hex()))
                    if state['update_selected']:
                        uc.mem_write(this + 0xc, struct.pack('<I', words[1]))
                uc.reg_write(UC_X86_REG_ESP, sp + 4 + argc * 4)
                uc.reg_write(UC_X86_REG_EIP, words[0])
                return
            assert any(a <= address < b for a, b in ranges), ('unexpected execution', hex(address))
        uc.hook_add(UC_HOOK_BLOCK, block)
        machines.append(uc)

    counts = dict.fromkeys(FUNCTIONS, 0)
    rng = random.Random(0x51f9f0)
    scroller = ARENA + 0x100
    icons = [ARENA + 0x2000 + i * 0x100 for i in range(8)]

    def fixture(count, disabled, selected, allow=0, vertical=0):
        memory = bytearray(rng.randbytes(0x10000))
        sentinel = scroller + 4
        struct.pack_into('<IIII', memory, 0x100, ARENA + 0x1000,
                         icons[0] + 4 if count else sentinel,
                         icons[count - 1] + 4 if count else sentinel,
                         icons[selected] if selected is not None else 0)
        struct.pack_into('<I', memory, 0x1014, SELECT)
        struct.pack_into('<I', memory, 0x1104, REACT)
        memory[0x12c], memory[0x130] = vertical, allow
        for i in range(count):
            offset = icons[i] - ARENA
            struct.pack_into('<III', memory, offset, ARENA + 0x1100,
                             icons[i + 1] + 4 if i + 1 < count else sentinel,
                             icons[i - 1] + 4 if i else sentinel)
            memory[offset + 0x20] = 0x80 if disabled & (1 << i) else 0
        return memory

    def compare(name, memory, args=(), cleanup=0, mask=0, update_selected=True):
        outputs = []
        for rebuilt, uc in zip((False, True), machines):
            uc.mem_write(ARENA, bytes(memory))
            uc.mem_write(STACK, struct.pack('<I', STOP) + struct.pack('<' + 'I' * len(args), *args))
            uc.reg_write(UC_X86_REG_ESP, STACK)
            uc.reg_write(UC_X86_REG_ECX, scroller)
            for reg in REGISTERS:
                uc.reg_write(reg, 0x12345678)
            state.update(trace=[], update_selected=update_selected)
            uc.emu_start(entries[name] if rebuilt else FUNCTIONS[name][0], STOP, count=10000)
            assert uc.reg_read(UC_X86_REG_EIP) == STOP, (name, args)
            assert uc.reg_read(UC_X86_REG_ESP) == STACK + 4 + cleanup
            assert all(uc.reg_read(r) == 0x12345678 for r in REGISTERS)
            outputs.append((uc.reg_read(UC_X86_REG_EAX) & mask, state['trace'],
                            bytes(uc.mem_read(ARENA, 0xe000))))
        assert outputs[0] == outputs[1], (name, args, outputs[0][:2], outputs[1][:2])
        counts[name] += 1

    for count in range(1, 7):
        for disabled in range(1 << count):
            for selected in range(count):
                for allow in (0, 1, 0x80):
                    for direction in (-1, 1, 0, -10, 10):
                        memory = fixture(count, disabled, selected, allow)
                        update = rng.choice((False, True))
                        compare('StepNoWrap', memory, [direction & 0xffffffff], 4, update_selected=update)
                        # Preserve the original all-disabled nontermination; skip this invalid call state.
                        if allow or disabled != (1 << count) - 1 or direction not in (-1, 1):
                            compare('StepWrap', memory, [direction & 0xffffffff], 4, update_selected=update)
                    memory = fixture(count, disabled, selected, allow, vertical=rng.choice((0, 1, 0xff)))
                    if allow or disabled != (1 << count) - 1 or not memory[0x12c]:
                        compare('Next', memory)
                        compare('Previous', memory)
                    compare('AtFirst', memory, mask=0xff)
                    compare('AtLast', memory, mask=0xff)
                    for target in (0, icons[selected], ARENA + 0x4000):
                        compare('IndexOf', memory, [target], 4, 0xffffffff)
    for trial in range(1000):
        memory = fixture(0, 0, None)
        compare('IndexOf', memory, [rng.choice((0, icons[0]))], 4, 0xffffffff)
        memory = fixture(1, 0, 0)
        sender = rng.getrandbits(32)
        struct.pack_into('<I', memory, 0x2010, sender)
        if trial % 3 == 0:
            struct.pack_into('<I', memory, 0x10c, 0)
        compare('Dispatch', memory, [rng.getrandbits(32), sender if trial % 2 else sender ^ 1,
                                     rng.getrandbits(32), rng.getrandbits(32)], 16)
    report = {'comparisons': counts, 'total': sum(counts.values()), 'byte_matching': 'not established',
              'controlled_interfaces': ['slot +0x14 selection callback', 'icon slot +4 React'],
              'fingerprints': {name: {'address': hex(a), 'size': n,
                                     'sha256': hashlib.sha256(image[a-0x400000:a-0x400000+n]).hexdigest()}
                               for name, (a, n) in FUNCTIONS.items()}}
    (BUILD / 'verification.json').write_text(json.dumps(report, indent=2) + '\n')
    print(json.dumps(report, indent=2))


if __name__ == '__main__':
    main()
