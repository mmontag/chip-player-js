import { test } from 'node:test';
import assert from 'node:assert/strict';
import axios from 'redaxios';
import Player from '../src/players/Player';
import SIDPlayer from '../src/players/SIDPlayer';
import MDXPlayer from '../src/players/MDXPlayer';
import N64Player from '../src/players/N64Player';
import GSFPlayer from '../src/players/GSFPlayer';

test('a new load and suspension invalidate previous player work', () => {
  const player = new Player({}, 44100);
  const first = player.beginLoad();
  const second = player.beginLoad();
  assert.equal(first.aborted, true);
  assert.equal(second.aborted, false);
  player.suspend();
  assert.equal(second.aborted, true);
});

test('late SID metadata cannot overwrite a newer song or publish after stop', async () => {
  const pending = [];
  const originalGet = axios.get;
  axios.get = () => new Promise(resolve => pending.push(resolve));
  try {
    const core = { _malloc: () => 0, _sid_stop() {} };
    const player = new SIDPlayer(core, 44100);
    const updates = [];
    player.on('playerStateUpdate', state => updates.push(state));
    player.getBasePlayerState = () => ({ metadata: player.metadata });
    player.getSidMetadata('old', player.beginLoad());
    player.getSidMetadata('new', player.beginLoad());
    pending[1]({ status: 200, data: { lengths: '2:00', name: 'New', author: 'B', copyright: '2' } });
    await new Promise(resolve => setImmediate(resolve));
    pending[0]({ status: 200, data: { lengths: '1:00', name: 'Old', author: 'A', copyright: '1' } });
    await new Promise(resolve => setImmediate(resolve));
    assert.equal(player.metadata.formatted.title, 'New');
    assert.equal(player.subtuneDurations[0], 120000);
    assert.equal(updates.length, 1);
    player.getSidMetadata('stopped', player.beginLoad());
    player.stop();
    pending[2]({ status: 200, data: { lengths: '3:00', name: 'Stopped' } });
    await new Promise(resolve => setImmediate(resolve));
    assert.equal(player.metadata.formatted.title, 'New');
    assert.equal(updates.length, 2); // the second event is stop, not metadata
  } finally {
    axios.get = originalGet;
  }
});

test('stopping an MDX player while filesystem work is pending prevents opening the file', async () => {
  let finishSync;
  const calls = [];
  const core = {
    FS: { mkdirTree() {}, mount() {}, filesystems: { IDBFS: {} }, analyzePath: () => ({ exists: false }),
      writeFile() {}, syncfs(_populate, cb) { finishSync = cb; } },
    _malloc: () => 0, _mdx_create_context: () => 1, _mdx_set_rate() {}, _mdx_set_dir() {},
    _mdx_close() {}, ccall: name => { calls.push(name); return 0; },
  };
  const player = new MDXPlayer(core, 44100);
  const loading = player.loadData(new Uint8Array([1]), 'old.mdx', {});
  player.stop();
  finishSync(null);
  await loading;
  assert.deepEqual(calls, []);
});

for (const [PlayerClass, shutdown] of [[N64Player, '_n64_shutdown'], [GSFPlayer, '_gsf_shutdown']]) {
  test(`${PlayerClass.name} ignores a superseded filesystem load`, async () => {
    let finishSync;
    const calls = [];
    const core = {
      FS: { mkdirTree() {}, mount() {}, filesystems: { IDBFS: {} }, analyzePath: () => ({ exists: false }),
        writeFile() {}, syncfs(_populate, cb) { finishSync = cb; } },
      _malloc: () => 0, [shutdown]() {}, ccall: name => { calls.push(name); return 0; },
    };
    const player = new PlayerClass(core, 44100);
    const loading = player.loadData(new Uint8Array([1]), 'old', {});
    player.stop();
    finishSync(null);
    await loading;
    assert.deepEqual(calls, []);
  });
}

test('MDX checks cancellation again after audio context suspension', async () => {
  let release;
  const calls = [];
  const core = {
    FS: { mkdirTree() {}, mount() {}, filesystems: { IDBFS: {} }, analyzePath: () => ({ exists: true }) },
    _malloc: () => 0, _mdx_create_context: () => 1, _mdx_set_rate() {}, _mdx_set_dir() {},
    _mdx_close() {}, ccall: name => { calls.push(name); return null; },
  };
  const player = new MDXPlayer(core, 44100);
  player.muteAudioDuringCall = async (_node, fn) => {
    await new Promise(resolve => { release = resolve; });
    fn();
  };
  const loading = player.loadData(new Uint8Array([1]), 'old.mdx', {});
  await new Promise(resolve => setImmediate(resolve));
  player.stop();
  release();
  await loading;
  assert.deepEqual(calls, ['mdx_get_pdx_filename']);
});
