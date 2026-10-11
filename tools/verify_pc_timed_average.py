#!/usr/bin/env python3
"""Original/rebuilt circular timed-average constructors and stateful updates."""
import hashlib
import json
import random
import re
import struct
import subprocess
import sys

import pefile
from unicorn import Uc, UC_ARCH_X86, UC_MODE_32, UC_HOOK_CODE
from unicorn.x86_const import *
from pc_coff import link_objects, read_object
from probe_pc_race_reward import ROOT, EXE
from verify_pc_engine import CONVERSION

BUILD = ROOT / 'build/analysis/timed-average'
BASE, STACK, STOP = 0x2000000, 0x200f000, 0x2100000


def main():
    steering = '--steering' in sys.argv[1:]
    manifest_name = 'human-steering' if steering else 'timed-average'
    manifest = json.loads((ROOT / ('config/SPEED2_EXE_1_2/' + manifest_name + '.json')).read_text())
    raw = EXE.read_bytes()
    assert hashlib.sha256(raw).hexdigest() == manifest['exe_sha256']
    image = pefile.PE(data=raw).get_memory_mapped_image()
    for f in manifest['functions']:
        assert hashlib.sha256(image[f['address'] - 0x400000:f['address'] - 0x400000 + f['size']]).hexdigest() == f['sha256']
    assert hashlib.sha256(image[CONVERSION[0] - 0x400000:CONVERSION[0] - 0x400000 + CONVERSION[1]]).hexdigest() == CONVERSION[2]
    BUILD.mkdir(parents=True, exist_ok=True)
    obj = BUILD / 'TimedAverage.obj'
    source = 'src/Speed/Indep/Libs/bWare/TimedAverage.cpp'
    if steering:
        unity = BUILD / 'steering-analysis.cpp'
        unity.write_text(''.join('#include "' + str(ROOT / path) + '"\n' for path in
                                (source, 'src/Speed/Indep/Src/Physics/EngineCurve.cpp',
                                 'src/Speed/Indep/Src/Physics/HumanSteering.cpp')))
        source = str(unity.relative_to(ROOT))
    subprocess.run([str(ROOT / 'build/tools/wibo'), 'build/compilers/Win32/7.1/cl.exe',
                    '/nologo', '/c', '/Ox', '/Ob0', '/Z7', '/EHsc', '/Fo' + str(obj.relative_to(ROOT)),
                    source], cwd=ROOT, check=True)
    externals = {'_malloc': 0x75c6c8, '_free': 0x75c6da, '__ftol2': CONVERSION[0], '__fltused': BASE + 0x8000}
    globals_ = {name: int(address, 16) for name, address in re.findall(r'^(\w+) = (0x[0-9a-f]+);',
                (ROOT / 'src/Platform/Native/OriginalSymbols.ld').read_text(), re.M)}
    for symbol in read_object(obj)['symbols'].values():
        name = symbol['name']
        match = re.match(r'^\?(\w+)@@', name)
        if symbol['section'] == 0 and match and match[1] in globals_:
            externals[name] = globals_[match[1]]
    linked = link_objects([obj], externals)
    functions = {}
    for f in manifest['functions']:
        if f.get('kind') == 'dependency':
            continue
        matches = [a for n, a in linked['exports'].items() if n.startswith('?' + f.get('symbol', f['method']) + '@')]
        assert len(matches) == 1, matches
        functions[f['method']] = (f['address'], matches[0])
    machines, cursors, traces = [], [0, 0], [[], []]
    allowed_original = [(f['address'], f['address'] + f['size']) for f in manifest['functions']]
    for side in (0, 1):
        uc = Uc(UC_ARCH_X86, UC_MODE_32)
        uc.mem_map(0x400000, (len(image) + 4095) & ~4095)
        uc.mem_write(0x400000, image)
        uc.mem_map(BASE, 0x10000)
        uc.mem_map(0, 0x1000)  # Original steering constructor's FS:0 SEH chain.
        uc.mem_map(STOP, 0x1000)
        uc.mem_map(linked['address'], linked['allocation'])
        for address, body in linked['blocks']:
            uc.mem_write(address, body)

        def hook(machine, address, size, user, side=side):
            if address == STOP:
                return  # Controlled fstp observation of the floating return.
            if address == 0x75c6da:
                sp = machine.reg_read(UC_X86_REG_ESP)
                pointer = struct.unpack('<I', machine.mem_read(sp + 4, 4))[0]
                traces[side].append(('free', pointer))
                machine.reg_write(UC_X86_REG_EAX, 0xaabbccdd)
                machine.reg_write(UC_X86_REG_ECX, 0xeeeeeeee)
                machine.reg_write(UC_X86_REG_EDX, 0xffffffff)
                machine.reg_write(UC_X86_REG_EIP, struct.unpack('<I', machine.mem_read(sp, 4))[0])
                machine.reg_write(UC_X86_REG_ESP, sp + 4)
                return
            if address == 0x75c6c8:
                sp = machine.reg_read(UC_X86_REG_ESP)
                length = struct.unpack('<I', machine.mem_read(sp + 4, 4))[0]
                assert 0 <= length <= 1024
                allocation = cursors[side]
                cursors[side] += max(16, (length + 15) & ~15)
                assert cursors[side] < BASE + 0x8000
                traces[side].append((length, allocation))
                machine.reg_write(UC_X86_REG_EAX, allocation)
                machine.reg_write(UC_X86_REG_ECX, 0xeeeeeeee)
                machine.reg_write(UC_X86_REG_EDX, 0xffffffff)
                machine.reg_write(UC_X86_REG_EIP, struct.unpack('<I', machine.mem_read(sp, 4))[0])
                machine.reg_write(UC_X86_REG_ESP, sp + 4)
                return
            allowed = linked['ranges'] if side else allowed_original
            allowed = allowed + [(CONVERSION[0], CONVERSION[0] + CONVERSION[1])]
            assert any(a <= address < b for a, b in allowed), hex(address)
        uc.hook_add(UC_HOOK_CODE, hook)
        machines.append(uc)

    counts = {method: 0 for method in functions}
    exact = dict(counts)

    def reset():
        for side, uc in enumerate(machines):
            uc.mem_write(BASE, b'\xa5' * 0x10000)
            uc.reg_write(UC_X86_REG_FPCW, 0x027f)
            cursors[side] = BASE + 0x1000
            traces[side] = []

    def call(method, arguments):
        states = []
        for side, uc in enumerate(machines):
            uc.mem_write(STACK, struct.pack('<I', STOP) + arguments)
            uc.reg_write(UC_X86_REG_ESP, STACK)
            uc.reg_write(UC_X86_REG_ECX, BASE)
            for reg in (UC_X86_REG_EBX, UC_X86_REG_ESI, UC_X86_REG_EDI, UC_X86_REG_EBP):
                uc.reg_write(reg, 0x12345678)
            uc.emu_start(functions[method][side], STOP, count=20000)
            assert uc.reg_read(UC_X86_REG_EIP) == STOP, method
            assert uc.reg_read(UC_X86_REG_ESP) == STACK + 4 + len(arguments), method
            for reg in (UC_X86_REG_EBX, UC_X86_REG_ESI, UC_X86_REG_EDI, UC_X86_REG_EBP):
                assert uc.reg_read(reg) == 0x12345678, method
            if method in ('Construct', 'ConstructTimed', 'ConstructSteering'):
                assert uc.reg_read(UC_X86_REG_EAX) == BASE, method
            if method == 'CalculateRate':
                uc.mem_write(STOP, b'\xd9\x1d' + struct.pack('<I', BASE + 0x7ffc))
                uc.emu_start(STOP, STOP + 6, count=1)
            states.append(bytes(uc.mem_read(BASE, 0x8000)))
        counts[method] += 1
        assert traces[0] == traces[1], (method, traces)
        exact[method] += int(states[0] == states[1])
        assert states[0] == states[1], (method, counts, [(hex(i), a, b) for i, (a, b) in enumerate(zip(*states)) if a != b][:16])

    if steering:
        rng = random.Random(0x4060e0)
        modes = ((), (0x89e7d8,), (0x89e7d9,), (0x89e7e2,), (0x890104,), (0x890108,), (0x890110,))
        for trial in range(700):
            reset()
            call('ConstructSteering', struct.pack('<I', BASE + 0x9000))
            for step in range(6):
                value = rng.uniform(-1, 1)
                for uc in machines:
                    for a in (0x89e7d8, 0x89e7d9, 0x89e7e2):
                        uc.mem_write(a, bytes([int(a in modes[trial % len(modes)])]))
                    for a in (0x890100, 0x890104, 0x890108, 0x890110, 0x890114):
                        uc.mem_write(a, struct.pack('<i', int(a in modes[trial % len(modes)])))
                    uc.mem_write(0x86518c, struct.pack('<i', step * 67 + 1))
                    uc.mem_write(BASE + 0x942c, struct.pack('<f', (trial % 101) - 30))
                    uc.mem_write(BASE + 0x20, struct.pack('<f', (trial % 21) / 10.0 - 1))
                    uc.mem_write(BASE + 0x84, struct.pack('<f', (trial % 17) / 8.0 - 1))
                call('CalculateRate', struct.pack('<ffi', value, (1 / 120, 1 / 60, 0.1)[trial % 3], trial % 4))
            call('DestroySteering', b'')
    else:
        verify_average(reset, call)
    report = dict(executable_sha256=manifest['exe_sha256'], cases=counts,
                  bit_identical_state_cases=exact, byte_matching='not established',
                  controlled_interfaces=['malloc 0x75c6c8 and free 0x75c6da; deterministic addresses and caller-saved register clobbering'],
                  limitations=['Finite capacities and monotonic time sequences; invalid input and full-window exhaustion excluded.',
                               'Original __ftol2 executes as an analysis runtime dependency.',
                               'Allocation-failure/SEH unwinding, full steering composition and native driving integration not established.'])
    (BUILD / (manifest_name + '-verification.json')).write_text(json.dumps(report, indent=2) + '\n')
    print(json.dumps(report, indent=2))


def verify_average(reset, call):
    for capacity in (0, 1, 2, 5, 6, 18, 66, 127, 255, 256):
        reset()
        call('Construct', struct.pack('<i', capacity))
        call('Destroy', b'')
    rng = random.Random(0x576aa0)
    for window, frequency in ((0.15, 120), (0.55, 120), (1, 4), (0.25, 8), (1, 255)):
        reset()
        call('ConstructTimed', struct.pack('<ff', window, frequency))
        time = 0.0
        for trial in range(1200):
            # Positive spacing within the window prevents exhausting every slot
            # in the original expiry loop. This exercises wrap, expiry and zeros.
            time += rng.choice((0, 0.5 / frequency, 1 / frequency, 2 / frequency))
            value = rng.choice((0, -1, 1, rng.uniform(-45, 45)))
            call('Advance', struct.pack('<ff', value, time))
        call('DestroyTimed', b'')


if __name__ == '__main__':
    main()
