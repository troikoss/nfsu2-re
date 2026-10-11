#!/usr/bin/env python3
"""Verify per-upgrade original record modifiers with recovered fraction queries."""
import hashlib
import json
import random
import re
import struct
import subprocess
import pefile
from unicorn import Uc, UC_ARCH_X86, UC_MODE_32, UC_HOOK_CODE
from unicorn.x86_const import *
from pc_coff import link_objects, read_object
from probe_pc_race_reward import ROOT, EXE, GAME_DIR
from verify_pc_physics_parameters import retail_records
from verify_pc_engine import CONVERSION

BUILD = ROOT / 'build/analysis/physics-record-upgrades'
BASE, STACK, STOP = 0x2000000, 0x200f000, 0x2100000
SOURCE, RECORD, SELECTION, GROUPS = BASE + 0x1000, BASE + 0x2000, BASE + 0x3000, BASE + 0x4000


def retail_sources():
    names = {name for name, _ in retail_records()}
    raw = (GAME_DIR / 'GLOBAL/GLOBALB.BUN').read_bytes()
    offset = 0
    while offset + 8 <= len(raw):
        kind, size = struct.unpack_from('<II', raw, offset)
        if kind == 0x34600:
            for start in range(offset + 16, offset + 8 + size, 0x890):
                name = raw[start:start + 32].split(b'\0')[0].decode('ascii')
                if name in names:
                    yield name, raw[start + 0x110:start + 0x890]
            return
        offset += 8 + size
    raise AssertionError('No CarTypeInfo chunk')


def main():
    manifest = json.loads((ROOT / 'config/SPEED2_EXE_1_2/physics-record-upgrades.json').read_text())
    dependency = json.loads((ROOT / 'config/SPEED2_EXE_1_2/physics-construction.json').read_text())
    raw = EXE.read_bytes()
    assert hashlib.sha256(raw).hexdigest() == manifest['exe_sha256'] == dependency['exe_sha256']
    image = pefile.PE(data=raw).get_memory_mapped_image()
    dep_functions = [f for f in dependency['functions'] if f['address'] in (0x5a6580, 0x500100, 0x5a61a0)]
    functions = manifest['functions'] + dep_functions
    for f in functions:
        assert hashlib.sha256(image[f['address'] - 0x400000:f['address'] - 0x400000 + f['size']]).hexdigest() == f['sha256']
    assert hashlib.sha256(image[CONVERSION[0] - 0x400000:CONVERSION[0] - 0x400000 + CONVERSION[1]]).hexdigest() == CONVERSION[2]
    BUILD.mkdir(parents=True, exist_ok=True)
    source = BUILD / 'record-upgrades-analysis.cpp'
    source.write_text(''.join('#include "' + str(ROOT / 'src/Speed/Indep/Src/Physics' / (unit + '.cpp')) + '"\n'
                              for unit in ('PhysicsUpgrades', 'PhysicsRecordUpgrades')))
    obj = BUILD / 'record-upgrades-analysis.obj'
    subprocess.run([str(ROOT / 'build/tools/wibo'), 'build/compilers/Win32/7.1/cl.exe', '/nologo',
                    '/c', '/Ox', '/Op', '/Ob0', '/Z7', '/EHsc', '/Fo' + str(obj.relative_to(ROOT)),
                    str(source.relative_to(ROOT))], cwd=ROOT, check=True)
    globals_ = {name: int(address, 16) for name, address in re.findall(r'^(\w+) = (0x[0-9a-f]+);',
                (ROOT / 'src/Platform/Native/OriginalSymbols.ld').read_text(), re.M)}
    externals = {'__ftol2': CONVERSION[0], '__fltused': BASE + 0x9000}
    for symbol in read_object(obj)['symbols'].values():
        match = re.match(r'^\?(\w+)@@', symbol['name'])
        if symbol['section'] == 0 and match and match[1] in globals_:
            externals[symbol['name']] = globals_[match[1]]
    linked = link_objects([obj], externals)
    methods = {}
    for f in manifest['functions']:
        entries = [a for s, a in linked['exports'].items() if s.startswith('?' + f['method'] + '@')]
        assert len(entries) == 1, entries
        methods[f['method']] = (f['address'], entries[0])
    machines = []
    visits = dict.fromkeys((0x5a6c25, 0x5a722a, 0x5a71f5, 0x5a666a), 0)
    for side in (0, 1):
        uc = Uc(UC_ARCH_X86, UC_MODE_32)
        uc.mem_map(0x400000, (len(image) + 4095) & ~4095)
        uc.mem_write(0x400000, image)
        uc.mem_map(BASE, 0x10000)
        uc.mem_map(STOP, 0x1000)
        uc.mem_map(linked['address'], linked['allocation'])
        for address, body in linked['blocks']:
            uc.mem_write(address, body)
        ranges = linked['ranges'] if side else [(f['address'], f['address'] + f['size']) for f in functions]
        ranges = ranges + [(CONVERSION[0], CONVERSION[0] + CONVERSION[1])]
        def guard(machine, address, size, user, ranges=ranges, side=side):
            if not side and address in visits:
                visits[address] += 1
            assert any(a <= address < b for a, b in ranges), hex(address)
        uc.hook_add(UC_HOOK_CODE, guard)
        machines.append(uc)
    rng = random.Random(0x5a6b50)
    counts = dict.fromkeys(methods, 0)
    items = [GROUPS + group * 0x17c + 0xc + item * 0x5c for group in range(30) for item in range(2)]
    records = list(retail_sources())
    for name, record in records:
        for trial in range(8):
            state = bytearray(b'\xa5' * 0x8000)
            state[SOURCE - BASE:SOURCE - BASE + 0x780] = record
            state[RECORD - BASE:RECORD - BASE + 0x2b0] = record[:0x2b0]
            state[SELECTION - BASE:SELECTION - BASE + 0x94] = bytes(0x94)
            for group in range(30):
                struct.pack_into('<iiI', state, GROUPS - BASE + group * 0x17c, group // 3, 1 + group % 3, 2)
            for i, item in enumerate(items):
                struct.pack_into('<i', state, item - BASE, i)
                # All-zero/all-selected and component/grade boundaries, followed
                # by nonuniform finite weights. Keep the selected source rows real.
                for component in range(4):
                    weight = (0.0, 1/6, 1/3, 0.5, 1.0, -0.5, 2.0, rng.uniform(-0.5, 2.0))[trial]
                    struct.pack_into('<f', state, item - BASE + 0x30 + component * 4, weight)
                for j in range(7):
                    struct.pack_into('<i', state, item - BASE + 0x40 + j * 4, -1)
                state[SELECTION - BASE + 0x50 + i] = 0 if trial == 0 else (rng.randrange(2) if trial == 7 else 1)
            for method, entries in methods.items():
                outcomes = []
                for side, uc in enumerate(machines):
                    uc.mem_write(BASE, bytes(state))
                    uc.mem_write(0x8383f8, struct.pack('<Ii', GROUPS, 30))
                    uc.mem_write(0x837b90, b'\0' * (68 * 4))
                    for i, item in enumerate(items):
                        uc.mem_write(0x837b90 + i * 4, struct.pack('<I', item))
                    uc.mem_write(STACK, struct.pack('<III', STOP, RECORD, SELECTION))
                    uc.reg_write(UC_X86_REG_ECX, SOURCE)
                    uc.reg_write(UC_X86_REG_ESP, STACK)
                    uc.reg_write(UC_X86_REG_FPCW, 0x027f)
                    for reg in (UC_X86_REG_EBX, UC_X86_REG_ESI, UC_X86_REG_EDI, UC_X86_REG_EBP):
                        uc.reg_write(reg, 0x12345678)
                    uc.emu_start(entries[side], STOP, count=100000)
                    assert uc.reg_read(UC_X86_REG_EIP) == STOP
                    assert uc.reg_read(UC_X86_REG_ESP) == STACK + 12
                    assert all(uc.reg_read(reg) == 0x12345678 for reg in (UC_X86_REG_EBX, UC_X86_REG_ESI, UC_X86_REG_EDI, UC_X86_REG_EBP))
                    outcomes.append(bytes(uc.mem_read(BASE, 0x8000)))
                for offset in range(0, 0x8000, 4):
                    a, b = outcomes[0][offset:offset + 4], outcomes[1][offset:offset + 4]
                    assert a == b, (method, name, trial, hex(offset), a.hex(), b.hex())
                counts[method] += 1
    assert all(visits.values()), visits
    report = dict(executable_sha256=manifest['exe_sha256'], bit_identical_cases=counts,
                  retail_record_count=len(records), original_branch_visits={hex(a): n for a, n in visits.items()},
                  byte_matching='not established', controlled_interfaces=[],
                  limitations=['Finite retail source rows and synthetic upgrade groups; malformed inputs/NaNs/alternate precision excluded.',
                               'Original __ftol2 executes as an analysis dependency.',
                               'Rebuilt fraction queries and their full covered callees execute recovered source.',
                               'Full record composition, tuning and native integration remain unresolved.'])
    (BUILD / 'verification.json').write_text(json.dumps(report, indent=2) + '\n')
    print(json.dumps(report, indent=2))


if __name__ == '__main__':
    main()
