#!/usr/bin/env python3
"""Execute original/rebuilt FEng coordinates, text metrics and script baseline writes."""
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
                              UC_X86_REG_EBX, UC_X86_REG_ESI, UC_X86_REG_EDI, UC_X86_REG_EBP,
                              UC_X86_REG_FPCW, UC_X86_REG_FPSW, UC_X86_REG_FPTAG)
from probe_pc_race_reward import ROOT, EXE
from pc_coff import link_objects, read_object

BUILD = ROOT / 'build/analysis/feng-coordinates'
FUNCTIONS = {'FindFEngGlyph': (0x6f45b0, 61), 'TextWidth': (0x50bec0, 118), 'TextHeight': (0x50bf40, 223),
             'FEngHorizontalOffset': (0x50c020, 43), 'FEngVerticalOffset': (0x50c050, 43),
             'FEngVectorNear': (0x504500, 158), 'FEngStateOffset': (0x5046f0, 101),
             'SetVectorState': (0x5178d0, 254), 'FEngGetTopLeft': (0x51d070, 225),
             'FEngSetTopLeft': (0x51d380, 307), 'FEngSetCentre': (0x51d4d0, 304), 'FEngSetSize': (0x50cb00, 63), 'FEngGetCentre': (0x51d620, 235),
             'FEngGetSize': (0x51d730, 155), 'FEngSetWidth': (0x495590, 48), 'FEngSetHeight': (0x525db0, 86)}
GLOBALS = {'fengEpsilon7992EC': 0x7992ec, 'fengStateKeys7992F4': 0x7992f4}
INTERFACES = {'FindFEngFont': 0x51bd70, 'LineWidth': 0x50bce0}
ARENA, STACK, STOP = 0x2000000, 0x200f000, 0x2100000
WIDTH_RETURN, FLOAT_RESULT = ARENA + 0xd010, ARENA + 0xd000
FLOAT_STOP, WIDTH_STUB = STOP + 0x40, STOP + 0x80
REGISTERS = (UC_X86_REG_EBX, UC_X86_REG_ESI, UC_X86_REG_EDI, UC_X86_REG_EBP)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source', default='src/Speed/Indep/Src/Frontend/FEngCoordinates.cpp')
    args = parser.parse_args()
    raw = EXE.read_bytes()
    assert hashlib.sha256(raw).hexdigest() == 'f9dd86c054878ce6276beb07c1fd61874f7a1e4bf1f241b084c65b73e24168a7'
    image = pefile.PE(data=raw).get_memory_mapped_image()
    BUILD.mkdir(parents=True, exist_ok=True)
    obj = BUILD / 'FEngCoordinates.obj'
    subprocess.run([str(ROOT / 'build/tools/wibo'), 'build/compilers/Win32/7.1/cl.exe', '/nologo', '/c',
                    '/Ox', '/Ob0', '/Z7', '/EHsc', '/Fo' + str(obj.relative_to(ROOT)),
                    '/Isrc/Speed/Indep/Src/Frontend', os.path.relpath(args.source, ROOT)], cwd=ROOT, check=True)
    externals = {}
    for s in read_object(obj)['symbols'].values():
        if s['section'] == 0 and s['name'].startswith('?'):
            short = s['name'][1:].split('@')[0]
            externals[s['name']] = GLOBALS[short] if short in GLOBALS else INTERFACES[short]
    linked = link_objects([obj], externals)
    entries = {name: next(a for n, a in linked['exports'].items() if n.startswith('?' + name + '@'))
               for name in FUNCTIONS}
    state, machines = {}, []
    for rebuilt in (False, True):
        uc = Uc(UC_ARCH_X86, UC_MODE_32)
        uc.mem_map(0x400000, (len(image) + 4095) & ~4095)
        uc.mem_write(0x400000, image)
        uc.mem_map(ARENA, 0x10000)
        uc.mem_map(STOP, 0x1000)
        uc.mem_write(FLOAT_STOP, b'\xd9\x1d' + struct.pack('<I', FLOAT_RESULT) + b'\xe9' + struct.pack('<i', STOP - (FLOAT_STOP + 11)))
        uc.mem_write(WIDTH_STUB, b'\xd9\x05' + struct.pack('<I', WIDTH_RETURN) + b'\xc2\x10\x00')
        uc.mem_map(linked['address'], linked['allocation'])
        for address, body in linked['blocks']:
            uc.mem_write(address, body)
        ranges = linked['ranges'] if rebuilt else [(a, a + n) for a, n in FUNCTIONS.values()]

        def block(uc, address, size, user, ranges=ranges):
            if address == STOP:
                return
            if address in (0x51bd70, 0x50bce0):
                argc = 1 if address == 0x51bd70 else 4
                sp = uc.reg_read(UC_X86_REG_ESP)
                words = struct.unpack('<' + 'I' * (argc + 1), uc.mem_read(sp, (argc + 1) * 4))
                state['trace'].append((address, uc.reg_read(UC_X86_REG_ECX) if argc == 4 else 0, words[1:]))
                if address == 0x51bd70:
                    uc.reg_write(UC_X86_REG_EAX, state['font'])
                    uc.reg_write(UC_X86_REG_ESP, sp + 4)
                    uc.reg_write(UC_X86_REG_EIP, words[0])
                else:
                    # Script the low-level line-measurement result while retaining x87 ABI/cleanup.
                    index = (words[1] - state['text']) // 2
                    width = state['widths'][index % len(state['widths'])]
                    uc.mem_write(WIDTH_RETURN, struct.pack('<f', width))
                    uc.reg_write(UC_X86_REG_EIP, WIDTH_STUB)
                return
            assert address in (FLOAT_STOP, WIDTH_STUB) or any(a <= address < b for a, b in ranges), ('unexpected execution', hex(address))
        uc.hook_add(UC_HOOK_BLOCK, block)
        machines.append(uc)

    rng = random.Random(0x51d4d0)
    counts = dict.fromkeys(FUNCTIONS, 0)
    object_, data, text, font, fntf = [ARENA + n for n in (0x100, 0x400, 0x800, 0x1000, 0x1200)]
    keys = struct.unpack('<11i', image[0x7992f4-0x400000:0x7992f4-0x400000+44])

    def compare(name, memory, args, this=0, cleanup=0, mask=0, floating=False, font_found=0, precision=0x027f):
        outputs = []
        for rebuilt, uc in zip((False, True), machines):
            uc.mem_write(ARENA, bytes(memory))
            uc.mem_write(STACK, struct.pack('<I', FLOAT_STOP if floating else STOP) + args)
            uc.reg_write(UC_X86_REG_ESP, STACK)
            uc.reg_write(UC_X86_REG_ECX, this)
            uc.reg_write(UC_X86_REG_FPCW, precision)
            uc.reg_write(UC_X86_REG_FPSW, 0)
            uc.reg_write(UC_X86_REG_FPTAG, 0xffff)
            for reg in REGISTERS:
                uc.reg_write(reg, 0x12345678)
            state.update(trace=[], font=font_found, text=text, widths=[0.0, 0.5, 12.25, 3.125, 7.75])
            uc.emu_start(entries[name] if rebuilt else FUNCTIONS[name][0], STOP, count=50000)
            assert uc.reg_read(UC_X86_REG_EIP) == STOP, (name, args.hex())
            assert uc.reg_read(UC_X86_REG_ESP) == STACK + 4 + cleanup, (name, rebuilt, hex(uc.reg_read(UC_X86_REG_ESP)))
            assert uc.reg_read(UC_X86_REG_FPCW) == precision
            assert all(uc.reg_read(r) == 0x12345678 for r in REGISTERS)
            outputs.append((uc.reg_read(UC_X86_REG_EAX) & mask, state['trace'], bytes(uc.mem_read(ARENA, 0xe000))))
        if outputs[0] != outputs[1]:
            diffs = [(hex(i), outputs[0][2][i], outputs[1][2][i]) for i in range(0xe000) if outputs[0][2][i] != outputs[1][2][i]][:16]
            print('difference', name, args.hex(), diffs, outputs[0][:2], outputs[1][:2])
        assert outputs[0] == outputs[1], (name, args.hex())
        counts[name] += 1

    def fixture():
        memory = bytearray(rng.randbytes(0x10000))
        struct.pack_into('<I', memory, 0x12c, data)
        for i in range(40):
            struct.pack_into('<f', memory, 0x400 + i * 4, rng.choice((0.0, -0.0, 1.0, -1.0, 0.5, rng.uniform(-1000, 1000))))
        struct.pack_into('<I', memory, 0x164, text)
        struct.pack_into('<III', memory, 0x16c, rng.randrange(256), rng.randrange(20), 0)
        string = rng.choice(('', 'A', 'ABC', '^', 'A^', 'A^B', 'A\nB\rC', '\u0120\u010d\u01ff', '^\r^A', 'AAA^^'))
        struct.pack_into('<' + 'H' * (len(string) + 1), memory, 0x800, *[ord(c) for c in string], 0)
        struct.pack_into('<I', memory, 0x100c, fntf)
        struct.pack_into('<f', memory, 0x1024, rng.choice((0.0, 12.0, 21.5, 48.0)))
        struct.pack_into('<H', memory, 0x120a, 96)
        stride = rng.choice((12, 16))
        struct.pack_into('<II', memory, 0x120c, 0x40000 if stride == 16 else 0, 0)
        struct.pack_into('<I', memory, 0x1214, 0x400)
        memory[0x1400:0x2000] = bytes(0xc00)
        for i in range(96):
            struct.pack_into('<H', memory, 0x1600 + i * stride, 0x20 + i)
        scripts = rng.randrange(4)
        struct.pack_into('<I', memory, 0x14c, ARENA + 0x3000 if scripts else 0)
        for j in range(scripts):
            offset = 0x3000 + j * 0x100
            tracks, count = ARENA + 0x4000 + j * 0x200, rng.randrange(6)
            struct.pack_into('<I', memory, offset + 4, ARENA + offset + 0x100 if j + 1 < scripts else 0)
            struct.pack_into('<II', memory, offset + 0x1c, count, tracks)
            for k in range(count):
                off = tracks - ARENA + k * 0x38
                memory[off + 7] = rng.choice(keys + (-1, -128, 127)) & 0xff
                for c in range(4):
                    struct.pack_into('<f', memory, off + 0x18 + c * 4, rng.uniform(-10, 10))
        return memory

    for trial in range(2000):
        memory = fixture()
        precision = rng.choice((0x027f, 0x037f))
        kind = rng.choice((-1, 0, 1, 2, 3, 5, 7, 9, 10, 0xff, 0x100, 0x101))
        struct.pack_into('<i', memory, 0x118, kind)
        found = rng.choice((0, font))
        current = rng.choice((0, object_))
        compare('FEngGetTopLeft', memory, struct.pack('<III', current, ARENA + 0x600, ARENA + 0x604), font_found=found, precision=precision)
        xy = struct.pack('<ff', rng.uniform(-500, 500), rng.uniform(-500, 500))
        compare('FEngSetTopLeft', memory, struct.pack('<I', current) + xy, font_found=found, precision=precision)
        compare('FEngSetCentre', memory, struct.pack('<I', current) + xy, font_found=found, precision=precision)
        compare('FEngGetCentre', memory, struct.pack('<III', current, ARENA + 0x600, ARENA + 0x604), font_found=found, precision=precision)
        compare('FEngGetSize', memory, struct.pack('<III', current, ARENA + 0x600, ARENA + 0x604), font_found=found, precision=precision)
        # Width/height getters leave scratch uninitialised when no font is found; only defined input paths.
        if current and (struct.unpack_from('<i', memory, 0x118)[0] != 2 or found):
            for name in ('FEngSetWidth', 'FEngSetHeight'):
                compare(name, memory, struct.pack('<If', current, rng.uniform(-1000, 1000)), font_found=found, precision=precision)
        compare('FEngSetSize', memory, struct.pack('<I', current) + xy, precision=precision)
        compare('TextWidth', memory, struct.pack('<II', text, rng.choice((0, 0x20, 0xff))), font, 8, floating=True, precision=precision)
        compare('TextHeight', memory, struct.pack('<Ii', rng.choice((0, text)), rng.randrange(-4, 12)), font, 8, floating=True, precision=precision)
        compare('FindFEngGlyph', memory, struct.pack('<IIII', rng.randrange(256), fntf + 0x400, 96, 16 if struct.unpack_from('<I', memory, 0x120c)[0] else 12), mask=0xffffffff)
        compare('FEngStateOffset', memory, struct.pack('<i', rng.randrange(-2, 15)), cleanup=4, mask=0xffffffff)
        key = rng.randrange(11)
        compare('SetVectorState', memory, struct.pack('<III', key, data + 0x60, rng.choice((0, 1, 2, 0x80))), object_, 12, precision=precision)
        for name in ('FEngHorizontalOffset', 'FEngVerticalOffset'):
            compare(name, memory, struct.pack('<If', rng.randrange(256), rng.uniform(-1000, 1000)), cleanup=8, floating=True, precision=precision)
        other = rng.choice((data, data + 4, ARENA + 0x600))
        if trial % 3 == 0:
            memory[0x600:0x60c] = memory[0x400:0x40c]
            struct.pack_into('<f', memory, 0x604, struct.unpack_from('<f', memory, 0x404)[0] + rng.choice((0.0, 1e-6, -1e-6, 2e-6)))
        compare('FEngVectorNear', memory, struct.pack('<II', data, other), mask=0xff, precision=precision)
    # NaNs, infinities and epsilon edges are checked independently of font callbacks.
    for bits in (0, 0x80000000, 0x3f800000, 0xbf800000, 0x7f800000, 0xff800000, 0x7fc00000, 0x358637bd):
        for other_bits in (bits, 0, 0x7fc00000):
            memory = fixture()
            memory[0x400:0x40c] = struct.pack('<III', bits, bits, bits)
            memory[0x600:0x60c] = struct.pack('<III', other_bits, other_bits, other_bits)
            compare('FEngVectorNear', memory, struct.pack('<II', data, ARENA + 0x600), mask=0xff)
    report = {'comparisons': counts, 'total': sum(counts.values()), 'byte_matching': 'not established',
              'controlled_interfaces': ['font lookup (0x51bd70)', 'line measurement (0x50bce0)'],
              'x87_precision': ['53-bit', '64-bit'],
              'fingerprints': {n: {'address': hex(a), 'size': s, 'sha256': hashlib.sha256(image[a-0x400000:a-0x400000+s]).hexdigest()}
                               for n, (a, s) in FUNCTIONS.items()}}
    (BUILD / 'verification.json').write_text(json.dumps(report, indent=2) + '\n')
    print(json.dumps(report, indent=2))


if __name__ == '__main__':
    main()
