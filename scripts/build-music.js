const fs = require('fs');
const os = require('os');
const path = require('path');
const crypto = require('crypto');
const Database = require('better-sqlite3');
const { Command } = require('commander');
const chalk = require('chalk');
const { toArabic } = require('roman-numerals');
const { FORMATS } = require('../src/config/index');
const { parseMetadata, cleanString } = require('./metadata-parsers');
const { describeSubtunes } = require('./subtune-headers');


const program = new Command();

program
  .name('build-music-db')
  .description('Builds Chip Player music database.')
  .option('-l, --limit <number>', 'Max number of files to process', 0)
  .option('-d, --dryrun', 'Dry run, do not write to database', false)
  .option('-v, --verbose', 'Verbose output', false)
  .option('-f, --force', 'Force re-process (overwrite existing entries)', false)
  .option('--filter <path>', 'Scan only a specific subdirectory (relative to catalog root)', '')
  .option('-r, --reset-db', 'Delete and recreate the database table', false)
  .option('-n, --no-skip-unmodified', 'Force reprocessing of unmodified files (checks mtime)')
  .option('--import-hvsc', 'Import HVSC catalog tables (hvsc_files, sid_release_map)', false)
  .option('--hvsc-only', 'Only import HVSC tables and exit without scanning music files', false)
  .parse(process.argv);

const options = program.opts();
const limit = parseInt(options.limit);

// Config
const CATALOG_DIR = path.resolve(__dirname, '../catalog');
const DB_PATH = path.resolve(__dirname, '../server/catalog.db');
const VALID_EXTENSIONS = new Set(FORMATS.map(f => '.'+f));

// Regex
const ROMAN_NUMERAL_NINE_REGEX = /\bix\b/i;
const ROMAN_NUMERAL_REGEX = /\b([IVXLC]+|[ivxlc]+)([-.,) ]|$)/;
const FRONT_ART_REGEX = /(front|cover)/i;
const IMAGE_EXT_REGEX = /\.(gif|png|jpg|jpeg)$/i;
const TEXT_EXT_REGEX = /\.(text|txt|doc)$/i;
const SF2_REGEX = /SF2=(.+?)\.sf2/;

const NUMERIC_COLLATOR = new Intl.Collator(undefined, { numeric: true, sensitivity: 'base' });

function tableExists(database, tableName) {
  try {
    const row = database.prepare("SELECT 1 FROM sqlite_master WHERE type = 'table' AND name = ?").get(tableName);
    return Boolean(row);
  } catch (e) {
    return false;
  }
}

// Initialize DB.
// A dry run must leave the catalog untouched, but it still has to run the
// additive migrations (the INSERTs reference the new columns) and exercise the
// same code as a real build. So take a snapshot and work on that; the real
// catalog file is never opened for writing.
const dbExists = fs.existsSync(DB_PATH);
let db = new Database(DB_PATH);
let dryRunDbPath = null;
if (options.dryrun) {
  dryRunDbPath = path.join(os.tmpdir(), `catalog-dryrun-${process.pid}.db`);
  try {
    fs.rmSync(dryRunDbPath, { force: true });
    // VACUUM INTO snapshots the committed state including anything still in WAL.
    db.exec(`VACUUM INTO '${dryRunDbPath.replace(/'/g, "''")}'`);
    db.close();
    db = new Database(dryRunDbPath);
  } catch (e) {
    console.warn(chalk.yellow(`Could not snapshot the catalog for a dry run (${e.message}); using a temporary empty database.`));
    db.close();
    fs.rmSync(dryRunDbPath, { force: true });
    db = new Database(dryRunDbPath);
  }
}
db.pragma('journal_mode = WAL');

process.on('SIGINT', () => {
  console.log(chalk.yellow('\n\nScript interrupted! Closing database...'));
  // Leave the search index consistent with what was committed before the
  // interrupt, same as the finally does for an aborted run.
  if (!options.dryrun) {
    try {
      rebuildSongs();
    } catch (err) {
      // ignore
    }
  }
  try {
    db.close();
  } catch (err) {
    // ignore
  }
  process.exit(0);
});

// Get initial count
let initialCount = 0;
try {
  const row = db.prepare('SELECT COUNT(*) as count FROM music').get();
  initialCount = row ? row.count : 0;
} catch (e) {
  // Table might not exist yet
}

if (options.resetDb) {
  if (options.verbose) console.log(chalk.yellow('Resetting database tables...'));
  db.exec(`
    DROP TABLE IF EXISTS song_fts;
    DROP TABLE IF EXISTS song;
    DROP TABLE IF EXISTS subtune;
    DROP TABLE IF EXISTS subtune_fts;
    DROP TABLE IF EXISTS music;
    DROP TABLE IF EXISTS directories;
    DROP TABLE IF EXISTS images;
    DROP TABLE IF EXISTS texts;
    DROP TABLE IF EXISTS music_fts;
    DROP TABLE IF EXISTS hvsc_files;
    DROP TABLE IF EXISTS sid_release_map;
  `);
}

// 1. Create Tables
db.exec(`
  -- The song index supersedes the per-table FTS pair (music_fts/subtune_fts):
  -- /search reads one index over songs, so a file and its sub-tunes rank in one
  -- list. Drop the old tables and their triggers so an existing catalog migrates
  -- in place.
  DROP TRIGGER IF EXISTS music_ai;
  DROP TRIGGER IF EXISTS music_ad;
  DROP TRIGGER IF EXISTS music_au;
  DROP TRIGGER IF EXISTS subtune_ai;
  DROP TRIGGER IF EXISTS subtune_ad;
  DROP TRIGGER IF EXISTS subtune_au;
  DROP TABLE IF EXISTS music_fts;
  DROP TABLE IF EXISTS subtune_fts;

  CREATE TABLE IF NOT EXISTS images (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    path TEXT UNIQUE
  );

  CREATE TABLE IF NOT EXISTS texts (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    hash TEXT UNIQUE,
    content TEXT
  );

  CREATE TABLE IF NOT EXISTS directories (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    parent_id INTEGER,
    name TEXT,
    path TEXT UNIQUE,            -- Full relative path for API lookup
    image_id INTEGER,            -- Resolved image for this folder
    text_ids TEXT,               -- JSON array of text IDs
    sort_order INTEGER DEFAULT 0,
    count INTEGER DEFAULT 0,     -- Recursive count of music files
    total_size INTEGER DEFAULT 0,-- Recursive size in bytes
    mtime TEXT,                  -- Directory modification time
    FOREIGN KEY(parent_id) REFERENCES directories(id),
    FOREIGN KEY(image_id) REFERENCES images(id)
  );

  CREATE TABLE IF NOT EXISTS music (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    directory_id INTEGER,
    filename TEXT,
    path TEXT UNIQUE,            -- Full relative path
    extension TEXT,
    song_id TEXT,                -- Truncated hash
    title TEXT,
    artist TEXT,
    contributor TEXT,            -- Sequencer / Ripper / Transcriber
    game TEXT,
    system TEXT,
    copyright TEXT,
    release_date TEXT,           -- ISO date parsed from metadata (year -> Jan 1)
    file_size INTEGER,
    mtime TEXT,
    raw_meta TEXT,
    image_id INTEGER,            -- Resolved image for this file
    text_ids TEXT,               -- JSON array of text IDs
    soundfont TEXT,              -- Relative path to soundfont
    md5 TEXT,                    -- MD5 hash for tracker modules
    subtune_count INTEGER DEFAULT 1, -- Playable sub-tunes (1 = single song)
    sort_order INTEGER DEFAULT 0,
    FOREIGN KEY(directory_id) REFERENCES directories(id),
    FOREIGN KEY(image_id) REFERENCES images(id)
  );

  -- Each row is a playable sub-tune within a single file (e.g. an NSF/NSFE/SID
  -- containing several tunes). A file with subtune_count > 1 is presented as a
  -- "song folder". Single-song files have no rows here, so a sub-tune and a
  -- plain song are the same kind of thing to the client.
  CREATE TABLE IF NOT EXISTS subtune (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    music_id INTEGER NOT NULL,
    subtune INTEGER NOT NULL,    -- 0-based, matches the player's subtune index
    title TEXT,
    length_ms INTEGER,
    date TEXT,                   -- per-sub-tune date when the format provides one
    sort_order INTEGER DEFAULT 0,
    UNIQUE(music_id, subtune),
    FOREIGN KEY(music_id) REFERENCES music(id) ON DELETE CASCADE
  );

  -- Every playable song, one row each: a single-song file is one row, a "song
  -- folder" is one row per sub-tune. Rebuilt from music + subtune at the end of
  -- a run (rebuildSongs) and the single index /search reads, so a file and its
  -- sub-tunes are ranked in one list.
  CREATE TABLE IF NOT EXISTS song (
    id INTEGER PRIMARY KEY,      -- the FTS rowid
    music_id INTEGER NOT NULL,
    subtune INTEGER NOT NULL,    -- 0-based, matches the player's subtune index
    path TEXT NOT NULL,
    song_id TEXT,
    title TEXT,                  -- the sub-tune's own label (NULL when unlabelled)
    file_title TEXT,             -- the parent file's title, so its name matches every song it holds
    artist TEXT,
    game TEXT,
    system TEXT,
    filename TEXT,
    subtune_count INTEGER NOT NULL DEFAULT 1,  -- the file's count: >1 = a song folder
    UNIQUE(music_id, subtune),
    FOREIGN KEY(music_id) REFERENCES music(id) ON DELETE CASCADE
  );

  CREATE VIRTUAL TABLE IF NOT EXISTS song_fts USING fts5(
    title, file_title, artist, game, system, filename, path,
    content='song', content_rowid='id',
    prefix='1 2 3'
  );

  CREATE INDEX IF NOT EXISTS idx_directories_parent ON directories(parent_id);
  CREATE INDEX IF NOT EXISTS idx_music_directory ON music(directory_id);
  CREATE INDEX IF NOT EXISTS idx_music_sort ON music(sort_order);
  CREATE INDEX IF NOT EXISTS idx_music_songid ON music(song_id);
  CREATE INDEX IF NOT EXISTS idx_subtune_music ON subtune(music_id);
  CREATE INDEX IF NOT EXISTS idx_directories_sort ON directories(sort_order);
  CREATE INDEX IF NOT EXISTS idx_song_music ON song(music_id);
`);

// The song index is rebuilt from music + subtune on every run rather than kept
// incrementally: it is the only thing /search reads, and a build is the only
// time the set of songs changes. `song.title` is the sub-tune's own label (NULL
// for a single-song file or an unlabelled tune, so the client falls back to
// `Tune N`); `file_title` carries the parent's name so a file-name query matches
// every song the file holds -- a song folder's name finds its songs.
function rebuildSongs() {
  // Swapped in one transaction, like the per-file work: the statements below
  // commit one at a time otherwise, so a reader landing in the middle sees no
  // index at all, and a hard kill leaves one half built. The index is derived,
  // so all the rollback can cost is a stale index until the next run.
  const swapSongs = db.transaction(() => {
    db.exec(`
      DELETE FROM song;
      INSERT INTO song (music_id, subtune, path, song_id, title, file_title, artist, game, system, filename, subtune_count)
        SELECT id, 0, path, song_id, NULL, title, artist, game, system, filename, COALESCE(subtune_count, 1)
          FROM music WHERE subtune_count <= 1;
      INSERT INTO song (music_id, subtune, path, song_id, title, file_title, artist, game, system, filename, subtune_count)
        SELECT m.id, st.subtune, m.path, m.song_id, NULLIF(st.title, ''), m.title, m.artist, m.game, m.system, m.filename, COALESCE(m.subtune_count, 1)
          FROM subtune st JOIN music m ON m.id = st.music_id;
      INSERT INTO song_fts(song_fts) VALUES('rebuild');
    `);
  });
  swapSongs();
}

// Additive migrations for databases created before these columns existed.
// Returns true when the column was just added.
function ensureColumn(table, column, type) {
  const columns = db.prepare(`PRAGMA table_info(${table})`).all();
  if (!columns.some(c => c.name === column)) {
    db.exec(`ALTER TABLE ${table} ADD COLUMN ${column} ${type}`);
    return true;
  }
  return false;
}
const addedSubtuneCount = ensureColumn('music', 'subtune_count', 'INTEGER DEFAULT 1');
ensureColumn('music', 'release_date', 'TEXT');
ensureColumn('music', 'contributor', 'TEXT');
ensureColumn('subtune', 'date', 'TEXT');
ensureColumn('song', 'subtune_count', 'INTEGER DEFAULT 1');

// A catalog created before sub-tunes existed has no sub-tune rows, so unchanged
// multi-song files get skipped and never gain them. "Does this catalog still
// need the backfill?" cannot be read off the data: the ALTER gives every
// existing row subtune_count = 1, which is indistinguishable from a genuinely
// single-song file. So record it in the database header instead, and only an
// unfiltered build is allowed to mark it done -- a --filter run adds the column
// to the real catalog without covering the rest of it.
const SUBTUNE_BACKFILL_VERSION = 1;
const backfillDone = db.pragma('user_version', { simple: true }) >= SUBTUNE_BACKFILL_VERSION;
const needsBackfill = addedSubtuneCount || !backfillDone;
if (needsBackfill) {
  options.skipUnmodified = false;
  if (options.verbose) {
    console.log(chalk.cyan(addedSubtuneCount
      ? 'Backfilling sub-tunes (newly added sub-tune count)...'
      : 'Backfilling sub-tunes (catalog predates them)...'));
  }
}

function importHvscCatalog(targetDb, opts = {}) {
  const hvscSqlPath = path.resolve(__dirname, 'hvsc_files_sqlite.sql');
  const sidMapSqlPath = path.resolve(__dirname, 'sid_release_map_sqlite.sql');

  if (!fs.existsSync(hvscSqlPath) || !fs.existsSync(sidMapSqlPath)) {
    console.warn(chalk.yellow('HVSC SQL dump files not found in scripts directory; skipping HVSC import.'));
    return;
  }

  console.log(chalk.cyan('Importing HVSC catalog tables (hvsc_files, sid_release_map)...'));

  // Drop existing tables before re-importing
  targetDb.exec(`
    DROP TABLE IF EXISTS hvsc_files;
    DROP TABLE IF EXISTS sid_release_map;
  `);

  // 1. Import hvsc_files
  let hvscSql = fs.readFileSync(hvscSqlPath, 'utf8');
  hvscSql = hvscSql.replace(/^PRAGMA\s+[^;]+;\s*/gmi, '');
  hvscSql = hvscSql.replace(/,[\s\t]*ADD\s+PRIMARY\s+KEY[\s\S]*?END TRANSACTION;/i, 'END TRANSACTION;');
  targetDb.exec(hvscSql);

  // 2. Ensure hvsc_files indexes exist
  targetDb.exec(`
    CREATE INDEX IF NOT EXISTS hvsc_files_fullname_index ON hvsc_files (fullname);
    CREATE INDEX IF NOT EXISTS hvsc_files_hash_index ON hvsc_files (hash);
  `);

  // 3. Import sid_release_map
  let sidMapSql = fs.readFileSync(sidMapSqlPath, 'utf8');
  sidMapSql = sidMapSql.replace(/^PRAGMA\s+[^;]+;\s*/gmi, '');
  targetDb.exec(sidMapSql);

  // Restore WAL mode in case imported pragmas altered journal mode
  targetDb.pragma('journal_mode = WAL');

  const hvscCount = targetDb.prepare('SELECT COUNT(*) as count FROM hvsc_files').get()?.count || 0;
  const sidMapCount = targetDb.prepare('SELECT COUNT(*) as count FROM sid_release_map').get()?.count || 0;
  console.log(chalk.green(`✔ HVSC catalog imported (${hvscCount.toLocaleString()} files, ${sidMapCount.toLocaleString()} release mappings).`));
}

const shouldImportHvsc = options.importHvsc || options.hvscOnly || options.resetDb || !dbExists || !tableExists(db, 'hvsc_files');
if (shouldImportHvsc) {
  if (options.dryrun) {
    console.log(chalk.cyan('Dry run mode: Skipping HVSC catalog import.'));
  } else {
    importHvscCatalog(db, options);
  }
}

if (options.hvscOnly) {
  console.log(chalk.green('Done. Checkpointing WAL...'));
  db.pragma('wal_checkpoint(TRUNCATE)');
  db.close();
  process.exit(0);
}

// Statements
const insertMusicStmt = db.prepare(`
    INSERT OR REPLACE INTO music (
      directory_id, filename, path, extension, song_id, title, artist, contributor, game, system,
      copyright, release_date, file_size, mtime, raw_meta, image_id, text_ids, soundfont, md5, subtune_count, sort_order
    )
    VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
`);

const insertSubtuneStmt = db.prepare(`
    INSERT INTO subtune (music_id, subtune, title, length_ms, date, sort_order)
    VALUES (?, ?, ?, ?, ?, ?)
`);

const deleteSubtunesStmt = db.prepare('DELETE FROM subtune WHERE music_id = ?');

const findMusicIdStmt = db.prepare('SELECT id FROM music WHERE path = ?');

const insertDirStmt = db.prepare(`
    INSERT INTO directories (parent_id, name, path, image_id, text_ids, sort_order, count, total_size, mtime)
    VALUES (?, ?, ?, ?, ?, ?, 0, 0, ?)
    ON CONFLICT(path) DO UPDATE SET
      parent_id = COALESCE(excluded.parent_id, directories.parent_id),
      name = excluded.name,
      image_id = excluded.image_id,
      text_ids = excluded.text_ids,
      sort_order = excluded.sort_order,
      mtime = excluded.mtime
`);

const findDirIdStmt = db.prepare('SELECT id FROM directories WHERE path = ?');

const updateDirStatsStmt = db.prepare('UPDATE directories SET count = ?, total_size = ? WHERE id = ?');

const findImageStmt = db.prepare('SELECT id FROM images WHERE path = ?');
const insertImageStmt = db.prepare('INSERT INTO images (path) VALUES (?)');

const findTextStmt = db.prepare('SELECT id FROM texts WHERE hash = ?');
const insertTextStmt = db.prepare('INSERT INTO texts (hash, content) VALUES (?, ?)');

const updateSortOrderStmt = db.prepare('UPDATE music SET sort_order = ? WHERE path = ?');

function findMatchingPrefix(rootDir, relativeFilter) {
  const exactPath = path.join(rootDir, relativeFilter);
  if (fs.existsSync(exactPath)) {
    return [relativeFilter];
  }

  const parts = relativeFilter.split(/[/\\]+/).filter(Boolean);
  let currentDirs = [''];

  for (let i = 0; i < parts.length; i++) {
    const part = parts[i];
    const isLast = (i === parts.length - 1);
    const nextDirs = [];

    for (const cur of currentDirs) {
      const dirPath = path.join(rootDir, cur);
      try {
        const entries = fs.readdirSync(dirPath, { withFileTypes: true });
        for (const entry of entries) {
          if (entry.name.startsWith('.')) continue;
          if (entry.name === 'node_modules') continue;

          if (isLast) {
            if (entry.name.startsWith(part)) {
              nextDirs.push(path.join(cur, entry.name));
            }
          } else {
            if (entry.isDirectory() && (entry.name === part || entry.name.startsWith(part))) {
              nextDirs.push(path.join(cur, entry.name));
            }
          }
        }
      } catch (e) {
        // Ignore unreadable directories
      }
    }
    currentDirs = nextDirs;
    if (currentDirs.length === 0) break;
  }

  return currentDirs;
}

// Calculate scan root
const scanRelativeBase = options.filter ? options.filter.replace(/^[/\\]+/, '').replace(/[/\\]+$/, '').split(/[/\\]+/).join(path.sep) : '';
const scanTarget = scanRelativeBase ? path.join(CATALOG_DIR, scanRelativeBase) : CATALOG_DIR;

let matchingPrefixes = [];
if (scanRelativeBase) {
  if (fs.existsSync(scanTarget)) {
    matchingPrefixes = [scanRelativeBase];
  } else {
    matchingPrefixes = findMatchingPrefix(CATALOG_DIR, scanRelativeBase);
    if (matchingPrefixes.length === 0) {
      console.error(chalk.red(`Error: Scan path does not exist: ${scanTarget}`));
      process.exit(1);
    }
  }
}

if (scanRelativeBase && matchingPrefixes.length > 0 && !fs.existsSync(scanTarget)) {
  console.log(chalk.green(`Scanning catalog matching prefix "${scanRelativeBase}" (${matchingPrefixes.length} match${matchingPrefixes.length === 1 ? '' : 'es'})...`));
} else {
  console.log(chalk.green(`Scanning ${scanTarget}...`));
}
if (options.dryrun) console.log(chalk.cyan('Dry run mode: Database will not be modified.'));

// Pre-fetch existing files for incremental update and stats
const existingFiles = new Map();
if (!options.resetDb) {
  if (options.verbose) console.log(chalk.cyan('Loading existing file cache...'));
  try {
    const rows = db.prepare('SELECT path, mtime, sort_order FROM music').all();
    for (const row of rows) {
      existingFiles.set(row.path, { mtime: row.mtime, sort_order: row.sort_order });
    }
    if (options.verbose) console.log(chalk.cyan(`Loaded ${existingFiles.size} existing entries.`));
  } catch (err) {
    // ignore
  }
}

let count = 0;
let processed = 0;
let skipped = 0;
let addedCount = 0;
let modifiedCount = 0;
const processedSamples = [];
const seenPaths = new Set();

// --- Helper Functions ---

function replaceRomanWithArabic(str) {
  try {
    return str.replace(ROMAN_NUMERAL_REGEX, (_, match) => String(toArabic(match)).padStart(4, '0'));
  } catch (e) {
    return str;
  }
}

function detectSystemFromPath(p) {
  const parts = p.split(path.sep);
  if (parts.length > 0) return parts[0];
  return 'Unknown';
}

function getSidecarFiles(dirPath) {
  try {
    return fs.readdirSync(dirPath, { withFileTypes: true });
  } catch (e) {
    return [];
  }
}

function findArt(entries, dirPath) {
  // Prioritize "front" images
  const frontImg = entries.find(e => e.isFile() && FRONT_ART_REGEX.test(e.name) && IMAGE_EXT_REGEX.test(e.name));
  if (frontImg) {
    return path.join(dirPath, frontImg.name);
  }

  // Fallback to any image
  const img = entries.find(e => e.isFile() && IMAGE_EXT_REGEX.test(e.name));
  return img ? path.join(dirPath, img.name) : null;
}

function findTextInEntries(entries, fullPath) {
  return entries
    .filter(e => e.isFile() && TEXT_EXT_REGEX.test(e.name))
    .map(e => path.join(fullPath, e.name));
}

function findSpecificSidecar(entries, baseName, extensions) {
  const regex = new RegExp(`^${escapeRegExp(baseName)}\\.(${extensions.join('|')})$`, 'i');
  const match = entries.find(e => e.isFile() && regex.test(e.name));
  return match ? match.name : null;
}

function escapeRegExp(string) {
  return string.replace(/[.*+?^${}()|[\]\\]/g, '\\$&');
}

// Formats that can contain more than one sub-tune in a single file.
// --- DB Helpers ---

function getImageId(imagePath) {
  if (!imagePath) return null;
  const existing = findImageStmt.get(imagePath);
  if (existing) return existing.id;
  const result = insertImageStmt.run(imagePath);
  return result.lastInsertRowid;
}

function getTextId(content) {
  if (!content) return null;
  const hash = crypto.createHash('sha256').update(content).digest('hex');
  const existing = findTextStmt.get(hash);
  if (existing) return existing.id;
  const result = insertTextStmt.run(hash, content);
  return result.lastInsertRowid;
}

// --- Main Recursive Walker ---

async function processDirectory(fullPath, relativePath, parentId = null, parentState = {}) {
  if (limit > 0 && processed >= limit) return { count: 0, size: 0 };

  const entries = getSidecarFiles(fullPath);
  
  // 1. Resolve Directory-Level Metadata (Inheritance)
  let currentImagePath = findArt(entries, relativePath); 
  if (!currentImagePath) currentImagePath = parentState.inheritedImagePath || null;
  
  const currentImageId = !options.dryrun ? getImageId(currentImagePath) : null;

  // Text Inheritance
  let currentTextIds = [];
  const textFiles = findTextInEntries(entries, fullPath);
  
  if (textFiles.length > 0) {
    for (const tf of textFiles) {
      try {
        const content = fs.readFileSync(tf, 'utf8');
        if (!options.dryrun) {
          const id = getTextId(content);
          if (id) currentTextIds.push(id);
        }
      } catch (e) { console.warn(`Failed to read text: ${tf}`); }
    }
  } else {
    currentTextIds = parentState.inheritedTextIds || [];
  }

  // 2. Identify Children for Sorting
  const allChildren = [];
  for (const entry of entries) {
    const entryFullPath = path.join(fullPath, entry.name);
    const entryRelativePath = path.join(relativePath, entry.name);

    if (entry.isDirectory()) {
      allChildren.push({ type: 'dir', name: entry.name, fullPath: entryFullPath, relativePath: entryRelativePath });
    } else {
      const ext = path.extname(entry.name).toLowerCase();
      if (VALID_EXTENSIONS.has(ext)) {
        allChildren.push({ type: 'file', name: entry.name, fullPath: entryFullPath, relativePath: entryRelativePath, ext });
      }
    }
  }

  // 3. Calculate Sort Order
  const arabicMap = {};
  const needsRomanNumeralSort = allChildren.some(item => item.name.match(ROMAN_NUMERAL_NINE_REGEX));
  
  if (needsRomanNumeralSort) {
    allChildren.forEach(item => arabicMap[item.name] = replaceRomanWithArabic(item.name));
  }

  allChildren.sort((a, b) => {
    const strA = needsRomanNumeralSort ? arabicMap[a.name] : a.name;
    const strB = needsRomanNumeralSort ? arabicMap[b.name] : b.name;
    return NUMERIC_COLLATOR.compare(strA, strB);
  });

  allChildren.forEach((item, idx) => item.sortOrder = idx);

  // Filter children for processing
  const children = allChildren.filter(item => {
    if (!scanRelativeBase) return true;
    const rel = item.relativePath;
    const filter = scanRelativeBase;
    if (rel.startsWith(filter)) return true;
    if (item.type === 'dir' && filter.startsWith(rel + path.sep)) return true;
    return false;
  });

  // 4. Save Directory Entry (Initial)
  let currentDirId = null;

  if (!options.dryrun) {
    const dirName = relativePath === '' ? 'ROOT' : path.basename(relativePath);
    const dirStat = fs.statSync(fullPath);
    
    const result = insertDirStmt.run(
      parentId,
      dirName,
      relativePath,
      currentImageId,
      JSON.stringify(currentTextIds),
      parentState.sortOrder || 0,
      dirStat.mtime.toISOString()
    );

    // Fetch the ID (needed because ON CONFLICT UPDATE doesn't return reliable lastInsertRowid)
    const dirRow = findDirIdStmt.get(relativePath);
    currentDirId = dirRow.id;
  }

  // 5. Process Children & Calculate Stats
  const nextParentState = {
    inheritedImagePath: currentImagePath,
    inheritedTextIds: currentTextIds
  };

  let recursiveCount = 0;
  let recursiveSize = 0;

  const files = children.filter(c => c.type !== 'dir');
  const subdirs = children.filter(c => c.type === 'dir');

  // Process files in transaction
  if (files.length > 0) {
    const runFileTx = db.transaction(() => {
      let localCount = 0;
      let localSize = 0;
      for (const child of files) {
        if (limit > 0 && processed >= limit) break;
        const fileSize = processFile(child, currentDirId, entries, currentImagePath, currentTextIds);
        localCount++;
        localSize += fileSize;
      }
      return { count: localCount, size: localSize };
    });
    
    const stats = runFileTx();
    recursiveCount += stats.count;
    recursiveSize += stats.size;
  }

  // Process directories (async recursion)
  for (const child of subdirs) {
    if (limit > 0 && processed >= limit) break;
    if (child.name.startsWith('.')) continue;
    if (child.name === 'node_modules') continue;
    
    // Yield to event loop to allow SIGINT to fire
    await new Promise(r => setImmediate(r));

    const stats = await processDirectory(child.fullPath, child.relativePath, currentDirId, { 
      ...nextParentState, 
      sortOrder: child.sortOrder 
    });
    // console.log(`\rIndexed ${child.relativePath} (${stats.count})\r`);
    recursiveCount += stats.count;
    recursiveSize += stats.size;
  }
    
  // Update stats (skip if this directory is an ancestor of a filtered scan)
  const isFilterAncestor = Boolean(scanRelativeBase && (
    relativePath === '' || scanRelativeBase.startsWith(relativePath + path.sep)
  ));
  if (!options.dryrun && currentDirId !== null && !isFilterAncestor) {
    updateDirStatsStmt.run(recursiveCount, recursiveSize, currentDirId);
  }
  
  return { count: recursiveCount, size: recursiveSize };
}

function processFile(child, directoryId, dirEntries, dirImagePath, dirTextIds) {
  const { fullPath, relativePath, name, ext, sortOrder } = child;
  
  const stat = fs.statSync(fullPath);

  // Check for incremental skip
  if (!options.force && options.skipUnmodified && existingFiles.has(relativePath)) {
    const cached = existingFiles.get(relativePath);
    if (cached.mtime === stat.mtime.toISOString()) {
      // File hasn't changed.
      // Check if sort order needs update
      if (!options.dryrun && cached.sort_order !== sortOrder) {
        updateSortOrderStmt.run(sortOrder, relativePath);
      }
      
      skipped++;
      count++;
      if (!options.verbose && count % 100 === 0) {
        process.stdout.write(`\rProcessed ${count} files (Skipped: ${skipped})...`);
      }
      seenPaths.add(relativePath);
      return stat.size;
    }
  }
  
  const buffer = fs.readFileSync(fullPath);

  // Semantic Hash
  const digest = crypto.createHash('sha256').update(buffer).digest();
  const songId = digest.toString('base64url').substring(0, 8);
  const extension = ext.substring(1);

  // Metadata
  const meta = parseMetadata(buffer, extension, relativePath);
  const title = meta.title || path.basename(name, ext);
  const system = meta.system !== undefined ? meta.system : detectSystemFromPath(relativePath);
  const subtunes = describeSubtunes(extension, meta);

  // --- Sidecar Resolution for File ---
  
  // 1. Image
  const baseName = path.basename(name, ext);
  const specificImageName = findSpecificSidecar(dirEntries, baseName, ['png', 'jpg', 'jpeg', 'gif']);
  const finalImagePath = specificImageName ? path.join(path.dirname(relativePath), specificImageName) : dirImagePath;
  
  const finalImageId = !options.dryrun ? getImageId(finalImagePath) : null;

  // 2. Text
  const specificTextName = findSpecificSidecar(dirEntries, baseName, ['txt', 'text', 'doc']);
  let finalTextIds = dirTextIds;
  
  if (specificTextName) {
    try {
      const content = fs.readFileSync(path.join(path.dirname(fullPath), specificTextName), 'utf8');
      if (!options.dryrun) {
        const id = getTextId(content);
        if (id) {
          finalTextIds = [id, ...dirTextIds];
        }
      }
    } catch (e) {}
  }

  // 3. Soundfont (MIDI only)
  let soundfont = null;
  if (extension === 'mid' || extension === 'midi') {
    const header = buffer.subarray(0, 1024).toString('utf8'); 
    const match = header.match(SF2_REGEX);
    
    if (match && match[1]) {
      const sfName = match[1] + '.sf2';
      if (dirEntries.find(e => e.name === sfName)) {
        soundfont = path.join(path.dirname(relativePath), sfName);
      }
    }
    
    if (!soundfont) {
      const sfName = findSpecificSidecar(dirEntries, baseName, ['sf2']);
      if (sfName) {
        soundfont = path.join(path.dirname(relativePath), sfName);
      }
    }

    if (!soundfont) {
      const anySf2 = dirEntries.find(e => e.name.toLowerCase().endsWith('.sf2'));
      if (anySf2) {
        soundfont = path.join(path.dirname(relativePath), anySf2.name);
      }
    }
  }

  // 4. MD5 (Tracker only)
  let md5 = null;
  if (['it', 's3m', 'xm', 'mod'].includes(extension)) {
    md5 = crypto.createHash('md5').update(buffer).digest('hex');
  }

  if (options.verbose) {
    console.log(chalk.green(`[ADD] ${relativePath}`));
  }

  if (!options.dryrun) {
    // INSERT OR REPLACE deletes and reinserts the music row. With foreign keys
    // enforced that rejects the old subtune children, so clear them first and
    // rewrite both below.
    const previousMusicRow = findMusicIdStmt.get(relativePath);
    if (previousMusicRow) {
      deleteSubtunesStmt.run(previousMusicRow.id);
    }

    insertMusicStmt.run(
      directoryId,
      name,
      relativePath,
      extension,
      songId,
      title,
      meta.artist || null,
      meta.contributor || null,
      meta.game || null,
      system,
      meta.copyright || null,
      meta.date || null,
      stat.size,
      stat.mtime.toISOString(),
      JSON.stringify(meta),
      finalImageId,
      JSON.stringify(finalTextIds),
      soundfont,
      md5,
      subtunes.length || 1,
      sortOrder
    );

    const musicRow = findMusicIdStmt.get(relativePath);
    if (musicRow) {
      subtunes.forEach((sub, i) => {
        insertSubtuneStmt.run(musicRow.id, sub.subtune, sub.title, sub.lengthMs, sub.date, i);
      });
    }
  }

  if (existingFiles.has(relativePath)) {
    modifiedCount++;
  } else {
    addedCount++;
  }

  seenPaths.add(relativePath);
  processed++;
  if (processedSamples.length < 5) {
    processedSamples.push(relativePath);
  } else if (processed % 5 === 0) {
    const r = Math.floor(Math.random() * processed);
    if (r < 5) {
      processedSamples[r] = relativePath;
    }
  }

  count++;
  if (!options.verbose && count % 100 === 0) {
    process.stdout.write(`\rProcessed ${count} files (Skipped: ${skipped})...`);
  }
  
  return stat.size;
}

// Start
const startTime = Date.now();
// Start with root
processDirectory(CATALOG_DIR, '')
  .then(() => {
    console.log(chalk.green(`\nDone in ${((Date.now() - startTime) / 1000).toFixed(2)}s`));
    
    // Identify deleted files
    const pathsToDelete = [];
    if (limit === 0 || processed < limit) {
      for (const [p] of existingFiles) {
        if (!seenPaths.has(p)) {
          // Only delete if it falls within the current scan filter
          let inScope = true;
          if (scanRelativeBase) {
             inScope = p.startsWith(scanRelativeBase);
          }
          
          if (inScope) {
             pathsToDelete.push(p);
          }
        }
      }
    }

    if (pathsToDelete.length > 0) {
      if (options.verbose) console.log(chalk.yellow(`Found ${pathsToDelete.length} orphaned entries...`));
      
      if (!options.dryrun) {
        const deleteStmt = db.prepare('DELETE FROM music WHERE path = ?');
        const deleteTransaction = db.transaction((paths) => {
          for (const p of paths) {
            // Delete sub-tune children first: databases created before the FK
            // gained ON DELETE CASCADE would otherwise reject the parent delete.
            const row = findMusicIdStmt.get(p);
            if (row) deleteSubtunesStmt.run(row.id);
            deleteStmt.run(p);
          }
        });
        deleteTransaction(pathsToDelete);
      }
    }

    if (!options.dryrun) {
      let deletedDirs = 0;
      let changes;
      const deleteEmptyDirsStmt = db.prepare(`
        DELETE FROM directories 
        WHERE NOT EXISTS (SELECT 1 FROM music WHERE directory_id = directories.id) 
          AND NOT EXISTS (SELECT 1 FROM directories AS child WHERE child.parent_id = directories.id)
      `);
      do {
        const info = deleteEmptyDirsStmt.run();
        changes = info.changes;
        deletedDirs += changes;
      } while (changes > 0);

      if (deletedDirs > 0) {
        console.log(chalk.yellow(`Removed ${deletedDirs} orphaned directories...`));
      }
    }

    let finalCount = 0;
    if (!options.dryrun) {
      try {
        const row = db.prepare('SELECT COUNT(*) as count FROM music').get();
        finalCount = row ? row.count : 0;
      } catch (e) {}
    } else {
      finalCount = initialCount + addedCount - pathsToDelete.length;
    }

    const removedCount = pathsToDelete.length;

    console.log(chalk.gray('───────────────────────────────────────────────────'));
    console.log('Run Summary');
    console.log(chalk.gray('───────────────────────────────────────────────────'));
    console.log(`Total songs before: ${chalk.white(initialCount)}`);
    console.log(`Added:              ${chalk.green(addedCount)}`);
    console.log(`Modified:           ${chalk.yellow(modifiedCount)}`);
    console.log(`Removed:            ${chalk.red(removedCount)}`);
    console.log(`Total songs after:  ${chalk.white(finalCount)}`);
    console.log(chalk.gray('───────────────────────────────────────────────────'));

    if (processedSamples.length > 0) {
      console.log(chalk.cyan('Sample of processed files:'));
      processedSamples.forEach(p => console.log(` - ${p}`));
    }

    if (!options.dryrun) {
      // Only a build that scanned the whole catalog can vouch that every
      // multi-song file now has its rows. A run cut short by --limit (or a
      // filter) has not seen every file, so it must not mark the backfill done.
      if (!options.filter && (limit === 0 || processed < limit)) db.pragma(`user_version = ${SUBTUNE_BACKFILL_VERSION}`);
      console.log(`Database saved to ${DB_PATH}`);
    }
  })
  .catch(err => {
    console.error(chalk.red('Error during scan:'), err);
  })
  .finally(() => {
    // Rebuild the search index last, here rather than in the happy path: a run
    // that aborts part way still leaves `song`/`song_fts` consistent with
    // music + subtune, the way the triggers this replaced kept the index in sync
    // at every write. A dry run only writes to a throwaway copy, and a rebuild
    // failure must not mask whatever error brought us here.
    if (!options.dryrun) {
      try {
        rebuildSongs();
      } catch (e) {
        console.error(chalk.red('Could not rebuild the song index:'), e.message);
      }
    }
    db.pragma('wal_checkpoint(TRUNCATE)'); // Forces all WAL data into the .db file
    db.close();
    if (dryRunDbPath) {
      for (const f of [dryRunDbPath, `${dryRunDbPath}-wal`, `${dryRunDbPath}-shm`]) {
        try { fs.rmSync(f, { force: true }); } catch (e) { /* ignore */ }
      }
    }
  });

module.exports = {
  importHvscCatalog,
};
