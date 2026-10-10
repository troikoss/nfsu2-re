#!/usr/bin/env python3
"""Compare original/rebuilt drivetrain coupling with real recovered callees."""
import hashlib
import json
import random
import struct
import subprocess
import pefile
from unicorn import Uc, UC_ARCH_X86, UC_MODE_32, UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_ECX, UC_X86_REG_ESP, UC_X86_REG_EIP, UC_X86_REG_EBX, UC_X86_REG_ESI, UC_X86_REG_EDI, UC_X86_REG_EBP, UC_X86_REG_FPCW
from probe_pc_race_reward import ROOT, EXE
from pc_coff import link_objects
from verify_pc_drift_ranking import ADDRESSES, coff_function
from verify_pc_engine import ORIGINAL as ENGINE_ORIGINAL, FLOATS, CONVERSION, CURVE, DRIVE
from verify_pc_drivetrain_wheel_feedback import FUNCTIONS as WHEEL_FUNCTIONS
from verify_pc_drivetrain import VALUE, ORIGINAL as VALUE_ORIGINAL
from verify_pc_engine_post_update import CLAMP, FLOATS as POST_FLOATS, ORIGINAL as POST_ORIGINAL
from verify_pc_drive_feedback_curve import EVALUATE, ORIGINAL_CURVE, ORIGINAL_LERP
from verify_pc_drivetrain_synchronization import SYMBOL as SYNC, ORIGINAL as SYNC_ORIGINAL
from verify_pc_drivetrain_distribution import SYMBOL as DISTRIBUTE, ADDRESS as DISTRIBUTE_ADDRESS, SIZE as DISTRIBUTE_SIZE, DIGEST as DISTRIBUTE_DIGEST

BUILD = ROOT/'build/analysis/racing'
INITIAL = '?ApplyInitialDriveValue@Drivetrain@@QAEXXZ'
CORRECTION = '?ApplyDriveCorrection@Drivetrain@@QAEXXZ'
FINISH = '?FinishDriveCoupling@Drivetrain@@QAEXXZ'
LERP = '?InterpolateDriveFeedback@@YGXPAM00M@Z'
ORIGINAL = {
    FINISH: (0x5aa830, 294, "b95888e596b20554280a11f28796b4d7510836e4c1b33ab6b5787c5c45821340"),
    INITIAL: (0x594cb0, 233, '5176544eb8374c2a0726baf15380c039481123737b3a21cc184339a4a24fc8ed'),
    CORRECTION: (0x594da0, 225, 'd663fcb5e9f686e221bc1bcae5d81bb9930e4f7d874bc9780dd21818d0c33a89'),
    DISTRIBUTE: (DISTRIBUTE_ADDRESS, DISTRIBUTE_SIZE, DISTRIBUTE_DIGEST),
    CURVE: ENGINE_ORIGINAL[CURVE], DRIVE: ENGINE_ORIGINAL[DRIVE],
}
RATE = '?GetWheelDerivedRate@Drivetrain@@QAEMXZ'
ORIGINAL[RATE] = WHEEL_FUNCTIONS[RATE][1:]
SCALAR = '?GetAverageWheelScalar@Drivetrain@@QAEMXZ'
ORIGINAL.update({VALUE: VALUE_ORIGINAL[VALUE], SCALAR: WHEEL_FUNCTIONS[SCALAR][1:], SYNC: SYNC_ORIGINAL[SYNC],
                 CLAMP: POST_ORIGINAL[CLAMP], EVALUATE: ORIGINAL_CURVE, LERP: ORIGINAL_LERP})
GLOBALS = {'drivetrainPairThreshold': 0x7fbffc, 'drivetrainSumEpsilon': 0x7845a0,
           'drivetrainDriveDifferenceLimit': 0x7fbf34, 'drivetrainCrossingThreshold': 0x78a514,
           'drivetrainCrossingScale': 0x7a1924, 'drivetrainSynchronizationScale': 0x78426c,
           'drivetrainSynchronizationThreshold': 0x7864d8, 'drivetrainFinishingInputThreshold': 0x784554,
           'drivetrainFinishingWheelThreshold': 0x784358}


def main():
    BUILD.mkdir(parents=True, exist_ok=True)
    raw = EXE.read_bytes()
    assert hashlib.sha256(raw).hexdigest() == 'f9dd86c054878ce6276beb07c1fd61874f7a1e4bf1f241b084c65b73e24168a7'
    pe = pefile.PE(data=raw)
    image, base = pe.get_memory_mapped_image(), pe.OPTIONAL_HEADER.ImageBase
    for address, size, digest in (*ORIGINAL.values(), CONVERSION):
        assert hashlib.sha256(image[address-base:address-base+size]).hexdigest() == digest
    units = {FINISH: 'DrivetrainFinishing', SCALAR: 'DrivetrainWheelScalar', VALUE: 'DrivetrainWheelValue',
             SYNC: 'DrivetrainSynchronization', CLAMP: 'EngineMath', EVALUATE: 'DriveFeedbackCurve', LERP: 'DriveFeedbackCurve', INITIAL: 'DrivetrainCoupling', CORRECTION: 'DrivetrainCoupling', DISTRIBUTE: 'DrivetrainDistribution',
             RATE: 'DrivetrainWheelRate', CURVE: 'EngineCurve', DRIVE: 'EngineDriveValue'}
    paths = []
    for unit in dict.fromkeys(units.values()):
        obj = BUILD/(unit+'.obj')
        subprocess.run([str(ROOT/'build/tools/wibo'), 'build/compilers/Win32/7.1/cl.exe', '/nologo', '/c', '/Ox', '/Ob0', '/Z7', '/EHsc',
                        '/Fo'+str(obj.relative_to(ROOT)), 'src/Speed/Indep/Src/Physics/'+unit+'.cpp'], cwd=ROOT, check=True)
        paths.append(obj)
    externals = {'?'+name+'@@3MA': address for name, address in {**POST_FLOATS, **GLOBALS}.items()}
    externals.update({'?engineExtraCoefficients@@3PAMA': 0x7a1798, '__ftol2': CONVERSION[0],
                      '?drivetrainFinishingCoefficients@@3PAMA': 0x7a18ac, '?drivetrainFinishingCurve@@3UDriveFeedbackCurve@@A': 0x7fc8c0})
    linked = link_objects(paths, externals)
    ADDRESSES.update(externals)
    ADDRESSES.update({symbol: data[0] for symbol, data in ORIGINAL.items()})
    ADDRESSES.update({'__real@00000000': 0x7d7d3c, '__real@3f800000': 0x784250,
                      '__real@3f000000': 0x784260, '__real@3e800000': 0x784d28,
                      '__real@3a83126f': 0x78435c, '__real@3f7fbe77': 0x79b158})
    records = []
    for symbol in (INITIAL, CORRECTION, FINISH):
        body, relocs = coff_function(BUILD/(units[symbol]+'.obj'), symbol)
        address, size, digest = ORIGINAL[symbol]
        records.append(dict(symbol=symbol, original_address=hex(address), original_size=size, rebuilt_size=len(body), original_sha256=digest,
                            byte_identical_after_relocation=body == image[address-base:address-base+size], relocations=relocs))
    this, spec, wheels, wheel_specs = 0x2000000, 0x2001000, 0x2002000, 0x2004000
    engine, engine_spec, owner, attributes, auxiliary, samples, stack, stop = 0x2005000, 0x2006000, 0x2007000, 0x2008000, 0x2009000, 0x200a000, 0x200f000, 0x2100000
    state_size = 0xb000
    machines, entries = [], []
    finishing_branches = {"synchronization": 0, "bounded_feedback": 0, "ratio_two_curve": 0}
    branch_sites = {0x5aa8a2: "synchronization", 0x5aa8d8: "bounded_feedback", 0x5aa92e: "ratio_two_curve"}
    for rebuilt in (False, True):
        uc = Uc(UC_ARCH_X86, UC_MODE_32)
        uc.mem_map(base, (len(image)+4095) & ~4095)
        uc.mem_write(base, image)
        uc.mem_map(this, 0x10000)
        uc.mem_map(stop, 0x1000)
        uc.mem_map(linked['address'], linked['allocation'])
        if rebuilt:
            for address, body in linked['blocks']:
                uc.mem_write(address, body)
        ranges = linked['ranges'] if rebuilt else [(a, a+n) for a, n, _ in ORIGINAL.values()]
        ranges = ranges + [(CONVERSION[0], CONVERSION[0]+CONVERSION[1])]
        def guard(machine, address, size, user, ranges=ranges):
            assert any(a <= address < b for a, b in ranges), hex(address)
            if address in branch_sites:
                finishing_branches[branch_sites[address]] += 1
        uc.hook_add(UC_HOOK_CODE, guard)
        machines.append(uc)
        entries.append(linked['exports'] if rebuilt else {symbol: data[0] for symbol, data in ORIGINAL.items()})
    rng = random.Random(0x594cb0)
    counts = {INITIAL: 0, CORRECTION: 0, FINISH: 0, 'initial_then_five_corrections': 0, 'initial_five_corrections_finish': 0}
    selectors = [0, 0.5, 1]
    for va in (0x78435c, 0x79b158):
        bits = struct.unpack_from('<I', image, va-base)[0]
        selectors += [struct.unpack('<f', struct.pack('<I', bits+d))[0] for d in (-1, 0, 1)]
    writable = [this+0x4c, this+0x58, this+0x70, engine+0x7c] + [wheels+i*0x400+off for i in range(4) for off in (0x28, 0x108)]
    for trial in range(2304):
        state = bytearray(rng.randbytes(state_size))
        def put(address, fmt, value):
            struct.pack_into(fmt, state, address-this, value)
        ratio_index, mode = trial%8, (trial//8)%4
        engine_state = rng.uniform(-100, 1000)
        wheel_value = engine_state + (-400, -300, -210, -100, 0, 100, 210, 300, 400)[trial%9]
        for address, value in ((this+0x18, spec), (this+0x20, engine), (this+0x34, auxiliary), (this+0x3c, ratio_index),
                               (this+0x44, mode), (this+0xf0, (trial//4)%4), (auxiliary+0x3e4, trial%5-1), (auxiliary+0x42c, (trial//32)%4),
                               (engine+0x18, engine_spec), (engine+0x1c, owner), (engine+0x20, this), (owner, attributes),
                               (engine+0x74, trial%2), (engine+0x84, (-1, 0, 1, 3)[(trial//2)%4])):
            put(address, '<i' if value < 0 else '<I', value)
        put(engine+0x88, '<B', (trial//4)%2)
        for address, value in ((this+0x14, rng.uniform(0, 1)), (this+0x4c, wheel_value),
                               (this+0x58, (-3000, -2000, -100, 0, 100, 2000, 3000)[trial%7]),
                               (this+0x70, rng.uniform(-1000, 1000)), (this+0xec, (0, 0.5, 1)[trial//768]),
                               (spec+8, rng.uniform(0.5, 5)), (spec+12, rng.uniform(0.5, 5)), (spec+16, selectors[trial%9]),
                               (spec+0x14, rng.uniform(0.1, 50)),
                               (spec+0x20+ratio_index*4, (-3.1, 0, 3.4, 2.1, 1.4, 1.1, 0.9, 0.75)[ratio_index]),
                               (auxiliary+0x3f0, (-100, 0, 100)[trial%3]), (engine+0x14, engine_state), (engine+0x78, (0, 0.4, 1)[trial%3]), (engine+0x7c, rng.uniform(-100, 100)),
                               (engine+0x80, rng.uniform(0, 1)), (engine+0x8c, (0, 0.6, 1, 1.5)[trial%4]),
                               (engine+0x90, rng.uniform(0.5, 2)), (engine_spec+0xc, rng.uniform(0.1, 5)),
                               (attributes+0x2ac, rng.uniform(0, 2))):
            put(address, '<f', value)
        for i, count in enumerate((9, 3, 9, 9)):
            data = samples+i*0x100
            struct.pack_into('<ifffI', state, engine+0x24+i*0x14-this, count, 750, 8500, (count-1)/7750, data)
            for j in range(count):
                put(data+j*4, '<f', rng.uniform(-100, 500))
        for i in range(4):
            wheel, data = wheels+i*0x400, wheel_specs+i*0x100
            put(this+0x24+i*4, '<I', wheel)
            put(wheel+0x14, '<I', data)
            put(wheel+0x10c, '<I', ((trial//16)>>i)&1)
            put(wheel+0x11c, '<B', ((trial//256)>>i)&1)
            for address, value in ((wheel+0x28, rng.uniform(-500, 500)), (wheel+0x108, rng.uniform(-10000, 10000)),
                                   (wheel+0x124, rng.uniform(-100, 100)), (wheel+0x2c, rng.uniform(0.1, 100)),
                                   (wheel+0x16c, (-0.1, 0.1, 0.5)[(trial+i)%3]), (data+0x10, rng.uniform(0.1, 5))):
                put(address, '<f', value)
        for precision in (0x027f, 0x037f):
            for label, sequence in ((INITIAL, [INITIAL]), (CORRECTION, [CORRECTION]), (FINISH, [FINISH]),
                                    ('initial_then_five_corrections', [INITIAL]+[CORRECTION]*5),
                                    ('initial_five_corrections_finish', [INITIAL]+[CORRECTION]*5+[FINISH])):
                results = []
                for uc, entry in zip(machines, entries):
                    uc.mem_write(this, bytes(state))
                    for symbol in sequence:
                        uc.mem_write(stack, struct.pack('<I', stop))
                        uc.reg_write(UC_X86_REG_ESP, stack)
                        uc.reg_write(UC_X86_REG_ECX, this)
                        uc.reg_write(UC_X86_REG_FPCW, precision)
                        saved = (UC_X86_REG_EBX, UC_X86_REG_ESI, UC_X86_REG_EDI, UC_X86_REG_EBP)
                        for reg in saved:
                            uc.reg_write(reg, 0x12345678)
                        uc.emu_start(entry[symbol], stop, count=10000)
                        assert uc.reg_read(UC_X86_REG_EIP) == stop
                        assert uc.reg_read(UC_X86_REG_ESP) == stack+4
                        assert all(uc.reg_read(reg) == 0x12345678 for reg in saved)
                    result = bytes(uc.mem_read(this, state_size))
                    unchanged = bytearray(result)
                    for address in writable:
                        offset = address-this
                        unchanged[offset:offset+4] = state[offset:offset+4]
                    assert unchanged == state
                    if label == CORRECTION and mode != 3:
                        assert result == state
                    results.append(result)
                if results[0] != results[1]:
                    differences = [(hex(this+i), results[0][i:i+4].hex(), results[1][i:i+4].hex()) for i in range(0, state_size, 4) if results[0][i:i+4] != results[1][i:i+4]]
                    raise AssertionError((trial, precision, label, differences))
                counts[label] += 1
    assert all(finishing_branches.values()), finishing_branches
    report = dict(finishing_branch_calls=finishing_branches, executable_sha256=hashlib.sha256(raw).hexdigest(), functions=records, comparisons=counts,
                  compiler='MSVC 7.1 /Ox /Ob0 /Z7 /EHsc', stubbed_callees=[], original_runtime_dependency=CONVERSION,
                  checks=['exact whole input state', 'only twelve observed float fields writable', 'stack and preserved registers', 'non-active correction no-op'],
                  limitations=['Synthetic finite curves and states', 'Five consecutive corrections omit intervening wheel/body updates',
                               'Actual vehicle parameter loading and full mover not recovered'])
    (BUILD/'drivetrain-coupling-verification.json').write_text(json.dumps(report, indent=2)+'\n')
    print(json.dumps(report, indent=2))


if __name__ == '__main__':
    main()
