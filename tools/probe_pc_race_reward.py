#!/usr/bin/env python3
"""Probe original x86 reward instructions, not a gameplay implementation.

The unidentified callee at 0x5287a0 is stubbed with controlled returns.
This verifies the caller's selection branches only, not race eligibility.
"""
import os
import hashlib
import itertools
import json
from pathlib import Path
import struct
import pefile
from unicorn import Uc, UC_ARCH_X86, UC_MODE_32, UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EAX, UC_X86_REG_ECX, UC_X86_REG_ESP, UC_X86_REG_EIP, UC_X86_REG_ESI

ROOT = Path(__file__).resolve().parents[1]
EXE = Path(os.environ.get("UG2_EXE", str(ROOT.parent / "Need for Speed Underground 2/speed2.exe")))
ENTRY, END, CALLEE = 0x532e50, 0x532e9f, 0x5287a0
EXPECTED_HASH = "56587b0b1dd3d92ddd82b9c03cc38df0d052a12b8be1c75cb24a72bfc065525e"


def main():
    raw = EXE.read_bytes()
    pe = pefile.PE(data=raw)
    base = pe.OPTIONAL_HEADER.ImageBase
    image = pe.get_memory_mapped_image()
    digest = hashlib.sha256(image[ENTRY-base:END-base]).hexdigest()
    if base != 0x400000 or digest != EXPECTED_HASH:
        raise SystemExit("Unsupported executable: routine fingerprint differs")
    machine = Uc(UC_ARCH_X86, UC_MODE_32)
    machine.mem_map(base, (len(image)+4095) & ~4095)
    machine.mem_write(base, image)
    machine.mem_map(0x2000000, 0x30000)
    career, race, sponsor, stack, stop = 0x2000000, 0x2010000, 0x2011000, 0x202f000, 0x202ff00
    state = {}

    def write32(address, value):
        machine.mem_write(address, struct.pack("<I", value))

    def hook(uc, address, size, user):
        if address == CALLEE:
            state["calls"] += 1
            sp = uc.reg_read(UC_X86_REG_ESP)
            assert uc.reg_read(UC_X86_REG_ECX) == career + 0xd24
            assert bytes(uc.mem_read(sp+4, 4)) == b"\0"*4
            target = struct.unpack("<I", uc.mem_read(sp, 4))[0]
            uc.reg_write(UC_X86_REG_EAX, state["gate"])
            uc.reg_write(UC_X86_REG_ESP, sp+8)
            uc.reg_write(UC_X86_REG_EIP, target)

    machine.hook_add(UC_HOOK_CODE, hook)
    cases = []
    for global_on, gate, kind, sponsor_on, stage, bank, sponsor_bank in itertools.product(
            (0, 1), (0, 1), (0, 1, 2, 3, 255), (0, 1), (0, 1, 4),
            (0, 1250, 0xffffffff), (0, 2500, 65535)):
        state.update(gate=gate, calls=0)
        write32(0x863470, global_on)
        write32(career+0x8730, race)
        write32(career+0x7af8, stage)
        write32(career+0x73c0+stage*0x104, sponsor if sponsor_on else 0)
        write32(race+0x30, bank)
        machine.mem_write(race+0x34, bytes([kind]))
        machine.mem_write(sponsor+2, struct.pack("<H", sponsor_bank))
        write32(stack, stop)
        machine.reg_write(UC_X86_REG_ESP, stack)
        machine.reg_write(UC_X86_REG_ECX, career)
        machine.reg_write(UC_X86_REG_ESI, 0x12345678)
        machine.emu_start(ENTRY, stop, count=100)
        observed = machine.reg_read(UC_X86_REG_EAX)
        expected = 0 if not (global_on and gate) else (sponsor_bank if kind == 1 and sponsor_on else bank)
        assert observed == expected
        assert state["calls"] == global_on
        assert machine.reg_read(UC_X86_REG_ESP) == stack+4
        assert machine.reg_read(UC_X86_REG_ESI) == 0x12345678
        cases.append(dict(global_on=global_on, stub_gate=gate, reward_type=kind,
                          sponsor_present=sponsor_on, stage=stage, bank_bits=bank,
                          sponsor_bank=sponsor_bank, returned_bits=observed))
    report = dict(executable_sha256=hashlib.sha256(raw).hexdigest(), routine_sha256=digest,
                  entry=hex(ENTRY), end_exclusive=hex(END), stubbed_callee=hex(CALLEE),
                  limitation="Caller branches only; callee semantics and retail runtime not verified",
                  passed=len(cases), cases=cases)
    output = ROOT / "build/analysis/racing/reward-probe.json"
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(json.dumps(report, indent=2)+"\n")
    print(json.dumps({k: v for k, v in report.items() if k != "cases"}, indent=2))


if __name__ == "__main__":
    main()
