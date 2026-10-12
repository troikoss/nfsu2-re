#!/usr/bin/env python3
"""Execute original/rebuilt game-event record/apply (0x578ef0, 0x571830, 0x4a7b30) and 0x407280."""
import hashlib
import json
import random
import re
import struct
import subprocess

import pefile
from unicorn import Uc, UC_ARCH_X86, UC_MODE_32, UC_HOOK_BLOCK
from unicorn.x86_const import (UC_X86_REG_EAX, UC_X86_REG_ECX, UC_X86_REG_ESP, UC_X86_REG_EIP,
                              UC_X86_REG_EBX, UC_X86_REG_ESI, UC_X86_REG_EDI, UC_X86_REG_EBP,
                              UC_X86_REG_FPCW, UC_X86_REG_FPSW, UC_X86_REG_FPTAG)
from probe_pc_race_reward import ROOT, EXE
from pc_coff import link_objects, read_object

BUILD = ROOT / "build/analysis/replay-events"
SOURCES = ["src/Speed/Indep/Src/Frontend/ReplayEvents.cpp", "src/Speed/Indep/Src/Physics/HumanDriverInput.cpp"]
FUNCTIONS = {
    "?Value578EF0@Replay8650B8@": (0x578ef0, 214),
    "?Overflow571830@Replay8650B8@": (0x571830, 21),
    "?ApplyReplayEvent@": (0x4a7b30, 131),
    "?ApplyTransmission@HumanDriverInput@": (0x407280, 140),
    "?ControlEvent@DrivetrainContext@": (0x578e90, 80),
}
ARENA, STACK, STOP = 0x2000000, 0x20ff000, 0x2100000
SLOT34 = STOP + 0x100
UNUSED = STOP + 0xf00


def main():
    raw = EXE.read_bytes()
    assert hashlib.sha256(raw).hexdigest() == "f9dd86c054878ce6276beb07c1fd61874f7a1e4bf1f241b084c65b73e24168a7"
    image = pefile.PE(data=raw).get_memory_mapped_image()
    BUILD.mkdir(parents=True, exist_ok=True)
    objects = []
    for source in SOURCES:
        obj = BUILD / (source.rsplit("/", 1)[1][:-4] + ".obj")
        subprocess.run([str(ROOT / "build/tools/wibo"), "build/compilers/Win32/7.1/cl.exe", "/nologo", "/c", "/Ox",
                        "/Ob0", "/Z7", "/EHsc", "/Fo" + str(obj.relative_to(ROOT)), source], cwd=ROOT, check=True)
        objects.append(obj)
    ld = {n: int(a, 16) for n, a in re.findall(r"^(\w+) = (0x[0-9a-f]+);",
          (ROOT / "src/Platform/Native/OriginalSymbols.ld").read_text(), re.M)}
    symbols = [s for o in objects for s in read_object(o)["symbols"].values()]
    defined = {s["name"] for s in symbols if s["section"] > 0}
    externals = {"__ftol2": 0x75bed4, "__fltused": UNUSED, "_nfs_strcmp": 0x43dc30}
    for s in symbols:
        name = s["name"]
        if s["section"] != 0 or name in defined or name in externals:
            continue
        short = name[1:].split("@")[0]
        if "@@3" in name and short in ld:
            externals[name] = ld[short]
        else:
            externals[name] = UNUSED  # HumanDriverInput.cpp callees outside 0x407280: never executed.
    linked = link_objects(objects, externals)
    entries = {p: next(a for n, a in linked["exports"].items() if n.startswith(p)) for p in FUNCTIONS}
    state = {}
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
        own = linked["ranges"] if rebuilt else [(a, a + n) for a, n in FUNCTIONS.values()]

        def block(uc, address, size, user, own=own):
            if address == STOP:
                return
            if address == SLOT34:
                sp = uc.reg_read(UC_X86_REG_ESP)
                ret, arg = struct.unpack("<II", uc.mem_read(sp, 8))
                state["trace"].append(("SetTransmission", uc.reg_read(UC_X86_REG_ECX), arg))
                uc.reg_write(UC_X86_REG_EAX, 0x55)
                uc.reg_write(UC_X86_REG_ESP, sp + 8)
                uc.reg_write(UC_X86_REG_EIP, ret)
                return
            assert any(a <= address < b for a, b in own + [(0x75bed4, 0x75bf49), (0x43dc30, 0x43dc5c)]), ("unexpected", hex(address))
        uc.hook_add(UC_HOOK_BLOCK, block)
        machines.append(uc)

    counts = dict.fromkeys(FUNCTIONS, 0)
    rng = random.Random(0x578ef0)

    def compare(prefix, memory, globals_, args, this=0, cleanup=0, mask=0xffffffff):
        outputs = []
        for rebuilt, uc in zip((False, True), machines):
            uc.mem_write(ARENA, bytes(memory))
            for address, data in globals_:
                uc.mem_write(address, bytes(data))
            uc.mem_write(STACK, struct.pack("<I", STOP) + args)
            uc.reg_write(UC_X86_REG_ESP, STACK)
            uc.reg_write(UC_X86_REG_ECX, this)
            uc.reg_write(UC_X86_REG_FPCW, 0x027f)
            uc.reg_write(UC_X86_REG_FPSW, 0)
            uc.reg_write(UC_X86_REG_FPTAG, 0xffff)
            for reg in (UC_X86_REG_EBX, UC_X86_REG_ESI, UC_X86_REG_EDI, UC_X86_REG_EBP):
                uc.reg_write(reg, 0x12345678)
            state["trace"] = []
            try:
                uc.emu_start(entries[prefix] if rebuilt else FUNCTIONS[prefix][0], STOP, count=20000)
            except Exception:
                print("fault", prefix, rebuilt, hex(uc.reg_read(UC_X86_REG_EIP)), args.hex())
                raise
            assert uc.reg_read(UC_X86_REG_EIP) == STOP
            assert uc.reg_read(UC_X86_REG_ESP) == STACK + 4 + cleanup, (prefix, rebuilt)
            assert all(uc.reg_read(r) == 0x12345678 for r in (UC_X86_REG_EBX, UC_X86_REG_ESI, UC_X86_REG_EDI, UC_X86_REG_EBP))
            outputs.append((uc.reg_read(UC_X86_REG_EAX) & mask, state["trace"], bytes(uc.mem_read(ARENA, 0x10000)),
                            [bytes(uc.mem_read(a, len(d))) for a, d in globals_]))
        assert outputs[0] == outputs[1], (prefix, outputs[0][:2], outputs[1][:2])
        counts[prefix] += 1

    player, car, inputs, vtable, mover, drivetrain = (ARENA + 0x100, ARENA + 0x1000, ARENA + 0x2000, ARENA + 0x2400,
                                                      ARENA + 0x2800, ARENA + 0x2c00)
    race = ARENA + 0x3000
    replay = ARENA + 0x8000  # 0x50f0 bytes reach ARENA + 0xd0f0

    def scenario():
        memory = bytearray(rng.randbytes(0x10000))
        struct.pack_into("<I", memory, 0x104, car)
        struct.pack_into("<I", memory, 0x1030, inputs)
        struct.pack_into("<I", memory, 0x2000, vtable)
        struct.pack_into("<I", memory, 0x2400 + 0x34, SLOT34)
        struct.pack_into("<I", memory, 0x1028, rng.choice((1, 1, 0, 3)))
        struct.pack_into("<I", memory, 0x1034, mover)
        struct.pack_into("<I", memory, 0x284c, drivetrain)
        struct.pack_into("<i", memory, 0x1434, rng.randrange(-1, 7))
        struct.pack_into("<I", memory, 0x22a8, car)
        struct.pack_into("<i", memory, 0x8078, rng.choice((-1, 0, 5)))
        struct.pack_into("<i", memory, 0x8080, rng.choice((0, 0, 1)))
        struct.pack_into("<i", memory, 0x808c, rng.choice((0, 1, 1)))
        count = rng.choice((0, 1, 5, 0xff, 0x100))
        struct.pack_into("<i", memory, 0xc0e4, count)
        frame = rng.choice((10, 11))
        if count and count <= 0x100:
            at = 0xc0ec + (count - 1) * 16
            struct.pack_into("<iiii", memory, at, rng.choice((1, 2)), rng.choice((10, 11)), rng.choice((0, 1)), 77)
        globals_ = [(0x8900ac, struct.pack("<II", player, player)), (0x890118, struct.pack("<I", race)),
                    (0x7fb724, struct.pack("<i", frame))]
        return memory, globals_

    for trial in range(4000):
        memory, globals_ = scenario()
        args = struct.pack("<iiiI", rng.choice((0, 1, 2, 3, 4, -1)), rng.choice((0, 1)), rng.choice((0, 1, 2, 3, -7, 300)),
                           rng.choice((0, 1, 0x100)))
        compare("?Value578EF0@Replay8650B8@", memory, globals_, args, replay, cleanup=16)
        compare("?Overflow571830@Replay8650B8@", memory, globals_, b"", replay, mask=0)
        event = ARENA + 0xe000
        struct.pack_into("<iiii", memory, 0xe000, rng.choice((0, 1, 2, 3, 4)), 9, rng.choice((0, 1)),
                         rng.choice((0, 1, 2, 3, -1, 1000)))
        compare("?ApplyReplayEvent@", memory, globals_, struct.pack("<I", event))
        compare("?ApplyTransmission@HumanDriverInput@", memory, globals_, struct.pack("<i", rng.choice((0, 1, 2, 3, 4, -1))),
                inputs, cleanup=4, mask=0xff)
    names = [b"INSTANT_REPLAY", b"INSTANT_REPLAY2", b"INSTANT", b"", b"OTHER_CAMERA"]
    for trial in range(2000):
        memory, globals_ = scenario()
        context = ARENA + 0x8000
        struct.pack_into("<i", memory, 0x8004, rng.choice((0, 1, 7)))
        nodes = [ARENA + 0x4000 + i * 0x200 for i in range(rng.randrange(0, 4))]
        chain = [context + 0x98] + nodes
        for i, node in enumerate(chain):
            struct.pack_into("<II", memory, node - ARENA, chain[(i + 1) % len(chain)], chain[i - 1])
        for node in nodes:
            name = rng.choice(names)
            memory[node - ARENA + 8:node - ARENA + 8 + len(name) + 1] = name + b"\0"
            struct.pack_into("<f", memory, node - ARENA + 0x1d8, rng.choice((0.0, 1.5, -3.25, 1e30)))
        compare("?ControlEvent@DrivetrainContext@", memory, globals_,
                struct.pack("<fI", rng.choice((0.0, 0.5, 2.0, -1.0)), 0x784558), context, cleanup=8, mask=0)
    report = dict(executable_sha256=hashlib.sha256(raw).hexdigest(), comparisons=counts,
                  functions={p: dict(address=hex(a), size=n,
                                     sha256=hashlib.sha256(image[a - 0x400000:a - 0x400000 + n]).hexdigest())
                             for p, (a, n) in FUNCTIONS.items()},
                  traced_interfaces={"MoverInput vtable +0x34": "receiver and mode"}, byte_matching="Not established")
    (BUILD / "verification.json").write_text(json.dumps(report, indent=2) + "\n")
    print(json.dumps(counts, indent=2))
    print("PASS", sum(counts.values()), "comparisons")


if __name__ == "__main__":
    main()
