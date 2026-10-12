#!/usr/bin/env python3
"""Original/rebuilt scroller placement, fade, reset/frame ordering and cubic tween."""
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
from verify_pc_feng_colour_scripts import FUNCTIONS as COLOUR_SCRIPTS

BUILD = ROOT / 'build/analysis/icon-scroller-layout'
FUNCTIONS = {'LimitEndSlope': (0x5748c0, 80), 'LimitAcceleration': (0x574910, 199),
             'UpdateScrollerTween': (0x574e30, 260), 'ScrollerFadeFactor': (0x50d8d0, 170),
             'SetWindowVisibility': (0x50d9c0, 86), 'SetIconAlpha': (0x51fd60, 110),
             'LayoutIcon': (0x52f8c0, 425), 'ResetLayout': (0x538a30, 244), 'UpdateLayout': (0x538940, 225)}
GLOBALS = {'tweenSixth786D94': 0x786d94, 'frontendDelta865198': 0x865198, 'realClock8651AC': 0x8651ac,
           'scrollerClock8641A0': 0x8641a0, 'fengEpsilon7992EC': 0x7992ec,
           'fengStateKeys7992F4': 0x7992f4, 'fengVisibilityDirty8384D4': 0x8384d4}
INTERFACES = {'FngFormatHash': (0x50b9c0, 1, False), 'FngSetObjectScript': (0x51cf70, 3, False),
              'FindFEngFont': (0x51bd70, 1, False),
              'LineWidth': (0x50bce0, 4, True)}
ARENA, STACK, STOP = 0x2000000, 0x200f000, 0x2100000
SELECT, FLOAT_STOP, FLOAT_RESULT = STOP + 0x100, STOP + 0x40, ARENA + 0xe000
REGISTERS = (UC_X86_REG_EBX, UC_X86_REG_ESI, UC_X86_REG_EDI, UC_X86_REG_EBP)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source', default='src/Speed/Indep/Src/Frontend/IconScrollerLayout.cpp')
    args = parser.parse_args()
    raw = EXE.read_bytes()
    assert hashlib.sha256(raw).hexdigest() == 'f9dd86c054878ce6276beb07c1fd61874f7a1e4bf1f241b084c65b73e24168a7'
    image = pefile.PE(data=raw).get_memory_mapped_image()
    BUILD.mkdir(parents=True, exist_ok=True)
    objects = []
    for i, source in enumerate((args.source, 'src/Speed/Indep/Src/Frontend/FEngCoordinates.cpp',
                                'src/Speed/Indep/Src/Hud/HudVisibility.cpp', 'src/Speed/Indep/Src/Frontend/FEngColour.cpp',
                                'src/Speed/Indep/Src/Frontend/IconScrollerScripts.cpp', 'src/Speed/Indep/Src/Frontend/ScrollerScrollBar.cpp')):
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
    entries = {name: next(a for n, a in linked['exports'].items() if n.startswith('?' + name + '@')) for name in FUNCTIONS}
    state, machines = {}, []
    retained = [(0x75bed4, 0x75bf49), (0x43be60, 0x43be7f)]
    dependencies = [(a, a + n) for a, n in list(COORDINATES.values()) + list(COLOUR_SCRIPTS.values())] + [(0x52e200, 0x52e31f), (0x52e320, 0x52e464), (0x538080, 0x53812d), (0x50ca00, 0x50ca45), (0x50ca50, 0x50caa2)]
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
                if name in ('FngFormatHash', 'FngSetObjectScript'):
                    pointer = words[1] if name == 'FngFormatHash' else words[2]
                    label = bytearray()
                    while (c := uc.mem_read(pointer + len(label), 1)[0]): label.append(c)
                    label = label.decode('ascii')
                    state['trace'].append((name, label) if name == 'FngFormatHash' else (name, words[1], label, words[3] & 255))
                    if name == 'FngFormatHash': uc.reg_write(UC_X86_REG_EAX, 123 if label == 'FadeIn' else 456)
                else:
                    state['trace'].append((name, this, words[1:]))
                if name == 'Select':
                    # Selection itself is verified separately. Capture pre-selection geometry/order.
                    state['trace'].append(('before_select', bytes(uc.mem_read(this + 0x90, 0x40)).hex()))
                    uc.mem_write(this + 0xc, struct.pack('<I', words[1]))
                    uc.mem_write(this + 0xc8, struct.pack('<I', state['selected_index']))
                    uc.reg_write(UC_X86_REG_EAX, 1)
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

    def compare(name, memory, args=b'', this=0, cleanup=0, floating=False, precision=0x027f):
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
            state.update(trace=[], scroller=scroller, selected_index=3)
            uc.emu_start(entries[name] if rebuilt else FUNCTIONS[name][0], STOP, count=50000)
            assert uc.reg_read(UC_X86_REG_EIP) == STOP
            assert uc.reg_read(UC_X86_REG_ESP) == STACK + 4 + cleanup, (name, rebuilt, hex(uc.reg_read(UC_X86_REG_ESP)))
            assert all(uc.reg_read(r) == 0x12345678 for r in REGISTERS)
            assert uc.reg_read(UC_X86_REG_FPCW) == precision
            outputs.append((state['trace'], bytes(uc.mem_read(ARENA, 0xe004)), bytes(uc.mem_read(0x8641a0, 4))))
        if outputs[0] != outputs[1]:
            diff = [(hex(i), outputs[0][1][i], outputs[1][1][i]) for i in range(0xe004) if outputs[0][1][i] != outputs[1][1][i]][:12]
            print('difference', name, args.hex(), diff, outputs[0][0], outputs[1][0])
        assert outputs[0] == outputs[1], (name, args.hex())
        counts[name] += 1

    def fixture(count=4):
        memory = bytearray(0x10000)
        struct.pack_into('<IIII', memory, 0x100, ARENA + 0x4000,
                         icons[0] + 4 if count else scroller + 4,
                         icons[count - 1] + 4 if count else scroller + 4,
                         rng.choice([0] + icons[:count]))
        struct.pack_into('<I', memory, 0x4014, SELECT)
        memory[0x12d] = rng.choice((0, 1))
        memory[0x12e] = rng.choice((0, 1, 0x80)); memory[0x130] = rng.choice((0, 1))
        struct.pack_into('<i', memory, 0x128, count + 1)
        struct.pack_into('<i', memory, 0x134, rng.choice((-9, -1, 1, 10)))
        struct.pack_into('<i', memory, 0x1c0, rng.randrange(4))
        struct.pack_into('<i', memory, 0x1c8, rng.randrange(1, count + 1) if count else -1)
        struct.pack_into('<f', memory, 0x1cc, 480.0)
        struct.pack_into('<ff', memory, 0x1d4, rng.uniform(-10, 10), rng.uniform(-20, 20))
        memory[0x1e4] = rng.choice((0, 1, 0x80))
        struct.pack_into('<12f', memory, 0x190, 0, 0, 0, 0, 0, 0, 0, 0, 0, .3, 1, 0)
        for i in range(count):
            offset = icons[i] - ARENA
            struct.pack_into('<III', memory, offset, 0,
                             icons[i + 1] + 4 if i + 1 < count else scroller + 4,
                             icons[i - 1] + 4 if i else scroller + 4)
            struct.pack_into('<IffI', memory, offset + 0x10, objects_[i], 120.0 * i, 100.0 * i, 0xffccf48a)
            memory[offset + 0x20] = rng.choice((0, 1))
            struct.pack_into('<ff', memory, offset + 0x24, rng.choice((80.0, 60.0)), rng.choice((80.0, 50.0)))
            struct.pack_into('<i', memory, objects_[i] - ARENA + 0x18, 1)
            struct.pack_into('<I', memory, objects_[i] - ARENA + 0x2c, data_[i])
            struct.pack_into('<4I', memory, data_[i] - ARENA, 138, 244, 204, 255)
            struct.pack_into('<fff', memory, data_[i] - ARENA + 0x38, 80, 80, 1)
        return memory

    for trial in range(2500):
        memory = fixture()
        precision = rng.choice((0x027f, 0x037f))
        tween = scroller + 0x90
        vals = [rng.uniform(-10, 10) for _ in range(9)] + [rng.choice((.3, .5, 1.0, 2.5)), rng.choice((0.0, .5, 1.0, 2.0))]
        struct.pack_into('<11fhh', memory, 0x190, *vals, rng.choice((-1, 0, 1, 2, 3)), rng.choice((0, 1, 2)))
        limit = rng.choice((0.0, .1, 1.0, 10.0))
        compare('LimitEndSlope', memory, struct.pack('<f', limit), tween, 4, precision=precision)
        compare('LimitAcceleration', memory, struct.pack('<f', limit), tween, 4, precision=precision)
        compare('UpdateScrollerTween', memory, struct.pack('<Ifff', tween, rng.choice((0.0, 1.0/60, .5, 2.0)), limit, limit), precision=precision)
    for trial in range(1200):
        memory = fixture(rng.randrange(1, 6))
        icon = icons[rng.randrange(struct.unpack_from('<i', memory, 0x128)[0] - 1)]
        position = rng.choice((-240.0, 240.0, -240.0001, 240.0001, 0.0, rng.uniform(-300, 300)))
        compare('ScrollerFadeFactor', memory, struct.pack('<4f', position, 0, 480, 64), floating=True)
        compare('SetWindowVisibility', memory, struct.pack('<If', icon, position), scroller, 8)
        compare('SetIconAlpha', memory, struct.pack('<If', rng.choice((0, icon)), rng.choice((0, .5, 1, 1.5, -.5))), scroller, 8)
        compare('LayoutIcon', memory, struct.pack('<I', rng.choice((0, icon))), scroller, 4)
        count = struct.unpack_from('<i', memory, 0x128)[0] - 1
        compare('ResetLayout', memory, struct.pack('<i', rng.choice([0, -1] + list(range(1, count + 1)))), scroller, 4)
        compare('UpdateLayout', memory, this=scroller)
    for count in (0, 1):
        memory = fixture(count)
        struct.pack_into('<I', memory, 0x10c, 0)
        compare('UpdateLayout', memory, this=scroller)
    # State/precision edges from the original x87 branches, including zero duration.
    for duration in (0.0, -0.0, -1.0, float('inf'), float('nan')):
        for status in (0, 1, 2, 3):
            memory = fixture()
            struct.pack_into('<11fhh', memory, 0x190, 1, .5, 2, 0, 1, 2, .5, 1, 0, duration, 1, status, 0)
            compare('LimitEndSlope', memory, struct.pack('<f', 1), scroller + 0x90, 4)
            compare('LimitAcceleration', memory, struct.pack('<f', 1), scroller + 0x90, 4)
            compare('UpdateScrollerTween', memory, struct.pack('<Ifff', scroller + 0x90, 0, 1, 1))
    report = {'comparisons': counts, 'total': sum(counts.values()), 'byte_matching': 'not established',
              'controlled_interfaces': list(INTERFACES) + ['selection slot +0x14'],
              'retained': ['0x43be60 list helper', '0x75bed4 float-to-int runtime'],
              'fingerprints': {n: {'address': hex(a), 'size': s, 'sha256': hashlib.sha256(image[a-0x400000:a-0x400000+s]).hexdigest()}
                               for n, (a, s) in FUNCTIONS.items()}}
    (BUILD / 'verification.json').write_text(json.dumps(report, indent=2) + '\n'); print(json.dumps(report, indent=2))


if __name__ == '__main__': main()
