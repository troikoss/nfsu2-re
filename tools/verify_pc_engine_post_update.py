#!/usr/bin/env python3
"""Compare UG2's recovered engine post-step and real helper chain.

Synthetic mapped states; the original executable is not launched or changed.
No instruction helpers are intercepted or replaced with fixture returns.
"""
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
from verify_pc_engine import CONTROL, ADJUST, CONVERSION, FLOATS as ENGINE_FLOATS, ORIGINAL as ENGINE_ORIGINAL
from verify_pc_drift_ranking import ADDRESSES, coff_function

BUILD = ROOT / "build/analysis/racing"
MIN = "?EngineMinimum@@YAMMM@Z"
CLAMP = "?EngineClamp@@YAMMMM@Z"
UNITS = "?EngineSineUnits@@YAMI@Z"
SINE = "?EngineSine@@YAMM@Z"
FEEDBACK = "?AdvanceFeedback@Engine@@QAEXM@Z"
POST = "?UpdateAfterIntegration@Engine@@QAEXXZ"
ORIGINAL = {CONTROL: ENGINE_ORIGINAL[CONTROL], ADJUST: ENGINE_ORIGINAL[ADJUST],
            MIN: (0x401070, 25, "1707985bde0df61ced361a9efbace1b42a67933ce1f049abe0fbe4747e683da9"),
            CLAMP: (0x401090, 45, "0bbc0be0d446e0bca0d5cc1d9ff3682ac001f6bf36f61fd3957cf28a57c7d4c0"),
            UNITS: (0x43c2b0, 150, "d030d6898336bb087f67dfe850c07f69dab4da2d322330227b386fbe3a729981"),
            SINE: (0x43c350, 24, "fe02c7e756c3cd0e138483fd06a9c480e24de8d2995121d4b6bac08daedf8b04"),
            FEEDBACK: (0x5a0f30, 675, "1c9b677b4a78d0c4b31f56063732483fe136e714182508fcba6d7d6ad94eec97"),
            POST: (0x5aa760, 196, "7e73d36297d6b0d19cc3de2354b09b658073a78d3bd068813db091f18a1532f1")}
FLOATS = {**ENGINE_FLOATS, "engineScalarScale": 0x7a1d38, "engineLimiterOffset": 0x7a18dc,
          "engineLimiterFraction": 0x7a18e0, "engineSubstepDuration": 0x7fc800,
          "engineFeedbackDivisor": 0x7a18e4, "engineFeedbackNegativeScale": 0x7a18e8,
          "engineFeedbackControlThreshold": 0x7a18ec, "engineFeedbackRiseScale": 0x7a18f0,
          "engineFeedbackCrossingThreshold": 0x7a18f4, "engineFeedbackDuration": 0x7a18f8,
          "engineFeedbackOscillationScale": 0x7a18fc, "engineFeedbackPhaseScale": 0x7a1900,
          "engineFeedbackPhaseOffset": 0x787598, "engineHalfPi": 0x784254,
          "engineAngleScale": 0x78423c, "engineAngleUnit": 0x784240, "engineThreeHalfPi": 0x786da0,
          "engineTwoPi": 0x786d9c, "enginePi": 0x786d98, "engineSineC3": 0x786d94,
          "engineSineC5": 0x786d90, "engineSineC7": 0x786d8c, "engineSineC9": 0x786d88}


def main():
    raw = EXE.read_bytes()
    assert hashlib.sha256(raw).hexdigest() == "f9dd86c054878ce6276beb07c1fd61874f7a1e4bf1f241b084c65b73e24168a7"
    pe = pefile.PE(data=raw)
    image, base = pe.get_memory_mapped_image(), pe.OPTIONAL_HEADER.ImageBase
    for address, size, digest in (*ORIGINAL.values(), CONVERSION):
        assert hashlib.sha256(image[address-base:address-base+size]).hexdigest() == digest
    units = ("EngineAdjustment", "EngineMath", "EngineFeedback", "EnginePostUpdate")
    BUILD.mkdir(parents=True, exist_ok=True)
    paths = []
    for unit in units:
        path = BUILD/(unit+".obj")
        subprocess.run([str(ROOT/"build/tools/wibo"), "build/compilers/Win32/7.1/cl.exe", "/nologo", "/c",
                        "/Ox", "/Ob0", "/Z7", "/EHsc", "/Fo"+str(path.relative_to(ROOT)),
                        "src/Speed/Indep/Src/Physics/"+unit+".cpp"], cwd=ROOT, check=True)
        paths.append(path)
    linked = link_objects(paths, {**{"?"+name+"@@3MA": a for name, a in FLOATS.items()}, "__ftol2": CONVERSION[0]})
    # Boundable helper bodies can be checked with original-address relocation.
    # The feedback switch's local jump-table linkage is behavior-tested below.
    ADDRESSES.update({"?"+name+"@@3MA": a for name, a in FLOATS.items()})
    ADDRESSES.update({symbol: data[0] for symbol, data in ORIGINAL.items()})
    ADDRESSES.update({"__ftol2": CONVERSION[0], "__real@00000000": 0x7d7d3c,
                      "__real@3f800000": 0x784250, "__real@bf800000": 0x7842a0,
                      "__real@40000000": 0x7844c0})
    byte_records = {}
    for symbol in (MIN, CLAMP, SINE):
        body, relocations = coff_function(BUILD/"EngineMath.obj", symbol)
        address, size, _ = ORIGINAL[symbol]
        byte_records[symbol] = dict(byte_identical_after_relocation=body == image[address-base:address-base+size], relocations=relocations)
    engine, spec, drivetrain, output, stack, stop = 0x2000000, 0x2001000, 0x2002000, 0x2003000, 0x200f000, 0x2101000
    wrappers = {MIN: (0x2100000, 2), CLAMP: (0x2100100, 3), UNITS: (0x2100200, 1), SINE: (0x2100300, 1)}
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
        ranges = (linked["ranges"] if rebuilt else [(data[0], data[0]+data[1]) for data in ORIGINAL.values()]) + [(CONVERSION[0], CONVERSION[0]+CONVERSION[1])]
        for symbol, (wrapper, count) in wrappers.items():
            prefix = bytes([0xff, 0x74, 0x24, count*4])*count
            code = prefix+b"\xe8"+struct.pack("<i", entry[symbol]-wrapper-len(prefix)-5)
            code += b"\x83\xc4"+bytes([count*4])+b"\xd9\x1d"+struct.pack("<I", output)+b"\xc2"+struct.pack("<H", count*4)
            uc.mem_write(wrapper, code)
            ranges.append((wrapper, wrapper+len(code)))
        def guard(machine, address, size, user, ranges=ranges):
            if not any(start <= address < end for start, end in ranges):
                raise RuntimeError(f"Unexpected instruction/helper at {address:#x}")
        uc.hook_add(UC_HOOK_CODE, guard)
        uc.mem_write(engine+0x18, struct.pack("<I", spec))
        uc.mem_write(engine+0x20, struct.pack("<I", drivetrain))
        machines.append(uc)
        entries.append(entry)
    saved = (UC_X86_REG_EBX, UC_X86_REG_ESI, UC_X86_REG_EDI, UC_X86_REG_EBP)
    def run(uc, entry, precision, args=b"", floating=False):
        uc.mem_write(output-16, b"\xa5"*36)
        uc.mem_write(stack, struct.pack("<I", stop)+args)
        uc.reg_write(UC_X86_REG_ESP, stack)
        uc.reg_write(UC_X86_REG_ECX, engine)
        uc.reg_write(UC_X86_REG_FPCW, precision)
        for reg in saved:
            uc.reg_write(reg, 0x12345678)
        uc.emu_start(entry, stop, count=10000)
        assert uc.reg_read(UC_X86_REG_EIP) == stop
        assert uc.reg_read(UC_X86_REG_ESP) == stack+4+len(args)
        assert all(uc.reg_read(reg) == 0x12345678 for reg in saved)
        data = bytes(uc.mem_read(output-16, 36))
        assert data[:16] == b"\xa5"*16 and data[20:] == b"\xa5"*16
        if not floating:
            assert data == b"\xa5"*36
        return data[16:20]
    def compare_float(symbol, args, precision, label):
        outputs = [run(uc, wrappers[symbol][0], precision, args, floating=True) for uc in machines]
        assert outputs[0] == outputs[1], (label, precision, [o.hex() for o in outputs])
    counts = dict(sine_unit_cases=0, sine_angle_cases=0, clamp_cases=0, feedback_cases=0, post_update_cases=0, limiter_boundary_cases=0)
    def neighbors(value):
        bits = struct.unpack("<I", struct.pack("<f", value))[0]
        return [struct.unpack("<f", struct.pack("<I", bits+d))[0] for d in (-1, 0, 1)]
    scale = struct.unpack_from("<f", image, FLOATS["engineScalarScale"]-base)[0]
    converted_scale = struct.unpack_from("<f", image, FLOATS["engineValueScale"]-base)[0]
    offset = struct.unpack_from("<f", image, FLOATS["engineLimiterOffset"]-base)[0]
    fraction = struct.unpack_from("<f", image, FLOATS["engineLimiterFraction"]-base)[0]
    for precision, minimum, maximum, flag in itertools.product((0x027f, 0x037f), (750.0, 1000.0), (6000.0, 8500.0), (0, 1, 2)):
        for boundary in (minimum*scale, maximum*scale, (maximum+offset)/converted_scale, (maximum-maximum*fraction)/converted_scale):
            for scalar in neighbors(boundary):
                outcomes = []
                for uc, entry in zip(machines, entries):
                    uc.mem_write(spec, struct.pack("<3f", minimum, 4000, maximum))
                    uc.mem_write(engine+0x14, struct.pack("<f", scalar))
                    uc.mem_write(engine+0x88, bytes([flag]))
                    uc.mem_write(engine+0x98, struct.pack("<f", 0.0))
                    uc.mem_write(engine+0x8c, struct.pack("<f", -1.0))
                    run(uc, entry[POST], precision)
                    outcomes.append(bytes(uc.mem_read(engine, 0xb0)))
                assert outcomes[0] == outcomes[1], ("limiter boundary", precision, minimum, maximum, flag, scalar,
                        [(i, a, b) for i, (a, b) in enumerate(zip(*outcomes)) if a != b])
                counts["limiter_boundary_cases"] += 1
    # Exhaust all original 16-bit angular inputs in both x87 precision modes.
    for precision, value in itertools.product((0x027f, 0x037f), range(65536)):
        compare_float(UNITS, struct.pack("<I", value), precision, ("units", value))
        counts["sine_unit_cases"] += 1
    for precision, angle in itertools.product((0x027f, 0x037f), (-100.0, -6.28, -1.57, -0.0, 0.0, 1.57, 3.14, 6.28, 100.0)):
        compare_float(SINE, struct.pack("<f", angle), precision, ("angle", angle))
        counts["sine_angle_cases"] += 1
    for precision, a, b, c in itertools.product((0x027f, 0x037f), (-2.0, -0.0, 0.0, 1.0, 3.0), (-2.0, 0.0, 2.0), (-1.0, 0.0, 1.0)):
        compare_float(MIN, struct.pack("<2f", a, b), precision, ("minimum", a, b))
        compare_float(CLAMP, struct.pack("<3f", a, b, c), precision, ("clamp", a, b, c))
        counts["clamp_cases"] += 1
    rng = random.Random(0x5a0f30)
    for precision, mode, capacity, duration, control, flag, trial in itertools.product(
            (0x027f, 0x037f), (-1, 0, 1, 2, 3), (-1.0, 0.0, 10.0), (0.0, 1/240, 0.1), (0.0, 0.3, 1.0), (0, 1), range(4)):
        values = {0x14: rng.uniform(50, 950), 0x78: control, 0x7c: rng.uniform(-10, 10), 0x80: 1.0,
                  0x8c: rng.choice((-1.0, 0.1, 1.1)), 0x98: capacity, 0x9c: rng.uniform(-30, 0),
                  0xa4: rng.uniform(-30, -1), 0xa8: rng.choice((0.01, 0.1, 0.4, 0.49)), 0xac: rng.choice((0.0, -10.0, -30.0))}
        for symbol in (FEEDBACK, POST):
            outcomes = []
            for uc, entry in zip(machines, entries):
                for offset, value in values.items():
                    uc.mem_write(engine+offset, struct.pack("<f", value))
                uc.mem_write(engine+0xa0, struct.pack("<I", mode & 0xffffffff))
                uc.mem_write(engine+0x88, bytes([flag]))
                uc.mem_write(spec, struct.pack("<3f", 750, 4000, 8500))
                uc.mem_write(drivetrain+0x3c, struct.pack("<I", 2))
                uc.mem_write(drivetrain+0x68, struct.pack("<f", 0.0))
                uc.mem_write(0x7fc800, struct.pack("<f", duration))
                before = bytes(uc.mem_read(engine, 0xb0))
                run(uc, entry[symbol], precision, struct.pack("<f", duration) if symbol == FEEDBACK else b"")
                after = bytes(uc.mem_read(engine, 0xb0))
                allowed = set(range(0x9c, 0xb0))
                if symbol == POST:
                    allowed |= set(range(0x14, 0x18)) | set(range(0x7c, 0x80)) | {0x88} | set(range(0x8c, 0x90))
                assert all(a == b or i in allowed for i, (a, b) in enumerate(zip(before, after)))
                outcomes.append(after)
            if outcomes[0] != outcomes[1]:
                differences = [(i, a, b) for i, (a, b) in enumerate(zip(*outcomes)) if a != b]
                raise AssertionError((symbol, precision, mode, capacity, duration, control, flag, trial, differences, values))
            counts["feedback_cases" if symbol == FEEDBACK else "post_update_cases"] += 1
    records = [dict(symbol=symbol, original_address=hex(data[0]), original_size=data[1], original_sha256=data[2],
                    rebuilt_size=linked["functions"][symbol]["size"], byte_match_status="Not assessed by section-linked execution")
               for symbol, data in ORIGINAL.items()]
    for record in records:
        if record["symbol"] in byte_records:
            record.update(byte_records[record["symbol"]])
            del record["byte_match_status"]
        elif record["original_size"] != record["rebuilt_size"]:
            record["byte_identical_after_relocation"] = False
            record["byte_match_reason"] = "Compiled/original body lengths differ"
            del record["byte_match_status"]
    report = dict(executable_sha256=hashlib.sha256(raw).hexdigest(), compiler="MSVC 7.1 /Ox /Ob0 /Z7 /EHsc",
                  functions=records, **counts, stubbed_callees=[], original_runtime_helper=hex(CONVERSION[0]),
                  limitations=["Finite synthetic states and parameters", "Engine constructor/parameter loading and complete vehicle excluded"])
    (BUILD/"engine-post-update-verification.json").write_text(json.dumps(report, indent=2)+"\n")
    print(json.dumps(report, indent=2))


if __name__ == "__main__":
    main()
