import autoBind from 'auto-bind';
import MIDIEvents from './midi/MIDIEvents';

/**
 * The MIDIFilePlayer is the engine that parses MIDI file data, and fires
 * appropriate methods on a MIDI softsynth (noteOn, noteOff, etc.) or forwards
 * MIDI messages to Web MIDI devices.
 *
 * A lot of complexity was introduced in the MIDIFilePlayer in an effort to
 * play nicely with hardware MIDI devices. Behaviors that are deterministic
 * in a softsynth can be nondeterministic on a hardware device, such as seeking
 * through the file. Non-note events must be replayed in these cases, and
 * throttled for hardware devices.
 */

// Constants
const BUFFER_AHEAD = 33;
const DELAY_MS_PER_CC_EVENT = 2;
const SKIP_SILENCE_THRESHOLD_MS = 50;

const CC_SUSTAIN_PEDAL = 64;
const CC_ALL_SOUND_OFF = 120;
const CC_RESET_ALL_CONTROLLERS = 121;
const CC_BANK_SELECT_MSB = 0;
const CC_BANK_SELECT_LSB = 32;
const CC_SOSTENUTO = 66;
const SYSEX_GM_RESET = [0x7E, 0x7F, 0x09, 0x01, 0xF7];
const SEQUENCED_CONTROLLERS = [6, 38, 96, 97, 98, 99, 100, 101];
// GM's default for every controller that has one (RP-015), used at a loop wrap: a
// controller the region writes but the loop start does not has no authored value to
// restore, so the finished pass's last value would be the next pass's opening one.
// Volume, pan, expression and the modulation/depth family are the audible ones. The
// pedals are released separately, because a note whose NOTE_OFF already ran keeps
// sounding under them; the sequenced RPN/NRPN family is meaningless without the pair it
// addresses, so it is left to the file.
const GM_CC_DEFAULTS = {
  1: 0,     // modulation
  5: 0,     // portamento time
  7: 100,   // volume
  8: 64,    // balance
  10: 64,   // pan
  11: 127,  // expression
  64: 0,    // damper
  65: 0,    // portamento on/off
  66: 0,    // sostenuto
  67: 0,    // soft
  71: 64, 72: 64, 73: 64, 74: 64, 75: 64, 76: 64, 77: 64, 78: 64,   // sound controllers
  84: 0,    // portamento control
  91: 40,   // reverb send
  92: 0,    // tremolo
  93: 0,    // chorus send
  94: 0,    // celeste
  95: 0,    // phaser
};
const META_LABELS = {
  [MIDIEvents.EVENT_META_TEXT]: 'Text',
  [MIDIEvents.EVENT_META_COPYRIGHT_NOTICE]: 'Copyright',
  [MIDIEvents.EVENT_META_TRACK_NAME]: 'Track',
  [MIDIEvents.EVENT_META_INSTRUMENT_NAME]: 'Instrument',
  [MIDIEvents.EVENT_META_LYRICS]: 'Lyrics',
  [MIDIEvents.EVENT_META_MARKER]: 'Marker',
  [MIDIEvents.EVENT_META_CUE_POINT]: 'Cue point',
};

function printSysex(data) {
  return ('[F0 ' + data.map(n => ('0' + n.toString(16)).slice(-2)).join(' ') + ']').toUpperCase();
}

// MIDIPlayer constructor
function MIDIPlayer(options) {
  autoBind(this);
  options = options || {};
  this.output = options.output || null; // Web MIDI output device (has a .send() method)
  this.synth = options.synth || null; // MIDI synth (has .noteOn(), .noteOff(), render()...)
  this.programChangeCb = options.programChangeCb;
  this.speed = 1;
  this.skipSilence = options.skipSilence || false; // skip silence at beginning of file
  this.lastProcessPlayTimestamp = 0;
  this.lastSendTimestamp = 0;
  this.events = [];
  this.paused = true;
  this.useWebMIDI = options.useWebMIDI || false;
  this.sampleRate = options.sampleRate || 44100;

  this.channelsInUse = [];
  this.channelMask = [];
  this.channelProgramNums = [];
  this.textInfo = [];
  this.isAuditioning = false;
  // Loop region in first-pass ms ({startMs, lengthMs, jumpMs, jumpIndex}),
  // or null when the file defines no loop. jumpMs/jumpIndex start the second
  // pass (the slider band); wrapping there instead of the loop start keeps
  // the head cycling in the band, like the libvgm Repeat One.
  this.loop = null;
  this.looping = false;
  // Note-on pitches still ringing per channel, for a clean loop wrap.
  this.activeNotes = new Map();
  // Set when the transport plays past the band with Repeat One off, i.e. the
  // listener is in the outro. Enabling Repeat One there must not rewind one loop
  // (the Repeat One contract: "never jump the transport backward into the loop"),
  // so the wrap stays off and the outro plays out to the song's end. Cleared on
  // load and on a seek, which re-arms it.
  this.loopRegionLeft = false;

  // Disabled due to "Page prevented back/forward cache restoration".
  // window.addEventListener('unload', this.stop);
}

// Parsing all tracks and add their events in a single event queue
MIDIPlayer.prototype.load = function (midiFile, useTrackLoops = false) {
  this.stop();
  this.position = 0;
  this.elapsedTime = 0;
  this.loop = null;
  this.activeNotes.clear();
  this.loopRegionLeft = false;
  // Same expanded list the piano roll parses, so audio and roll agree.
  const { events, loopStartMs, loopEndMs } = midiFile.getPlaybackEvents(useTrackLoops);
  this.events = events;
  if (loopEndMs != null && loopEndMs > (loopStartMs || 0)) {
    const startMs = loopStartMs || 0;
    const lengthMs = loopEndMs - startMs;
    // The band is the instance the wrap covers: the second pass when the file
    // is expanded (jump to its start, one loop length past the region), and the
    // region itself when it is not (jump back to the loop start). Either way
    // the wrap target and the band are the same span, so a composed ending after
    // the loop plays only with repeat off (matching the slider band instead of
    // repeating the ending every pass).
    const jumpMs = useTrackLoops ? loopEndMs : startMs;
    let jumpIndex = events.findIndex(e => e.playTime >= jumpMs);
    if (jumpIndex < 0) jumpIndex = events.length;
    const endMs = jumpMs + lengthMs;
    let endIndex = events.findIndex(e => e.playTime >= endMs);
    if (endIndex < 0) endIndex = events.length;
    // The resumed pass opens in the state the loop's own events were authored in, so a
    // controller the region writes but the loop start does not has no authored value to
    // restore: the finished pass's last value would open the next one (a fade, pan or
    // pedal written inside the region). Name those here, once, rather than at every wrap.
    const regionCcs = new Map();
    const beforeCcs = new Set();
    for (const e of events) {
      if (e.subtype !== MIDIEvents.EVENT_MIDI_CONTROLLER) continue;
      if (e.playTime < startMs) beforeCcs.add(e.channel + ':' + e.param1);
      else if (e.playTime < startMs + lengthMs) {
        regionCcs.set(e.channel + ':' + e.param1, [e.channel, e.param1]);
      }
    }
    const wrapDefaults = [...regionCcs].filter(([k]) => !beforeCcs.has(k)).map(([, v]) => v);
    this.loop = { startMs, lengthMs, jumpMs, jumpIndex, endMs, endIndex, wrapDefaults };
  }
  this.summarizeMidiEvents();
};

MIDIPlayer.prototype.getLoopRegion = function () {
  return this.loop;
};

MIDIPlayer.prototype.setLooping = function (looping) {
  this.looping = !!looping;
};

MIDIPlayer.prototype.doSkipSilence = function () {
  if (this.useWebMIDI) return;

  const firstNote = this.events.find(e => e.subtype === MIDIEvents.EVENT_MIDI_NOTE_ON);
  if (firstNote && firstNote.playTime > SKIP_SILENCE_THRESHOLD_MS) {
    this.setPosition(firstNote.playTime - SKIP_SILENCE_THRESHOLD_MS);
  }
};

MIDIPlayer.prototype.play = function (endCallback) {
  if (0 === this.position) {
    this.endCallback = endCallback;
    this.reset();

    this.lastProcessPlayTimestamp = performance.now();
    if (this.skipSilence && !this.useWebMIDI) {
      this.doSkipSilence();
    }

    this.resume();
    return 1;
  }
  return 0;
};

MIDIPlayer.prototype.processPlaySynth = function (buffer, bufferSize) {
  this.lastProcessPlayTimestamp = performance.now();
  const bufferStart = buffer;
  let bytesWritten = 0;
  let batchSize = 64;
  let event = null;
  const synth = this.synth;
  const msPerBatch = this.speed * 1000 * (batchSize / this.sampleRate) / 2;
  // With Repeat One, events at or past the band end belong to the outro; stop
  // applying them here so the wrap below fires at the band end, not the list end.
  // Auditioning (the piano roll's preview) disables both wrap paths below, so
  // the cap must come off with it or events past the band end -- the outro's
  // note-offs included -- are never applied while auditioning. It also comes off
  // once the region has been left (Repeat One enabled in the outro), so that
  // outro still plays and the song ends rather than hanging on the band end.
  const loopEndIdx = (this.looping && this.loop && !this.isAuditioning && !this.loopRegionLeft)
    ? this.loop.endIndex : Infinity;

  for (let samplesRemaining = bufferSize * 2;
       samplesRemaining > 0;
       samplesRemaining -= batchSize) {
    if (batchSize > samplesRemaining) batchSize = samplesRemaining;

    if (!this.paused) {
      this.elapsedTime += msPerBatch;
      // Wrap as soon as the clock reaches the band end, inside this batch, so the
      // new pass starts in this same output buffer rather than a whole buffer
      // late (which shows as a gap, or the old pass's held sound, each wrap).
      if (this.looping && this.loop && !this.isAuditioning && !this.loopRegionLeft &&
          this.position >= this.loop.endIndex && this.elapsedTime >= this.loop.endMs) {
        this.loopJump();
      }
      // Repeat One off and the clock has passed the band: the listener is in the
      // outro. Remember it so enabling Repeat One now does not rewind (above).
      if (!this.looping && this.loop && !this.isAuditioning &&
          this.elapsedTime > this.loop.endMs) {
        this.loopRegionLeft = true;
      }
      let pos = this.position;
      for (;
           this.events[pos] && this.elapsedTime >= this.events[pos].playTime && pos < loopEndIdx;
           pos++) {
        event = this.events[pos];
        switch (event.subtype) {
          case MIDIEvents.EVENT_MIDI_NOTE_ON:
            if (!this.channelMask[event.channel]) break;
            if (event.param2 === 0) { // velocity
              synth.noteOff(event.channel, event.param1);
              this.activeNotes.delete((event.channel << 7) | event.param1);
            } else {
              synth.noteOn(event.channel, event.param1, event.param2);
              this.activeNotes.set((event.channel << 7) | event.param1, 1);
            }
            break;
          case MIDIEvents.EVENT_MIDI_NOTE_OFF:
            synth.noteOff(event.channel, event.param1);
            this.activeNotes.delete((event.channel << 7) | event.param1);
            break;
          case MIDIEvents.EVENT_MIDI_PROGRAM_CHANGE:
            this.handleProgramChange(event.channel, event.param1);
            synth.programChange(event.channel, event.param1);
            break;
          case MIDIEvents.EVENT_MIDI_PITCH_BEND:
            synth.pitchBend(event.channel, (event.param2 << 7) + event.param1);
            break;
          case MIDIEvents.EVENT_MIDI_CONTROLLER:
            synth.controlChange(event.channel, event.param1, event.param2);
            break;
          // case MIDIEvents.EVENT_MIDI_CHANNEL_AFTERTOUCH:
          //   synth.channelAftertouch(event.channel, event.param1);
          //   break;
          // case MIDIEvents.EVENT_MIDI_NOTE_AFTERTOUCH:
          //   synth.noteAftertouch(event.channel, event.param1, event.param2);
          //   break;
          default:
            break;
        }
      }
      this.position = pos;
    }

    // Render the block of audio samples in float format
    synth.render(buffer, batchSize);
    buffer += batchSize * 4; // sizeof(float)
    bytesWritten += batchSize;
  }

  if (!this.paused && !this.isAuditioning) {
    // Repeat One with a loop region wraps back to the wrap target instead of
    // ending: the in-loop note-offs already fired, so only still-ringing
    // notes are cut and the loop continues like a native engine loop.
    //
    // `position` reaches endIndex as soon as the last in-band event is read,
    // which is often before the clock reaches the band end, so a trailing rest
    // would be dropped on every pass. Wait for elapsedTime to reach endMs
    // before wrapping; until then keep rendering the (silent) remainder.
    if (this.looping && this.loop && !this.loopRegionLeft &&
        this.position >= this.loop.endIndex) {
      if (this.elapsedTime >= this.loop.endMs) {
        this.loopJump();
      }
      return bytesWritten;
    }
    if (this.position >= this.events.length) {
      // Last MIDI event has been processed.
      // Continue synthesis until silence is detected.
      // This allows voices with a long release tail to complete.
      // Fast method: when entire buffer is below threshold, consider it silence.
      let synthStillActive = 0;
      const threshold = 0.001;
      for (let i = 0; i < bufferSize; i+=8) {
        if (synth.getValue(bufferStart + i, 'float') > threshold) { // Check left channel only
          synthStillActive = 1;          // Exit early
          break;
        }
      }
      if (synthStillActive === 0) {
        this.position = 0;
        this.paused = true;
        return 0;
      }
    }
  }

  return bytesWritten;
};

MIDIPlayer.prototype.processPlay = function () {

  const now = performance.now();
  const deltaTime = (now - this.lastProcessPlayTimestamp) * this.speed;
  this.lastProcessPlayTimestamp = now;

  if (this.paused) return;

  let throttleDelayMs = 0;
  let delay = 0;
  let message = null;

  this.elapsedTime += deltaTime;
  // Repeat One off and the clock has passed the band: the listener is in the
  // outro. Same flag as the synth path, so enabling Repeat One there does not
  // rewind.
  if (!this.looping && this.loop && !this.isAuditioning &&
      this.elapsedTime > this.loop.endMs) {
    this.loopRegionLeft = true;
  }
  // With Repeat One, stop scheduling at the band end so the outro is skipped;
  // matches processPlaySynth's loopEndIdx (and it comes off once the region has
  // been left, so the outro is scheduled and the song ends).
  const loopEndIdx = (this.looping && this.loop && !this.isAuditioning && !this.loopRegionLeft)
    ? this.loop.endIndex : Infinity;
  let event = this.events[this.position];
  let wraps = 0;
  for (;;) {
    // Wrap and reschedule the new pass in this same invocation, so events at
    // the band start are not a whole callback late. The wrap fires as soon as
    // the look-ahead window reaches the band end, not when the clock does:
    // Web MIDI schedules by absolute time, so the new pass's opening events
    // have to be queued now, at their future timestamps -- waiting for the
    // clock leaves them unscheduled until the first callback past the boundary,
    // which lands them up to a block late.
    if (loopEndIdx !== Infinity && this.position >= loopEndIdx) {
      if (this.elapsedTime + BUFFER_AHEAD >= this.loop.endMs && wraps++ < 4) {
        this.loopJump();
        event = this.events[this.position];
        continue;
      }
      break; // the look-ahead window has not reached the band end yet
    }
    if (!(this.events[this.position] && event.playTime < this.elapsedTime + BUFFER_AHEAD)) break;
    message = null;
    delay = 0;
    if (event.type === MIDIEvents.EVENT_SYSEX || event.type === MIDIEvents.EVENT_DIVSYSEX) {
      // Spread out scheduling of sysex messages. Example: Predator (XG).mid
      message = [event.type, ...event.data];
      if (event.data && event.data[3] === 0 && event.data[4] === 0)
        delay = 50;
      else
        delay = event.data.length / 2;
      if (throttleDelayMs > 0)
        console.debug("Sysex event at %s ms delayed %s ms:", Math.floor(event.playTime), throttleDelayMs, printSysex(event.data));
      else
        console.debug("Sysex event at %s ms:", Math.floor(event.playTime), printSysex(event.data));
    } else {
      switch (event.subtype) {
        case MIDIEvents.EVENT_MIDI_PROGRAM_CHANGE:
          this.handleProgramChange(event.channel, event.param1);
          message = [(event.subtype << 4) + event.channel, event.param1];
          break;
        case MIDIEvents.EVENT_MIDI_CHANNEL_AFTERTOUCH:
          message = [(event.subtype << 4) + event.channel, event.param1];
          break;
        case MIDIEvents.EVENT_MIDI_NOTE_OFF:
          if (!this.channelMask[event.channel]) break;
          // Web MIDI output does not go through processAudio, so track sounding
          // notes here too: loopJump() cuts the ringing ones at a wrap.
          this.activeNotes.delete((event.channel << 7) | event.param1);
          message = [(event.subtype << 4) + event.channel, event.param1, event.param2 || 0x00];
          break;
        case MIDIEvents.EVENT_MIDI_NOTE_ON:
          if (!this.channelMask[event.channel]) break;
          if (event.param2 === 0) {
            this.activeNotes.delete((event.channel << 7) | event.param1);
          } else {
            this.activeNotes.set((event.channel << 7) | event.param1, 1);
          }
          message = [(event.subtype << 4) + event.channel, event.param1, event.param2 || 0x00];
          break;
        case MIDIEvents.EVENT_MIDI_NOTE_AFTERTOUCH:
        case MIDIEvents.EVENT_MIDI_CONTROLLER:
        case MIDIEvents.EVENT_MIDI_PITCH_BEND:
          if (!this.channelMask[event.channel]) break;
          message = [(event.subtype << 4) + event.channel, event.param1, event.param2 || 0x00];
          break;
        default:
      }
    }
    if (message) {
      const scaledPlayTime = this.wallTimeFor(event.playTime);
      this.send(message, scaledPlayTime + throttleDelayMs);
      throttleDelayMs += delay;
      this.lastSendTimestamp = scaledPlayTime;
    }
    this.position++;
    event = this.events[this.position];
  }

  if (loopEndIdx !== Infinity && this.position >= loopEndIdx) {
    // Waiting for the clock to reach the band end (the outro is skipped).
    return;
  }
  if (this.position >= this.events.length) {
    setTimeout(this.endCallback, BUFFER_AHEAD + 100);
    this.position = 0;
    this.paused = true;
  }
};

// Wall-clock time (performance.now() base) at which `ms` of song time occurs,
// for Web MIDI's absolute scheduling. Song time runs `speed` times faster than
// wall time, and elapsedTime is the song time as of lastProcessPlayTimestamp.
MIDIPlayer.prototype.wallTimeFor = function (ms) {
  return this.lastProcessPlayTimestamp + (ms - this.elapsedTime) / this.speed;
};

MIDIPlayer.prototype.handleProgramChange = function (channel, program) {
  this.channelProgramNums[channel] = program;
  this.programChangeCb();
};

MIDIPlayer.prototype.togglePause = function () {
  this.paused = !this.paused;
  if (this.paused === true) {
    this.panic(this.lastSendTimestamp + 10);
  }
  return this.paused;
};

MIDIPlayer.prototype.resume = function () {
  this.paused = false;
};

MIDIPlayer.prototype.stop = function () {
  this.paused = true;
  this.activeNotes.clear();
  this.panic();
};

MIDIPlayer.prototype.send = function (message, timestamp) {
  try {
    this.output.send(message, timestamp);
  } catch (e) {
    console.warn(e);
    console.warn(message);
  }
};

// TODO: fix confusion between reset and panic
MIDIPlayer.prototype.panic = function (timestamp) {
  if (this.useWebMIDI) {
    for (let ch = 0; ch < 16; ch++) {
      this.send([(MIDIEvents.EVENT_MIDI_CONTROLLER << 4) + ch, CC_SUSTAIN_PEDAL, 0], timestamp);
      this.send([(MIDIEvents.EVENT_MIDI_CONTROLLER << 4) + ch, CC_ALL_SOUND_OFF, 0], timestamp);
      this.send([(MIDIEvents.EVENT_MIDI_CONTROLLER << 4) + ch, CC_RESET_ALL_CONTROLLERS, 0], timestamp);
    }
  } else {
    // Release sustain pedal and reset pitch bend on all channels
    for (let ch = 0; ch < 16; ch++) {
      this.synth.controlChange(ch, CC_SUSTAIN_PEDAL, 0);
      this.synth.pitchBend(ch, 8192);
    }
    this.synth.panic();
  }
};

MIDIPlayer.prototype.reset = function (timestamp) {
  if (this.useWebMIDI) {
    this.send([MIDIEvents.EVENT_SYSEX, ...SYSEX_GM_RESET]);
    for (let ch = 0; ch < 16; ch++) {
      this.send([(MIDIEvents.EVENT_MIDI_CONTROLLER << 4) + ch, CC_SUSTAIN_PEDAL, 0], timestamp);
      this.send([(MIDIEvents.EVENT_MIDI_CONTROLLER << 4) + ch, CC_ALL_SOUND_OFF, 0], timestamp);
      this.send([(MIDIEvents.EVENT_MIDI_CONTROLLER << 4) + ch, CC_RESET_ALL_CONTROLLERS, 0], timestamp);
    }
  } else {
    this.synth.reset();
  }
};

// --- Chip Player JS support ---

MIDIPlayer.prototype.getDuration = function () {
  if (this.events && this.events.length > 0) {
    return this.events[this.events.length - 1].playTime;
  }
  return 0;
};

MIDIPlayer.prototype.getPosition = function () {
  return this.elapsedTime;
};

MIDIPlayer.prototype.setOutput = function (output) {
  this.panic(this.lastSendTimestamp + 10);
  this.output = output;
  // Trigger replay of all program change events
  this.setPosition(this.getPosition() - 10);
};

MIDIPlayer.prototype.getSpeed = function () {
  return this.speed;
}

MIDIPlayer.prototype.setSpeed = function (speed) {
  this.speed = Math.max(0.1, Math.min(10, speed));
};

MIDIPlayer.prototype.setPositionSynth = function (eventList) {
  const synth = this.synth;
  eventList.forEach(event => {
    switch (event.subtype) {
      case MIDIEvents.EVENT_MIDI_PROGRAM_CHANGE:
        // handleProgramChange() is called in setPosition()
        synth.programChange(event.channel, event.param1);
        break;
      case MIDIEvents.EVENT_MIDI_PITCH_BEND:
        synth.pitchBend(event.channel, (event.param2 << 7) + event.param1);
        break;
      case MIDIEvents.EVENT_MIDI_CONTROLLER:
      default:
        synth.controlChange(event.channel, event.param1, event.param2);
        break;
    }
  });
};

MIDIPlayer.prototype.setPositionWebMidi = function (ms, eventList) {
  const wasPaused = this.paused;
  this.paused = true;

  let message;
  eventList.forEach((event, i) => {
    if (event.subtype === MIDIEvents.EVENT_MIDI_PROGRAM_CHANGE) {
      // handleProgramChange() is called in setPosition()
      message = [(event.subtype << 4) + event.channel, event.param1];
    } else if (event.subtype === MIDIEvents.EVENT_MIDI_PITCH_BEND) {
      message = [(event.subtype << 4) + event.channel, event.param1, event.param2];
    } else if (event.subtype === MIDIEvents.EVENT_MIDI_CONTROLLER) {
      message = [(event.subtype << 4) + event.channel, event.param1, event.param2];
    }
    this.send(message, this.lastSendTimestamp + 20 + i * DELAY_MS_PER_CC_EVENT);
  });

  const numEvents = eventList.length;
  const messageDelay = numEvents * DELAY_MS_PER_CC_EVENT;
  console.log("Scheduled %s events. Resuming playback in %s ms...", numEvents, messageDelay);
  if (!wasPaused) {
    setTimeout(this.resume, messageDelay);
  }
};

MIDIPlayer.prototype.collectStateEvents = function (fromIdx, toMs) {
  // Newest program/controller state before toMs, for restoring the synth
  // after a transport move (seek or loop wrap). RPN/NRPN/data-entry
  // controllers must replay in order; everything else is last-wins.
  const eventMap = {};
  const sequenced = [];
  // Bank select takes effect on the *next* program change, so the bank a program
  // was authored with is the one selected before it -- a later selection is still
  // pending. Both are state at the target, so both are replayed, in that order.
  const bankInForce = new Map();      // channel -> { [cc]: event }
  const bankAtProgram = new Map();    // channel -> { [cc]: event }
  const noteBank = (channel, event) => {
    const b = bankInForce.get(channel) || {};
    b[event.param1] = event;
    bankInForce.set(channel, b);
  };
  // A *forward* seek collects from the current event, so the bank a channel is already
  // on lives before `fromIdx`. Without this seed a program change inside the range whose
  // bank was selected earlier looks like it has none, and the default-bank rule below
  // claims bank 0 for a channel the file had on another. The wrap collects from 0, so
  // this loop is empty there.
  for (let i = 0; i < fromIdx; i++) {
    const e = this.events[i];
    if (e.subtype === MIDIEvents.EVENT_MIDI_CONTROLLER &&
        (e.param1 === CC_BANK_SELECT_MSB || e.param1 === CC_BANK_SELECT_LSB)) {
      noteBank(e.channel, e);
    }
  }
  let pos = fromIdx;
  while (this.events[pos] && this.events[pos].playTime < toMs) {
    const event = this.events[pos];
    if (event.subtype === MIDIEvents.EVENT_MIDI_PROGRAM_CHANGE) {
      this.handleProgramChange(event.channel, event.param1);
      eventMap[`${event.subtype}-${event.channel}`] = event;
      bankAtProgram.set(event.channel, Object.assign({}, bankInForce.get(event.channel)));
    } else if (event.subtype === MIDIEvents.EVENT_MIDI_PITCH_BEND) {
      // Last pitch-bend state, so a loop wrap does not carry a bent ending
      // into the next pass when the authored state before the wrap is centered.
      eventMap[`${event.subtype}-${event.channel}`] = event;
    } else if (event.subtype === MIDIEvents.EVENT_MIDI_CONTROLLER) {
      // These controllers (RPN, NRPN, Data Entry) must be sequenced in order
      if (SEQUENCED_CONTROLLERS.includes(event.param1)) {
        // console.log('Sequenced event: ch %d -- %d - %d -- %d ms', event.channel, event.param1, event.param2, event.playTime);
        sequenced.push(event);
      } else {
        // All others, we only care about the last event
        eventMap[`${event.subtype}-${event.channel}-${event.param1}`] = event;
        if (event.param1 === CC_BANK_SELECT_MSB || event.param1 === CC_BANK_SELECT_LSB) {
          noteBank(event.channel, event);
        }
      }
    }
    pos++;
  }
  // The replay is ordered so the *patch* comes out the way the first pass had it:
  // the bank each program was authored with (MSB, then LSB), the programs, then any
  // bank selected after them (still pending for the next program change), then the
  // rest of the last-wins state, then the sequenced RPN/NRPN pair. Replaying the
  // latest bank first -- what last-wins alone gives -- selects the program on a bank
  // the pass never used, which is a different instrument at the wrap.
  const collected = Object.values(eventMap);
  const isBankSelect = (e) => e.subtype === MIDIEvents.EVENT_MIDI_CONTROLLER &&
    (e.param1 === CC_BANK_SELECT_MSB || e.param1 === CC_BANK_SELECT_LSB);
  const isProgram = (e) => e.subtype === MIDIEvents.EVENT_MIDI_PROGRAM_CHANGE;
  // Bank 0 is the MIDI default, and the synth may be on any bank by the time we
  // replay this, so a program the file gave no bank says 0/0 out loud rather than
  // inheriting whatever the finished pass left.
  const bankSelect = (channel, param1, param2) => ({
    channel, param1, param2, subtype: MIDIEvents.EVENT_MIDI_CONTROLLER,
  });
  const banksForPrograms = [];
  for (const [channel, b] of bankAtProgram) {
    banksForPrograms.push(bankSelect(channel, CC_BANK_SELECT_MSB, b[CC_BANK_SELECT_MSB] ? b[CC_BANK_SELECT_MSB].param2 : 0));
    banksForPrograms.push(bankSelect(channel, CC_BANK_SELECT_LSB, b[CC_BANK_SELECT_LSB] ? b[CC_BANK_SELECT_LSB].param2 : 0));
  }
  const programs = collected.filter(isProgram);
  // A collected bank select that differs from the one its program was authored with
  // is a later, pending selection (or belongs to a channel with no program change).
  const pendingBanks = collected.filter((e) => {
    if (!isBankSelect(e)) return false;
    const b = bankAtProgram.get(e.channel);
    if (!b) return true;
    return e.param2 !== (b[e.param1] ? b[e.param1].param2 : 0);
  });
  const rest = collected.filter((e) => !isBankSelect(e) && !isProgram(e));
  return { eventList: banksForPrograms.concat(programs, pendingBanks, rest, sequenced), endIdx: pos };
};

MIDIPlayer.prototype.setPosition = function (ms) {
  if (ms < 0 || ms > this.getDuration()) return;

  this.lastProcessPlayTimestamp = performance.now();
  this.panic(this.lastSendTimestamp + 10);
  this.activeNotes.clear();
  const fromIdx = ms < this.elapsedTime ? 0 : this.position;
  const { eventList, endIdx } = this.collectStateEvents(fromIdx, ms);

  if (this.useWebMIDI) {
    this.setPositionWebMidi(ms, eventList);
  } else {
    this.setPositionSynth(eventList);
  }

  this.elapsedTime = ms;
  this.position = endIdx;
  // A seek is a transport move, so the flag describes where it landed rather than
  // being cleared: past the band the region has been left -- the ending plays out
  // and the song ends, which is the ending Repeat One then repeats -- and inside
  // it the pass plays on and wraps as usual. Clearing it outright rewound a
  // listener who sought into the ending. See loopRegionLeft.
  this.loopRegionLeft = !!(this.loop && !this.isAuditioning && ms > this.loop.endMs);
};

// Repeat-One wrap to the wrap target (the slider band's start). No
// panic: the loop is authored to continue, so program/controller state is
// restored silently and only notes still ringing from the finished pass are
// cut — the closest a JS event loop gets to a native engine loop.
MIDIPlayer.prototype.loopJump = function () {
  const loop = this.loop;
  if (!loop) return;
  const ringing = [...this.activeNotes.keys()];
  this.activeNotes.clear();
  // The state the resumed pass should open in is the state the loop's own events were
  // authored in: the loop start, not the wrap target. Expanded they differ -- the wrap
  // target is one region later, and the events queued from it are the region's own first
  // events -- so taking the state there hands the next pass the state the region ENDED
  // in, and a fade, pan or bend written inside the region reopens at its old end value.
  // (Not expanded they are the same point. A seek is deliberately incremental instead:
  // there the live state IS the state at the target.) Nothing in the list can stand for
  // a value the file never authored before the loop start, so the two things the
  // finished pass leaves behind that it cannot restore -- notes still ringing, and a
  // value no controller covers -- are ended explicitly at the boundary below.
  const { eventList } = this.collectStateEvents(0, loop.startMs);
  if (this.useWebMIDI) {
    // These messages belong at the band end, not at the last event the
    // scheduler sent: the last in-band event can sit a whole rest before the
    // boundary (and the wrap fires inside the look-ahead window), so that
    // timestamp cuts held notes and switches the sound while the pass is still
    // playing. The new pass's events are queued at or after the boundary, and
    // same-timestamp messages keep their send order, so this can share it. A
    // wrap detected past the boundary sends as soon as possible, which is all
    // a past timestamp can mean.
    const boundary = Math.max(this.wallTimeFor(loop.endMs), this.lastProcessPlayTimestamp);
    for (const key of ringing) {
      this.send([(MIDIEvents.EVENT_MIDI_NOTE_OFF << 4) + (key >> 7), key & 0x7f, 0],
        boundary);
    }
    // Release the pedal family before restoring the next pass's controller state:
    // a note whose NOTE_OFF already ran is no longer in `ringing`, but keeps
    // sounding while the damper *or* the sostenuto pedal holds it, and the restore
    // would leave it on. An all-notes-off would not do it -- that message leaves
    // what a pedal is holding.
    for (let ch = 0; ch < 16; ch++) {
      this.send([(MIDIEvents.EVENT_MIDI_CONTROLLER << 4) + ch, CC_SUSTAIN_PEDAL, 0],
        boundary);
      this.send([(MIDIEvents.EVENT_MIDI_CONTROLLER << 4) + ch, CC_SOSTENUTO, 0],
        boundary);
    }
    // Centre, the value a channel has before any bend: the collected list only
    // carries bends the file authored, so this is the one that has to be said.
    for (let ch = 0; ch < 16; ch++) {
      this.send([(MIDIEvents.EVENT_MIDI_PITCH_BEND << 4) + ch, 0, 64], boundary);
    }
    for (const [channel, param1] of loop.wrapDefaults) {
      const value = GM_CC_DEFAULTS[param1];
      if (value === undefined) continue;
      this.send([(MIDIEvents.EVENT_MIDI_CONTROLLER << 4) + channel, param1, value], boundary);
    }
    for (const event of eventList) {
      let message = null;
      if (event.subtype === MIDIEvents.EVENT_MIDI_PROGRAM_CHANGE) {
        message = [(event.subtype << 4) + event.channel, event.param1];
      } else if (event.subtype === MIDIEvents.EVENT_MIDI_PITCH_BEND) {
        message = [(event.subtype << 4) + event.channel, event.param1, event.param2];
      } else if (event.subtype === MIDIEvents.EVENT_MIDI_CONTROLLER) {
        message = [(event.subtype << 4) + event.channel, event.param1, event.param2];
      }
      if (message) this.send(message, boundary);
    }
  } else {
    for (const key of ringing) {
      if (this.synth) this.synth.noteOff(key >> 7, key & 0x7f);
    }
    if (this.synth) {
      // Same release on the synth, then restore the loop start's state.
      for (let ch = 0; ch < 16; ch++) {
        this.synth.controlChange(ch, CC_SUSTAIN_PEDAL, 0);
        this.synth.controlChange(ch, CC_SOSTENUTO, 0);
        this.synth.pitchBend(ch, 8192);
      }
      for (const [channel, param1] of loop.wrapDefaults) {
        const value = GM_CC_DEFAULTS[param1];
        if (value === undefined) continue;
        this.synth.controlChange(channel, param1, value);
      }
      this.setPositionSynth(eventList);
    }
  }
  // The wrap can be detected up to a processing block away from the band end:
  // the synth renders whole buffers (clock past endMs), and Web MIDI wraps
  // within its look-ahead window (clock still before endMs). Carry the signed
  // difference -- exactly `elapsedTime - loop.lengthMs`, since jumpMs is the
  // band start one loop length back from endMs. That keeps the clock a pure
  // music time mod the loop length whichever way it is detected, so the new
  // pass is scheduled against the original boundary rather than the callback
  // that noticed it: the loop does not drift, and a Web MIDI note at the band
  // start is queued for the boundary instead of being sent already late.
  this.elapsedTime = loop.jumpMs + (this.elapsedTime - loop.endMs);
  this.position = loop.jumpIndex;
};

MIDIPlayer.prototype.getChannelInUse = function (ch) {
  return !!this.channelsInUse[ch];
};

MIDIPlayer.prototype.getChannelProgramNum = function (ch) {
  return this.channelProgramNums[ch];
};

MIDIPlayer.prototype.summarizeMidiEvents = function () {
  this.textInfo = [];
  const channelsInUse = this.channelsInUse;
  const channelProgramNums = this.channelProgramNums;
  const channelMask = this.channelMask;
  for (let i = 0; i < 16; i++) {
    channelsInUse[i] = 0;
    channelProgramNums[i] = 0;
    channelMask[i] = true;
  }

  for (let j = 0; j < this.events.length; j++) {
    const event = this.events[j];
    switch (event.subtype) {
      case MIDIEvents.EVENT_MIDI_NOTE_ON:
        if (event.param2 > 0) channelsInUse[event.channel] = 1;
        break;
      case MIDIEvents.EVENT_MIDI_PROGRAM_CHANGE:
        if (!channelProgramNums[event.channel]) this.handleProgramChange(event.channel, event.param1);
        break;
      case MIDIEvents.EVENT_META_TEXT:
      case MIDIEvents.EVENT_META_COPYRIGHT_NOTICE:
      case MIDIEvents.EVENT_META_TRACK_NAME:
      // case MIDIEvents.EVENT_META_INSTRUMENT_NAME:
      case MIDIEvents.EVENT_META_LYRICS:
      case MIDIEvents.EVENT_META_MARKER:
      case MIDIEvents.EVENT_META_CUE_POINT:
        const text = event.data.map(c => String.fromCharCode(c)).join('').trim();
        if (text && !text.match(/nstd/i))
          this.textInfo.push(`${META_LABELS[event.subtype]}: ${text}`);
        break;
      default:
        break;
    }
  }
};

MIDIPlayer.prototype.setChannelMute = function (ch, isMuted) {
  this.channelMask[ch] = !isMuted;
  if (isMuted) {
    // TODO separate synth from webMidi
    const timestamp = this.lastSendTimestamp + 10;
    this.send([(MIDIEvents.EVENT_MIDI_CONTROLLER << 4) + ch, CC_SUSTAIN_PEDAL, 0], timestamp);
    this.send([(MIDIEvents.EVENT_MIDI_CONTROLLER << 4) + ch, CC_ALL_SOUND_OFF, 0], timestamp);
    this.synth.panicChannel(ch);
  }
};

MIDIPlayer.prototype.setUseWebMIDI = function (useWebMIDI) {
  this.useWebMIDI = useWebMIDI;
  // Trigger replay of all program change events
  this.setPosition(this.getPosition() - 10);
};

export default MIDIPlayer;
