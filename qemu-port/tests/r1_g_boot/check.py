#!/usr/bin/env python3
"""Host staging/source checks only; never QEMU."""
import argparse
import importlib.util
from pathlib import Path
import shutil

p=argparse.ArgumentParser();p.add_argument('--assets',type=Path,required=True);p.add_argument('--scratch',type=Path,required=True);a=p.parse_args();a.scratch.mkdir(parents=True,exist_ok=False)
port=Path(__file__).resolve().parents[2];spec=importlib.util.spec_from_file_location('stage',port/'tools/stage_r1_g.py');s=importlib.util.module_from_spec(spec);spec.loader.exec_module(s)
s.verify(a.assets)
for label,transform in [('byte',lambda b:b.replace('0x7a','0x7b',1)),('code',lambda b:b+'\nint evil;\n')]:
    out=a.scratch/label;shutil.copytree(a.assets,out);f=out/'r1_g_assets.inc';f.write_text(transform(f.read_text()))
    try:s.verify(out)
    except ValueError:pass
    else:raise AssertionError('tamper accepted '+label)
launch=(port/'variant/r1_g_launch.cpp').read_text();legacy=(port/'variant/e32load.cpp').read_text()
assert 'ExecRecRemember' not in launch.replace('// segments in legacy ExecRecRemember','') or 'ExecRecRemember(' not in launch
assert 'CheckedClose' not in launch and '->Release(' not in launch
assert 'E32EnsureUserTss();' in launch and 'TssInitOnce' not in legacy
assert 'return Observe(slot)' in launch and 'TemplateCheck(image,*info)' in launch
assert 'result.process=0;result.handle=0' in launch
stable = (port/'tools/test_stable.py').read_text()
assert 'R1_G_ASSET_DIR' in stable
assert 'SKIP test_r1_g_boot' in stable
print('PASS pinned staging, byte/source tamper rejection, separate ownership/source checks; no guest run')
