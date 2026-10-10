#!/usr/bin/env python3
"""Execute original/rebuilt wheel feedback consumers; no callee fixtures."""
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
from verify_pc_drift_ranking import ADDRESSES, coff_function

BUILD = ROOT / 'build/analysis/racing'
FUNCTIONS = {
    '?GetWheelDerivedRate@Drivetrain@@QAEMXZ': ('DrivetrainWheelRate', 0x594840, 197, '638a2b166683ab9ffb69868ca6221fdf9a448ca0abdaed9ce724dd25cbf7b402'),
    '?GetAverageWheelScalar@Drivetrain@@QAEMXZ': ('DrivetrainWheelScalar', 0x5947c0, 115, 'eff0ab720277335bce2ddd15bc0aeecc5c82f3cca77fa45ac4508f825ddbe466'),
}
CONSTANTS = {'__real@00000000': 0x7d7d3c, '__real@3f000000': 0x784260,
             '__real@3a83126f': 0x78435c, '__real@3f7fbe77': 0x79b158}


def main():
    BUILD.mkdir(parents=True, exist_ok=True)
    raw = EXE.read_bytes()
    pe = pefile.PE(data=raw)
    image, base = pe.get_memory_mapped_image(), pe.OPTIONAL_HEADER.ImageBase
    assert base == 0x400000
    ADDRESSES.update(CONSTANTS)
    bodies, records = {}, []
    for symbol, (unit, address, size, digest) in FUNCTIONS.items():
        original = image[address-base:address-base+size]
        assert hashlib.sha256(original).hexdigest() == digest
        obj = BUILD / (unit + '.obj')
        subprocess.run([str(ROOT/'build/tools/wibo'), 'build/compilers/Win32/7.1/cl.exe', '/nologo', '/c', '/Ox', '/Ob0', '/Z7', '/EHsc',
                        '/Fo'+str(obj.relative_to(ROOT)), 'src/Speed/Indep/Src/Physics/'+unit+'.cpp'], cwd=ROOT, check=True)
        body, relocations = coff_function(obj, symbol)
        bodies[symbol] = body
        records.append(dict(symbol=symbol, original_address=hex(address), original_size=size, rebuilt_size=len(body),
                            original_sha256=digest, byte_identical_after_relocation=body == original, relocations=relocations))
    this, spec, wheels, wheel_specs, output, stack = 0x2000000, 0x2001000, 0x2002000, 0x2004000, 0x2005000, 0x200f000
    candidate, wrapper, stop = 0x2200000, 0x2202000, 0x2203000
    machines = []
    for rebuilt in (False, True):
        uc = Uc(UC_ARCH_X86, UC_MODE_32)
        uc.mem_map(base, (len(image)+4095) & ~4095)
        uc.mem_write(base, image)
        uc.mem_map(this, 0x10000)
        uc.mem_map(candidate, 0x4000)
        allowed = []
        for i, (symbol, body) in enumerate(bodies.items()):
            entry = candidate+i*0x1000 if rebuilt else FUNCTIONS[symbol][1]
            if rebuilt:
                uc.mem_write(entry, body)
            allowed.append((entry, entry+(len(body) if rebuilt else FUNCTIONS[symbol][2])))
        allowed.append((wrapper, wrapper+12))
        def guard(machine, address, size, context, allowed=allowed):
            assert any(a <= address < b for a, b in allowed), hex(address)
        uc.hook_add(UC_HOOK_CODE, guard)
        machines.append(uc)

    def run(uc, symbol, control, rebuilt):
        index = list(FUNCTIONS).index(symbol)
        entry = candidate+index*0x1000 if rebuilt else FUNCTIONS[symbol][1]
        code = b'\xe8'+struct.pack('<i', entry-wrapper-5)+b'\xd9\x1d'+struct.pack('<I', output)+b'\xc3'
        uc.mem_write(wrapper, code)
        uc.mem_write(output-16, b'\xa5'*36)
        uc.mem_write(stack, struct.pack('<I', stop))
        uc.reg_write(UC_X86_REG_ESP, stack)
        uc.reg_write(UC_X86_REG_ECX, this)
        uc.reg_write(UC_X86_REG_FPCW, control)
        saved = (UC_X86_REG_EBX, UC_X86_REG_ESI, UC_X86_REG_EDI, UC_X86_REG_EBP)
        for reg in saved:
            uc.reg_write(reg, 0x12345678)
        before = bytes(uc.mem_read(this, 0x5000))
        uc.emu_start(wrapper, stop, count=1000)
        assert uc.reg_read(UC_X86_REG_EIP) == stop
        assert uc.reg_read(UC_X86_REG_ESP) == stack+4
        assert all(uc.reg_read(reg) == 0x12345678 for reg in saved)
        assert bytes(uc.mem_read(this, 0x5000)) == before
        result = bytes(uc.mem_read(output-16, 36))
        assert result[:16] == b'\xa5'*16 and result[20:] == b'\xa5'*16
        return result[16:20]

    rng = random.Random(0x594840)
    selectors = [-0.5, 0, 0.5, 1, 1.5]
    for va in (0x78435c, 0x79b158):
        bits = struct.unpack_from('<I', image, va-base)[0]
        selectors += [struct.unpack('<f', struct.pack('<I', bits+d))[0] for d in (-1, 0, 1)]
    counts = {symbol: 0 for symbol in FUNCTIONS}
    for selector, ratio_index, control, trial in itertools.product(selectors, range(8), (0x027f, 0x037f), range(24)):
        state = bytearray(rng.randbytes(0x5000))
        def put(address, fmt, value):
            struct.pack_into(fmt, state, address-this, value)
        put(this+0x18, '<I', spec)
        put(this+0x3c, '<I', ratio_index)
        for address, value in ((spec+8, rng.uniform(0.5, 5)), (spec+12, rng.uniform(0.5, 5)), (spec+16, selector),
                               (spec+0x20+ratio_index*4, (-3.1, 0, 3.4, 2.1, 1.4, 1.1, 0.9, 0.75)[ratio_index])):
            put(address, '<f', value)
        for i in range(4):
            wheel, data = wheels+i*0x400, wheel_specs+i*0x100
            put(this+0x24+i*4, '<I', wheel)
            put(wheel+0x14, '<I', data)
            for address, value in ((data+0x10, rng.uniform(-5, 5)), (wheel+0x108, rng.uniform(-10000, 10000)),
                                   (wheel+0x124, rng.uniform(-500, 500)), (wheel+0x2c, rng.uniform(0.1, 100)),
                                   (wheel+0x16c, rng.uniform(-500, 500))):
                put(address, '<f', value if trial else 0.0 if address != wheel+0x2c else 1.0)
        for uc in machines:
            uc.mem_write(this, bytes(state))
        for symbol in FUNCTIONS:
            results = [run(uc, symbol, control, rebuilt) for uc, rebuilt in zip(machines, (False, True))]
            assert results[0] == results[1], (symbol, selector, ratio_index, control, trial, [r.hex() for r in results])
            counts[symbol] += 1
    report = dict(executable_sha256=hashlib.sha256(raw).hexdigest(), functions=records, comparisons=counts,
                  compiler='MSVC 7.1 /Ox /Ob0 /Z7 /EHsc', stubbed_callees=[],
                  checks=['exact binary32 return', 'whole input memory unchanged', 'stack balance', 'callee-saved registers', 'bounded instruction execution'],
                  limitations=['Finite synthetic inputs and nonzero wheel divisors', 'Physical units unresolved', 'Not a complete contact or vehicle solver'])
    (BUILD/'drivetrain-wheel-feedback-verification.json').write_text(json.dumps(report, indent=2)+'\n')
    print(json.dumps(report, indent=2))


if __name__ == '__main__':
    main()
