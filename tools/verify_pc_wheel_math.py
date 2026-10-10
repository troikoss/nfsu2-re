#!/usr/bin/env python3
"""Execute original/rebuilt wheel correction math, including output aliases."""
import hashlib
import itertools
import json
import random
import struct
import subprocess
import pefile
from unicorn import Uc, UC_ARCH_X86, UC_MODE_32, UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_ESP, UC_X86_REG_EIP, UC_X86_REG_EBX, UC_X86_REG_ESI, UC_X86_REG_EDI, UC_X86_REG_EBP, UC_X86_REG_FPCW
from probe_pc_race_reward import ROOT, EXE
from pc_coff import link_objects, read_object
from verify_pc_drift_ranking import ADDRESSES, coff_function

BUILD = ROOT/'build/analysis/racing'
ORIGINAL = {
    'WheelIdentity': (0x43c780, 59, '70a173c3844b12f15cdc5190c3b14976d553a4d12fac9f561e0860bc9a7986dc'),
    'WheelTranspose': (0x43c9d0, 215, '6cdc4a130b7c92cac533b81ebb2511222f047abee1fdcd04997e8678d130e6b4'),
    'WheelTransform': (0x441000, 100, '362c03a20d26f777a9ff4eb3020755b805c0ed44dfb4c8d922a661891cd7f178'),
    'WheelScaleAdd': (0x43d0c0, 51, '9538c21066a1769643d8425d5374171b0be62f2ade655cfd2619579cdbf8699e'),
    'WheelNormalize': (0x43cf10, 113, '23ba23dfc72112b19545c124d27a47e20d0d98f9f5b58e7513da66fb4b68a1c7'),
    'WheelCross': (0x43d1e0, 61, '8c904d0caf0b06c2b61ee571dac3f5d6b442833e6603da1cae3eaad1a449800d'),
}


def main():
    raw = EXE.read_bytes()
    pe = pefile.PE(data=raw)
    image, base = pe.get_memory_mapped_image(), pe.OPTIONAL_HEADER.ImageBase
    obj = BUILD/'WheelMath.obj'
    subprocess.run([str(ROOT/'build/tools/wibo'), 'build/compilers/Win32/7.1/cl.exe', '/nologo', '/c', '/Ox', '/Ob0', '/Z7', '/EHsc',
                    '/Fo'+str(obj.relative_to(ROOT)), 'src/Speed/Indep/Src/Physics/WheelMath.cpp'], cwd=ROOT, check=True)
    symbols = {}
    for name in ORIGINAL:
        matches = [s['name'] for s in read_object(obj)['symbols'].values() if s['name'].startswith('?'+name+'@@')]
        assert len(matches) == 1, (name, matches)
        symbols[name] = matches[0]
    linked = link_objects([obj], {})
    ADDRESSES.update({'__real@3f800000': 0x784250, '__real@00000000': 0x7d7d3c})
    records = []
    for name, (address, size, digest) in ORIGINAL.items():
        original = image[address-base:address-base+size]
        assert hashlib.sha256(original).hexdigest() == digest
        body, relocs = coff_function(obj, symbols[name])
        records.append(dict(name=name, symbol=symbols[name], original_address=hex(address), original_size=size,
                            original_sha256=digest, rebuilt_size=len(body), byte_identical_after_relocation=body == original, relocations=relocs))
    arena, matrix, a, b, output, stack, stop = 0x2000000, 0x2001000, 0x2002000, 0x2002100, 0x2003000, 0x200f000, 0x2100000
    state_size = 0x4000
    machines, entries = [], []
    for rebuilt in (False, True):
        uc = Uc(UC_ARCH_X86, UC_MODE_32)
        uc.mem_map(base, (len(image)+4095) & ~4095)
        uc.mem_write(base, image)
        uc.mem_map(arena, 0x10000)
        uc.mem_map(stop, 0x1000)
        uc.mem_map(linked['address'], linked['allocation'])
        if rebuilt:
            for address, body in linked['blocks']:
                uc.mem_write(address, body)
        ranges = linked['ranges'] if rebuilt else [(address, address+size) for address, size, _ in ORIGINAL.values()]
        def guard(machine, address, size, user, ranges=ranges):
            assert any(start <= address < end for start, end in ranges), hex(address)
        uc.hook_add(UC_HOOK_CODE, guard)
        machines.append(uc)
        entries.append({name: linked['exports'][symbol] for name, symbol in symbols.items()} if rebuilt else {name: data[0] for name, data in ORIGINAL.items()})
    rng = random.Random(0x59d600)
    counts = {name: 0 for name in ORIGINAL}
    for name, precision, alias, trial in itertools.product(ORIGINAL, (0x027f, 0x037f), range(4), range(256)):
        state = bytearray(rng.randbytes(state_size))
        data = [rng.uniform(-100, 100) for _ in range(16)]
        av = [rng.uniform(-1e4, 1e4) for _ in range(3)]
        bv = [rng.uniform(-1e4, 1e4) for _ in range(3)]
        if trial%16 == 0:
            av = [0, 0, 0]
        elif trial%16 == 1:
            av = [1, 0, 0]
        elif trial%16 == 2:
            av = [1e-10, -1e-10, 1e-10]
        struct.pack_into('<16f', state, matrix-arena, *data)
        struct.pack_into('<3f', state, a-arena, *av)
        struct.pack_into('<3f', state, b-arena, *bv)
        target = (output, a, b, a+4)[alias]
        if name in ('WheelIdentity', 'WheelTranspose'):
            target = (output, matrix, matrix+4, matrix-4)[alias]
            args = [target] if name == 'WheelIdentity' else [target, matrix]
            written_size = 64
        elif name == 'WheelTransform':
            target = (output, a, matrix, a+4)[alias]
            args, written_size = [target, matrix, a], 12
        elif name == 'WheelScaleAdd':
            args, written_size = [target, a, b], 12
        elif name == 'WheelNormalize':
            args, written_size = [target, a], 12
        else:
            args, written_size = [target, a, b], 12
        packed = struct.pack('<'+'I'*len(args), *args)
        if name == 'WheelScaleAdd':
            packed += struct.pack('<f', rng.uniform(-100, 100))
        results = []
        for uc, entry in zip(machines, entries):
            uc.mem_write(arena, bytes(state))
            uc.mem_write(stack, struct.pack('<I', stop)+packed)
            uc.reg_write(UC_X86_REG_ESP, stack)
            uc.reg_write(UC_X86_REG_FPCW, precision)
            saved = (UC_X86_REG_EBX, UC_X86_REG_ESI, UC_X86_REG_EDI, UC_X86_REG_EBP)
            for reg in saved:
                uc.reg_write(reg, 0x12345678)
            uc.emu_start(entry[name], stop, count=2000)
            assert uc.reg_read(UC_X86_REG_EIP) == stop
            assert uc.reg_read(UC_X86_REG_ESP) == stack+4
            assert all(uc.reg_read(reg) == 0x12345678 for reg in saved)
            result = bytes(uc.mem_read(arena, state_size))
            restored = bytearray(result)
            offset = target-arena
            restored[offset:offset+written_size] = state[offset:offset+written_size]
            assert restored == state
            results.append(result)
        if results[0] != results[1]:
            differences = [(hex(arena+i), results[0][i:i+4].hex(), results[1][i:i+4].hex()) for i in range(0, state_size, 4) if results[0][i:i+4] != results[1][i:i+4]]
            raise AssertionError((name, precision, alias, trial, differences))
        counts[name] += 1
    report = dict(executable_sha256=hashlib.sha256(raw).hexdigest(), functions=records, comparisons=counts,
                  compiler='MSVC 7.1 /Ox /Ob0 /Z7 /EHsc', stubbed_callees=[],
                  checks=['whole state exact', 'writes restricted to result', 'input/output aliases and partial overlaps', 'stack and preserved registers'],
                  limitations=['Finite normal inputs plus zero/small vectors', 'Not full wheel correction'])
    (BUILD/'wheel-math-verification.json').write_text(json.dumps(report, indent=2)+'\n')
    print(json.dumps(report, indent=2))


if __name__ == '__main__':
    main()
