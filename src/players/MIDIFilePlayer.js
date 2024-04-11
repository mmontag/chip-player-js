import autoBind from 'auto-bind';

const MIDIEvents = require('midievents');
require('./midi/midi-helpers');

/**
 * The MIDIFilePlayer is the engine that parses MIDI file data, and fires
 * appropriate methods on a MIDI softsynth (noteOn, noteOff, etc.) or forwards
 * MIDI messages to Web MIDI devices.
 *
 * A lot of complexity was introduced in the MIDIFilePlayer in an effort to
 * play nicely with hardware MIDI devices. Behaviors that are deterministic
 * in a softsynth can be nondeterministic on a hardware device, such as
 * skipping silence at the beginning of a MIDI file, or seeking through the
 * file. Non-note events must be replayed in these cases, and throttled for
 * hardware devices, especially in the case of sysex commands that might make
 * the hardware busy for 0.5 seconds, such as a GM reset command sent to a
 * Roland SC-55. The time allowed for these events is a best-guess - so your
 * mileage may vary.
 */

// Constants
const BUFFER_AHEAD = 33;
const DELAY_MS_PER_CC_EVENT = 2;
// const DELAY_MS_PER_SYSEX_EVENT = 15;
const DELAY_MS_PER_SYSEX_BYTE = 0.5;
const DELAY_MS_PER_XG_SYSTEM_EVENT = 500;

const CC_SUSTAIN_PEDAL = 64;
const CC_ALL_SOUND_OFF = 120;
const CC_RESET_ALL_CONTROLLERS = 121;
const SYSEX_GM_RESET = [0x7E, 0x7F, 0x09, 0x01, 0xF7];
const SEQUENCED_CONTROLLERS = [6, 38, 96, 97, 98, 99, 100, 101];
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
  this.useWebMIDI = false;
  this.sampleRate = options.sampleRate || 44100;

  this.channelsInUse = [];
  this.channelMask = [];
  this.channelProgramNums = [];
  this.textInfo = [];

  window.addEventListener('unload', this.stop);
}

// Parsing all tracks and add their events in a single event queue
MIDIPlayer.prototype.load = function (midiFile, useTrackLoops = false) {
  this.stop();
  this.position = 0;
  this.elapsedTime = 0;
  const tracks = midiFile.tracks.map((_, i) => midiFile.getTrackEvents(i));
  if (useTrackLoops) {
    console.debug('Processing MIDI track loops...');
    this.events = midiFile.getLoopedEvents(tracks, 2);
  } else {
    this.events = midiFile.getEvents();
  }
  this.summarizeMidiEvents();
};

MIDIPlayer.prototype.doSkipSilence = function () {
  let firstNoteDelay = 0;
  let messageDelay = 0;
  let numSysexEvents = 0;
  const firstNote = this.events.find(e => e.subtype === MIDIEvents.EVENT_MIDI_NOTE_ON);
  if (firstNote && firstNote.playTime > 0) {
    // Accelerated playback of all events prior to first note
    if (this.useWebMIDI) {
      let message;
      const eventList = [];
      while (this.events[this.position] !== firstNote) {
        const event = this.events[this.position];
        this.position++;

        // Throttle sysex events
        // (In some cases this may actually increase the time to first note)
        if (event.type === MIDIEvents.EVENT_SYSEX || event.type === MIDIEvents.EVENT_DIVSYSEX) {
          console.debug("Sysex event at %s ms:", Math.floor(event.playTime), printSysex(event.data));
          if (event.data && event.data[3] === 0 && event.data[4] === 0) {
            firstNoteDelay += DELAY_MS_PER_XG_SYSTEM_EVENT;
            messageDelay += DELAY_MS_PER_XG_SYSTEM_EVENT;
          } else {
            const delay = DELAY_MS_PER_SYSEX_BYTE * event.length;
            firstNoteDelay += delay;
            messageDelay += delay;
          }
          numSysexEvents++;
          message = [event.type, ...event.data];
        } else if (MIDIEvents.MIDI_1PARAM_EVENTS.indexOf(event.subtype) !== -1) {
          firstNoteDelay += DELAY_MS_PER_CC_EVENT;
          messageDelay += DELAY_MS_PER_CC_EVENT;
          message = [(event.subtype << 4) + event.channel, event.param1];
        } else if (MIDIEvents.MIDI_2PARAMS_EVENTS.indexOf(event.subtype) !== -1) {
          firstNoteDelay += DELAY_MS_PER_CC_EVENT;
          messageDelay += DELAY_MS_PER_CC_EVENT;
          message = [(event.subtype << 4) + event.channel, event.param1, (event.param2 || 0x00)];
        } else {
          continue;
        }
        eventList.push({
          message: message,
          timestamp: this.lastProcessPlayTimestamp + messageDelay,
        });
      }

      eventList.forEach(({ message, timestamp }) => {
        this.send(message, timestamp);
      });
      // Set lastProcessPlayTimestamp to a point in the past so that the first note event plays immediately.
      console.log("Time to first note %s ms was updated to %s ms; %s sysex events",
        Math.round(firstNote.playTime), firstNoteDelay, numSysexEvents);
      this.lastProcessPlayTimestamp += (firstNoteDelay - firstNote.playTime);
    } else {
      this.setPosition(firstNote.playTime - 50);
    }
  }
};

MIDIPlayer.prototype.play = function (endCallback) {
  if (0 === this.position) {
    this.endCallback = endCallback;
    this.reset();

    this.lastProcessPlayTimestamp = performance.now();
    if (this.skipSilence) {
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

  for (let samplesRemaining = bufferSize * 2;
       samplesRemaining > 0;
       samplesRemaining -= batchSize) {
    if (batchSize > samplesRemaining) batchSize = samplesRemaining;

    if (!this.paused) {
      let pos = this.position;
      for (this.elapsedTime += msPerBatch;
           this.events[pos] && this.elapsedTime >= this.events[pos].playTime;
           pos++) {
        event = this.events[pos];
        switch (event.subtype) {
          case MIDIEvents.EVENT_MIDI_NOTE_ON:
            if (!this.channelMask[event.channel]) break;
            if (event.param2 === 0) // velocity
              synth.noteOff(event.channel, event.param1);
            else
              synth.noteOn(event.channel, event.param1, event.param2);
            break;
          case MIDIEvents.EVENT_MIDI_NOTE_OFF:
            synth.noteOff(event.channel, event.param1);
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
  let event = this.events[this.position];
  while (this.events[this.position] && event.playTime < this.elapsedTime + BUFFER_AHEAD) {
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
        case MIDIEvents.EVENT_MIDI_NOTE_ON:
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
      const scaledPlayTime = (event.playTime - this.elapsedTime) / this.speed + this.lastProcessPlayTimestamp;
      this.send(message, scaledPlayTime + throttleDelayMs);
      throttleDelayMs += delay;
      this.lastSendTimestamp = scaledPlayTime;
    }
    this.position++;
    event = this.events[this.position];
  }

  if (this.position >= this.events.length) {
    setTimeout(this.endCallback, BUFFER_AHEAD + 100);
    this.position = 0;
    this.paused = true;
  }
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
    // Release sustain pedal on all channels
    for (let ch = 0; ch < 16; ch++) {
      this.synth.controlChange(ch, CC_SUSTAIN_PEDAL, 0);
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

MIDIPlayer.prototype.setPosition = function (ms) {
  if (ms < 0 || ms > this.getDuration()) return;

  this.lastProcessPlayTimestamp = performance.now();
  this.panic(this.lastSendTimestamp + 10);
  let eventMap = {};
  let eventList = [];
  let pos = this.position;

  if (ms < this.elapsedTime) {
    pos = 0;
  }

  while (this.events[pos] && this.events[pos].playTime < ms) {
    const event = this.events[pos];
    if (event.subtype === MIDIEvents.EVENT_MIDI_PROGRAM_CHANGE) {
      this.handleProgramChange(event.channel, event.param1);
      eventMap[`${event.subtype}-${event.channel}`] = event;
    } else if (event.subtype === MIDIEvents.EVENT_MIDI_CONTROLLER) {
      // These controllers (RPN, NRPN, Data Entry) must be sequenced in order
      if (SEQUENCED_CONTROLLERS.includes(event.param1)) {
        // console.log('Sequenced event: ch %d -- %d - %d -- %d ms', event.channel, event.param1, event.param2, event.playTime);
        eventList.push(event);
      } else {
        // All others, we only care about the last event
        eventMap[`${event.subtype}-${event.channel}-${event.param1}`] = event;
      }
    }
    pos++;
  }

  eventList = Object.values(eventMap).concat(eventList);

  if (this.useWebMIDI) {
    this.setPositionWebMidi(ms, eventList);
  } else {
    this.setPositionSynth(eventList);
  }

  this.elapsedTime = ms;
  this.position = pos;
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
        channelsInUse[event.channel] = 1;
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
