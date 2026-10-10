#!/usr/bin/env python3
"""Compare post-coupling body force and its real pair/vector callees with UG2."""
import hashlib
import itertools
import json
import random
import struct
import subprocess
import pefile
from unicorn import Uc, UC_ARCH_X86, UC_MODE_32, UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_ECX, UC_X86_REG_ESP, UC_X86_REG_EIP, UC_X86_REG_EBX, UC_X86_REG_ESI, UC_X86_REG_EDI, UC_X86_REG_EBP, UC_X86_REG_FPCW
from probe_pc_race_reward import ROOT, EXE
from pc_coff import link_objects

BUILD=ROOT/'build/analysis/racing'
LOOKUP='?Lookup@DriveFeedbackCurve@@QAEMM@Z'
SCALE='?WheelScale@@YAXPAUWheelVector@@0M@Z'
ADD='?AddForce@MoverBody@@QAEXPAUWheelVector@@@Z'
APPLY='?ApplyOpposingBodyForce@VehicleMover@@QAEXXZ'
ORIGINAL={LOOKUP:(0x576790,190,'28e5ee5be52e01e60a219f43be3b446201a2e4b0229596f740eece353c1f8266'),
          SCALE:(0x4011f0,39,'a9be14492340c82cdef0b2b91e6ebacaca90aff535850e5236ed51a2d48b6f42'),
          ADD:(0x59bc20,53,'94c3055219f1c1790007a2829c53dd7fcd2aa469d6a8f59f8639b869c97ad332'),
          APPLY:(0x5a5b60,188,'2fc2b4afdff4c166b1eaf38da1758f898f0b20781b4d41a88eda12f4665dbbac')}


def main():
    raw=EXE.read_bytes();assert hashlib.sha256(raw).hexdigest()=='f9dd86c054878ce6276beb07c1fd61874f7a1e4bf1f241b084c65b73e24168a7'
    pe=pefile.PE(data=raw);image,base=pe.get_memory_mapped_image(),pe.OPTIONAL_HEADER.ImageBase
    for address,size,digest in ORIGINAL.values():assert hashlib.sha256(image[address-base:address-base+size]).hexdigest()==digest
    paths=[]
    for unit in ('PairCurveLookup','BodyForce','MoverBodyForce'):
        obj=BUILD/(unit+'.obj');subprocess.run([str(ROOT/'build/tools/wibo'),'build/compilers/Win32/7.1/cl.exe','/nologo','/c','/Ox','/Ob0','/Z7','/EHsc','/Fo'+str(obj.relative_to(ROOT)),'src/Speed/Indep/Src/Physics/'+unit+'.cpp'],cwd=ROOT,check=True);paths.append(obj)
    externals={'?pairCurveEpsilon@@3MA':0x787594,'?moverOpposingForceThreshold@@3MA':0x7a1a50,'?moverOpposingForceCurve@@3UDriveFeedbackCurve@@A':0x86b4a0}
    linked=link_objects(paths,externals)
    arena,mover,body,owner,table,pairs,input_vector,output,stack,wrapper,stop=0x2000000,0x2000000,0x2001000,0x2002000,0x2003000,0x2004000,0x2005000,0x2006000,0x200f000,0x2100000,0x2101000
    machines=[];visits={};sites={0x5767aa:'first_endpoint',0x5767c2:'last_endpoint',0x5767fa:'first_fallback',0x576827:'interpolation',0x57683e:'midpoint',0x5a5be5:'body_curve',0x5a5c12:'body_add'}
    for rebuilt in (False,True):
        uc=Uc(UC_ARCH_X86,UC_MODE_32);uc.mem_map(base,(len(image)+4095)&~4095);uc.mem_write(base,image)
        uc.mem_map(arena,0x10000);uc.mem_map(wrapper,0x2000);uc.mem_map(linked['address'],linked['allocation'])
        if rebuilt:
            for address,code in linked['blocks']:uc.mem_write(address,code)
        entry=linked['exports'][LOOKUP] if rebuilt else ORIGINAL[LOOKUP][0]
        code=b'\xe8'+struct.pack('<i',entry-wrapper-5)+b'\xd9\x1d'+struct.pack('<I',output)+b'\xc3';uc.mem_write(wrapper,code)
        ranges=(linked['ranges'] if rebuilt else [(a,a+n) for a,n,d in ORIGINAL.values()])+[(wrapper,wrapper+len(code))]
        def guard(machine,address,size,user,ranges=ranges,rebuilt=rebuilt):
            assert any(a<=address<b for a,b in ranges),hex(address)
            if not rebuilt and address in sites:
                key=sites[address];visits[key]=visits.get(key,0)+1
        uc.hook_add(UC_HOOK_CODE,guard);machines.append(uc)
    matching={}
    for symbol,(address,size,digest) in ORIGINAL.items():
        function=linked['functions'][symbol]
        block_address,block=next((a,b) for a,b in linked['blocks'] if a<=function['address']<a+len(b))
        candidate=block[function['address']-block_address:function['address']-block_address+function['size']]
        # Same-size leaf helpers have no absolute/relative relocations; larger
        # callers are nonmatching by size, regardless of analysis load address.
        matching[symbol]=candidate==image[address-base:address-base+size]
    rng=random.Random(0x5a5b60);counts={s:0 for s in ORIGINAL}
    def compare(symbol,state,args,precision,writable,ecx):
        results=[]
        for rebuilt,uc in zip((False,True),machines):
            uc.mem_write(arena,bytes(state));uc.mem_write(0x86b4a0,bytes(state[table-arena:table-arena+8]));uc.mem_write(output-16,b'\xa5'*36)
            uc.reg_write(UC_X86_REG_ECX,ecx);uc.reg_write(UC_X86_REG_FPCW,precision)
            saved=(UC_X86_REG_EBX,UC_X86_REG_ESI,UC_X86_REG_EDI,UC_X86_REG_EBP)
            for reg in saved:uc.reg_write(reg,0x12345678)
            entry=linked['exports'][symbol] if rebuilt else ORIGINAL[symbol][0]
            if symbol==LOOKUP:
                uc.mem_write(stack,struct.pack('<IfI',0,*args,stop));uc.reg_write(UC_X86_REG_ESP,stack+4);uc.emu_start(wrapper,stop,count=10000);expected=stack+12
            else:
                uc.mem_write(stack,struct.pack('<I',stop)+args);uc.reg_write(UC_X86_REG_ESP,stack);uc.emu_start(entry,stop,count=10000)
                expected=stack+(8 if symbol==ADD else 4)
            assert uc.reg_read(UC_X86_REG_EIP)==stop
            assert uc.reg_read(UC_X86_REG_ESP)==expected
            assert uc.reg_read(UC_X86_REG_FPCW)==precision
            assert all(uc.reg_read(reg)==0x12345678 for reg in saved)
            result=bytes(uc.mem_read(arena,len(state)));masked=bytearray(result)
            for address in writable:masked[address-arena:address-arena+4]=state[address-arena:address-arena+4]
            assert masked==state,(symbol,'write footprint')
            out=bytes(uc.mem_read(output-16,36));assert out[:16]==b'\xa5'*16 and out[20:]==b'\xa5'*16
            results.append((result,out[16:20]))
        assert results[0]==results[1],(symbol,precision,args,[(hex(a),results[0][0][a-arena:a-arena+4].hex(),results[1][0][a-arena:a-arena+4].hex()) for a in writable],results[0][1].hex(),results[1][1].hex())
        counts[symbol]+=1
    def state_base():
        state=bytearray(b'\xa5'*0x6000)
        struct.pack_into('<I',state,0x20,body);struct.pack_into('<I',state,0x5c,owner)
        return state
    epsilon=struct.unpack_from('<f',image,0x787594-base)[0]
    for trial in range(512):
        state=state_base();count=(-1,0,1,2,3,5,9)[trial%7];n=max(count,1)
        coordinates=([0,epsilon*0.5,1] if count==3 and trial%2==0 else [float(i*10) for i in range(n)])
        struct.pack_into('<Ii',state,table-arena,pairs,count)
        for j in range(n):struct.pack_into('<ff',state,pairs-arena+j*8,coordinates[j],rng.uniform(-50,50))
        values=[-1,0,coordinates[-1],coordinates[-1]+1,rng.uniform(0,coordinates[-1]+1)]
        if n==3:values.append(epsilon*0.25)
        for value,precision in itertools.product(values,(0x027f,0x037f)):compare(LOOKUP,state,(value,),precision,[],table)
    for trial in range(512):
        state=state_base()
        for address in (input_vector,output-0x100,body+0x11c):
            for j in range(5):struct.pack_into('<f',state,address-arena+j*4,rng.uniform(-100,100))
        for j in range(3):struct.pack_into('<f',state,body+0x120-arena+j*4,rng.uniform(-100,100))
        for precision,alias in itertools.product((0x027f,0x037f),(-4,0,4,1000)):
            source=body+0x120+alias if alias!=1000 else input_vector
            compare(ADD,state,struct.pack('<I',source),precision,[body+off for off in (0x120,0x124,0x128)],body)
            target=input_vector+alias if alias!=1000 else output-0x100
            compare(SCALE,state,struct.pack('<IIf',target,input_vector,rng.choice((-2,-0.0,0,0.5,1,2))),precision,[target+j*4 for j in range(3)],0)
    threshold=struct.unpack_from('<f',image,0x7a1a50-base)[0]
    def neighbor(value,delta):
        bits=struct.unpack('<I',struct.pack('<f',value))[0]
        return struct.unpack('<f',struct.pack('<I',bits+delta))[0]
    for trial in range(1536):
        state=state_base();struct.pack_into('<i',state,owner+0x38-arena,(-1,0,1,2,3)[trial%5]);struct.pack_into('<i',state,owner+0x424-arena,(-1,0,1,2,3)[trial%5])
        # Independent state gates, speed threshold and zero/multicomponent motion.
        struct.pack_into('<i',state,owner+0x424-arena,(-1,0,1,2,3)[(trial//5)%5])
        velocity=([0,0,0] if trial%4==0 else [threshold*0.5,0,0] if trial%4==1 else [threshold*2,0,0] if trial%4==2 else [rng.uniform(-10,10) for _ in range(3)])
        if trial>=1024:
            velocity=[neighbor(threshold,(-1,0,1)[trial%3]),0,0]
        for j,v in enumerate(velocity):struct.pack_into('<f',state,body+0xc0-arena+j*4,v)
        struct.pack_into('<f',state,body+0x164-arena,rng.uniform(0.1,1500))
        for j in range(3):struct.pack_into('<f',state,body+0x120-arena+j*4,rng.uniform(-100,100))
        struct.pack_into('<Ii',state,table-arena,pairs,3)
        for j in range(3):struct.pack_into('<ff',state,pairs-arena+j*8,j*100,rng.uniform(0,2))
        for precision in (0x027f,0x037f):compare(APPLY,state,b'',precision,[body+off for off in (0x120,0x124,0x128)],mover)
    assert set(visits)==set(sites.values()),visits
    report=dict(executable_sha256=hashlib.sha256(raw).hexdigest(),functions=[dict(symbol=s,original_address=hex(a),original_size=n,original_sha256=d,rebuilt_size=linked['functions'][s]['size'],byte_identical=matching[s]) for s,(a,n,d) in ORIGINAL.items()],comparisons=counts,branch_visits=visits,stubbed_callees=[],checks=['exact whole state and float return','write footprint and guards','aliases and partial overlaps','state/mode/speed gates','stack/register/control-word preservation'],limitations=['Finite synthetic tables and body states','Retail initialization of opposing-force curve not recovered','Does not integrate body pose or generate contacts'])
    (BUILD/'body-force-verification.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report,indent=2))


if __name__=='__main__':main()
