#!/usr/bin/env python3
"""Execute UG2 eligibility and payout bytes with synthetic single-entry state.

No gameplay algorithm is implemented here. Multi-entry and drift helper paths
are outside this probe; unexpected calls fail instead of being stubbed.
"""
import hashlib
import itertools
import json
import struct
import pefile
from unicorn import Uc, UC_ARCH_X86, UC_MODE_32, UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EAX, UC_X86_REG_ECX, UC_X86_REG_ESP, UC_X86_REG_EIP, UC_X86_REG_EBX, UC_X86_REG_ESI, UC_X86_REG_EDI, UC_X86_REG_EBP
from probe_pc_race_reward import ROOT, EXE, ENTRY, END, EXPECTED_HASH

PREDICATE, PREDICATE_END = 0x5287a0, 0x528980
PREDICATE_HASH = "1decbbe918562e1f97a99c0fe5eca9d3d295804813051d4d201ac64d09ab4423"


def main(replacements=None):
    raw = EXE.read_bytes()
    pe = pefile.PE(data=raw)
    image, base = pe.get_memory_mapped_image(), pe.OPTIONAL_HEADER.ImageBase
    for start, end, digest in ((ENTRY, END, EXPECTED_HASH),
                               (PREDICATE, PREDICATE_END, PREDICATE_HASH),
                               (0x56dd90, 0x56ddc6, "a959195c6e92fd576592b0702f9d7369971943adc3e35f1d5307363c3140f75a"),
                               (0x502fd0, 0x502fd3, "58367ffa2a0179375018fa0f5c26da24391e42ebe0ed8fdda35a21fc7bdc396f")):
        if base != 0x400000 or hashlib.sha256(image[start-base:end-base]).hexdigest() != digest:
            raise SystemExit("Unsupported routine fingerprint")
    uc = Uc(UC_ARCH_X86, UC_MODE_32)
    uc.mem_map(base, (len(image)+4095) & ~4095)
    uc.mem_write(base, image)
    for address, body in (replacements or {}).items():
        uc.mem_write(address, body)
    uc.mem_map(0x2000000, 0x40000)
    career, race, sponsor, detail = 0x2000000, 0x2010000, 0x2011000, 0x2012000
    stack, stop = 0x203f000, 0x203ff00
    result_state, block = career+0xd24, career+0xd24+0x54
    allowed = ((ENTRY, END), (PREDICATE, PREDICATE_END), (0x56dd90, 0x56ddc6), (0x502fd0, 0x502fd3))

    def guard(machine, address, size, context):
        if not any(start <= address < end for start, end in allowed):
            raise RuntimeError(f"Unexpected instruction/helper at {address:#x}")

    uc.hook_add(UC_HOOK_CODE, guard)

    def w32(address, value):
        uc.mem_write(address, struct.pack("<I", value & 0xffffffff))

    def run(entry, receiver, argument=None):
        w32(stack, stop)
        if argument is not None:
            w32(stack+4, argument)
        uc.reg_write(UC_X86_REG_ESP, stack)
        uc.reg_write(UC_X86_REG_ECX, receiver)
        saved = (UC_X86_REG_EBX, UC_X86_REG_ESI, UC_X86_REG_EDI, UC_X86_REG_EBP)
        for reg in saved:
            uc.reg_write(reg, 0x12345678)
        uc.emu_start(entry, stop, count=1000)
        assert uc.reg_read(UC_X86_REG_EIP) == stop, "Instruction limit exhausted"
        assert uc.reg_read(UC_X86_REG_ESP) == stack + (8 if argument is not None else 4)
        assert all(uc.reg_read(reg) == 0x12345678 for reg in saved)
        return uc.reg_read(UC_X86_REG_EAX)

    w32(0x863470, race)
    uc.mem_write(race+0xf, b"\x00")  # excludes unresolved drift helper
    w32(career+0x8730, race)
    w32(career+0x7af8, 0)
    w32(career+0x73c0, sponsor)
    w32(race+0x30, 1250)
    uc.mem_write(sponsor+2, struct.pack("<H", 2500))
    cases = []
    for override, count, status, classification, first_index, detail_count, pointer_on, detail_flag in itertools.product(
            (0, 1), (0, 1), (0, 3, 4), (0, 1, 2, 11, 12), (0, 1),
            (2,), (0, 1), (0, 1)):
        uc.mem_write(result_state, bytes([override]))
        w32(result_state+0x34e8, count)
        w32(block+0x14, status)
        # +0xd20 is also the nested record count (+0x30 +0xcf0).
        # Keep it positive so the original's record search has a valid target.
        w32(block+0x30+0xcf0, detail_count)
        for index in (0, 1):
            record = block+0x30+index*0x224
            w32(record+4, 0 if index == first_index else 1)
            w32(record+8, classification)
        w32(block+0xd20, detail_count)
        w32(block+0xd08, detail if pointer_on else 0)
        w32(detail+4, detail_flag)
        observations = []
        for argument in (0, 1):
            try:
                observed = run(PREDICATE, result_state, argument) & 255
            except Exception as error:
                raise RuntimeError(f"State {(override, count, status, classification, first_index, detail_count, pointer_on, detail_flag, argument)}, EIP={uc.reg_read(UC_X86_REG_EIP):#x}") from error
            expected = bool(override or (count == 0) or (
                status in ((3, 4) if argument == 0 else (4,)) and
                classification in (1, 11) and detail_count > 0 and pointer_on and not detail_flag))
            assert observed == int(expected)
            observations.append(observed)
        for reward_type in (0, 1):
            uc.mem_write(race+0x34, bytes([reward_type]))
            returned = run(ENTRY, career)
            assert returned == ((2500 if reward_type == 1 else 1250) if observations[0] else 0)
        cases.append(dict(override=override, entry_count=count, status=status,
                          classification=classification, first_zero_index=first_index,
                          detail_count=detail_count, detail_pointer=pointer_on,
                          detail_flag=detail_flag, predicate_low_bytes=observations))
    drift_cases = []
    sentinel, list_nodes = 0x864630, 0x2020000
    uc.mem_write(result_state, b"\x00")
    w32(result_state+0x34e8, 1)
    w32(block+0x14, 4)
    w32(block+0xd20, 2)
    w32(block+0x34, 0)
    w32(block+0x38, 1)
    # The drift branch bypasses the non-drift detail predicate.
    w32(block+0xd08, 0)
    uc.mem_write(race+0xf, b"\x05")
    for identifiers in ([], [0], [1], [0, 1], [1, 0], [1, 2, 0], [0, 0], [2, 1]):
        w32(sentinel, list_nodes if identifiers else sentinel)
        w32(sentinel+4, list_nodes+(len(identifiers)-1)*16 if identifiers else sentinel)
        for i, identifier in enumerate(identifiers):
            w32(list_nodes+i*16, list_nodes+(i+1)*16 if i+1 < len(identifiers) else sentinel)
            w32(list_nodes+i*16+8, identifier)
        expected = int(bool(identifiers) and identifiers[0] == 0)
        for argument in (0, 1):
            assert run(PREDICATE, result_state, argument) & 255 == expected
        for reward_type in (0, 1):
            uc.mem_write(race+0x34, bytes([reward_type]))
            assert run(ENTRY, career) == ((2500 if reward_type == 1 else 1250) if expected else 0)
        drift_cases.append(dict(identifiers=identifiers, eligible=expected))
    report = dict(executable_sha256=hashlib.sha256(raw).hexdigest(),
                  predicate_sha256=PREDICATE_HASH, reward_sha256=EXPECTED_HASH,
                  state_combinations=len(cases), predicate_executions=len(cases)*2,
                  integrated_payout_executions=len(cases)*2, stubbed_callees=[],
                  drift_predicate_executions=len(drift_cases)*2,
                  drift_integrated_payout_executions=len(drift_cases)*2,
                  rebuilt_addresses=[hex(address) for address in (replacements or {})],
                  limitations=["Synthetic states, not retail traces", "Zero/single entry only",
                               "Drift list construction/scoring not recovered", "Field meanings remain partly unidentified"],
                  drift_cases=drift_cases, cases=cases)
    output = ROOT / "build/analysis/racing" / ("rebuilt-eligibility-probe.json" if replacements else "eligibility-probe.json")
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(json.dumps(report, indent=2)+"\n")
    print(json.dumps({k: v for k, v in report.items() if k != "cases"}, indent=2))


if __name__ == "__main__":
    main()
