const Database = require('better-sqlite3');
const path = require('path');

// Initialize DB
// Database error should be fatal; the server can't function without it.
const DB_PATH = path.resolve(__dirname, 'catalog.db');
const db = new Database(DB_PATH, { readonly: true });
console.log(`Connected to database at ${DB_PATH}`);

const dbStatements = {
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
      SELECT m.title, m.artist, m.game, m.system, m.copyright, i.path as image_path
      FROM music m
               LEFT JOIN images i ON m.image_id = i.id
      WHERE m.path = ?
  `),
  getTextContentStmt: db.prepare('SELECT content FROM texts WHERE id = ?'),
  getShuffleStmt: db.prepare('SELECT path FROM music WHERE path LIKE ? ORDER BY RANDOM() LIMIT ?'),
  getTotalStmt: db.prepare('SELECT COUNT(*) as total FROM music'),
}

module.exports = {
  db,
  dbStatements,
};
