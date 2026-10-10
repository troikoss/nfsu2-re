#!/usr/bin/env python3
"""Compare original/rebuilt five-pass mover, with recovered real callees."""
import hashlib
import json
import math
import random
import struct
import subprocess
import pefile
from unicorn import Uc, UC_ARCH_X86, UC_MODE_32, UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_ECX, UC_X86_REG_ESP, UC_X86_REG_EIP, UC_X86_REG_EBX, UC_X86_REG_ESI, UC_X86_REG_EDI, UC_X86_REG_EBP, UC_X86_REG_FPCW
from probe_pc_race_reward import ROOT, EXE
from pc_coff import link_objects
import verify_pc_drivetrain_coupling as coupling
import verify_pc_wheel_preparation_helpers as helpers
import verify_pc_wheel_preparation as preparation
from verify_pc_engine import CONVERSION

BUILD = ROOT/'build/analysis/racing'
SYMBOL = '?CorrectWheelCoupling@VehicleMover@@QAEXXZ'
ADDRESS, SIZE, DIGEST = 0x5abdf0, 1136, '93627fe9e8572e1972ebad92d88d7b027eb5f414d101e72603f635a388162687'


def main():
    raw = EXE.read_bytes()
    assert hashlib.sha256(raw).hexdigest() == 'f9dd86c054878ce6276beb07c1fd61874f7a1e4bf1f241b084c65b73e24168a7'
    pe = pefile.PE(data=raw); image, base = pe.get_memory_mapped_image(), pe.OPTIONAL_HEADER.ImageBase
    originals = list(coupling.ORIGINAL.values()) + list(helpers.ORIGINAL.values()) + list(preparation.WHEEL_ORIGINAL.values())
    originals += [(ADDRESS, SIZE, DIGEST), (preparation.ADDRESS, preparation.SIZE, preparation.DIGEST),
                  (preparation.CORRECT_ADDRESS, preparation.CORRECT_SIZE, preparation.CORRECT_DIGEST),
                  (preparation.SCALE_ADDRESS, preparation.SCALE_SIZE, preparation.SCALE_DIGEST),
                  (preparation.ANGLE_ADDRESS, preparation.ANGLE_SIZE, preparation.ANGLE_DIGEST),
                  preparation.MATH_ORIGINAL[preparation.SINE], preparation.MATH_ORIGINAL[preparation.UNITS], CONVERSION]
    for address, size, digest in originals:
        assert hashlib.sha256(image[address-base:address-base+size]).hexdigest() == digest
    units = ('VehicleMover', 'DrivetrainCoupling', 'DrivetrainDistribution', 'EngineDriveValue', 'EngineCurve',
             'DrivetrainWheelRate', 'DrivetrainWheelValue', 'DrivetrainWheelScalar', 'DrivetrainFinishing',
             'DriveFeedbackCurve', 'DrivetrainSynchronization', 'EngineMath', 'WheelPreparation',
             'WheelPreparationHelpers', 'WheelForceCorrection', 'WheelCorrectionScale', 'WheelMath', 'WheelAngle')
    paths = []
    for unit in units:
        obj = BUILD/(unit+'.obj')
        subprocess.run([str(ROOT/'build/tools/wibo'), 'build/compilers/Win32/7.1/cl.exe', '/nologo', '/c', '/Ox', '/Ob0', '/Z7', '/EHsc',
                        '/Fo'+str(obj.relative_to(ROOT)), 'src/Speed/Indep/Src/Physics/'+unit+'.cpp'], cwd=ROOT, check=True)
        paths.append(obj)
    globals_ = {**coupling.POST_FLOATS, **coupling.GLOBALS, **preparation.FLOATS, **preparation.GLOBALS}
    externals = {'?'+name+'@@3MA': address for name, address in globals_.items()}
    externals.update(preparation.SCALE_GLOBALS)
    externals.update({'?engineExtraCoefficients@@3PAMA': 0x7a1798, '__ftol2': CONVERSION[0],
                      '?drivetrainFinishingCoefficients@@3PAMA': 0x7a18ac, '?drivetrainFinishingCurve@@3UDriveFeedbackCurve@@A': 0x7fc8c0,
                      '?wheelSurfaces@@3PAUWheelSurface@@A': 0x800f04, '?wheelForceBasisMode@@3HA': 0x7fc7fc,
                      '?wheelAngleTable@@3PAEA': preparation.TABLE, '?wheelAngleRatioScale@@3MA': 0x786db8,
                      '?wheelLateralMode@@3HA': 0x7fc7f0, '?wheelLateralFallbackCurve@@3UEngineCurve@@A': 0x7fc954,
                      '?wheelLateralCurves@@3PAPAUEngineCurve@@A': 0x7fc7d4, '?wheelLateralLastCurve@@3PAUEngineCurve@@A': 0x7fc7ec})
    linked = link_objects(paths, externals)
    arena, mover, body, owner, mode, arms, drive, spec, engine, engine_spec, attributes, aux, wheels, wheel_specs, correction, prep, curves, samples, stack, stop = (
        0x2000000, 0x2000000, 0x2001000, 0x2002000, 0x2002100, 0x2003000, 0x2004000, 0x2004200, 0x2005000,
        0x2005200, 0x2006000, 0x2007000, 0x2008000, 0x200a000, 0x200b000, 0x200c000, 0x200d000, 0x200e000, 0x202f000, 0x2100000)
    state_size = 0x10000
    sites = {0x5abe29: 'prepare', 0x5abe70: 'correction_pass', 0x5ac229: 'drive_correction', 0x5ac243: 'force_correction',
             0x5ac254: 'finish', 0x5ac035: 'alternate_matrix', 0x5ac03d: 'base_matrix',
             0x5ac068: 'wheel_zero_adjust', 0x5ac0db: 'wheel_one_adjust', 0x5ac151: 'wheel_two_adjust', 0x5ac1c7: 'wheel_three_adjust'}
    visits = {}; machines = []
    for rebuilt in (False, True):
        uc = Uc(UC_ARCH_X86, UC_MODE_32)
        uc.mem_map(base, (len(image)+4095)&~4095); uc.mem_write(base, image)
        uc.mem_map(arena, 0x30000); uc.mem_map(stop, 0x1000)
        uc.mem_map(linked['address'], linked['allocation'])
        if rebuilt:
            for address, code in linked['blocks']: uc.mem_write(address, code)
        ranges = linked['ranges'] if rebuilt else [(a, a+n) for a, n, _ in originals]+[(0x43c9c0,0x43c9c5)]
        if rebuilt: ranges += [(CONVERSION[0],CONVERSION[0]+CONVERSION[1])]
        def guard(machine, address, size, user, ranges=ranges, rebuilt=rebuilt):
            assert any(a <= address < b for a, b in ranges), hex(address)
            if not rebuilt and address in sites:
                key=sites[address];visits[key]=visits.get(key,0)+1
        uc.hook_add(UC_HOOK_CODE, guard); machines.append(uc)
    rng = random.Random(ADDRESS); comparisons = 0
    writable = [drive+off for off in (0x4c,0x58,0x70)] + [engine+0x7c]
    for j in range(4): writable += [wheels+j*0x400+off for off in (0x28,0x100,0x104,0x108,0x10c,0x118,0x124,0x128,0x130,0x134,0x138,0x16c,0x170)]
    for trial in range(1536):
        state = bytearray(b'\xa5'*state_size)
        def put(address, fmt, value): struct.pack_into(fmt,state,address-arena,value)
        def f(address,value): put(address,'<f',value)
        def i(address,value): put(address,'<i',value)
        i(mover+0x20,body);i(mover+0x4c,drive);i(mover+0x5c,owner);i(owner+0x3c,mode);put(mode+0x18,'<b',(-1,0,1,127)[trial%4])
        i(drive+0x18,spec);i(drive+0x20,engine);i(drive+0x34,aux)
        for off,val in ((0x3c,trial%8),(0x44,(trial//8)%4),(0xf0,(trial//32)%4)): i(drive+off,val)
        for off,val in ((0x14,rng.uniform(0,1)),(0x4c,rng.uniform(-100,100)),(0x58,rng.choice((-3000,-100,0,100,3000))),(0x70,0),(0xec,(0,0.5,1)[trial%3])): f(drive+off,val)
        for off,val in ((8,1.5),(0xc,2),(0x10,(0,0.5,1)[trial%3]),(0x14,1)) : f(spec+off,val)
        for j,val in enumerate((-3,0,3,2,1.5,1,0.9,0.7)): f(spec+0x20+j*4,val)
        i(engine+0x18,engine_spec);i(engine+0x1c,attributes);i(attributes,attributes+0x100);f(attributes+0x100+0x2ac,1);i(engine+0x20,drive)
        for off,val in ((0x14,rng.uniform(0,500)),(0x78,0.5),(0x7c,0),(0x80,0.5),(0x8c,0.5),(0x90,1)): f(engine+off,val)
        i(engine+0x74,trial%2);i(engine+0x84,(-1,0,1,3)[trial%4]);put(engine+0x88,'<B',trial%2);f(engine_spec+0xc,1)
        i(aux+0x3e4,trial%5-1);f(aux+0x3ec,(0,0.25,20)[trial%3]);f(aux+0x3f0,trial%3*50);i(aux+0x424,trial%2);i(aux+0x42c,trial%4)
        for matrix in (body+0x30,body+0x170,body+0x1b0):
            for j in range(16): f(matrix+j*4,float(j%5==0))
        f(body+0x170+0x28,0.01);f(body+0x1b0+0x28,0.02);i(body+0x158,trial%2);f(body+0x164,rng.uniform(500,1500))
        for j,val in enumerate((rng.uniform(-2,2),rng.uniform(-2,2),0)): f(body+0x110+j*4,val)
        if trial >= 768:
            rotation=rng.uniform(-1,1);c,s=math.cos(rotation),math.sin(rotation)
            for off,val in ((0,c),(4,s),(16,-s),(20,c)): f(body+0x30+off,val)
            f(body+0x170+0x28,rng.uniform(0.005,0.02));f(body+0x1b0+0x28,rng.uniform(0.005,0.02))
        curve_records=[]
        for j in range(12):
            record=struct.pack('<ifffI',5,0,100,0.04,samples+j*0x40)
            start=curves+j*0x20-arena;state[start:start+20]=record
            for k in range(5): f(samples+j*0x40+k*4,rng.uniform(0,20))
            curve_records.append(record)
        for j in range(4):
            start=engine+0x24+j*0x14-arena;state[start:start+20]=curve_records[8+j]
            wheel=wheels+j*0x400;ws=wheel_specs+j*0x40;cs=correction+j*0x40;ps=prep+j*0x40;arm=arms+j*0x100
            i(mover+0x28+j*4,arm);i(mover+0x38+j*4,wheel);i(drive+0x24+j*4,wheel)
            f(arm+0x90,(-1,1,-1,1)[j]);f(arm+0x94,(1,1,-1,-1)[j])
            i(wheel+0x14,ws);i(wheel+0x18,cs);i(wheel+0x1c,ps);i(wheel+0x20,aux)
            f(ws+0x10,0.5);i(ws+0x18,2);i(wheel+0x114,trial%3);i(wheel+0x10c,(trial>>j)&1);put(wheel+0x11c,'<B',(trial>>j)&1)
            for off,val in ((0,1),(4,1),(8,1),(0xc,1),(0x10,1)): f(cs+off,val)
            for off,val in ((4,0),(8,0)): f(ps+off,val)
            for k in range(16): f(wheel+0x70+k*4,float(k%5==0))
            vx=rng.choice((-1,0,1));vy=rng.choice((0,0.1,1))
            for off,val in ((0xb0,1),(0xb4,0),(0xb8,0),(0xc0,0),(0xc4,1),(0xc8,0),(0xf0,vx),(0xf4,vy),(0xf8,0),
                (0x150,0),(0x154,0),(0x158,1),(0x28,2*vx if trial%3==0 else rng.uniform(-10,10)),(0x2c,1),
                (0x164,rng.uniform(10,500)),(0x178,1),(0x17c,1),(0x180,1),(0x184,1),(0x188,1),(0x190,1),(0x194,1)):
                f(wheel+off,val)
            if trial >= 768:
                rotation=rng.uniform(-0.5,0.5);c,s=math.cos(rotation),math.sin(rotation)
                for off,val in ((0x70,c),(0x74,s),(0x80,-s),(0x84,c),(0xb0,c),(0xb4,s),(0xc0,-s),(0xc4,c)):
                    f(wheel+off,val)
                f(arm+0x90,rng.uniform(-2,2));f(arm+0x94,rng.uniform(-2,2))
                f(ws+0x10,rng.uniform(0.2,1));f(wheel+0x2c,rng.uniform(0.1,2))
        for precision in (0x027f,0x037f):
            results=[]
            for rebuilt,uc in zip((False,True),machines):
                uc.mem_write(arena,bytes(state));uc.mem_write(0x7fc7fc,struct.pack('<i',trial%5));uc.mem_write(0x7fc7f0,struct.pack('<i',trial%2))
                uc.mem_write(0x7fc954,curve_records[7])
                for j in range(7): uc.mem_write(0x7fc7d4+j*4,struct.pack('<I',curves+j*0x20))
                for j in range(3): uc.mem_write(0x800f04+j*0x98,struct.pack('<ff',(0.1,0.5,1)[j],(0.1,0.5,1)[j]))
                uc.mem_write(stack,struct.pack('<I',stop));uc.reg_write(UC_X86_REG_ESP,stack);uc.reg_write(UC_X86_REG_ECX,mover);uc.reg_write(UC_X86_REG_FPCW,precision)
                saved=(UC_X86_REG_EBX,UC_X86_REG_ESI,UC_X86_REG_EDI,UC_X86_REG_EBP)
                for reg in saved: uc.reg_write(reg,0x12345678)
                uc.emu_start(linked['exports'][SYMBOL] if rebuilt else ADDRESS,stop,count=100000)
                assert uc.reg_read(UC_X86_REG_EIP)==stop
                assert uc.reg_read(UC_X86_REG_ESP)==stack+4
                assert uc.reg_read(UC_X86_REG_FPCW)==precision
                assert all(uc.reg_read(reg)==0x12345678 for reg in saved)
                result=bytes(uc.mem_read(arena,state_size));masked=bytearray(result)
                for address in writable: masked[address-arena:address-arena+4]=state[address-arena:address-arena+4]
                for j in range(4): masked[wheels+j*0x400+0x160-arena]=state[wheels+j*0x400+0x160-arena]
                assert masked==state,('write footprint',trial,rebuilt)
                results.append(result)
            if results[0]!=results[1]:
                differences=[(hex(a),struct.unpack_from('<f',results[0],a-arena)[0],struct.unpack_from('<f',results[1],a-arena)[0]) for a in writable if results[0][a-arena:a-arena+4]!=results[1][a-arena:a-arena+4]]
                raise AssertionError((trial,precision,differences))
            comparisons+=1
    assert set(visits)==set(sites.values()),visits
    for name, multiplier in (('prepare',4),('correction_pass',5),('drive_correction',5),('force_correction',4),('finish',1)):
        assert visits[name] == comparisons*multiplier, (name,visits[name],comparisons)
    report=dict(original_address=hex(ADDRESS),original_size=SIZE,original_sha256=DIGEST,rebuilt_size=linked['functions'][SYMBOL]['size'],comparisons=comparisons,
                branch_visits=visits,stubbed_callees=[],original_runtime_dependency=CONVERSION,
                checks=['exact whole state','write footprint','original four preparations/five corrections/four force corrections/finish','stack/register/control-word preservation'],
                limitations=['Synthetic bounded vehicle states','Original conversion runtime and retail data remain dependencies','This routine does not integrate body pose or produce contacts','Native playable demo remains incomplete'])
    (BUILD/'vehicle-mover-verification.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report,indent=2))


if __name__=='__main__':main()
