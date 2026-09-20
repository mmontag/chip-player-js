import MIDIFile from '../players/midi/midi-helpers.js';
import MIDIEvents from '../players/midi/MIDIEvents.js';
import { GM_DRUM_KITS, GM_INSTRUMENTS } from '../gm-patch-map.js';

const {
  EVENT_MIDI_NOTE_ON,
  EVENT_MIDI_NOTE_OFF,
  EVENT_MIDI_CONTROLLER,
  EVENT_MIDI_PROGRAM_CHANGE,
  EVENT_MIDI_PITCH_BEND,
  EVENT_META_TRACK_NAME,
  EVENT_META_INSTRUMENT_NAME,
} = MIDIEvents;

const CC_DATA_ENTRY_MSB = 6;
const CC_DATA_ENTRY_LSB = 38;
const CC_SUSTAIN_PEDAL = 64;
const CC_DATA_INCREMENT = 96;
const CC_DATA_DECREMENT = 97;
const CC_NRPN_LSB = 98;
const CC_NRPN_MSB = 99;
const CC_RPN_LSB = 100;
const CC_RPN_MSB = 101;
const CC_ALL_SOUND_OFF = 120;
const CC_RESET_ALL_CONTROLLERS = 121;
const CC_ALL_NOTES_OFF = 123;

const NOTE_NAMES = ['C', 'C♯', 'D', 'D♯', 'E', 'F', 'F♯', 'G', 'G♯', 'A', 'A♯', 'B'];

/**
 * Returns a human-readable note name with octave (e.g. 60 -> "C4", 61 -> "C♯4").
 */
export function getNoteName(pitch) {
  const note = NOTE_NAMES[pitch % 12];
  const octave = Math.floor(pitch / 12) - 1;
  return `${note}${octave}`;
}

/**
 * Returns true if pitch corresponds to a black piano key (C♯, D♯, F♯, G♯, A♯).
 */
export function isBlackKey(pitch) {
  const semitone = pitch % 12;
  return semitone === 1 || semitone === 3 || semitone === 6 || semitone === 8 || semitone === 10;
}

/**
 * Returns true if buffer contains valid MIDI file header ('MThd' or 'RIFF....RMID').
 */
export function isMidiData(buffer) {
  if (!buffer) return false;
  const byteLength = buffer.byteLength;
  if (!byteLength || byteLength < 14) return false;

  const ab = buffer instanceof ArrayBuffer ? buffer : buffer.buffer;
  const byteOffset = buffer.byteOffset || 0;
  if (!ab || ab.byteLength < byteOffset + 12) return false;

  const view = new DataView(ab, byteOffset, Math.min(16, byteLength));
  // Standard MIDI file 'MThd' (0x4D546864)
  if (view.getUint32(0) === 0x4d546864) return true;
  // RIFF MIDI file ('RIFF' ... 'RMID')
  if (view.getUint32(0) === 0x52494646 && byteLength >= 12) {
    if (view.getUint32(8) === 0x524d4944) return true;
  }
  return false;
}

function addBendPoint(bendsArray, timeMs, semitones) {
  if (!bendsArray) return;
  const len = bendsArray.length;
  if (len > 0) {
    const last = bendsArray[len - 1];
    if (last.timeMs === timeMs) {
      last.semitoneOffset = semitones;
      return;
    }
  }
  bendsArray.push({ timeMs, semitoneOffset: semitones });
}

function ensureBoundaries(points, startMs, endMs) {
  if (!points || points.length === 0) return points;
  if (points[0].timeMs > startMs) {
    points.unshift({ timeMs: startMs, semitoneOffset: points[0].semitoneOffset });
  } else {
    points[0].timeMs = startMs;
  }
  const last = points[points.length - 1];
  if (last.timeMs < endMs) {
    points.push({ timeMs: endMs, semitoneOffset: last.semitoneOffset });
  } else {
    last.timeMs = endMs;
  }
  return points;
}

function simplifyBends(points) {
  if (!points || points.length <= 2) return points;
  const result = [points[0]];
  for (let i = 1; i < points.length - 1; i++) {
    const prev = result[result.length - 1];
    const curr = points[i];
    const next = points[i + 1];

    const dt = next.timeMs - prev.timeMs;
    if (dt <= 0) continue;

    const tRatio = (curr.timeMs - prev.timeMs) / dt;
    const expectedBend = prev.semitoneOffset + tRatio * (next.semitoneOffset - prev.semitoneOffset);
    if (Math.abs(curr.semitoneOffset - expectedBend) > 0.04) {
      result.push(curr);
    }
  }
  result.push(points[points.length - 1]);
  return result;
}

function finalizeNoteBends(note) {
  if (note.keyBends && note.keyBends.length > 0) {
    ensureBoundaries(note.keyBends, note.startMs, note.endMs);
    const hasNonZero = note.keyBends.some(b => Math.abs(b.semitoneOffset) > 0.02);
    note.bends = hasNonZero ? simplifyBends(note.keyBends) : null;
  } else {
    note.bends = null;
  }
  delete note.keyBends;

  if (note.sustainBends && note.sustainBends.length > 0) {
    const susEnd = note.sustainEndMs || note.endMs;
    ensureBoundaries(note.sustainBends, note.endMs, susEnd);
    const hasNonZero = note.sustainBends.some(b => Math.abs(b.semitoneOffset) > 0.02);
    note.sustainBends = hasNonZero ? simplifyBends(note.sustainBends) : null;
  } else {
    note.sustainBends = null;
  }
}

/**
 * Evaluates pitch bend offset in semitones at a specific time timestamp.
 */
export function getNotePitchOffsetAt(bends, timeMs) {
  if (!bends || bends.length === 0) return 0;
  if (timeMs <= bends[0].timeMs) return bends[0].semitoneOffset;
  const last = bends[bends.length - 1];
  if (timeMs >= last.timeMs) return last.semitoneOffset;

  for (let i = 0; i < bends.length - 1; i++) {
    const p0 = bends[i];
    const p1 = bends[i + 1];
    if (timeMs >= p0.timeMs && timeMs <= p1.timeMs) {
      const dt = p1.timeMs - p0.timeMs;
      if (dt <= 0) return p0.semitoneOffset;
      const ratio = (timeMs - p0.timeMs) / dt;
      return p0.semitoneOffset + ratio * (p1.semitoneOffset - p0.semitoneOffset);
    }
  }
  return last.semitoneOffset;
}

/**
 * Parses a raw MIDI file ArrayBuffer or Uint8Array into structured notes, channels, and metadata.
 * Completely decoupled from synthesizers and audio players.
 */
export function parseMidiData(buffer) {
  if (!isMidiData(buffer)) return null;

  const midiFile = new MIDIFile(buffer);
  const events = midiFile.getEvents();

  const notes = [];
  const openNotes = new Map(); // key: `${track}_${channel}_${pitch}` -> note
  const sustainedPedalNotes = Array.from({ length: 16 }, () => []);
  const sustainPedalDown = new Array(16).fill(false);

  const channelPrograms = new Uint8Array(16);
  const channelUsed = new Array(16).fill(false);
  const channelNoteCounts = new Array(16).fill(0);
  const channelRpnMsb = new Int16Array(16).fill(-1);
  const channelRpnLsb = new Int16Array(16).fill(-1);
  const channelBendRange = new Float32Array(16).fill(2.0); // GM default: +/- 2 semitones
  const channelCurrentBend = new Float32Array(16).fill(0.0); // Current bend in semitones
  const trackNames = {};
  const trackUsedChannels = {};
  const trackNoteCounts = {};

  let maxTimeMs = 0;

  for (let i = 0; i < events.length; i++) {
    const ev = events[i];
    const playTime = ev.playTime || 0;
    if (playTime > maxTimeMs) {
      maxTimeMs = playTime;
    }

    const { subtype, track = 0, channel = 0, param1, param2, data } = ev;

    if (subtype === EVENT_MIDI_NOTE_ON && param2 > 0) {
      // Note On with non-zero velocity
      channelUsed[channel] = true;
      channelNoteCounts[channel]++;
      trackNoteCounts[track] = (trackNoteCounts[track] || 0) + 1;
      if (!trackUsedChannels[track]) trackUsedChannels[track] = new Set();
      trackUsedChannels[track].add(channel);

      const key = `${track}_${channel}_${param1}`;
      if (openNotes.has(key)) {
        // Prior note not explicitly closed; close it at this timestamp
        const priorNote = openNotes.get(key);
        addBendPoint(priorNote.keyBends, playTime, channelCurrentBend[channel]);
        priorNote.endMs = Math.max(priorNote.startMs + 1, playTime);
        if (sustainPedalDown[channel]) {
          priorNote.sustainEndMs = playTime;
        }
        priorNote.durationMs = (priorNote.sustainEndMs || priorNote.endMs) - priorNote.startMs;
        notes.push(priorNote);
        openNotes.delete(key);
      }

      // If this pitch on this channel was ringing in sustained state from a prior stroke, terminate it
      const sustainedList = sustainedPedalNotes[channel];
      const sustainedIdx = sustainedList.findIndex(n => n.pitch === param1);
      if (sustainedIdx !== -1) {
        const priorSustained = sustainedList[sustainedIdx];
        sustainedList.splice(sustainedIdx, 1);
        addBendPoint(priorSustained.sustainBends, playTime, channelCurrentBend[channel]);
        priorSustained.sustainEndMs = Math.max(priorSustained.endMs, playTime);
        priorSustained.durationMs = priorSustained.sustainEndMs - priorSustained.startMs;
        notes.push(priorSustained);
      }

      openNotes.set(key, {
        pitch: param1,
        velocity: param2,
        track,
        channel,
        program: channelPrograms[channel],
        startMs: playTime,
        endMs: null,
        sustainEndMs: null,
        keyBends: [{ timeMs: playTime, semitoneOffset: channelCurrentBend[channel] }],
        sustainBends: null,
      });
    } else if (subtype === EVENT_MIDI_NOTE_OFF || (subtype === EVENT_MIDI_NOTE_ON && param2 === 0)) {
      // Note Off
      const key = `${track}_${channel}_${param1}`;
      if (openNotes.has(key)) {
        const note = openNotes.get(key);
        openNotes.delete(key);
        addBendPoint(note.keyBends, playTime, channelCurrentBend[channel]);
        note.endMs = Math.max(note.startMs + 1, playTime);

        if (sustainPedalDown[channel]) {
          // Key released while sustain pedal is held down -> note remains sustained by pedal
          note.sustainBends = [{ timeMs: playTime, semitoneOffset: channelCurrentBend[channel] }];
          sustainedPedalNotes[channel].push(note);
        } else {
          // Key released without pedal -> note stops immediately
          note.durationMs = note.endMs - note.startMs;
          notes.push(note);
        }
      }
    } else if (subtype === EVENT_MIDI_CONTROLLER) {
      if (param1 === CC_SUSTAIN_PEDAL) {
        const isDown = param2 >= 64;
        sustainPedalDown[channel] = isDown;
        if (!isDown) {
          // Sustain pedal released: terminate all notes sustained by pedal on this channel
          const list = sustainedPedalNotes[channel];
          for (let s = 0; s < list.length; s++) {
            const note = list[s];
            addBendPoint(note.sustainBends, playTime, channelCurrentBend[channel]);
            note.sustainEndMs = Math.max(note.endMs, playTime);
            note.durationMs = note.sustainEndMs - note.startMs;
            notes.push(note);
          }
          sustainedPedalNotes[channel] = [];
        }
      } else if (param1 === CC_RPN_MSB) {
        channelRpnMsb[channel] = param2;
      } else if (param1 === CC_RPN_LSB) {
        channelRpnLsb[channel] = param2;
      } else if (param1 === CC_NRPN_MSB || param1 === CC_NRPN_LSB) {
        channelRpnMsb[channel] = -1;
        channelRpnLsb[channel] = -1;
      } else if (param1 === CC_DATA_ENTRY_MSB) {
        if (channelRpnMsb[channel] === 0 && channelRpnLsb[channel] === 0) {
          channelBendRange[channel] = param2;
        }
      } else if (param1 === CC_DATA_ENTRY_LSB) {
        if (channelRpnMsb[channel] === 0 && channelRpnLsb[channel] === 0) {
          channelBendRange[channel] = Math.floor(channelBendRange[channel]) + param2 / 100;
        }
      } else if (param1 === CC_DATA_INCREMENT) {
        if (channelRpnMsb[channel] === 0 && channelRpnLsb[channel] === 0) {
          channelBendRange[channel] = Math.min(127, channelBendRange[channel] + 1);
        }
      } else if (param1 === CC_DATA_DECREMENT) {
        if (channelRpnMsb[channel] === 0 && channelRpnLsb[channel] === 0) {
          channelBendRange[channel] = Math.max(0, channelBendRange[channel] - 1);
        }
      } else if (param1 === CC_RESET_ALL_CONTROLLERS) {
        channelRpnMsb[channel] = -1;
        channelRpnLsb[channel] = -1;
        channelCurrentBend[channel] = 0;
      } else if (param1 === CC_ALL_SOUND_OFF || param1 === CC_ALL_NOTES_OFF) {
        const list = sustainedPedalNotes[channel];
        for (let s = 0; s < list.length; s++) {
          const note = list[s];
          addBendPoint(note.sustainBends, playTime, channelCurrentBend[channel]);
          note.sustainEndMs = Math.max(note.endMs, playTime);
          note.durationMs = note.sustainEndMs - note.startMs;
          notes.push(note);
        }
        sustainedPedalNotes[channel] = [];

        for (const [k, note] of openNotes.entries()) {
          if (note.channel === channel) {
            addBendPoint(note.keyBends, playTime, channelCurrentBend[channel]);
            note.endMs = Math.max(note.startMs + 1, playTime);
            note.durationMs = note.endMs - note.startMs;
            notes.push(note);
            openNotes.delete(k);
          }
        }
      }
    } else if (subtype === EVENT_MIDI_PITCH_BEND) {
      const rawValue = (param2 << 7) | param1;
      const normalized = (rawValue - 8192) / 8192;
      const semitones = normalized * channelBendRange[channel];
      channelCurrentBend[channel] = semitones;

      for (const note of openNotes.values()) {
        if (note.channel === channel) {
          addBendPoint(note.keyBends, playTime, semitones);
        }
      }
      const sustainedList = sustainedPedalNotes[channel];
      for (let s = 0; s < sustainedList.length; s++) {
        addBendPoint(sustainedList[s].sustainBends, playTime, semitones);
      }
    } else if (subtype === EVENT_MIDI_PROGRAM_CHANGE) {
      channelPrograms[channel] = param1;
    } else if (subtype === EVENT_META_TRACK_NAME || subtype === EVENT_META_INSTRUMENT_NAME) {
      if (data && Array.isArray(data)) {
        const text = data.map(c => String.fromCharCode(c)).join('').trim();
        if (text && !trackNames[track]) {
          trackNames[track] = text;
        }
      }
    }
  }

  // Close any notes that were still open at song end
  for (const note of openNotes.values()) {
    addBendPoint(note.keyBends, maxTimeMs, channelCurrentBend[note.channel]);
    note.endMs = Math.max(note.startMs + 1, maxTimeMs);
    if (sustainPedalDown[note.channel]) {
      note.sustainEndMs = maxTimeMs;
    }
    note.durationMs = (note.sustainEndMs || note.endMs) - note.startMs;
    notes.push(note);
  }

  // Close any notes still sustained under pedal at song end
  for (let ch = 0; ch < 16; ch++) {
    const list = sustainedPedalNotes[ch];
    for (let s = 0; s < list.length; s++) {
      const note = list[s];
      addBendPoint(note.sustainBends, maxTimeMs, channelCurrentBend[ch]);
      note.sustainEndMs = Math.max(note.endMs, maxTimeMs);
      note.durationMs = note.sustainEndMs - note.startMs;
      notes.push(note);
    }
  }

  // Finalize note bends for all notes
  for (let i = 0; i < notes.length; i++) {
    finalizeNoteBends(notes[i]);
  }

  // Sort notes by startMs ascending for efficient binary-search culling
  notes.sort((a, b) => a.startMs - b.startMs);

  let maxNoteDurationMs = 1000;
  for (let i = 0; i < notes.length; i++) {
    if (notes[i].durationMs > maxNoteDurationMs) {
      maxNoteDurationMs = notes[i].durationMs;
    }
  }

  // Build channel list metadata
  const channels = [];
  for (let ch = 0; ch < 16; ch++) {
    if (channelUsed[ch]) {
      const pgm = channelPrograms[ch];
      const instrumentName = ch === 9
        ? (GM_DRUM_KITS[pgm] || 'Standard Drum Kit').trim()
        : (GM_INSTRUMENTS[pgm] || `Patch ${pgm}`).trim();

      channels.push({
        channel: ch,
        program: pgm,
        instrumentName,
        noteCount: channelNoteCounts[ch],
      });
    }
  }

  // Build track list metadata
  const numTracks = midiFile.tracks ? midiFile.tracks.length : 1;
  const tracks = [];
  for (let trk = 0; trk < numTracks; trk++) {
    const count = trackNoteCounts[trk] || 0;
    if (count > 0 || trackNames[trk]) {
      const channelSet = trackUsedChannels[trk] || new Set();
      const firstChannel = channelSet.size > 0 ? Array.from(channelSet)[0] : 0;
      const pgm = channelPrograms[firstChannel];
      const instrumentName = firstChannel === 9
        ? (GM_DRUM_KITS[pgm] || 'Standard Drum Kit').trim()
        : (GM_INSTRUMENTS[pgm] || `Patch ${pgm}`).trim();

      tracks.push({
        track: trk,
        name: trackNames[trk] || `Track ${trk + 1}`,
        channels: Array.from(channelSet),
        instrumentName,
        noteCount: count,
      });
    }
  }

  return {
    notes,
    channels,
    tracks,
    durationMs: maxTimeMs,
    maxNoteDurationMs,
    format: midiFile.header ? midiFile.header.getFormat() : 1,
  };
}

/**
 * Binary search to find the index of the first note that could be visible at minTimeMs.
 */
export function findFirstVisibleNoteIndex(notes, minTimeMs, maxDurationMs = 30000) {
  const targetMs = minTimeMs - maxDurationMs;
  let low = 0;
  let high = notes.length - 1;
  let result = 0;

  while (low <= high) {
    const mid = (low + high) >> 1;
    if (notes[mid].startMs >= targetMs) {
      result = mid;
      high = mid - 1;
    } else {
      low = mid + 1;
    }
  }
  return result;
}
