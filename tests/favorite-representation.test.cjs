const { test } = require('node:test');
const assert = require('node:assert/strict');

test('favorite representation preserves size when full item is provided', () => {
  const item = {
    path: 'MIDI/Journey/Send Her My Love.mid',
    songId: 'JBMSn-hp',
    size: 38192,
    mtime: '2024-01-01',
    idx: 5,
    name: 'Send Her My Love.mid',
  };

  const { path: songPath, songId, size } = item;
  const now = Math.floor(Date.now() / 1000);
  const fave = {
    path: songPath,
    songId,
    size,
    href: item.href || `https://gifx.co/music/${encodeURIComponent(songPath)}`,
    mtime: now,
  };

  assert.equal(fave.path, 'MIDI/Journey/Send Her My Love.mid');
  assert.equal(fave.songId, 'JBMSn-hp');
  assert.equal(fave.size, 38192);
  assert.equal(typeof fave.mtime, 'number');
  assert.equal(fave.idx, undefined, 'idx must not leak into favorite item');
});

test('server /user/favorites/add returns full hydrated favorite object', () => {
  const { dbStatements } = require('../server/database.js');
  const song = dbStatements.getSongByPathStmt.get('MIDI/Journey/Send Her My Love.mid');
  assert.ok(song, 'song should exist in catalog');
  assert.equal(typeof song.file_size, 'number', 'file_size must be a number');
  assert.ok(song.file_size > 0, 'file_size must be positive');

  const mtime = Math.floor(Date.now() / 1000);
  const favoriteResponse = {
    songId: song.song_id,
    path: song.path,
    size: song.file_size,
    mtime,
    href: `https://gifx.co/music/${song.path}`,
  };

  assert.equal(favoriteResponse.size, song.file_size);
  assert.equal(favoriteResponse.songId, song.song_id);
  assert.equal(favoriteResponse.path, song.path);
  assert.equal(favoriteResponse.href, `https://gifx.co/music/${song.path}`);

  // Test reconciliation replacing the matching favorite with endpoint response
  const faves = [
    { path: 'other/path.mid', songId: '12345678', size: 100 },
    { path: song.path, songId: song.song_id, size: undefined },
  ];
  const reconciled = faves.map(f => (f.path === song.path ? favoriteResponse : f));
  assert.equal(reconciled[1].size, song.file_size);
  assert.equal(reconciled[1].href, `https://gifx.co/music/${song.path}`);
});
