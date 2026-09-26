/**
 * Chip Player JS - MIDI Metadata Parsing & Strategy Routing
 *
 * Provides multiple metadata extraction strategies:
 * - 'filepath': Heuristic based on directory structure and exact filename
 * - 'internal': Binary SMF parser extracting Track 0/1 sequence names, markers, lyrics
 * - 'routed': Auto-routing based on MIDI_STRATEGY_MAP with intelligent fallbacks
 */

const fs = require('fs');
const path = require('path');
const { cleanString, decodeBuffer } = require('./metadata-utils');

/**
 * Strategy Routing Map
 *
 * Maps catalog directory patterns to specific metadata parsing strategies.
 * Evaluated in order from top to bottom.
 */
const MIDI_STRATEGY_MAP = [
  // Curated collections where filenames and folder structures are human-curated
  { pattern: /^MIDI\//i, strategy: 'filepath' },
  { pattern: /^Classical MIDI\//i, strategy: 'filepath' },
  { pattern: /^Battle of the Bits\//i, strategy: 'filepath' },
  { pattern: /^MIDI Datasets\//i, strategy: 'filepath' },

  // Purpose-built strategy for Roland SMF (sidecar parsing + filepath fallback)
  { pattern: /^Roland SMF MIDI Disks\//i, strategy: 'rolandSmf' },

  // Collections with cryptic 8.3 BBS filenames where Track 0 has rich sequence names
  { pattern: /^OnlyMIDIs\//i, strategy: 'internal' },
  { pattern: /^Sound Canvas MIDI Collection\//i, strategy: 'internal' },
  { pattern: /^Tune 1000 SMF MIDI Disks\//i, strategy: 'internal' },
];

/**
 * Default MIDI metadata parsing strategy used when no pattern in MIDI_STRATEGY_MAP matches.
 */
const DEFAULT_MIDI_STRATEGY = 'filepath';

// --- Strategy Routing & Dispatch ---

const MIDI_STRATEGIES = {
  internal: (buf, relPath) => {
    const meta = parseMidiInternal(buf);
    if (relPath) {
      const pathMeta = guessMetadataFromPath(relPath);
      if (!meta.title) meta.title = pathMeta.title;
      if (!meta.artist && pathMeta.artist) meta.artist = pathMeta.artist;
      if (!meta.game && pathMeta.game) meta.game = pathMeta.game;
      if (pathMeta.game && isGameTitleRedundant(meta.title, pathMeta.game)) {
        meta.title = pathMeta.title;
      }
    }
    return meta;
  },
  filepath: (buf, relPath) => guessMetadataFromPath(relPath),
  rolandSmf: (buf, relPath) => parseMidiRolandSmf(buf, relPath),
  'roland-smf': (buf, relPath) => parseMidiRolandSmf(buf, relPath), // backward compatibility alias
  roland: (buf, relPath) => parseMidiRolandSmf(buf, relPath),
  routed: (buf, relPath) => parseMidiRouted(buf, relPath),
};

/**
 * Strategy 3: Routed / Auto strategy.
 * Evaluates MIDI_STRATEGY_MAP; falls back to DEFAULT_MIDI_STRATEGY ('filepath').
 */
function parseMidiRouted(buf, relPath) {
  if (relPath) {
    for (const rule of MIDI_STRATEGY_MAP) {
      if (rule.pattern.test(relPath)) {
        const strat = MIDI_STRATEGIES[rule.strategy];
        if (strat) return strat(buf, relPath);
      }
    }
    // Default fallback when no route matches
    const defaultStrat = MIDI_STRATEGIES[DEFAULT_MIDI_STRATEGY];
    if (defaultStrat) return defaultStrat(buf, relPath);
  }

  // Fallback when no relPath is provided
  return parseMidiInternal(buf);
}

function parseMidiWithStrategy(buf, relPath = null, strategy = null) {
  if (strategy && MIDI_STRATEGIES[strategy]) {
    return MIDI_STRATEGIES[strategy](buf, relPath);
  }
  return parseMidiRouted(buf, relPath);
}

function parseMIDI(buf, relPath = null, strategy = null) {
  return parseMidiWithStrategy(buf, relPath, strategy);
}

// --- Strategy: Roland SMF (Sidecar Parser with Filepath Fallback) ---

/**
 * Strategy: 'roland-smf'
 * Roland SMF MIDI Disks strategy:
 * - Checks for track-matching .DOC sidecars (Shift-JIS encoded with 【アーティスト】, 【作曲者名】, 【曲名】).
 * - Checks for 'Original Directory.txt' DOS directory listings with "Title / Artist".
 * - Falls back to filepath heuristics (clean album title and single-artist collection detection).
 * - Ensures sequencing studios (Music Brains, Team-khy, Idecs, Tone Factory) and release years are never used as artist.
 */
function parseMidiRolandSmf(buf, relPath) {
  const meta = parseMidiInternal(buf);
  meta.system = null;
  meta.game = null;

  if (!relPath) return meta;

  // Resolve directory and filename
  let fullPath = relPath;
  if (!path.isAbsolute(fullPath)) {
    if (fs.existsSync(fullPath)) {
      fullPath = path.resolve(fullPath);
    } else if (fs.existsSync(path.resolve('./catalog', relPath))) {
      fullPath = path.resolve('./catalog', relPath);
    } else if (fs.existsSync(path.resolve(__dirname, '../catalog', relPath))) {
      fullPath = path.resolve(__dirname, '../catalog', relPath);
    }
  }

  const dirPath = path.dirname(fullPath);
  const dirName = path.basename(dirPath);
  const fileName = path.basename(relPath);
  const ext = path.extname(fileName);
  const rawBase = path.basename(fileName, ext);

  // Extract file code (e.g. "L8010_01" from "L8010_01 - Never an Absolution.MID" or "S1004_02")
  const hyphenIdx = rawBase.indexOf(' - ');
  const fileCode = (hyphenIdx !== -1 ? rawBase.substring(0, hyphenIdx) : rawBase).trim().toUpperCase();
  const filenameTitle = hyphenIdx !== -1 ? rawBase.substring(hyphenIdx + 3).trim() : rawBase;

  let artist = null;
  let sidecarTitle = null;

  // 1. Check sidecar files if directory exists on disk
  if (fs.existsSync(dirPath)) {
    const dirFiles = fs.readdirSync(dirPath);

    // 1a. Try matching .DOC sidecar (e.g. L8010_01.DOC)
    const docName = dirFiles.find(f => {
      const fBase = path.basename(f, path.extname(f)).toUpperCase();
      return fBase === fileCode && f.toLowerCase().endsWith('.doc');
    });

    if (docName) {
      const docBuf = fs.readFileSync(path.join(dirPath, docName));
      const docContent = decodeBuffer(docBuf);

      const artistM = docContent.match(/【[ \t\u3000]*アーティスト[ \t\u3000]*】([^\r\n]+)/);
      const composerM = docContent.match(/【[ \t\u3000]*作曲者名[ \t\u3000]*】([^\r\n]+)/);
      const titleM = docContent.match(/【[ \t\u3000]*曲名[ \t\u3000]*】([^\r\n]+)/);

      if (titleM && titleM[1].trim()) {
        sidecarTitle = titleM[1].trim();
      }

      if (artistM && artistM[1].trim() && artistM[1].trim().toUpperCase() !== 'N/S') {
        artist = artistM[1].trim();
      } else if (composerM && composerM[1].trim() && composerM[1].trim().toUpperCase() !== 'N/S') {
        artist = composerM[1].trim();
      }
    }

    // 1b. Try Original Directory.txt sidecar
    if (!artist) {
      const origTxt = dirFiles.find(f => f.toLowerCase() === 'original directory.txt');
      if (origTxt) {
        const txtBuf = fs.readFileSync(path.join(dirPath, origTxt));
        const content = decodeBuffer(txtBuf);
        const lines = content.split(/[\r\n]+/);
        for (const line of lines) {
          if (line.toUpperCase().includes(fileCode)) {
            const slashIdx = line.indexOf('/');
            if (slashIdx !== -1) {
              artist = line.substring(slashIdx + 1).trim();
            }
            break;
          }
        }
      }
    }
  }

  // 2. Extract album and single-artist candidate from directory name
  let dName = dirName.replace(/^[A-Z0-9]+-[A-Z0-9]+\s*-\s*/i, '').trim();
  while (true) {
    const m = dName.match(/\s*\(([^()]+|\([^()]*\))*\)$/);
    if (!m) break;
    dName = dName.substring(0, m.index).trim();
  }
  const album = dName;

  let dirArtist = null;
  if (/featuring Joe Hisaishi/i.test(album)) {
    dirArtist = 'Joe Hisaishi';
  } else {
    const mCol = album.match(/^(.+?)\s+(?:Collection|Works|Best(?:\s+Collection)?|Complete Best|Super Classics)(?:\s+.*|$)/i);
    if (mCol) {
      const candidate = mCol[1].trim();
      const isGeneric = /^(?:standard\s*jazz|classic\s*orchestral|japanese\s*fusion|french\s*pops|theme\s*park|classical\s*guitar|techno\s*trax|movie\s*themes|the\s*cabaret\s*sounds|fusion\s*&|.*film|.*funk)/i.test(candidate);
      if (!isGeneric) dirArtist = candidate;
    } else if (/^(?:Jamiroquai|Lenny Kravitz)$/i.test(album)) {
      dirArtist = album;
    } else if (album.startsWith('Yes - ')) {
      dirArtist = 'Yes';
    }
  }

  // If a single-artist disk was detected from directory name:
  // Prefer the clean English/Latin directory artist over Japanese sidecar text or sidecar typos
  if (dirArtist) {
    if (!artist) {
      artist = dirArtist;
    } else if (/[\u3000-\u30ff\u4e00-\u9faf]/.test(artist)) {
      // Sidecar artist was in Japanese (Katakana or Kanji)
      artist = dirArtist;
    } else {
      // Check for typo in Latin sidecar artist (e.g. 'Led Zepperin' vs 'Led Zeppelin', 'John Williamas他')
      const normDir = dirArtist.toLowerCase().replace(/[^a-z]/g, '');
      const normSide = artist.toLowerCase().replace(/[^a-z]/g, '');
      if (normSide.startsWith(normDir.substring(0, 4)) && normDir.length >= 5) {
        artist = dirArtist;
      }
    }
  }

  // Sanity check: Ensure artist is never a sequencer studio or release year
  if (artist) {
    artist = artist.replace(/^["'“”]+|["'“”]+$/g, '').trim();
    if (/^(?:Music Brains|Team-khy|Idecs|Tone Factory)$/i.test(artist) || /^\d{4}$/.test(artist)) {
      artist = null;
    }
  }
  if (meta.artist) {
    if (/^(?:Music Brains|Team-khy|Idecs|Tone Factory)$/i.test(meta.artist) || /^\d{4}$/.test(meta.artist)) {
      meta.artist = null;
    }
  }

  // 3. Resolve title
  // Prefer internal SMF title if valid and not just the file code
  let chosenTitle = meta.title;
  if (!chosenTitle || chosenTitle.toUpperCase() === fileCode) {
    chosenTitle = filenameTitle || sidecarTitle;
  }
  meta.title = chosenTitle;

  meta.artist = artist || meta.artist || null;
  meta.game = null;
  meta.system = null;

  return meta;
}

// --- Title Redundancy Helper ---

function normalizeTitle(s) {
  if (!s) return '';
  return s.toLowerCase()
    .replace(/\bviii\b/g, '8')
    .replace(/\bvii\b/g, '7')
    .replace(/\bvi\b/g, '6')
    .replace(/\biv\b/g, '4')
    .replace(/\bv\b/g, '5')
    .replace(/\biii\b/g, '3')
    .replace(/\bii\b/g, '2')
    .replace(/\bi\b/g, '1')
    .replace(/[^a-z0-9]/g, '');
}

function isGameTitleRedundant(internalTitle, game) {
  if (!internalTitle || !game) return false;
  return normalizeTitle(internalTitle) === normalizeTitle(game);
}

// --- Strategy 2: Filename & Path Metadata Guesser ---

function isGameDir(name) {
  if (!name) return false;
  return /^(.+?)\s*\(([^,]+)(?:,\s*([^)]+))?\)$/.test(name.trim());
}

function isTechnicalOrCategoryDir(name) {
  if (!name) return true;
  const n = name.trim();
  // If it has game metadata like (Platform, Year), it's a game, not technical!
  if (isGameDir(n)) return false;

  // Categories like - Arranged -, - Soundtrack -, _FilmThemes, !Others, (by Composer)
  if (/^-.*-$/.test(n) || /^_/.test(n) || /^!/.test(n) || /^\(by\s+/i.test(n)) return true;

  // Numeric (years, track numbers) or memory size (512K, 4MB)
  if (/^\d+$/.test(n) || /^\d+[km]b?$/i.test(n)) return true;

  // Sound hardware, modules, revisions, rips, discs, sections
  if (/^(gm|gmv|gs|xg|xgsong|mt-?32|mt|mt-allsongs|fm|adl|adlib|rol(-[a-z0-9]+)?|gmd|ygm|opl\d*|sc-?\d+([a-z0-9\s]+)?|mu\d+.*|mid|midi|mpu-?401|sb|soundblaster|tandy|speaker|pc speaker|roland|wss|gus|cm-32l|cm-64|lapc.*|genmidi|general midi)$/i.test(n)) {
    return true;
  }
  if (/^awe(\s*\(.*\))?$/i.test(n)) return true;
  if (/^(orig|original|original game rip|game rip.*|extract|previous_version|changed_instruments|init|demo|test|patch test.*|hardtofix|bad-old|dg-mt-rips|wolfmidi)$/i.test(n)) return true;
  if (/^(disc|cd|vol|volume|part|side|bonus)\s*[-_#]?\s*\d*$/i.test(n)) return true;
  if (/^(in-level|cinematic|mission combat songs|cutscenes?|levels?|stage.*|scenes?)$/i.test(n)) return true;
  if (/^(mid-[a-z0-9]+|gm-[a-z0-9]+)$/i.test(n)) return true;
  if (/midi-rips|soundfont|fallback/i.test(n)) return true;
  return false;
}

/**
 * Strategy 2: Filename & Filepath metadata guesser.
 * Uses exact filename as the title (including .mid), and infers artist/game from path.
 * Skips technical sound hardware subdirectories (GM, MT, ROL-GM, Disc 1, etc.)
 */
function guessMetadataFromPath(relPath) {
  if (!relPath) return { system: 'MIDI' };

  const normalized = relPath.replace(/\\/g, '/').replace(/^\/+/, '');
  const parts = normalized.split('/');
  const fileName = parts[parts.length - 1];
  const rootDir = parts[0];

  // Exact filename as title: don't strip anything off, not even .mid
  const title = fileName;
  let artist = null;
  let game = null;
  let system = 'MIDI';

  // Filter non-technical candidate directories between root and file
  const candidateDirs = [];
  for (let i = 1; i < parts.length - 1; i++) {
    if (!isTechnicalOrCategoryDir(parts[i])) {
      candidateDirs.push(parts[i].trim());
    }
  }

  if (rootDir === 'Battle of the Bits') {
    const extIdx = fileName.lastIndexOf('.');
    const baseName = extIdx !== -1 ? fileName.substring(0, extIdx) : fileName;
    const botbMatch = baseName.match(/^BotB\s+\d+\s+(.+?)\s+-\s+(.+)$/i);
    if (botbMatch) {
      artist = botbMatch[1].trim();
    }
    if (candidateDirs.length > 0) game = candidateDirs[0];
  } else if (rootDir === 'MIDI') {
    if (candidateDirs.length > 0) {
      artist = candidateDirs[0].replace(/^"|"$/g, '').trim();
      if (candidateDirs.length > 1) game = candidateDirs[candidateDirs.length - 1];
    }
  } else if (rootDir === 'Classical MIDI') {
    if (candidateDirs.length > 0) {
      artist = candidateDirs[0];
      if (candidateDirs.length > 1) game = candidateDirs[candidateDirs.length - 1];
    }
  } else if (rootDir === 'Contemporary') {
    if (candidateDirs.length > 0) {
      artist = candidateDirs[0];
      if (candidateDirs.length > 1) game = candidateDirs[candidateDirs.length - 1];
    }
  } else if (rootDir === 'Piano E-Competition MIDI') {
    if (candidateDirs.length > 0) {
      artist = candidateDirs[candidateDirs.length - 1];
    }
  } else if (rootDir === 'MIDI Datasets') {
    const dataset = parts[1];
    const subDirs = candidateDirs.filter(d => d !== dataset);
    if (subDirs.length > 0) {
      artist = subDirs[0];
      if (subDirs.length > 1) {
        game = subDirs[1];
      } else {
        game = dataset;
      }
    } else if (dataset === 'GiantMIDI-Piano') {
      const commaTokens = fileName.split(',').map(s => s.trim());
      if (commaTokens.length >= 3) {
        artist = `${commaTokens[0]}, ${commaTokens[1]}`;
      }
      game = dataset;
    } else {
      game = dataset;
    }
  } else if (rootDir === 'Nintendo 64 (SoundFont MIDI)') {
    if (candidateDirs.length > 0) game = candidateDirs[0];
    system = 'Nintendo 64';
  } else if (rootDir === 'Sound Canvas MIDI Collection') {
    if (candidateDirs.length >= 2) {
      artist = candidateDirs[0];
      game = candidateDirs[1];
    } else if (candidateDirs.length === 1) {
      game = candidateDirs[0];
    }
  } else if (rootDir === 'Tune 1000 SMF MIDI Disks') {
    if (candidateDirs.length > 0) {
      const dir = candidateDirs[0];
      const match = dir.match(/^\d+\s*-\s*(.+)$/);
      const album = match ? match[1].trim() : dir;
      game = album;
      artist = album;
    }
  } else if (rootDir === 'Roland SMF MIDI Disks') {
    system = null;
    game = null;
    if (candidateDirs.length > 0) {
      let album = candidateDirs[0].replace(/^[A-Z0-9]+-[A-Z0-9]+\s*-\s*/i, '').trim();
      while (true) {
        const m = album.match(/\s*\(([^()]+|\([^()]*\))*\)$/);
        if (!m) break;
        album = album.substring(0, m.index).trim();
      }
      if (/featuring Joe Hisaishi/i.test(album)) {
        artist = 'Joe Hisaishi';
      } else {
        const mCol = album.match(/^(.+?)\s+(?:Collection|Works|Best(?:\s+Collection)?|Complete Best|Super Classics)(?:\s+.*|$)/i);
        if (mCol) {
          const candidate = mCol[1].trim();
          const isGeneric = /^(?:standard\s*jazz|classic\s*orchestral|japanese\s*fusion|french\s*pops|theme\s*park|classical\s*guitar|techno\s*trax|movie\s*themes|the\s*cabaret\s*sounds|fusion\s*&|.*film|.*funk)/i.test(candidate);
          if (!isGeneric) artist = candidate;
        } else if (/^(?:Jamiroquai|Lenny Kravitz)$/i.test(album)) {
          artist = album;
        } else if (album.startsWith('Yes - ')) {
          artist = 'Yes';
        }
      }
    }
  } else if (rootDir === 'Game MIDI') {
    // Anything in Game MIDI uses the top child directory (skipping category folders like - Arranged - and - Soundtrack -)
    const isSpecialCategory = parts.length > 2 && (parts[1].startsWith('-') || parts[1].startsWith('_'));
    const topChild = isSpecialCategory ? parts[2] : (parts.length > 1 ? parts[1] : null);

    if (topChild) {
      const match = topChild.match(/^(.+?)\s*\(([^,]+)(?:,\s*([^)]+))?\)$/);
      if (match) {
        game = match[1].trim();
        system = match[2].trim();
        artist = match[1].trim();
      } else {
        game = topChild.trim();
        artist = topChild.trim();
      }
    }
  } else if (rootDir === 'Game Mods') {
    if (candidateDirs.length > 0) {
      const match = candidateDirs[0].match(/^(.+?)\s*\(([^,]+)(?:,\s*([^)]+))?\)$/);
      if (match) {
        game = match[1].trim();
        system = match[2].trim();
        artist = match[1].trim();
      } else {
        game = candidateDirs[0];
        artist = candidateDirs[0];
      }
    }
  } else if (rootDir === 'Demo MIDI') {
    if (candidateDirs.length > 0) {
      game = candidateDirs[candidateDirs.length - 1];
      if (candidateDirs.length > 1) artist = candidateDirs[0];
    }
  } else if (rootDir === 'vgmusic.com MIDI') {
    if (parts.length >= 4) {
      system = parts[3].toUpperCase();
    }
  } else {
    // Generic fallback for any other collections
    if (candidateDirs.length > 0) {
      artist = candidateDirs[0];
    }
  }

  const meta = { system, title };
  if (artist) meta.artist = artist;
  if (game) meta.game = game;
  return meta;
}

// --- Strategy 1: Internal Binary SMF Parser ---

const MIDI_BLOCKLIST = new Set([
  'untitled', 'untitled track', 'untitled song', 'tempo', 'tempo track', 'tempo control',
  'meta', 'meta track', 'end track', 'sysex', 'sysex events',
  'reset', 'master parameters', 'control track', 'conductor track', 'conductor',
  'system setup', 'setup', 'start', 'words', 'soft karaoke', 'loopstart', 'loopend',
  'winjammer demo', 'winjammer', 'cakewalk', 'cubase', 'anvil studio', 'noteworthy composer',
  // Generic Instrument Names (often found as track names)
  'piano', 'grand piano', 'acoustic grand piano', 'bright piano', 'electric grand',
  'rhodes', 'e.piano', 'e.piano 1', 'e.piano 2', 'harpsichord', 'clavinet',
  'celesta', 'glockenspiel', 'music box', 'vibraphone', 'marimba', 'xylophone',
  'tubular bells', 'dulcimer', 'drawbar organ', 'percussive organ', 'rock organ',
  'church organ', 'reed organ', 'accordion', 'harmonica', 'bandoneon',
  'nylon guitar', 'steel guitar', 'jazz guitar', 'clean guitar', 'muted guitar',
  'overdrive guitar', 'dist guitar', 'distortion guitar', 'guitar harmonics',
  'acoustic bass', 'finger bass', 'fingered bass', 'fingered bs', 'fng. bass',
  'pick bass', 'picked bass', 'fretless bass', 'slap bass', 'slap bass 1', 'slap bass 2',
  'synth bass', 'synth bass 1', 'synth bass 2', 'synth bass 101', 'rubber bass',
  'violin', 'viola', 'cello', 'contrabass', 'tremolo strings', 'pizzicato',
  'orchestral harp', 'timpani', 'string ensemble 1', 'string ensemble 2',
  'synth strings 1', 'synth strings 2', 'strings', 'string', 'strings l', 'strings r',
  'choir', 'voice oohs', 'synth voice', 'orchestra hit',
  'trumpet', 'trombone', 'tuba', 'muted trumpet', 'french horn', 'brass section',
  'synth brass', 'synth brass 1', 'synth brass 2', 'soprano sax', 'alto sax', 'tenor sax',
  'baritone sax', 'oboe', 'english horn', 'bassoon', 'clarinet', 'piccolo', 'flute',
  'pan flute', 'blown bottle', 'shakuhachi', 'whistle', 'ocarina',
  'lead vox', 'vox', 'lead', 'lead 1', 'lead 2', 'saw lead', 'saw',
  'drums', 'drumset', 'standard kit', 'power kit', 'electronic kit', 'tr-808 kit',
  'jazz kit', 'brush kit', 'orchestra kit', 'percussion', 'rap drumset',
  'melody', 'melody 1', 'melody 2', 'rh melody', 'treble', 'r.h.', 'l.h.',
  'instrument 1', 'track 1', 'track 2',
  // Additional GM instruments and standard track labels
  'bass guitar', 'guitar', 'guitars', 'bass', 'harp', 'harps', 'tubebell', 'pizzicato strings',
  'elec bass finger', 'g.midi drums', 'snare', 'cymbals', 'hi-hat', 'hihat', 'kick',
  'recorder', 'gs reset', 'gs reset - syx', 'gm reset', 'xg reset',
  'organ', 'organs', 'flutes', 'horns', 'trumpets', 'trombones', 'sax', 'oboes', 'clarinets', 'bells',
  'volume', 'pan', 'panpot', 'expression', 'reverb', 'modulation', 'pitch bend',
  'fhorn', 'f.horn', 'fr horn', 'fr. horn', 'french horn', 'vibe', 'vibes', 'pizz', 'pizz.', 'pizzicato',
  'bottleblow', 'bottle blow', 'synbass', 'syn-bass', 'synthbass',
  'square wave', 'sawtooth wave', 'sine wave', 'triangle wave', 'saw wave', 'pulse wave',
  // Common Japanese instrument and track labels
  'ベース', 'ピアノ', 'ドラム', 'ギター', 'ストリングス', 'ブラス', 'リード', 'フルート',
  'ハープ', 'コーラス', 'ボイス', 'シンセ', '伴奏', '和音', '効果音', 'リズム', 'パート', 'トラック',
  'メロディー', 'メロディ', 'メロ', 'ﾒﾛ', 'ﾒﾛﾃﾞｨｰ'
]);

const MIDI_REGEX_BLOCKLIST = [
  /^(?:track|trk|channel|ch|part|staff|st|tk)\s*[-#._]?\s*\d+[a-z]?$/i,
  /^(?:strings?|brass|piano|synth|bass|organ|guitar|lead|pad|choir|flute|horn|reeds?|drums?|percussion|vox|voice|synbass|fhorn|pizz|vibe)\s*(?:#?\s*\d+|[lr]|\(.*\))?$/i,
  /^(?:melody|melo)\b/i,
  /^(?:ﾍﾞｰｽ|ベース|ﾋﾟｱﾉ|ピアノ|ﾄﾞﾗﾑ|ドラム|ｷﾞﾀｰ|ギター|ｽﾄﾘﾝｸﾞｽ|ストリングス|ﾌﾞﾗｽ|ブラス|ﾘｰﾄﾞ|リード|ﾌﾙｰﾄ|フルート|ﾊｰﾌﾟ|ハープ|ｺｰﾗｽ|コーラス|ﾎﾞｲｽ|ボイス|ｼﾝｾ|シンセ|ﾘｽﾞﾑ|リズム|ﾊﾟｰﾄ|パート|ﾄﾗｯｸ|トラック|ﾒﾛ|メロ)(?:\s|$|[\d\-_])/,
  /^(?:ﾒﾛ|メロ|トラック|パート)\s*[-#._]?\s*\d+$/i,
  /^score$/i,
  /^staff$/i,
  /^copyright/i, // Don't use copyright strings as titles
  /^sequenced\s+by/i,
  /^by\s+/i,
  /^made\s+by/i,
  /^made$/i,
  /^generated\s+by/i,
  /^converted\s+by/i,
  /^arranged\s+by/i,
  /^xmi2mid/i,
  /^mid2mid/i,
  /^hmp2mid/i,
  /^seq[-\s]?\d+$/i,
  /^written\s+by/i,
  /^composed\s+by/i,
  /^recorded\s+by/i,
  /^author/i,
  /^piano\s*\d*\/loop/i,
  /^all rights/i,
  /^sf2=/i, // Ignore my custom SF2=... SoundFont text events
  /^midi\s+part/i,
  /^block:/i,
  /^[0-9a-f]{2}h\.\.\./i,
  /サイト名/i,
  /^https?:\/\//i,
  /^www\./i,
  /^mailto:/i,
  /^strings\s*(?:low|high|mid|l|r)?(?:\s*[\/,]\s*.*)?$/i,
  /^melody\s*\(.*\)$/i,
  /^.*template\s*(\(.*\))?$/i,
  // Disclaimers, promotional ads, spam, contact info, dividers
  /^this\s+(?:sequence|file|song|midi|tune|track|music)\b/i,
  /^than\s+\d+\s+minutes/i,
  /style\s+disk/i,
  /fake\s+disk/i,
  /band-in-a-box/i,
  /catalog\s+contact/i,
  /box\s*\d+/i,
  /fax\/voicemail/i,
  /[a-z0-9._%+-]+@[a-z0-9.-]+\.[a-z]{2,}/i,
  /^please\s+(?:distribute|keep|read|note)\b/i,
  /^freely\s+but\b/i,
  /^attached\.\s+thank\s+you/i,
  /^best\s+heard\s+with\b/i,
  /^version\s*\d+/i,
  /^[-=_*~]{3,}$/,
  /^\*+\s*please\s+read/i,
];

function readVarLen(buf, offset) {
  let value = 0;
  let bytesRead = 0;
  while (offset + bytesRead < buf.length) {
    const byte = buf[offset + bytesRead];
    bytesRead++;
    value = (value << 7) | (byte & 0x7F);
    if (!(byte & 0x80)) break;
    if (bytesRead >= 4) break;
  }
  return { value, bytesRead };
}

function scoreMidiString(text, type, dt = 0, isKaraoke = false) {
  if (!text) return -100;
  const cleaned = cleanString(text);
  if (cleaned.length < 2) return -100;
  const lower = cleaned.toLowerCase().trim();

  // 1. Hard Blocklist
  if (MIDI_BLOCKLIST.has(lower)) return -100;

  // 2. Regex Blocklist
  if (MIDI_REGEX_BLOCKLIST.some(rx => rx.test(lower))) return -100;

  let score = 0;

  // 3. Type Priority
  if (isKaraoke) score += 60; // Soft Karaoke title tag @T
  if (type === 0x03) score += 50; // Sequence Name on Track 0
  if (type === 0x01) score += 20; // Text Event
  if (type === 0x06) score += 20; // Marker

  // 4. Prefix Bonus
  if (lower.startsWith('title:') || lower.startsWith('song:') || lower.startsWith('sequence:')) {
    score += 50;
  }

  // 5. Japanese enclosure bonus (often indicates title/game)
  if (/[【「『].+[】」』]/.test(cleaned) && !/サイト|url|http/i.test(cleaned)) {
    score += 20;
  }

  // 6. Header event bonus (events at dt === 0 are header declarations)
  if (dt === 0) score += 10;

  // 7. Very short strings (e.g. "I", "II") might be section markers
  if (cleaned.length < 3) score -= 10;

  return score;
}

function scanTrackEvents(buf, trackOffset, trackLen, format, trackIdx, meta, candidates) {
  let cursor = trackOffset + 8;
  const trackEnd = Math.min(trackOffset + 8 + trackLen, buf.length);
  let runningStatus = 0;

  while (cursor < trackEnd) {
    const dt = readVarLen(buf, cursor);
    cursor += dt.bytesRead;
    if (cursor >= trackEnd) break;

    let status = buf[cursor];
    if (status & 0x80) {
      cursor++;
      if (status < 0xF0) {
        runningStatus = status;
      }
    } else {
      status = runningStatus;
    }

    if (status === 0xFF) {
      const type = buf[cursor++];
      const lenInfo = readVarLen(buf, cursor);
      cursor += lenInfo.bytesRead;
      const len = lenInfo.value;
      if (cursor + len > buf.length) break;

      const textBuf = buf.subarray(cursor, cursor + len);
      cursor += len;

      const rawText = decodeBuffer(textBuf);
      const text = cleanString(rawText);

      if (text) {
        if (type === 0x02) {
          if (!meta.copyright) meta.copyright = text;
        } else if (type === 0x03 || type === 0x01 || type === 0x06) {
          let candidateText = text;
          let isKaraoke = false;
          if (type === 0x01 && candidateText.startsWith('@T')) {
            candidateText = candidateText.substring(2).trim();
            isKaraoke = true;
          } else if (type === 0x01 && candidateText.startsWith('@A')) {
            if (!meta.artist) meta.artist = candidateText.substring(2).trim();
            continue;
          }
          const s = scoreMidiString(candidateText, type, dt.value, isKaraoke);
          if (s > 0) {
            const trackBonus = trackIdx === 0 ? 20 : Math.max(0, 15 - trackIdx * 2);
            candidates.push({ text: candidateText, score: s + trackBonus, track: trackIdx, type });
          } else if (trackIdx > 0 && type === 0x03) {
            // If Track 1 has a track name that was rejected as an instrument or blocklist,
            // this file uses tracks for channel instruments. Stop scanning subsequent tracks.
            return false;
          }
        } else if (type === 0x2F) {
          break; // End of Track
        }
      }
    } else if (status === 0xF0 || status === 0xF7) {
      runningStatus = 0;
      const lenInfo = readVarLen(buf, cursor);
      cursor += lenInfo.bytesRead + lenInfo.value;
    } else if (status >= 0x80) {
      if (format === 0 && candidates.length > 0 && status >= 0x80 && status < 0xF0) {
        break; // In Format 0, stop scanning once note messages begin
      }
      const typeNibble = status & 0xF0;
      if (typeNibble === 0xC0 || typeNibble === 0xD0) {
        cursor += 1;
      } else {
        cursor += 2;
      }
    } else {
      cursor++;
    }
  }
  return true;
}

function extractMidiTitleAndArtist(rawText) {
  let title = rawText;
  const lower = title.toLowerCase();
  if (lower.startsWith('title:')) title = title.substring(6).trim();
  else if (lower.startsWith('song:')) title = title.substring(5).trim();
  else if (lower.startsWith('sequence:')) title = title.substring(9).trim();

  let artist = null;
  // Tune 1000 and similar formats: "TITLE     ;ARTIST / COMPOSER"
  if (title.includes(';')) {
    const semiIdx = title.indexOf(';');
    const artistPart = title.substring(semiIdx + 1).trim();
    title = title.substring(0, semiIdx).trim();
    if (artistPart) {
      artist = artistPart.replace(/^(?:words?\s+(?:&|and)\s+music\s+by|words?\s+by|music\s+by|composed\s+by|written\s+by|by)\s+/i, '').trim();
    }
  }

  return { title, artist };
}

/**
 * Strategy 1: Bug-fixed binary SMF parser.
 * Reads MThd header, isolates Track 0 for Format 1 files, and extracts clean sequence titles.
 * If Track 0 has no title (common in Format 1 tempo tracks), scans Track 1 for sequence title.
 */
function parseMidiInternal(buf) {
  const meta = { system: 'MIDI' };

  if (buf.length < 14 || buf.toString('ascii', 0, 4) !== 'MThd') {
    return fallbackScanMIDI(buf);
  }

  const headerLen = buf.readUInt32BE(4);
  const format = buf.readUInt16BE(8);
  const ntracks = buf.readUInt16BE(10);

  let trackOffset = 8 + headerLen;
  if (trackOffset + 8 > buf.length || buf.toString('ascii', trackOffset, trackOffset + 4) !== 'MTrk') {
    return fallbackScanMIDI(buf);
  }

  const trackLen = buf.readUInt32BE(trackOffset + 4);
  const candidates = [];

  // 1. Scan Track 0
  scanTrackEvents(buf, trackOffset, trackLen, format, 0, meta, candidates);

  // 2. In Format 1, check if Track 0 had a strong sequence title (type === 0x03).
  // If not, scan Track 1 (the primary sequence/melody track)
  const hasStrongTrack0Title = candidates.some(c => c.track === 0 && c.type === 0x03);

  if (!hasStrongTrack0Title && format === 1 && ntracks > 1) {
    let nextTrackOffset = trackOffset + 8 + trackLen;
    let trackIdx = 1;
    while (trackIdx < Math.min(ntracks, 4) && nextTrackOffset + 8 < buf.length) {
      if (buf.toString('ascii', nextTrackOffset, nextTrackOffset + 4) !== 'MTrk') break;
      const curTrackLen = buf.readUInt32BE(nextTrackOffset + 4);
      const prevCandidatesLen = candidates.length;
      const keepSearching = scanTrackEvents(buf, nextTrackOffset, curTrackLen, format, trackIdx, meta, candidates);
      // If Track 1 yielded a valid candidate, or if it was an instrument channel, stop
      if (candidates.length > prevCandidatesLen || !keepSearching) break;
      nextTrackOffset += 8 + curTrackLen;
      trackIdx++;
    }
  }

  candidates.sort((a, b) => b.score - a.score);

  if (candidates.length > 0) {
    const res = extractMidiTitleAndArtist(candidates[0].text);
    meta.title = res.title;
    if (!meta.artist && res.artist) meta.artist = res.artist;
  }

  return meta;
}

/**
 * Fallback scanner for non-standard SMF files (e.g. raw dumps or missing MThd)
 */
function fallbackScanMIDI(buf) {
  const meta = { system: 'MIDI' };
  const mtrkIdx = buf.indexOf('MTrk');
  if (mtrkIdx === -1) return meta;

  const candidates = [];
  let cursor = mtrkIdx + 8;
  const limit = Math.min(cursor + 4096, buf.length);

  while (cursor < limit) {
    const dt = readVarLen(buf, cursor);
    cursor += dt.bytesRead;
    if (cursor >= limit) break;

    if (buf[cursor] === 0xFF) {
      cursor++;
      const type = buf[cursor++];
      const lenInfo = readVarLen(buf, cursor);
      cursor += lenInfo.bytesRead;
      const len = lenInfo.value;
      if (cursor + len > buf.length) break;

      const textBuf = buf.subarray(cursor, cursor + len);
      cursor += len;

      const text = cleanString(decodeBuffer(textBuf));

      if (text) {
        if (type === 0x02) {
          if (!meta.copyright) meta.copyright = text;
        } else if (type === 0x03 || type === 0x01 || type === 0x06) {
          const s = scoreMidiString(text, type, dt.value);
          if (s > 0) candidates.push({ text, score: s });
        }
      }
    } else {
      cursor++;
    }
  }

  candidates.sort((a, b) => b.score - a.score);
  if (candidates.length > 0) {
    const res = extractMidiTitleAndArtist(candidates[0].text);
    meta.title = res.title;
    if (!meta.artist && res.artist) meta.artist = res.artist;
  }
  return meta;
}

module.exports = {
  MIDI_STRATEGY_MAP,
  DEFAULT_MIDI_STRATEGY,
  MIDI_STRATEGIES,
  parseMIDI,
  parseMidiWithStrategy,
  parseMidiRouted,
  parseMidiInternal,
  parseMidiRolandSmf,
  guessMetadataFromPath,
  extractMidiTitleAndArtist,
};
