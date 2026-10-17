const fs = require('fs');
const path = require('path');
const zlib = require('zlib');
const { cleanString, decodeBuffer, readStr } = require('./metadata-utils');
const {
  parseMIDI,
  guessMetadataFromPath,
  parseMidiInternal,
  parseMidiRouted,
  parseMidiWithStrategy,
  extractMidiTitleAndArtist,
  MIDI_STRATEGIES,
  MIDI_STRATEGY_MAP,
} = require('./metadata-midi');
const {
  parseNSF,
  parseNSFe,
  parseGBS,
  parseAY,
  parseSID,
  extractDate,
} = require('./subtune-headers');
const { parsePSF } = require('../src/psf');

/**
 * Extract metadata from a buffer based on the file extension.
 * Returns: { title, artist, game, system, copyright, comment, date, duration }
 */
function parseMetadata(buffer, ext, relPath = null, strategy = null) {
  const extLower = ext.toLowerCase();
  const parser = PARSERS[extLower];
  if (!parser) return {};

  try {
    const meta = (extLower === 'mid' || extLower === 'midi')
      ? parseMidiWithStrategy(buffer, relPath, strategy)
      : parser(buffer);
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

// --- Parsers ---

const PARSERS = {
  vgm: parseVGM,
  vgz: (buf) => parseVGM(zlib.gunzipSync(buf)),
  nsf: parseNSF,
  nsfe: parseNSFe,
  gbs: parseGBS,
  ay: parseAY,
  spc: parseSPC,
  mod: parseMOD,
  xm: parseXM,
  it: parseIT,
  s3m: parseS3M,
  mid: parseMIDI,
  midi: parseMIDI,
  mdx: parseMIDI, // Fallback: MDX often uses SMF-like structures or no header, but standard MIDI parser won't hurt if format is different.
  gsf: (buf) => parsePSFTags(buf, 'Game Boy Advance'),
  minigsf: (buf) => parsePSFTags(buf, 'Game Boy Advance'),
  miniusf: (buf) => parsePSFTags(buf, 'Nintendo 64'),
  sid: parseSID,
  mus: parseSID, // The single-tune C64 Sidplayer format has no PSID header; parseSID only supplies its C64 label.
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
        const start = cursor;
        while (cursor < buf.length - 1) {
          if (buf.readUInt16LE(cursor) === 0) {
            const strBuf = buf.subarray(start, cursor);
            cursor += 2; // Skip null terminator
            return strBuf.toString('utf16le');
          }
          cursor += 2;
        }
        return '';
      };

      meta.title = readString();             // Track Name (English)
      readString();                          // Track Name (Japanese)
      meta.game = readString();              // Game Name (English)
      readString();                          // Game Name (Japanese)
      meta.system = readString();            // System Name (English)
      readString();                          // System Name (Japanese)
      meta.artist = readString();            // Author (English)
      readString();                          // Author (Japanese)
      meta.date = extractDate(readString()); // Release Date
      meta.ripper = readString();     // Ripper
      meta.comment = readString();    // Notes
    } else {
      console.warn(`[VGM] GD3 pointer exists (0x${gd3Offset.toString(16)}) but 'Gd3 ' magic missing.`);
    }
  }

  return meta;
}

function parseSPC(buf) {
  // Spec: https://wiki.superfamicom.org/spc700-reference#spc-file-format-header
  if (buf.length < 0x100 || buf.toString('ascii', 0, 27) !== 'SNES-SPC700 Sound File Data') {
    console.warn('[SPC] Missing SPC700 signature.');
    return {};
  }

  // Check ID666 format: Text vs Binary
  // Standard ID666 uses text at 0x2E
  const isBinaryId666 = buf[0x23] === 0x1A; // Spec deviation sometimes used

  let title = readStr(buf, 0x2E, 0x4E);
  let game = readStr(buf, 0x4E, 0x6E);
  let dumper = readStr(buf, 0x6E, 0x7E);
  let comment = readStr(buf, 0x7E, 0x9E);
  let artist = readStr(buf, 0xB1, 0xD1);

  // Parse Length (seconds)
  let duration = 0;
  if (!isBinaryId666) {
    const secondsStr = buf.toString('ascii', 0xA9, 0xAC).trim();
    const parsed = parseInt(secondsStr, 10);
    if (!isNaN(parsed)) duration = parsed;
  } else {
    duration = buf.readUInt16LE(0xA9);
  }

  return {
    title,
    game,
    artist,
    comment,
    dumper,
    duration,
    system: 'SNES'
  };
}

function parseMOD(buf) {
  // MOD titles are the first 20 bytes
  if (buf.length < 20) return {};
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

function parsePSFTags(buf, defaultSystem = 'Game Boy Advance') {
  return parsePSF(buf, { defaultSystem });
}


module.exports = {
  parseMetadata,
  guessMetadataFromPath,
  parseMidiInternal,
  parseMidiRouted,
  parseMidiWithStrategy,
  parseMIDI,
  extractMidiTitleAndArtist,
  MIDI_STRATEGIES,
  MIDI_STRATEGY_MAP,
  cleanString,
  decodeBuffer,
  readStr,
  parsePSFTags,
};
