#!/usr/bin/env python3
"""Verify original physics drive-curve/speed helpers with local retail records."""
import itertools
import hashlib
import json
import re
import struct
import subprocess

import pefile
from unicorn import Uc, UC_ARCH_X86, UC_MODE_32, UC_HOOK_CODE
from unicorn.x86_const import *
from pc_coff import link_objects, read_object
from probe_pc_race_reward import ROOT, EXE, GAME_DIR
from verify_pc_engine import CONVERSION

BUILD = ROOT / 'build/analysis/physics-parameters'
BASE, STACK, STOP = 0x2000000, 0x200f000, 0x2100000


def retail_records():
    raw = (GAME_DIR / 'GLOBAL/GLOBALB.BUN').read_bytes()
    offset = 0
    while offset + 8 <= len(raw):
        kind, size = struct.unpack_from('<II', raw, offset)
        if kind == 0x34600:
            for start in range(offset + 16, offset + 8 + size, 0x890):
                name = raw[start:start + 32].split(b'\0')[0].decode('ascii')
                record = raw[start + 0x110:start + 0x3c0]
                mass = struct.unpack_from('<f', record, 0x110)[0]
                gears = struct.unpack_from('<i', record, 0x1c8)[0]
                engine_start, _, engine_end = struct.unpack_from('<fff', record, 0x1f0)
                if mass > 0 and 2 <= gears <= 6 and engine_end > engine_start:
                    yield name, record
            return
        offset += 8 + size
    raise AssertionError('No CarTypeInfo chunk')


def main():
    manifest = json.loads((ROOT / 'config/SPEED2_EXE_1_2/physics-parameters.json').read_text())
    raw = EXE.read_bytes()
    assert hashlib.sha256(raw).hexdigest() == manifest['exe_sha256']
    image = pefile.PE(data=raw).get_memory_mapped_image()
    for f in manifest['functions']:
        assert hashlib.sha256(image[f['address'] - 0x400000:f['address'] - 0x400000 + f['size']]).hexdigest() == f['sha256']
    assert hashlib.sha256(image[CONVERSION[0] - 0x400000:CONVERSION[0] - 0x400000 + CONVERSION[1]]).hexdigest() == CONVERSION[2]
    BUILD.mkdir(parents=True, exist_ok=True)
    source = BUILD / 'physics-analysis.cpp'
    source.write_text(''.join('#include "' + str(ROOT / 'src/Speed/Indep/Src/Physics' / (unit + '.cpp')) + '"\n'
                              for unit in ('EngineCurve', 'DrivetrainSetup', 'PhysicsDriveCurve', 'PhysicsRefresh')))
    obj = BUILD / 'physics-analysis.obj'
    subprocess.run([str(ROOT / 'build/tools/wibo'), 'build/compilers/Win32/7.1/cl.exe', '/nologo',
                    '/c', '/Ox', '/Op', '/Ob0', '/Z7', '/EHsc', '/Fo' + str(obj.relative_to(ROOT)),
                    str(source.relative_to(ROOT))], cwd=ROOT, check=True)
    globals_ = {name: int(address, 16) for name, address in re.findall(r'^(\w+) = (0x[0-9a-f]+);',
                (ROOT / 'src/Platform/Native/OriginalSymbols.ld').read_text(), re.M)}
    externals = {'__ftol2': CONVERSION[0], '__fltused': BASE + 0x8000,
                 '?Compose@PhysicsParameters@@QAEXHHH@Z': 0x5b4dc0,
                 # These link otherwise unused methods in DrivetrainSetup.cpp.
                 # Execution guards reject them: they are not test boundaries.
                 '?RefreshVariations@Drivetrain@@QAEXXZ': 0x594710, '_nfs_strcpy': 0x43dbd0}
    for symbol in read_object(obj)['symbols'].values():
        match = re.match(r'^\?(\w+)@@', symbol['name'])
        if symbol['section'] == 0 and match:
            if match[1] in globals_:
                externals[symbol['name']] = globals_[match[1]]
            elif match[1] == 'nfs_strcpy':
                externals[symbol['name']] = 0x43dbd0
    linked = link_objects([obj], externals)
    methods = {}
    for f in manifest['functions']:
        if f['kind'] != 'function':
            continue
        entries = [address for name, address in linked['exports'].items() if name.startswith('?' + f['method'] + '@')]
        assert len(entries) == 1, entries
        methods[f['method']] = (f['address'], entries[0])
    machines, traces = [], [[], []]
    controlled = [False]
    original_ranges = [(f['address'], f['address'] + f['size']) for f in manifest['functions']]
    for side in (0, 1):
        uc = Uc(UC_ARCH_X86, UC_MODE_32)
        uc.mem_map(0x400000, (len(image) + 4095) & ~4095)
        uc.mem_write(0x400000, image)
        uc.mem_map(BASE, 0x10000)
        uc.mem_map(STOP, 0x1000)
        uc.mem_map(linked['address'], linked['allocation'])
        for address, body in linked['blocks']:
            uc.mem_write(address, body)

        def hook(machine, address, size, user, side=side):
            if address == STOP:
                return
            if address == 0x5b4dc0:
                assert controlled[0], hex(address)
                sp = machine.reg_read(UC_X86_REG_ESP)
                args = bytes(machine.mem_read(sp + 4, 12))
                traces[side].append((address, machine.reg_read(UC_X86_REG_ECX), args))
                machine.reg_write(UC_X86_REG_EAX, 0xaabbccdd)
                machine.reg_write(UC_X86_REG_ECX, 0xeeeeeeee)
                machine.reg_write(UC_X86_REG_EDX, 0xffffffff)
                machine.reg_write(UC_X86_REG_EIP, struct.unpack('<I', machine.mem_read(sp, 4))[0])
                machine.reg_write(UC_X86_REG_ESP, sp + 16)
                return
            allowed = linked['ranges'] if side else original_ranges
            allowed = allowed + [(CONVERSION[0], CONVERSION[0] + CONVERSION[1])]
            assert any(a <= address < b for a, b in allowed), hex(address)
        uc.hook_add(UC_HOOK_CODE, hook)
        machines.append(uc)

    counts = {method: 0 for method in methods}
    exact = dict(counts)
    maximum_error = 0.0

    def compare(method, refresh_args=None):
        states, scratch = [], []
        for side, uc in enumerate(machines):
            args = struct.pack('<II', BASE + 0x55c, BASE) if method == 'BuildPhysicsDriveCurve' else b''
            if method == 'Refresh':
                args = struct.pack('<III', *(a & 0xffffffff for a in refresh_args))
            uc.mem_write(STACK, struct.pack('<I', STOP) + args)
            uc.reg_write(UC_X86_REG_ESP, STACK)
            uc.reg_write(UC_X86_REG_ECX, BASE)
            for reg in (UC_X86_REG_EBX, UC_X86_REG_ESI, UC_X86_REG_EDI, UC_X86_REG_EBP):
                uc.reg_write(reg, 0x12345678)
            traces[side] = []
            uc.emu_start(methods[method][side], STOP, count=1000000)
            assert uc.reg_read(UC_X86_REG_EIP) == STOP, method
            assert uc.reg_read(UC_X86_REG_ESP) == STACK + 4 + (12 if method == 'Refresh' else 0), method
            # BuildPhysicsDriveCurve is cdecl: the caller retains its two arguments.
            for reg in (UC_X86_REG_EBX, UC_X86_REG_ESI, UC_X86_REG_EDI, UC_X86_REG_EBP):
                assert uc.reg_read(reg) == 0x12345678, method
            if method == 'GetPhysicsSpeed':
                uc.mem_write(STOP, b'\xd9\x1d' + struct.pack('<I', BASE + 0x800))
                uc.emu_start(STOP, STOP + 6, count=1)
            states.append(bytes(uc.mem_read(BASE, 0x804)))
            scratch.append(bytes(uc.mem_read(0x86b61c, 93 * 4)))
        counts[method] += 1
        exact[method] += int(states[0] == states[1] and scratch[0] == scratch[1])
        assert traces[0] == traces[1], (method, counts, [len(t) for t in traces], [t[:2] + t[-3:] for t in traces])
        for buffers in (states, scratch):
            for offset in range(0, len(buffers[0]), 4):
                a, b = buffers[0][offset:offset + 4], buffers[1][offset:offset + 4]
                assert a == b, (method, counts, hex(offset), a.hex(), b.hex())

    records = list(retail_records())
    for name, record in records:
        for count in (2, 9, 16, 64):
            state = bytearray(b'\xa5' * 0x10000)
            state[0x30:0x2e0] = record
            struct.pack_into('<i', state, 0, 0)
            state[4] = 0
            struct.pack_into('<IfffI', state, 0x55c, count, 0, 55.555, 1.13333, BASE + 0x45c)
            for i in range(64):
                struct.pack_into('<f', state, 0x45c + i * 4, 4 if i < 63 else 0)
            for uc in machines:
                uc.mem_write(BASE, bytes(state))
                uc.mem_write(0x86b61c, b'\xa5' * (93 * 4))
                uc.reg_write(UC_X86_REG_FPCW, 0x027f)
            controlled[0] = False
            compare('BuildPhysicsDriveCurve')
            compare('GetPhysicsSpeed')
            # Keep the populated record/curve, but exercise unresolved interface
            # selection without pretending to recover those callees here.
            if count == 64:
                for limit in (-1.0, 10.0, 10000.0):
                    for uc in machines:
                        uc.mem_write(BASE + 0x2c0, struct.pack('<f', limit))
                    compare('GetPhysicsSpeed')
                controlled[0] = True
                mode_flags = (0x89e7d8, 0x89e7d9, 0x89e7e3, 0x890108,
                              0x890104, 0x89010c, 0x890114)
                for flag in (None,) + mode_flags:
                    for uc in machines:
                        uc.mem_write(BASE + 4, b'\xff')
                        uc.mem_write(BASE + 0x2c0, struct.pack('<f', 10.0))
                        for address in mode_flags:
                            uc.mem_write(address, bytes(1 if address in (0x89e7d8, 0x89e7d9, 0x89e7e3) else 4))
                        if flag is not None:
                            uc.mem_write(flag, b'\x01')
                    compare('GetPhysicsSpeed')
                for uc in machines:
                    uc.mem_write(BASE, struct.pack('<i', -1))
                compare('GetPhysicsSpeed')
                for uc in machines:
                    for address in mode_flags:
                        uc.mem_write(address, bytes(1 if address in (0x89e7d8, 0x89e7d9, 0x89e7e3) else 4))
    controlled[0] = True
    for mask, car, select, refresh, flag in itertools.product(range(128), (-1, 0),
            (0, 1, -1, 256), (0, -1, 0x12345678), (0, -1, 0x12345678)):
        for uc in machines:
            uc.mem_write(BASE, struct.pack('<i', car))
            for i, address in enumerate(mode_flags):
                uc.mem_write(address, struct.pack('<I', (mask >> i) & 1)[:1 if address in (0x89e7d8, 0x89e7d9, 0x89e7e3) else 4])
        compare('Refresh', (refresh, flag, select))
    report = dict(executable_sha256=manifest['exe_sha256'], retail_records=[name for name, _ in records],
                  cases=counts, bit_identical_state_cases=exact, maximum_absolute_float_error=maximum_error,
                  byte_matching='not established',
                  controlled_interfaces=['Compose 0x5b4dc0 only in explicit sentinel/refresh cases; full arguments traced and caller registers clobbered'],
                  limitations=['Finite stock records/output sizes and speed limits; full upgrades and steering integration not established.',
                               'MSVC /Op preserves arithmetic order; original x87 unrounded loop comparison is explicit.',
                               'Original __ftol2 executes as an analysis runtime dependency.',
                               'Computed shift points, curve lookup and Refresh execute recovered source in rebuilt cases.'])
    (BUILD / 'verification.json').write_text(json.dumps(report, indent=2) + '\n')
    print(json.dumps(report, indent=2))


if __name__ == '__main__':
    main()
