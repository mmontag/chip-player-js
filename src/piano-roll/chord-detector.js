// Chord Detection and Harmonic Analysis for Piano Roll
// Evaluates actively sounding MIDI notes using interval scoring with jazz heuristics.

export const NOTE_NAMES = ['C', 'C♯', 'D', 'E♭', 'E', 'F', 'F♯', 'G', 'A♭', 'A', 'B♭', 'B'];

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
    priority: 80,
    isExtension: true,
    extensionIntervals: [9],
  },
  {
    quality: 'maj13',
    required: [0, 4, 11, 9], // 1, 3, 7, 13
    optional: [7, 2],        // 5, 9
    avoid: [1, 3, 8, 10],
    priority: 80,
    isExtension: true,
    extensionIntervals: [9],
  },
  {
    quality: 'm13',
    required: [0, 3, 10, 9], // 1, b3, b7, 13
    optional: [7, 2],        // 5, 9
    avoid: [1, 4, 8, 11],
    priority: 80,
    isExtension: true,
    extensionIntervals: [9],
  },
  {
    quality: '7(♭13)',
    required: [0, 4, 10, 8], // 1, 3, b7, b13
    optional: [2],           // 9
    avoid: [1, 7, 9, 11],
    priority: 78,
    isExtension: true,
    extensionIntervals: [8],
  },

  // --- 11th Chords ---
  {
    quality: '11',
    required: [0, 10, 2, 5], // 1, b7, 9, 11 (3 omitted in dominant 11)
    optional: [7, 4],        // 5, 3
    avoid: [1, 8, 9, 11],
    priority: 76,
    isExtension: true,
    extensionIntervals: [2, 5],
  },
  {
    quality: 'm11',
    required: [0, 3, 10, 5], // 1, b3, b7, 11
    optional: [7, 2],        // 5, 9
    avoid: [1, 4, 8, 9, 11],
    priority: 76,
    isExtension: true,
    extensionIntervals: [5],
  },
  {
    quality: 'maj7(♯11)',
    required: [0, 4, 11, 6], // 1, 3, 7, #11
    optional: [7, 2, 9],     // 5, 9, 13
    avoid: [1, 3, 5, 8, 10],
    priority: 76,
    isExtension: true,
    extensionIntervals: [6],
  },
  {
    quality: '7(♯11)',
    required: [0, 4, 10, 6], // 1, 3, b7, #11
    optional: [7, 2, 9],     // 5, 9, 13
    avoid: [1, 3, 5, 8, 11],
    priority: 76,
    isExtension: true,
    extensionIntervals: [6],
  },

  // --- 9th Chords ---
  {
    quality: '9',
    required: [0, 4, 10, 2], // 1, 3, b7, 9
    optional: [7],           // 5 (omitted 5th common in jazz)
    avoid: [1, 3, 8, 9, 11],
    priority: 72,
    isExtension: true,
    extensionIntervals: [2],
  },
  {
    quality: 'maj9',
    required: [0, 4, 11, 2], // 1, 3, 7, 9
    optional: [7],           // 5
    avoid: [1, 3, 8, 9, 10],
    priority: 72,
    isExtension: true,
    extensionIntervals: [2],
  },
  {
    quality: 'm9',
    required: [0, 3, 10, 2], // 1, b3, b7, 9
    optional: [7],           // 5
    avoid: [1, 4, 8, 9, 11],
    priority: 72,
    isExtension: true,
    extensionIntervals: [2],
  },
  {
    quality: 'm(maj9)',
    required: [0, 3, 11, 2], // 1, b3, 7, 9
    optional: [7],           // 5
    avoid: [1, 4, 8, 9, 10],
    priority: 72,
    isExtension: true,
    extensionIntervals: [2],
  },
  {
    quality: '7(♭9)',
    required: [0, 4, 10, 1], // 1, 3, b7, b9
    optional: [7],           // 5
    avoid: [2, 3, 8, 9, 11],
    priority: 74,
    isExtension: true,
    extensionIntervals: [1],
  },
  {
    quality: '7(♯9)',
    required: [0, 4, 10, 3], // 1, 3, b7, #9 (Hendrix chord)
    optional: [7],           // 5
    avoid: [1, 2, 8, 9, 11],
    priority: 74,
    isExtension: true,
    extensionIntervals: [3],
  },
  {
    quality: 'add9',
    required: [0, 4, 2],     // 1, 3, 9 (no 7th)
    optional: [7],           // 5
    avoid: [1, 3, 10, 11],
    priority: 44,
    isExtension: true,
    extensionIntervals: [2],
  },
  {
    quality: 'm(add9)',
    required: [0, 3, 2],     // 1, b3, 9 (no 7th)
    optional: [7],           // 5
    avoid: [1, 4, 10, 11],
    priority: 44,
    isExtension: true,
    extensionIntervals: [2],
  },

  // --- 7th Chords ---
  {
    quality: '7',
    required: [0, 4, 10],    // 1, 3, b7
    optional: [7],           // 5
    avoid: [1, 3, 11],
    tensions: [2, 9],
    priority: 62,
  },
  {
    quality: 'maj7',
    required: [0, 4, 11],    // 1, 3, 7
    optional: [7],           // 5
    avoid: [1, 3, 5, 10],
    tensions: [2, 9],
    priority: 62,
  },
  {
    quality: 'm7',
    required: [0, 3, 10],    // 1, b3, b7
    optional: [7],           // 5
    avoid: [1, 4, 11],
    tensions: [2, 5, 9],
    priority: 62,
  },
  {
    quality: 'm(maj7)',
    required: [0, 3, 11],    // 1, b3, 7
    optional: [7],           // 5
    avoid: [1, 4, 10],
    tensions: [2, 9],
    priority: 62,
  },
  {
    quality: 'm7♭5',         // Half-diminished
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
    quality: '7♯5',
    required: [0, 4, 8, 10], // 1, 3, #5, b7
    optional: [],
    avoid: [1, 2, 7, 9, 11],
    priority: 65,
  },
  {
    quality: 'maj7♯5',
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
    avoid: [1, 3, 10, 11],
    tensions: [2],
    priority: 44,
    isExtension: true,
    extensionIntervals: [9],
  },
  {
    quality: 'm6',
    required: [0, 3, 9],     // 1, b3, 6
    optional: [7],           // 5
    avoid: [1, 4, 10, 11],
    tensions: [2],
    priority: 44,
    isExtension: true,
    extensionIntervals: [9],
  },
  {
    quality: '6/9',
    required: [0, 4, 9, 2],  // 1, 3, 6, 9
    optional: [7],           // 5
    avoid: [1, 3, 10, 11],
    priority: 64,
    isExtension: true,
    extensionIntervals: [2],
  },

  // --- Triads ---
  {
    quality: '',             // Major triad
    required: [0, 4],        // 1, 3
    optional: [7],           // 5
    avoid: [1, 3, 5, 8],
    tensions: [2, 9],
    priority: 42,
  },
  {
    quality: 'm',            // Minor triad
    required: [0, 3],        // 1, b3
    optional: [7],           // 5
    avoid: [1, 4, 6, 8],
    tensions: [2, 5, 9],
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

// Precompute 12-bit masks for required, optional, allowed intervals, and compatible tensions
for (const tmpl of CHORD_TEMPLATES) {
  tmpl.requiredMask = tmpl.required.reduce((m, i) => m | (1 << i), 0);
  tmpl.optionalMask = (tmpl.optional || []).reduce((m, i) => m | (1 << i), 0);
  tmpl.allowedMask = tmpl.requiredMask | tmpl.optionalMask;
  tmpl.tensionMask = (tmpl.tensions || []).reduce((m, i) => m | (1 << i), 0);
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
 * @param {boolean} [options.allowExtensions=true] Whether to detect 9th, 11th, 13th, and add9 extensions
 * @param {number} [options.extensionThreshold=0.5] Resistance to detecting extensions (0.0=eager, 1.0=strict)
 * @param {number} [options.extensionMinWeight=0.6] Minimum activation weight for extension notes
 * @returns {string} e.g. "C", "Dm7", "G7(♭9)", "Cmaj13", "F/G", or ""
 */
export function detectChord(soundingNotes, options = {}) {
  if (!soundingNotes || soundingNotes.length === 0) return '';

  const minNotes = options.minNotes !== undefined ? options.minNotes : 2;
  const excludeAtonal = options.excludeAtonal !== false;
  const weights = options.weights || null;
  const allowExtensions = options.allowExtensions !== false;
  const extensionThreshold = typeof options.extensionThreshold === 'number' ? options.extensionThreshold : 0.5;
  const extensionMinWeight = typeof options.extensionMinWeight === 'number' ? options.extensionMinWeight : 0.6;

  let bassPitch = typeof options.bassPitch === 'number' ? options.bassPitch : Infinity;
  let pitchClassesMask = 0;
  const uniquePcs = [];

  for (let i = 0; i < soundingNotes.length; i++) {
    const item = soundingNotes[i];
    const pitch = typeof item === 'number' ? item : item.pitch;

    if (typeof pitch !== 'number' || isNaN(pitch) || pitch < 0) continue;

    // Exclude atonal instruments (channel 9 drums, atonal GM patches, sound effects)
    if (excludeAtonal && isAtonalInstrument(item)) continue;

    if (options.bassPitch === undefined && pitch < bassPitch) {
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

  const bassPc = bassPitch !== Infinity ? (((Math.round(bassPitch) % 12) + 12) % 12) : -1;

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

      // Extension restrictions:
      if (tmpl.isExtension) {
        if (!allowExtensions) continue;

        // If weights are provided, extension intervals must meet minimum activation weight
        if (weights && tmpl.extensionIntervals) {
          let extensionMet = true;
          for (let e = 0; e < tmpl.extensionIntervals.length; e++) {
            const extPc = (root + tmpl.extensionIntervals[e]) % 12;
            const w = weights[extPc] !== undefined ? weights[extPc] : 1;
            if (w < extensionMinWeight) {
              extensionMet = false;
              break;
            }
          }
          if (!extensionMet) continue;
        }
      }

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

      // Penalize extended chords according to extensionThreshold (simplicity preference)
      if (tmpl.isExtension) {
        score -= extensionThreshold * 40;
      }

      // Reward matching required and optional tones
      if (weights) {
        let reqWeight = 0;
        for (let r = 0; r < tmpl.required.length; r++) {
          const pc = (root + tmpl.required[r]) % 12;
          reqWeight += weights[pc] !== undefined ? weights[pc] : 1;
        }
        score += reqWeight * 6;

        let optWeight = 0;
        if (tmpl.optional) {
          for (let o = 0; o < tmpl.optional.length; o++) {
            const optInterval = tmpl.optional[o];
            if ((actualIntervalMask & (1 << optInterval)) !== 0) {
              const pc = (root + optInterval) % 12;
              optWeight += weights[pc] !== undefined ? weights[pc] : 1;
            }
          }
        }
        score += optWeight * 4;

        // Penalize extra unaccounted notes weighted by their energy
        let extraPenalty = 0;
        for (let pIdx = 0; pIdx < uniquePcs.length; pIdx++) {
          const pc = uniquePcs[pIdx];
          const interval = (pc - root + 12) % 12;
          if ((tmpl.allowedMask & (1 << interval)) === 0) {
            const w = weights[pc] !== undefined ? weights[pc] : 1;
            if (tmpl.tensionMask && (tmpl.tensionMask & (1 << interval)) !== 0) {
              // Compatible melodic tension receives a mild penalty instead of full clash penalty
              extraPenalty += (6 + 10 * extensionThreshold) * w;
            } else {
              extraPenalty += 30 * w;
            }
          }
        }
        score -= extraPenalty;
      } else {
        score += tmpl.required.length * 6;
        const matchedOptional = actualIntervalMask & tmpl.optionalMask;
        score += popcount(matchedOptional) * 4;

        let extraPenalty = 0;
        for (let pIdx = 0; pIdx < uniquePcs.length; pIdx++) {
          const pc = uniquePcs[pIdx];
          const interval = (pc - root + 12) % 12;
          if ((tmpl.allowedMask & (1 << interval)) === 0) {
            if (tmpl.tensionMask && (tmpl.tensionMask & (1 << interval)) !== 0) {
              extraPenalty += (6 + 10 * extensionThreshold);
            } else {
              extraPenalty += 30;
            }
          }
        }
        score -= extraPenalty;
      }

      // Bass note heuristics:
      if (bassPc >= 0) {
        const bassWeight = weights ? (weights[bassPc] !== undefined ? weights[bassPc] : 1) : 1;
        if (root === bassPc) {
          // Root position chord is acoustically most prominent
          score += 25 * bassWeight;
        } else {
          // Inversion: check if the bass note is an allowable chord tone
          const bassInterval = (bassPc - root + 12) % 12;
          if ((tmpl.allowedMask & (1 << bassInterval)) !== 0) {
            score -= 15 * bassWeight; // Standard inversion (e.g. 3rd, 5th, or 7th in bass)
          } else {
            score -= 35 * bassWeight; // Foreign bass note penalty
          }
        }
      }

      if (weights) {
        const rootWeight = weights[root] !== undefined ? weights[root] : 1;
        if (rootWeight < 0.6 && root !== bassPc) {
          // A decaying or soft note cannot act as the root of an inverted / slash chord
          score -= (1.0 - rootWeight) * 50;
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
  if (bassPc >= 0 && bestRoot !== bassPc) {
    const bassName = NOTE_NAMES[bassPc];
    return `${chordName}/${bassName}`;
  }

  return chordName;
}

/**
 * Stateful Leaky Integrator for temporal chord recognition.
 * Accumulates pitch class activations over time with exponential decay,
 * allowing arpeggios, stride bass, and broken chords to be smoothly recognized,
 * while debouncing transient passing notes to prevent display flicker.
 */
export class ChordIntegrator {
  constructor(config = {}) {
    this.config = config;
    this.pitchActivations = new Float32Array(12);
    this.bassPitch = Infinity;
    this.bassWeight = 0;
    this.lastTimeMs = -1;
    this.currentChord = '';
    this.pendingChord = null;
    this.pendingChordTimeMs = 0;
    this.prevSoundingPitches = new Set();
    this.prevSoundingCount = 0;
  }

  reset() {
    this.pitchActivations.fill(0);
    this.bassPitch = Infinity;
    this.bassWeight = 0;
    this.lastTimeMs = -1;
    this.currentChord = '';
    this.pendingChord = null;
    this.pendingChordTimeMs = 0;
    this.prevSoundingPitches = new Set();
    this.prevSoundingCount = 0;
  }

  update(soundingNotes, currentTimeMs, config = {}) {
    const isLeakyEnabled = config.HARMONIC_LEAKY_INTEGRATOR !== false;
    const minNotes = config.HARMONIC_ANALYSIS_MIN_NOTES !== undefined ? config.HARMONIC_ANALYSIS_MIN_NOTES : 2;
    const excludeAtonal = config.HARMONIC_ANALYSIS_EXCLUDE_ATONAL !== false;
    const allowExtensions = config.HARMONIC_ALLOW_EXTENSIONS !== false;
    const extensionThreshold = typeof config.HARMONIC_EXTENSION_THRESHOLD === 'number' ? config.HARMONIC_EXTENSION_THRESHOLD : 0.5;
    const extensionMinWeight = typeof config.HARMONIC_EXTENSION_MIN_WEIGHT === 'number' ? config.HARMONIC_EXTENSION_MIN_WEIGHT : 0.6;
    const flushOnChord = config.HARMONIC_FLUSH_ON_CHORD !== false;
    const preserveStrideBass = config.HARMONIC_PRESERVE_STRIDE_BASS !== false;
    const flushOnOnsetSurge = config.HARMONIC_FLUSH_ON_ONSET_SURGE !== false;

    // If leaky integrator is disabled, perform instantaneous detection directly
    if (!isLeakyEnabled) {
      this.reset();
      const detected = detectChord(soundingNotes, {
        minNotes,
        excludeAtonal,
        allowExtensions,
        extensionThreshold,
        extensionMinWeight,
      });
      this.currentChord = detected;
      return detected;
    }

    const decayMs = config.HARMONIC_DECAY_MS || 600;
    const bassDecayMs = config.HARMONIC_BASS_DECAY_MS || 1000;
    const threshold = config.HARMONIC_ACTIVATION_THRESHOLD !== undefined ? config.HARMONIC_ACTIVATION_THRESHOLD : 0.15;
    const changeThresholdMs = config.HARMONIC_CHANGE_THRESHOLD_MS !== undefined ? config.HARMONIC_CHANGE_THRESHOLD_MS : 50;

    // Handle seeking, time jumps, or initial frame
    const dt = this.lastTimeMs >= 0 ? currentTimeMs - this.lastTimeMs : 0;
    if (this.lastTimeMs < 0 || dt < 0 || dt > 1500) {
      // Discontinuity detected: reset leaky memory
      this.pitchActivations.fill(0);
      this.bassPitch = Infinity;
      this.bassWeight = 0;
      this.pendingChord = null;
      this.prevSoundingPitches.clear();
      this.prevSoundingCount = 0;
    } else if (dt > 0) {
      // 1. Decay pitch activations
      const decayFactor = Math.exp(-dt / decayMs);
      for (let i = 0; i < 12; i++) {
        this.pitchActivations[i] *= decayFactor;
        if (this.pitchActivations[i] < 0.001) {
          this.pitchActivations[i] = 0;
        }
      }

      // 2. Decay bass memory
      if (this.bassPitch !== Infinity) {
        const bassDecayFactor = Math.exp(-dt / bassDecayMs);
        this.bassWeight *= bassDecayFactor;
        if (this.bassWeight < 0.05) {
          this.bassPitch = Infinity;
          this.bassWeight = 0;
        }
      }
    }
    this.lastTimeMs = currentTimeMs;

    // 3. Inspect actively sounding notes and detect onsets
    const soundingPcs = new Set();
    let lowestSoundingPitch = Infinity;
    const currentSoundingPitches = new Set();
    const newOnsetPitchClasses = new Set();
    let activeCount = 0;

    if (soundingNotes && soundingNotes.length > 0) {
      for (let i = 0; i < soundingNotes.length; i++) {
        const note = soundingNotes[i];
        if (excludeAtonal && isAtonalInstrument(note)) continue;

        const pitch = typeof note === 'number' ? note : note.pitch;
        if (typeof pitch !== 'number' || isNaN(pitch) || pitch < 0) continue;

        const roundedPitch = Math.round(pitch);
        const pc = ((roundedPitch % 12) + 12) % 12;
        soundingPcs.add(pc);
        currentSoundingPitches.add(roundedPitch);
        activeCount++;

        if (!this.prevSoundingPitches.has(roundedPitch)) {
          newOnsetPitchClasses.add(pc);
        }

        if (pitch < lowestSoundingPitch) {
          lowestSoundingPitch = pitch;
        }
      }
    }

    // Check for active stride bass (only valid if previous event was sparse < 3 notes, e.g. single bass note)
    const hasActiveStrideBass = preserveStrideBass &&
      this.prevSoundingCount < 3 &&
      this.bassPitch < 60 &&
      this.bassPitch < lowestSoundingPitch &&
      this.bassWeight > 0.3;

    // Smart Flushing Heuristic A: Onset Surge (Coordinated block chord arrival)
    if (flushOnOnsetSurge && newOnsetPitchClasses.size >= 2 && soundingPcs.size >= 3) {
      if (!hasActiveStrideBass || lowestSoundingPitch < 60) {
        this.bassPitch = lowestSoundingPitch;
        this.bassWeight = 1.0;
        this.bassTimeMs = currentTimeMs;
      }
    }

    // Smart Flushing Heuristic B: Sufficient Sounding Harmony Flush
    // When >= 3 notes are actively sounding right now, flush non-sounding ghost notes
    if (flushOnChord && soundingPcs.size >= 3) {
      const bassPc = (this.bassPitch !== Infinity && this.bassPitch >= 0)
        ? ((Math.round(this.bassPitch) % 12) + 12) % 12
        : -1;

      for (let pc = 0; pc < 12; pc++) {
        if (!soundingPcs.has(pc)) {
          if (hasActiveStrideBass && pc === bassPc) {
            // Retain active low stride bass note
          } else {
            this.pitchActivations[pc] = 0;
          }
        }
      }
    }

    // 4. Inject energy from actively sounding notes
    if (soundingNotes && soundingNotes.length > 0) {
      for (let i = 0; i < soundingNotes.length; i++) {
        const note = soundingNotes[i];
        if (excludeAtonal && isAtonalInstrument(note)) continue;

        const pitch = typeof note === 'number' ? note : note.pitch;
        if (typeof pitch !== 'number' || isNaN(pitch) || pitch < 0) continue;

        const pc = ((Math.round(pitch) % 12) + 12) % 12;
        const velocity = (typeof note === 'object' && typeof note.velocity === 'number')
          ? note.velocity
          : 100;
        const weight = Math.max(0.5, Math.min(1.0, velocity / 127));

        this.pitchActivations[pc] = Math.max(this.pitchActivations[pc], weight);
      }
    }

    // Update bass memory for non-surge cases
    if (lowestSoundingPitch !== Infinity) {
      if (
        this.bassPitch === Infinity ||
        lowestSoundingPitch <= this.bassPitch ||
        this.bassWeight < 0.35 ||
        (!hasActiveStrideBass && lowestSoundingPitch < 60) ||
        (dt > 0 && currentTimeMs - this.bassTimeMs > (bassDecayMs * 0.6) && lowestSoundingPitch < 60)
      ) {
        this.bassPitch = lowestSoundingPitch;
        this.bassWeight = 1.0;
        this.bassTimeMs = currentTimeMs;
      }
    }

    this.prevSoundingPitches = currentSoundingPitches;
    this.prevSoundingCount = activeCount;

    // 4. Gather active pitch classes above threshold
    const activeNotes = [];
    const activeWeights = [];

    for (let pc = 0; pc < 12; pc++) {
      const act = this.pitchActivations[pc];
      if (act >= threshold) {
        activeNotes.push({
          pitch: 60 + pc,
          weight: act,
        });
        activeWeights[pc] = act;
      } else {
        activeWeights[pc] = 0;
      }
    }

    // Determine effective bass pitch:
    let effectiveBassPitch = this.bassPitch;
    if (effectiveBassPitch === Infinity || this.bassWeight < threshold) {
      for (let pc = 0; pc < 12; pc++) {
        if (this.pitchActivations[pc] >= threshold) {
          effectiveBassPitch = 60 + pc;
          break;
        }
      }
    }

    // Run chord detection on active pitch classes with weights
    const rawChord = detectChord(activeNotes, {
      minNotes,
      weights: activeWeights,
      bassPitch: effectiveBassPitch,
      excludeAtonal: false,
      allowExtensions,
      extensionThreshold,
      extensionMinWeight,
    });

    // 5. Chord Stability / Debounce (Hysteresis)
    if (changeThresholdMs <= 0 || !this.currentChord || dt === 0) {
      this.currentChord = rawChord;
      this.pendingChord = null;
      return rawChord;
    }

    if (rawChord === this.currentChord) {
      this.pendingChord = null;
      return this.currentChord;
    }

    if (this.pendingChord !== rawChord) {
      this.pendingChord = rawChord;
      this.pendingChordTimeMs = currentTimeMs;
    } else if (currentTimeMs - this.pendingChordTimeMs >= changeThresholdMs) {
      this.currentChord = rawChord;
      this.pendingChord = null;
    }

    return this.currentChord;
  }
}

const NOTE_TO_SEMITONE = {
  'C': 0, 'B#': 0, 'B♯': 0,
  'C#': 1, 'C♯': 1, 'Db': 1, 'D♭': 1,
  'D': 2,
  'D#': 3, 'D♯': 3, 'Eb': 3, 'E♭': 3,
  'E': 4, 'Fb': 4, 'F♭': 4,
  'F': 5, 'E#': 5, 'E♯': 5,
  'F#': 6, 'F♯': 6, 'Gb': 6, 'G♭': 6,
  'G': 7,
  'G#': 8, 'G♯': 8, 'Ab': 8, 'A♭': 8,
  'A': 9,
  'A#': 10, 'A♯': 10, 'Bb': 10, 'B♭': 10,
  'B': 11, 'Cb': 11, 'C♭': 11,
};

/**
 * Converts a chord name (e.g. 'Cmaj7', 'Am/G', 'G7(♭9)') into an array of MIDI pitch numbers
 * voiced specifically for acoustic piano auditioning.
 *
 * @param {string} chordName - Detected chord string
 * @returns {number[]} Array of MIDI note numbers in ascending order
 */
export function getChordPitches(chordName) {
  if (!chordName || typeof chordName !== 'string') return [];
  const parts = chordName.split('/');
  const chordPart = parts[0].trim();
  const slashBass = parts.length > 1 ? parts[1].trim() : null;

  let rootPitchClass = -1;
  let quality = '';
  if (chordPart.length >= 2 && NOTE_TO_SEMITONE[chordPart.slice(0, 2)] !== undefined) {
    rootPitchClass = NOTE_TO_SEMITONE[chordPart.slice(0, 2)];
    quality = chordPart.slice(2);
  } else if (chordPart.length >= 1 && NOTE_TO_SEMITONE[chordPart.slice(0, 1)] !== undefined) {
    rootPitchClass = NOTE_TO_SEMITONE[chordPart.slice(0, 1)];
    quality = chordPart.slice(1);
  } else {
    return [];
  }

  let bassPitchClass = rootPitchClass;
  if (slashBass && NOTE_TO_SEMITONE[slashBass] !== undefined) {
    bassPitchClass = NOTE_TO_SEMITONE[slashBass];
  }

  const tmpl = CHORD_TEMPLATES.find(t => t.quality === quality);
  let intervals = tmpl ? [...tmpl.required] : [0, 4, 7];
  if (tmpl && tmpl.optional) {
    if (tmpl.optional.includes(7) && !intervals.includes(6) && !intervals.includes(8) && !tmpl.quality.includes('13')) {
      intervals.push(7);
    }
    if ((tmpl.quality.includes('13') || tmpl.quality === '6/9') && tmpl.optional.includes(2)) {
      intervals.push(2);
    }
  }

  // Bass note: Octave 2-3 (between E2 (40) and Eb3 (51))
  let bass = 36 + bassPitchClass;
  if (bass < 40) bass += 12;

  // Right hand root around C4 (between F3 (53) and E4 (64)):
  let rhRoot = 60 + rootPitchClass;
  if (rhRoot >= 65) rhRoot -= 12;

  const extensionIntervals = tmpl && tmpl.extensionIntervals ? tmpl.extensionIntervals : [];
  const chordPitches = [];

  // Right hand chord tones:
  for (const iv of intervals) {
    let semitonesAboveRoot = iv;
    if (extensionIntervals.includes(iv) || (iv === 2 && tmpl && (tmpl.quality.includes('13') || tmpl.quality.includes('9')))) {
      semitonesAboveRoot += 12;
    }
    chordPitches.push(rhRoot + semitonesAboveRoot);
  }

  return Array.from(new Set([bass, ...chordPitches])).sort((a, b) => a - b);
}
