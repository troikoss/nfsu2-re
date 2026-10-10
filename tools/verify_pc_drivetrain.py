#!/usr/bin/env python3
"""Compile recovered UG2 drivetrain consumers and compare original execution."""
import hashlib
import itertools
import json
import random
import struct
import subprocess
import pefile
from unicorn import Uc, UC_ARCH_X86, UC_MODE_32, UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EAX, UC_X86_REG_ECX, UC_X86_REG_ESP, UC_X86_REG_EIP, UC_X86_REG_EBX, UC_X86_REG_ESI, UC_X86_REG_EDI, UC_X86_REG_EBP, UC_X86_REG_FPCW
from probe_pc_race_reward import ROOT, EXE
from verify_pc_drift_ranking import ADDRESSES, coff_function

BUILD = ROOT / "build/analysis/racing"
VALUE = "?GetWheelDerivedValue@Drivetrain@@QAEMXZ"
RATE = "?WriteDerivative@Drivetrain@@QAEHPAM@Z"
ORIGINAL = {VALUE: (0x5945f0, 117, "9f91e5f805cd637690f9297d5e1ff054e501f7314433058ea7a7961b16b2a7cb"),
            RATE: (0x5949b0, 113, "c0bda45979735e718fff1549be02300d2443f3fc20ccfaa4642d92b9c4b7fd7c")}
CONSTANTS = {"__real@00000000": 0x7d7d3c, "__real@3f000000": 0x784260,
             "__real@3f800000": 0x784250, "__real@3a83126f": 0x78435c,
             "__real@3f7fbe77": 0x79b158,
             "?drivetrainNegativeScale@@3MA": 0x7a18d4,
             "?drivetrainModeCoefficients@@3PAMA": 0x7fc298}


def main():
    BUILD.mkdir(parents=True, exist_ok=True)
    raw = EXE.read_bytes()
    pe = pefile.PE(data=raw)
    image, base = pe.get_memory_mapped_image(), pe.OPTIONAL_HEADER.ImageBase
    ADDRESSES.update(CONSTANTS)
    ADDRESSES.update({name: data[0] for name, data in ORIGINAL.items()})
    objects, records = {}, []
    for unit, symbol in (("DrivetrainWheelValue", VALUE), ("DrivetrainDerivative", RATE)):
        obj = BUILD / (unit+".obj")
        subprocess.run([str(ROOT/"build/tools/wibo"), "build/compilers/Win32/7.1/cl.exe", "/nologo",
                        "/c", "/Ox", "/Z7", "/EHsc", "/Fo"+str(obj.relative_to(ROOT)),
                        "src/Speed/Indep/Src/Physics/"+unit+".cpp"], cwd=ROOT, check=True)
        body, relocations = coff_function(obj, symbol)
        address, size, digest = ORIGINAL[symbol]
        original = image[address-base:address-base+size]
        if base != 0x400000 or hashlib.sha256(original).hexdigest() != digest:
            raise ValueError("Unsupported original routine fingerprint")
        objects[symbol] = obj
        records.append(dict(symbol=symbol, original_address=hex(address), original_size=size,
                            rebuilt_size=len(body), original_sha256=digest,
                            byte_identical_after_relocation=original == body, relocations=relocations))
    candidate = {VALUE: 0x2200000, RATE: 0x2201000}
    ADDRESSES.update(candidate)
    bodies = {symbol: coff_function(obj, symbol)[0] for symbol, obj in objects.items()}
    object_address, spec, wheels, output, stack, stop, wrapper = 0x2000000, 0x2001000, 0x2002000, 0x2003000, 0x200f000, 0x2203000, 0x2202000
    machines = []
    for rebuilt in (False, True):
        uc = Uc(UC_ARCH_X86, UC_MODE_32)
        uc.mem_map(base, (len(image)+4095) & ~4095)
        uc.mem_write(base, image)
        uc.mem_map(0x2000000, 0x10000)
        uc.mem_map(0x2200000, 0x4000)
        if rebuilt:
            for symbol, body in bodies.items():
                uc.mem_write(candidate[symbol], body)
        value_entry = candidate[VALUE] if rebuilt else ORIGINAL[VALUE][0]
        # Probe-only call/store wrapper captures the x87 float return as binary32.
        code = b"\xe8"+struct.pack("<i", value_entry-wrapper-5)+b"\xd9\x1d"+struct.pack("<I", output)+b"\xc3"
        uc.mem_write(wrapper, code)
        ranges = [(wrapper, wrapper+len(code))]
        ranges += [(candidate[symbol], candidate[symbol]+len(body)) for symbol, body in bodies.items()] if rebuilt else [(a, a+n) for a, n, _ in ORIGINAL.values()]
        def guard(machine, address, size, context, ranges=ranges):
            if not any(start <= address < end for start, end in ranges):
                raise RuntimeError(f"Unexpected instruction at {address:#x}")
        uc.hook_add(UC_HOOK_CODE, guard)
        uc.mem_write(object_address+0x18, struct.pack("<I", spec))
        for i in range(4):
            uc.mem_write(object_address+0x24+i*4, struct.pack("<I", wheels+i*0x100))
        machines.append(uc)

    def run(uc, symbol, control, rebuilt):
        uc.mem_write(output-16, b"\xa5"*36)
        uc.mem_write(stack, struct.pack("<II", stop, output))
        uc.reg_write(UC_X86_REG_ESP, stack)
        uc.reg_write(UC_X86_REG_ECX, object_address)
        uc.reg_write(UC_X86_REG_FPCW, control)
        saved = (UC_X86_REG_EBX, UC_X86_REG_ESI, UC_X86_REG_EDI, UC_X86_REG_EBP)
        for reg in saved:
            uc.reg_write(reg, 0x12345678)
        uc.emu_start(wrapper if symbol == VALUE else (candidate[RATE] if rebuilt else ORIGINAL[RATE][0]), stop, count=1000)
        assert uc.reg_read(UC_X86_REG_EIP) == stop
        assert uc.reg_read(UC_X86_REG_ESP) == stack+(4 if symbol == VALUE else 8)
        assert all(uc.reg_read(reg) == 0x12345678 for reg in saved)
        if symbol == RATE:
            assert uc.reg_read(UC_X86_REG_EAX) == 0
        buffer = bytes(uc.mem_read(output-16, 36))
        assert buffer[:16] == b"\xa5"*16 and buffer[20:] == b"\xa5"*16
        return buffer[16:20]

    rng, value_cases, rate_cases = random.Random(0x126890), 0, 0
    selectors = [-0.5, 0, 0.5, 1, 1.5]
    for va in (0x78435c, 0x79b158):
        bits = struct.unpack_from("<I", image, va-base)[0]
        selectors += [struct.unpack("<f", struct.pack("<I", bits+delta))[0] for delta in (-1, 0, 1)]
    for selector, ratio_index, control, trial in itertools.product(selectors, range(8), (0x027f, 0x037f), range(8)):
        wheel_values = [rng.uniform(-500, 500) for _ in range(4)]
        front, rear = rng.uniform(0.5, 5), rng.uniform(0.5, 5)
        ratio = (-3.1, 0, 3.4, 2.1, 1.4, 1.1, 0.9, 0.75)[ratio_index]
        for uc in machines:
            for address, value in ((spec+8, front), (spec+12, rear), (spec+16, selector), (spec+0x20+ratio_index*4, ratio)):
                uc.mem_write(address, struct.pack("<f", value))
            uc.mem_write(object_address+0x3c, struct.pack("<I", ratio_index))
            for i, value in enumerate(wheel_values):
                uc.mem_write(wheels+i*0x100+0x28, struct.pack("<f", value))
        results = [run(uc, VALUE, control, rebuilt) for uc, rebuilt in zip(machines, (False, True))]
        assert results[0] == results[1], (selector, ratio_index, control, results)
        value_cases += 1
    for status, adjustment, gate, mode, control, trial in itertools.product(
            (0, 2, 3, 4), (-2.0, -0.1, -0.0, 0.0, 0.1, 1.0, 2.0), (0, 2, 3, 4),
            range(4), (0x027f, 0x037f), range(3)):
        spec_factor, coefficient = rng.uniform(-50, 50), rng.uniform(-5, 5)
        for uc in machines:
            for address, value in ((object_address+0x60, status), (object_address+0x38, gate), (object_address+0x44, mode)):
                uc.mem_write(address, struct.pack("<I", value))
            for address, value in ((object_address+0x68, adjustment), (object_address+0xe8, coefficient), (spec+0x1c, spec_factor)):
                uc.mem_write(address, struct.pack("<f", value))
        results = [run(uc, RATE, control, rebuilt) for uc, rebuilt in zip(machines, (False, True))]
        assert results[0] == results[1], (status, adjustment, gate, mode, control, results)
        rate_cases += 1
    report = dict(executable_sha256=hashlib.sha256(raw).hexdigest(), functions=records,
                  compiler="MSVC 7.1 /Ox /Z7 /EHsc", wheel_value_cases=value_cases,
                  derivative_cases=rate_cases, stubbed_callees=[],
                  limitations=["Finite synthetic inputs only", "Modes 0..3 and gear indices 0..7",
                               "Field units and upstream force construction unresolved", "Not a full vehicle solver"])
    (BUILD/"drivetrain-verification.json").write_text(json.dumps(report, indent=2)+"\n")
    print(json.dumps(report, indent=2))


if __name__ == "__main__":
    main()
