var MIDIEvents = require('midievents');

// Constants
var BUFFER_AHEAD = 33;
var PAGE_HIDDEN_BUFFER_AHEAD = 6000;
const DELAY_MS_PER_CC_EVENT = 2;
const DELAY_MS_PER_SYSEX_EVENT = 2;
const DELAY_MS_PER_XG_SYSTEM_EVENT = 500;

const CC_ALL_SOUND_OFF = 120;
const CC_RESET_ALL_CONTROLLERS = 121;

// MIDIPlayer constructor
function MIDIPlayer(options) {
  var i;

  options = options || {};
  this.output = options.output || null; // midi output
  this.volume = options.volume || 100; // volume in percents
  this.speed = 1;
  this.skipSilence = options.skipSilence || false; // skip silence at beginning of file
  this.startTime = -1; // ms since page load
  this.pauseTime = -1; // ms elapsed before player paused
  this.events = [];
  this.notesOn = new Array(32); // notesOn[channel][note]
  for(i = 31; 0 <= i; i--) {
    this.notesOn[i] = [];
  }
  this.midiFile = null;
  window.addEventListener('unload', this.stop.bind(this));

  this.doSkipSilence = this.doSkipSilence.bind(this);
  this.play = this.play.bind(this);
  this.processPlay = this.processPlay.bind(this);
  this.getDuration = this.getDuration.bind(this);
  this.getPosition = this.getPosition.bind(this);
  this.reset = this.reset.bind(this);
  this.send = this.send.bind(this);
  this.setOutput = this.setOutput.bind(this);
  this.setPosition = this.setPosition.bind(this);
  this.setSpeed = this.setSpeed.bind(this);
}

// Parsing all tracks and add their events in a single event queue
MIDIPlayer.prototype.load = function(midiFile) {
  this.stop();
  this.position = 0;
  this.elapsedTime = 0;
  this.midiFile = midiFile;
  this.events = this.midiFile.getEvents();
};

MIDIPlayer.prototype.doSkipSilence = function() {
  let firstNoteDelay = 0;
  let messageDelay = 0;
  let numSysexEvents = 0;
  const firstNote = this.events.find(e => e.subtype === MIDIEvents.EVENT_MIDI_NOTE_ON);
  if (firstNote && firstNote.playTime > 0) {
    // Accelerated playback of all events prior to first note
    var message;
    while (this.events[this.position] !== firstNote) {
      const event = this.events[this.position];
      this.position++;

      // console.log("MIDI Event", event);
      if (event.type === MIDIEvents.EVENT_SYSEX) {
        console.log("sysex event:", event);
        if (event.data && event.data[3] === 0 && event.data[4] === 0) {
          firstNoteDelay += DELAY_MS_PER_XG_SYSTEM_EVENT;
        } else {
          firstNoteDelay += DELAY_MS_PER_SYSEX_EVENT;
        }
        numSysexEvents++;
        message = [MIDIEvents.EVENT_SYSEX, ...event.data];
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
      this.send(message, this.startTime + messageDelay);
    }

    // Set startTime to a point in the past so that the first note event plays immediately.
    console.log("Time to first note at %s ms was cut by %s ms (setup time %s ms; %s sysex events)",
      Math.round(firstNote.playTime), Math.round(firstNote.playTime - firstNoteDelay), firstNoteDelay, numSysexEvents);
    this.startTime = this.lastProcessPlayTimestamp = (this.startTime + firstNoteDelay) - firstNote.playTime;
  }
};

MIDIPlayer.prototype.play = function(endCallback) {
  this.endCallback = endCallback;
  if(0 === this.position) {
    this.reset();
    this.startTime = this.lastProcessPlayTimestamp = performance.now();
    if (this.skipSilence) {
      this.doSkipSilence();
    }

    this.timeout = setTimeout(this.processPlay, 0);
    this.resume();
    return 1;
  }
  return 0;
};

MIDIPlayer.prototype.processPlay = function() {
  this.timeout = setTimeout(this.processPlay, BUFFER_AHEAD);

  const now = performance.now();
  const deltaTime = (now - this.lastProcessPlayTimestamp) * this.speed;
  this.lastProcessPlayTimestamp = now;

  this.elapsedTime += deltaTime;
  var event;
  var index;
  var param2;
  var bufferAhead = (
    document.hidden || document.mozHidden || document.webkitHidden ||
    document.msHidden || document.oHidden ?
      PAGE_HIDDEN_BUFFER_AHEAD:
      BUFFER_AHEAD
  );
  event = this.events[this.position];
  while(this.events[this.position] && event.playTime < this.elapsedTime + bufferAhead) {
    param2 = 0;
    if(event.subtype === MIDIEvents.EVENT_MIDI_NOTE_ON) {
      param2 = Math.floor(event.param2 * ((this.volume || 1) / 100));
      this.notesOn[event.channel].push(event.param1);
    } else if(event.subtype === MIDIEvents.EVENT_MIDI_NOTE_OFF) {
      index = this.notesOn[event.channel].indexOf(event.param1);
      if(-1 !== index) {
        this.notesOn[event.channel].splice(index, 1);
      }
    }

    var message = null;
    if (event.subtype === MIDIEvents.EVENT_SYSEX) {
      message = [MIDIEvents.EVENT_SYSEX, ...event.data];
    } else if (MIDIEvents.MIDI_1PARAM_EVENTS.indexOf(event.subtype) !== -1) {
      message = [(event.subtype << 4) + event.channel, event.param1];
    } else if (MIDIEvents.MIDI_2PARAMS_EVENTS.indexOf(event.subtype) !== -1) {
      message = [(event.subtype << 4) + event.channel, event.param1, (param2 || event.param2 || 0x00)];
    }
    if (message) {
      const scaledPlayTime = (event.playTime - this.elapsedTime) / this.speed + this.lastProcessPlayTimestamp;
      this.send(message, scaledPlayTime); //, Math.max(0, Math.floor(event.playTime + this.startTime)));
      this.lastPlayTime = scaledPlayTime;
    }

    this.position++;
    event = this.events[this.position];
  }

  if(this.position < this.events.length - 1) {
    this.timeout = setTimeout(this.processPlay, bufferAhead);
  }
  if(this.position >= this.events.length)  {
    setTimeout(this.endCallback, BUFFER_AHEAD + 100);
    this.position = 0;
  }
};

MIDIPlayer.prototype.pause = function() {
  if(this.timeout) {
    clearTimeout(this.timeout);
    this.timeout = null;
    this.pauseTime = performance.now();
    this.reset(this.lastPlayTime + 10);
    return true;
  }
  return false;
};

MIDIPlayer.prototype.resume = function(endCallback) {
  this.endCallback = endCallback;
  if(this.events && this.events[this.position] && !this.timeout) {
    this.startTime = this.startTime + (performance.now() - this.pauseTime) / this.speed;
    this.lastProcessPlayTimestamp = performance.now();
    this.timeout = setTimeout(this.processPlay, 0);
    return this.events[this.position].playTime;
  }
  return 0;
};

MIDIPlayer.prototype.stop = function() {
  var i;

  if(this.pause()) {
    this.position = 0;
    for(i = 31; 0 <= i; i--) {
      this.notesOn[i] = [];
    }
    return true;
  }
  return false;
};

MIDIPlayer.prototype.send = function(message, timestamp) {
  try {
    this.output.send(message, timestamp);
  } catch (e) {
    console.warn(e);
    console.warn(message);
  }
};

MIDIPlayer.prototype.reset = function(timestamp) {
  for(let ch = 0; ch < 16; ch++) {
    this.output.send([(MIDIEvents.EVENT_MIDI_CONTROLLER << 4) + ch, CC_ALL_SOUND_OFF, 0 ], timestamp);
    this.output.send([(MIDIEvents.EVENT_MIDI_CONTROLLER << 4) + ch, CC_RESET_ALL_CONTROLLERS, 0 ], timestamp);
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
  if (this.timeout) {
    this.reset(this.lastPlayTime + 10);
  }
  this.output = output;
};

MIDIPlayer.prototype.setSpeed = function (speed) {
  this.speed = Math.max(0.1, Math.min(10, speed));
};

MIDIPlayer.prototype.setPosition = function(ms) {
  if (ms < 0 || ms > this.getDuration()) return;

  this.reset(this.lastPlayTime + 10);

  let eventKey = null;
  let eventValue = null;
  let eventMap = {};
  let pos = this.position;

  if (ms < this.elapsedTime) {
    pos = 0;
  }

  while (this.events[pos] && this.events[pos].playTime < ms) {
    const event = this.events[pos];
    if (event.subtype === MIDIEvents.EVENT_MIDI_PROGRAM_CHANGE || event.subtype === MIDIEvents.EVENT_MIDI_CONTROLLER) {
      if (event.subtype === MIDIEvents.EVENT_MIDI_PROGRAM_CHANGE) {
        eventKey = `${event.subtype}-${event.channel}`;
        eventValue = [(event.subtype << 4) + event.channel, event.param1];
      }
      if (event.subtype === MIDIEvents.EVENT_MIDI_CONTROLLER) {
        eventKey = `${event.subtype}-${event.channel}-${event.param1}`;
        eventValue = [(event.subtype << 4) + event.channel, event.param1, event.param2];
      }
      eventMap[eventKey] = eventValue;
    }
    pos++;
  }

  Object.values(eventMap).forEach((message, i) => {
    this.send(message, this.lastPlayTime + 20 + i * DELAY_MS_PER_CC_EVENT);
  });

  this.elapsedTime = ms;
  this.position = pos;
  this.startTime = performance.now() - ms;

  const numEvents = Object.values(eventMap).length;
  const messageDelay = numEvents * DELAY_MS_PER_CC_EVENT;
  if (this.timeout) {
    clearTimeout(this.timeout);
    console.log("Scheduled %s events. Resuming playback in %s ms...", numEvents, messageDelay);
    this.timeout = setTimeout(this.processPlay, messageDelay);
  }
};

// module.exports = MIDIPlayer;
export default MIDIPlayer;
