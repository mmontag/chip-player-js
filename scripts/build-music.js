const fs = require('fs');
const path = require('path');
const crypto = require('crypto');
const Database = require('better-sqlite3');
const { parseMetadata } = require('./metadata-parsers');
const { Command } = require('commander');
const chalk = require('chalk');

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
// Pointing to the root 'catalog' allows the filter to select specific subfolders like 'Battle of the Bits'
const CATALOG_DIR = path.resolve(__dirname, '../catalog');
const DB_PATH = path.resolve(__dirname, './untracked/catalog.db');
const VALID_EXTENSIONS = new Set([
  '.ay', '.gbs', '.it', '.mdx', '.mid', '.midi', '.miniusf',
  '.mod', '.nsf', '.nsfe', '.spc', '.s3m', '.v2m', '.vgm', '.vgz', '.xm'
]);

// Initialize DB
const db = new Database(DB_PATH);

if (options.resetDb) {
  if (options.verbose) console.log(chalk.yellow('Resetting database tables...'));
  db.exec(`
    DROP TABLE IF EXISTS songs;
    DROP TABLE IF EXISTS songs_fts;
  `);
}

// 1. Create Tables
// Note: PRIMARY KEY and UNIQUE constraints automatically create implicit indexes in SQLite.
db.exec(`
  CREATE TABLE IF NOT EXISTS songs (
    path TEXT PRIMARY KEY,       -- Filesystem path is the true unique ID
    extension TEXT,              -- Extension (moved after path)
    song_id TEXT,                -- Truncated hash (not unique)
    title TEXT,
    artist TEXT,
    game TEXT,
    system TEXT,
    copyright TEXT,
    file_size INTEGER,
    mtime TEXT,
    raw_meta TEXT
  );
  
  -- FTS Virtual Table for fast search
  CREATE VIRTUAL TABLE IF NOT EXISTS songs_fts USING fts5(
    title, artist, game, system, content='songs', content_rowid='rowid'
  );
  
  -- Trigger to keep FTS in sync
  CREATE TRIGGER IF NOT EXISTS songs_ai AFTER INSERT ON songs BEGIN
    INSERT INTO songs_fts(rowid, title, artist, game, system) VALUES (new.rowid, new.title, new.artist, new.game, new.system);
  END;

  -- Performance Indexes for Browsing/Filtering
  CREATE INDEX IF NOT EXISTS idx_song_id ON songs(song_id);
  -- CREATE INDEX IF NOT EXISTS idx_system ON songs(system);
  -- CREATE INDEX IF NOT EXISTS idx_game ON songs(game);
  CREATE INDEX IF NOT EXISTS idx_artist ON songs(artist);
  CREATE INDEX IF NOT EXISTS idx_extension ON songs(extension);
`);

// Statements
const existsStmt = db.prepare('SELECT 1 FROM songs WHERE path = ?');

const insertStmt = db.prepare(`
    INSERT OR REPLACE INTO songs (path, extension, song_id, title, artist, game, system, copyright, file_size, mtime, raw_meta)
    VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
`);

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
let skipped = 0;
let processed = 0;

function walk(dir, relativeDir = '') {
  if (limit > 0 && processed >= limit) return;

  const files = fs.readdirSync(dir);

  for (const file of files) {
    if (limit > 0 && processed >= limit) return;

    const fullPath = path.join(dir, file);
    const stat = fs.statSync(fullPath);
    const relativePath = path.join(relativeDir, file);

    if (stat.isDirectory()) {
      walk(fullPath, relativePath);
    } else {
      const ext = path.extname(file).toLowerCase();
      if (!VALID_EXTENSIONS.has(ext)) continue;

      processFile(fullPath, relativePath, ext, stat);

      count++;
      if (!options.verbose && count % 100 === 0) {
        process.stdout.write(`\rScanned ${count} files (Processed: ${processed}, Skipped: ${skipped})...`);
      }
    }
  }
}

function processFile(fullPath, relativePath, ext, stat) {
  // 1. Existence Check (Fast Skip)
  if (!options.force && !options.resetDb) {
    if (existsStmt.get(relativePath)) {
      if (options.verbose) console.log(chalk.gray(`[SKIP] ${relativePath}`));
      skipped++;
      return;
    }
  }

  // 2. Process File
  const buffer = fs.readFileSync(fullPath);

  // Semantic Hash & ID Generation
  const digest = crypto.createHash('sha256').update(buffer).digest();
  const songId = digest.toString('base64url').substring(0, 8);

  // Format Extension (remove dot)
  const extension = ext.substring(1);

  // Extract Metadata
  const meta = parseMetadata(buffer, extension);

  // Fallbacks
  const title = meta.title || path.basename(relativePath, ext);
  const system = meta.system || detectSystemFromPath(relativePath);

  if (options.verbose) {
    console.log(chalk.green(`[NEW/UPD] ${relativePath}`));
    console.log(`  ID: ${chalk.cyan(songId)} | System: ${chalk.magenta(system)}`);
    if (meta.title) console.log(`  Title: ${chalk.white(meta.title)}`);
  }

  if (!options.dryrun) {
    insertStmt.run(
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
      JSON.stringify(meta)
    );
  }

  processed++;
}

function detectSystemFromPath(p) {
  const parts = p.split(path.sep);
  if (parts.length > 0) return parts[0];
  return 'Unknown';
}

if (options.dryrun) {
  walk(scanRoot, scanRelativeBase);
} else {
  const runTransaction = db.transaction(() => walk(scanRoot, scanRelativeBase));
  runTransaction();
}

console.log(chalk.green(`\nDone!`));
console.log(`Total Scanned: ${count}`);
console.log(`Processed:     ${processed}`);
console.log(`Skipped:       ${skipped}`);

if (!options.dryrun) {
  console.log(`Database saved to ${DB_PATH}`);
}
