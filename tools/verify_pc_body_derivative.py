#!/usr/bin/env python3
"""Compare the original body derivative and matrix writer with real callees."""
import hashlib
import json
import random
import struct
import subprocess
import pefile
from unicorn import Uc, UC_ARCH_X86, UC_MODE_32, UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EAX, UC_X86_REG_ECX, UC_X86_REG_ESP, UC_X86_REG_EIP, UC_X86_REG_EBX, UC_X86_REG_ESI, UC_X86_REG_EDI, UC_X86_REG_EBP, UC_X86_REG_FPCW
from probe_pc_race_reward import ROOT, EXE
from pc_coff import link_objects
from verify_pc_drift_ranking import ADDRESSES, coff_function
from verify_pc_engine import ORIGINAL as ENGINE_ORIGINAL, CURVE, CONVERSION
from verify_pc_wheel_math import ORIGINAL as WHEEL_ORIGINAL

BUILD=ROOT/'build/analysis/racing'
DERIVATIVE='?WriteDerivative@MoverBody@@QAEPAMPAM@Z'
MATRIX='?BodyMatrixDerivative@@YAXPAUWheelMatrix@@PAUWheelVector@@0@Z'
ORIGINAL={DERIVATIVE:(0x59c1e0,1033,'702e0049115d9834fc68e24a1a7f763234e3cc9a086dc225b64b311872f67db6'),
          MATRIX:(0x59c120,186,'2a41e986b95c53ca99f09cce43d89367e088b47730de9b2380484d13cede1bbb')}
GLOBALS={'bodyThresholdBase':0x7a1e80,'bodyFlagThresholdScale':0x7a1b18,'bodyPositiveTiltLimit':0x7a1af4,
         'bodyPositiveTiltFactor':0x7a1af0,'bodyNegativeTiltLimit':0x7a1aec,'bodyNegativeTiltFactor':0x7a1ae8,
         'bodyHorizontalScale':0x7a1e7c,'bodyTiltResponseScale':0x7a1afc,'bodyTiltResponseMaximum':0x7a1af8,
         'bodyAScale':0x7a1b00,'bodyCScale':0x7a1b04,'bodyScalarRise':0x7a1b10,'bodyScalarFall':0x7a1b14,
         'bodyAngularDamping':0x7a1b0c,'bodyLinearDamping':0x7a1b08}


def main():
    raw=EXE.read_bytes();assert hashlib.sha256(raw).hexdigest()=='f9dd86c054878ce6276beb07c1fd61874f7a1e4bf1f241b084c65b73e24168a7'
    pe=pefile.PE(data=raw);image,base=pe.get_memory_mapped_image(),pe.OPTIONAL_HEADER.ImageBase
    originals=list(ORIGINAL.values())+list(WHEEL_ORIGINAL.values())+[ENGINE_ORIGINAL[CURVE],CONVERSION]
    for a,n,d in originals:assert hashlib.sha256(image[a-base:a-base+n]).hexdigest()==d
    paths=[]
    for unit in ('BodyDerivative','BodyMatrixDerivative','WheelMath','EngineCurve'):
        obj=BUILD/(unit+'.obj');subprocess.run([str(ROOT/'build/tools/wibo'),'build/compilers/Win32/7.1/cl.exe','/nologo','/c','/Ox','/Ob0','/Z7','/EHsc','/Fo'+str(obj.relative_to(ROOT)),'src/Speed/Indep/Src/Physics/'+unit+'.cpp'],cwd=ROOT,check=True);paths.append(obj)
    externals={'?'+k+'@@3MA':v for k,v in GLOBALS.items()}
    externals.update({'?bodyDampingCurve@@3UEngineCurve@@A':0x7fc92c,'?bodyScalarCurve@@3UEngineCurve@@A':0x7fc940,'__ftol2':CONVERSION[0]})
    linked=link_objects(paths,externals)
    ADDRESSES.update({'__real@00000000':0x7d7d3c,'__real@3f800000':0x784250,MATRIX:ORIGINAL[MATRIX][0]})
    matrix_code,matrix_relocations=coff_function(BUILD/'BodyMatrixDerivative.obj',MATRIX)
    matrix_original=ORIGINAL[MATRIX]
    matrix_matching=matrix_code==image[matrix_original[0]-base:matrix_original[0]-base+matrix_original[1]]
    arena,body,matrix,vector,output,samples,stack,stop=0x2000000,0x2000000,0x2001000,0x2002000,0x2003000,0x2004000,0x200f000,0x2100000
    visits={};sites={0x59c205:'conditional_adjustment',0x59c250:'positive_tilt_limit',0x59c28b:'negative_tilt_limit',0x59c311:'body_vector_adjustment',0x59c348:'response_limit',0x59c423:'damping_curve',0x59c4d3:'scalar_rise',0x59c4dd:'scalar_fall',0x59c4f6:'state_damping',0x59c5ad:'upper_limit',0x59c5d2:'lower_limit'}
    machines=[]
    for rebuilt in (False,True):
        uc=Uc(UC_ARCH_X86,UC_MODE_32);uc.mem_map(base,(len(image)+4095)&~4095);uc.mem_write(base,image)
        uc.mem_map(arena,0x10000);uc.mem_map(stop,0x1000);uc.mem_map(linked['address'],linked['allocation'])
        if rebuilt:
            for address,code in linked['blocks']:uc.mem_write(address,code)
        ranges=linked['ranges']+[(CONVERSION[0],CONVERSION[0]+CONVERSION[1])] if rebuilt else [(a,a+n) for a,n,d in originals]+[(0x43c9c0,0x43c9c5)]
        def guard(machine,address,size,user,ranges=ranges,rebuilt=rebuilt):
            assert any(a<=address<b for a,b in ranges),hex(address)
            if not rebuilt and address in sites:
                k=sites[address];visits[k]=visits.get(k,0)+1
        uc.hook_add(UC_HOOK_CODE,guard);machines.append(uc)
    rng=random.Random(0x59c1e0);counts={DERIVATIVE:0,MATRIX:0}
    def compare(symbol,state,args,writable,precision,curves):
        results=[]
        for rebuilt,uc in zip((False,True),machines):
            uc.mem_write(arena,bytes(state))
            for address,record in curves:uc.mem_write(address,record)
            uc.mem_write(stack,struct.pack('<I',stop)+args);uc.reg_write(UC_X86_REG_ESP,stack);uc.reg_write(UC_X86_REG_ECX,body);uc.reg_write(UC_X86_REG_FPCW,precision)
            saved=(UC_X86_REG_EBX,UC_X86_REG_ESI,UC_X86_REG_EDI,UC_X86_REG_EBP)
            for reg in saved:uc.reg_write(reg,0x12345678)
            uc.emu_start(linked['exports'][symbol] if rebuilt else ORIGINAL[symbol][0],stop,count=10000)
            assert uc.reg_read(UC_X86_REG_EIP)==stop
            assert uc.reg_read(UC_X86_REG_ESP)==stack+(8 if symbol==DERIVATIVE else 4)
            assert all(uc.reg_read(reg)==0x12345678 for reg in saved)
            assert uc.reg_read(UC_X86_REG_FPCW)==precision
            if symbol==DERIVATIVE:assert uc.reg_read(UC_X86_REG_EAX)==output
            after=bytes(uc.mem_read(arena,len(state)));masked=bytearray(after)
            for a in writable:masked[a-arena:a-arena+4]=state[a-arena:a-arena+4]
            assert masked==state,('write footprint',symbol)
            results.append(after)
        if results[0]!=results[1]:
            differences=[(hex(a),struct.unpack_from('<f',results[0],a-arena)[0],struct.unpack_from('<f',results[1],a-arena)[0],results[0][a-arena:a-arena+4].hex(),results[1][a-arena:a-arena+4].hex()) for a in writable if results[0][a-arena:a-arena+4]!=results[1][a-arena:a-arena+4]]
            raise AssertionError((symbol,counts[symbol],precision,differences))
        counts[symbol]+=1
    for trial in range(1024):
        state=bytearray(b'\xa5'*0x5000)
        def f(a,v):struct.pack_into('<f',state,a-arena,v)
        def i(a,v):struct.pack_into('<i',state,a-arena,v)
        for j in range(16):f(body+0x30+j*4,float(j%5==0))
        f(body+0x38,(-2,-0.1,0,0.1,2)[trial%5])
        for off in (0x70,0x80,0xa0,0xc0,0x110,0x120):
            for j in range(3):f(body+off+j*4,rng.uniform(-50,50))
        f(body+0xa0,rng.choice((-500,0,500)));f(body+0xa4,rng.choice((-500,0,500)))
        f(body+0x90,(0,0.5,2)[trial%3]);f(body+0x200,(0,1,20)[trial%3]);f(body+0x204,(0,1,10)[trial%3]);f(body+0x208,1)
        i(body+0x150,trial%2);i(body+0x154,(trial//2)%2);i(body+0x15c,(trial//4)%2)
        curves=[]
        for j,address in enumerate((0x7fc92c,0x7fc940)):
            data=samples+j*0x100
            curves.append((address,struct.pack('<ifffI',5,0,1000,0.004,data)))
            for k in range(5):f(data+k*4,rng.uniform(0,1))
        writable=[body+o for o in (0x110,0x114,0x118)]+[output+j*4 for j in ([0,1,2]+list(range(4,20))+[20,21,22,24,25,26,28])]
        for precision in (0x027f,0x037f):compare(DERIVATIVE,state,struct.pack('<I',output),writable,precision,curves)
    for trial in range(512):
        state=bytearray(b'\xa5'*0x5000)
        for j in range(16):struct.pack_into('<f',state,matrix-arena+j*4,rng.uniform(-2,2))
        for j in range(3):struct.pack_into('<f',state,vector-arena+j*4,rng.uniform(-50,50))
        for precision,target in ((p,t) for p in (0x027f,0x037f) for t in (output,matrix,matrix+4,vector)):
            compare(MATRIX,state,struct.pack('<III',target,vector,matrix),[target+j*4 for j in range(16)],precision,[])
    assert set(visits)==set(sites.values()),visits
    report=dict(executable_sha256=hashlib.sha256(raw).hexdigest(),functions=[dict(symbol=s,original_address=hex(a),original_size=n,original_sha256=d,rebuilt_size=linked['functions'][s]['size'],byte_identical_after_relocation=matrix_matching if s==MATRIX else False) for s,(a,n,d) in ORIGINAL.items()],comparisons=counts,branch_visits=visits,stubbed_callees=[],original_runtime_dependency=CONVERSION,checks=['exact whole state and output','untouched output holes','conditional body writes','matrix aliases/partial overlaps','stack/register/control-word/return preservation'],limitations=['Finite synthetic body/curve inputs','Runtime curve initialization not recovered','Integrator composition and body post-update still incomplete'])
    (BUILD/'body-derivative-verification.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report,indent=2))


if __name__=='__main__':main()
