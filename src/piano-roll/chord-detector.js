// Chord Detection and Harmonic Analysis for Piano Roll
// Evaluates actively sounding MIDI notes using interval scoring with jazz heuristics.

export const NOTE_NAMES = ['C', 'C#', 'D', 'Eb', 'E', 'F', 'F#', 'G', 'Ab', 'A', 'Bb', 'B'];

/**
 * Predefined chord templates covering triads, 6ths, 7ths, 9ths, 11ths, 13ths, and suspensions.
 * Intervals are semitones relative to root (0..11).
 */
export const CHORD_TEMPLATES = [
  // --- 13th Chords ---
  {
    quality: '13',
    required: [0, 4, 10, 9], // 1, 3, b7, 13
    optional: [7, 2],        // 5, 9
    avoid: [1, 3, 8, 11],
    priority: 88,
  },
  {
    quality: 'maj13',
    required: [0, 4, 11, 9], // 1, 3, 7, 13
    optional: [7, 2],        // 5, 9
    avoid: [1, 3, 8, 10],
    priority: 88,
  },
  {
    quality: 'm13',
    required: [0, 3, 10, 9], // 1, b3, b7, 13
    optional: [7, 2],        // 5, 9
    avoid: [1, 4, 8, 11],
    priority: 88,
  },
  {
    quality: '7(b13)',
    required: [0, 4, 10, 8], // 1, 3, b7, b13
    optional: [2],           // 9
    avoid: [1, 7, 9, 11],
    priority: 86,
  },

  // --- 11th Chords ---
  {
    quality: '11',
    required: [0, 10, 2, 5], // 1, b7, 9, 11 (3 omitted in dominant 11)
    optional: [7, 4],        // 5, 3
    avoid: [1, 8, 9, 11],
    priority: 82,
  },
  {
    quality: 'm11',
    required: [0, 3, 10, 5], // 1, b3, b7, 11
    optional: [7, 2],        // 5, 9
    avoid: [1, 4, 8, 9, 11],
    priority: 84,
  },
  {
    quality: 'maj7(#11)',
    required: [0, 4, 11, 6], // 1, 3, 7, #11
    optional: [7, 2, 9],     // 5, 9, 13
    avoid: [1, 3, 5, 8, 10],
    priority: 84,
  },
  {
    quality: '7(#11)',
    required: [0, 4, 10, 6], // 1, 3, b7, #11
    optional: [7, 2, 9],     // 5, 9, 13
    avoid: [1, 3, 5, 8, 11],
    priority: 84,
  },

  // --- 9th Chords ---
  {
    quality: '9',
    required: [0, 4, 10, 2], // 1, 3, b7, 9
    optional: [7],           // 5 (omitted 5th common in jazz)
    avoid: [1, 3, 8, 9, 11],
    priority: 78,
  },
  {
    quality: 'maj9',
    required: [0, 4, 11, 2], // 1, 3, 7, 9
    optional: [7],           // 5
    avoid: [1, 3, 8, 9, 10],
    priority: 78,
  },
  {
    quality: 'm9',
    required: [0, 3, 10, 2], // 1, b3, b7, 9
    optional: [7],           // 5
    avoid: [1, 4, 8, 9, 11],
    priority: 78,
  },
  {
    quality: 'm(maj9)',
    required: [0, 3, 11, 2], // 1, b3, 7, 9
    optional: [7],           // 5
    avoid: [1, 4, 8, 9, 10],
    priority: 78,
  },
  {
    quality: '7(b9)',
    required: [0, 4, 10, 1], // 1, 3, b7, b9
    optional: [7],           // 5
    avoid: [2, 3, 8, 9, 11],
    priority: 79,
  },
  {
    quality: '7(#9)',
    required: [0, 4, 10, 3], // 1, 3, b7, #9 (Hendrix chord)
    optional: [7],           // 5
    avoid: [1, 2, 8, 9, 11],
    priority: 79,
  },
  {
    quality: 'add9',
    required: [0, 4, 2],     // 1, 3, 9 (no 7th)
    optional: [7],           // 5
    avoid: [1, 3, 10, 11],
    priority: 68,
  },
  {
    quality: 'm(add9)',
    required: [0, 3, 2],     // 1, b3, 9 (no 7th)
    optional: [7],           // 5
    avoid: [1, 4, 10, 11],
    priority: 68,
  },

  // --- 7th Chords ---
  {
    quality: '7',
    required: [0, 4, 10],    // 1, 3, b7
    optional: [7],           // 5
    avoid: [1, 2, 3, 9, 11],
    priority: 62,
  },
  {
    quality: 'maj7',
    required: [0, 4, 11],    // 1, 3, 7
    optional: [7],           // 5
    avoid: [1, 2, 3, 9, 10],
    priority: 62,
  },
  {
    quality: 'm7',
    required: [0, 3, 10],    // 1, b3, b7
    optional: [7],           // 5
    avoid: [1, 2, 4, 9, 11],
    priority: 62,
  },
  {
    quality: 'm(maj7)',
    required: [0, 3, 11],    // 1, b3, 7
    optional: [7],           // 5
    avoid: [1, 2, 4, 9, 10],
    priority: 62,
  },
  {
    quality: 'm7b5',         // Half-diminished
    required: [0, 3, 6, 10], // 1, b3, b5, b7
    optional: [],
    avoid: [1, 2, 4, 7, 9, 11],
    priority: 66,
  },
  {
    quality: 'dim7',         // Full diminished
    required: [0, 3, 6, 9],  // 1, b3, b5, bb7
    optional: [],
    avoid: [1, 2, 4, 7, 10, 11],
    priority: 66,
  },
  {
    quality: '7sus4',
    required: [0, 5, 10],    // 1, 4, b7
    optional: [7, 2],        // 5, 9
    avoid: [1, 3, 4, 9, 11],
    priority: 64,
  },
  {
    quality: '7#5',
    required: [0, 4, 8, 10], // 1, 3, #5, b7
    optional: [],
    avoid: [1, 2, 7, 9, 11],
    priority: 65,
  },
  {
    quality: 'maj7#5',
    required: [0, 4, 8, 11], // 1, 3, #5, 7
    optional: [],
    avoid: [1, 2, 7, 9, 10],
    priority: 65,
  },

  // --- 6th Chords ---
  {
    quality: '6',
    required: [0, 4, 9],     // 1, 3, 6
    optional: [7],           // 5
    avoid: [1, 2, 3, 10, 11],
    priority: 58,
  },
  {
    quality: 'm6',
    required: [0, 3, 9],     // 1, b3, 6
    optional: [7],           // 5
    avoid: [1, 2, 4, 10, 11],
    priority: 58,
  },
  {
    quality: '6/9',
    required: [0, 4, 9, 2],  // 1, 3, 6, 9
    optional: [7],           // 5
    avoid: [1, 3, 10, 11],
    priority: 74,
  },

  // --- Triads ---
  {
    quality: '',             // Major triad
    required: [0, 4],        // 1, 3
    optional: [7],           // 5
    avoid: [1, 2, 3, 5, 6, 8, 9, 10, 11],
    priority: 42,
  },
  {
    quality: 'm',            // Minor triad
    required: [0, 3],        // 1, b3
    optional: [7],           // 5
    avoid: [1, 2, 4, 5, 6, 8, 9, 10, 11],
    priority: 42,
  },
  {
    quality: 'dim',          // Diminished triad
    required: [0, 3, 6],     // 1, b3, b5
    optional: [],
    avoid: [1, 2, 4, 5, 7, 8, 9, 10, 11],
    priority: 46,
  },
  {
    quality: 'aug',          // Augmented triad
    required: [0, 4, 8],     // 1, 3, #5
    optional: [],
    avoid: [1, 2, 3, 5, 6, 7, 9, 10, 11],
    priority: 46,
  },
  {
    quality: 'sus4',
    required: [0, 5],        // 1, 4
    optional: [7],           // 5
    avoid: [1, 2, 3, 4, 6, 8, 9, 10, 11],
    priority: 38,
  },
  {
    quality: 'sus2',
    required: [0, 2],        // 1, 2
    optional: [7],           // 5
    avoid: [1, 3, 4, 5, 6, 8, 9, 10, 11],
    priority: 38,
  },
  {
    quality: '5',            // Power chord
    required: [0, 7],        // 1, 5
    optional: [],
    avoid: [1, 2, 3, 4, 5, 6, 8, 9, 10, 11],
    priority: 25,
  },
];

// Precompute 12-bit masks for required, optional, and allowed intervals
for (const tmpl of CHORD_TEMPLATES) {
  tmpl.requiredMask = tmpl.required.reduce((m, i) => m | (1 << i), 0);
  tmpl.optionalMask = (tmpl.optional || []).reduce((m, i) => m | (1 << i), 0);
  tmpl.allowedMask = tmpl.requiredMask | tmpl.optionalMask;
}

/**
 * General MIDI program numbers (0-indexed) for atonal instruments:
 * - Percussive: Tinkle Bell (112), Agogo (113), Woodblock (115), Taiko Drum (116),
 *   Melodic Tom (117), Synth Drum / Synth Tom (118), Reverse Cymbal (119).
 *   (Note: Steel Drums [114] is pitched/tonal and intentionally omitted).
 * - Sound Effects: Guitar Fret Noise (120), Breath Noise (121), Seashore (122),
 *   Bird Tweet (123), Telephone Ring (124), Helicopter (125), Applause (126), Gunshot (127).
 */
export const ATONAL_GM_PROGRAMS = new Set([
  112, // Tinkle Bell
  113, // Agogo
  115, // Woodblock
  116, // Taiko Drum
  117, // Melodic Tom
  118, // Synth Drum (Synth Tom)
  119, // Reverse Cymbal
  120, // Guitar Fret Noise
  121, // Breath Noise
  122, // Seashore
  123, // Bird Tweet
  124, // Telephone Ring
  125, // Helicopter
  126, // Applause
  127, // Gunshot
]);

/**
 * Checks whether an instrument or note represents an atonal / percussion sound based on
 * MIDI channel (channel 9 standard drums) or General MIDI program number.
 *
 * @param {Object} note Note metadata with { channel, program }
 * @returns {boolean}
 */
export function isAtonalInstrument(note) {
  if (!note || typeof note !== 'object') return false;
  // General MIDI channel 9 is standard percussion / drums
  if (note.channel === 9) return true;
  // Atonal General MIDI program numbers
  if (typeof note.program === 'number' && ATONAL_GM_PROGRAMS.has(note.program)) return true;
  return false;
}

/**
 * Returns the number of set bits (1s) in a 32-bit integer.
 */
function popcount(n) {
  let count = 0;
  let v = n;
  while (v > 0) {
    count += v & 1;
    v >>= 1;
  }
  return count;
}

/**
 * Analyzes sounding notes and returns a human-readable chord name or empty string.
 *
 * @param {Array<number|Object>} soundingNotes Array of MIDI pitch numbers or note objects with {pitch, channel}
 * @param {Object} [options]
 * @param {number} [options.minNotes=2] Minimum distinct pitch classes required to form a chord
 * @param {boolean} [options.excludeAtonal=true] Whether to exclude atonal instruments
 * @returns {string} e.g. "C", "Dm7", "G7(b9)", "Cmaj13", "F/G", or ""
 */
export function detectChord(soundingNotes, options = {}) {
  if (!soundingNotes || soundingNotes.length === 0) return '';

  const minNotes = options.minNotes !== undefined ? options.minNotes : 2;
  const excludeAtonal = options.excludeAtonal !== false;

  let bassPitch = Infinity;
  let pitchClassesMask = 0;
  const uniquePcs = [];

  for (let i = 0; i < soundingNotes.length; i++) {
    const item = soundingNotes[i];
    const pitch = typeof item === 'number' ? item : item.pitch;

    if (typeof pitch !== 'number' || isNaN(pitch) || pitch < 0) continue;

    // Exclude atonal instruments (channel 9 drums, atonal GM patches, sound effects)
    if (excludeAtonal && isAtonalInstrument(item)) continue;

    if (pitch < bassPitch) {
      bassPitch = pitch;
    }

    const pc = ((Math.round(pitch) % 12) + 12) % 12;
    const bit = 1 << pc;
    if ((pitchClassesMask & bit) === 0) {
      pitchClassesMask |= bit;
      uniquePcs.push(pc);
    }
  }

  const numDistinctPcs = uniquePcs.length;
  if (numDistinctPcs < minNotes) {
    if (numDistinctPcs === 1 && minNotes <= 1) {
      return NOTE_NAMES[uniquePcs[0]];
    }
    return '';
  }

  const bassPc = bassPitch % 12;

  let bestScore = -Infinity;
  let bestRoot = -1;
  let bestTemplate = null;

  // Evaluate each candidate root present in the sounding pitch classes
  for (let rIdx = 0; rIdx < uniquePcs.length; rIdx++) {
    const root = uniquePcs[rIdx];

    // Shift pitchClassesMask so that 'root' is at bit 0
    // Interval i is at: (pc - root + 12) % 12
    let actualIntervalMask = 0;
    for (let pIdx = 0; pIdx < uniquePcs.length; pIdx++) {
      const interval = (uniquePcs[pIdx] - root + 12) % 12;
      actualIntervalMask |= (1 << interval);
    }

    for (let tIdx = 0; tIdx < CHORD_TEMPLATES.length; tIdx++) {
      const tmpl = CHORD_TEMPLATES[tIdx];

      // Check if all required intervals are present
      if ((actualIntervalMask & tmpl.requiredMask) !== tmpl.requiredMask) {
        continue;
      }

      // Check for avoided intervals (strong clashes)
      if (tmpl.avoid) {
        let hasClash = false;
        for (let a = 0; a < tmpl.avoid.length; a++) {
          if ((actualIntervalMask & (1 << tmpl.avoid[a])) !== 0) {
            hasClash = true;
            break;
          }
        }
        if (hasClash) continue;
      }

      // Calculate score
      let score = tmpl.priority;

      // Reward matching required and optional tones
      score += tmpl.required.length * 6;
      const matchedOptional = actualIntervalMask & tmpl.optionalMask;
      score += popcount(matchedOptional) * 4;

      // Penalize extra unaccounted notes
      const extraMask = actualIntervalMask & ~tmpl.allowedMask;
      const extraCount = popcount(extraMask);
      score -= extraCount * 30;

      // Bass note heuristics:
      if (root === bassPc) {
        // Root position chord is acoustically most prominent
        score += 25;
      } else {
        // Inversion: check if the bass note is an allowable chord tone
        const bassInterval = (bassPc - root + 12) % 12;
        if ((tmpl.allowedMask & (1 << bassInterval)) !== 0) {
          score -= 15; // Standard inversion (e.g. 3rd, 5th, or 7th in bass)
        } else {
          score -= 35; // Foreign bass note penalty
        }
      }

      if (score > bestScore) {
        bestScore = score;
        bestRoot = root;
        bestTemplate = tmpl;
      }
    }
  }

  if (!bestTemplate || bestScore < 0) {
    return '';
  }

  const rootName = NOTE_NAMES[bestRoot];
  const chordName = rootName + bestTemplate.quality;

  // Slash chord / inversion handling:
  // If the lowest sounding pitch class is not the chord root, append /Bass
  if (bestRoot !== bassPc) {
    const bassName = NOTE_NAMES[bassPc];
    return `${chordName}/${bassName}`;
  }

  return chordName;
}
