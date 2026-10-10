#!/usr/bin/env python3
"""Compare full UG2 wheel force correction with rebuilt, non-stubbed callees."""
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
from verify_pc_engine import CONVERSION
from verify_pc_engine_post_update import FLOATS, ORIGINAL as MATH_ORIGINAL, SINE, UNITS
from verify_pc_wheel_math import ORIGINAL as WHEEL_ORIGINAL
from verify_pc_wheel_correction_scale import GLOBALS as SCALE_GLOBALS, SYMBOL as SCALE, ADDRESS as SCALE_ADDRESS, SIZE as SCALE_SIZE, DIGEST as SCALE_DIGEST
from verify_pc_wheel_angle import SYMBOL as ANGLE, ADDRESS as ANGLE_ADDRESS, SIZE as ANGLE_SIZE, DIGEST as ANGLE_DIGEST, TABLE, TABLE_SIZE, TABLE_DIGEST

BUILD = ROOT/'build/analysis/racing'
SYMBOL = '?CorrectForces@DrivetrainWheel@@QAEXM@Z'
ADDRESS, SIZE, DIGEST = 0x59d600, 1258, '1ce2dd3dd9958515a72465145a0593ab9cdb2fdd0570b2ae9f840ba0f8bd1b22'
GLOBALS = {'wheelDriveForceScale': 0x7fc720, 'wheelForceLimitScale': 0x7fc718,
           'wheelForceAngleScale': 0x7844f0, 'wheelForcePi': 0x7a1788}


def main():
    raw = EXE.read_bytes()
    assert hashlib.sha256(raw).hexdigest() == 'f9dd86c054878ce6276beb07c1fd61874f7a1e4bf1f241b084c65b73e24168a7'
    pe = pefile.PE(data=raw)
    image, base = pe.get_memory_mapped_image(), pe.OPTIONAL_HEADER.ImageBase
    originals = [(ADDRESS, SIZE, DIGEST), (SCALE_ADDRESS, SCALE_SIZE, SCALE_DIGEST),
                 (ANGLE_ADDRESS, ANGLE_SIZE, ANGLE_DIGEST), MATH_ORIGINAL[SINE], MATH_ORIGINAL[UNITS],
                 *WHEEL_ORIGINAL.values(), CONVERSION, (TABLE, TABLE_SIZE, TABLE_DIGEST)]
    for address, size, digest in originals:
        assert hashlib.sha256(image[address-base:address-base+size]).hexdigest() == digest
    paths = []
    for unit in ('WheelForceCorrection', 'WheelCorrectionScale', 'WheelMath', 'WheelAngle', 'EngineMath'):
        obj = BUILD/(unit+'.obj')
        subprocess.run([str(ROOT/'build/tools/wibo'), 'build/compilers/Win32/7.1/cl.exe', '/nologo', '/c', '/Ox', '/Ob0', '/Z7', '/EHsc',
                        '/Fo'+str(obj.relative_to(ROOT)), 'src/Speed/Indep/Src/Physics/'+unit+'.cpp'], cwd=ROOT, check=True)
        paths.append(obj)
    externals = {'?'+name+'@@3MA': address for name, address in {**FLOATS, **GLOBALS}.items()}
    externals.update(SCALE_GLOBALS)
    externals.update({'?wheelSurfaces@@3PAUWheelSurface@@A': 0x800f04, '?wheelForceBasisMode@@3HA': 0x7fc7fc,
                      '?wheelAngleTable@@3PAEA': TABLE, '?wheelAngleRatioScale@@3MA': 0x786db8, '__ftol2': CONVERSION[0]})
    linked = link_objects(paths, externals)
    this, spec, corr_spec, aux, stack, stop = 0x2000000, 0x2001000, 0x2002000, 0x2003000, 0x200f000, 0x2100000
    machines = []
    visits = {}
    sites = {0x59d630: 'drive_scale', 0x59d6ae: 'scale_helper', 0x59d6db: 'force_limit', 0x59d707: 'undo_scale',
             0x59d72f: 'force_ratio', 0x59d74a: 'ratio_one', 0x59d764: 'flag_ratio_one', 0x59d774: 'no_contact',
             0x59d81e: 'negative_local_x', 0x59d884: 'angle_correction', 0x59d8b1: 'basis_one',
             0x59d92a: 'basis_two', 0x59d9bb: 'basis_three', 0x59da40: 'basis_four'}
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
    inputs = []
    # Cover both sides of saturation, scalar zero, sign and flag cases in every basis mode.
    for mode, forces, velocity, flags, helper_mode in itertools.product(
            (-1, 0, 1, 2, 3, 4, 5), ((0, 0), (0.0005, 0), (1, 0), (3, 4), (-3, -4), (50, -20)),
            ((0, 0, 0), (1, 0, 0), (2, 3, 0), (-2, -3, 0)), ((0, 0, 0), (1, 0, 1), (1, 1, 1), (1, 0, 0)), (0, 1)):
        inputs.append((mode, forces, velocity, flags, helper_mode, False))
    inputs += [(rng.choice(range(-1, 6)), (rng.uniform(-100, 100), rng.uniform(-100, 100)),
                tuple(rng.uniform(-40, 40) for _ in range(3)),
                (rng.randrange(2), rng.randrange(2), rng.randrange(2)), rng.randrange(3), True) for _ in range(2048)]
    inputs = [case + ({},) for case in inputs]
    def neighbors(value):
        bits = struct.unpack('<I', struct.pack('<f', value))[0]
        return [struct.unpack('<f', struct.pack('<I', bits+d))[0] for d in (-1, 0, 1)]
    # Threshold neighbors and the exact zero-projection normalization path.
    for mode, length, limit, direction in itertools.product(
            (0, 1, 2, 3, 4), neighbors(0.001) + neighbors(1.0), neighbors(0.001) + neighbors(1.0), (-1, 1)):
        inputs.append((mode, (length, 0), (direction, 2, 0), (0, 0, 1), 0, False,
                       {0x108: -1, 0x164: limit, 0x150: 1, 0x154: 0, 0x158: 0}))
    count = 0
    writable = [0x124, 0x128, 0x130, 0x134, 0x138, 0x170]
    for mode, forces, velocity, flags, helper_mode, randomized, overrides in inputs:
        state = bytearray(b'\xa5'*0x4000)
        def f(offset, value): struct.pack_into('<f', state, offset, value)
        def i(offset, value): struct.pack_into('<i', state, offset, value)
        i(0x14, spec); i(0x18, corr_spec); i(0x20, aux)
        f(0x28, rng.choice((-1, 0, 1))); i(0x1018, rng.choice((1, 2, 3)))
        f(0x200c, rng.uniform(0.1, 2) if randomized else 1)
        f(0x33ec, rng.choice((0, 20, 40))); i(0x3424, helper_mode)
        matrix = ([rng.uniform(-1, 1) for _ in range(16)] if randomized else [float(j%5 == 0) for j in range(16)])
        # Exercise translated/non-orthogonal matrix data as well as normal wheel bases.
        for j, value in enumerate(matrix): f(0x70+j*4, value)
        for j, value in enumerate(velocity): f(0xf0+j*4, value)
        normal = tuple(rng.uniform(-1, 1) for _ in range(3)) if randomized else (0, 0, 1)
        for j, value in enumerate(normal): f(0x150+j*4, value)
        f(0x108, rng.choice((-1, 0, 1))); i(0x10c, flags[0]); i(0x114, rng.randrange(3))
        state[0x160], state[0x11c] = flags[1:]
        f(0x124, forces[0]); f(0x128, forces[1])
        f(0x164, rng.choice((0, 0.0005, 1, 5, 30)))
        for offset in (0x180, 0x184, 0x188): f(offset, rng.uniform(0.1, 2) if randomized else 1)
        for offset, value in overrides.items(): f(offset, value)
        drive_scale, limit_scale = rng.choice((0.5, 1, 2)), 1 if overrides else rng.choice((0.5, 1, 2))
        for precision in (0x027f, 0x037f):
            results = []
            for rebuilt, uc in zip((False, True), machines):
                uc.mem_write(this, bytes(state))
                uc.mem_write(0x7fc7fc, struct.pack('<i', mode))
                uc.mem_write(0x7fc720, struct.pack('<f', drive_scale))
                uc.mem_write(0x7fc718, struct.pack('<f', limit_scale))
                for surface in range(3): uc.mem_write(0x800f04+surface*0x98, struct.pack('<f', 1 if overrides else (0, 0.0005, 1)[surface]))
                uc.mem_write(stack, struct.pack('<If', stop, 123.5))
                uc.reg_write(UC_X86_REG_ECX, this); uc.reg_write(UC_X86_REG_ESP, stack)
                uc.reg_write(UC_X86_REG_FPCW, precision)
                saved = (UC_X86_REG_EBX, UC_X86_REG_ESI, UC_X86_REG_EDI, UC_X86_REG_EBP)
                for reg in saved: uc.reg_write(reg, 0x12345678)
                uc.emu_start(linked['exports'][SYMBOL] if rebuilt else ADDRESS, stop, count=10000)
                assert uc.reg_read(UC_X86_REG_EIP) == stop
                assert uc.reg_read(UC_X86_REG_ESP) == stack+8
                assert uc.reg_read(UC_X86_REG_FPCW) == precision
                assert all(uc.reg_read(reg) == 0x12345678 for reg in saved)
                after = bytes(uc.mem_read(this, len(state)))
                masked = bytearray(after)
                for offset in writable: masked[offset:offset+4] = state[offset:offset+4]
                assert masked == state, ('unexpected state write', mode, rebuilt)
                results.append(after)
            if results[0] != results[1]:
                differences = [(hex(off), struct.unpack_from('<f', results[0], off)[0], struct.unpack_from('<f', results[1], off)[0],
                                results[0][off:off+4].hex(), results[1][off:off+4].hex()) for off in writable if results[0][off:off+4] != results[1][off:off+4]]
                raise AssertionError((count, mode, forces, velocity, flags, helper_mode, randomized, precision, differences))
            count += 1
    assert set(visits) == set(sites.values()), visits
    report = dict(original_address=hex(ADDRESS), original_size=SIZE, original_sha256=DIGEST,
                  rebuilt_size=linked['functions'][SYMBOL]['size'], byte_identical=False,
                  comparisons=count, branch_visits=visits, stubbed_callees=[], original_runtime_dependency=CONVERSION,
                  compiler='MSVC 7.1 /Ox /Ob0 /Z7 /EHsc', executable_sha256=hashlib.sha256(raw).hexdigest(),
                  checks=['exact output and full state bytes', 'write footprint', 'stack and preserved registers', 'x87 precision preservation', 'all four basis modes and default', 'real recovered helper chain'],
                  limitations=['Finite synthetic inputs', 'Surface and angle data remain retail inputs', 'Original conversion helper still required', 'Full wheel update and vehicle mover remain incomplete'])
    (BUILD/'wheel-force-correction-verification.json').write_text(json.dumps(report, indent=2)+'\n')
    print(json.dumps(report, indent=2))


if __name__ == '__main__': main()
