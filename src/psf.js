import pathe from 'pathe';

const VERSION_SYSTEM_MAP = {
  0x01: 'PlayStation',
  0x02: 'PlayStation 2',
  0x11: 'Sega Saturn',
  0x12: 'Sega Dreamcast',
  0x21: 'Nintendo 64',
  0x22: 'Game Boy Advance',
  0x23: 'Nintendo DS',
  0x41: 'Capcom QSound',
};

/**
 * Parses time strings formatted as mm:ss[.xx] or hh:mm:ss[.xx] into seconds.
 */
export function parsePsfTime(str) {
  if (!str) return 0;
  const parts = str.split(':').map(p => parseFloat(p));
  if (parts.some(isNaN)) return 0;
  if (parts.length === 3) {
    return parts[0] * 3600 + parts[1] * 60 + parts[2];
  } else if (parts.length === 2) {
    return parts[0] * 60 + parts[1];
  } else if (parts.length === 1) {
    return parts[0];
  }
  return 0;
}

/**
 * Parses PSF header and tags from a Uint8Array or ArrayBuffer.
 * Works across GSF (GBA), USF (N64), PSF1/2, and other PSF variants.
 *
 * @param {Uint8Array|ArrayBuffer} data - Binary data of the PSF file
 * @param {Object} [options]
 * @param {string} [options.filename] - Original filename for fallback title
 * @param {string} [options.defaultSystem] - System name fallback
 * @returns {Object} Parsed metadata object
 */
export function parsePSF(data, options = {}) {
  const bytes = (data instanceof Uint8Array) ? data : new Uint8Array(data);
  const result = {
    title: null,
    artist: null,
    game: null,
    year: null,
    copyright: null,
    comment: null,
    genre: null,
    length: null,
    fade: null,
    duration: 0,
    system: options.defaultSystem || null,
    libraries: [],
    tags: {},
    formatted: {
      title: '',
      subtitle: '',
    },
  };

  if (!bytes || bytes.length < 16) {
    applyFallbacks(result, options);
    return result;
  }

  // Check PSF signature "PSF" (0x50, 0x53, 0x46)
  if (bytes[0] !== 0x50 || bytes[1] !== 0x53 || bytes[2] !== 0x46) {
    applyFallbacks(result, options);
    return result;
  }

  const version = bytes[3];
  if (!result.system && VERSION_SYSTEM_MAP[version]) {
    result.system = VERSION_SYSTEM_MAP[version];
  }

  const reservedSize = (bytes[4] | (bytes[5] << 8) | (bytes[6] << 16) | (bytes[7] << 24)) >>> 0;
  const exeSize = (bytes[8] | (bytes[9] << 8) | (bytes[10] << 16) | (bytes[11] << 24)) >>> 0;

  let tagOffset = 16 + reservedSize + exeSize;
  let hasTag = false;

  // Check if [TAG] starts at expected offset
  if (tagOffset + 5 <= bytes.length &&
      bytes[tagOffset] === 0x5B && bytes[tagOffset + 1] === 0x54 &&
      bytes[tagOffset + 2] === 0x41 && bytes[tagOffset + 3] === 0x47 &&
      bytes[tagOffset + 4] === 0x5D) {
    hasTag = true;
    tagOffset += 5;
  } else {
    // Fallback: search backwards for [TAG] signature
    const tagSig = [0x5B, 0x54, 0x41, 0x47, 0x5D];
    for (let i = bytes.length - 5; i >= 16; i--) {
      if (bytes[i] === tagSig[0] && bytes[i + 1] === tagSig[1] &&
          bytes[i + 2] === tagSig[2] && bytes[i + 3] === tagSig[3] &&
          bytes[i + 4] === tagSig[4]) {
        hasTag = true;
        tagOffset = i + 5;
        break;
      }
    }
  }

  if (hasTag && tagOffset < bytes.length) {
    const tagBytes = bytes.subarray(tagOffset);
    let tagStr = new TextDecoder('utf-8').decode(tagBytes);
    if (tagStr.includes('\uFFFD')) {
      tagStr = new TextDecoder('latin1').decode(tagBytes);
    }

    const lines = tagStr.split(/\r?\n/);
    let lastKey = null;

    for (const rawLine of lines) {
      const line = rawLine.trim();
      if (!line) continue;
      const eqIdx = line.indexOf('=');
      if (eqIdx !== -1) {
        const key = line.slice(0, eqIdx).trim().toLowerCase();
        const value = line.slice(eqIdx + 1).trim();
        lastKey = key;

        if (result.tags[key] !== undefined) {
          if (Array.isArray(result.tags[key])) {
            result.tags[key].push(value);
          } else {
            result.tags[key] = [result.tags[key], value];
          }
        } else {
          result.tags[key] = value;
        }

        if (key === '_lib' || /^(_lib\d*)$/i.test(key)) {
          result.libraries.push(value);
        }
      } else if (lastKey && (lastKey === 'comment' || lastKey === 'title' || lastKey === 'artist')) {
        // Multi-line continuation
        if (typeof result.tags[lastKey] === 'string') {
          result.tags[lastKey] += ' ' + line;
        }
      }
    }
  }

  const getTag = (key) => {
    const val = result.tags[key];
    if (Array.isArray(val)) return val.join(', ');
    return val || null;
  };

  result.title = getTag('title');
  result.artist = getTag('artist');
  result.game = getTag('game');
  result.year = getTag('year');
  result.copyright = getTag('copyright');
  result.comment = getTag('comment');
  result.genre = getTag('genre');
  result.length = getTag('length');
  result.fade = getTag('fade');

  const songSec = parsePsfTime(result.length);
  const fadeSec = parsePsfTime(result.fade);
  result.duration = songSec + fadeSec;

  applyFallbacks(result, options);
  return result;
}

function applyFallbacks(result, options) {
  if (!result.title && options.filename) {
    const base = pathe.basename(options.filename);
    result.title = base.replace(/\.[^.]+$/, '');
  }

  const title = result.title || '';
  const game = result.game || '';
  const artist = result.artist || '';
  const system = result.system || '';
  const dateOrCopyright = result.copyright || result.year || '';

  const displayTitle = (game && !title.toLowerCase().startsWith(game.toLowerCase()))
    ? `${game} - ${title}`
    : title;

  const subtitleParts = [artist, system].filter(Boolean);
  let displaySubtitle = subtitleParts.join(' - ');
  if (dateOrCopyright) {
    displaySubtitle += (displaySubtitle ? ' ' : '') + `(${dateOrCopyright})`;
  }

  result.formatted = {
    title: displayTitle,
    subtitle: displaySubtitle,
  };
}

export default parsePSF;
