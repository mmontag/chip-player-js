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
  // In vertical orientation:
  // - 'top-to-bottom': Waterfall style. Playhead is at the top, notes cascade downwards.
  // - 'bottom-to-top': Notes scroll upwards towards or past the playhead.
  // In horizontal orientation:
  // - 'left-to-right': Time flows from left to right.
  // - 'right-to-left': Time flows from right to left.
  DIRECTION: 'top-to-bottom',

  // Sync position of the playhead line:
  // Options: 'top', 'center', 'bottom', 'left', 'right', or a normalized number (0.0 to 1.0).
  // Default 'top' matches the Spectrogram's waterfall behavior.
  SYNC_POSITION: 'top',

  // Offset in pixels for the playhead from the top/left edge when SYNC_POSITION is 'top' or 'left'.
  PLAYHEAD_OFFSET_PX: 2,

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
  PITCH_ZOOM_MODE: 'fixed',

  // Pixels per note lane when PITCH_ZOOM_MODE is 'fixed'.
  // 5px * 88 notes = 440px (neatly fits 448px visualizer width with 4px margin).
  PIXELS_PER_NOTE: 5,

  // Pitch range:
  // - 88: Standard piano keyboard range (MIDI notes 21 [A0] to 108 [C8]).
  // - 128: Full MIDI note range (MIDI notes 0 [C-1] to 127 [G9]).
  NOTE_RANGE: 88,

  // Color mode:
  // - 'channel': Notes are colored by MIDI channel (0-15).
  // - 'track': Notes are colored by MIDI track index.
  COLOR_BY: 'channel',

  // Background and lane styling:
  BACKGROUND_COLOR: '#000000',
  BLACK_KEY_LANE_TINT: 'rgba(255, 255, 255, 0.05)', // 5% tint for black keys (C#, D#, F#, G#, A#)
  WHITE_KEY_LANE_COLOR: '#000000',
  GRID_LINE_COLOR: 'rgba(255, 255, 255, 0.08)',
  OCTAVE_LINE_COLOR: 'rgba(255, 255, 255, 0.22)',

  // Playhead line styling:
  PLAYHEAD_COLOR: '#ff3366',
  PLAYHEAD_LINE_WIDTH: 2,

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

  // Top-left overlay list showing channels/tracks:
  SHOW_TRACK_CHANNEL_LIST: true,
  LIST_MODE: 'channel', // 'channel' or 'track'

  // Note appearance:
  SHOW_NOTE_NAMES: false, // Display note names (e.g. C#4) on notes if box size permits
  NOTE_MIN_LENGTH_PX: 3,  // Minimum length in pixels to ensure very brief notes remain visible
  NOTE_CORNER_RADIUS: 2,  // Border radius for drawn notes
  NOTE_GAP_PX: 1,         // Gap between adjacent notes horizontally/vertically
  ACTIVE_NOTE_GLOW: true, // Brightness boost for actively sounding notes at the playhead
  MUTED_OPACITY: 0.15,    // Opacity for notes on muted channels
  SUSTAIN_OPACITY: 0.5,   // Opacity for the portion of a note held by the sustain pedal (CC 64)
};
