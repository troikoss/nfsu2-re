#!/usr/bin/env python3
"""Execute original/rebuilt track position markers (0x5d8470..0x5d8660) and start grid 0x5eb5d0."""
import hashlib
import json
import random
import re
import struct
import subprocess

import pefile
from unicorn import Uc, UC_ARCH_X86, UC_MODE_32, UC_HOOK_BLOCK
from unicorn.x86_const import (UC_X86_REG_GDTR, UC_X86_REG_CS, UC_X86_REG_SS, UC_X86_REG_DS, UC_X86_REG_ES,
                              UC_X86_REG_FS, UC_X86_REG_EAX, UC_X86_REG_ECX, UC_X86_REG_ESP, UC_X86_REG_EIP,
                              UC_X86_REG_EBX, UC_X86_REG_ESI, UC_X86_REG_EDI, UC_X86_REG_EBP,
                              UC_X86_REG_FPCW, UC_X86_REG_FPSW, UC_X86_REG_FPTAG)
from probe_pc_race_reward import ROOT, EXE
from pc_coff import link_objects, read_object

BUILD = ROOT / "build/analysis/track-markers"
FUNCTIONS = {
    "?LoadTrackPositionMarkers@": (0x5d8470, 87),
    "?UnloadTrackPositionMarkers@": (0x5d84d0, 77),
    "?CountTrackPositionMarkers@@YAHHI@Z": (0x5d8520, 58),
    "?GetTrackPositionMarker@@YAPAUTrackPositionMarker@@HIH@Z": (0x5d85d0, 74),
    "?CountTrackPositionMarkers@@YAHI@Z": (0x5d8560, 48),
    "?CountTrackPositionMarkers@@YAHPBD@Z": (0x5d8590, 54),
    "?GetTrackPositionMarker@@YAPAUTrackPositionMarker@@IH@Z": (0x5d8620, 58),
    "?GetTrackPositionMarker@@YAPAUTrackPositionMarker@@PBDH@Z": (0x5d8660, 64),
    "?Construct@WRoadNav@": (0x5da2f0, 53),
    "?GetStartingPositions@": (0x5eb5d0, 602),
    "?Construct@WCollider@": (0x5e0b40, 167),
    "?Destruct@WCollider@": (0x5e0bf0, 72),
    "?Release@WCollider@": (0x5dc9c0, 105),
    "?ReleaseArea@WCollider@": (0x5d80a0, 72),
}
# Executed as original bytes on both sides (verified elsewhere or trivial).
RETAINED = {"?HudNameHash@": (0x43db50, 0x48), "?HudAngleUnits@": (0x43e6d0, 0x60), "?Destroy@WRoadNav@": (0x4022c0, 1)}
RETAINED_RANGES = [(0x43db50, 0x43dbd0), (0x43e6d0, 0x43e816), (0x43c490, 0x43c568), (0x4022c0, 0x4022c1), (0x75bed4, 0x75bf49), (0x43d220, 0x43d243),
                   (0x502fd0, 0x502fd3)]
GLOBALS = {"trackPositionMarkers88F740": 0x88f740, "currentTrack883DAC": 0x883dac, "gameSession89E7A0": 0x89e7a0}
ARENA, STACK, STOP = 0x2000000, 0x20ff000, 0x2100000
INTERFACES = {  # name: (original address, stack arguments)
    "Construct@WCollider": (0x5e0b40, 0), "Destruct@WCollider": (0x5e0bf0, 0),
    "UpdatePack@WCollider": (0x5e68d0, 2), "QueryGround@WCollider": (0x5e7e70, 4),
    "Advance5DA370@WRoadNav": (0x5da370, 3),
}
STUB = STOP + 0x400
TIB, GDT = 0x2120000, 0x2130000
GROUND = STOP + 0x800


def main():
    raw = EXE.read_bytes()
    assert hashlib.sha256(raw).hexdigest() == "f9dd86c054878ce6276beb07c1fd61874f7a1e4bf1f241b084c65b73e24168a7"
    image = pefile.PE(data=raw).get_memory_mapped_image()
    BUILD.mkdir(parents=True, exist_ok=True)
    obj = BUILD / "TrackPositionMarkers.obj"
    subprocess.run([str(ROOT / "build/tools/wibo"), "build/compilers/Win32/7.1/cl.exe", "/nologo", "/c", "/Ox",
                    "/Ob0", "/Z7", "/EHsc", "/Fo" + str(obj.relative_to(ROOT)),
                    "src/Speed/Indep/Src/World/TrackPositionMarkers.cpp"], cwd=ROOT, check=True)
    collision = BUILD / "WCollision.obj"
    subprocess.run([str(ROOT / "build/tools/wibo"), "build/compilers/Win32/7.1/cl.exe", "/nologo", "/c", "/Ox",
                    "/Ob0", "/Z7", "/EHsc", "/Fo" + str(collision.relative_to(ROOT)),
                    "src/Speed/Indep/Src/World/WCollision.cpp"], cwd=ROOT, check=True)
    ld = {n: int(a, 16) for n, a in re.findall(r"^(\w+) = (0x[0-9a-f]+);",
          (ROOT / "src/Platform/Native/OriginalSymbols.ld").read_text(), re.M)}
    symbols = list(read_object(obj)["symbols"].values()) + list(read_object(collision)["symbols"].values())
    defined = {s["name"] for s in symbols if s["section"] > 0}
    externals = {}
    stubs = {}
    for s in symbols:
        name = s["name"]
        if s["section"] != 0 or name in defined or not name.startswith("?"):
            continue
        short = name.split("@@")[0][1:]
        retained = next((a for p, (a, n) in RETAINED.items() if name.startswith(p)), None)
        if retained is not None:
            externals[name] = retained
        elif short in GLOBALS:
            externals[name] = GLOBALS[short]
        elif short in INTERFACES:
            externals[name] = STUB + 0x10 * list(INTERFACES).index(short)
        elif re.fullmatch(r"[A-Za-z]+[0-9A-F]{6}", short):
            externals[name] = int(short[-6:], 16)
        elif short in ld:
            externals[name] = ld[short]
        elif name.startswith("?") and "@@YA" in name or name.startswith("??2"):
            externals[name] = STOP + 0xf00  # Unrelated WCollision.cpp callees: never executed here.
        else:
            raise AssertionError(name)
    for name in ("__ftol2", "__fltused"):
        externals[name] = 0x75bed4 if name == "__ftol2" else STOP + 0xf00
    linked = link_objects([obj, collision], externals)
    entries = {p: next(a for n, a in linked["exports"].items() if n.startswith(p)) for p in FUNCTIONS}
    state = {}
    machines = []
    for rebuilt in (False, True):
        uc = Uc(UC_ARCH_X86, UC_MODE_32)
        uc.mem_map(0x400000, (len(image) + 4095) & ~4095)
        uc.mem_write(0x400000, image)
        uc.mem_map(ARENA, 0x20000)
        uc.mem_map(STACK - 0x2000, 0x3000)
        uc.mem_map(STOP, 0x1000)
        uc.mem_map(linked["address"], linked["allocation"])
        for address, body in linked["blocks"]:
            uc.mem_write(address, body)
        # Flat descriptors plus FS based at TIB for the original's fs:[0] SEH frame.
        uc.mem_map(TIB, 0x1000)
        uc.mem_map(GDT, 0x1000)

        def descriptor(base, limit, access, flags=0xc):
            return ((limit & 0xffff) | (base & 0xffffff) << 16 | access << 40 | (limit >> 16 & 0xf) << 48
                    | flags << 52 | (base >> 24) << 56)
        uc.mem_write(GDT + 8, struct.pack("<3Q", descriptor(0, 0xfffff, 0x9b), descriptor(0, 0xfffff, 0x93),
                                          descriptor(TIB, 0xfff, 0x93, 0x4)))
        uc.reg_write(UC_X86_REG_GDTR, (0, GDT, 0x1f, 0))
        for register, selector in ((UC_X86_REG_CS, 8), (UC_X86_REG_SS, 16), (UC_X86_REG_DS, 16),
                                   (UC_X86_REG_ES, 16), (UC_X86_REG_FS, 24)):
            uc.reg_write(register, selector)
        # fld dword [GROUND]; ret 0x10
        uc.mem_write(STOP + 0x300, b"\xd9\x05" + struct.pack("<I", GROUND) + b"\xc2\x10\x00")
        hooks = {(STUB + 0x10 * i if rebuilt else a): (n, count) for i, (n, (a, count)) in enumerate(INTERFACES.items())}
        if rebuilt:  # The rebuilt collider methods are linked; trace them only inside GetStartingPositions.
            for n in ("Construct@WCollider", "Destruct@WCollider", "UpdatePack@WCollider", "QueryGround@WCollider"):
                address = next(a for sym, a in linked["exports"].items() if sym.startswith("?" + n.split("@")[0] + "@WCollider@"))
                hooks[address] = (n, INTERFACES[n][1])
        own = linked["ranges"] if rebuilt else [(a, a + n) for a, n in FUNCTIONS.values()]
        ranges = own + RETAINED_RANGES + [(STOP + 0x300, STOP + 0x309)]

        def block(uc, address, size, user, hooks=hooks, ranges=ranges):
            if address == STOP:
                return
            if address in hooks and state["trace_colliders"]:
                name, count = hooks[address]
                sp = uc.reg_read(UC_X86_REG_ESP)
                ret = struct.unpack("<I", uc.mem_read(sp, 4))[0]
                args = list(struct.unpack("<%dI" % count, uc.mem_read(sp + 4, 4 * count)))
                this = uc.reg_read(UC_X86_REG_ECX)
                if name in ("UpdatePack@WCollider", "QueryGround@WCollider"):
                    args[0] = bytes(uc.mem_read(args[0], 12)).hex()  # Stack point: compare its values.
                state["trace"].append((name, args))
                if name == "UpdatePack@WCollider":
                    uc.mem_write(this + 8, struct.pack("<I", state["packs"].pop(0)))
                elif name == "Construct@WCollider":
                    uc.reg_write(UC_X86_REG_EAX, this)
                elif name == "Advance5DA370@WRoadNav":
                    k = len(state["trace"])
                    uc.mem_write(this + 8, struct.pack("<ff", 100.0 + k * 1.5, -50.0 - k))
                    uc.mem_write(this + 0x28, struct.pack("<ff", *state["direction"]))
                if name == "QueryGround@WCollider":
                    uc.mem_write(GROUND, struct.pack("<f", state["heights"].pop(0)))
                    uc.reg_write(UC_X86_REG_ESP, sp)
                    uc.reg_write(UC_X86_REG_EIP, STOP + 0x300)
                    return
                uc.reg_write(UC_X86_REG_ESP, sp + 4 + 4 * count)
                uc.reg_write(UC_X86_REG_EIP, ret)
                return
            assert any(a <= address < b for a, b in ranges), ("unexpected execution", hex(address))
        uc.hook_add(UC_HOOK_BLOCK, block)
        machines.append(uc)

    rng = random.Random(0x34146)
    counts = dict.fromkeys(FUNCTIONS, 0)

    def bhash(text, seed=0xffffffff):
        for c in text.encode():
            seed = (seed * 33 + c) & 0xffffffff
        return seed

    def compare(prefix, memory, globals_, args, this=0, packs=(), heights=(), direction=(1.0, 0.0), result="eax"):
        outputs = []
        for rebuilt, uc in zip((False, True), machines):
            uc.mem_write(ARENA, bytes(memory))
            for address, data in globals_:
                uc.mem_write(address, bytes(data))
            uc.mem_write(STACK, struct.pack("<I", STOP) + args)
            uc.mem_write(TIB, struct.pack("<I", 0xffffffff))
            uc.reg_write(UC_X86_REG_ESP, STACK)
            uc.reg_write(UC_X86_REG_ECX, this)
            uc.reg_write(UC_X86_REG_FPCW, 0x027f)
            uc.reg_write(UC_X86_REG_FPSW, 0)
            uc.reg_write(UC_X86_REG_FPTAG, 0xffff)
            for reg in (UC_X86_REG_EBX, UC_X86_REG_ESI, UC_X86_REG_EDI, UC_X86_REG_EBP):
                uc.reg_write(reg, 0x12345678)
            state.update(trace_colliders=not prefix.startswith(("?Construct@WCollider", "?Destruct@WCollider")),
                         trace=[], packs=list(packs), heights=list(heights), direction=direction)
            uc.emu_start(entries[prefix] if rebuilt else FUNCTIONS[prefix][0], STOP, count=200000)
            assert uc.reg_read(UC_X86_REG_EIP) == STOP
            assert bytes(uc.mem_read(TIB, 4)) == b"\xff\xff\xff\xff"
            cleanup = len(args) if prefix.startswith("?Construct@WRoadNav") and False else 0
            assert uc.reg_read(UC_X86_REG_ESP) == STACK + 4 + cleanup, (prefix, rebuilt)
            assert all(uc.reg_read(r) == 0x12345678 for r in (UC_X86_REG_EBX, UC_X86_REG_ESI, UC_X86_REG_EDI, UC_X86_REG_EBP))
            value = uc.reg_read(UC_X86_REG_EAX) if result == "eax" else None
            outputs.append((value, state["trace"], bytes(uc.mem_read(ARENA, 0x20000)),
                            [bytes(uc.mem_read(a, len(d))) for a, d in globals_], state["packs"], state["heights"]))
        assert outputs[0] == outputs[1], (prefix, outputs[0][:2], outputs[1][:2])
        counts[prefix] += 1
        return outputs[0]

    names = ["ForwardStartingPosition", "BackwardStartingPosition", "ForwardStartLine", "SplitTimeBoundary1"]
    hashes = [bhash(n) for n in names] + [0x12345678]
    track_info = ARENA + 0x18000

    def build(count, tracks=(0, 4000, 4001), slots=None):
        memory = bytearray(rng.randbytes(0x20000))
        markers = []
        head = 0x88f740
        previous = head
        nodes = []
        for i in range(count):
            address = ARENA + 0x1000 + i * 0x30
            nodes.append(address)
        for i, address in enumerate(nodes):
            nxt = nodes[i + 1] if i + 1 < len(nodes) else head
            prv = nodes[i - 1] if i else head
            off = address - ARENA
            hsh = rng.choice(hashes)
            slot = slots[i] if slots else rng.choice((0, 1, 2, 3, 4, 5, 6, 7, -1))
            struct.pack_into("<IIIi3ffHHi8s", memory, off, nxt, prv, hsh, slot,
                             rng.uniform(-2000, 2000), rng.uniform(-2000, 2000), rng.uniform(0, 80), 0.0,
                             rng.randrange(65536), 0, rng.choice(tracks), b"x")
        list_head = struct.pack("<II", nodes[0] if nodes else head, nodes[-1] if nodes else head)
        struct.pack_into("<h", memory, 0x18000 + 0x8a, rng.choice((4000, 4001, 0)))
        current = rng.choice((0, track_info))
        globals_ = [(0x88f740, list_head), (0x883dac, struct.pack("<I", current)),
                    (0x89e7a4, struct.pack("<i", rng.choice((0, 0, 1))))]
        return memory, globals_

    # Loader and unloader on real-shaped chunks at varying alignments.
    for trial in range(400):
        memory, globals_ = build(rng.randrange(0, 4))
        misalign = rng.randrange(0, 16)
        chunk = ARENA + 0x4000 + misalign
        entries_count = rng.randrange(0, 9)
        start = (chunk + 0x17) & ~0xf
        size = start + entries_count * 0x30 - chunk - 8 + rng.choice((0, 0, 4, 0x2f))
        kind = rng.choice((0x34146, 0x34146, 0x34146, 0x34147))
        struct.pack_into("<II", memory, chunk - ARENA, kind, size & 0xffffffff)
        out = compare("?LoadTrackPositionMarkers@", memory, globals_, struct.pack("<I", chunk))
        loaded = bytearray(out[2])
        loaded_globals = [(a, d) for (a, _), d in zip(globals_, out[3])]
        compare("?UnloadTrackPositionMarkers@", loaded, loaded_globals, struct.pack("<I", chunk))
    for trial in range(1500):
        memory, globals_ = build(rng.randrange(0, 20))
        for hsh in rng.sample(hashes, 3):
            track = rng.choice((0, 4000, 4001))
            compare("?CountTrackPositionMarkers@@YAHHI@Z", memory, globals_, struct.pack("<iI", track, hsh))
            compare("?CountTrackPositionMarkers@@YAHI@Z", memory, globals_, struct.pack("<I", hsh))
            for index in (0, 1, 3, 7, 30):
                compare("?GetTrackPositionMarker@@YAPAUTrackPositionMarker@@HIH@Z", memory, globals_,
                        struct.pack("<iIi", track, hsh, index))
                compare("?GetTrackPositionMarker@@YAPAUTrackPositionMarker@@IH@Z", memory, globals_,
                        struct.pack("<Ii", hsh, index))
        name = rng.choice(names)
        memory[0x8000:0x8000 + len(name) + 1] = name.encode() + b"\0"
        compare("?CountTrackPositionMarkers@@YAHPBD@Z", memory, globals_, struct.pack("<I", ARENA + 0x8000))
        compare("?GetTrackPositionMarker@@YAPAUTrackPositionMarker@@PBDH@Z", memory, globals_,
                struct.pack("<Ii", ARENA + 0x8000, rng.randrange(4)))
    for trial in range(50):
        memory, globals_ = build(0)
        compare("?Construct@WRoadNav@", memory, globals_, b"", this=ARENA + 0x9000)
    for trial in range(3000):
        complete = rng.random() < 0.6
        count = rng.choice((0, 1, 2, 6, 6, 8))
        markers = rng.randrange(0, 12)
        slots = None
        if complete:
            markers = max(markers, 8)
            slots = list(range(8)) + [rng.choice((0, 1, 9)) for _ in range(markers - 8)]
            rng.shuffle(slots)
        memory, globals_ = build(markers, tracks=(0, 4000) if complete else (0, 4000, 4001), slots=slots)
        if complete:
            for i in range(markers):
                struct.pack_into("<I", memory, 0x1000 + i * 0x30 + 8, bhash("ForwardStartingPosition"))
            globals_[2] = (0x89e7a4, struct.pack("<i", 0))
        packs = [rng.choice((0, ARENA + 0xa000)) for _ in range(count)]
        heights = [rng.choice((12.5, -3.25, 53.5, 0.0)) for _ in range(count)]
        direction = (rng.uniform(-1, 1), rng.uniform(-1, 1))
        compare("?GetStartingPositions@", memory, globals_, struct.pack("<iI", count, ARENA + 0x10000),
                packs=packs, heights=heights, direction=direction, result=None)
    for trial in range(600):
        memory, globals_ = build(0)
        collider = ARENA + 0xc000
        others = [ARENA + 0xd000, ARENA + 0xd400]
        # Unpacked list 0x88f748 with zero to two other colliders, collider linked into it or a pack list.
        chain = [0x88f748] + others[:rng.randrange(3)]
        linked_collider = rng.random() < 0.7
        if linked_collider:
            chain.insert(rng.randrange(1, len(chain) + 1), collider)
        for i, node in enumerate(chain):
            nxt, prv = chain[(i + 1) % len(chain)], chain[i - 1]
            data = struct.pack("<II", nxt, prv)
            if node == 0x88f748:
                globals_.append((0x88f748, data))
            else:
                memory[node - ARENA:node - ARENA + 8] = data
        if not linked_collider:
            struct.pack_into("<II", memory, collider - ARENA, ARENA + 0xe000, ARENA + 0xe000)
            struct.pack_into("<II", memory, 0xe000, collider, collider)
        struct.pack_into("<I", memory, collider - ARENA + 8, rng.choice((0, 0, ARENA + 0xf000)))
        faces = rng.randrange(0, 5)
        struct.pack_into("<i", memory, collider - ARENA + 0x2bc, faces)
        for i in range(4):
            struct.pack_into("<I", memory, collider - ARENA + 0x2c0 + 4 * i, ARENA + 0x11000 + i * 0x80)
        area = rng.randrange(0, 4)
        struct.pack_into("<i", memory, collider - ARENA + 0x2e0, area)
        for i in range(4):
            struct.pack_into("<I", memory, collider - ARENA + 0x3c + 4 * i, ARENA + 0x12000 + i * 0x80)
        if linked_collider:
            compare("?Destruct@WCollider@", memory, globals_, b"", this=collider)
        compare("?Construct@WCollider@", memory, globals_, b"", this=collider,
                result="eax" if False else None)
    report = dict(executable_sha256=hashlib.sha256(raw).hexdigest(), comparisons=counts,
                  functions={p: dict(address=hex(a), size=n,
                                     sha256=hashlib.sha256(image[a - 0x400000:a - 0x400000 + n]).hexdigest())
                             for p, (a, n) in FUNCTIONS.items()},
                  traced_interfaces={n: hex(a) for n, (a, c) in INTERFACES.items()},
                  retained_original=["bStringHash 0x43db50/0x43db80", "angle 0x43e6d0", "empty 0x4022c0"],
                  byte_matching="Not established", compiler="MSVC 7.1 /Ox /Ob0 /Z7 /EHsc",
                  checks=["EAX results", "whole arena and list/track/session globals",
                          "collider and road-navigator call traces with point values", "stack and saved registers"],
                  limitations=["Collider routines (recovered in WCollision.cpp) and road navigation 0x5da370 are "
                               "scripted interfaces here", "Finite randomised marker lists"])
    (BUILD / "verification.json").write_text(json.dumps(report, indent=2) + "\n")
    print(json.dumps(report["comparisons"], indent=2))
    print("PASS", sum(counts.values()), "comparisons")


if __name__ == "__main__":
    main()
