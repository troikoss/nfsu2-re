#!/usr/bin/env python3
"""Original/rebuilt scrollbar construction, visibility and scaled/fixed geometry."""
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
from verify_pc_feng_coordinates import FUNCTIONS as COORDINATES

BUILD = ROOT / 'build/analysis/scroller-scrollbar'
FUNCTIONS = {'UpdateScaled': (0x52e200, 287), 'UpdateFixed': (0x52e320, 324), 'Update': (0x538080, 173), 'Constructor': (0x542ac0, 465), 'FngFindNamedImage': (0x4953e0, 22)}
GLOBALS = {'tweenSixth786D94': 0x786d94, 'frontendDelta865198': 0x865198, 'realClock8651AC': 0x8651ac,
           'scrollerClock8641A0': 0x8641a0, 'fengEpsilon7992EC': 0x7992ec,
           'fengStateKeys7992F4': 0x7992f4, 'fengVisibilityDirty8384D4': 0x8384d4}
INTERFACES = {'FngFormatBuffer': (0x50c960, 5, False), 'FngHash': (0x505450, 1, False), 'FngFindImageByHash': (0x537a60, 2, False), 'FindFEngFont': (0x51bd70, 1, False),
              'LineWidth': (0x50bce0, 4, True)}
ARENA, STACK, STOP = 0x2000000, 0x200f000, 0x2100000
SELECT, FLOAT_STOP, FLOAT_RESULT = STOP + 0x100, STOP + 0x40, ARENA + 0xe000
REGISTERS = (UC_X86_REG_EBX, UC_X86_REG_ESI, UC_X86_REG_EDI, UC_X86_REG_EBP)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source', default='src/Speed/Indep/Src/Frontend/ScrollerScrollBar.cpp')
    args = parser.parse_args()
    raw = EXE.read_bytes()
    assert hashlib.sha256(raw).hexdigest() == 'f9dd86c054878ce6276beb07c1fd61874f7a1e4bf1f241b084c65b73e24168a7'
    image = pefile.PE(data=raw).get_memory_mapped_image()
    BUILD.mkdir(parents=True, exist_ok=True)
    objects = []
    for i, source in enumerate((args.source, 'src/Speed/Indep/Src/Frontend/FEngCoordinates.cpp',
                                'src/Speed/Indep/Src/Hud/HudVisibility.cpp')):
        obj = BUILD / ('layout-%d.obj' % i)
        subprocess.run([str(ROOT / 'build/tools/wibo'), 'build/compilers/Win32/7.1/cl.exe', '/nologo', '/c',
                        '/Ox', '/Ob0', '/Z7', '/EHsc', '/Fo' + str(obj.relative_to(ROOT)),
                        '/Isrc/Speed/Indep/Src/Frontend', os.path.relpath(source, ROOT)], cwd=ROOT, check=True)
        objects.append(obj)
    # The list helper is retained unchanged in original/rebuilt executions.
    externals = {'__ftol2': 0x75bed4, '__fltused': STOP + 0xf00}
    symbols = [s for o in objects for s in read_object(o)['symbols'].values()]
    defined = {s['name'] for s in symbols if s['section'] > 0}
    for s in symbols:
        if s['section'] != 0 or s['name'] in defined or s['name'] in externals:
            continue
        name = s['name'][1:].split('@')[0]
        if name in GLOBALS: externals[s['name']] = GLOBALS[name]
        elif name == 'NodeAt': externals[s['name']] = 0x43be60
        elif name in INTERFACES: externals[s['name']] = INTERFACES[name][0]
        else: raise AssertionError(s['name'])
    linked = link_objects(objects, externals)
    entries = {name: next(a for n, a in linked['exports'].items() if n.startswith('??0ScrollerScrollBar@' if name == 'Constructor' else '?' + name + '@')) for name in FUNCTIONS}
    state, machines = {}, []
    retained = [(0x75bed4, 0x75bf49), (0x43be60, 0x43be7f)]
    dependencies = [(a, a + n) for a, n in list(COORDINATES.values())] + [(0x50ca00, 0x50ca45), (0x50ca50, 0x50caa2)]
    for rebuilt in (False, True):
        uc = Uc(UC_ARCH_X86, UC_MODE_32)
        uc.mem_map(0x400000, (len(image) + 4095) & ~4095)
        uc.mem_write(0x400000, image)
        uc.mem_map(ARENA, 0x10000)
        uc.mem_map(STOP, 0x1000)
        uc.mem_write(FLOAT_STOP, b'\xd9\x1d' + struct.pack('<I', FLOAT_RESULT) + b'\xe9' + struct.pack('<i', STOP - (FLOAT_STOP + 11)))
        uc.mem_map(linked['address'], linked['allocation'])
        for address, body in linked['blocks']: uc.mem_write(address, body)
        ranges = (linked['ranges'] if rebuilt else [(a, a + n) for a, n in FUNCTIONS.values()] + dependencies) + retained
        hooks = {a: (n, argc, member) for n, (a, argc, member) in INTERFACES.items()}

        def block(uc, address, size, user, ranges=ranges):
            if address in (STOP, FLOAT_STOP): return
            if address == SELECT or address in hooks:
                name, argc, member = ('Select', 1, True) if address == SELECT else hooks[address]
                assert name not in ('FindFEngFont', 'LineWidth'), 'image-only placement fixture reached font interface'
                sp = uc.reg_read(UC_X86_REG_ESP)
                words = struct.unpack('<' + 'I' * (argc + 1), uc.mem_read(sp, (argc + 1) * 4))
                this = uc.reg_read(UC_X86_REG_ECX) if member else 0
                def string(pointer):
                    if not pointer: return None
                    out = bytearray()
                    while (c := uc.mem_read(pointer + len(out), 1)[0]): out.append(c)
                    return out.decode('ascii')
                if name == 'FngFormatBuffer':
                    fmt, prefix, suffix = map(string, words[3:6])
                    state['trace'].append((name, words[2], fmt, prefix, suffix))
                    text_ = (prefix + suffix).encode('ascii')
                    uc.mem_write(words[1], text_ + b'\0'); uc.reg_write(UC_X86_REG_EAX, len(text_))
                elif name == 'FngHash':
                    text_ = string(words[1]); state['trace'].append((name, text_))
                    h = 0 if text_ is None else sum((i+1)*c for i,c in enumerate(text_.encode('ascii')))
                    uc.reg_write(UC_X86_REG_EAX, h)
                elif name == 'FngFindImageByHash':
                    state['trace'].append((name, words[1:]))
                    uc.reg_write(UC_X86_REG_EAX, state.get('lookups', {}).get(words[2], 0))
                uc.reg_write(UC_X86_REG_ESP, sp + 4 + (4 * argc if member else 0))
                uc.reg_write(UC_X86_REG_EIP, words[0])
                return
            assert any(a <= address < b for a, b in ranges), ('unexpected execution', hex(address))
        uc.hook_add(UC_HOOK_BLOCK, block)
        machines.append(uc)

    rng = random.Random(0x538940)
    counts = dict.fromkeys(FUNCTIONS, 0)
    scroller = ARENA + 0x100
    icons = [ARENA + 0x1000 + i * 0x100 for i in range(5)]
    objects_ = [ARENA + 0x2000 + i * 0x100 for i in range(5)]
    data_ = [ARENA + 0x3000 + i * 0x100 for i in range(5)]

    def compare(name, memory, args=b'', this=0, cleanup=0, floating=False, precision=0x027f, lookups=None):
        outputs = []
        for rebuilt, uc in zip((False, True), machines):
            uc.mem_write(ARENA, bytes(memory))
            uc.mem_write(0x865198, struct.pack('<f', 1.0 / 60))
            uc.mem_write(0x8651ac, struct.pack('<i', 123456))
            uc.mem_write(0x8641a0, struct.pack('<i', -1))
            uc.mem_write(0x8384d4, bytes(4))
            uc.mem_write(STACK, struct.pack('<I', FLOAT_STOP if floating else STOP) + args)
            uc.reg_write(UC_X86_REG_ESP, STACK); uc.reg_write(UC_X86_REG_ECX, this)
            uc.reg_write(UC_X86_REG_FPCW, precision); uc.reg_write(UC_X86_REG_FPSW, 0); uc.reg_write(UC_X86_REG_FPTAG, 0xffff)
            for reg in REGISTERS: uc.reg_write(reg, 0x12345678)
            state.update(trace=[], scroller=scroller, selected_index=3, lookups=lookups or {})
            uc.emu_start(entries[name] if rebuilt else FUNCTIONS[name][0], STOP, count=50000)
            assert uc.reg_read(UC_X86_REG_EIP) == STOP
            assert uc.reg_read(UC_X86_REG_ESP) == STACK + 4 + cleanup, (name, rebuilt, hex(uc.reg_read(UC_X86_REG_ESP)))
            assert all(uc.reg_read(r) == 0x12345678 for r in REGISTERS)
            assert uc.reg_read(UC_X86_REG_FPCW) == precision
            outputs.append((([uc.reg_read(UC_X86_REG_EAX)] if name in ('Constructor','FngFindNamedImage') else []) + state['trace'], bytes(uc.mem_read(ARENA, 0xe004)), bytes(uc.mem_read(0x8384d4, 4))))
        if outputs[0] != outputs[1]:
            diff = [(hex(i), outputs[0][1][i], outputs[1][1][i]) for i in range(0xe004) if outputs[0][1][i] != outputs[1][1][i]][:12]
            print('difference', name, args.hex(), diff, outputs[0][0], outputs[1][0])
        assert outputs[0] == outputs[1], (name, args.hex())
        counts[name] += 1


    def fixture():
        memory = bytearray(rng.randbytes(0x10000))
        memory[0x100:0x104] = bytes(rng.choice((0, 1, 0x80)) for _ in range(4))
        struct.pack_into('<10f', memory, 0x10c, *[rng.choice((0.0, 1.0, -1.0, rng.uniform(-1000, 1000))) for _ in range(10)])
        for i in range(6):
            obj, data = ARENA + 0x1000 + i * 0x100, ARENA + 0x2000 + i * 0x100
            struct.pack_into('<I', memory, 0x140 + i * 4, obj if i == 1 else rng.choice((0, obj)))
            struct.pack_into('<i', memory, obj - ARENA + 0x18, rng.choice((1, 7, 9)))
            struct.pack_into('<I', memory, obj - ARENA + 0x2c, data)
            struct.pack_into('<II', memory, obj - ARENA + 0x4c, 0, 0)
            struct.pack_into('<3f', memory, data - ARENA + 0x1c, rng.uniform(-1000, 1000), rng.uniform(-1000, 1000), 0)
            struct.pack_into('<3f', memory, data - ARENA + 0x38, rng.uniform(-1000, 1000), rng.uniform(-1000, 1000), 1)
        return memory

    for trial in range(5000):
        memory = fixture()
        precision = rng.choice((0x027f, 0x037f))
        page = rng.choice((-2147483648, -16777217, -1, 0, 1, 5, 16777217, 2147483647))
        count = rng.choice((-2147483648, -16777217, -1, 0, 1, 5, 16777217, 2147483647))
        selected = rng.choice((-2147483648, -1, 0, 1, 2, count, 2147483647))
        if trial % 2 == 0:
            count = rng.randrange(2, 500); page = rng.randrange(1, count); selected = rng.randrange(1, count + 1)
        args_ = struct.pack('<3i', page, count, selected)
        for name in ('UpdateScaled','UpdateFixed','Update'): compare(name, memory, args_, scroller, 12, precision=precision)
    for extent in (13.0, 123.456, 1.0e-4, 10000.123):
        for count in (3, 7, 19, 101):
            for selected in range(1, count + 1):
                memory = fixture(); memory[0x100] = 0; memory[0x102] = 1
                struct.pack_into('<4f', memory, 0x11c, 0, 0, extent, extent)
                compare('UpdateScaled', memory, struct.pack('<3i', 1, count, selected), scroller, 12, precision=0x037f)
    for value in (float('nan'), float('inf'), -float('inf'), -0.0, 0.0):
        for precision in (0x027f, 0x037f):
            for vertical in (0, 1):
                for deferred in (0, 1):
                    memory = fixture(); memory[0x100] = vertical; memory[0x102] = deferred
                    struct.pack_into('<4f', memory, 0x124, value, value, value, value)
                    for name in ('UpdateScaled','UpdateFixed','Update'): compare(name, memory, struct.pack('<3i', 1, 5, 3), scroller, 12, precision=precision)
    # No geometry is requested in this branch, so null handle/arrow bindings are valid.
    memory = fixture(); memory[0x140:0x158] = bytes(24)
    compare('Update', memory, struct.pack('<3i', 1, 0, 0), scroller, 12)
    for trial in range(2000):
        memory = fixture(); prefix = 'ScrollBar' if trial % 2 else 'Other'
        memory[0x500:0x540] = bytes(64); memory[0x500:0x500+len(prefix)+1] = prefix.encode()+b'\0'
        lookups = {}
        for i,suffix in enumerate(('_Backing','_Handle','_Arrow_1','_Arrow_2','_Backing_End_1','_Backing_End_2')):
            label = (prefix+suffix).encode(); h = sum((j+1)*c for j,c in enumerate(label))
            lookups[h] = rng.choice((0, ARENA+0x1000+i*0x100))
            struct.pack_into('<i', memory, 0x1018+i*0x100, 1)
        lookups[sum((j+1)*c for j,c in enumerate(prefix.encode()))] = rng.choice((0, ARENA+0x1000))
        package = rng.choice((0, ARENA+0x700)); prefix_ptr = rng.choice((0, ARENA+0x500))
        flags = [rng.choice((0,1,0x80,255)) for _ in range(3)]
        compare('Constructor', memory, struct.pack('<5I',package,prefix_ptr,*flags), scroller, 20, lookups=lookups)
        compare('FngFindNamedImage', memory, struct.pack('<II',package,ARENA+0x500), lookups=lookups)
    report = {'comparisons': counts, 'total': sum(counts.values()), 'byte_matching': 'not established',
              'controlled_interfaces': ['format 0x50c960', 'hash 0x505450', 'typed lookup 0x537a60'], 'linked_but_unreached_interfaces': ['FindFEngFont', 'LineWidth'],
              'fixture_scope': 'image handle, valid geometry state; null bindings in hidden branch',
              'fingerprints': {n: {'address': hex(a), 'size': size, 'sha256': hashlib.sha256(image[a-0x400000:a-0x400000+size]).hexdigest()} for n, (a, size) in FUNCTIONS.items()}}
    (BUILD / 'verification.json').write_text(json.dumps(report, indent=2)+'\n'); print(json.dumps(report, indent=2))


if __name__ == '__main__': main()
