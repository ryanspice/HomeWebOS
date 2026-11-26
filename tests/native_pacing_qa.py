#!/usr/bin/env python3
"""Bounded SDL/GLES timing checks; no display-refresh or TV performance claim."""
from __future__ import annotations

import argparse
import json
import os
import pathlib
import subprocess
import tempfile


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument('--binary', type=pathlib.Path, required=True)
    parser.add_argument('--baseline-binary', type=pathlib.Path)
    parser.add_argument('--assets', type=pathlib.Path, required=True)
    parser.add_argument('--out', type=pathlib.Path, required=True)
    args = parser.parse_args()
    args.out.mkdir(parents=True, exist_ok=True)
    results = []
    env = dict(os.environ, SPICE_RETROPIE_TOKEN='', SPICE_CPP_PRIVATE_LUNA='0')

    def check(name, condition, detail):
        results.append(dict(name=name, passed=bool(condition), detail=detail))
        print(('PASS ' if condition else 'FAIL ') + name + ': ' + str(detail), flush=True)

    with tempfile.TemporaryDirectory(prefix='spice-pacing-qa-') as tmp:
        temp = pathlib.Path(tmp)

        def run(name, binary, actions, *, frames=2, step=None):
            script = temp / (name + '-script.json')
            report = args.out / (name + '.json')
            script.write_text(json.dumps(actions), encoding='utf-8')
            cmd = [str(binary.resolve()), '--assets', str(args.assets.resolve()),
                   '--config', str(temp / (name + '-settings.json')), '--no-vsync', '--fps', '30',
                   '--width', '1280', '--height', '720', '--frames', str(frames),
                   '--script', str(script), '--report', str(report)]
            if step is not None:
                cmd += ['--step-ms', str(step)]
            proc = subprocess.run(cmd, env=env, capture_output=True, text=True, timeout=40)
            (args.out / (name + '.log')).write_text(proc.stdout + proc.stderr, encoding='utf-8')
            if proc.returncode:
                raise RuntimeError(f'{name} exited {proc.returncode}: {proc.stdout}\n{proc.stderr}')
            data = json.loads(report.read_text(encoding='utf-8'))
            assert data['capture_frames'] == frames and data['tv_verified'] is False
            return data

        event = [dict(frame=1, action='right'), dict(frame=1, action='trace')]
        candidate = run('wall-candidate', args.binary, event)
        first = candidate['motion_trace'][0]['motion']
        check('wall-clock input starts near the beginning of its artwork track',
              candidate['animation_clock'] == 'wall-clock' and candidate['focus'] == 'rail:1' and first['artwork_active']
              and 0 <= first['artwork_progress'] < .03,
              {'progress': first['artwork_progress'], 'focus': candidate['focus']})

        if args.baseline_binary:
            baseline = run('wall-baseline', args.baseline_binary, event)
            prior = baseline['motion_trace'][0]['motion']
            check('baseline consumes the prior rendered-frame interval',
                  prior['artwork_active'] and prior['artwork_progress'] > .05,
                  {'baseline_progress': prior['artwork_progress'], 'candidate_progress': first['artwork_progress']})

        fixed = run('fixed-step', args.binary, event, step=10)
        fixed_progress = fixed['motion_trace'][0]['motion']['artwork_progress']
        check('fixed-step input still includes its 10 ms capture step',
              fixed['animation_clock'] == 'fixed-step-desktop-qa' and abs(fixed_progress - 1 / 30) < .0001,
              {'progress': fixed_progress})

        reduced_actions = [dict(frame=1, action='motion'), dict(frame=1, action='right'), dict(frame=1, action='trace')]
        reduced = run('reduced', args.binary, reduced_actions)
        sample = reduced['motion_trace'][0]['motion']
        check('reduced-motion input paints the settled state',
              reduced['reduced_motion'] and sample['focus_rect'] == sample['focus_target']
              and not sample['artwork_active'], {'motion': sample})

        lifecycle_actions = [dict(frame=0, action='right'), dict(frame=1, action='blur'), dict(frame=1, action='trace'),
                             dict(frame=2, action='focus'), dict(frame=2, action='trace'), dict(frame=3, action='trace')]
        lifecycle = run('lifecycle', args.binary, lifecycle_actions, frames=4)
        traces = {item['tick']: item['motion'] for item in lifecycle['motion_trace']}
        before = traces[1]['focus_rect']['x']
        restored = traces[2]['focus_rect']['x']
        later = traces[3]['focus_rect']['x']
        check('hidden time does not advance the resumed track',
              not traces[1]['active'] and traces[2]['active'] and abs(restored - before) < .5
              and abs(later - restored) > .01,
              {'blur_x': before, 'resume_x': restored, 'next_x': later})

    summary = dict(scope='SDL/GLES desktop, software-rendered; no physical refresh or TV claim',
                   tests=results, passed=sum(item['passed'] for item in results),
                   failed=sum(not item['passed'] for item in results))
    (args.out / 'pacing-qa.json').write_text(json.dumps(summary, indent=2) + '\n', encoding='utf-8')
    print(f"RESULT {summary['passed']} passed / {summary['failed']} failed", flush=True)
    return 1 if summary['failed'] else 0


if __name__ == '__main__':
    raise SystemExit(main())
