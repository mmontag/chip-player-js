const MIDIEvents = require('midievents');

// Constants
const BUFFER_AHEAD = 33;
const DELAY_MS_PER_CC_EVENT = 2;
const DELAY_MS_PER_SYSEX_EVENT = 15;
const DELAY_MS_PER_XG_SYSTEM_EVENT = 500;

const CC_SUSTAIN_PEDAL = 64;
const CC_ALL_SOUND_OFF = 120;
const CC_RESET_ALL_CONTROLLERS = 121;

function printSysex(data) {
  return ('[F0 ' + data.map(n => ('0' + n.toString(16)).slice(-2)).join(' ') + ']').toUpperCase();
}

// MIDIPlayer constructor
function MIDIPlayer(options) {
  options = options || {};
  this.output = options.output || null; // Web MIDI output device (has a .send() method)
  this.synth = options.synth || null; // MIDI synth (has .noteOn(), .noteOff(), render()...)
  this.speed = 1;
  this.skipSilence = options.skipSilence || false; // skip silence at beginning of file
  this.lastProcessPlayTimestamp = 0;
  this.lastSendTimestamp = 0;
  this.events = [];
  this.paused = true;
  this.useWebMIDI = false;

  this.channelsInUse = [];
  this.channelsMuted = [];
  this.channelProgramNums = [];

  this.doSkipSilence = this.doSkipSilence.bind(this);
  this.play = this.play.bind(this);
  this.processPlay = this.processPlay.bind(this);
  this.processPlaySynth = this.processPlaySynth.bind(this);
  this.getChannelInUse = this.getChannelInUse.bind(this);
  this.getChannelProgramNum = this.getChannelProgramNum.bind(this);
  this.getChannelsInUseAndInitialPrograms = this.getChannelsInUseAndInitialPrograms.bind(this);
  this.getDuration = this.getDuration.bind(this);
  this.getPosition = this.getPosition.bind(this);
  this.panic = this.panic.bind(this);
  this.reset = this.reset.bind(this);
  this.resume = this.resume.bind(this);
  this.send = this.send.bind(this);
  this.stop = this.stop.bind(this);
  this.setChannelMute = this.setChannelMute.bind(this);
  this.setOutput = this.setOutput.bind(this);
  this.setPosition = this.setPosition.bind(this);
  this.setPositionSynth = this.setPositionSynth.bind(this);
  this.setPositionWebMidi = this.setPositionWebMidi.bind(this);
  this.setUseWebMIDI = this.setUseWebMIDI.bind(this);
  this.setSpeed = this.setSpeed.bind(this);
  this.togglePause = this.togglePause.bind(this);

  window.addEventListener('unload', this.stop);
}

// Parsing all tracks and add their events in a single event queue
MIDIPlayer.prototype.load = function(midiFile) {
  this.stop();
  this.position = 0;
  this.elapsedTime = 0;
  this.events = midiFile.getEvents();
  this.getChannelsInUseAndInitialPrograms();
};

MIDIPlayer.prototype.doSkipSilence = function() {
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
            firstNoteDelay += DELAY_MS_PER_SYSEX_EVENT;
            messageDelay += DELAY_MS_PER_SYSEX_EVENT;
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

      eventList.forEach(({message, timestamp}) => {
        this.send(message, timestamp);
      });
      // Set lastProcessPlayTimestamp to a point in the past so that the first note event plays immediately.
      console.log("Time to first note %s ms was updated to %s ms; %s sysex events",
        Math.round(firstNote.playTime), firstNoteDelay, numSysexEvents);
      this.lastProcessPlayTimestamp += (firstNoteDelay - firstNote.playTime);
    } else {
      this.setPosition(firstNote.playTime - 10);
    }
  }
};

MIDIPlayer.prototype.play = function(endCallback) {
  this.endCallback = endCallback;
  if(0 === this.position) {
    this.panic();

    this.lastProcessPlayTimestamp = performance.now();
    if (this.skipSilence) {
      this.doSkipSilence();
    }

    this.resume();
    return 1;
  }
  return 0;
};

MIDIPlayer.prototype.processPlaySynth = function(buffer, bufferSize) {
  this.lastProcessPlayTimestamp = performance.now();

  let bytesWritten = 0;
  let batchSize = 64;
  let event = null;
  const synth = this.synth;
  const msPerBatch = this.speed * 1000 * (batchSize / 44100) / 2;

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
            if (this.channelsMuted[event.channel]) break;
            if (event.param2 === 0) // velocity
              synth.noteOff(event.channel, event.param1);
            else
              synth.noteOn(event.channel, event.param1, event.param2);
            break;
          case MIDIEvents.EVENT_MIDI_NOTE_OFF:
            synth.noteOff(event.channel, event.param1);
            break;
          case MIDIEvents.EVENT_MIDI_PROGRAM_CHANGE:
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
    const threshold = 0.05;
    for (let i = 0; i < bufferSize; i++) {
      if (buffer[i * 2] > threshold) { // Check left channel only
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

MIDIPlayer.prototype.processPlay = function() {

  const now = performance.now();
  const deltaTime = (now - this.lastProcessPlayTimestamp) * this.speed;
  this.lastProcessPlayTimestamp = now;

  if (this.paused) return;

  let throttleDelayMs = 0;
  let delay = 0;
  let message = null;

  this.elapsedTime += deltaTime;
  let event = this.events[this.position];
  while(this.events[this.position] && event.playTime < this.elapsedTime + BUFFER_AHEAD) {
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
        case MIDIEvents.EVENT_MIDI_CHANNEL_AFTERTOUCH:
          message = [(event.subtype << 4) + event.channel, event.param1];
          break;
        case MIDIEvents.EVENT_MIDI_NOTE_OFF:
        case MIDIEvents.EVENT_MIDI_NOTE_ON:
        case MIDIEvents.EVENT_MIDI_NOTE_AFTERTOUCH:
        case MIDIEvents.EVENT_MIDI_CONTROLLER:
        case MIDIEvents.EVENT_MIDI_PITCH_BEND:
          if (this.channelsMuted[event.channel]) break;
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

  if(this.position >= this.events.length)  {
    setTimeout(this.endCallback, BUFFER_AHEAD + 100);
    this.position = 0;
    this.paused = true;
  }
};

MIDIPlayer.prototype.togglePause = function() {
  this.paused = !this.paused;
  if (this.paused === true) {
    this.panic(this.lastSendTimestamp + 10);
  }
  return this.paused;
};

MIDIPlayer.prototype.resume = function() {
  this.paused = false;
};

MIDIPlayer.prototype.stop = function() {
  this.paused = true;
  this.panic();
};

MIDIPlayer.prototype.send = function(message, timestamp) {
  try {
    this.output.send(message, timestamp);
  } catch (e) {
    console.warn(e);
    console.warn(message);
  }
};

// TODO: fix confusion between reset and panic
MIDIPlayer.prototype.panic = function(timestamp) {
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

MIDIPlayer.prototype.reset = function(timestamp) {
  if (this.synth) {
    this.synth.reset();
  } else {
    for (let ch = 0; ch < 16; ch++) {
      this.send([(MIDIEvents.EVENT_MIDI_CONTROLLER << 4) + ch, CC_SUSTAIN_PEDAL, 0], timestamp);
      this.send([(MIDIEvents.EVENT_MIDI_CONTROLLER << 4) + ch, CC_ALL_SOUND_OFF, 0], timestamp);
      this.send([(MIDIEvents.EVENT_MIDI_CONTROLLER << 4) + ch, CC_RESET_ALL_CONTROLLERS, 0], timestamp);
    }
  }
};

// --- Chip Player JS support ---

MIDIPlayer.prototype.getDuration = function() {
  if (this.events && this.events.length > 0) {
    return this.events[this.events.length - 1].playTime;
  }
  return 0;
};

MIDIPlayer.prototype.getPosition = function() {
  return this.elapsedTime;
};

MIDIPlayer.prototype.setOutput = function(output) {
  this.panic(this.lastSendTimestamp + 10);
  this.output = output;
};

MIDIPlayer.prototype.setSpeed = function (speed) {
  this.speed = Math.max(0.1, Math.min(10, speed));
};

MIDIPlayer.prototype.setPositionSynth = function(eventMap) {
  const synth = this.synth;
  Object.values(eventMap).forEach(event => {
    switch (event.subtype) {
      case MIDIEvents.EVENT_MIDI_PROGRAM_CHANGE:
        synth.programChange(event.channel, event.param1);
        break;
      case MIDIEvents.EVENT_MIDI_CONTROLLER:
      default:
        synth.controlChange(event.channel, event.param1, event.param2);
        break;
    }
  });
};

MIDIPlayer.prototype.setPositionWebMidi = function(ms, eventMap) {
  this.paused = true;

  let message;
  Object.values(eventMap).forEach((event, i) => {
    if (event.subtype === MIDIEvents.EVENT_MIDI_PROGRAM_CHANGE) {
      message = [(event.subtype << 4) + event.channel, event.param1];
    } else if (event.subtype === MIDIEvents.EVENT_MIDI_CONTROLLER) {
      message = [(event.subtype << 4) + event.channel, event.param1, event.param2];
    }
    this.send(message, this.lastSendTimestamp + 20 + i * DELAY_MS_PER_CC_EVENT);
  });

  const numEvents = Object.values(eventMap).length;
  const messageDelay = numEvents * DELAY_MS_PER_CC_EVENT;
  console.log("Scheduled %s events. Resuming playback in %s ms...", numEvents, messageDelay);
  setTimeout(this.resume, messageDelay);
};

MIDIPlayer.prototype.setPosition = function(ms) {
  if (ms < 0 || ms > this.getDuration()) return;

  this.lastProcessPlayTimestamp = performance.now();
  this.panic(this.lastSendTimestamp + 10);
  let eventMap = {};
  let pos = this.position;

  if (ms < this.elapsedTime) {
    pos = 0;
  }

  while (this.events[pos] && this.events[pos].playTime < ms) {
    const event = this.events[pos];
    if (event.subtype === MIDIEvents.EVENT_MIDI_PROGRAM_CHANGE) {
      eventMap[`${event.subtype}-${event.channel}`] = event;
    } else if (event.subtype === MIDIEvents.EVENT_MIDI_CONTROLLER) {
      eventMap[`${event.subtype}-${event.channel}-${event.param1}`] = event;
    }
    pos++;
  }

  if (this.useWebMIDI) {
    this.setPositionWebMidi(ms, eventMap);
  } else {
    this.setPositionSynth(eventMap);
  }

  this.elapsedTime = ms;
  this.position = pos;
};

MIDIPlayer.prototype.getChannelInUse = function(ch) {
  return !!this.channelsInUse[ch];
};

MIDIPlayer.prototype.getChannelProgramNum = function(ch) {
  return this.channelProgramNums[ch];
};

MIDIPlayer.prototype.getChannelsInUseAndInitialPrograms = function() {
  const channelsInUse = this.channelsInUse;
  const channelProgramNums = this.channelProgramNums;
  const channelsMuted = this.channelsMuted;
  for (let i = 0; i < 16; i++) {
    channelsInUse[i] = 0;
    channelProgramNums[i] = 0;
    channelsMuted[i] = 0;
  }

  for (let j = 0; j < this.events.length; j++) {
    const event = this.events[j];
    if (event.subtype === MIDIEvents.EVENT_MIDI_NOTE_ON) {
      channelsInUse[event.channel] = 1;
    }
    if (event.subtype === MIDIEvents.EVENT_MIDI_PROGRAM_CHANGE && !channelProgramNums[event.channel]) {
      channelProgramNums[event.channel] = event.param1;
    }
  }
};

MIDIPlayer.prototype.setChannelMute = function(ch, isMuted) {
  this.channelsMuted[ch] = isMuted;
  if (isMuted) {
    // TODO separate synth from webMidi
    const timestamp = this.lastSendTimestamp + 10;
    this.send([(MIDIEvents.EVENT_MIDI_CONTROLLER << 4) + ch, CC_SUSTAIN_PEDAL, 0], timestamp);
    this.send([(MIDIEvents.EVENT_MIDI_CONTROLLER << 4) + ch, CC_ALL_SOUND_OFF, 0], timestamp);
    this.synth.panicChannel(ch);
  }
};

MIDIPlayer.prototype.setUseWebMIDI = function(useWebMIDI) {
  this.useWebMIDI = useWebMIDI;

};

export default MIDIPlayer;
