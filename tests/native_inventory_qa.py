#!/usr/bin/env python3
"""Native refresh/candidate behavior. TV data is read locally; synthetic delta is labelled."""
import argparse, copy, errno, json, os, pathlib, pty, queue, subprocess, sys, threading
sys.path.insert(0, str(pathlib.Path(__file__).resolve().parents[1] / 'tools'))
import prepare_tv_data as prep

def main():
    parser=argparse.ArgumentParser()
    for key in ('binary','assets','bundle','out'):parser.add_argument('--'+key,type=pathlib.Path,required=True)
    args=parser.parse_args();args.out.mkdir(parents=True,exist_ok=False)
    env=dict(os.environ,SPICE_CPP_PRIVATE_LUNA='0',SPICE_RETROPIE_TOKEN='')
    cases=[]
    def command(name,actions,*,bundle=None,view='home',width=3840,height=2160,frames=84,fixed=True):
        script=args.out/(name+'.input.json');script.write_text(json.dumps(actions),encoding='utf8')
        cmd=[str(args.binary.resolve()),'--assets',str(args.assets.resolve()),'--prepared-data',str((bundle or args.bundle).resolve()),'--config',str(args.out/(name+'-settings.json')),
             '--width',str(width),'--height',str(height),'--fps','30' if not fixed else '240','--no-vsync','--frames',str(frames),'--view',view,'--script',str(script),
             '--report',str(args.out/(name+'.json')),'--screenshot',str(args.out/(name+'.png'))]
        if fixed:cmd+=['--step-ms',str(1000/120)]
        return cmd
    def inspect(name,expected_requests=0):
        value=json.loads((args.out/(name+'.json')).read_text())
        assert not value['tv_verified']
        assert value['service_requests_posted']['apps']==expected_requests,(name,value['service_requests_posted'])
        assert all(value['service_requests_posted'][key]==0 for key in ('device','inputs','settings','retropie','probe','launch'))
        assert not value['inventory_scan']['pending']
        cases.append(dict(name=name,passed=True));print('PASS '+name,flush=True)
        return value
    def run(name,actions,requests=0,**kw):
        result=subprocess.run(command(name,actions,**kw),env=env,text=True,capture_output=True,timeout=90)
        (args.out/(name+'.log')).write_text(result.stdout+result.stderr,encoding='utf8')
        assert result.returncode==0,(name,result.stderr)
        return inspect(name,requests)
    run('home-footer-2160',[])
    scan=run('footer-click-scan',[dict(frame=2,action='click',x=3300,y=2012)],1)
    assert scan['view']=='library' and scan['inventory_scan']['candidate_count']>0 and scan['inventory_scan']['new_count']==0
    assert scan['inventory_scan']['scope']=='developer'
    run('keyboard-r-scan',[dict(frame=2,action='refresh')],1)
    focused=run('dpad-footer-reachable',[dict(frame=2,action='hover',x=2734,y=1800),dict(frame=4,action='down')])
    assert focused['focus']=='refresh-items',focused['focus']
    filtered=run('review-filter',[dict(frame=2,action='refresh'),dict(frame=55,action='click',x=1760,y=1710)],1)
    assert filtered['inventory_scan']['review_filter']
    typed=run('r-remains-search-text',[dict(frame=2,action='text',text='r'),dict(frame=4,action='refresh')],view='library')
    assert typed['filter']=='r'
    run('footer-click-720',[dict(frame=2,action='click',x=1100,y=671)],1,width=1280,height=720)

    # Prepare A and B ahead of time. A is loaded before the first GL diagnostic;
    # only the atomic pointer changes afterward. No timing-based startup guess.
    root=args.out/'synthetic-owner-import'
    data,images,_,_=prep.stage_bundle(args.bundle,root)
    data=copy.deepcopy(data);data['source']=dict(kind='import',label='Synthetic owner-import scan fixture');data['device']={}
    for row in data['apps']:row['inventory_confirmed']=False
    a=prep.commit(root,data,images,(root/'current.json').read_bytes());pointer_a=(root/'current.json').read_bytes()
    updated=copy.deepcopy(a[0]);updated['apps'].append(prep.app_metadata(dict(id='com.example.scan.fixture',title='Synthetic new item',version='1'),data['captured_at'],False,'owner-import'))
    prep.commit(root,updated,a[1],pointer_a);pointer_b=(root/'current.json').read_bytes()
    def dynamic(name,extra=(),failure=False):
        temp=root/'pointer-a.tmp';temp.write_bytes(pointer_a);os.replace(temp,root/'current.json')
        cmd=command(name,[dict(frame=45,action='refresh'),*extra],bundle=root,width=1280,height=720,frames=210 if failure else 105,fixed=False)
        master,slave=pty.openpty()
        try:process=subprocess.Popen(cmd,env=env,stdout=slave,stderr=slave,text=True,bufsize=1)
        except BaseException:os.close(master);raise
        finally:os.close(slave)
        lines=[];ready=queue.Queue()
        def read():
            with os.fdopen(master,'r',encoding='utf8',errors='replace') as stream:
                try:
                    for line in stream:
                        lines.append(line)
                        if 'OpenGL' in line:ready.put(line)
                except OSError as error:
                    if error.errno!=errno.EIO:raise
        thread=threading.Thread(target=read,daemon=True);thread.start()
        try:
            ready.get(timeout=15) # load_prepared completed before SDL/GL diagnostics.
            if failure:os.replace(root/'current.json',root/'held-current.json')
            else:
                temp=root/'pointer-b.tmp';temp.write_bytes(pointer_b);os.replace(temp,root/'current.json')
            result=process.wait(timeout=30)
        finally:
            if process.poll() is None:process.kill();process.wait(timeout=3)
            thread.join(timeout=3);(args.out/(name+'.log')).write_text(''.join(lines),encoding='utf8')
        assert result==0,(name,lines[-8:])
        return inspect(name,1)

    fresh=dynamic('synthetic-new-item')
    assert fresh['inventory_scan']['new_ids']==['com.example.scan.fixture'],fresh['inventory_scan']
    assert fresh['focus']=='app:com.example.scan.fixture'
    assert fresh['selected_app']=='com.example.scan.fixture'
    assert fresh['prepared_data']['generation']!=json.loads(pointer_b)['manifest'].split('/')[1]
    pinned=dynamic('new-item-pin',[dict(frame=76,action='click',x=960,y=573)])
    assert pinned['pin_target']=='com.example.scan.fixture'
    saved=json.loads((args.out/'new-item-pin-settings.json').read_text())
    assert saved['favorites']==['com.example.scan.fixture']
    paged=dynamic('page-pin-target',[dict(frame=65,action='click',x=278,y=573),dict(frame=80,action='click',x=960,y=573)])
    expected=data['apps'][9]['id']
    assert paged['pin_target']==expected,(paged['pin_target'],expected)
    assert json.loads((args.out/'page-pin-target-settings.json').read_text())['favorites']==[expected]
    reviewed=dynamic('filter-pin-target',[dict(frame=65,action='click',x=590,y=573),dict(frame=80,action='click',x=960,y=573)])
    assert reviewed['inventory_scan']['review_filter']
    assert reviewed['pin_target'] in [c['id'] for c in reviewed['inventory_scan']['candidates']]
    assert json.loads((args.out/'filter-pin-target-settings.json').read_text())['favorites']==[reviewed['pin_target']]
    navigated=dynamic('scan-keeps-user-focus',[dict(frame=45,action='right')])
    assert navigated['inventory_scan']['new_count']==1
    assert navigated['focus']=='app:'+data['apps'][1]['id'],navigated['focus']
    assert navigated['selected_app']==data['apps'][1]['id']
    failed=dynamic('scan-failure-persists',failure=True)
    assert failed['inventory_notice'].startswith('Scan unavailable:') and 'previous data:' in failed['inventory_notice']
    long_root=args.out/'long-identity-fixture'
    long_root.mkdir()
    long_id='com.example.'+'w'*116
    long_version='W'*64
    row=prep.app_metadata(dict(id=long_id,title='Long identity fixture',version=long_version),data['captured_at'],False,'owner-import')
    prep.commit(long_root,dict(schema=prep.BUNDLE_SCHEMA,captured_at=data['captured_at'],source=dict(kind='import'),device={},apps=[row]),{},None)
    identity=run('full-selected-identity',[],view='library',bundle=long_root)
    joined=''.join(identity['selected_identity'])
    assert long_id in joined and long_version in joined and len(identity['selected_identity'])<=3
    summary=dict(scope='Native desktop scan QA; real C3 snapshot plus explicitly synthetic owner-import delta; no TV calls or uninstall actions',passed=len(cases),failed=0,tests=cases,
                 real_candidate_count=scan['inventory_scan']['candidate_count'],synthetic_new_item_passed=True,tv_app_tested=False)
    (args.out/'inventory-native-qa.json').write_text(json.dumps(summary,indent=2)+'\n')
    print('RESULT '+str(len(cases))+' passed / 0 failed',flush=True)
if __name__=='__main__':main()
