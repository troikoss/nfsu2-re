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


FLOATING = ('GetOffset', 'GetValue', 'ConvertUnits', 'GetGearValue', 'GetSign', 'EngineMaximum', 'EngineMinimum',
            'GetLinkedExtreme', 'EngineRemapClamped')
# Bytes of stack arguments removed by the callee.
CLEANUP = dict(GetOffset=8, GetField=12, NormalizeEngine=4, GetValue=8, SetOffset=12, IsActive=4, ApplyEntry=12,
               ConvertUnits=12, GetGearValue=8, GetSign=4, EngineMaximum=0, EngineMinimum=0, GetLinkedExtreme=8,
               GetBalanceRange=20, GetPairRange=20, ExpandPhysicsTuningRange=0, EngineRemapClamped=0)


def original_symbols():
    """Map OriginalSymbols.ld data names to their float/double/int mangled forms."""
    names = {}
    for line in (ROOT / 'src/Platform/Native/OriginalSymbols.ld').read_text().splitlines():
        parts = line.replace(';', '').split('=')
        if len(parts) == 2 and parts[1].strip().startswith('0x'):
            name, address = parts[0].strip(), int(parts[1], 16)
            for suffix in ('3MA', '3NA', '3HA'):
                names['?%s@@%s' % (name, suffix)] = address
    return names


def bits(value):
    return struct.unpack('<I', struct.pack('<f', value))[0]


def main():
    manifest = json.loads((ROOT / 'config/SPEED2_EXE_1_2/physics-tuning-values.json').read_text())
    extra = json.loads((ROOT / 'config/SPEED2_EXE_1_2/physics-extra-tuning.json').read_text())
    raw = EXE.read_bytes()
    assert hashlib.sha256(raw).hexdigest() == manifest['exe_sha256']
    image = pefile.PE(data=raw).get_memory_mapped_image()
    callees = [f for f in extra['functions'] if f['method'] in ('ApplyExtra', 'RemoveExtra', 'FindPhysicsTuningEntry')]
    for f in manifest['functions'] + manifest['dependencies'] + callees:
        body = image[f['address'] - 0x400000:f['address'] - 0x400000 + f['size']]
        assert hashlib.sha256(body).hexdigest() == f['sha256'], f.get('method', f.get('name'))
    constants = {0x784258: 100.0, 0x78425c: 0.009999999776482582, 0x7842a0: -1.0, 0x784250: 1.0,
                 0x79da34: 0.1111111119389534, 0x784260: 0.5, 0x7d7d3c: 0.0, 0x7a1e50: 100.5,
                 0x7a1e6c: 39.369998931884766, 0x7a1e68: 0.025400051847100258, 0x784550: 1000.0,
                 0x78435c: 0.0010000000474974513, 0x79befc: 0.7375621199607849, 0x7904fc: 3.5999999046325684,
                 0x7a1e54: 0.2777777910232544, 0x78426c: 2.236999988555908, 0x7842a8: 0.4470272660255432,
                 0x7a1d38: 0.10471966862678528, 0x784554: 0.20000000298023224, 0x7850ec: 0.800000011920929}
    for address, value in constants.items():
        assert struct.unpack_from('<f', image, address - 0x400000)[0] == value, hex(address)
    assert struct.unpack_from('<dd', image, 0x7a1e58 - 0x400000) == (0.001, 1.35581795)
    BUILD.mkdir(parents=True, exist_ok=True)
    source = BUILD / 'tuning-values-analysis.cpp'
    source.write_text(''.join('#include "' + str(ROOT / 'src/Speed/Indep/Src/Physics' / (unit + '.cpp')) + '"\n'
                              for unit in ('PhysicsRecordExtra', 'PhysicsTuningSelection', 'EngineMath', 'PhysicsTuningValues')))
    obj = BUILD / 'tuning-values-analysis.obj'
    subprocess.run([str(ROOT / 'build/tools/wibo'), 'build/compilers/Win32/7.1/cl.exe', '/nologo',
                    '/c', '/Ox', '/Op', '/Ob0', '/Z7', '/EHsc', '/Fo' + str(obj.relative_to(ROOT)),
                    str(source.relative_to(ROOT))], cwd=ROOT, check=True)
    externals = original_symbols()
    externals.update({'?carTypeInfoTable@@3PAEA': TABLE_POINTER, '?physicsTuningEntries@@3PAUPhysicsTuningEntry@@A': 0x86b340,
                      '?physicsTuningEntryCount@@3HA': 0x86b344, '__ftol2': 0x75bed4, '__fltused': BASE + 0x9000})
    linked = link_objects([obj], externals)
    entries = {}
    for f in manifest['functions']:
        found = [a for s, a in linked['exports'].items() if s.startswith('?' + f['method'] + '@')]
        assert len(found) == 1, (f['method'], found)
        entries[f['method']] = (f['address'], found[0])
    runtime = [(f['address'], f['address'] + f['size']) for f in manifest['dependencies']]
    wrappers = {}
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
        ranges = ranges + runtime
        # Probe-only wrappers, one per method and written once: Unicorn keeps
        # translated blocks, so rewriting a shared wrapper would run stale code.
        for slot, method in enumerate(FLOATING):
            wrapper = WRAPPER + 0x20 * slot
            wrappers[method] = wrapper
            target = entries[method][side]
            uc.mem_write(wrapper, b'\xe8' + struct.pack('<i', target - wrapper - 5) + b'\xdb\x3d' + struct.pack('<I', OUT)
                         + b'\x68' + struct.pack('<I', STOP) + b'\xc3')
            ranges = ranges + [(wrapper, wrapper + 0x20)]
        def guard(machine, address, size, user, ranges=ranges):
            assert any(a <= address < b for a, b in ranges), hex(address)
        uc.hook_add(UC_HOOK_CODE, guard)
        machines.append(uc)
    counts = dict.fromkeys(entries, 0)

    def compare(method, state, args, receiver, control=0x027f, table=0, count=0):
        outcomes = []
        for side, uc in enumerate(machines):
            uc.mem_write(BASE, bytes(state))
            uc.mem_write(TABLE_POINTER, struct.pack('<I', ROWS))
            uc.mem_write(0x86b340, struct.pack('<Ii', table, count))
            uc.mem_write(OUT, b'\xa5' * 16)
            words = [a & 0xffffffff for a in args]
            if method in FLOATING:
                uc.mem_write(STACK, struct.pack('<' + 'I' * len(words), *words))
                start, expected = wrappers[method], STACK + CLEANUP[method]
            else:
                uc.mem_write(STACK, struct.pack('<' + 'I' * (1 + len(words)), STOP, *words))
                start, expected = entries[method][side], STACK + 4 + CLEANUP[method]
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
            if method in FLOATING:
                result = bytes(uc.mem_read(OUT, 10))
            elif method == 'GetField':
                result = uc.reg_read(UC_X86_REG_EAX)
            elif method == 'IsActive':
                result = uc.reg_read(UC_X86_REG_EAX) & 0xff  # bool: only al is defined
            else:
                result = None
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
    specials = [0.0, -0.0, 1.0, -1.0, 0.5, -0.5, 1e-30, -1e-30, 3.4e38, -3.4e38, float('inf'), float('-inf'),
                0.199999988079071, 0.20000000298023224, 0.20000001788139343, 0.7999999523162842,
                0.800000011920929, 0.8000000715255737, 16777216.0, 123.456]
    nan = struct.unpack('<f', struct.pack('<I', 0x7fc00000))[0]

    # Offset store: rounding bias, truncation, clamps, overflowing conversions and NaN.
    rng = random.Random(0x59af80)
    stores = [-2.0, -1.01, -1.005, -1.0049999952316284, -1.0, -0.995, -0.005, -0.004999999888241291, 0.0, -0.0,
              0.004999999888241291, 0.005, 0.5, 0.995, 1.0, 1.005, 1.0049999952316284, 1.01, 2.0, 1e10, -1e10, nan]
    stores += [rng.uniform(-1.2, 1.2) for _ in range(40)]
    for control in CONTROLS:
        for value in stores:
            state = bytearray(b'\xa5' * 0x8000)
            for index in range(-2, 28):
                for component in (range(9) if index in (23, 24) else (0, 4)):
                    compare('SetOffset', state, (bits(value), index, component), TUNING + 0x14, control)

    # Entry-dependent tests share a lookup table with every kind byte and
    # signed priorities; pairs are distinct and aliased.
    rng = random.Random(0x59b160)
    for trial in range(48):
        state = bytearray(b'\xa5' * 0x8000)
        for i in range(68):
            state[SELECTION - BASE + 0x50 + i] = rng.randrange(2) if trial else 1
        for i in range(30):
            v8, vc = rng.choice(specials + [rng.uniform(-3, 3)]), rng.choice(specials + [rng.uniform(-3, 3)])
            struct.pack_into('<ibbBBff', state, ROWS - BASE + 0x1800 + i * 0x18, i % 68, (-1, 0, 5, 127)[(i + trial) % 4],
                             i % 25, (0, 1, 2, 3, 4, 5, 255)[(i + trial) % 7], 0xa5, v8, vc)
        struct.pack_into('<IIII', state, TUNING - BASE, 0, 1, SELECTION, RECORD)
        struct.pack_into('<i', state, RECORD - BASE + 0x1c8, (-2, 0, 1, 3, 6, 8)[trial % 6])
        first, second = rng.choice(specials + [rng.uniform(-3, 3)]), rng.choice(specials + [rng.uniform(-3, 3)])
        struct.pack_into('<ff', state, OUT - 0x100 - BASE, first, second)
        table = (ROWS + 0x1800, 0)[trial == 47]
        for index in list(range(-2, 28)) + [-129, 128]:
            compare('IsActive', state, (index,), TUNING, table=table, count=30)
            for pair in ((OUT - 0x100, OUT - 0xfc), (OUT - 0xfc, OUT - 0x100), (OUT - 0x100, OUT - 0x100)):
                for control in (CONTROLS if trial < 8 else CONTROLS[:1]):
                    compare('ApplyEntry', state, (index,) + pair, TUNING, control, table=table, count=30)

    # Unit conversion: both scale sets, every kind, low-byte flag and specials.
    rng = random.Random(0x59b4a0)
    for control in CONTROLS:
        for value in specials + [nan] + [rng.uniform(-500, 500) for _ in range(12)]:
            for system in (1, 0, 2, -1):
                state = bytearray(0x8000)
                struct.pack_into('<i', state, TUNING - BASE + 4, system)
                for index in range(-2, 28):
                    for forward in (0, 1, 0x100, 0xff):
                        compare('ConvertUnits', state, (index, bits(value), forward), TUNING, control)

    # Gear value: selector around both thresholds, NaN, zero and signed values.
    rng = random.Random(0x59b5c0)
    selectors = [0.199999988079071, 0.20000000298023224, 0.20000001788139343, 0.5, 0.7999999523162842,
                 0.800000011920929, 0.8000000715255737, -1.0, 2.0, nan, float('inf'), 0.0, -0.0]
    for control in CONTROLS:
        for trial, selector in enumerate(selectors * 6):
            state = bytearray(0x8000)
            struct.pack_into('<I', state, TUNING - BASE + 0xc, RECORD)
            for offset in (0x50, 0x80, 0x1b8, 0x1f4):
                value = rng.choice(specials) if trial % 3 == 0 else rng.uniform(-9000, 9000)
                struct.pack_into('<f', state, RECORD - BASE + offset, value)
            struct.pack_into('<f', state, RECORD - BASE + 0x1c0, selector)
            for value in (0.0, -0.0, 1.0, -2.5, nan, rng.uniform(-100, 100), 3.4e38):
                for inverse in (0, 1, 0x100):
                    compare('GetGearValue', state, (bits(value), inverse), TUNING, control)

    state = bytearray(0x8000)
    for index in list(range(-2, 30)) + [-0x80000000, 0x7fffffff]:
        for receiver in (0, TUNING):
            compare('GetSign', state, (index,), receiver)
    for a in specials + [nan]:
        for b in specials + [nan]:
            compare('EngineMaximum', state, (bits(a), bits(b)), 0)
            compare('EngineMinimum', state, (bits(a), bits(b)), 0)

    # Shared range helpers: specials, NaN, equal and inverted input ranges.
    rng = random.Random(0x401790)
    floats = specials + [nan] + [rng.uniform(-50, 50) for _ in range(6)]
    for low in floats:
        for value in floats:
            state = bytearray(0x8000)
            struct.pack_into('<ff', state, OUT - 0x100 - BASE, low, rng.choice(floats))
            compare('ExpandPhysicsTuningRange', state, (OUT - 0x100, bits(value)), 0)
    for control in CONTROLS:
        for trial in range(1500):
            args = [rng.choice(floats) if trial % 2 else rng.uniform(-10, 10) for _ in range(5)]
            if trial % 7 == 0:
                args[2] = args[1]
            compare('EngineRemapClamped', bytearray(0x8000), tuple(bits(v) for v in args), 0, control)

    # Linked extremes: every kind, component and tie/NaN pattern.
    rng = random.Random(0x5a79f0)
    for trial in range(24):
        state = bytearray(0x8000)
        for i in range(41):
            value = rng.choice(specials + [nan, 1e7, -1e7, 1.0]) if trial % 2 else rng.uniform(-2e7, 2e7)
            struct.pack_into('<f', state, TUNING - BASE + 0x40 + 4 * i, value)
        for index in list(range(-3, 31)) + [0x7fffffff, -0x80000000]:
            for component in (range(9) if index in (23, 24) else (0, 3)):
                if index in (0x7fffffff, -0x80000000):
                    continue  # the original reads values40[index] out of bounds
                compare('GetLinkedExtreme', state, (index, component), TUNING)

    # Balance/pair ranges: retail CarTypeInfo rows and entries for kinds 1/5/19/20.
    rng = random.Random(0x5a7b80)
    for trial, row in enumerate(rows):
        state = bytearray(b'\xa5' * 0x8000)
        state[ROWS - BASE + 0x890 + 0x110:ROWS - BASE + 0x890 + 0x890] = row
        for i in range(68):
            state[SELECTION - BASE + 0x50 + i] = rng.randrange(2)
        for i in range(12):
            struct.pack_into('<ibbBBff', state, ROWS - BASE + 0x1800 + 0x890 + i * 0x18, i % 68, rng.randrange(-2, 3),
                             (1, 5, 19, 20)[i % 4], rng.choice((0, 1, 2, 3, 4, 7)), 0xa5,
                             rng.choice(specials[:8] + [rng.uniform(-1, 1)]), rng.choice(specials[:8] + [rng.uniform(-1, 1)]))
        struct.pack_into('<iiII', state, TUNING - BASE, 1, 0, SELECTION, RECORD)
        for index in (1, 5, 19, 20, 0, -1, 24):
            for component in (0, 1):
                for average in (0, 1, 0x100):
                    for method in ('GetBalanceRange', 'GetPairRange'):
                        for control in (CONTROLS if trial < 4 else CONTROLS[:1]):
                            compare(method, state, (index, component, average, OUT - 0x100, OUT - 0xfc), TUNING, control,
                                    table=ROWS + 0x1800 + 0x890, count=12 if trial % 5 else 0)

    report = dict(executable_sha256=manifest['exe_sha256'], bit_identical_cases=counts,
                  byte_matching='not established', controlled_interfaces=[],
                  executed_callees=['0x599d90 RemoveExtra, 0x5997e0 ApplyExtra, 0x59ae40 lookup and 0x401070 minimum: recovered source on the rebuilt side',
                                    '0x75bed4 __ftol2: original runtime on both sides'],
                  limitations=['Finite retail rows, perturbed records, synthetic flags and lookup tables.',
                               'Indices outside 0..24 make 0x5a7da0 dereference null in the original; not exercised.',
                               'Return values compared as 80-bit x87 stores under 24/53/64-bit precision control.',
                               'NaN appears only in offset stores, conversions, gear values and min/max, not in value reads.',
                               'Range selection uses components 0/1 only; larger components read caller stack slots.',
                               'Full tuning application 0x5b4780 and its remaining callees are not yet recovered.'])
    (BUILD / 'verification.json').write_text(json.dumps(report, indent=2) + '\n')
    print(json.dumps(report, indent=2))


if __name__ == '__main__':
    main()
