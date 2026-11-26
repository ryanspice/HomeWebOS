#!/usr/bin/env python3
"""Exercise the actual native executable. Run inside Xvfb or a desktop session.
Screenshots and reports are produced by glReadPixels / native application state,
not by a web renderer. No TV is accessed; --script is disabled in webOS builds.
"""
from __future__ import annotations
import argparse, json, os, pathlib, struct, subprocess, tempfile, time

# Every gate below is an assert. Under -O those checks vanish and this script
# would still write a green report, so refuse optimized interpreters.
if not __debug__:
    raise RuntimeError('native_qa.py must not run with python -O / PYTHONOPTIMIZE')


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument('--binary', type=pathlib.Path, required=True)
    parser.add_argument('--assets', type=pathlib.Path, required=True)
    parser.add_argument('--out', type=pathlib.Path, required=True)
    args = parser.parse_args()
    args.out.mkdir(parents=True, exist_ok=True)
    # A failed run aborts on an assert before the summary is written. Remove any
    # earlier green report so a consumer cannot read stale evidence.
    (args.out / 'native-qa.json').unlink(missing_ok=True)
    results = []
    env = dict(os.environ, SPICE_RETROPIE_TOKEN='', SPICE_CPP_PRIVATE_LUNA='0')
    # Caller chooses software rendering or a real desktop GPU. Never claim TV proof.
    with tempfile.TemporaryDirectory(prefix='spice-native-qa-') as tmp:
        temp = pathlib.Path(tmp)
        def run(name, *, width=1920, height=1080, actions=(), view='home', frames=20, screenshot=False, config=None):
            script = temp / (name + '.json')
            script.write_text(json.dumps(list(actions)), encoding='utf8')
            report = args.out / (name + '.json')
            config = config or temp / (name + '-settings.json')
            cmd = [str(args.binary.resolve()), '--assets', str(args.assets.resolve()),
                   '--config', str(config), '--no-vsync', '--fps', '120',
                   '--width', str(width), '--height', str(height), '--frames', str(frames),
                   '--report', str(report), '--script', str(script), '--view', view]
            if screenshot:
                cmd += ['--screenshot', str(args.out / (name + '.png'))]
            start = time.monotonic()
            p = subprocess.run(cmd, env=env, text=True, capture_output=True, timeout=40)
            (args.out / (name + '.log')).write_text(p.stdout + p.stderr, encoding='utf8')
            assert p.returncode == 0, f'{name}: {p.stdout}\n{p.stderr}'
            data = json.loads(report.read_text())
            assert data['runtime'] == 'desktop-demo' and data['tv_verified'] is False
            assert data['drawable_width'] == width and data['drawable_height'] == height
            assert data['capture_frames'] == frames
            if screenshot:
                png = (args.out / (name + '.png')).read_bytes()
                assert png[:8] == b'\x89PNG\r\n\x1a\n'
                assert struct.unpack('>II', png[16:24]) == (width, height)
            results.append({'name': name, 'passed': True, 'seconds': round(time.monotonic()-start,3),
                            'gl': data['gl_version'], 'renderer': data['gl_renderer']})
            print('PASS', name, flush=True)
            return data
        def action(frame, name, **kw):
            return dict(frame=frame, action=name, **kw)
        for width,height in [(1280,720),(1920,1080),(3840,2160)]:
            data=run(f'home-{height}',width=width,height=height,frames=18,screenshot=True)
            nodes={n['id']:n for n in data['focus_targets']}
            assert len([n for n in nodes if n.startswith('extra:')])==9
            assert len([n for n in nodes if n.startswith('card:')])==5
            assert nodes['extra:0']['x']==nodes['rail:3']['x']
            assert nodes['extra:4']['x']==nodes['rail:4']['x']
            assert nodes['extra:7']['x']==nodes['rail:5']['x']
            assert nodes['card:0']['y']==832
        data=run('dpad-staircase',actions=[action(i+1,a) for i,a in enumerate(['right','right','right','down','down'])])
        assert data['focus']=='extra:4', data['focus']
        data=run('overlay-restore',actions=[action(1,'right'),action(2,'right'),action(3,'right'),action(4,'control'),action(8,'back')])
        assert data['view']=='home' and data['focus']=='rail:3'
        for width,height in [(1280,720),(3840,2160)]:
            data=run(f'pointer-{height}',width=width,height=height,
                actions=[action(2,'click',x=int(1460*width/1920),y=int(400*height/1080))])
            assert data['view']=='processor',data['view']
        data=run('library-filter',actions=[action(1,'library'),action(2,'text',text='Aether')])
        assert data['filter']=='Aether'
        apps=[x for x in data['focus_targets'] if x['id'].startswith('app:')]
        assert len(apps)==1 and apps[0]['id']=='app:sample.aether'
        data=run('library-utf8',actions=[action(1,'library'),action(2,'text',text='é'),action(3,'backspace')])
        assert data['view']=='library' and data['filter']==''
        config=temp/'persist.json'
        data=run('motion-and-collapse',config=config,actions=[action(1,'expand'),action(2,'motion')])
        assert not data['expanded'] and data['reduced_motion']
        data=run('settings-reload',config=config)
        assert not data['expanded'] and data['reduced_motion']
        data=run('blur-focus-recovery',actions=[action(2,'blur'),action(6,'focus')])
        assert data['active'] and data['view']=='home'
        data=run('native-launch-blocked',view='homebrew',actions=[action(3,'click',x=1250,y=850)],frames=24)
        assert 'Desktop mode' in data['last_notice'],data['last_notice']
        data=run('handoff-confirmation',view='settings',actions=[action(2,'click',x=700,y=510)])
        assert data['view']=='confirm' and not data['handoff_enabled']
        data=run('handoff-cancel',view='settings',actions=[action(2,'click',x=700,y=510),action(4,'back')])
        assert not data['handoff_enabled']
        bad=temp/'invalid-settings.json';bad.write_text('{broken')
        data=run('corrupt-settings-preserved',config=bad,actions=[action(2,'motion')])
        assert bad.read_text()=='{broken'
        assert data['config_writable'] is False
        data=run('game-home',actions=[action(2,'right')],screenshot=True,frames=32)
        assert data['selected_app']=='sample.eclipse'
        data=run('designs-catalog',view='designs',frames=20,screenshot=True)
        assert data['designs']==50,data['designs']
        assert data['design_category']=='All' and data['design_selected']==''
        tiles=[x['id'] for x in data['focus_targets'] if x['id'].startswith('design:')]
        assert len(tiles)==15,len(tiles)
        assert all(x.startswith('design:') or ':' not in x for x in tiles)
        assert 'design-previous' not in [x['id'] for x in data['focus_targets']]
        assert data['design_page']==1 and data['design_matches']==50
        data=run('designs-filter',view='designs',actions=[action(1,'click',x=564,y=242)])
        assert data['design_category']=='Streaming',data['design_category']
        assert data['focus']=='design-cat:Streaming',data['focus']
        assert len([x for x in data['focus_targets'] if x['id'].startswith('design:')])==15
        data=run('designs-search',view='designs',actions=[action(1,'text',text='orbit')])
        found=[x['id'] for x in data['focus_targets'] if x['id'].startswith('design:')]
        assert len(found)==3,found
        data=run('designs-inspector',view='designs',actions=[action(1,'click',x=266,y=391)],screenshot=True)
        assert data['design_selected']=='live-tv',data['design_selected']
        data=run('designs-inspector-back',view='designs',actions=[action(1,'click',x=266,y=391),action(4,'back')])
        assert data['design_selected']=='' and data['view']=='designs'
        assert data['focus']=='design:live-tv'
        data=run('designs-inspector-step',view='designs',actions=[action(1,'click',x=266,y=391),action(3,'click',x=1666,y=958),action(5,'click',x=1666,y=958)])
        assert data['design_selected'] not in ('','live-tv'),data['design_selected']
        data=run('designs-inspector-prev',view='designs',actions=[action(1,'click',x=266,y=391),action(3,'click',x=1510,y=958)])
        assert data['design_selected'] not in ('','live-tv'),data['design_selected']
        data=run('designs-no-launch',view='designs',actions=[action(1,'ok'),action(3,'ok')])
        assert not data['last_notice'].lower().startswith('launch'),data['last_notice']
        data=run('designs-entry',view='settings',actions=[action(1,'click',x=600,y=848)])
        assert data['view']=='designs' and data['focus']=='design:live-tv'
        data=run('designs-return-settings',view='settings',actions=[action(1,'click',x=600,y=848),action(3,'back')])
        assert data['view']=='settings' and data['focus']=='designs'
        data=run('designs-return-home',actions=[action(1,'click',x=1622,y=68),action(3,'click',x=600,y=848),action(5,'back'),action(7,'back')])
        assert data['view']=='home' and data['focus']=='settings'
        data=run('designs-category-dpad',view='designs',actions=[action(i+1,a) for i,a in enumerate(['up','right','right','ok'])],screenshot=True)
        assert data['design_category']=='Streaming' and data['focus']=='design-cat:Streaming'
        data=run('designs-final-page',view='designs',actions=[action(i,'click',x=400,y=1012) for i in (1,3,5)],screenshot=True)
        assert data['design_page']==4
        ids=[x['id'] for x in data['focus_targets']]
        assert len([x for x in ids if x.startswith('design:')])==5
        assert 'design-next' not in ids and 'design-previous' in ids
        data=run('designs-page-search',view='designs',actions=[action(1,'click',x=400,y=1012),action(3,'text',text='orbit')])
        assert data['design_page']==1 and data['design_matches']==3
        assert data['focus'].startswith('design:')
        assert not any(x['id'] in ('design-next','design-previous') for x in data['focus_targets'])
        # These are SDL keydown events as well as text events: searching must not
        # activate C/E/M/T/F launcher shortcuts or persist preference changes.
        typed=[]
        for key,letter in [('control','c'),('expand','e'),('motion','m'),('theme','t'),('favorite','f')]:
            typed.extend([action(len(typed)+1,key),action(len(typed)+2,'text',text=letter)])
        data=run('designs-typing-shortcuts',view='designs',actions=typed)
        assert data['view']=='designs' and data['filter']=='cemtf'
        assert data['expanded'] and not data['reduced_motion'] and data['theme']=='ps5'
        data=run('designs-empty',view='designs',actions=[action(1,'click',x=750,y=242),action(3,'text',text='no-such-concept')],screenshot=True)
        assert data['design_matches']==0 and any(x['id']=='design-reset' for x in data['focus_targets'])
        data=run('designs-empty-reset',view='designs',actions=[action(1,'click',x=750,y=242),action(3,'text',text='no-such-concept'),action(5,'click',x=960,y=602)])
        assert data['filter']=='' and data['design_category']=='All' and data['design_matches']==50
        assert data['focus']=='design:live-tv'
        data=run('designs-clear-search',view='designs',actions=[action(1,'click',x=750,y=242),action(3,'text',text='no-such-concept'),action(5,'click',x=1755,y=242)])
        assert data['filter']=='' and data['design_category']=='Sports' and data['design_matches']==4
        assert data['focus']=='design-search'
        data=run('designs-utf8-backspace',view='designs',actions=[action(1,'text',text='é'),action(3,'backspace'),action(5,'backspace')])
        assert data['view']=='designs' and data['filter']=='' and data['focus']=='design:live-tv'
        search_dpad=['up']+['right']*7+['ok','ok','right','ok','back']
        data=run('designs-keyboard-dpad',view='designs',actions=[action(i+1,a) for i,a in enumerate(search_dpad)],frames=32)
        assert data['filter']=='ab' and not data['design_search_open'] and data['focus']=='design-search'
        data=run('designs-keyboard-results',view='designs',actions=[action(1,'click',x=1580,y=242),action(3,'text',text='orbit')],screenshot=True)
        assert data['design_search_open'] and data['design_matches']==3 and data['focus']=='design-key:A'
        assert all(x['id'].startswith('design-key:') or x['id'] in ('design-delete','design-key-clear','design-search-done') for x in data['focus_targets'])
        data=run('designs-keyboard-done',view='designs',actions=[action(1,'click',x=1580,y=242),action(3,'text',text='orbit'),action(5,'click',x=1250,y=800)])
        assert not data['design_search_open'] and data['filter']=='orbit' and data['design_matches']==3
        assert data['focus'].startswith('design:')
        data=run('designs-keyboard-delete',view='designs',actions=[action(1,'click',x=1580,y=242),action(3,'text',text='é'),action(5,'click',x=725,y=800),action(7,'click',x=725,y=800)])
        assert data['design_search_open'] and data['filter']=='' and data['focus']=='design-delete'
        data=run('designs-keyboard-clear',view='designs',actions=[action(1,'click',x=1580,y=242),action(3,'text',text='orbit'),action(5,'click',x=940,y=800)])
        assert data['design_search_open'] and data['filter']=='' and data['focus']=='design-key-clear'
        data=run('designs-inspector-wrap-restore',view='designs',actions=[action(1,'ok'),action(3,'click',x=1510,y=958),action(5,'back')])
        assert data['design_selected']=='' and data['design_page']==4 and data['focus'].startswith('design:')
        tiles=[x['id'] for x in data['focus_targets'] if x['id'].startswith('design:')]
        assert data['focus']==tiles[-1]
        for width,height in [(1280,720),(3840,2160)]:
            data=run(f'designs-keyboard-{height}',view='designs',width=width,height=height,
                actions=[action(1,'click',x=int(1580*width/1920),y=int(242*height/1080)),action(3,'text',text='orbit')],screenshot=True)
            assert data['design_search_open'] and data['design_matches']==3
        for view in ['control','designs','processor','retro','homebrew','tv','diagnostics','settings','preview']:
            data=run(view,view=view,screenshot=True,frames=28)
            assert data['view']==view
        # An unknown deeplink view falls back to Home (normal relaunch path),
        # so it is a positive control, not an expected-failure case.
        bad_view = subprocess.run([str(args.binary.resolve()),'--assets',str(args.assets.resolve()),
                                   '--view','bogus','--frames','1'],env=env,capture_output=True,text=True,timeout=15)
        assert bad_view.returncode == 0, bad_view.stderr
        (args.out/'invalid-view.log').write_text(bad_view.stdout+bad_view.stderr)
        results.append({'name':'invalid-view-falls-back-home','passed':True})
        print('PASS invalid-view-falls-back-home',flush=True)
        for name,flags in [('no-webos-on-host',['--webos']),('missing-font',['--font','/missing.ttf'])]:
            p=subprocess.run([str(args.binary.resolve()),'--assets',str(args.assets.resolve()),*flags,'--frames','1'],env=env,capture_output=True,text=True,timeout=15)
            assert p.returncode!=0,name
            (args.out/(name+'.log')).write_text(p.stdout+p.stderr)
            results.append({'name':name,'passed':True,'expected_failure':True})
            print('PASS',name,flush=True)
    (args.out/'native-qa.json').write_text(json.dumps({'tv_tested':False,'tests':results,'passed':len(results),'failed':0},indent=2)+'\n')
    print(f'RESULT {len(results)} native scenarios passed')
    return 0

if __name__=='__main__':
    raise SystemExit(main())
