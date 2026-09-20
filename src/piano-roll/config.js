// Configuration constants for the Piano Roll Visualizer.
// All variable behaviors and appearance settings are specified here for easy editing.

export const PIANO_ROLL_CONFIG = {
  // Animation behavior:
  // - 'continuous': Notes scroll smoothly past a stationary playhead line.
  // - 'paginated': Viewport remains static for a page of time while the playhead
  //                moves across; once it reaches the boundary, the viewport flips to the next page.
  ANIMATION_BEHAVIOR: 'continuous',

  // Orientation:
  // - 'vertical': Pitch runs across the width, time runs along the height.
  // - 'horizontal': Pitch runs along the height, time runs across the width.
  ORIENTATION: 'vertical',

  // Direction:
  // - 'forward': Standard flow towards the playhead (top-to-bottom in vertical, right-to-left in horizontal).
  // - 'reverse': Reverse flow (bottom-to-top in vertical, left-to-right in horizontal).
  DIRECTION: 'forward',

  // Sync position of the playhead line:
  // Options: 'start', 'middle', 'end', or a normalized number (0.0 to 1.0).
  // - 'end': Notes flow towards the playhead (bottom in vertical forward, left in horizontal forward).
  // - 'middle': Centered in the viewport.
  // - 'start': Notes flow away from the playhead (top in vertical forward, right in horizontal forward).
  SYNC_POSITION: 'end',

  // Offset in pixels for the playhead from the viewport boundary when SYNC_POSITION is 'start' or 'end'.
  PLAYHEAD_OFFSET_PX: 0,

  // Time Zoom:
  // - 'fixed': Constant scrolling speed in pixels per second.
  // - 'fit-song': Dynamically scale time so the entire song duration fits within the viewport.
  TIME_ZOOM_MODE: 'fixed',

  // Scrolling speed in pixels per second when TIME_ZOOM_MODE is 'fixed'.
  // Independent of MIDI tempo and PPQ (100px/sec = 0.1 px/ms).
  PIXELS_PER_SECOND: 100,

  // Pitch Zoom:
  // - 'fill': Evenly scale notes across the pitch axis to fill the viewport dimensions.
  // - 'fixed': Use a fixed number of pixels per note lane (PIXELS_PER_NOTE).
  PITCH_ZOOM_MODE: 'fill',

  // Pixels per note lane when PITCH_ZOOM_MODE is 'fixed'.
  // 5px * 88 notes = 440px (neatly fits 448px visualizer width with 4px margin).
  PIXELS_PER_NOTE: 5,


  // Color mode:
  // - 'channel': Notes are colored by MIDI channel (0-15).
  // - 'track': Notes are colored by MIDI track index.
  COLOR_BY: 'channel',

  // Background and lane styling:
  BACKGROUND_COLOR: '#222',
  BLACK_KEY_LANE_TINT: 'rgba(0, 0, 0, 0.2)',
  WHITE_KEY_LANE_COLOR: '#000000',
  GRID_LINE_COLOR: 'rgba(255, 255, 255, 0.08)',
  OCTAVE_LINE_COLOR: 'rgba(255, 255, 255, 0.1)',

  // Playhead line styling:
  PLAYHEAD_COLOR: '#ffff0000',
  PLAYHEAD_LINE_WIDTH: 1,

  // 16 distinct, high-contrast colors for MIDI channels 0 to 15:
  CHANNEL_COLORS: [
    '#38bdf8', // Ch 1: Sky Blue
    '#ec4899', // Ch 2: Pink
    '#10b981', // Ch 3: Emerald Green
    '#f59e0b', // Ch 4: Amber / Warm Gold
    '#8b5cf6', // Ch 5: Violet / Purple
    '#06b6d4', // Ch 6: Cyan
    '#ef4444', // Ch 7: Red
    '#84cc16', // Ch 8: Lime Green
    '#f97316', // Ch 9: Orange
    '#a855f7', // Ch 10: Percussion / Drums (Lavender/Purple)
    '#14b8a6', // Ch 11: Teal
    '#e11d48', // Ch 12: Rose
    '#6366f1', // Ch 13: Indigo
    '#d97706', // Ch 14: Dark Amber
    '#22c55e', // Ch 15: Bright Green
    '#eab308', // Ch 16: Yellow
  ],

  // Track colors for fallback or when COLOR_BY is 'track':
  TRACK_COLORS: [
    '#38bdf8', '#ec4899', '#10b981', '#f59e0b',
    '#8b5cf6', '#06b6d4', '#ef4444', '#84cc16',
    '#f97316', '#a855f7', '#14b8a6', '#e11d48',
    '#6366f1', '#d97706', '#22c55e', '#eab308'
  ],

  // Note appearance:
  SHOW_NOTE_NAMES: false, // Display note names (e.g. C#4) on notes if box size permits
  NOTE_MIN_LENGTH_PX: 3,  // Minimum length in pixels to ensure very brief notes remain visible
  NOTE_CORNER_RADIUS: 3,  // Border radius for drawn notes
  NOTE_GAP_PX: 1,         // Gap between adjacent notes horizontally/vertically
  ACTIVE_NOTE_GLOW: true, // Brightness boost for actively sounding notes at the playhead
  ACTIVE_NOTE_GLOW_OPACITY: 0.8, // Opacity of the brightness boost overlay for actively sounding notes (0.0 to 1.0)
  ACTIVE_NOTE_FATTEN: 3,  // Extra width in pixels for actively sounding notes
  ACTIVE_NOTE_DECAY_MS: 640, // Duration in ms for active note highlight/grow fade-out (fixed decay)
  ACTIVE_NOTE_EASING: 'ease-out', // Fade-out easing: 'linear', 'ease-out', 'ease-in', 'cubic', 'sine', 'exponential'
  MUTED_OPACITY: 0.15,    // Opacity for notes on muted channels
  SUSTAIN_OPACITY: 0.5,   // Opacity for the portion of a note held by the sustain pedal (CC 64)
  ENABLE_PITCH_BEND: true, // Visualize MIDI pitch bends as continuous ribbons
  // Piano Keyboard settings:
  SHOW_KEYBOARD: true, // Display interactive piano keyboard under the playhead
  KEYBOARD_ASPECT_RATIO: 0.11, // Height-to-width ratio of the 88-key keyboard (0.125 * 440px = 55px)
  KEYBOARD_BLACK_KEY_HEIGHT_RATIO: 0.6, // Height of black keys as a fraction of white key height
  KEYBOARD_WHITE_KEY_COLOR: '#ffffff', // Rest fill color for white keys
  KEYBOARD_BLACK_KEY_COLOR: '#111111', // Rest fill color for black keys
  KEYBOARD_STROKE_COLOR: '#444444', // 1-pixel stroke between white keys and bottom border
  KEYBOARD_BLACK_KEY_STROKE_COLOR: '#000000', // Outline stroke for black keys
  KEYBOARD_SUSTAIN_ILLUMINATION: true, // Illuminate keys held by sustain pedal at sustain opacity
  // Harmonic Analysis settings:
  SHOW_HARMONIC_ANALYSIS: true, // Display detected chord label in the visualizer
  HARMONIC_ANALYSIS_MIN_NOTES: 2, // Minimum distinct pitch classes required to detect a chord
  HARMONIC_ANALYSIS_EXCLUDE_ATONAL: true, // Exclude atonal instruments (synth toms, taiko drum, woodblock, SFX)
};

export const DECAY_EASINGS = {
  'linear': (t) => 1 - t,
  'ease-out': (t) => (1 - t) * (1 - t),
  'quad-out': (t) => (1 - t) * (1 - t),
  'ease-in': (t) => 1 - t * t,
  'quad-in': (t) => 1 - t * t,
  'cubic': (t) => Math.pow(1 - t, 3),
  'cubic-out': (t) => Math.pow(1 - t, 3),
  'sine': (t) => Math.cos(t * Math.PI * 0.5),
  'sine-out': (t) => Math.cos(t * Math.PI * 0.5),
  'exponential': (t) => (Math.exp(-4 * t) - Math.exp(-4)) / (1 - Math.exp(-4)),
  'exp-out': (t) => (Math.exp(-4 * t) - Math.exp(-4)) / (1 - Math.exp(-4)),
};

export function getDecayIntensity(progress, easing = 'linear') {
  const t = Math.max(0, Math.min(1, progress));
  if (typeof easing === 'function') {
    return Math.max(0, Math.min(1, easing(t)));
  }
  const fn = DECAY_EASINGS[easing] || DECAY_EASINGS.linear;
  return Math.max(0, Math.min(1, fn(t)));
}

