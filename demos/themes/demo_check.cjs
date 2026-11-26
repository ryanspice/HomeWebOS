'use strict';
// Offline interaction smoke of the standalone HTML; not a native renderer test.
const fs = require('fs');
const path = require('path');
const vm = require('vm');
const assert = require('assert');
const html = fs.readFileSync(path.join(__dirname, 'demo.html'), 'utf8');
const refs = [...html.matchAll(/(?:src="|url\(')(geometric\/[^"')]+\.png)/g)].map(m => m[1]);
assert(refs.length >= 8);
for (const ref of refs) assert(fs.statSync(path.join(__dirname, ref)).isFile(), ref);
const buttons = [...html.matchAll(/<button aria-pressed="(true|false)" data-slot="([^"]+)">/g)].map(m => ({
  dataset: {slot: m[2]}, attributes: {'aria-pressed': m[1]},
  setAttribute(k, v) { this.attributes[k] = v; },
  addEventListener(k, callback) { assert.equal(k, 'click'); this.click = callback; }
}));
assert.equal(buttons.length, 6);
const label = {textContent: 'navigation.search'};
const document = {
  querySelectorAll(selector) { assert.equal(selector, '.rail button'); return buttons; },
  querySelector(selector) { assert.equal(selector, '#slot'); return label; }
};
const script = html.match(/<script>([\s\S]*?)<\/script>/)[1];
vm.runInNewContext(script, {document}, {timeout: 1000});
for (const button of buttons) {
  button.click();
  assert.equal(label.textContent, button.dataset.slot);
  assert.equal(buttons.filter(b => b.attributes['aria-pressed'] === 'true').length, 1);
  assert.equal(button.attributes['aria-pressed'], 'true');
}
console.log('PASS standalone demo: artwork references and six semantic-icon interactions');
