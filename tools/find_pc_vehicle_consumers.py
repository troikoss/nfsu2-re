#!/usr/bin/env python3
"""Locate UG2 instructions referencing candidate vehicle fields or named strings.

This produces leads, not semantic identification. Identical displacements occur
in unrelated structures; only subsequent pointer/data-flow tracing can confirm
a vehicle-data consumer. No executable or asset is modified.
"""
import hashlib
import json
from capstone import Cs, CS_ARCH_X86, CS_MODE_32
from capstone.x86_const import X86_OP_MEM, X86_OP_IMM, X86_REG_ESP, X86_REG_EBP
import pefile
from probe_pc_race_reward import ROOT, EXE

FIELDS = {0x220, 0x224, 0x228, 0x22c, 0x230, 0x244, 0x258, 0x2c0,
          0x300, 0x304, 0x308, *range(0x310, 0x334, 4), 0x380, 0x460,
          0x4a0, 0x4e0, 0x530, 0x570, 0x5b0, 0x5f0}
ANCHORS = (b"CAR_ENGINE_TORQUE", b"SEQ_CAR_TORQUE", b"PhysicsMover",
           b"TORQUE_DATA_%d", b"MAX_TORQUE_DATA", b"Nitrous", b"PPOTransmission")


def main():
    data = EXE.read_bytes()
    pe = pefile.PE(data=data)
    image, base = pe.get_memory_mapped_image(), pe.OPTIONAL_HEADER.ImageBase
    if hashlib.sha256(data).hexdigest() != "f9dd86c054878ce6276beb07c1fd61874f7a1e4bf1f241b084c65b73e24168a7":
        raise SystemExit("Unsupported executable fingerprint")
    index = json.loads((ROOT/"build/analysis/function-index.json").read_text())
    # The seed index stores metadata followed by the independently hashed ranges.
    functions = index["functions"]
    anchor_addresses = {}
    for label in ANCHORS:
        start = 0
        while True:
            offset = data.find(label+b"\0", start)
            if offset < 0:
                break
            if offset == 0 or data[offset-1] == 0:
                anchor_addresses[base+pe.get_rva_from_offset(offset)] = label.decode("ascii")
            start = offset+1
    disassembler = Cs(CS_ARCH_X86, CS_MODE_32)
    disassembler.detail = True
    candidates = []
    for function in functions:
        address, end = int(function["start"], 16), int(function["end"], 16)
        body = image[address-base:end-base]
        if hashlib.sha256(body).hexdigest() != function["sha256"]:
            raise ValueError(f"Stale function seed at {address:#x}")
        hits, offsets = [], set()
        for instruction in disassembler.disasm(body, address):
            field_hits, string_hits = set(), set()
            for operand in instruction.operands:
                if (operand.type == X86_OP_MEM and operand.mem.disp in FIELDS
                        and operand.mem.base not in (0, X86_REG_ESP, X86_REG_EBP)):
                    field_hits.add(operand.mem.disp)
                if operand.type == X86_OP_IMM and operand.imm in anchor_addresses:
                    string_hits.add(anchor_addresses[operand.imm])
                if operand.type == X86_OP_MEM and operand.mem.disp in anchor_addresses:
                    string_hits.add(anchor_addresses[operand.mem.disp])
            if field_hits or string_hits:
                offsets.update(field_hits)
                hits.append(dict(address=hex(instruction.address), mnemonic=instruction.mnemonic,
                                 operands=instruction.op_str, field_offsets=[hex(x) for x in sorted(field_hits)],
                                 string_anchors=sorted(string_hits)))
        if len(offsets) >= 3 or any(hit["string_anchors"] for hit in hits):
            candidates.append(dict(**function, distinct_offsets=len(offsets), hits=hits))
    candidates.sort(key=lambda item: (-sum(bool(hit["string_anchors"]) for hit in item["hits"]),
                                      -item["distinct_offsets"], int(item["start"], 16)))
    report = dict(executable_sha256=hashlib.sha256(data).hexdigest(),
                  limitation="Candidate offsets/strings only; pointer provenance and mechanics unresolved",
                  seeded_ranges_scanned=len(functions), candidates=candidates,
                  anchor_addresses={hex(address): name for address, name in anchor_addresses.items()})
    output = ROOT / "build/analysis/racing/vehicle-consumer-candidates.json"
    output.write_text(json.dumps(report, indent=2)+"\n")
    print(json.dumps(dict(seeded_ranges_scanned=len(functions), candidate_count=len(candidates),
                         anchor_addresses=report["anchor_addresses"],
                         top_candidates=[{key: item[key] for key in ("start", "end", "name", "distinct_offsets")}
                                         for item in candidates[:12]]), indent=2))


if __name__ == "__main__":
    main()
