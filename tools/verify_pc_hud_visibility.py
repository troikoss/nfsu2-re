#!/usr/bin/env python3
"""Execute original/rebuilt HUD feature masks and recursive FEng visibility unhooked."""
import hashlib
import itertools
import json
import random
import struct
import subprocess

import pefile
from unicorn import Uc, UC_ARCH_X86, UC_MODE_32, UC_HOOK_CODE
from unicorn.x86_const import (UC_X86_REG_ECX, UC_X86_REG_ESP, UC_X86_REG_EIP,
                              UC_X86_REG_EBX, UC_X86_REG_ESI, UC_X86_REG_EDI,
                              UC_X86_REG_EBP, UC_X86_REG_FPCW)
from probe_pc_race_reward import ROOT, EXE
from pc_coff import link_objects

BUILD = ROOT / "build/analysis/minimap"
FUNCTIONS = {"HudHideObject": (0x50ca00, 69), "HudShowObject": (0x50ca50, 82),
             "ApplyMasks": (0x4c6690, 85)}


def main():
    BUILD.mkdir(parents=True, exist_ok=True)
    raw = EXE.read_bytes()
    assert hashlib.sha256(raw).hexdigest() == "f9dd86c054878ce6276beb07c1fd61874f7a1e4bf1f241b084c65b73e24168a7"
    pe = pefile.PE(data=raw)
    image, base = pe.get_memory_mapped_image(), pe.OPTIONAL_HEADER.ImageBase
    assert base == 0x400000
    obj = BUILD / "HudVisibility.obj"
    subprocess.run([str(ROOT / "build/tools/wibo"), "build/compilers/Win32/7.1/cl.exe",
                    "/nologo", "/c", "/Ox", "/Ob0", "/Z7", "/EHsc",
                    "/Fo" + str(obj.relative_to(ROOT)), "src/Speed/Indep/Src/Hud/HudVisibility.cpp"],
                   cwd=ROOT, check=True)
    linked = link_objects([obj], {"?fengVisibilityDirty8384D4@@3HA": 0x8384d4})
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
        ranges = ranges + [(0x502fd0, 0x502fd3)]
        def guard(machine, address, size, user, ranges=ranges):
            assert any(a <= address < b for a, b in ranges), hex(address)
        uc.hook_add(UC_HOOK_CODE, guard)
        machines.append(uc)

    rng = random.Random(0x4c6690)
    counts = {name: 0 for name in FUNCTIONS}
    def compare(name, state, args, dirty, this=0):
        outputs = []
        for rebuilt, uc in zip((False, True), machines):
            uc.mem_write(arena, bytes(state))
            uc.mem_write(0x8384d4, struct.pack("<i", dirty))
            uc.mem_write(stack - 128, b"\xa5" * 256)
            uc.mem_write(stack, struct.pack("<I", stop) + args)
            uc.reg_write(UC_X86_REG_ESP, stack)
            uc.reg_write(UC_X86_REG_ECX, this)
            uc.reg_write(UC_X86_REG_FPCW, 0x027f)
            saved = (UC_X86_REG_EBX, UC_X86_REG_ESI, UC_X86_REG_EDI, UC_X86_REG_EBP)
            for reg in saved:
                uc.reg_write(reg, 0x12345678)
            uc.emu_start(entries[name] if rebuilt else FUNCTIONS[name][0], stop, count=5000)
            assert uc.reg_read(UC_X86_REG_EIP) == stop
            assert uc.reg_read(UC_X86_REG_ESP) == stack + 4 + (len(args) if this else 0)
            assert uc.reg_read(UC_X86_REG_FPCW) == 0x027f
            assert all(uc.reg_read(reg) == 0x12345678 for reg in saved)
            assert bytes(uc.mem_read(0x8384d4, 4)) == struct.pack("<i", dirty)
            outputs.append(bytes(uc.mem_read(arena, 0x8000)))
        assert outputs[0] == outputs[1], (name, dirty, args.hex())
        counts[name] += 1
        return outputs[0]

    def tree(shape):
        state = bytearray(rng.randbytes(0x10000))
        # Root group with four border children; a nested group exercises recursion.
        for i in range(7):
            off = 0x1000 + i * 0x100
            struct.pack_into("<I", state, off + 4, arena + off + 0x100 if i < 6 else 0)
            struct.pack_into("<iI", state, off + 0x18, 1, rng.choice((0, 1, 0x400000, 0x400001, rng.getrandbits(32))))
            struct.pack_into("<iI", state, off + 0x60, 0, 0)
        if shape == 1:
            struct.pack_into("<i", state, 0x1018, 5)
            struct.pack_into("<iI", state, 0x1060, 4, arena + 0x1100)
        elif shape == 2:
            struct.pack_into("<i", state, 0x1018, 5)
            struct.pack_into("<iI", state, 0x1060, 2, arena + 0x1100)
            struct.pack_into("<i", state, 0x1218, 5)
            struct.pack_into("<iI", state, 0x1260, 2, arena + 0x1500)
        elif shape in (3, 4):
            struct.pack_into("<i", state, 0x1018, 5)
            struct.pack_into("<iI", state, 0x1060, 0 if shape == 3 else -1, 0xdeadbeef)
        return state

    for name, dirty, shape, trial in itertools.product(("HudHideObject", "HudShowObject"), (0, 1, -1), range(5), range(64)):
        state = tree(shape)
        result = compare(name, state, struct.pack("<I", arena + 0x1000), dirty)
        # Reapplying exercises the no-dirty, already-hidden/shown branches.
        compare(name, bytearray(result) + state[0x8000:], struct.pack("<I", arena + 0x1000), dirty)
    for name, dirty in itertools.product(("HudHideObject", "HudShowObject"), (0, 1, -1)):
        compare(name, tree(0), struct.pack("<I", 0), dirty)
    cases = [(0, 0), (1, 0), (0, 1), (0x80000000, 0), (0, 0x80000000), (0xffffffff, 0xffffffff)]
    for required, current, dirty, empty in itertools.product(cases, cases, (0, 1), (False, True)):
        state = tree(2)
        head, node1, node2 = arena + 0x2000, arena + 0x2100, arena + 0x2200
        struct.pack_into("<II", state, 0x2000, head if empty else node1, head if empty else node2)
        struct.pack_into("<II", state, 0x2010, *required)
        struct.pack_into("<III", state, 0x2100, node2, head, arena + 0x1000)
        struct.pack_into("<III", state, 0x2200, head, node1, 0) # Original null object handling.
        result = compare("ApplyMasks", state, struct.pack("<II", *current), dirty, head)
        assert struct.unpack_from("<II", result, 0x2018) == current
        expected = bool((required[0] & current[0]) or (required[1] & current[1]))
        if not empty:
            assert bool(struct.unpack_from("<I", result, 0x101c)[0] & 1) != expected
    report = dict(executable_sha256=hashlib.sha256(raw).hexdigest(), comparisons=counts,
                  functions={name: dict(address=hex(a), size=n, sha256=hashlib.sha256(image[a-base:a-base+n]).hexdigest())
                             for name, (a, n) in FUNCTIONS.items()},
                  byte_matching="Not established", compiler="MSVC 7.1 /Ox /Ob0 /Z7 /EHsc",
                  stubbed_callees=[], checks=["whole object/feature arena exact", "both mask words including high bits",
                                            "dirty-bit conditions", "recursive groups and four border children",
                                            "empty lists, null objects, nonpositive child counts, repeated transitions",
                                            "stack and callee-saved registers"],
                  limitations=["Finite synthetic object trees and masks", "Native allocation/asset loading remain seams",
                               "Does not recover the full player mask producer or the full minimap constructor"])
    (BUILD / "visibility-verification.json").write_text(json.dumps(report, indent=2) + "\n")
    print(json.dumps(report, indent=2))


if __name__ == "__main__":
    main()
