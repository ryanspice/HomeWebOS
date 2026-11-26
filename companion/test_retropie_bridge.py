"""Local-only companion tests; no external network or RetroPie dependency."""
import http.client,importlib.util,pathlib,tempfile,threading,unittest,json
from http.server import ThreadingHTTPServer
MODULE=pathlib.Path(__file__).resolve().parents[1]/'companion'/'retropie_bridge.py'
spec=importlib.util.spec_from_file_location('retro_bridge_test',MODULE)
bridge=importlib.util.module_from_spec(spec);spec.loader.exec_module(bridge)
class CompanionTests(unittest.TestCase):
 @classmethod
 def setUpClass(cls):
  cls.tmp=tempfile.TemporaryDirectory();bridge.ROMS=pathlib.Path(cls.tmp.name)
  (bridge.ROMS/'nes').mkdir();bridge.TOKEN='synthetic-test-token-long-enough'
  bridge.ALLOW_LAUNCH=False
  cls.server=ThreadingHTTPServer(('127.0.0.1',0),bridge.Handler)
  cls.thread=threading.Thread(target=cls.server.serve_forever,daemon=True);cls.thread.start()
 @classmethod
 def tearDownClass(cls):
  cls.server.shutdown();cls.server.server_close();cls.thread.join(timeout=2);cls.tmp.cleanup()
 def request(self,method,path,origin=None,token=None):
  c=http.client.HTTPConnection('127.0.0.1',self.server.server_port,timeout=3)
  headers={};
  if origin is not None:headers['Origin']=origin
  if token is not None:headers['Authorization']='Bearer '+token
  c.request(method,path,headers=headers);r=c.getresponse();status=r.status;headers=r.getheaders();body=r.read();c.close();return status,dict(headers),body
 def test_missing_token_refused(self):
  status,_,_=self.request('GET','/v1/status');self.assertEqual(status,401)
 def test_authenticated_status(self):
  status,_,body=self.request('GET','/v1/status',token=bridge.TOKEN);self.assertEqual(status,200)
  data=json.loads(body);self.assertEqual(data['systems'],['nes'])
 def test_file_origin_preflight(self):
  status,headers,_=self.request('OPTIONS','/v1/status',origin='null');self.assertEqual(status,204)
  self.assertEqual(headers['Access-Control-Allow-Origin'],'null')
  self.assertIn('Authorization',headers['Access-Control-Allow-Headers'])
 def test_remote_origins_not_preflight_allowed(self):
  status,_,_=self.request('OPTIONS','/v1/status',origin='https://evil.example');self.assertEqual(status,403)
 def test_origin_null_response_and_auth(self):
  status,headers,_=self.request('GET','/v1/status',origin='null',token=bridge.TOKEN)
  self.assertEqual(status,200);self.assertEqual(headers['Access-Control-Allow-Origin'],'null')
 def test_remote_launch_disabled(self):
  status,_,_=self.request('POST','/v1/actions/launch',token=bridge.TOKEN);self.assertEqual(status,403)
if __name__=='__main__':unittest.main()
