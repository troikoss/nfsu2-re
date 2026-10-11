#!/usr/bin/env python3
"""Compare original upgrade ranking and nearest-fraction search with recovered callees."""
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
from verify_pc_engine import CONVERSION

BUILD = ROOT / 'build/analysis/physics-upgrade-search'
BASE, STACK, STOP = 0x2000000, 0x200f000, 0x2100000
GROUPS = BASE + 0x2000


def main():
    manifest = json.loads((ROOT / 'config/SPEED2_EXE_1_2/physics-upgrade-search.json').read_text())
    dependency = json.loads((ROOT / 'config/SPEED2_EXE_1_2/physics-construction.json').read_text())
    functions = manifest['functions'] + [f for f in dependency['functions'] if f['address'] not in (0x5ac6e0, 0x5a7970)]
    raw = EXE.read_bytes()
    assert hashlib.sha256(raw).hexdigest() == manifest['exe_sha256']
    image = pefile.PE(data=raw).get_memory_mapped_image()
    for f in functions:
        assert hashlib.sha256(image[f['address'] - 0x400000:f['address'] - 0x400000 + f['size']]).hexdigest() == f['sha256']
    assert hashlib.sha256(image[CONVERSION[0] - 0x400000:CONVERSION[0] - 0x400000 + CONVERSION[1]]).hexdigest() == CONVERSION[2]
    BUILD.mkdir(parents=True, exist_ok=True)
    source = BUILD / 'construction-analysis.cpp'
    source.write_text(''.join('#include "' + str(ROOT / 'src/Speed/Indep/Src/Physics' / (unit + '.cpp')) + '"\n'
                              for unit in ('PhysicsUpgrades', 'PhysicsUpgradeSearch')))
    obj = BUILD / 'construction-analysis.obj'
    subprocess.run([str(ROOT / 'build/tools/wibo'), 'build/compilers/Win32/7.1/cl.exe', '/nologo',
                    '/c', '/Ox', '/Ob0', '/Z7', '/EHsc', '/Fo' + str(obj.relative_to(ROOT)),
                    str(source.relative_to(ROOT))], cwd=ROOT, check=True)
    externals = {'?physicsUpgradeGroupCount8383FC@@3HA': 0x8383fc,
                 '?physicsUpgradeGroups8383F8@@3PAUPhysicsUpgradeGroup@@A': 0x8383f8,
                 '?physicsUpgradeItems837B90@@3PAPAUPhysicsUpgradeItem@@A': 0x837b90,
                 '?physicsUpgradeGradeScale@@3MA': 0x787adc, '?physicsUpgradeSearchBias@@3NA': 0x7864e0,
                 '?physicsUpgradeSearchEpsilon@@3MA': 0x7844f4, '__ftol2': CONVERSION[0], '__fltused': BASE + 0x9000}
    linked = link_objects([obj], externals)
    entries = {}
    for f in manifest['functions']:
        matches = [a for s, a in linked['exports'].items() if s.startswith(f['symbol_prefix'])]
        assert len(matches) == 1, (f, matches)
        entries[f['address']] = (f['address'], matches[0])
    machines = []
    visits = dict.fromkeys((0x5a6291, 0x5a62ae, 0x5a6a1f, 0x5a6ace), 0)
    for side in (0, 1):
        uc = Uc(UC_ARCH_X86, UC_MODE_32)
        uc.mem_map(0x400000, (len(image) + 4095) & ~4095)
        uc.mem_write(0x400000, image)
        uc.mem_map(BASE, 0x10000)
        uc.mem_map(STOP, 0x1000)
        uc.mem_map(linked['address'], linked['allocation'])
        for address, body in linked['blocks']:
            uc.mem_write(address, body)
        ranges = linked['ranges'] if side else [(f['address'], f['address'] + f['size']) for f in functions]
        ranges = ranges + [(CONVERSION[0], CONVERSION[0] + CONVERSION[1])]
        def guard(machine, address, size, user, ranges=ranges, side=side):
            if not side and address in visits:
                visits[address] += 1
            if address == STOP:
                return
            assert any(a <= address < b for a, b in ranges), hex(address)
        uc.hook_add(UC_HOOK_CODE, guard)
        machines.append(uc)
    rng = random.Random(0x5ac6e0)
    counts = {hex(a): 0 for a in entries}
    # Thirty complete (kind, level) groups with two independent items each.
    # Candidate activation cannot fail due to unmet prerequisites in these fixtures.
    addresses = [GROUPS + group * 0x17c + 0xc + item * 0x5c for group in range(30) for item in range(2)]

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
            uc.emu_start(entries[address][side], STOP, count=1000000)
            assert uc.reg_read(UC_X86_REG_EIP) == STOP, (side, hex(address), args, counts, hex(uc.reg_read(UC_X86_REG_EIP)))
            assert uc.reg_read(UC_X86_REG_ESP) == STACK + 4 + cleanup
            assert all(uc.reg_read(reg) == 0x12345678 for reg in (UC_X86_REG_EBX, UC_X86_REG_ESI, UC_X86_REG_EDI, UC_X86_REG_EBP))
            if address in (0x591130, 0x5a6880):
                uc.mem_write(STOP, b'\xdd\x1d' + struct.pack('<I', BASE + 0x1800))
                uc.emu_start(STOP, STOP + 6, count=1)
            results.append((bytes(uc.mem_read(BASE, 0x8000)), bytes(uc.mem_read(0x837b90, 68 * 4)),
                            bytes(uc.mem_read(0x8383f8, 8)), uc.reg_read(UC_X86_REG_EAX) if address == 0x5a6210 else None))
        assert results[0] == results[1], (hex(address), args, counts)
        counts[hex(address)] += 1

    for trial in range(16):
        state = bytearray(b'\xa5' * 0x10000)
        for i in range(20):
            struct.pack_into('<f', state, i * 4, rng.uniform(0, 3))
        state[0x50:0x94] = bytes(68)
        for group in range(30):
            struct.pack_into('<iiI', state, GROUPS - BASE + group * 0x17c, group // 3, 1 + group % 3, 2)
        for i, address in enumerate(addresses):
            struct.pack_into('<i', state, address - BASE, i)
            for j in range(4):
                weight = rng.choice((0.05, 0.15, 0.3, 0.6, 1/3))
                struct.pack_into('<f', state, address - BASE + 0x30 + j * 4, weight)
            for j in range(7):
                struct.pack_into('<i', state, address - BASE + 0x40 + j * 4, -1)
            state[0x50 + i] = 0 if trial == 0 else rng.randrange(2)
        struct.pack_into('<iii', state, 0x1900, 0, 5, 9)
        load(state, 30)
        for item in addresses:
            compare(0x591130, item, (), 0)
        for level in (0, 1, 2, 3):
            for count in (-1, 0, 1, 2, 3):
                for smallest in (0, 1, -1, 256):
                    compare(0x5a6210, 0, (BASE, level, BASE + 0x1900, count, smallest), 0)
        for target in (-0.9, -0.1, 0.0, 0.05, 0.2, 0.5, 0.9, 1.0, 1.2):
            bits = struct.unpack('<I', struct.pack('<f', target))[0]
            for pointer, count in ((BASE + 0x1900, 1), (BASE + 0x1900, 3), (0, 0), (0, -1)):
                load(state, 30)
                compare(0x5a6880, BASE, (bits, pointer, count), 12)
        # Non-contiguous -1.0 sentinels retain the original leading-field sum.
        for weights in ((-1.0, 0.2, -1.0, 0.8), (0.2, -1.0, 0.8, -1.0),
                        (0.25, 0.5, 0.75, 1.0), (0.25, -1.0, -1.0, -1.0)):
            for uc in machines:
                uc.mem_write(addresses[0] + 0x30, struct.pack('<ffff', *weights))
            compare(0x591130, addresses[0], (), 0)
    assert all(visits.values()), visits
    report = dict(original_branch_visits={hex(a): count for a, count in visits.items()}, executable_sha256=manifest['exe_sha256'], bit_identical_cases=counts,
                  byte_matching='not established', controlled_interfaces=[],
                  limitations=['Finite synthetic descriptors; missing group lookups, cyclic prerequisites and failed candidate activation excluded.',
                               'All covered original callees execute recovered source on the rebuilt side.',
                               'Original __ftol2 executes as an analysis runtime dependency.',
                               'Chunk loading and full record/tuning composition are not covered.'])
    (BUILD / 'verification.json').write_text(json.dumps(report, indent=2) + '\n')
    print(json.dumps(report, indent=2))


if __name__ == '__main__':
    main()
