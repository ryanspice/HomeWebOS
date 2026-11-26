import importlib.util
import json
from pathlib import Path
import tempfile
import unittest
spec=importlib.util.spec_from_file_location('adapt',Path(__file__).resolve().parents[1]/'tools/agent/adapt.py')
a=importlib.util.module_from_spec(spec);spec.loader.exec_module(a)
class AdapterTests(unittest.TestCase):
    def setUp(self):
        self.tmp=tempfile.TemporaryDirectory();self.addCleanup(self.tmp.cleanup);self.root=Path(self.tmp.name)
    def source(self, data):
        p=self.root/'source.json';p.write_text(json.dumps(data));return p
    def test_new_file_only(self):
        p=self.source({});
        with self.assertRaises(ValueError):a.migrate(p,p)
        self.assertEqual(p.read_text(),'{}')
    def test_credentials_and_handoff_not_copied(self):
        p=self.source({'token':'secret','retropie_token':'secret','unknown':{'password':'secret'},'handoff_on_dwell':True,'favorites':['com.test','bad;id']})
        r=a.migrate(p,self.root/'new.json');self.assertNotIn('secret',json.dumps(r));self.assertFalse(r['handoff_on_dwell']);self.assertEqual(r['favorites'],['com.test'])
    def test_bounds(self):
        p=self.source({'retropie_host':'8.8.8.8','retropie_port':2,'retropie_hdmi':9,'preview_dwell_seconds':999,'theme':'bogus'})
        r=a.migrate(p,self.root/'new.json');self.assertEqual((r['retropie_host'],r['retropie_port'],r['retropie_hdmi'],r['preview_dwell_seconds'],r['theme']),('',8767,1,8,'ps5'))
    def test_expected_fields(self):
        p=self.source({'retropie_host':'192.168.1.12','theme':'metro','processor_app_id':'com.task.manager','remote_scancodes':{'F5':'ok','F6':'shell'}})
        r=a.migrate(p,self.root/'new.json');self.assertEqual(r['theme'],'metro');self.assertEqual(r['remote_scancodes'],{'F5':'ok'})
    def test_inspection_is_read_only(self):
        t=self.root/'repo';t.mkdir();(t/'Cargo.toml').write_text('[package]\nname="spice"');before=(t/'Cargo.toml').read_bytes()
        r=a.inspect(t,self.root/'report');self.assertEqual(r['architecture'],'rust');self.assertEqual((t/'Cargo.toml').read_bytes(),before);self.assertEqual(len(list(t.iterdir())),1)
    def test_report_inside_target_denied(self):
        t=self.root/'repo';t.mkdir()
        with self.assertRaises(ValueError):a.inspect(t,t/'out')
    def test_existing_report_denied(self):
        t=self.root/'repo';t.mkdir();out=self.root/'out';out.mkdir()
        with self.assertRaises(ValueError):a.inspect(t,out)
    def test_oversize_input_denied(self):
        p=self.source({'large':'x'*131073})
        with self.assertRaises(ValueError):a.migrate(p,self.root/'new.json')
    def test_nonfinite_json_denied(self):
        p=self.root/'source.json';p.write_text('{"x":NaN}')
        with self.assertRaises(ValueError):a.migrate(p,self.root/'new.json')
    def test_theme_type_confusion(self):
        p=self.source({'theme':[]});r=a.migrate(p,self.root/'new.json');self.assertEqual(r['theme'],'ps5')
    def test_symlink_input_denied(self):
        p=self.source({});link=self.root/'link.json'
        try:link.symlink_to(p)
        except OSError:self.skipTest('Symlinks not available')
        with self.assertRaises(ValueError):a.migrate(link,self.root/'new.json')
    def test_ip_validation(self):
        self.assertTrue(a.private_ipv4('172.16.0.1'));self.assertFalse(a.private_ipv4('127.0.0.1'));self.assertFalse(a.private_ipv4('192.168.01.2'))
if __name__=='__main__':unittest.main()
