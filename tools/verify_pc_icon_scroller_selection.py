#!/usr/bin/env python3
"""Execute original/rebuilt scroller selection with real focus flag and queue routines."""
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

BUILD = ROOT / 'build/analysis/icon-scroller-selection'
FUNCTIONS = {'SelectIcon': (0x52f680, 306), 'SelectIndex': (0x52f7c0, 256), 'NodeAt': (0x43be60, 31)}
RETAINED = [(0x51f820, 0x51f867), (0x51cc60, 0x51ccc8), (0x52ce40, 0x52ce9e),
            (0x52b3a0, 0x52b42d)]
INTERFACES = {'FngFindPackage': 0x52cef0, 'FngHash': 0x505450,
              'FngFindRecord': 0x50b840, 'FindNode': 0x51bb70, 'Post': 0x51aa20}
ARENA, STACK, STOP = 0x2000000, 0x200f000, 0x2100000
REGISTERS = (UC_X86_REG_EBX, UC_X86_REG_ESI, UC_X86_REG_EDI, UC_X86_REG_EBP)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source', default='src/Speed/Indep/Src/Frontend/IconScrollerSelection.cpp')
    args = parser.parse_args()
    raw = EXE.read_bytes()
    assert hashlib.sha256(raw).hexdigest() == 'f9dd86c054878ce6276beb07c1fd61874f7a1e4bf1f241b084c65b73e24168a7'
    image = pefile.PE(data=raw).get_memory_mapped_image()
    BUILD.mkdir(parents=True, exist_ok=True)
    objects = []
    for i, source in enumerate([args.source, 'src/Speed/Indep/Src/Frontend/IconScrollerNavigation.cpp',
                                'src/Speed/Indep/Src/Frontend/FEngPackage.cpp']):
        obj = BUILD / ('selection-%d.obj' % i)
        subprocess.run([str(ROOT / 'build/tools/wibo'), 'build/compilers/Win32/7.1/cl.exe', '/nologo', '/c',
                        '/Ox', '/Ob0', '/Z7', '/EHsc', '/Fo' + str(obj.relative_to(ROOT)),
                        '/Isrc/Speed/Indep/Src/Frontend', os.path.relpath(source, ROOT)], cwd=ROOT, check=True)
        objects.append(obj)
    symbols = [s for o in objects for s in read_object(o)['symbols'].values()]
    defined = {s['name'] for s in symbols if s['section'] > 0}
    externals = {}
    for s in symbols:
        if s['section'] != 0 or s['name'] in defined or not s['name'].startswith('?'):
            continue
        name = s['name'][1:].split('@')[0]
        globals_ = {'fengManager8384C4': 0x8384c4, 'fengArchive8637B0': 0x8637b0}
        externals[s['name']] = globals_[name] if name in globals_ else INTERFACES[name]
    linked = link_objects(objects, externals)
    entries = {name: next(a for n, a in linked['exports'].items() if n.startswith('?' + name + '@'))
               for name in FUNCTIONS}
    state, machines = {}, []
    for rebuilt in (False, True):
        uc = Uc(UC_ARCH_X86, UC_MODE_32)
        uc.mem_map(0x400000, (len(image) + 4095) & ~4095)
        uc.mem_write(0x400000, image)
        uc.mem_map(ARENA, 0x10000)
        uc.mem_map(STOP, 0x1000)
        uc.mem_map(linked['address'], linked['allocation'])
        for address, body in linked['blocks']:
            uc.mem_write(address, body)
        ranges = linked['ranges'] if rebuilt else [(a, a + n) for a, n in FUNCTIONS.values()] + RETAINED

        def block(uc, address, size, user, ranges=ranges):
            if address == STOP:
                return
            if address in (0x51bb70, 0x51aa20):
                sp, this = uc.reg_read(UC_X86_REG_ESP), uc.reg_read(UC_X86_REG_ECX)
                argc = 1 if address == 0x51bb70 else 5
                words = struct.unpack('<' + 'I' * (argc + 1), uc.mem_read(sp, (argc + 1) * 4))
                state['trace'].append((address, this, words[1:]))
                if address == 0x51bb70:
                    uc.reg_write(UC_X86_REG_EAX, state['found'])
                else:
                    state['trace'].append(('focus', struct.unpack('<I', uc.mem_read(words[3] + 0x8c, 4))[0]))
                uc.reg_write(UC_X86_REG_ESP, sp + 4 + argc * 4)
                uc.reg_write(UC_X86_REG_EIP, words[0])
                return
            assert any(a <= address < b for a, b in ranges), ('unexpected execution', hex(address))
        uc.hook_add(UC_HOOK_BLOCK, block)
        machines.append(uc)

    rng = random.Random(0x52f680)
    scroller, manager, queue, node, package = [ARENA + n for n in (0x100, 0x400, 0x500, 0x600, 0x700)]
    icons = [ARENA + 0x2000 + i * 0x100 for i in range(8)]
    feobjects = [ARENA + 0x4000 + i * 0x100 for i in range(10)]
    counts = dict.fromkeys(FUNCTIONS, 0)

    def compare(name, memory, arg, manager_enabled=0, found=0, this=None):
        outputs = []
        for rebuilt, uc in zip((False, True), machines):
            uc.mem_write(ARENA, bytes(memory))
            uc.mem_write(0x8384c4, struct.pack('<I', manager if manager_enabled else 0))
            uc.mem_write(STACK, struct.pack('<II', STOP, arg & 0xffffffff))
            uc.reg_write(UC_X86_REG_ESP, STACK)
            uc.reg_write(UC_X86_REG_ECX, scroller if this is None else this)
            for reg in REGISTERS:
                uc.reg_write(reg, 0x12345678)
            state.update(trace=[], found=found)
            uc.emu_start(entries[name] if rebuilt else FUNCTIONS[name][0], STOP, count=30000)
            assert uc.reg_read(UC_X86_REG_EIP) == STOP
            assert uc.reg_read(UC_X86_REG_ESP) == STACK + 8
            assert all(uc.reg_read(r) == 0x12345678 for r in REGISTERS)
            outputs.append((uc.reg_read(UC_X86_REG_EAX) & (0xffffffff if name == 'NodeAt' else 0xff),
                            state['trace'], bytes(uc.mem_read(ARENA, 0xe000))))
        assert outputs[0] == outputs[1], (name, arg, outputs[0][:2], outputs[1][:2])
        counts[name] += 1

    for trial in range(4000):
        memory = bytearray(rng.randbytes(0x10000))
        count = rng.randrange(1, 9)
        sentinel = scroller + 4
        struct.pack_into('<IIII', memory, 0x100, 0, icons[0] + 4, icons[count - 1] + 4,
                         rng.choice([0] + icons[:count]))
        # Sentinel-as-icon is deliberately addressable for out-of-range NodeAt results.
        struct.pack_into('<I', memory, 0x110, feobjects[8])
        struct.pack_into('<I', memory, 0x120, 0)
        memory[0x130] = rng.choice((0, 1, 2, 0x80))
        struct.pack_into('<I', memory, 0x408, queue)
        struct.pack_into('<I', memory, 0x618, rng.choice((0, package)))
        struct.pack_into('<II', memory, 0x790, ARENA + 0x1000, rng.randrange(11))
        struct.pack_into('<10I', memory, 0x1000, *rng.sample(feobjects, 10))
        struct.pack_into('<I', memory, 0x78c, rng.choice([0] + feobjects))
        for i, object_ in enumerate(feobjects):
            struct.pack_into('<I', memory, object_ - ARENA + 0x10, rng.randrange(5))
        for i in range(count):
            offset = icons[i] - ARENA
            struct.pack_into('<III', memory, offset, 0,
                             icons[i + 1] + 4 if i + 1 < count else sentinel,
                             icons[i - 1] + 4 if i else sentinel)
            struct.pack_into('<I', memory, offset + 0x10, feobjects[i])
            memory[offset + 0x20] = rng.choice((0, 1, 0x80))
        manager_enabled, found = rng.choice((0, 1)), rng.choice((0, node))
        target = rng.choice(icons[:count])
        compare('SelectIcon', memory, target, manager_enabled, found)
        index = rng.choice(list(range(count)) + [-1, count, count + 1, 0x7fffffff])
        compare('SelectIndex', memory, index, manager_enabled, found)
        compare('NodeAt', memory, index, this=sentinel)
    for count in range(9):
        memory = bytearray(0x10000)
        sentinel = scroller + 4
        struct.pack_into('<II', memory, 0x104, icons[0] + 4 if count else sentinel,
                         icons[count - 1] + 4 if count else sentinel)
        for i in range(count):
            struct.pack_into('<I', memory, icons[i] - ARENA + 4, icons[i + 1] + 4 if i + 1 < count else sentinel)
        for index in (-1, 0, 1, count - 1, count, count + 1, 0x7fffffff):
            compare('NodeAt', memory, index, this=sentinel)
    report = {'comparisons': counts, 'total': sum(counts.values()), 'byte_matching': 'not established',
              'controlled_interfaces': ['archive FindNode (0x51bb70)', 'queue Post (0x51aa20)'],
              'fingerprints': {name: {'address': hex(a), 'size': n,
                                     'sha256': hashlib.sha256(image[a-0x400000:a-0x400000+n]).hexdigest()}
                               for name, (a, n) in FUNCTIONS.items()}}
    (BUILD / 'verification.json').write_text(json.dumps(report, indent=2) + '\n')
    print(json.dumps(report, indent=2))


if __name__ == '__main__':
    main()
