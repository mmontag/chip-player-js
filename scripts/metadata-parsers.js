const fs = require('fs');
const zlib = require('zlib');
// TextDecoder is global in modern Node.js (11+)

/**
 * Extract metadata from a buffer based on the file extension.
 * Returns: { title, artist, game, system, copyright, comment, duration }
 */
function parseMetadata(buffer, ext) {
  const parser = PARSERS[ext.toLowerCase()];
  if (!parser) return {};

  try {
    const meta = parser(buffer);
    // Final cleanup pass
    Object.keys(meta).forEach(k => {
      if (typeof meta[k] === 'string') {
        meta[k] = cleanString(meta[k]);
      }
    });
    return meta;
  } catch (e) {
    console.error(`[${ext.toUpperCase()}] ERROR:`, e.message);
    return {};
  }
}

/**
 * Clean string helper
 * - Stops at null terminator
 * - Trims whitespace
 * - Removes non-printable characters (control chars, DEL)
 */
function cleanString(str) {
  if (!str) return '';

  let cleaned = str;

  // 1. Find null terminator and slice
  const nullIndex = cleaned.indexOf('\0');
  if (nullIndex !== -1) cleaned = cleaned.substring(0, nullIndex);

  // 2. Remove control characters (0-31) and DEL (127)
  // We allow high-bit characters (>127) for Latin-1/Unicode support
  // eslint-disable-next-line no-control-regex
  cleaned = cleaned.replace(/[\x00-\x1F\x7F]/g, '');

  // 3. Trim whitespace
  return cleaned.trim();
}

/**
 * Robust String Decoder
 * Detects Shift-JIS vs Latin-1 (Binary)
 */
function decodeBuffer(buf) {
  if (isShiftJIS(buf)) {
    try {
      return new TextDecoder('shift-jis').decode(buf);
    } catch (e) {
      // Fallback if decode fails
    }
  }
  // Fallback to Latin1 to preserve binary data 1:1
  return buf.toString('latin1');
}

/**
 * Heuristic to detect Shift-JIS
 * * Shift-JIS byte ranges:
 * - ASCII: 0x00-0x7F
 * - Half-width Katakana: 0xA1-0xDF
 * - Double-byte Lead: 0x81-0x9F, 0xE0-0xEF
 * - Double-byte Trail: 0x40-0x7E, 0x80-0xFC
 * * Latin-1:
 * - 0x80-0x9F are Control Characters (unused in standard text)
 * - 0xA0-0xFF are symbols/accented chars
 */
function isShiftJIS(buf) {
  let sjisScore = 0;
  let len = buf.length;
  let i = 0;

  while (i < len) {
    const b = buf[i];

    // ASCII (Neutral)
    if (b < 0x80) {
      i++;
      continue;
    }

    // Half-width Katakana (Strong indicator if appearing in clusters)
    if (b >= 0xA1 && b <= 0xDF) {
      sjisScore += 1;
      i++;
      continue;
    }

    // Double-byte Lead
    if ((b >= 0x81 && b <= 0x9F) || (b >= 0xE0 && b <= 0xEF)) {
      if (i + 1 >= len) return false; // Incomplete sequence
      const trail = buf[i + 1];

      // Check Valid Trail
      if ((trail >= 0x40 && trail <= 0x7E) || (trail >= 0x80 && trail <= 0xFC)) {
        sjisScore += 5; // Strong indicator
        i += 2;
        continue;
      }
    }

    // If we hit high bytes that are invalid SJIS leads or trails,
    // it's likely Latin-1 (e.g. 0xC0-0xD6 accented chars without valid trails)
    // However, 0xC0 is NOT a valid SJIS lead.
    // In SJIS, 0xF0-0xFF are unused/reserved/gaiji.

    // Simple rule: If we found valid double-byte pairs, assume SJIS.
    // If we found NO double-byte pairs and mostly high bytes that aren't Katakana, assume Latin-1.
    i++;
  }

  // Threshold: If we saw at least one clear double-byte pair or several katakana
  return sjisScore > 0;
}


/**
 * Helper to read string preventing high-bit stripping (Node 'ascii' is destructive)
 * We use 'latin1' to map bytes 1:1 to characters.
 */
function readStr(buf, start, end) {
  return decodeBuffer(buf.subarray(start, end));
}

// --- Parsers ---

const PARSERS = {
  vgm: parseVGM,
  vgz: (buf) => parseVGM(zlib.gunzipSync(buf)),
  nsf: parseNSF,
  nsfe: parseNSFe,
  gbs: parseGBS,
  spc: parseSPC,
  mod: parseMOD,
  xm: parseXM,
  it: parseIT,
  s3m: parseS3M,
  mid: parseMIDI,
  midi: parseMIDI,
  mdx: parseMIDI, // Fallback: MDX often uses SMF-like structures or no header, but standard MIDI parser won't hurt if format is different.
};

function parseVGM(buf) {
  // Spec: https://vgmrips.net/wiki/VGM_Specification
  if (buf.length < 64) {
    console.warn('[VGM] File too short (<64 bytes)');
    return {};
  }

  const version = buf.readUInt32LE(0x08);
  let gd3Offset = 0;

  if (version >= 0x100) { // VGM 1.00+
    const relativeOffset = buf.readUInt32LE(0x14);
    if (relativeOffset > 0) gd3Offset = relativeOffset + 0x14;
  }

  const meta = { system: 'VGM' };

  // Parse GD3 Tag if present
  if (gd3Offset && gd3Offset < buf.length) {
    if (buf.toString('ascii', gd3Offset, gd3Offset + 4) === 'Gd3 ') {
      let cursor = gd3Offset + 12; // Skip ID (4), Version (4), Length (4)

      const readString = () => {
        let end = cursor;
        while (end < buf.length && (buf[end] !== 0 || buf[end+1] !== 0)) end += 2;
        const str = buf.toString('utf16le', cursor, end);
        cursor = end + 2;
        return str;
      };

      meta.title = readString(); // Track Title (En)
      readString();              // Track Title (Jp)
      meta.game = readString();  // Game Name (En)
      readString();              // Game Name (Jp)
      meta.system = readString();// System Name (En)
      readString();              // System Name (Jp)
      meta.artist = readString();// Author (En)
    } else {
      console.warn(`[VGM] GD3 pointer exists (0x${gd3Offset.toString(16)}) but 'Gd3 ' magic missing.`);
    }
  }

  return meta;
}

function parseNSF(buf) {
  // Spec: https://www.nesdev.org/wiki/NSF
  if (buf.length < 0x80) return {};

  const magic = buf.toString('ascii', 0, 5);
  // Use latin1 to preserve 8-bit chars like ©
  return {
    title: readStr(buf, 0x0E, 0x2E),
    artist: readStr(buf, 0x2E, 0x4E),
    copyright: readStr(buf, 0x4E, 0x6E),
    system: 'NES'
  };
}

function parseNSFe(buf) {
  // Spec: https://www.nesdev.org/wiki/NSFe
  if (buf.toString('ascii', 0, 4) !== 'NSFE') {
    return parseNSF(buf);
  }

  const meta = { system: 'NES' };
  let cursor = 4;

  while (cursor < buf.length) {
    if (cursor + 8 > buf.length) break;

    const chunkSize = buf.readUInt32LE(cursor);
    const chunkType = buf.toString('ascii', cursor + 4, cursor + 8);
    const chunkDataStart = cursor + 8;
    const nextChunk = chunkDataStart + chunkSize;

    if (chunkType === 'auth') {
      let localCursor = chunkDataStart;

      const readChunkStr = () => {
        if (localCursor >= nextChunk) return '';
        let end = localCursor;
        while (end < nextChunk && buf[end] !== 0) end++;
        const str = readStr(buf, localCursor, end);
        localCursor = end + 1; // Skip null
        return str;
      };

      meta.game = readChunkStr();
      meta.artist = readChunkStr();
      meta.copyright = readChunkStr();
      if (!meta.title) meta.title = meta.game;
    }
    else if (chunkType === 'tlbl') {
      let localCursor = chunkDataStart;
      let end = localCursor;
      while (end < nextChunk && buf[end] !== 0) end++;
      const firstTrackTitle = readStr(buf, localCursor, end);

      if (firstTrackTitle) {
        meta.title = firstTrackTitle;
      }
    }
    else if (chunkType === 'NEND') {
      break;
    }

    cursor = nextChunk;
  }

  return meta;
}

function parseGBS(buf) {
  if (buf.toString('ascii', 0, 3) !== 'GBS') {
    console.warn('[GBS] Missing GBS signature.');
  }
  return {
    title: readStr(buf, 0x10, 0x30),
    artist: readStr(buf, 0x30, 0x50),
    copyright: readStr(buf, 0x50, 0x70),
    system: 'Gameboy'
  };
}

function parseSPC(buf) {
  const magic = buf.toString('ascii', 0, 27);
  if (!magic.startsWith('SNES-SPC700 Sound File Data')) {
    console.warn('[SPC] Missing SPC700 signature.');
  }
  return {
    title: readStr(buf, 0x2E, 0x4E),
    game: readStr(buf, 0x4E, 0x6E),
    artist: readStr(buf, 0xB1, 0xD1),
    comment: readStr(buf, 0x7E, 0x9E),
    system: 'SNES'
  };
}

function parseMOD(buf) {
  return {
    title: readStr(buf, 0x00, 0x14),
    system: 'Amiga'
  };
}

function parseS3M(buf) {
  return {
    title: readStr(buf, 0x00, 0x1C),
    system: 'PC'
  };
}

function parseXM(buf) {
  return {
    title: readStr(buf, 0x11, 0x25),
    system: 'PC'
  };
}

function parseIT(buf) {
  return {
    title: readStr(buf, 0x04, 0x1E),
    system: 'PC'
  };
}

// --- Improved MIDI Heuristics ---

const MIDI_BLOCKLIST = new Set([
  'untitled', 'untitled track', 'tempo', 'tempo track', 'tempo control',
  'meta', 'meta track', 'end track', 'sysex', 'sysex events',
  'reset', 'master parameters', 'control track',
  // Generic Instrument Names (often found as track names)
  'piano', 'grand piano', 'acoustic grand piano', 'drums', 'standard kit',
  'guitar', 'nylon guitar', 'steel guitar', 'bass', 'acoustic bass',
  'fingered bass', 'picked bass', 'string ensemble 1', 'string ensemble 2',
  'synth strings 1', 'synth strings 2', 'trumpet', 'trombone', 'tuba',
  'french horn', 'brass section', 'saxophone', 'alto sax', 'tenor sax',
  'flute', 'clarinet', 'oboe', 'english horn', 'bassoon', 'piccolo',
  'violin', 'viola', 'cello', 'contrabass', 'treble', 'r.h.', 'l.h.',
  'rh melody', 'instrument 1', 'track 1', 'track 2'
]);

const MIDI_REGEX_BLOCKLIST = [
  /^track\s*\d+$/i,
  /^channel\s*\d+$/i,
  /^instrument\s*\d+$/i,
  /^score$/i,
  /^staff$/i,
  /^copyright/i, // Don't use copyright strings as titles
  /^sequenced by/i,
  /^by /i,
  /^generated by/i
];

function scoreMidiString(text, type, index) {
  if (!text || text.length < 2) return -100;
  const lower = text.toLowerCase().trim();

  // 1. Hard Blocklist
  if (MIDI_BLOCKLIST.has(lower)) return -100;

  // 2. Regex Blocklist
  if (MIDI_REGEX_BLOCKLIST.some(rx => rx.test(lower))) return -100;

  let score = 0;

  // 3. Type Priority
  if (type === 0x03) score += 10; // Sequence/Track Name preferred over Text (0x01)
  if (type === 0x01) score += 5;

  // 4. Prefix Bonus
  if (lower.startsWith('title:') || lower.startsWith('song:') || lower.startsWith('sequence:')) {
    score += 50;
  }

  // 5. Position Heuristic (earlier events in file/track are usually better)
  // We prefer the first valid string found, so slight penalty for later ones?
  // Actually, let's just assume the first good one is best, but this scoring helps disambiguate types.

  // 6. Length Heuristic
  // Very short strings (e.g. "I", "II") might be section markers
  if (text.length < 3) score -= 5;
  if (text.length > 50) score -= 5; // Very long might be comments/lyrics

  return score;
}

function parseMIDI(buf) {
  const meta = { system: 'MIDI' };

  // Quick check for RIFF MIDI which we don't handle well yet
  const mtrkIdx = buf.indexOf('MTrk');
  if (mtrkIdx === -1) {
    if (buf.indexOf('RIFF') === -1) {
      // console.warn('[MIDI] No MTrk chunk found.');
    }
    return meta;
  }

  const candidates = []; // { text, score }

  // Simple Scanner Implementation
  // A robust parser would follow the chunk structure.
  // This scans the first 8KB for Meta Events, which is usually sufficient for headers.
  let cursor = mtrkIdx + 8;
  const limit = Math.min(cursor + 8192, buf.length);

  while (cursor < limit) {
    // Variable Length Delta Time scanner
    let byte = buf[cursor++];
    if (byte & 0x80) {
      byte = buf[cursor++];
      if (byte & 0x80) {
        byte = buf[cursor++];
        if (byte & 0x80) cursor++;
      }
    }

    if (cursor >= limit) break;

    // Check for Meta Event Prefix (0xFF)
    if (buf[cursor] === 0xFF) {
      const type = buf[cursor + 1];
      const len = buf[cursor + 2];

      // Simple varlen length check (supports 1-byte length only for simplicity in scanner)
      // Most title strings are short < 127 bytes.
      if (cursor + 3 + len > buf.length) break;

      // Extract raw buffer first to detect encoding
      const textBuf = buf.subarray(cursor + 3, cursor + 3 + len);
      const text = decodeBuffer(textBuf).trim();

      // 0x03: Sequence/Track Name
      // 0x01: Text Event
      // 0x02: Copyright
      if (text) {
        if (type === 0x02) {
          if (!meta.copyright) meta.copyright = cleanString(text);
        } else if (type === 0x03 || type === 0x01) {
          const s = scoreMidiString(text, type);
          if (s > 0) {
            candidates.push({ text: cleanString(text), score: s });
          }
        }
      }

      cursor += 3 + len;
    } else {
      cursor++;
    }
  }

  // Sort candidates by score descending
  candidates.sort((a, b) => b.score - a.score);

  if (candidates.length > 0) {
    // Strip prefixes like "Title: " if they gave us the bonus points
    let best = candidates[0].text;
    const lower = best.toLowerCase();
    if (lower.startsWith('title:')) best = best.substring(6).trim();
    else if (lower.startsWith('song:')) best = best.substring(5).trim();
    else if (lower.startsWith('sequence:')) best = best.substring(9).trim();

    meta.title = best;
  }

  return meta;
}

module.exports = { parseMetadata };
