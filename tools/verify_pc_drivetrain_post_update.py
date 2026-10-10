#!/usr/bin/env python3
"""Execute reconstructed drivetrain post-update against unhooked UG2 bodies.

Synthetic mapped state only: no original executable launch or data modification.
Local COFF sections include compiled jump tables; no helpers are stubbed.
"""
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
from verify_pc_drivetrain import VALUE, ORIGINAL as VALUE_ORIGINAL
from pc_coff import link_objects
from verify_pc_drift_ranking import ADDRESSES, coff_function

BUILD = ROOT / "build/analysis/racing"
POST = "?UpdateAfterIntegration@Drivetrain@@QAEXXZ"
LOW = "?IsBelowComponentLimit@Drivetrain@@QAEHXZ"
NEAR = "?IsWithinComponentTolerance@Drivetrain@@QAEHXZ"
REFRESH = "?RefreshVariations@Drivetrain@@QAEXXZ"
SCALED = "?GetScaledValue@DrivetrainRandom@@QAEMM@Z"
RANDOM = "?GetDrivetrainRandom@@YAMMPAI@Z"
ORIGINAL = {VALUE: VALUE_ORIGINAL[VALUE],
            POST: (0x5a1400, 745, "900882593a8fda0fcca4529c11e4f8041b67690c321574f79d0f1aaead33f98d"),
            LOW: (0x594e90, 55, "78ec26270dd229d71d200126a772911049379b4eef0399990c206e3d88770947"),
            NEAR: (0x594ed0, 33, "65830db8b75ab421673d90b7ee1c683948e8d3f431711b7723e904432c5bf0b0"),
            REFRESH: (0x594710, 150, "0d3950c00380a5635ecc5cf75a73b6ef91a686fb8cf65e7576fea16bbc366751"),
            SCALED: (0x5ea500, 23, "e25b117253f7b541f7f5d7c710998296a0c852b521df442a933830305109b40c"),
            RANDOM: (0x43c170, 74, "179ed0a2fd1646539e3ac0f5730b4d2a524a122613d817519ec83f6ae1ee95bf")}
EXTERNALS = {"drivetrainComponentThreshold": 0x78758c, "drivetrainComponentScale": 0x7a1d38,
             "drivetrainComponentEpsilon": 0x7843ec, "drivetrainDifferenceTolerance": 0x7fc028,
             "drivetrainValueScale": 0x7a1d28, "drivetrainControlScale": 0x7844d8,
             "drivetrainClockScale": 0x784268, "drivetrainTimerLimit": 0x7a18c8,
             "drivetrainAdjustmentDuration": 0x7a18cc, "drivetrainAdjustmentScale": 0x7a18d0,
             "drivetrainRandomScale": 0x786ce8}


def main():
    BUILD.mkdir(parents=True, exist_ok=True)
    raw = EXE.read_bytes()
    assert hashlib.sha256(raw).hexdigest() == "f9dd86c054878ce6276beb07c1fd61874f7a1e4bf1f241b084c65b73e24168a7"
    pe = pefile.PE(data=raw)
    image, base = pe.get_memory_mapped_image(), pe.OPTIONAL_HEADER.ImageBase
    for address, size, digest in ORIGINAL.values():
        assert hashlib.sha256(image[address-base:address-base+size]).hexdigest() == digest
    paths = []
    for unit in ("DrivetrainWheelValue", "DrivetrainPredicates", "DrivetrainRandom", "DrivetrainPostUpdate"):
        path = BUILD/(unit+".obj")
        subprocess.run([str(ROOT/"build/tools/wibo"), "build/compilers/Win32/7.1/cl.exe", "/nologo", "/c",
                        "/Ox", "/Ob0", "/Z7", "/EHsc", "/Fo"+str(path.relative_to(ROOT)),
                        "src/Speed/Indep/Src/Physics/"+unit+".cpp"], cwd=ROOT, check=True)
        paths.append(path)
    externals = {"?"+name+"@@3MA": address for name, address in EXTERNALS.items()}
    externals.update({"?drivetrainClock@@3HA": 0x86518c,
                      "?drivetrainContext@@3PAUDrivetrainContext@@A": 0x8650b8,
                      "?drivetrainRandom@@3PAUDrivetrainRandom@@A": 0x890080})
    linked = link_objects(paths, externals)
    records = [dict(symbol=symbol, original_address=hex(data[0]), original_size=data[1], original_sha256=data[2],
                    rebuilt_size=linked["functions"][symbol]["size"], byte_match_status="Not assessed by section-linked execution")
               for symbol, data in ORIGINAL.items()]
    # Compare bounded function bodies with original-address relocation where
    # they contain no local jump-table relocations. The post-update's section
    # linkage is for behavior verification, not a byte-match claim.
    ADDRESSES.update(externals)
    ADDRESSES.update({symbol: data[0] for symbol, data in ORIGINAL.items()})
    body_units = {LOW: "DrivetrainPredicates", NEAR: "DrivetrainPredicates",
                  REFRESH: "DrivetrainRandom", SCALED: "DrivetrainRandom", RANDOM: "DrivetrainRandom"}
    for record in records:
        symbol = record["symbol"]
        if symbol not in body_units:
            continue
        body, relocations = coff_function(BUILD/(body_units[symbol]+".obj"), symbol)
        address, size, _ = ORIGINAL[symbol]
        record["byte_identical_after_relocation"] = body == image[address-base:address-base+size]
        record["relocations"] = relocations
        del record["byte_match_status"]
    node, spec, component, values, wheels = 0x2000000, 0x2001000, 0x2002000, 0x2003000, 0x2004000
    context, context_entry, random_object = 0x2005000, 0x2006000, 0x2007000
    stack, stop, wrapper, output = 0x200f000, 0x2101000, 0x2100000, 0x2008000
    machines, entries = [], []
    for rebuilt in (False, True):
        uc = Uc(UC_ARCH_X86, UC_MODE_32)
        uc.mem_map(base, (len(image)+4095) & ~4095)
        uc.mem_write(base, image)
        uc.mem_map(0x2000000, 0x10000)
        uc.mem_map(0x2100000, 0x2000)
        uc.mem_map(linked["address"], linked["allocation"])
        entry = linked["exports"] if rebuilt else {symbol: data[0] for symbol, data in ORIGINAL.items()}
        if rebuilt:
            for address, body in linked["blocks"]:
                uc.mem_write(address, body)
        # A cdecl analysis wrapper forwards the existing range/seed arguments,
        # captures ST0 in binary32, then returns. It is not production game code.
        code = b"\xff\x74\x24\x08\xff\x74\x24\x08\xe8"+struct.pack("<i", entry[RANDOM]-wrapper-13)
        code += b"\x83\xc4\x08\xd9\x1d"+struct.pack("<I", output)+b"\xc3"
        uc.mem_write(wrapper, code)
        ranges = linked["ranges"] if rebuilt else [(data[0], data[0]+data[1]) for data in ORIGINAL.values()]
        ranges = ranges+[(wrapper, wrapper+len(code))]
        def guard(machine, address, size, user, ranges=ranges):
            if not any(start <= address < end for start, end in ranges):
                raise RuntimeError(f"Unexpected instruction at {address:#x}")
        uc.hook_add(UC_HOOK_CODE, guard)
        for address, value in ((node+0x18, spec), (node+0x20, component), (component+0x18, values),
                               (0x8650b8, context), (0x890080, random_object)):
            uc.mem_write(address, struct.pack("<I", value))
        for i in range(4):
            uc.mem_write(node+0x24+i*4, struct.pack("<I", wheels+i*0x100))
        uc.mem_write(spec+0x20, struct.pack("<8f", -3.1, 0, 3.4, 2.1, 1.4, 1.1, 0.9, 0.75))
        machines.append(uc)
        entries.append(entry)

    saved = (UC_X86_REG_EBX, UC_X86_REG_ESI, UC_X86_REG_EDI, UC_X86_REG_EBP)
    def execute(uc, entry, control, receiver=node, args=b""):
        uc.mem_write(stack, struct.pack("<I", stop)+args)
        uc.reg_write(UC_X86_REG_ESP, stack)
        uc.reg_write(UC_X86_REG_ECX, receiver)
        uc.reg_write(UC_X86_REG_FPCW, control)
        for reg in saved:
            uc.reg_write(reg, 0x12345678)
        uc.emu_start(entry, stop, count=10000)
        assert uc.reg_read(UC_X86_REG_EIP) == stop
        assert uc.reg_read(UC_X86_REG_ESP) == stack+4
        assert all(uc.reg_read(reg) == 0x12345678 for reg in saved)
        return uc.reg_read(UC_X86_REG_EAX)

    rng, random_cases, post_cases = random.Random(0x5a1400), 0, 0
    seeds = [0, 1, 0x7ffffffe, 0x7fffffff, 0x80000000, 0xffffffff]+[rng.randrange(0x100000000) for _ in range(32)]
    for seed, scale, control in itertools.product(seeds, (-100.0, -0.0, 0.0, 0.001, 1.0, 100.0), (0x027f, 0x037f)):
        outcomes = []
        for uc in machines:
            uc.mem_write(random_object+0xf8, struct.pack("<I", seed))
            execute(uc, wrapper, control, args=struct.pack("<fI", scale, random_object+0xf8))
            outcomes.append((bytes(uc.mem_read(output, 4)), bytes(uc.mem_read(random_object+0xf8, 4))))
        assert outcomes[0] == outcomes[1], (seed, scale, control, outcomes)
        random_cases += 1
    for status, mode, state, pending, flag, ratio, control in itertools.product(
            (0, 3), (-1, 0, 1, 2, 3, 4), (-0.1, 0.0, 0.5, 1.0, 1.1), (0, 1), (0, 1), (0, 1, 2, 7), (0x027f, 0x037f)):
        clock, seed, suppression = rng.choice((0, 2000, 4000, -2)), rng.choice(seeds), rng.choice((0, 1, 2))
        float_inputs = {node+0x14: state, node+0x5c: rng.choice((0.0, 0.1, 0.5)),
                        node+0x64: rng.choice((0.0, 0.1, 0.5)), node+0x68: rng.choice((-0.1, 0.0, 0.5)),
                        component+0x14: rng.choice((-50.0, 0.0, 50.0, 100.0)),
                        component+0x78: rng.choice((0.0, 0.3, 0.5)), values: rng.uniform(-1000, 1000),
                        spec+8: rng.uniform(0.5, 5), spec+12: rng.uniform(0.5, 5), spec+16: rng.choice((0.0, 0.5, 1.0))}
        for i in range(4):
            float_inputs[wheels+i*0x100+0x28] = rng.uniform(-50, 50)
        ranges = [rng.uniform(-5, 5) for _ in range(6)]
        requested = rng.choice((0, 1, 2, 7))
        outcomes = []
        for uc, entry in zip(machines, entries):
            for address, value in ((node+0x60, status), (node+0x44, mode & 0xffffffff), (node+0x48, pending),
                                   (node+0x3c, ratio), (node+0x38, requested), (0x86518c, clock & 0xffffffff),
                                   (context+0xa0, context_entry if suppression else 0), (random_object+0xf8, seed)):
                uc.mem_write(address, struct.pack("<I", value))
            uc.mem_write(node+0x74, bytes([flag]))
            uc.mem_write(context_entry+0x41, bytes([suppression == 2]))
            uc.mem_write(node+0xcc, struct.pack("<6f", *ranges))
            uc.mem_write(node+0x4c, b"\xa5"*4)
            uc.mem_write(node+0x6c, b"\xa5"*4)
            uc.mem_write(node+0x78, b"\xa5"*12)
            for address, value in float_inputs.items():
                uc.mem_write(address, struct.pack("<f", value))
            before = [bytes(uc.mem_read(address, size)) for address, size in ((node, 0xec), (component, 0x84), (spec, 0x40))]
            execute(uc, entry[POST], control)
            after = [bytes(uc.mem_read(address, size)) for address, size in ((node, 0xec), (component, 0x84), (spec, 0x40))]
            allowed = set(range(0x14, 0x18)) | set(range(0x3c, 0x40)) | set(range(0x44, 0x50)) | set(range(0x5c, 0x60)) | set(range(0x64, 0x70)) | set(range(0x78, 0x84))
            assert all(a == b or i in allowed for i, (a, b) in enumerate(zip(before[0], after[0])))
            assert after[1][:0x80] == before[1][:0x80] and after[2] == before[2]
            outcomes.append((after, bytes(uc.mem_read(random_object+0xf8, 4))))
        if outcomes[0] != outcomes[1]:
            differences = [(region, i, a, b) for region, (left, right) in enumerate(zip(outcomes[0][0], outcomes[1][0]))
                           for i, (a, b) in enumerate(zip(left, right)) if a != b]
            raise AssertionError((status, mode, state, pending, flag, ratio, control, differences,
                                  outcomes[0][1].hex(), outcomes[1][1].hex()))
        post_cases += 1
    report = dict(executable_sha256=hashlib.sha256(raw).hexdigest(), compiler="MSVC 7.1 /Ox /Ob0 /Z7 /EHsc", functions=records,
                  random_cases=random_cases, post_update_cases=post_cases, stubbed_callees=[],
                  limitations=["Finite synthetic state only", "Not a full vehicle integration or retail trace",
                               "Semantic field names and full parameter provenance unresolved"])
    (BUILD/"drivetrain-post-update-verification.json").write_text(json.dumps(report, indent=2)+"\n")
    print(json.dumps(report, indent=2))


if __name__ == "__main__":
    main()
