#!/usr/bin/env python3
"""Read-only target inventory and explicit, separate-file preference migration.
No tool here edits a checkout, invokes a TV, executes source, or deploys a package.
"""
from __future__ import annotations
import argparse
import hashlib
import json
import pathlib
import re
import sys

CONTRACTS = {
    'layout-focus': ('src/ui.rs', 'src/ui.cpp', 'src/core.cpp'),
    'renderer-high-dpi': ('src/main.rs', 'src/render.cpp', 'src/main.cpp'),
    'native-service-boundary': ('src/platform/webos.rs', 'src/services.cpp'),
    'preview-state': ('src/living.rs', 'include/spice/core.hpp', 'src/core.cpp'),
    'preferences': ('src/model.rs', 'settings.example.json'),
    'integrations': ('src/integrations.rs', 'src/services.cpp'),
    'build-identity': ('Cargo.toml', 'CMakeLists.txt', 'appinfo.json'),
    'verification': ('tests/core_tests.cpp', 'tests/native_qa.py', 'tests')
}
APP = re.compile(r'[A-Za-z0-9_.-]{1,160}\Z')
KEY = re.compile(r'[A-Za-z0-9_]{1,40}\.[A-Za-z0-9_]{1,60}\Z')
THEMES = {'spice','metro','xmb','nxe','ps4','ps5'}
ACTIONS = {'left','right','up','down','ok','back','control','settings','library','diagnostics'}


def read_json(path: pathlib.Path) -> dict:
    if path.is_symlink() or not path.is_file():
        raise ValueError('A regular non-symlink JSON file is required')
    if path.stat().st_size > 131072:
        raise ValueError('JSON input exceeds 128 KiB')
    value = json.loads(path.read_text(encoding='utf-8'), parse_constant=lambda s: (_ for _ in ()).throw(ValueError('Nonfinite JSON')))
    if not isinstance(value, dict):
        raise ValueError('Expected an object')
    return value


def private_ipv4(s: object) -> bool:
    if not isinstance(s, str) or not re.fullmatch(r'(?:0|[1-9][0-9]{0,2})(?:\.(?:0|[1-9][0-9]{0,2})){3}', s):
        return False
    a = [int(x) for x in s.split('.')]
    return all(x < 256 for x in a) and (a[0] == 10 or a[:2] == [192,168] or a[0] == 172 and 16 <= a[1] <= 31)


def migrate(source: pathlib.Path, destination: pathlib.Path) -> dict:
    if source.is_symlink():
        raise ValueError('Use the original regular source file, not a symlink')
    source = source.resolve(strict=True)
    # Resolve before writing; never overwrite the original or any other existing file.
    if destination.exists() or destination.is_symlink() or destination.resolve() == source:
        raise ValueError('Destination must be a new separate file')
    old = read_json(source)
    def strings(name, maximum, pattern):
        values = old.get(name, [])
        if not isinstance(values, list):
            return []
        return list(dict.fromkeys(v for v in values[:maximum] if isinstance(v,str) and pattern.fullmatch(v)))
    def integer(name, default, low, high):
        value = old.get(name, default)
        return value if type(value) is int and low <= value <= high else default
    new = {
        'schema': 1,
        'theme': old.get('theme') if isinstance(old.get('theme'),str) and old.get('theme') in THEMES else 'ps5',
        'expanded_icons': old.get('expanded_icons',True) is not False,
        'reduced_motion': old.get('reduced_motion') is True,
        # Migration never carries automatic app launching into a fresh native port.
        'handoff_on_dwell': False,
        'preview_dwell_seconds': integer('preview_dwell_seconds',8,3,30),
        'favorites': strings('favorites',64,APP),
        'pinned_settings': strings('pinned_settings',8,KEY),
        'custom_settings': strings('custom_settings',20,KEY),
        'processor_app_id': old.get('processor_app_id','') if isinstance(old.get('processor_app_id',''),str) and APP.fullmatch(old.get('processor_app_id','')) else '',
        'retropie_host': old.get('retropie_host','') if private_ipv4(old.get('retropie_host')) else '',
        'retropie_port': integer('retropie_port',8767,1024,65535),
        'retropie_hdmi': integer('retropie_hdmi',1,1,4),
        'remote_scancodes': {},
        'preview_profiles': {}
    }
    mapping = old.get('remote_scancodes',{})
    if isinstance(mapping,dict):
        new['remote_scancodes'] = {k:v for k,v in list(mapping.items())[:32] if isinstance(k,str) and 0 < len(k) < 40 and isinstance(v,str) and v in ACTIONS}
    # No arbitrary unknown fields or credentials are copied across runtimes.
    destination.parent.mkdir(parents=True,exist_ok=True)
    with destination.open('x',encoding='utf-8') as file:
        json.dump(new,file,indent=2);file.write('\n')
    return new


def inspect(target: pathlib.Path, output: pathlib.Path) -> dict:
    if target.is_symlink():
        raise ValueError('Use the real target directory, not a symlink')
    target = target.resolve(strict=True)
    output = output.resolve()
    if not target.is_dir() or output == target or target in output.parents:
        raise ValueError('Output must be outside the target checkout')
    if output.exists():
        raise ValueError('Use a new empty report path; reports never overwrite files')
    records = []
    for relative in sorted({p for paths in CONTRACTS.values() for p in paths}):
        path = target / relative
        if path.is_symlink() or not path.exists():
            continue
        resolved = path.resolve()
        if target not in resolved.parents:
            continue
        if path.is_file() and path.stat().st_size <= 1048576:
            records.append({'path': relative, 'bytes': path.stat().st_size,
                            'sha256': hashlib.sha256(path.read_bytes()).hexdigest()})
    rust = (target/'Cargo.toml').is_file()
    cpp = (target/'CMakeLists.txt').is_file()
    report = {'schema':1,'target':str(target),'architecture': 'mixed' if rust and cpp else 'rust' if rust else 'cmake-cpp' if cpp else 'unknown',
              'classification':'Static inventory only; not compilation, feature parity, or hardware proof',
              'target_modified':False,'files':records,
              'contracts': {name: [p for p in paths if any(x['path']==p for x in records)] for name,paths in CONTRACTS.items()}}
    output.mkdir(parents=True,exist_ok=False)
    (output/'inventory.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
    text = '''# C++ adaptation review\n\nThis report is file-presence evidence, not feature verification. The target was not modified.\n\n## Integration order\n\n1. Read the target's own AGENTS.md and existing changes before editing.\n2. Keep the deployed app and its identity unchanged. C++ remains com.spice.home.cpp.\n3. Build/test the pure C++ core, then native SDL rendering. Preserve target-specific build fixes.\n4. Compare layout, focus, state serialization and service boundaries against the target, one contract at a time.\n5. Prove ARM ABI/runtime dependencies, Luna permissions and input scancodes on the device.\n6. Keep inline TV video, 120 Hz presentation and full theme parity marked unverified.\n\n## Reference\n\nC++ entry: src/main.cpp. Renderer: src/render.cpp. State/navigation: src/core.cpp and src/ui.cpp.\nBounded native services: src/services.cpp. Test gates: tests/core_tests.cpp and tests/native_qa.py.\n\nDo not copy entire source folders over a working checkout. Do not infer private API grants from manifest declarations.\nDo not install a Home hook, execute elevated shell commands, or auto-update Homebrew apps.\n'''
    (output/'ADAPTATION-PLAN.md').write_text(text,encoding='utf-8')
    return report


def main() -> int:
    p=argparse.ArgumentParser(description=__doc__)
    sub=p.add_subparsers(dest='command',required=True)
    a=sub.add_parser('inspect');a.add_argument('--target',type=pathlib.Path,required=True);a.add_argument('--out',type=pathlib.Path,required=True)
    a=sub.add_parser('migrate');a.add_argument('--from',dest='source',type=pathlib.Path,required=True);a.add_argument('--to',dest='destination',type=pathlib.Path,required=True)
    args=p.parse_args()
    try:
        if args.command=='inspect':
            r=inspect(args.target,args.out);print(f"Read-only report written: {args.out} ({r['architecture']})")
        else:
            migrate(args.source,args.destination);print(f'New settings written: {args.destination}. Handoff remains disabled; credentials not copied.')
        return 0
    except (ValueError,OSError,json.JSONDecodeError) as exc:
        print(f'STOPPED: {exc}',file=sys.stderr);return 2

if __name__=='__main__':
    raise SystemExit(main())
