#!/usr/bin/env python3
"""Compare the original in-game options screens and option items.

Covers UI_PauseOptionsMain.fng (PauseOptionsMainFNGObject, category icons) and
UI_PauseOptions.fng (PauseOptionsFNGObject, the four category setups and every
option's React/SetLabels/InitSlider). FEng, IconScroller, the options list base,
the option item bases and game-setting routines are controlled interfaces hooked
at their retail addresses in both machines. Rebuilt vtables are checked slot by
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

BUILD = ROOT / 'build/analysis/pause-options'
SOURCE = 'src/Speed/Indep/Src/Frontend/PauseOptions.cpp'

# name: (original address, span, rebuilt symbol prefix, stack arguments, returns, this kind)
FUNCTIONS = {
    'CreateMain': (0x4f47f0, 95, '?CreatePauseOptionsMainFNGObject@@', 1, 'eax', None),
    'MainConstructor': (0x4eb580, 160, '??0PauseOptionsMainFNGObject@@', 1, 'eax', 'main'),
    'MainScalarDestructor': (0x4eb620, 30, '??_GPauseOptionsMainFNGObject@@', 1, 'eax', 'main'),
    'MainDestructor': (0x4eb640, 11, '??1PauseOptionsMainFNGObject@@', 0, None, 'main'),
    'MainUnknownSlot0C': (0x49dfc0, 21, '?UnknownSlot0C@PauseOptionsMainFNGObject@@', 5, 'eax', 'main'),
    'MainSetup': (0x4c3000, 463, '?Setup@PauseOptionsMainFNGObject@@', 0, None, 'main'),
    'MainMessageHandler': (0x4a7d60, 519, '?MessageHandler@PauseOptionsMainFNGObject@@', 4, None, 'main'),
    'IconReact7918B8': (0x49a620, 23, '?React@PauseOptionsIcon7918B8@@', 5, None, 'icon'),
    'IconReact7918C0': (0x49a640, 23, '?React@PauseOptionsIcon7918C0@@', 5, None, 'icon'),
    'IconReact7918C8': (0x49a660, 23, '?React@PauseOptionsIcon7918C8@@', 5, None, 'icon'),
    'IconReact7918D0': (0x49a680, 23, '?React@PauseOptionsIcon7918D0@@', 5, None, 'icon'),
    'CreateOptions': (0x4eb650, 95, '?CreatePauseOptionsFNGObject@@', 1, 'eax', None),
    'OptionsConstructor': (0x4d93c0, 162, '??0PauseOptionsFNGObject@@', 1, 'eax', 'options'),
    'OptionsScalarDestructor': (0x4eb6b0, 30, '??_GPauseOptionsFNGObject@@', 1, 'eax', 'options'),
    'OptionsDestructor': (0x4d9470, 11, '??1PauseOptionsFNGObject@@', 0, None, 'options'),
    'OptionsSetup': (0x4c4bd0, 169, '?Setup@PauseOptionsFNGObject@@', 0, None, 'options'),
    'PromptSave': (0x4a7f70, 159, '?PromptSave@PauseOptionsFNGObject@@', 1, 'al', 'options'),
    'OptionsMessageHandler': (0x4a8010, 945, '?MessageHandler@PauseOptionsFNGObject@@', 4, None, 'options'),
    'SetupAudio': (0x4c3370, 837, '?SetupAudio@PauseOptionsFNGObject@@', 0, None, 'options'),
    'SetupDisplay': (0x4c36c0, 546, '?SetupDisplay@PauseOptionsFNGObject@@', 0, None, 'options'),
    'SetupGameplay': (0x4c38f0, 886, '?SetupGameplay@PauseOptionsFNGObject@@', 0, None, 'options'),
    'SetupPlayer': (0x4c4740, 1157, '?SetupPlayer@PauseOptionsFNGObject@@', 0, None, 'options'),
    'PlayerSlot': (0x4a7ae0, 67, '?PlayerSlot@@', 0, 'eax', None),
}
# Option classes: vtable -> (kind, React, SetLabels, InitSlider, (span, span, span)).
OPTIONS = {
    '7918F8': ('slider', 0x49a720, 0x49a770, 0x49a790), '791A88': ('slider', 0x49aa40, 0x49aa90, 0x49aab0),
    '791AD8': ('slider', 0x49aae0, 0x49ab30, 0x49ab50), '791A38': ('slider', 0x49a9a0, 0x49a9f0, 0x49aa10),
    '791948': ('slider', 0x49a7c0, 0x49a810, 0x49a830), '791998': ('slider', 0x49a860, 0x49a8b0, 0x49a8d0),
    '7919E8': ('slider', 0x49a900, 0x49a950, 0x49a970),
    '791F30': ('toggle', 0x49b600, 0x49b690, None), '791F78': ('slider', 0x49b740, 0x49b7a0, 0x49b7c0),
    '792060': ('button', 0x49b930, 0x49b960, None), '7920A0': ('button', 0x49b980, 0x49b9e0, None),
    '791D20': ('toggle', 0x49af20, 0x49af70, None), '791C00': ('toggle', 0x49ad40, 0x49adc0, None),
    '791C48': ('toggle', 0x49ae00, 0x49ae40, None), '791C90': ('toggle', 0x49ae80, 0x49aee0, None),
    '791D68': ('toggle', 0x49b0b0, 0x49b100, None), '791CD8': ('toggle', 0x49afe0, 0x49b040, None),
    '793D68': ('toggle', 0x49b170, 0x49b210, None),
    '795660': ('toggle', 0x4c3c70, 0x4c3d80, None), '7955F0': ('toggle', 0x4c31d0, 0x4c32e0, None),
    '7956A8': ('toggle', 0x4c3e10, 0x4c3e60, None), '791DB0': ('toggle', 0x49b260, 0x49b2b0, None),
    '7956F0': ('toggle', 0x4c3f50, 0x4c4010, None), '795738': ('toggle', 0x4c4090, 0x4c4150, None),
    '795780': ('toggle', 0x4c41d0, 0x4c4290, None), '795810': ('toggle', 0x4c4450, 0x4c4510, None),
    '795858': ('toggle', 0x4c4600, 0x4c46c0, None), '7957C8': ('toggle', 0x4c4310, 0x4c43d0, None),
}
SPANS = {0x49a720: 71, 0x49a770: 18, 0x49a790: 47, 0x49b600: 131, 0x49b690: 151, 0x49b740: 82, 0x49b7a0: 18,
         0x49b7c0: 47, 0x49b930: 38, 0x49b960: 18, 0x49b980: 83, 0x49b9e0: 18, 0x49af20: 70, 0x49af70: 97,
         0x49ad40: 116, 0x49adc0: 64, 0x49ae00: 57, 0x49ae40: 51, 0x49ae80: 88, 0x49aee0: 51, 0x49b0b0: 72,
         0x49b100: 99, 0x49afe0: 83, 0x49b040: 99, 0x49b170: 147, 0x49b210: 78, 0x4c3c70: 271, 0x4c3d80: 144,
         0x4c31d0: 271, 0x4c32e0: 144, 0x4c3e10: 67, 0x4c3e60: 210, 0x49b260: 67, 0x49b2b0: 61, 0x4b5f70: 44,
         0x4b5fa0: 44, 0x4c3f50: 181, 0x4c4010: 119, 0x4c4090: 181, 0x4c4150: 119, 0x4c41d0: 181, 0x4c4290: 119,
         0x4c4450: 189, 0x4c4510: 211, 0x4c4600: 181, 0x4c46c0: 119, 0x4c4310: 181, 0x4c43d0: 119}
for v, (kind, react, labels, init) in OPTIONS.items():
    FUNCTIONS['React' + v] = (react, SPANS.get(react, 71), f'?React@PauseOption{v}@@', 2, None, 'option:' + v)
    FUNCTIONS['SetLabels' + v] = (labels, SPANS.get(labels, 18), f'?SetLabels@PauseOption{v}@@', 0, None, 'option:' + v)
    if init:
        FUNCTIONS['InitSlider' + v] = (init, SPANS.get(init, 47), f'?InitSlider@PauseOption{v}@@', 0, None, 'option:' + v)
FUNCTIONS['Enable791DB0'] = (0x4b5f70, 44, '?Enable@PauseOption791DB0@@', 0, None, 'option:791DB0')
FUNCTIONS['Disable791DB0'] = (0x4b5fa0, 44, '?Disable@PauseOption791DB0@@', 0, None, 'option:791DB0')

PLAYER_OPTIONS = ('795660', '7955F0', '7956A8', '7956F0', '795738', '795780', '795810', '795858', '7957C8')
SLOTS = {'slider': 19, 'toggle': 17, 'button': 16}
SCALAR = {'slider': 0x4cf620, 'toggle': 0x4aea10, 'button': 0x4aea10}
VTABLES = {'PauseOptionsMainFNGObject': (0x797ccc, 7, 0x4eb620), 'PauseOptionsFNGObject': (0x796504, 5, 0x4eb6b0)}
VTABLES.update({'PauseOptionsIcon' + v: (int(v, 16), 2, 0x4aeb70) for v in ('7918B8', '7918C0', '7918C8', '7918D0')})
VTABLES.update({'PauseOption' + v: (int(v, 16), SLOTS[k[0]], SCALAR[k[0]]) for v, k in OPTIONS.items()})

GLOBALS = {
    'gameFlow8900D8': 0x8900d8, 'pcHelpBar8384D8': 0x8384d8, 'replay8650B8': 0x8650b8, 'players8900AC': 0x8900ac,
    'gameMode850078': 0x850078, 'value83AA04': 0x83aa04, 'value89E7E6': 0x89e7e6, 'value89E7D8': 0x89e7d8,
    'value890108': 0x890108, 'volumes83AA28': 0x83aa28, 'audio82B884': 0x82b884, 'jumpCamera83AA20': 0x83aa20,
    'mirror83AA18': 0x83aa18, 'units83AA1A': 0x83aa1a, 'raceMap83AA1B': 0x83aa1b, 'exploreMap83AA1C': 0x83aa1c,
    'difficulty83AC30': 0x83ac30, 'playerSettings83AA64': 0x83aa64, 'settings83AAB4': 0x83aab4,
    'memoryCard83A9E0': 0x83a9e0, 'prompt844AD8': 0x844ad8, 'autosave844AD9': 0x844ad9,
    'resolution870D1C': 0x870d1c, 'resolutions86E880': 0x86e880, 'levelOfDetail87099C': 0x87099c,
    'displayChanged8709D1': 0x8709d1, 'careerData85AD40': 0x85ad40,
}
# Rebuilt symbol key: (retail address, stack arguments, thiscall, string argument indexes)
INTERFACES = {
    'FngHash': (0x505450, 1, False, (0,)), 'FngFindObject': (0x537a10, 2, False, ()),
    'FngFindObject5379C0': (0x5379c0, 2, False, ()), 'FngSetLabelString': (0x50c900, 2, False, ()),
    'FngSetPackageValue10': (0x50b8c0, 2, False, (0,)), 'FngGetPackageValue10': (0x50b8a0, 1, False, (0,)),
    'FngHelpBarName': (0x50cea0, 0, False, ()), 'FngIsLoaded': (0x52cf60, 1, False, (0,)),
    'FngRemove': (0x5379a0, 1, False, (0,)), 'FngAdd': (0x555f10, 4, False, (0, 1)), 'FngShow': (0x537980, 3, False, (0, 1)),
    'FngScriptActive': (0x537cc0, 3, False, (0, 2)), 'FngSetScript': (0x537c00, 4, False, (0, 2)),
    'FngScriptRunning': (0x537d40, 3, False, (0,)), 'FngSetScriptHash': (0x537c60, 4, False, (0,)),
    'FngSendMessage': (0x55dda0, 2, False, (1,)), 'FngShowDialog': (0x558790, 9, False, (0, 1)),
    'FngHideObject': (0x50ca00, 1, False, ()), 'FngShowObject': (0x50ca50, 1, False, ()),
    'FngPrintf': (0x537be0, 2, False, (1,)), 'FngSetObjectScript': (0x51cf70, 3, False, (1,)),
    'FngPlayMenuSound': (0x50d150, 3, False, ()), 'ControllerAssigned': (0x571a60, 1, False, ()),
    'FngFilterSlot0C': (0x50d1d0, 2, False, ()),
    'CareerData85AD40::Query500B60': (0x500b60, 0, True, ()), 'Value5B8240': (0x5b8240, 0, False, ()),
    'SetPresetDisplayLevelFromLevelOfDetailAndLimit': (0x5be9f0, 0, False, ()),
    'SaveRegistrySettings': (0x5beea0, 0, False, ()), 'Value5CEC90': (0x5cec90, 0, False, ()),
    '??2@YAPAXI@Z': (0x575620, 1, False, ()), '??3@YAXPAX@Z': (0x575630, 1, False, ()),
    'GameFlow8900D8::Queue8': (0x5f4240, 1, True, ()),
    'PCHelpBarFNGObject::Show': (0x54e6a0, 0, True, ()), 'PCHelpBarFNGObject::SyncByMask': (0x54e6e0, 2, True, ()),
    'Audio82B884::Apply': (0x469050, 1, True, ()), 'Settings83AAB4::Value5026F0': (0x5026f0, 0, True, ()),
    'MemoryCard83A9E0::Value502FE0': (0x502fe0, 0, True, ()), 'Replay8650B8::Value578EF0': (0x578ef0, 4, True, ()),
    'Player8900AC::Value5F2FA0': (0x5f2fa0, 1, True, ()), 'UIProgressBar::SetValue': (0x4ff550, 1, True, ()),
    '??0IconScrollerFNGObject@@QAE@PAX@Z': (0x552330, 1, True, ()),
    '??1IconScrollerFNGObject@@UAE@XZ': (0x4defb0, 0, True, ()),
    'IconScrollerFNGObject::MessageHandler': (0x543d40, 4, True, ()),
    'IconScrollerFNGObject::StopInput': (0x52fbf0, 0, True, ()),
    'IconScrollerFNGObject::Value50DA20': (0x50da20, 4, True, ()),
    'IconScrollerFNGObject::SelectedIconChanged': (0x50da50, 0, True, ()),
    'IconScrollerFNGObject::AddIcon': (0x50dab0, 1, True, ()),
    'IconScrollerFNGObject::UnknownSlot04': (0x40d080, 1, True, ()),
    '??0IconScrollerIcon@@QAE@III@Z': (0x51f670, 3, True, ()), '??1IconScrollerIcon@@UAE@XZ': (0x527770, 0, True, ()),
    '??0OptionsScrollerFNGObject@@QAE@PAX@Z': (0x552780, 1, True, ()),
    '??1OptionsScrollerFNGObject@@UAE@XZ': (0x4cbc30, 0, True, ()),
    'OptionsScrollerFNGObject::MessageHandler': (0x556020, 4, True, ()),
    'OptionsScrollerFNGObject::UnknownSlot04': (0x40d080, 1, True, ()),
    'OptionsScrollerFNGObject::UnknownSlot0C': (0x570a70, 5, True, ()),
    'OptionsScrollerFNGObject::AddOption54EF30': (0x54ef30, 2, True, ()),
    'OptionsScrollerFNGObject::AddOption54F170': (0x54f170, 2, True, ()),
    'OptionsScrollerFNGObject::AddSlider54F410': (0x54f410, 2, True, ()),
    'OptionsScrollerFNGObject::AddSlider54F800': (0x54f800, 2, True, ()),
    'OptionsScrollerFNGObject::AddButton': (0x54ee40, 1, True, ()),
    'OptionsScrollerFNGObject::Item': (0x51fe70, 1, True, ()),
    'OptionsScrollerFNGObject::StoreMessage': (0x50dae0, 4, True, ()),
    'OptionsScrollerFNGObject::Select': (0x539650, 1, True, ()),
    '??0OptionButton@@QAE@PAM0E@Z': (0x51dfc0, 3, True, ()), '??0OptionToggle@@QAE@PAM000E@Z': (0x51e230, 5, True, ()),
    '??0OptionSlider@@QAE@PAM000E@Z': (0x51e840, 5, True, ()),
    '??1OptionItem@@UAE@XZ': (0x4cf650, 0, True, ()), '??1OptionSlider@@UAE@XZ': (0x4cf640, 0, True, ()),
    'OptionItem::Enable': (0x50d7c0, 0, True, ()), 'OptionItem::Disable': (0x50d7d0, 0, True, ()),
    'OptionItem::UnknownSlot30': (0x4047f0, 1, True, ()), 'OptionItem::UnknownSlot34': (0x4ae9b0, 0, True, ()),
    'OptionItem::SoundColumn': (0x50d500, 2, True, ()),
    'OptionButton::Click': (0x52d650, 3, True, ()), 'OptionButton::UnknownSlot10': (0x52d6b0, 0, True, ()),
    'OptionButton::Show': (0x50d530, 0, True, ()), 'OptionButton::Hide': (0x50d550, 0, True, ()),
    'OptionButton::Highlight': (0x52d770, 1, True, ()), 'OptionButton::UnHighlight': (0x51e020, 0, True, ()),
    'OptionButton::UnknownSlot2C': (0x4ae9c0, 0, True, ()), 'OptionButton::SoundColumn': (0x50d800, 2, True, ()),
    'OptionButton::SoundRow': (0x40cbb0, 0, True, ()),
    'OptionToggle::Click': (0x52d9f0, 3, True, ()), 'OptionToggle::UnknownSlot10': (0x52dac0, 0, True, ()),
    'OptionToggle::Show': (0x50d5d0, 0, True, ()), 'OptionToggle::Hide': (0x50d610, 0, True, ()),
    'OptionToggle::Enable': (0x51e270, 0, True, ()), 'OptionToggle::Disable': (0x51e3e0, 0, True, ()),
    'OptionToggle::Highlight': (0x52db40, 1, True, ()), 'OptionToggle::UnHighlight': (0x51e550, 0, True, ()),
    'OptionToggle::UnknownSlot2C': (0x4ae9c0, 0, True, ()), 'OptionToggle::UnknownSlot30': (0x52d930, 1, True, ()),
    'OptionToggle::UnknownSlot34': (0x52d990, 0, True, ()), 'OptionToggle::SoundRow': (0x408cd0, 0, True, ()),
    'OptionToggle::AnimateArrows': (0x51e6b0, 1, True, ()),
    'OptionSlider::Click': (0x52d9f0, 3, True, ()), 'OptionSlider::UnknownSlot10': (0x52def0, 0, True, ()),
    'OptionSlider::Show': (0x50d740, 0, True, ()), 'OptionSlider::Hide': (0x50d780, 0, True, ()),
    'OptionSlider::Highlight': (0x52dff0, 1, True, ()), 'OptionSlider::UnHighlight': (0x51e8d0, 0, True, ()),
    'OptionSlider::UnknownSlot2C': (0x4ae9c0, 0, True, ()), 'OptionSlider::UnknownSlot30': (0x52d930, 1, True, ()),
    'OptionSlider::UnknownSlot34': (0x52d990, 0, True, ()), 'OptionSlider::SoundColumn': (0x50d6f0, 2, True, ()),
    'OptionSlider::SoundRow': (0x40cb20, 0, True, ()), 'OptionSlider::AnimateArrows': (0x51e6b0, 1, True, ()),
}
# Retail code both machines may run: OptionSlider::GetValue (fld [ecx+0x84]) and __ftol2.
SHARED_CODE = {'OptionSlider::GetValue': (0x45d300, 7), '__ftol2': (0x75bed4, 0x75)}

ARENA, STACK, STOP, TIB, GDT = 0x2000000, 0x200f000, 0x2100000, 0x2120000, 0x2130000
THIS, ICON, SCROLLER_VTABLE, PLAYERS, FLOW, HELPBAR = (ARENA + 0x100, ARENA + 0x500, ARENA + 0x400, ARENA + 0x600,
                                                       ARENA + 0x800, ARENA + 0x900)
OPTION, RESOLUTION, ITEM, BAR_VTABLE, ITEM_VTABLE = (ARENA + 0x1000, ARENA + 0x1100, ARENA + 0x1200, ARENA + 0x1400,
                                                     ARENA + 0x1480)
REPLAY, AUDIO, STRINGS, HEAP = ARENA + 0x1800, ARENA + 0x1700, ARENA + 0x9100, ARENA + 0xa000
NAME_OBJECT, DATA_OBJECT = 0x11110000, 0x22220000
FAKE_SLOTS = {STOP + 0x300: ('Scroller::UnknownSlot20', 1, True, ()),
              STOP + 0x340: ('UIProgressBar::HandleMessage', 1, True, ()),
              STOP + 0x360: ('Item::SetLabels', 0, True, ()), STOP + 0x380: ('Item::Disable', 0, True, ())}
WATCHED = ((0x83aa04, 4), (0x83aa18, 0x2c), (0x83aa64, 0x50), (0x83ac30, 4), (0x844ad8, 2), (0x870d1c, 4),
           (0x87099c, 4), (0x8709d1, 1))
CONSTRUCTORS = {'??0OptionButton@@QAE@PAM0E@Z': 2, '??0OptionToggle@@QAE@PAM000E@Z': 4,
                '??0OptionSlider@@QAE@PAM000E@Z': 4}


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
    obj = BUILD / 'PauseOptions.obj'
    subprocess.run([str(ROOT / 'build/tools/wibo'), 'build/compilers/Win32/7.1/cl.exe', '/nologo', '/c', '/Ox', '/Ob0',
                    '/Z7', '/EHsc', '/Fo' + str(obj.relative_to(ROOT)), SOURCE], cwd=ROOT, check=True)
    symbols = read_object(obj)['symbols'].values()
    defined = {s['name'] for s in symbols if s['section'] > 0}
    externals = {'___CxxFrameHandler': 0x75bfd2, '__except_list': 0, '__fltused': 0, '__ftol2': 0x75bed4}
    shared = {k: a for k, (a, _) in SHARED_CODE.items()}
    for symbol in symbols:
        name = symbol['name']
        if symbol['section'] != 0 or symbol['storage'] != 2 or name in defined or name in externals:
            continue
        key = symbol_key(name)
        if key in GLOBALS:
            externals[name] = GLOBALS[key]
        elif key in INTERFACES:
            externals[name] = INTERFACES[key][0]
        elif key in shared:
            externals[name] = shared[key]
        else:
            raise AssertionError(name)
    weak = {s['name'] for s in symbols if s['name'].startswith('??_E') and s['name'] not in defined}
    linked = link_objects([obj], dict(externals, **dict.fromkeys(weak, 0)))
    linked = link_objects([obj], dict(externals, **{n: linked['exports']['??_G' + n[4:]] for n in weak}))
    exports = linked['exports']
    entries = {n: next(a for s, a in exports.items() if s.startswith(f[2]) and s in defined)
               for n, f in FUNCTIONS.items()}
    vtables = {exports[f'??_7{c}@@6B@']: a for c, (a, _, _) in VTABLES.items()}
    retail_vtable = {c: a for c, (a, _, _) in VTABLES.items()}

    rebuilt_to_retail = {entries[n]: FUNCTIONS[n][0] for n in entries}
    for c, (_, _, scalar) in VTABLES.items():
        rebuilt_to_retail[exports[f'??_G{c}@@UAEPAXI@Z']] = scalar
    blocks = dict(linked['blocks'])

    def linked_u32(address):
        for base, body in blocks.items():
            if base <= address < base + len(body):
                return struct.unpack_from('<I', body, address - base)[0]
        raise KeyError(hex(address))
    vtable_slots = 0
    for c, (retail, count, _) in VTABLES.items():
        rebuilt = exports[f'??_7{c}@@6B@']
        for i in range(count):
            want = struct.unpack_from('<I', image, retail - 0x400000 + i * 4)[0]
            got = linked_u32(rebuilt + i * 4)
            assert rebuilt_to_retail.get(got, got) == want, (c, i, hex(got), hex(want))
            vtable_slots += 1

    hooks = {a: (n, count, this, strings) for n, (a, count, this, strings) in INTERFACES.items()}
    hooks.update(FAKE_SLOTS)
    shared_ranges = [(a, a + n) for a, n in SHARED_CODE.values()]
    original_ranges = [(a, a + n) for a, n, *_ in FUNCTIONS.values()] + shared_ranges
    rebuilt_ranges = [(linked['address'], linked['address'] + linked['allocation'])] + shared_ranges
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
                ranges = rebuilt_ranges if rebuilt else original_ranges
                assert address == STOP or any(a <= address < b for a, b in ranges), ('unexpected execution', rebuilt, hex(address))
                return
            name, count, thiscall, strings = hooks[address]
            sp = uc.reg_read(UC_X86_REG_ESP)
            args = [u32(uc, sp + 4 + i * 4) for i in range(count)]
            for i in strings:
                args[i] = cstring(uc, args[i])
            if name in ('??0OptionToggle@@QAE@PAM000E@Z', '??0OptionSlider@@QAE@PAM000E@Z', '??0OptionButton@@QAE@PAM0E@Z'):
                args[-1] &= 0xff  # unsigned char argument pushed as a full register
            if name == 'FngSetObjectScript' or name == 'Replay8650B8::Value578EF0':
                args[-1] &= 0xff
            ecx = uc.reg_read(UC_X86_REG_ECX)
            record = [name, ecx if thiscall else None, args]
            result = 0
            if name in ('OptionsScrollerFNGObject::AddOption54EF30', 'OptionsScrollerFNGObject::AddOption54F170',
                        'OptionsScrollerFNGObject::AddSlider54F410', 'OptionsScrollerFNGObject::AddSlider54F800',
                        'OptionsScrollerFNGObject::AddButton', 'IconScrollerFNGObject::AddIcon'):
                size = 0x48 if name.startswith('Icon') else 0xa0
                record.append(normalise(uc.mem_read(args[0], size), rebuilt).hex() if args[0] else None)
                state['added'] += 1
                result = state['added']
            elif name == 'FngHash':
                result = 0xffffffff
                for c in args[0].upper().encode():
                    result = (result * 33 + c) & 0xffffffff
            elif name in ('FngFindObject', 'FngFindObject5379C0'):
                result = (args[0] ^ args[1] * 0x9e3779b1) & 0xffffffff
            elif name == 'FngGetPackageValue10':
                result = state['package_value']
            elif name == 'FngHelpBarName':
                result = STRINGS
            elif name == 'FngIsLoaded':
                result = 0x12345600 | state['loaded']
            elif name in ('FngScriptActive', 'FngScriptRunning'):
                result = 0x12345600 | state['script']
            elif name == 'ControllerAssigned':
                result = state['assigned']
            elif name == 'FngFilterSlot0C':
                result = (args[0] * 3 + args[1]) & 0xffffffff
            elif name == '??2@YAPAXI@Z':
                result = 0 if state['null_new'] else HEAP + state['allocations'] * 0xa0
                state['allocations'] += 1
            elif name == 'CareerData85AD40::Query500B60':
                result = 0x12345600 | state['v500b60']
            elif name == 'Value5B8240':
                result = 0x12345600 | state['v5b8240']
            elif name == 'Settings83AAB4::Value5026F0':
                result = state['v5026f0']
            elif name == 'MemoryCard83A9E0::Value502FE0':
                result = 0x12345600 | state['memcard']
            elif name == 'UIProgressBar::HandleMessage':
                result = 0x12345600 | state['bar']
            elif name == 'OptionsScrollerFNGObject::Item':
                result = ITEM if state['item'] else 0
            elif name in ('??0IconScrollerFNGObject@@QAE@PAX@Z', '??0OptionsScrollerFNGObject@@QAE@PAX@Z'):
                uc.mem_write(ecx + 4, struct.pack('<I', STRINGS + 0x80))
                uc.mem_write(ecx + 0x20, struct.pack('<I', 0x31))
                if name.startswith('??0Icon'):
                    uc.mem_write(ecx + 0x4c, struct.pack('<I', SCROLLER_VTABLE))
                result = ecx
            elif name in CONSTRUCTORS or name == '??0IconScrollerIcon@@QAE@III@Z':
                result = ecx
            state['trace'].append(tuple(record))
            uc.reg_write(UC_X86_REG_EAX, result)
            uc.reg_write(UC_X86_REG_ESP, sp + 4 + (count * 4 if thiscall else 0))
            uc.reg_write(UC_X86_REG_EIP, u32(uc, sp))
        uc.hook_add(UC_HOOK_CODE, hook)
        machines.append(uc)

    counts = dict.fromkeys(FUNCTIONS, 0)

    def compare(name, args=(), category=0, mode=0, players=((1, 0), (2, 0)), flow_player=1, loaded=0, script=0,
                assigned=1, package_value=0, null_new=0, prepared=True, fields=None, settings=None, v500b60=1,
                v5b8240=1, v5026f0=0, memcard=0, bar=1, item=0, flags=(0, 0, 0), replay=True, option=None):
        outputs = []
        kind, this_kind = None, FUNCTIONS[name][5]
        for rebuilt, uc in zip((False, True), machines):
            memory = bytearray(0x10000)
            if this_kind in ('main', 'options'):
                cls = 'PauseOptionsMainFNGObject' if this_kind == 'main' else 'PauseOptionsFNGObject'
                struct.pack_into('<II', memory, THIS - ARENA, exports[f'??_7{cls}@@6B@'] if rebuilt else retail_vtable[cls],
                                 STRINGS + 0x80)
                struct.pack_into('<I', memory, THIS - ARENA + 0x20, 0x31)
                if this_kind == 'main':
                    struct.pack_into('<I', memory, THIS - ARENA + 0x4c, SCROLLER_VTABLE)
                if not prepared:
                    memory[THIS - ARENA:THIS - ARENA + 0x15c] = bytes(0x15c)
                for offset, (fmt, value) in (fields or {}).items():
                    struct.pack_into(fmt, memory, THIS - ARENA + offset, value)
            struct.pack_into('<I', memory, SCROLLER_VTABLE - ARENA + 0x20, STOP + 0x300)
            struct.pack_into('<I', memory, BAR_VTABLE - ARENA + 4, STOP + 0x340)
            struct.pack_into('<I', memory, ITEM_VTABLE - ARENA + 0xc, STOP + 0x360)
            struct.pack_into('<I', memory, ITEM_VTABLE - ARENA + 0x20, STOP + 0x380)
            struct.pack_into('<I', memory, ITEM - ARENA, ITEM_VTABLE)
            memory[ITEM - ARENA + 0x28] = item >> 1
            struct.pack_into('<I', memory, ICON - ARENA, exports['??_7PauseOptionsIcon7918B8@@6B@'] if rebuilt else 0x7918b8)
            if option:
                vtable, extra = option
                struct.pack_into('<I', memory, OPTION - ARENA, exports[f'??_7PauseOption{vtable}@@6B@'] if rebuilt else int(vtable, 16))
                struct.pack_into('<II', memory, OPTION - ARENA + 0x2c, NAME_OBJECT, DATA_OBJECT)
                memory[OPTION - ARENA + 0x28] = 1
                if OPTIONS[vtable][0] == 'slider':
                    struct.pack_into('<I', memory, OPTION - ARENA + 0x54, BAR_VTABLE)
                for offset, (fmt, value) in extra.items():
                    struct.pack_into(fmt, memory, OPTION - ARENA + offset, value)
                struct.pack_into('<I', memory, RESOLUTION - ARENA, exports['??_7PauseOption791F30@@6B@'] if rebuilt else 0x791f30)
                struct.pack_into('<II', memory, RESOLUTION - ARENA + 0x2c, NAME_OBJECT + 0x100, DATA_OBJECT + 0x100)
            struct.pack_into('<ii', memory, FLOW - ARENA + 0x20, flow_player, 0)
            memory[STRINGS - ARENA:STRINGS - ARENA + 8] = b'HelpBar\0'
            memory[STRINGS - ARENA + 0x80:STRINGS - ARENA + 0x8c] = b'Options.fng\0'
            table = [0, 0]
            for i, player in enumerate(players):
                if player is not None:
                    struct.pack_into('<i', memory, PLAYERS - ARENA + i * 0x80 + 8, player[0])
                    table[i] = PLAYERS + i * 0x80
            uc.mem_write(ARENA, bytes(memory))
            uc.mem_write(TIB, struct.pack('<I', 0xffffffff))
            settings_bytes = settings or {}
            writes = [(0x8900d8, struct.pack('<I', FLOW)), (0x8384d8, struct.pack('<I', HELPBAR)),
                      (0x8650b8, struct.pack('<I', REPLAY if replay else 0)), (0x8900ac, struct.pack('<II', *table)),
                      (0x850078, struct.pack('<i', mode)), (0x83aa04, struct.pack('<i', category)),
                      (0x89e7e6, bytes([flags[0]])), (0x89e7d8, bytes([flags[1]])),
                      (0x890108, struct.pack('<i', flags[2])), (0x82b884, struct.pack('<I', AUDIO))]
            for address, data in settings_bytes.items():
                writes.append((address, data))
            for address, data in writes:
                uc.mem_write(address, data)
            if this_kind is None or this_kind in ('main', 'options'):
                this = THIS
            elif this_kind == 'icon':
                this = ICON
            else:
                this = OPTION
            if name.startswith('Create'):
                this = 0
            uc.mem_write(STACK, struct.pack('<I', STOP) + b''.join(struct.pack('<I', a) for a in args))
            uc.reg_write(UC_X86_REG_ESP, STACK)
            uc.reg_write(UC_X86_REG_ECX, this)
            uc.reg_write(UC_X86_REG_EAX, 0xdeadbeef)
            for reg in (UC_X86_REG_EBX, UC_X86_REG_ESI, UC_X86_REG_EDI, UC_X86_REG_EBP):
                uc.reg_write(reg, 0x12345678)
            state.update(trace=[], loaded=loaded, script=script, assigned=assigned, package_value=package_value,
                         null_new=null_new, allocations=0, added=0, v500b60=v500b60, v5b8240=v5b8240,
                         v5026f0=v5026f0, memcard=memcard, bar=bar, item=item & 1)
            start = entries[name] if rebuilt else FUNCTIONS[name][0]
            try:
                uc.emu_start(start, STOP, count=400000)
            except Exception:
                print(name, rebuilt, 'eip', hex(uc.reg_read(UC_X86_REG_EIP)), state['trace'][-3:])
                raise
            assert uc.reg_read(UC_X86_REG_EIP) == STOP, (name, rebuilt, hex(uc.reg_read(UC_X86_REG_EIP)))
            stack_args = FUNCTIONS[name][3]
            thiscall = this_kind is not None
            assert uc.reg_read(UC_X86_REG_ESP) == STACK + 4 + (stack_args * 4 if thiscall else 0), (
                name, rebuilt, hex(uc.reg_read(UC_X86_REG_ESP)))
            assert all(uc.reg_read(r) == 0x12345678 for r in (UC_X86_REG_EBX, UC_X86_REG_ESI, UC_X86_REG_EDI,
                                                                UC_X86_REG_EBP)), (name, rebuilt)
            assert u32(uc, TIB) == 0xffffffff, (name, rebuilt)
            kind = FUNCTIONS[name][4]
            result = uc.reg_read(UC_X86_REG_EAX) & (0xff if kind == 'al' else 0xffffffff) if kind else None
            outputs.append((state['trace'], result, normalise(uc.mem_read(ARENA, STACK - 0x1000 - ARENA), rebuilt),
                            b''.join(bytes(uc.mem_read(a, n)) for a, n in WATCHED)))
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

    player_cases = (((1, 0), (2, 0)), ((5, 0), (1, 0)), ((5, 0), (6, 0)), (None, (1, 0)), (None, None), ((5, 0), None))

    # PlayerSlot (0x4a7ae0).
    for players, flow_player in itertools.product(player_cases, (1, 2, -1)):
        compare('PlayerSlot', players=players, flow_player=flow_player)

    # UI_PauseOptionsMain.fng.
    for loaded, package_value in itertools.product((0, 1), (0, 2, 4)):
        compare('MainSetup', loaded=loaded, package_value=package_value)
        compare('MainConstructor', args=(ARENA + 0x700,), loaded=loaded, package_value=package_value, prepared=False)
        compare('CreateMain', args=(ARENA + 0x700,), loaded=loaded, prepared=False)
    compare('CreateMain', args=(ARENA + 0x700,), null_new=1, prepared=False)
    for loaded, flags in itertools.product((0, 1), (0, 1, 2, 3)):
        compare('MainDestructor', loaded=loaded)
        compare('MainScalarDestructor', args=(flags,), loaded=loaded)
    for args in ((1, 2, 3, 4, 5), (0xffffffff, 0, 0, 0, 0x80000000)):
        compare('MainUnknownSlot0C', args=args)
    main_messages = (0xc98356ba, 0x911ab364, 0xb5af2461, 0xe1fde1d1, 0x0c407210, 0, 0xc98356bb, 0xffffffff)
    for message, value150, script, assigned, loaded, dyno in itertools.product(
            main_messages, (0, 1), (0, 1), (0, 1, 0x100), (0, 1), (0, 1)):
        for value13c in ((0x0c407210, 0x911ab364, 0xb5af2461, 0, 0x12345678) if message == 0xe1fde1d1 else (0,)):
            compare('MainMessageHandler', args=(message, 0x11, 0x22, 0x33), script=script, assigned=assigned,
                    loaded=loaded, flags=(dyno, 0, 0), fields={0x13c: ('<I', value13c), 0x150: ('<B', value150)})
    for name in ('IconReact7918B8', 'IconReact7918C0', 'IconReact7918C8', 'IconReact7918D0'):
        for message in (0x0c407210, 0x0c407211, 0):
            compare(name, args=(STRINGS + 0x80, message, 1, 2, 3))

    # UI_PauseOptions.fng construction and category setups.
    for category, players, v500b60, v5026f0, flags, v5b8240, loaded in itertools.product(
            (0, 1, 2, 3, 4), (((1, 0), (2, 0)), ((5, 0), (1, 0))), (0, 1), (0, 2), ((0, 0, 0), (1, 1, 0), (0, 0, 3)),
            (0, 1), (0, 1)):
        common = dict(category=category, players=players, v500b60=v500b60, v5026f0=v5026f0, flags=flags,
                      v5b8240=v5b8240, loaded=loaded, settings={0x844ad8: bytes([category & 1, category >> 1 & 1])})
        if loaded == 0:
            compare('OptionsSetup', package_value=category, **common)
            for setup in ('SetupAudio', 'SetupDisplay', 'SetupGameplay', 'SetupPlayer'):
                compare(setup, **common)
        if v5b8240 == 1 and flags != (0, 0, 3):
            compare('OptionsConstructor', args=(ARENA + 0x700,), prepared=False, **common)
            compare('CreateOptions', args=(ARENA + 0x700,), prepared=False, **common)
    compare('CreateOptions', args=(ARENA + 0x700,), null_new=1, prepared=False)
    # SetupDisplay stores the resolution item through the slider pointer even when allocation failed.
    for setup in ('SetupAudio', 'SetupGameplay', 'SetupPlayer'):
        compare(setup, null_new=1)
    for flags in (0, 1, 2, 3):
        compare('OptionsDestructor')
        compare('OptionsScalarDestructor', args=(flags,))
    for prompt, autosave, value159, value15a, result in itertools.product((0, 1, 2), (0, 1, 2), (0, 1, 2), (0, 1),
                                                                          (0x11111111, 0x33333333)):
        compare('PromptSave', args=(result,), settings={0x844ad8: bytes([prompt, autosave])},
                fields={0x159: ('<B', value159), 0x15a: ('<B', value15a), 0x15b: ('<B', 7)})
    options_messages = (0x911ab364, 0x0c407210, 0xc98356ba, 0x22222222, 0x11111111, 0x33333333, 0x35f8620b,
                        0xb5af2461, 0xd05fc3a3, 0xe1fde1d1, 0, 0x12345678)
    for message, dialog, value158, category, script, loaded in itertools.product(
            options_messages, (0, 1), (0, 1), (1, 2), (0, 1), (0, 1)):
        for value138, item, prompt in itertools.product(
                (0x0c407210, 0x911ab364, 0xb5af2461, 0) if message in (0xe1fde1d1, 0x35f8620b) else (0,),
                (0, 1, 3) if message == 0x35f8620b else (0,),
                ((0, 1), (1, 0)) if message in (0x911ab364, 0xb5af2461) and category == 2 else ((1, 0),)):
            compare('OptionsMessageHandler', args=(message, 0x11, 0x22, 0x33), category=category, script=script,
                    loaded=loaded, item=item, settings={0x844ad8: bytes(prompt)},
                    fields={0x138: ('<I', value138), 0x154: ('<i', 3), 0x158: ('<B', value158),
                            0x159: ('<B', 1), 0x15a: ('<B', 0), 0x15b: ('<B', dialog)})

    # Option items.
    horizontal = (0x9120409e, 0xb5971bf1, 0x0c407210, 0x406415e3, 0x72619778)
    volumes = (0.0, 0.35, 1.0)
    for v, (kind, react, labels, init) in OPTIONS.items():
        option_cases = [dict()]
        settings_cases = [dict()]
        if kind == 'slider':
            option_cases = [{0x84: ('<f', value)} for value in (0.0, 0.5, 1.0, 3.6)]
            if v == '791F78':
                option_cases = [{**c, 0x98: ('<I', RESOLUTION)} for c in option_cases]
                settings_cases = [{0x87099c: struct.pack('<i', d), 0x870d1c: struct.pack('<i', r)}
                                  for d in (1, 3, 5) for r in (0, 4, 6)]
            else:
                settings_cases = [{0x83aa28: struct.pack('<7f', *[volumes[(i + k) % 3] for i in range(7)])}
                                  for k in range(3)]
        elif v == '791F30':
            settings_cases = [{0x870d1c: struct.pack('<i', mode), 0x86e880: struct.pack('<6i', *table)}
                              for mode in (0, 2, 5, 6)
                              for table in ((1, 1, 1, 1, 1, 1), (1, 0, 1, 0, 0, 1), (0, 0, 1, 0, 0, 0), (1, 0, 0, 0, 0, 0))]
        elif v == '791D20':
            settings_cases = [{0x83ac30: struct.pack('<i', d)} for d in (-1, 0, 1, 2, 3)]
        elif v == '791C00':
            settings_cases = [{0x83aa20: struct.pack('<f', f)} for f in (0.0, 127.0, 3.5, -0.0)]
        elif v in ('791C48', '791C90'):
            address = 0x83aa18 if v == '791C48' else 0x83aa1a
            settings_cases = [{address: bytes([b])} for b in (0, 1, 2, 0xff)]
        elif v in ('791D68', '791CD8'):
            address = 0x83aa1c if v == '791D68' else 0x83aa1b
            settings_cases = [{address: bytes([b])} for b in (0, 1, 2, 3, 0xff)]
        elif v == '793D68':
            settings_cases = [{0x844ad8: bytes([p, a])} for p in (0, 1, 2) for a in (0, 1, 2)]
        elif v == '791DB0':
            option_cases = [{0x54: ('<i', p)} for p in (0, 1)]
            settings_cases = [{0x83aa64 + 4: bytes([a]), 0x83aa64 + 0x28 + 4: bytes([b])} for a in (0, 1, 2) for b in (0, 1)]
        elif kind == 'toggle':
            # Player settings: both records, several values per field.
            settings_cases = [{0x83aa64: bytes([a, b, c, a, b, c]) + bytes(0xa) + struct.pack('<i', c * 2 - 1)
                               + bytes([c * 2, a, b]) + bytes(0x11)
                               + bytes([b, c, a, b, c, a]) + bytes(0xa) + struct.pack('<i', a + 3) + bytes([5, b, a])
                               + bytes(0x11)}
                              for a, b, c in ((0, 1, 2), (1, 0, 0), (2, 2, 1), (0, 0, 3))]
        modes = (0, 4) if v == '791CD8' else (0,)
        for option_extra, settings, mode, players in itertools.product(
                option_cases, settings_cases, modes,
                (((1, 0), (2, 0)), ((5, 0), (1, 0)), ((5, 0), (6, 0))) if v in PLAYER_OPTIONS else (((1, 0), (2, 0)),)):
            common = dict(option=(v, option_extra), settings=settings, mode=mode, players=players)
            compare('SetLabels' + v, **common)
            if init:
                compare('InitSlider' + v, **common)
            for message, bar, memcard, replay in itertools.product(
                    horizontal, (0, 1) if kind == 'slider' else (1,), (0, 1) if v == '793D68' else (0,),
                    (True, False) if v == '791C00' else (True,)):
                compare('React' + v, args=(STRINGS + 0x80, message), bar=bar, memcard=memcard, replay=replay, **common)
    for name in ('Enable791DB0', 'Disable791DB0'):
        for enabled in (0, 1):
            compare(name, option=('791DB0', {0x28: ('<B', enabled)}))

    report = {'reference_sha256': hashlib.sha256(raw).hexdigest(), 'comparisons': counts, 'total': sum(counts.values()),
              'fingerprints': {n: {'address': hex(a), 'span': size,
                                   'sha256': hashlib.sha256(image[a - 0x400000:a - 0x400000 + size]).hexdigest()}
                               for n, (a, size, *_) in FUNCTIONS.items()},
              'vtables': {c: hex(a) for c, (a, _, _) in VTABLES.items()}, 'vtable_slots_checked': vtable_slots,
              'interfaces': {n: hex(v[0]) for n, v in INTERFACES.items()},
              'shared_code': {n: hex(a) for n, (a, _) in SHARED_CODE.items()}, 'byte_matching': False,
              'execution_guard': True}
    (BUILD / 'verification.json').write_text(json.dumps(report, indent=2) + '\n')
    missing = [n for n, c in counts.items() if not c]
    assert not missing, missing
    print('PASS', report['total'], 'comparisons over', len(counts), 'routines,', vtable_slots, 'vtable slots')


if __name__ == '__main__':
    main()
