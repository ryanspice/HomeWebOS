#!/usr/bin/env python3
"""Deterministic archive packaging. No build, install, network access or font redistribution."""
from __future__ import annotations
import argparse, gzip, hashlib, io, pathlib, stat, tarfile, zipfile

ROOT=pathlib.Path(__file__).resolve().parents[1]
IGNORE={'.git','build','dist','__pycache__'}
FONT_SUFFIXES={'.ttf','.otf','.ttc','.woff','.woff2','.pfa','.pfb'}

def package(out: pathlib.Path):
    out=out.resolve()
    if out==ROOT or ROOT in out.parents:
        raise ValueError('Archive destination must be outside the source tree')
    out.mkdir(parents=True,exist_ok=True)
    files=[]
    for p in sorted(ROOT.rglob('*')):
        relative=p.relative_to(ROOT)
        if any(x in IGNORE for x in relative.parts) or p.suffix=='.pyc' or relative.as_posix()=='SHA256SUMS.txt':
            continue
        if p.is_symlink():raise ValueError(f'Symlink needs explicit review: {relative}')
        if not p.is_file():continue
        if p.suffix.lower() in FONT_SUFFIXES:raise ValueError(f'Font redistribution prohibited: {relative}')
        files.append(p)
    manifest=''.join(f'{hashlib.sha256(p.read_bytes()).hexdigest()}  {p.relative_to(ROOT).as_posix()}\n' for p in files)
    (ROOT/'SHA256SUMS.txt').write_text(manifest,encoding='utf8')
    files.append(ROOT/'SHA256SUMS.txt');files.sort()
    base='spice-home-cpp-v0.1.0'
    zpath=out/(base+'.zip');tpath=out/(base+'.tar.gz')
    with zipfile.ZipFile(zpath,'w',compression=zipfile.ZIP_DEFLATED,compresslevel=9) as z:
        for p in files:
            name='spice-home-cpp/'+p.relative_to(ROOT).as_posix()
            info=zipfile.ZipInfo(name,date_time=(2026,11,4,0,0,0));info.create_system=3
            mode=0o755 if p.stat().st_mode&stat.S_IXUSR else 0o644
            info.external_attr=(stat.S_IFREG|mode)<<16;info.compress_type=zipfile.ZIP_DEFLATED
            z.writestr(info,p.read_bytes(),compresslevel=9)
    with tpath.open('wb') as raw, gzip.GzipFile(filename='',mode='wb',fileobj=raw,mtime=0,compresslevel=9) as gz, tarfile.open(fileobj=gz,mode='w') as tar:
        for p in files:
            data=p.read_bytes();info=tarfile.TarInfo('spice-home-cpp/'+p.relative_to(ROOT).as_posix())
            info.size=len(data);info.mode=0o755 if p.stat().st_mode&stat.S_IXUSR else 0o644
            info.mtime=0;info.uid=info.gid=0;info.uname=info.gname=''
            tar.addfile(info,io.BytesIO(data))
    with zipfile.ZipFile(zpath) as z:
        if z.testzip() is not None:raise RuntimeError('ZIP integrity failure')
    with tarfile.open(tpath,'r:gz') as t:
        members=t.getmembers()
        if len(members)!=len(files):raise RuntimeError('TAR member count mismatch')
    sums=''.join(f'{hashlib.sha256(p.read_bytes()).hexdigest()}  {p.name}\n' for p in [zpath,tpath])
    (out/(base+'-SHA256SUMS.txt')).write_text(sums,encoding='utf8')
    print(f'Packaged {len(files)} files; ZIP/TAR integrity passed; no fonts.')
    print(sums,end='')

if __name__=='__main__':
    parser=argparse.ArgumentParser();parser.add_argument('--out',type=pathlib.Path,required=True)
    package(parser.parse_args().out)
