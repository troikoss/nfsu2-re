#!/usr/bin/env python3
"""Compare UG2's quantized angle helper, retaining its real conversion runtime."""
import hashlib
import itertools
import json
import math
import random
import struct
import subprocess
import pefile
from unicorn import Uc, UC_ARCH_X86, UC_MODE_32, UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EAX, UC_X86_REG_ESP, UC_X86_REG_EIP, UC_X86_REG_EBX, UC_X86_REG_ESI, UC_X86_REG_EDI, UC_X86_REG_EBP, UC_X86_REG_FPCW
from probe_pc_race_reward import ROOT, EXE
from verify_pc_engine import CONVERSION
from verify_pc_drift_ranking import ADDRESSES, coff_function

BUILD = ROOT/'build/analysis/racing'
SYMBOL = '?WheelAngle@@YAGMM@Z'
ADDRESS, SIZE, DIGEST = 0x43c570, 218, '6881379cb2c8626ffbf1ad4492e2b4c26001033cd857bb1f68ebe4d64144be1c'
TABLE, TABLE_SIZE = 0x7eacb0, 257
TABLE_DIGEST = '102c6e653b6de83cffed0c10f96f067e84edc027adf2025c895602c8f4995afb'


def main():
    raw = EXE.read_bytes()
    pe = pefile.PE(data=raw)
    image, base = pe.get_memory_mapped_image(), pe.OPTIONAL_HEADER.ImageBase
    assert base == 0x400000
    assert hashlib.sha256(raw).hexdigest() == 'f9dd86c054878ce6276beb07c1fd61874f7a1e4bf1f241b084c65b73e24168a7'
    for address, size, digest in ((ADDRESS, SIZE, DIGEST), CONVERSION, (TABLE, TABLE_SIZE, TABLE_DIGEST)):
        assert hashlib.sha256(image[address-base:address-base+size]).hexdigest() == digest
    obj = BUILD/'WheelAngle.obj'
    subprocess.run([str(ROOT/'build/tools/wibo'), 'build/compilers/Win32/7.1/cl.exe', '/nologo', '/c', '/Ox', '/Ob0', '/Z7', '/EHsc',
                    '/Fo'+str(obj.relative_to(ROOT)), 'src/Speed/Indep/Src/Physics/WheelAngle.cpp'], cwd=ROOT, check=True)
    ADDRESSES[SYMBOL] = ADDRESS
    ADDRESSES.update({'?wheelAngleTable@@3PAEA': TABLE, '?wheelAngleRatioScale@@3MA': 0x786db8,
                      '__ftol2': CONVERSION[0], '__real@00000000': 0x7d7d3c})
    relocated_original_body, relocs = coff_function(obj, SYMBOL)
    ADDRESSES[SYMBOL] = 0x2200000
    body = coff_function(obj, SYMBOL)[0]
    arena, stack, candidate, stop = 0x2000000, 0x200f000, 0x2200000, 0x2202000
    machines = []
    for rebuilt in (False, True):
        uc = Uc(UC_ARCH_X86, UC_MODE_32)
        uc.mem_map(base, (len(image)+4095) & ~4095)
        uc.mem_write(base, image)
        uc.mem_map(arena, 0x10000)
        uc.mem_map(candidate, 0x3000)
        entry = candidate if rebuilt else ADDRESS
        if rebuilt:
            uc.mem_write(entry, body)
        ranges = [(entry, entry+(len(body) if rebuilt else SIZE)), (CONVERSION[0], CONVERSION[0]+CONVERSION[1])]
        def guard(machine, address, size, user, ranges=ranges):
            assert any(a <= address < b for a, b in ranges), hex(address)
        uc.hook_add(UC_HOOK_CODE, guard)
        machines.append(uc)
    def run(uc, entry, x, y, precision):
        uc.mem_write(stack-128, b'\xa5'*160)
        uc.mem_write(stack, struct.pack('<Iff', stop, x, y))
        uc.reg_write(UC_X86_REG_ESP, stack)
        uc.reg_write(UC_X86_REG_FPCW, precision)
        saved = (UC_X86_REG_EBX, UC_X86_REG_ESI, UC_X86_REG_EDI, UC_X86_REG_EBP)
        for reg in saved:
            uc.reg_write(reg, 0x12345678)
        uc.emu_start(entry, stop, count=2000)
        assert uc.reg_read(UC_X86_REG_EIP) == stop
        assert uc.reg_read(UC_X86_REG_ESP) == stack+4
        assert all(uc.reg_read(reg) == 0x12345678 for reg in saved)
        assert uc.reg_read(UC_X86_REG_FPCW) == precision
        # Original overwrites its first argument when taking its absolute value.
        # cdecl's parameter slots are callee scratch, not read-only inputs.
        for offset, value in ((4, x), (8, y)):
            assert bytes(uc.mem_read(stack+offset, 4)) in (struct.pack('<f', value), struct.pack('<f', abs(value)))
        assert bytes(uc.mem_read(stack+12, 20)) == b'\xa5'*20
        return uc.reg_read(UC_X86_REG_EAX) & 0xffff
    def neighbors(value):
        if value == 0:
            return [0, 1e-40]
        bits = struct.unpack('<I', struct.pack('<f', value))[0]
        return [struct.unpack('<f', struct.pack('<I', bits+d))[0] for d in (-1, 0, 1)]
    inputs = [(x, y) for x, y in itertools.product((-1, -0.0, 0, 1), repeat=2)]
    for knot in range(257):
        for ratio, sign_x, sign_y, swap in itertools.product(neighbors(knot/256), (-1, 1), (-1, 1), (False, True)):
            if ratio < 0 or ratio > 1:
                continue
            x, y = (ratio, 1) if swap else (1, ratio)
            inputs.append((x*sign_x, y*sign_y))
    rng = random.Random(0x43c570)
    inputs += [(math.ldexp(rng.uniform(-1, 1), rng.randrange(-30, 31)), math.ldexp(rng.uniform(-1, 1), rng.randrange(-30, 31))) for _ in range(2048)]
    count = 0
    for (x, y), precision in itertools.product(inputs, (0x027f, 0x037f)):
        results = [run(uc, candidate if rebuilt else ADDRESS, x, y, precision) for uc, rebuilt in zip(machines, (False, True))]
        assert results[0] == results[1], (x, y, precision, results)
        count += 1
    report = dict(executable_sha256=hashlib.sha256(raw).hexdigest(), original_address=hex(ADDRESS), original_size=SIZE,
                  original_sha256=DIGEST, rebuilt_size=len(body), byte_identical_after_relocation=relocated_original_body == image[ADDRESS-base:ADDRESS-base+SIZE],
                  compiler='MSVC 7.1 /Ox /Ob0 /Z7 /EHsc', comparisons=count, relocations=relocs, stubbed_callees=[],
                  original_runtime_dependency=CONVERSION, table_range=dict(address=hex(TABLE), size=TABLE_SIZE, sha256=TABLE_DIGEST),
                  checks=['exact low-16-bit return', 'all quadrants and axes', 'lookup-bin neighbors', 'argument slot writes limited to observed absolute-value scratch', 'stack/register/precision preservation'],
                  limitations=['Finite inputs', 'High EAX bits are not the return contract', 'Original conversion helper and mapped lookup data still required', 'Not full wheel correction'])
    (BUILD/'wheel-angle-verification.json').write_text(json.dumps(report, indent=2)+'\n')
    print(json.dumps(report, indent=2))


if __name__ == '__main__':
    main()
