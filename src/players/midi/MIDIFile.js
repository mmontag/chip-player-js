'use strict';

// MIDIFile : Read (and soon edit) a MIDI file in a given ArrayBuffer

// Dependencies
var MIDIFileHeader = require('./MIDIFileHeader');
var MIDIFileTrack = require('./MIDIFileTrack');
var MIDIEvents = require('midievents');
var UTF8 = require('utf-8');

function ensureArrayBuffer(buf) {
  if (buf) {
    if (buf instanceof ArrayBuffer) {
      return buf;
    }
    if (buf instanceof Uint8Array) {
      // Copy/convert to standard Uint8Array, because derived classes like
      // node.js Buffers might have unexpected data in the .buffer property.
      return new Uint8Array(buf).buffer;
    }
  }
  throw new Error('Unsupported buffer type, need ArrayBuffer or Uint8Array');
}

// Constructor
function MIDIFile(buffer, strictMode) {
  var track;
  var curIndex;
  var i;
  var j;

  // If not buffer given, creating a new MIDI file
  if (!buffer) {
    // Creating the content
    this.header = new MIDIFileHeader();
    this.tracks = [new MIDIFileTrack()];
    // if a buffer is provided, parsing him
  } else {
    buffer = ensureArrayBuffer(buffer);
    // Minimum MIDI file size is a headerChunk size (14bytes)
    // and an empty track (8+3bytes)
    if (25 > buffer.byteLength) {
      throw new Error(
        'A buffer of a valid MIDI file must have, at least, a' +
          ' size of 25bytes.'
      );
    }
    // Reading header
    this.header = new MIDIFileHeader(buffer, strictMode);
    this.tracks = [];
    curIndex = MIDIFileHeader.HEADER_LENGTH;
    // Reading tracks
    for (i = 0, j = this.header.getTracksCount(); i < j; i++) {
      // Testing the buffer length
      if (strictMode && curIndex >= buffer.byteLength - 1) {
        throw new Error(
          "Couldn't find datas corresponding to the track #" + i + '.'
        );
      }
      // Creating the track object
      track = new MIDIFileTrack(buffer, curIndex, strictMode);
      this.tracks.push(track);
      // Updating index to the track end
      curIndex += track.getTrackLength() + 8;
    }
    // Testing integrity : curIndex should be at the end of the buffer
    if (strictMode && curIndex !== buffer.byteLength) {
      throw new Error('It seems that the buffer contains too much datas.');
    }
  }
}

// Events reading helpers
MIDIFile.prototype.getEvents = function(type, subtype) {
  var events;
  var event;
  var playTime = 0;
  var filteredEvents = [];
  var format = this.header.getFormat();
  var tickResolution = this.header.getTickResolution();
  var i;
  var j;
  var trackParsers;
  var smallestDelta;

  // Reading events
  // if the read is sequential
  if (1 !== format || 1 === this.tracks.length) {
    for (i = 0, j = this.tracks.length; i < j; i++) {
      // reset playtime if format is 2
      playTime = 2 === format && playTime ? playTime : 0;
      events = MIDIEvents.createParser(
        this.tracks[i].getTrackContent(),
        0,
        false
      );
      // loooping through events
      event = events.next();
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
          filteredEvents.push(event);
        }
        event = events.next();
      }
    }
    // the read is concurrent
  } else {
    trackParsers = [];
    smallestDelta = -1;

    // Creating parsers
    for (i = 0, j = this.tracks.length; i < j; i++) {
      trackParsers[i] = {};
      trackParsers[i].parser = MIDIEvents.createParser(
        this.tracks[i].getTrackContent(),
        0,
        false
      );
      trackParsers[i].curEvent = trackParsers[i].parser.next();
    }
    // Filling events
    do {
      smallestDelta = -1;
      // finding the smallest event
      for (i = 0, j = trackParsers.length; i < j; i++) {
        if (trackParsers[i].curEvent) {
          if (
            -1 === smallestDelta ||
            trackParsers[i].curEvent.delta <
              trackParsers[smallestDelta].curEvent.delta
          ) {
            smallestDelta = i;
          }
        }
      }
      if (-1 !== smallestDelta) {
        // removing the delta of previous events
        for (i = 0, j = trackParsers.length; i < j; i++) {
          if (i !== smallestDelta && trackParsers[i].curEvent) {
            trackParsers[i].curEvent.delta -=
              trackParsers[smallestDelta].curEvent.delta;
          }
        }
        // filling values
        event = trackParsers[smallestDelta].curEvent;
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
          filteredEvents.push(event);
        }
        // getting next event
        trackParsers[smallestDelta].curEvent = trackParsers[
          smallestDelta
        ].parser.next();
      }
    } while (-1 !== smallestDelta);
  }
  return filteredEvents;
};

MIDIFile.prototype.getMidiEvents = function() {
  return this.getEvents(MIDIEvents.EVENT_MIDI);
};

MIDIFile.prototype.getLyrics = function() {
  var events = this.getEvents(MIDIEvents.EVENT_META);
  var texts = [];
  var lyrics = [];
  var event;
  var i;
  var j;

  for (i = 0, j = events.length; i < j; i++) {
    event = events[i];
    // Lyrics
    if (event.subtype === MIDIEvents.EVENT_META_LYRICS) {
      lyrics.push(event);
      // Texts
    } else if (event.subtype === MIDIEvents.EVENT_META_TEXT) {
      // Ignore special texts
      if ('@' === String.fromCharCode(event.data[0])) {
        if ('T' === String.fromCharCode(event.data[1])) {
          // console.log('Title : ' + event.text.substring(2));
        } else if ('I' === String.fromCharCode(event.data[1])) {
          // console.log('Info : ' + event.text.substring(2));
        } else if ('L' === String.fromCharCode(event.data[1])) {
          // console.log('Lang : ' + event.text.substring(2));
        }
        // karaoke text follows, remove all previous text
      } else if (
        0 === String.fromCharCode.apply(String, event.data).indexOf('words')
      ) {
        texts.length = 0;
        // console.log('Word marker found');
        // Karaoke texts
        // If playtime is greater than 0
      } else if (0 !== event.playTime) {
        texts.push(event);
      }
    }
  }
  // Choosing the right lyrics
  if (2 < lyrics.length) {
    texts = lyrics;
  } else if (!texts.length) {
    texts = [];
  }
  // Convert texts and detect encoding
  try {
    texts.forEach(function(event) {
      event.text = UTF8.getStringFromBytes(event.data, 0, event.length, true);
    });
  } catch (e) {
    texts.forEach(function(event) {
      event.text = event.data
        .map(function(c) {
          return String.fromCharCode(c);
        })
        .join('');
    });
  }
  return texts;
};

// Basic events reading
MIDIFile.prototype.getTrackEvents = function(index) {
  var event;
  var events = [];
  var parser;
  if (index > this.tracks.length || 0 > index) {
    throw Error('Invalid track index (' + index + ')');
  }
  parser = MIDIEvents.createParser(
    this.tracks[index].getTrackContent(),
    0,
    false
  );
  event = parser.next();
  do {
    events.push(event);
    event = parser.next();
  } while (event);
  return events;
};

// Basic events writting
MIDIFile.prototype.setTrackEvents = function(index, events) {
  var bufferLength;
  var destination;

  if (index > this.tracks.length || 0 > index) {
    throw Error('Invalid track index (' + index + ')');
  }
  if (!events || !events.length) {
    throw Error('A track must contain at least one event, none given.');
  }
  bufferLength = MIDIEvents.getRequiredBufferLength(events);
  destination = new Uint8Array(bufferLength);
  MIDIEvents.writeToTrack(events, destination);
  this.tracks[index].setTrackContent(destination);
};

// Remove a track
MIDIFile.prototype.deleteTrack = function(index) {
  if (index > this.tracks.length || 0 > index) {
    throw Error('Invalid track index (' + index + ')');
  }
  this.tracks.splice(index, 1);
  this.header.setTracksCount(this.tracks.length);
};

// Add a track
MIDIFile.prototype.addTrack = function(index) {
  var track;

  if (index > this.tracks.length || 0 > index) {
    throw Error('Invalid track index (' + index + ')');
  }
  track = new MIDIFileTrack();
  if (index === this.tracks.length) {
    this.tracks.push(track);
  } else {
    this.tracks.splice(index, 0, track);
  }
  this.header.setTracksCount(this.tracks.length);
};

// Retrieve the content in a buffer
MIDIFile.prototype.getContent = function() {
  var bufferLength;
  var destination;
  var origin;
  var i;
  var j;
  var k;
  var l;
  var m;
  var n;

  // Calculating the buffer content
  // - initialize with the header length
  bufferLength = MIDIFileHeader.HEADER_LENGTH;
  // - add tracks length
  for (i = 0, j = this.tracks.length; i < j; i++) {
    bufferLength += this.tracks[i].getTrackLength() + 8;
  }
  // Creating the destination buffer
  destination = new Uint8Array(bufferLength);
  // Adding header
  origin = new Uint8Array(
    this.header.datas.buffer,
    this.header.datas.byteOffset,
    MIDIFileHeader.HEADER_LENGTH
  );
  for (i = 0, j = MIDIFileHeader.HEADER_LENGTH; i < j; i++) {
    destination[i] = origin[i];
  }
  // Adding tracks
  for (k = 0, l = this.tracks.length; k < l; k++) {
    origin = new Uint8Array(
      this.tracks[k].datas.buffer,
      this.tracks[k].datas.byteOffset,
      this.tracks[k].datas.byteLength
    );
    for (m = 0, n = this.tracks[k].datas.byteLength; m < n; m++) {
      destination[i++] = origin[m];
    }
  }
  return destination.buffer;
};

// Exports Track/Header constructors
MIDIFile.Header = MIDIFileHeader;
MIDIFile.Track = MIDIFileTrack;

module.exports = MIDIFile;
