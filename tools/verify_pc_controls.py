#!/usr/bin/env python3
"""Bounded comparison of PC keyboard binding React/SetLabels methods.

Device capture, label formatting and virtual label refresh are controlled interfaces.
Does not verify the complete controller screen, DirectInput or profile persistence.
"""
import hashlib
import json
import struct
import subprocess
from pathlib import Path
import pefile
from unicorn import Uc, UC_ARCH_X86, UC_MODE_32, UC_HOOK_CODE
from unicorn.x86_const import *
from pc_coff import read_object, link_objects
from probe_pc_race_reward import ROOT, EXE


def main():
    manifest = json.loads((ROOT / 'config/SPEED2_EXE_1_2/pc-controls.json').read_text())
    raw = EXE.read_bytes()
    assert hashlib.sha256(raw).hexdigest() == manifest['exe_sha256']
    image = pefile.PE(data=raw).get_memory_mapped_image()
    out = ROOT / 'build/analysis/controls'; out.mkdir(parents=True, exist_ok=True)
    obj = out / 'PCControls.obj'
    subprocess.run([str(ROOT / 'build/tools/wibo'), 'build/compilers/Win32/7.1/cl.exe', '/nologo', '/c', '/Ox', '/Ob0',
                    '/Z7', '/EHsc', '/Fo' + str(obj.relative_to(ROOT)), 'src/Speed/Indep/Src/Frontend/PCControls.cpp'],
                   cwd=ROOT, check=True)
    symbols = list(read_object(obj)['symbols'].values())
    externals = {}
    for s in symbols:
        if s['section'] == 0 and s['storage'] == 2:
            n = s['name']
            if n.startswith('?controlDevice874C40@@'): externals[n] = 0x874c40
            elif n.startswith('?BeginBinding@ControlDevice874C40@@'): externals[n] = 0x5c2100
            elif n.startswith('?SetPCControlBindingLabel@@'): externals[n] = 0x4acf40
            else: raise AssertionError(n)
    linked = link_objects([obj], externals)
    arena, stack, stop = 0x2000000, 0x200f000, 0x2100000
    this, vtable, refresh = arena + 0x100, arena + 0x200, stop + 0x100
    counts = {}
    for f in manifest['functions']:
        start, size = f['address'], f['size']
        assert hashlib.sha256(image[start-0x400000:start-0x400000+size]).hexdigest() == f['sha256']
        prefix = '?' + f['method'] + '@' + f['class_name'] + '@@'
        rebuilt = next(a for n,a in linked['exports'].items() if n.startswith(prefix))
        count = 0
        for label in [0, arena+0x800, 0x11110000, 0xffffffff]:
            for message in ([0,0x406415e3,0x911ab364,0xffffffff] if f['method']=='React' else [0]):
                results = []
                for entry in [start, rebuilt]:
                    uc = Uc(UC_ARCH_X86, UC_MODE_32)
                    uc.mem_map(0x400000, (len(image)+4095)&~4095);uc.mem_write(0x400000,image)
                    uc.mem_map(arena,0x10000);uc.mem_map(stop,0x1000);uc.mem_write(stop,b'\x90'*0x200)
                    uc.mem_map(linked['address'],linked['allocation'])
                    for a,b in linked['blocks']: uc.mem_write(a,b)
                    uc.mem_write(this,bytes(range(0x38)))
                    uc.mem_write(this,struct.pack('<I',vtable));uc.mem_write(this+0x2c,struct.pack('<I',label))
                    uc.mem_write(vtable+12,struct.pack('<I',refresh))
                    args=[arena+0x400,message] if f['method']=='React' else []
                    uc.mem_write(stack,struct.pack('<I',stop)+b''.join(struct.pack('<I',v) for v in args))
                    uc.reg_write(UC_X86_REG_ESP,stack);uc.reg_write(UC_X86_REG_ECX,this)
                    for reg in [UC_X86_REG_EBX,UC_X86_REG_ESI,UC_X86_REG_EDI,UC_X86_REG_EBP]:uc.reg_write(reg,0x12345678)
                    trace=[]
                    def hook(machine,a,n,data):
                        sp=machine.reg_read(UC_X86_REG_ESP)
                        if a in [0x5c2100,0x4acf40,refresh]:
                            argc=2 if a==0x5c2100 else 4 if a==0x4acf40 else 0
                            vals=struct.unpack('<'+'I'*argc,machine.mem_read(sp+4,argc*4)) if argc else ()
                            receiver=machine.reg_read(UC_X86_REG_ECX) if a!=0x4acf40 else None
                            trace.append((a,receiver,vals))
                            machine.reg_write(UC_X86_REG_EAX,0xaabbccdd)
                            machine.reg_write(UC_X86_REG_ECX,0xeeeeeeee);machine.reg_write(UC_X86_REG_EDX,0xffffffff)
                            machine.reg_write(UC_X86_REG_EIP,struct.unpack('<I',machine.mem_read(sp,4))[0])
                            machine.reg_write(UC_X86_REG_ESP,sp+4+(argc*4 if a==0x5c2100 else 0))
                        else:
                            allowed = start<=a<start+size if entry==start else linked['address']<=a<linked['address']+linked['allocation']
                            assert allowed, (f,a)
                    uc.hook_add(UC_HOOK_CODE,hook);uc.emu_start(entry,stop,count=1000)
                    assert uc.reg_read(UC_X86_REG_EIP)==stop
                    assert uc.reg_read(UC_X86_REG_ESP)==stack+4+len(args)*4
                    assert all(uc.reg_read(r)==0x12345678 for r in [UC_X86_REG_EBX,UC_X86_REG_ESI,UC_X86_REG_EDI,UC_X86_REG_EBP])
                    results.append((trace,bytes(uc.mem_read(this,0x38))))
                assert results[0]==results[1],(f,results)
                count+=1
        counts[f['class_name']+'::'+f['method']]=count
    report=dict(exe_sha256=manifest['exe_sha256'],routine_count=len(counts),comparisons=sum(counts.values()),cases=counts,
                byte_matching=False, limitations=['Capture/label interfaces controlled; virtual label refresh hooked',
                'Full screen, device lists, DirectInput and profile persistence not verified'])
    (out/'verification.json').write_text(json.dumps(report,indent=2)+'\n')
    print('PASS',report['comparisons'],'comparisons over',len(counts),'PC keyboard binding methods')

if __name__ == '__main__': main()
