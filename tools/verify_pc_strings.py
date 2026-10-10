#!/usr/bin/env python3
"""Build and verify the initial PC v1.2 strings slice without a full-game split."""

import argparse
import ctypes
import hashlib
import json
from pathlib import Path
import random
import struct
import subprocess

import pefile
from unicorn import Uc, UcError, UC_ARCH_X86, UC_MODE_32, UC_ERR_READ_UNMAPPED
from unicorn.x86_const import (UC_X86_REG_EAX, UC_X86_REG_ESP, UC_X86_REG_EIP,
                              UC_X86_REG_EBX, UC_X86_REG_ESI, UC_X86_REG_EDI,
                              UC_X86_REG_EBP)

ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / "src/Speed/Indep/Libs/bWare/Strings.cpp"
BUILD = ROOT / "build/pc-strings"
MANIFEST = ROOT / "config/SPEED2_EXE_1_2/strings.json"


def run(*args):
    subprocess.run([str(arg) for arg in args], cwd=ROOT, check=True)


def write_json(path, value):
    path.write_text(json.dumps(value, indent=2) + "\n", encoding="utf-8")


def coff_functions(path):
    """Read MSVC COFF function definitions, including their auxiliary sizes."""
    data = path.read_bytes()
    machine, count, _, symoff, nsyms, optional, _ = struct.unpack_from("<HHIIIHH", data)
    if machine != 0x14C:
        raise ValueError("Expected an x86 COFF object")
    sections = [struct.unpack_from("<8sIIIIIIHHI", data, 20 + optional + i * 40)
                for i in range(count)]
    strings = symoff + nsyms * 18
    result = {}
    i = 0
    while i < nsyms:
        offset = symoff + i * 18
        raw, value, section, kind, storage, aux = struct.unpack_from("<8sIhHBB", data, offset)
        if raw[:4] == b"\0" * 4:
            nameoff = strings + struct.unpack_from("<I", raw, 4)[0]
            name = data[nameoff:data.index(b"\0", nameoff)].decode("ascii")
        else:
            name = raw.rstrip(b"\0").decode("ascii")
        if kind == 0x20 and storage == 2 and section > 0 and aux:
            size = struct.unpack_from("<I", data, offset + 22)[0]
            sec = sections[section - 1]
            if sec[7] or value + size > sec[3]:
                raise ValueError("Strings slice must have bounded, relocation-free functions")
            start = sec[4] + value
            result[name] = data[start:start + size]
        i += 1 + aux
    return result


def prepare(exe, entries):
    pe = pefile.PE(str(exe), fast_load=True)
    if (pe.FILE_HEADER.Machine != 0x14C or pe.OPTIONAL_HEADER.Magic != 0x10B
            or pe.OPTIONAL_HEADER.ImageBase != 0x400000
            or pe.OPTIONAL_HEADER.AddressOfEntryPoint != 0x35BCC7):
        raise ValueError("Executable does not have the expected PC v1.2 PE layout")
    original = {}
    for entry in entries:
        address = int(entry["address"], 16)
        code = pe.get_data(address - 0x400000, entry["size"])
        if hashlib.sha256(code).hexdigest() != entry["sha256"]:
            raise ValueError(f"Original function fingerprint differs: {entry['name']}")
        original[entry["name"]] = code
    # This is a manually verified, selected-function model in delink's IDA
    # interchange format. It is not a complete IDA export or full-game split.
    model = {
        "delink_ida_version": 1,
        "meta": {"arch": "x86", "procname": "metapc", "bits": 32,
                 "endian": "little", "image_base": 0x400000,
                 "min_ea": 0x401000, "max_ea": 0x400000 + pe.OPTIONAL_HEADER.SizeOfImage,
                 "filetype": "PE", "input_file": exe.name},
        "segments": [], "functions": [], "names": [], "relocations": [],
    }
    for entry in entries:
        start = int(entry["address"], 16)
        name = "_" + entry["name"]
        model["segments"].append({"name": ".text", "start": start,
            "end": start + entry["size"], "perm_r": True, "perm_w": False,
            "perm_x": True, "class": "CODE"})
        model["functions"].append({"start": start, "end": start + entry["size"],
            "name": name, "thunk": False, "lib": False, "static": False, "public": True})
        model["names"].append({"addr": start, "name": name, "public": True,
                               "weak": False, "is_func": True})
    write_json(BUILD / "symbols.json", model)
    write_json(BUILD / "splits.json", {"Strings.obj": [entry["address"] for entry in entries]})
    return original


class OriginalRunner:
    def __init__(self, code):
        self.uc = Uc(UC_ARCH_X86, UC_MODE_32)
        self.uc.mem_map(0x100000, 0x1000)
        self.uc.mem_write(0x100000, code)
        self.uc.mem_map(0x200000, 0x10000)
        self.uc.mem_map(0x300000, 0x2000)

    def call(self, data, seed):
        pointer = 0 if data is None else 0x200000
        if data is not None:
            if len(data) >= 0x10000:
                raise ValueError("Test string too large")
            self.uc.mem_write(pointer, data + b"\0")
        return self.call_args(pointer, seed)

    def call_args(self, *args):
        stack = 0x301000
        self.uc.mem_write(stack, struct.pack("<" + "I" * (len(args) + 1),
                                            0x100F00, *(arg & 0xFFFFFFFF for arg in args)))
        self.uc.reg_write(UC_X86_REG_ESP, stack)
        self.uc.reg_write(UC_X86_REG_EAX, 0xDEADBEEF)
        saved = {UC_X86_REG_EBX: 0x11223344, UC_X86_REG_ESI: 0x55667788,
                 UC_X86_REG_EDI: 0x99AABBCC, UC_X86_REG_EBP: 0xDDEEFF00}
        for register, value in saved.items():
            self.uc.reg_write(register, value)
        self.uc.emu_start(0x100000, 0x100F00, count=1000000)
        if self.uc.reg_read(UC_X86_REG_EIP) != 0x100F00:
            raise ValueError("Original function did not return within instruction limit")
        if self.uc.reg_read(UC_X86_REG_ESP) != stack + 4:
            raise ValueError("Unexpected calling convention")
        if any(self.uc.reg_read(register) != value for register, value in saved.items()):
            raise ValueError("Original function changed a callee-saved register")
        return self.uc.reg_read(UC_X86_REG_EAX)


def verify_behavior(original):
    run("g++", "-std=c++98", "-O2", "-shared", "-fPIC", SOURCE,
        "-o", BUILD / "strings-host.so")
    host = ctypes.CDLL(str(BUILD / "strings-host.so"))
    rng = random.Random(0x126890)
    cases = [b"", b"a", b"A", b"NFS Underground 2", b"aZ[az{",
             bytes(range(1, 256)), b"\x80\xffaz", b"abc\0ignored", b"x" * 4096]
    cases += [bytes(rng.randrange(1, 256) for _ in range(rng.randrange(256)))
              for _ in range(500)]
    counts = {}
    for name, code in original.items():
        if name.startswith("nfs_"):
            continue
        fn = getattr(host, name)
        seeded = name == "cshashWithStartValue"
        fn.argtypes = [ctypes.c_char_p] + ([ctypes.c_uint32] if seeded else [])
        fn.restype = ctypes.c_uint32
        runner = OriginalRunner(code)
        seeds = [0, 1, 0xFFFFFFFF, 0x80000000, 0x12345678] if seeded else [0]
        count = 0
        for data in cases + [None]:
            if data is None and name in ("cshash", "cshashWithStartValue"):
                # These routines dereference their argument in the original.
                # Never call the host implementation with an invalid pointer.
                try:
                    runner.call(None, 0)
                except UcError as error:
                    if error.errno != UC_ERR_READ_UNMAPPED:
                        raise
                    continue
                raise ValueError(f"Expected null-pointer fault for {name}")
            for seed in seeds:
                expected = runner.call(data, seed)
                actual = fn(data, seed) if seeded else fn(data)
                if actual != expected:
                    raise ValueError(f"Behavior differs for {name}: {data!r}, seed {seed:#x}")
                count += 1
        counts[name] = count
    counts.update(verify_copies(host, original, cases))
    counts.update(verify_comparisons(host, original, rng))
    return counts


def verify_copies(host, original, cases):
    counts = {}
    for name in ("nfs_strcpy", "nfs_strncpy"):
        fn = getattr(host, name)
        bounded = name == "nfs_strncpy"
        fn.argtypes = [ctypes.c_void_p, ctypes.c_void_p] + ([ctypes.c_int] if bounded else [])
        fn.restype = ctypes.c_void_p
        runner = OriginalRunner(original[name])
        count = 0
        for data in cases:
            length = len(data.split(b"\0", 1)[0])
            limits = [-1, 0, 1, length, length + 1, length + 5] if bounded else [None]
            for limit in limits:
                # Surround both buffers with sentinels; compare every byte, including
                # bytes beyond the terminator and beyond a bounded copy's limit.
                src_bytes = b"\xC7" * 16 + data + b"\0" + b"\xB6" * 32
                dst_bytes = b"\xA5" * (len(data) + 65)
                src = ctypes.create_string_buffer(src_bytes, len(src_bytes))
                dst = ctypes.create_string_buffer(dst_bytes, len(dst_bytes))
                runner.uc.mem_write(0x201000, src_bytes)
                runner.uc.mem_write(0x209000, dst_bytes)
                args = [0x209010, 0x201010] + ([limit] if bounded else [])
                result = runner.call_args(*args)
                host_args = [ctypes.addressof(dst) + 16, ctypes.addressof(src) + 16]
                if bounded:
                    host_args.append(limit)
                actual = fn(*host_args)
                if result != 0x209010 or actual != ctypes.addressof(dst) + 16:
                    raise ValueError(f"Wrong destination return value: {name}")
                if bytes(runner.uc.mem_read(0x209000, len(dst_bytes))) != dst.raw:
                    raise ValueError(f"Destination buffer differs: {name}, limit {limit}")
                if bytes(runner.uc.mem_read(0x201000, len(src_bytes))) != src.raw:
                    raise ValueError(f"Source buffer differs: {name}")
                if src.raw != src_bytes:
                    raise ValueError(f"Copy modified its source: {name}")
                count += 1
        if bounded:
            if runner.call_args(0, 0, 0) != 0 or fn(None, None, 0) is not None:
                raise ValueError("Zero-length bounded copy must not dereference either pointer")
            count += 1
        counts[name] = count
    return counts


def verify_comparisons(host, original, rng):
    # All one-byte pairs, including signed-byte ordering and ASCII folding boundaries.
    pairs = [(bytes([a]), bytes([b])) for a in range(256) for b in range(256)]
    pairs += [(b"", b""), (b"a", b"b"), (b"ab", b"ac"), (b"az", b"aa"),
              (b"a", b"ab"), (b"ab", b"a"), (b"abc\0x", b"abc\0y"),
              (b"AlphaZ", b"aLPHAz"), (b"x" * 4096, b"x" * 4096)]
    for _ in range(500):
        prefix = bytes(rng.randrange(1, 256) for _ in range(rng.randrange(64)))
        pairs.append((prefix + bytes([rng.randrange(1, 256)]),
                      prefix + bytes([rng.randrange(1, 256)])))
    counts = {}
    for name in ("nfs_strcmp", "nfs_stricmp", "nfs_strncmp"):
        fn = getattr(host, name)
        bounded = name == "nfs_strncmp"
        fn.argtypes = [ctypes.c_void_p, ctypes.c_void_p] + ([ctypes.c_int] if bounded else [])
        fn.restype = ctypes.c_int
        runner = OriginalRunner(original[name])
        count = 0
        for a, b in pairs:
            limits = [-1, 0, 1, 2, max(len(a), len(b)) + 1] if bounded else [None]
            for limit in limits:
                # The original bounded comparator can read a[-1]/b[-1] on
                # initially empty strings. These guard bytes make that access defined.
                a_bytes = b"\x81" + a + b"\0" + b"\xC7" * 16
                b_bytes = b"\x7F" + b + b"\0" + b"\xB6" * 16
                aa = ctypes.create_string_buffer(a_bytes, len(a_bytes))
                bb = ctypes.create_string_buffer(b_bytes, len(b_bytes))
                runner.uc.mem_write(0x201000, a_bytes)
                runner.uc.mem_write(0x205000, b_bytes)
                args = [0x201001, 0x205001] + ([limit] if bounded else [])
                expected = ctypes.c_int32(runner.call_args(*args)).value
                host_args = [ctypes.addressof(aa) + 1, ctypes.addressof(bb) + 1]
                if bounded:
                    host_args.append(limit)
                if fn(*host_args) != expected:
                    raise ValueError(f"Comparison differs: {name}, {a!r}, {b!r}, limit {limit}")
                if (aa.raw != a_bytes or bb.raw != b_bytes
                        or bytes(runner.uc.mem_read(0x201000, len(a_bytes))) != a_bytes
                        or bytes(runner.uc.mem_read(0x205000, len(b_bytes))) != b_bytes):
                    raise ValueError(f"Comparison modified an input: {name}")
                count += 1
        if bounded:
            if runner.call_args(0, 0, 0) != 0 or fn(None, None, 0) != 0:
                raise ValueError("Zero-length comparison must not dereference either pointer")
            count += 1
        counts[name] = count
    return counts


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--exe", type=Path,
                        default=ROOT / "orig/SPEED2_EXE_1_2/speed2.exe")
    args = parser.parse_args()
    BUILD.mkdir(parents=True, exist_ok=True)
    entries = json.loads(MANIFEST.read_text())
    original = prepare(args.exe.resolve(), entries)
    run(ROOT / "build/tools/delink", "ida-split", BUILD / "symbols.json",
        args.exe.resolve(), "--idapro", BUILD / "splits.json", "-o", BUILD / "original")
    candidate = BUILD / "Strings.obj"
    run(ROOT / "build/tools/wibo", "build/compilers/Win32/7.1/cl.exe",
        "/nologo", "/c", "/Ox", "/Z7", "/EHsc",
        "/Fo" + str(candidate.relative_to(ROOT)), SOURCE.relative_to(ROOT))
    compiled = coff_functions(candidate)
    for name, code in original.items():
        if compiled.get("_" + name) != code:
            raise ValueError(f"Compiled function body is not byte-identical: {name}")
    write_json(BUILD / "objdiff.json", {"min_version": "3.7.0", "units": [{
        "name": "PC v1.2 strings slice", "target_path": str(BUILD / "original/Strings.obj"),
        "base_path": str(candidate)}]})
    run(ROOT / "build/tools/objdiff-cli", "diff", "-1", BUILD / "original/Strings.obj",
        "-2", candidate, "-o", BUILD / "diff.json")
    diff = json.loads((BUILD / "diff.json").read_text())
    scores = {s["name"]: s.get("match_percent") for s in diff["left"]["symbols"]}
    for name in original:
        if scores.get("_" + name) != 100.0:
            raise ValueError(f"objdiff did not report 100% for {name}")
    counts = verify_behavior(original)
    report = {"scope": "Selected PC v1.2 function bodies; inter-function padding excluded",
              "executable_sha256": hashlib.sha256(args.exe.read_bytes()).hexdigest(),
              "compiler": "MSVC 7.1 /Ox /Z7 /EHsc", "objdiff_version": "3.7.0",
              "functions": [{**entry, "byte_identical": True, "objdiff_match_percent": 100.0,
                             "behavior_cases": counts[entry["name"]]} for entry in entries]}
    write_json(BUILD / "verification.json", report)
    for entry in report["functions"]:
        print(f"{entry['name']}: {entry['size']} identical bytes, objdiff 100%, "
              f"{entry['behavior_cases']} behavior cases")


if __name__ == "__main__":
    main()
