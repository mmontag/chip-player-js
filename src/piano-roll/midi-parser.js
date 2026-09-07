import MIDIFile from '../players/midi/midi-helpers.js';
import MIDIEvents from '../players/midi/MIDIEvents.js';
import { GM_DRUM_KITS, GM_INSTRUMENTS } from '../gm-patch-map.js';

const {
  EVENT_MIDI_NOTE_ON,
  EVENT_MIDI_NOTE_OFF,
  EVENT_MIDI_CONTROLLER,
  EVENT_MIDI_PROGRAM_CHANGE,
  EVENT_META_TRACK_NAME,
  EVENT_META_INSTRUMENT_NAME,
} = MIDIEvents;

const CC_SUSTAIN_PEDAL = 64;
const CC_ALL_SOUND_OFF = 120;
const CC_ALL_NOTES_OFF = 123;

const NOTE_NAMES = ['C', 'C#', 'D', 'D#', 'E', 'F', 'F#', 'G', 'G#', 'A', 'A#', 'B'];

/**
 * Returns a human-readable note name with octave (e.g. 60 -> "C4", 61 -> "C#4").
 */
export function getNoteName(pitch) {
  const note = NOTE_NAMES[pitch % 12];
  const octave = Math.floor(pitch / 12) - 1;
  return `${note}${octave}`;
}

/**
 * Returns true if pitch corresponds to a black piano key (C#, D#, F#, G#, A#).
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
        priorSustained.sustainEndMs = Math.max(priorSustained.endMs, playTime);
        priorSustained.durationMs = priorSustained.sustainEndMs - priorSustained.startMs;
        notes.push(priorSustained);
      }

      openNotes.set(key, {
        pitch: param1,
        velocity: param2,
        track,
        channel,
        startMs: playTime,
        endMs: null,
        sustainEndMs: null,
      });
    } else if (subtype === EVENT_MIDI_NOTE_OFF || (subtype === EVENT_MIDI_NOTE_ON && param2 === 0)) {
      // Note Off
      const key = `${track}_${channel}_${param1}`;
      if (openNotes.has(key)) {
        const note = openNotes.get(key);
        openNotes.delete(key);
        note.endMs = Math.max(note.startMs + 1, playTime);

        if (sustainPedalDown[channel]) {
          // Key released while sustain pedal is held down -> note remains sustained by pedal
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
            note.sustainEndMs = Math.max(note.endMs, playTime);
            note.durationMs = note.sustainEndMs - note.startMs;
            notes.push(note);
          }
          sustainedPedalNotes[channel] = [];
        }
      } else if (param1 === CC_ALL_SOUND_OFF || param1 === CC_ALL_NOTES_OFF) {
        const list = sustainedPedalNotes[channel];
        for (let s = 0; s < list.length; s++) {
          const note = list[s];
          note.sustainEndMs = Math.max(note.endMs, playTime);
          note.durationMs = note.sustainEndMs - note.startMs;
          notes.push(note);
        }
        sustainedPedalNotes[channel] = [];

        for (const [k, note] of openNotes.entries()) {
          if (note.channel === channel) {
            note.endMs = Math.max(note.startMs + 1, playTime);
            note.durationMs = note.endMs - note.startMs;
            notes.push(note);
            openNotes.delete(k);
          }
        }
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
      note.sustainEndMs = Math.max(note.endMs, maxTimeMs);
      note.durationMs = note.sustainEndMs - note.startMs;
      notes.push(note);
    }
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
