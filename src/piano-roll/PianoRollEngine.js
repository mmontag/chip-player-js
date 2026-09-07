import { PIANO_ROLL_CONFIG } from './config';
import { findFirstVisibleNoteIndex, getNoteName, getNotePitchOffsetAt, isBlackKey } from './midi-parser';

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
    this.isDirty = true;
    this.render();
  }

  resize(width, height) {
    if (this.canvas.width !== width || this.canvas.height !== height) {
      this.canvas.width = width;
      this.canvas.height = height;
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

  drawVerticalRibbon(ctx, bends, startMs, endMs, yStart, yEnd, rootPitch, drawW, offset, minPitch, maxPitch, laneWidth, xOffset, gap) {
    const N = bends.length;
    if (N === 0) return;
    const duration = Math.max(1, endMs - startMs);

    ctx.beginPath();
    for (let i = 0; i < N; i++) {
      const b = bends[i];
      const ratio = Math.max(0, Math.min(1, (b.timeMs - startMs) / duration));
      const y = yStart + ratio * (yEnd - yStart);
      const p = Math.max(minPitch, Math.min(maxPitch, rootPitch + b.semitoneOffset));
      const x = xOffset + (p - minPitch) * laneWidth + gap / 2 - offset;
      if (i === 0) {
        ctx.moveTo(x, y);
      } else {
        ctx.lineTo(x, y);
      }
    }
    for (let i = N - 1; i >= 0; i--) {
      const b = bends[i];
      const ratio = Math.max(0, Math.min(1, (b.timeMs - startMs) / duration));
      const y = yStart + ratio * (yEnd - yStart);
      const p = Math.max(minPitch, Math.min(maxPitch, rootPitch + b.semitoneOffset));
      const x = xOffset + (p - minPitch) * laneWidth + gap / 2 - offset + drawW;
      ctx.lineTo(x, y);
    }
    ctx.closePath();
    ctx.fill();
  }

  drawHorizontalRibbon(ctx, bends, startMs, endMs, xStart, xEnd, rootPitch, drawH, offset, minPitch, maxPitch, laneHeight, yOffset, gap) {
    const N = bends.length;
    if (N === 0) return;
    const duration = Math.max(1, endMs - startMs);

    ctx.beginPath();
    for (let i = 0; i < N; i++) {
      const b = bends[i];
      const ratio = Math.max(0, Math.min(1, (b.timeMs - startMs) / duration));
      const x = xStart + ratio * (xEnd - xStart);
      const p = Math.max(minPitch, Math.min(maxPitch, rootPitch + b.semitoneOffset));
      const y = yOffset + (maxPitch - p) * laneHeight + gap / 2 - offset;
      if (i === 0) {
        ctx.moveTo(x, y);
      } else {
        ctx.lineTo(x, y);
      }
    }
    for (let i = N - 1; i >= 0; i--) {
      const b = bends[i];
      const ratio = Math.max(0, Math.min(1, (b.timeMs - startMs) / duration));
      const x = xStart + ratio * (xEnd - xStart);
      const p = Math.max(minPitch, Math.min(maxPitch, rootPitch + b.semitoneOffset));
      const y = yOffset + (maxPitch - p) * laneHeight + gap / 2 - offset + drawH;
      ctx.lineTo(x, y);
    }
    ctx.closePath();
    ctx.fill();
  }

  render(timestamp) {
    const { canvas, ctx, config } = this;
    const width = canvas.width;
    const height = canvas.height;
    if (width === 0 || height === 0) return;

    const currentTimeMs = this.getSmoothPositionMs(timestamp);

    const isVertical = config.ORIENTATION === 'vertical';
    const isTopToBottom = config.DIRECTION === 'top-to-bottom';
    const isContinuous = config.ANIMATION_BEHAVIOR === 'continuous';

    // Pitch parameters
    const noteRange = config.NOTE_RANGE === 128 ? 128 : 88;
    const minPitch = noteRange === 128 ? 0 : 21; // 21 is A0
    const maxPitch = noteRange === 128 ? 127 : 108; // 108 is C8
    const pitchCount = maxPitch - minPitch + 1;

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
    let playheadCoord;
    if (config.SYNC_POSITION === 'top' || config.SYNC_POSITION === 'left') {
      playheadCoord = config.PLAYHEAD_OFFSET_PX || 2;
    } else if (config.SYNC_POSITION === 'center') {
      playheadCoord = Math.floor(timeDimension / 2);
    } else if (config.SYNC_POSITION === 'bottom' || config.SYNC_POSITION === 'right') {
      playheadCoord = timeDimension - (config.PLAYHEAD_OFFSET_PX || 2);
    } else if (typeof config.SYNC_POSITION === 'number') {
      playheadCoord = Math.floor(timeDimension * config.SYNC_POSITION);
    } else {
      playheadCoord = config.PLAYHEAD_OFFSET_PX || 2;
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
      let laneWidth;
      let xOffset = 0;
      if (config.PITCH_ZOOM_MODE === 'fill') {
        laneWidth = width / pitchCount;
      } else {
        laneWidth = config.PIXELS_PER_NOTE;
        const totalPitchWidth = pitchCount * laneWidth;
        xOffset = Math.max(0, Math.floor((width - totalPitchWidth) / 2));
      }

      ctx.fillStyle = config.BLACK_KEY_LANE_TINT;
      ctx.beginPath();
      for (let p = minPitch; p <= maxPitch; p++) {
        const laneX = xOffset + (p - minPitch) * laneWidth;
        if (isBlackKey(p)) {
          ctx.fillRect(laneX, 0, laneWidth, height);
        }
        // Octave lines on C notes
        if (p % 12 === 0) {
          ctx.moveTo(laneX, 0);
          ctx.lineTo(laneX, height);
        }
      }
      ctx.strokeStyle = config.OCTAVE_LINE_COLOR;
      ctx.lineWidth = 1;
      ctx.stroke();
    } else {
      // Horizontal orientation: pitches along Y axis (low at bottom, high at top)
      let laneHeight;
      let yOffset = 0;
      if (config.PITCH_ZOOM_MODE === 'fill') {
        laneHeight = height / pitchCount;
      } else {
        laneHeight = config.PIXELS_PER_NOTE;
        const totalPitchHeight = pitchCount * laneHeight;
        yOffset = Math.max(0, Math.floor((height - totalPitchHeight) / 2));
      }

      ctx.fillStyle = config.BLACK_KEY_LANE_TINT;
      ctx.beginPath();
      for (let p = minPitch; p <= maxPitch; p++) {
        const laneY = yOffset + (maxPitch - p) * laneHeight;
        if (isBlackKey(p)) {
          ctx.fillRect(0, laneY, width, laneHeight);
        }
        if (p % 12 === 0) {
          ctx.moveTo(0, laneY);
          ctx.lineTo(width, laneY);
        }
      }
      ctx.strokeStyle = config.OCTAVE_LINE_COLOR;
      ctx.lineWidth = 1;
      ctx.stroke();
    }

    // 2. Visible time window calculation
    let minVisibleMs;
    let maxVisibleMs;

    if (isContinuous) {
      if (isVertical) {
        if (isTopToBottom) {
          // Playhead at top, notes cascade downward.
          // Note Y = playheadCoord + (currentTimeMs - note.startMs) * pxPerMs.
          // Note is visible when Note Y_trailing <= height AND Note Y_leading >= 0.
          minVisibleMs = currentTimeMs - (height - playheadCoord) / pxPerMs;
          maxVisibleMs = currentTimeMs + playheadCoord / pxPerMs;
        } else {
          // Bottom to top
          minVisibleMs = currentTimeMs - playheadCoord / pxPerMs;
          maxVisibleMs = currentTimeMs + (height - playheadCoord) / pxPerMs;
        }
      } else {
        // Horizontal
        if (config.DIRECTION === 'right-to-left') {
          minVisibleMs = currentTimeMs - (width - playheadCoord) / pxPerMs;
          maxVisibleMs = currentTimeMs + playheadCoord / pxPerMs;
        } else {
          minVisibleMs = currentTimeMs - playheadCoord / pxPerMs;
          maxVisibleMs = currentTimeMs + (width - playheadCoord) / pxPerMs;
        }
      }
    } else {
      // Paginated
      minVisibleMs = pageStartMs;
      maxVisibleMs = pageStartMs + pageDurationMs;
    }

    // 3. Draw notes
    const notes = this.notes;
    if (notes.length > 0) {
      const startIndex = findFirstVisibleNoteIndex(notes, minVisibleMs, this.maxNoteDurationMs);
      const minLengthPx = config.NOTE_MIN_LENGTH_PX;
      const cornerRadius = config.NOTE_CORNER_RADIUS;
      const gap = config.NOTE_GAP_PX;
      const sustainOpacity = config.SUSTAIN_OPACITY !== undefined ? config.SUSTAIN_OPACITY : 0.5;

      // Pitch lane dimensions
      let laneWidth, xOffset, laneHeight, yOffset;
      if (isVertical) {
        laneWidth = config.PITCH_ZOOM_MODE === 'fill'
          ? width / pitchCount
          : config.PIXELS_PER_NOTE;
        const totalPitchWidth = pitchCount * laneWidth;
        xOffset = config.PITCH_ZOOM_MODE === 'fill'
          ? 0
          : Math.max(0, Math.floor((width - totalPitchWidth) / 2));
      } else {
        laneHeight = config.PITCH_ZOOM_MODE === 'fill'
          ? height / pitchCount
          : config.PIXELS_PER_NOTE;
        const totalPitchHeight = pitchCount * laneHeight;
        yOffset = config.PITCH_ZOOM_MODE === 'fill'
          ? 0
          : Math.max(0, Math.floor((height - totalPitchHeight) / 2));
      }

      // Time to canvas coordinate mapping
      const getTimeCoord = (timeMs) => {
        if (isContinuous) {
          if (isVertical) {
            return isTopToBottom
              ? playheadCoord + (currentTimeMs - timeMs) * pxPerMs
              : playheadCoord - (currentTimeMs - timeMs) * pxPerMs;
          } else {
            return config.DIRECTION === 'right-to-left'
              ? playheadCoord + (currentTimeMs - timeMs) * pxPerMs
              : playheadCoord - (currentTimeMs - timeMs) * pxPerMs;
          }
        } else {
          // Paginated
          if (isVertical) {
            return isTopToBottom
              ? (timeMs - pageStartMs) * pxPerMs
              : height - (timeMs - pageStartMs) * pxPerMs;
          } else {
            return (timeMs - pageStartMs) * pxPerMs;
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
      const glowColor = `rgba(255, 255, 255, ${glowOpacity})`;
      const fattenPx = Math.max(0, config.ACTIVE_NOTE_FATTEN || 0);
      const fattenOffset = Math.floor(fattenPx / 2);

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

        const isKeySounding = note.startMs <= currentTimeMs && currentTimeMs <= note.endMs;
        const isSustainSounding = hasSustain && currentTimeMs > note.endMs && currentTimeMs <= note.sustainEndMs;
        const isNoteActive = (isKeySounding || isSustainSounding) && !isMuted;
        const fatten = isNoteActive ? fattenPx : 0;
        const offset = isNoteActive ? fattenOffset : 0;

        // Base color
        const baseColor = config.COLOR_BY === 'track'
          ? this.getTrackColor(note.track)
          : this.getChannelColor(note.channel);

        const baseAlpha = isMuted ? config.MUTED_OPACITY : 1.0;

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
            const susStartCoord = isTimeDecreasingCoord ? keyPos : keyPos + keyDim;
            const susEndCoord = isTimeDecreasingCoord ? sustainPos : sustainPos + sustainDim;
            if (sustainHasBends) {
              this.drawVerticalRibbon(ctx, note.sustainBends, note.endMs, note.sustainEndMs, susStartCoord, susEndCoord, note.pitch, drawW, offset, minPitch, maxPitch, laneWidth, xOffset, gap);
            } else {
              drawRect(drawX, sustainPos, drawW, sustainDim, sustainRadii);
            }

            if (isSustainSounding && isGlowEnabled && !isMuted) {
              ctx.fillStyle = glowColor;
              if (sustainHasBends) {
                this.drawVerticalRibbon(ctx, note.sustainBends, note.endMs, note.sustainEndMs, susStartCoord, susEndCoord, note.pitch, drawW, offset, minPitch, maxPitch, laneWidth, xOffset, gap);
              } else {
                drawRect(drawX, sustainPos, drawW, sustainDim, sustainRadii);
              }
            }
          }

          // 2. Draw key-held portion
          ctx.globalAlpha = baseAlpha;
          ctx.fillStyle = baseColor;
          const keyStartCoord = cStart;
          const keyEndCoord = isTimeDecreasingCoord ? keyPos : keyPos + keyDim;
          if (keyHasBends) {
            this.drawVerticalRibbon(ctx, note.bends, note.startMs, note.endMs, keyStartCoord, keyEndCoord, note.pitch, drawW, offset, minPitch, maxPitch, laneWidth, xOffset, gap);
          } else {
            drawRect(drawX, keyPos, drawW, keyDim, keyRadii);
          }

          if (isKeySounding && isGlowEnabled && !isMuted) {
            ctx.fillStyle = glowColor;
            if (keyHasBends) {
              this.drawVerticalRibbon(ctx, note.bends, note.startMs, note.endMs, keyStartCoord, keyEndCoord, note.pitch, drawW, offset, minPitch, maxPitch, laneWidth, xOffset, gap);
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
            const susStartCoord = isTimeDecreasingCoord ? keyPos : keyPos + keyDim;
            const susEndCoord = isTimeDecreasingCoord ? sustainPos : sustainPos + sustainDim;
            if (sustainHasBends) {
              this.drawHorizontalRibbon(ctx, note.sustainBends, note.endMs, note.sustainEndMs, susStartCoord, susEndCoord, note.pitch, drawH, offset, minPitch, maxPitch, laneHeight, yOffset, gap);
            } else {
              drawRect(sustainPos, drawY, sustainDim, drawH, sustainRadii);
            }

            if (isSustainSounding && isGlowEnabled && !isMuted) {
              ctx.fillStyle = glowColor;
              if (sustainHasBends) {
                this.drawHorizontalRibbon(ctx, note.sustainBends, note.endMs, note.sustainEndMs, susStartCoord, susEndCoord, note.pitch, drawH, offset, minPitch, maxPitch, laneHeight, yOffset, gap);
              } else {
                drawRect(sustainPos, drawY, sustainDim, drawH, sustainRadii);
              }
            }
          }

          // 2. Draw key-held portion
          ctx.globalAlpha = baseAlpha;
          ctx.fillStyle = baseColor;
          const keyStartCoord = cStart;
          const keyEndCoord = isTimeDecreasingCoord ? keyPos : keyPos + keyDim;
          if (keyHasBends) {
            this.drawHorizontalRibbon(ctx, note.bends, note.startMs, note.endMs, keyStartCoord, keyEndCoord, note.pitch, drawH, offset, minPitch, maxPitch, laneHeight, yOffset, gap);
          } else {
            drawRect(keyPos, drawY, keyDim, drawH, keyRadii);
          }

          if (isKeySounding && isGlowEnabled && !isMuted) {
            ctx.fillStyle = glowColor;
            if (keyHasBends) {
              this.drawHorizontalRibbon(ctx, note.bends, note.startMs, note.endMs, keyStartCoord, keyEndCoord, note.pitch, drawH, offset, minPitch, maxPitch, laneHeight, yOffset, gap);
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

    // 4. Draw playhead line
    let actualPlayheadCoord = playheadCoord;
    if (!isContinuous) {
      actualPlayheadCoord = (currentTimeMs - pageStartMs) * pxPerMs;
      if (!isTopToBottom && isVertical) {
        actualPlayheadCoord = height - actualPlayheadCoord;
      }
    }

    ctx.strokeStyle = config.PLAYHEAD_COLOR;
    ctx.lineWidth = config.PLAYHEAD_LINE_WIDTH;
    ctx.beginPath();
    if (isVertical) {
      ctx.moveTo(0, actualPlayheadCoord);
      ctx.lineTo(width, actualPlayheadCoord);
    } else {
      ctx.moveTo(actualPlayheadCoord, 0);
      ctx.lineTo(actualPlayheadCoord, height);
    }
    ctx.stroke();
  }
}
