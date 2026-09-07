# Piano Roll Visualizer Implementation Plan

## Goal Description
Build a modular, encapsulated **Piano Roll Visualizer** for MIDI playback in `chip-player-js`.

- **Milestone M1**: The Piano Roll acts as a "drop-in" replacement for the Spectrogram visualizer, occupying the same screen area in the right sidebar (width: 448px, height: 800px) and scrolling vertically downwards with the playhead at the top (matching the Spectrogram's waterfall behavior). Notes are colored by MIDI channel. All variable behaviors (orientation, direction, zoom, animation behavior, note range, etc.) are configured via easily editable constants.
- **Milestone M2**: Support alternative placements (e.g. main content area) while keeping the Settings panel visible for muting MIDI channels. Muted channels are dimmed in the piano roll.

Key architectural goal: **Total decoupling from the MIDI sequencer and synthesizer engines**. The visualizer will ingest the raw MIDI file buffer and synchronize solely via a playback position query (`getCurrentPositionMs()`), ensuring 100% compatibility with both the current synth architecture and the upcoming SpessaSynth migration on the `spessa` branch.

---

## User Review & Decisions Incorporated

> [!IMPORTANT]
> **Branch Strategy**: Work will be performed on a dedicated branch created from `master`: `feature/piano-roll`.

> [!NOTE]
> **Decisions Applied**:
> 1. **Coloring by Channel**: Notes in M1 will be colored by MIDI channel (channels 0–15) using a 16-color palette.
> 2. **Default & Toggle**: When a MIDI file loads, the visualizer defaults to Piano Roll, with an option in the visualizer controls to toggle back to Spectrogram.
> 3. **Waterfall with Top Playhead**: To match the Spectrogram's behavior, the playhead line is positioned at the top of the piano roll, and notes scroll downwards as time advances.

---

## Architecture & Data Flow

```mermaid
flowchart TD
    subgraph Sequencer & Storage
        Seq[Sequencer.js]
        Buf[(Raw MIDI ArrayBuffer)]
        Seq -->|loads| Buf
    end

    subgraph Playback Engines (Decoupled)
        M1[MIDIPlayer.js - FluidLite/ADLMIDI]
        M2[MIDIPlayer2.js - SpessaSynth]
        Seq -->|controls| M1
        Seq -.->|or controls| M2
    end

    subgraph Decoupled Piano Roll Component
        Buf -->|ArrayBuffer| Parser[midi-parser.js]
        Parser -->|Parsed Notes & Channels| Engine[PianoRollEngine.js]
        Seq -->|getCurrentPositionMs()| Engine
        Seq -->|voiceMask / muting| Engine
        Engine -->|2D Canvas Draw| Canvas[Canvas Element]
        Engine -->|Channel Meta| Overlay[ChannelTrackList.jsx]
    end
```

### Decoupling Guarantees
- The `PianoRoll` component does not import or know about `chipCore`, `audioCtx`, `sourceNode`, `MIDIFilePlayer`, `tinyplayer.c`, or `spessasynth`.
- It requires only:
  1. `midiData`: raw `ArrayBuffer` or `Uint8Array`.
  2. `getCurrentPositionMs`: `() => number` returning playback time in milliseconds.
  3. `paused`: boolean.
  4. `voiceMask`: array of booleans indicating channel mute states.
- Compatible immediately with both `master` and the `spessa` branch without modification.

---

## Proposed Changes

### Configuration Module

#### [NEW] [config.js](file:///Users/montag/src/chip-player-js/src/piano-roll/config.js)
Contains all variable behaviors and styling constants in one easy-to-edit file:

```javascript
export const PIANO_ROLL_CONFIG = {
  // Animation behavior: 'continuous' (smooth scroll past stationary playhead)
  //                  or 'paginated' (playhead moves across page, then flips)
  ANIMATION_BEHAVIOR: 'continuous',

  // Orientation: 'vertical' (time on Y, pitch on X)
  //           or 'horizontal' (time on X, pitch on Y)
  ORIENTATION: 'vertical',

  // Direction:
  // For vertical: 'top-to-bottom' (waterfall/falling) or 'bottom-to-top'
  // For horizontal: 'left-to-right' or 'right-to-left'
  DIRECTION: 'top-to-bottom',

  // Sync position of playhead:
  // 'top', 'center', 'bottom' (or 'left', 'center', 'right' in horizontal)
  // Set to 'top' to match Spectrogram waterfall behavior
  SYNC_POSITION: 'top',

  // Time Zoom:
  // 'fixed' (uses PIXELS_PER_SECOND) or 'fit-song' (entire song duration fits viewport)
  TIME_ZOOM_MODE: 'fixed',
  PIXELS_PER_SECOND: 100, // 100px per second = 0.1 px/ms (independent of tempo/PPQ)

  // Pitch Zoom:
  // 'fill' (pitches scale to fill canvas dimension)
  // 'fixed' (uses PIXELS_PER_NOTE, e.g. 5px * 88 notes = 440px)
  PITCH_ZOOM_MODE: 'fixed',
  PIXELS_PER_NOTE: 5,

  // Note Range: 88 (MIDI 21 [A0] to 108 [C8]) or 128 (MIDI 0 to 127)
  NOTE_RANGE: 88,

  // Color notes by: 'channel' (M1 default) or 'track'
  COLOR_BY: 'channel',

  // Background and Lane Colors
  BACKGROUND_COLOR: '#000000',
  BLACK_KEY_LANE_TINT: 'rgba(255, 255, 255, 0.05)', // 5% tint
  WHITE_KEY_LANE_COLOR: '#000000',
  GRID_LINE_COLOR: 'rgba(255, 255, 255, 0.08)',
  PLAYHEAD_COLOR: '#ff3366',
  PLAYHEAD_LINE_WIDTH: 2,

  // 16 Channel Palette (high contrast, distinct hues for channels 0-15)
  CHANNEL_COLORS: [
    '#00d2ff', '#3a7bd5', '#f857a6', '#ff5858',
    '#43e97b', '#38f9d7', '#fa709a', '#fee140',
    '#a18cd1', '#fbc2eb', '#fad0c4', '#ffd1ff',
    '#ff9a9e', '#fecfef', '#a1c4fd', '#c2e9fb'
  ],

  // Overlay Channel/Track List in top-left corner
  SHOW_TRACK_CHANNEL_LIST: true,
  LIST_MODE: 'channel', // 'channel' or 'track'

  // Note Display
  SHOW_NOTE_NAMES: false, // Display note names (e.g. C#4) on notes when space allows
  NOTE_MIN_LENGTH_PX: 3,  // Minimum visible length for very short notes
  NOTE_CORNER_RADIUS: 2,  // Rounded note corners
  ACTIVE_NOTE_GLOW: true, // Brighten / highlight sounding notes
  MUTED_OPACITY: 0.15     // Dimmed opacity for muted channels
};
```

---

### MIDI Parsing Module

#### [NEW] [midi-parser.js](file:///Users/montag/src/chip-player-js/src/piano-roll/midi-parser.js)
Extracts note records and channel metadata from an ArrayBuffer without any dependency on the audio sequencer:
- Parses standard MIDI files (Format 0 and Format 1) using the existing `MIDIFile` / `MIDIEvents` helpers.
- Converts delta times into absolute milliseconds using the MIDI tempo map.
- Pairs Note On and Note Off events into note objects:
  ```javascript
  {
    pitch: number,       // 0-127
    startMs: number,     // absolute start time in ms
    endMs: number,       // absolute end time in ms
    durationMs: number,  // endMs - startMs
    track: number,       // track index
    channel: number,     // channel 0-15
    velocity: number     // 1-127
  }
  ```
- Extracts Channel metadata (Program changes / GM instrument names via `GM_INSTRUMENTS`, channels in use) and Track metadata.
- Notes are kept sorted by `startMs` to allow $O(\log N)$ binary search querying for viewport culling.

---

### Piano Roll Rendering Engine

#### [NEW] [PianoRollEngine.js](file:///Users/montag/src/chip-player-js/src/piano-roll/PianoRollEngine.js)
Core 2D canvas renderer and requestAnimationFrame loop:
- **Waterfall Rendering with Playhead at Top**:
  - `playheadY` is at the top of the canvas (or offset slightly by `PLAYHEAD_TOP_OFFSET_PX` e.g. 2px).
  - Notes currently sounding sit on the playhead line.
  - As time advances, notes scroll downwards (`y = playheadY + (currentTimeMs - note.startMs) * pxPerMs`).
  - Notes that have already ended trail off towards the bottom of the canvas.
- **Viewport Calculation**:
  - Computes `visibleStartMs` and `visibleEndMs` from `currentPositionMs`, zoom level (`pixelsPerSecond`), and playhead position.
  - In `continuous` mode: viewport moves smoothly with `currentPositionMs`.
  - In `paginated` mode: viewport is static for `pageSizeMs = viewportDimension / pixelsPerSecond`, and flips when the playhead exceeds the page boundary.
- **Coordinate Mapping**:
  - Maps `(pitch, timeMs)` to `(x, y)` on canvas based on:
    - `ORIENTATION`: `'vertical'` vs `'horizontal'`
    - `DIRECTION`: `'top-to-bottom'` vs `'bottom-to-top'`
    - `PITCH_ZOOM_MODE`: `'fill'` vs `'fixed'`
- **High-Performance Culling**:
  - Uses binary search to find notes intersecting `[visibleStartMs, visibleEndMs]`.
  - Only visible notes (~20–200 notes per frame) are processed and drawn.
- **Visuals**:
  - Draws background lanes (with 5% tint on black keys C#, D#, F#, G#, A#).
  - Draws subtle octave separators (C lanes).
  - Draws notes colored by channel (`CHANNEL_COLORS[channel % 16]`).
  - Dims muted/hidden channels.
  - Highlights actively sounding notes at the playhead line.
  - Optionally renders note names (e.g. "C4", "G#5") if box dimensions exceed minimum text threshold.
  - Draws the playhead line at `SYNC_POSITION`.

---

### React Component & UI Overlay

#### [NEW] [PianoRollVisualizer.js](file:///Users/montag/src/chip-player-js/src/piano-roll/PianoRollVisualizer.js)
React component wrapping the engine:
- Manages the canvas element and lifecycle (`componentDidMount`, `componentDidUpdate`, `componentWillUnmount`).
- Renders the **Channel List overlay** in the top-left corner:
  - Displays color pill, channel name / instrument name, and visibility icon.
  - Click toggles channel visibility in the visualizer.
  - Option+Click (or Alt+Click) solos the channel (toggles all other channels).
- Responsive to width and height props (default: 448px width, 800px height for M1 drop-in replacement).

---

### Integration into chip-player-js

#### [MODIFY] [Sequencer.js](file:///Users/montag/src/chip-player-js/src/Sequencer.js)
- Retain the current song buffer in `this.currSongBuffer = buffer` in `playSongBuffer()`.
- Provide getter: `getCurrSongBuffer(): ArrayBuffer`.
- Include `songBuffer` in `sequencerStateUpdate` events.

#### [MODIFY] [App.js](file:///Users/montag/src/chip-player-js/src/components/App.js)
- Pass `sequencer={this.sequencer}`, `voiceMask={this.state.voiceMask}`, and `currentSongBuffer={this.state.currentSongBuffer}` to `<Visualizer />`.

#### [MODIFY] [Visualizer.js](file:///Users/montag/src/chip-player-js/src/components/Visualizer.js)
- Detect if the current song is a MIDI file (`/\.(mid|midi|smf)$/i.test(songPath)`).
- When a MIDI file is active:
  - Render `<PianoRollVisualizer>` in place of the Spectrogram canvas by default, with matching dimensions (`width={VIS_WIDTH}`, `height={800}`).
  - Add visualizer mode option (Piano Roll / Spectrogram) to let users toggle if desired.

---

## Milestone M2 Plan (Main Content Area & Channel Muting)

1. **Main Area Placement**:
   - Provide an expand / placement toggle button to mount `<PianoRollVisualizer>` in `App-main-content-area` with responsive width (`100%`) and height.
   - Keep the `Settings` panel open side-by-side or docked.
2. **Channel Muting Integration**:
   - The user mutes channels via the existing `Settings` panel voice mask.
   - `App.js` passes `voiceMask` down to `PianoRollVisualizer`.
   - The Piano Roll dims muted channels in real time (e.g. at 15% opacity or grayscale).

---

## Verification Plan

### Automated Tests / Linting
- Run ESLint to ensure no syntax or lint errors:
  ```bash
  npx eslint src/piano-roll/ src/components/Visualizer.js
  ```
- Run unit test parsing check with Node on sample MIDI data to verify note start/end times and metadata extraction:
  ```bash
  node -e "const { parseMidiNotes } = require('./src/piano-roll/midi-parser'); console.log('Parser loaded successfully');"
  ```

### Manual Verification
1. **Drop-in Replacement (Milestone M1)**:
   - Start player (`npm start` or dev server).
   - Play a non-MIDI song (e.g. SID or NSF) -> verify Spectrogram functions normally.
   - Play a MIDI file -> verify Piano Roll automatically appears in the visualizer panel with playhead at top and notes cascading downwards.
2. **Visual Verification**:
   - Verify black background and 5% tint on black key lanes (C#, D#, F#, G#, A#).
   - Verify notes scroll smoothly down vertically towards the bottom from the top playhead.
   - Verify notes match playback timing in sync with audio.
   - Verify each channel (0-15) has a distinct color.
3. **Interactive Channel Overlay**:
   - Verify top-left overlay lists channels with instrument names.
   - Click an item -> verify notes of that channel hide/show.
   - Option+Click an item -> verify solo behavior (all other channels hide).
4. **Config Constants Testing**:
   - Change `ORIENTATION` to `'horizontal'` -> verify time runs horizontally.
   - Change `ANIMATION_BEHAVIOR` to `'paginated'` -> verify page-flipping behavior.
   - Change `TIME_ZOOM_MODE` to `'fit-song'` -> verify entire song spans the viewport.
   - Change `NOTE_RANGE` from `88` to `128` -> verify full 128-note range.
   - Toggle `SHOW_NOTE_NAMES` to `true` -> verify note names (C4, F#3, etc.) appear on notes.
