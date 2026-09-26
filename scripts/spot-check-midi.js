#!/usr/bin/env node

/**
 * Chip Player JS - MIDI Metadata Spot-Check Tool
 *
 * Allows deterministic spot-checking and comparison of metadata parsing strategies
 * across catalog MIDI files.
 */

const fs = require('fs');
const path = require('path');
const { Command } = require('commander');
const chalk = require('chalk');
const Database = require('better-sqlite3');
const { parseMetadata } = require('./metadata-parsers');
const {
  guessMetadataFromPath,
  parseMidiInternal,
  MIDI_STRATEGIES,
  MIDI_STRATEGY_MAP,
} = require('./metadata-midi');

const CATALOG_DIR = path.resolve(__dirname, '../catalog');
const DB_PATH = path.resolve(__dirname, '../server/catalog.db');

const program = new Command();

program
  .name('spot-check-midi')
  .description('Spot-checks MIDI metadata parsing strategies across the catalog.')
  .option('-s, --strategy <name>', 'Strategy to test: compare, routed, internal, filepath', 'compare')
  .option('-p, --prefix <path>', 'Catalog directory prefix/filter (e.g. OnlyMIDIs, MIDI)', '')
  .option('-l, --limit <number>', 'Number of files to check', '30')
  .option('--seed <number>', 'Random seed for deterministic sampling', '12345')
  .option('--no-random', 'Process files in order instead of random sampling')
  .option('-d, --diff-only', 'Only show entries where current and new strategy differ')
  .option('--no-db', 'Do not query catalog.db for current entries')
  .parse(process.argv);

const options = program.opts();
const limit = Math.max(1, parseInt(options.limit, 10) || 30);
const seed = parseInt(options.seed, 10) || 12345;
const isRandom = options.random !== false;

// Seeded PRNG: Mulberry32
function createPrng(a) {
  return function() {
    let t = a += 0x6D2B79F5;
    t = Math.imul(t ^ (t >>> 15), t | 1);
    t ^= t + Math.imul(t ^ (t >>> 7), t | 61);
    return ((t ^ (t >>> 14)) >>> 0) / 4294967296;
  };
}

// Fetch file candidates from DB or filesystem
function getCandidates() {
  const normalizedPrefix = options.prefix.replace(/^\/+|\/+$/g, '');

  if (options.db && fs.existsSync(DB_PATH)) {
    const db = new Database(DB_PATH, { readonly: true });
    let rows = [];
    if (normalizedPrefix) {
      rows = db.prepare(`
        SELECT path, title, artist, game, system 
        FROM music 
        WHERE (path = ? OR path LIKE ?) AND extension IN ('mid', 'midi')
        ORDER BY id
      `).all(normalizedPrefix, `${normalizedPrefix}%`);
    } else {
      rows = db.prepare(`
        SELECT path, title, artist, game, system 
        FROM music 
        WHERE extension IN ('mid', 'midi')
        ORDER BY id
      `).all();
    }
    db.close();
    return rows;
  }

  // Fallback to filesystem scan if database is not available
  const scanTarget = normalizedPrefix ? path.join(CATALOG_DIR, normalizedPrefix) : CATALOG_DIR;
  if (!fs.existsSync(scanTarget)) {
    console.error(chalk.red(`Error: Target directory does not exist: ${scanTarget}`));
    process.exit(1);
  }

  const results = [];
  function walk(dir, rel) {
    const entries = fs.readdirSync(dir, { withFileTypes: true });
    for (const e of entries) {
      if (e.name.startsWith('.')) continue;
      const curRel = rel ? `${rel}/${e.name}` : e.name;
      const curFull = path.join(dir, e.name);
      if (e.isDirectory()) {
        walk(curFull, curRel);
      } else {
        const ext = path.extname(e.name).toLowerCase();
        if (ext === '.mid' || ext === '.midi') {
          results.push({ path: curRel, title: null, artist: null, game: null, system: null });
        }
      }
    }
  }
  walk(scanTarget, normalizedPrefix);
  return results;
}

function truncate(str, maxLen) {
  if (!str) return '(none)';
  if (str.length <= maxLen) return str;
  return str.substring(0, maxLen - 1) + '…';
}

function pad(str, len) {
  const visibleLen = (str || '').replace(/\u001b\[[0-9;]*m/g, '').length;
  if (visibleLen >= len) return str;
  return str + ' '.repeat(len - visibleLen);
}

function run() {
  const startTime = Date.now();
  const allCandidates = getCandidates();

  if (allCandidates.length === 0) {
    console.log(chalk.yellow(`No MIDI files found matching prefix: "${options.prefix}"`));
    return;
  }

  let selected = [];
  if (isRandom && allCandidates.length > limit) {
    const prng = createPrng(seed);
    const shuffled = [...allCandidates];
    // Fisher-Yates with seeded PRNG
    for (let i = shuffled.length - 1; i > 0; i--) {
      const j = Math.floor(prng() * (i + 1));
      [shuffled[i], shuffled[j]] = [shuffled[j], shuffled[i]];
    }
    selected = shuffled.slice(0, limit);
  } else {
    selected = allCandidates.slice(0, limit);
  }

  console.log(chalk.cyan.bold(`\nChip Player JS - MIDI Metadata Spot-Check`));
  console.log(chalk.gray(`Catalog files matching: ${allCandidates.length} | Sampling: ${selected.length} | Strategy: ${options.strategy} | Random seed: ${isRandom ? seed : 'off (sequential)'}`));
  if (options.diffOnly) {
    console.log(chalk.gray(`Filter: --diff-only (showing only files where current != bestguess)\n`));
  } else {
    console.log('');
  }

  const titleColWidth = 42;
  const artistColWidth = 32;

  let shownCount = 0;
  let diffCount = 0;

  for (let idx = 0; idx < selected.length; idx++) {
    const item = selected[idx];
    const fullPath = path.join(CATALOG_DIR, item.path);

    if (!fs.existsSync(fullPath)) continue;

    const buffer = fs.readFileSync(fullPath);

    // Compute variations
    const currentMeta = {
      title: item.title || '(none)',
      artist: item.artist || item.game || '(none)',
    };

    const filenameMeta = guessMetadataFromPath(item.path);
    const internalMeta = parseMidiInternal(buffer);
    const bestguessMeta = parseMetadata(buffer, 'mid', item.path, 'routed');

    const fnTitle = filenameMeta.title || '(none)';
    const fnArtist = filenameMeta.artist || filenameMeta.game || '(none)';

    const intTitle = internalMeta.title || '(none)';
    const intArtist = internalMeta.artist || internalMeta.game || '(none)';

    const bestTitle = bestguessMeta.title || '(none)';
    const bestArtist = bestguessMeta.artist || bestguessMeta.game || '(none)';

    const hasDiff = currentMeta.title !== bestTitle || currentMeta.artist !== bestArtist;
    if (hasDiff) diffCount++;

    if (options.diffOnly && !hasDiff) continue;

    shownCount++;

    // Print File Header
    console.log(chalk.bold.white(`${item.path}`));

    // Column Header
    const colHeader = `              ${pad('Title', titleColWidth)}  ${pad('Artist / Context', artistColWidth)}`;
    console.log(chalk.dim(colHeader));

    // Current DB Row
    if (options.db) {
      const curTitleStr = truncate(currentMeta.title, titleColWidth);
      const curArtistStr = truncate(currentMeta.artist, artistColWidth);
      console.log(`  ${chalk.yellow('current:   ')} ${pad(curTitleStr, titleColWidth)}  ${chalk.dim(curArtistStr)}`);
    }

    if (options.strategy === 'compare') {
      // Filename Guesser
      const fnTitleStr = truncate(fnTitle, titleColWidth);
      const fnArtistStr = truncate(fnArtist, artistColWidth);
      console.log(`  ${chalk.blue('filename:  ')} ${pad(fnTitleStr, titleColWidth)}  ${chalk.dim(fnArtistStr)}`);

      // Binary Track 0 Parser
      const intTitleStr = truncate(intTitle, titleColWidth);
      const intArtistStr = truncate(intArtist, artistColWidth);
      console.log(`  ${chalk.magenta('internal:  ')} ${pad(intTitleStr, titleColWidth)}  ${chalk.dim(intArtistStr)}`);

      // Bestguess / Routed
      const bgTitleStr = truncate(bestTitle, titleColWidth);
      const bgArtistStr = truncate(bestArtist, artistColWidth);
      console.log(`  ${chalk.green.bold('bestguess: ')} ${pad(chalk.green(bgTitleStr), titleColWidth)}  ${chalk.green(bgArtistStr)}`);
    } else {
      // Specific Strategy requested
      const chosen = parseMetadata(buffer, 'mid', item.path, options.strategy);
      const chTitle = truncate(chosen.title || '(none)', titleColWidth);
      const chArtist = truncate(chosen.artist || chosen.game || '(none)', artistColWidth);
      console.log(`  ${chalk.green.bold(pad(options.strategy + ':', 11))} ${pad(chalk.green(chTitle), titleColWidth)}  ${chalk.green(chArtist)}`);
    }

    console.log('');
  }

  const duration = ((Date.now() - startTime) / 1000).toFixed(2);
  console.log(chalk.cyan(`─── Summary ───`));
  console.log(`Checked: ${selected.length} files | Shown: ${shownCount} | Discrepancies vs DB: ${diffCount} (${((diffCount / selected.length) * 100).toFixed(1)}%) | Elapsed: ${duration}s\n`);
}

run();
