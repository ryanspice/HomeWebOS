#!/usr/bin/env python3
"""Native fixed-step interaction and motion checks; never a refresh-rate benchmark."""
from __future__ import annotations
import argparse
import json
import os
import pathlib
import struct
import subprocess
import tempfile


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument('--binary', type=pathlib.Path, required=True)
    parser.add_argument('--assets', type=pathlib.Path, required=True)
    parser.add_argument('--out', type=pathlib.Path, required=True)
    parser.add_argument('--baseline', action='store_true', help='Record known failures without accepting them')
    args = parser.parse_args()
    args.out.mkdir(parents=True, exist_ok=True)
    results = []
    env = dict(os.environ, SPICE_RETROPIE_TOKEN='', SPICE_CPP_PRIVATE_LUNA='0')

    def action(frame, name, **values):
        return dict(frame=frame, action=name, **values)

    def check(name, condition):
        results.append(dict(name=name, passed=bool(condition)))
        print(('PASS ' if condition else 'FAIL ') + name, flush=True)

    with tempfile.TemporaryDirectory(prefix='spice-motion-qa-') as tmp:
        temp = pathlib.Path(tmp)

        def run(name, *, actions=(), view='home', frames=12, hz=120, width=1920, height=1080, screenshot=False):
            script = args.out / (name + '.input.json')
            script.write_text(json.dumps(list(actions), indent=2) + '\n')
            report = args.out / (name + '.json')
            cmd = [str(args.binary.resolve()), '--assets', str(args.assets.resolve()),
                   '--config', str(temp / (name + '-settings.json')), '--no-vsync', '--fps', '120',
                   '--width', str(width), '--height', str(height), '--frames', str(frames),
                   '--step-ms', str(1000 / hz), '--view', view, '--script', str(script), '--report', str(report)]
            if screenshot:
                cmd += ['--screenshot', str(args.out / (name + '.png'))]
            p = subprocess.run(cmd, env=env, text=True, capture_output=True, timeout=60)
            (args.out / (name + '.log')).write_text(p.stdout + p.stderr)
            if p.returncode:
                raise RuntimeError(f'{name}: {p.stdout}\n{p.stderr}')
            data = json.loads(report.read_text())
            assert data['animation_clock'] == 'fixed-step-desktop-qa' and data['tv_verified'] is False
            assert data['capture_frames'] == frames
            assert (data['drawable_width'], data['drawable_height']) == (width, height)
            if screenshot:
                header = (args.out / (name + '.png')).read_bytes()[:24]
                assert header[:8] == b'\x89PNG\r\n\x1a\n' and struct.unpack('>II', header[16:24]) == (width, height)
            return data

        data = run('panel-visible-edge-click', view='settings', actions=[action(1, 'click', x=1600, y=195)])
        check('moving panel activates at painted lower edge', data['view'] == 'home')
        data = run('panel-unpainted-edge-click', view='settings', actions=[action(1, 'click', x=1600, y=135)])
        check('moving panel rejects unpainted resting edge', data['view'] == 'settings')
        data = run('control-visible-edge-click', view='control', actions=[action(1, 'click', x=500, y=780)])
        check('moving Control Center card shares hit bounds', data['view'] == 'processor')
        data = run('inspector-visible-edge-click', view='designs', actions=[action(0, 'ok'), action(1, 'click', x=200, y=1048)])
        check('moving inspector Back shares hit bounds', data['view'] == 'designs' and data['design_selected'] == '')

        data = run('focus-settles', hz=240, frames=40, actions=[action(0, 'right')])
        check('focus reaches exact target after 167 simulated ms', data['motion']['focus_settled'] and data['motion']['focus_rect'] == data['motion']['focus_target'])
        perf = data['performance']
        check('focus move repaints cached home at most once', perf['home_cache_repaints'] <= 1 and perf['home_cache_hits'] > 0)
        check('focus move stays within the interaction draw budget', perf['draw_calls_total'] <= 6000)
        data = run('blur-tracks', hz=240, actions=[action(0, 'right'), action(1, 'trace'), action(2, 'blur'), action(2, 'trace'), action(4, 'trace'), action(6, 'focus'), action(6, 'trace'), action(7, 'trace')])
        traces = {x['tick']: x['motion'] for x in data['motion_trace']}
        frozen_fields = ['focus_rect', 'panel_progress', 'artwork_from', 'artwork_to', 'artwork_progress']
        check('all presentation tracks freeze while inactive', all(traces[1][k] == traces[2][k] == traces[4][k] for k in frozen_fields))
        check('resume first frame consumes no hidden elapsed time', traces[6]['focus_rect'] == traces[4]['focus_rect'] and traces[6]['artwork_progress'] == traces[4]['artwork_progress'])
        check('blur stops redraw and resume advances deliberately', not traces[4]['active'] and not traces[4]['redraw_requested'] and traces[6]['active'] and traces[7]['focus_rect'] != traces[4]['focus_rect'])

        for view in ('settings', 'inspector'):
            start = [action(0, 'ok')] if view == 'inspector' else []
            initial = 'designs' if view == 'inspector' else view
            lifecycle = start + [action(0, 'trace'), action(1, 'blur'), action(1, 'trace'),
                                 action(3, 'trace'), action(4, 'focus'), action(4, 'trace'), action(5, 'trace')]
            data = run(view + '-entry-lifecycle', view=initial, actions=lifecycle)
            traces = {x['tick']: x['motion'] for x in data['motion_trace']}
            fields = ['panel_progress', 'panel_offset_y', 'focus_rect', 'artwork_progress']
            check(view + ' entry freezes every presentation track', all(traces[0][k] == traces[1][k] == traces[3][k] for k in fields))
            check(view + ' entry resumes without a hidden-time jump', traces[4]['panel_progress'] == traces[3]['panel_progress'] and traces[5]['panel_progress'] > traces[4]['panel_progress'])
            data = run(view + '-entry-reduced', view=initial, frames=2,
                       actions=start + [action(1, 'motion'), action(1, 'trace')])
            motion = data['motion']
            check(view + ' entry reduced motion resolves next update', data['reduced_motion'] and motion['panel_progress'] == 1 and motion['panel_offset_y'] == 0 and motion['focus_rect'] == motion['focus_target'])

        data = run('rapid-reversal-return', frames=8, actions=[action(0, 'right'), action(1, 'right'),
                   action(2, 'left'), action(3, 'control'), action(4, 'back')])
        check('rapid reversal and overlay return retain actionable focus', data['focus'] == 'rail:1' and data['selected_app'] == 'sample.eclipse' and data['motion']['focus_rect'] == data['motion']['focus_target'] and data['motion']['artwork_pending'] == '')
        roundtrip = [action(0, 'click', x=1623, y=70), action(1, 'click', x=600, y=850),
                     action(2, 'click', x=1580, y=242), action(3, 'text', text='live'), action(4, 'back'),
                     action(5, 'click', x=200, y=390), action(6, 'back'), action(7, 'back'), action(8, 'trace')]
        data = run('catalog-keyboard-inspector-return', frames=9, actions=roundtrip)
        check('catalog keyboard inspector return restores settings focus pose', data['view'] == 'settings' and data['focus'] == 'designs' and data['motion']['focus_rect'] == data['motion']['focus_target'])
        data = run('catalog-launcher-return', frames=11, actions=roundtrip + [action(9, 'back')])
        check('full catalog round trip restores launcher focus pose', data['view'] == 'home' and data['focus'] == 'settings' and data['motion']['focus_rect'] == data['motion']['focus_target'])

        rapid = [action(0, 'right'), action(1, 'trace'), action(2, 'right'), action(2, 'trace'), action(3, 'right'), action(3, 'trace')]
        data = run('artwork-coalesces', hz=240, frames=4, actions=rapid, screenshot=True)
        traces = {x['tick']: x['motion'] for x in data['motion_trace']}
        check('rapid focus preserves displayed artwork pair', (traces[1]['artwork_from'], traces[1]['artwork_to']) == (traces[3]['artwork_from'], traces[3]['artwork_to']) and traces[3]['artwork_progress'] > traces[1]['artwork_progress'])
        check('only latest artwork request waits; selection is immediate', traces[3]['artwork_pending'] == 'ember' and data['selected_app'] == 'sample.ember' and data['focus'] == 'rail:3')
        data = run('artwork-final', hz=240, frames=160, actions=rapid)
        check('completed artwork has one retained image and no queue', data['motion']['artwork_from'] == data['motion']['artwork_to'] == 'ember' and data['motion']['artwork_pending'] == '' and not data['motion']['artwork_active'])
        data = run('reduced-mid-flight', frames=2, actions=[action(0, 'right'), action(1, 'motion')])
        motion = data['motion']
        check('reduced motion resolves focus and artwork next update', data['reduced_motion'] and motion['focus_rect'] == motion['focus_target'] and motion['artwork_from'] == motion['artwork_to'] and not motion['artwork_active'])

        poses = []
        for hz in (60, 120, 240):
            data = run(f'elapsed-100ms-{hz}', hz=hz, frames=hz // 10, actions=[action(0, 'right')])
            poses.append(data['motion'])
        check('60/120/240 steps agree at 100 simulated ms', all(abs(p['focus_rect'][k] - poses[0]['focus_rect'][k]) <= .125 for p in poses for k in ('x', 'y', 'w', 'h')) and all(abs(p['artwork_progress'] - poses[0]['artwork_progress']) <= .0001 for p in poses))

        for width, height in ((1280, 720), (3840, 2160)):
            data = run(f'panel-hit-{height}', view='settings', width=width, height=height,
                       actions=[action(1, 'click', x=round(1600 * width / 1920), y=round(195 * height / 1080))])
            check(f'painted hit bounds survive {height}p scaling', data['view'] == 'home')
        for width, height in ((1365, 769), (1920, 1200), (2560, 1080)):
            scale = min(width / 1920, height / 1080)
            left, top = (width - 1920 * scale) / 2, (height - 1080 * scale) / 2
            data = run(f'panel-hit-{width}x{height}', view='settings', width=width, height=height,
                       actions=[action(1, 'click', x=round(left + 1600 * scale), y=round(top + 195 * scale))])
            check(f'painted hit bounds survive {width}x{height} letterbox', data['view'] == 'home')
            if top > 1 or left > 1:
                x, y = (round(left + 1600 * scale), round(top / 2)) if top > 1 else (round(left / 2), round(195 * scale))
                data = run(f'bar-reject-{width}x{height}', view='settings', width=width, height=height,
                           actions=[action(1, 'click', x=x, y=y)])
                check(f'letterbox bars reject activation at {width}x{height}', data['view'] == 'settings')
        for view in ('control', 'settings'):
            run(f'{view}-entry-1080', view=view, frames=2, screenshot=True)
            run(f'{view}-settled-1080', view=view, frames=48, hz=60, screenshot=True)
        run('inspector-entry-2160', view='designs', width=3840, height=2160, frames=2,
            actions=[action(0, 'ok')], screenshot=True)
        run('home-settled-2160', width=3840, height=2160, hz=60, frames=48,
            actions=[action(0, 'right')], screenshot=True)

        for name, flags in [('unbounded-step', ['--step-ms', '4']), ('invalid-step', ['--frames', '1', '--step-ms', 'nan']),
                            ('zero-step', ['--frames', '1', '--step-ms', '0']), ('oversized-step', ['--frames', '1', '--step-ms', '51'])]:
            p = subprocess.run([str(args.binary.resolve()), '--assets', str(args.assets.resolve()), *flags],
                               env=env, capture_output=True, text=True, timeout=10)
            check(name + ' rejected', p.returncode != 0)
        summary = dict(scope='Native SDL/GLES2 fixed-step animation QA; not GPU, refresh or human proof',
                       baseline=args.baseline, tv_tested=False, passed=sum(x['passed'] for x in results),
                       failed=sum(not x['passed'] for x in results), tests=results)
        (args.out / 'motion-qa.json').write_text(json.dumps(summary, indent=2) + '\n')
        print(f"RESULT {summary['passed']} passed / {summary['failed']} failed", flush=True)
        return 0 if args.baseline or not summary['failed'] else 1


if __name__ == '__main__':
    raise SystemExit(main())
