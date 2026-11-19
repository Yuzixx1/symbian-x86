#!/usr/bin/env python3
"""Actual C++ core tests; explicit immutable fixture/scratch paths."""
import argparse
import hashlib
import importlib.util
import json
import os
from pathlib import Path
import struct
import subprocess


def main():
    ap=argparse.ArgumentParser()
    ap.add_argument('--v1',type=Path,required=True)
    ap.add_argument('--fixture',type=Path,required=True)
    ap.add_argument('--scratch',type=Path,required=True)
    a=ap.parse_args();a.scratch.mkdir(parents=True,exist_ok=False)
    here=Path(__file__).resolve().parent;port=here.parents[1];module=port/'loader'
    spec=importlib.util.spec_from_file_location('vectors',port/'tests/e32g_format/run.py')
    v=importlib.util.module_from_spec(spec);spec.loader.exec_module(v)
    manifest=json.loads((port/'tests/e32g_format/v1_manifest.json').read_text())
    before={}
    for name,record in manifest.items():
        digest=hashlib.sha256((a.v1/name).read_bytes()).hexdigest();assert digest==record['sha256'];before[str(a.v1/name)]=digest
    digest=hashlib.sha256(a.fixture.read_bytes()).hexdigest()
    assert digest=='87fde53b240e54ac9aecf4024546b77c91f06a57d83f60c0101c704e0ed6a6a0'
    before[str(a.fixture)]=digest
    # Independently constructed synthetic layout from format tests (not pilot).
    root=v.synthetic();(a.scratch/'root.e32').write_bytes(root)
    dll=v.synthetic();v.put(dll,0,0x10000079);v.put(dll,4,0x1000008d);v.put(dll,8,0x100039e5)
    v.put(dll,44,v.u32(dll,44)|1);v.put(dll,84,0);v.put(dll,108,0)
    v.put(dll,12,v.checked_uid(dll));v.seal(dll);(a.scratch/'dll.e32').write_bytes(dll)
    cycle=v.synthetic();v.put(cycle,0,0x10000079);v.put(cycle,4,0x1000008d);v.put(cycle,8,0x100039e5)
    v.put(cycle,44,v.u32(cycle,44)|1)
    # Synthetic DLL export ordinal required by synthetic IAT is 1, not 85.
    v.put(root,v.u32(root,100)+v.u32(root,96),1);(a.scratch/'root.e32').write_bytes(root)
    v.put(cycle,v.u32(cycle,100)+v.u32(cycle,96),1)
    v.put(cycle,12,v.checked_uid(cycle));v.seal(cycle);(a.scratch/'cycle.e32').write_bytes(cycle)
    data=bytearray(root);co=v.u32(data,100);ro=v.u32(data,112)
    # Add type 2000 code-site relocation; block was padded to four bytes.
    struct.pack_into('<H',data,ro+18,0x2024);v.put(data,ro+4,2)
    v.put(data,co+36,0x700004);v.put(data,80,0x700000)
    dataoff=len(data);data+=struct.pack('<II',0x400020,0x12345678)
    v.put(data,104,dataoff);v.put(data,52,8);v.put(data,68,16)
    dro=len(data);data+=struct.pack('<IIIIHH',12,1,0,12,0x1000,0)
    v.put(data,116,dro);v.seal(data);(a.scratch/'data.e32').write_bytes(data)
    general=bytearray(data);struct.pack_into('<H',general,ro+18,0x3024)
    (a.scratch/'general_data.e32').write_bytes(general)
    unaligned=bytearray(root);v.put(unaligned,co+33,0x400020)
    struct.pack_into('<H',unaligned,ro+16,0x1021)
    (a.scratch/'unaligned.e32').write_bytes(unaligned)
    # Diamond: EXE -> second.dll -> euser.dll, EXE -> euser.dll.
    second=bytearray(cycle);v.put(second,8,0x11112222);v.put(second,12,v.checked_uid(second));v.seal(second)
    (a.scratch/'second.e32').write_bytes(second)
    diamond=bytearray(root[:co+128]);names=[b'second{000a0000}[11112222].dll',b'euser{000a0000}[100039e5].dll']
    imp=bytearray(struct.pack('<IIIII',0,20,1,20+len(names[0])+1,1)+names[0]+b'\0'+names[1]+b'\0')
    while len(imp)%4:imp.append(0)
    v.put(imp,0,len(imp));v.put(diamond,co+68,1);v.put(diamond,84,2)
    v.put(diamond,112,len(diamond)+len(imp));diamond+=imp+root[ro:];v.seal(diamond)
    (a.scratch/'diamond.e32').write_bytes(diamond)
    env=os.environ.copy();env['TMPDIR']=str(a.scratch)
    commands=[]
    def run(cmd):
        done=subprocess.run(cmd,capture_output=True,text=True,env=env)
        commands.append(dict(argv=cmd,status=done.returncode,stdout=done.stdout,stderr=done.stderr))
        (a.scratch/'commands.json').write_text(json.dumps(commands,indent=2)+'\n')
        print(done.stdout,end='');print(done.stderr,end='')
        assert done.returncode==0,cmd
    common=['g++','-std=c++98','-pedantic','-Wall','-Wextra','-Werror','-fno-exceptions','-fno-rtti','-I'+str(module)]
    for name in ['e32g_plan','e32g_load']:
        run(common+['-ffreestanding','-c',str(module/(name+'.cpp')),'-o',str(a.scratch/(name+'.o'))])
    # Format worker independently owns C++98 conversion. Compile current format
    # with its existing modern mode if needed, never patch another card's source.
    run(['g++','-std=c++11','-Wall','-Wextra','-Werror','-fno-exceptions','-fno-rtti','-c',str(module/'e32g_format.cpp'),'-o',str(a.scratch/'format.o')])
    run(common+[str(here/'core_test.cpp'),str(a.scratch/'e32g_plan.o'),str(a.scratch/'e32g_load.o'),str(a.scratch/'format.o'),'-o',str(a.scratch/'core_test')])
    run([str(a.scratch/'core_test'),str(a.v1),str(a.scratch),str(a.fixture)])
    for path,h in before.items():assert hashlib.sha256(Path(path).read_bytes()).hexdigest()==h
    (a.scratch/'inputs.json').write_text(json.dumps(before,indent=2)+'\n')
    print('PASS immutable fixture hashes preserved; strict C++98 shared core compiled')


if __name__=='__main__':main()
