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

test('catalog metadata enriches player state and ignores superseded or stopped requests', async () => {
  const pending = [];
  const originalFetch = globalThis.fetch;
  globalThis.fetch = (url, options) => new Promise(resolve => pending.push({ url, signal: options.signal, resolve }));
  try {
    const player = new Player({}, 44100);
    player.metadata = { title: 'Song' };
    player.infoTexts = ['Embedded'];
    const updates = [];
    player.on('playerStateUpdate', state => updates.push(state));

    // 1. Superseded before completion
    const firstSignal = player.beginLoad();
    player.fetchCatalogMetadata('dir/old.spc', firstSignal);
    const secondSignal = player.beginLoad();
    player.resume();
    player.fetchCatalogMetadata('dir/new.spc', secondSignal);
    assert.equal(pending[0].signal.aborted, true);
    assert.equal(pending[1].signal.aborted, false);

    pending[1].resolve(new Response(JSON.stringify({
      songId: 42,
      imageUrl: 'dir/cover.png',
      infoTexts: ['Catalog text'],
      md5: 'abc',
    }), { status: 200 }));
    pending[0].resolve(new Response(JSON.stringify({
      songId: 1,
      imageUrl: 'dir/old.png',
      infoTexts: ['Old text'],
      md5: 'old',
    }), { status: 200 }));
    await new Promise(resolve => setImmediate(resolve));

    assert.equal(updates.length, 1);
    assert.deepEqual(updates[0], {
      metadata: { title: 'Song', imageUrl: 'http://localhost:8080/catalog/dir%2Fcover.png' },
      infoTexts: ['Embedded', 'Catalog text'],
      md5: 'abc',
      songId: 42,
    });
    assert.deepEqual(player.getBasePlayerState().infoTexts, ['Embedded', 'Catalog text']);
  } finally {
    globalThis.fetch = originalFetch;
  }
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

test('muting a MIDI channel silences only the selected output', async () => {
  const { default: MIDIFilePlayer } = await import('../src/players/MIDIFilePlayer');
  for (const useWebMIDI of [false, true]) {
    const sent = [], silenced = [];
    const player = new MIDIFilePlayer({
      useWebMIDI,
      output: { send: (message, time) => sent.push({ message, time }) },
      synth: { panicChannel: channel => silenced.push(channel) },
    });
    player.lastSendTimestamp = 100;
    player.setChannelMute(3, true);
    assert.equal(player.channelMask[3], false);
    assert.deepEqual(silenced, useWebMIDI ? [] : [3]);
    assert.deepEqual(sent, useWebMIDI ? [
      { message: [0xb3, 64, 0], time: 110 },
      { message: [0xb3, 120, 0], time: 110 },
    ] : []);
    player.setChannelMute(3, false);
    assert.equal(player.channelMask[3], true);
    assert.equal(sent.length, useWebMIDI ? 2 : 0);
    assert.equal(silenced.length, useWebMIDI ? 0 : 1);
  }
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

test('sequencer ignores events and failed loads from superseded players', async () => {
  const { default: Sequencer } = await import('../src/Sequencer');
  let failOld;
  const old = Object.assign(new Player({}, 44100), {
    canPlay: ext => ext === 'old',
    loadData: () => new Promise((_, reject) => { failOld = reject; }),
  });
  const current = Object.assign(new Player({}, 44100), {
    canPlay: ext => ext === 'new',
    loadData: async () => {},
  });
  const sequencer = new Sequencer([old, current], { read: () => new Uint8Array([1]) }, () => ({}));
  const errors = [], updates = [];
  sequencer.on('playerError', error => errors.push(error));
  sequencer.on('sequencerStateUpdate', state => updates.push(state));
  sequencer.playSong('local/first.old');
  sequencer.playSong('local/second.new');
  old.emit('playerStateUpdate', { metadata: { title: 'Old' } });
  old.emit('playerError', 'Old error');
  failOld(new Error('Old load failed'));
  await new Promise(resolve => setImmediate(resolve));
  assert.deepEqual(errors, []);
  assert.equal(updates.length, 0);
  assert.equal(sequencer.currSongPath, 'local/second.new');
});

test('superseded song requests cannot load after a later selection or ejection', async () => {
  const { default: Sequencer } = await import('../src/Sequencer');
  const originalFetch = globalThis.fetch;
  const requests = [];
  const loads = [];
  globalThis.fetch = (url, options) => new Promise((resolve, reject) => requests.push({ url, signal: options.signal, resolve, reject }));
  const player = (key, extension) => Object.assign(new Player({}, 44100), {
    canPlay: ext => ext === extension,
    stop() { this.suspend(); },
    fetchCatalogMetadata: async () => ({}),
    loadData: async (_data, path) => { loads.push({ key, path }); },
  });
  const gme = player('gme', 'spc');
  const midi = player('midi', 'mid');
  const sequencer = new Sequencer([gme, midi], { read: () => new Uint8Array([1]) }, () => ({}));
  const respond = request => request.resolve(new Response(new Uint8Array([1]), { status: 200 }));
  const settle = () => new Promise(resolve => setImmediate(resolve));
  try {
    sequencer.playContext(['a.spc']);
    sequencer.playSong('b.mid');
    sequencer.playSong('a.spc');
    assert.equal(requests[0].signal.aborted, true);
    assert.equal(requests[1].signal.aborted, true);
    assert.equal(requests[2].signal.aborted, false);
    respond(requests[2]);
    await settle();
    // Resolve canceled fetches anyway to cover a response already in flight.
    respond(requests[1]);
    respond(requests[0]);
    await settle();
    assert.deepEqual(loads, [{ key: 'gme', path: 'a.spc' }]);
    assert.equal(sequencer.getPlayer(), gme);
    assert.equal(sequencer.getCurrSongPath(), 'a.spc');

    sequencer.playSong('b.mid');
    sequencer.playSong('local/a.spc');
    assert.equal(requests[3].signal.aborted, true);
    respond(requests[3]);
    await settle();
    assert.deepEqual(loads, [{ key: 'gme', path: 'a.spc' }, { key: 'gme', path: 'local/a.spc' }]);
    assert.equal(sequencer.getCurrSongPath(), 'local/a.spc');

    sequencer.playContext(['b.mid']);
    sequencer.nextSong();
    respond(requests[4]);
    await settle();
    assert.equal(sequencer.getPlayer(), null);
    assert.equal(loads.length, 2);

    const errors = [];
    sequencer.on('playerError', error => errors.push(error));
    sequencer.playSong('b.mid');
    sequencer.playSong('local/a.spc');
    requests[5].reject(new Error('stale failure'));
    await settle();
    assert.deepEqual(errors, []);
    sequencer.playSong('b.mid');
    requests[6].reject(new Error('network failure'));
    await settle();
    assert.deepEqual(errors, ['network failure: b.mid']);
  } finally {
    globalThis.fetch = originalFetch;
  }
});

test('Web MIDI completes in the playback loop only after scheduled output', async () => {
  const { default: MIDIFilePlayer } = await import('../src/players/MIDIFilePlayer');
  const player = new MIDIFilePlayer({ useWebMIDI: true, output: { send() {} }, programChangeCb() {} });
  player.events = [{ subtype: 9, channel: 0, param1: 60, param2: 100, playTime: 1000 }];
  player.channelMask[0] = true;
  player.position = 0;
  player.elapsedTime = 980;
  player.lastProcessPlayTimestamp = performance.now();
  player.resume();
  assert.equal(player.processPlay(), 1); // the last note is queued ahead
  assert.equal(player.position, 1);
  player.elapsedTime = 1101;
  player.lastSendTimestamp = performance.now() + 50;
  assert.equal(player.processPlay(), 1); // queued output has not finished
  player.lastSendTimestamp = performance.now() - 101;
  assert.equal(player.processPlay(), 0);
  assert.equal(player.paused, true);
  assert.equal(player.processPlay(), 1); // no second completion
});

test('stopping or seeking Web MIDI before completion does not end the next song', async () => {
  const { default: MIDIFilePlayer } = await import('../src/players/MIDIFilePlayer');
  const player = new MIDIFilePlayer({ useWebMIDI: true, output: { send() {} }, programChangeCb() {} });
  player.events = [{ subtype: 9, channel: 0, param1: 60, param2: 100, playTime: 1000 }];
  player.channelMask[0] = true;
  player.position = 1;
  player.elapsedTime = 990;
  player.lastProcessPlayTimestamp = performance.now();
  player.resume();
  player.stop();
  assert.equal(player.processPlay(), 1);
  player.position = 0;
  player.elapsedTime = 0;
  player.lastProcessPlayTimestamp = performance.now();
  player.resume();
  assert.equal(player.processPlay(), 1);
  player.setPosition(100);
  assert.equal(player.position, 0);
  assert.equal(player.processPlay(), 1);
});

test('MIDI device discovery only selects an output when one exists', async () => {
  const { default: MIDIPlayer } = await import('../src/players/MIDIPlayer');
  const originalNavigator = Object.getOwnPropertyDescriptor(globalThis, 'navigator');
  try {
    for (const count of [0, 1, 2]) {
      const outputs = new Map(Array.from({ length: count }, (_, i) => [`${i}`, { name: `Device ${i + 1}` }]));
      Object.defineProperty(globalThis, 'navigator', { configurable: true, value: { requestMIDIAccess: async () => ({ outputs }) } });
      const player = Object.create(MIDIPlayer.prototype);
      const calls = [];
      player.webMidiIsInitialized = false;
      player.paramDefs = [{ id: 'mididevice', options: [{ items: [] }] }];
      player.setParameter = (...args) => calls.push(args);
      player.ensureWebMidiInitialized();
      await new Promise(resolve => setImmediate(resolve));
      assert.equal(player.paramDefs[0].options[0].items.length, count);
      assert.deepEqual(calls, count ? [['mididevice', 1]] : []);
    }
  } finally {
    if (originalNavigator) Object.defineProperty(globalThis, 'navigator', originalNavigator);
    else delete globalThis.navigator;
  }
});
