#!/usr/bin/env python3
"""Compare keyboard/analog steering, the steering limit and slip helpers with original x86."""
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
from verify_pc_engine import CONVERSION
from verify_pc_physics_compose import callee_closure
from verify_pc_physics_record_upgrades import retail_sources

BUILD = ROOT / 'build/analysis/human-steering-update'
BASE, STACK, STOP, WRAPPER, OUT = 0x2000000, 0x201f000, 0x2100000, 0x2100100, 0x2100800
# /Op lowers sqrt to __CIsqrt; the original uses inline fsqrt. For the
# non-negative sums of squares here an analysis-only fsqrt/ret helper is
# equivalent and is the only code at this address.
CISQRT = 0x2100c00
STEERING, CAR, INFO, INPUT, BODY = BASE, BASE + 0x9000, BASE + 0xa000, BASE + 0xb000, BASE + 0xb800
MALLOC, FREE = 0x75c6c8, 0x75c6da
# Already verified routines executed as original code on both sides.
CONTROLLED = {'?GetSlipAngle@DrivetrainAuxiliary@@QAEMXZ': 0x591d90,
              '?GetPhysicsSpeed@PhysicsParameters@@QAEMXZ': 0x5b5930}
FLOATING = ('CalculateMaximum', 'UpdateKeyboard', 'UpdateAnalog', 'GetSteeringSlip')
CLEANUP = dict(CalculateMaximum=12, UpdateKeyboard=12, UpdateAnalog=12, GetSteeringSlip=0, IsAnalogInputDevice=0)
SAVED = (UC_X86_REG_EBX, UC_X86_REG_ESI, UC_X86_REG_EDI, UC_X86_REG_EBP)
FLAGS8 = (0x89e7d8, 0x89e7d9, 0x89e7e2)
FLAGS32 = (0x890100, 0x890104, 0x890108, 0x890110, 0x890114)


def main():
    manifest = json.loads((ROOT / 'config/SPEED2_EXE_1_2/human-steering-update.json').read_text())
    raw = EXE.read_bytes()
    assert hashlib.sha256(raw).hexdigest() == manifest['exe_sha256']
    image = pefile.PE(data=raw).get_memory_mapped_image()
    index = {int(f['start'], 16): f for f in json.loads((ROOT / 'build/analysis/function-index.json').read_text())['functions']}
    for f in manifest['functions']:
        assert hashlib.sha256(image[f['address'] - 0x400000:f['address'] - 0x400000 + f['size']]).hexdigest() == f['sha256'], f['method']

    def ranges_for(roots):
        result = []
        for address in sorted(callee_closure(roots)):
            if address not in index:
                continue  # unresolved indirect targets in the static call graph
            entry = index[address]
            assert hashlib.sha256(image[address - 0x400000:address - 0x400000 + entry['size']]).hexdigest() == entry['sha256']
            result.append((address, address + entry['size']))
        return result

    original_ranges = ranges_for([f['address'] for f in manifest['functions']])
    controlled_ranges = ranges_for(list(CONTROLLED.values()))
    constructor_ranges = ranges_for([0x405c60])
    BUILD.mkdir(parents=True, exist_ok=True)
    # Dependencies are compiled as their own verifiers compile them (without
    # /Op); the new routines use /Op to keep their binary32 spills.
    def compile_unit(name, paths, op):
        unity = BUILD / (name + '.cpp')
        unity.write_text(''.join('#include "%s"\n' % (ROOT / path) for path in paths))
        obj = BUILD / (name + '.obj')
        subprocess.run([str(ROOT / 'build/tools/wibo'), 'build/compilers/Win32/7.1/cl.exe', '/nologo', '/c', '/Ox']
                       + (['/Op'] if op else []) + ['/Ob0', '/Z7', '/EHsc', '/Fo' + str(obj.relative_to(ROOT)),
                       str(unity.relative_to(ROOT))], cwd=ROOT, check=True)
        return obj
    objects = [compile_unit('steering-dependencies', (
        'src/Speed/Indep/Libs/bWare/TimedAverage.cpp', 'src/Speed/Indep/Src/Physics/EngineCurve.cpp',
        'src/Speed/Indep/Src/Physics/DriveFeedbackCurve.cpp', 'src/Speed/Indep/Src/Physics/PairCurveLookup.cpp',
        'src/Speed/Indep/Src/Physics/HumanSteering.cpp'), False),
        compile_unit('steering-update', ('src/Speed/Indep/Src/Physics/HumanSteeringUpdate.cpp',), True)]
    globals_ = {name: int(address, 16) for name, address in re.findall(r'^(\w+) = (0x[0-9a-f]+);',
                (ROOT / 'src/Platform/Native/OriginalSymbols.ld').read_text(), re.M)}
    externals = {'_malloc': MALLOC, '_free': FREE, '__ftol2': CONVERSION[0], '__fltused': BASE + 0xff00, '__CIsqrt': CISQRT}
    externals.update(CONTROLLED)
    for obj in objects:
        for symbol in read_object(obj)['symbols'].values():
            match = re.match(r'^\?(\w+)@@', symbol['name'])
            if symbol['section'] == 0 and match and match[1] in globals_:
                externals[symbol['name']] = globals_[match[1]]
    linked = link_objects(objects, externals)
    entries = {}
    for f in manifest['functions']:
        found = [a for s, a in linked['exports'].items() if s.startswith('?' + f['method'] + '@')]
        assert len(found) == 1, (f['method'], found)
        entries[f['method']] = (f['address'], found[0])
    construct = (0x405c60, [a for s, a in linked['exports'].items() if s.startswith('?Construct@HumanSteering@')][0])
    runtime = [(CONVERSION[0], CONVERSION[0] + CONVERSION[1])]
    machines, cursors = [], [0, 0]
    for side in (0, 1):
        uc = Uc(UC_ARCH_X86, UC_MODE_32)
        uc.mem_map(0x400000, (len(image) + 4095) & ~4095)
        uc.mem_write(0x400000, image)
        uc.mem_map(0, 0x1000)  # Original steering constructor's FS:0 SEH chain.
        uc.mem_map(BASE, 0x20000)
        uc.mem_map(STOP, 0x1000)
        uc.mem_map(linked['address'], linked['allocation'])
        for address, body in linked['blocks']:
            uc.mem_write(address, body)
        uc.mem_write(CISQRT, b'\xd9\xfa\xc3')
        wrappers = {}
        for slot, method in enumerate(FLOATING):
            wrapper = WRAPPER + 0x20 * slot
            target = entries[method][side]
            uc.mem_write(wrapper, b'\xe8' + struct.pack('<i', target - wrapper - 5) + b'\xdb\x3d' + struct.pack('<I', OUT)
                         + b'\x68' + struct.pack('<I', STOP) + b'\xc3')
            wrappers[method] = wrapper
        allowed = (linked['ranges'] if side else original_ranges) + controlled_ranges + runtime + [(WRAPPER, WRAPPER + 0x100)] + ([(CISQRT, CISQRT + 3)] if side else [])
        if not side:
            allowed = allowed + constructor_ranges

        def hook(machine, address, size, user, side=side, allowed=allowed):
            if address in (MALLOC, FREE):
                sp = machine.reg_read(UC_X86_REG_ESP)
                if address == MALLOC:
                    length = struct.unpack('<I', machine.mem_read(sp + 4, 4))[0]
                    machine.reg_write(UC_X86_REG_EAX, cursors[side])
                    cursors[side] += max(16, (length + 15) & ~15)
                machine.reg_write(UC_X86_REG_ECX, 0xeeeeeeee)
                machine.reg_write(UC_X86_REG_EDX, 0xffffffff)
                machine.reg_write(UC_X86_REG_EIP, struct.unpack('<I', machine.mem_read(sp, 4))[0])
                machine.reg_write(UC_X86_REG_ESP, sp + 4)
                return
            assert any(a <= address and address + size <= b for a, b in allowed), hex(address)
        uc.hook_add(UC_HOOK_BLOCK, hook)
        machines.append((uc, wrappers))
    counts = dict.fromkeys(entries, 0)

    def run(uc, start, words, receiver):
        uc.mem_write(STACK, struct.pack('<' + 'I' * len(words), *words))
        uc.reg_write(UC_X86_REG_ECX, receiver)
        uc.reg_write(UC_X86_REG_ESP, STACK)
        uc.reg_write(UC_X86_REG_FPCW, 0x027f)
        uc.reg_write(UC_X86_REG_FPSW, 0)
        uc.reg_write(UC_X86_REG_FPTAG, 0xffff)
        for reg in SAVED:
            uc.reg_write(reg, 0x12345678)
        uc.emu_start(start, STOP, count=5000000)
        assert uc.reg_read(UC_X86_REG_EIP) == STOP
        assert all(uc.reg_read(reg) == 0x12345678 for reg in SAVED)

    def compare(method, state, globals_state, args, receiver=STEERING):
        outcomes = []
        for side, (uc, wrappers) in enumerate(machines):
            uc.mem_write(BASE, bytes(state))
            for address, data in globals_state:
                uc.mem_write(address, data)
            uc.mem_write(OUT, b'\xa5' * 16)
            words = [a & 0xffffffff for a in args]
            if method in FLOATING:
                run(uc, wrappers[method], words, receiver)
                assert uc.reg_read(UC_X86_REG_ESP) == STACK + CLEANUP[method], method
                result = bytes(uc.mem_read(OUT, 10))
            else:
                run(uc, entries[method][side], [STOP] + words, receiver)
                assert uc.reg_read(UC_X86_REG_ESP) == STACK + 4 + CLEANUP[method], method
                result = uc.reg_read(UC_X86_REG_EAX) & 0xff
            assert (uc.reg_read(UC_X86_REG_FPSW) >> 11) & 7 == 0, method
            outcomes.append((bytes(uc.mem_read(BASE, 0x10000)), result))
        assert outcomes[0] == outcomes[1], (method, counts[method], args,
                                            next((hex(BASE + i) for i, (a, b) in enumerate(zip(outcomes[0][0], outcomes[1][0])) if a != b), None),
                                            outcomes[0][1], outcomes[1][1])
        counts[method] += 1

    # Construct the steering object once with the original constructor so both
    # sides start from identical allocations, then vary car and input state.
    uc0 = machines[0][0]
    uc0.mem_write(BASE, bytes(0x10000))
    cursors[0] = BASE + 0x1000
    run(uc0, construct[0], [STOP, CAR], STEERING)
    constructed = bytearray(uc0.mem_read(BASE, 0x10000))
    rows = [row for _, row in retail_sources()]
    rng = random.Random(0x416bb0)
    nan = struct.unpack('<f', struct.pack('<I', 0x7fc00000))[0]
    modes = ((), (0x89e7d8,), (0x89e7d9,), (0x89e7e2,), (0x890104,), (0x890108,), (0x890110,), (0x890114,))
    for trial in range(1600):
        state = bytearray(constructed)
        row = rows[trial % len(rows)]
        info = INFO - BASE
        state[info + 0x40:info + 0x40 + 0x2b0] = row[:0x2b0]
        struct.pack_into('<ibxxxf', state, info + 0x10, -1, 1, 0.0)
        struct.pack_into('<ifffI', state, info + 0x10 + 0x55c, 64, 0.0, 55.5555572509765625, 1.13399994373321533203125, INFO + 0x10 + 0x45c)
        for i in range(64):
            struct.pack_into('<f', state, info + 0x10 + 0x45c + 4 * i, rng.uniform(1, 12) if i < 60 else 0.0)
        car = CAR - BASE
        speed = rng.choice((0.0, 0.5, 1.0, 5.0, rng.uniform(0, 80)))
        struct.pack_into('<I', state, car + 0x10, INFO)
        struct.pack_into('<I', state, car + 0x20, INFO + 0x40)
        struct.pack_into('<I', state, car + 0x30, INPUT)
        struct.pack_into('<I', state, car + 0x3c, BODY)
        struct.pack_into('<f', state, car + 0x42c, speed)
        struct.pack_into('<fff', state, car + 0x70, *(rng.uniform(-40, 40) for _ in range(3)))
        struct.pack_into('<H', state, car + 0x40c, rng.randrange(65536))
        struct.pack_into('<f', state, car + 0x128, rng.choice((0.0, rng.uniform(-3, 3))))
        struct.pack_into('<fff', state, INPUT - BASE + 0x20c, rng.uniform(0, 1), rng.choice((0.0, 1.0)), rng.uniform(0, 1))
        stamp = rng.choice((0, 0x7fffffff, 1000, 3000, 3900))
        struct.pack_into('<i', state, BODY - BASE + 0x140, stamp)
        struct.pack_into('<IBxxxffB', state, 0, CAR, rng.randrange(2), rng.choice((1.0, 0.5, 2.0)), 7.0, rng.randrange(2))
        struct.pack_into('<f', state, 0x20, rng.choice((0.0, 0.3, -0.8, rng.uniform(-1, 1))))
        struct.pack_into('<H', state, 0x7c, rng.choice((0, rng.randrange(65536))))
        struct.pack_into('<ff', state, 0x80, rng.choice((45.0, rng.uniform(0, 45))), rng.uniform(-1, 1))
        mode = modes[trial % len(modes)]
        globals_state = [(a, bytes([int(a in mode)])) for a in FLAGS8] + \
                        [(a, struct.pack('<i', int(a in mode))) for a in FLAGS32] + \
                        [(0x86518c, struct.pack('<i', 4000 + trial % 7))]
        # Recovery curve 0x81e5b0 is filled at runtime; give it synthetic pairs.
        pairs = [(0.0, rng.uniform(0, 1)), (0.01, rng.uniform(0, 1)), (0.03, rng.uniform(0, 1)), (2.0, 1.0)]
        for i, pair in enumerate(pairs):
            struct.pack_into('<ff', state, 0xf000 + 8 * i, *pair)
        globals_state.append((0x81e5b0, struct.pack('<Ii', BASE + 0xf000, (4, 0, 1)[trial % 3])))
        value = rng.choice((0.0, 1.0, -1.0, 0.5, rng.uniform(-1.2, 1.2), nan if trial % 97 == 0 else 0.25))
        duration = rng.choice((1 / 60, 1 / 120, 0.1))
        compare('CalculateMaximum', state, globals_state, (struct.unpack('<I', struct.pack('<f', value))[0],
                                                           struct.unpack('<I', struct.pack('<f', duration))[0], 0))
        compare('UpdateKeyboard', state, globals_state, (struct.unpack('<I', struct.pack('<f', value))[0],
                                                         struct.unpack('<I', struct.pack('<f', duration))[0], trial % 3))
        compare('UpdateAnalog', state, globals_state, (struct.unpack('<I', struct.pack('<f', value))[0],
                                                       struct.unpack('<I', struct.pack('<f', duration))[0], 0))
        compare('GetSteeringSlip', state, globals_state, (), CAR + 0x40)
    for device in range(-1, 9):
        for present in (0, 1):
            for flag in (0, 1, 0x80):
                state = bytearray(constructed)
                globals_state = [(0x86ffb4 + 4 * device, struct.pack('<I', present * 0x1234)), (0x86e5bc + device, bytes([flag]))]
                compare('IsAnalogInputDevice', state, globals_state, (device,))
    report = dict(executable_sha256=manifest['exe_sha256'], bit_identical_cases=counts,
                  byte_matching='not established',
                  controlled_interfaces=['0x591d90 slip angle and 0x5b5930 physics speed execute as original code on both sides (verified separately)',
                                         'malloc 0x75c6c8/free 0x75c6da for the original constructor',
                                         'analysis-only fsqrt helper for MSVC /Op __CIsqrt (original uses inline fsqrt)'],
                  limitations=['Synthetic car/input states on 46 retail records; physics speed uses a synthetic drive curve with a set class index.',
                               'Recovery curve 0x81e5b0 (runtime-filled) uses synthetic pairs with counts 4/0/1.',
                               'The yaw feedback curve leaves its output unchanged when empty; the original then reads a stale stack slot.',
                               'Only 53-bit x87 precision.'])
    (BUILD / 'verification.json').write_text(json.dumps(report, indent=2) + '\n')
    print(json.dumps(report, indent=2))


if __name__ == '__main__':
    main()
