#!/usr/bin/env python3
"""Actual native local-theme startup/mapping/fallback checks, without TV or web calls."""
import argparse
import copy
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools'))
import prepare_tv_data as tv


def main():
    parser = argparse.ArgumentParser()
    for key in ('binary', 'assets', 'bundle', 'fixture', 'out'):
        parser.add_argument('--' + key, type=Path, required=True)
    args = parser.parse_args()
    args.out.mkdir(parents=True, exist_ok=False)
    env = dict(os.environ, SPICE_CPP_PRIVATE_LUNA='0', SPICE_RETROPIE_TOKEN='')
    results = []
    # The producer's prepared fixture is validated by the actual C++ startup
    # loader on every run. Reading expected metadata needs no Pillow in WSL;
    # the real preparation/default/previous helper has separate Windows tests.
    pointer = json.loads((args.fixture / 'current.json').read_text())
    manifest_bytes = (args.fixture / pointer['selected']['manifest']).read_bytes()
    assert hashlib.sha256(manifest_bytes).hexdigest() == pointer['selected']['sha256']
    manifest = json.loads(manifest_bytes)
    expected_icon = next(a['icon'] for a in manifest['apps'] if a['id'] == 'com.example.geometric')
    expected_hero = next(a['hero'] for a in manifest['apps'] if a['id'] == 'com.example.geometric')
    prepared_root = args.out / 'synthetic-owner-import'
    data, images, _, _ = tv.stage_bundle(args.bundle, prepared_root)
    data = copy.deepcopy(data)
    data['source'] = dict(kind='import', label='Synthetic local-theme mapping fixture')
    data['device'] = {}
    for app in data['apps']:
        app['inventory_confirmed'] = False
    data['apps'].insert(0, tv.app_metadata(dict(id='com.example.geometric', title='Original theme fixture', version='1'), data['captured_at'], False, 'owner-import'))
    tv.commit(prepared_root, data, images, (prepared_root / 'current.json').read_bytes())

    def run(name, *, theme=args.fixture, config=None, prepared=True, width=1280, height=720, view='home', fail=False):
        report = args.out / (name + '.json')
        cmd = [str(args.binary.resolve()), '--assets', str(args.assets.resolve()),
               '--config', str(config or args.out / (name + '-settings.json')),
               '--no-vsync', '--fps', '240', '--step-ms', str(1000 / 120), '--frames', '70',
               '--width', str(width), '--height', str(height), '--view', view,
               '--report', str(report), '--screenshot', str(args.out / (name + '.png'))]
        if theme is not None:
            cmd += ['--theme-data', str(theme.resolve())]
        if prepared:
            cmd += ['--prepared-data', str(prepared_root.resolve())]
        completed = subprocess.run(cmd, env=env, text=True, capture_output=True, timeout=90)
        (args.out / (name + '.log')).write_text(completed.stdout + completed.stderr, encoding='utf8')
        if fail:
            assert completed.returncode != 0 and 'Theme invalid:' in completed.stderr, (name, completed.stdout, completed.stderr)
            assert not report.exists()
            value = None
        else:
            assert completed.returncode == 0, (name, completed.stdout, completed.stderr)
            value = json.loads(report.read_text())
            assert not value['tv_verified']
            assert value['theme_data']['interactive_io'] is False
            assert all(value['service_requests_posted'][k] == 0 for k in ('apps', 'device', 'inputs', 'settings', 'retropie', 'probe', 'launch'))
        results.append(dict(name=name, passed=True))
        print('PASS ' + name, flush=True)
        return value

    home = run('theme-home-720')
    assert home['theme_data']['loaded'] and home['theme_data']['name'] == 'Geometric Blue'
    assert home['theme_data']['actual_texture_bytes'] == manifest['rgba_bytes'] - 1600 * 900 * 4
    assert home['selected_app'] == 'com.example.geometric'
    assert home['motion']['artwork_from'] == home['motion']['artwork_to'] == 'theme/' + expected_hero
    mapped = next(a for a in home['theme_data']['resolved_apps'] if a['id'] == 'com.example.geometric')
    assert mapped['icon'] == 'theme/' + expected_icon and mapped['artwork'] == 'theme/' + expected_hero
    assert len(home['theme_data']['semantic_icons']) == 6
    assert home['theme_data']['startup_compressed_bytes_retained'] == 0
    run('theme-home-2160', width=3840, height=2160)
    run('theme-control-720', view='control')
    run('theme-settings-720', view='settings')
    global_home = run('theme-global-fallback', prepared=False)
    assert global_home['motion']['artwork_to'] == 'theme/' + manifest['background']
    prepared_app = next(a for a in data['apps'] if a.get('hero_key'))
    resolved = next(a for a in home['theme_data']['resolved_apps'] if a['id'] == prepared_app['id'])
    assert not resolved['artwork'].startswith('theme/')  # missing policy keeps valid prepared hero

    selection = args.out / 'selection-data'
    shutil.copytree(args.fixture, selection / 'themes')
    def select_fixture_default():
        path = selection / 'themes' / 'current.json'
        value = json.loads(path.read_text())
        value['previous'], value['selected'] = value['selected'], None
        temp = path.with_suffix('.fixture.tmp');temp.write_text(json.dumps(value), encoding='utf8');os.replace(temp, path)
    def select_fixture_previous():
        path = selection / 'themes' / 'current.json'
        value = json.loads(path.read_text())
        value['selected'], value['previous'] = value['previous'], value['selected']
        temp = path.with_suffix('.fixture.tmp');temp.write_text(json.dumps(value), encoding='utf8');os.replace(temp, path)
    select_fixture_default()
    default = run('theme-default-selection', theme=selection / 'themes')
    assert not default['theme_data']['loaded'] and not default['theme_data']['error']
    assert default['theme_data']['actual_texture_bytes'] == 0
    select_fixture_previous()
    previous = run('theme-previous-selection', theme=selection / 'themes')
    assert previous['theme_data']['loaded'] and previous['theme_data']['generation'] == home['theme_data']['generation']

    implicit = args.out / 'implicit'
    shutil.copytree(args.fixture, implicit / 'themes')
    (implicit / 'themes' / 'current.json').write_text('{bad', encoding='utf8')
    run('theme-explicit-invalid', theme=implicit / 'themes', fail=True)
    fallback = run('theme-implicit-invalid', theme=None, config=implicit / 'settings.json')
    assert not fallback['theme_data']['loaded'] and 'Theme invalid:' in fallback['theme_data']['error']
    assert fallback['theme_data']['actual_texture_bytes'] == 0
    (args.out / 'theme-native-qa.json').write_text(json.dumps(dict(passed=len(results), cases=results, tv_access=False, source='native GLES2 framebuffer'), indent=2), encoding='utf8')
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
