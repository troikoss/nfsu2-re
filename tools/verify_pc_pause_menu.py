#!/usr/bin/env python3
"""Compare the original UI_Pause.fng object, option list and option items.

FEng, game-flow, career, profile and IconScroller base routines are controlled
interfaces hooked at their retail addresses in both machines, so cdecl and
thiscall contracts are compared directly. Rebuilt vtables are checked slot by
slot against the retail tables and normalised to retail addresses in memory.
"""
import hashlib
import itertools
import json
import struct
import subprocess

import pefile
from unicorn import Uc, UC_ARCH_X86, UC_MODE_32, UC_HOOK_CODE
from unicorn.x86_const import *
from probe_pc_race_reward import ROOT, EXE
from pc_coff import link_objects, read_object

BUILD = ROOT / 'build/analysis/pause-menu'
SOURCE = 'src/Speed/Indep/Src/Frontend/PauseMenu.cpp'
# name: (original address, span, rebuilt symbol prefix, stack arguments, returns)
FUNCTIONS = {
    'CreatePauseFNGObject': (0x4f4790, 95, '?CreatePauseFNGObject@@', 1, 'eax'),
    'Constructor': (0x4eb410, 215, '??0PauseFNGObject@@', 1, 'eax'),
    'ScalarDestructor': (0x4eb4f0, 30, '??_GPauseFNGObject@@', 1, 'eax'),
    'Destructor': (0x4eb510, 106, '??1PauseFNGObject@@', 0, None),
    'Setup': (0x4d9300, 191, '?Setup@PauseFNGObject@@', 0, None),
    'UnknownSlot0C': (0x49dfc0, 21, '?UnknownSlot0C@PauseFNGObject@@', 5, 'eax'),
    'AcceptsInput': (0x4a7bc0, 45, '?AcceptsInput@PauseFNGObject@@', 0, 'al'),
    'OptionsAvailable': (0x4a7bf0, 66, '?OptionsAvailable@PauseFNGObject@@', 0, 'al'),
    'QuitToCrib': (0x4a7c70, 67, '?QuitToCrib@PauseFNGObject@@', 0, None),
    'QuitToCribKeepHelpBar': (0x4a7cc0, 37, '?QuitToCribKeepHelpBar@PauseFNGObject@@', 0, None),
    'QuitToProfile': (0x4a7cf0, 99, '?QuitToProfile@PauseFNGObject@@', 0, None),
    'MessageHandler': (0x4c2170, 838, '?MessageHandler@PauseFNGObject@@', 4, None),
    'SetupFreeroam': (0x4c2900, 711, '?SetupFreeroam@PauseFNGObject@@', 0, None),
    'SetupRace': (0x4c2bd0, 1071, '?SetupRace@PauseFNGObject@@', 0, None),
    'Icon7954F0': (0x4c2640, 100, '??0PauseIcon7954F0@@', 3, 'eax'),
    'Icon795528': (0x4c27f0, 41, '??0PauseIcon795528@@', 3, 'eax'),
    'React7954D0': (0x4c24c0, 68, '?React@PauseIcon7954D0@@', 5, None),
    'React7954D8': (0x4c2510, 68, '?React@PauseIcon7954D8@@', 5, None),
    'React7954E0': (0x4c2560, 111, '?React@PauseIcon7954E0@@', 5, None),
    'React7954E8': (0x4c25d0, 111, '?React@PauseIcon7954E8@@', 5, None),
    'React7954F0': (0x4c26b0, 75, '?React@PauseIcon7954F0@@', 5, None),
    'React795510': (0x4c2700, 33, '?React@PauseIcon795510@@', 5, None),
    'React795518': (0x4c2730, 76, '?React@PauseIcon795518@@', 5, None),
    'React795520': (0x4c2780, 111, '?React@PauseIcon795520@@', 5, None),
    'React795528': (0x4c2820, 209, '?React@PauseIcon795528@@', 5, None),
    'IconScalarDestructor': (0x4aeb70, 30, None, 1, 'eax'),
}
VTABLES = {'PauseFNGObject': (0x797cb0, 7)}
VTABLES.update({'PauseIcon' + v: (int(v, 16), 2) for v in
                ('7954D0', '7954D8', '7954E0', '7954E8', '7954F0', '795510', '795518', '795520', '795528')})
GLOBALS = {
    'gameFlow8900D8': 0x8900d8, 'realTimer8651AC': 0x8651ac, 'gameMode850078': 0x850078,
    'raceType89E7B0': 0x89e7b0, 'value83AA04': 0x83aa04, 'pcHelpBar8384D8': 0x8384d8,
    'career85AD40': 0x85ad40, 'profileData83A9D0': 0x83a9d0, 'network89CF48': 0x89cf48,
    'replay8650B8': 0x8650b8, 'value88F1A8': 0x88f1a8, 'replayPackage8363E4': 0x8363e4,
    'value850098': 0x850098, 'value8635A8': 0x8635a8, 'value85BA67': 0x85ba67,
    'value8500D0': 0x8500d0, 'players8900AC': 0x8900ac,
}
# Rebuilt symbol: (retail address, stack arguments, thiscall, string argument indexes)
INTERFACES = {
    'FngHash': (0x505450, 1, False, (0,)), 'FngFindObject': (0x537a10, 2, False, ()),
    'FngSetLabelString': (0x50c900, 2, False, ()), 'FngSetPackageValue10': (0x50b8c0, 2, False, ()),
    'FngGetPackageValue10': (0x50b8a0, 1, False, ()), 'FngHelpBarName': (0x50cea0, 0, False, ()),
    'FngIsLoaded': (0x52cf60, 1, False, ()), 'FngRemove': (0x5379a0, 1, False, ()),
    'FngAdd': (0x555f10, 4, False, ()), 'FngShow': (0x537980, 3, False, (1,)),
    'FngSetControlMask': (0x537950, 2, False, ()), 'FngControlMask': (0x50d2d0, 1, False, ()),
    'FngScriptActive': (0x537cc0, 3, False, (2,)), 'FngSetScript': (0x537c00, 4, False, (2,)),
    'FngSendMessage': (0x55dda0, 2, False, ()), 'FngShowDialog': (0x558790, 9, False, (1,)),
    'FngSetReturnPackage': (0x50c8d0, 1, False, (0,)), 'ControllerAssigned': (0x571a60, 1, False, ()),
    'FngFilterSlot0C': (0x50d1d0, 2, False, ()),
    '??2@YAPAXI@Z': (0x575620, 1, False, ()), '??3@YAXPAX@Z': (0x575630, 1, False, ()),
    'GameFlow8900D8::Queue': (0x5f41e0, 2, True, ()), 'GameFlow8900D8::Queue8': (0x5f4240, 1, True, ()),
    'PCHelpBarFNGObject::Show': (0x54e6a0, 0, True, ()),
    'PCHelpBarFNGObject::SyncByMask': (0x54e6e0, 2, True, ()),
    'Career85AD40::Value513B30': (0x513b30, 0, True, ()), 'Career85AD40::Value5139C0': (0x5139c0, 0, True, ()),
    'ProfileData83A9D0::Value502FF0': (0x502ff0, 1, True, ()),
    'Network89CF48::Value5F0120': (0x5f0120, 0, True, ()),
    '??0IconScrollerFNGObject@@QAE@PAX@Z': (0x552330, 1, True, ()),
    '??1IconScrollerFNGObject@@UAE@XZ': (0x4defb0, 0, True, ()),
    'IconScrollerFNGObject::MessageHandler': (0x543d40, 4, True, ()),
    'IconScrollerFNGObject::StopInput': (0x52fbf0, 0, True, ()),
    'IconScrollerFNGObject::Value50DA20': (0x50da20, 4, True, ()),
    'IconScrollerFNGObject::SelectedIconChanged': (0x50da50, 0, True, ()),
    'IconScrollerFNGObject::AddIcon': (0x50dab0, 1, True, ()),
    'IconScrollerFNGObject::UnknownSlot04': (0x40d080, 1, True, ()),
    '??0IconScrollerIcon@@QAE@III@Z': (0x51f670, 3, True, ()),
    '??1IconScrollerIcon@@UAE@XZ': (0x527770, 0, True, ()),
}
ARENA, STACK, STOP, TIB, GDT = 0x2000000, 0x200f000, 0x2100000, 0x2120000, 0x2130000
THIS, SCROLLER_VTABLE, FLOW, HELPBAR, PLAYERS, REPLAY, ICON = (ARENA + 0x100, ARENA + 0x400, ARENA + 0x800, ARENA + 0x900,
                                                             ARENA + 0x600, ARENA + 0x2000, ARENA + 0x500)
HEAP, STRINGS = ARENA + 0xa000, ARENA + 0x9100
FAKE_SLOTS = {STOP + 0x300: ('Scroller::UnknownSlot04', 0, True, ()), STOP + 0x320: ('Scroller::UnknownSlot20', 1, True, ())}
WATCHED_GLOBALS = (0x83aa04, 0x8363e4)


def symbol_key(name):
    if name.startswith('??'):
        return name
    head = name[1:].split('@@')[0].split('@')
    return '::'.join(reversed(head))


def main():
    BUILD.mkdir(parents=True, exist_ok=True)
    raw = EXE.read_bytes()
    assert hashlib.sha256(raw).hexdigest() == 'f9dd86c054878ce6276beb07c1fd61874f7a1e4bf1f241b084c65b73e24168a7'
    image = pefile.PE(data=raw).get_memory_mapped_image()
    obj = BUILD / 'PauseMenu.obj'
    subprocess.run([str(ROOT / 'build/tools/wibo'), 'build/compilers/Win32/7.1/cl.exe', '/nologo', '/c', '/Ox', '/Ob0',
                    '/Z7', '/EHsc', '/Fo' + str(obj.relative_to(ROOT)), SOURCE], cwd=ROOT, check=True)
    symbols = read_object(obj)['symbols'].values()
    defined = {s['name'] for s in symbols if s['section'] > 0}
    externals = {'___CxxFrameHandler': 0x75bfd2, '__except_list': 0}
    for symbol in symbols:
        name = symbol['name']
        if symbol['section'] != 0 or symbol['storage'] != 2 or name in defined or name in externals:
            continue
        key = symbol_key(name)
        if key in GLOBALS:
            externals[name] = GLOBALS[key]
        elif key in INTERFACES:
            externals[name] = INTERFACES[key][0]
        else:
            raise AssertionError(name)
    # ??_E vector deleting destructors are weak externals aliasing ??_G.
    weak = {s['name'] for s in symbols if s['name'].startswith('??_E') and s['name'] not in defined}
    linked = link_objects([obj], dict(externals, **dict.fromkeys(weak, 0)))
    linked = link_objects([obj], dict(externals, **{n: linked['exports']['??_G' + n[4:]] for n in weak}))
    exports = linked['exports']
    entries = {n: next(a for s, a in exports.items() if s.startswith(f[2]) and s in defined)
               for n, f in FUNCTIONS.items() if f[2]}
    vtables = {exports[f'??_7{c}@@6B@']: a for c, (a, _) in VTABLES.items()}

    # Static check: every rebuilt vtable slot resolves to the retail slot target.
    rebuilt_to_retail = {entries[n]: FUNCTIONS[n][0] for n in entries}
    for c in VTABLES:
        rebuilt_to_retail[exports[f'??_G{c}@@UAEPAXI@Z']] = 0x4eb4f0 if c == 'PauseFNGObject' else 0x4aeb70
    blocks = dict(linked['blocks'])
    def linked_u32(address):
        for base, body in blocks.items():
            if base <= address < base + len(body):
                return struct.unpack_from('<I', body, address - base)[0]
        raise KeyError(hex(address))
    vtable_slots = 0
    for c, (retail, count) in VTABLES.items():
        rebuilt = exports[f'??_7{c}@@6B@']
        for i in range(count):
            want = struct.unpack_from('<I', image, retail - 0x400000 + i * 4)[0]
            got = linked_u32(rebuilt + i * 4)
            assert rebuilt_to_retail.get(got, got) == want, (c, i, hex(got), hex(want))
            vtable_slots += 1

    hooks = {a: (n, count, this, strings) for n, (a, count, this, strings) in INTERFACES.items()}
    hooks.update(FAKE_SLOTS)
    original_ranges = [(a, a + n) for a, n, *_ in FUNCTIONS.values()]
    state = {}
    machines = []

    def u32(uc, address):
        return struct.unpack('<I', uc.mem_read(address, 4))[0]

    def cstring(uc, address):
        return bytes(uc.mem_read(address, 64)).split(b'\0')[0].decode('ascii')

    def normalise(data, rebuilt):
        if not rebuilt:
            return bytes(data)
        data = bytearray(data)
        for offset in range(0, len(data) - 3, 4):
            value = struct.unpack_from('<I', data, offset)[0]
            if value in vtables:
                struct.pack_into('<I', data, offset, vtables[value])
        return bytes(data)

    for rebuilt in (False, True):
        uc = Uc(UC_ARCH_X86, UC_MODE_32)
        uc.mem_map(0x400000, (len(image) + 4095) & ~4095)
        uc.mem_write(0x400000, image)
        uc.mem_map(ARENA, 0x10000)
        uc.mem_map(STOP, 0x20000)
        uc.mem_map(TIB, 0x1000)
        uc.mem_map(linked['address'], linked['allocation'])
        for address, body in linked['blocks']:
            uc.mem_write(address, body)
        # Flat code/data descriptors plus FS based at TIB for the fs:[0] SEH frames.
        uc.mem_map(GDT, 0x1000)
        def descriptor(base, limit, access, flags=0xc):
            return ((limit & 0xffff) | (base & 0xffffff) << 16 | access << 40 | (limit >> 16 & 0xf) << 48
                    | flags << 52 | (base >> 24) << 56)
        uc.mem_write(GDT + 8, struct.pack('<3Q', descriptor(0, 0xfffff, 0x9b), descriptor(0, 0xfffff, 0x93),
                                          descriptor(TIB, 0xfff, 0x93, 0x4)))
        uc.reg_write(UC_X86_REG_GDTR, (0, GDT, 0x1f, 0))
        for register, selector in ((UC_X86_REG_CS, 8), (UC_X86_REG_SS, 16), (UC_X86_REG_DS, 16),
                                   (UC_X86_REG_ES, 16), (UC_X86_REG_FS, 24)):
            uc.reg_write(register, selector)

        def hook(uc, address, size, user, rebuilt=rebuilt):
            if address not in hooks:
                ranges = original_ranges if not rebuilt else [(linked['address'], linked['address'] + linked['allocation'])]
                assert address == STOP or any(a <= address < b for a, b in ranges), ('unexpected execution', rebuilt, hex(address))
                return
            name, count, thiscall, strings = hooks[address]
            sp = uc.reg_read(UC_X86_REG_ESP)
            args = [u32(uc, sp + 4 + i * 4) for i in range(count)]
            for i in strings:
                args[i] = cstring(uc, args[i])
            ecx = uc.reg_read(UC_X86_REG_ECX)
            record = [name, ecx if thiscall else None, args]
            result = 0
            if name == 'IconScrollerFNGObject::AddIcon':
                record.append(normalise(uc.mem_read(args[0], 0x48), rebuilt).hex() if args[0] else None)
            elif name == 'FngHash':
                result = 0xffffffff
                for c in args[0].upper().encode():
                    result = (result * 33 + c) & 0xffffffff
            elif name == 'FngFindObject':
                result = (args[0] ^ args[1] * 0x9e3779b1) & 0xffffffff
            elif name == 'FngGetPackageValue10':
                result = state['package_value']
            elif name == 'FngHelpBarName':
                result = STRINGS
            elif name == 'FngIsLoaded':
                result = 0x12345600 | state['loaded']
            elif name == 'FngControlMask':
                result = args[0] ^ 0x5a5a00
            elif name == 'FngScriptActive':
                result = 0x12345600 | state['script']
            elif name == 'ControllerAssigned':
                result = state['assigned']
            elif name == 'FngFilterSlot0C':
                result = (args[0] * 3 + args[1]) & 0xffffffff
            elif name == '??2@YAPAXI@Z':
                result = 0 if state['null_new'] else HEAP + state['allocations'] * 0x50
                state['allocations'] += 1
            elif name == 'ProfileData83A9D0::Value502FF0':
                result = STRINGS + 0x40
            elif name == 'Network89CF48::Value5F0120':
                result = 0x12345600 | state['network']
            elif name == '??0IconScrollerFNGObject@@QAE@PAX@Z':
                # Stand-in for the base state the pause routines read.
                uc.mem_write(ecx + 4, struct.pack('<I', STRINGS + 0x80))
                uc.mem_write(ecx + 0x20, struct.pack('<I', 0x31))
                uc.mem_write(ecx + 0x4c, struct.pack('<I', SCROLLER_VTABLE))
                result = ecx
            elif name == '??0IconScrollerIcon@@QAE@III@Z':
                result = ecx
            state['trace'].append(tuple(record))
            uc.reg_write(UC_X86_REG_EAX, result)
            uc.reg_write(UC_X86_REG_ESP, sp + 4 + (count * 4 if thiscall else 0))
            uc.reg_write(UC_X86_REG_EIP, u32(uc, sp))
        uc.hook_add(UC_HOOK_CODE, hook)
        machines.append(uc)

    counts = dict.fromkeys(FUNCTIONS, 0)

    def compare(name, args=(), mode=1, race=0, value8500d0=0, value8635a8=0, value85ba67=0, players=(), flow=(1, 0),
                timer=(1, 2), loaded=0, script=0, assigned=1, network=0, value850098=0, replay=(1, 1, 0), value13c=0,
                package_value=0, null_new=0, prepared=True):
        outputs = []
        for rebuilt, uc in zip((False, True), machines):
            memory = bytearray(0x10000)
            struct.pack_into('<II', memory, THIS - ARENA, exports['??_7PauseFNGObject@@6B@'] if rebuilt else 0x797cb0, STRINGS + 0x80)
            struct.pack_into('<I', memory, THIS - ARENA + 0x20, 0x31)
            struct.pack_into('<I', memory, THIS - ARENA + 0x4c, SCROLLER_VTABLE)
            if not prepared:
                memory[THIS - ARENA:THIS - ARENA + 0x158] = bytes(0x158)
            struct.pack_into('<I', memory, THIS - ARENA + 0x13c, value13c)
            struct.pack_into('<I', memory, THIS - ARENA + 0x150, timer[0])
            struct.pack_into('<II', memory, SCROLLER_VTABLE - ARENA + 4, STOP + 0x300, 0)
            struct.pack_into('<I', memory, SCROLLER_VTABLE - ARENA + 0x20, STOP + 0x320)
            struct.pack_into('<i', memory, FLOW - ARENA, flow[0])
            struct.pack_into('<ii', memory, FLOW - ARENA + 0x20, 1, flow[1])
            struct.pack_into('<i', memory, REPLAY - ARENA + 0xb8, replay[1])
            memory[REPLAY - ARENA + 0x50ec] = replay[0]
            memory[STRINGS - ARENA:STRINGS - ARENA + 8] = b'HelpBar\0'
            memory[STRINGS - ARENA + 0x40:STRINGS - ARENA + 0x4c] = b'Profile.fng\0'
            memory[STRINGS - ARENA + 0x80:STRINGS - ARENA + 0x8a] = b'Pause.fng\0'
            struct.pack_into('<I', memory, ICON - ARENA, exports['??_7PauseIcon7954D0@@6B@'] if rebuilt else 0x7954d0)
            table = [0, 0]
            for i, (player, value50) in enumerate(players):
                if player is not None:
                    struct.pack_into('<ii', memory, PLAYERS - ARENA + i * 0x80 + 8, player, 0)
                    struct.pack_into('<i', memory, PLAYERS - ARENA + i * 0x80 + 0x50, value50)
                    table[i] = PLAYERS + i * 0x80
            uc.mem_write(ARENA, bytes(memory))
            uc.mem_write(TIB, struct.pack('<I', 0xffffffff))
            for address, data in ((0x8900d8, struct.pack('<I', FLOW)), (0x8651ac, struct.pack('<I', timer[1])),
                                  (0x850078, struct.pack('<i', mode)), (0x89e7b0, struct.pack('<i', race)),
                                  (0x83aa04, struct.pack('<i', 0x77)), (0x8384d8, struct.pack('<I', HELPBAR)),
                                  (0x8650b8, struct.pack('<I', REPLAY)), (0x88f1a8, struct.pack('<i', replay[2])),
                                  (0x8363e4, struct.pack('<I', 0x66)), (0x850098, bytes([value850098])),
                                  (0x8635a8, bytes([value8635a8])), (0x85ba67, bytes([value85ba67])),
                                  (0x8500d0, struct.pack('<i', value8500d0)), (0x8900ac, struct.pack('<II', *table))):
                uc.mem_write(address, data)
            this = ICON if name.startswith('React') or name == 'IconScalarDestructor' else HEAP + 0x800 if name.startswith('Icon') else THIS
            uc.mem_write(STACK, struct.pack('<I', STOP) + b''.join(struct.pack('<I', a) for a in args))
            uc.reg_write(UC_X86_REG_ESP, STACK)
            uc.reg_write(UC_X86_REG_ECX, this)
            uc.reg_write(UC_X86_REG_EAX, 0xdeadbeef)
            for reg in (UC_X86_REG_EBX, UC_X86_REG_ESI, UC_X86_REG_EDI, UC_X86_REG_EBP):
                uc.reg_write(reg, 0x12345678)
            state.update(trace=[], loaded=loaded, script=script, assigned=assigned, network=network,
                         package_value=package_value, null_new=null_new, allocations=0)
            start = FUNCTIONS[name][0]
            if rebuilt:
                start = entries[name] if name != 'IconScalarDestructor' else exports['??_GPauseIcon7954D0@@UAEPAXI@Z']
            try:
                uc.emu_start(start, STOP, count=200000)
            except Exception:
                print(name, rebuilt, 'eip', hex(uc.reg_read(UC_X86_REG_EIP)), 'esp', hex(uc.reg_read(UC_X86_REG_ESP)), state['trace'][-3:])
                raise
            assert uc.reg_read(UC_X86_REG_EIP) == STOP, (name, rebuilt, hex(uc.reg_read(UC_X86_REG_EIP)))
            stack_args = FUNCTIONS[name][3]
            thiscall = name not in ('CreatePauseFNGObject',)
            assert uc.reg_read(UC_X86_REG_ESP) == STACK + 4 + (stack_args * 4 if thiscall else 0), (name, rebuilt, hex(uc.reg_read(UC_X86_REG_ESP)))
            assert all(uc.reg_read(r) == 0x12345678 for r in (UC_X86_REG_EBX, UC_X86_REG_ESI, UC_X86_REG_EDI, UC_X86_REG_EBP)), (name, rebuilt)
            assert u32(uc, TIB) == 0xffffffff, (name, rebuilt)
            kind = FUNCTIONS[name][4]
            result = uc.reg_read(UC_X86_REG_EAX) & (0xff if kind == 'al' else 0xffffffff) if kind else None
            outputs.append((state['trace'], result, normalise(uc.mem_read(ARENA, STACK - 0x1000 - ARENA), rebuilt),
                            b''.join(bytes(uc.mem_read(a, 4)) for a in WATCHED_GLOBALS)))
        if outputs[0] != outputs[1]:
            for a, b in zip(outputs[0][0], outputs[1][0]):
                if a != b:
                    print('trace differs', a, b)
                    break
            print('results', outputs[0][1], outputs[1][1], 'globals', outputs[0][3].hex(), outputs[1][3].hex())
            m0, m1 = outputs[0][2], outputs[1][2]
            diffs = [hex(ARENA + i) for i in range(len(m0)) if m0[i] != m1[i]][:16]
            raise AssertionError((name, len(outputs[0][0]), len(outputs[1][0]), diffs))
        counts[name] += 1

    modes = (0, 1, 2, 4, 8, 0x200)
    player_cases = ((), ((1, 0),), ((1, 5),), ((0, 0), (1, 7)), ((0, 0),), ((None, 0), (1, 0)))
    for mode, race, value8500d0, value8635a8, value85ba67, players in itertools.product(
            modes, (0, 1, 7), (0, 2, 3, 4), (0, 1), (0, 1), player_cases):
        common = dict(mode=mode, race=race, value8500d0=value8500d0, value8635a8=value8635a8,
                      value85ba67=value85ba67, players=players)
        compare('SetupFreeroam', **common)
        compare('SetupRace', **common)
        if value8500d0 in (0, 3) and players in ((), ((1, 5),)):
            compare('OptionsAvailable', **common)
            for loaded in (0, 1):
                compare('Setup', loaded=loaded, package_value=race * 3, **common)
                compare('Constructor', args=(ARENA + 0x700,), loaded=loaded, prepared=False, **common)
                compare('CreatePauseFNGObject', args=(ARENA + 0x700,), loaded=loaded, prepared=False, **common)
    compare('CreatePauseFNGObject', args=(ARENA + 0x700,), null_new=1, prepared=False)
    for players, race in itertools.product(player_cases, (0, 1, 7)):
        compare('OptionsAvailable', players=players, race=race)
    for loaded, flags in itertools.product((0, 1), (0, 1, 2, 3)):
        compare('Destructor', loaded=loaded)
        compare('ScalarDestructor', args=(flags,), loaded=loaded)
        compare('IconScalarDestructor', args=(flags,))
    for timer, assigned in itertools.product(((1, 1), (1, 2)), (0, 1, 0x100)):
        compare('AcceptsInput', timer=timer, assigned=assigned)
    for mode, loaded in itertools.product(modes, (0, 1)):
        compare('QuitToCrib', mode=mode, loaded=loaded)
        compare('QuitToCribKeepHelpBar', mode=mode, loaded=loaded)
        compare('QuitToProfile', mode=mode, loaded=loaded)
    for args in ((1, 2, 3, 4, 5), (0xffffffff, 0, 0, 0, 0x80000000)):
        compare('UnknownSlot0C', args=args)
    cases = (0x451e768e, 0x30f32a49, 0x0c407210, 0x13999fc2, 0x4a05ae7f, 0x30ed2368, 0x33c42e03, 0x43da9fd0,
             0xb4623f67, 0x911ab364, 0xb5af2461, 0xe1a57d51)
    messages = cases + (0xe1fde1d1, 0, 0x0c40720f, 0x30ed2369, 0xffffffff)
    for message, timer, assigned, loaded, script, mode in itertools.product(
            messages, ((1, 1), (1, 2)), (0, 1), (0, 1), (0, 1), (1, 2)):
        for value13c in (cases + (0, 0x12345678)) if message == 0xe1fde1d1 else (0,):
            compare('MessageHandler', args=(message, 0x11, 0x22, 0x33), timer=timer, assigned=assigned,
                    loaded=loaded, script=script, mode=mode, value13c=value13c)
    for replay, args in itertools.product(((0, 1, 0), (1, 0, 0), (1, -1, 0), (1, 1, 1), (1, 1, 0)),
                                          ((1, 2, 0), (0x84d1d864, 0x7840428a, 0))):
        compare('Icon7954F0', args=args, replay=replay)
        compare('Icon795528', args=args)
    reacts = [n for n in FUNCTIONS if n.startswith('React')]
    for name, message, mode, flow, network, value850098, replay, loaded in itertools.product(
            reacts, (0x0c407210, 0x0c407211), modes, ((6, 4), (6, 3), (5, 4)), (0, 1), (0, 1),
            ((1, 1, 0), (0, 1, 0)), (0, 1)):
        compare(name, args=(STRINGS + 0x80, message, 1, 2, 3), mode=mode, flow=flow, network=network,
                value850098=value850098, replay=replay, loaded=loaded)

    report = {'reference_sha256': hashlib.sha256(raw).hexdigest(), 'comparisons': counts, 'total': sum(counts.values()),
              'fingerprints': {n: {'address': hex(a), 'span': size, 'sha256': hashlib.sha256(image[a - 0x400000:a - 0x400000 + size]).hexdigest()}
                               for n, (a, size, *_) in FUNCTIONS.items()},
              'vtables': {c: hex(a) for c, (a, _) in VTABLES.items()}, 'vtable_slots_checked': vtable_slots,
              'interfaces': {n: hex(v[0]) for n, v in INTERFACES.items()}, 'byte_matching': False,
              'execution_guard': True}
    (BUILD / 'verification.json').write_text(json.dumps(report, indent=2) + '\n')
    print(json.dumps(counts, indent=2))
    print('PASS', report['total'], 'comparisons,', vtable_slots, 'vtable slots')


if __name__ == '__main__':
    main()
