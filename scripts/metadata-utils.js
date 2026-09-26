/**
 * Chip Player JS - Metadata String & Buffer Utilities
 */

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
 * Shift-JIS byte ranges:
 * - ASCII: 0x00-0x7F
 * - Half-width Katakana: 0xA1-0xDF
 * - Double-byte Lead: 0x81-0x9F, 0xE0-0xEF
 * - Double-byte Trail: 0x40-0x7E, 0x80-0xFC
 * Latin-1:
 * - 0x80-0x9F are Control Characters (unused in standard text)
 * - 0xA0-0xFF are symbols/accented chars
 */
function isShiftJIS(buf) {
  let sjisScore = 0;
  const len = buf.length;
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

module.exports = {
  cleanString,
  decodeBuffer,
  isShiftJIS,
  readStr,
};
