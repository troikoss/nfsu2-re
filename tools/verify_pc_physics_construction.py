#!/usr/bin/env python3
"""Compare original physics construction and upgrade selection with real callees."""
import hashlib
import json
import random
import struct
import subprocess
import pefile
from unicorn import Uc, UC_ARCH_X86, UC_MODE_32, UC_HOOK_CODE
from unicorn.x86_const import *
from pc_coff import link_objects, read_object
from probe_pc_race_reward import ROOT, EXE

BUILD = ROOT / 'build/analysis/physics-construction'
BASE, STACK, STOP = 0x2000000, 0x200f000, 0x2100000
GROUPS = BASE + 0x2000


def main():
    manifest = json.loads((ROOT / 'config/SPEED2_EXE_1_2/physics-construction.json').read_text())
    raw = EXE.read_bytes()
    assert hashlib.sha256(raw).hexdigest() == manifest['exe_sha256']
    image = pefile.PE(data=raw).get_memory_mapped_image()
    for f in manifest['functions']:
        assert hashlib.sha256(image[f['address'] - 0x400000:f['address'] - 0x400000 + f['size']]).hexdigest() == f['sha256']
    BUILD.mkdir(parents=True, exist_ok=True)
    source = BUILD / 'construction-analysis.cpp'
    source.write_text(''.join('#include "' + str(ROOT / 'src/Speed/Indep/Src/Physics' / (unit + '.cpp')) + '"\n'
                              for unit in ('PhysicsUpgrades', 'PhysicsParameters')))
    obj = BUILD / 'construction-analysis.obj'
    subprocess.run([str(ROOT / 'build/tools/wibo'), 'build/compilers/Win32/7.1/cl.exe', '/nologo',
                    '/c', '/Ox', '/Ob0', '/Z7', '/EHsc', '/Fo' + str(obj.relative_to(ROOT)),
                    str(source.relative_to(ROOT))], cwd=ROOT, check=True)
    externals = {'?physicsUpgradeGroupCount8383FC@@3HA': 0x8383fc,
                 '?physicsUpgradeGroups8383F8@@3PAUPhysicsUpgradeGroup@@A': 0x8383f8,
                 '?physicsUpgradeItems837B90@@3PAPAUPhysicsUpgradeItem@@A': 0x837b90,
                 '__fltused': BASE + 0x8000}
    linked = link_objects([obj], externals)
    entries = {}
    for f in manifest['functions']:
        matches = [a for s, a in linked['exports'].items() if s.startswith(f['symbol_prefix'])]
        assert len(matches) == 1, (f, matches)
        entries[f['address']] = (f['address'], matches[0])
    machines = []
    for side in (0, 1):
        uc = Uc(UC_ARCH_X86, UC_MODE_32)
        uc.mem_map(0x400000, (len(image) + 4095) & ~4095)
        uc.mem_write(0x400000, image)
        uc.mem_map(BASE, 0x10000)
        uc.mem_map(STOP, 0x1000)
        uc.mem_map(linked['address'], linked['allocation'])
        for address, body in linked['blocks']:
            uc.mem_write(address, body)
        ranges = linked['ranges'] if side else [(f['address'], f['address'] + f['size']) for f in manifest['functions']]
        def guard(machine, address, size, user, ranges=ranges):
            assert any(a <= address < b for a, b in ranges), hex(address)
        uc.hook_add(UC_HOOK_CODE, guard)
        machines.append(uc)
    rng = random.Random(0x5ac6e0)
    counts = {hex(a): 0 for a in entries}
    # Four descriptors, each containing four items; references form an acyclic
    # prerequisite graph. Exclusions exercise disabling, not invented recursion.
    addresses = [GROUPS + group * 0x17c + 0xc + item * 0x5c for group in range(4) for item in range(4)]

    def load(state, group_count):
        for uc in machines:
            uc.mem_write(BASE, bytes(state))
            uc.mem_write(0x8383f8, struct.pack('<Ii', GROUPS, group_count))
            uc.mem_write(0x837b90, b'\0' * (68 * 4))
            for i, address in enumerate(addresses):
                uc.mem_write(0x837b90 + i * 4, struct.pack('<I', address))
            uc.reg_write(UC_X86_REG_FPCW, 0x027f)

    def compare(address, receiver, args, cleanup):
        results = []
        for side, uc in enumerate(machines):
            uc.mem_write(STACK, struct.pack('<I', STOP) + b''.join(struct.pack('<I', a & 0xffffffff) for a in args))
            uc.reg_write(UC_X86_REG_ESP, STACK)
            uc.reg_write(UC_X86_REG_ECX, receiver)
            for reg in (UC_X86_REG_EBX, UC_X86_REG_ESI, UC_X86_REG_EDI, UC_X86_REG_EBP):
                uc.reg_write(reg, 0x12345678)
            uc.emu_start(entries[address][side], STOP, count=100000)
            assert uc.reg_read(UC_X86_REG_EIP) == STOP
            assert uc.reg_read(UC_X86_REG_ESP) == STACK + 4 + cleanup
            assert all(uc.reg_read(reg) == 0x12345678 for reg in (UC_X86_REG_EBX, UC_X86_REG_ESI, UC_X86_REG_EDI, UC_X86_REG_EBP))
            results.append((bytes(uc.mem_read(BASE, 0x4000)), bytes(uc.mem_read(0x837b90, 68 * 4)),
                            bytes(uc.mem_read(0x8383f8, 8)), uc.reg_read(UC_X86_REG_EAX)))
        assert results[0] == results[1], (hex(address), args, counts)
        counts[hex(address)] += 1

    for trial in range(128):
        state = bytearray(rng.randbytes(0x10000))
        for group in range(4):
            struct.pack_into('<i', state, GROUPS - BASE + group * 0x17c, group)
            struct.pack_into('<I', state, GROUPS - BASE + group * 0x17c + 8, trial % 5)
        for i, address in enumerate(addresses):
            struct.pack_into('<i', state, address - BASE, i)
            for j in range(5):
                struct.pack_into('<i', state, address - BASE + 0x40 + j * 4, rng.choice((-1,) + tuple(range(16))))
            for j in range(2):
                struct.pack_into('<i', state, address - BASE + 0x54 + j * 4, rng.choice((-1,) + tuple(range(i))))
        load(state, trial % 5)
        compare(0x5a7970, BASE, (), 0)
        load(state, trial % 5)
        compare(0x5ac6e0, BASE, (), 0)
        # Stateful selection sequences retain the constructed selection. Bitwise
        # comparison includes padding, untouched fields and descriptor buffers.
        for step in range(24):
            selected, recursive = rng.choice((0, 1, -1, 256)), rng.choice((0, 1, -1, 256))
            index = rng.randrange(16)
            compare(0x5a6320, BASE + 0x2e0, (addresses[index], selected, recursive), 12)
            compare(0x599680, BASE + 0x2e0, (rng.randrange(20), selected, recursive), 12)
            compare(0x5a6400, BASE + 0x2e0, (GROUPS + rng.randrange(4) * 0x17c, selected), 8)
            compare(0x5a6450, BASE + 0x2e0, (rng.choice((-1, 0, 1, 2, 3, 9)),), 4)
        compare(0x500070, 0, (), 0)
        compare(0x500080, 0, (trial % 4,), 0)
        compare(0x500150, 0, (trial % 20,), 0)
    report = dict(executable_sha256=manifest['exe_sha256'], bit_identical_cases=counts,
                  byte_matching='not established', controlled_interfaces=[],
                  limitations=['Finite synthetic descriptor graphs; cycles and malformed indices/counts excluded.',
                               'All covered original callees execute recovered source on the rebuilt side.',
                               'Chunk loading and full record/tuning composition are not covered.'])
    (BUILD / 'verification.json').write_text(json.dumps(report, indent=2) + '\n')
    print(json.dumps(report, indent=2))


if __name__ == '__main__':
    main()
