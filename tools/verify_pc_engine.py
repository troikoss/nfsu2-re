#!/usr/bin/env python3
"""Compare recovered UG2 engine curves, drive value and derivative unhooked.

Finite synthetic records are analysis fixtures, not replacement game parameters.
The original float-to-integer helper executes on both sides; it is not stubbed.
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
from pc_coff import link_objects
from verify_pc_drift_ranking import ADDRESSES, coff_function

BUILD = ROOT / "build/analysis/racing"
CURVE = "?Lookup@EngineCurve@@QAEMM@Z"
DRIVE = "?ComputeDriveValue@Engine@@QAEMXZ"
RATE = "?WriteDerivative@Engine@@QAEHPAM@Z"
CONTROL = "?GetEffectiveControl@Engine@@QAEMXZ"
ADJUST = "?AdvanceAdjustment@Engine@@QAEXM@Z"
ORIGINAL = {CURVE: (0x5764d0, 138, "0b2593774ad52b8e54ebad8bb77708fa9e7b72f5c1805b7614866eea8bece4c7"),
            DRIVE: (0x594300, 406, "0af67834f4e55710981cbd8fb981085cae3ac7fa805c0320260d541d601f77e3"),
            RATE: (0x5a1290, 56, "4626bcf97c685b772e955a3dabb10518c2f390c6bfd1a00e5d17b32f532ed351"),
            CONTROL: (0x5942e0, 27, "cfbf76d7fc302af3414a8c2644175dd96d0d83694756ecac8b09e17c99f041cf"),
            ADJUST: (0x5a11e0, 172, "7735504551aea41e31d787aaac31d0a2ef2495a692da07a24cf29f711871955a")}
CONVERSION = (0x75bed4, 117, "5333b186c02836974c6f792303aeb2c00d856316b93ccbbe65f51def6ae661b4")
FLOATS = {"engineValueScale": 0x7a1d28, "engineFlagScale": 0x7fbff4,
          "engineAdjustmentThreshold": 0x7fbff0, "engineAdjustmentScale": 0x7a1914,
          "engineFlagDragScale": 0x7a18d8, "engineRatioOneDragScale": 0x7a1918,
          "engineOtherRatioDragScale": 0x7a191c, "engineModeDivisorScale": 0x7a1920,
          "engineAdjustmentUpper": 0x7a190c, "engineAdjustmentStop": 0x7a1910,
          "engineAdjustmentAboveRate": 0x7a1904, "engineAdjustmentBelowRate": 0x7a1908}


def main():
    BUILD.mkdir(parents=True, exist_ok=True)
    raw = EXE.read_bytes()
    assert hashlib.sha256(raw).hexdigest() == "f9dd86c054878ce6276beb07c1fd61874f7a1e4bf1f241b084c65b73e24168a7"
    pe = pefile.PE(data=raw)
    image, base = pe.get_memory_mapped_image(), pe.OPTIONAL_HEADER.ImageBase
    for address, size, digest in (*ORIGINAL.values(), CONVERSION):
        assert hashlib.sha256(image[address-base:address-base+size]).hexdigest() == digest
    units = {CURVE: "EngineCurve", DRIVE: "EngineDriveValue", RATE: "EngineDerivative", CONTROL: "EngineAdjustment", ADJUST: "EngineAdjustment"}
    paths = []
    for unit in dict.fromkeys(units.values()):
        path = BUILD/(unit+".obj")
        subprocess.run([str(ROOT/"build/tools/wibo"), "build/compilers/Win32/7.1/cl.exe", "/nologo", "/c",
                        "/Ox", "/Ob0", "/Z7", "/EHsc", "/Fo"+str(path.relative_to(ROOT)),
                        "src/Speed/Indep/Src/Physics/"+unit+".cpp"], cwd=ROOT, check=True)
        paths.append(path)
    externals = {"?"+name+"@@3MA": address for name, address in FLOATS.items()}
    externals.update({"?engineExtraCoefficients@@3PAMA": 0x7a1798, "__ftol2": CONVERSION[0]})
    linked = link_objects(paths, externals)
    ADDRESSES.update(externals)
    ADDRESSES.update({symbol: data[0] for symbol, data in ORIGINAL.items()})
    ADDRESSES.update({"__real@00000000": 0x7d7d3c, "__real@3f800000": 0x784250})
    records = []
    for symbol, data in ORIGINAL.items():
        body, relocations = coff_function(BUILD/(units[symbol]+".obj"), symbol)
        records.append(dict(symbol=symbol, original_address=hex(data[0]), original_size=data[1], original_sha256=data[2],
                            rebuilt_size=len(body), byte_identical_after_relocation=body == image[data[0]-base:data[0]-base+data[1]],
                            relocations=relocations))
    engine, spec, drivetrain, owner, attributes, samples = 0x2000000, 0x2001000, 0x2002000, 0x2003000, 0x2004000, 0x2005000
    standalone, output, stack, stop = 0x2006000, 0x2007000, 0x200f000, 0x2101000
    wrappers = {CURVE: 0x2100000, DRIVE: 0x2100100, CONTROL: 0x2100200}
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
        ranges = linked["ranges"] if rebuilt else [(data[0], data[0]+data[1]) for data in ORIGINAL.values()]
        ranges = ranges+[(CONVERSION[0], CONVERSION[0]+CONVERSION[1])]
        for symbol, wrapper in wrappers.items():
            prefix = b"\xff\x74\x24\x04" if symbol == CURVE else b""
            code = prefix+b"\xe8"+struct.pack("<i", entry[symbol]-wrapper-len(prefix)-5)
            code += b"\xd9\x1d"+struct.pack("<I", output)+(b"\xc2\x04\x00" if symbol == CURVE else b"\xc3")
            uc.mem_write(wrapper, code)
            ranges.append((wrapper, wrapper+len(code)))
        def guard(machine, address, size, user, ranges=ranges):
            if not any(start <= address < end for start, end in ranges):
                raise RuntimeError(f"Unexpected instruction/helper at {address:#x}")
        uc.hook_add(UC_HOOK_CODE, guard)
        for address, value in ((engine+0x18, spec), (engine+0x1c, owner), (engine+0x20, drivetrain), (owner, attributes)):
            uc.mem_write(address, struct.pack("<I", value))
        machines.append(uc)
        entries.append(entry)

    saved = (UC_X86_REG_EBX, UC_X86_REG_ESI, UC_X86_REG_EDI, UC_X86_REG_EBP)
    def run(uc, entry, control, receiver, args=b"", popped=0, integer_return=False):
        uc.mem_write(output-16, b"\xa5"*36)
        uc.mem_write(stack, struct.pack("<I", stop)+args)
        uc.reg_write(UC_X86_REG_ESP, stack)
        uc.reg_write(UC_X86_REG_ECX, receiver)
        uc.reg_write(UC_X86_REG_FPCW, control)
        for reg in saved:
            uc.reg_write(reg, 0x12345678)
        uc.emu_start(entry, stop, count=10000)
        assert uc.reg_read(UC_X86_REG_EIP) == stop
        assert uc.reg_read(UC_X86_REG_ESP) == stack+4+popped
        assert all(uc.reg_read(reg) == 0x12345678 for reg in saved)
        if integer_return:
            assert uc.reg_read(UC_X86_REG_EAX) == 0
        buffer = bytes(uc.mem_read(output-16, 36))
        assert buffer[:16] == b"\xa5"*16 and buffer[20:] == b"\xa5"*16
        return buffer[16:20]

    def write_curve(uc, address, count, start, end, inverse, data, sample_address):
        uc.mem_write(address, struct.pack("<ifffI", count, start, end, inverse, sample_address))
        uc.mem_write(sample_address, struct.pack("<"+"f"*count, *data))

    rng, curve_cases, engine_cases = random.Random(0x594300), 0, 0
    def neighbors(value):
        bits = struct.unpack("<I", struct.pack("<f", value))[0]
        if value == 0:
            return [-1e-40, 0.0, 1e-40]
        return [struct.unpack("<f", struct.pack("<I", bits+d))[0] for d in (-1, 0, 1)]
    for count, start, spacing, control in itertools.product((1, 2, 3, 9), (0.0, 750.0), (0.001, 0.0025, 1.0), (0x027f, 0x037f)):
        data = [rng.uniform(-100, 100) for _ in range(count)]
        inputs = [start-1000, start+1000/spacing]
        for knot in range(count+1):
            inputs.extend(neighbors(start+knot/spacing))
        for uc in machines:
            write_curve(uc, standalone, count, start, start+(count-1)/spacing, spacing, data, samples)
        for input_value in inputs:
            results = [run(uc, wrappers[CURVE], control, standalone, struct.pack("<f", input_value), popped=4) for uc in machines]
            assert results[0] == results[1], (count, start, spacing, control, input_value, results)
            curve_cases += 1

    threshold = struct.unpack_from("<f", image, FLOATS["engineAdjustmentThreshold"]-base)[0]
    adjustment_values = [-1.0, 0.0, 1.0, 1.5]+neighbors(threshold)
    for flag, limiter, extra, adjustment, input_value, ratio, precision, speed in itertools.product(
            (0, 1), (0, 1), (-1, 0, 1, 3), adjustment_values, (0.0, 0.4, 1.0), (0, 1, 2),
            (0x027f, 0x037f), (0.0, 750.0, 3000.0, 9000.0)):
        coefficient, control, feedback, divisor = rng.uniform(0.5, 2), rng.uniform(0.5, 1.5), rng.uniform(-50, 50), rng.uniform(0.1, 5)
        mode, divisor_scale, extra_scale = rng.randrange(4), rng.choice((0.5, 1.0, 2.0)), rng.uniform(0.0, 2.0)
        data_sets = [[rng.uniform(-100, 500) for _ in range(9)], [rng.uniform(0, 100) for _ in range(3)],
                     [rng.uniform(-100, 500) for _ in range(9)], [rng.uniform(-10, 10) for _ in range(9)]]
        for uc in machines:
            for i, data in enumerate(data_sets):
                write_curve(uc, engine+0x24+i*0x14, len(data), 750, 8500, (len(data)-1)/7750, data, samples+i*0x100)
            for address, value in ((engine+0x14, speed/struct.unpack_from("<f", image, 0x7a1d28-base)[0]),
                                   (engine+0x78, input_value), (engine+0x7c, feedback), (engine+0x80, control),
                                   (engine+0x8c, adjustment), (engine+0x90, coefficient), (spec+0xc, divisor),
                                   (0x7a1920, divisor_scale), (attributes+0x2ac, extra_scale)):
                uc.mem_write(address, struct.pack("<f", value))
            for address, value in ((engine+0x74, flag), (engine+0x84, extra & 0xffffffff), (drivetrain+0x3c, ratio), (drivetrain+0x44, mode)):
                uc.mem_write(address, struct.pack("<I", value))
            uc.mem_write(engine+0x88, bytes([limiter]))
        results = []
        for uc, entry in zip(machines, entries):
            before = [bytes(uc.mem_read(a, n)) for a, n in ((engine, 0x94), (spec, 0x10), (drivetrain, 0x48), (samples, 0x400))]
            drive = run(uc, wrappers[DRIVE], precision, engine)
            rate = run(uc, entry[RATE], precision, engine, struct.pack("<I", output), popped=4, integer_return=True)
            assert before == [bytes(uc.mem_read(a, n)) for a, n in ((engine, 0x94), (spec, 0x10), (drivetrain, 0x48), (samples, 0x400))]
            results.append((drive, rate))
        assert results[0] == results[1], (flag, limiter, extra, adjustment, input_value, ratio, precision, speed, results)
        engine_cases += 1
    control_cases, adjustment_cases = 0, 0
    for flag, input_value, control, precision in itertools.product((0, 1, 2), (-1.0, -0.0, 0.0, 0.1, 1.0, 2.0), (-1.0, 0.0, 0.7, 1.0), (0x027f, 0x037f)):
        results = []
        for uc in machines:
            uc.mem_write(engine+0x88, bytes([flag]))
            uc.mem_write(engine+0x78, struct.pack("<f", input_value))
            uc.mem_write(engine+0x80, struct.pack("<f", control))
            results.append(run(uc, wrappers[CONTROL], precision, engine))
        assert results[0] == results[1], (flag, input_value, control, precision, results)
        control_cases += 1
    adjustment_values = [-1.0, -0.0, 0.0, 0.6]+neighbors(1.2)+neighbors(2.0)
    for adjustment, ratio, drivetrain_adjustment, duration, precision, speed in itertools.product(
            adjustment_values, (1, 2, 3), (-1.0, 0.0, 1.0), (-0.1, 0.0, 1/60, 0.1, 1.0),
            (0x027f, 0x037f), (750.0, 4000.0, 9000.0)):
        results = []
        for uc, entry in zip(machines, entries):
            for address, value in ((engine+0x8c, adjustment), (drivetrain+0x68, drivetrain_adjustment),
                                   (engine+0x14, speed/struct.unpack_from("<f", image, 0x7a1d28-base)[0]), (spec+4, 4000.0)):
                uc.mem_write(address, struct.pack("<f", value))
            uc.mem_write(drivetrain+0x3c, struct.pack("<I", ratio))
            before = bytes(uc.mem_read(engine, 0x94))
            run(uc, entry[ADJUST], precision, engine, struct.pack("<f", duration), popped=4)
            after = bytes(uc.mem_read(engine, 0x94))
            assert before[:0x8c] == after[:0x8c] and before[0x90:] == after[0x90:]
            results.append(after)
        assert results[0] == results[1], (adjustment, ratio, drivetrain_adjustment, duration, precision, speed,
                                         [r[0x8c:0x90].hex() for r in results])
        adjustment_cases += 1
    report = dict(executable_sha256=hashlib.sha256(raw).hexdigest(), compiler="MSVC 7.1 /Ox /Ob0 /Z7 /EHsc",
                  functions=records, curve_cases=curve_cases, engine_drive_and_derivative_cases=engine_cases,
                  effective_control_cases=control_cases, adjustment_cases=adjustment_cases,
                  original_runtime_helper=dict(address=hex(CONVERSION[0]), size=CONVERSION[1], sha256=CONVERSION[2]),
                  stubbed_callees=[], limitations=["Finite synthetic parameters only", "Additional-drive indices -1,0,1,3",
                                                  "Engine post-update, force feedback and car body/contact dynamics excluded"])
    (BUILD/"engine-verification.json").write_text(json.dumps(report, indent=2)+"\n")
    print(json.dumps(report, indent=2))


if __name__ == "__main__":
    main()
