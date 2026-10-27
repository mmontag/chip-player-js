const { test } = require('node:test');
const assert = require('node:assert/strict');
const fs = require('node:fs');
const vm = require('node:vm');
const source = fs.readFileSync('src/players/SIDPlayer.js', 'utf8')
  .replace(/^import .*;\n/gm, '')
  .replace('export default class SIDPlayer', 'class SIDPlayer') + '\nthis.SIDPlayer = SIDPlayer;';

test('late SID metadata cannot overwrite a newer song', async () => {
  const pending = [];
  const context = { Player: class {}, autoBind() {}, API_BASE: '/api',
    axios: { get: () => new Promise(resolve => pending.push(resolve)) }, console };
  vm.runInNewContext(source, context);
  const player = Object.create(context.SIDPlayer.prototype);
  const updates = [];
  player.emit = (_event, state) => updates.push(state);
  player.getBasePlayerState = () => ({ metadata: player.metadata });
  let generation = 1;
  player.getSidMetadata('old', () => generation === 1);
  generation = 2;
  player.getSidMetadata('new', () => generation === 2);
  pending[1]({ status: 200, data: { lengths: '2:00', name: 'New', author: 'B', copyright: '2' } });
  await new Promise(resolve => setImmediate(resolve));
  pending[0]({ status: 200, data: { lengths: '1:00', name: 'Old', author: 'A', copyright: '1' } });
  await new Promise(resolve => setImmediate(resolve));
  assert.equal(player.metadata.formatted.title, 'New');
  assert.equal(player.subtuneDurations[0], 120000);
  assert.equal(updates.length, 1);
});
