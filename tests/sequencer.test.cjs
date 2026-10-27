const { test } = require('node:test');
const assert = require('node:assert/strict');
const { EventEmitter } = require('node:events');
const fs = require('node:fs');
const vm = require('node:vm');

function createSequencer(get) {
  const source = fs.readFileSync('src/Sequencer.js', 'utf8')
    .replace(/^import .*;\n/gm, '')
    .replace(/export const /g, 'const ')
    .replace('export default class Sequencer', 'class Sequencer') + '\nthis.Sequencer = Sequencer;';
  const sandbox = { EventEmitter, autoBind() {}, shuffle: items => items,
    getUrlFromFilepath: path => path, axios: { get }, AbortController,
    setTimeout, clearTimeout, console, Uint8Array, fetch };
  vm.runInNewContext(source, sandbox);
  const loads = [];
  const player = { on() {}, canPlay: ext => ext === 'mid', suspend() {}, setLooping() {},
    loadData: async (_data, path) => loads.push(path), getVoiceMask: () => [] };
  const sequencer = new sandbox.Sequencer([player], { read: () => new Uint8Array([1]) }, () => ({}));
  return { sequencer, loads };
}

test('empty context leaves current playback intact and reports the problem', () => {
  const { sequencer } = createSequencer();
  let message;
  sequencer.on('playerError', value => { message = value; });
  sequencer.context = ['existing.mid'];
  sequencer.playContext([]);
  assert.equal(message, 'No songs found to play.');
  assert.equal(sequencer.context[0], 'existing.mid');
});

test('unsupported extension reports an error rather than dereferencing undefined', () => {
  const { sequencer } = createSequencer();
  let message;
  sequencer.on('playerError', value => { message = value; });
  sequencer.playSong('bad.xyz');
  assert.match(message, /\.xyz.*not recognized/);
});

test('a local selection aborts and ignores an earlier remote response', async () => {
  let resolveRequest, options;
  const { sequencer, loads } = createSequencer((_url, opts) => {
    options = opts;
    return new Promise(resolve => { resolveRequest = resolve; });
  });
  sequencer.playSong('remote.mid');
  const request = sequencer.songRequest;
  sequencer.playSong('local/new.mid');
  assert.equal(request.signal.aborted, true);
  assert.equal(typeof options.fetch, 'function');
  resolveRequest({ data: new Uint8Array([2]) });
  await new Promise(resolve => setImmediate(resolve));
  assert.deepEqual(loads, ['local/new.mid']);
  assert.equal(sequencer.currSongPath, 'local/new.mid');
});

test('a server outage does not advance through the queue', async () => {
  let requests = 0;
  const { sequencer } = createSequencer(() => {
    requests++;
    return Promise.reject({ status: 503 });
  });
  sequencer.on('playerError', () => {});
  sequencer.playContext(['one.mid', 'two.mid']);
  await new Promise(resolve => setTimeout(resolve, 1200));
  assert.equal(requests, 1);
  assert.equal(sequencer.currIdx, 0);
  assert.equal(sequencer.context.length, 2);
});
