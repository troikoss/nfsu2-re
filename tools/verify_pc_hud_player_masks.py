#!/usr/bin/env python3
"""Execute the original/rebuilt HUD player mask producer 0x600090 and its helpers."""
import hashlib
import json
import random
import struct
import os
import subprocess

import pefile
from unicorn import Uc, UC_ARCH_X86, UC_MODE_32, UC_HOOK_BLOCK
from unicorn.x86_const import (UC_X86_REG_EAX, UC_X86_REG_ECX, UC_X86_REG_EDX, UC_X86_REG_ESP,
                              UC_X86_REG_EIP, UC_X86_REG_EBX, UC_X86_REG_ESI, UC_X86_REG_EDI,
                              UC_X86_REG_EBP, UC_X86_REG_FPCW, UC_X86_REG_FPSW, UC_X86_REG_FPTAG)
from probe_pc_race_reward import ROOT, EXE
from pc_coff import link_objects, read_object

BUILD = ROOT / "build/analysis/hudmasks"
SOURCE = "src/Speed/Indep/Src/Hud/HudPlayerMasks.cpp"
# Rebuilt symbol prefix: (original address, size).
FUNCTIONS = {
    "?GetHudMask@PlayerRecord@": (0x600090, 1247),
    "?GetScoreMask@PlayerRecord@": (0x5ea8c0, 29),
    "?GetMapMask@PlayerRecord@": (0x5ea8e0, 73),
    "?GetCameraMask@PlayerRecord@": (0x5ea930, 68),
    "?GetGaugeMask@PlayerRecord@": (0x5f32a0, 265),
    "?GetRaceMask@PlayerRecord@": (0x5f33b0, 270),
    "?GetAmbientMask@PlayerRecord@": (0x5f34c0, 140),
    "?QueryRestart@PlayerRecord@": (0x5fff50, 306),
    "?Query5F85B0@RaceStatus890118@": (0x5f85b0, 80),
    "?Reset5FCA90@RaceStatus890118@": (0x5fca90, 106),
    "?QueryState5F0880@ReplayRecorder@": (0x5f0880, 108),
    "?Query571250@Object865480@": (0x571250, 40),
    "?GetTimer414Seconds@DrivetrainAuxiliary@": (0x401860, 37),
    "?Query401910@GameSession89E7A0@": (0x401910, 25),
    "?Query401930@GameSession89E7A0@": (0x401930, 25),
    "?Query5ED990@GameSession89E7A0@": (0x5ed990, 31),
    "?Query5EA190@GameFlow8900D8@": (0x5ea190, 27),
    "?Query457BA0@ProfileData83A9D0@": (0x457ba0, 27),
    "?Flag5012C0@Object85EF50@": (0x5012c0, 4),
    "?Flag5012D0@Object85EF50@": (0x5012d0, 4),
    "?Debug510050@": (0x510050, 6),
    "?IsSet@HudFlagHolder@": (0x501690, 9),
    "?IsCashSuppressed@HudFlagHolder@": (0x501680, 11),
    "?Query502420@CareerState861E7C@": (0x502420, 17),
    "?Query502440@CareerState861E7C@": (0x502440, 25),
    "?Query502460@CareerState861E7C@": (0x502460, 37),
    "?Query502490@CareerState861E7C@": (0x502490, 25),
    "?Query5024B0@CareerState861E7C@": (0x5024b0, 25),
}
# Callees verified by their own verifiers; both sides execute the original bytes.
RETAINED = {"?QueryState5F00F0@ReplayRecorder@": (0x5f00f0, 34),
            "?Query4018F0@GameSession89E7A0@": (0x4018f0, 25)}
GLOBALS = {
    "gameFlow8900D8": 0x8900d8, "raceStatus890118": 0x890118, "players8900B4": 0x8900b4,
    "playerCount8900BC": 0x8900bc, "value82A3A4": 0x82a3a4, "object85EF50": 0x85ef50,
    "object865480": 0x865480, "careerState861E7C": 0x861e7c, "hudHolder85B940": 0x85b940,
    "gameSession89E7A0": 0x89e7a0, "replayRecorder": 0x89cf48, "profileData83A9D0": 0x83a9d0,
    "playerSettings83AA64": 0x83aa64, "mirror83AA18": 0x83aa18, "raceMap83AA1B": 0x83aa1b,
    "exploreMap83AA1C": 0x83aa1c, "settings83AAB4": 0x83aab4, "gameMode850078": 0x850078,
    "value850088": 0x850088, "value83A7A4": 0x83a7a4, "value89E818": 0x89e818,
    "cameraWarned89E82C": 0x89e82c, "gameFlag890104": 0x890104, "gameFlag890108": 0x890108,
    "gameFlag89010C": 0x89010c, "frameTotal7FB720": 0x7fb720, "drivetrainClock": 0x86518c,
    "drivetrainClockScale": 0x784268, "worldFramePeriod": 0x7875bc, "hudZero7D7D3C": 0x7d7d3c,
    "replayFinishDelay7A5700": 0x7a5700, "restartWindow7A5498": 0x7a5498,
    "restartLimit7844F8": 0x7844f8,
}
# Writable global ranges randomised per case and compared afterwards.
STATE_RANGES = [(0x7fb720, 4), (0x82a3a4, 4), (0x83a7a4, 4), (0x83aa18, 0xa0),
                (0x850078, 4), (0x850088, 4), (0x85b940, 0x120), (0x85ef50, 0x20), (0x86518c, 4),
                (0x861e7c, 0xc), (0x8669f4, 4), (0x866a14, 4), (0x8900b4, 0xc), (0x890104, 0xc),
                (0x890118, 4), (0x8900d8, 4), (0x89cf48, 0x60), (0x89e7a0, 0x8d)]
ARENA, STACK, STOP = 0x2000000, 0x20ff000, 0x2100000
RESET = STOP + 0x100      # MoverOwnerObject::Reset5FC2A0 (thiscall, one argument)
QUERY28 = STOP + 0x200    # CameraMover vtable slot +0x28
FLOAT_WRAPPER = STOP + 0x300
FLOAT_OUT = STOP + 0x800
TRIALS = int(os.environ.get("UG2_MASK_TRIALS", "20000"))


def compile_object():
    BUILD.mkdir(parents=True, exist_ok=True)
    obj = BUILD / "HudPlayerMasks.obj"
    subprocess.run([str(ROOT / "build/tools/wibo"), "build/compilers/Win32/7.1/cl.exe", "/nologo", "/c",
                    "/Ox", "/Ob0", "/Z7", "/EHsc", "/Fo" + str(obj.relative_to(ROOT)), SOURCE],
                   cwd=ROOT, check=True)
    return obj


def main():
    raw = EXE.read_bytes()
    assert hashlib.sha256(raw).hexdigest() == "f9dd86c054878ce6276beb07c1fd61874f7a1e4bf1f241b084c65b73e24168a7"
    pe = pefile.PE(data=raw)
    image = pe.get_memory_mapped_image()
    obj = compile_object()
    externals = {}
    for symbol in read_object(obj)["symbols"].values():
        name = symbol["name"]
        if symbol["section"] != 0 or not name.startswith("?"):
            continue
        short = name.split("@")[0][1:]
        retained = next((a for prefix, (a, n) in RETAINED.items() if name.startswith(prefix)), None)
        if retained is not None:
            externals[name] = retained
        elif short in GLOBALS:
            externals[name] = GLOBALS[short]
        elif name.startswith("?Reset5FC2A0@MoverOwnerObject@"):
            externals[name] = RESET
        else:
            raise AssertionError(name)
    linked = link_objects([obj], externals)
    entries = {prefix: next(a for s, a in linked["exports"].items() if s.startswith(prefix)) for prefix in FUNCTIONS}
    retained_ranges = [(a, a + n) for a, n in RETAINED.values()]
    state = {}
    covered = set()
    machines = []
    for rebuilt in (False, True):
        uc = Uc(UC_ARCH_X86, UC_MODE_32)
        uc.mem_map(0x400000, (len(image) + 4095) & ~4095)
        uc.mem_write(0x400000, image)
        uc.mem_map(ARENA, 0x10000)
        uc.mem_map(STACK - 0x1000, 0x2000)
        uc.mem_map(STOP, 0x1000)
        uc.mem_map(linked["address"], linked["allocation"])
        for address, body in linked["blocks"]:
            uc.mem_write(address, body)
        # fstp tbyte [FLOAT_OUT]; jmp STOP
        uc.mem_write(FLOAT_WRAPPER, b"\xdb\x3d" + struct.pack("<I", FLOAT_OUT) + b"\xe9"
                     + struct.pack("<i", STOP - (FLOAT_WRAPPER + 11)))
        own = linked["ranges"] if rebuilt else [(a, a + n) for a, n in FUNCTIONS.values()]
        ranges = own + retained_ranges + [(FLOAT_WRAPPER, FLOAT_WRAPPER + 11)]

        reset = RESET if rebuilt else 0x5fc2a0
        rebuilt_flag = rebuilt

        def block(uc, address, size, user, ranges=ranges, reset=reset, rebuilt=rebuilt):
            sp = uc.reg_read(UC_X86_REG_ESP)
            ret = struct.unpack("<I", uc.mem_read(sp, 4))[0]
            if address == reset:
                arg = struct.unpack("<I", uc.mem_read(sp + 4, 4))[0]
                state["trace"].append(("Reset5FC2A0", uc.reg_read(UC_X86_REG_ECX), arg))
                uc.reg_write(UC_X86_REG_ESP, sp + 8)
                uc.reg_write(UC_X86_REG_EIP, ret)
                return
            if address == QUERY28:
                state["trace"].append(("Query28", uc.reg_read(UC_X86_REG_ECX)))
                uc.reg_write(UC_X86_REG_EAX, state["query28"])
                uc.reg_write(UC_X86_REG_ESP, sp + 4)
                uc.reg_write(UC_X86_REG_EIP, ret)
                return
            if address == STOP:
                return
            if not rebuilt:
                covered.add(address)
            assert any(a <= address < b for a, b in ranges), ("unexpected execution", hex(address))
        uc.hook_add(UC_HOOK_BLOCK, block)
        machines.append(uc)

    rng = random.Random(0x600090)
    player0, player1, car, carobj, camera, vtable, flow, race, substate = (
        ARENA + 0x1000, ARENA + 0x1400, ARENA + 0x2000, ARENA + 0x3000, ARENA + 0x3400, ARENA + 0x3500,
        ARENA + 0x3600, ARENA + 0x3700, ARENA + 0x3800)

    def pick(*values):
        return rng.choice(values)

    def scenario():
        memory = bytearray(rng.randbytes(0x10000))
        globals_ = {}
        for address, size in STATE_RANGES:
            globals_[address] = bytearray(rng.randbytes(size))
        def g(address, fmt, value):
            for base, data in globals_.items():
                if base <= address < base + len(data):
                    struct.pack_into(fmt, data, address - base, value)
                    return
            raise AssertionError(hex(address))
        def m(address, fmt, value):
            struct.pack_into(fmt, memory, address - ARENA, value)
        for player in (player0, player1):
            m(player + 4, "<I", pick(car, car, car, 0))
            m(player + 0x10, "<i", pick(0, 0, 1))
            m(player + 0x28, "<b", pick(0, 0, 0, 1, 2, 3, 4, -1))
            m(player + 0x50, "<i", pick(0, 0, 100, 1000, 1179, 1180, 1181, 1300, 2000, -5))
            m(player + 0x58, "<i", pick(0, 0, 0, 0, 1, 2, 3, 5, 6, 7, 8, -1))
            m(player + 0x234, "<i", pick(0, 0, 0, 1))
            m(player + 0x238, "<I", pick(0, camera, camera, camera))
            m(player + 0x249, "<B", pick(0, 0, 1))
            m(player + 0x24a, "<B", pick(0, 0, 0, 1))
        m(car + 0x1c, "<I", pick(carobj, carobj, 0))
        m(carobj + 0x10, "<H", pick(0, 0, 1))
        m(car + 0x454, "<i", pick(0, 0x7fffffff, 1, 4000, -4000, 0x7ffffffe))
        m(camera, "<I", vtable)
        m(vtable + 0x28, "<I", QUERY28)
        m(camera + 0xc, "<i", pick(1, 1, 1, *range(0, 0x14)))
        m(camera + 0x14, "<i", pick(0, 1, 1, 1))
        m(flow, "<i", pick(0, 1, 3, 4, 4, 5, 6, 6, 7))
        m(flow + 0x24, "<i", pick(0, 1, 2, 3, 4))
        m(race, "<i", pick(0, 0, 1, 2, 3, 3, 4))
        m(race + 0x24, "<i", pick(0, 1))
        m(race + 0x28, "<i", pick(0, 1))
        m(substate, "<i", pick(0, 1, 2, 3, 4, 5, 6, 7))
        g(0x8900d8, "<I", flow)
        g(0x890118, "<I", race)
        g(0x8900b4, "<I", player0)
        g(0x8900b8, "<I", player1)
        g(0x8900bc, "<i", pick(0, 1, 1, 2, 2))
        g(0x7fb720, "<i", pick(0, 1000, 1180, 1200, 1239, 1240, 1241, 2000, 5000))
        g(0x82a3a4, "<i", pick(0, 0, 0, 1, -1))
        g(0x83a7a4, "<B", pick(0, 1))
        for row in (0x83aa64, 0x83aa8c):
            for k in range(6):
                g(row + k, "<B", pick(0, 1, 1))
            g(row + 0x10, "<i", pick(0, 1, 2, 3))
            g(row + 0x14, "<B", pick(0, 4, 4, 1))
        g(0x83aa18, "<B", pick(0, 1))
        g(0x83aa1b, "<B", pick(0, 1))
        g(0x83aa1c, "<B", pick(0, 2))
        g(0x83aab4, "<i", pick(0, 8, 8, 3))
        g(0x850078, "<i", pick(0, 1, 1, 4, 8, 0x200))
        g(0x850088, "<i", pick(0, 8))
        g(0x85b948, "<i", pick(0, 1))
        g(0x85ba5c, "<i", pick(0, 1, 2))
        g(0x85ef6c, "<B", pick(0, 0, 0, 1))
        g(0x85ef6d, "<B", pick(0, 0, 0, 1))
        g(0x86518c, "<i", pick(0, 1000, 12000, 13000, 20000))
        g(0x861e7c, "<i", pick(0, 1, 2, 3, 5, 6, 8, 9, 10))
        g(0x861e84, "<B", pick(0, 1))
        g(0x8669f4, "<i", pick(0, 0, 1, -1))
        g(0x866a14, "<i", pick(0, 0, 1, -1))
        g(0x890104, "<i", pick(0, 0, 1))
        g(0x890108, "<i", pick(0, 0, 1))
        g(0x89010c, "<i", pick(0, 0, 1))
        g(0x89cf48, "<i", pick(0, 1, 6, 6))
        g(0x89cf4c, "<I", substate)
        g(0x89cf69, "<B", pick(0, 0, 1))
        g(0x89cfa0, "<i", pick(0, 0x7fffffff, 1000, 1, 13000))
        g(0x89e7b0, "<i", pick(0, 0, 1, 2, 7, 3))
        g(0x89e7b4, "<i", pick(0, 1))
        g(0x89e7bc, "<i", pick(0, 1))
        for k in range(4):
            g(0x89e7c0 + 4 * k, "<i", pick(0, 0, 1))
        g(0x89e7d8, "<B", pick(0, 0, 1))
        g(0x89e7d9, "<B", pick(0, 0, 1))
        g(0x89e7e3, "<B", pick(0, 0, 1))
        g(0x89e7e6, "<B", pick(0, 0, 0, 1))
        g(0x89e818, "<i", pick(0, 0x7fffffff, 5))
        g(0x89e82c, "<B", pick(0, 1))
        if struct.unpack_from("<I", memory, car + 0x1c - ARENA)[0] == 0:
            g(0x89e7e6, "<B", 0)  # 0x5f34c0 dereferences the car's +0x1c object.
        return memory, globals_

    counts = dict.fromkeys(FUNCTIONS, 0)
    def compare(prefix, this, memory, globals_, query28):
        outputs = []
        for rebuilt, uc in zip((False, True), machines):
            uc.mem_write(ARENA, bytes(memory))
            for address, data in globals_.items():
                uc.mem_write(address, bytes(data))
            floating = prefix.startswith("?GetTimer414Seconds")
            uc.mem_write(STACK, struct.pack("<I", FLOAT_WRAPPER if floating else STOP))
            uc.mem_write(FLOAT_OUT, bytes(10))
            uc.reg_write(UC_X86_REG_ESP, STACK)
            uc.reg_write(UC_X86_REG_ECX, this)
            uc.reg_write(UC_X86_REG_FPCW, 0x027f)
            uc.reg_write(UC_X86_REG_FPSW, 0)
            uc.reg_write(UC_X86_REG_FPTAG, 0xffff)
            saved = (UC_X86_REG_EBX, UC_X86_REG_ESI, UC_X86_REG_EDI, UC_X86_REG_EBP)
            for reg in saved:
                uc.reg_write(reg, 0x12345678)
            uc.reg_write(UC_X86_REG_EAX, 0xa5a5a5a5)
            uc.reg_write(UC_X86_REG_EDX, 0x5a5a5a5a)
            state.update(trace=[], query28=query28)
            try:
                uc.emu_start(entries[prefix] if rebuilt else FUNCTIONS[prefix][0], STOP, count=20000)
            except Exception:
                print("fault", prefix, rebuilt, hex(this), hex(uc.reg_read(UC_X86_REG_EIP)))
                raise
            assert uc.reg_read(UC_X86_REG_EIP) == STOP, (prefix, rebuilt, hex(uc.reg_read(UC_X86_REG_EIP)))
            assert uc.reg_read(UC_X86_REG_ESP) == STACK + 4
            assert all(uc.reg_read(reg) == 0x12345678 for reg in saved)
            eax, edx = uc.reg_read(UC_X86_REG_EAX), uc.reg_read(UC_X86_REG_EDX)
            if prefix.endswith("PlayerRecord@") and "Mask" in prefix and "Camera" not in prefix:
                result = (eax, edx)
            elif floating:
                result = bytes(uc.mem_read(FLOAT_OUT, 10))
            elif prefix.startswith(("?GetCameraMask", "?Query5F85B0", "?QueryState5F0880", "?Query5EA190")):
                result = eax
            elif prefix.startswith("?Reset5FCA90"):
                result = None
            else:
                result = eax & 0xff
            outputs.append((result, state["trace"], bytes(uc.mem_read(ARENA, 0x4000)),
                            [bytes(uc.mem_read(a, len(d))) for a, d in globals_.items()]))
        assert outputs[0] == outputs[1], (prefix, hex(this), outputs[0][:2], outputs[1][:2])
        counts[prefix] += 1
        return outputs[0][0]

    receivers = {"PlayerRecord": (player0, player1), "RaceStatus890118": (race,), "ReplayRecorder": (0x89cf48,),
                 "Object865480": (0x865480,), "DrivetrainAuxiliary": (car + 0x40,),
                 "GameSession89E7A0": (0x89e7a0,), "GameFlow8900D8": (flow,), "ProfileData83A9D0": (0x83a9d0,),
                 "Object85EF50": (0x85ef50,), "HudFlagHolder": (0x85b940,), "CareerState861E7C": (0x861e7c,)}
    masks = {}
    for trial in range(TRIALS):
        memory, globals_ = scenario()
        query28 = rng.choice((0, 1, 0x100))
        for prefix in FUNCTIONS:
            cls = prefix.split("@")[1] if prefix.count("@") > 1 else None
            for this in receivers.get(cls, (0,)):
                if prefix.startswith("?GetAmbientMask") and struct.unpack_from("<I", memory, car + 0x1c - ARENA)[0] == 0:
                    continue  # 0x5f34c0 dereferences the car's +0x1c object.
                if cls == "PlayerRecord" and prefix not in ("?GetHudMask@PlayerRecord@", "?GetScoreMask@PlayerRecord@",
                                                             "?GetMapMask@PlayerRecord@", "?GetRaceMask@PlayerRecord@",
                                                             "?GetCameraMask@PlayerRecord@", "?QueryRestart@PlayerRecord@") \
                        and struct.unpack_from("<I", memory, this - ARENA + 4)[0] == 0:
                    continue  # Car-reading helpers are reached only with a car.
                result = compare(prefix, this, memory, globals_, query28)
                if prefix.startswith("?GetHudMask"):
                    masks[result] = masks.get(result, 0) + 1
    from capstone import Cs, CS_ARCH_X86, CS_MODE_32
    starts = set()
    md = Cs(CS_ARCH_X86, CS_MODE_32)
    for a, n in FUNCTIONS.values():
        starts.add(a)
        for ins in md.disasm(image[a - 0x400000:a - 0x400000 + n], a):
            if ins.mnemonic.startswith("j") or ins.mnemonic == "call":
                starts.add(ins.address + ins.size)
                if ins.op_str.startswith("0x"):
                    target = int(ins.op_str, 16)
                    if a <= target < a + n:
                        starts.add(target)
    # The 0x600090 jump-table targets.
    starts.update((0x6000ca, 0x600564, 0x6000d8))
    missed = sorted(hex(x) for x in starts - covered
                    if any(a <= x < a + n for a, n in FUNCTIONS.values()) and image[x - 0x400000] != 0x90)
    report = dict(
        executable_sha256=hashlib.sha256(raw).hexdigest(), comparisons=counts,
        distinct_hud_masks=len(masks),
        original_block_starts_missed=missed,
        functions={p.strip("?@"): dict(address=hex(a), size=n,
                                       sha256=hashlib.sha256(image[a - 0x400000:a - 0x400000 + n]).hexdigest())
                   for p, (a, n) in FUNCTIONS.items()},
        retained_original_callees={p.strip("?@"): hex(a) for p, (a, n) in RETAINED.items()},
        traced_interfaces=["MoverOwnerObject::Reset5FC2A0 0x5fc2a0 (receiver, car)",
                           "CameraMover vtable +0x28 (receiver, scripted result)"],
        byte_matching="Not established", compiler="MSVC 7.1 /Ox /Ob0 /Z7 /EHsc",
        checks=["EAX:EDX mask words, bool/int results and x87 80-bit return", "callee trace order and arguments",
                "arena (player, car, camera, flow, race objects) and every read global range after execution",
                "stack cleanup and callee-saved registers"],
        limitations=["Finite randomised scenarios over chosen field values",
                     "0x5fc2a0 and camera vtable methods are traced interfaces, not executed",
                     "The caller 0x600590 and controller update 0x4ec5a0 are not recovered here"])
    (BUILD / "hud-player-masks-verification.json").write_text(json.dumps(report, indent=2) + "\n")
    print(json.dumps(report, indent=2))


if __name__ == "__main__":
    main()
