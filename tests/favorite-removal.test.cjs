const { test } = require('node:test');
const assert = require('node:assert/strict');
const fs = require('node:fs');
const Database = require('better-sqlite3');

test('unfavoriting removes duplicate paths and keeps other favorites', () => {
  const source = fs.readFileSync('server/database.js', 'utf8');
  const match = source.match(/removeFavoriteByPathStmt: db\.prepare\(`([\s\S]*?)`\)/);
  assert.ok(match, 'favorite removal statement exists');
  const db = new Database(':memory:');
  try {
    db.exec('CREATE TABLE playlists (user_id TEXT, type TEXT, items TEXT, modified_at INTEGER)');
    db.prepare('INSERT INTO playlists VALUES (?, ?, ?, ?)').run('user', 'favorites', JSON.stringify([
      { path: 'same.mid' }, { path: 'other.mid' }, { path: 'same.mid' },
    ]), 0);
    const result = db.prepare(match[1]).run({ userId: 'user', path: 'same.mid', now: 1 });
    assert.equal(result.changes, 1);
    assert.deepEqual(JSON.parse(db.prepare('SELECT items FROM playlists').get().items), [{ path: 'other.mid' }]);
    assert.equal(db.prepare(match[1]).run({ userId: 'missing', path: 'same.mid', now: 2 }).changes, 0);
  } finally {
    db.close();
  }
});
