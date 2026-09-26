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

/**
 * Extract metadata from a buffer based on the file extension.
 * Returns: { title, artist, game, system, copyright, comment, duration }
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

      meta.title = readString();      // Track Name (English)
      readString();                   // Track Name (Japanese)
      meta.game = readString();       // Game Name (English)
      readString();                   // Game Name (Japanese)
      meta.system = readString();     // System Name (English)
      readString();                   // System Name (Japanese)
      meta.artist = readString();     // Author (English)
      readString();                   // Author (Japanese)
      meta.date = readString();       // Release Date
      meta.ripper = readString();     // Ripper
      meta.comment = readString();    // Notes
    }
  }

  return meta;
}

function parseNSF(buf) {
  // Spec: https://wiki.nesdev.org/w/index.php/NSF
  if (buf.length < 128 || buf.toString('ascii', 0, 5) !== 'NESM\x1A') {
    return {};
  }

  return {
    title: readStr(buf, 0x0E, 0x2E),
    artist: readStr(buf, 0x2E, 0x4E),
    copyright: readStr(buf, 0x4E, 0x6E),
    system: 'NES'
  };
}

function parseNSFe(buf) {
  // Spec: https://wiki.nesdev.org/w/index.php/NSFe
  if (buf.length < 4 || buf.toString('ascii', 0, 4) !== 'NSFE') {
    return {};
  }

  const meta = { system: 'NES' };
  let offset = 4;

  while (offset + 8 <= buf.length) {
    const chunkLen = buf.readUInt32LE(offset);
    const chunkId = buf.toString('ascii', offset + 4, offset + 8);
    const chunkDataStart = offset + 8;
    const chunkDataEnd = chunkDataStart + chunkLen;

    if (chunkDataEnd > buf.length) break;

    if (chunkId === 'auth') {
      // Structure: 4 null-terminated strings: Game, Artist, Copyright, Ripper
      const strings = [];
      let strStart = chunkDataStart;

      for (let i = chunkDataStart; i < chunkDataEnd; i++) {
        if (buf[i] === 0) {
          strings.push(decodeBuffer(buf.subarray(strStart, i)));
          strStart = i + 1;
        }
      }

      if (strings[0]) meta.game = strings[0];
      if (strings[1]) meta.artist = strings[1];
      if (strings[2]) meta.copyright = strings[2];
      if (strings[3]) meta.ripper = strings[3];

    } else if (chunkId === 'tlbl') {
      // Track labels (First string is Track 1 title)
      const firstNull = buf.indexOf(0, chunkDataData = chunkDataStart);
      if (firstNull !== -1 && firstNull < chunkDataEnd) {
        meta.title = decodeBuffer(buf.subarray(chunkDataStart, firstNull));
      }
    }

    offset += 8 + chunkLen;
  }

  return meta;
}

function parseGBS(buf) {
  // Spec: https://gbdev.gg8.se/wiki/articles/GBS_Music_Format
  if (buf.length < 112 || buf.toString('ascii', 0, 3) !== 'GBS') {
    return {};
  }

  return {
    title: readStr(buf, 0x10, 0x30),
    artist: readStr(buf, 0x30, 0x50),
    copyright: readStr(buf, 0x50, 0x70),
    system: 'Game Boy'
  };
}

function parseSPC(buf) {
  // Spec: https://wiki.superfamicom.org/spc700-reference#spc-file-format-header
  if (buf.length < 0x100 || buf.toString('ascii', 0, 27) !== 'SNES-SPC700 Sound File Data') {
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
};
