#!/usr/bin/env python3
"""Comparable native captures and counters; fixed-step QA, not physical refresh proof."""
import argparse,json,os,pathlib,subprocess,tempfile
parser=argparse.ArgumentParser();parser.add_argument('--binary',type=pathlib.Path,required=True);parser.add_argument('--baseline',type=pathlib.Path,required=True);parser.add_argument('--out',type=pathlib.Path,required=True)
args=parser.parse_args();args.out.mkdir(parents=True,exist_ok=True)
env=dict(os.environ,SPICE_CPP_PRIVATE_LUNA='0',SPICE_RETROPIE_TOKEN='')
cases=[('home','home',[]),('catalog','designs',[]),('inspector','designs',[dict(frame=0,action='ok')]),
       ('keyboard','designs',[dict(frame=0,action='click',x=3160,y=484),dict(frame=2,action='text',text='orbit')]),
       ('control','control',[]),('settings','settings',[])]
modes=[('baseline',args.baseline,[]),('immediate',args.binary,[]),('batched',args.binary,['--batch-renderer']),('legacy-backdrop',args.binary,['--legacy-background'])]
rows=[]
with tempfile.TemporaryDirectory(prefix='spice-visual-matrix-') as tmp:
 for mode,binary,flags in modes:
  for name,view,actions in cases:
   folder=args.out/mode;folder.mkdir(exist_ok=True)
   source=folder/(name+'.input.json');source.write_text(json.dumps(actions)+'\n')
   report=folder/(name+'.json');image=folder/(name+'.png')
   command=[str(binary.resolve()),*flags,'--assets',str(pathlib.Path('assets').resolve()),'--config',str(pathlib.Path(tmp)/(mode+'-'+name+'.json')),
     '--no-vsync','--fps','240','--width','3840','--height','2160','--frames','48','--step-ms',str(1000/60),'--view',view,
     '--script',str(source),'--report',str(report),'--screenshot',str(image)]
   result=subprocess.run(command,env=env,text=True,capture_output=True,timeout=90)
   (folder/(name+'.log')).write_text(result.stdout+result.stderr)
   assert result.returncode==0,(mode,name,result.stderr)
   data=json.loads(report.read_text());assert data['drawable_width']==3840 and data['drawable_height']==2160 and not data['tv_verified']
   assert data['view']==view and data['animation_clock']=='fixed-step-desktop-qa'
   perf=data.get('performance',{})
   row=dict(mode=mode,scene=name,frame_draws=data['draw_calls'],texture_bytes=data['texture_bytes'],
     renderer=data['gl_renderer'],clock=data['animation_clock'],performance=perf,report=str(report),capture=str(image))
   rows.append(row);print(json.dumps({k:row[k] for k in ('mode','scene','frame_draws','texture_bytes')}),flush=True)
summary=dict(scope='Actual native 4K captures/counters at fixed animation times; software renderer; not hardware throughput or physical presentation',
 tv_tested=False,physical_240hz_verified=False,rows=rows)
(args.out/'matrix.json').write_text(json.dumps(summary,indent=2)+'\n')
print('RESULT 24 native 4K scene/path captures completed',flush=True)
