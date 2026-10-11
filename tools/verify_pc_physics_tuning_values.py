#!/usr/bin/env python3
"""Compare tuning value offset/field/read/engine-normalization helpers with original x86."""
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

BUILD = ROOT / 'build/analysis/physics-tuning-values'
BASE, STACK, STOP, WRAPPER = 0x2000000, 0x200f000, 0x2100000, 0x2100100
TUNING, RECORD, SELECTION, ROWS, OUT = [BASE + n for n in (0x100, 0x1000, 0x2000, 0x3000, 0x8000)]
TABLE_POINTER = 0x8a1ccc
CONTROLS = (0x027f, 0x037f, 0x007f)  # 53-, 64- and 24-bit x87 precision
SAVED = (UC_X86_REG_EBX, UC_X86_REG_ESI, UC_X86_REG_EDI, UC_X86_REG_EBP)


def main():
    manifest = json.loads((ROOT / 'config/SPEED2_EXE_1_2/physics-tuning-values.json').read_text())
    extra = json.loads((ROOT / 'config/SPEED2_EXE_1_2/physics-extra-tuning.json').read_text())
    raw = EXE.read_bytes()
    assert hashlib.sha256(raw).hexdigest() == manifest['exe_sha256']
    image = pefile.PE(data=raw).get_memory_mapped_image()
    callees = [f for f in extra['functions'] if f['method'] in ('ApplyExtra', 'RemoveExtra')]
    for f in manifest['functions'] + callees:
        body = image[f['address'] - 0x400000:f['address'] - 0x400000 + f['size']]
        assert hashlib.sha256(body).hexdigest() == f['sha256'], f['method']
    constants = {0x784258: 100.0, 0x78425c: 0.009999999776482582, 0x7842a0: -1.0, 0x784250: 1.0,
                 0x79da34: 0.1111111119389534, 0x784260: 0.5, 0x7d7d3c: 0.0}
    for address, value in constants.items():
        assert struct.unpack_from('<f', image, address - 0x400000)[0] == value, hex(address)
    BUILD.mkdir(parents=True, exist_ok=True)
    source = BUILD / 'tuning-values-analysis.cpp'
    source.write_text(''.join('#include "' + str(ROOT / 'src/Speed/Indep/Src/Physics' / (unit + '.cpp')) + '"\n'
                              for unit in ('PhysicsRecordExtra', 'PhysicsTuningValues')))
    obj = BUILD / 'tuning-values-analysis.obj'
    subprocess.run([str(ROOT / 'build/tools/wibo'), 'build/compilers/Win32/7.1/cl.exe', '/nologo',
                    '/c', '/Ox', '/Op', '/Ob0', '/Z7', '/EHsc', '/Fo' + str(obj.relative_to(ROOT)),
                    str(source.relative_to(ROOT))], cwd=ROOT, check=True)
    linked = link_objects([obj], {'?carTypeInfoTable@@3PAEA': TABLE_POINTER,
                                  '?physicsTuningCentre@@3MA': 0x784258, '?physicsTuningScale@@3MA': 0x78425c,
                                  '?physicsTuningMinimum@@3MA': 0x7842a0, '?physicsTuningMaximum@@3MA': 0x784250,
                                  '?physicsTuningEngineScale@@3MA': 0x79da34, '__fltused': BASE + 0x9000})
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
        ranges = linked['ranges'] if side else [(f['address'], f['address'] + f['size']) for f in manifest['functions'] + callees]
        # Probe-only wrappers, one per method and written once: Unicorn keeps
        # translated blocks, so rewriting a shared wrapper would run stale code.
        for slot, method in enumerate(('GetOffset', 'GetValue')):
            wrapper = WRAPPER + 0x20 * slot
            target = entries[method][side]
            uc.mem_write(wrapper, b'\xe8' + struct.pack('<i', target - wrapper - 5) + b'\xdb\x3d' + struct.pack('<I', OUT)
                         + b'\x68' + struct.pack('<I', STOP) + b'\xc3')
            ranges = ranges + [(wrapper, wrapper + 0x20)]
        def guard(machine, address, size, user, ranges=ranges):
            assert any(a <= address < b for a, b in ranges), hex(address)
        uc.hook_add(UC_HOOK_CODE, guard)
        machines.append(uc)
    counts = dict.fromkeys(entries, 0)
    cleanup = dict(GetOffset=8, GetField=12, NormalizeEngine=4, GetValue=8)

    def compare(method, state, args, receiver, control=0x027f):
        outcomes = []
        for side, uc in enumerate(machines):
            uc.mem_write(BASE, bytes(state))
            uc.mem_write(TABLE_POINTER, struct.pack('<I', ROWS))
            uc.mem_write(OUT, b'\xa5' * 16)
            words = [a & 0xffffffff for a in args]
            floating = method in ('GetOffset', 'GetValue')
            if floating:
                uc.mem_write(STACK, struct.pack('<' + 'I' * len(words), *words))
                start, expected = WRAPPER + 0x20 * (method == 'GetValue'), STACK + 4 * len(words)
            else:
                uc.mem_write(STACK, struct.pack('<' + 'I' * (1 + len(words)), STOP, *words))
                start, expected = entries[method][side], STACK + 4 + cleanup[method]
            uc.reg_write(UC_X86_REG_ECX, receiver)
            uc.reg_write(UC_X86_REG_ESP, STACK)
            uc.reg_write(UC_X86_REG_FPCW, control)
            uc.reg_write(UC_X86_REG_FPSW, 0)
            uc.reg_write(UC_X86_REG_FPTAG, 0xffff)
            uc.reg_write(UC_X86_REG_EAX, 0x5a5a5a5a)
            for reg in SAVED:
                uc.reg_write(reg, 0x12345678)
            uc.emu_start(start, STOP, count=200000)
            assert uc.reg_read(UC_X86_REG_EIP) == STOP
            assert uc.reg_read(UC_X86_REG_ESP) == expected, (method, side)
            assert all(uc.reg_read(reg) == 0x12345678 for reg in SAVED)
            assert (uc.reg_read(UC_X86_REG_FPSW) >> 11) & 7 == 0, (method, side, 'x87 stack depth')
            result = bytes(uc.mem_read(OUT, 10)) if floating else (uc.reg_read(UC_X86_REG_EAX) if method == 'GetField' else None)
            outcomes.append((bytes(uc.mem_read(BASE, 0x8000)), result))
        assert outcomes[0] == outcomes[1], (method, counts[method], args, hex(control),
                                            next((hex(i) for i, (a, b) in enumerate(zip(outcomes[0][0], outcomes[1][0])) if a != b), None),
                                            outcomes[0][1], outcomes[1][1])
        counts[method] += 1
        return outcomes[0][1]

    # Offset query: every byte value at every component slot, indices beyond the
    # three kinds (default zero) and negative indices that read preceding fields.
    for control in CONTROLS:
        for v in range(256):
            state = bytearray(b'\xa5' * 0x8000)
            for k in range(41):
                state[TUNING - BASE + 0x14 + k] = (v + 97 * k) & 255
            for index in range(-2, 28):
                for component in (range(9) if index in (23, 24) else (0, 5)):
                    compare('GetOffset', state, (index, component), TUNING + 0x14, control)

    # Field addresses: receiver is ignored; out-of-range indices return null.
    state = bytearray(0x8000)
    for index in list(range(-3, 28)) + [-0x80000000, 0x7fffffff]:
        for component in (-1, 0, 1, 2, 8, 9):
            for receiver in (0, TUNING, 0xdeadbeef):
                compare('GetField', state, (RECORD, index, component), receiver)

    rng = random.Random(0x59b3c0)
    samples = [-0.0, 0.0, 1.0, -1.0, 16777216.0, -16777216.0, 0.5, 2.0 ** 50, -2.0 ** 50, 1e-30, 3.4e38, -3.4e38]
    for control in CONTROLS:
        for trial in range(400):
            state = bytearray(0x8000)
            struct.pack_into('<I', state, TUNING - BASE + 0xc, RECORD)
            for i in range(9):
                value = rng.choice(samples) if trial % 2 else rng.uniform(-5000.0, 5000.0)
                struct.pack_into('<f', state, RECORD - BASE + 0x200 + 4 * i, value)
            total = rng.choice(samples + [rng.uniform(-1e5, 1e5)])
            compare('NormalizeEngine', state, (struct.unpack('<I', struct.pack('<f', total))[0],), TUNING, control)

    # Value reads: retail rows as CarTypeInfo sources and records; every valid
    # index and component, outermost and nested depths, and extra flag masks.
    flags = (0, 1, 2, 3, 4, 6, 7, 8)
    rng = random.Random(0x5a7da0)
    rows = [row for _, row in retail_sources()]
    for trial, row in enumerate(rows):
        for row_index in (0, 2):
            for depth in (0, 1, 3, -1):
                for control in (CONTROLS if depth == 0 else CONTROLS[:1]):
                    state = bytearray(b'\xa5' * 0x8000)
                    for r in range(3):
                        state[ROWS - BASE + r * 0x890 + 0x110:ROWS - BASE + r * 0x890 + 0x890] = rows[(trial + r) % len(rows)]
                    record = bytearray(rows[(trial + row_index) % len(rows)][:0x2b0])
                    for i in range(0, 0x2b0, 4):
                        if rng.randrange(4) == 0:
                            struct.pack_into('<f', record, i, struct.unpack_from('<f', record, i)[0] + rng.uniform(-2.0, 2.0))
                    state[RECORD - BASE:RECORD - BASE + 0x2b0] = record
                    state[SELECTION - BASE:SELECTION - BASE + 0x94] = bytes(0x94)
                    mask = rng.randrange(256)
                    for bit, flag in enumerate(flags):
                        struct.pack_into('<I', state, SELECTION - BASE + 0x28 + 4 * flag, (1, 0x80000000)[bit % 2] if mask & (1 << bit) else 0)
                    struct.pack_into('<iiII', state, TUNING - BASE, row_index, 0, SELECTION, RECORD)
                    struct.pack_into('<i', state, TUNING - BASE + 0xe4, depth)
                    for index in range(25):
                        for component in (range(9) if index in (23, 24) else (0, 1)):
                            compare('GetValue', state, (index, component), TUNING, control)
    # Signed zero and cancellation in averaged, source-relative and engine reads.
    for a, b in [(-0.0, -0.0), (0.0, -0.0), (16777216.0, 1.0), (2.0 ** 50, -2.0 ** 50), (3.4e38, 3.4e38), (1.0, 1e-30)]:
        for mask in (0, 0xff):
            state = bytearray(0x8000)
            for offset in (0x18, 0x48, 0x78, 0xa8, 0xdc, 0xfc, 0x178, 0x198, 0x200, 0x224):
                for i in range(9):
                    struct.pack_into('<f', state, RECORD - BASE + offset + 4 * i, (a, b)[i % 2])
                    struct.pack_into('<f', state, ROWS - BASE + 0x110 + offset + 4 * i, (b, a)[i % 2])
            for bit, flag in enumerate(flags):
                struct.pack_into('<I', state, SELECTION - BASE + 0x28 + 4 * flag, 1 if mask & (1 << bit) else 0)
            struct.pack_into('<iiII', state, TUNING - BASE, 0, 0, SELECTION, RECORD)
            for index in range(25):
                for component in (range(9) if index in (23, 24) else (0, 1)):
                    compare('GetValue', state, (index, component), TUNING)
    report = dict(executable_sha256=manifest['exe_sha256'], bit_identical_cases=counts,
                  byte_matching='not established', controlled_interfaces=[],
                  executed_callees=['0x599d90 RemoveExtra', '0x5997e0 ApplyExtra (recovered source on the rebuilt side)'],
                  limitations=['Finite retail rows, perturbed records and synthetic flags; NaN inputs excluded.',
                               'Indices outside 0..24 make 0x5a7da0 dereference null in the original; not exercised.',
                               'Return values compared as 80-bit x87 stores under 24/53/64-bit precision control.',
                               'Callers 0x5b4780/0x5b2c00/0x5b3060/0x5b3100/0x5b3e40/0x5b3fa0/0x5b4670 remain unrecovered.'])
    (BUILD / 'verification.json').write_text(json.dumps(report, indent=2) + '\n')
    print(json.dumps(report, indent=2))


if __name__ == '__main__':
    main()
