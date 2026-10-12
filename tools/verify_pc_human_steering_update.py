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
              '?GetPhysicsSpeed@PhysicsParameters@@QAEMXZ': 0x5b5930,
              '?WheelLength@@YAMPAUWheelVector@@@Z': 0x440f70,
              '?bIsSlotPoolFull@@YAHPAX@Z': 0x43cd10, '?bSlotPoolAllocate@@YAPAXPAX@Z': 0x43f8f0,
              '?Normalize2@@YAXPAM0@Z': 0x43ce50}
# Unrecovered force-feedback manager methods: hooked on both sides, which
# record each call and return scripted answers. Values are argument counts.
FF_METHODS = {0x5bfd70: 1, 0x5bfd90: 2, 0x5bfe00: 1, 0x5bfe70: 1, 0x5c9150: 4, 0x5c9040: 2, 0x5c9550: 3,
              0x5c9590: 2, 0x5c9760: 2, 0x5c9d90: 2, 0x5c9fd0: 4, 0x5cd850: 1, 0x5cd910: 1, 0x5cd9d0: 1,
              0x5cda10: 1, 0x5cdbb0: 1,
              0x407a30: 1}  # pedals 0x407a30 (verified separately) are traced, not executed
FF_MANAGER = 0x2100f00
FLOATING = ('CalculateMaximum', 'UpdateKeyboard', 'UpdateAnalog', 'GetSteeringSlip', 'UpdateAlternate', 'Invert', 'bDistance')
CLEANUP = dict(CalculateMaximum=12, UpdateKeyboard=12, UpdateAnalog=12, GetSteeringSlip=0, IsAnalogInputDevice=0,
               UpdateAlternate=12, Invert=4, UpdateForceFeedback=8, GetBlendedBasis=4, QueryFlag5EB420=0, Query4018F0=0,
               **{'Update@HumanSteering': 12, 'Update@HumanDriverInput': 4, 'ClearActions': 0, 'CanUseWheelControl': 4,
                  'UpdateWheelControl': 4, 'IsInputDeviceAssigned': 0, 'SetClutchInput': 4, 'HasPendingShift': 0,
                  'GetWheelState': 4, 'Query401950': 0, 'UpdateColliderFaceRecords': 0, 'TrackColliderFace': 0,
                  'Initialise@ColliderFaceRecord': 8, 'HumanInputHook4022C0': 0, 'UpdateLength': 0, 'FindIndex': 12,
                  'RemoveFront': 8, 'FindPointAhead': 8, 'Advance@InputRoute': 8, 'bDistance': 0, 'GetPosition': 0, 'GetVelocity': 0,
                  'ConstructBase': 0, 'ConstructHuman': 4})
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

    # 0x41e2c0 is absent from the static call graph; add its direct callees.
    original_ranges = ranges_for([f['address'] for f in manifest['functions']] + [0x5eb8f0])
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
        'src/Speed/Indep/Src/Physics/HumanSteering.cpp', 'src/Speed/Indep/Src/Physics/EngineMath.cpp',
        'src/Speed/Indep/Src/Physics/MoverInput.cpp'), False),
        compile_unit('curve-invert', ('src/Speed/Indep/Src/Physics/EngineCurveInvert.cpp',), True),
        compile_unit('steering-update', ('src/Speed/Indep/Src/Physics/HumanSteeringUpdate.cpp',
                                         'src/Speed/Indep/Src/Physics/HumanSteeringFeedback.cpp',
                                         'src/Speed/Indep/Src/Physics/HumanDriverUpdate.cpp',
                                         'src/Speed/Indep/Src/Physics/DrivetrainClutchInput.cpp',
                                         'src/Speed/Indep/Src/World/WColliderFaceRecords.cpp',
                                         'src/Speed/Indep/Src/Physics/MoverInputRoute.cpp'), True)]
    externals_extra = {'?UpdatePedals@HumanDriverInput@@QAEXM@Z': 0x407a30}
    globals_ = {name: int(address, 16) for name, address in re.findall(r'^(\w+) = (0x[0-9a-f]+);',
                (ROOT / 'src/Platform/Native/OriginalSymbols.ld').read_text(), re.M)}
    externals = {'_malloc': MALLOC, '_free': FREE, '__ftol2': CONVERSION[0], '__fltused': BASE + 0xff00, '__CIsqrt': CISQRT}
    externals.update(CONTROLLED)
    externals.update(externals_extra)
    for obj in objects:
        for symbol in read_object(obj)['symbols'].values():
            match = re.match(r'^\?(\w+)@@', symbol['name'])
            if symbol['section'] == 0 and match and match[1] in globals_:
                externals[symbol['name']] = globals_[match[1]]
            ff = re.match(r'^\?\w+([0-9A-F]{6})@ForceFeedbackManager@@', symbol['name'])
            if symbol['section'] == 0 and ff:
                externals[symbol['name']] = int(ff[1], 16)
    linked = link_objects(objects, externals)
    entries = {}
    for f in manifest['functions']:
        found = [a for s, a in linked['exports'].items() if s.startswith('?' + f['method'] + '@')]
        assert len(found) == 1, (f['method'], found)
        entries[f['method']] = (f['address'], found[0])
    construct = (0x405c60, [a for s, a in linked['exports'].items() if s.startswith('?Construct@HumanSteering@')][0])
    runtime = [(CONVERSION[0], CONVERSION[0] + CONVERSION[1])]
    machines, cursors = [], [0, 0]
    traces, answers = [[], []], {}
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
            if address in FF_METHODS:
                sp = machine.reg_read(UC_X86_REG_ESP)
                count = FF_METHODS[address]
                args = struct.unpack('<%dI' % count, machine.mem_read(sp + 4, 4 * count))
                traces[side].append((hex(address), hex(machine.reg_read(UC_X86_REG_ECX)), args))
                machine.reg_write(UC_X86_REG_EAX, answers.get((address, args[1] if count > 1 else None), answers.get(address, 0)))
                machine.reg_write(UC_X86_REG_ECX, 0xeeeeeeee)
                machine.reg_write(UC_X86_REG_EDX, 0xffffffff)
                machine.reg_write(UC_X86_REG_EIP, struct.unpack('<I', machine.mem_read(sp, 4))[0])
                machine.reg_write(UC_X86_REG_ESP, sp + 4 + 4 * count)
                return
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

    def compare(method, state, globals_state, args, receiver=STEERING, mask=None):
        outcomes = []
        traces[0][:], traces[1][:] = [], []
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
                widths = {'QueryFlag5EB420': 0xffffffff, 'IsInputDeviceAssigned': 0xffffffff, 'GetWheelState': 0xffffffff,
                          'FindIndex': 0xffffffff, 'FindPointAhead': 0xffffffff, 'GetPosition': 0xffffffff,
                          'GetVelocity': 0xffffffff, 'ConstructBase': 0xffffffff, 'ConstructHuman': 0xffffffff,
                          'Update@HumanSteering': 0xffff, 'Initialise@ColliderFaceRecord': 0xffffffff}
                result = uc.reg_read(UC_X86_REG_EAX) & widths.get(method, 0xff)
                if method in ('UpdateForceFeedback', 'GetBlendedBasis', 'Update@HumanDriverInput', 'ClearActions',
                              'UpdateWheelControl', 'SetClutchInput', 'UpdateColliderFaceRecords', 'TrackColliderFace',
                              'HumanInputHook4022C0', 'UpdateLength', 'RemoveFront', 'Advance@InputRoute'):
                    result = None
            assert (uc.reg_read(UC_X86_REG_FPSW) >> 11) & 7 == 0, method
            memory = bytearray(uc.mem_read(BASE, 0x10000))
            if mask is not None:
                memory[mask - BASE:mask - BASE + 4] = b'\0\0\0\0'  # vtable identity write (native C++)
            outcomes.append((bytes(memory), result))
        assert traces[0] == traces[1], (method, counts[method], traces)
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
        compare('UpdateAlternate', state, globals_state, (struct.unpack('<I', struct.pack('<f', value))[0],
                                                          struct.unpack('<I', struct.pack('<f', duration))[0], trial % 3))
    # Inverse lookup on the original curves and synthetic monotonic,
    # decreasing, plateaued and single-sample curves.
    curves = [0x7e9e60, 0x7e9eb0, 0x7e9ed8, 0x7e9f00, 0x7e9fb4, 0x7e9fdc]
    synthetic = [[0.0, 0.1, 0.4, 0.9, 1.0], [1.0, 0.5, 0.5, 0.2, 0.0], [0.2, 0.2, 0.2], [0.5], [0.0, 1.0, 0.0, 1.0]]
    for k, samples in enumerate(synthetic):
        curve = BASE + 0xe000 + k * 0x80
        curves.append(curve)
        state_extra = struct.pack('<ifffI', len(samples), -1.0 + k, 3.0, (len(samples) - 1) / 4.0 or 1.0, curve + 0x20) + bytes(12) + \
            b''.join(struct.pack('<f', v) for v in samples)
        constructed[curve - BASE:curve - BASE + len(state_extra)] = state_extra
    for curve in curves:
        for value in [i / 16.0 for i in range(-4, 21)] + [0.1000001, 0.0999999, 1e-7, -0.0, nan, 1e6]:
            compare('Invert', constructed, [], (struct.unpack('<I', struct.pack('<f', value))[0],), curve)
    # Force feedback: every guard, both stop branches, torque/spring/jolt/load/
    # surface/rumble paths with scripted manager answers.
    rng = random.Random(0x406460)
    ff_cases = 0
    for trial in range(3000):
        state = bytearray(constructed)
        car = CAR - BASE
        mover, wheel0, wheel1, entry, flow = 0xc000, 0xc400, 0xc800, 0xcc00, 0xcd00
        struct.pack_into('<i', state, car + 0x28, (1, 1, 1, 1, 0, 2)[trial % 6])
        struct.pack_into('<I', state, car + 0x18, 0 if trial % 23 == 0 else BASE + entry)
        struct.pack_into('<I', state, car + 0x34, BASE + mover)
        struct.pack_into('<II', state, mover + 0x38, BASE + wheel0, BASE + wheel1)
        for wheel in (wheel0, wheel1):
            struct.pack_into('<fff', state, wheel + 0xb0, *(rng.uniform(-2, 2) for _ in range(3)))
            struct.pack_into('<fff', state, wheel + 0xc0, *(rng.uniform(-2, 2) for _ in range(3)))
            struct.pack_into('<ff', state, wheel + 0x124, rng.choice((0.0, rng.uniform(-900, 900))), rng.choice((0.0, rng.uniform(-900, 900))))
        struct.pack_into('<fff', state, car + 0x70, *(rng.choice((0.0, rng.uniform(-60, 60))) for _ in range(3)))
        struct.pack_into('<ffff', state, car + 0x370, *(rng.choice((0.0, rng.uniform(0, 0.3))) for _ in range(4)))
        struct.pack_into('<BB', state, car + 0x390, rng.randrange(2), rng.randrange(2))
        struct.pack_into('<ii', state, car + 0x394, rng.choice((0, 8, 9, 10, 11, 12, 13, 15, 16, 17)), rng.choice((0, 8, 9, 10, 11, 13, 15, 16, 17, 18)))
        index = rng.randrange(4)
        struct.pack_into('<i', state, entry + 0x10, index)
        struct.pack_into('<i', state, flow, (4, 4, 4, 3)[trial % 4])
        struct.pack_into('<ff', state, 0x08, 1.0, rng.choice((7.0, 3.5)))
        device = rng.randrange(4)
        answers.clear()
        answers[0x5bfd70] = int(trial % 17 != 0)
        answers[0x5bfd90] = rng.randrange(2)
        answers[0x5c9040] = rng.randrange(2)
        globals_state = [(0x870760, struct.pack('<I', 0 if trial % 29 == 0 else FF_MANAGER)),
                         (0x8900d8, struct.pack('<I', 0 if trial % 31 == 0 else BASE + flow)),
                         (0x83aa68 + index * 0x28, bytes([int(trial % 5 != 0)])),
                         (0x89e7a0 + 0x38, bytes([int(trial % 7 == 0)])), (0x890108, struct.pack('<i', int(trial % 11 == 0))),
                         (0x86518c, struct.pack('<i', 1000 + trial * 37)),
                         (0x827aac + device * 4, struct.pack('<f', rng.choice((0.0, rng.uniform(0, 80))))),
                         (0x827aa4 + device * 4, struct.pack('<f', rng.choice((0.0, rng.uniform(0, 30), 1e6))))]
        value = rng.choice((1.0, -1.0, 0.5, 0.0, rng.uniform(-1, 1)))
        compare('UpdateForceFeedback', state, globals_state, (struct.unpack('<I', struct.pack('<f', value))[0], device))
        compare('GetBlendedBasis', state, globals_state, (BASE + 0xce00,), BASE + wheel0)
        compare('QueryFlag5EB420', state, globals_state, (), BASE + entry)
        compare('Query4018F0', state, globals_state, (), 0x89e7a0)
    # Human input update chain. A second steering object lives at input
    # +0x220, built with the original constructor like the first.
    HUMAN = BASE + 0x4000
    uc0.mem_write(BASE, bytes(constructed))
    cursors[0] = BASE + 0x6000
    run(uc0, construct[0], [STOP, CAR], HUMAN + 0x220)
    human_state = bytearray(uc0.mem_read(BASE, 0x10000))
    rng = random.Random(0x41e2c0)
    for trial in range(2500):
        state = bytearray(human_state)
        row = rows[trial % len(rows)]
        info = INFO - BASE
        state[info + 0x40:info + 0x40 + 0x2b0] = row[:0x2b0]
        struct.pack_into('<ibxxxf', state, info + 0x10, -1, 1, 0.0)
        struct.pack_into('<ifffI', state, info + 0x10 + 0x55c, 64, 0.0, 55.5555572509765625, 1.13399994373321533203125, INFO + 0x10 + 0x45c)
        for i in range(64):
            struct.pack_into('<f', state, info + 0x10 + 0x45c + 4 * i, rng.uniform(1, 12) if i < 60 else 0.0)
        car = CAR - BASE
        mover, drive = 0xc000, 0xc600
        struct.pack_into('<I', state, car + 0x10, INFO)
        struct.pack_into('<I', state, car + 0x20, INFO + 0x40)
        struct.pack_into('<I', state, car + 0x30, INPUT)
        struct.pack_into('<I', state, car + 0x3c, BODY)
        struct.pack_into('<i', state, car + 0x28, (1, 1, 1, 0)[trial % 4])
        struct.pack_into('<I', state, car + 0x18, 0)
        struct.pack_into('<I', state, car + 0x34, BASE + mover)
        struct.pack_into('<I', state, mover + 0x4c, BASE + drive)
        struct.pack_into('<4i', state, mover + 0x74, *(rng.choice((0, 1, 2)) for _ in range(4)))
        struct.pack_into('<f', state, drive + 0x14, rng.uniform(0, 1))
        struct.pack_into('<i', state, drive + 0x48, rng.randrange(2))
        struct.pack_into('<f', state, car + 0x42c, rng.choice((0.0, rng.uniform(0, 70))))
        struct.pack_into('<f', state, car + 0x430, rng.choice((0.0, rng.uniform(0, 20))))
        struct.pack_into('<i', state, car + 0x434, rng.randrange(-1, 6))
        struct.pack_into('<fff', state, car + 0x70, *(rng.uniform(-40, 40) for _ in range(3)))
        struct.pack_into('<i', state, car + 0x5c4 + 0x2e0, 0)
        struct.pack_into('<i', state, BODY - BASE + 0x140, rng.choice((0, 0x7fffffff, 3990)))
        struct.pack_into('<fff', state, INPUT - BASE + 0x20c, rng.uniform(0, 1), 0.0, rng.uniform(0, 1))
        h = HUMAN - BASE
        struct.pack_into('<H', state, h + 0x1e0, rng.randrange(65536))
        struct.pack_into('<ii', state, h + 0x1e4, rng.randrange(-1, 6), 1)
        struct.pack_into('<I', state, h + 0x2a8, CAR)
        struct.pack_into('<i', state, h + 0x2ac, (-1, 0, 1, 2)[trial % 4])
        struct.pack_into('<B', state, h + 0x2b4, int(trial % 9 == 0))
        struct.pack_into('<i', state, h + 0x2b8, int(trial % 13 == 0))
        struct.pack_into('<i', state, h + 0x2c0, rng.choice((0, 3, 4)))
        struct.pack_into('<f', state, h + 0x2c8, rng.choice((0.0, 1.0, -1.0, rng.uniform(-1, 1))))
        struct.pack_into('<fff', state, h + 0x2d4, rng.uniform(0, 1), 0.0, rng.uniform(0, 1))
        struct.pack_into('<f', state, h + 0x2ec, rng.choice((0.0, 0.5, 1.0)))
        struct.pack_into('<f', state, h + 0x2f4, rng.choice((1.0, 0.0, 0.5, 0.97, 0.02, rng.uniform(0, 1))))
        struct.pack_into('<BBBBb', state, h + 0x2f8, int(trial % 5 != 0), rng.randrange(2), rng.choice((0, 0xaa, 1)),
                         rng.choice((0, 1, 2)), rng.choice((0, 0, 1, -1)))
        struct.pack_into('<i', state, h + 0x300, rng.choice((0, 0x7fffffff, 1000, 3990)))
        struct.pack_into('<4f', state, h + 0x304, *(rng.choice((0.0, 0.9, -0.9, 0.5, rng.uniform(-1, 1))) for _ in range(4)))
        mode = modes[trial % len(modes)]
        globals_state = [(a, bytes([int(a in mode)])) for a in FLAGS8] + \
                        [(a, struct.pack('<i', int(a in mode))) for a in FLAGS32] + \
                        [(0x86518c, struct.pack('<i', 4000 + trial % 7)),
                         (0x864788, bytes((0, 0xff, rng.choice((0, 0xff))))),
                         (0x81cb8c, struct.pack('<i', int(trial % 11 == 0))),
                         (0x89e7a0 + 0x38, bytes((int(trial % 7 == 0), rng.randrange(2)))),
                         (0x89e7e3, bytes((int(trial % 17 == 0),))), (0x89010c, struct.pack('<i', int(trial % 19 == 0))),
                         (0x7e94a0, bytes((int(trial % 3 != 0),)))]
        pairs = [(0.0, 0.3), (0.01, 0.6), (0.03, 0.9), (2.0, 1.0)]
        for i, pair in enumerate(pairs):
            struct.pack_into('<ff', state, 0xf000 + 8 * i, *pair)
        globals_state.append((0x81e5b0, struct.pack('<Ii', BASE + 0xf000, 4)))
        duration = rng.choice((1 / 60, 1 / 120, 0.1))
        compare('Update@HumanDriverInput', state, globals_state, (struct.unpack('<I', struct.pack('<f', duration))[0],), HUMAN)
        compare('Update@HumanSteering', state, globals_state, (struct.unpack('<I', struct.pack('<f', rng.uniform(-1, 1)))[0],
                struct.unpack('<I', struct.pack('<f', duration))[0], rng.randrange(3)), HUMAN + 0x220)
        compare('ClearActions', state, globals_state, (), HUMAN)
        compare('CanUseWheelControl', state, globals_state, (rng.choice((0, 1, 0x100)),), HUMAN)
        compare('UpdateWheelControl', state, globals_state, (0,), HUMAN)
        compare('HasPendingShift', state, globals_state, (), CAR)
        compare('GetWheelState', state, globals_state, (rng.randrange(4),), CAR)
        compare('Query401950', state, globals_state, (), 0x89e7a0)
        compare('HumanInputHook4022C0', state, globals_state, (CAR,))
        compare('IsInputDeviceAssigned', state, globals_state, (rng.randrange(3),))
    # Clutch input boundaries, rising/falling and NaN.
    for previous in (0.0, 0.5, 1.0, nan):
        for value in (0.0, -0.0, 0.0499999, 0.05, 0.0500001, 0.5, 0.94999, 0.95, 0.950001, 1.0, 1.5, -1.0, nan, 0.4, 0.6):
            state = bytearray(constructed)
            struct.pack_into('<f', state, 0xc600 + 0x14, previous)
            compare('SetClutchInput', state, [], (struct.unpack('<I', struct.pack('<f', value))[0],), BASE + 0xc600)
    # Collider face records with an original-layout slot pool and list.
    for trial in range(400):
        state = bytearray(constructed)
        pool, faces, records = 0xd000, 0xd100, 0xe800
        count = rng.choice((0, 1, 3, 6))
        capacity = rng.choice((2, 4, 8))
        free = [BASE + records + i * 0xc0 for i in range(capacity)]
        for i, node in enumerate(free):
            struct.pack_into('<I', state, node - BASE, free[i + 1] if i + 1 < capacity else 0)
        struct.pack_into('<IIii', state, pool + 0x10, free[0], 0, rng.randrange(0, 2), 0)
        struct.pack_into('<i', state, pool + 0x2c, capacity)
        for i in range(count):
            face = faces + i * 0x80
            struct.pack_into('<BBBb', state, face + 8, 0, 4, rng.choice((0, 2, 4, 6, 8, 10)), 0)
            struct.pack_into('<I', state, face + 0xc, rng.choice((0, 0x1000 + i, 0x1001)))
            for v in range(20):
                struct.pack_into('<f', state, face + 0x30 + 4 * v, rng.uniform(-100, 100))
            struct.pack_into('<I', state, car + 0x5c4 + 0x3c + 4 * i, BASE + face)
        struct.pack_into('<i', state, car + 0x5c4 + 0x2e0, count)
        globals_state = [(0x88eadc, struct.pack('<II', 0x88eadc, 0x88eadc)),
                         (0x883e5c, struct.pack('<I', 0 if trial % 9 == 0 else BASE + pool)),
                         (0x883e60, struct.pack('<i', int(trial % 11 != 0))),
                         (0x802040, struct.pack('<i', trial % 2)), (0x7fb720, struct.pack('<i', trial))]
        compare('UpdateColliderFaceRecords', state, globals_state, (CAR + 0x5c4, CAR + 0x60))
    # Route trimming/lookahead and the input constructors.
    rng = random.Random(0x41d160)
    ROUTE = BASE + 0x5000
    for trial in range(2500):
        state = bytearray(constructed)
        r = ROUTE - BASE
        count = rng.choice((0, 1, 2, 5, 12, 20))
        struct.pack_into('<bB', state, r, count, rng.randrange(2))
        x, y = rng.uniform(-50, 50), rng.uniform(-50, 50)
        for i in range(20):
            x += rng.uniform(-3, 12)
            y += rng.uniform(-3, 12)
            struct.pack_into('<fff', state, r + 0x10 + 16 * i, x, y, rng.uniform(-2, 2))
            struct.pack_into('<hh', state, r + 0x150 + 2 * i, *(rng.randrange(-30000, 30000) for _ in range(2)))
            struct.pack_into('<h', state, r + 0x178 + 2 * i, rng.randrange(-30000, 30000))
            state[r + 0x1a0 + i] = rng.randrange(256)
        struct.pack_into('<bxxxiiif', state, r + 0x1b4, rng.randrange(-2, 20), rng.choice((0, 4001, 4002)), 7, 9, 3.0)
        struct.pack_into('<BB', state, r + 0x1c8, rng.randrange(2), int(trial % 4 != 0))
        car = CAR - BASE
        struct.pack_into('<f', state, car + 0x42c, rng.choice((0.0, 10.0, 30.0, 45.0, rng.uniform(0, 80))))
        struct.pack_into('<fff', state, car + 0x60, rng.uniform(-60, 60), rng.uniform(-60, 60), 0.0)
        struct.pack_into('<fff', state, car + 0x70, *((0.0, 0.0, 0.0) if trial % 7 == 0 else
                         (rng.choice((0.0, rng.uniform(-30, 30))), rng.uniform(-30, 30), 0.0)))
        globals_state = [(0x86518c, struct.pack('<i', 4001 + trial % 3)), (0x7837c4, struct.pack('<f', rng.choice((0.7, 0.0, -0.3)))),
                         (0x7837c8, struct.pack('<f', rng.choice((0.3, 0.0)))), (0x827a20, struct.pack('<f', rng.choice((0.0, 4.0, 100.0))))]
        distance = rng.choice((0.0, -1.0, 0.0005, 0.002, 5.0, 20.0, 60.0, rng.uniform(0, 100)))
        dbits = struct.unpack('<I', struct.pack('<f', distance))[0]
        compare('Advance@InputRoute', state, globals_state, (CAR, dbits), ROUTE)
        compare('FindPointAhead', state, globals_state, (CAR, dbits), ROUTE)
        compare('FindIndex', state, globals_state, (struct.unpack('<I', struct.pack('<f', rng.uniform(0, 60)))[0], CAR + 0x60, CAR + 0x70), ROUTE)
        compare('RemoveFront', state, globals_state, (rng.randrange(0, 22), rng.choice((0, 1, 0x100))), ROUTE)
        compare('UpdateLength', state, globals_state, (), ROUTE)
        compare('bDistance', state, globals_state, (ROUTE + 0x10, ROUTE + 0x20))
        compare('GetPosition', state, globals_state, (), CAR)
        compare('GetVelocity', state, globals_state, (), CAR)
    # Constructors: original vtable identity writes at +0 are masked.
    INPUT2 = BASE + 0x5800
    for trial in range(60):
        state = bytearray(constructed)
        state[INPUT2 - BASE:INPUT2 - BASE + 0x314] = bytes(rng.randrange(256) for _ in range(0x314))
        race = rng.choice((0, BASE + 0xef00))
        state[0xef00:0xef04] = struct.pack('<i', rng.choice((0, 4, 3)))
        globals_state = [(0x890118, struct.pack('<I', race)), (0x86518c, struct.pack('<i', 777 + trial))]
        cursors[0] = cursors[1] = BASE + 0x7000
        compare('ConstructHuman', state, globals_state, (CAR,), INPUT2, mask=INPUT2)
        cursors[0] = cursors[1] = BASE + 0x7000
        compare('ConstructBase', state, globals_state, (), INPUT2, mask=INPUT2)
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
                                         'analysis-only fsqrt helper for MSVC /Op __CIsqrt (original uses inline fsqrt)',
                                         '0x440f70 vector length runs as original code; force-feedback manager methods are hooked, traced and scripted'],
                  limitations=['Synthetic car/input states on 46 retail records; physics speed uses a synthetic drive curve with a set class index.',
                               'Recovery curve 0x81e5b0 (runtime-filled) uses synthetic pairs with counts 4/0/1.',
                               'The yaw feedback curve leaves its output unchanged when empty; the original then reads a stale stack slot.',
                               'Only 53-bit x87 precision.'])
    (BUILD / 'verification.json').write_text(json.dumps(report, indent=2) + '\n')
    print(json.dumps(report, indent=2))


if __name__ == '__main__':
    main()
