#!/usr/bin/env python3
"""Compare the original wheel correction scaling helper with recovered source."""
import hashlib
import itertools
import json
import struct
import subprocess
import pefile
from unicorn import Uc, UC_ARCH_X86, UC_MODE_32, UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_ECX, UC_X86_REG_ESP, UC_X86_REG_EIP, UC_X86_REG_EBX, UC_X86_REG_ESI, UC_X86_REG_EDI, UC_X86_REG_EBP, UC_X86_REG_FPCW
from probe_pc_race_reward import ROOT, EXE
from verify_pc_drift_ranking import ADDRESSES, coff_function

BUILD = ROOT/'build/analysis/racing'
SYMBOL = '?GetCorrectionScale@DrivetrainWheel@@QAEMXZ'
ADDRESS, SIZE, DIGEST = 0x59d490, 128, '48b8135f47cd23d4d3b6f54ab02adfb296a7983c854b4a118f974de8867a16fb'
GLOBALS = {'?wheelCorrectionOffset@@3MA': 0x7a1eb0, '?wheelCorrectionRangeScale@@3MA': 0x7850fc,
           '?wheelCorrectionMinimum@@3MA': 0x7fc71c}


def main():
    raw = EXE.read_bytes()
    pe = pefile.PE(data=raw)
    image, base = pe.get_memory_mapped_image(), pe.OPTIONAL_HEADER.ImageBase
    original = image[ADDRESS-base:ADDRESS-base+SIZE]
    assert hashlib.sha256(original).hexdigest() == DIGEST
    obj = BUILD/'WheelCorrectionScale.obj'
    subprocess.run([str(ROOT/'build/tools/wibo'), 'build/compilers/Win32/7.1/cl.exe', '/nologo', '/c', '/Ox', '/Ob0', '/Z7', '/EHsc',
                    '/Fo'+str(obj.relative_to(ROOT)), 'src/Speed/Indep/Src/Physics/WheelCorrectionScale.cpp'], cwd=ROOT, check=True)
    ADDRESSES.update(GLOBALS)
    ADDRESSES.update({'__real@3f800000': 0x784250, '__real@00000000': 0x7d7d3c})
    body, relocs = coff_function(obj, SYMBOL)
    this, spec, aux, output, stack, candidate, wrapper, stop = 0x2000000, 0x2001000, 0x2002000, 0x2004000, 0x200f000, 0x2200000, 0x2201000, 0x2202000
    machines = []
    for rebuilt in (False, True):
        uc = Uc(UC_ARCH_X86, UC_MODE_32)
        uc.mem_map(base, (len(image)+4095) & ~4095)
        uc.mem_write(base, image)
        uc.mem_map(this, 0x10000)
        uc.mem_map(candidate, 0x3000)
        entry = candidate if rebuilt else ADDRESS
        if rebuilt:
            uc.mem_write(entry, body)
        code = b'\xe8'+struct.pack('<i', entry-wrapper-5)+b'\xd9\x1d'+struct.pack('<I', output)+b'\xc3'
        uc.mem_write(wrapper, code)
        ranges = [(entry, entry+(len(body) if rebuilt else SIZE)), (wrapper, wrapper+len(code))]
        def guard(machine, address, size, user, ranges=ranges):
            assert any(a <= address < b for a, b in ranges), hex(address)
        uc.hook_add(UC_HOOK_CODE, guard)
        uc.mem_write(this+0x14, struct.pack('<I', spec))
        uc.mem_write(this+0x20, struct.pack('<I', aux))
        machines.append(uc)
    def neighbors(value):
        bits = struct.unpack('<I', struct.pack('<f', value))[0]
        return [struct.unpack('<f', struct.pack('<I', bits+d))[0] for d in (-1, 0, 1)]
    offset = struct.unpack_from('<f', image, GLOBALS['?wheelCorrectionOffset@@3MA']-base)[0]
    range_scale = struct.unpack_from('<f', image, GLOBALS['?wheelCorrectionRangeScale@@3MA']-base)[0]
    values = [-100, 0, 100] + neighbors(offset) + neighbors(offset+1/range_scale)
    count = 0
    for flag, wheel_value, mode, value, minimum, precision in itertools.product((0, 1, 2), (-1, -0.0, 0, 1), (-1, 0, 1, 2, 3), values,
                                                                              (0, 0.85, 1), (0x027f, 0x037f)):
        results = []
        for uc in machines:
            uc.mem_write(this+0x10c, struct.pack('<i', flag))
            uc.mem_write(this+0x28, struct.pack('<f', wheel_value))
            uc.mem_write(spec+0x18, struct.pack('<i', mode))
            uc.mem_write(aux+0x3ec, struct.pack('<f', value))
            uc.mem_write(GLOBALS['?wheelCorrectionMinimum@@3MA'], struct.pack('<f', minimum))
            before = bytes(uc.mem_read(this, 0x3000))
            uc.mem_write(output-16, b'\xa5'*36)
            uc.mem_write(stack, struct.pack('<I', stop))
            uc.reg_write(UC_X86_REG_ESP, stack)
            uc.reg_write(UC_X86_REG_ECX, this)
            uc.reg_write(UC_X86_REG_FPCW, precision)
            saved = (UC_X86_REG_EBX, UC_X86_REG_ESI, UC_X86_REG_EDI, UC_X86_REG_EBP)
            for reg in saved:
                uc.reg_write(reg, 0x12345678)
            uc.emu_start(wrapper, stop, count=1000)
            assert uc.reg_read(UC_X86_REG_EIP) == stop
            assert uc.reg_read(UC_X86_REG_ESP) == stack+4
            assert all(uc.reg_read(reg) == 0x12345678 for reg in saved)
            assert bytes(uc.mem_read(this, 0x3000)) == before
            result = bytes(uc.mem_read(output-16, 36))
            assert result[:16] == b'\xa5'*16 and result[20:] == b'\xa5'*16
            results.append(result[16:20])
        assert results[0] == results[1], (flag, wheel_value, mode, value, minimum, precision, results)
        count += 1
    report = dict(executable_sha256=hashlib.sha256(raw).hexdigest(), original_address=hex(ADDRESS), original_size=SIZE,
                  original_sha256=DIGEST, rebuilt_size=len(body), byte_identical_after_relocation=body == original,
                  comparisons=count, compiler='MSVC 7.1 /Ox /Ob0 /Z7 /EHsc', relocations=relocs, stubbed_callees=[],
                  checks=['exact binary32 return', 'unchanged input state', 'guard bytes', 'stack and preserved registers'],
                  limitations=['Finite synthetic inputs', 'Wheel field units unresolved', 'Not full wheel correction'])
    (BUILD/'wheel-correction-scale-verification.json').write_text(json.dumps(report, indent=2)+'\n')
    print(json.dumps(report, indent=2))


if __name__ == '__main__':
    main()
