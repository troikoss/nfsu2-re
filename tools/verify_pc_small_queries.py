#!/usr/bin/env python3
"""Execute original/rebuilt collision notifications (0x483e20, 0x483e40, 0x5e0360) and profile readers."""
import hashlib
import json
import random
import struct
import subprocess

import pefile
from unicorn import Uc, UC_ARCH_X86, UC_MODE_32, UC_HOOK_BLOCK
from unicorn.x86_const import (UC_X86_REG_EAX, UC_X86_REG_ECX, UC_X86_REG_ESP, UC_X86_REG_EIP,
                              UC_X86_REG_EBX, UC_X86_REG_ESI, UC_X86_REG_EDI, UC_X86_REG_EBP)
from probe_pc_race_reward import ROOT, EXE
from pc_coff import link_objects, read_object

BUILD = ROOT / "build/analysis/small-queries"
SOURCES = ["src/Speed/Indep/Src/Physics/Collision/CollisionNotify.cpp",
           "src/Speed/Indep/Src/Frontend/ProfileQueries.cpp",
           "src/Speed/Indep/Src/Frontend/PlayerCamera.cpp",
           "src/Speed/Indep/Src/Frontend/FrontendQueries.cpp"]
FUNCTIONS = {
    "?RegisterCollision@Audio82B884@": (0x483e20, 20),
    "?UnregisterCollision@Audio82B884@": (0x483e40, 20),
    "?PostGameMessage@": (0x5e0360, 71),
    "?Value5B8240@": (0x5b8240, 6),
    "?Value5026F0@Settings83AAB4@": (0x5026f0, 24),
    "?Value502FE0@MemoryCard83A9E0@": (0x502fe0, 4),
    "?CycleCamera@PlayerSettings83AA64@": (0x502540, 97),
    "?Value5F2FA0@Player8900AC@": (0x5f2fa0, 98),
    "?FngControlMask@": (0x50d2d0, 51),
    "?Value502FF0@ProfileData83A9D0@": (0x502ff0, 71),
}
GLOBALS = {"audioEnabled7FAAA8": 0x7faaa8, "gameMessagePool883E38": 0x883e38, "gameMessageCount876DC8": 0x876dc8,
           "gameMessages800ECC": 0x800ecc, "feedbackDevice87078C": 0x87078c,
           "playerSettings83AA64": 0x83aa64, "hudDrift89E7D9": 0x89e7d9, "hudDriftOverride890104": 0x890104,
           "trackNumber89E7A0": 0x89e7a0, "viewMode8707DC": 0x8707dc,
           "uiMainFng78F530": 0x78f530, "uiQRModeSelectFng78F398": 0x78f398, "pcOLLobbyFng78F320": 0x78f320}
# Original bOMalloc 0x440e50 -> 0x440d40 runs on both sides (verified by verify_pc_bslotpool.py).
RETAINED = {"?bOMalloc@": 0x440e50, "?DriveCameraTypeForOption@": 0x502500}
RETAINED_RANGES = [(0x440e50, 0x440e59), (0x440d40, 0x440d9a), (0x502500, 0x50252b)]
# Camera table lookup: original 0x441140 (type) reads the view mode itself; the rebuilt call
# passes it (CameraSettingsTable::Find). Both are traced with a scripted result.
INTERFACES = {"Add481CD0": 0x481cd0, "Remove481E50": 0x481e50, "Find": 0x441140, "SetType20": None}
ARENA, STACK, STOP = 0x2000000, 0x200f000, 0x2100000


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
    symbols = [s for o in objects for s in read_object(o)["symbols"].values()]
    defined = {s["name"] for s in symbols if s["section"] > 0}
    externals = {}
    for s in symbols:
        name = s["name"]
        if s["section"] != 0 or name in defined or not name.startswith("?"):
            continue
        short = name[1:].split("@")[0]
        if short in GLOBALS:
            externals[name] = GLOBALS[short]
        elif short in INTERFACES:
            externals[name] = STOP + 0x100 + 0x10 * list(INTERFACES).index(short)
        elif any(name.startswith(p) for p in RETAINED):
            externals[name] = next(a for p, a in RETAINED.items() if name.startswith(p))
        else:
            raise AssertionError(name)
    linked = link_objects(objects, externals)
    entries = {p: next(a for n, a in linked["exports"].items() if n.startswith(p)) for p in FUNCTIONS}
    state = {}
    machines = []
    for rebuilt in (False, True):
        uc = Uc(UC_ARCH_X86, UC_MODE_32)
        uc.mem_map(0x400000, (len(image) + 4095) & ~4095)
        uc.mem_write(0x400000, image)
        uc.mem_map(ARENA, 0x10000)
        uc.mem_map(STOP, 0x1000)
        uc.mem_map(linked["address"], linked["allocation"])
        for address, body in linked["blocks"]:
            uc.mem_write(address, body)
        hooks = {(STOP + 0x100 + 0x10 * i if rebuilt or a is None else a): n for i, (n, a) in enumerate(INTERFACES.items())}
        own = linked["ranges"] if rebuilt else [(a, a + n) for a, n in FUNCTIONS.values()]
        ranges = own + RETAINED_RANGES

        def block(uc, address, size, user, hooks=hooks, ranges=ranges):
            if address == STOP:
                return
            if address in hooks:
                sp = uc.reg_read(UC_X86_REG_ESP)
                ret, arg = struct.unpack("<II", uc.mem_read(sp, 8))
                name = hooks[address]
                state["trace"].append((name, uc.reg_read(UC_X86_REG_ECX), arg))
                cleanup = 8
                if name == "Find":
                    if address != 0x441140:  # rebuilt passes the view mode read by 0x5b98a0
                        assert struct.unpack("<i", uc.mem_read(sp + 8, 4))[0] == struct.unpack("<i", uc.mem_read(0x8707dc, 4))[0]
                        cleanup = 12
                    uc.reg_write(UC_X86_REG_EAX, state["found"])
                uc.reg_write(UC_X86_REG_ESP, sp + cleanup)
                uc.reg_write(UC_X86_REG_EIP, ret)
                return
            assert any(a <= address < b for a, b in ranges), ("unexpected execution", hex(address))
        uc.hook_add(UC_HOOK_BLOCK, block)
        machines.append(uc)

    counts = dict.fromkeys(FUNCTIONS, 0)
    rng = random.Random(0x5e0360)

    def compare(prefix, memory, globals_, args=b"", this=0, cleanup=0, mask=0xffffffff, found=0):
        outputs = []
        for rebuilt, uc in zip((False, True), machines):
            uc.mem_write(ARENA, bytes(memory))
            for address, data in globals_:
                uc.mem_write(address, bytes(data))
            uc.mem_write(STACK, struct.pack("<I", STOP) + args)
            uc.reg_write(UC_X86_REG_ESP, STACK)
            uc.reg_write(UC_X86_REG_ECX, this)
            for reg in (UC_X86_REG_EBX, UC_X86_REG_ESI, UC_X86_REG_EDI, UC_X86_REG_EBP):
                uc.reg_write(reg, 0x12345678)
            state["trace"] = []
            state["found"] = found
            uc.emu_start(entries[prefix] if rebuilt else FUNCTIONS[prefix][0], STOP, count=10000)
            assert uc.reg_read(UC_X86_REG_EIP) == STOP
            assert uc.reg_read(UC_X86_REG_ESP) == STACK + 4 + cleanup, (prefix, rebuilt)
            assert all(uc.reg_read(r) == 0x12345678 for r in (UC_X86_REG_EBX, UC_X86_REG_ESI, UC_X86_REG_EDI, UC_X86_REG_EBP))
            outputs.append((uc.reg_read(UC_X86_REG_EAX) & mask, state["trace"], bytes(uc.mem_read(ARENA, 0xe000)),
                            [bytes(uc.mem_read(a, len(d))) for a, d in globals_]))
        if outputs[0] != outputs[1]:
            a, b = outputs[0][2], outputs[1][2]
            diff = [hex(i) for i in range(len(a)) if a[i] != b[i]][:8]
            print("diff", prefix, diff, [x.hex() for x in outputs[0][3]], [x.hex() for x in outputs[1][3]])
        assert outputs[0] == outputs[1], (prefix, outputs[0][:2], outputs[1][:2])
        counts[prefix] += 1

    for trial in range(2000):
        memory = bytearray(rng.randbytes(0x10000))
        audio = ARENA + 0x100
        struct.pack_into("<I", memory, 0x100 + 0x3c, ARENA + 0x800)
        enabled = rng.choice((0, 1, 2, -1))
        g = [(0x7faaa8, struct.pack("<i", enabled))]
        event = rng.getrandbits(32)
        compare("?RegisterCollision@Audio82B884@", memory, g, struct.pack("<I", event), audio, cleanup=4, mask=0)
        compare("?UnregisterCollision@Audio82B884@", memory, g, struct.pack("<I", event), audio, cleanup=4, mask=0)
        # Message pool: a slot pool with zero to two free slots (no expansion) and a queue list.
        pool = ARENA + 0x2000
        slots = [ARENA + 0x3000 + i * 0x40 for i in range(rng.randrange(0, 3))]
        flags = rng.choice((0, 2))
        struct.pack_into("<IIIIIiiiiiii", memory, 0x2000, 0, 0, 0, 0, slots[0] if slots else 0, flags,
                         rng.randrange(4), rng.randrange(4, 8), 0, 8, 0x24, 8)
        for i, slot in enumerate(slots):
            struct.pack_into("<I", memory, slot - ARENA, slots[i + 1] if i + 1 < len(slots) else 0)
        head = ARENA + 0x4000
        tail = rng.choice((head, ARENA + 0x4100))
        struct.pack_into("<II", memory, 0x4000, tail, tail)
        struct.pack_into("<II", memory, 0x4100, head, head)
        g = [(0x883e38, struct.pack("<I", pool)), (0x800ecc, struct.pack("<I", head)),
             (0x876dc8, struct.pack("<i", rng.randrange(-5, 50)))]
        compare("?PostGameMessage@", memory, g, struct.pack("<i", rng.choice((0x20004, 7, -1))))
        compare("?Value5B8240@", memory, [(0x87078c, bytes([rng.randrange(256)]))], mask=0xff)
        settings = ARENA + 0x6000
        struct.pack_into("<i", memory, 0x6000, rng.choice(list(range(13)) + [12, 12]))
        compare("?Value5026F0@Settings83AAB4@", memory, [], this=settings)
        card = ARENA + 0x7000
        memory[0x7020] = rng.choice((0, 1, 2, 0x80, 0xff))
        compare("?Value502FE0@MemoryCard83A9E0@", memory, [], this=card, mask=0xff)
    for trial in range(3000):
        memory = bytearray(rng.randbytes(0x10000))
        player, car, cartype, camera, vtable = ARENA + 0x100, ARENA + 0x1000, ARENA + 0x2000, ARENA + 0x3000, ARENA + 0x3100
        index = rng.choice((0, 1))
        struct.pack_into("<I", memory, 0x104, car)
        struct.pack_into("<i", memory, 0x110, index)
        struct.pack_into("<I", memory, 0x100 + 0x238, rng.choice((0, camera)))
        struct.pack_into("<I", memory, 0x100c, cartype)
        struct.pack_into("<I", memory, 0x3000, vtable)
        struct.pack_into("<I", memory, 0x3120, STOP + 0x100 + 0x10 * list(INTERFACES).index("SetType20"))
        rows = bytearray(rng.randbytes(0x50))
        for row in range(2):
            struct.pack_into("<i", rows, row * 0x28 + 0x10, rng.choice((-2, -1, 0, 1, 2, 3, 4, 5, 6)))
        g = [(0x83aa64, rows), (0x89e7d9, bytes([rng.choice((0, 0, 1))])),
             (0x890104, struct.pack("<i", rng.choice((0, 0, 1)))),
             (0x89e7a0, struct.pack("<i", rng.choice((4000, 0x104d, 0x104e, 0x1050, 0x1053, 0x1054)))),
             (0x8707dc, struct.pack("<i", rng.choice((1, 3))))]
        step = rng.choice((1, -1, 0, 2, -2))
        found = rng.choice((0, cartype + 0x730))
        compare("?Value5F2FA0@Player8900AC@", memory, g, struct.pack("<i", step), player, cleanup=4, found=found)
        compare("?CycleCamera@PlayerSettings83AA64@", memory, g, struct.pack("<i", step), 0x83aa64 + 0x28 * index,
                cleanup=4, mask=0)
    for player in list(range(-2, 7)) + [0x7fffffff, -0x80000000]:
        compare("?FngControlMask@", bytearray(0x10000), [], struct.pack("<i", player))
    for mode in (0, 1, 2, 3, 4, 8, 0x200, 0x100, -1):
        for value in (0, 1, -1):
            compare("?Value502FF0@ProfileData83A9D0@", bytearray(0x10000), [(0x83a9d0 + 0x156a8, struct.pack("<i", mode))],
                    struct.pack("<i", value), 0x83a9d0, cleanup=4)
    report = dict(executable_sha256=hashlib.sha256(raw).hexdigest(), comparisons=counts,
                  functions={p: dict(address=hex(a), size=n,
                                     sha256=hashlib.sha256(image[a - 0x400000:a - 0x400000 + n]).hexdigest())
                             for p, (a, n) in FUNCTIONS.items()},
                  traced_interfaces={n: hex(a) if a else "vtable +0x20" for n, a in INTERFACES.items()},
                  retained_original=["bOMalloc 0x440e50/0x440d40"], byte_matching="Not established")
    (BUILD / "verification.json").write_text(json.dumps(report, indent=2) + "\n")
    print(json.dumps(counts, indent=2))
    print("PASS", sum(counts.values()), "comparisons")


if __name__ == "__main__":
    main()
