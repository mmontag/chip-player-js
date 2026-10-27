const { test } = require('node:test');
const assert = require('node:assert/strict');
const fs = require('node:fs');
const vm = require('node:vm');
const source = fs.readFileSync('src/SettingsSync.js', 'utf8')
  .replace('export default class SettingsSync', 'class SettingsSync') + '\nthis.SettingsSync = SettingsSync;';
const context = { console, setTimeout, clearTimeout };
vm.runInNewContext(source, context);
const SettingsSync = context.SettingsSync;
function storage() {
  const data = new Map();
  return { getItem: key => data.get(key) || null, setItem: (key, val) => data.set(key, val),
    removeItem: key => data.delete(key) };
}

test('failed saves survive recreation and are cleared only after successful retry', async () => {
  const store = storage();
  const first = new SettingsSync('user-a', store, async () => { throw Error('offline'); });
  first.enqueue({ theme: 'dark' });
  await first.flush();
  assert.equal(JSON.parse(store.getItem('pending-settings:user-a')).theme, 'dark');
  first.dispose();
  const other = new SettingsSync('user-b', store, async () => {});
  assert.equal(other.pending, null);
  other.dispose();
  let saved;
  const restored = new SettingsSync('user-a', store, async settings => { saved = settings; });
  assert.equal(restored.pending.theme, 'dark');
  await restored.flush();
  assert.equal(saved.theme, 'dark');
  assert.equal(store.getItem('pending-settings:user-a'), null);
  restored.dispose();
});

test('an older in-flight save cannot clear a newer pending edit', async () => {
  const store = storage();
  let release;
  const sent = [];
  const sync = new SettingsSync('user-a', store, settings => {
    sent.push(settings.theme);
    if (sent.length === 1) return new Promise(resolve => { release = resolve; });
    return Promise.resolve();
  });
  sync.enqueue({ theme: 'first' });
  const first = sync.flush();
  sync.enqueue({ theme: 'second' });
  release();
  await first;
  assert.equal(JSON.parse(store.getItem('pending-settings:user-a')).theme, 'second');
  await sync.flush();
  assert.deepEqual(sent, ['first', 'second']);
  assert.equal(store.getItem('pending-settings:user-a'), null);
  sync.dispose();
});
