#!/usr/bin/env python3
"""Compare a real UG2 drivetrain derivative through the recovered integrator.

The controller's final callback is a declared no-op fixture. Actual drivetrain
count, state getter, derivative and pre-integration instructions run unhooked.
This deliberately excludes the vehicle's other blocks and post-step dynamics.
"""
import hashlib
import itertools
import json
import random
import struct
import subprocess
import pefile
from unicorn import Uc, UC_ARCH_X86, UC_MODE_32, UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EAX, UC_X86_REG_ECX, UC_X86_REG_EDX, UC_X86_REG_ESP, UC_X86_REG_EIP, UC_X86_REG_EBX, UC_X86_REG_ESI, UC_X86_REG_EDI, UC_X86_REG_EBP, UC_X86_REG_FPCW
from probe_pc_race_reward import ROOT, EXE
from verify_pc_drift_ranking import ADDRESSES, coff_function
from verify_pc_drivetrain import CONSTANTS, RATE, ORIGINAL as DRIVETRAIN_ORIGINAL
from verify_pc_vehicle_integrator import STEP, COLLECT, ORIGINAL as INTEGRATOR_ORIGINAL

BUILD = ROOT / "build/analysis/racing"
COUNT = "?GetValueCount@Drivetrain@@QAEHXZ"
VALUES = "?GetValues@Drivetrain@@QAEPAMXZ"
BEFORE = "?BeforeIntegrate@Drivetrain@@QAEHXZ"
ORIGINAL = {**INTEGRATOR_ORIGINAL, RATE: DRIVETRAIN_ORIGINAL[RATE],
            COUNT: (0x40cb20, 6, "2db31f4e09597946e56e859813bfdad7046d457c32332c3a882654d1e3ffdf67"),
            VALUES: (0x5aa700, 4, "dd9fb9b70598fa70d2f544d70e0faf8ff028fc6b8fcb88f35c9d2eaf38d86e7d"),
            BEFORE: (0x50d510, 3, "4bc724f3b1d0caf4fe369c18cba3102e6c4ea057f63fe1587e3973134a7f755e")}


def main():
    BUILD.mkdir(parents=True, exist_ok=True)
    raw = EXE.read_bytes()
    if hashlib.sha256(raw).hexdigest() != "f9dd86c054878ce6276beb07c1fd61874f7a1e4bf1f241b084c65b73e24168a7":
        raise ValueError("Unsupported executable fingerprint")
    pe = pefile.PE(data=raw)
    image, base = pe.get_memory_mapped_image(), pe.OPTIONAL_HEADER.ImageBase
    ADDRESSES.update(CONSTANTS)
    ADDRESSES.update({name: entry[0] for name, entry in ORIGINAL.items()})
    units = {STEP: "VehicleIntegrator", COLLECT: "VehicleDerivatives", RATE: "DrivetrainDerivative",
             COUNT: "DrivetrainAccessors", VALUES: "DrivetrainAccessors", BEFORE: "DrivetrainAccessors"}
    for unit in sorted(set(units.values())):
        subprocess.run([str(ROOT/"build/tools/wibo"), "build/compilers/Win32/7.1/cl.exe", "/nologo",
                        "/c", "/Ox", "/Z7", "/EHsc", "/Fobuild/analysis/racing/"+unit+".obj",
                        "src/Speed/Indep/Src/Physics/"+unit+".cpp"], cwd=ROOT, check=True)
    records = []
    for symbol, (address, size, digest) in ORIGINAL.items():
        original = image[address-base:address-base+size]
        assert hashlib.sha256(original).hexdigest() == digest
        body, _ = coff_function(BUILD/(units[symbol]+".obj"), symbol)
        records.append(dict(symbol=symbol, address=hex(address), original_sha256=digest,
                            original_size=size, rebuilt_size=len(body), byte_identical_after_relocation=body == original))
    candidate = {symbol: 0x2200000+i*0x1000 for i, symbol in enumerate(ORIGINAL)}
    ADDRESSES.update(candidate)
    bodies = {symbol: coff_function(BUILD/(unit+".obj"), symbol)[0] for symbol, unit in units.items()}
    controller, node, spec, vtable, controller_vtable = 0x2000000, 0x2001000, 0x2002000, 0x2003000, 0x2003100
    stack, after, stop = 0x200f000, 0x2100000, 0x2101000
    machines, contexts = [], []
    for rebuilt in (False, True):
        uc = Uc(UC_ARCH_X86, UC_MODE_32)
        uc.mem_map(base, (len(image)+4095) & ~4095)
        uc.mem_write(base, image)
        uc.mem_map(0x2000000, 0x10000)
        uc.mem_map(0x2100000, 0x2000)
        uc.mem_map(0x2200000, 0x6000)
        for symbol, body in bodies.items():
            uc.mem_write(candidate[symbol], body)
        entries = candidate if rebuilt else {symbol: entry[0] for symbol, entry in ORIGINAL.items()}
        ranges = [(entries[symbol], entries[symbol]+(len(bodies[symbol]) if rebuilt else entry[1]))
                  for symbol, entry in ORIGINAL.items()]
        context = dict(trace=[])
        def hook(machine, address, size, user, ranges=ranges, entries=entries, context=context):
            if address == after:
                assert machine.reg_read(UC_X86_REG_ECX) == controller
                context["trace"].append("controller_after_fixture")
                sp = machine.reg_read(UC_X86_REG_ESP)
                returned = struct.unpack("<I", machine.mem_read(sp, 4))[0]
                machine.reg_write(UC_X86_REG_EAX, 0)
                machine.reg_write(UC_X86_REG_ECX, 0xfedcba98)
                machine.reg_write(UC_X86_REG_EDX, 0xdeadbeef)
                machine.reg_write(UC_X86_REG_ESP, sp+4)
                machine.reg_write(UC_X86_REG_EIP, returned)
            elif not any(start <= address < end for start, end in ranges):
                raise RuntimeError(f"Unexpected instruction at {address:#x}")
            else:
                for symbol, entry in entries.items():
                    if address == entry:
                        context["trace"].append(symbol)
        uc.hook_add(UC_HOOK_CODE, hook)
        def w32(address, value):
            uc.mem_write(address, struct.pack("<I", value))
        w32(controller, controller_vtable)
        w32(controller_vtable+4, after)
        w32(controller+0xc, node+4)
        w32(node, vtable)
        w32(node+4, controller+0xc)
        w32(node+0x18, spec)
        for slot, symbol in ((4, COUNT), (12, VALUES), (24, RATE), (32, BEFORE)):
            w32(vtable+slot, entries[symbol])
        machines.append(uc)
        contexts.append(context)
    rng, cases = random.Random(0x126890), 0
    expected = [STEP, COLLECT, RATE, COUNT, COUNT, VALUES, BEFORE, "controller_after_fixture"]
    for status, adjustment, gate, mode, duration, control in itertools.product(
            (0, 2, 3, 4), (-0.1, -0.0, 0.0, 0.1, 1.0), (2, 3), range(4),
            (0.0, 1/240, 1/60, 0.1), (0x027f, 0x037f)):
        values = (rng.uniform(-100, 100), rng.uniform(-5, 5), rng.uniform(-50, 50))
        outcomes = []
        for uc, context, entry in zip(machines, contexts, (ORIGINAL[STEP][0], candidate[STEP])):
            context["trace"] = []
            for address, value in ((node+0x60, status), (node+0x38, gate), (node+0x44, mode)):
                uc.mem_write(address, struct.pack("<I", value))
            for address, value in ((node+0x14, values[0]), (node+0xe8, values[1]),
                                   (spec+0x1c, values[2]), (node+0x68, adjustment)):
                uc.mem_write(address, struct.pack("<f", value))
            before = bytes(uc.mem_read(node, 0xec))
            uc.mem_write(stack, struct.pack("<If", stop, duration))
            uc.reg_write(UC_X86_REG_ESP, stack)
            uc.reg_write(UC_X86_REG_ECX, controller)
            uc.reg_write(UC_X86_REG_FPCW, control)
            saved = (UC_X86_REG_EBX, UC_X86_REG_ESI, UC_X86_REG_EDI, UC_X86_REG_EBP)
            for register in saved:
                uc.reg_write(register, 0x12345678)
            uc.emu_start(entry, stop, count=10000)
            assert uc.reg_read(UC_X86_REG_EIP) == stop
            assert uc.reg_read(UC_X86_REG_ESP) == stack+8
            assert all(uc.reg_read(register) == 0x12345678 for register in saved)
            assert context["trace"] == expected
            outcome = bytes(uc.mem_read(node, 0xec))
            assert outcome[:0x14] == before[:0x14] and outcome[0x18:] == before[0x18:]
            outcomes.append(outcome[0x14:0x18])
        assert outcomes[0] == outcomes[1], (status, adjustment, gate, mode, duration, control, outcomes)
        cases += 1
    report = dict(executable_sha256=hashlib.sha256(raw).hexdigest(), functions=records, cases=cases,
                  original_callbacks_executed=[RATE, COUNT, VALUES, BEFORE],
                  fixture_callbacks=["Controller final callback: no-op with caller-saved register clobber"],
                  limitations=["Synthetic single-block list", "Finite inputs, modes 0..3",
                               "Other vehicle blocks and controller post-step dynamics excluded"])
    (BUILD/"drivetrain-integration-verification.json").write_text(json.dumps(report, indent=2)+"\n")
    print(json.dumps(report, indent=2))


if __name__ == "__main__":
    main()
