#!/usr/bin/env python3
"""Compare recovered integrator source with UG2 using controlled block callbacks.

Callbacks are explicit fixtures, not recovered tyre/drivetrain physics. Both
the original and rebuilt dispatcher/collector instructions execute in full.
"""
import hashlib
import json
import random
import struct
import subprocess
import pefile
from unicorn import Uc, UC_ARCH_X86, UC_MODE_32, UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EAX, UC_X86_REG_ECX, UC_X86_REG_EDX, UC_X86_REG_ESP, UC_X86_REG_EIP, UC_X86_REG_EBX, UC_X86_REG_ESI, UC_X86_REG_EDI, UC_X86_REG_EBP, UC_X86_REG_FPCW
from probe_pc_race_reward import ROOT, EXE
from verify_pc_drift_ranking import ADDRESSES, coff_function

BUILD = ROOT / "build/analysis/racing"
STEP = "?Integrate@VehicleIntegrator@@QAEXM@Z"
COLLECT = "?CollectDerivatives@VehicleIntegrator@@QAEXPAM@Z"
ORIGINAL = {STEP: (0x5aa480, 275, "ca810adb415a4b18f40c21bdda1ad6a2882aee3d5275cfbbf7a95b4459901d1a"),
            COLLECT: (0x5a0d20, 91, "b1fe80481921e3b9631b63170d2299c9c86b0a3eab6ea225d2f2a50ebf3bf0a4")}


def main():
    BUILD.mkdir(parents=True, exist_ok=True)
    pe = pefile.PE(str(EXE))
    image, base = pe.get_memory_mapped_image(), pe.OPTIONAL_HEADER.ImageBase
    records, objects = [], {}
    ADDRESSES.update({name: entry[0] for name, entry in ORIGINAL.items()})
    for unit, symbol in (("VehicleDerivatives", COLLECT), ("VehicleIntegrator", STEP)):
        obj = BUILD / (unit+".obj")
        subprocess.run([str(ROOT/"build/tools/wibo"), "build/compilers/Win32/7.1/cl.exe",
                        "/nologo", "/c", "/Ox", "/Z7", "/EHsc", "/Fo"+str(obj.relative_to(ROOT)),
                        "src/Speed/Indep/Src/Physics/"+unit+".cpp"], cwd=ROOT, check=True)
        body, relocs = coff_function(obj, symbol)
        address, size, digest = ORIGINAL[symbol]
        original = image[address-base:address-base+size]
        if base != 0x400000 or hashlib.sha256(original).hexdigest() != digest:
            raise ValueError("Original routine fingerprint differs")
        records.append(dict(symbol=symbol, original_address=hex(address), original_size=size,
                            rebuilt_size=len(body), byte_identical_after_relocation=body == original,
                            original_sha256=digest, relocations=relocs))
        objects[symbol] = obj
    candidate_addresses = {STEP: 0x2200000, COLLECT: 0x2201000}
    ADDRESSES.update(candidate_addresses)
    rebuilt = {symbol: coff_function(obj, symbol)[0] for symbol, obj in objects.items()}
    callbacks = {0x2100000: "count", 0x2100010: "values", 0x2100020: "rates",
                 0x2100030: "before", 0x2100040: "after"}
    controller, node_base, states, stack, stop = 0x2000000, 0x2001000, 0x2003000, 0x201f000, 0x2101000
    node_vtable, controller_vtable = 0x2000100, 0x2000200
    machines, contexts = [], []
    for is_rebuilt in (False, True):
        uc = Uc(UC_ARCH_X86, UC_MODE_32)
        uc.mem_map(base, (len(image)+4095) & ~4095)
        uc.mem_write(base, image)
        uc.mem_map(0x2000000, 0x20000)
        uc.mem_map(0x2100000, 0x2000)
        uc.mem_map(0x2200000, 0x2000)
        for symbol, body in rebuilt.items():
            uc.mem_write(candidate_addresses[symbol], body)
        context = dict(trace=[], nodes={}, initial=[], counts=[])
        ranges = [(entry[0], entry[0]+entry[1]) for entry in ORIGINAL.values()] if not is_rebuilt else [
            (candidate_addresses[name], candidate_addresses[name]+len(body)) for name, body in rebuilt.items()]

        def hook(machine, address, size, user, context=context, ranges=ranges):
            if address not in callbacks:
                if not any(start <= address < end for start, end in ranges):
                    raise RuntimeError(f"Unexpected instruction/helper at {address:#x}")
                return
            event, receiver = callbacks[address], machine.reg_read(UC_X86_REG_ECX)
            ordinal = context["nodes"].get(receiver)
            if event == "after":
                assert receiver == controller
            else:
                assert ordinal is not None
            context["trace"].append((event, ordinal))
            sp = machine.reg_read(UC_X86_REG_ESP)
            returned = struct.unpack("<I", machine.mem_read(sp, 4))[0]
            result = 0
            if event == "count":
                result = context["counts"][ordinal]
            elif event == "values":
                result = states+ordinal*0x400
            elif event == "rates":
                output = struct.unpack("<I", machine.mem_read(sp+4, 4))[0]
                # A fixture couples derivatives to other blocks' live states,
                # exposing incorrect collect/update interleaving.
                shared = sum(struct.unpack("<f", machine.mem_read(states+j*0x400, 4))[0]
                             for j, count in enumerate(context["counts"]) if count)
                for i in range(context["counts"][ordinal]):
                    live = struct.unpack("<f", machine.mem_read(states+ordinal*0x400+i*4, 4))[0]
                    machine.mem_write(output+i*4, struct.pack("<f", live*0.5-shared*0.125+(i-2)*0.25))
            elif event == "before" and context["counts"][ordinal]:
                position = states+ordinal*0x400
                value = struct.unpack("<f", machine.mem_read(position, 4))[0]
                machine.mem_write(position, struct.pack("<f", value+0.25))
            machine.reg_write(UC_X86_REG_EAX, result)
            machine.reg_write(UC_X86_REG_ECX, 0xfedcba98)
            machine.reg_write(UC_X86_REG_EDX, 0xdeadbeef)
            machine.reg_write(UC_X86_REG_ESP, sp+(8 if event == "rates" else 4))
            machine.reg_write(UC_X86_REG_EIP, returned)

        uc.hook_add(UC_HOOK_CODE, hook)
        machines.append(uc)
        contexts.append(context)
    shapes = [(), (0,), (1,), (3,), (4,), (5,), (8,), (16,), (140,), (0, 1),
              (4, 5), (7, 3, 0), (9, 13, 21), (64, 64), (40, 40, 40)]
    rng, count = random.Random(0x126890), 0
    for shape in shapes:
        for duration in (0.0, 0.001, 1/60, 0.1):
            for control in (0x027f, 0x037f):
                for trial in range(4):
                    initial = [[rng.uniform(-100, 100) for _ in range(n)] for n in shape]
                    outcomes = []
                    for uc, context, entry in zip(machines, contexts, (ORIGINAL[STEP][0], candidate_addresses[STEP])):
                        context.update(trace=[], counts=shape, nodes={node_base+i*0x40: i for i in range(len(shape))})
                        def w32(address, value):
                            uc.mem_write(address, struct.pack("<I", value))
                        w32(controller, controller_vtable)
                        w32(controller_vtable+4, 0x2100040)
                        w32(controller+0xc, node_base+4 if shape else controller+0xc)
                        for i, n in enumerate(shape):
                            node = node_base+i*0x40
                            w32(node, node_vtable)
                            w32(node+4, node_base+(i+1)*0x40+4 if i+1 < len(shape) else controller+0xc)
                            uc.mem_write(states+i*0x400-16, b"\xa5"*(n*4+32))
                            if n:
                                uc.mem_write(states+i*0x400, struct.pack("<"+"f"*n, *initial[i]))
                        for slot, target in ((4, 0x2100000), (12, 0x2100010), (24, 0x2100020), (32, 0x2100030)):
                            w32(node_vtable+slot, target)
                        uc.mem_write(stack, struct.pack("<If", stop, duration))
                        uc.reg_write(UC_X86_REG_ESP, stack)
                        uc.reg_write(UC_X86_REG_ECX, controller)
                        uc.reg_write(UC_X86_REG_FPCW, control)
                        saved = (UC_X86_REG_EBX, UC_X86_REG_ESI, UC_X86_REG_EDI, UC_X86_REG_EBP)
                        for register in saved:
                            uc.reg_write(register, 0x12345678)
                        uc.emu_start(entry, stop, count=100000)
                        assert uc.reg_read(UC_X86_REG_EIP) == stop
                        assert uc.reg_read(UC_X86_REG_ESP) == stack+8
                        assert all(uc.reg_read(register) == 0x12345678 for register in saved)
                        buffers = [bytes(uc.mem_read(states+i*0x400-16, n*4+32)) for i, n in enumerate(shape)]
                        assert all(data[:16] == b"\xa5"*16 and data[-16:] == b"\xa5"*16 for data in buffers)
                        expected_trace = [(event, i) for i in range(len(shape)) for event in ("rates", "count")]
                        expected_trace += [(event, i) for i in range(len(shape)) for event in ("count", "values", "before")]
                        expected_trace += [("after", None)]
                        assert context["trace"] == expected_trace
                        outcomes.append(buffers)
                    assert outcomes[0] == outcomes[1], (shape, duration, control, trial)
                    count += 1
    report = dict(functions=records, compiler="MSVC 7.1 /Ox /Z7 /EHsc", differential_cases=count,
                  fixture_callbacks=list(callbacks.values()),
                  limitations=["Controlled virtual callbacks, not original force formulas",
                               "Finite values and aggregate state sizes through 140 only",
                               "No retail runtime trace or full vehicle solver"])
    (BUILD/"vehicle-integrator-verification.json").write_text(json.dumps(report, indent=2)+"\n")
    print(json.dumps(report, indent=2))


if __name__ == "__main__":
    main()
