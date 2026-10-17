const Database = require('better-sqlite3');
const path = require('path');

// Initialize DB
// Database error should be fatal; the server can't function without it.
const CATALOG_DB_PATH = path.resolve(__dirname, 'catalog.db');
const db = new Database(CATALOG_DB_PATH);
console.log(`Connected to database at ${CATALOG_DB_PATH}`);

const USER_DB_PATH = path.resolve(__dirname, 'users.db');
db.exec(`ATTACH DATABASE '${USER_DB_PATH}' AS user_db`);
db.exec(`CREATE INDEX IF NOT EXISTS user_db.idx_playbacks_played_at ON playbacks(played_at)`);
console.log(`Attached user database at ${USER_DB_PATH}`);

const CSDB_DB_PATH = path.resolve(__dirname, 'csdb.db');
db.exec(`ATTACH DATABASE '${CSDB_DB_PATH}' AS csdb_db`);
console.log(`Attached CSdb database at ${CSDB_DB_PATH}`);

// playbacks.subtune is new; add it in place for databases created before it.
const playbackColumns = db.prepare('PRAGMA user_db.table_info(playbacks)').all();
if (playbackColumns.length > 0 && !playbackColumns.some(c => c.name === 'subtune')) {
  db.exec('ALTER TABLE user_db.playbacks ADD COLUMN subtune INTEGER DEFAULT 0');
  console.log('Added playbacks.subtune column');
}

const dbStatements = {
  // Catalog
  // One index over songs: a single-song file is one row, a song folder is one
  // row per sub-tune, so a file and its sub-tunes rank in a single list. Rows
  // carry the parent file's path and song_id plus the 0-based subtune, so each
  // result is playable directly; `title` is the sub-tune's own label (NULL when
  // it has none).
  searchSongStmt: db.prepare(`
      SELECT s.path as file, s.song_id as song_id, s.subtune as subtune, s.title as title,
             s.subtune_count as subtuneCount
      FROM song_fts
      JOIN song s ON s.id = song_fts.rowid
      WHERE song_fts MATCH ?
      ORDER BY rank
      LIMIT ?
  `),
  getDirIdStmt: db.prepare('SELECT id FROM directories WHERE path = ?'),
  getDirChildrenStmt: db.prepare(`
      SELECT name as path, NULL as song_id, 'directory' as type, sort_order, total_size as size, mtime, count, 0 as subtune_count, NULL as release_date
      FROM directories
      WHERE parent_id = ?
      UNION ALL
      SELECT filename as path, song_id, 'file' as type, sort_order, file_size as size, mtime, 0 as count, subtune_count, release_date
      FROM music
      WHERE directory_id = ?
      ORDER BY type, sort_order, path COLLATE NOCASE
  `),
  // The sub-tunes of a single multi-song file, in play order.
  getSubtunesStmt: db.prepare(`
      SELECT st.subtune, st.title, st.length_ms, st.date
      FROM subtune st
      JOIN music m ON m.id = st.music_id
      WHERE m.path = ?
      ORDER BY st.sort_order, st.subtune
  `),
  getMetadataStmt: db.prepare(`
      SELECT m.song_id, m.image_id, m.text_ids, m.soundfont, m.md5, m.contributor, m.subtune_count, i.path as image_path
      FROM music m
               LEFT JOIN images i ON m.image_id = i.id
      WHERE m.path = ?
      LIMIT 1
  `),
  // Metadata for one sub-tune of a file.
  getSubtuneMetadataStmt: db.prepare(`
      SELECT st.title
      FROM subtune st
      JOIN music m ON m.id = st.music_id
      WHERE m.path = ? AND st.subtune = ?
      LIMIT 1
  `),
  getSidMetadataByHashStmt: db.prepare(`
      SELECT * FROM hvsc_files WHERE hash = ? LIMIT 1
  `),
  getSidMetadataByPathStmt: db.prepare(`
      SELECT * FROM hvsc_files WHERE fullname = ? LIMIT 1
  `),
  getSongByPathStmt: db.prepare(`
      SELECT m.song_id, m.path, m.title, m.artist, m.contributor, m.game, m.system, m.copyright,
             m.file_size, m.mtime, m.release_date, m.subtune_count, i.path as image_path
      FROM music m
               LEFT JOIN images i ON m.image_id = i.id
      WHERE m.path = ?
      LIMIT 1
  `),
  // Used to validate a playback log's sub-tune against the file's count.
  getSubtuneCountBySongIdStmt: db.prepare(`
      SELECT subtune_count FROM music WHERE song_id = ? LIMIT 1
  `),
  // Used to populate meta tags. Favor entries with images
  getSongByIdStmt: db.prepare(`
      SELECT m.song_id, m.path, m.title, m.artist, m.contributor, m.game, m.system, m.copyright, m.subtune_count, i.path as image_path
      FROM music m
               LEFT JOIN images i ON m.image_id = i.id
      WHERE m.song_id LIKE ?
      ORDER BY i.path DESC
      LIMIT 1
  `),
  getSongImageByIdStmt: db.prepare(`
      SELECT m.path, i.path as image_path
      FROM music m
               LEFT JOIN images i ON m.image_id = i.id
      WHERE m.song_id GLOB ?
  `),
  getTextContentStmt: db.prepare('SELECT content FROM texts WHERE id = ?'),
  // Shuffle play, one row per playable song: a multi-song file contributes a row
  // per sub-tune (only multi-song files have them), so a sub-tune is shuffled
  // exactly like a single-song file. The first parameter matches a file at
  // exactly this path and the second matches everything below it, so a directory
  // shuffles recursively while a file shuffles as itself. The second parameter
  // is a LIKE pattern, so the prefix is escaped (see escapeLike) to keep any
  // wildcards in the path literal rather than matching a sibling.
  getShuffleStmt: db.prepare(`
      SELECT m.path, COALESCE(st.subtune, 0) AS subtune
      FROM music m
      LEFT JOIN subtune st ON st.music_id = m.id
      WHERE m.path = ? OR m.path LIKE ? ESCAPE '\\'
      ORDER BY RANDOM() LIMIT ?
  `),
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
      INSERT INTO user_db.playbacks (user_id, ip_address, song_id, subtune, played_at, duration_ms)
      VALUES (?, ?, ?, ?, ?, ?)
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
              '$.size', m.file_size,
              '$.subtuneCount', COALESCE(m.subtune_count, 1),
              '$.subtuneTitle', (
                  SELECT st.title FROM subtune st
                  WHERE st.music_id = m.id
                    AND st.subtune = COALESCE(json_extract(je.value, '$.subtune'), 0)
              )
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
      VALUES (@userId, 'Favorites', @now, @now, 'favorites', json_array(json_object('songId', (SELECT song_id FROM music WHERE path = @path), 'mtime', @now, 'path', @path, 'subtune', @subtune)))
      ON CONFLICT(user_id) WHERE type = 'favorites' DO UPDATE SET
          items = json_insert(items, '$[#]', json_object('songId', (SELECT song_id FROM music WHERE path = @path), 'mtime', @now, 'path', @path, 'subtune', @subtune)),
          modified_at = @now
  `),

  removeFavoriteByPathStmt: db.prepare(`
      UPDATE playlists
      SET items = (
          SELECT json_group_array(value)
          FROM json_each(items)
          WHERE NOT (
              json_extract(value, '$.path') = @path
              AND COALESCE(json_extract(value, '$.subtune'), 0) = @subtune
          )
      ),
      modified_at = @now
      WHERE user_id = @userId AND type = 'favorites'
  `),

  getCsdbXmlStmt: db.prepare('SELECT xml FROM csdb_db.sids WHERE csdbid = ? LIMIT 1'),
  insertCsdbXmlStmt: db.prepare(`
      INSERT INTO csdb_db.sids (csdbid, xml, fetched_at)
      VALUES (@csdbid, @xml, @now)
  `),

  // Top Charts
  // Grouped by (song_id, subtune), so sub-tunes rank individually. Old
  // playbacks have no subtune and fall back to 0.
  getGlobalTopStmt: db.prepare(`
      SELECT
        top.song_id,
        top.subtune,
        top.plays,
        m.path,
        m.file_size,
        m.mtime,
        COALESCE(m.subtune_count, 1) AS subtuneCount,
        (SELECT st.title FROM subtune st WHERE st.music_id = m.id AND st.subtune = top.subtune) as subtune_title
      FROM (
        SELECT song_id, COALESCE(subtune, 0) as subtune, COUNT(*) as plays
        FROM user_db.playbacks
        WHERE played_at >= ?
        GROUP BY song_id, COALESCE(subtune, 0)
        ORDER BY plays DESC
        LIMIT ?
      ) top
      JOIN music m ON m.rowid = (
        SELECT rowid FROM music WHERE song_id = top.song_id ORDER BY mtime LIMIT 1
      )
      LIMIT ?
  `),

  getUserTopStmt: db.prepare(`
      SELECT
        top.song_id,
        top.subtune,
        top.plays,
        m.path,
        m.file_size,
        m.mtime,
        COALESCE(m.subtune_count, 1) AS subtuneCount,
        (SELECT st.title FROM subtune st WHERE st.music_id = m.id AND st.subtune = top.subtune) as subtune_title
      FROM (
        SELECT song_id, COALESCE(subtune, 0) as subtune, COUNT(*) as plays
        FROM user_db.playbacks
        WHERE user_id = ? AND played_at >= ?
        GROUP BY song_id, COALESCE(subtune, 0)
        ORDER BY plays DESC
        LIMIT ?
      ) top
      JOIN music m ON m.rowid = (
        SELECT rowid FROM music WHERE song_id = top.song_id ORDER BY mtime LIMIT 1
      )
      LIMIT ?
  `),

  getTopFavoritesStmt: db.prepare(`
      SELECT
        top.song_id,
        top.subtune,
        top.favorites as count,
        top.favorites as plays,
        m.path,
        m.file_size,
        m.mtime,
        COALESCE(m.subtune_count, 1) AS subtuneCount,
        (SELECT st.title FROM subtune st WHERE st.music_id = m.id AND st.subtune = top.subtune) as subtune_title
      FROM (
        SELECT
          json_extract(je.value, '$.songId') as song_id,
          COALESCE(json_extract(je.value, '$.subtune'), 0) as subtune,
          COUNT(DISTINCT p.user_id) as favorites
        FROM user_db.playlists p, json_each(p.items) je
        WHERE p.type = 'favorites' AND json_extract(je.value, '$.songId') IS NOT NULL
        GROUP BY song_id, subtune
        ORDER BY favorites DESC
        LIMIT ?
      ) top
      JOIN music m ON m.rowid = (
        SELECT rowid FROM music WHERE song_id = top.song_id ORDER BY mtime LIMIT 1
      )
      LIMIT ?
  `),

  // Directory Metadata for SEO
  getDirMetadataStmt: db.prepare(`
      SELECT d.id, d.name, d.path, d.count, d.total_size, i.path as image_path
      FROM directories d
      LEFT JOIN images i ON d.image_id = i.id
      WHERE d.path = ?
      LIMIT 1
  `),
  getDirSampleSongsStmt: db.prepare(`
      SELECT title, artist, game, system, extension, filename, song_id
      FROM music
      WHERE directory_id = ?
      ORDER BY sort_order, path
      LIMIT 8
  `),
  getDirSampleSubdirsStmt: db.prepare(`
      SELECT name, count
      FROM directories
      WHERE parent_id = ?
      ORDER BY count DESC
      LIMIT 6
  `),
  getDirDirectExtensionsStmt: db.prepare(`
      SELECT DISTINCT extension
      FROM music
      WHERE directory_id = ?
  `),
  getDirSubtreeExtensionsStmt: db.prepare(`
      SELECT DISTINCT extension
      FROM music
      WHERE path >= ? AND path < ?
      LIMIT 6
  `),
}

module.exports = {
  db,
  dbStatements,
};
