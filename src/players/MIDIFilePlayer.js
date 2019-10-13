var MIDIEvents = require('midievents');

// Constants
var PLAY_BUFFER_DELAY = 33;
var PAGE_HIDDEN_BUFFER_DELAY = 6000;
const DELAY_MS_PER_CC_EVENT = 2;
const DELAY_MS_PER_SYSEX_EVENT = 2;
const DELAY_MS_PER_XG_SYSTEM_EVENT = 500;

// MIDIPlayer constructor
function MIDIPlayer(options) {
  var i;

  options = options || {};
  this.output = options.output || null; // midi output
  this.volume = options.volume || 100; // volume in percents
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
}

// Parsing all tracks and add their events in a single event queue
MIDIPlayer.prototype.load = function(midiFile) {
  this.stop();
  this.position = 0;
  this.midiFile = midiFile;
  this.events = this.midiFile.getEvents();
};

MIDIPlayer.prototype.play = function(endCallback) {
  this.endCallback = endCallback;
  if(0 === this.position) {
    // All Sound Off
    this.output.send([ MIDIEvents.EVENT_MIDI_CONTROLLER << 4, 120, 0 ]);
    // Reset All Controllers
    this.output.send([ MIDIEvents.EVENT_MIDI_CONTROLLER << 4, 121, 0 ]);
    // XG All Parameter Reset
    // this.output.send([ 0xF0, 0x43, 0x10, 0x4C, 0x00, 0x00, 0x7F, 0x00, 0xF7 ]);

    this.startTime = performance.now();
    let firstNoteDelay = 0;
    let messageDelay = 0;
    let numSysexEvents = 0;
    if (this.skipSilence) {
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
          try {
            this.output.send(message, this.startTime + messageDelay);
          } catch (e) {
            console.warn(e);
          }
        }

        // Set startTime to a point in the past so that the first note event plays immediately.
        console.log("Time to first note at %s ms was cut by %s ms (setup time %s ms; %s sysex events)",
          Math.round(firstNote.playTime), Math.round(firstNote.playTime - firstNoteDelay), firstNoteDelay, numSysexEvents);
        this.startTime = (this.startTime + firstNoteDelay) - firstNote.playTime;
      }
    }

    this.timeout = setTimeout(this.processPlay.bind(this), 0);
    return 1;
  }
  return 0;
};

MIDIPlayer.prototype.processPlay = function() {
  var elapsedTime = performance.now() - this.startTime;
  var event;
  var index;
  var param2;
  var bufferDelay = (
    document.hidden || document.mozHidden || document.webkitHidden ||
    document.msHidden || document.oHidden ?
      PAGE_HIDDEN_BUFFER_DELAY :
      PLAY_BUFFER_DELAY
  );
  event = this.events[this.position];
  while(this.events[this.position] && event.playTime - elapsedTime < bufferDelay) {
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
    if (message)
      this.output.send(message, Math.max(0, Math.floor(event.playTime + this.startTime)));

    this.lastPlayTime = event.playTime + this.startTime;
    this.position++;
    event = this.events[this.position];
  }
  if(this.position < this.events.length - 1) {
    this.timeout = setTimeout(this.processPlay.bind(this), PLAY_BUFFER_DELAY - 250);
  } else {
    setTimeout(this.endCallback, PLAY_BUFFER_DELAY + 100);
    this.position = 0;
  }
};

MIDIPlayer.prototype.pause = function() {
  var i;
  var j;

  if(this.timeout) {
    clearTimeout(this.timeout);
    this.timeout = null;
    this.pauseTime = performance.now();
    // All sound off
    this.output.send([ MIDIEvents.EVENT_MIDI_CONTROLLER << 4, 120, 0 ]);
    for(i = this.notesOn.length - 1; 0 <= i; i--) {
      for(j = this.notesOn[i].length - 1; 0 <= j; j--) {
        this.output.send([(MIDIEvents.EVENT_MIDI_NOTE_OFF << 4) + i, this.notesOn[i][j],
          0x00], this.lastPlayTime + 100);
      }
    }
    return true;
  }
  return false;
};

MIDIPlayer.prototype.resume = function(endCallback) {
  this.endCallback = endCallback;
  if(this.events && this.events[this.position] && !this.timeout) {
    this.startTime += performance.now() - this.pauseTime;
    this.timeout = setTimeout(this.processPlay.bind(this), 0);
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

// module.exports = MIDIPlayer;
export default MIDIPlayer;
