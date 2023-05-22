import { EVENT_MIDI, EVENT_MIDI_CONTROLLER } from 'midievents';
import MIDIEvents from 'midievents';
import MIDIFile from 'midifile';

const CC_102_TRACK_LOOP_START = 102;
const CC_103_TRACK_LOOP_END = 103;
const CC_123_ALL_NOTES_OFF = 123;

class EventIterator {
  constructor(events) {
    this.events = events;
    this.curEvent = events[0];
    this.pos = 0;
    this.loopStartPos = null;
    this.elapsedLoops = 0;
    this.curTick = 0;
  }

  loop() {
    // Sanity check
    if (this.curTick < 1000000 && this.elapsedLoops < 1000) {
      this.pos = this.loopStartPos;
      console.debug('Channel %d looped at tick %d (loop %d completed)', this.curEvent.channel, this.curTick, this.elapsedLoops);
      this.elapsedLoops++;
    }
  }

  next() {
    const event = this.events[this.pos++];
    if (!event) return null;

    this.curTick += event.delta;

    if (event.type === EVENT_MIDI && event.subtype === EVENT_MIDI_CONTROLLER) {
      if (event.param1 === CC_102_TRACK_LOOP_START) {
        // console.log('loop start ch %d id %d', event.channel, event.param2);
        this.loopStartPos = this.pos;
      }
      if (event.param1 === CC_103_TRACK_LOOP_END) {
        // console.log('loop end  ch %d id %d', event.channel, event.param2);
        this.loop();
      }
    }

    // Return a copy because track consolidation will mutate the events.
    return { ...event };
  }
}

// Monkey patch MIDIFile class.
MIDIFile.prototype.getLoopedEvents = function (tracks, loopCount = 1) {
  let event;
  let playTime = 0;
  const combinedEvents = [];
  const format = this.header.getFormat();
  let tickResolution = this.header.getTickResolution();
  let i;
  let j;
  let smallestDelta;
  let loopCountReached = false;
  const type = null;
  const subtype = null;

  // Reading events
  // if the read is sequential
  if (1 !== format || 1 === this.tracks.length) {
    for (i = 0, j = this.tracks.length; i < j; i++) {
      // reset playtime if format is 2
      playTime = 2 === format && playTime ? playTime : 0;
      const eventIterator = new EventIterator(tracks[i]);
      // loooping through events
      event = eventIterator.next();
      while (event) {
        playTime += event.delta ? event.delta * tickResolution / 1000 : 0;
        if (event.type === MIDIEvents.EVENT_META) {
          // tempo change events
          if (event.subtype === MIDIEvents.EVENT_META_SET_TEMPO) {
            tickResolution = this.header.getTickResolution(event.tempo);
          }
        }
        // push the asked events
        if (
          (!type || event.type === type) &&
          (!subtype || (event.subtype && event.subtype === subtype))
        ) {
          event.playTime = playTime;
          combinedEvents.push(event);
        }
        event = eventIterator.next();
      }
    }
    // the read is concurrent
  } else {
    smallestDelta = -1;

    const trackIterators = [];

    // Creating iterators
    for (i = 0, j = tracks.length; i < j; i++) {
      printTrack(i, tracks[i]);
      trackIterators[i] = new EventIterator(tracks[i]);
      trackIterators[i].curEvent = trackIterators[i].next();
    }
    // Filling events
    do {
      smallestDelta = -1;
      // Find the shortest event
      for (i = 0, j = trackIterators.length; i < j; i++) {
        if (trackIterators[i].curEvent) {
          if (
            -1 === smallestDelta ||
            trackIterators[i].curEvent.delta <
            trackIterators[smallestDelta].curEvent.delta
          ) {
            smallestDelta = i;
          } else if (
            // Prioritize tracks that haven't caught up with the loop count
            trackIterators[i].curEvent.delta ===
            trackIterators[smallestDelta].curEvent.delta &&
            trackIterators[i].elapsedLoops <
            trackIterators[smallestDelta].elapsedLoops
          ) {
            smallestDelta = i;
          }
        }
      }
      if (-1 !== smallestDelta) {
        // Subtract delta of previous events
        for (i = 0, j = trackIterators.length; i < j; i++) {
          if (i !== smallestDelta && trackIterators[i].curEvent) {
            trackIterators[i].curEvent.delta -=
              trackIterators[smallestDelta].curEvent.delta;
          }
        }
        // filling values
        event = trackIterators[smallestDelta].curEvent;
        playTime += event.delta ? event.delta * tickResolution / 1000 : 0;
        if (event.type === MIDIEvents.EVENT_META) {
          // tempo change events
          if (event.subtype === MIDIEvents.EVENT_META_SET_TEMPO) {
            tickResolution = this.header.getTickResolution(event.tempo);
          }
        }
        // push midi events
        if (
          (!type || event.type === type) &&
          (!subtype || (event.subtype && event.subtype === subtype))
        ) {
          event.playTime = playTime;
          event.track = smallestDelta;
          combinedEvents.push(event);
        }
        // get next event
        trackIterators[smallestDelta].curEvent = trackIterators[smallestDelta].next();

        // check elapsed loops
        loopCountReached = trackIterators.every(it => it.curEvent == null || it.elapsedLoops >= loopCount);
      }
    } while (-1 !== smallestDelta && loopCountReached === false);

    for (let i = 0; i < trackIterators.length; i++) {
      if (trackIterators[i]?.curEvent) {
        combinedEvents.push(allNotesOff(i, trackIterators[i].curEvent.channel, playTime));
      }
    }
  }
  console.debug(combinedEvents);
  return combinedEvents;
};

function isLoopStart(event) {
  return (
    event.param1 === CC_102_TRACK_LOOP_START &&
    event.subtype === EVENT_MIDI_CONTROLLER &&
    event.type === EVENT_MIDI
  );
}

function isLoopEnd(event) {
  return (
    event.param1 === CC_103_TRACK_LOOP_END &&
    event.subtype === EVENT_MIDI_CONTROLLER &&
    event.type === EVENT_MIDI
  );
}

function isAllNotesOff(event) {
  return (
    event.param1 === CC_123_ALL_NOTES_OFF &&
    event.subtype === EVENT_MIDI_CONTROLLER &&
    event.type === EVENT_MIDI
  );
}

function allNotesOff(track, channel, playTime) {
  return {
    channel: channel,
    track: track,
    playTime: playTime,
    type: EVENT_MIDI,
    subtype: EVENT_MIDI_CONTROLLER,
    param1: CC_123_ALL_NOTES_OFF,
    delta: 0,
  };
}

function printTrack(t, events) {
  const ticksPerChar = 1000;
  let charArr = [];
  let tick = 0;
  for (let i = 0; i < events.length; i++) {
    const event = events[i];
    tick += event.delta;
    const j = Math.floor(tick / ticksPerChar);
    if (isAllNotesOff(event)) {
      charArr[j] = 'X';
      tick += ticksPerChar; // force next char so this doesn't get overwritten
    } else if (isLoopStart(event)) {
      charArr[j] = '>';
    } else if (isLoopEnd(event)) {
      charArr[j] = '<';
    } else if (charArr[j] == null) {
      charArr[j] = '·';
    } else if (charArr[j] === '·') {
      charArr[j] = '-';
    } else if (charArr[j] === '-') {
      charArr[j] = '=';
    }
  }
  t = (''+t).padStart(2, '0');
  const viz = [];
  for (let k = 0; k < charArr.length; k++) {
    viz[k] = charArr[k] || ' ';
  }
  console.debug(`Track ${t} |${viz.join('')}|`);
}
