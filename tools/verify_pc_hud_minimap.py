#!/usr/bin/env python3
"""Bounded original/rebuilt minimap helper comparisons; retail data remains local."""
import hashlib
import itertools
import json
import random
import re
import struct
import subprocess

import pefile
from unicorn import Uc, UC_ARCH_X86, UC_MODE_32, UC_HOOK_CODE
from unicorn.x86_const import *
from probe_pc_race_reward import ROOT, EXE
from pc_coff import link_objects, read_object
from verify_pc_engine import CONVERSION

BUILD = ROOT / "build/analysis/minimap"
FUNCTIONS = {"HudArcSineUnits": (0x43c490, 216), "HudAngleUnits": (0x43e6d0, 326),
             "LoadTrackInfoChunk": (0x5d3eb0, 55), "FindTrackInfo": (0x5d3e40, 58), "GetPose": (0x4ac2b0, 0x1ab),
             "FindIndicator": (0x4ca800, 0x40)}


def main():
    BUILD.mkdir(parents=True, exist_ok=True)
    raw = EXE.read_bytes()
    assert hashlib.sha256(raw).hexdigest() == "f9dd86c054878ce6276beb07c1fd61874f7a1e4bf1f241b084c65b73e24168a7"
    pe = pefile.PE(data=raw)
    image, base = pe.get_memory_mapped_image(), pe.OPTIONAL_HEADER.ImageBase
    obj = BUILD / "HudMinimap.obj"
    subprocess.run([str(ROOT / "build/tools/wibo"), "build/compilers/Win32/7.1/cl.exe",
                    "/nologo", "/c", "/Ox", "/Ob0", "/Z7", "/EHsc",
                    "/Fo" + str(obj.relative_to(ROOT)), "src/Speed/Indep/Src/Hud/HudMinimap.cpp"],
                   cwd=ROOT, check=True)
    assert base == 0x400000
    for address, size, digest in [CONVERSION, (0x7ea628, 1536, "39bc638916ff039f81e6c23b77583376fe5799ffbbca573dc61215a74e1a6217")]:
        assert hashlib.sha256(image[address-base:address-base+size]).hexdigest() == digest
    symbols = read_object(obj)["symbols"]
    externals = {"__ftol2": CONVERSION[0], "?EngineSine@@YAMM@Z": 0x43c350,
                 "?HudNameHash@@YAIPBDI@Z": 0x43db80,
                 "?HudMinimapSeam@@YAXPBD@Z": 0x2100000}
    for symbol in symbols.values():
        name = symbol["name"]
        match = re.match(r"\?[^@]+?([0-9A-F]{6,8})@@", name)
        if symbol["section"] == 0 and match:
            externals[name] = int(match[1], 16)
    externals["?engineHalfPi@@3MA"] = 0x784254
    externals["?IsSet@HudFlagHolder@@QAE_NXZ"] = 0x501690 # Recovered in HudPlayerMasks.cpp.
    linked = link_objects([obj], externals)
    entries = {name: next(address for symbol, address in linked["exports"].items()
                          if symbol.startswith("?" + name + "@")) for name in FUNCTIONS}
    arena, stack, stop = 0x2000000, 0x200f000, 0x2100000
    machines = []
    for rebuilt in (False, True):
        uc = Uc(UC_ARCH_X86, UC_MODE_32)
        uc.mem_map(base, (len(image) + 4095) & ~4095)
        uc.mem_write(base, image)
        uc.mem_map(arena, 0x10000)
        uc.mem_map(stop, 0x1000)
        uc.mem_map(linked["address"], linked["allocation"])
        for address, body in linked["blocks"]:
            uc.mem_write(address, body)
        ranges = linked["ranges"] if rebuilt else [(a, a + n) for a, n in FUNCTIONS.values()]
        ranges = ranges + [(CONVERSION[0], CONVERSION[0] + CONVERSION[1]),
                           (0x43c2b0, 0x43c387), (0x401100, 0x40111a), (0x502fd0, 0x502fd3),
                           (0x501690, 0x501699)]
        def guard(machine, address, size, user, ranges=ranges):
            assert any(a <= address < b for a, b in ranges), hex(address)
        uc.hook_add(UC_HOOK_CODE, guard)
        machines.append(uc)

    def compare(name, args, precision=0x027f, state=None, this=0, output=None):
        results = []
        for rebuilt, uc in zip((False, True), machines):
            uc.mem_write(arena, state or bytes(0x10000))
            uc.mem_write(stack - 128, b"\xa5" * 256)
            uc.mem_write(stack, struct.pack("<I", stop) + args)
            uc.reg_write(UC_X86_REG_ESP, stack)
            uc.reg_write(UC_X86_REG_ECX, this)
            uc.reg_write(UC_X86_REG_FPCW, precision)
            saved = (UC_X86_REG_EBX, UC_X86_REG_ESI, UC_X86_REG_EDI, UC_X86_REG_EBP)
            for reg in saved:
                uc.reg_write(reg, 0x12345678)
            uc.emu_start(entries[name] if rebuilt else FUNCTIONS[name][0], stop, count=5000)
            assert uc.reg_read(UC_X86_REG_EIP) == stop
            assert uc.reg_read(UC_X86_REG_ESP) == stack + 4 + (len(args) if this else 0)
            assert uc.reg_read(UC_X86_REG_FPCW) == precision
            assert all(uc.reg_read(reg) == 0x12345678 for reg in saved)
            value = uc.reg_read(UC_X86_REG_EAX)
            if name.startswith("Hud"):
                value &= 0xffff
            if name == "GetPose":
                value = bytes(uc.mem_read(output, 16))
            results.append((value, bytes(uc.mem_read(0x883da0, 8))))
        assert results[0] == results[1], (name, args.hex(), hex(precision), results)

    def neighbors(value):
        bits = struct.unpack("<I", struct.pack("<f", value))[0]
        return [struct.unpack("<f", struct.pack("<I", bits + d))[0]
                for d in (-1, 0, 1) if 0 <= bits + d < 0x7f800000]
    ratios = [0.0, -0.0, 1.0, -1.0, 1.01, -1.01]
    for level in range(12):
        base_value = 1.0 - 2.0 ** (-level)
        step = 2.0 ** (-level - 1) / 16
        for index in range(17):
            ratios += neighbors(base_value + index * step)
    counts = {name: 0 for name in FUNCTIONS}
    for ratio, sign, precision in itertools.product(ratios, (-1, 1), (0x027f, 0x037f)):
        compare("HudArcSineUnits", struct.pack("<f", ratio * sign), precision)
        counts["HudArcSineUnits"] += 1
    rng = random.Random(0x43e6d0)
    vectors = list(itertools.product((-1, -0.01, -0.0, 0.0, 0.01, 1), repeat=2))
    vectors += [(rng.uniform(-1000, 1000), rng.uniform(-1000, 1000)) for _ in range(2048)]
    for x, y, precision in ((x, y, p) for x, y in vectors for p in (0x027f, 0x037f)):
        compare("HudAngleUnits", struct.pack("<ff", x, y), precision)
        counts["HudAngleUnits"] += 1
    for chunk_id, length in itertools.product((0, 0x34200, 0x34201, 0x34202), (0, 0x127, 0x128, 0x129, 0x370)):
        state = bytearray(0x10000)
        struct.pack_into("<II", state, 0, chunk_id, length)
        compare("LoadTrackInfoChunk", struct.pack("<I", arena), state=bytes(state))
        counts["LoadTrackInfoChunk"] += 1
    state = bytearray(0x10000)
    for index, number in enumerate((4000, 4011, -1, 4011)):
        struct.pack_into("<h", state, index * 0x128 + 0x8a, number)
    for count, number in itertools.product((-1, 0, 1, 4), (4000, 4011, -1, 65535, 4800)):
        for uc in machines:
            uc.mem_write(0x883da0, struct.pack("<Ii", arena, count))
        compare("FindTrackInfo", struct.pack("<i", number), state=bytes(state))
        counts["FindTrackInfo"] += 1
    for indicator_type, precision, trial in itertools.product((1, 2, 4, 5, 18), (0x027f, 0x037f), range(128)):
        state = bytearray(0x10000)
        node, slot, player, owner, output = arena + 0x1000, arena + 0x2000, arena + 0x3000, arena + 0x4000, arena + 0x6000
        struct.pack_into("<I", state, 0x101c, slot)
        struct.pack_into("<II", state, 0x2000, 0, indicator_type)
        struct.pack_into("<3f", state, 0x100c, rng.uniform(-1e4, 1e4), rng.uniform(-1e4, 1e4), rng.uniform(-360, 360))
        struct.pack_into("<I", state, 0x3004, owner)
        struct.pack_into("<2f", state, 0x4050, rng.uniform(-1e4, 1e4), rng.uniform(-1e4, 1e4))
        struct.pack_into("<H", state, 0x440c, rng.randrange(65536))
        for uc in machines:
            uc.mem_write(0x8900ac, struct.pack("<I", player))
        compare("GetPose", struct.pack("<II", output, output + 8), precision, bytes(state), node, output)
        counts["GetPose"] += 1
    for requested_type, requested_id in itertools.product((0, 1, 4, 18), (-1, 0, 1, 2)):
        state = bytearray(0x10000)
        head, node1, node2, slot = arena + 0x1000, arena + 0x2000, arena + 0x2100, arena + 0x3000
        struct.pack_into("<II", state, 0x1000, node1, node2)
        struct.pack_into("<IIi", state, 0x2000, node2, head, 0)
        struct.pack_into("<I", state, 0x201c, slot)
        struct.pack_into("<IIi", state, 0x2100, head, node1, 1)
        struct.pack_into("<II", state, 0x3000, 0, 4)
        compare("FindIndicator", struct.pack("<ii", requested_type, requested_id), state=bytes(state), this=head)
        counts["FindIndicator"] += 1
    report = dict(executable_sha256=hashlib.sha256(raw).hexdigest(), comparisons=counts,
                  functions={name: dict(address=hex(a), size=n, sha256=hashlib.sha256(image[a-base:a-base+n]).hexdigest())
                             for name, (a, n) in FUNCTIONS.items()},
                  byte_matching="Not established", compiler="MSVC 7.1 /Ox /Ob0 /Z7 /EHsc",
                  checks=["exact return, pose output bytes and TrackInfo global writes", "lookup-bin neighbors", "both x87 precisions",
                          "stack and callee-saved registers"],
                  limitations=["Finite inputs; nonfinite angles excluded", "Original conversion helper and arcsine table retained",
                               "Per-frame update, visibility/refresh, AI/career indicators and native rendering are not covered",
                               "GetPose retains original sine helper; type 3 is excluded"])
    (BUILD / "verification.json").write_text(json.dumps(report, indent=2) + "\n")
    print(json.dumps(report, indent=2))


if __name__ == "__main__":
    main()
