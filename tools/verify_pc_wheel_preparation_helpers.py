#!/usr/bin/env python3
"""Compare original wheel preparation helpers with rebuilt real curve calls."""
import hashlib
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

BUILD = ROOT/'build/analysis/racing'
LOCK = '?UpdateLockState@DrivetrainWheel@@QAEXM@Z'
LATERAL = '?EvaluateLateralForce@DrivetrainWheel@@QAEMMM@Z'
ORIGINAL = {LOCK: (0x59d400, 133, '1a60de089b5dc3883d95b3f3ab5813e59fe11852314161404f2c3c2e8989ec19'),
            LATERAL: (0x59d510, 225, '08709c1a9d3871eb658f86f1c361890e9dd6d6d6d018d139943919d308c1adca'),
            CURVE: ENGINE_ORIGINAL[CURVE]}
GLOBALS = {'wheelPreparationScale4': 0x7fc708, 'wheelPreparationScale8': 0x7fc70c,
           'wheelLockSumScale': 0x7fc710, 'wheelLockValueScale': 0x7fc714,
           'wheelForceLimitScale': 0x7fc718, 'wheelLateralInputScale': 0x7fc7f8,
           'wheelLateralOutputScale': 0x7fc7f4, 'wheelLateralFallbackScale': 0x7fc740,
           'wheelLateralAngleScale': 0x78424c, 'wheelLateralIndexScale': 0x78821c}


def main():
    raw = EXE.read_bytes()
    assert hashlib.sha256(raw).hexdigest() == 'f9dd86c054878ce6276beb07c1fd61874f7a1e4bf1f241b084c65b73e24168a7'
    pe = pefile.PE(data=raw); image, base = pe.get_memory_mapped_image(), pe.OPTIONAL_HEADER.ImageBase
    for address, size, digest in (*ORIGINAL.values(), CONVERSION):
        assert hashlib.sha256(image[address-base:address-base+size]).hexdigest() == digest
    paths = []
    for unit in ('WheelPreparationHelpers', 'EngineCurve'):
        obj = BUILD/(unit+'.obj')
        subprocess.run([str(ROOT/'build/tools/wibo'), 'build/compilers/Win32/7.1/cl.exe', '/nologo', '/c', '/Ox', '/Ob0', '/Z7', '/EHsc',
                        '/Fo'+str(obj.relative_to(ROOT)), 'src/Speed/Indep/Src/Physics/'+unit+'.cpp'], cwd=ROOT, check=True)
        paths.append(obj)
    externals = {'?'+name+'@@3MA': address for name, address in GLOBALS.items()}
    externals.update({'?wheelLateralMode@@3HA': 0x7fc7f0,
                      '?wheelLateralFallbackCurve@@3UEngineCurve@@A': 0x7fc954,
                      '?wheelLateralCurves@@3PAPAUEngineCurve@@A': 0x7fc7d4,
                      '?wheelLateralLastCurve@@3PAUEngineCurve@@A': 0x7fc7ec, '__ftol2': CONVERSION[0]})
    linked = link_objects(paths, externals)
    this, spec, correction, prep, curves, samples, output, stack, wrapper, stop = (
        0x2000000, 0x2001000, 0x2002000, 0x2003000, 0x2004000, 0x2005000, 0x2008000, 0x200f000, 0x2101000, 0x2102000)
    machines = []
    branch_sites = {0x59d45a: 'locked_zero', 0x59d46a: 'unlocked_zero', 0x59d476: 'unlocked_preserve',
                    0x59d51d: 'fallback_curve', 0x59d585: 'last_curve', 0x59d5a8: 'interpolated_curves'}
    visits = {}
    for rebuilt in (False, True):
        uc = Uc(UC_ARCH_X86, UC_MODE_32)
        uc.mem_map(base, (len(image)+4095) & ~4095); uc.mem_write(base, image)
        uc.mem_map(this, 0x10000); uc.mem_map(wrapper, 0x2000)
        uc.mem_map(linked['address'], linked['allocation'])
        if rebuilt:
            for address, body in linked['blocks']: uc.mem_write(address, body)
        entry = linked['exports'][LATERAL] if rebuilt else ORIGINAL[LATERAL][0]
        code = b'\xe8'+struct.pack('<i', entry-wrapper-5)+b'\xd9\x1d'+struct.pack('<I', output)+b'\xc3'
        # thiscall target sees arguments beyond the wrapper return address.
        # Shift the wrapper stack down one word when testing the float return.
        uc.mem_write(wrapper, code)
        ranges = (linked['ranges'] if rebuilt else [(a, a+n) for a, n, _ in ORIGINAL.values()]) + [(CONVERSION[0], CONVERSION[0]+CONVERSION[1]), (wrapper, wrapper+len(code))]
        def guard(machine, address, size, user, ranges=ranges, rebuilt=rebuilt):
            assert any(a <= address < b for a, b in ranges), hex(address)
            if not rebuilt and address in branch_sites:
                name = branch_sites[address]; visits[name] = visits.get(name, 0)+1
        uc.hook_add(UC_HOOK_CODE, guard)
        machines.append(uc)
    rng = random.Random(0x59d510)
    counts = {LOCK: 0, LATERAL: 0}
    for trial in range(2304):
        state = bytearray(b'\xa5'*0x7000)
        def f(offset, value): struct.pack_into('<f', state, offset, value)
        def i(offset, value): struct.pack_into('<i', state, offset, value)
        i(0x14, spec); i(0x18, correction); i(0x1c, prep)
        value = (-10, -1, -0.0, 0, 1, 10)[trial%6]
        f(0x28, value); i(0x10c, trial%3)
        radius = rng.uniform(0.1, 2); f(0x1010, radius)
        f(0x3004, (0, 0.1, 2, 100)[trial%4]); f(0x3008, (0, 0.1, 2, 100)[(trial//4)%4])
        f(0x178, rng.uniform(0, 2)); f(0x17c, rng.uniform(0, 2)); f(0x164, rng.uniform(0, 100))
        angle_limit = rng.uniform(0.1, 3)
        f(0x2000, angle_limit); f(0x2004, rng.uniform(0.1, 3)); f(0x2008, rng.uniform(0.1, 3))
        curve_records = []
        for index in range(8):
            curve, data, count = curves+index*0x100, samples+index*0x100, (1, 2, 5, 9)[(trial+index)%4]
            start, spacing = rng.choice((-10, 0, 1)), rng.choice((0.1, 1, 4))
            record = struct.pack('<ifffI', count, start, start+(count-1)/spacing, spacing, data)
            state[curve-this:curve-this+len(record)] = record
            for n in range(count): f(data-this+n*4, rng.uniform(-100, 100))
            curve_records.append(record)
        scales = {name: rng.choice((0.1, 0.5, 1, 2)) for name in GLOBALS}
        # Keep the real angle/index conversion constants from the retail image.
        for name in ('wheelLateralAngleScale', 'wheelLateralIndexScale'):
            scales[name] = struct.unpack_from('<f', image, GLOBALS[name]-base)[0]
        position = (0, 0.1, 0.999, 1, 1.001, 3.5, 5.999, 6, 6.001, 10)[trial%10]
        angle = position*angle_limit/(scales['wheelLateralAngleScale']*scales['wheelLateralIndexScale']*0.5)
        lock_input, lateral_input = rng.choice((0, 0.5, 1, 50)), rng.uniform(-10, 100)
        for precision in (0x027f, 0x037f):
            for symbol, args in ((LOCK, (lock_input,)), (LATERAL, (lateral_input, angle))):
                results = []
                for rebuilt, uc in zip((False, True), machines):
                    uc.mem_write(this, bytes(state))
                    for name, val in scales.items(): uc.mem_write(GLOBALS[name], struct.pack('<f', val))
                    uc.mem_write(0x7fc7f0, struct.pack('<i', trial%3))
                    uc.mem_write(0x7fc954, curve_records[7])
                    for j in range(7): uc.mem_write(0x7fc7d4+j*4, struct.pack('<I', curves+j*0x100))
                    uc.mem_write(output-16, b'\xa5'*36)
                    entry = linked['exports'][symbol] if rebuilt else ORIGINAL[symbol][0]
                    uc.reg_write(UC_X86_REG_ECX, this); uc.reg_write(UC_X86_REG_FPCW, precision)
                    saved = (UC_X86_REG_EBX, UC_X86_REG_ESI, UC_X86_REG_EDI, UC_X86_REG_EBP)
                    for reg in saved: uc.reg_write(reg, 0x12345678)
                    if symbol == LATERAL:
                        # CALL creates the target's return slot; wrapper owns the final slot.
                        uc.mem_write(stack, struct.pack('<IffI', 0x12345678, *args, stop))
                        uc.reg_write(UC_X86_REG_ESP, stack+4)
                        uc.emu_start(wrapper, stop, count=10000)
                    else:
                        uc.mem_write(stack, struct.pack('<If', stop, *args)); uc.reg_write(UC_X86_REG_ESP, stack)
                        uc.emu_start(entry, stop, count=10000)
                    assert uc.reg_read(UC_X86_REG_EIP) == stop
                    assert uc.reg_read(UC_X86_REG_ESP) == stack+(16 if symbol == LATERAL else 8)
                    assert uc.reg_read(UC_X86_REG_FPCW) == precision
                    assert all(uc.reg_read(reg) == 0x12345678 for reg in saved)
                    after = bytes(uc.mem_read(this, len(state)))
                    masked = bytearray(after)
                    if symbol == LOCK:
                        for offset in (0x28, 0x10c): masked[offset:offset+4] = state[offset:offset+4]
                    assert masked == state, ('unexpected write', symbol)
                    result = bytes(uc.mem_read(output-16, 36))
                    assert result[:16] == b'\xa5'*16 and result[20:] == b'\xa5'*16
                    results.append((after, result[16:20]))
                assert results[0] == results[1], (trial, symbol, precision, results[0][1].hex(), results[1][1].hex(),
                    [(hex(off), results[0][0][off:off+4].hex(), results[1][0][off:off+4].hex()) for off in (0x28, 0x10c) if results[0][0][off:off+4] != results[1][0][off:off+4]])
                counts[symbol] += 1
    assert set(visits) == set(branch_sites.values()), visits
    report = dict(executable_sha256=hashlib.sha256(raw).hexdigest(), compiler='MSVC 7.1 /Ox /Ob0 /Z7 /EHsc',
                  functions=[dict(symbol=s, original_address=hex(a), original_size=n, original_sha256=d,
                                  rebuilt_size=linked['functions'][s]['size']) for s, (a, n, d) in ORIGINAL.items() if s != CURVE],
                  comparisons=counts, branch_visits=visits, stubbed_callees=[], original_runtime_dependency=CONVERSION,
                  checks=['exact whole state and float return', 'write footprint and guards', 'stack/register/precision preservation', 'real original/rebuilt curve calls'],
                  limitations=['Finite synthetic records and curves', 'Nonnegative lateral angles only', 'Original conversion runtime still required', 'Wheel preparation caller remains unrecovered'])
    (BUILD/'wheel-preparation-helpers-verification.json').write_text(json.dumps(report, indent=2)+'\n')
    print(json.dumps(report, indent=2))


if __name__ == '__main__': main()
