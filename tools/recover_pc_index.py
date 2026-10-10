#!/usr/bin/env python3
"""Extract factual function metadata from a community IDC export without executing it."""

import argparse
import hashlib
import json
from pathlib import Path
import re

import pefile

ROOT = Path(__file__).resolve().parents[1]


def recover(idc_path, exe_path):
    text = idc_path.read_text(encoding="utf-8", errors="replace")
    names = {}
    for address, quoted in re.findall(
            r'set_name\s*\(\s*(0[Xx][0-9A-Fa-f]+)\s*,\s*("(?:[^"\\]|\\.)*")\s*\)', text):
        names[int(address, 16)] = json.loads(quoted)
    boundaries = sorted(set((int(a, 16), int(b, 16)) for a, b in re.findall(
        r'add_func\s*\(\s*(0[Xx][0-9A-Fa-f]+)\s*,\s*(0[Xx][0-9A-Fa-f]+)\s*\)', text)))
    pe = pefile.PE(str(exe_path), fast_load=True)
    base = pe.OPTIONAL_HEADER.ImageBase
    if pe.FILE_HEADER.Machine != 0x14C or base != 0x400000:
        raise ValueError("Expected the PC x86 executable at base 0x400000")
    if pe.OPTIONAL_HEADER.AddressOfEntryPoint != 0x35BCC7:
        raise ValueError("Expected the documented PC v1.2 entry point")
    functions = []
    rejected = []
    previous_end = 0
    for start, end in boundaries:
        sec = pe.get_section_by_rva(start - base)
        if (end <= start or sec is None or not sec.Characteristics & 0x20000000
                or end - base > sec.VirtualAddress + sec.SizeOfRawData
                or start < previous_end):
            rejected.append({"start": hex(start), "end": hex(end)})
            continue
        code = pe.get_data(start - base, end - start)
        functions.append({"start": hex(start), "end": hex(end), "size": end - start,
                          "name": names.get(start, f"sub_{start:08X}"),
                          "community_named": start in names,
                          "sha256": hashlib.sha256(code).hexdigest()})
        previous_end = end
    # Cross-check the established matches before treating this export as a seed.
    by_start = {int(f["start"], 16): f for f in functions}
    for verified in json.loads((ROOT / "config/SPEED2_EXE_1_2/strings.json").read_text()):
        recovered = by_start.get(int(verified["address"], 16))
        if (not recovered or recovered["size"] != verified["size"]
                or recovered["sha256"] != verified["sha256"]):
            raise ValueError(f"Community boundary differs from verified function: {verified['name']}")
    return {
        "scope": "Community function-boundary seeds; not proof of complete or correct analysis",
        "source": "https://github.com/yugecin/nfsu2-re/tree/idc",
        "idc_sha256": hashlib.sha256(idc_path.read_bytes()).hexdigest(),
        "executable_sha256": hashlib.sha256(exe_path.read_bytes()).hexdigest(),
        "function_count": len(functions),
        "community_named_count": sum(f["community_named"] for f in functions),
        "function_bytes": sum(f["size"] for f in functions),
        "functions": functions, "rejected_boundaries": rejected,
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--idc", type=Path, required=True)
    parser.add_argument("--exe", type=Path, default=ROOT / "orig/SPEED2_EXE_1_2/speed2.exe")
    parser.add_argument("--output", type=Path, default=ROOT / "build/analysis/function-index.json")
    args = parser.parse_args()
    result = recover(args.idc, args.exe)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(result, indent=2) + "\n", encoding="utf-8")
    print(f"{result['function_count']} function seeds, {result['community_named_count']} named, "
          f"{result['function_bytes']} bytes; {len(result['rejected_boundaries'])} rejected")


if __name__ == "__main__":
    main()
