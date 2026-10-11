#!/usr/bin/env python3
"""Compare recovered mode variants and interpolation against the original PC code."""
import hashlib
import json
import random
import re
import struct
import subprocess
import pefile
from unicorn import Uc, UC_ARCH_X86, UC_MODE_32, UC_HOOK_CODE
from unicorn.x86_const import *
from pc_coff import link_objects, read_object
from probe_pc_race_reward import ROOT, EXE
from verify_pc_physics_parameters import retail_records

BUILD = ROOT / 'build/analysis/physics-variants'
BASE, STACK, STOP = 0x2000000, 0x200f000, 0x2100000


def main():
    manifest = json.loads((ROOT / 'config/SPEED2_EXE_1_2/physics-variants.json').read_text())
    raw = EXE.read_bytes()
    assert hashlib.sha256(raw).hexdigest() == manifest['exe_sha256']
    image = pefile.PE(data=raw).get_memory_mapped_image()
    for f in manifest['functions']:
        assert hashlib.sha256(image[f['address'] - 0x400000:f['address'] - 0x400000 + f['size']]).hexdigest() == f['sha256']
    BUILD.mkdir(parents=True, exist_ok=True)
    source = BUILD / 'variants-analysis.cpp'
    source.write_text(''.join('#include "' + str(ROOT / 'src/Speed/Indep/Src/Physics' / (unit + '.cpp')) + '"\n'
                              for unit in ('PhysicsVariants',)))
    obj = BUILD / 'variants-analysis.obj'
    subprocess.run([str(ROOT / 'build/tools/wibo'), 'build/compilers/Win32/7.1/cl.exe', '/nologo',
                    '/c', '/Ox', '/Op', '/Ob0', '/Z7', '/EHsc', '/Fo' + str(obj.relative_to(ROOT)),
                    str(source.relative_to(ROOT))], cwd=ROOT, check=True)
    math_obj = BUILD / 'WheelMath.obj'
    subprocess.run([str(ROOT / 'build/tools/wibo'), 'build/compilers/Win32/7.1/cl.exe', '/nologo',
                    '/c', '/Ox', '/Ob0', '/Z7', '/EHsc', '/Fo' + str(math_obj.relative_to(ROOT)),
                    'src/Speed/Indep/Src/Physics/WheelMath.cpp'], cwd=ROOT, check=True)
    globals_ = {name: int(address, 16) for name, address in re.findall(r'^(\w+) = (0x[0-9a-f]+);',
                (ROOT / 'src/Platform/Native/OriginalSymbols.ld').read_text(), re.M)}
    externals = {'__fltused': BASE + 0x8000}
    for symbol in read_object(obj)['symbols'].values():
        match = re.match(r'^\?(\w+)@@', symbol['name'])
        if symbol['section'] == 0 and match and match[1] in globals_:
            externals[symbol['name']] = globals_[match[1]]
    linked = link_objects([obj, math_obj], externals)
    methods = {}
    for f in manifest['functions']:
        if f['method'] == 'WheelIdentity':
            continue
        entries = [a for s, a in linked['exports'].items() if s.startswith('?' + f['method'] + '@')]
        assert len(entries) == 1, entries
        methods[f['method']] = (f['address'], entries[0])
    machines = []
    for side in (0, 1):
        uc = Uc(UC_ARCH_X86, UC_MODE_32)
        uc.mem_map(0x400000, (len(image) + 4095) & ~4095)
        uc.mem_write(0x400000, image)
        uc.mem_map(BASE, 0x10000)
        uc.mem_map(STOP, 0x1000)
        uc.mem_map(linked['address'], linked['allocation'])
        for a, body in linked['blocks']:
            uc.mem_write(a, body)
        ranges = linked['ranges'] if side else [(f['address'], f['address'] + f['size']) for f in manifest['functions']]
        def guard(machine, address, size, user, ranges=ranges):
            assert any(a <= address < b for a, b in ranges), hex(address)
        uc.hook_add(UC_HOOK_CODE, guard)
        machines.append(uc)
    rng = random.Random(0x5a7530)
    counts = dict.fromkeys(methods, 0)
    thresholds = [struct.unpack_from('<f', image, a - 0x400000)[0] for a in (0x784554, 0x7850ec)]
    selectors = [0.0, 0.5, 1.0]
    for t in thresholds:
        bits = struct.unpack('<I', struct.pack('<f', t))[0]
        selectors.extend(struct.unpack('<f', struct.pack('<I', bits + delta))[0] for delta in (-1, 0, 1))
    records = list(retail_records())
    for name, record in records:
        for level in (0.0, 1.5, 3.0, None):
            for selector in selectors:
                state = bytearray(b'\xa5' * 0x1000)
                state[:0x2b0] = record
                struct.pack_into('<f', state, 0x1c0, selector)
                for i in range(10):
                    struct.pack_into('<f', state, 0x600 + i * 4, rng.uniform(0, 3) if level is None else level)
                for i in range(22):
                    struct.pack_into('<f', state, 0x900 + i * 4, rng.uniform(-0.5, 2.0))
                for method, entries in methods.items():
                    outcomes = []
                    for side, uc in enumerate(machines):
                        uc.mem_write(BASE, bytes(state))
                        args = (BASE, BASE + 0x600, BASE + 0x900) if method == 'ApplyPhysicsVariant' else (BASE, BASE + 0x600)
                        uc.mem_write(STACK, struct.pack('<I', STOP) + struct.pack('<' + 'I' * len(args), *args))
                        uc.reg_write(UC_X86_REG_ESP, STACK)
                        uc.reg_write(UC_X86_REG_FPCW, 0x027f)
                        for reg in (UC_X86_REG_EBX, UC_X86_REG_ESI, UC_X86_REG_EDI, UC_X86_REG_EBP):
                            uc.reg_write(reg, 0x12345678)
                        uc.emu_start(entries[side], STOP, count=10000)
                        assert uc.reg_read(UC_X86_REG_EIP) == STOP
                        assert uc.reg_read(UC_X86_REG_ESP) == STACK + (4 if method == 'ApplyPhysicsVariant' else 12)
                        assert all(uc.reg_read(reg) == 0x12345678 for reg in (UC_X86_REG_EBX, UC_X86_REG_ESI, UC_X86_REG_EDI, UC_X86_REG_EBP))
                        outcomes.append(bytes(uc.mem_read(BASE, 0x1000)))
                    for offset in range(0, 0x1000, 4):
                        a, b = outcomes[0][offset:offset + 4], outcomes[1][offset:offset + 4]
                        assert a == b, (method, name, level, selector, hex(offset), a.hex(), b.hex())
                    counts[method] += 1
    report = dict(executable_sha256=manifest['exe_sha256'], bit_identical_cases=counts,
                  retail_record_count=len(records), selectors=selectors,
                  byte_matching='not established', controlled_interfaces=[],
                  limitations=['Finite retail records, upgrade levels and interpolation rows; NaNs and alternate FPU precision excluded.',
                               'Wrappers use executable tables; arbitrary rows exercise the shared application routine.',
                               'Recovered matrix identity executes on rebuilt side. Full composer/native mode integration unresolved.'])
    (BUILD / 'verification.json').write_text(json.dumps(report, indent=2) + '\n')
    print(json.dumps(report, indent=2))


if __name__ == '__main__':
    main()
