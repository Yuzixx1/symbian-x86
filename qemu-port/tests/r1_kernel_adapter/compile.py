#!/usr/bin/env python3
"""Private compile only: read current kernel flags without invoking make."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shlex
import subprocess

ap=argparse.ArgumentParser()
ap.add_argument('--scratch',type=Path,required=True)
ap.add_argument('--exports',type=Path,required=True)
a=ap.parse_args();a.scratch.mkdir(parents=True,exist_ok=False)
root=Path(__file__).resolve().parents[3];port=root/'qemu-port';eka=root/'kernelhwsrv/kernel/eka'
lines=(port/'Makefile').read_text().splitlines()
values={}
for key in ['KERN_FLAGS','KERN_DEFS','KERN_INCLUDES']:
    text=next(x.split(':=',1)[1].strip() for x in lines if x.startswith(key+' :='))
    values[key]=[x.replace('$(EKA)',str(eka)).replace('$(QPORT)/build/export',str(a.exports)).replace('$(QPORT)',str(port)) for x in shlex.split(text)]
env=os.environ.copy();env['TMPDIR']=str(a.scratch)
results=[]
for name in ['r1_g_objects','r1_g_bootstrap']:
    src=port/'variant'/(name+'.cpp')
    cmd=['g++']+values['KERN_FLAGS']+values['KERN_DEFS']+values['KERN_INCLUDES']+['-c',str(src),'-o',str(a.scratch/(name+'.o'))]
    done=subprocess.run(cmd,capture_output=True,text=True,env=env)
    results.append(dict(argv=cmd,status=done.returncode,stdout=done.stdout,stderr=done.stderr,source_sha256=hashlib.sha256(src.read_bytes()).hexdigest()))
    (a.scratch/'commands.json').write_text(json.dumps(results,indent=2)+'\n')
    print(done.stdout,end='');print(done.stderr,end='')
    if done.returncode:raise SystemExit(done.returncode)
print('PASS private kernel compile-only; no link or execution')
