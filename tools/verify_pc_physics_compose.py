#!/usr/bin/env python3
"""Compare the physics parameter composer, top speed and performance simulation with original x86."""
import hashlib
import os
import json
import random
import re
import struct
import subprocess
import pefile
from unicorn import Uc, UC_ARCH_X86, UC_MODE_32, UC_HOOK_BLOCK
from unicorn.x86_const import *
from pc_coff import link_objects, read_object
from probe_pc_race_reward import ROOT, EXE
from verify_pc_physics_record_upgrades import retail_sources
from verify_pc_engine import CONVERSION

BUILD = ROOT / 'build/analysis/physics-compose'
BASE, STACK, STOP = 0x2000000, 0x201f000, 0x2100000
PARAMS, ROWS, GROUPS, ENTRIES = BASE + 0x1000, BASE + 0x3000, BASE + 0x8000, BASE + 0xc000
TABLE_POINTER = 0x8a1ccc
SAVED = (UC_X86_REG_EBX, UC_X86_REG_ESI, UC_X86_REG_EDI, UC_X86_REG_EBP)
CLEANUP = dict(Compose=12, GetDriveArea=0, SimulatePerformance=0)
PHYSICS = ROOT / 'src/Speed/Indep/Src/Physics'
OP_UNITS = ('PhysicsRecordUpgrades', 'PhysicsRecordBuild', 'PhysicsRecordExtra', 'PhysicsTuningSelection',
            'EngineMath', 'EngineCurve', 'PhysicsTuningValues', 'DrivetrainSetup', 'PhysicsDriveCurve',
            'PhysicsRefresh', 'PhysicsVariants', 'PhysicsCompose')


def compile_unit(name, units, op):
    source = BUILD / (name + '.cpp')
    source.write_text(''.join('#include "%s"\n' % (PHYSICS / (unit + '.cpp')) for unit in units))
    obj = BUILD / (name + '.obj')
    subprocess.run([str(ROOT / 'build/tools/wibo'), 'build/compilers/Win32/7.1/cl.exe', '/nologo', '/c', '/Ox']
                   + (['/Op'] if op else []) + ['/Ob0', '/Z7', '/EHsc', '/Fo' + str(obj.relative_to(ROOT)),
                   str(source.relative_to(ROOT))], cwd=ROOT, check=True)
    return obj


def callee_closure(roots):
    graph = {}
    for line in (ROOT / 'build/ghidra/callgraph.jsonl').read_text().splitlines():
        entry = json.loads(line)
        address = entry['a'] if isinstance(entry['a'], int) else int(entry['a'], 16)
        graph[address] = [c if isinstance(c, int) else int(c, 16) for c in entry.get('calls', [])]
    seen, pending = set(), list(roots)
    while pending:
        address = pending.pop()
        if address not in seen:
            seen.add(address)
            pending.extend(graph.get(address, []))
    return seen


def main():
    manifest = json.loads((ROOT / 'config/SPEED2_EXE_1_2/physics-compose.json').read_text())
    raw = EXE.read_bytes()
    assert hashlib.sha256(raw).hexdigest() == manifest['exe_sha256']
    image = pefile.PE(data=raw).get_memory_mapped_image()
    index = {int(f['start'], 16): f for f in json.loads((ROOT / 'build/analysis/function-index.json').read_text())['functions']}
    for f in manifest['functions']:
        assert hashlib.sha256(image[f['address'] - 0x400000:f['address'] - 0x400000 + f['size']]).hexdigest() == f['sha256'], f['method']
    # Original side: the static callee closure of the three roots, each body
    # fingerprinted from the function index (jump tables follow their bodies).
    closure = callee_closure([f['address'] for f in manifest['functions']])
    original_ranges = []
    for address in sorted(closure):
        entry = index[address]
        body = image[address - 0x400000:address - 0x400000 + entry['size']]
        assert hashlib.sha256(body).hexdigest() == entry['sha256'], hex(address)
        original_ranges.append((address, address + entry['size']))
    BUILD.mkdir(parents=True, exist_ok=True)
    objects = [compile_unit('compose-analysis', OP_UNITS, True),
               compile_unit('compose-upgrades', ('PhysicsUpgrades', 'PhysicsUpgradeSearch'), False),
               compile_unit('compose-wheel-math', ('WheelMath',), False)]
    globals_ = {name: int(address, 16) for name, address in re.findall(r'^(\w+) = (0x[0-9a-f]+);',
                (ROOT / 'src/Platform/Native/OriginalSymbols.ld').read_text(), re.M)}
    externals = {'__ftol2': CONVERSION[0], '__fltused': BASE + 0x1f000,
                 # Unused methods in DrivetrainSetup.cpp; guards reject execution.
                 '?RefreshVariations@Drivetrain@@QAEXXZ': 0x594710, '_nfs_strcpy': 0x43dbd0}
    for obj in objects:
        for symbol in read_object(obj)['symbols'].values():
            match = re.match(r'^\?(\w+)@@', symbol['name'])
            if symbol['section'] == 0 and match:
                if match[1] in globals_:
                    externals[symbol['name']] = globals_[match[1]]
                elif match[1] == 'nfs_strcpy':
                    externals[symbol['name']] = 0x43dbd0
    linked = link_objects(objects, externals)
    entries = {}
    for f in manifest['functions']:
        found = [a for s, a in linked['exports'].items() if s.startswith('?' + f['method'] + '@')]
        assert len(found) == 1, (f['method'], found)
        entries[f['method']] = (f['address'], found[0])
    runtime = [(CONVERSION[0], CONVERSION[0] + CONVERSION[1])]
    machines = []
    for side in (0, 1):
        uc = Uc(UC_ARCH_X86, UC_MODE_32)
        uc.mem_map(0x400000, (len(image) + 4095) & ~4095)
        uc.mem_write(0x400000, image)
        uc.mem_map(BASE, 0x20000)
        uc.mem_map(STOP, 0x1000)
        uc.mem_map(linked['address'], linked['allocation'])
        for address, body in linked['blocks']:
            uc.mem_write(address, body)
        ranges = (linked['ranges'] if side else original_ranges) + runtime
        def guard(machine, address, size, user, ranges=ranges):
            assert any(a <= address and address + size <= b for a, b in ranges), hex(address)
        uc.hook_add(UC_HOOK_BLOCK, guard)
        machines.append(uc)
    counts = dict.fromkeys(entries, 0)

    def compare(method, state, args, globals_state):
        outcomes = []
        for side, uc in enumerate(machines):
            uc.mem_write(BASE, bytes(state))
            for address, data in globals_state:
                uc.mem_write(address, data)
            words = [a & 0xffffffff for a in args]
            uc.mem_write(STACK, struct.pack('<' + 'I' * (1 + len(words)), STOP, *words))
            uc.reg_write(UC_X86_REG_ECX, PARAMS)
            uc.reg_write(UC_X86_REG_ESP, STACK)
            uc.reg_write(UC_X86_REG_FPCW, 0x027f)
            uc.reg_write(UC_X86_REG_FPSW, 0)
            uc.reg_write(UC_X86_REG_FPTAG, 0xffff)
            for reg in SAVED:
                uc.reg_write(reg, 0x12345678)
            uc.emu_start(entries[method][side], STOP, count=200000000)
            assert uc.reg_read(UC_X86_REG_EIP) == STOP
            assert uc.reg_read(UC_X86_REG_ESP) == STACK + 4 + CLEANUP[method], (method, side)
            assert all(uc.reg_read(reg) == 0x12345678 for reg in SAVED)
            depth = (uc.reg_read(UC_X86_REG_FPSW) >> 11) & 7
            # Float returns leave one value on the x87 stack; store it at 80 bits.
            result = None
            if method == 'GetDriveArea':
                assert depth == 7, depth
                result = uc.reg_read(UC_X86_REG_FP7)
            else:
                assert depth == 0, depth
            outcomes.append((bytes(uc.mem_read(BASE, 0x10000)), result))  # stack lies above
        assert outcomes[0] == outcomes[1], (method, counts[method], args,
                                            next((hex(BASE + i) for i, (a, b) in enumerate(zip(outcomes[0][0], outcomes[1][0])) if a != b), None),
                                            outcomes[0][1], outcomes[1][1])
        counts[method] += 1

    rng = random.Random(0x5b4dc0)
    rows = [row for _, row in retail_sources()]
    only = os.environ.get('UG2_COMPOSE_TRIAL')  # debugging aid: run one trial
    for trial, row in enumerate(rows):
        for variant in range(6):
            state = bytearray(0x20000 - 0x1000)
            state[ROWS - BASE + 0x890 + 0x110:ROWS - BASE + 0x890 + 0x890] = row
            if variant == 5:
                struct.pack_into('<i', state, ROWS - BASE + 0x890 + 0x844, 1)
            p = PARAMS - BASE
            struct.pack_into('<ibxxx', state, p, 1, (-1, 0, 2)[variant % 3])
            for kind in range(10):
                struct.pack_into('<f', state, p + 0x2e0 + kind * 4, rng.choice((0.0, 1.0, 2.0, 3.0, rng.uniform(0, 3))) if variant else 0.0)
                struct.pack_into('<I', state, p + 0x2e0 + 0x28 + kind * 4, rng.randrange(2) if variant else 0)
            for i in range(68):
                state[p + 0x2e0 + 0x50 + i] = rng.randrange(2) if variant else 0
            # Upgrade items own selection bytes 0..59 and BuildRecord's search
            # rewrites them. Tuning entries use bytes 60..67 instead, with gear
            # kinds and the 16/17 pair sharing one byte each, because partial
            # selections make the original read stale stack slots.
            tuning_byte = lambda kind: 60 if 10 <= kind <= 15 else 61 if kind in (16, 17) else 62 + kind % 6
            t = p + 0x374
            struct.pack_into('<iiII', state, t, 1, rng.choice((0, 1)), PARAMS + 0x2e0, PARAMS + 0x30)
            for i in range(41):
                state[t + 0x14 + i] = rng.choice((100, 100, 0, 200, rng.randrange(256))) if variant else 100
            struct.pack_into('<ifffI', state, p + 0x55c, 64, 0.0, 55.5555572509765625, 1.13399994373321533203125, PARAMS + 0x45c)
            for group in range(30):
                struct.pack_into('<iiI', state, GROUPS - BASE + group * 0x17c, group // 3, 1 + group % 3, 2)
            items = [GROUPS + group * 0x17c + 0xc + item * 0x5c for group in range(30) for item in range(2)]
            for i, item in enumerate(items):
                struct.pack_into('<i', state, item - BASE, i)
                for component in range(4):
                    struct.pack_into('<f', state, item - BASE + 0x30 + component * 4, rng.uniform(0, 1))
                for j in range(7):
                    struct.pack_into('<i', state, item - BASE + 0x40 + j * 4, -1)
            for i in range(25):
                struct.pack_into('<ibbBBffff', state, ENTRIES - BASE + i * 0x18, tuning_byte(i), 0, i, rng.randrange(5), 0xa5,
                                 rng.uniform(-0.5, 0.5), rng.uniform(-0.5, 0.5), -99.99, -99.99)
            globals_state = [(TABLE_POINTER, struct.pack('<I', ROWS)), (0x8383f8, struct.pack('<Ii', GROUPS, 30)),
                             (0x837b90, b''.join(struct.pack('<I', item) for item in items) + bytes(8 * 4)),
                             (0x86b340, struct.pack('<Ii', ENTRIES, 25)), (0x7fc290, bytes((trial % 2,)))]
            mode = (0, 3, 10, 5, 2, 7)[variant]
            if only is not None and trial != int(only):
                continue
            for refresh in (0, 1, 0x100):
                for flag in ((0, 1) if variant < 2 else (0,)):
                    compare('Compose', state, (mode, refresh, flag), globals_state)
            if variant < 3:
                compare('GetDriveArea', state, (), globals_state)
            if variant < 2:
                compare('SimulatePerformance', state, (), globals_state)
    report = dict(executable_sha256=manifest['exe_sha256'], bit_identical_cases=counts,
                  original_closure=len(closure), byte_matching='not established', controlled_interfaces=[],
                  limitations=['46 retail CarTypeInfo rows with synthetic upgrade groups, selections and tuning entries.',
                               'Tuning entries use selection bytes 60..67 (untouched by upgrade items); gear kinds and the 16/17 pair share one byte, since partial selections read stale stack slots.',
                               'Only 53-bit x87 precision; NaN inputs excluded.',
                               'Rebuilt side executes all recovered callees; __ftol2 is original runtime code on both sides.'])
    (BUILD / 'verification.json').write_text(json.dumps(report, indent=2) + '\n')
    print(json.dumps(report, indent=2))


if __name__ == '__main__':
    main()
