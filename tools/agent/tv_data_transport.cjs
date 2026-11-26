#!/usr/bin/env node
'use strict';
// Read-only collection over the existing LG CLI's ssh2 dependency. No CLI/profile
// initializer, pairing, privilege change, remote write, arbitrary URI or shell API.
const fs = require('node:fs');
const path = require('node:path');
const os = require('node:os');
const readline = require('node:readline');
const crypto = require('node:crypto');
const { createRequire } = require('node:module');

const appRoots = ['/usr/palm/applications/', '/media/cryptofs/apps/usr/palm/applications/', '/media/developer/apps/usr/palm/applications/'];
const artRoots = [...appRoots, '/usr/share/icons/', '/usr/palm/frameworks/'];
const safeId = s => typeof s === 'string' && /^[a-zA-Z0-9][a-zA-Z0-9._-]{1,127}$/.test(s);
const quote = s => "'" + s.replaceAll("'", "'\\''") + "'";
const reply = value => process.stdout.write(JSON.stringify(value) + '\n');
const fault = code => Object.assign(new Error(code), { safeCode: code });
let client, input, active, configHash, configPath;
let closing = false;
function close() { closing = true; if (input) input.close(); if (client) client.end(); }
function configUnchanged() {
  return configPath && crypto.createHash('sha256').update(fs.readFileSync(configPath)).digest('hex') === configHash;
}
function parseArgs(argv) {
  const result = { privateReads: false, 'luna-client': 'public' };
  for (let i = 0; i < argv.length; ++i) {
    if (argv[i] === '--private-reads') result.privateReads = true;
    else if (['--device', '--cli-root', '--luna-client'].includes(argv[i]) && i + 1 < argv.length) result[argv[i].slice(2)] = argv[++i];
    else throw fault('invalid-transport-arguments');
  }
  if (!safeId(result.device)) throw fault('explicit-device-required');
  if (!['public', 'private'].includes(result['luna-client']) || result['luna-client'] === 'private' && !result.privateReads) throw fault('invalid-luna-client-scope');
  return result;
}
function cliRoot(explicit) {
  const candidates = [explicit, process.env.APPDATA && path.join(process.env.APPDATA, 'npm/node_modules/@webos-tools/cli'),
    path.join(os.homedir(), '.npm-global/lib/node_modules/@webos-tools/cli'), '/usr/local/lib/node_modules/@webos-tools/cli'];
  for (const candidate of candidates.filter(Boolean)) if (fs.existsSync(path.join(candidate, 'files/conf/config.json'))) return fs.realpathSync(candidate);
  throw fault('installed-webos-cli-not-found');
}
function existingDevice(root, name) {
  const defaults = JSON.parse(fs.readFileSync(path.join(root, 'files/conf/config.json'), 'utf8'));
  configPath = path.join(process.env.APPDATA || process.env.HOME || process.env.USERPROFILE, defaults.dataDir || '.webos', 'novacom-devices.json');
  const bytes = fs.readFileSync(configPath);
  configHash = crypto.createHash('sha256').update(bytes).digest('hex');
  const list = JSON.parse(bytes);
  const found = list.filter(d => d.name === name);
  if (found.length !== 1) throw fault('configured-device-not-found');
  const device = found[0];
  const host = device.host || device.ip;
  if (!host || !Number.isInteger(Number(device.port)) || !device.username) throw fault('unsupported-device-record');
  const options = { host, port: Number(device.port), username: device.username, readyTimeout: 8000, keepaliveInterval: 5000 };
  if (typeof device.privateKey === 'string') options.privateKey = Buffer.from(device.privateKey, 'base64');
  else if (device.privateKey && typeof device.privateKey.openSsh === 'string') {
    const keyName = device.privateKey.openSsh;
    if (path.basename(keyName) !== keyName) throw fault('unsupported-key-reference');
    options.privateKey = fs.readFileSync(path.join(os.homedir(), '.ssh', keyName));
  }
  if (typeof device.password === 'string') options.password = device.password;
  if (typeof device.passphrase === 'string') options.passphrase = device.passphrase;
  if (!options.privateKey && options.password === undefined) throw fault('configured-authentication-unavailable');
  return options;
}
function exec(command, timeoutMs = 7000, sizeLimit = 1024 * 1024) {
  return new Promise((resolve, reject) => client.exec(command, (err, stream) => {
    if (err) return reject(fault('remote-command-unavailable'));
    let size = 0, chunks = [], settled = false;
    const timer = setTimeout(() => finish(fault('remote-request-timeout')), timeoutMs);
    function finish(error) {
      if (settled) return;
      settled = true; clearTimeout(timer);
      if (error) { stream.close(); reject(error); }
      else resolve(Buffer.concat(chunks).toString('utf8'));
    }
    stream.on('data', chunk => { size += chunk.length; if (size > sizeLimit) return finish(fault('remote-response-too-large')); chunks.push(chunk); });
    stream.stderr.on('data', () => {}); // Never export raw provider/auth/service errors.
    stream.on('error', () => finish(fault('remote-command-failed')));
    stream.on('close', code => finish(code === 0 ? null : fault('remote-command-denied-or-failed')));
  }));
}
function sftpCall(sftp, method, ...args) {
  return new Promise((resolve, reject) => sftp[method](...args, (error, value) => error ? reject(fault('remote-file-unavailable')) : resolve(value)));
}
async function readFile(sftp, requested) {
  if (typeof requested !== 'string' || requested.length > 1024 || requested.includes('\0') || requested.includes('\\') || requested.split('/').includes('..')) throw fault('file-path-denied');
  const real = await sftpCall(sftp, 'realpath', requested);
  const isInfo = path.posix.basename(real) === 'appinfo.json';
  const roots = isInfo ? appRoots : artRoots;
  if (!roots.some(root => real.startsWith(root)) || (!isInfo && !/\.(png|jpe?g|webp)$/i.test(real))) throw fault('file-path-denied');
  const stat = await sftpCall(sftp, 'stat', real);
  const limit = isInfo ? 128 * 1024 : 8 * 1024 * 1024;
  if (!stat.isFile() || stat.size < 1 || stat.size > limit) throw fault('remote-file-size-denied');
  const handle = await sftpCall(sftp, 'open', real, 'r');
  const chunks = []; let offset = 0;
  try {
    while (offset <= limit) {
      const buffer = Buffer.alloc(Math.min(65536, limit + 1 - offset));
      if (!buffer.length) throw fault('remote-file-grew-too-large');
      const count = await new Promise((resolve, reject) => sftp.read(handle, buffer, 0, buffer.length, offset, (error, n) => error ? reject(fault('remote-file-read-failed')) : resolve(n)));
      if (!count) break;
      offset += count; chunks.push(buffer.subarray(0, count));
    }
  } finally { await sftpCall(sftp, 'close', handle); }
  if (offset > limit) throw fault('remote-file-grew-too-large');
  return { path: real, bytes: offset, base64: Buffer.concat(chunks).toString('base64') };
}
// Mounted /usr/palm and the device font/store roots are invisible to the
// prisoner SFTP user, so these reads go through the on-device root shim.
const ROOT_PREFIXES = ['/usr/palm/applications/com.webos.app.', '/usr/share/fonts/', '/media/internal/', '/var/db/', '/media/cryptofs/apps/usr/palm/applications/', '/media/developer/apps/usr/palm/applications/', '/media/system/apps/usr/palm/applications/', '/mnt/otncabi/usr/palm/applications/'];
function rootPathOk(value) {
  return typeof value === 'string' && value.length <= 1024 && value.startsWith('/') &&
    !value.includes('\0') && !value.includes('\\') && !value.split('/').includes('..') &&
    ROOT_PREFIXES.some(prefix => value.startsWith(prefix));
}
const ROOT_READ_SCRIPT = `import os,json,base64,hashlib,sys
os.setns(os.open('/proc/1/ns/mnt',os.O_RDONLY),0);os.setuid(0)
cap=int(sys.argv[1]);out={};total=0
for p in sys.argv[2:]:
    try:
        b=open(p,'rb').read()
    except Exception:
        out[p]={'error':'unreadable'};continue
    if total+len(b)>cap:
        out[p]={'error':'over-budget'};continue
    total+=len(b)
    out[p]={'bytes':len(b),'sha256':hashlib.sha256(b).hexdigest(),'base64':base64.b64encode(b).decode()}
print(json.dumps(out))`;
async function rootRead(paths, limit) {  if (!Array.isArray(paths) || paths.length < 1 || paths.length > 8) throw fault('invalid-request');
  if (!paths.every(rootPathOk)) throw fault('file-path-denied');
  const budget = Math.min(Number(limit) || 500000, 2 * 1024 * 1024);
  const raw = await exec('/dev/shm/rootpy -E -S -c ' + quote(ROOT_READ_SCRIPT) + ' ' + budget + ' ' + paths.map(quote).join(' '), 25000, 2 * 1024 * 1024);
  const start = raw.indexOf('{');
  if (start < 0) throw fault('remote-json-unavailable');
  let files;
  try { files = JSON.parse(raw.slice(start)); } catch (_) { throw fault('remote-json-invalid'); }
  const total = Object.values(files).reduce((sum, entry) => sum + (entry.bytes || 0), 0);
  return { files, total_bytes: total };
}
const ROOT_LUNA = new Set([
  'luna://com.webos.applicationManager/listLaunchPoints',
  'luna://com.webos.service.tv.systemproperty/getSystemInfo',
]);
const ROOT_LUNA_SCRIPT = `import os,json,subprocess,sys
os.setns(os.open('/proc/1/ns/mnt',os.O_RDONLY),0);os.setuid(0)
p=subprocess.run(['luna-send','-n','1','-f',sys.argv[1],sys.argv[2]],capture_output=True,text=True,timeout=20)
print(p.stdout)`;
async function rootLuna(uri, body) {
  if (!ROOT_LUNA.has(uri)) throw fault('luna-uri-denied');
  const payload = JSON.stringify(body && typeof body === 'object' ? body : {});
  if (payload.length > 1024) throw fault('request-too-large');
  const raw = await exec('/dev/shm/rootpy -E -S -c ' + quote(ROOT_LUNA_SCRIPT) + ' ' + quote(uri) + ' ' + quote(payload), 30000, 2 * 1024 * 1024);
  const start = raw.indexOf('{');
  if (start < 0) throw fault('remote-json-unavailable');
  let json;
  try { json = JSON.parse(raw.slice(start)); } catch (_) { throw fault('remote-json-invalid'); }
  if (json.returnValue !== true) throw fault('service-denied-or-unavailable');
  return json;
}
async function request(req, sftp, privateReads, lunaClient) {  if (!req || !safeId(req.request_id)) throw fault('invalid-request');
  if (req.op === 'file') return readFile(sftp, req.path);
  if (req.op === 'rootread') return rootRead(req.paths, req.limit);
  if (req.op === 'rootluna') return rootLuna(req.uri, req.body);
  if (req.op === 'appdirs') {
    const paths = [];
    for (const root of appRoots) {
      try { for (const entry of (await sftpCall(sftp, 'readdir', root)).slice(0, 512)) if (safeId(entry.filename) && entry.attrs.isDirectory()) paths.push(root + entry.filename + '/appinfo.json'); }
      catch (_) {} // Each fixed package root is optional/permission dependent.
    }
    return { paths: paths.slice(0, 400) };
  }
  const operations = {
    device: ['luna://com.webos.service.tv.systemproperty/getSystemInfo', { keys: ['modelName', 'firmwareVersion', 'sdkVersion', 'UHD'] }, false],
    launchpoints: ['luna://com.webos.applicationManager/listLaunchPoints', {}, true],
    // The installed TV CLI uses dev/listApps for ares-install --listfull.
    apps: [lunaClient === 'private' ? 'luna://com.webos.applicationManager/listApps' : 'luna://com.webos.applicationManager/dev/listApps', {}, lunaClient === 'private'],
    appinfo: ['luna://com.webos.applicationManager/getAppInfo', { id: req.app_id }, true],
    status: ['luna://com.webos.applicationManager/getAppLoadStatus', { id: req.app_id }, false]
  };
  if (!Object.hasOwn(operations, req.op)) throw fault('operation-denied');
  if (['appinfo', 'status'].includes(req.op) && !safeId(req.app_id)) throw fault('invalid-app-id');
  const [uri, body, isPrivate] = operations[req.op];
  if (isPrivate && !privateReads) throw fault('private-read-not-enabled');
  const raw = await exec((isPrivate && lunaClient === 'private' ? 'luna-send' : 'luna-send-pub') + ' -n 1 -f ' + quote(uri) + ' ' + quote(JSON.stringify(body)));
  const start = raw.indexOf('{');
  if (start < 0) throw fault('remote-json-unavailable');
  let json; try { json = JSON.parse(raw.slice(start)); } catch (_) { throw fault('remote-json-invalid'); }
  if (json.returnValue !== true) throw fault('service-denied-or-unavailable');
  return json;
}
async function main() {
  const args = parseArgs(process.argv.slice(2));
  const root = cliRoot(args['cli-root']);
  const requireCli = createRequire(path.join(root, 'package.json'));
  const { Client } = requireCli('ssh2');
  client = new Client();
  client.on('error', () => { if (!closing) { reply({ fatal: 'configured-connection-failed' }); close(); } });
  await new Promise((resolve, reject) => { client.once('ready', resolve); client.once('error', () => reject(fault('configured-connection-failed'))); client.connect(existingDevice(root, args.device)); });
  const sftp = await new Promise((resolve, reject) => client.sftp((err, value) => err ? reject(fault('sftp-unavailable')) : resolve(value)));
  reply({ ready: true, device: args.device, device_configuration_sha256: configHash, remote_writes: false });
  input = readline.createInterface({ input: process.stdin, crlfDelay: Infinity });
  for await (const line of input) {
    if (line.length > 4096) throw fault('request-too-large');
    let req;
    try { req = JSON.parse(line); } catch (_) { reply({ ok: false, code: 'invalid-json' }); continue; }
    if (req.op === 'close') { reply({ ok: true, configuration_unchanged: configUnchanged() }); break; }
    try {
      active = setTimeout(() => { reply({ fatal: 'operation-deadline' }); close(); }, ['rootread', 'rootluna'].includes(req.op) ? 30000 : 10000);
      const result = await request(req, sftp, args.privateReads, args['luna-client']);
      clearTimeout(active); reply({ request_id: req.request_id, ok: true, data: result });
    } catch (error) { clearTimeout(active); reply({ request_id: req.request_id, ok: false, code: error.safeCode || 'read-unavailable' }); }
  }
  close();
}
if (require.main === module) main().catch(error => { reply({ fatal: error.safeCode || 'transport-unavailable' }); close(); process.exitCode = 1; });
module.exports = { parseArgs, request, safeId };
