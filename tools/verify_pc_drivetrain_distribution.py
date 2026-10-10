#!/usr/bin/env python3
"""Verify the recovered original torque-distribution writer without callees."""
import hashlib
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
SYMBOL = '?DistributeDriveValue@Drivetrain@@QAEXM@Z'
ADDRESS, SIZE = 0x594a30, 636
DIGEST = '1f0f6fcab7e4926b21de327954988b64030838e1278e75eec56c7bc518ed8c2d'


def main():
    raw = EXE.read_bytes()
    pe = pefile.PE(data=raw)
    image, base = pe.get_memory_mapped_image(), pe.OPTIONAL_HEADER.ImageBase
    assert base == 0x400000
    original = image[ADDRESS-base:ADDRESS-base+SIZE]
    assert hashlib.sha256(original).hexdigest() == DIGEST
    ADDRESSES.update({'__real@3e800000': 0x784d28, '__real@3f800000': 0x784250,
                      '__real@3f000000': 0x784260, '__real@3727c5ac': 0x7845a0,
                      '?drivetrainSumEpsilon@@3MA': 0x7845a0, '?drivetrainPairThreshold@@3MA': 0x7fbffc})
    obj = BUILD/'DrivetrainDistribution.obj'
    subprocess.run([str(ROOT/'build/tools/wibo'), 'build/compilers/Win32/7.1/cl.exe', '/nologo', '/c', '/Ox', '/Ob0', '/Z7', '/EHsc',
                    '/Fo'+str(obj.relative_to(ROOT)), 'src/Speed/Indep/Src/Physics/DrivetrainDistribution.cpp'], cwd=ROOT, check=True)
    body, relocs = coff_function(obj, SYMBOL)
    this, spec, wheels, component, auxiliary, stack, candidate, stop = 0x2000000, 0x2001000, 0x2002000, 0x2004000, 0x2005000, 0x200f000, 0x2200000, 0x2202000
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
        def guard(machine, address, size, context, entry=entry, length=len(body) if rebuilt else SIZE):
            assert entry <= address < entry+length, hex(address)
        uc.hook_add(UC_HOOK_CODE, guard)
        machines.append(uc)
    rng = random.Random(0x594a30)
    state_size = 0x6000
    writable = [this+0x70, component+0x7c] + [wheels+i*0x400+0x108 for i in range(4)]
    cases = 0
    for trial in range(24576):
        state = bytearray(rng.randbytes(state_size))
        def put(address, fmt, value):
            struct.pack_into(fmt, state, address-this, value)
        ratio_index = trial % 8
        selector = (trial//8) % 4
        contacts = (trial//32) % 16
        blocked = (trial//512) % 16
        for address, value in ((this+0x18, spec), (this+0x20, component), (this+0x34, auxiliary),
                               (this+0x3c, ratio_index), (auxiliary+0x3e4, (trial//64)%5-1), (auxiliary+0x42c, selector)):
            put(address, '<I' if value >= 0 else '<i', value)
        for address, value in ((this+0x70, rng.uniform(-100, 100)), (this+0xec, (0, 0.5, 1)[trial//8192]),
                               (component+0x7c, rng.uniform(-100, 100)), (spec+8, rng.uniform(0.5, 5)),
                               (spec+12, rng.uniform(0.5, 5)), (spec+0x20+ratio_index*4, (-3.1, 0, 3.4, 2.1, 1.4, 1.1, 0.9, 0.75)[ratio_index])):
            put(address, '<f', value)
        speeds = [(0, 0, 0, 0), (10, -10, -20, 20), (1, 3, 3, 1), (-2, 3, 3, -2),
                  (0.000001, 0.000001, 0.000001, 0.000001), tuple(rng.uniform(-500, 500) for _ in range(4))][trial%6]
        for i in range(4):
            wheel = wheels+i*0x400
            put(this+0x24+i*4, '<I', wheel)
            put(wheel+0x28, '<f', speeds[i])
            put(wheel+0x108, '<f', rng.uniform(-10000, 10000))
            put(wheel+0x10c, '<I', (blocked>>i)&1)
            put(wheel+0x11c, '<B', (contacts>>i)&1)
        input_value = (0, -0.0, -1000, 1000, rng.uniform(-10000, 10000))[trial%5]
        for precision in (0x027f, 0x037f):
            results = []
            for uc, rebuilt in zip(machines, (False, True)):
                uc.mem_write(this, bytes(state))
                uc.mem_write(stack, struct.pack('<If', stop, input_value))
                uc.reg_write(UC_X86_REG_ESP, stack)
                uc.reg_write(UC_X86_REG_ECX, this)
                uc.reg_write(UC_X86_REG_FPCW, precision)
                saved = (UC_X86_REG_EBX, UC_X86_REG_ESI, UC_X86_REG_EDI, UC_X86_REG_EBP)
                for reg in saved:
                    uc.reg_write(reg, 0x12345678)
                uc.emu_start(candidate if rebuilt else ADDRESS, stop, count=2000)
                assert uc.reg_read(UC_X86_REG_EIP) == stop
                assert uc.reg_read(UC_X86_REG_ESP) == stack+8
                assert all(uc.reg_read(reg) == 0x12345678 for reg in saved)
                result = bytes(uc.mem_read(this, state_size))
                unchanged = bytearray(result)
                for address in writable:
                    offset = address-this
                    unchanged[offset:offset+4] = state[offset:offset+4]
                assert unchanged == state
                if ratio_index == 1:
                    assert result == state
                results.append(result)
            if results[0] != results[1]:
                offsets = [(hex(this+i), results[0][i:i+4].hex(), results[1][i:i+4].hex()) for i in range(0, state_size, 4) if results[0][i:i+4] != results[1][i:i+4]]
                raise AssertionError((trial, precision, input_value, offsets))
            cases += 1
    report = dict(executable_sha256=hashlib.sha256(raw).hexdigest(), original_address=hex(ADDRESS), original_sha256=DIGEST,
                  original_size=SIZE, rebuilt_size=len(body), byte_identical_after_relocation=body == original,
                  compiler='MSVC 7.1 /Ox /Ob0 /Z7 /EHsc', comparisons=cases, relocations=relocs, stubbed_callees=[],
                  checks=['entire mapped state exact', 'only observed six float fields writable', 'neutral no-op', 'stack and preserved registers'],
                  limitations=['Finite synthetic states', 'Not full tyre/body correction chain', 'Units and vehicle parameter loading unresolved'])
    (BUILD/'drivetrain-distribution-verification.json').write_text(json.dumps(report, indent=2)+'\n')
    print(json.dumps(report, indent=2))


if __name__ == '__main__':
    main()
