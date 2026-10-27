const { test } = require('node:test');
const assert = require('node:assert/strict');
const fs = require('node:fs');
const vm = require('node:vm');
const { EventEmitter } = require('node:events');

function loadClass(name, writeFile) {
  const source = fs.readFileSync(`src/players/${name}.js`, 'utf8')
    .replace(/^import .*;\n/gm, '')
    .replace(`export default class ${name}`, `class ${name}`) + `\nthis.Result = ${name};`;
  class Player extends EventEmitter {
    constructor(core) { super(); this.core = core; this.bufferSize = 8; this.sampleRate = 44100; this.params = {}; }
    static metadataFromFilepath() { return {}; }
    resolveParamValues() {}
    getBasePlayerState() { return {}; }
    resume() {}
  }
  const calls = [];
  const core = { FS: { mkdirTree() {}, mount() {}, filesystems: { IDBFS: {} } },
    _malloc: () => 0, _mdx_create_context: () => 1, _mdx_set_rate() {}, _mdx_set_dir() {},
    ccall: name => { calls.push(name); return name === 'mdx_get_pdx_filename' ? null : 0; } };
  const context = { Player, autoBind() {}, parsePSF: () => ({ libraries: [] }),
    ensureEmscFileWithData: writeFile, ensureEmscFileWithUrl: async () => {},
    pathe: { dirname: () => 'local', basename: x => x }, pathJoin: (...x) => x.join('/'),
    CATALOG_PREFIX: '', chipImage: '', console, Uint8Array, TextDecoder };
  vm.runInNewContext(source, context);
  return { player: new context.Result(core), calls };
}

for (const name of ['N64Player', 'GSFPlayer', 'MDXPlayer']) {
  test(`${name} does not open a superseded file after filesystem work`, async () => {
    let release;
    const { player, calls } = loadClass(name, () => new Promise(resolve => { release = resolve; }));
    let current = true;
    const loading = player.loadData(new Uint8Array([1]), 'old', {}, 0, () => current);
    current = false;
    release();
    await loading;
    assert.deepEqual(calls, []);
  });
}

test('MDX does not open a superseded file after audio context suspension', async () => {
  const { player, calls } = loadClass('MDXPlayer', async () => {});
  let release;
  const suspended = new Promise(resolve => { release = resolve; });
  player.audioNode = {};
  player.muteAudioDuringCall = async (_node, fn) => { await suspended; fn(); };
  let current = true;
  const loading = player.loadData(new Uint8Array([1]), 'old', {}, 0, () => current);
  await new Promise(resolve => setImmediate(resolve));
  current = false;
  release();
  await loading;
  assert.deepEqual(calls, ['mdx_get_pdx_filename']);
});
