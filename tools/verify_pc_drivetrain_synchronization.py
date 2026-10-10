#!/usr/bin/env python3
"""Verify wheel synchronization with the recovered wheel-value callee."""
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
from verify_pc_drift_ranking import ADDRESSES, coff_function
from verify_pc_drivetrain import VALUE, ORIGINAL as VALUE_ORIGINAL

BUILD = ROOT/'build/analysis/racing'
SYMBOL = '?SynchronizeWheelValues@Drivetrain@@QAEXM@Z'
ORIGINAL = {SYMBOL: (0x5a12d0, 303, '3583f930b5d94bd94bdce67908b177f4e33f556f88b7d4ea42eeff2bc82a5f16'), VALUE: VALUE_ORIGINAL[VALUE]}
GLOBALS = {'?drivetrainSynchronizationScale@@3MA': 0x78426c, '?drivetrainSynchronizationThreshold@@3MA': 0x7864d8}


def main():
    raw = EXE.read_bytes()
    pe = pefile.PE(data=raw)
    image, base = pe.get_memory_mapped_image(), pe.OPTIONAL_HEADER.ImageBase
    for address, size, digest in ORIGINAL.values():
        assert hashlib.sha256(image[address-base:address-base+size]).hexdigest() == digest
    paths = []
    for unit in ('DrivetrainSynchronization', 'DrivetrainWheelValue'):
        obj = BUILD/(unit+'.obj')
        subprocess.run([str(ROOT/'build/tools/wibo'), 'build/compilers/Win32/7.1/cl.exe', '/nologo', '/c', '/Ox', '/Ob0', '/Z7', '/EHsc',
                        '/Fo'+str(obj.relative_to(ROOT)), 'src/Speed/Indep/Src/Physics/'+unit+'.cpp'], cwd=ROOT, check=True)
        paths.append(obj)
    linked = link_objects(paths, GLOBALS)
    ADDRESSES.update(GLOBALS)
    ADDRESSES.update({symbol: data[0] for symbol, data in ORIGINAL.items()})
    ADDRESSES.update({'__real@3f000000': 0x784260, '__real@00000000': 0x7d7d3c,
                      '__real@3a83126f': 0x78435c, '__real@3f7fbe77': 0x79b158})
    body, relocs = coff_function(paths[0], SYMBOL)
    this, spec, wheels, aux, stack, stop = 0x2000000, 0x2001000, 0x2002000, 0x2004000, 0x200f000, 0x2100000
    state_size = 0x5000
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
        ranges = linked['ranges'] if rebuilt else [(a, a+n) for a, n, _ in ORIGINAL.values()]
        def guard(machine, address, size, user, ranges=ranges):
            assert any(a <= address < b for a, b in ranges), hex(address)
        uc.hook_add(UC_HOOK_CODE, guard)
        machines.append(uc)
        entries.append(linked['exports'][SYMBOL] if rebuilt else ORIGINAL[SYMBOL][0])
    rng = random.Random(0x5a12d0)
    def neighbors(value):
        bits = struct.unpack('<I', struct.pack('<f', value))[0]
        return [struct.unpack('<f', struct.pack('<I', bits+d))[0] for d in (-1, 0, 1)]
    selectors = [0, 0.5, 1]
    for va in (0x78435c, 0x79b158):
        selectors += neighbors(struct.unpack_from('<f', image, va-base)[0])
    scale = struct.unpack_from('<f', image, 0x78426c-base)[0]
    speeds = [-100, 0, 100] + neighbors(40/scale)
    count = 0
    for selector, ratio_index, contacts, speed, precision in itertools.product(selectors, (0, 2, 3, 4, 5, 6, 7), range(16), speeds, (0x027f, 0x037f)):
        state = bytearray(rng.randbytes(state_size))
        def put(address, fmt, value):
            struct.pack_into(fmt, state, address-this, value)
        for address, value in ((this+0x18, spec), (this+0x34, aux), (this+0x3c, ratio_index)):
            put(address, '<I', value)
        for address, value in ((spec+8, rng.uniform(0.5, 5)), (spec+12, rng.uniform(0.5, 5)), (spec+16, selector),
                               (spec+0x20+ratio_index*4, (-3.1, 0, 3.4, 2.1, 1.4, 1.1, 0.9, 0.75)[ratio_index]), (aux+0x3f0, speed)):
            put(address, '<f', value)
        for i in range(4):
            wheel = wheels+i*0x400
            put(this+0x24+i*4, '<I', wheel)
            put(wheel+0x28, '<f', rng.uniform(-500, 500))
            put(wheel+0x11c, '<B', (contacts>>i)&1)
        input_value = rng.uniform(-1000, 1000)
        results = []
        for uc, entry in zip(machines, entries):
            uc.mem_write(this, bytes(state))
            uc.mem_write(stack, struct.pack('<If', stop, input_value))
            uc.reg_write(UC_X86_REG_ESP, stack)
            uc.reg_write(UC_X86_REG_ECX, this)
            uc.reg_write(UC_X86_REG_FPCW, precision)
            saved = (UC_X86_REG_EBX, UC_X86_REG_ESI, UC_X86_REG_EDI, UC_X86_REG_EBP)
            for reg in saved:
                uc.reg_write(reg, 0x12345678)
            uc.emu_start(entry, stop, count=2000)
            assert uc.reg_read(UC_X86_REG_EIP) == stop
            assert uc.reg_read(UC_X86_REG_ESP) == stack+8
            assert all(uc.reg_read(reg) == 0x12345678 for reg in saved)
            result = bytes(uc.mem_read(this, state_size))
            unchanged = bytearray(result)
            for i in range(4):
                offset = wheels+i*0x400+0x28-this
                unchanged[offset:offset+4] = state[offset:offset+4]
            assert unchanged == state
            results.append(result)
        if results[0] != results[1]:
            differences = [(hex(this+i), results[0][i:i+4].hex(), results[1][i:i+4].hex()) for i in range(0, state_size, 4) if results[0][i:i+4] != results[1][i:i+4]]
            raise AssertionError((selector, ratio_index, contacts, speed, precision, differences))
        count += 1
    address, size, digest = ORIGINAL[SYMBOL]
    report = dict(executable_sha256=hashlib.sha256(raw).hexdigest(), original_address=hex(address), original_size=size,
                  original_sha256=digest, rebuilt_size=len(body), byte_identical_after_relocation=body == image[address-base:address-base+size],
                  comparisons=count, compiler='MSVC 7.1 /Ox /Ob0 /Z7 /EHsc', relocations=relocs, stubbed_callees=[],
                  checks=['exact whole state', 'writes restricted to four wheel scalars', 'stack and preserved registers'],
                  limitations=['Finite synthetic inputs', 'Neutral zero-ratio division excluded', 'Units and complete contact/body solver unresolved'])
    (BUILD/'drivetrain-synchronization-verification.json').write_text(json.dumps(report, indent=2)+'\n')
    print(json.dumps(report, indent=2))


if __name__ == '__main__':
    main()
