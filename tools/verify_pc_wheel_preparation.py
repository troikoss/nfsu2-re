#!/usr/bin/env python3
"""Compare original wheel preparation with its complete rebuilt helper chain."""
import hashlib
import itertools
import json
import random
import struct
import subprocess
import pefile
from unicorn import Uc, UC_ARCH_X86, UC_MODE_32, UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_ECX, UC_X86_REG_ESP, UC_X86_REG_EIP, UC_X86_REG_EBX, UC_X86_REG_ESI, UC_X86_REG_EDI, UC_X86_REG_EBP, UC_X86_REG_FPCW
from probe_pc_race_reward import ROOT, EXE
from pc_coff import link_objects
from verify_pc_engine import CONVERSION, CURVE, ORIGINAL as ENGINE_ORIGINAL
from verify_pc_wheel_preparation_helpers import ORIGINAL as PREP_HELPERS, GLOBALS as PREP_GLOBALS
from verify_pc_wheel_force_correction import SYMBOL as CORRECT, ADDRESS as CORRECT_ADDRESS, SIZE as CORRECT_SIZE, DIGEST as CORRECT_DIGEST
from verify_pc_engine_post_update import FLOATS, ORIGINAL as MATH_ORIGINAL, SINE, UNITS
from verify_pc_wheel_math import ORIGINAL as WHEEL_ORIGINAL
from verify_pc_wheel_correction_scale import GLOBALS as SCALE_GLOBALS, SYMBOL as SCALE, ADDRESS as SCALE_ADDRESS, SIZE as SCALE_SIZE, DIGEST as SCALE_DIGEST
from verify_pc_wheel_angle import SYMBOL as ANGLE, ADDRESS as ANGLE_ADDRESS, SIZE as ANGLE_SIZE, DIGEST as ANGLE_DIGEST, TABLE, TABLE_SIZE, TABLE_DIGEST

BUILD = ROOT/'build/analysis/racing'
SYMBOL = '?PrepareForces@DrivetrainWheel@@QAEXMH@Z'
ADDRESS, SIZE, DIGEST = 0x5a8280, 1078, '7dda4e05e79a9c83ebe616e0c7f0f4e4b3f733486458575f439eb5e0a3368c75'
GLOBALS = {**PREP_GLOBALS, 'wheelPreparationThreshold': 0x7fc704, 'wheelPreparationSpeedScale': 0x78426c,
           'wheelPreparationCoefficientThreshold': 0x784358, 'wheelDriveForceScale': 0x7fc720, 'wheelForceLimitScale': 0x7fc718,
           'wheelForceAngleScale': 0x7844f0, 'wheelForcePi': 0x7a1788}


def main():
    raw = EXE.read_bytes()
    assert hashlib.sha256(raw).hexdigest() == 'f9dd86c054878ce6276beb07c1fd61874f7a1e4bf1f241b084c65b73e24168a7'
    pe = pefile.PE(data=raw)
    image, base = pe.get_memory_mapped_image(), pe.OPTIONAL_HEADER.ImageBase
    originals = [(CORRECT_ADDRESS, CORRECT_SIZE, CORRECT_DIGEST), *PREP_HELPERS.values(), (ADDRESS, SIZE, DIGEST), (SCALE_ADDRESS, SCALE_SIZE, SCALE_DIGEST),
                 (ANGLE_ADDRESS, ANGLE_SIZE, ANGLE_DIGEST), MATH_ORIGINAL[SINE], MATH_ORIGINAL[UNITS],
                 *WHEEL_ORIGINAL.values(), CONVERSION, (TABLE, TABLE_SIZE, TABLE_DIGEST)]
    for address, size, digest in originals:
        assert hashlib.sha256(image[address-base:address-base+size]).hexdigest() == digest
    paths = []
    for unit in ('WheelForceCorrection', 'WheelPreparation', 'WheelPreparationHelpers', 'EngineCurve', 'WheelCorrectionScale', 'WheelMath', 'WheelAngle', 'EngineMath'):
        obj = BUILD/(unit+'.obj')
        subprocess.run([str(ROOT/'build/tools/wibo'), 'build/compilers/Win32/7.1/cl.exe', '/nologo', '/c', '/Ox', '/Ob0', '/Z7', '/EHsc',
                        '/Fo'+str(obj.relative_to(ROOT)), 'src/Speed/Indep/Src/Physics/'+unit+'.cpp'], cwd=ROOT, check=True)
        paths.append(obj)
    externals = {'?'+name+'@@3MA': address for name, address in {**FLOATS, **GLOBALS}.items()}
    externals.update(SCALE_GLOBALS)
    externals.update({'?wheelSurfaces@@3PAUWheelSurface@@A': 0x800f04, '?wheelForceBasisMode@@3HA': 0x7fc7fc,
                      '?wheelAngleTable@@3PAEA': TABLE, '?wheelAngleRatioScale@@3MA': 0x786db8, '__ftol2': CONVERSION[0]})
    externals.update({'?wheelLateralMode@@3HA': 0x7fc7f0,
                      '?wheelLateralFallbackCurve@@3UEngineCurve@@A': 0x7fc954,
                      '?wheelLateralCurves@@3PAPAUEngineCurve@@A': 0x7fc7d4,
                      '?wheelLateralLastCurve@@3PAUEngineCurve@@A': 0x7fc7ec})
    linked = link_objects(paths, externals)
    this, spec, corr_spec, aux, stack, stop = 0x2000000, 0x2001000, 0x2002000, 0x2003000, 0x200f000, 0x2100000
    machines = []
    visits = {}
    sites = {0x5a82cd: 'first_drive_update', 0x5a82f7: 'second_drive_update',
             0x5a841c: 'negative_drive_limit', 0x5a84cb: 'scale_helper',
             0x5a8522: 'lock_helper', 0x5a855a: 'slip_forces',
             0x5a85c8: 'low_speed_rescale', 0x5a8603: 'fallback_forces',
             0x5a8682: 'lateral_curve'}
    for rebuilt in (False, True):
        uc = Uc(UC_ARCH_X86, UC_MODE_32)
        uc.mem_map(base, (len(image)+4095) & ~4095); uc.mem_write(base, image)
        uc.mem_map(this, 0x10000); uc.mem_map(stop, 0x1000)
        uc.mem_map(linked['address'], linked['allocation'])
        if rebuilt:
            for address, data in linked['blocks']:
                uc.mem_write(address, data)
        ranges = (linked['ranges'] + [(CONVERSION[0], CONVERSION[0]+CONVERSION[1])]) if rebuilt else (
            [(a, a+n) for a, n, _ in originals] + [(0x43c9c0, 0x43c9c5)])
        def guard(machine, address, size, user, ranges=ranges, rebuilt=rebuilt):
            assert any(a <= address < b for a, b in ranges), hex(address)
            if not rebuilt and address in sites:
                key = sites[address]; visits[key] = visits.get(key, 0)+1
        uc.hook_add(UC_HOOK_CODE, guard)
        machines.append(uc)
    rng = random.Random(ADDRESS)
    inputs = [(velocity, value, flag, mode, helper_mode, False)
              for velocity, value, flag, mode, helper_mode in itertools.product(
                  ((0, 0, 0), (1, 0, 0), (-1, 0, 0), (0, 1, 0), (3, 4, 0), (-3, -4, 0)),
                  (-20, -1, 0, 1, 20), (0, 1), (0, 1), (0, 1))]
    inputs += [(tuple(rng.uniform(-30, 30) for _ in range(3)), rng.uniform(-30, 30), rng.randrange(2),
                rng.randrange(2), rng.randrange(2), True) for _ in range(2048)]
    count = 0
    counts = {'preparation': 0, 'preparation_then_correction': 0}
    writable = [0x28, 0x100, 0x104, 0x108, 0x10c, 0x118, 0x124, 0x128, 0x130, 0x134, 0x138, 0x16c, 0x170]
    for velocity, value, flag, mode, helper_mode, randomized in inputs:
        state = bytearray(b'\xa5'*0x9000)
        def f(offset, value): struct.pack_into('<f', state, offset, value)
        def i(offset, value): struct.pack_into('<i', state, offset, value)
        i(0x14, spec); i(0x18, corr_spec); i(0x20, aux); i(0x1c, this+0x6000)
        f(0x28, value); i(0x1018, rng.choice((1, 2, 3))); f(0x1010, rng.uniform(0.1, 2) if randomized else 0.5)
        f(0x2000, rng.uniform(0.1, 3)); f(0x2004, rng.uniform(0.1, 3)); f(0x2008, rng.uniform(0.1, 3))
        f(0x200c, rng.uniform(0.1, 2)); f(0x2010, rng.uniform(0.1, 2))
        f(0x33ec, rng.choice((0, 0.25, 20, 40))); i(0x3424, helper_mode)
        for off in (0x6004, 0x6008): f(off, rng.choice((0, 0.1, 10)))
        matrix = ([rng.uniform(-1, 1) for _ in range(16)] if randomized else [float(j%5 == 0) for j in range(16)])
        for j, val in enumerate(matrix): f(0x70+j*4, val)
        for j, val in enumerate(velocity): f(0xf0+j*4, val)
        f(0x108, rng.choice((-10, -1, 0, 1, 10))); i(0x10c, flag); i(0x114, rng.randrange(3))
        f(0x164, rng.choice((0, 0.1, 1, 30)))
        for off in (0x178, 0x17c, 0x180, 0x184, 0x188, 0x190, 0x194): f(off, rng.uniform(0.1, 2) if randomized else 1)
        curve_records = []
        for j in range(8):
            curve, data, n = 0x4000+j*0x40, 0x5000+j*0x100, rng.choice((1, 2, 5, 9))
            record = struct.pack('<ifffI', n, 0, (n-1)*10, 0.1, this+data)
            state[curve:curve+20] = record
            for k in range(n): f(data+k*4, rng.uniform(-20, 100))
            curve_records.append(record)
        lateral_mode = rng.randrange(3)
        for j in range(3): f(0x150+j*4, (0, 0, 1)[j])
        state[0x11c] = rng.randrange(2)
        for precision, compose in itertools.product((0x027f, 0x037f), (False, True)):
            label = 'preparation_then_correction' if compose else 'preparation'
            results = []
            for rebuilt, uc in zip((False, True), machines):
                uc.mem_write(this, bytes(state))
                uc.mem_write(0x7fc7fc, struct.pack('<i', mode))
                uc.mem_write(0x7fc720, struct.pack('<f', 1))
                uc.mem_write(0x7fc718, struct.pack('<f', 1))
                uc.mem_write(0x7fc7f0, struct.pack('<i', lateral_mode))
                uc.mem_write(0x7fc954, curve_records[7])
                for j in range(7): uc.mem_write(0x7fc7d4+j*4, struct.pack('<I', this+0x4000+j*0x40))
                for surface in range(3): uc.mem_write(0x800f04+surface*0x98, struct.pack('<f', (0.1, 0.5, 1)[surface]))
                for surface in range(3): uc.mem_write(0x800f08+surface*0x98, struct.pack('<f', (0, 0.1, 1)[surface]))
                uc.mem_write(stack, struct.pack('<Ifi', stop, 123.5, mode))
                uc.reg_write(UC_X86_REG_ECX, this); uc.reg_write(UC_X86_REG_ESP, stack)
                uc.reg_write(UC_X86_REG_FPCW, precision)
                saved = (UC_X86_REG_EBX, UC_X86_REG_ESI, UC_X86_REG_EDI, UC_X86_REG_EBP)
                for reg in saved: uc.reg_write(reg, 0x12345678)
                uc.emu_start(linked['exports'][SYMBOL] if rebuilt else ADDRESS, stop, count=10000)
                assert uc.reg_read(UC_X86_REG_EIP) == stop
                assert uc.reg_read(UC_X86_REG_ESP) == stack+12
                if compose:
                    uc.mem_write(stack, struct.pack('<If', stop, 0))
                    uc.reg_write(UC_X86_REG_ESP, stack)
                    uc.reg_write(UC_X86_REG_ECX, this)
                    uc.emu_start(linked['exports'][CORRECT] if rebuilt else CORRECT_ADDRESS, stop, count=10000)
                    assert uc.reg_read(UC_X86_REG_EIP) == stop
                    assert uc.reg_read(UC_X86_REG_ESP) == stack+8
                assert uc.reg_read(UC_X86_REG_FPCW) == precision
                assert all(uc.reg_read(reg) == 0x12345678 for reg in saved)
                after = bytes(uc.mem_read(this, len(state)))
                masked = bytearray(after)
                for offset in writable: masked[offset:offset+4] = state[offset:offset+4]
                masked[0x160] = state[0x160]
                assert masked == state, ('unexpected state write', mode, rebuilt)
                results.append(after)
            if results[0] != results[1]:
                differences = [(hex(off), struct.unpack_from('<f', results[0], off)[0], struct.unpack_from('<f', results[1], off)[0],
                                results[0][off:off+4].hex(), results[1][off:off+4].hex()) for off in writable if results[0][off:off+4] != results[1][off:off+4]]
                raise AssertionError((count, mode, velocity, value, flag, helper_mode, randomized, precision, differences, {hex(off): struct.unpack_from('<f', state, off)[0] for off in (0x108, 0x6004, 0x6008, 0x178, 0x17c)}, {hex(va): struct.unpack_from('<f', image, va-base)[0] for va in (0x7fc708, 0x7fc70c)}))
            count += 1
            counts[label] += 1
    assert set(visits) == set(sites.values()), visits
    report = dict(original_address=hex(ADDRESS), original_size=SIZE, original_sha256=DIGEST,
                  rebuilt_size=linked['functions'][SYMBOL]['size'], byte_identical=False,
                  comparisons=counts, branch_visits=visits, stubbed_callees=[], original_runtime_dependency=CONVERSION,
                  compiler='MSVC 7.1 /Ox /Ob0 /Z7 /EHsc', executable_sha256=hashlib.sha256(raw).hexdigest(),
                  checks=['exact output and full state bytes', 'write footprint', 'stack and preserved registers', 'x87 precision preservation', 'slip and fallback branches', 'real recovered helper chain'],
                  limitations=['Finite synthetic inputs', 'Surface and angle data remain retail inputs', 'Original conversion helper still required', 'Preparation/correction composition excludes body/contact updates', 'Full wheel update and vehicle mover remain incomplete'])
    (BUILD/'wheel-preparation-verification.json').write_text(json.dumps(report, indent=2)+'\n')
    print(json.dumps(report, indent=2))


if __name__ == '__main__': main()
