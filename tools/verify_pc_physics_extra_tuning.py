#!/usr/bin/env python3
"""Compare extra record adjustments and tuning eligibility with original x86."""
import hashlib
import json
import random
import struct
import subprocess
import pefile
from unicorn import Uc, UC_ARCH_X86, UC_MODE_32, UC_HOOK_CODE
from unicorn.x86_const import *
from pc_coff import link_objects
from probe_pc_race_reward import ROOT, EXE
from verify_pc_physics_record_upgrades import retail_sources

BUILD = ROOT / 'build/analysis/physics-extra-tuning'
BASE, STACK, STOP = 0x2000000, 0x200f000, 0x2100000
SOURCE, RECORD, SELECTION, TABLE, TUNING = [BASE + n for n in (0x1000, 0x2000, 0x3000, 0x4000, 0x6000)]


def main():
    manifest = json.loads((ROOT / 'config/SPEED2_EXE_1_2/physics-extra-tuning.json').read_text())
    raw = EXE.read_bytes()
    assert hashlib.sha256(raw).hexdigest() == manifest['exe_sha256']
    image = pefile.PE(data=raw).get_memory_mapped_image()
    for f in manifest['functions']:
        body = image[f['address'] - 0x400000:f['address'] - 0x400000 + f['size']]
        assert hashlib.sha256(body).hexdigest() == f['sha256']
    assert struct.unpack_from('<f', image, 0x784250 - 0x400000)[0] == 1.0
    assert struct.unpack_from('<I', image, 0x7d7d3c - 0x400000)[0] == 0
    BUILD.mkdir(parents=True, exist_ok=True)
    source = BUILD / 'extra-tuning-analysis.cpp'
    source.write_text(''.join('#include "' + str(ROOT / 'src/Speed/Indep/Src/Physics' / (unit + '.cpp')) + '"\n'
                              for unit in ('PhysicsRecordExtra', 'PhysicsTuningSelection')))
    obj = BUILD / 'extra-tuning-analysis.obj'
    subprocess.run([str(ROOT / 'build/tools/wibo'), 'build/compilers/Win32/7.1/cl.exe', '/nologo',
                    '/c', '/Ox', '/Op', '/Ob0', '/Z7', '/EHsc', '/Fo' + str(obj.relative_to(ROOT)),
                    str(source.relative_to(ROOT))], cwd=ROOT, check=True)
    linked = link_objects([obj], {'?physicsTuningEntries@@3PAUPhysicsTuningEntry@@A': 0x86b340,
                                  '?physicsTuningEntryCount@@3HA': 0x86b344, '__fltused': BASE + 0x9000})
    entries = {}
    for f in manifest['functions']:
        found = [a for s, a in linked['exports'].items() if s.startswith('?' + f['method'] + '@')]
        assert len(found) == 1, found
        entries[f['method']] = (f['address'], found[0])
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
    counts = dict.fromkeys(entries, 0)

    def compare(method, state, args, receiver=0, table=TABLE, count=75):
        outcomes = []
        for side, uc in enumerate(machines):
            uc.mem_write(BASE, bytes(state))
            uc.mem_write(0x86b340, struct.pack('<Ii', table, count))
            uc.mem_write(STACK, struct.pack('<' + 'I' * (1 + len(args)), STOP, *[a & 0xffffffff for a in args]))
            uc.reg_write(UC_X86_REG_ECX, receiver)
            uc.reg_write(UC_X86_REG_ESP, STACK)
            uc.reg_write(UC_X86_REG_FPCW, 0x027f)
            for reg in (UC_X86_REG_EBX, UC_X86_REG_ESI, UC_X86_REG_EDI, UC_X86_REG_EBP):
                uc.reg_write(reg, 0x12345678)
            uc.emu_start(entries[method][side], STOP, count=100000)
            assert uc.reg_read(UC_X86_REG_EIP) == STOP
            cleanup = 12 if method in ('ApplyExtra', 'RemoveExtra') else 4
            assert uc.reg_read(UC_X86_REG_ESP) == STACK + cleanup
            assert all(uc.reg_read(reg) == 0x12345678 for reg in (UC_X86_REG_EBX, UC_X86_REG_ESI, UC_X86_REG_EDI, UC_X86_REG_EBP))
            result = uc.reg_read(UC_X86_REG_EAX) if method not in ('ApplyExtra', 'RemoveExtra') else None
            outcomes.append((bytes(uc.mem_read(BASE, 0x8000)), result))
        assert outcomes[0] == outcomes[1], (method, counts[method], next((hex(i) for i, (a, b) in enumerate(zip(outcomes[0][0], outcomes[1][0])) if a != b), None), outcomes[0][1], outcomes[1][1])
        counts[method] += 1
        return outcomes[0][1]

    flags = (0, 1, 2, 3, 4, 6, 7, 8)
    for name, source_row in retail_sources():
        for mask in range(256):
            state = bytearray(b'\xa5' * 0x8000)
            state[SOURCE - BASE:SOURCE - BASE + 0x780] = source_row
            state[RECORD - BASE:RECORD - BASE + 0x2b0] = source_row[:0x2b0]
            state[SELECTION - BASE:SELECTION - BASE + 0x94] = bytes(0x94)
            for bit, flag in enumerate(flags):
                struct.pack_into('<I', state, SELECTION - BASE + 0x28 + 4 * flag, (1, 0x80000000, 0xffffffff)[mask % 3] if mask & (1 << bit) else 0)
            compare('ApplyExtra', state, (RECORD, SELECTION), SOURCE)
            compare('RemoveExtra', state, (RECORD, SELECTION), SOURCE)
    # Signed zero and cancellation expose erroneous intermediate float reloads.
    for initial, first, second, third in [(-0.0, -0.0, -0.0, -0.0), (1.0, 16777216.0, -16777216.0, 0.5),
                                       (1.0, 2.0**50, -2.0**50, -1.0), (-1.0, 2.0**24, -2.0**24, 0.0)]:
        for mask in range(8):
            state = bytearray(0x8000)
            for i in range(9):
                for base, value in ((0x200, initial), (0x224, initial)):
                    struct.pack_into('<f', state, RECORD - BASE + base + i * 4, value)
                for base, value in ((0x2d0, first), (0x2f4, second), (0x318, third)):
                    struct.pack_into('<f', state, SOURCE - BASE + base + i * 4, value)
            for bit, flag in enumerate((3, 7, 4)):
                struct.pack_into('<I', state, SELECTION - BASE + 0x28 + flag * 4, (mask >> bit) & 1)
            compare('ApplyExtra', state, (RECORD, SELECTION), SOURCE)
            compare('RemoveExtra', state, (RECORD, SELECTION), SOURCE)
    rng = random.Random(0x59ae40)
    for trial in range(128):
        state = bytearray(b'\xa5' * 0x8000)
        for i in range(68):
            state[SELECTION - BASE + 0x50 + i] = rng.randrange(2) if trial else 0
        for i in range(75):
            # Every kind has competing entries; include signed priorities and ties.
            struct.pack_into('<ibb', state, TABLE - BASE + i * 0x18, i % 68, (-128, -1, 0, 127)[(i + trial) % 4], (i % 25) if trial % 2 else (-128, -1, 0, 24, 127)[i % 5])
        for table, count in ((0, 75), (TABLE, -1), (TABLE, 0), (TABLE, 1), (TABLE, 75)):
            for index in (-129, -128, -1, 0, 10, 15, 24, 25, 127, 128):
                compare('FindPhysicsTuningEntry', state, (index, SELECTION), table=table, count=count)
            for gears in (-2, 0, 1, 3, 6, 8):
                struct.pack_into('<IIII', state, TUNING - BASE, 0, 0, SELECTION, RECORD)
                struct.pack_into('<i', state, RECORD - BASE + 0x1c8, gears)
                compare('CountActive', state, (), TUNING, table=table, count=count)
    report = dict(executable_sha256=manifest['exe_sha256'], bit_identical_cases=counts,
                  byte_matching='not established', controlled_interfaces=[],
                  limitations=['Finite retail rows and synthetic flags/tables; NaNs, malformed indices and overlapping objects excluded.',
                               'Comparison uses 53-bit x87 arithmetic; full tuning application and table installation remain unresolved.'])
    (BUILD / 'verification.json').write_text(json.dumps(report, indent=2) + '\n')
    print(json.dumps(report, indent=2))


if __name__ == '__main__':
    main()
