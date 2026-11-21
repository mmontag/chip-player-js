const { test } = require('node:test');
const assert = require('node:assert/strict');
const fs = require('node:fs');
const Database = require('better-sqlite3');

test('unfavoriting removes matching sub-tune duplicates and keeps siblings', () => {
  const source = fs.readFileSync('server/database.js', 'utf8');
  const match = source.match(/removeFavoriteByPathStmt: db\.prepare\(`([\s\S]*?)`\)/);
  assert.ok(match, 'favorite removal statement exists');
  const db = new Database(':memory:');
  try {
    db.exec('CREATE TABLE playlists (user_id TEXT, type TEXT, items TEXT, modified_at INTEGER)');
    // Two sub-tunes share the path, and the one being removed is duplicated.
    db.prepare('INSERT INTO playlists VALUES (?, ?, ?, ?)').run('user', 'favorites', JSON.stringify([
      { path: 'same.mid', subtune: 0 },
      { path: 'same.mid', subtune: 1 },
      { path: 'other.mid' },
      { path: 'same.mid' }, // implicit subtune 0, a duplicate of the first
    ]), 0);
    // Removal is keyed by (path, subtune), so the statement needs both.
    const result = db.prepare(match[1]).run({ userId: 'user', path: 'same.mid', subtune: 0, now: 1 });
    assert.equal(result.changes, 1);
    assert.deepEqual(JSON.parse(db.prepare('SELECT items FROM playlists').get().items), [
      { path: 'same.mid', subtune: 1 },
      { path: 'other.mid' },
    ], 'only the selected sub-tune is removed; the sibling survives');
    assert.equal(db.prepare(match[1]).run({ userId: 'missing', path: 'same.mid', subtune: 0, now: 2 }).changes, 0);
  } finally {
    db.close();
  }
});
