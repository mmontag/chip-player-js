const fs = require('fs');
const path = require('path');
const crypto = require('crypto');
const Database = require('better-sqlite3');
const { parseMetadata } = require('./metadata-parsers');
const { Command } = require('commander');
const chalk = require('chalk');
const { toArabic } = require('roman-numerals');

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
  .parse(process.argv);

const options = program.opts();
const limit = parseInt(options.limit);

// Config
const CATALOG_DIR = path.resolve(__dirname, '../catalog');
const DB_PATH = path.resolve(__dirname, '../server/catalog.db');
const VALID_EXTENSIONS = new Set([
  '.ay', '.gbs', '.it', '.mdx', '.mid', '.midi', '.miniusf',
  '.mod', '.nsf', '.nsfe', '.spc', '.s3m', '.v2m', '.vgm', '.vgz', '.xm'
]);

// Regex for Roman Numeral Sort
const romanNumeralNineRegex = /\bix\b/i;
const romanNumeralRegex = /\b([IVXLC]+|[ivxlc]+)([-.,) ]|$)/; 
const NUMERIC_COLLATOR = new Intl.Collator(undefined, { numeric: true, sensitivity: 'base' });

// Initialize DB
const db = new Database(DB_PATH);

if (options.resetDb) {
  if (options.verbose) console.log(chalk.yellow('Resetting database tables...'));
  db.exec(`
    DROP TABLE IF EXISTS music;
    DROP TABLE IF EXISTS directories;
    DROP TABLE IF EXISTS images;
    DROP TABLE IF EXISTS texts;
    DROP TABLE IF EXISTS music_fts;
  `);
}

// 1. Create Tables
db.exec(`
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
    game TEXT,
    system TEXT,
    copyright TEXT,
    file_size INTEGER,
    mtime TEXT,
    raw_meta TEXT,
    image_id INTEGER,            -- Resolved image for this file
    text_ids TEXT,               -- JSON array of text IDs
    soundfont TEXT,              -- Relative path to soundfont
    md5 TEXT,                    -- MD5 hash for tracker modules
    sort_order INTEGER DEFAULT 0,
    FOREIGN KEY(directory_id) REFERENCES directories(id),
    FOREIGN KEY(image_id) REFERENCES images(id)
  );
  
  -- FTS Virtual Table
  CREATE VIRTUAL TABLE IF NOT EXISTS music_fts USING fts5(
    title, artist, game, system, filename, path, 
    content='music', content_rowid='id', 
    prefix='1 2 3'
  );
  
  -- Triggers
  CREATE TRIGGER IF NOT EXISTS music_ai AFTER INSERT ON music BEGIN
    INSERT INTO music_fts(rowid, title, artist, game, system, filename, path) 
    VALUES (new.id, new.title, new.artist, new.game, new.system, new.filename, new.path);
  END;

  CREATE TRIGGER IF NOT EXISTS music_ad AFTER DELETE ON music BEGIN
    INSERT INTO music_fts(music_fts, rowid, title, artist, game, system, filename, path) 
    VALUES ('delete', old.id, old.title, old.artist, old.game, old.system, old.filename, old.path);
  END;

  CREATE TRIGGER IF NOT EXISTS music_au AFTER UPDATE ON music BEGIN
    INSERT INTO music_fts(music_fts, rowid, title, artist, game, system, filename, path) 
    VALUES ('delete', old.id, old.title, old.artist, old.game, old.system, old.filename, old.path);
    INSERT INTO music_fts(rowid, title, artist, game, system, filename, path) 
    VALUES (new.id, new.title, new.artist, new.game, new.system, new.filename, new.path);
  END;

  CREATE INDEX IF NOT EXISTS idx_directories_parent ON directories(parent_id);
  CREATE INDEX IF NOT EXISTS idx_music_directory ON music(directory_id);
  CREATE INDEX IF NOT EXISTS idx_music_sort ON music(sort_order);
  CREATE INDEX IF NOT EXISTS idx_music_songid ON music(song_id);
  CREATE INDEX IF NOT EXISTS idx_directories_sort ON directories(sort_order);
`);

// Statements
const insertMusicStmt = db.prepare(`
    INSERT OR REPLACE INTO music (
      directory_id, filename, path, extension, song_id, title, artist, game, system, 
      copyright, file_size, mtime, raw_meta, image_id, text_ids, soundfont, md5, sort_order
    )
    VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
`);

const insertDirStmt = db.prepare(`
    INSERT OR REPLACE INTO directories (parent_id, name, path, image_id, text_ids, sort_order, count, total_size, mtime)
    VALUES (?, ?, ?, ?, ?, ?, 0, 0, ?)
`);

const updateDirStatsStmt = db.prepare('UPDATE directories SET count = ?, total_size = ? WHERE id = ?');

const findImageStmt = db.prepare('SELECT id FROM images WHERE path = ?');
const insertImageStmt = db.prepare('INSERT INTO images (path) VALUES (?)');

const findTextStmt = db.prepare('SELECT id FROM texts WHERE hash = ?');
const insertTextStmt = db.prepare('INSERT INTO texts (hash, content) VALUES (?, ?)');

// Calculate scan root
const scanRoot = options.filter ? path.join(CATALOG_DIR, options.filter) : CATALOG_DIR;
const scanRelativeBase = options.filter || '';

if (!fs.existsSync(scanRoot)) {
  console.error(chalk.red(`Error: Scan path does not exist: ${scanRoot}`));
  process.exit(1);
}

console.log(chalk.green(`Scanning ${scanRoot}...`));
if (options.dryrun) console.log(chalk.cyan('Dry run mode: Database will not be modified.'));

let count = 0;
let processed = 0;

// --- Helper Functions ---

function replaceRomanWithArabic(str) {
  try {
    return str.replace(romanNumeralRegex, (_, match) => String(toArabic(match)).padStart(4, '0'));
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

function findImageInEntries(entries, dirPath) {
  const img = entries.find(e => e.isFile() && /\.(gif|png|jpg|jpeg)$/i.test(e.name));
  return img ? path.join(dirPath, img.name) : null;
}

function findTextInEntries(entries, fullPath) {
  return entries
    .filter(e => e.isFile() && /\.(text|txt|doc)$/i.test(e.name))
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

function processDirectory(fullPath, relativePath, parentId = null, parentState = {}) {
  if (limit > 0 && processed >= limit) return { count: 0, size: 0 };

  const entries = getSidecarFiles(fullPath);
  
  // 1. Resolve Directory-Level Metadata (Inheritance)
  let currentImagePath = findImageInEntries(entries, relativePath); 
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
  const children = [];
  for (const entry of entries) {
    const entryFullPath = path.join(fullPath, entry.name);
    const entryRelativePath = path.join(relativePath, entry.name);
    
    if (entry.isDirectory()) {
      children.push({ type: 'dir', name: entry.name, fullPath: entryFullPath, relativePath: entryRelativePath });
    } else {
      const ext = path.extname(entry.name).toLowerCase();
      if (VALID_EXTENSIONS.has(ext)) {
        children.push({ type: 'file', name: entry.name, fullPath: entryFullPath, relativePath: entryRelativePath, ext });
      }
    }
  }

  // 3. Calculate Sort Order
  const arabicMap = {};
  const needsRomanNumeralSort = children.some(item => item.name.match(romanNumeralNineRegex));
  
  if (needsRomanNumeralSort) {
    children.forEach(item => arabicMap[item.name] = replaceRomanWithArabic(item.name));
  }

  children.sort((a, b) => {
    const strA = needsRomanNumeralSort ? arabicMap[a.name] : a.name;
    const strB = needsRomanNumeralSort ? arabicMap[b.name] : b.name;
    return NUMERIC_COLLATOR.compare(strA, strB);
  });

  children.forEach((item, idx) => item.sortOrder = idx);

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
    currentDirId = result.lastInsertRowid;
  }

  // 5. Process Children & Calculate Stats
  const nextParentState = {
    inheritedImagePath: currentImagePath,
    inheritedTextIds: currentTextIds
  };

  let recursiveCount = 0;
  let recursiveSize = 0;

  const runTransaction = db.transaction(() => {
    for (const child of children) {
      if (limit > 0 && processed >= limit) return;

      if (child.type === 'dir') {
        const stats = processDirectory(child.fullPath, child.relativePath, currentDirId, { 
          ...nextParentState, 
          sortOrder: child.sortOrder 
        });
        recursiveCount += stats.count;
        recursiveSize += stats.size;
      } else {
        const fileSize = processFile(child, currentDirId, entries, currentImagePath, currentTextIds);
        recursiveCount++;
        recursiveSize += fileSize;
      }
    }
    
    // Update stats
    if (!options.dryrun && currentDirId !== null) {
      updateDirStatsStmt.run(recursiveCount, recursiveSize, currentDirId);
    }
  });
  
  runTransaction();
  
  return { count: recursiveCount, size: recursiveSize };
}

function processFile(child, directoryId, dirEntries, dirImagePath, dirTextIds) {
  const { fullPath, relativePath, name, ext, sortOrder } = child;
  
  const buffer = fs.readFileSync(fullPath);
  const stat = fs.statSync(fullPath);

  // Semantic Hash
  const digest = crypto.createHash('sha256').update(buffer).digest();
  const songId = digest.toString('base64url').substring(0, 8);
  const extension = ext.substring(1);

  // Metadata
  const meta = parseMetadata(buffer, extension);
  const title = meta.title || path.basename(name, ext);
  const system = meta.system || detectSystemFromPath(relativePath);

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
    const sf2Regex = /SF2=(.+?)\.sf2/;
    const header = buffer.subarray(0, 1024).toString('utf8'); 
    const match = header.match(sf2Regex);
    
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
    insertMusicStmt.run(
      directoryId,
      name,
      relativePath,
      extension,
      songId,
      title,
      meta.artist || null,
      meta.game || null,
      system,
      meta.copyright || null,
      stat.size,
      stat.mtime.toISOString(),
      JSON.stringify(meta),
      finalImageId,
      JSON.stringify(finalTextIds),
      soundfont,
      md5,
      sortOrder
    );
  }

  processed++;
  count++;
  if (!options.verbose && count % 100 === 0) {
    process.stdout.write(`\rProcessed ${count} files...`);
  }
  
  return stat.size;
}

// Start
const startTime = Date.now();
// Start with root
processDirectory(scanRoot, scanRelativeBase);

console.log(chalk.green(`\nDone in ${((Date.now() - startTime) / 1000).toFixed(2)}s`));
console.log(`Total Processed: ${processed}`);

if (!options.dryrun) {
  console.log(`Database saved to ${DB_PATH}`);
}
