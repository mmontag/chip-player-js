const Database = require('better-sqlite3');
const path = require('path');

// Initialize DB
// Database error should be fatal; the server can't function without it.
const CATALOG_DB_PATH = path.resolve(__dirname, 'catalog.db');
const db = new Database(CATALOG_DB_PATH);
console.log(`Connected to database at ${CATALOG_DB_PATH}`);

// Attach user database
const USER_DB_PATH = path.resolve(__dirname, 'users.db');
db.exec(`ATTACH DATABASE '${USER_DB_PATH}' AS user_db`);
console.log(`Attached user database at ${USER_DB_PATH}`);

const dbStatements = {
  // Catalog
  searchStmt: db.prepare(`
      SELECT music_fts.path as file, song_id
      FROM music_fts
      JOIN music m ON m.id = music_fts.rowid
      WHERE music_fts MATCH ?
      ORDER BY rank
      LIMIT ?
  `),
  getDirIdStmt: db.prepare('SELECT id FROM directories WHERE path = ?'),
  getDirChildrenStmt: db.prepare(`
      SELECT name as path, NULL as song_id, 'directory' as type, sort_order, total_size as size, mtime, count
      FROM directories
      WHERE parent_id = ?
      UNION ALL
      SELECT filename as path, song_id, 'file' as type, sort_order, file_size as size, mtime, 0 as count
      FROM music
      WHERE directory_id = ?
      ORDER BY type, sort_order
  `),
  getMetadataStmt: db.prepare(`
      SELECT m.song_id, m.image_id, m.text_ids, m.soundfont, m.md5, i.path as image_path
      FROM music m
               LEFT JOIN images i ON m.image_id = i.id
      WHERE m.path = ?
      LIMIT 1
  `),
  getSongByPathStmt: db.prepare(`
      SELECT m.song_id, m.path, m.title, m.artist, m.game, m.system, m.copyright, i.path as image_path
      FROM music m
               LEFT JOIN images i ON m.image_id = i.id
      WHERE m.path = ?
      LIMIT 1
  `),
  getSongByIdStmt: db.prepare(`
      SELECT m.song_id, m.path, m.title, m.artist, m.game, m.system, m.copyright, i.path as image_path
      FROM music m
               LEFT JOIN images i ON m.image_id = i.id
      WHERE m.song_id LIKE ?
      LIMIT 1
  `),
  getSongImageByIdStmt: db.prepare(`
      SELECT i.path as image_path
      FROM music m
               LEFT JOIN images i ON m.image_id = i.id
      WHERE m.song_id GLOB ?
  `),
  getTextContentStmt: db.prepare('SELECT content FROM texts WHERE id = ?'),
  getShuffleStmt: db.prepare('SELECT path FROM music WHERE path LIKE ? ORDER BY RANDOM() LIMIT ?'),
  getTotalStmt: db.prepare('SELECT COUNT(*) as total, COUNT(DISTINCT song_id) as `unique` FROM music'),

  // Users
  getUserStmt: db.prepare('SELECT * FROM users WHERE id = ?'),
  insertUserStmt: db.prepare(`
      INSERT INTO users (id, email, display_name, photo_url, created_at, last_login, settings)
      VALUES (?, ?, ?, ?, ?, ?, '{}')
  `),
  updateUserProfileStmt: db.prepare(`
      UPDATE users 
      SET email = ?, display_name = ?, photo_url = ?, last_login = ? 
      WHERE id = ?
  `),
  insertPlaybackStmt: db.prepare(`
      INSERT INTO user_db.playbacks (user_id, ip_address, song_id, played_at, duration_ms)
      VALUES (?, ?, ?, ?, ?)
  `),

  // Settings
  getUserSettingsStmt: db.prepare('SELECT settings FROM users WHERE id = ?'),
  updateUserSettingsStmt: db.prepare(`UPDATE users SET settings = json_patch(settings, ?) WHERE id = ?`),
  replaceUserSettingsStmt: db.prepare(`UPDATE users SET settings = ? WHERE id = ?`),

  // Favorites
  // TODO: remove hardcoded prefix
  getFavoritesStmt: db.prepare(`
      SELECT json_group_array(
          json_set(
              je.value,
              '$.href', CONCAT('https://gifx.co/music/', m.path),
              '$.path', m.path,
              '$.size', m.file_size
          )
      ) as items
      FROM user_db.playlists p, json_each(p.items) je
      LEFT JOIN music m ON m.rowid = (
          SELECT rowid FROM music WHERE song_id = json_extract(je.value, '$.songId') ORDER BY mtime LIMIT 1
      )
      WHERE p.user_id = ? AND p.type = 'favorites' AND song_id IS NOT NULL
  `),
  
  // Adds a favorite to the JSON array.
  // If the playlist doesn't exist, it creates it.
  // If the playlist exists, it appends the new item to the end of the array.
  addFavoriteStmt: db.prepare(`
      INSERT INTO playlists (user_id, title, created_at, modified_at, type, items)
      VALUES (@userId, 'Favorites', @now, @now, 'favorites', json_array(json_object('songId', @songId, 'mtime', @now, 'href', @href)))
      ON CONFLICT(user_id) WHERE type = 'favorites' DO UPDATE SET
          items = json_insert(items, '$[#]', json_object('songId', @songId, 'mtime', @now, 'href', @href)),
          modified_at = @now
  `),

  addFavoriteByPathStmt: db.prepare(`
      INSERT INTO playlists (user_id, title, created_at, modified_at, type, items)
      VALUES (@userId, 'Favorites', @now, @now, 'favorites', json_array(json_object('songId', (SELECT song_id FROM music WHERE path = @path), 'mtime', @now, 'path', @path)))
      ON CONFLICT(user_id) WHERE type = 'favorites' DO UPDATE SET
          items = json_insert(items, '$[#]', json_object('songId', (SELECT song_id FROM music WHERE path = @path), 'mtime', @now, 'path', @path)),
          modified_at = @now
  `),

  removeFavoriteByPathStmt: db.prepare(`
      UPDATE playlists
      SET items = (
          SELECT json_group_array(value)
          FROM json_each(items)
          WHERE json_extract(value, '$.path') != @path
      ),
      modified_at = @now
      WHERE user_id = @userId AND type = 'favorites'
  `),
}

module.exports = {
  db,
  dbStatements,
};
