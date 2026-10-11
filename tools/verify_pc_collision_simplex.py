#!/usr/bin/env python3
"""Execute the original and rebuilt GJK simplex sub-algorithm 0x595040 (and the simplex
copies 0x598130/0x598240) on synthetic point sets and compare every output byte.

The rebuilt object is MSVC 7.1 (/Ox /Ob0); its vector helpers link to the original
0x43d1e0/0x43cf10/0x43d0c0, which execute on both sides. Behavioural evidence only."""
import hashlib
import itertools
import random
import struct
import subprocess
import pefile
from unicorn import Uc, UC_ARCH_X86, UC_MODE_32, UC_HOOK_CODE
from unicorn.x86_const import (UC_X86_REG_ESP, UC_X86_REG_EIP, UC_X86_REG_ECX, UC_X86_REG_EBX, UC_X86_REG_ESI,
                               UC_X86_REG_EDI, UC_X86_REG_EBP, UC_X86_REG_FPCW)
from probe_pc_race_reward import ROOT, EXE
from pc_coff import link_objects, read_object

BUILD = ROOT/'build/analysis/collision'
SOURCE = 'src/Speed/Indep/Src/Physics/Collision/CollisionSimplex.cpp'
ORIGINAL = {
    'Distance': (0x595040, 0x30d9, '?Distance@CollisionSimplex@@QAEMPAUWheelVector@@@Z'),
    'CopyFrom': (0x598130, 0x10b, '?CopyFrom@CollisionSimplex@@QAEXPAU1@@Z'),
    'Assign': (0x598240, 0x14f, '?Assign@CollisionSimplex@@QAEXPAU1@@Z'),
}
HELPERS = {'?WheelCross@@YAXPAUWheelVector@@00@Z': (0x43d1e0, 61),
           '?WheelNormalize@@YAXPAUWheelVector@@0@Z': (0x43cf10, 113),
           '?WheelScaleAdd@@YAXPAUWheelVector@@00M@Z': (0x43d0c0, 51),
           '?collisionParallelLimit@@3MA': (0x7a1d40, 0)}


def main():
    raw = EXE.read_bytes()
    pe = pefile.PE(data=raw)
    image, base = pe.get_memory_mapped_image(), pe.OPTIONAL_HEADER.ImageBase
    BUILD.mkdir(parents=True, exist_ok=True)
    obj = BUILD/'CollisionSimplex.obj'
    subprocess.run([str(ROOT/'build/tools/wibo'), 'build/compilers/Win32/7.1/cl.exe', '/nologo', '/c', '/Ox', '/Ob0', '/Z7',
                    '/EHsc', '/Fo'+str(obj.relative_to(ROOT)), SOURCE], cwd=ROOT, check=True, capture_output=True)
    linked = link_objects([obj], {name: address for name, (address, _) in HELPERS.items()})
    sizes = {s['name']: s['size'] for s in read_object(obj)['symbols'].values() if s['size']}
    records = []
    for name, (address, size, symbol) in ORIGINAL.items():
        original = image[address-base:address-base+size]
        records.append(dict(name=name, original=hex(address), original_size=size, rebuilt_size=sizes[symbol],
                            original_sha256=hashlib.sha256(original).hexdigest(), byte_matching='not claimed'))
    simplex, other, direction, output, stack, stop, wrapper = 0x2000000, 0x2001000, 0x2002000, 0x2003000, 0x200f000, 0x2100000, 0x2101000
    helper_ranges = [(address, address+size) for address, size in HELPERS.values() if size]
    machines, entries = [], []
    for rebuilt in (False, True):
        uc = Uc(UC_ARCH_X86, UC_MODE_32)
        uc.mem_map(base, (len(image)+4095) & ~4095)
        uc.mem_write(base, image)
        uc.mem_map(0x2000000, 0x10000)
        uc.mem_map(stop, 0x2000)
        uc.mem_map(linked['address'], linked['allocation'])
        for address, body in linked['blocks']:
            uc.mem_write(address, body)
        entry = {name: (linked['exports'][symbol] if rebuilt else address) for name, (address, _, symbol) in ORIGINAL.items()}
        # Probe-only wrapper: call the routine, store the x87 return as binary32.
        code = (b'\x68'+struct.pack('<I', direction)+b'\xe8'+struct.pack('<i', entry['Distance']-wrapper-10)
                +b'\xd9\x1d'+struct.pack('<I', output)+b'\xc3')
        uc.mem_write(wrapper, code)
        ranges = helper_ranges+[(wrapper, wrapper+len(code))]
        ranges += linked['ranges'] if rebuilt else [(address, address+size) for address, size, _ in ORIGINAL.values()]
        def guard(machine, address, size, user, ranges=ranges):
            if not any(start <= address < end for start, end in ranges):
                raise RuntimeError(f'Unexpected instruction at {address:#x}')
        uc.hook_add(UC_HOOK_CODE, guard)
        machines.append(uc)
        entries.append(entry)

    def call(uc, entry, target, argument, control, float_return):
        uc.mem_write(output, b'\xa5'*4)
        uc.mem_write(stack, struct.pack('<II', stop, argument))
        uc.reg_write(UC_X86_REG_ESP, stack)
        uc.reg_write(UC_X86_REG_ECX, simplex)
        uc.reg_write(UC_X86_REG_FPCW, control)
        saved = (UC_X86_REG_EBX, UC_X86_REG_ESI, UC_X86_REG_EDI, UC_X86_REG_EBP)
        for reg in saved:
            uc.reg_write(reg, 0x12345678)
        if float_return:
            uc.emu_start(wrapper, stop, count=200000)
        else:
            uc.emu_start(target, stop, count=20000)
        assert uc.reg_read(UC_X86_REG_EIP) == stop
        assert all(uc.reg_read(reg) == 0x12345678 for reg in saved)
        return bytes(uc.mem_read(output, 4))

    rng = random.Random(0x595040)
    cases = exact = 0
    failures = []

    def close(x, y):
        return x == y or abs(x-y) <= 1e-4*max(1.0, abs(x), abs(y))

    def compare(first, second):
        # Integers (indices, counts, started flag) must match; floats within rounding.
        ra, rb = struct.unpack('<f', first[0])[0], struct.unpack('<f', second[0])[0]
        if not close(ra, rb):
            return ('return', ra, rb)
        sa, sb = first[1], second[1]
        for offset in range(0xb0, 0xdc, 4):
            if sa[offset:offset+4] != sb[offset:offset+4]:
                return ('integer', hex(offset), struct.unpack_from('<i', sa, offset)[0], struct.unpack_from('<i', sb, offset)[0])
        for offset in list(range(0, 0xa0, 4)):
            if offset % 16 == 12:
                continue
            x, y = struct.unpack_from('<f', sa, offset)[0], struct.unpack_from('<f', sb, offset)[0]
            if not close(x, y):
                return ('float', hex(offset), x, y)
        for offset in range(0, 12, 4):
            x, y = struct.unpack_from('<f', first[2], offset)[0], struct.unpack_from('<f', second[2], offset)[0]
            if not close(x, y):
                return ('direction', offset, x, y)
        return None

    def point(scale):
        return [rng.uniform(-scale, scale) for _ in range(3)]

    for count_a, count_b, control, trial in itertools.product(range(1, 5), range(1, 5), (0x027f, 0x037f), range(200)):
        state = bytearray(rng.randbytes(0xe0))
        scale = rng.choice((0.5, 3.0, 40.0))
        offset = point(scale*2)
        a = [point(scale) for _ in range(4)]
        b = [[offset[k]+value for k, value in enumerate(point(scale))] for _ in range(4)]
        if trial % 7 == 1:
            b = [[value+scale*4 for value in p] for p in b]
        if trial % 11 == 2 and count_a > 1:
            a[1] = list(a[0])
        if trial % 13 == 3 and count_b > 2:
            b[2] = [(b[0][k]+b[1][k])*0.5 for k in range(3)]
        for i in range(4):
            struct.pack_into('<3f', state, i*16, *a[i])
            struct.pack_into('<3f', state, 0x40+i*16, *b[i])
            struct.pack_into('<i', state, 0xb0+i*4, rng.randrange(0, 40))
            struct.pack_into('<i', state, 0xc4+i*4, rng.randrange(0, 40))
        struct.pack_into('<i', state, 0xc0, count_a)
        struct.pack_into('<i', state, 0xd4, count_b)
        results = []
        for uc, entry in zip(machines, entries):
            uc.mem_write(simplex, bytes(state))
            uc.mem_write(direction, b'\x5a'*16)
            returned = call(uc, entry, entry['Distance'], direction, control, True)
            results.append((returned, bytes(uc.mem_read(simplex, 0xe0)), bytes(uc.mem_read(direction, 16))))
        if results[0] == results[1]:
            exact += 1
        else:
            problem = compare(results[0], results[1])
            if problem:
                detail = []
                for result in results:
                    st = result[1]
                    detail.append(dict(ret=struct.unpack('<f', result[0])[0], countA=struct.unpack_from('<i', st, 0xc0)[0],
                                       countB=struct.unpack_from('<i', st, 0xd4)[0],
                                       idxA=struct.unpack_from('<4i', st, 0xb0), idxB=struct.unpack_from('<4i', st, 0xc4),
                                       dir=struct.unpack_from('<3f', result[2], 0)))
                failures.append((count_a, count_b, control, trial, problem, detail))
        cases += 1
        for name in ('CopyFrom', 'Assign'):
            outputs = []
            for uc, entry in zip(machines, entries):
                uc.mem_write(simplex, bytes(0xe0))
                uc.mem_write(other, bytes(state))
                call(uc, entry, entry[name], other, control, False)
                outputs.append(bytes(uc.mem_read(simplex, 0xe0)))
            assert outputs[0] == outputs[1], name
    print({'functions': records, 'distance_cases': cases, 'bit_identical': exact, 'failures': len(failures)})
    for failure in failures:
        print(failure)


if __name__ == '__main__':
    main()
