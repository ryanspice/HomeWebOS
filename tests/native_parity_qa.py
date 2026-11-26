#!/usr/bin/env python3
"""Native controller/search/return-context regressions. No TV or live web calls."""
import argparse
import json
import os
from pathlib import Path
import struct
import subprocess


def main():
    parser = argparse.ArgumentParser()
    for key in ('binary', 'assets', 'bundle', 'out'):
        parser.add_argument('--' + key, type=Path, required=True)
    args = parser.parse_args()
    args.out.mkdir(parents=True, exist_ok=False)
    env = dict(os.environ, SPICE_CPP_PRIVATE_LUNA='0', SPICE_RETROPIE_TOKEN='')
    results = []

    def action(frame, name, **kw):
        return dict(frame=frame, action=name, **kw)

    def run(name, actions=(), *, view='library', prepared=False, width=1280, height=720, frames=80, seed=None):
        script = args.out / (name + '.input.json')
        script.write_text(json.dumps(list(actions)), encoding='utf8')
        report = args.out / (name + '.json')
        if seed is not None:
            (args.out / (name + '-settings.json')).write_text(json.dumps(seed), encoding='utf8')
        cmd = [str(args.binary.resolve()), '--assets', str(args.assets.resolve()),
               '--config', str(args.out / (name + '-settings.json')), '--no-vsync',
               '--fps', '240', '--step-ms', str(1000 / 120), '--view', view,
               '--width', str(width), '--height', str(height), '--frames', str(frames),
               '--script', str(script), '--report', str(report),
               '--screenshot', str(args.out / (name + '.png'))]
        if prepared:
            cmd += ['--prepared-data', str(args.bundle.resolve())]
        completed = subprocess.run(cmd, env=env, text=True, capture_output=True, timeout=90)
        (args.out / (name + '.log')).write_text(completed.stdout + completed.stderr, encoding='utf8')
        assert completed.returncode == 0, (name, completed.stdout, completed.stderr)
        value = json.loads(report.read_text())
        assert value['runtime'] == 'desktop-demo' and not value['tv_verified']
        assert all(value['service_requests_posted'][k] == 0 for k in ('apps', 'device', 'inputs', 'settings', 'retropie', 'probe', 'launch'))
        png = (args.out / (name + '.png')).read_bytes()
        assert png[:8] == b'\x89PNG\r\n\x1a\n' and struct.unpack('>II', png[16:24]) == (width, height)
        results.append(dict(name=name, passed=True, renderer=value['gl_renderer']))
        print('PASS ' + name, flush=True)
        return value

    def ids(value):
        return [node['id'] for node in value['focus_targets']]

    # Actual D-pad/OK key events reach Search, enter AE, then choose Show matches.
    keys = ['right', 'right', 'up', 'ok', 'ok'] + ['right'] * 4 + ['ok']
    keyboard = [action(26 + i, key) for i, key in enumerate(keys)]
    opened = run('library-keyboard-controller', keyboard)
    assert opened['library_search_open'] and opened['filter'] == 'ae', opened
    done = run('library-keyboard-results', keyboard + [action(40 + i, key) for i, key in enumerate(['down'] * 4 + ['right', 'ok'])])
    assert not done['library_search_open'] and done['filter'] == 'ae'
    assert done['focus'] == 'app:sample.aether' and done['selected_app'] == 'sample.aether', done
    back = run('library-keyboard-back', keyboard + [action(42, 'back')])
    assert not back['library_search_open'] and back['filter'] == 'ae' and back['focus'] == 'library-search'
    selected = run('library-keyboard-back-down', keyboard + [action(42, 'back'), action(44, 'down')])
    assert selected['filter'] == 'ae' and selected['focus'] == 'app:sample.aether' and selected['pin_target'] == 'sample.aether'
    empty_down = run('library-empty-search-down', [action(26, 'click', x=965, y=161), action(28, 'text', text='no-match'), action(30, 'back'), action(32, 'down')])
    assert empty_down['filter'] == 'no-match' and empty_down['focus'] == 'refresh-apps' and empty_down['pin_target'] == ''
    empty = run('library-empty-delete', [action(26, 'click', x=965, y=161), action(28, 'click', x=475, y=536)])
    assert empty['library_search_open'] and empty['filter'] == ''
    typed = run('library-keyboard-utf8', [action(26, 'click', x=965, y=161), action(28, 'text', text='é'), action(29, 'backspace'), action(30, 'text', text='fr')])
    assert typed['library_search_open'] and typed['filter'] == 'fr'

    nested = run('library-nested-return', [action(26, 'text', text='aether'), action(28, 'open:control'), action(30, 'open:settings'), action(32, 'back'), action(34, 'back')])
    assert nested['view'] == 'library' and nested['filter'] == 'aether' and nested['selected_app'] == 'sample.aether'
    assert nested['focus'] == 'app:sample.aether' and nested['navigation_depth'] == 1
    toggle = run('control-toggle-return', [action(26, 'text', text='aether'), action(28, 'open:control'), action(30, 'open:control')])
    assert toggle['view'] == 'library' and toggle['filter'] == 'aether'
    paged = run('library-page-return', [action(26, 'click', x=411, y=573), action(28, 'right'), action(30, 'open:control'), action(32, 'back')], prepared=True)
    assert paged['view'] == 'library' and paged['library_page'] == 2 and paged['focus'].startswith('app:')
    assert paged['selected_app'] == paged['focus'][4:]
    reviewed = run('library-review-return', [action(26, 'click', x=590, y=573), action(28, 'open:control'), action(30, 'back')], prepared=True)
    assert reviewed['inventory_scan']['review_filter'] and reviewed['focus'] == 'review-candidates'
    initial = run('library-first-page', prepared=True)
    assert 'previous' not in ids(initial) and 'next' in ids(initial)
    last = run('library-last-page', [action(26, 'click', x=411, y=573), action(28, 'click', x=411, y=573)], prepared=True)
    assert last['library_page'] == 3 and 'next' not in ids(last) and 'previous' in ids(last)
    moving = run('immediate-action-target', [action(1, 'right')], view='home', frames=2)
    assert moving['selected_app'] == 'sample.eclipse' and moving['motion']['focus_rect'] != moving['motion']['focus_target']
    high = run('library-keyboard-2160', [action(26, 'click', x=2895, y=483)], width=3840, height=2160)
    assert high['library_search_open'] and 'library-search-done' in ids(high)
    for n in high['presented_focus_targets']:
        assert 0 <= n['x'] and n['x'] + n['w'] <= 1920 and 0 <= n['y'] and n['y'] + n['h'] <= 1080
    saved = run('background-settings-final-flush', [action(1, 'expand'), action(1, 'motion'), action(1, 'theme'), action(2, 'motion')], view='home', seed={'unrelated': {'keep': 7}})
    status = saved['settings_persistence']
    assert status['state'] == 'saved' and status['shutdown_drained'] and status['sequence'] == 4
    settings = json.loads((args.out / 'background-settings-final-flush-settings.json').read_text())
    assert settings['unrelated'] == {'keep': 7} and settings['reduced_motion'] is False
    (args.out / 'parity-native-qa.json').write_text(json.dumps(dict(passed=len(results), cases=results, tv_access=False, hardware_controller=False), indent=2), encoding='utf8')
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
