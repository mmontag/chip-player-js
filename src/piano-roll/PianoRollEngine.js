import { PIANO_ROLL_CONFIG, getDecayIntensity } from './config.js';
import { findFirstVisibleNoteIndex, getNoteName, getNotePitchOffsetAt, isBlackKey } from './midi-parser.js';

export default class PianoRollEngine {
  constructor(canvas, options = {}) {
    this.canvas = canvas;
    this.ctx = canvas.getContext('2d', { alpha: false });
    this.config = { ...PIANO_ROLL_CONFIG, ...options };

    this.notes = [];
    this.channels = [];
    this.tracks = [];
    this.durationMs = 0;
    this.maxNoteDurationMs = 30000;

    this.getCurrentPositionMs = options.getCurrentPositionMs || (() => 0);
    this.getPlaybackRate = options.getPlaybackRate || (() => 1.0);
    this.getAudioLatencyMsCallback = options.getAudioLatencyMs;
    this.isPaused = options.isPaused !== undefined ? options.isPaused : true;
    this.hiddenChannels = new Set();
    this.hiddenTracks = new Set();
    this.voiceMask = null; // array of booleans if controlled externally by Settings

    this.animFrameId = null;
    this.lastFrameTime = 0;
    this.lastRawPos = -1;
    this.smoothPos = 0;
    this.isDirty = true;

    this.keyboardCanvas = null;
    this.keyboardGeometry = null;
    this.keyboardCacheKey = '';

    this.onFrame = this.onFrame.bind(this);
    this.render = this.render.bind(this);

    if (!this.isPaused) {
      this.start();
    }
  }

  setMidiData(parsedMidi) {
    if (!parsedMidi) {
      this.notes = [];
      this.channels = [];
      this.tracks = [];
      this.durationMs = 0;
      this.maxNoteDurationMs = 30000;
    } else {
      this.notes = parsedMidi.notes || [];
      this.channels = parsedMidi.channels || [];
      this.tracks = parsedMidi.tracks || [];
      this.durationMs = parsedMidi.durationMs || 0;
      this.maxNoteDurationMs = parsedMidi.maxNoteDurationMs || 30000;
    }
    this.lastFrameTime = 0;
    this.lastRawPos = -1;
    this.smoothPos = 0;
    this.isDirty = true;
    this.render();
    if (!this.isPaused) {
      this.start();
    }
  }

  setPaused(paused) {
    this.isPaused = paused;
    if (!paused) {
      this.start();
    } else {
      this.stop();
      this.isDirty = true;
      this.render();
    }
  }

  setVoiceMask(voiceMask) {
    this.voiceMask = voiceMask;
    this.isDirty = true;
    this.render();
  }

  setHiddenChannels(hiddenSet) {
    this.hiddenChannels = new Set(hiddenSet);
    this.isDirty = true;
    this.render();
  }

  setHiddenTracks(hiddenSet) {
    this.hiddenTracks = new Set(hiddenSet);
    this.isDirty = true;
    this.render();
  }

  updateConfig(newConfig) {
    this.config = { ...this.config, ...newConfig };
    this.keyboardCacheKey = '';
    this.isDirty = true;
    this.render();
  }

  resize(width, height) {
    if (this.canvas.width !== width || this.canvas.height !== height) {
      this.canvas.width = width;
      this.canvas.height = height;
      this.keyboardCacheKey = '';
      this.isDirty = true;
      this.render();
    }
  }

  start() {
    if (this.animFrameId === null) {
      this.lastFrameTime = 0;
      this.animFrameId = requestAnimationFrame(this.onFrame);
    }
  }

  stop() {
    if (this.animFrameId !== null) {
      cancelAnimationFrame(this.animFrameId);
      this.animFrameId = null;
    }
    this.lastFrameTime = 0;
  }

  destroy() {
    this.stop();
    this.keyboardCanvas = null;
    this.keyboardGeometry = null;
    this.keyboardCacheKey = '';
  }

  onFrame(timestamp) {
    if (!this.isPaused) {
      this.animFrameId = requestAnimationFrame(this.onFrame);
      this.render(timestamp);
    } else {
      this.animFrameId = null;
    }
  }

  getAudioLatencyMs() {
    if (typeof this.getAudioLatencyMsCallback === 'function') {
      const ms = this.getAudioLatencyMsCallback();
      if (typeof ms === 'number' && !isNaN(ms)) return ms;
    }
    return 0;
  }

  getSmoothPositionMs(timestamp) {
    const rawPos = Math.max(0, this.getCurrentPositionMs() || 0);
    const now = (typeof timestamp === 'number' && timestamp > 0) ? timestamp : performance.now();
    const speed = (this.getPlaybackRate ? this.getPlaybackRate() : 1.0) || 1.0;
    const latencyOffset = this.getAudioLatencyMs();

    if (this.isPaused) {
      this.smoothPos = rawPos;
      this.lastFrameTime = now;
      this.lastRawPos = rawPos;
      return Math.max(0, rawPos - latencyOffset);
    }

    if (this.lastFrameTime === 0) {
      this.smoothPos = rawPos;
      this.lastFrameTime = now;
      this.lastRawPos = rawPos;
      return Math.max(0, rawPos - latencyOffset);
    }

    const dt = Math.max(0, Math.min(now - this.lastFrameTime, 100));
    this.lastFrameTime = now;

    // Advance smooth position by elapsed frame time scaled by playback speed
    this.smoothPos += dt * speed;

    // Check for seek, rewind, or loop
    const discrepancy = rawPos - this.smoothPos;
    if (rawPos < this.lastRawPos || Math.abs(discrepancy) > 150) {
      this.smoothPos = rawPos;
    } else if (rawPos !== this.lastRawPos) {
      // Audio buffer updated: gently correct any drift (15% per buffer update)
      this.smoothPos += discrepancy * 0.15;
    }

    this.lastRawPos = rawPos;
    return Math.max(0, this.smoothPos - latencyOffset);
  }

  getChannelColor(channel) {
    const colors = this.config.CHANNEL_COLORS;
    return colors[channel % colors.length];
  }

  getTrackColor(track) {
    const colors = this.config.TRACK_COLORS;
    return colors[track % colors.length];
  }

  drawBend(ctx, bends, startMs, endMs, timeStart, timeEnd, rootPitch, lineWidth, isVertical, minPitch, maxPitch, laneSize, pitchOffset) {
    const N = bends.length;
    if (N <= 1) return;
    const duration = Math.max(1, endMs - startMs);

    ctx.lineWidth = lineWidth;
    ctx.lineCap = (this.config && this.config.NOTE_CORNER_RADIUS === 0) ? 'butt' : 'round';
    ctx.lineJoin = 'round';
    ctx.beginPath();
    for (let i = 0; i < N; i++) {
      const b = bends[i];
      const ratio = Math.max(0, Math.min(1, (b.timeMs - startMs) / duration));
      const timeCoord = timeStart + ratio * (timeEnd - timeStart);
      const p = Math.max(minPitch, Math.min(maxPitch, rootPitch + b.semitoneOffset));
      const pitchCoord = isVertical
        ? pitchOffset + (p - minPitch) * laneSize + laneSize / 2
        : pitchOffset + (maxPitch - p) * laneSize + laneSize / 2;

      const x = isVertical ? pitchCoord : timeCoord;
      const y = isVertical ? timeCoord : pitchCoord;
      if (i === 0) {
        ctx.moveTo(x, y);
      } else {
        ctx.lineTo(x, y);
      }
    }
    ctx.stroke();
  }

  ensureKeyboardCache(width, height, isVertical, pitchOffset, laneSize, totalPitchDimension, minPitch, maxPitch) {
    const { config } = this;
    const aspectRatio = config.KEYBOARD_ASPECT_RATIO || 0.125;
    const keyboardSize = Math.max(1, Math.round(totalPitchDimension * aspectRatio));
    const blackKeyHeightRatio = config.KEYBOARD_BLACK_KEY_HEIGHT_RATIO || 0.6;
    const blackKeyLength = Math.max(1, Math.round(keyboardSize * blackKeyHeightRatio));

    const cacheKey = `${width}_${height}_${isVertical}_${keyboardSize}_${pitchOffset}_${laneSize}_${totalPitchDimension}_${minPitch}_${maxPitch}_${blackKeyLength}_${config.KEYBOARD_WHITE_KEY_COLOR}_${config.KEYBOARD_BLACK_KEY_COLOR}_${config.KEYBOARD_STROKE_COLOR}_${config.KEYBOARD_BLACK_KEY_STROKE_COLOR}`;

    if (this.keyboardCacheKey === cacheKey && this.keyboardCanvas && this.keyboardGeometry) {
      return this.keyboardGeometry;
    }

    const whitePitches = [];
    const blackKeys = {};
    const whiteKeys = {};
    let whiteDividers = [];

    if (!this.keyboardCanvas) {
      this.keyboardCanvas = document.createElement('canvas');
    }

    if (isVertical) {
      // 1. Vertical mode: pitch along X axis (left to right, A0 to C8)
      const bkThickness = Math.max(3, Math.round(laneSize * 1.1));
      for (let p = minPitch; p <= maxPitch; p++) {
        if (isBlackKey(p)) {
          const laneCenter = pitchOffset + (p - minPitch) * laneSize + laneSize / 2;
          const bkX = Math.floor(laneCenter - bkThickness / 2);
          blackKeys[p] = {
            pitch: p,
            x: bkX,
            w: bkThickness,
            h: blackKeyLength,
          };
        } else {
          whitePitches.push(p);
        }
      }

      const numWhite = whitePitches.length;
      whiteDividers = [];
      for (let i = 0; i <= numWhite; i++) {
        whiteDividers.push(Math.round(pitchOffset + (i * totalPitchDimension) / numWhite));
      }

      for (let i = 0; i < numWhite; i++) {
        const p = whitePitches[i];
        const xStart = whiteDividers[i];
        const xEnd = whiteDividers[i + 1];
        whiteKeys[p] = {
          pitch: p,
          x: xStart,
          w: Math.max(1, xEnd - xStart),
        };
      }

      this.keyboardCanvas.width = width;
      this.keyboardCanvas.height = keyboardSize;
      const kCtx = this.keyboardCanvas.getContext('2d', { alpha: false });

      kCtx.fillStyle = config.BACKGROUND_COLOR;
      kCtx.fillRect(0, 0, width, keyboardSize);

      // Step 1: Base paint white rect
      kCtx.fillStyle = config.KEYBOARD_WHITE_KEY_COLOR || '#ffffff';
      kCtx.fillRect(pitchOffset, 0, totalPitchDimension, keyboardSize);

      // Step 2: Full-height vertical divider lines
      kCtx.fillStyle = config.KEYBOARD_STROKE_COLOR || '#444444';
      for (let i = 1; i < numWhite; i++) {
        const strokeX = whiteDividers[i] - 1;
        kCtx.fillRect(strokeX, 0, 1, keyboardSize);
      }
      kCtx.fillRect(Math.max(0, pitchOffset), 0, 1, keyboardSize);
      kCtx.fillRect(pitchOffset + totalPitchDimension - 1, 0, 1, keyboardSize);
      kCtx.fillRect(pitchOffset, keyboardSize - 1, totalPitchDimension, 1);

      // Step 3: Black keys on top
      kCtx.fillStyle = config.KEYBOARD_BLACK_KEY_COLOR || '#111111';
      for (const p in blackKeys) {
        const bk = blackKeys[p];
        kCtx.fillRect(bk.x, 0, bk.w, bk.h);
      }

      kCtx.strokeStyle = config.KEYBOARD_BLACK_KEY_STROKE_COLOR || '#000000';
      kCtx.lineWidth = 1;
      for (const p in blackKeys) {
        const bk = blackKeys[p];
        kCtx.strokeRect(bk.x + 0.5, 0.5, bk.w - 1, bk.h - 1);
      }
    } else {
      // 2. Horizontal mode (rotated 90 deg CCW): pitch along Y axis (top to bottom, C8 at top, A0 at bottom)
      const bkThickness = Math.max(3, Math.round(laneSize * 1.1));
      for (let p = maxPitch; p >= minPitch; p--) {
        if (isBlackKey(p)) {
          const laneCenter = pitchOffset + (maxPitch - p) * laneSize + laneSize / 2;
          const bkY = Math.floor(laneCenter - bkThickness / 2);
          blackKeys[p] = {
            pitch: p,
            y: bkY,
            w: blackKeyLength,
            h: bkThickness,
          };
        } else {
          whitePitches.push(p);
        }
      }

      const numWhite = whitePitches.length;
      whiteDividers = [];
      for (let i = 0; i <= numWhite; i++) {
        whiteDividers.push(Math.round(pitchOffset + (i * totalPitchDimension) / numWhite));
      }

      for (let i = 0; i < numWhite; i++) {
        const p = whitePitches[i];
        const yStart = whiteDividers[i];
        const yEnd = whiteDividers[i + 1];
        whiteKeys[p] = {
          pitch: p,
          y: yStart,
          h: Math.max(1, yEnd - yStart),
        };
      }

      this.keyboardCanvas.width = keyboardSize;
      this.keyboardCanvas.height = height;
      const kCtx = this.keyboardCanvas.getContext('2d', { alpha: false });

      kCtx.fillStyle = config.BACKGROUND_COLOR;
      kCtx.fillRect(0, 0, keyboardSize, height);

      // Step 1: Base paint white rect
      kCtx.fillStyle = config.KEYBOARD_WHITE_KEY_COLOR || '#ffffff';
      kCtx.fillRect(0, pitchOffset, keyboardSize, totalPitchDimension);

      // Step 2: Full-width horizontal divider lines
      kCtx.fillStyle = config.KEYBOARD_STROKE_COLOR || '#444444';
      for (let i = 1; i < numWhite; i++) {
        const strokeY = whiteDividers[i] - 1;
        kCtx.fillRect(0, strokeY, keyboardSize, 1);
      }
      kCtx.fillRect(0, Math.max(0, pitchOffset), keyboardSize, 1);
      kCtx.fillRect(0, pitchOffset + totalPitchDimension - 1, keyboardSize, 1);
      kCtx.fillRect(0, pitchOffset, 1, totalPitchDimension);

      // Step 3: Black keys on top (attached to right edge of keyboardCanvas)
      kCtx.fillStyle = config.KEYBOARD_BLACK_KEY_COLOR || '#111111';
      for (const p in blackKeys) {
        const bk = blackKeys[p];
        kCtx.fillRect(keyboardSize - bk.w, bk.y, bk.w, bk.h);
      }

      kCtx.strokeStyle = config.KEYBOARD_BLACK_KEY_STROKE_COLOR || '#000000';
      kCtx.lineWidth = 1;
      for (const p in blackKeys) {
        const bk = blackKeys[p];
        kCtx.strokeRect(keyboardSize - bk.w + 0.5, bk.y + 0.5, bk.w - 1, bk.h - 1);
      }
    }

    this.keyboardCacheKey = cacheKey;
    this.keyboardGeometry = {
      keyboardSize,
      blackKeyLength,
      whiteKeys,
      blackKeys,
      whiteDividers,
    };

    return this.keyboardGeometry;
  }

  render(timestamp) {
    const { canvas, ctx, config } = this;
    const width = canvas.width;
    const height = canvas.height;
    if (width === 0 || height === 0) return;

    const currentTimeMs = this.getSmoothPositionMs(timestamp);

    const isVertical = config.ORIENTATION === 'vertical';
    const isForward = config.DIRECTION !== 'reverse' && config.DIRECTION !== 'bottom-to-top' && config.DIRECTION !== 'left-to-right';
    const isContinuous = config.ANIMATION_BEHAVIOR === 'continuous';

    // Pitch parameters
    const noteRange = config.NOTE_RANGE === 128 ? 128 : 88;
    const minPitch = noteRange === 128 ? 0 : 21; // 21 is A0
    const maxPitch = noteRange === 128 ? 127 : 108; // 108 is C8
    const pitchCount = maxPitch - minPitch + 1;

    let laneSize = 0;
    let pitchOffset = 0;
    let totalPitchDimension = isVertical ? width : height;
    if (isVertical) {
      if (config.PITCH_ZOOM_MODE === 'fill') {
        laneSize = width / pitchCount;
        totalPitchDimension = width;
      } else {
        laneSize = config.PIXELS_PER_NOTE;
        totalPitchDimension = pitchCount * laneSize;
        pitchOffset = Math.max(0, Math.floor((width - totalPitchDimension) / 2));
      }
    } else {
      if (config.PITCH_ZOOM_MODE === 'fill') {
        laneSize = height / pitchCount;
        totalPitchDimension = height;
      } else {
        laneSize = config.PIXELS_PER_NOTE;
        totalPitchDimension = pitchCount * laneSize;
        pitchOffset = Math.max(0, Math.floor((height - totalPitchDimension) / 2));
      }
    }

    const laneWidth = laneSize;
    const xOffset = pitchOffset;
    const laneHeight = laneSize;
    const yOffset = pitchOffset;

    const isKeyboardVisible = config.SHOW_KEYBOARD !== false;
    const keyboardSize = isKeyboardVisible
      ? Math.max(1, Math.round(totalPitchDimension * (config.KEYBOARD_ASPECT_RATIO || 0.125)))
      : 0;

    // Time scaling (pixels per millisecond)
    let pxPerMs;
    if (config.TIME_ZOOM_MODE === 'fit-song' && this.durationMs > 0) {
      const timeDimension = isVertical ? height : width;
      pxPerMs = timeDimension / this.durationMs;
    } else {
      pxPerMs = config.PIXELS_PER_SECOND / 1000.0;
    }

    // Playhead calculation
    const timeDimension = isVertical ? height : width;
    const lineWidth = Math.max(1, Math.round(config.PLAYHEAD_LINE_WIDTH || 1));
    const offset = typeof config.PLAYHEAD_OFFSET_PX === 'number' ? config.PLAYHEAD_OFFSET_PX : 2;

    // Resolve sync position target: 'start', 'middle', or 'end'
    // Legacy support: 'top', 'bottom', 'left', 'right', 'center'
    let syncMode = config.SYNC_POSITION;
    if (syncMode === 'center') syncMode = 'middle';
    else if (syncMode === 'bottom') syncMode = isVertical ? (isForward ? 'end' : 'start') : 'end';
    else if (syncMode === 'top') syncMode = isVertical ? (isForward ? 'start' : 'end') : 'start';
    else if (syncMode === 'left') syncMode = !isVertical ? (isForward ? 'end' : 'start') : 'end';
    else if (syncMode === 'right') syncMode = !isVertical ? (isForward ? 'start' : 'end') : 'start';

    // Start position coordinate (where notes originate)
    let coordStart;
    // End position coordinate (destination where notes strike the playhead/keyboard)
    let coordEnd;

    if (isVertical) {
      if (isForward) {
        // Forward vertical: notes flow top -> bottom
        coordStart = offset;
        coordEnd = height - offset - lineWidth - (isKeyboardVisible ? keyboardSize : 0);
      } else {
        // Reverse vertical: notes flow bottom -> top
        coordStart = height - offset - lineWidth;
        coordEnd = offset + (isKeyboardVisible ? keyboardSize : 0);
      }
    } else {
      if (isForward) {
        // Forward horizontal: notes flow right -> left
        coordStart = width - offset - lineWidth;
        coordEnd = offset + (isKeyboardVisible ? keyboardSize : 0);
      } else {
        // Reverse horizontal: notes flow left -> right
        coordStart = offset;
        coordEnd = width - offset - lineWidth - (isKeyboardVisible ? keyboardSize : 0);
      }
    }

    let playheadCoord;
    if (syncMode === 'end') {
      playheadCoord = coordEnd;
    } else if (syncMode === 'middle') {
      playheadCoord = Math.floor(timeDimension / 2);
    } else if (syncMode === 'start') {
      playheadCoord = coordStart;
    } else if (typeof syncMode === 'number') {
      // Interpolate between start (0.0) and end (1.0)
      playheadCoord = Math.round(coordStart + syncMode * (coordEnd - coordStart));
    } else {
      playheadCoord = coordEnd;
    }

    // Page-based time for paginated animation behavior
    const pageDurationMs = timeDimension / pxPerMs;
    const pageIndex = Math.floor(currentTimeMs / pageDurationMs);
    const pageStartMs = pageIndex * pageDurationMs;

    // Clear background
    ctx.fillStyle = config.BACKGROUND_COLOR;
    ctx.fillRect(0, 0, width, height);

    // 1. Draw pitch lanes (background grid)
    if (isVertical) {
      ctx.fillStyle = config.BLACK_KEY_LANE_TINT;
      for (let p = minPitch; p <= maxPitch; p++) {
        const laneX = Math.round(pitchOffset + (p - minPitch) * laneSize);
        if (isBlackKey(p)) {
          const nextLaneX = Math.round(pitchOffset + (p + 1 - minPitch) * laneSize);
          ctx.fillRect(laneX, 0, nextLaneX - laneX, height);
        }
      }

      ctx.fillStyle = config.OCTAVE_LINE_COLOR;
      for (let p = minPitch; p <= maxPitch; p++) {
        if (p % 12 === 0) {
          const laneX = Math.round(pitchOffset + (p - minPitch) * laneSize);
          ctx.fillRect(laneX, 0, 1, height);
        }
      }
    } else {
      ctx.fillStyle = config.BLACK_KEY_LANE_TINT;
      for (let p = minPitch; p <= maxPitch; p++) {
        const laneY = Math.round(pitchOffset + (maxPitch - p) * laneSize);
        if (isBlackKey(p)) {
          const nextLaneY = Math.round(pitchOffset + (maxPitch - p + 1) * laneSize);
          ctx.fillRect(0, laneY, width, nextLaneY - laneY);
        }
      }

      ctx.fillStyle = config.OCTAVE_LINE_COLOR;
      for (let p = minPitch; p <= maxPitch; p++) {
        if (p % 12 === 0) {
          const laneY = Math.round(pitchOffset + (maxPitch - p) * laneSize);
          ctx.fillRect(0, laneY, width, 1);
        }
      }
    }

    // 2. Visible time window calculation
    let minVisibleMs;
    let maxVisibleMs;

    if (isContinuous) {
      if (isVertical) {
        if (isForward) {
          // Top to bottom: notes cascade downward towards playhead
          minVisibleMs = currentTimeMs - (height - playheadCoord) / pxPerMs;
          maxVisibleMs = currentTimeMs + playheadCoord / pxPerMs;
        } else {
          // Bottom to top: notes scroll upward
          minVisibleMs = currentTimeMs - playheadCoord / pxPerMs;
          maxVisibleMs = currentTimeMs + (height - playheadCoord) / pxPerMs;
        }
      } else {
        // Horizontal
        if (isForward) {
          // Right to left: notes flow leftward towards playhead
          minVisibleMs = currentTimeMs - playheadCoord / pxPerMs;
          maxVisibleMs = currentTimeMs + (width - playheadCoord) / pxPerMs;
        } else {
          // Left to right
          minVisibleMs = currentTimeMs - (width - playheadCoord) / pxPerMs;
          maxVisibleMs = currentTimeMs + playheadCoord / pxPerMs;
        }
      }
    } else {
      // Paginated
      minVisibleMs = pageStartMs;
      maxVisibleMs = pageStartMs + pageDurationMs;
    }

    // 3. Draw notes
    const activeKeys = new Map();
    const notes = this.notes;
    if (notes.length > 0) {
      const startIndex = findFirstVisibleNoteIndex(notes, minVisibleMs, this.maxNoteDurationMs);
      const minLengthPx = config.NOTE_MIN_LENGTH_PX;
      const cornerRadius = config.NOTE_CORNER_RADIUS;
      const gap = config.NOTE_GAP_PX;
      const sustainOpacity = config.SUSTAIN_OPACITY !== undefined ? config.SUSTAIN_OPACITY : 0.5;

      // Time to canvas coordinate mapping
      const getTimeCoord = (timeMs) => {
        if (isContinuous) {
          if (isVertical) {
            return isForward
              ? playheadCoord + (currentTimeMs - timeMs) * pxPerMs
              : playheadCoord - (currentTimeMs - timeMs) * pxPerMs;
          } else {
            return isForward
              ? playheadCoord - (currentTimeMs - timeMs) * pxPerMs
              : playheadCoord + (currentTimeMs - timeMs) * pxPerMs;
          }
        } else {
          // Paginated
          if (isVertical) {
            return isForward
              ? (timeMs - pageStartMs) * pxPerMs
              : height - (timeMs - pageStartMs) * pxPerMs;
          } else {
            return isForward
              ? width - (timeMs - pageStartMs) * pxPerMs
              : (timeMs - pageStartMs) * pxPerMs;
          }
        }
      };

      const isTimeDecreasingCoord = getTimeCoord(1000) < getTimeCoord(0);

      const drawRect = (x, y, w, h, radii) => {
        if (typeof ctx.roundRect === 'function' && radii) {
          ctx.beginPath();
          ctx.roundRect(x, y, w, h, radii);
          ctx.fill();
        } else {
          ctx.fillRect(x, y, w, h);
        }
      };

      const glowOpacity = typeof config.ACTIVE_NOTE_GLOW_OPACITY === 'number'
        ? config.ACTIVE_NOTE_GLOW_OPACITY
        : (typeof config.ACTIVE_NOTE_GLOW === 'number' ? config.ACTIVE_NOTE_GLOW : 0.4);
      const isGlowEnabled = config.ACTIVE_NOTE_GLOW !== false && glowOpacity > 0;
      const fattenPx = Math.max(0, config.ACTIVE_NOTE_FATTEN || 0);
      const decayMs = typeof config.ACTIVE_NOTE_DECAY_MS === 'number' ? config.ACTIVE_NOTE_DECAY_MS : 250;
      const decayEasing = config.ACTIVE_NOTE_EASING || 'linear';

      for (let i = startIndex; i < notes.length; i++) {
        const note = notes[i];
        if (note.startMs > maxVisibleMs) break;

        const hasSustain = note.sustainEndMs && note.sustainEndMs > note.endMs;
        const noteEndMs = hasSustain ? note.sustainEndMs : note.endMs;
        if (noteEndMs < minVisibleMs) continue;

        // Check if track or channel is hidden
        if (this.hiddenChannels.has(note.channel) || this.hiddenTracks.has(note.track)) {
          continue;
        }

        // Check if voice mask mutes this channel
        let isMuted = false;
        if (this.voiceMask && this.voiceMask[note.channel] === false) {
          isMuted = true;
        }

        let keyHasGlow = false;
        let sustainHasGlow = false;
        let currentGlowOpacity = 0;
        let fatten = 0;

        if (!isMuted) {
          if (decayMs > 0) {
            const elapsedMs = currentTimeMs - note.startMs;
            if (elapsedMs >= 0 && elapsedMs < decayMs) {
              const intensity = getDecayIntensity(elapsedMs / decayMs, decayEasing);
              fatten = fattenPx * intensity;
              currentGlowOpacity = glowOpacity * intensity;
              const hasGlow = isGlowEnabled && currentGlowOpacity > 0.001;
              keyHasGlow = hasGlow;
              sustainHasGlow = hasGlow;
            }
          } else {
            const isKeySounding = note.startMs <= currentTimeMs && currentTimeMs <= note.endMs;
            const isSustainSounding = hasSustain && currentTimeMs > note.endMs && currentTimeMs <= note.sustainEndMs;
            if (isKeySounding || isSustainSounding) {
              fatten = fattenPx;
              currentGlowOpacity = glowOpacity;
              keyHasGlow = isKeySounding && isGlowEnabled;
              sustainHasGlow = isSustainSounding && isGlowEnabled;
            }
          }
        }

        const offset = fatten / 2;
        const glowColor = currentGlowOpacity > 0 ? `rgba(255, 255, 255, ${currentGlowOpacity})` : null;

        // Base color
        const baseColor = config.COLOR_BY === 'track'
          ? this.getTrackColor(note.track)
          : this.getChannelColor(note.channel);

        const baseAlpha = isMuted ? config.MUTED_OPACITY : 1.0;

        // Track sounding notes for piano keyboard illumination
        if (!isMuted && isKeyboardVisible && note.startMs <= currentTimeMs && noteEndMs >= currentTimeMs) {
          const isKeySounding = currentTimeMs <= note.endMs;
          const isSustainSounding = hasSustain && currentTimeMs > note.endMs && currentTimeMs <= note.sustainEndMs;
          const useSustain = config.KEYBOARD_SUSTAIN_ILLUMINATION !== false;
          if (isKeySounding || (isSustainSounding && useSustain)) {
            const bendOffset = (config.ENABLE_PITCH_BEND !== false && note.bends && note.bends.length > 1)
              ? getNotePitchOffsetAt(note.bends, currentTimeMs)
              : 0;
            const currentPitch = Math.round(note.pitch + bendOffset);
            if (currentPitch >= minPitch && currentPitch <= maxPitch) {
              const existing = activeKeys.get(currentPitch);
              if (!existing || (!existing.isKey && isKeySounding)) {
                activeKeys.set(currentPitch, {
                  color: baseColor,
                  isKey: isKeySounding,
                  isSustain: !isKeySounding && isSustainSounding,
                });
              }
            }
          }
        }

        const cStart = getTimeCoord(note.startMs);
        const cEnd = getTimeCoord(note.endMs);
        const cSustain = hasSustain ? getTimeCoord(note.sustainEndMs) : cEnd;

        let keyPos, keyDim, sustainPos, sustainDim;
        let keyRadii, sustainRadii;

        if (isTimeDecreasingCoord) {
          // Coordinates decrease as time advances (e.g. top-to-bottom waterfall)
          // cStart is the leading edge (largest coord)
          keyDim = Math.max(minLengthPx, cStart - cEnd);
          keyPos = cStart - keyDim;

          // Sustained segment connects at keyPos and extends to lower coords
          sustainDim = hasSustain ? Math.max(minLengthPx, cEnd - cSustain) : 0;
          sustainPos = keyPos - sustainDim;

          if (isVertical) {
            keyRadii = hasSustain && cornerRadius > 0 ? [0, 0, cornerRadius, cornerRadius] : cornerRadius;
            sustainRadii = cornerRadius > 0 ? [cornerRadius, cornerRadius, 0, 0] : 0;
          } else {
            keyRadii = hasSustain && cornerRadius > 0 ? [0, cornerRadius, cornerRadius, 0] : cornerRadius;
            sustainRadii = cornerRadius > 0 ? [cornerRadius, 0, 0, cornerRadius] : 0;
          }
        } else {
          // Coordinates increase as time advances (e.g. bottom-to-top or left-to-right)
          // cStart is the leading edge (smallest coord)
          keyDim = Math.max(minLengthPx, cEnd - cStart);
          keyPos = cStart;

          // Sustained segment connects at keyPos + keyDim and extends to higher coords
          sustainDim = hasSustain ? Math.max(minLengthPx, cSustain - cEnd) : 0;
          sustainPos = keyPos + keyDim;

          if (isVertical) {
            keyRadii = hasSustain && cornerRadius > 0 ? [cornerRadius, cornerRadius, 0, 0] : cornerRadius;
            sustainRadii = cornerRadius > 0 ? [0, 0, cornerRadius, cornerRadius] : 0;
          } else {
            keyRadii = hasSustain && cornerRadius > 0 ? [cornerRadius, 0, 0, cornerRadius] : cornerRadius;
            sustainRadii = cornerRadius > 0 ? [0, cornerRadius, cornerRadius, 0] : 0;
          }
        }

        const usePitchBend = config.ENABLE_PITCH_BEND !== false;
        const keyHasBends = usePitchBend && note.bends && note.bends.length > 1;
        const sustainHasBends = usePitchBend && note.sustainBends && note.sustainBends.length > 1;

        if (isVertical) {
          const pitch = Math.max(minPitch, Math.min(maxPitch, note.pitch));
          const noteX = xOffset + (pitch - minPitch) * laneWidth + gap / 2;
          const noteW = Math.max(1, laneWidth - gap);
          const drawX = noteX - offset;
          const drawW = noteW + fatten;

          // 1. Draw sustained portion at 50% opacity
          if (hasSustain) {
            ctx.globalAlpha = baseAlpha * sustainOpacity;
            ctx.fillStyle = baseColor;
            ctx.strokeStyle = baseColor;
            const susStartCoord = isTimeDecreasingCoord ? keyPos : keyPos + keyDim;
            const susEndCoord = isTimeDecreasingCoord ? sustainPos : sustainPos + sustainDim;
            if (sustainHasBends) {
              this.drawBend(ctx, note.sustainBends, note.endMs, note.sustainEndMs, susStartCoord, susEndCoord, note.pitch, drawW, true, minPitch, maxPitch, laneWidth, xOffset);
            } else {
              drawRect(drawX, sustainPos, drawW, sustainDim, sustainRadii);
            }

            if (sustainHasGlow) {
              ctx.fillStyle = glowColor;
              ctx.strokeStyle = glowColor;
              if (sustainHasBends) {
                this.drawBend(ctx, note.sustainBends, note.endMs, note.sustainEndMs, susStartCoord, susEndCoord, note.pitch, drawW, true, minPitch, maxPitch, laneWidth, xOffset);
              } else {
                drawRect(drawX, sustainPos, drawW, sustainDim, sustainRadii);
              }
            }
          }

          // 2. Draw key-held portion
          ctx.globalAlpha = baseAlpha;
          ctx.fillStyle = baseColor;
          ctx.strokeStyle = baseColor;
          const keyStartCoord = cStart;
          const keyEndCoord = isTimeDecreasingCoord ? keyPos : keyPos + keyDim;
          if (keyHasBends) {
            this.drawBend(ctx, note.bends, note.startMs, note.endMs, keyStartCoord, keyEndCoord, note.pitch, drawW, true, minPitch, maxPitch, laneWidth, xOffset);
          } else {
            drawRect(drawX, keyPos, drawW, keyDim, keyRadii);
          }

          if (keyHasGlow) {
            ctx.fillStyle = glowColor;
            ctx.strokeStyle = glowColor;
            if (keyHasBends) {
              this.drawBend(ctx, note.bends, note.startMs, note.endMs, keyStartCoord, keyEndCoord, note.pitch, drawW, true, minPitch, maxPitch, laneWidth, xOffset);
            } else {
              drawRect(drawX, keyPos, drawW, keyDim, keyRadii);
            }
          }

          if (config.SHOW_NOTE_NAMES && !isMuted && drawW >= 12 && keyDim >= 10) {
            const bendOffset = keyHasBends ? getNotePitchOffsetAt(note.bends, currentTimeMs) : 0;
            const currentPitch = Math.round(note.pitch + bendOffset);
            const textPitch = Math.max(minPitch, Math.min(maxPitch, note.pitch + bendOffset));
            const textX = xOffset + (textPitch - minPitch) * laneWidth + gap / 2 - offset + drawW / 2;
            ctx.fillStyle = '#ffffff';
            ctx.font = '9px monospace';
            ctx.textAlign = 'center';
            ctx.textBaseline = 'middle';
            ctx.fillText(getNoteName(currentPitch), textX, keyPos + keyDim / 2);
          }
        } else {
          const pitch = Math.max(minPitch, Math.min(maxPitch, note.pitch));
          const noteY = yOffset + (maxPitch - pitch) * laneHeight + gap / 2;
          const noteH = Math.max(1, laneHeight - gap);
          const drawY = noteY - offset;
          const drawH = noteH + fatten;

          // 1. Draw sustained portion at 50% opacity
          if (hasSustain) {
            ctx.globalAlpha = baseAlpha * sustainOpacity;
            ctx.fillStyle = baseColor;
            ctx.strokeStyle = baseColor;
            const susStartCoord = isTimeDecreasingCoord ? keyPos : keyPos + keyDim;
            const susEndCoord = isTimeDecreasingCoord ? sustainPos : sustainPos + sustainDim;
            if (sustainHasBends) {
              this.drawBend(ctx, note.sustainBends, note.endMs, note.sustainEndMs, susStartCoord, susEndCoord, note.pitch, drawH, false, minPitch, maxPitch, laneHeight, yOffset);
            } else {
              drawRect(sustainPos, drawY, sustainDim, drawH, sustainRadii);
            }

            if (sustainHasGlow) {
              ctx.fillStyle = glowColor;
              ctx.strokeStyle = glowColor;
              if (sustainHasBends) {
                this.drawBend(ctx, note.sustainBends, note.endMs, note.sustainEndMs, susStartCoord, susEndCoord, note.pitch, drawH, false, minPitch, maxPitch, laneHeight, yOffset);
              } else {
                drawRect(sustainPos, drawY, sustainDim, drawH, sustainRadii);
              }
            }
          }

          // 2. Draw key-held portion
          ctx.globalAlpha = baseAlpha;
          ctx.fillStyle = baseColor;
          ctx.strokeStyle = baseColor;
          const keyStartCoord = cStart;
          const keyEndCoord = isTimeDecreasingCoord ? keyPos : keyPos + keyDim;
          if (keyHasBends) {
            this.drawBend(ctx, note.bends, note.startMs, note.endMs, keyStartCoord, keyEndCoord, note.pitch, drawH, false, minPitch, maxPitch, laneHeight, yOffset);
          } else {
            drawRect(keyPos, drawY, keyDim, drawH, keyRadii);
          }

          if (keyHasGlow) {
            ctx.fillStyle = glowColor;
            ctx.strokeStyle = glowColor;
            if (keyHasBends) {
              this.drawBend(ctx, note.bends, note.startMs, note.endMs, keyStartCoord, keyEndCoord, note.pitch, drawH, false, minPitch, maxPitch, laneHeight, yOffset);
            } else {
              drawRect(keyPos, drawY, keyDim, drawH, keyRadii);
            }
          }

          if (config.SHOW_NOTE_NAMES && !isMuted && keyDim >= 12 && drawH >= 10) {
            const bendOffset = keyHasBends ? getNotePitchOffsetAt(note.bends, currentTimeMs) : 0;
            const currentPitch = Math.round(note.pitch + bendOffset);
            const textPitch = Math.max(minPitch, Math.min(maxPitch, note.pitch + bendOffset));
            const textY = yOffset + (maxPitch - textPitch) * laneHeight + gap / 2 - offset + drawH / 2;
            ctx.fillStyle = '#ffffff';
            ctx.font = '9px monospace';
            ctx.textAlign = 'center';
            ctx.textBaseline = 'middle';
            ctx.fillText(getNoteName(currentPitch), keyPos + keyDim / 2, textY);
          }
        }
      }
      ctx.globalAlpha = 1.0;
    }

    // 4. Draw piano keyboard and playhead line
    let actualPlayheadCoord = playheadCoord;
    if (!isContinuous) {
      actualPlayheadCoord = (currentTimeMs - pageStartMs) * pxPerMs;
      if (!isForward && isVertical) {
        actualPlayheadCoord = height - actualPlayheadCoord;
      } else if (!isVertical && isForward) {
        actualPlayheadCoord = width - actualPlayheadCoord;
      }
    }

    const lineCoord = Math.floor(actualPlayheadCoord);

    if (isKeyboardVisible) {
      const geo = this.ensureKeyboardCache(width, height, isVertical, pitchOffset, laneSize, totalPitchDimension, minPitch, maxPitch);
      if (geo && this.keyboardCanvas) {
        if (isVertical) {
          const keyboardY = lineCoord + lineWidth;

          // Draw cached base layer
          ctx.drawImage(this.keyboardCanvas, 0, keyboardY);

          // Draw illuminated active keys
          if (activeKeys.size > 0) {
            const blackKeysToRedraw = new Set();

            // 1. Draw active white keys as full-height rectangles
            for (const [pitch, info] of activeKeys) {
              const wk = geo.whiteKeys[pitch];
              if (wk) {
                ctx.globalAlpha = info.isSustain ? (config.SUSTAIN_OPACITY || 0.5) : 1.0;
                ctx.fillStyle = info.color;
                ctx.fillRect(wk.x, keyboardY, Math.max(1, wk.w - 1), keyboardSize);

                // Mark adjacent black keys to be redrawn on top
                if (geo.blackKeys[pitch - 1]) blackKeysToRedraw.add(pitch - 1);
                if (geo.blackKeys[pitch + 1]) blackKeysToRedraw.add(pitch + 1);
              } else if (geo.blackKeys[pitch]) {
                blackKeysToRedraw.add(pitch);
              }
            }

            // 2. Paint black keys on top (active in note color, idle in black)
            for (const bkPitch of blackKeysToRedraw) {
              const bk = geo.blackKeys[bkPitch];
              const activeInfo = activeKeys.get(bkPitch);
              ctx.globalAlpha = (activeInfo && activeInfo.isSustain) ? (config.SUSTAIN_OPACITY || 0.5) : 1.0;
              ctx.fillStyle = activeInfo ? activeInfo.color : (config.KEYBOARD_BLACK_KEY_COLOR || '#111111');
              ctx.fillRect(bk.x, keyboardY, bk.w, bk.h);
              ctx.strokeStyle = config.KEYBOARD_BLACK_KEY_STROKE_COLOR || '#000000';
              ctx.lineWidth = 1;
              ctx.strokeRect(bk.x + 0.5, keyboardY + 0.5, bk.w - 1, bk.h - 1);
            }
            ctx.globalAlpha = 1.0;
          }
        } else {
          // Horizontal orientation: keyboard rotated 90 deg CCW, displayed to the left of the playhead
          const keyboardX = lineCoord - keyboardSize;

          // Draw cached base layer
          ctx.drawImage(this.keyboardCanvas, keyboardX, 0);

          // Draw illuminated active keys
          if (activeKeys.size > 0) {
            const blackKeysToRedraw = new Set();

            // 1. Draw active white keys as full-width rectangles
            for (const [pitch, info] of activeKeys) {
              const wk = geo.whiteKeys[pitch];
              if (wk) {
                ctx.globalAlpha = info.isSustain ? (config.SUSTAIN_OPACITY || 0.5) : 1.0;
                ctx.fillStyle = info.color;
                ctx.fillRect(keyboardX, wk.y, keyboardSize, Math.max(1, wk.h - 1));

                // Mark adjacent black keys to be redrawn on top
                if (geo.blackKeys[pitch - 1]) blackKeysToRedraw.add(pitch - 1);
                if (geo.blackKeys[pitch + 1]) blackKeysToRedraw.add(pitch + 1);
              } else if (geo.blackKeys[pitch]) {
                blackKeysToRedraw.add(pitch);
              }
            }

            // 2. Paint black keys on top (attached to right edge of keyboard, touching playhead)
            for (const bkPitch of blackKeysToRedraw) {
              const bk = geo.blackKeys[bkPitch];
              const activeInfo = activeKeys.get(bkPitch);
              const bkX = keyboardX + keyboardSize - bk.w;
              ctx.globalAlpha = (activeInfo && activeInfo.isSustain) ? (config.SUSTAIN_OPACITY || 0.5) : 1.0;
              ctx.fillStyle = activeInfo ? activeInfo.color : (config.KEYBOARD_BLACK_KEY_COLOR || '#111111');
              ctx.fillRect(bkX, bk.y, bk.w, bk.h);
              ctx.strokeStyle = config.KEYBOARD_BLACK_KEY_STROKE_COLOR || '#000000';
              ctx.lineWidth = 1;
              ctx.strokeRect(bkX + 0.5, bk.y + 0.5, bk.w - 1, bk.h - 1);
            }
            ctx.globalAlpha = 1.0;
          }
        }
      }
    }

    ctx.fillStyle = config.PLAYHEAD_COLOR;
    if (isVertical) {
      ctx.fillRect(0, lineCoord, width, lineWidth);
    } else {
      ctx.fillRect(lineCoord, 0, lineWidth, height);
    }
  }
}
