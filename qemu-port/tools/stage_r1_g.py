#!/usr/bin/env python3
"""Local research staging only. Never distributes images or accepts unpinned bytes."""
import argparse
import hashlib
import json
from pathlib import Path

PINS={
 'r1a_fixture.exe':'87fde53b240e54ac9aecf4024546b77c91f06a57d83f60c0101c704e0ed6a6a0',
 'euser.dll':'4851fec4b6439bf5263e6f10f7f3010828f84df55df050c710f57077f2dd017b',
 'efsrv.dll':'332d97274a151bb17a18112e728684b98c8462fe8723bfadb1de0d471e372b6d',
 'HAL.DLL':'4a95dbe01d201b8a0b347e0e5c4cc39f798a7d89d6e163d7f63d71d3a04cb150',
 'EKDATA.DLL':'2e11e7287325a7829a28b7cb510c5605390e57b1953a0b16704b8210c4aced0f',
 'EKDATA.00':'2e11e7287325a7829a28b7cb510c5605390e57b1953a0b16704b8210c4aced0f'}

CONSOLE_STAGE='/home/yuzi/.cache/sybian-r1/r1-console-staging-v1/sys/bin'

def stage_source(n):
    # Accepted card-24/26 console assets come from the verified staging dir;
    # fixture/EUSER/EFSRV bytes come from v1/fixture.
    if n in ('HAL.DLL','EKDATA.DLL','EKDATA.00'):
        return Path(CONSOLE_STAGE)/n
    return None

def stage(v1,fixture,out):
    data={}
    for n,h in PINS.items():
        src=stage_source(n)
        b=(src if src is not None else (fixture if n=='r1a_fixture.exe' else v1/n)).read_bytes()
        if hashlib.sha256(b).hexdigest()!=h:raise ValueError('hash mismatch: '+n)
        data[n]=b
    for n,b in data.items():
        if hashlib.sha256(b).hexdigest()!=PINS[n]:raise ValueError('hash mismatch: '+n)
    # Validate everything before creating output; fresh-only, no overwrite.
    out.mkdir(parents=True,exist_ok=False)
    lines=['// LOCAL RESEARCH ONLY: generated hash-verified immutable images.',
           '// Do not redistribute without independent licensing clearance.']
    for i,(n,b) in enumerate(data.items()):
        lines.append('static const TUint8 gBytes%d[] __attribute__((aligned(4))) = {'%i)
        lines += [','.join('0x%02x'%x for x in b[k:k+24])+',' for k in range(0,len(b),24)]
        lines.append('};')
        name='C:\\sys\\bin\\'+n
        lines.append('static const uint16_t gName%d[] = {%s};'%(i,','.join(str(ord(c)) for c in name)))
    lines.append('static const R1Kernel::ManifestEntry gManifest[] = {')
    for i,n in enumerate(data):
        lines.append('{{gName%d,sizeof(gName%d)/2},{gBytes%d,sizeof(gBytes%d)}},'%(i,i,i,i))
    lines.append('};')
    (out/'r1_g_assets.inc').write_text('\n'.join(lines)+'\n')
    (out/'provenance.json').write_text(json.dumps(dict(hashes=PINS,v1=str(v1.resolve()),fixture=str(fixture.resolve()),console_stage=CONSOLE_STAGE,local_research_only=True),indent=2)+'\n')

def verify(out):
    import re
    text=(out/'r1_g_assets.inc').read_text()
    for i,n in enumerate(PINS):
        match=re.search(r'gBytes%d\[\].*?= \{(.*?)\};'%i,text,re.S)
        if not match:raise ValueError('missing array: '+n)
        b=bytes(int(x,16) for x in re.findall(r'0x([0-9a-f]{2})',match.group(1)))
        if hashlib.sha256(b).hexdigest()!=PINS[n]:raise ValueError('staged hash mismatch: '+n)
        expected=','.join(str(ord(c)) for c in 'C:\\sys\\bin\\'+n)
        if 'gName%d[] = {%s};'%(i,expected) not in text:raise ValueError('manifest name mismatch')
    # Generated source is executable build input: reject ANY extra/edited text by
    # regenerating canonical text from decoded pinned bytes, without disk writes.
    lines=['// LOCAL RESEARCH ONLY: generated hash-verified immutable images.',
           '// Do not redistribute without independent licensing clearance.']
    for i,n in enumerate(PINS):
        match=re.search(r'gBytes%d\[\].*?= \{(.*?)\};'%i,text,re.S)
        b=bytes(int(x,16) for x in re.findall(r'0x([0-9a-f]{2})',match.group(1)))
        lines.append('static const TUint8 gBytes%d[] __attribute__((aligned(4))) = {'%i)
        lines += [','.join('0x%02x'%x for x in b[k:k+24])+',' for k in range(0,len(b),24)]
        lines += ['};','static const uint16_t gName%d[] = {%s};'%(i,','.join(str(ord(c)) for c in 'C:\\sys\\bin\\'+n))]
    lines.append('static const R1Kernel::ManifestEntry gManifest[] = {')
    for i in range(len(PINS)):
        lines.append('{{gName%d,sizeof(gName%d)/2},{gBytes%d,sizeof(gBytes%d)}},'%(i,i,i,i))
    lines.append('};')
    if text!='\n'.join(lines)+'\n':raise ValueError('noncanonical staged source')

if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('--v1',type=Path);p.add_argument('--fixture',type=Path);p.add_argument('--out',type=Path,required=True);p.add_argument('--verify',action='store_true')
    a=p.parse_args()
    if a.verify:verify(a.out)
    elif a.v1 and a.fixture:stage(a.v1,a.fixture,a.out)
    else:p.error('staging requires --v1 and --fixture')
