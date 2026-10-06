/**
 * Hermetic & Idempotent VGMRips Organizer
 *
 * Scans raw VGMRips archive, remaps system hierarchy to clean catalog standards,
 * normalizes game folder names (underscores -> spaces), extracts archives with
 * exact timestamp preservation, merges local custom rips from VGMRips-local, cleans up
 * legacy flat folders, and destructively removes source ZIPs upon successful extraction.
 */

const fs = require('fs');
const path = require('path');
const glob = require('glob');
const chalk = require('chalk');
const { Command, Option } = require('commander');
const { execFileSync } = require('child_process');

const program = new Command();

program
  .name('build-vgmrips')
  .description('Imports and restructures raw VGMRips mirror into clean, system-organized catalog folders.')
  .option('-s, --source <path>', 'Path to pristine VGMRips source archive', fs.existsSync('/Users/montag/Music/VGMRips') ? '/Users/montag/Music/VGMRips' : '/Users/montag/Music/VGM Rips')
  .option('--local-source <path>', 'Path to local custom rips directory (never deleted)', '/Users/montag/Music/VGMRips-local')
  .option('-t, --target <path>', 'Path to target catalog directory', '/Users/montag/Music/Chip Archive/VGM Rips')
  .option('-d, --dry-run', 'Preview changes without modifying disk', false)
  .option('-v, --verbose', 'Verbose logging output', false)
  .option('--clean-legacy', 'Purge legacy flat duplicate folders from target', true)
  .option('--delete-zip', 'Delete source zip files after extraction', true)
  .addOption(new Option('--no-delete-zip').hideHelp())
  .option('--filter <pattern>', 'Filter to process only specific systems or games matching regex', '')
  .option('-l, --limit <number>', 'Limit processing to first N games', 0)
  .parse(process.argv);

const options = program.opts();
const limit = parseInt(options.limit, 10) || 0;

// Deterministic source folder to target system mappings
const FOLDER_MAP = {
  'GameBoy': 'Nintendo GameBoy',
  'MegaDrive': 'Sega Genesis',
  'TurboGrafx': 'NEC TurboGrafx 16',
  'NES': 'Nintendo NES',
  'NeoGeo': 'SNK Neo Geo',
  'NeoGeoPocket': 'SNK Neo Geo Pocket',
  'SegaPico': 'Sega Pico',
  'WonderSwan': 'Bandai WonderSwan',
  'Pinball': 'Pinball',
  'Computers/Atari': 'Atari ST',
  'Computers/Fujitsu': 'Fujitsu FM Towns',
  'Computers/IBM_PC': 'PC DOS',
  'Computers/MSX': 'MSX',
  'Computers/ZX_Spectrum': 'Sinclair ZX Spectrum',
  'Arcade/Capcom': 'Arcade/Capcom',
  'Arcade/Konami': 'Arcade/Konami',
  'Arcade/Namco': 'Arcade/Namco',
  'Arcade/SegaSys': 'Arcade/Sega',
  'Arcade/Taito': 'Arcade/Taito',
  'Arcade': 'Arcade',
};

// Tag / Metadata fallbacks for Other, root files, and mixed folders
const TAG_MAP = [
  ['Atari ST', 'Atari ST'],
  ['Apple IIgs', 'Apple IIgs'],
  ['Apple II', 'Apple II'],
  ['Atari Lynx', 'Atari Lynx'],
  ['Nintendo Virtual Boy', 'Nintendo Virtual Boy'],
  ['Virtual Boy', 'Nintendo Virtual Boy'],
  ['Amstrad CPC+', 'Amstrad CPC'],
  ['Amstrad CPC', 'Amstrad CPC'],
  ['Sega Game 1000', 'Sega SG-1000'],
  ['Atari 7800', 'Atari 7800'],
  ['Atari 5200', 'Atari 5200'],
  ['Atari 400/800', 'Atari 8-bit'],
  ['Vectrex', 'Vectrex'],
  ['Intellivision', 'Mattel Intellivision'],
  ['Game Boy', 'Nintendo GameBoy'],
  ['MSX2+', 'MSX'],
  ['MSX2', 'MSX'],
  ['MSX', 'MSX'],
  ['PC / DOS', 'PC DOS'],
  ['PC/AT', 'PC DOS'],
  ['DOS', 'PC DOS'],
  ['Sega Mega Drive', 'Sega Genesis'],
  ['TurboGrafx-16', 'NEC TurboGrafx 16'],
  ['Famicom Disk System', 'Nintendo NES'],
  ['Nintendo Entertainment System', 'Nintendo NES'],
  ['Family Computer', 'Nintendo NES'],
  ['NES', 'Nintendo NES'],
  ['Neo-Geo MVS', 'SNK Neo Geo'],
  ['NeoGeo Pocket Color', 'SNK Neo Geo Pocket'],
  ['NeoGeo Pocket', 'SNK Neo Geo Pocket'],
  ['Mega System 32', 'Arcade'],
  ['Sega System 16', 'Arcade/Sega'],
  ['Sega System E', 'Arcade/Sega'],
  ['Tecmo System', 'Arcade'],
  ['Arcade Machine', 'Arcade'],
  ['Arcade', 'Arcade'],
];

// Curated top-level system directory names used to differentiate legacy flat folders
const CURATED_SYSTEMS = new Set([
  'Arcade',
  'Atari 5200',
  'Atari 7800',
  'Atari 8-bit',
  'Atari Lynx',
  'Atari ST',
  'Apple II',
  'Apple IIgs',
  'Amstrad CPC',
  'Bandai WonderSwan',
  'Fujitsu FM Towns',
  'Mattel Intellivision',
  'MSX',
  'NEC PC-88',
  'NEC PC-98',
  'NEC TurboGrafx 16',
  'Nintendo GameBoy',
  'Nintendo NES',
  'Nintendo Virtual Boy',
  'PC DOS',
  'Pinball',
  'Sega Genesis',
  'Sega Pico',
  'Sega SG-1000',
  'Sharp X1',
  'Sharp X68000',
  'Sinclair ZX Spectrum',
  'SNK Neo Geo',
  'SNK Neo Geo Pocket',
  'Vectrex',
]);

/**
 * Normalizes game folder name by replacing underscores with spaces,
 * collapsing redundant whitespace, and trimming.
 */
function normalizeGameDir(rawName) {
  if (!rawName) return '';
  return rawName
    .replace(/_/g, ' ')
    .replace(/\s+/g, ' ')
    .trim();
}

/**
 * Reads internal .txt metadata from a ZIP archive via stream extraction.
 */
function readZipTxt(zipPath) {
  try {
    return execFileSync('/usr/bin/unzip', ['-p', zipPath, '*.txt'], {
      encoding: 'utf8',
      stdio: ['ignore', 'pipe', 'ignore'],
      maxBuffer: 1024 * 1024,
    });
  } catch (e) {
    return '';
  }
}

/**
 * Resolves a ZIP file to its curated system destination directory.
 */
function resolveSystem(relDir, filename, zipPath) {
  if (FOLDER_MAP[relDir]) {
    return FOLDER_MAP[relDir];
  }

  const base = path.basename(filename, '.zip');

  // Split Sharp Computers
  if (relDir === 'Computers/Sharp') {
    if (/x1|x-1/i.test(base)) return 'Sharp X1';
    return 'Sharp X68000';
  }

  // Split NEC Computers
  if (relDir === 'Computers/NEC') {
    if (/pc-88/i.test(base)) return 'NEC PC-88';
    if (/pc-98/i.test(base)) return 'NEC PC-98';
    const txt = readZipTxt(zipPath);
    if (/pc-88/i.test(txt)) return 'NEC PC-88';
    return 'NEC PC-98';
  }

  // Parenthesized platform tag in filename e.g. Game_Foo_(Atari_ST).zip
  const tagMatch = base.match(/\(([^)]+)\)$/);
  const tag = tagMatch ? tagMatch[1].replace(/_/g, ' ') : '';
  if (tag) {
    for (const [k, v] of TAG_MAP) {
      if (tag.toLowerCase().includes(k.toLowerCase())) {
        return v;
      }
    }
  }

  // Check internal .txt header metadata
  const txt = readZipTxt(zipPath);
  if (txt) {
    const lines = txt.split('\n').slice(0, 25);
    for (const line of lines) {
      if (/^system:/i.test(line)) {
        const sysVal = line.replace(/^system:\s*/i, '').trim();
        for (const [k, v] of TAG_MAP) {
          if (sysVal.toLowerCase().includes(k.toLowerCase())) {
            return v;
          }
        }
      }
    }
  }

  return 'Unknown';
}

/**
 * Merges local custom rips from localSource into the target catalog without modifying localSource.
 */
function mergeLocalRips(localSourceDir, targetDir, dryRun, verbose) {
  const protectedBaseNames = new Set();
  if (!fs.existsSync(localSourceDir)) return protectedBaseNames;

  const entries = fs.readdirSync(localSourceDir, { withFileTypes: true });

  for (const entry of entries) {
    if (!entry.isDirectory() || entry.name.startsWith('.')) continue;

    const rawDirName = entry.name;
    const srcGamePath = path.join(localSourceDir, rawDirName);
    const system = resolveSystem('', rawDirName, '');
    const cleanGameDir = normalizeGameDir(rawDirName);
    const destSystemDir = path.join(targetDir, system);
    const destGamePath = path.join(destSystemDir, cleanGameDir);

    protectedBaseNames.add(rawDirName);
    protectedBaseNames.add(cleanGameDir);

    console.log(chalk.cyan(`[Local Rip] Merging custom rip: ${rawDirName}`));
    console.log(chalk.green(`  → Target: ${system}/${cleanGameDir}`));

    if (!dryRun) {
      fs.mkdirSync(destSystemDir, { recursive: true });
      fs.cpSync(srcGamePath, destGamePath, { recursive: true, preserveTimestamps: true });

      // Remove internal zip if present in target to leave pure tracks
      const nestedZips = glob.sync('*.zip', { cwd: destGamePath });
      for (const nz of nestedZips) {
        fs.unlinkSync(path.join(destGamePath, nz));
      }
      console.log(chalk.green(`✔ ${cleanGameDir} successfully merged.`));
    } else {
      console.log(chalk.gray(`  [dry-run] Would copy ${srcGamePath} -> ${destGamePath}`));
    }
  }

  return protectedBaseNames;
}

/**
 * Purges legacy flat duplicate folders from target.
 */
function cleanupLegacyTarget(targetDir, protectedNames, dryRun, verbose) {
  if (!fs.existsSync(targetDir)) return;

  const entries = fs.readdirSync(targetDir, { withFileTypes: true });
  const legacyDirs = [];

  for (const entry of entries) {
    if (!entry.isDirectory() || entry.name.startsWith('.')) continue;

    if (!CURATED_SYSTEMS.has(entry.name) && !protectedNames.has(entry.name)) {
      legacyDirs.push(entry.name);
    }
  }

  if (legacyDirs.length > 0) {
    console.log(chalk.yellow(`[Legacy Cleanup] Found ${legacyDirs.length} obsolete flat folders in target.`));
    for (const d of legacyDirs) {
      const fullLegacyPath = path.join(targetDir, d);
      if (verbose) {
        console.log(chalk.gray(`  Removing legacy folder: ${d}`));
      }
      if (!dryRun) {
        fs.rmSync(fullLegacyPath, { recursive: true, force: true });
      }
    }
    if (!dryRun) {
      console.log(chalk.green(`✔ Cleared ${legacyDirs.length} legacy flat duplicate folders.`));
    } else {
      console.log(chalk.gray(`  [dry-run] Would remove ${legacyDirs.length} legacy folders.`));
    }
  }
}

async function main() {
  console.log(chalk.cyan('===================================================='));
  console.log(chalk.cyan('VGMRips Collection Importer & Restructurer'));
  console.log(chalk.cyan('===================================================='));
  console.log(`Source archive: ${chalk.bold(options.source)}`);
  console.log(`Local source:   ${chalk.bold(options.localSource)}`);
  console.log(`Target catalog: ${chalk.bold(options.target)}`);
  console.log(`Mode:           ${options.deleteZip ? chalk.yellow('Destructive (extract & delete source ZIPs)') : chalk.cyan('Preserve source archives')}`);
  console.log(`Dry run:        ${options.dryRun ? chalk.yellow('YES') : chalk.green('NO')}`);
  if (options.filter) console.log(`Filter:         ${chalk.magenta(options.filter)}`);
  if (limit > 0) console.log(`Limit:          ${chalk.magenta(limit)}`);
  console.log('');

  if (!fs.existsSync(options.source)) {
    console.error(chalk.red(`Error: Source archive path does not exist: ${options.source}`));
    process.exit(1);
  }

  // Step 1: Merge local custom rips from --local-source (never deleted)
  const protectedNames = mergeLocalRips(options.localSource, options.target, options.dryRun, options.verbose);

  // Step 2: Clean up legacy flat duplicate folders
  if (options.cleanLegacy) {
    cleanupLegacyTarget(options.target, protectedNames, options.dryRun, options.verbose);
    console.log('');
  }

  // Step 2: Scan source archive for all ZIPs
  console.log(chalk.blue('Scanning source directory for game archives...'));
  let allZips = glob.sync('**/*.zip', { cwd: options.source, nodir: true }).sort();

  if (allZips.length === 0) {
    console.log(chalk.yellow('No ZIP archives found in source directory. Catalog is already fully imported.'));
    return;
  }

  // Apply optional filter regex
  if (options.filter) {
    const filterRe = new RegExp(options.filter, 'i');
    allZips = allZips.filter((z) => filterRe.test(z));
    console.log(`Filtered to ${chalk.bold(allZips.length)} matching archives.`);
  }

  // Apply optional limit
  if (limit > 0 && allZips.length > limit) {
    allZips = allZips.slice(0, limit);
    console.log(`Processing limited subset: ${chalk.bold(allZips.length)} archives.`);
  }

  console.log(chalk.green(`Discovered ${allZips.length} archives to process.`));
  console.log('');

  // Step 3: Build planning array
  const importPlan = [];
  const systemCounts = {};
  const unresolved = [];

  for (const relZip of allZips) {
    const zipFullPath = path.join(options.source, relZip);
    const relDir = path.dirname(relZip) === '.' ? '' : path.dirname(relZip);
    const filename = path.basename(relZip);
    const baseName = path.basename(relZip, '.zip');

    const system = resolveSystem(relDir, filename, zipFullPath);
    if (system === 'Unknown') {
      unresolved.push(relZip);
      continue;
    }

    const cleanGameDir = normalizeGameDir(baseName);
    const targetGamePath = path.join(options.target, system, cleanGameDir);

    systemCounts[system] = (systemCounts[system] || 0) + 1;

    importPlan.push({
      relZip,
      zipFullPath,
      system,
      cleanGameDir,
      targetGamePath,
    });
  }

  if (unresolved.length > 0) {
    console.error(chalk.red(`Found ${unresolved.length} unmapped archives:`));
    unresolved.slice(0, 10).forEach((u) => console.error(chalk.red(`  - ${u}`)));
    process.exit(1);
  }

  console.log(chalk.bold('Planned Distribution across Systems:'));
  Object.entries(systemCounts)
    .sort((a, b) => b[1] - a[1])
    .forEach(([sys, count]) => {
      console.log(`  ${sys.padEnd(28)} : ${count.toString().padStart(4)} games`);
    });
  console.log('');

  if (options.dryRun) {
    console.log(chalk.yellow('----------------------------------------------------'));
    console.log(chalk.yellow('[dry-run] Sample planned extractions:'));
    importPlan.slice(0, 10).forEach((item) => {
      console.log(chalk.gray(`  ${item.relZip} → ${item.system}/${item.cleanGameDir}/`));
    });
    console.log(chalk.yellow('----------------------------------------------------'));
    console.log(chalk.green(`✔ Dry run completed successfully. No disk modifications were made.`));
    return;
  }

  // Step 4: Execute Extractions and Cleanup
  let processedCount = 0;
  const total = importPlan.length;
  const progressStep = Math.max(1, Math.floor(total / 50));

  console.log(chalk.blue(`Beginning archive extraction to ${options.target}...`));

  for (const item of importPlan) {
    const { zipFullPath, targetGamePath } = item;

    // Create target game directory
    fs.mkdirSync(targetGamePath, { recursive: true });

    // Capture source zip stat for folder mtime synchronization
    const zipStat = fs.statSync(zipFullPath);

    // 1. Extract archive preserving timestamps
    execFileSync('/usr/bin/ditto', ['-x', '-k', zipFullPath, targetGamePath]);

    // 2. Align game folder timestamp with zip archive timestamp
    fs.utimesSync(targetGamePath, zipStat.atime, zipStat.mtime);

    // 3. Destructive cleanup if delete-zip
    if (options.deleteZip) {
      fs.unlinkSync(zipFullPath);
    }

    processedCount++;

    if (options.verbose) {
      console.log(chalk.gray(`  [✔] Extracted: ${item.system}/${item.cleanGameDir}`));
    } else if (processedCount % progressStep === 0 || processedCount === total) {
      const pct = Math.round((processedCount / total) * 100);
      process.stdout.write(`\r${chalk.cyan('Extracting games: ')} ${processedCount} / ${total} (${pct}%)`);
    }
  }

  process.stdout.write('\n');
  console.log(chalk.green.bold('✔ Import and restructuring complete!'));
  console.log(`Processed: ${chalk.bold(processedCount)} games into ${chalk.bold(options.target)}`);

  // Clean up any empty folders left behind in source
  if (options.deleteZip) {
    cleanEmptyDirs(options.source);
  }
}

/**
 * Recursively removes empty directories from source.
 */
function cleanEmptyDirs(dir) {
  if (!fs.existsSync(dir)) return;
  const files = fs.readdirSync(dir);
  for (const file of files) {
    const fullPath = path.join(dir, file);
    if (fs.statSync(fullPath).isDirectory()) {
      cleanEmptyDirs(fullPath);
    }
  }
  const remaining = fs.readdirSync(dir);
  if (remaining.length === 0 || (remaining.length === 1 && remaining[0] === '.DS_Store')) {
    if (remaining[0] === '.DS_Store') {
      fs.unlinkSync(path.join(dir, '.DS_Store'));
    }
    try {
      fs.rmdirSync(dir);
    } catch (e) {
      // Ignore if cannot remove
    }
  }
}

main().catch((err) => {
  console.error(chalk.red('Fatal error during build-vgmrips:'), err);
  process.exit(1);
});
