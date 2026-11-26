'use strict';
const assert = require('node:assert/strict');
const { parseArgs, request, safeId } = require('../tools/agent/tv_data_transport.cjs');
let checks = 0;
function check(name, fn) { fn(); ++checks; process.stdout.write('PASS ' + name + '\n'); }
async function rejected(name, req, sftp, code) {
  await assert.rejects(request(req, sftp, false, 'public'), error => error.safeCode === code);
  ++checks; process.stdout.write('PASS ' + name + '\n');
}
(async () => {
  check('explicit configured device is required', () => assert.throws(() => parseArgs([])));
  check('profile names cannot contain shell syntax', () => assert.throws(() => parseArgs(['--device', 'c3;id'])));
  check('common path defaults to public client', () => assert.equal(parseArgs(['--device', 'c3'])['luna-client'], 'public'));
  check('private client requires explicit read scope', () => assert.throws(() => parseArgs(['--device', 'c3', '--luna-client', 'private'])));
  check('private scope is explicit and has no fallback', () => assert.equal(parseArgs(['--device', 'c3', '--private-reads', '--luna-client', 'private'])['luna-client'], 'private'));
  check('app ids are bounded data', () => { assert(safeId('com.example.app')); assert(!safeId('com.example;reboot')); assert(!safeId('x'.repeat(129))); });
  await rejected('launch is absent from collection RPC', { request_id: 'test-id', op: 'launch' }, null, 'operation-denied');
  await rejected('install is absent from collection RPC', { request_id: 'test-id', op: 'install' }, null, 'operation-denied');
  await rejected('unknown/prototype operations denied', { request_id: 'test-id', op: 'toString' }, null, 'operation-denied');
  await rejected('private inventory blocked without opt-in', { request_id: 'test-id', op: 'launchpoints' }, null, 'private-read-not-enabled');
  await rejected('file traversal denied before SFTP', { request_id: 'test-id', op: 'file', path: '/usr/palm/applications/../secret.png' }, null, 'file-path-denied');
  let statCalled = false;
  const outside = { realpath: (_path, next) => next(null, '/private/secret.png'), stat: () => { statCalled = true; } };
  await rejected('resolved symlink escape denied before stat/read', { request_id: 'test-id', op: 'file', path: '/usr/palm/applications/com.example/icon.png' }, outside, 'file-path-denied');
  assert.equal(statCalled, false);
  const executable = { realpath: (_path, next) => next(null, '/usr/palm/applications/com.example/main.js'), stat: () => { statCalled = true; } };
  await rejected('only appinfo and supported graphics can be copied', { request_id: 'test-id', op: 'file', path: '/usr/palm/applications/com.example/main.js' }, executable, 'file-path-denied');
  assert.equal(statCalled, false);
  console.log('RESULT ' + checks + ' passed / 0 failed; provider/device requests=0');
})().catch(error => { console.error(error); process.exitCode = 1; });
