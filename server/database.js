const Database = require('better-sqlite3');
const path = require('path');

// Initialize DB
// Database error should be fatal; the server can't function without it.
const CATALOG_DB_PATH = path.resolve(__dirname, 'catalog.db');
const db = new Database(CATALOG_DB_PATH, { readonly: true });
console.log(`Connected to database at ${CATALOG_DB_PATH}`);

// Attach user database
const USER_DB_PATH = path.resolve(__dirname, 'users.db');
db.exec(`ATTACH DATABASE '${USER_DB_PATH}' AS user_db`);
console.log(`Attached user database at ${USER_DB_PATH}`);

const dbStatements = {
  // Catalog
  searchStmt: db.prepare(`
      SELECT path as file, title, artist, game, system
      FROM music_fts
      WHERE music_fts MATCH ?
      ORDER BY rank
      LIMIT ?
  `),
  getDirIdStmt: db.prepare('SELECT id FROM directories WHERE path = ?'),
  getDirChildrenStmt: db.prepare(`
      SELECT name as path, 'directory' as type, sort_order, total_size as size, mtime, count
      FROM directories
      WHERE parent_id = ?
      UNION ALL
      SELECT filename as path, 'file' as type, sort_order, file_size as size, mtime, 0 as count
      FROM music
      WHERE directory_id = ?
      ORDER BY type, sort_order
  `),
  getMetadataStmt: db.prepare(`
      SELECT m.image_id, m.text_ids, m.soundfont, m.md5, i.path as image_path
      FROM music m
               LEFT JOIN images i ON m.image_id = i.id
      WHERE m.path = ?
  `),
  getSongInfoStmt: db.prepare(`
      SELECT m.song_id, m.title, m.artist, m.game, m.system, m.copyright, i.path as image_path
      FROM music m
               LEFT JOIN images i ON m.image_id = i.id
      WHERE m.path = ?
  `),
  getSongImageByIdStmt: db.prepare(`
      SELECT i.path as image_path
      FROM music m
               LEFT JOIN images i ON m.image_id = i.id
      WHERE m.song_id GLOB ?
  `),
  getTextContentStmt: db.prepare('SELECT content FROM texts WHERE id = ?'),
  getShuffleStmt: db.prepare('SELECT path FROM music WHERE path LIKE ? ORDER BY RANDOM() LIMIT ?'),
  getTotalStmt: db.prepare('SELECT COUNT(*) as total FROM music'),

  // Users
  getUserStmt: db.prepare('SELECT * FROM users WHERE id = ?'),
  insertUserStmt: db.prepare(`
      INSERT INTO users (id, email, display_name, photo_url, settings)
      VALUES (?, ?, ?, ?, '{}')
  `),

  // Favorites
  getFavoritesStmt: db.prepare("SELECT items FROM playlists WHERE user_id = ? AND type = 'favorites'"),
  
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

  // Removes a favorite from the JSON array by songId.
  // This uses a subquery to filter the JSON array.
  removeFavoriteStmt: db.prepare(`
      UPDATE playlists
      SET items = (
          SELECT json_group_array(value)
          FROM json_each(items)
          WHERE json_extract(value, '$.songId') != @songId
      ),
      modified_at = @now
      WHERE user_id = @userId AND type = 'favorites'
  `),
}

module.exports = {
  db,
  dbStatements,
};
