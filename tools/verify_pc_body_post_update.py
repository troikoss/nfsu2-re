#!/usr/bin/env python3
"""Finite original/rebuilt body post-update comparisons, with real math callees."""
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
from verify_pc_wheel_math import ORIGINAL as WHEEL_ORIGINAL

BUILD=ROOT/'build/analysis/racing'
POST='?PostUpdate@MoverBody@@QAEXXZ'
FORCE='?ApplyForceAtPoint@MoverBody@@QAEXPAUWheelVector@@0@Z'
COPY='?BodyMatrixCopy@@YAXPAUWheelMatrix@@0@Z'
PRODUCT='?BodyMatrixProduct@@YAXPAUWheelMatrix@@00@Z'
MULTIPLY='?WheelMultiply@@YAXPAUWheelMatrix@@00@Z'
ORIGINAL={POST:(0x59c5f0,903,'ed626e273b5656a956983ae688a8e92bb00e6d82b2646b1b083b373f7786712e'),
 FORCE:(0x59bb00,274,'c1944cdd2028b5fd927fe5d4c3eab388c7b45ae4859a3d4d4ccb76f5dd4e4de7'),
 COPY:(0x4012b0,139,'fb247fd091bb1c0716c7469fa30d5f1c0bb26f8b0b94ef93dbdb1677b137497c'),
 PRODUCT:(0x4013f0,572,'a5dc685803952957b6b3155260155228b840d6b71364ff7e6b4a0867cf4450c5'),
 MULTIPLY:(0x43c990,24,'4ba425a1a1e3e279c94cfbbcba083ef27a6c8b3918d577cbb59c53a9f7d645fb')}
GLOBALS={'?bodyPostSpeedLimit@@3MA':0x7fc6ec,'?bodyPostMassThreshold@@3NA':0x7a1e88,
 '?bodyPostResponseScale@@3MA':0x7a1e84,'?bodyPostAngularLimit@@3MA':0x7a1b1c,
 '?bodyPointForceLimit@@3MA':0x7a1ad8,'?bodyPointTorqueLimit@@3MA':0x7a1adc}


def main():
 raw=EXE.read_bytes();assert hashlib.sha256(raw).hexdigest()=='f9dd86c054878ce6276beb07c1fd61874f7a1e4bf1f241b084c65b73e24168a7'
 pe=pefile.PE(data=raw);image,base=pe.get_memory_mapped_image(),pe.OPTIONAL_HEADER.ImageBase
 originals=list(ORIGINAL.values())+list(WHEEL_ORIGINAL.values())
 for a,n,d in originals:assert hashlib.sha256(image[a-base:a-base+n]).hexdigest()==d
 paths=[]
 for unit in ('BodyPostUpdate','BodyPointForce','BodyMatrixProduct','WheelMath'):
  obj=BUILD/(unit+'.obj');subprocess.run([str(ROOT/'build/tools/wibo'),'build/compilers/Win32/7.1/cl.exe','/nologo','/c','/Ox','/Ob0','/Z7','/EHsc','/Fo'+str(obj.relative_to(ROOT)),'src/Speed/Indep/Src/Physics/'+unit+'.cpp'],cwd=ROOT,check=True);paths.append(obj)
 linked=link_objects(paths,GLOBALS)
 arena,body,first,second,out,stack,stop=0x2000000,0x2000000,0x2001000,0x2002000,0x2003000,0x200f000,0x2100000
 sites={0x59c680:'speed_limit',0x59c732:'matrix170',0x59c738:'matrix_selected',0x59c7a9:'angular_limit',0x59c7e2:'negative_scalar',0x59c925:'gravity',0x59bb48:'force_cap',0x59bbcd:'torque_cap'}
 visits={};machines=[]
 for rebuilt in (False,True):
  uc=Uc(UC_ARCH_X86,UC_MODE_32);uc.mem_map(base,(len(image)+4095)&~4095);uc.mem_write(base,image)
  uc.mem_map(arena,0x10000);uc.mem_map(stop,0x1000);uc.mem_map(linked['address'],linked['allocation'])
  if rebuilt:
   for address,code in linked['blocks']:uc.mem_write(address,code)
  ranges=linked['ranges'] if rebuilt else [(a,a+n) for a,n,d in originals]+[(0x43c9c0,0x43c9c5)]
  def guard(machine,address,size,user,ranges=ranges,rebuilt=rebuilt):
   assert any(a<=address<b for a,b in ranges),hex(address)
   if not rebuilt and address in sites:
    name=sites[address];visits[name]=visits.get(name,0)+1
  uc.hook_add(UC_HOOK_CODE,guard);machines.append(uc)
 rng=random.Random(0x59c5f0);counts={s:0 for s in ORIGINAL}
 def compare(symbol,state,args,writable,precision):
  results=[]
  for rebuilt,uc in zip((False,True),machines):
   uc.mem_write(arena,bytes(state));uc.mem_write(stack,struct.pack('<I',stop)+args)
   uc.reg_write(UC_X86_REG_ESP,stack);uc.reg_write(UC_X86_REG_ECX,body);uc.reg_write(UC_X86_REG_FPCW,precision)
   saved=(UC_X86_REG_EBX,UC_X86_REG_ESI,UC_X86_REG_EDI,UC_X86_REG_EBP)
   for reg in saved:uc.reg_write(reg,0x12345678)
   uc.emu_start(linked['exports'][symbol] if rebuilt else ORIGINAL[symbol][0],stop,count=10000)
   assert uc.reg_read(UC_X86_REG_EIP)==stop
   assert uc.reg_read(UC_X86_REG_ESP)==stack+(12 if symbol==FORCE else 4)
   assert all(uc.reg_read(reg)==0x12345678 for reg in saved)
   assert uc.reg_read(UC_X86_REG_FPCW)==precision
   after=bytes(uc.mem_read(arena,len(state)));masked=bytearray(after)
   for a in writable:masked[a-arena:a-arena+4]=state[a-arena:a-arena+4]
   assert masked==state,('write footprint',symbol)
   results.append(after)
  if results[0]!=results[1]:
   diff=[(hex(a),struct.unpack_from('<f',results[0],a-arena)[0],struct.unpack_from('<f',results[1],a-arena)[0]) for a in writable if results[0][a-arena:a-arena+4]!=results[1][a-arena:a-arena+4]]
   raise AssertionError((symbol,counts[symbol],hex(precision),diff))
  counts[symbol]+=1
 def f(state,a,v):struct.pack_into('<f',state,a-arena,v)
 def v(state,a,values):
  for j,value in enumerate(values):f(state,a+j*4,value)
 def value(a,fmt='f'):return struct.unpack_from('<'+fmt,image,a-base)[0]
 def neighbors(x):
  b=struct.unpack('<I',struct.pack('<f',x))[0]
  return [struct.unpack('<f',struct.pack('<I',b+d))[0] for d in (-1,0,1)]
 speed=value(0x7fc6ec);angular=value(0x7a1b1c);mass=value(0x7a1e88,'d');force_limit=value(0x7a1ad8);torque_limit=value(0x7a1adc)
 for trial in range(1024):
  state=bytearray(b'\xa5'*0x5000)
  for off in (0x30,0x170,0x1b0):
   for j in range(16):f(state,body+off+j*4,rng.uniform(-2,2))
  for off in (0x20,0x70,0x80,0x110,0x120):v(state,body+off,[rng.uniform(-200,200) for j in range(3)])
  if trial%4==0:v(state,body+0x70,(neighbors(speed)[trial%3],0,0))
  f(state,body+0x164,neighbors(mass)[trial%3] if trial%2 else 1500)
  f(state,body+0x168,1 if trial%4==0 else rng.choice((0,0.001,0.5,1)))
  f(state,body+0x90,rng.choice((-1,-0.0,0,1)))
  f(state,body+0x160,rng.choice((-1,0,9.81,force_limit)))
  struct.pack_into('<i',state,0x158,trial%2)
  writable=[body+o for o in list(range(0x70,0x7c,4))+[0x90]+list(range(0xa0,0xac,4))+list(range(0xb0,0xbc,4))+list(range(0xc0,0xcc,4))+list(range(0xd0,0x110,4))+list(range(0x110,0x11c,4))+list(range(0x120,0x12c,4))+list(range(0x130,0x13c,4))+list(range(0x140,0x14c,4))]
  for precision in (0x027f,0x037f):compare(POST,state,b'',writable,precision)
 for trial in range(512):
  state=bytearray(b'\xa5'*0x5000)
  v(state,body+0x110,[rng.uniform(-10,10) for j in range(3)]);v(state,body+0x120,[rng.uniform(-10,10) for j in range(3)])
  scale=rng.choice((0,1,force_limit,force_limit*2))
  v(state,first,[rng.uniform(-scale,scale) for j in range(3)]);v(state,second,[rng.uniform(-100,100) for j in range(3)])
  if trial%4==0:v(state,first,(neighbors(force_limit)[trial%3],0,0));v(state,second,(0,1,0))
  targets=((first,second),(body+0x110,second),(first,body+0x110),(body+0x114,body+0x120))
  for precision in (0x027f,0x037f):
   for force,point in targets:compare(FORCE,state,struct.pack('<II',force,point),[body+o for o in (0x110,0x114,0x118,0x120,0x124,0x128)],precision)
 for trial in range(256):
  state=bytearray(b'\xa5'*0x5000)
  for j in range(16):f(state,first+j*4,rng.uniform(-4,4));f(state,second+j*4,rng.uniform(-4,4))
  for precision in (0x027f,0x037f):
   for target in (out,first,first+4,first-4,second,second+8):
    writable=[target+j*4 for j in range(16)]
    compare(COPY,state,struct.pack('<II',target,first),writable,precision)
    for symbol in (PRODUCT,MULTIPLY):compare(symbol,state,struct.pack('<III',target,first,second),writable,precision)
 assert set(visits)==set(sites.values()),visits
 report=dict(executable_sha256=hashlib.sha256(raw).hexdigest(),functions=[dict(symbol=s,original_address=hex(a),original_size=n,original_sha256=d,rebuilt_size=linked['functions'][s]['size'],byte_matching_status='not established') for s,(a,n,d) in ORIGINAL.items()],comparisons=counts,branch_visits=visits,stubbed_callees=[],original_runtime_dependencies=[],checks=['exact whole state','write footprint','force/point accumulator aliases','matrix partial overlaps','finite limit neighbors','stack/register/control-word preservation'],limitations=['Synthetic finite body and matrix inputs','Retail body initialization not recovered','Full integrator lifecycle and native playability still unverified'])
 (BUILD/'body-post-update-verification.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report,indent=2))

if __name__=='__main__':main()
