/**
 * Sub-tune header parsers — the catalog's "how many songs are in this file, and
 * what are they called" logic, in one place.
 *
 * These five formats pack several songs into one file (see MULTISONG_EXTENSIONS).
 * The parsers read only the file's header / small chunks, never the audio, and
 * are deliberately free of Node built-ins and Buffer-only methods: they operate
 * on any byte view (a Node Buffer here, a Uint8Array in the browser), so the
 * client can import this file directly to list a dropped local file's sub-tunes
 * without shipping the full parser set or a Buffer polyfill.
 *
 * The sibling `metadata-parsers.js` requires these back and re-exports them into
 * its PARSERS map, so `build-music.js` and the client share one definition.
 */

const { cleanString, decodeBuffer, readStr } = require('./metadata-utils');

// --- Byte readers (Buffer- and Uint8Array-compatible) ---

function u16be(buf, off) {
  return (buf[off] << 8) | buf[off + 1];
}

function i16be(buf, off) {
  const v = u16be(buf, off);
  return v & 0x8000 ? v - 0x10000 : v;
}

function u32le(buf, off) {
  return (buf[off] | (buf[off + 1] << 8) | (buf[off + 2] << 16) | (buf[off + 3] << 24)) >>> 0;
}

function asciiStr(buf, start, end) {
  let s = '';
  for (let i = start; i < end; i++) s += String.fromCharCode(buf[i] & 0x7f);
  return s;
}

// --- Release date extraction ---

const DATE_YMD_REGEX = /\b(1[89]\d{2}|20\d{2})[-/.](0?[1-9]|1[0-2])[-/.](0?[1-9]|[12]\d|3[01])\b/;
const DATE_YM_REGEX = /\b(1[89]\d{2}|20\d{2})[-/.](0?[1-9]|1[0-2])\b/;
const DATE_Y_REGEX = /\b(1[89]\d{2}|20\d{2})\b/;

/**
 * Extract a release date from free-form metadata strings (copyright, released,
 * GD3 release date, ...). Formats vary wildly: "1988 Konami", "(C)1984 CAPCOM",
 * "2008-2009", "1988-12-16". Normalizes to an ISO date, defaulting the unknown
 * parts to Jan 1. Returns null when no year is found.
 */
function extractDate(...strings) {
  const isValidYmd = (y, mo, d) =>
    d >= 1 && d <= new Date(Date.UTC(y, mo, 0)).getUTCDate();
  for (const str of strings) {
    if (!str) continue;
    let m = str.match(DATE_YMD_REGEX);
    if (m) {
      const [y, mo, d] = [Number(m[1]), Number(m[2]), Number(m[3])];
      // Ignore impossible dates (e.g. "2023-02-31") rather than store them.
      if (isValidYmd(y, mo, d)) {
        return `${m[1]}-${m[2].padStart(2, '0')}-${m[3].padStart(2, '0')}`;
      }
    }
    m = str.match(DATE_YM_REGEX);
    if (m) return `${m[1]}-${m[2].padStart(2, '0')}-01`;
    m = str.match(DATE_Y_REGEX);
    if (m) return `${m[1]}-01-01`;
  }
  return null;
}

// --- Format parsers ---

function parseNSF(buf) {
  // Spec: https://wiki.nesdev.org/w/index.php/NSF
  if (buf.length < 128 || asciiStr(buf, 0, 5) !== 'NESM\x1A') {
    return {};
  }

  // The total number of songs is stored at offset 0x06, and the
  // starting song index (1-based) at 0x07.
  const numSongs = buf[0x06] || 1;
  const startingSong = buf[0x07] || 1;

  // Use latin1 to preserve 8-bit chars like ©
  const copyright = readStr(buf, 0x4E, 0x6E);
  return {
    title: readStr(buf, 0x0E, 0x2E),
    artist: readStr(buf, 0x2E, 0x4E),
    copyright,
    date: extractDate(copyright),
    system: 'NES',
    numSongs,
    startingSong,
  };
}

/**
 * Parse the NSFE `tlbl`/`auth` string chunks into an array of strings.
 * The chunk is a run of null-terminated strings.
 */
function parseNsfeStrings(buf, start, end) {
  const strings = [];
  let cursor = start;
  while (cursor < end) {
    let strEnd = cursor;
    while (strEnd < end && buf[strEnd] !== 0) strEnd++;
    strings.push(readStr(buf, cursor, strEnd));
    cursor = strEnd + 1;
  }
  return strings;
}

function parseNSFe(buf) {
  // Spec: https://wiki.nesdev.org/w/index.php/NSFe
  //
  // NSFE is a chunked ("tagged") format. Each chunk is a little-endian
  // uint32 size followed by a 4-byte ASCII tag (INFO, BANK, time, auth,
  // plst, tlbl, DATA, NEND, ...). See game-music-emu/gme/Nsfe_Emu.cpp.
  if (asciiStr(buf, 0, 4) !== 'NSFE') {
    return parseNSF(buf);
  }

  const meta = { system: 'NES' };
  let cursor = 4;

  // NSFE does not embed a base NSF header; counts come from the INFO chunk.
  // `trackCount` is the number of tracks physically present in the DATA chunk.
  // The `plst` (playlist) chunk, when present, is the authoritative list of
  // sub-tunes (and their order/remapping), mirroring game-music-emu.
  let trackCount = 1;
  let startingSong = 1;

  let trackLabels = [];   // labels indexed by physical track
  let playlist = null;    // array of physical track indices

  while (cursor + 8 <= buf.length) {
    const chunkSize = u32le(buf, cursor);
    const chunkType = asciiStr(buf, cursor + 4, cursor + 8);
    const chunkDataStart = cursor + 8;
    const nextChunk = Math.min(chunkDataStart + chunkSize, buf.length);

    switch (chunkType) {
      case 'INFO': {
        // load_addr(2), init_addr(2), play_addr(2), speed_flags(1),
        // chip_flags(1), track_count(1), first_track(1), unused(6)
        if (chunkSize >= 9) {
          trackCount = buf[chunkDataStart + 8] || 1;
        }
        if (chunkSize >= 10) {
          // NSFE `first_track` is 0-based (unlike the 1-based NSF header).
          startingSong = buf[chunkDataStart + 9] + 1;
        }
        break;
      }
      case 'auth': {
        const strs = parseNsfeStrings(buf, chunkDataStart, nextChunk);
        if (strs[0]) meta.game = strs[0];
        if (strs[1]) meta.artist = strs[1];
        if (strs[2]) meta.copyright = strs[2];
        if (strs[3]) meta.dumper = strs[3];
        if (!meta.title) meta.title = meta.game;
        break;
      }
      case 'tlbl': {
        trackLabels = parseNsfeStrings(buf, chunkDataStart, nextChunk);
        break;
      }
      case 'plst': {
        // A byte array of physical track indices.
        playlist = Array.from(buf.subarray(chunkDataStart, nextChunk));
        break;
      }
      case 'time': // per-track lengths in ms; reserved for future use.
      case 'fade': // per-track fadeout times in ms; reserved for future use.
        break;
      default:
        break;
    }

    if (chunkType === 'NEND') break;
    if (nextChunk <= cursor) break;
    cursor = nextChunk;
  }

  // Mirror game-music-emu's Nsfe_Info:
  //   - with a non-empty playlist, songs are the playlist entries remapped
  //     into the physical track list;
  //   - otherwise, songs are all physical tracks 1:1.
  const usePlaylist = Array.isArray(playlist) && playlist.length > 0;
  const songCount = usePlaylist ? playlist.length : trackCount;

  const subtitles = [];
  for (let i = 0; i < songCount; i++) {
    const physical = usePlaylist ? playlist[i] : i;
    const label = trackLabels[physical];
    // Keep a slot for unlabeled tracks so labels stay aligned to their tune.
    subtitles.push(label != null ? label : null);
  }

  meta.numSongs = songCount;
  meta.startingSong = startingSong;
  if (subtitles.length > 0) {
    meta.trackLabels = subtitles;
  }
  // A single empty label is the spec's "one unnamed track" placeholder.
  if (meta.trackLabels && meta.trackLabels.length === 1 && meta.trackLabels[0] === '') {
    delete meta.trackLabels;
  }
  if (meta.trackLabels && meta.trackLabels[0]) {
    meta.title = meta.trackLabels[0];
  }

  meta.date = extractDate(meta.copyright);

  return meta;
}

function parseGBS(buf) {
  // Spec: https://gbdev.gg8.se/wiki/articles/GBS_Music_Format (deleted?)
  //       https://ocremix.org/info/GBS_Format_Specification
  //       https://web.archive.org/web/20250518043814/https://ftp.modland.com/pub/documents/format_documentation/Gameboy%20Sound%20System%20%28.gbs%29.txt
  // Layout mirrors game-music-emu/gme/Gbs_Core.h: track_count at 0x04,
  // first_track (1-based) at 0x05, then three null-padded 32-byte strings.
  if (buf.length < 0x70 || asciiStr(buf, 0, 3) !== 'GBS') {
    console.warn('[GBS] Missing GBS signature.');
    return {};
  }

  const copyright = readStr(buf, 0x50, 0x70);
  return {
    title: readStr(buf, 0x10, 0x30),
    artist: readStr(buf, 0x30, 0x50),
    copyright,
    date: extractDate(copyright),
    system: 'Game Boy',
    numSongs: buf[0x04] || 1,
    startingSong: buf[0x05] || 1,
  };
}

/**
 * Read a null-terminated string at a big-endian offset field, where the
 * offset is relative to the field's own position (the ZXAYEMUL convention;
 * see game-music-emu/gme/Ay_Emu.cpp:get_data). Returns null when the field
 * is zero or points outside the buffer.
 */
function readAyStringAt(buf, fieldPos) {
  if (fieldPos + 2 > buf.length) return null;
  const offset = i16be(buf, fieldPos);
  if (!offset) return null;
  const start = fieldPos + offset;
  // The offset is signed (backward pointers are legal), so a value pointing
  // before byte zero is invalid: subarray() would read it relative to the end
  // and hand back unrelated trailing bytes as the string.
  if (start < 0 || start >= buf.length) return null;
  let end = start;
  while (end < buf.length && buf[end] !== 0) end++;
  return decodeBuffer(buf.subarray(start, end));
}

function parseAY(buf) {
  // Layout mirrors game-music-emu/gme/Ay_Emu.h:header_t. The header holds
  // offsets (not inline strings); per-track names live in the tracks table.
  if (buf.length < 0x14) return {};
  if (asciiStr(buf, 0, 8) !== 'ZXAYEMUL') {
    console.warn('[AY] Missing ZXAYEMUL signature.');
    return { system: 'ZX Spectrum' };
  }
  const maxTrack = buf[0x10];
  const numSongs = (maxTrack || 0) + 1;

  const meta = {
    system: 'ZX Spectrum',
    numSongs,
    startingSong: (buf[0x11] || 0) + 1,
  };
  const author = readAyStringAt(buf, 0x0C);
  if (author) meta.artist = author;
  const comment = readAyStringAt(buf, 0x0E);
  if (comment) meta.comment = comment;

  // Tracks table: one 4-byte entry per song; the first word of each entry
  // points at that song's null-terminated name.
  const tracksOff = i16be(buf, 0x12);
  const tracksPos = 0x12 + tracksOff;
  if (tracksOff && tracksPos >= 0x14 && tracksPos + numSongs * 4 <= buf.length) {
    const labels = [];
    for (let i = 0; i < numSongs; i++) {
      labels.push(readAyStringAt(buf, tracksPos + i * 4));
    }
    if (labels.some((l) => l != null && l !== '')) {
      meta.trackLabels = labels.map((l) => (l ? l : null));
      if (meta.trackLabels[0]) meta.title = meta.trackLabels[0];
    }
  }

  return meta;
}

function parseSID(buf) {
  // Spec: https://www.hvsc.c64.org/download/C64Music/DOCUMENTS/SID_file_format.txt
  if (buf.length < 0x76 || asciiStr(buf, 0, 4) !== 'PSID') {
    // PSID is the common variant; RSID shares the same header layout.
    if (asciiStr(buf, 0, 4) !== 'RSID') {
      console.warn('[SID] Missing PSID/RSID signature.');
      return { system: 'C64' };
    }
  }

  // Metadata strings live at fixed offsets in the header:
  // name at 0x16 (22), author at 0x36 (54), released at 0x56 (86),
  // each 32 bytes null-padded.
  const name = readStr(buf, 0x16, 0x36);
  const author = readStr(buf, 0x36, 0x56);
  const released = readStr(buf, 0x56, 0x76);

  // Song counts: word at 0x0E (total songs), word at 0x10 (start song).
  let numSongs = u16be(buf, 0x0E) || 1;
  const startingSong = u16be(buf, 0x10) || 1;
  if (numSongs === 0) numSongs = 1;

  // Per-song PAL/NTSC lives in the 32-bit speed field at 0x12 (bit i => song i),
  // not exposed here: nothing consumes it, and the old offset (0x18) sat inside
  // the title. Decode it from 0x12 if a caller ever needs it.

  return {
    title: name,
    artist: author,
    copyright: released,
    date: extractDate(released),
    system: 'C64',
    numSongs,
    startingSong,
  };
}

// --- Sub-tune description ---

const MULTISONG_EXTENSIONS = new Set(['nsf', 'nsfe', 'sid', 'gbs', 'ay']);

/**
 * Describe the playable sub-tunes inside a file from parser metadata.
 *
 * Returns an array of { subtune, title, lengthMs } when the file contains 2+
 * sub-tunes, otherwise an empty array. Single-song files have no sub-tunes, which
 * keeps the browse tree flat (a plain file).
 *
 *   - subtune:  0-based index, matches the player's subtune index
 *   - title:    per-track label when the format provides one, else null
 *               (clients show "Tune N" as a fallback)
 *   - lengthMs: duration in ms when known (reserved; parsers don't emit yet)
 *   - date:     per-track release date when the format provides one (rare)
 */
function describeSubtunes(extension, meta) {
  if (!MULTISONG_EXTENSIONS.has(extension)) return [];

  const numSongs = meta.numSongs || 1;
  if (numSongs < 2) return [];

  const labels = Array.isArray(meta.trackLabels) ? meta.trackLabels : [];
  const lengths = Array.isArray(meta.trackLengths) ? meta.trackLengths : [];
  const dates = Array.isArray(meta.trackDates) ? meta.trackDates : [];

  const subtunes = [];
  for (let i = 0; i < numSongs; i++) {
    const label = labels[i] != null ? cleanString(String(labels[i])) : '';
    subtunes.push({
      subtune: i,
      title: label || null,
      lengthMs: Number.isFinite(lengths[i]) ? lengths[i] : null,
      date: dates[i] != null ? String(dates[i]).trim() || null : null,
    });
  }
  return subtunes;
}

const SUBTUNE_PARSERS = {
  nsf: parseNSF,
  nsfe: parseNSFe,
  gbs: parseGBS,
  ay: parseAY,
  sid: parseSID,
};

/**
 * Client entry point: given a file extension and its raw bytes (a Uint8Array in
 * the browser), return the playable sub-tunes. [] for a single-song or unknown
 * format, or when the header is malformed. Wraps parse + describeSubtunes so the
 * client needs no format knowledge of its own.
 */
function describeSubtunesInBuffer(extension, bytes) {
  const ext = String(extension).toLowerCase();
  const parser = SUBTUNE_PARSERS[ext];
  if (!parser) return [];
  try {
    return describeSubtunes(ext, parser(bytes) || {});
  } catch (e) {
    return [];
  }
}

module.exports = {
  parseNSF,
  parseNSFe,
  parseGBS,
  parseAY,
  parseSID,
  extractDate,
  describeSubtunes,
  describeSubtunesInBuffer,
  MULTISONG_EXTENSIONS,
};
