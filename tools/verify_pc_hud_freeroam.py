#!/usr/bin/env python3
"""Compare original freeroam HUD FEng traces, state writes, and vehicle readers.

FEng, localisation, cash, GPS, notification presentation and nitrous fraction
providers are controlled interfaces,
not verification of their internals. Retail arithmetic constants are retained.
"""
import hashlib
import itertools
import json
import re
import random
import struct
import subprocess

import pefile
from unicorn import Uc, UC_ARCH_X86, UC_MODE_32, UC_HOOK_CODE
from unicorn.x86_const import *
from probe_pc_race_reward import ROOT, EXE
from pc_coff import link_objects, read_object

BUILD = ROOT / 'build/analysis/freeroam-hud'
FUNCTIONS = {
    'HudTurboAngle': (0x4aa160, 71),
    'HudReadBoost': (0x5ecec0, 94),
    'HudReadHeat': (0x5eccd0, 87),
    'HeatUpdate': (0x4c7aa0, 125),
    'GpsUpdate': (0x4c8410, 434),
    'NotificationUpdate': (0x4da9a0, 468),
    'HudNotificationUpdate': (0x4c9030, 115),
    'HudNotificationNext': (0x4c90b0, 112),
    'HudNotificationPriority': (0x4abf20, 39),
    'HudReadShiftWindow': (0x594f00, 31),
    'HudReadShiftPoint': (0x5ecd30, 53),
    'TurboUpdate': (0x4c6b80, 302),
    'NitrousUpdate': (0x4c7b20, 757),
    'RegionUpdate': (0x4a9e70, 115),
    'CashUpdate': (0x4a9ef0, 250),
    'RewardUpdate': (0x4a9ff0, 235),
    'HudLerpVector3': (0x505150, 71),
    'HudLerpColour': (0x5051a0, 190),
    'ControllerMasks': (0x4ca090, 1500),
    'GaugeUpdate': (0x4c68d0, 676),
    'HudNeedleAngle': (0x4aa0e0, 124),
    'UseMetric5EA8B0': (0x5ea8b0, 15),
    'MapUpdate': (0x4da640, 167),
    'Query500B60': (0x500b60, 28),
    'Count500400': (0x500400, 86),
}
GLOBAL_ADDRESSES = {
    'engineSpeedOverride': 0x7fbed8, 'engineSpeedOverrideUnset': 0x784258,
    'hudGpsReceiver82B884': 0x82b884, 'hudClock86518C': 0x86518c,
    'hudDrag89E7D8': 0x89e7d8, 'hudDragOverride890108': 0x890108,
    'hudPlayerCount89E7C0': 0x89e7c0, 'hudOne784250': 0x784250,
    'hudHalf784260': 0x784260, 'hudZero7D7D3C': 0x7d7d3c,
    'hudNegativeOne7842A0': 0x7842a0, 'hudFullTurn78450C': 0x78450c,
    'hudTurboCentre78E058': 0x78e058, 'hudTurboSweep78E05C': 0x78e05c,
    'hudDragTurboCentre78E060': 0x78e060, 'hudDragTurboSweep78E064': 0x78e064,
    'hudNosEnd795AEC': 0x795aec, 'hudNosSweep795AF0': 0x795af0,
    'hudNosRotation8364B0': 0x8364b0, 'gearboxShiftWeights': 0x7fbf60,
    'gearboxMatchWindow': 0x7fbf5c, 'hudRewardPending83649C': 0x83649c,
    'hudRewardBase8364A0': 0x8364a0, 'hudRewardExtra8364A4': 0x8364a4,
    'hudRewardType8364A8': 0x8364a8, 'hudRewardDelay8364AC': 0x8364ac,
    'hudDrift89E7D9': 0x89e7d9, 'hudHolder85B940': 0x85b940, 'careerBank861E74': 0x861e74, 'hudDriftOverride890104': 0x890104, 'hudRaceType89E7B0': 0x89e7b0,
}
# Original address, stack argument count, callee cleanup. GetNitrousElapsed
# and vehicle readers have thiscall contracts; FEng calls are cdecl.
INTERFACES = {
    'HudHideObject': (0x50ca00, 1, 0), 'HudShowObject': (0x50ca50, 1, 0),
    'HudObjectRotation': (0x51ced0, 2, 0), 'HudObjectColour': (0x51d7f0, 2, 0),
    'HudTextPrint': (0x537be0, 0, 0), 'HudLookupObject': (0x537a10, 2, 0),
    'HudObjectHash': (0x505450, 1, 0), 'HudLocalisedText': (0x4ffa80, 1, 0),
    'HudObjectScript': (0x51cf70, 3, 0), 'IsCashSuppressed': (0x501680, 0, 0),
    'GetBalance': (0x500f90, 0, 0), 'GetLanguage4FF970': (0x4ff970, 0, 0),
    'HudVerticalBar': (0x4c66f0, 6, 24), 'HudHorizontalBar': (0x4c67e0, 6, 24),
    'GetNitrousElapsed': (0x5925b0, 0, 0),
    'ApplyMasks': (0x4c6690, 2, 8),
    'HudLookupAnyObject': (0x5379c0, 2, 0),
    'HudPackageScriptActive': (0x537cc0, 3, 0),
    'HudPackageScript': (0x537c00, 4, 0),
    'HudPlayerVisibility': (0x5f1e10, 1, 4),
    'HudGpsMode': (0x501100, 0, 0), 'HudGpsPosition': (0x5010c0, 2, 8),
    'HudObjectTexture': (0x50c900, 2, 0), 'HudObjectPosition': (0x51d4d0, 3, 0),
    'HudGpsEvent': (0x458520, 1, 4),
    'GetClassIndex': (0x5b5330, 0, 0),
    'Update': (0x4ca840, 0, 0),  # HudMinimap::Update, verified by verify_pc_hud_minimap.py
    'HudNotificationTexture': (0x50c990, 2, 0),
    'HudNotificationPrepare': (0x4c8ac0, 0, 0),
    'HudNotificationNormal': (0x4c8b80, 1, 4),
    'HudNotificationType11': (0x4c8dd0, 1, 4),
    'HudNotificationType13': (0x4abaf0, 1, 4),
    'HudNotificationType14': (0x4aba30, 1, 4),
    'HudNotificationSound': (0x51de30, 2, 0),
}


def main():
    BUILD.mkdir(parents=True, exist_ok=True)
    raw = EXE.read_bytes()
    assert hashlib.sha256(raw).hexdigest() == 'f9dd86c054878ce6276beb07c1fd61874f7a1e4bf1f241b084c65b73e24168a7'
    pe = pefile.PE(data=raw)
    image = pe.get_memory_mapped_image()
    obj = BUILD / 'HudFreeroam.obj'
    subprocess.run([str(ROOT / 'build/tools/wibo'), 'build/compilers/Win32/7.1/cl.exe',
                    '/nologo', '/c', '/Ox', '/Ob0', '/Z7', '/EHsc',
                    '/Fo' + str(obj.relative_to(ROOT)), 'src/Speed/Indep/Src/Hud/HudFreeroam.cpp'], cwd=ROOT, check=True)
    animation = BUILD / 'HudAnimation.obj'
    subprocess.run([str(ROOT / 'build/tools/wibo'), 'build/compilers/Win32/7.1/cl.exe',
                    '/nologo', '/c', '/Ox', '/Ob0', '/Z7', '/EHsc',
                    '/Fo' + str(animation.relative_to(ROOT)), 'src/Speed/Indep/Src/Hud/HudAnimation.cpp'], cwd=ROOT, check=True)
    controller = BUILD / 'HudController.obj'
    subprocess.run([str(ROOT / 'build/tools/wibo'), 'build/compilers/Win32/7.1/cl.exe',
                    '/nologo', '/c', '/Ox', '/Ob0', '/Z7', '/EHsc',
                    '/Fo' + str(controller.relative_to(ROOT)), 'src/Speed/Indep/Src/Hud/HudController.cpp'], cwd=ROOT, check=True)
    gauge = BUILD / 'HudGauge.obj'
    subprocess.run([str(ROOT / 'build/tools/wibo'), 'build/compilers/Win32/7.1/cl.exe',
                    '/nologo', '/c', '/Ox', '/Ob0', '/Z7', '/EHsc',
                    '/Fo' + str(gauge.relative_to(ROOT)), 'src/Speed/Indep/Src/Hud/HudGauge.cpp'], cwd=ROOT, check=True)
    symbols = [s for o in (controller, obj, animation, gauge) for s in read_object(o)['symbols'].values()]
    externals = {}
    defined = {symbol['name'] for symbol in symbols if symbol['section'] > 0}
    for symbol in symbols:
        if symbol['section'] != 0 or not symbol['name'].startswith('?') or symbol['name'] in defined:
            continue
        short = symbol['name'].split('@')[0][1:]
        if short in GLOBAL_ADDRESSES:
            externals[symbol['name']] = GLOBAL_ADDRESSES[short]
        elif re.fullmatch(r'[A-Za-z]+[0-9A-F]{6}', short) and short not in INTERFACES:
            externals[symbol['name']] = int(short[-6:], 16)
        elif short in INTERFACES:
            # Rebuilt interfaces use cdecl wrappers, including bar callbacks.
            externals[symbol['name']] = 0x2110000 + list(INTERFACES).index(short) * 0x100
        else:
            raise AssertionError(symbol['name'])
    externals['__ftol2'] = 0x75bed4
    linked = link_objects([obj, animation, controller, gauge], externals)
    entries = {}
    for name in FUNCTIONS:
        cls = {'TurboUpdate': 'HudTurboFeature', 'GaugeUpdate': 'HudGaugeFeature', 'NitrousUpdate': 'HudNitrousFeature',
               'NotificationUpdate': 'HudNotificationFeature', 'HeatUpdate': 'HudHeatFeature', 'GpsUpdate': 'HudGpsFeature',
               'RegionUpdate': 'HudRegionFeature', 'CashUpdate': 'HudCashFeature',
               'RewardUpdate': 'HudRewardFeature'}.get(name)
        prefix = {'MapUpdate': '?UpdateMap@HudGpsFeature@', 'Query500B60': '?Query500B60@CareerData85AD40@',
                  'Count500400': '?Count500400@CareerData85AD40@'}.get(name) or (
            '?ApplyMasks@HudController@' if name == 'ControllerMasks' else '?Update@' + cls + '@' if cls else '?' + name + '@')
        entries[name] = next(a for s, a in linked['exports'].items() if s.startswith(prefix))
    arena, stack, stop = 0x2000000, 0x200f000, 0x2100000
    state = {}
    machines = []
    def cstring(uc, address):
        data = bytes(uc.mem_read(address, 256))
        return data.split(b'\0')[0].decode('ascii')
    def u32(uc, address):
        return struct.unpack('<I', uc.mem_read(address, 4))[0]
    for rebuilt in (False, True):
        uc = Uc(UC_ARCH_X86, UC_MODE_32)
        uc.mem_map(0x400000, (len(image) + 4095) & ~4095)
        uc.mem_write(0x400000, image)
        uc.mem_map(arena, 0x10000)
        uc.mem_map(stop, 0x20000)
        uc.mem_map(linked['address'], linked['allocation'])
        for address, body in linked['blocks']: uc.mem_write(address, body)
        hooks = {0x2110000 + i * 0x100 if rebuilt else a: (n, count, 8 if rebuilt and n == 'ApplyMasks' else 0 if rebuilt else clean)
                 for i, (n, (a, count, clean)) in enumerate(INTERFACES.items())}
        def hook(uc, address, size, user, rebuilt=rebuilt, hooks=hooks):
            if address not in hooks:
                providers = [(stop + 0x100, stop + 0x107), (stop + 0x200, stop + 0x20b)]
                retained = [(0x75bed4, 0x75bf49), (0x5947b0, 0x5947bd)]
                ranges = [(a, a + n) for a, n in FUNCTIONS.values()] + retained if not rebuilt else [(linked['address'], linked['address'] + linked['allocation'])] + retained
                assert any(a <= address < b for a, b in ranges + providers), ('unexpected execution', rebuilt, hex(address))
                return
            name, count, cleanup = hooks[address]
            sp = uc.reg_read(UC_X86_REG_ESP)
            args = [u32(uc, sp + 4 + i * 4) for i in range(count)]
            if name == 'HudTextPrint':
                fmt = cstring(uc, u32(uc, sp + 8))
                count = {'%s': 1, '%$d': 1, '%s %$d': 2, '%s%s: %$d': 3, 'LOCALISED': 0, '%3d': 1, '%c': 1}[fmt]
                args = [u32(uc, sp + 4), fmt]
                for i in range(count):
                    value = u32(uc, sp + 12 + i * 4)
                    args.append(cstring(uc, value) if fmt in ('%s', '%s %$d', '%s%s: %$d') and i < (2 if fmt == '%s%s: %$d' else 1) else value)
            elif name in ('HudObjectHash',): args[0] = cstring(uc, args[0])
            elif name == 'HudObjectScript': args[1] = cstring(uc, args[1])
            elif name in ('HudPackageScriptActive', 'HudPackageScript'): args[2] = cstring(uc, args[2])
            elif name in ('ApplyMasks', 'Update'): args.insert(0, uc.reg_read(UC_X86_REG_ECX))
            elif name == 'HudPlayerVisibility':
                if rebuilt: args = [u32(uc, sp + 4), u32(uc, sp + 8)]
                else: args.insert(0, uc.reg_read(UC_X86_REG_ECX))
            if name.startswith('HudNotification') and name not in ('HudNotificationSound', 'HudNotificationTexture'):
                count = 1 if name == 'HudNotificationPrepare' else 2
                args = [u32(uc, sp + 4 + i * 4) for i in range(count)] if rebuilt else [uc.reg_read(UC_X86_REG_ECX)] + args
                args.append(bytes(uc.mem_read(args[0], 8)).hex())
            if name == 'HudGpsEvent':
                if rebuilt: args = [u32(uc, sp + 4), u32(uc, sp + 8)]
                else: args.insert(0, uc.reg_read(UC_X86_REG_ECX))
            if name == 'HudGpsPosition':
                uc.mem_write(args[0], struct.pack('<f', 12.5))
                uc.mem_write(args[1], struct.pack('<f', -32.75))
                args = [] # Stack-local pointers differ; subsequent position call checks their values.
            if name == 'GetNitrousElapsed':
                assert uc.reg_read(UC_X86_REG_ECX) == arena + 0x4040
                # Execute a tiny fld/ret provider so Unicorn models the x87 stack.
                uc.reg_write(UC_X86_REG_EIP, stop + 0x100)
                state['trace'].append((name, []))
                return
            state['trace'].append((name, args))
            result = 0
            if name == 'HudObjectHash':
                result = 0xffffffff
                for c in args[0].upper().encode(): result = (result * 33 + c) & 0xffffffff
            elif name in ('HudLookupObject', 'HudLookupAnyObject'): result = arena + 0x1300
            elif name == 'HudLocalisedText': result = arena + 0x9000
            elif name == 'IsCashSuppressed': result = state['suppressed']
            elif name == 'GetBalance': result = state['balance']
            elif name == 'GetLanguage4FF970': result = state['language']
            elif name == 'HudGpsMode': result = state['gps_mode']
            elif name == 'HudPackageScriptActive': result = state['script_active']
            elif name == 'GetClassIndex': result = state['tuning']
            uc.reg_write(UC_X86_REG_EAX, result)
            uc.reg_write(UC_X86_REG_ESP, sp + 4 + cleanup)
            uc.reg_write(UC_X86_REG_EIP, u32(uc, sp))
        uc.hook_add(UC_HOOK_CODE, hook)
        machines.append(uc)
    counts = dict.fromkeys(FUNCTIONS, 0)
    rng = random.Random(0x4c7b20)
    def compare(name, memory, args=b'', this=0, globals_=(), fraction=0.0, suppressed=0, balance=0, language=0, cw=0x27f, original_args=None, script_active=0, tuning=0, rpm=0., gps_mode=0):
        outputs = []
        for rebuilt, uc in zip((False, True), machines):
            uc.mem_write(arena, bytes(memory))
            uc.mem_write(arena + 0x9000, b'LOCALISED\0')
            uc.mem_write(arena + 0x9100, struct.pack('<f', fraction))
            uc.mem_write(stop + 0x100, b'\xd9\x05' + struct.pack('<I', arena + 0x9100) + b'\xc3')
            uc.mem_write(stop + 0x200, b'\xd9\x1d' + struct.pack('<I', arena + 0x9200) + b'\xe9' + struct.pack('<i', stop - (stop + 0x20b)))
            machine_args = original_args if not rebuilt and original_args is not None else args
            floating = name in ('HudTurboAngle', 'HudReadBoost', 'HudReadHeat', 'HudReadShiftWindow', 'HudReadShiftPoint', 'HudNeedleAngle')
            uc.mem_write(stack, struct.pack('<I', stop + 0x200 if floating else stop) + machine_args)
            uc.reg_write(UC_X86_REG_ESP, stack)
            uc.reg_write(UC_X86_REG_ECX, this)
            uc.reg_write(UC_X86_REG_FPCW, cw)
            uc.reg_write(UC_X86_REG_FPSW, 0)
            uc.reg_write(UC_X86_REG_FPTAG, 0xffff)
            for reg in (UC_X86_REG_EBX, UC_X86_REG_ESI, UC_X86_REG_EDI, UC_X86_REG_EBP): uc.reg_write(reg, 0x12345678)
            for a, data in globals_: uc.mem_write(a, bytes(data))
            state.update(trace=[], suppressed=suppressed, balance=balance, language=language, script_active=script_active, tuning=tuning, gps_mode=gps_mode)
            end = stop
            uc.emu_start(entries[name] if rebuilt else FUNCTIONS[name][0], end, count=10000)
            assert uc.reg_read(UC_X86_REG_EIP) == end, (name, hex(uc.reg_read(UC_X86_REG_EIP)))
            expected_cleanup = len(machine_args) if not rebuilt and name not in ('HudLerpVector3', 'HudLerpColour', 'HudNotificationPriority', 'HudNeedleAngle') or rebuilt and not name.startswith('Hud') else 0
            assert uc.reg_read(UC_X86_REG_ESP) == stack + 4 + expected_cleanup, (name, rebuilt, hex(uc.reg_read(UC_X86_REG_ESP)))
            assert uc.reg_read(UC_X86_REG_FPCW) == cw
            if True:
                assert all(uc.reg_read(r) == 0x12345678 for r in (UC_X86_REG_EBX, UC_X86_REG_ESI, UC_X86_REG_EDI, UC_X86_REG_EBP))
            outputs.append((state['trace'], bytes(uc.mem_read(arena, 0x9000)), bytes(uc.mem_read(arena + 0x9200, 4)) if floating else struct.pack('<I', uc.reg_read(UC_X86_REG_EAX)) if name in ('HudNotificationNext', 'HudNotificationPriority') else bytes([uc.reg_read(UC_X86_REG_EAX) & 0xff]) if name in ('UseMetric5EA8B0', 'Query500B60') else struct.pack('<I', uc.reg_read(UC_X86_REG_EAX)) if name == 'Count500400' else b'',
                            bytes(uc.mem_read(0x83649c, 24)), bytes(uc.mem_read(0x8364b0, 4))))
        assert outputs[0] == outputs[1], (name, fraction, cw, outputs[0][0], outputs[1][0], outputs[0][2:], outputs[1][2:])
        counts[name] += 1
    def frame(active=True):
        m = bytearray(0x10000)
        struct.pack_into('<4I', m, 0x10, 0x60, 0x40000, 0x60 if active else 0, 0x40000 if active else 0)
        for i in range(6): struct.pack_into('<I', m, 0x20 + i * 4, arena + 0x1000 + i * 0x100)
        struct.pack_into('<I', m, 0x3004, arena + 0x4000)
        struct.pack_into('<I', m, 0x4028, 1)
        struct.pack_into('<I', m, 0x4034, arena + 0x5000)
        struct.pack_into('<II', m, 0x5048, arena + 0x6000, arena + 0x7000)
        return m
    def globals_for(players=1, drag=0, override=0):
        return [(0x89e7c0, struct.pack('<i', players)), (0x89e7d8, bytes([drag])), (0x890108, struct.pack('<i', override)),
                (0x83649c, bytes(24)), (0x8364b0, bytes(4))]
    for cw in (0x27f, 0x37f):
        for active, active_slot, kind, next_state, next_kind in itertools.product((False, True), (-1, 0, 7), (0, 1, 0x13, 0x14), (0, 1, 4), (0, 1, 0x13, 0x14)):
            m = frame(active)
            if active_slot >= 0:
                m[0x60 + active_slot*0x14] = 2; m[0x61 + active_slot*0x14] = kind
            next_slot = 6 if active_slot == 7 else 1 if active_slot == 0 else 0
            m[0x60 + next_slot*0x14] = next_state; m[0x61 + next_slot*0x14] = next_kind
            g = globals_for() + [(0x86518c, struct.pack('<i', 123456))]
            compare('NotificationUpdate', m, struct.pack('<I', arena + 0x3000), arena, g, cw=cw)
        for kind in list(range(-2, 24)) + [-0x80000000, 0x7fffffff]:
            compare('HudNotificationPriority', frame(), struct.pack('<i', kind), globals_=globals_for(), cw=cw)
        for kind, entry_state in itertools.product((-128, -1, 0, 1, 17, 19, 20, 127), range(7)):
            m = frame(); m[0x2000] = entry_state; struct.pack_into('<b', m, 0x2001, kind)
            compare('HudNotificationUpdate', m, struct.pack('<II', arena + 0x2000, arena + 0x3000), arena + 0x2000, globals_for(), cw=cw, original_args=struct.pack('<I', arena + 0x3000))
        for trial in range(512):
            m = frame()
            for slot in range(8):
                m[0x60 + slot*0x14] = rng.randrange(7)
                struct.pack_into('<b', m, 0x61 + slot*0x14, rng.choice([-128, -1, 0, *range(1,24), 127]))
            compare('HudNotificationNext', m, struct.pack('<I', arena), arena, globals_for(), cw=cw, original_args=b'')
            compare('NotificationUpdate', m, struct.pack('<I', arena + 0x3000), arena, globals_for() + [(0x86518c, struct.pack('<i', trial))], cw=cw)
        for kind, heat in itertools.product((0, 1, 2, 3), (-1., -0., 0., 0.125, 0.99999994, 1., 1.000000119, 2.)):
            m = frame(); struct.pack_into('<I', m, 0x4028, kind); struct.pack_into('<f', m, 0x608c, heat)
            compare('HudReadHeat', m, struct.pack('<I', arena + 0x4000), arena + 0x4000, globals_for(), cw=cw, original_args=b'')
        for active, players, heat in itertools.product((False, True), (0, 1, 2), (-1., 0., 0.25, 1., 2.)):
            m = frame(active); struct.pack_into('<f', m, 0x608c, heat)
            compare('HeatUpdate', m, struct.pack('<I', arena + 0x3000), arena, globals_for(players), cw=cw)
        for mode, camera, kind, arrived, receiver in itertools.product(range(5), range(4), (0, 1), (0, 1), (0, 1)):
            m = frame(); struct.pack_into('<I', m, 0x4024, kind); struct.pack_into('<I', m, 0x402c, arena + 0x8000)
            struct.pack_into('<i', m, 0x83e4, camera); m[0x35] = arrived
            g = globals_for() + [(0x82b884, struct.pack('<I', arena + 0x8500 if receiver else 0))]
            compare('GpsUpdate', m, struct.pack('<I', arena + 0x3000), arena, g, cw=cw, gps_mode=mode)
        for kind, capacity, boost, pointer in itertools.product((0, 1, 2, 3), (-1., 0., 1., 10.), (-1., 0., 0.75), (False, True)):
            m = frame(); struct.pack_into('<I', m, 0x4028, kind); struct.pack_into('<ff', m, 0x6098, capacity, boost)
            flag = arena + 0x8100 if pointer else 0
            compare('HudReadBoost', m, struct.pack('<II', arena + 0x4000, flag), arena + 0x4000,
                    globals_for(), cw=cw, original_args=struct.pack('<I', flag))
        for kind, gear in itertools.product((0, 1, 2, 3), range(9)):
            m = frame(); struct.pack_into('<I', m, 0x4028, kind); struct.pack_into('<I', m, 0x5024, arena + 0x8000)
            for i in range(9):
                struct.pack_into('<f', m, 0x7084 + 4*i, 6500. + i*100)
                struct.pack_into('<f', m, 0x819c + 4*i, 7500. + i*100)
            compare('HudReadShiftPoint', m, struct.pack('<II', arena + 0x4000, gear), arena + 0x4000,
                    globals_for(), cw=cw, original_args=struct.pack('<I', gear))
        for tuning, gear, scale in itertools.product(range(4), range(9), (0., 0.5, 1., 1.5)):
            m = frame(); struct.pack_into('<i', m, 0x70f0, tuning); struct.pack_into('<f', m, 0x70e4, scale)
            compare('HudReadShiftWindow', m, struct.pack('<II', arena + 0x7000, gear), arena + 0x7000,
                    globals_for(), cw=cw, original_args=struct.pack('<I', gear))
        for metric, players, override in itertools.product((0, 1, 2), (1, 2), (100., 7250.)):
            compare('UseMetric5EA8B0', frame(), b'', globals_=[(0x83aa1a, bytes([metric]))], cw=cw)
        for rpm, sweep, maximum in itertools.product((-100., 0., 3000., 7999., 8000., 9000.), (-235., -0., 0., 120., 235., 359., 400.), (8000., 9500.)):
            m = frame(); struct.pack_into('<3f', m, 0xb1f0, 7000., 6500., maximum)
            compare('HudNeedleAngle', m, struct.pack('<ffII', rpm, sweep, arena + 0xb1f0, 0x12345678), globals_=globals_for(), cw=cw)
        for trial in range(1500):
            m = frame(rng.random() < 0.85)
            for k in range(0x30, 0x80, 4): struct.pack_into('<I', m, k, arena + 0x1000 + k * 0x10)
            if rng.random() < 0.3: struct.pack_into('<I', m, 0x64, 0)
            m[0xc0] = rng.choice((0, 0, 1)); struct.pack_into('<f', m, 0xc4, rng.choice((235., 180., -20., 0.)))
            car = 0x4000
            struct.pack_into('<I', m, 0x3004, arena + car)
            struct.pack_into('<I', m, car + 0xc, rng.choice((0, arena + 0x9800)))
            struct.pack_into('<I', m, car + 0x10, arena + 0xa000)
            struct.pack_into('<I', m, car + 0x14, arena + 0xa100)
            struct.pack_into('<i', m, 0xa104, rng.choice((0, 1, 1, 2)))
            m[0xaa00] = rng.choice((0, 0, 1))
            struct.pack_into('<I', m, car + 0x20, arena + 0xb000)
            struct.pack_into('<3f', m, 0xb1f0, 7000., rng.choice((6500., 7200., 0.5)), rng.choice((8000., 9500.)))
            struct.pack_into('<I', m, car + 0x28, rng.choice((0, 1, 1, 3)))
            struct.pack_into('<I', m, car + 0x34, arena + 0x5000)
            struct.pack_into('<I', m, 0x5024, arena + 0x8000)
            gear = rng.choice((-1, 0, 1, 2, 3, 4, 5, 6, 7, 8))
            struct.pack_into('<f', m, car + 0x430, rng.choice((-55.5, -0., 0., 12.25, 33.3, 90.)))
            struct.pack_into('<i', m, car + 0x434, gear)
            rpm = rng.choice((0., 900., 5999., 6400., 6500., 6501., 7000., 8100., 12000.))
            struct.pack_into('<f', m, car + 0x440, rpm)
            struct.pack_into('<i', m, car + 0x448, rng.choice((0, 1)))
            struct.pack_into('<I', m, car + 0x968, rng.choice((0, arena + 0xc800)))
            struct.pack_into('<f', m, 0xc858, rng.choice((3000., 7600.)))
            struct.pack_into('<i', m, 0x70f0, rng.randrange(4)); struct.pack_into('<f', m, 0x70e4, 0.75)
            for i in range(9):
                struct.pack_into('<f', m, 0x7084 + i * 4, 6500. + i * 50)
                struct.pack_into('<f', m, 0x819c + i * 4, 7500. + i * 50)
            g = globals_for(players=rng.choice((0, 1, 1, 2, 4))) + [
                (0x83aa1a, bytes([rng.choice((0, 1, 2))])),
                (0x7fbed8, struct.pack('<f', rng.choice((100., 100., 6800.))))]
            compare('GaugeUpdate', m, struct.pack('<I', arena + 0x3000), arena, g, cw=cw, tuning=rng.randrange(4))
        for trial in range(400):
            m = frame(rng.random() < 0.8)
            for k in (0x20, 0x24, 0x28, 0x2c, 0x30): struct.pack_into('<I', m, k, arena + 0x1000 + k * 0x10)
            m[0x35] = rng.choice((0, 1))
            struct.pack_into('<I', m, 0x3004, arena + 0x4000)
            struct.pack_into('<i', m, 0x4024, rng.choice((0, 1)))
            struct.pack_into('<I', m, 0x402c, arena + 0x8000)
            struct.pack_into('<i', m, 0x83e4, rng.choice((0, 1, 2, 3)))
            career = bytearray(16)
            for i in range(16): career[i] = rng.choice((0, 1, 1, 1)) if trial % 3 else 1
            mode = rng.choice((0, 1, 1, 4))
            race = rng.choice((0, 0, 7))
            g = globals_for() + [(0x8364d4, struct.pack('<I', rng.choice((0, arena + 0x9400)))),
                                 (0x8385b9, bytes([rng.choice((0, 1))])), (0x850078, struct.pack('<i', mode)),
                                 (0x89e7b0, struct.pack('<i', race)), (0x85ad40 + 0x8869, bytes(career)),
                                 (0x82b884, struct.pack('<I', rng.choice((0, arena + 0x8500))))]
            compare('MapUpdate', m, struct.pack('<I', arena + 0x3000), arena, g, cw=cw, gps_mode=rng.randrange(5))
            compare('Query500B60', m, b'', 0x85ad40, g, cw=cw)
            compare('Count500400', m, b'', 0x85ad40, g, cw=cw)
        for name in ('HudLerpVector3', 'HudLerpColour'):
            for fraction in (0., 0.125, 0.5, 0.875, 1.):
                for trial in range(64):
                    m = frame()
                    for at in (0x8000, 0x8010, 0x8020):
                        values = [rng.uniform(-1000, 1000) for _ in range(4)] if name == 'HudLerpVector3' else [rng.randrange(-400, 400) for _ in range(4)]
                        struct.pack_into('<4f' if name == 'HudLerpVector3' else '<4i', m, at, *values)
                    compare(name, m, struct.pack('<IIfII', arena + 0x8000, arena + 0x8010, fraction, arena + 0x8020, arena + 0x8030),
                            globals_=globals_for(), cw=cw)
        for boost in [-4., -1., -0., 0., 0.5, 1., 4.] + [rng.uniform(-4, 4) for _ in range(128)]:
            for start, end in ((-14., 222.), (158., 96.), (0., 360.), (-720., 720.)):
                compare('HudTurboAngle', frame(), struct.pack('<3f', boost, start, end), cw=cw, globals_=globals_for())
        for kind, installed, active, drag, override in itertools.product((0, 1, 3), (0., 1.), (False, True), (0, 1), (0, 1)):
            m = frame(active)
            struct.pack_into('<I', m, 0x4028, kind)
            struct.pack_into('<ff', m, 0x6098, installed, 0.75)
            compare('TurboUpdate', m, struct.pack('<I', arena + 0x3000), arena, globals_for(drag=drag, override=override), cw=cw)
        for active, players, drag, override, fraction in itertools.product((False, True), (1, 2), (0, 1), (0, 1), (-1., 0., 0.25, 0.99999994, 1., 1.000000119, 1.5, 2., 3.)):
            m = frame(active)
            struct.pack_into('<II', m, 0x2c, 0x96005a7d, 0x9600b4fa)
            compare('NitrousUpdate', m, struct.pack('<I', arena + 0x3000), arena, globals_for(players, drag, override), fraction=fraction, cw=cw)
        for active, old, new in itertools.product((False, True), (0, 1, 0xffffffff), (0, 1, 2, 0xffffffff)):
            m = frame(active); struct.pack_into('<I', m, 0x24, old); struct.pack_into('<I', m, 0xc9b8, new)
            compare('RegionUpdate', m, struct.pack('<I', arena + 0xc000), arena, globals_for(), cw=cw)
        for active, suppressed, reward, balance in itertools.product((False, True), (0, 1), (0, 1, -1, 1000), (0, -1, 123456)):
            m = frame(active); struct.pack_into('<i', m, 0x28, reward)
            compare('CashUpdate', m, struct.pack('<I', arena + 0x3000), this=arena, globals_=globals_for(), suppressed=suppressed, balance=balance, cw=cw)
        for active, pending, delay, kind, language in itertools.product((False, True), (0, 1), (-1, 0, 1, 2), range(6), (0, 1)):
            g = globals_for(); reward = bytearray(24); reward[0] = pending; struct.pack_into('<4i', reward, 4, 120, 30, kind, delay); g[3] = (0x83649c, reward)
            compare('RewardUpdate', frame(active), struct.pack('<I', arena + 0x3000), this=arena, globals_=g, language=language, cw=cw)
    bits = [0, 1, 4, 0x40, 0x80, 0x200, 0x400, 0x800, 0x1000, 0x4000, 0x20000,
            0x40000, 0x800000, 0x2000000, 0x20000000, 0x80000000, 0xffffffff]
    for presence, player_present, drift, script_active, race in itertools.product(range(3), (False, True), (0, 1), (0, 1), (0, 7)):
        for low, high in [(bit, 0) for bit in bits] + [(0, bit) for bit in bits] + [(rng.getrandbits(32), rng.getrandbits(32)) for _ in range(8)]:
            m = bytearray(0x10000)
            struct.pack_into('<II', m, 8, rng.getrandbits(32), rng.getrandbits(32))
            struct.pack_into('<I', m, 0x10, arena + 0x900)
            for slot in range(0x18, 0x84, 4):
                if presence == 1 or presence == 2 and slot % 8 == 0: struct.pack_into('<I', m, slot, arena + 0x2000 + slot * 16)
            g = globals_for() + [(0x89e7d9, bytes([drift])), (0x890104, struct.pack('<i', 0)), (0x89e7b0, struct.pack('<i', race))]
            compare('ControllerMasks', m, struct.pack('<III', arena + 0x3000 if player_present else 0, low, high), arena,
                    g, script_active=script_active)
    report = {'reference_sha256': hashlib.sha256(raw).hexdigest(), 'comparisons': counts, 'total': sum(counts.values()),
              'fingerprints': {n: {'address': hex(a), 'span': size, 'sha256': hashlib.sha256(image[a-0x400000:a-0x400000+size]).hexdigest()} for n, (a, size) in FUNCTIONS.items()},
              'interfaces': INTERFACES, 'byte_matching': False,
              'retained_helpers': ['__ftol2 0x75bed4', 'shift point getter 0x5947b0'],
              'interfaces_note': 'GetClassIndex 0x5b5330 is scripted; it is recovered in PhysicsRefresh.cpp',
              'execution_guard': True, 'x87_control_words': ['0x027f', '0x037f']}
    (BUILD / 'verification.json').write_text(json.dumps(report, indent=2) + '\n')
    print(json.dumps(counts, indent=2)); print('PASS', report['total'], 'comparisons')


if __name__ == '__main__': main()
