#!/usr/bin/env python3
"""Bounded original/rebuilt driver pedals and car frame-kinematics comparisons.

Uses real recovered setters, forward selection and replay predicate. Only the
event dispatcher is a controlled boundary. Kinematics are prefixes, stopped
before unrecovered world/player logic; this does not verify whole car updates.
"""
import hashlib
import itertools
import json
import math
import random
import re
import struct
import subprocess

import pefile
from unicorn import Uc, UC_ARCH_X86, UC_MODE_32, UC_HOOK_CODE
from unicorn.x86_const import *
from pc_coff import link_objects, read_object
from probe_pc_race_reward import ROOT, EXE
from verify_pc_engine import CONVERSION

BUILD = ROOT / 'build/analysis/demo-recovery'
ARENA, INPUT, CAR, RECORD, FLOW, VTABLE = 0x2000000, 0x2000000, 0x2001000, 0x2003000, 0x2004000, 0x2005000
STACK, STOP = 0x200f000, 0x2100000


def main():
    manifest = json.loads((ROOT / 'config/SPEED2_EXE_1_2/demo-driving.json').read_text())
    raw = EXE.read_bytes()
    assert hashlib.sha256(raw).hexdigest() == manifest['exe_sha256']
    image = pefile.PE(data=raw).get_memory_mapped_image()
    for f in manifest['functions']:
        body = image[f['address'] - 0x400000:f['address'] - 0x400000 + f['size']]
        assert hashlib.sha256(body).hexdigest() == f['sha256'], f['method']
    assert hashlib.sha256(image[CONVERSION[0] - 0x400000:CONVERSION[0] - 0x400000 + CONVERSION[1]]).hexdigest() == CONVERSION[2]
    BUILD.mkdir(parents=True, exist_ok=True)
    # A unity analysis unit avoids duplicate MSVC inline COMDAT exports in the
    # deliberately small COFF analysis loader; native production units stay split.
    unity = BUILD / 'driver-analysis.cpp'
    unity.write_text(''.join('#include "' + str(ROOT / 'src/Speed/Indep/Src/Physics' / (unit + '.cpp')) + '"\n'
                             for unit in ('HumanDriverInput', 'MoverInput', 'ReplayState', 'CarFrameKinematics', 'EngineCurve')))
    path = BUILD / 'driver-analysis.obj'
    subprocess.run([str(ROOT / 'build/tools/wibo'), 'build/compilers/Win32/7.1/cl.exe',
                    '/nologo', '/c', '/Ox', '/Ob0', '/Z7', '/EHsc',
                    '/Fo' + str(path.relative_to(ROOT)), str(unity.relative_to(ROOT))], cwd=ROOT, check=True)
    objects = [path]
    globals_ = {n: int(a, 16) for n, a in re.findall(r'^(\w+) = (0x[0-9a-f]+);',
                (ROOT / 'src/Platform/Native/OriginalSymbols.ld').read_text(), re.M)}
    # MSVC's floating-point marker is data, never an executed runtime boundary.
    externals = {'__ftol2': CONVERSION[0], '__fltused': 0x2006000}
    defined = {s['name'] for p in objects for s in read_object(p)['symbols'].values() if s['section'] > 0}
    for p in objects:
        for s in read_object(p)['symbols'].values():
            n = s['name']
            if s['section'] != 0 or s['storage'] != 2 or n in defined or n in externals:
                continue
            name = n[1:].split('@@')[0] if n.startswith('?') else n
            if name in globals_:
                externals[n] = globals_[name]
            elif n.startswith('?ControlEvent@DrivetrainContext@@'):
                externals[n] = 0x578e90
            else:
                raise AssertionError('Unresolved external: ' + n)
    linked = link_objects(objects, externals)
    methods = {}
    for f in manifest['functions']:
        if f['kind'] == 'dependency':
            continue
        prefix = '?' + f['method'] + '@'
        matches = [a for n, a in linked['exports'].items() if n.startswith(prefix)]
        assert len(matches) == 1, (prefix, matches)
        methods[f['method']] = (f, matches[0])
    original_calls = {0x5eb8f0: ('brake', 4), 0x5eb900: ('handbrake', 4),
                      0x5eb910: ('throttle', 4), 0x577310: ('timer', 4),
                      0x5764d0: ('curve', 4), 0x407310: ('forward', 0),
                      0x5f00f0: ('replay', 0)}
    rebuilt_calls = {}
    for tag, prefix in (('brake', '?SetBrake@'), ('handbrake', '?SetHandbrake@'),
                        ('throttle', '?SetThrottle@'), ('timer', '?Advance@DriverTimer@'),
                        ('curve', '?Lookup@EngineCurve@'), ('forward', '?SelectForward@'),
                        ('replay', '?QueryState5F00F0@')):
        entries = [address for name, address in linked['exports'].items() if name.startswith(prefix)]
        assert len(entries) == 1, (prefix, entries)
        rebuilt_calls[entries[0]] = (tag, 4 if tag in ('brake', 'handbrake', 'throttle', 'timer', 'curve') else 0)
    ranges = [(f['address'], f['address'] + f['size']) for f in manifest['functions']]
    ranges += [(CONVERSION[0], CONVERSION[0] + CONVERSION[1])]
    machines = []
    traces = [[], []]
    prefix_stop = [None, None]
    for rebuilt in (False, True):
        uc = Uc(UC_ARCH_X86, UC_MODE_32)
        uc.mem_map(0x400000, (len(image) + 4095) & ~4095)
        uc.mem_write(0x400000, image)
        uc.mem_map(ARENA, 0x10000)
        uc.mem_map(STOP, 0x1000)
        uc.mem_map(linked['address'], linked['allocation'])
        for a, b in linked['blocks']:
            uc.mem_write(a, b)
        side = int(rebuilt)

        def hook(machine, address, size, user, side=side):
            if address == prefix_stop[side]:
                machine.emu_stop()
                return
            calls = rebuilt_calls if side else original_calls
            if address in calls:
                tag, argument_size = calls[address]
                sp = machine.reg_read(UC_X86_REG_ESP)
                traces[side].append((tag, machine.reg_read(UC_X86_REG_ECX),
                                     bytes(machine.mem_read(sp + 4, argument_size))))
            if address == 0x578e90:
                sp = machine.reg_read(UC_X86_REG_ESP)
                traces[side].append((machine.reg_read(UC_X86_REG_ECX),) + struct.unpack('<II', machine.mem_read(sp + 4, 8)))
                machine.reg_write(UC_X86_REG_EAX, 0xaabbccdd)
                machine.reg_write(UC_X86_REG_ECX, 0xeeeeeeee)
                machine.reg_write(UC_X86_REG_EDX, 0xffffffff)
                machine.reg_write(UC_X86_REG_EIP, struct.unpack('<I', machine.mem_read(sp, 4))[0])
                machine.reg_write(UC_X86_REG_ESP, sp + 12)
                return
            allowed = linked['ranges'] + [(CONVERSION[0], CONVERSION[0] + CONVERSION[1])] if side else ranges
            assert any(a <= address < b for a, b in allowed), hex(address)
        uc.hook_add(UC_HOOK_CODE, hook)
        machines.append(uc)

    float_fields = {INPUT + o for o in (0x20c, 0x210, 0x214, 0x2bc)}
    float_fields |= {CAR + o for o in (0x438, 0x80, 0x84, 0x88, 0x120, 0x124, 0x128)}
    counts, bit_exact = {}, {}
    rng = random.Random(0x407a30)

    def compare(method, state, duration=None, argument=None, replay=(0, 0, 0), flags=(), frame=7, flow=4):
        f, rebuilt_entry = methods[method]
        results, returns = [], []
        for side, uc in enumerate(machines):
            uc.mem_write(ARENA, bytes(state))
            uc.mem_write(0x89cf69, bytes([replay[0]]))
            uc.mem_write(0x8669f4, struct.pack('<i', replay[1]))
            uc.mem_write(0x866a14, struct.pack('<i', replay[2]))
            for a in (0x89e7d8, 0x89e7d9, 0x89e7e2, 0x89e7e3):
                uc.mem_write(a, bytes([int(a in flags)]))
            for a in (0x890100, 0x890104, 0x890108, 0x89010c, 0x890110, 0x890114):
                uc.mem_write(a, struct.pack('<i', int(a in flags)))
            uc.mem_write(0x7fb724, struct.pack('<i', frame))
            uc.mem_write(0x8900d8, struct.pack('<I', FLOW))
            uc.mem_write(FLOW, struct.pack('<i', flow))
            # The actual human-input callback executes recovered forward selection.
            uc.mem_write(VTABLE + 0x30, struct.pack('<I', methods['SelectForward'][1] if side else 0x407310))
            uc.mem_write(0x7fbed8, image[0x7fbed8 - 0x400000:0x7fbed8 - 0x400000 + 4])
            args = b'' if duration is None else struct.pack('<f', duration)
            if argument is not None:
                args = struct.pack('<i', argument)
            uc.mem_write(STACK, struct.pack('<I', STOP) + args)
            uc.reg_write(UC_X86_REG_ESP, STACK)
            receiver = CAR if f['kind'] == 'prefix' else 0x89cf48 if method == 'QueryState5F00F0' else INPUT
            uc.reg_write(UC_X86_REG_ECX, receiver)
            for reg in (UC_X86_REG_EBX, UC_X86_REG_ESI, UC_X86_REG_EDI, UC_X86_REG_EBP):
                uc.reg_write(reg, 0x12345678)
            uc.reg_write(UC_X86_REG_FPCW, 0x027f)
            traces[side] = []
            prefix_stop[side] = f['address'] + f['size'] if f['kind'] == 'prefix' and not side else None
            uc.emu_start(rebuilt_entry if side else f['address'], STOP, count=10000)
            if f['kind'] != 'prefix' or side:
                assert uc.reg_read(UC_X86_REG_EIP) == STOP
                assert uc.reg_read(UC_X86_REG_ESP) == STACK + 4 + len(args)
                for reg in (UC_X86_REG_EBX, UC_X86_REG_ESI, UC_X86_REG_EDI, UC_X86_REG_EBP):
                    assert uc.reg_read(reg) == 0x12345678
            results.append(bytes(uc.mem_read(ARENA, 0x4000)))
            returns.append(uc.reg_read(UC_X86_REG_EAX))
        counts[method] = counts.get(method, 0) + 1
        bit_exact[method] = bit_exact.get(method, 0) + int(results[0] == results[1])
        assert traces[0] == traces[1], (method, traces)
        if method.startswith('Read') or method == 'QueryState5F00F0':
            assert returns[0] == returns[1], (method, returns)
        for off in range(0, 0x4000, 4):
            a, b = results[0][off:off + 4], results[1][off:off + 4]
            if a == b:
                continue
            address = ARENA + off
            assert address in float_fields, (method, hex(address), a.hex(), b.hex(), counts[method], struct.unpack_from('<f', state, CAR + 0x42c - ARENA)[0], struct.unpack_from('<f', state, 0x2d4)[0], struct.unpack_from('<f', state, 0x2dc)[0], state[0x2f0])
            x, y = struct.unpack('<f', a)[0], struct.unpack('<f', b)[0]
            assert math.isfinite(x) and math.isfinite(y) and math.isclose(x, y, rel_tol=2e-6, abs_tol=2e-6), (method, hex(address), x, y)

    def fixture():
        state = bytearray(b'\xa5' * 0x10000)
        def put(address, fmt, value):
            struct.pack_into(fmt, state, address - ARENA, value)
        put(INPUT, '<I', VTABLE)
        put(INPUT + 0x2a8, '<I', CAR)
        put(CAR + 0x20, '<I', RECORD)
        for o in (0x20c, 0x210, 0x214, 0x2bc, 0x2d4, 0x2dc, 0x2ec):
            put(INPUT + o, '<f', 0)
        for o in (0x1e4, 0x1e8, 0x2c0, 0x2cc):
            put(INPUT + o, '<i', 0)
        put(INPUT + 0x2f0, '<B', 0)
        for o in (0x42c, 0x430, 0x118, 0x440):
            put(CAR + o, '<f', 0)
        put(CAR + 0x434, '<i', 1)
        put(RECORD + 0x1f0, '<f', 7000)
        return state, put

    for gear in (-1, 0, 1, 2, 6, 0x7fffffff):
        for method, off in (('ReadGearRequest', 0x1e4), ('ReadDirectGear', 0x1e8)):
            state, put = fixture(); put(INPUT + off, '<i', gear)
            compare(method, state)
        for method in ('StoreGearRequest', 'StoreDirectGear'):
            state, put = fixture(); compare(method, state, argument=gear)
    for transmission, flags in itertools.product(range(5), ((), (0x89e7d8,), (0x890108,))):
        state, put = fixture(); put(INPUT + 0x2c0, '<i', transmission)
        compare('SelectForward', state, flags=flags)
    for replay in itertools.product((0, 1, 255), (-1, 0, 1), (-1, 0, 1)):
        state, _ = fixture(); compare('QueryState5F00F0', state, replay=replay)
    speeds = (-40, -30, -2.001, -2, -1.999, -0.5, 0, 0.499, 0.5, 0.501, 20)
    for speed, throttle, brake, flag in itertools.product(speeds, (0, 0.49, 0.5, 0.51, 0.6, 0.6001, 1), (0, 0.49, 0.5, 0.51, 0.6, 0.6001, 1), (0, 1)):
        state, put = fixture()
        put(CAR + 0x42c, '<f', speed); put(CAR + 0x430, '<f', abs(speed))
        put(INPUT + 0x2d4, '<f', throttle); put(INPUT + 0x2dc, '<f', brake)
        put(INPUT + 0x2f0, '<B', flag)
        compare('UpdatePedals', state, duration=1 / 60)
    modes = ((), (0x89e7d8,), (0x890108,), (0x89e7e2,), (0x890100,), (0x890110,))
    for trial in range(2048):
        state, put = fixture()
        for off in (0x20c, 0x2d4, 0x2dc, 0x2ec):
            put(INPUT + off, '<f', rng.choice((0, 0.25, 0.5, 0.6, 0.75, 1, rng.uniform(-0.1, 1.1))))
        put(INPUT + 0x2bc, '<f', rng.choice((0, 0.01, 0.1, 1)))
        put(INPUT + 0x2cc, '<i', rng.choice((0, 1, 66, 1000, 0x7fffffff)))
        put(INPUT + 0x2c0, '<i', trial % 5)
        put(INPUT + 0x1e4, '<i', trial % 4 - 1)
        put(INPUT + 0x2f0, '<B', trial % 2)
        put(CAR + 0x42c, '<f', rng.uniform(-50, 50)); put(CAR + 0x430, '<f', rng.uniform(0, 50))
        put(CAR + 0x118, '<f', rng.uniform(-4, 4)); put(CAR + 0x440, '<f', rng.uniform(4000, 9000))
        put(CAR + 0x434, '<i', trial % 4)
        compare('UpdatePedals', state, duration=rng.choice((0, 1 / 144, 1 / 60, 0.1)),
                flags=modes[trial % len(modes)], replay=(trial % 2, 0, 0), frame=trial % 16, flow=(4, 6)[trial % 2])
    for method in ('SnapshotFrameKinematics', 'UpdateFrameAcceleration'):
        for trial in range(512):
            state, put = fixture()
            for off in (0x60, 0x64, 0x68, 0x70, 0x74, 0x78, 0x110, 0x114, 0x118,
                        0x510, 0x514, 0x518, 0x520, 0x524, 0x528, 0x530, 0x534, 0x538):
                put(CAR + off, '<f', rng.uniform(-100, 100))
            compare(method, state, duration=(-0.1, 0, 1 / 240, 1 / 144, 1 / 60, 0.1)[trial % 6])
    report = dict(executable_sha256=manifest['exe_sha256'], cases=counts,
                  bit_identical_state_cases=bit_exact, byte_matching='not established',
                  controlled_interfaces=['event dispatcher 0x578e90'],
                  limitations=['Finite synthetic inputs; not full device capture, car construction or caller composition.',
                               'Kinematics compare only the original guarded prefixes.',
                               'Original __ftol2 executes as an analysis runtime dependency.'])
    (BUILD / 'verification.json').write_text(json.dumps(report, indent=2) + '\n')
    print(json.dumps(report, indent=2))


if __name__ == '__main__':
    main()
