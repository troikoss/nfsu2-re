#!/usr/bin/env python3
"""Execute UG2's original PhysicsMover-associated timestep partition fragment.

This is an instruction probe, not a replacement stepping algorithm. The full
update function and its callers are not recovered by this isolated fragment.
"""
import hashlib
import json
import math
import struct
import pefile
from unicorn import Uc, UC_ARCH_X86, UC_MODE_32, UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EAX, UC_X86_REG_EBP, UC_X86_REG_ESP, UC_X86_REG_EIP, UC_X86_REG_FPCW
from probe_pc_race_reward import ROOT, EXE


def main():
    raw = EXE.read_bytes()
    pe = pefile.PE(data=raw)
    base, image = pe.OPTIONAL_HEADER.ImageBase, pe.get_memory_mapped_image()
    for start, end, digest in (
        (0x5b72d0, 0x5b7562, "a8e4f0888958717ca75d905c1f6fb2982b6e1c378e267666e1a4ed3a4366a565"),
        (0x75bed4, 0x75bf49, "5333b186c02836974c6f792303aeb2c00d856316b93ccbbe65f51def6ae661b4")):
        if base != 0x400000 or hashlib.sha256(image[start-base:end-base]).hexdigest() != digest:
            raise ValueError("Unsupported original instruction fingerprint")
    constant = struct.unpack_from("<f", image, 0x7fc070-base)[0]
    assert image[0x7fc070-base:0x7fc074-base] == bytes.fromhex("e02d903b")
    uc = Uc(UC_ARCH_X86, UC_MODE_32)
    uc.mem_map(base, (len(image)+4095) & ~4095)
    uc.mem_write(base, image)
    uc.mem_map(0x2000000, 0x20000)
    frame, stack, start, end = 0x2010000, 0x200f000, 0x5b7344, 0x5b736d

    def guard(machine, address, size, user):
        if not (start <= address < end or 0x75bed4 <= address < 0x75bf49):
            raise RuntimeError(f"Unexpected helper at {address:#x}")

    uc.hook_add(UC_HOOK_CODE, guard)
    to_float = lambda value: struct.unpack("<f", struct.pack("<f", value))[0]
    cases = [(f"{hz}Hz", to_float(1/hz)) for hz in (10, 20, 30, 60, 90, 120, 144, 165, 240, 360, 1000)]
    cases.append(("zero", 0.0))
    for multiple in range(1, 33):
        center = to_float(multiple*constant)
        bits = struct.unpack("<I", struct.pack("<f", center))[0]
        for delta in (-1, 0, 1):
            cases.append((f"threshold-{multiple}-{delta:+d}", struct.unpack("<f", struct.pack("<I", bits+delta))[0]))
    observed = []
    for name, duration in cases:
        for control in (0x027f, 0x037f):
            uc.mem_write(frame+8, struct.pack("<f", duration))
            uc.reg_write(UC_X86_REG_EBP, frame)
            uc.reg_write(UC_X86_REG_ESP, stack)
            uc.reg_write(UC_X86_REG_FPCW, control)
            uc.emu_start(start, end, count=1000)
            assert uc.reg_read(UC_X86_REG_EIP) == end
            assert uc.reg_read(UC_X86_REG_ESP) == stack
            count = uc.reg_read(UC_X86_REG_EAX)
            step = struct.unpack("<f", uc.mem_read(frame-4, 4))[0]
            count_float = struct.unpack("<f", uc.mem_read(frame-8, 4))[0]
            quotient = duration/constant
            if control == 0x027f:
                quotient = to_float(quotient)
            expected_count = math.trunc(quotient)+1
            assert count == expected_count, (name, control, count, expected_count)
            assert count_float == float(count)
            assert step == to_float(duration/count)
            observed.append(dict(case=name, x87_control=hex(control), input_seconds=duration,
                                 count=count, substep_seconds=step))
    report = dict(executable_sha256=hashlib.sha256(raw).hexdigest(),
                  fragment=[hex(start), hex(end)], constant_address="0x7fc070",
                  constant_float=constant, stubbed_callees=[], passed=len(observed),
                  limitation="Partition fragment only; vehicle update and caller timestep source unresolved",
                  cases=observed)
    path = ROOT / "build/analysis/racing/vehicle-substeps-probe.json"
    path.write_text(json.dumps(report, indent=2)+"\n")
    print(json.dumps({k: v for k, v in report.items() if k != "cases"}, indent=2))
    print(json.dumps([case for case in observed if case["case"] in ("60Hz", "144Hz", "240Hz")], indent=2))


if __name__ == "__main__":
    main()
