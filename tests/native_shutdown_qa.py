#!/usr/bin/env python3
"""Verify SIGTERM drains a real native window and pending preferences on desktop."""
import argparse,json,os,pathlib,signal,subprocess,time
p=argparse.ArgumentParser()
for key in ('binary','assets','out'):p.add_argument('--'+key,type=pathlib.Path,required=True)
a=p.parse_args();a.out.mkdir(parents=True,exist_ok=False)
script=a.out/'input.json';script.write_text('[{"frame":20,"action":"motion"}]')
report=a.out/'report.json';config=a.out/'settings.json'
env=dict(os.environ,SPICE_CPP_PRIVATE_LUNA='0',SPICE_RETROPIE_TOKEN='')
child=subprocess.Popen([str(a.binary.resolve()),'--assets',str(a.assets.resolve()),'--config',str(config),'--report',str(report),'--script',str(script),'--frames','100000','--no-vsync','--fps','60'],env=env,stdout=subprocess.PIPE,stderr=subprocess.PIPE,text=True)
try:
    time.sleep(3)
    assert child.poll() is None,'Native process exited before termination check'
    child.send_signal(signal.SIGTERM)
    out,err=child.communicate(timeout=15)
    (a.out/'process.log').write_text(out+err)
    assert child.returncode==0,(child.returncode,out,err)
    value=json.loads(report.read_text())
    assert value['capture_frames']<100000 and value['settings_persistence']['shutdown_drained']
    assert value['settings_persistence']['state']=='saved'
    assert json.loads(config.read_text())['reduced_motion'] is True
    print('PASS SIGTERM exits native loop, drains changed preferences, and writes the final report')
finally:
    if child.poll() is None:child.kill();child.wait(timeout=5)
