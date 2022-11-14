const {
  EVENT_MIDI,
  EVENT_MIDI_CONTROLLER,
  EVENT_MIDI_NOTE_ON,
  EVENT_MIDI_NOTE_OFF,
  EVENT_META_END_OF_TRACK,
  EVENT_META,
  EVENT_META_MARKER,
} = require('midievents');
const MIDIFile = require('midifile');
const fs = require('fs');
const glob = require('glob');
const path = require('path');

const CC_102_TRACK_LOOP_START = 102;
const CC_103_TRACK_LOOP_END = 103;
const CC_123_ALL_NOTES_OFF = 123;
const DUMMY_EVENT = {
  type: EVENT_META,
  subtype: EVENT_META_MARKER,
  length: 0,
  delta: 0,
  channel: 0,
}

const DIR = '/Users/montag/Music/perfectdark';


const OUT_DIR = path.join(DIR, 'out');
// Offset: 00000195 - Event Delta Time: 96 -Abs 22848 (00000060)  -   !2573 Duration (B8) - Midi Channel 0 NoteNumber 37 Velocity 115 Duration 184
const genRegex = /^Offset: ([0-9A-F]+) - Event Delta Time: ([0-9]+) -Abs ([0-9]+)/;
// Offset: 000001DC - Event Delta Time: 4608 -Abs 23040 (0000A400) -   FF2DFFFF00000025 Count 255 LoopCount 255 OffsetBeginning 37 (01C1)
const loopRegex = /^Offset: ([0-9A-F]+) - Event Delta Time: ([0-9]+) -Abs ([0-9]+) \([0-9A-F]+\) - {3}[0-9A-F]+ Count 255 LoopCount 255 OffsetBeginning [0-9]+ \(([0-9A-F]+)\)/;
let dryRun = true;
let debugOutputFilenames = true;

/**
 * Input (expected files from N64SoundTool):
 *
 *   Foo 00000001 00A1B2C3.mid
 *   Foo 00000002 00A1B2C4.mid
 *   ...
 *   Foo 00000001 00A1B2C3.mid TrackParseDebug.txt
 *   Foo 00000002 00A1B2C4.mid TrackParseDebug.txt
 *   ...
 *   NS4FooFiles.inl (optional - from NintendoSynthy4 project)
 *     (https://github.com/L-Spiro/Nintendo-Synthy-4/blob/main/NS4/Src/Games/NS4Pilotwings64Files.inl)
 *
 * Output:
 *
 *   out/
 *     01 - First Track Name.mid
 *     02 - Second Track Name.mid
 *     ...
 */

const debugFiles = glob.sync(path.join(DIR, '*TrackParseDebug.txt'));
const midiFiles = debugFiles.map(f => f.replace(' TrackParseDebug.txt', ''));
const inlFile = glob.sync(path.join(DIR, '*.inl'))[0];
const niceTitleList = getNiceTitleListFromInlFile(inlFile);

const bundles = midiFiles
  .map((_, i) => {
    const parts = midiFiles[i].match(/^(.+? ([0-9A-F]{8} [0-9A-F]{8}))?.*\.mid$/);
    const hexId = parts[2];
    if (!hexId) throw Error('Could not get hex ID from MIDI filename ' + midiFiles[i]);
    const fallbackTitle = path.basename(midiFiles[i]);
    const niceTitle = niceTitleList ? getNiceTitleFromHexId(niceTitleList, hexId) : fallbackTitle;
    const outFile = `${debugOutputFilenames ? (hexId + ' ') : ''}${niceTitle}.mid`;
    if (debugOutputFilenames) {
      return {
        debugFile: debugFiles[i],
        inFile: midiFiles[i],
        outFile: outFile,
      };
    } else if (niceTitle) {
      // This will ignore input files that are missing from INL file.
      return {
        debugFile: debugFiles[i],
        inFile: midiFiles[i],
        outFile: outFile,
      };
    }
    return null;
  })
  .filter(o => o != null);
bundles.forEach(bundle => {
  repairN64Midi(bundle.inFile, bundle.outFile, bundle.debugFile);
});

function parseDebugLine(line) {
  let match = line.match(loopRegex);
  if (match) {
    return {
      offset: match[1],
      tick: parseInt(match[3], 10),
      loopStartOffset: match[4].padStart(8, '0'),
      line: line,
    };
  } else {
    match = line.match(genRegex);
    return {
      offset: match[1],
      tick: parseInt(match[3], 10),
      line: line,
    };
  }
}

function getNiceTitleListFromInlFile(inlFile) {
  if (!inlFile) return null;

  const inlText = fs.readFileSync(inlFile).toString();
  const trackNameRegEx = /([0-9A-F]{8} [0-9A-F]{8}?).+TrackParseDebug\.txt", u8"(.+?)",/;
  const hexSet = new Set();
  return inlText
    .split('\n')
    .map((line, i) => {
      const match = line.match(trackNameRegEx);
      // Some songs have duplicate entries in the INL, such as "Sans SFX" versions in Perfect Dark
      if (!match || hexSet.has(match[1])) {
        return null;
      } else {
        hexSet.add(match[1]);
        return {
          inlIdx: i,
          hex: match[1],
          name: match[2].replaceAll(':', ' –'), // Space + En-dash
        };
      }
    })
    .filter(info => info != null)
    .map((info, i, arr) => {
      const digits = Math.floor(Math.log10(arr.length)) + 1;
      const trackNum = (i + 1).toString().padStart(digits, '0');
      return {
        ...info,
        numberedName: `${trackNum} - ${info.name}`,
      };
    })
    // This assumes the midiFiles will be in hex sorted order and match 1:1 with inl titles.
    .sort((a, b) => a.hex.localeCompare(b.hex))
    .map((info, i) => ({
      ...info, midiFile: midiFiles[i],
    }));
}

function getNiceTitleFromHexId(niceTitleList, hexId) {
  // { u8"foo 0000002A 00293EF4.mid", u8"foo 0000002A 00293EF4.mid TrackParseDebug.txt", u8"Nice Title", 0 }
  return niceTitleList.find(info => info.hex === hexId)?.numberedName;
}

function repairN64Midi(midiFilename, outFilename, debugFilename) {
  console.log('Processing %s...', midiFilename);

  const midiData = fs.readFileSync(midiFilename);
  const debugLines = fs.readFileSync(debugFilename).toString().split('\n');

  const midi = new MIDIFile(midiData);
  const tracks = midi.tracks.map((_, i) => midi.getTrackEvents(i)).map(addTickToEvents);
  const loopRanges = parseLoopRanges(debugLines);
  const songLength = getSongLengthTicks(tracks);

  for (let i = 0; i < tracks.length; i++) {
    let fixedEvents = tracks[i];
    // fixedEvents = fixPrematureEndOfTrack(i, fixedEvents, songLength);
    console.log('---- Track %s: %s events', i, fixedEvents.length, loopRanges[i]);
    const loopRange = loopRanges[i];

    if (loopRange) {
      fixedEvents = removeExtraLoopEvents(i, fixedEvents, loopRange);
    }
    fixedEvents = fixOverlappingNotes(i, fixedEvents);
    fixedEvents = fixMissingLoopEnd(i, fixedEvents, songLength);
    fixedEvents = insertAllNotesOff(i, fixedEvents, songLength);

    midi.setTrackEvents(i, fixedEvents);
  }

  const newMidiData = midi.getContent();

  if (!dryRun) {
    if (!fs.existsSync(OUT_DIR)) {
      fs.mkdirSync(OUT_DIR);
    }
    fs.writeFileSync(path.join(OUT_DIR, outFilename), Buffer.from(newMidiData));
  }
  console.log('Wrote %s.', outFilename);
}

function parseLoopRanges(debugLines) {
  // Split debug to tracks
  let offsetToTickMap = {};
  const loopRanges = [];
  let state = 0;
  let trackIdx = -1;
  let debugEvents = [];
  for (let i = 0; i < debugLines.length; i++) {
    const line = debugLines[i].trim();
    // console.log(line);
    if (state === 0 && line === 'Tracks') {
      // console.log('state = 1');
      state = 1;
    } else if (state === 1) {
      if (line.startsWith('Track ')) {
        // Empty tracks in debug text are omitted in MIDI file.
        // if (!debugLines[i + 1].startsWith('No Track Data')) {
        //   trackIdx++;
        //   offsetToTickMap = {};
        // } else {
        //   console.log('%s has no track data', line);
        // }
        offsetToTickMap = {};
        trackIdx = parseInt(line.match(/^Track ([0-9A-F]+)/)[1], 16);
      } else if (line.startsWith('Offset')) {
        const debugEvent = parseDebugLine(line);
        // Offset addresses may be repeated (in unrolled loops); only the first one counts.
        if (offsetToTickMap[debugEvent.offset] == null) {
          offsetToTickMap[debugEvent.offset] = {
            line: i,
            tick: debugEvent.tick,
          };
        }
        if (debugEvent.loopStartOffset != null) {
          if (!offsetToTickMap[debugEvent.loopStartOffset]) {
            continue;
          }
          const loopStartLineIdx = offsetToTickMap[debugEvent.loopStartOffset].line - 1;
          const loopStartLine = debugLines[loopStartLineIdx].trim();
          const loopStartEvent = parseDebugLine(loopStartLine);
          console.log('Track %s loopStartOffset: %s', trackIdx, debugEvent.loopStartOffset);
          loopRanges[trackIdx] = {
            startTick: loopStartEvent.tick,
            endTick: debugEvent.tick,
          }
        }
      }
    }
  }

  console.log(loopRanges);
  return loopRanges;
}

function removeExtraLoopEvents(trackIdx, events, loopRange) {

  const mainLoopStart = events.find(ev => ev.tick === loopRange.startTick && isLoopStart(ev));
  const mainLoopEnd = events.find(ev => ev.tick === loopRange.endTick && isLoopEnd(ev));
  console.log('Track %s loopStart tick: %s, loopEnd tick: %s.', trackIdx, mainLoopStart?.tick, mainLoopEnd?.tick);
  // console.log('Track %d loop start event:', trackIdx, mainLoopStart);
  // console.log('Track %d loop end event:', trackIdx, mainLoopEnd);
  const newEvents = [];
  let numRepairedEvents = 0;
  for (let i = 0; i < events.length; i++) {
    const event = events[i];
    if (event === mainLoopStart || event === mainLoopEnd) {
      newEvents.push(event);
    } else if (isLoopStart(event) || isLoopEnd(event)) {
      // newEvents[newEvents.length - 1].delta += event.delta;
      // newEvents[newEvents.length - 1].tick = event.tick;
      numRepairedEvents++;
      newEvents.push({
        ...DUMMY_EVENT,
        channel: event.channel,
        delta: event.delta,
        tick: event.tick,
      });
    } else {
      newEvents.push(event);
    }
  }
  // const fixedEvents = events.filter((ev, i) => {
  //   if (ev === mainLoopStart || ev === mainLoopEnd) return true;
  //   if (isLoopStart(ev) || isLoopEnd(ev)) {
  //     if (events[i + 1]) {
  //       events[i + 1].delta += ev.delta;
  //     }
  //     return false;
  //   }
  //   return true;
  // });
  console.log('Filtered %d extra loop events in track %d.', numRepairedEvents, trackIdx);
  return newEvents;
}

function addTickToEvents(events) {
  let tick = 0;
  events.forEach(event => {
    tick += event.delta;
    event.tick = tick;
  });
  return events;
}

function getSongLengthTicks(tracks) {
  let lengthByLoopEnd = 0;
  let lengthByEndOfTrack = 0;
  for (let i = 0; i < tracks.length; i++) {
    const events = tracks[i];
    const loopEnd = events.find(ev => isLoopEnd(ev)) || { tick: 0 };
    lengthByLoopEnd = Math.max(lengthByLoopEnd, loopEnd.tick);
    const endTrack = events.find(ev => isEndOfTrack(ev)) || { tick: 0 };
    lengthByEndOfTrack = Math.max(lengthByEndOfTrack, endTrack.tick);
  }
  if (lengthByLoopEnd === 0) {
    console.log('Song length (end of track method) is %s ticks.', lengthByEndOfTrack);
    return lengthByEndOfTrack;
  } else {
    console.log('Song length (loop end method) is %s ticks.', lengthByLoopEnd);
    return lengthByLoopEnd;
  }
}

function fixMissingLoopEnd(trackIdx, events, songLength) {
  const endLoopIdx = events.findIndex(ev => isLoopEnd(ev));

  if (endLoopIdx === -1) {
    const endTrack = events.pop();
    if (!isEndOfTrack(endTrack)) throw Error('Last event in track %s not End of Track!', trackIdx);
    const lastEvent = events.pop();
    let diff = songLength - lastEvent.tick;
    console.log('diff', diff);
    if (diff < 0) {
      console.warn('Track was too long!');
      diff = 0;
    }

    const endLoop = {
      type: EVENT_MIDI,
      subtype: EVENT_MIDI_CONTROLLER,
      channel: endTrack.channel || 0,
      param1: CC_103_TRACK_LOOP_END,
      delta: diff,
      tick: songLength,
    };

    events.push(lastEvent);
    events.push(endLoop);
    events.push(endTrack);

    console.log('Added LOOP_END event to track %s.', trackIdx, endLoop);
  }

  // if (endTrack.tick !== songLength) {
  //   console.log('Track %s end adjusted from %s to %s.', trackIdx, endTrack.tick, songLength);
  //   const diff = songLength - endTrack.tick;
  //   if (diff < 0) console.warn('Track was too long!');
  //   endTrack.delta += diff;
  //   endTrack.tick += diff;
  // }
  return events;
}

function insertAllNotesOff(trackIdx, events, songLength) {
  const endLoopIdx = events.findIndex(ev => isLoopEnd(ev));

  if (endLoopIdx) {
    const endLoop = events[endLoopIdx];
    const allNotesOff = {
      type: EVENT_MIDI,
      subtype: EVENT_MIDI_CONTROLLER,
      param1: CC_123_ALL_NOTES_OFF,
      channel: endLoop.channel,
      delta: endLoop.delta,
      tick: endLoop.tick,
    };
    endLoop.delta = 0;
    events.splice(endLoopIdx, 0, allNotesOff);
    console.log('Added ALL_NOTES_OFF event to track %s.', trackIdx);
  }

  return events;
}

// function sortSameTickNoteEvents(events) {
//   events.sort((a, b) => {
//     if (a.tick === b.tick) {
//       if (a.type === EVENT_MIDI) {
//         if (a.subtype === EVENT_MIDI_NOTE_ON) {
//           return 1;
//         } else if (a.subtype === EVENT_MIDI_NOTE_OFF) {
//           return -1;
//         }
//       }
//       return 0;
//     }
//     return (a.tick - b.tick);
//   });
// }

function fixOverlappingNotes(trackIdx, events) {
  const noteCount = new Array(128).fill(0);
  const newEvents = [];
  let numRepairedNotes = 0;

  for (let event of events) {
    const p1 = event.param1;
    if (isNoteOn(event)) {
      if (noteCount[p1] > 1) {
        // Insert note off
        numRepairedNotes++;
        newEvents.push({
          type: EVENT_MIDI,
          subtype: EVENT_MIDI_NOTE_OFF,
          channel: event.channel,
          param1: p1,
          delta: event.delta,
        });
        event.delta = 0;
      }
      newEvents.push(event);
      noteCount[p1]++;
    } else if (isNoteOff(event)) {
      const lastEvent = newEvents[newEvents.length - 1];
      if (noteCount[p1] > 1 && lastEvent) {
        // Skip note off
        lastEvent.delta += event.delta;
        lastEvent.tick = event.tick;
        // event.delta = 0;
      } else {
        newEvents.push(event);
      }
      if (noteCount[p1] > 0)
        noteCount[p1]--;
    } else {
      newEvents.push(event);
    }
  }
  console.log('Fixed %d overlapping notes on track %d.', numRepairedNotes, trackIdx);
  return newEvents;
}

function isNoteOn(event) {
  return (
    event.subtype === EVENT_MIDI_NOTE_ON &&
    event.type === EVENT_MIDI &&
    event.param2 !== 0
  );
}

function isNoteOff(event) {
  return (
    event.type === EVENT_MIDI &&
    (event.subtype === EVENT_MIDI_NOTE_ON && event.param2 === 0 ||
      event.subtype === EVENT_MIDI_NOTE_OFF)
  );
}

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

function isEndOfTrack(event) {
  return (
    event.subtype === EVENT_META_END_OF_TRACK &&
    event.type === EVENT_META
  );
}