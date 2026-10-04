/**
 * Hermetic & Idempotent VGMusic Organizer
 *
 * Reads raw VGMusic mirror from source, flattens the deep 3-level folder structure
 * into curated system names, groups files into game subfolders, renames songs to
 * clean song titles (with contributor disambiguation on collisions), preserves
 * original index.html sidecars, and generates index.json manifests.
 */

const fs = require('fs');
const path = require('path');
const glob = require('glob');
const { Command } = require('commander');
const chalk = require('chalk');
const { execSync } = require('child_process');
const { parseIndexHtml } = require('./metadata-vgmusic');

const program = new Command();

program
  .name('build-vgmusic')
  .description('Restructures raw VGMusic mirror into flattened, game-organized folders with clean metadata.')
  .option('-s, --source <path>', 'Path to pristine raw VGMusic source', '/Users/montag/Music/vgmusic.com MIDI')
  .option('-t, --target <path>', 'Path to target catalog directory', '/Users/montag/Music/Chip Archive/vgmusic.com MIDI')
  .option('-d, --dry-run', 'Preview changes without modifying disk', false)
  .option('-v, --verbose', 'Verbose logging output', false)
  .option('--no-init-source', 'Do not copy source from target if source is missing')
  .parse(process.argv);

const options = program.opts();

// Deterministic 57-system mapping table
const SYSTEM_MAP = {
  // Computers
  'computer/amstrad/amstradcpc': 'Amstrad CPC',
  'computer/apple/appleii': 'Apple II',
  'computer/apple/macintosh': 'Apple Macintosh',
  'computer/atari/atari': 'Atari',
  'computer/commodore/amiga': 'Commodore Amiga',
  'computer/commodore/commodore': 'Commodore 64-128',
  'computer/microsoft/windows': 'PC DOS & Windows',
  'computer/miscellaneous/msx': 'MSX',
  'computer/nec/pc-88': 'NEC PC-88',
  'computer/nec/pc-98': 'NEC PC-98',
  'computer/sharp/x68000': 'Sharp X68000',
  'computer/sinclair/spectrum': 'Sinclair ZX Spectrum',
  'computer/tomy/tutor': 'Tomy Tutor',

  // Consoles - 3DO
  'console/3do/3do': '3DO',

  // Consoles - Atari
  'console/atari/2600': 'Atari 2600',
  'console/atari/7800': 'Atari 7800',
  'console/atari/lynx': 'Atari Lynx',

  // Consoles - Coleco / Magnavox / Mattel
  'console/coleco/colecovision': 'ColecoVision',
  'console/magnavox/odyssey2': 'Magnavox Odyssey 2',
  'console/mattel/intellivision': 'Mattel Intellivision',

  // Consoles - Microsoft
  'console/microsoft/xbox': 'Microsoft Xbox',
  'console/microsoft/xbox360': 'Microsoft Xbox 360',
  'console/microsoft/xboxone': 'Microsoft Xbox One',

  // Consoles - NEC
  'console/nec/pcfx': 'NEC PC-FX',
  'console/nec/sgx': 'NEC SuperGrafx',
  'console/nec/tduo': 'NEC TurboDuo',
  'console/nec/tg16': 'NEC TurboGrafx-16',

  // Consoles - Nintendo
  'console/nintendo/3ds': 'Nintendo 3DS',
  'console/nintendo/ds': 'Nintendo DS',
  'console/nintendo/gameboy': 'Nintendo Gameboy',
  'console/nintendo/gamecube': 'Nintendo GameCube',
  'console/nintendo/gba': 'Nintendo Gameboy Advance',
  'console/nintendo/n64': 'Nintendo 64',
  'console/nintendo/nes': 'Nintendo NES',
  'console/nintendo/snes': 'Nintendo SNES',
  'console/nintendo/switch': 'Nintendo Switch',
  'console/nintendo/virtualboy': 'Nintendo Virtual Boy',
  'console/nintendo/wii': 'Nintendo Wii',
  'console/nintendo/wiiu': 'Nintendo Wii U',

  // Consoles - Philips
  'console/philips/cd-i': 'Philips CD-i',

  // Consoles - Sega
  'console/sega/32x': 'Sega 32X',
  'console/sega/dreamcast': 'Sega Dreamcast',
  'console/sega/gamegear': 'Sega Game Gear',
  'console/sega/genesis': 'Sega Genesis',
  'console/sega/master': 'Sega Master System',
  'console/sega/saturn': 'Sega Saturn',
  'console/sega/segacd': 'Sega CD',

  // Consoles - SNK
  'console/snk/neogeo': 'SNK Neo Geo',
  'console/snk/neogeopocket': 'SNK Neo Geo Pocket',

  // Consoles - Sony
  'console/sony/ps1': 'Sony PlayStation',
  'console/sony/ps2': 'Sony PlayStation 2',
  'console/sony/ps3': 'Sony PlayStation 3',
  'console/sony/ps4': 'Sony PlayStation 4',
  'console/sony/psp': 'Sony PlayStation Portable',

  // Other
  'other/miscellaneous/arcade': 'Arcade',
  'other/miscellaneous/medley': '- Medleys -',
  'other/miscellaneous/piano': '- Piano Arrangements -',
};

/**
 * Sanitize characters not allowed on filesystems (Windows, POSIX, macOS).
 */
function sanitizeFilename(name) {
  if (!name) return '';
  return name
    .replace(/[/\\:*?"<>|]/g, (match) => {
      switch (match) {
        case ':': return ' -';
        case '/': return ' -';
        case '\\': return ' -';
        case '?': return '';
        case '*': return '+';
        case '"': return "'";
        case '<': return '(';
        case '>': return ')';
        case '|': return ' -';
        default: return '_';
      }
    })
    .replace(/\s+/g, ' ')
    .replace(/\.+$/, '')
    .trim();
}

/**
 * Strips artificial numeric submission suffixes added by VGMusic (e.g. "Overworld (2)" -> "Overworld").
 */
function extractCanonicalTitle(rawTitle) {
  if (!rawTitle) return '';
  const m = rawTitle.match(/^(.*?)\s*\((\d+)\)$/);
  return m ? m[1].trim() : rawTitle.trim();
}

async function main() {
  console.log(chalk.cyan('----------------------------------------------------'));
  console.log(chalk.cyan('VGMusic.com Collection Organizer'));
  console.log(chalk.cyan('----------------------------------------------------'));
  console.log(`Source (pristine): ${chalk.bold(options.source)}`);
  console.log(`Target (catalog):  ${chalk.bold(options.target)}`);
  console.log(`Dry run:           ${options.dryRun ? chalk.yellow('YES') : chalk.green('NO')}`);
  console.log('');

  // 1. Verify or initialize pristine source directory
  if (!fs.existsSync(options.source)) {
    if (options.initSource && fs.existsSync(options.target)) {
      console.log(chalk.yellow(`Pristine source not found at: ${options.source}`));
      console.log(chalk.yellow(`Copying from target to establish pristine raw mirror...`));
      if (!options.dryRun) {
        fs.mkdirSync(path.dirname(options.source), { recursive: true });
        execSync(`cp -R "${options.target}" "${options.source}"`, { stdio: 'inherit' });
        console.log(chalk.green(`Pristine source created at: ${options.source}`));
      } else {
        console.log(chalk.gray(`[dry-run] Would copy ${options.target} -> ${options.source}`));
      }
    } else {
      console.error(chalk.red(`Error: Source directory does not exist: ${options.source}`));
      process.exit(1);
    }
  }

  // 2. Discover all index.html files in pristine source
  const searchBase = options.dryRun && !fs.existsSync(options.source) ? options.target : options.source;
  const indexFiles = glob.sync('**/index.html', { cwd: searchBase, nodir: true }).sort();

  if (indexFiles.length === 0) {
    console.error(chalk.red(`No index.html files found in ${searchBase}`));
    process.exit(1);
  }

  console.log(chalk.green(`Found ${indexFiles.length} system directories with sidecars.`));
  console.log('');

  let totalSongsPlanned = 0;
  let totalGamesPlanned = 0;
  let collisionsDisambiguated = 0;

  const buildPlan = []; // Array of per-system operations

  for (const relIndexHtml of indexFiles) {
    const rawSysDir = path.dirname(relIndexHtml);
    const flattenedSysName = SYSTEM_MAP[rawSysDir];

    if (!flattenedSysName) {
      console.error(chalk.red(`Unmapped system directory: ${rawSysDir}`));
      process.exit(1);
    }

    const fullIndexHtmlPath = path.join(searchBase, relIndexHtml);
    const { system: sidecarSysTitle, files } = parseIndexHtml(fullIndexHtmlPath);

    // Group files by cleanGame -> canonicalTitleLower -> Array of items
    const gameGroups = new Map();

    for (const [origFn, meta] of files.entries()) {
      const ext = path.extname(origFn) || '.mid';
      const cleanGame = sanitizeFilename(meta.game || 'Unknown Game') || 'Unknown Game';
      const rawTitle = meta.title || path.basename(origFn, ext);
      const canonicalTitle = sanitizeFilename(extractCanonicalTitle(rawTitle)) || 'Untitled';
      const key = canonicalTitle.toLowerCase();

      if (!gameGroups.has(cleanGame)) {
        gameGroups.set(cleanGame, new Map());
      }
      const titleMap = gameGroups.get(cleanGame);
      if (!titleMap.has(key)) {
        titleMap.set(key, []);
      }
      titleMap.get(key).push({ origFn, meta, canonicalTitle, ext });
    }

    const assignedInGame = new Map(); // cleanGame -> Set of targetFilenames (lowercase)
    const targetFileEntries = [];
    const indexJsonFiles = {};

    for (const [cleanGame, titleMap] of gameGroups.entries()) {
      if (!assignedInGame.has(cleanGame)) {
        assignedInGame.set(cleanGame, new Set());
      }
      const usedSet = assignedInGame.get(cleanGame);

      for (const [titleKey, group] of titleMap.entries()) {
        const isMulti = group.length > 1;
        const nullContribCount = group.filter(x => !x.meta.contributor).length;

        for (const item of group) {
          const { origFn, meta, canonicalTitle, ext } = item;
          const cleanContrib = sanitizeFilename(meta.contributor);
          const origBase = path.basename(origFn, ext);

          let targetFn;
          if (!isMulti) {
            // Sole version in game
            targetFn = `${canonicalTitle}${ext}`;
          } else if (cleanContrib) {
            // Multi-version with contributor
            targetFn = `${canonicalTitle} (${cleanContrib})${ext}`;
            collisionsDisambiguated++;
          } else if (nullContribCount === 1) {
            // Sole uncredited version in multi-version group
            targetFn = `${canonicalTitle}${ext}`;
          } else {
            // Multiple uncredited versions, disambiguate with orig web filename
            targetFn = `${canonicalTitle} (${origBase})${ext}`;
            collisionsDisambiguated++;
          }

          // Handle duplicate targetFn within same game (e.g. same contributor sequenced two versions)
          if (usedSet.has(targetFn.toLowerCase())) {
            targetFn = cleanContrib
              ? `${canonicalTitle} (${cleanContrib} - ${origBase})${ext}`
              : `${canonicalTitle} (${origBase})${ext}`;
            collisionsDisambiguated++;
            if (usedSet.has(targetFn.toLowerCase())) {
              targetFn = `${canonicalTitle} (${origBase}_${path.basename(origFn, ext)})${ext}`;
            }
          }
          usedSet.add(targetFn.toLowerCase());

          const srcFilePath = path.join(searchBase, rawSysDir, origFn);
          const targetRelPath = path.join(flattenedSysName, cleanGame, targetFn);

          const systemVal = flattenedSysName.startsWith('-') ? null : flattenedSysName;

          targetFileEntries.push({
            srcFilePath,
            origFn,
            cleanGame,
            targetFn,
            targetRelPath,
            meta: {
              title: canonicalTitle,
              game: meta.game || cleanGame,
              system: systemVal,
              contributor: meta.contributor,
              origFile: origFn,
              origRelPath: path.join(rawSysDir, origFn),
              md5: meta.md5,
            },
          });

          const relKey = `${cleanGame}/${targetFn}`;
          indexJsonFiles[relKey] = {
            title: canonicalTitle,
            game: meta.game || cleanGame,
            system: systemVal,
            contributor: meta.contributor,
            origFile: origFn,
            origRelPath: path.join(rawSysDir, origFn),
            md5: meta.md5,
          };
        }
      }
    }

    totalSongsPlanned += targetFileEntries.length;
    totalGamesPlanned += assignedInGame.size;

    buildPlan.push({
      rawSysDir,
      flattenedSysName,
      fullIndexHtmlPath,
      targetFileEntries,
      indexJsonData: {
        system: flattenedSysName,
        gameCount: assignedInGame.size,
        songCount: targetFileEntries.length,
        files: indexJsonFiles,
      },
    });

    if (options.verbose) {
      console.log(`  [System] ${rawSysDir.padEnd(32)} -> ${flattenedSysName.padEnd(25)} (${assignedInGame.size} games, ${targetFileEntries.length} songs)`);
    }
  }

  console.log(chalk.bold('Reorganization Summary:'));
  console.log(`  Systems:               ${buildPlan.length}`);
  console.log(`  Unique Game Folders:   ${totalGamesPlanned}`);
  console.log(`  Total Songs:           ${totalSongsPlanned}`);
  console.log(`  Disambiguated Tracks:  ${collisionsDisambiguated}`);
  console.log('');

  if (options.dryRun) {
    console.log(chalk.yellow('[dry-run] Plan calculated successfully. No files were modified.'));
    return;
  }

  // 3. Staging and File Copying
  const stagingDir = path.join(path.dirname(options.target), `.vgmusic-staging-${Date.now()}`);
  console.log(chalk.blue(`Staging directory: ${stagingDir}`));
  fs.mkdirSync(stagingDir, { recursive: true });

  let copiedCount = 0;
  const progressStep = Math.max(1, Math.floor(totalSongsPlanned / 20));

  for (const sysPlan of buildPlan) {
    const sysTargetDir = path.join(stagingDir, sysPlan.flattenedSysName);
    fs.mkdirSync(sysTargetDir, { recursive: true });

    // Copy original index.html sidecar
    fs.copyFileSync(sysPlan.fullIndexHtmlPath, path.join(sysTargetDir, 'index.html'));

    // Write machine-readable index.json
    fs.writeFileSync(
      path.join(sysTargetDir, 'index.json'),
      JSON.stringify(sysPlan.indexJsonData, null, 2),
      'utf8'
    );

    // Copy each song into its game directory
    for (const entry of sysPlan.targetFileEntries) {
      const destDir = path.join(stagingDir, sysPlan.flattenedSysName, entry.cleanGame);
      if (!fs.existsSync(destDir)) {
        fs.mkdirSync(destDir, { recursive: true });
      }
      const destFile = path.join(destDir, entry.targetFn);

      if (fs.existsSync(entry.srcFilePath)) {
        fs.copyFileSync(entry.srcFilePath, destFile);
        copiedCount++;
        if (copiedCount % progressStep === 0 || copiedCount === totalSongsPlanned) {
          const pct = Math.round((copiedCount / totalSongsPlanned) * 100);
          process.stdout.write(`\r${chalk.cyan('Copying files: ')} ${copiedCount} / ${totalSongsPlanned} (${pct}%)`);
        }
      } else {
        console.warn(chalk.yellow(`\nWarning: source file missing on disk: ${entry.srcFilePath}`));
      }
    }
  }

  console.log('\n' + chalk.green('All files staged successfully.'));

  // 4. Atomic swap: backup/remove existing target and move staging into place
  console.log(chalk.blue(`Deploying structured archive to target: ${options.target}`));
  const backupDir = path.join(path.dirname(options.target), `.vgmusic-old-${Date.now()}`);

  if (fs.existsSync(options.target)) {
    fs.renameSync(options.target, backupDir);
  }
  fs.renameSync(stagingDir, options.target);

  // Clean up temporary old target directory
  if (fs.existsSync(backupDir)) {
    console.log(chalk.gray(`Cleaning up previous target directory...`));
    execSync(`rm -rf "${backupDir}"`);
  }

  console.log(chalk.green.bold('✓ Reorganization complete!'));
  console.log(`Target catalog folder is ready at: ${chalk.bold(options.target)}`);
}

main().catch((err) => {
  console.error(chalk.red('Fatal error during build-vgmusic:'), err);
  process.exit(1);
});
