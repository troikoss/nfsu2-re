#!/usr/bin/env python3
"""Verify original pair interpolation and its actual call boundary."""
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
from pc_coff import link_objects, read_object
from verify_pc_drift_ranking import ADDRESSES, coff_function

BUILD = ROOT/'build/analysis/racing'
EVALUATE = '?Evaluate@DriveFeedbackCurve@@QAEXPAMM@Z'
ORIGINAL_CURVE = (0x40c840, 195, '83eec8b8e85ad52c533955f72f37c36f636236bf99e60dbb306626faa1f2cc8b')
ORIGINAL_LERP = (0x576740, 37, 'ba1420cd04c9483da32a1602ba546cb0cd2041304a0cb08befd5e652f2e4b545')


def main():
    raw = EXE.read_bytes()
    pe = pefile.PE(data=raw)
    image, base = pe.get_memory_mapped_image(), pe.OPTIONAL_HEADER.ImageBase
    obj = BUILD/'DriveFeedbackCurve.obj'
    subprocess.run([str(ROOT/'build/tools/wibo'), 'build/compilers/Win32/7.1/cl.exe', '/nologo', '/c', '/Ox', '/Ob0', '/Z7', '/EHsc',
                    '/Fo'+str(obj.relative_to(ROOT)), 'src/Speed/Indep/Src/Physics/DriveFeedbackCurve.cpp'], cwd=ROOT, check=True)
    symbols = [s['name'] for s in read_object(obj)['symbols'].values() if s['name'].startswith('?InterpolateDriveFeedback@@')]
    assert len(symbols) == 1
    lerp = symbols[0]
    original = {EVALUATE: ORIGINAL_CURVE, lerp: ORIGINAL_LERP}
    linked = link_objects([obj], {})
    ADDRESSES.update({symbol: data[0] for symbol, data in original.items()})
    ADDRESSES.update({'__real@3f800000': 0x784250})
    records = []
    for symbol, (address, size, digest) in original.items():
        assert hashlib.sha256(image[address-base:address-base+size]).hexdigest() == digest
        body, relocs = coff_function(obj, symbol)
        records.append(dict(symbol=symbol, original_address=hex(address), original_size=size, original_sha256=digest,
                            rebuilt_size=len(body), byte_identical_after_relocation=body == image[address-base:address-base+size], relocations=relocs))
    this, pairs, output, stack, stop = 0x2000000, 0x2001000, 0x2002000, 0x200f000, 0x2100000
    machines, entries = [], []
    for rebuilt in (False, True):
        uc = Uc(UC_ARCH_X86, UC_MODE_32)
        uc.mem_map(base, (len(image)+4095) & ~4095)
        uc.mem_write(base, image)
        uc.mem_map(this, 0x10000)
        uc.mem_map(stop, 0x1000)
        uc.mem_map(linked['address'], linked['allocation'])
        if rebuilt:
            for address, data in linked['blocks']:
                uc.mem_write(address, data)
        ranges = linked['ranges'] if rebuilt else [(a, a+n) for a, n, _ in original.values()]
        def guard(machine, address, size, user, ranges=ranges):
            assert any(a <= address < b for a, b in ranges), hex(address)
        uc.hook_add(UC_HOOK_CODE, guard)
        machines.append(uc)
        entries.append(linked['exports'] if rebuilt else {symbol: data[0] for symbol, data in original.items()})
    def run(uc, entry, precision, args):
        uc.mem_write(output-16, b'\xa5'*36)
        uc.mem_write(stack, struct.pack('<I', stop)+args)
        uc.reg_write(UC_X86_REG_ESP, stack)
        uc.reg_write(UC_X86_REG_ECX, this)
        uc.reg_write(UC_X86_REG_FPCW, precision)
        saved = (UC_X86_REG_EBX, UC_X86_REG_ESI, UC_X86_REG_EDI, UC_X86_REG_EBP)
        for reg in saved:
            uc.reg_write(reg, 0x12345678)
        before = bytes(uc.mem_read(this, 0x2000))
        uc.emu_start(entry, stop, count=2000)
        assert uc.reg_read(UC_X86_REG_EIP) == stop
        assert uc.reg_read(UC_X86_REG_ESP) == stack+4+len(args)
        assert all(uc.reg_read(reg) == 0x12345678 for reg in saved)
        assert bytes(uc.mem_read(this, 0x2000)) == before
        result = bytes(uc.mem_read(output-16, 36))
        assert result[:16] == b'\xa5'*16 and result[20:] == b'\xa5'*16
        return result[16:20]
    rng, curve_cases, lerp_cases = random.Random(0x40c840), 0, 0
    def neighbors(value):
        if value == 0:
            return [-1e-40, 0, 1e-40]
        bits = struct.unpack('<I', struct.pack('<f', value))[0]
        return [struct.unpack('<f', struct.pack('<I', bits+d))[0] for d in (-1, 0, 1)]
    for count, precision, trial in itertools.product((-1, 0, 1, 2, 3, 5, 9), (0x027f, 0x037f), range(16)):
        coordinates = [float(i*250-500) for i in range(max(1, count))]
        values = [rng.uniform(-1000, 1000) for _ in coordinates]
        data = b''.join(struct.pack('<ff', x, y) for x, y in zip(coordinates, values))
        for uc in machines:
            uc.mem_write(this, struct.pack('<Ii', pairs, count))
            uc.mem_write(pairs, data)
        inputs = [-10000, 10000, rng.uniform(-1000, 3000)]
        for x in coordinates:
            inputs += neighbors(x)
        for value in inputs:
            args = struct.pack('<If', output, value)
            results = [run(uc, entry[EVALUATE], precision, args) for uc, entry in zip(machines, entries)]
            assert results[0] == results[1], (count, precision, value, results)
            if count <= 0:
                assert results[0] == b'\xa5'*4
            curve_cases += 1
    for fraction, precision, trial in itertools.product((-2, 0, 1e-7, 0.1, 0.5, 0.9999999, 1, 2), (0x027f, 0x037f), range(128)):
        previous, following = rng.uniform(-1e8, 1e8), rng.uniform(-1e8, 1e8)
        for uc in machines:
            uc.mem_write(pairs, struct.pack('<ff', previous, following))
        args = struct.pack('<IIIf', output, pairs+4, pairs, fraction)
        results = [run(uc, entry[lerp], precision, args) for uc, entry in zip(machines, entries)]
        assert results[0] == results[1], (fraction, precision, results)
        lerp_cases += 1
    report = dict(executable_sha256=hashlib.sha256(raw).hexdigest(), functions=records, curve_cases=curve_cases, interpolation_cases=lerp_cases,
                  compiler='MSVC 7.1 /Ox /Ob0 /Z7 /EHsc', stubbed_callees=[],
                  checks=['exact output', 'input memory preserved', 'empty tables preserve output', 'guard bytes', 'stack and preserved registers'],
                  limitations=['Finite inputs and sorted distinct coordinates', 'Not complete drivetrain finishing'])
    (BUILD/'drive-feedback-curve-verification.json').write_text(json.dumps(report, indent=2)+'\n')
    print(json.dumps(report, indent=2))


if __name__ == '__main__':
    main()
