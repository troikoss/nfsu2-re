#!/usr/bin/env python3
"""Original/rebuilt colour channels, state writes and scroller lock-script policy."""
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

BUILD = ROOT / 'build/analysis/feng-colour-scripts'
FUNCTIONS = {'Pack': (0x5054d0, 142), 'FEngCopyColour': (0x50ccc0, 60), 'FEngGetColour': (0x495570, 32),
             'FEngColourNear': (0x504640, 98), 'SetColourState': (0x517ad0, 307),
             'FEngSetColour': (0x51d7f0, 111), 'FngObjectScriptActive': (0x51d040, 47),
             'UpdateIconLockScripts': (0x51df00, 182)}
ARENA, STACK, STOP = 0x2000000, 0x200f000, 0x2100000
REGISTERS = (UC_X86_REG_EBX, UC_X86_REG_ESI, UC_X86_REG_EDI, UC_X86_REG_EBP)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--colour-source', default='src/Speed/Indep/Src/Frontend/FEngColour.cpp')
    parser.add_argument('--script-source', default='src/Speed/Indep/Src/Frontend/IconScrollerScripts.cpp')
    args = parser.parse_args()
    raw = EXE.read_bytes()
    assert hashlib.sha256(raw).hexdigest() == 'f9dd86c054878ce6276beb07c1fd61874f7a1e4bf1f241b084c65b73e24168a7'
    image = pefile.PE(data=raw).get_memory_mapped_image()
    BUILD.mkdir(parents=True, exist_ok=True)
    objects = []
    for i, source in enumerate((args.colour_source, args.script_source)):
        obj = BUILD / ('colour-script-%d.obj' % i)
        subprocess.run([str(ROOT / 'build/tools/wibo'), 'build/compilers/Win32/7.1/cl.exe', '/nologo', '/c',
                        '/Ox', '/Ob0', '/Z7', '/EHsc', '/Fo' + str(obj.relative_to(ROOT)),
                        '/Isrc/Speed/Indep/Src/Frontend', os.path.relpath(source, ROOT)], cwd=ROOT, check=True)
        objects.append(obj)
    symbols = [s for o in objects for s in read_object(o)['symbols'].values()]
    defined = {s['name'] for s in symbols if s['section'] > 0}
    targets = {'fengStateKeys7992F4': 0x7992f4, 'FEngStateOffset': 0x5046f0,
               'FngFormatHash': 0x50b9c0, 'FngSetObjectScript': 0x51cf70}
    externals = {s['name']: targets[s['name'][1:].split('@')[0]] for s in symbols
                 if s['section'] == 0 and s['name'] not in defined and s['name'].startswith('?')}
    linked = link_objects(objects, externals)
    entries = {name: next(a for n, a in linked['exports'].items() if n.startswith('?' + name + '@')) for name in FUNCTIONS}
    state, machines = {}, []
    for rebuilt in (False, True):
        uc = Uc(UC_ARCH_X86, UC_MODE_32)
        uc.mem_map(0x400000, (len(image) + 4095) & ~4095); uc.mem_write(0x400000, image)
        uc.mem_map(ARENA, 0x10000); uc.mem_map(STOP, 0x1000)
        uc.mem_map(linked['address'], linked['allocation'])
        for address, body in linked['blocks']: uc.mem_write(address, body)
        ranges = linked['ranges'] if rebuilt else [(a, a + n) for a, n in FUNCTIONS.values()]
        ranges = ranges + [(0x5046f0, 0x504755)]

        def string(uc, address):
            out = bytearray()
            while (c := uc.mem_read(address + len(out), 1)[0]): out.append(c)
            return out.decode('ascii')

        def block(uc, address, size, user, ranges=ranges):
            if address == STOP: return
            if address in (0x50b9c0, 0x51cf70):
                sp = uc.reg_read(UC_X86_REG_ESP)
                argc = 1 if address == 0x50b9c0 else 3
                words = struct.unpack('<' + 'I' * (argc + 1), uc.mem_read(sp, (argc + 1) * 4))
                if argc == 1:
                    name = string(uc, words[1]); state['trace'].append(('Hash', name))
                    uc.reg_write(UC_X86_REG_EAX, state['hashes'].get(name, state['hash']))
                else:
                    state['trace'].append(('Script', words[1], string(uc, words[2]), words[3] & 255))
                uc.reg_write(UC_X86_REG_ESP, sp + 4); uc.reg_write(UC_X86_REG_EIP, words[0]); return
            assert any(a <= address < b for a, b in ranges), ('unexpected execution', hex(address))
        uc.hook_add(UC_HOOK_BLOCK, block); machines.append(uc)

    rng = random.Random(0x517ad0)
    counts = dict.fromkeys(FUNCTIONS, 0)
    object_, data, value, icon, script, lock, button = [ARENA + n for n in (0x100, 0x400, 0x600, 0x800, 0xa00, 0xb00, 0xc00)]
    keys = struct.unpack('<11i', image[0x7992f4-0x400000:0x7992f4-0x400000+44])
    channel_values = [-0x80000000, -257, -1, 0, 1, 254, 255, 256, 257, 0x7fffffff]

    def compare(name, memory, args=(), this=0, cleanup=0, mask=0):
        outputs = []
        for rebuilt, uc in zip((False, True), machines):
            uc.mem_write(ARENA, bytes(memory)); uc.mem_write(STACK, struct.pack('<I', STOP) + struct.pack('<'+'I'*len(args), *[a & 0xffffffff for a in args]))
            uc.reg_write(UC_X86_REG_ESP, STACK); uc.reg_write(UC_X86_REG_ECX, this)
            for reg in REGISTERS: uc.reg_write(reg, 0x12345678)
            state.update(trace=[], hashes={'FadeIn': 123, 'Animate': 456}, hash=789)
            uc.emu_start(entries[name] if rebuilt else FUNCTIONS[name][0], STOP, count=30000)
            assert uc.reg_read(UC_X86_REG_EIP) == STOP
            assert uc.reg_read(UC_X86_REG_ESP) == STACK + 4 + cleanup, (name, rebuilt)
            assert all(uc.reg_read(r) == 0x12345678 for r in REGISTERS)
            outputs.append((uc.reg_read(UC_X86_REG_EAX) & mask, state['trace'], bytes(uc.mem_read(ARENA, 0xe000))))
        if outputs[0] != outputs[1]:
            diff = [(hex(i), outputs[0][2][i], outputs[1][2][i]) for i in range(0xe000) if outputs[0][2][i] != outputs[1][2][i]][:12]
            print('difference', name, args, diff, outputs[0][:2], outputs[1][:2])
        assert outputs[0] == outputs[1], (name, args)
        counts[name] += 1

    for trial in range(4000):
        memory = bytearray(rng.randbytes(0x10000)); struct.pack_into('<I', memory, 0x12c, data)
        channels = [rng.choice(channel_values + [rng.randrange(-1000, 1000)]) for _ in range(4)]
        struct.pack_into('<4i', memory, 0x400, *channels)
        compare('Pack', memory, this=data, mask=0xffffffff)
        current = rng.choice((0, object_))
        compare('FEngCopyColour', memory, [value, current])
        compare('FEngGetColour', memory, [current], mask=0xffffffff)
        other = [(c + rng.choice((-2, -1, 0, 1, 2))) & 0xffffffff for c in channels]
        struct.pack_into('<4I', memory, 0x600, *other)
        compare('FEngColourNear', memory, [value, data], mask=0xff)
        scripts = rng.randrange(4); struct.pack_into('<I', memory, 0x14c, ARENA + 0x2000 if scripts else 0)
        for j in range(scripts):
            off = 0x2000 + j * 0x100; tracks, count = ARENA + 0x3000 + j * 0x200, rng.randrange(6)
            struct.pack_into('<I', memory, off + 4, ARENA + off + 0x100 if j + 1 < scripts else 0)
            struct.pack_into('<II', memory, off + 0x1c, count, tracks)
            for k in range(count): memory[tracks - ARENA + k * 0x38 + 7] = rng.choice(keys + (-1, -128, 127)) & 255
        key = rng.randrange(11)
        compare('SetColourState', memory, [key, value, rng.choice((0, 1, 2, 0x80))], object_, 12)
        struct.pack_into('<i', memory, 0x118, rng.choice((-1, 1, 2, 5, 0xff, 0x100, 0x101)))
        compare('FEngSetColour', memory, [current, rng.getrandbits(32)])
        # Alias and repeated-match cases use observed load/store order.
        if trial % 20 == 0:
            compare('SetColourState', memory, [0, data, 1], object_, 12)
    for trial in range(2500):
        memory = bytearray(0x10000); memory[0xd00:0xd08] = b'Animate\0'
        for off in (0x854, 0xb54, 0xc54): struct.pack_into('<I', memory, off, rng.choice((0, script)))
        struct.pack_into('<i', memory, 0xa0c, rng.choice((0, 100, 200)))
        struct.pack_into('<i', memory, 0xa10, rng.choice((-1, 0, 99, 100, 101, 201)))
        struct.pack_into('<I', memory, 0xa30, rng.choice((123, 456, 789, 0)))
        memory[0x822], memory[0x823] = rng.choice((0, 1, 0x80)), rng.choice((0, 1, 0x80))
        compare('FngObjectScriptActive', memory, [rng.choice((0, button)), ARENA + 0xd00], mask=0xff)
        compare('UpdateIconLockScripts', memory, [icon, rng.choice((0, lock)), rng.choice((0, button))])
    report = {'comparisons': counts, 'total': sum(counts.values()), 'byte_matching': 'not established',
              'controlled_interfaces': ['formatted hash 0x50b9c0', 'script activation 0x51cf70'],
              'retained_dependency': 'previously verified state offset 0x5046f0',
              'fingerprints': {n: {'address': hex(a), 'size': size, 'sha256': hashlib.sha256(image[a-0x400000:a-0x400000+size]).hexdigest()}
                               for n,(a,size) in FUNCTIONS.items()}}
    (BUILD / 'verification.json').write_text(json.dumps(report, indent=2)+'\n'); print(json.dumps(report, indent=2))


if __name__ == '__main__': main()
