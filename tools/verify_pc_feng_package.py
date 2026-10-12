#!/usr/bin/env python3
"""Bounded original/rebuilt execution of FEng package masks, selection and focus."""
import argparse
import hashlib
import json
import os
import random
import struct
import subprocess

import pefile
from unicorn import Uc, UC_ARCH_X86, UC_MODE_32, UC_HOOK_BLOCK
from unicorn.x86_const import (UC_X86_REG_EAX, UC_X86_REG_ECX, UC_X86_REG_ESP, UC_X86_REG_EIP,
                              UC_X86_REG_EBX, UC_X86_REG_ESI, UC_X86_REG_EDI, UC_X86_REG_EBP)
from probe_pc_race_reward import ROOT, EXE
from pc_coff import link_objects, read_object

BUILD = ROOT / "build/analysis/feng-package"
FUNCTIONS = {
    "FngSetFocusEnabled": (0x51cc60, 104),
    "FngSetControlMask": (0x537950, 34),
    "FngGetPackageValue10": (0x50b8a0, 30),
    "FngSetPackageValue10": (0x50b8c0, 31),
    "SetFocus": (0x52b3a0, 141),
    "SetFocusByHash": (0x52ce40, 94),
}
INTERFACES = {"FngFindPackage": (0x52cef0, 1, False), "FngHash": (0x505450, 1, False),
              "FngFindRecord": (0x50b840, 1, False), "FindNode": (0x51bb70, 1, True),
              "Post": (0x51aa20, 5, True)}
ARENA, STACK, STOP = 0x2000000, 0x200f000, 0x2100000


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source", default="src/Speed/Indep/Src/Frontend/FEngPackage.cpp")
    args = parser.parse_args()
    raw = EXE.read_bytes()
    assert hashlib.sha256(raw).hexdigest() == "f9dd86c054878ce6276beb07c1fd61874f7a1e4bf1f241b084c65b73e24168a7"
    image = pefile.PE(data=raw).get_memory_mapped_image()
    BUILD.mkdir(parents=True, exist_ok=True)
    obj = BUILD / "FEngPackage.obj"
    subprocess.run([str(ROOT / "build/tools/wibo"), "build/compilers/Win32/7.1/cl.exe", "/nologo", "/c",
                    "/Ox", "/Ob0", "/Z7", "/EHsc", "/Fo" + str(obj.relative_to(ROOT)),
                    "/Isrc/Speed/Indep/Src/Frontend", os.path.relpath(args.source, ROOT)], cwd=ROOT, check=True)
    external = {}
    for symbol in read_object(obj)["symbols"].values():
        if symbol["section"] != 0 or not symbol["name"].startswith("?"):
            continue
        name = symbol["name"][1:].split("@")[0]
        external[symbol["name"]] = 0x8637b0 if name == "fengArchive8637B0" else INTERFACES[name][0]
    linked = link_objects([obj], external)
    entries = {name: next(a for n, a in linked["exports"].items() if n.startswith("?" + name + "@"))
               for name in FUNCTIONS}
    state, machines = {}, []
    for rebuilt in (False, True):
        uc = Uc(UC_ARCH_X86, UC_MODE_32)
        uc.mem_map(0x400000, (len(image) + 4095) & ~4095)
        uc.mem_write(0x400000, image)
        uc.mem_map(ARENA, 0x10000)
        uc.mem_map(STOP, 0x1000)
        uc.mem_map(linked["address"], linked["allocation"])
        for address, body in linked["blocks"]:
            uc.mem_write(address, body)
        ranges = linked["ranges"] if rebuilt else [(a, a + size) for a, size in FUNCTIONS.values()]
        hooks = {a: (name, argc, member) for name, (a, argc, member) in INTERFACES.items()}

        def block(uc, address, size, user, ranges=ranges):
            if address == STOP:
                return
            if address in hooks:
                name, argc, member = hooks[address]
                sp = uc.reg_read(UC_X86_REG_ESP)
                words = struct.unpack("<" + "I" * (argc + 1), uc.mem_read(sp, 4 * (argc + 1)))
                this = uc.reg_read(UC_X86_REG_ECX) if member else 0
                state["trace"].append((name, this, words[1:]))
                if name == "Post":
                    # Capture focus at every queue call; assignment must occur after notifications.
                    state["trace"].append(("focus", struct.unpack("<I", uc.mem_read(words[3] + 0x8c, 4))[0]))
                else:
                    uc.reg_write(UC_X86_REG_EAX, state[name])
                uc.reg_write(UC_X86_REG_ESP, sp + 4 + (argc * 4 if member else 0))
                uc.reg_write(UC_X86_REG_EIP, words[0])
                return
            assert any(a <= address < b for a, b in ranges), ("unexpected execution", hex(address))
        uc.hook_add(UC_HOOK_BLOCK, block)
        machines.append(uc)

    rng = random.Random(0x52b3a0)
    counts = dict.fromkeys(FUNCTIONS, 0)

    def compare(name, memory, args, this=0, cleanup=0, result=False, **responses):
        outputs = []
        for rebuilt, uc in zip((False, True), machines):
            uc.mem_write(ARENA, bytes(memory))
            uc.mem_write(STACK, struct.pack("<I", STOP) + struct.pack("<" + "I" * len(args), *args))
            uc.reg_write(UC_X86_REG_ESP, STACK)
            uc.reg_write(UC_X86_REG_ECX, this)
            for reg in (UC_X86_REG_EBX, UC_X86_REG_ESI, UC_X86_REG_EDI, UC_X86_REG_EBP):
                uc.reg_write(reg, 0x12345678)
            state.clear()
            state.update(responses, trace=[])
            uc.emu_start(entries[name] if rebuilt else FUNCTIONS[name][0], STOP, count=10000)
            assert uc.reg_read(UC_X86_REG_EIP) == STOP
            assert uc.reg_read(UC_X86_REG_ESP) == STACK + 4 + cleanup
            assert all(uc.reg_read(r) == 0x12345678 for r in
                       (UC_X86_REG_EBX, UC_X86_REG_ESI, UC_X86_REG_EDI, UC_X86_REG_EBP))
            outputs.append((uc.reg_read(UC_X86_REG_EAX) if result else None, state["trace"],
                            bytes(uc.mem_read(ARENA, 0xe000))))
        assert outputs[0] == outputs[1], (name, args, outputs[0][:2], outputs[1][:2])
        counts[name] += 1

    package, record, node, manager, queue = [ARENA + n for n in (0x100, 0x300, 0x400, 0x500, 0x600)]
    objects = [ARENA + 0x1000 + 0x100 * i for i in range(8)]
    for trial in range(2000):
        memory = bytearray(rng.randbytes(0x10000))
        mask = rng.choice((0, 1, 2, 8, 0xffffffff, 0x80000000, rng.getrandbits(32)))
        compare("FngSetControlMask", memory, [ARENA, mask], FngFindPackage=rng.choice((0, package)))
        found = rng.choice((0, record))
        hash_ = rng.getrandbits(32)
        compare("FngGetPackageValue10", memory, [ARENA], result=True, FngHash=hash_, FngFindRecord=found)
        compare("FngSetPackageValue10", memory, [ARENA, mask], FngHash=hash_, FngFindRecord=found)
        old, new = rng.choice([0] + objects), rng.choice([0] + objects)
        struct.pack_into("<I", memory, 0x18c, old)
        compare("SetFocus", memory, [queue, new, rng.choice((0, 1, 2, 0x80, 0xff))], package, 12)
        count = rng.randrange(9)
        target = rng.randrange(4)
        for i, object_ in enumerate(objects):
            struct.pack_into("<I", memory, object_ - ARENA + 0x10, rng.randrange(4))
        struct.pack_into("<II", memory, 0x190, ARENA + 0x800, count)
        struct.pack_into("<8I", memory, 0x800, *objects)
        struct.pack_into("<I", memory, 0x418, rng.choice((0, package)))
        struct.pack_into("<I", memory, 0x508, queue)
        compare("SetFocusByHash", memory, [ARENA, target], manager, 8, FindNode=rng.choice((0, node)))
        compare("FngSetFocusEnabled", memory, [ARENA, target, rng.choice((0, 1, 2, 0x80, 0xff))], cleanup=12, FindNode=rng.choice((0, node)))
    report = {"comparisons": counts, "total": sum(counts.values()), "byte_matching": "not established",
              "fingerprints": {name: {"address": hex(address), "size": size,
                                      "sha256": hashlib.sha256(image[address-0x400000:address-0x400000+size]).hexdigest()}
                               for name, (address, size) in FUNCTIONS.items()},
              "interfaces": INTERFACES}
    (BUILD / "verification.json").write_text(json.dumps(report, indent=2) + "\n")
    print(json.dumps(report, indent=2))


if __name__ == "__main__":
    main()
