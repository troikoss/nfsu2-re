#!/usr/bin/env python3
"""Classify UG2 PC v1.2 C++ static initializers that write vehicle-physics data.

Analysis-only. Reads the local executable, walks the CRT initializer table and
recognizes only simple instruction patterns (immediate stores, float copies and
calls to the two-field pair-curve constructor 0x576770). Anything else is reported
as complex rather than guessed. Output is written to ignored build/analysis.
"""
import json
import struct
import sys

import capstone
import pefile

from probe_pc_race_reward import EXE, ROOT

TABLE = 0x7e8264
PAIR_CURVE_CONSTRUCTOR = 0x576770
RANGES = ((0x7fbe00, 0x7fd000), (0x800b00, 0x803000), (0x865000, 0x866000),
          (0x86b000, 0x86c000), (0x890000, 0x891000), (0x89c000, 0x89f000))


def interesting(address):
    return any(low <= address < high for low, high in RANGES)


def analyse(image, base, entry):
    md = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_32)
    md.detail = True
    registers, pushes, stores, complex_reason = {}, [], [], None
    code = image[entry - base:entry - base + 0x200]
    for insn in md.disasm(code, entry):
        m, ops = insn.mnemonic, insn.operands
        if m == 'ret':
            break
        if m == 'nop':
            continue
        if m == 'mov' and ops[0].type == capstone.x86.X86_OP_MEM and ops[0].mem.base == 0 and ops[0].mem.index == 0:
            target = ops[0].mem.disp & 0xffffffff
            if ops[1].type == capstone.x86.X86_OP_IMM:
                stores.append((target, ops[1].imm & 0xffffffff))
                continue
            if ops[1].type == capstone.x86.X86_OP_REG and ops[1].reg in registers:
                stores.append((target, registers[ops[1].reg]))
                continue
        if m == 'mov' and ops[0].type == capstone.x86.X86_OP_REG and ops[1].type == capstone.x86.X86_OP_IMM:
            registers[ops[0].reg] = ops[1].imm & 0xffffffff
            continue
        if m == 'xor' and ops[0].type == capstone.x86.X86_OP_REG and ops[1].type == capstone.x86.X86_OP_REG \
                and ops[0].reg == ops[1].reg:
            registers[ops[0].reg] = 0
            continue
        if m == 'or' and ops[0].type == capstone.x86.X86_OP_REG and ops[1].type == capstone.x86.X86_OP_IMM \
                and ops[1].imm & 0xffffffff == 0xffffffff:
            registers[ops[0].reg] = 0xffffffff
            continue
        if m == 'push' and ops[0].type == capstone.x86.X86_OP_IMM:
            pushes.append(ops[0].imm & 0xffffffff)
            continue
        if m == 'call' and ops[0].type == capstone.x86.X86_OP_IMM and ops[0].imm == PAIR_CURVE_CONSTRUCTOR \
                and capstone.x86.X86_REG_ECX in registers and len(pushes) >= 2:
            this = registers[capstone.x86.X86_REG_ECX]
            stores.append((this, pushes[-1]))
            stores.append((this + 4, pushes[-2]))
            pushes = pushes[:-2]
            continue
        complex_reason = f'{insn.address:#x} {m} {insn.op_str}'
        break
    return stores, complex_reason


def main():
    pe = pefile.PE(str(EXE))
    image, base = pe.get_memory_mapped_image(), pe.OPTIONAL_HEADER.ImageBase
    entries, offset = [], TABLE
    while True:
        value = struct.unpack_from('<I', image, offset - base)[0]
        if value and not 0x401000 <= value < 0x783000:
            break
        if value:
            entries.append(value)
        offset += 4
    report = []
    for entry in entries:
        stores, reason = analyse(image, base, entry)
        hits = [s for s in stores if interesting(s[0])]
        if hits or (reason and any(interesting(t) for t, _ in stores)):
            report.append(dict(entry=hex(entry), stores=[(hex(a), hex(v)) for a, v in stores],
                               complex=reason))
        elif reason:
            report.append(dict(entry=hex(entry), stores=[], complex=reason, unrelated=True))
    out = ROOT / 'build/analysis/racing/static-initializers.json'
    out.write_text(json.dumps(dict(table=hex(TABLE), count=len(entries), initializers=report), indent=1) + '\n')
    relevant = [r for r in report if not r.get('unrelated')]
    print(len(entries), 'initializers;', len(relevant), 'write physics ranges;',
          sum(1 for r in report if r.get('unrelated')), 'complex/unclassified elsewhere')
    for r in relevant:
        print(r['entry'], r['complex'] or '', r['stores'])


if __name__ == '__main__':
    sys.exit(main())
