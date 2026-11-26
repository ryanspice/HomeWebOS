#!/usr/bin/env python3
"""Opt-in, loopback/LAN-only RetroPie companion for PS5 System Deck.

Python standard library only. No auto-install, root, arbitrary command or ROM download.
Default is read-only. Set RETROPIE_ALLOW_LAUNCH=1 only if you configured a
local emulationstation.service unit yourself. Requires a random token.
"""
import hmac
import json
import os
import pathlib
import re
import socket
import subprocess
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer

HOST = os.environ.get('RETROPIE_BIND', '127.0.0.1')
PORT = int(os.environ.get('RETROPIE_PORT', '8767'))
TOKEN = os.environ.get('RETROPIE_TOKEN', '')
ROMS = pathlib.Path(os.path.expanduser('~/RetroPie/roms'))
ALLOW_LAUNCH = os.environ.get('RETROPIE_ALLOW_LAUNCH') == '1'
SYSTEM_NAME = re.compile(r'^[a-zA-Z0-9_-]{1,35}$')


def read_cpu():
    try:
        text = pathlib.Path('/proc/stat').read_text().splitlines()[0].split()[1:]
        vals = list(map(int, text))
        # Boot-to-date CPU utilization is only a status approximation.
        return round(100 * (sum(vals) - vals[3] - (vals[4] if len(vals)>4 else 0)) / max(1, sum(vals)), 1)
    except (OSError, ValueError, IndexError):
        return 0


def read_memory():
    try:
        mem = dict((k, int(v.split()[0])) for k,v in (x.split(':',1) for x in
            pathlib.Path('/proc/meminfo').read_text().splitlines() if ':' in x))
        return round(100 * (1-mem.get('MemAvailable',0)/max(1,mem.get('MemTotal',1))),1)
    except (OSError, ValueError):
        return 0


def get_systems():
    try:
        return sorted(p.name for p in ROMS.iterdir() if p.is_dir() and SYSTEM_NAME.fullmatch(p.name))[:24]
    except OSError:
        return []


def es_running():
    try:
        proc = subprocess.run(['pgrep','-x','emulationstation'], capture_output=True, timeout=2,check=False)
        return proc.returncode == 0
    except (OSError, subprocess.TimeoutExpired):
        return False


class Handler(BaseHTTPRequestHandler):
    def _json(self, status, data):
        body=json.dumps(data, separators=(',',':')).encode()
        self.send_response(status)
        self.send_header('Content-Type','application/json')
        self.send_header('Cache-Control','no-store')
        if self.headers.get('Origin') == 'null':
            self.send_header('Access-Control-Allow-Origin', 'null')
            self.send_header('Vary', 'Origin')
        self.send_header('Content-Length',str(len(body)))
        self.end_headers()
        self.wfile.write(body)

    def do_OPTIONS(self):
        # Local-file webOS apps send Origin: null. A long random bearer token is
        # mandatory; never use Access-Control-Allow-Origin: * for this service.
        if self.headers.get('Origin') != 'null':
            self.send_response(403)
            self.end_headers()
            return
        self.send_response(204)
        self.send_header('Access-Control-Allow-Origin', 'null')
        self.send_header('Access-Control-Allow-Methods', 'GET, OPTIONS')
        self.send_header('Access-Control-Allow-Headers', 'Authorization, Accept')
        self.send_header('Access-Control-Max-Age', '600')
        self.send_header('Vary', 'Origin')
        self.end_headers()

    def _allowed(self):
        auth = self.headers.get('Authorization','')
        return hmac.compare_digest(auth, 'Bearer '+TOKEN)

    def do_GET(self):
        if not self._allowed():
            return self._json(401, {'error':'unauthorized'})
        if self.path == '/v1/status':
            return self._json(200, {'hostname':socket.gethostname()[:80], 'systems':get_systems(),
                'cpu_percent':read_cpu(),'memory_percent':read_memory(),
                'emulationstation_running':es_running(),'api_version':1})
        return self._json(404,{'error':'unknown endpoint'})

    def do_POST(self):
        if not self._allowed():
            return self._json(401, {'error':'unauthorized'})
        if self.path != '/v1/actions/launch':
            return self._json(404,{'error':'unknown endpoint'})
        try:
            length = int(self.headers.get('Content-Length','0'))
        except ValueError:
            return self._json(400,{'error':'invalid content length'})
        if length < 0:
            return self._json(400,{'error':'invalid content length'})
        if length > 256:
            return self._json(413,{'error':'request too large'})
        self.rfile.read(length)
        if not ALLOW_LAUNCH:
            return self._json(403,{'error':'remote launching not enabled by Pi owner'})
        try:
            # Fixed allowlisted command, no caller-controlled arguments. Must exist as a user service.
            r = subprocess.run(['systemctl','--user','start','emulationstation.service'],timeout=5,check=False)
            return self._json(200 if r.returncode==0 else 503, {'started':r.returncode==0})
        except (OSError,subprocess.TimeoutExpired):
            return self._json(503,{'error':'service unavailable'})

    def log_message(self, format, *args):
        # Do not log URL tokens or Authorization headers.
        print('%s %s' % (self.address_string(), format % args))


if __name__=='__main__':
    if len(TOKEN) < 24:
        raise SystemExit('Set RETROPIE_TOKEN to a random 24+ character secret; refusing to start.')
    if PORT < 1024 or PORT > 65535:
        raise SystemExit('Invalid port')
    print('RetroPie companion: status-only' if not ALLOW_LAUNCH else 'RetroPie companion: launch enabled')
    print(f'Listening on {HOST}:{PORT} (LAN HTTP: use trusted private network only)')
    ThreadingHTTPServer((HOST,PORT), Handler).serve_forever()
