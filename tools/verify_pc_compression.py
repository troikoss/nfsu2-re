#!/usr/bin/env python3
"""Differentially verify the recovered bWare decompressors against PC v1.2 code.

The original JDLZ (0x573700), HUFF (0x5744c0 -> 0x573840) and RAWW (0x5744f0)
routines run in Unicorn with the retail image mapped at its original addresses.
Inputs are the compressed texture slots of the player's own car texture packs.
"""

import argparse
import ctypes
from pathlib import Path
import random
import struct
import subprocess

import pefile
from unicorn import Uc, UC_ARCH_X86, UC_MODE_32
from unicorn.x86_const import UC_X86_REG_EAX, UC_X86_REG_ESP, UC_X86_REG_EIP

ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / "src/Speed/Indep/Libs/bWare/Compression.cpp"
BUILD = ROOT / "build/pc-compression"
INSTALLS = [ROOT / "Need for Speed Underground 2", ROOT.parent / "games/Need for Speed Underground 2"]

INPUT = 0x10000000
OUTPUT = 0x20000000
STACK = 0x30000000
RETURN = 0x30000F00


def find_install(path):
    for candidate in ([Path(path)] if path else INSTALLS):
        if (candidate / "speed2.exe").exists():
            return candidate
    raise SystemExit("PC v1.2 installation not found; pass --game")


class Original:
    def __init__(self, exe):
        pe = pefile.PE(str(exe), fast_load=True)
        self.uc = Uc(UC_ARCH_X86, UC_MODE_32)
        image = pe.get_memory_mapped_image()
        size = (max(len(image), pe.OPTIONAL_HEADER.SizeOfImage) + 0xFFF) & ~0xFFF
        self.uc.mem_map(0x400000, size)
        self.uc.mem_write(0x400000, image)
        self.uc.mem_map(STACK, 0x2000)
        self.uc.mem_map(INPUT, 0x800000)
        self.uc.mem_map(OUTPUT, 0x800000)

    def call(self, address, source, output_size):
        self.uc.mem_write(INPUT, source + b"\0" * 64)
        self.uc.mem_write(OUTPUT, b"\xA5" * (output_size + 64))
        stack = STACK + 0x1000
        self.uc.mem_write(stack, struct.pack("<III", RETURN, INPUT, OUTPUT))
        self.uc.reg_write(UC_X86_REG_ESP, stack)
        self.uc.emu_start(address, RETURN, count=200000000)
        if self.uc.reg_read(UC_X86_REG_EIP) != RETURN:
            raise ValueError(f"{address:#x} did not return")
        result = self.uc.reg_read(UC_X86_REG_EAX)
        return result, bytes(self.uc.mem_read(OUTPUT, output_size + 64))


def slots(path):
    data = path.read_bytes()
    found = []
    offset = data.find(struct.pack("<I", 0x33310003))
    while offset != -1:
        size = struct.unpack_from("<I", data, offset + 4)[0]
        for k in range(offset + 8, offset + 8 + size - 23, 24):
            _, start, packed, unpacked, _, _ = struct.unpack_from("<6I", data, k)
            if start + packed <= len(data):
                found.append((data[start:start + packed], unpacked))
        offset = data.find(struct.pack("<I", 0x33310003), offset + 8 + size)
    return found


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--game")
    parser.add_argument("--limit", type=int, default=400)
    args = parser.parse_args()
    game = find_install(args.game)
    BUILD.mkdir(parents=True, exist_ok=True)
    shim = BUILD / "shim.cpp"
    shim.write_text('int huffExtraBits = 0;\nint bDecompress(unsigned int *, unsigned char *);\n'
                    'extern "C" int VerifyDecompress(void *s, void *o) {\n'
                    '    return bDecompress((unsigned int *)s, (unsigned char *)o);\n}\n', encoding="ascii")
    subprocess.run(["g++", "-std=c++98", "-O2", "-shared", "-fPIC", str(SOURCE), str(shim),
                    "-o", str(BUILD / "compression-host.so")], check=True)
    host = ctypes.CDLL(str(BUILD / "compression-host.so"))
    host.VerifyDecompress.argtypes = [ctypes.c_void_p, ctypes.c_void_p]
    host.VerifyDecompress.restype = ctypes.c_int
    original = Original(game / "speed2.exe")
    cases = []
    for name in ("CARS/SKYLINE/TEXTURES.BIN", "CARS/TEXTURES.BIN", "CARS/SKYLINE/VINYLS.BIN"):
        cases += slots(game / name)
    rng = random.Random(0x573840)
    rng.shuffle(cases)
    cases = cases[:args.limit]
    # Synthetic RAWW blocks exercise the third wrapper.
    for length in (0, 1, 3, 4, 17, 1000):
        body = bytes(rng.randrange(256) for _ in range(length))
        cases.append((b"RAWW" + struct.pack("<BBHII", 1, 0, 0, length, length + 16) + body, length))
    counts = {}
    for source, size in cases:
        tag = source[:4].decode("latin1")
        address = {"JDLZ": 0x573700, "HUFF": 0x5744C0, "RAWW": 0x5744F0}[tag]
        expected, expected_bytes = original.call(address, source, size)
        src = ctypes.create_string_buffer(source + b"\0" * 64, len(source) + 64)
        out = ctypes.create_string_buffer(b"\xA5" * (size + 64), size + 64)
        actual = host.VerifyDecompress(src, out)
        if actual != expected or out.raw != expected_bytes:
            raise SystemExit(f"{tag} block differs (size {size}, result {actual} vs {expected})")
        counts[tag] = counts.get(tag, 0) + 1
    print("verified", ", ".join(f"{tag} {count}" for tag, count in sorted(counts.items())),
          "blocks byte-for-byte, including 64 guard bytes")


if __name__ == "__main__":
    main()
