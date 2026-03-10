'use strict';

// MIDIEvents : Read and edit events from various sources (ArrayBuffer, Stream)
function MIDIEvents() {
  throw new Error('MIDIEvents function not intended to be run.');
}

// Static constants
// Event types
MIDIEvents.EVENT_META = 0xff;
MIDIEvents.EVENT_SYSEX = 0xf0;
MIDIEvents.EVENT_DIVSYSEX = 0xf7;
MIDIEvents.EVENT_MIDI = 0x8;
// Meta event types
MIDIEvents.EVENT_META_SEQUENCE_NUMBER = 0x00;
MIDIEvents.EVENT_META_TEXT = 0x01;
MIDIEvents.EVENT_META_COPYRIGHT_NOTICE = 0x02;
MIDIEvents.EVENT_META_TRACK_NAME = 0x03;
MIDIEvents.EVENT_META_INSTRUMENT_NAME = 0x04;
MIDIEvents.EVENT_META_LYRICS = 0x05;
MIDIEvents.EVENT_META_MARKER = 0x06;
MIDIEvents.EVENT_META_CUE_POINT = 0x07;
MIDIEvents.EVENT_META_MIDI_CHANNEL_PREFIX = 0x20;
MIDIEvents.EVENT_META_END_OF_TRACK = 0x2f;
MIDIEvents.EVENT_META_SET_TEMPO = 0x51;
MIDIEvents.EVENT_META_SMTPE_OFFSET = 0x54;
MIDIEvents.EVENT_META_TIME_SIGNATURE = 0x58;
MIDIEvents.EVENT_META_KEY_SIGNATURE = 0x59;
MIDIEvents.EVENT_META_SEQUENCER_SPECIFIC = 0x7f;
// MIDI event types
MIDIEvents.EVENT_MIDI_NOTE_OFF = 0x8;
MIDIEvents.EVENT_MIDI_NOTE_ON = 0x9;
MIDIEvents.EVENT_MIDI_NOTE_AFTERTOUCH = 0xa;
MIDIEvents.EVENT_MIDI_CONTROLLER = 0xb;
MIDIEvents.EVENT_MIDI_PROGRAM_CHANGE = 0xc;
MIDIEvents.EVENT_MIDI_CHANNEL_AFTERTOUCH = 0xd;
MIDIEvents.EVENT_MIDI_PITCH_BEND = 0xe;
// MIDI event sizes
MIDIEvents.MIDI_1PARAM_EVENTS = [
  MIDIEvents.EVENT_MIDI_PROGRAM_CHANGE,
  MIDIEvents.EVENT_MIDI_CHANNEL_AFTERTOUCH,
];
MIDIEvents.MIDI_2PARAMS_EVENTS = [
  MIDIEvents.EVENT_MIDI_NOTE_OFF,
  MIDIEvents.EVENT_MIDI_NOTE_ON,
  MIDIEvents.EVENT_MIDI_NOTE_AFTERTOUCH,
  MIDIEvents.EVENT_MIDI_CONTROLLER,
  MIDIEvents.EVENT_MIDI_PITCH_BEND,
];

// Create an event stream parser
MIDIEvents.createParser = function midiEventsCreateParser(
  stream,
  startAt,
  strictMode
) {
  // Private vars
  // Common vars
  var eventTypeByte;
  var event;
  // MIDI events vars
  var MIDIEventType;
  var MIDIEventChannel;
  var MIDIEventParam1;

  // Wrap DataView into a data stream
  if (stream instanceof DataView) {
    stream = {
      position: startAt || 0,
      buffer: stream,
      readUint8: function() {
        return this.buffer.getUint8(this.position++);
      },
      readUint16: function() {
        var v = this.buffer.getUint16(this.position);
        this.position = this.position + 2;
        return v;
      },
      readUint32: function() {
        var v = this.buffer.getUint16(this.position);
        this.position = this.position + 2;
        return v;
      },
      readVarInt: function() {
        var v = 0;
        var i = 0;
        var b;

        while (4 > i++) {
          b = this.readUint8();

          if (b & 0x80) {
            v += b & 0x7f;
            v <<= 7;
          } else {
            return v + b;
          }
        }
        throw new Error(
          '0x' +
            this.position.toString(16) +
            ':' +
            ' Variable integer length cannot exceed 4 bytes'
        );
      },
      readBytes: function(length) {
        var bytes = [];

        for (; 0 < length; length--) {
          bytes.push(this.readUint8());
        }
        return bytes;
      },
      pos: function() {
        return '0x' + (this.buffer.byteOffset + this.position).toString(16);
      },
      end: function() {
        return this.position === this.buffer.byteLength;
      },
    };
    startAt = 0;
  }
  // Consume stream till not at start index
  if (0 < startAt) {
    while (startAt--) {
      stream.readUint8();
    }
  }
  // creating the parser object
  return {
    // Read the next event
    next: function() {
      // Check available datas
      if (stream.end()) {
        return null;
      }
      // Creating the event
      event = {
        // Memoize the event index
        index: stream.pos(),
        // Read the delta time
        delta: stream.readVarInt(),
      };
      // Read the eventTypeByte
      eventTypeByte = stream.readUint8();
      if (0xf0 === (eventTypeByte & 0xf0)) {
        // Meta events
        if (eventTypeByte === MIDIEvents.EVENT_META) {
          event.type = MIDIEvents.EVENT_META;
          event.subtype = stream.readUint8();
          event.length = stream.readVarInt();
          switch (event.subtype) {
            case MIDIEvents.EVENT_META_SEQUENCE_NUMBER:
              if (strictMode && 2 !== event.length) {
                throw new Error(stream.pos() + ' Bad metaevent length.');
              }
              event.msb = stream.readUint8();
              event.lsb = stream.readUint8();
              return event;
            case MIDIEvents.EVENT_META_TEXT:
            case MIDIEvents.EVENT_META_COPYRIGHT_NOTICE:
            case MIDIEvents.EVENT_META_TRACK_NAME:
            case MIDIEvents.EVENT_META_INSTRUMENT_NAME:
            case MIDIEvents.EVENT_META_LYRICS:
            case MIDIEvents.EVENT_META_MARKER:
            case MIDIEvents.EVENT_META_CUE_POINT:
              event.data = stream.readBytes(event.length);
              return event;
            case MIDIEvents.EVENT_META_MIDI_CHANNEL_PREFIX:
              if (strictMode && 1 !== event.length) {
                throw new Error(stream.pos() + ' Bad metaevent length.');
              }
              event.prefix = stream.readUint8();
              return event;
            case MIDIEvents.EVENT_META_END_OF_TRACK:
              if (strictMode && 0 !== event.length) {
                throw new Error(stream.pos() + ' Bad metaevent length.');
              }
              return event;
            case MIDIEvents.EVENT_META_SET_TEMPO:
              if (strictMode && 3 !== event.length) {
                throw new Error(
                  stream.pos() + ' Tempo meta event length must be 3.'
                );
              }
              event.tempo =
                (stream.readUint8() << 16) +
                (stream.readUint8() << 8) +
                stream.readUint8();
              event.tempoBPM = 60000000 / event.tempo;
              return event;
            case MIDIEvents.EVENT_META_SMTPE_OFFSET:
              if (strictMode && 5 !== event.length) {
                throw new Error(stream.pos() + ' Bad metaevent length.');
              }
              event.hour = stream.readUint8();
              if (strictMode && 23 < event.hour) {
                throw new Error(
                  stream.pos() +
                    ' SMTPE offset hour value must' +
                    ' be part of 0-23.'
                );
              }
              event.minutes = stream.readUint8();
              if (strictMode && 59 < event.minutes) {
                throw new Error(
                  stream.pos() +
                    ' SMTPE offset minutes value' +
                    ' must be part of 0-59.'
                );
              }
              event.seconds = stream.readUint8();
              if (strictMode && 59 < event.seconds) {
                throw new Error(
                  stream.pos() +
                    ' SMTPE offset seconds value' +
                    ' must be part of 0-59.'
                );
              }
              event.frames = stream.readUint8();
              if (strictMode && 30 < event.frames) {
                throw new Error(
                  stream.pos() +
                    ' SMTPE offset frames value must' +
                    ' be part of 0-30.'
                );
              }
              event.subframes = stream.readUint8();
              if (strictMode && 99 < event.subframes) {
                throw new Error(
                  stream.pos() +
                    ' SMTPE offset subframes value' +
                    ' must be part of 0-99.'
                );
              }
              return event;
            case MIDIEvents.EVENT_META_KEY_SIGNATURE:
              if (strictMode && 2 !== event.length) {
                throw new Error(stream.pos() + ' Bad metaevent length.');
              }
              event.key = stream.readUint8();
              if (strictMode && (-7 > event.key || 7 < event.key)) {
                throw new Error(stream.pos() + ' Bad metaevent length.');
              }
              event.scale = stream.readUint8();
              if (strictMode && 0 !== event.scale && 1 !== event.scale) {
                throw new Error(
                  stream.pos() +
                    ' Key signature scale value must' +
                    ' be 0 or 1.'
                );
              }
              return event;
            case MIDIEvents.EVENT_META_TIME_SIGNATURE:
              if (strictMode && 4 !== event.length) {
                throw new Error(stream.pos() + ' Bad metaevent length.');
              }
              event.data = stream.readBytes(event.length);
              event.param1 = event.data[0];
              event.param2 = event.data[1];
              event.param3 = event.data[2];
              event.param4 = event.data[3];
              return event;
            case MIDIEvents.EVENT_META_SEQUENCER_SPECIFIC:
              event.data = stream.readBytes(event.length);
              return event;
            default:
              if (strictMode) {
                throw new Error(
                  stream.pos() +
                    ' Unknown meta event type ' +
                    '(' +
                    event.subtype.toString(16) +
                    ').'
                );
              }
              event.data = stream.readBytes(event.length);
              return event;
          }
          // System events
        } else if (
          eventTypeByte === MIDIEvents.EVENT_SYSEX ||
          eventTypeByte === MIDIEvents.EVENT_DIVSYSEX
        ) {
          event.type = eventTypeByte;
          event.length = stream.readVarInt();
          event.data = stream.readBytes(event.length);
          return event;
          // Unknown event, assuming it's system like event
        } else {
          if (strictMode) {
            throw new Error(
              stream.pos() +
                ' Unknown event type ' +
                eventTypeByte.toString(16) +
                ', Delta: ' +
                event.delta +
                '.'
            );
          }
          event.type = eventTypeByte;
          event.badsubtype = stream.readVarInt();
          event.length = stream.readUint8();
          event.data = stream.readBytes(event.length);
          return event;
        }
        // MIDI eventsdestination[index++]
      } else {
        // running status
        if (0 === (eventTypeByte & 0x80)) {
          if (!MIDIEventType) {
            throw new Error(
              stream.pos() + ' Running status without previous event'
            );
          }
          MIDIEventParam1 = eventTypeByte;
        } else {
          MIDIEventType = eventTypeByte >> 4;
          MIDIEventChannel = eventTypeByte & 0x0f;
          MIDIEventParam1 = stream.readUint8();
        }
        event.type = MIDIEvents.EVENT_MIDI;
        event.subtype = MIDIEventType;
        event.channel = MIDIEventChannel;
        event.param1 = MIDIEventParam1;
        switch (MIDIEventType) {
          case MIDIEvents.EVENT_MIDI_NOTE_OFF:
            event.param2 = stream.readUint8();
            return event;
          case MIDIEvents.EVENT_MIDI_NOTE_ON:
            event.param2 = stream.readUint8();

            // If velocity is 0, it's a note off event in fact
            if (!event.param2) {
              event.subtype = MIDIEvents.EVENT_MIDI_NOTE_OFF;
              event.param2 = 127; // Find a standard telling what to do here
            }
            return event;
          case MIDIEvents.EVENT_MIDI_NOTE_AFTERTOUCH:
            event.param2 = stream.readUint8();
            return event;
          case MIDIEvents.EVENT_MIDI_CONTROLLER:
            event.param2 = stream.readUint8();
            return event;
          case MIDIEvents.EVENT_MIDI_PROGRAM_CHANGE:
            return event;
          case MIDIEvents.EVENT_MIDI_CHANNEL_AFTERTOUCH:
            return event;
          case MIDIEvents.EVENT_MIDI_PITCH_BEND:
            event.param2 = stream.readUint8();
            return event;
          default:
            if (strictMode) {
              throw new Error(
                stream.pos() +
                  ' Unknown MIDI event type ' +
                  '(' +
                  MIDIEventType.toString(16) +
                  ').'
              );
            }
            return event;
        }
      }
    },
  };
};

// Return the buffer length needed to encode the given events
MIDIEvents.writeToTrack = function midiEventsWriteToTrack(
  events,
  destination,
  strictMode
) {
  var index = 0;
  var i;
  var j;
  var k;
  var l;

  // Converting each event to binary MIDI datas
  for (i = 0, j = events.length; i < j; i++) {
    // Writing delta value
    if (events[i].delta >>> 28) {
      throw Error(
        'Event #' +
          i +
          ': Maximum delta time value reached (' +
          events[i].delta +
          '/134217728 max)'
      );
    }
    if (events[i].delta >>> 21) {
      destination[index++] = ((events[i].delta >>> 21) & 0x7f) | 0x80;
    }
    if (events[i].delta >>> 14) {
      destination[index++] = ((events[i].delta >>> 14) & 0x7f) | 0x80;
    }
    if (events[i].delta >>> 7) {
      destination[index++] = ((events[i].delta >>> 7) & 0x7f) | 0x80;
    }
    destination[index++] = events[i].delta & 0x7f;
    // MIDI Events encoding
    if (events[i].type === MIDIEvents.EVENT_MIDI) {
      // Adding the byte of subtype + channel
      destination[index++] = (events[i].subtype << 4) + events[i].channel;
      // Adding the byte of the first params
      destination[index++] = events[i].param1;
      // Adding a byte for the optionnal second param
      if (-1 !== MIDIEvents.MIDI_2PARAMS_EVENTS.indexOf(events[i].subtype)) {
        destination[index++] = events[i].param2;
      }
      // META / SYSEX events encoding
    } else {
      // Adding the event type byte
      destination[index++] = events[i].type;
      // Adding the META event subtype byte
      if (events[i].type === MIDIEvents.EVENT_META) {
        destination[index++] = events[i].subtype;
      }
      // Writing the event length bytes
      if (events[i].length >>> 28) {
        throw Error(
          'Event #' +
            i +
            ': Maximum length reached (' +
            events[i].length +
            '/134217728 max)'
        );
      }
      if (events[i].length >>> 21) {
        destination[index++] = ((events[i].length >>> 21) & 0x7f) | 0x80;
      }
      if (events[i].length >>> 14) {
        destination[index++] = ((events[i].length >>> 14) & 0x7f) | 0x80;
      }
      if (events[i].length >>> 7) {
        destination[index++] = ((events[i].length >>> 7) & 0x7f) | 0x80;
      }
      destination[index++] = events[i].length & 0x7f;
      if (events[i].type === MIDIEvents.EVENT_META) {
        switch (events[i].subtype) {
          case MIDIEvents.EVENT_META_SEQUENCE_NUMBER:
            destination[index++] = events[i].msb;
            destination[index++] = events[i].lsb;
            break;
          case MIDIEvents.EVENT_META_TEXT:
          case MIDIEvents.EVENT_META_COPYRIGHT_NOTICE:
          case MIDIEvents.EVENT_META_TRACK_NAME:
          case MIDIEvents.EVENT_META_INSTRUMENT_NAME:
          case MIDIEvents.EVENT_META_LYRICS:
          case MIDIEvents.EVENT_META_MARKER:
          case MIDIEvents.EVENT_META_CUE_POINT:
            for (k = 0, l = events[i].length; k < l; k++) {
              destination[index++] = events[i].data[k];
            }
            break;
          case MIDIEvents.EVENT_META_MIDI_CHANNEL_PREFIX:
            destination[index++] = events[i].prefix;
            break;
          case MIDIEvents.EVENT_META_END_OF_TRACK:
            break;
          case MIDIEvents.EVENT_META_SET_TEMPO:
            destination[index++] = events[i].tempo >> 16;
            destination[index++] = (events[i].tempo >> 8) & 0xff;
            destination[index++] = events[i].tempo & 0xff;
            break;
          case MIDIEvents.EVENT_META_SMTPE_OFFSET:
            if (strictMode && 23 < events[i].hour) {
              throw new Error(
                'Event #' +
                  i +
                  ': SMTPE offset hour value must be' +
                  ' part of 0-23.'
              );
            }
            destination[index++] = events[i].hour;
            if (strictMode && 59 < events[i].minutes) {
              throw new Error(
                'Event #' +
                  i +
                  ': SMTPE offset minutes value must' +
                  ' be part of 0-59.'
              );
            }
            destination[index++] = events[i].minutes;
            if (strictMode && 59 < events[i].seconds) {
              throw new Error(
                'Event #' +
                  i +
                  ': SMTPE offset seconds value must' +
                  ' be part of 0-59.'
              );
            }
            destination[index++] = events[i].seconds;
            if (strictMode && 30 < events[i].frames) {
              throw new Error(
                'Event #' +
                  i +
                  ': SMTPE offset frames amount must' +
                  ' be part of 0-30.'
              );
            }
            destination[index++] = events[i].frames;
            if (strictMode && 99 < events[i].subframes) {
              throw new Error(
                'Event #' +
                  i +
                  ': SMTPE offset subframes amount' +
                  ' must be part of 0-99.'
              );
            }
            destination[index++] = events[i].subframes;
            break;
          case MIDIEvents.EVENT_META_KEY_SIGNATURE:
            if (
              'number' != typeof events[i].key ||
              -7 > events[i].key ||
              7 < events[i].scale
            ) {
              throw new Error(
                'Event #' +
                  i +
                  ':The key signature key must be' +
                  ' between -7 and 7'
              );
            }
            if (
              'number' !== typeof events[i].scale ||
              0 > events[i].scale ||
              1 < events[i].scale
            ) {
              throw new Error(
                'Event #' + i + ':' + 'The key signature scale must be 0 or 1'
              );
            }
            destination[index++] = events[i].key;
            destination[index++] = events[i].scale;
            break;
          // Not implemented
          case MIDIEvents.EVENT_META_TIME_SIGNATURE:
          case MIDIEvents.EVENT_META_SEQUENCER_SPECIFIC:
          default:
            for (k = 0, l = events[i].length; k < l; k++) {
              destination[index++] = events[i].data[k];
            }
            break;
        }
        // Adding bytes corresponding to the sysex event datas
      } else {
        for (k = 0, l = events[i].length; k < l; k++) {
          destination[index++] = events[i].data[k];
        }
      }
    }
  }
};

// Return the buffer length needed to encode the given events
MIDIEvents.getRequiredBufferLength = function(events) {
  var bufferLength = 0;
  var i = 0;
  var j;

  // Calculating the track size by adding events lengths
  for (i = 0, j = events.length; i < j; i++) {
    // Computing necessary bytes to encode the delta value
    bufferLength +=
      events[i].delta >>> 21
        ? 4
        : events[i].delta >>> 14
          ? 3
          : events[i].delta >>> 7
            ? 2
            : 1;
    // MIDI Events have various fixed lengths
    if (events[i].type === MIDIEvents.EVENT_MIDI) {
      // Adding a byte for subtype + channel
      bufferLength++;
      // Adding a byte for the first params
      bufferLength++;
      // Adding a byte for the optionnal second param
      if (-1 !== MIDIEvents.MIDI_2PARAMS_EVENTS.indexOf(events[i].subtype)) {
        bufferLength++;
      }
      // META / SYSEX events lengths are self defined
    } else {
      // Adding a byte for the event type
      bufferLength++;
      // Adding a byte for META events subtype
      if (events[i].type === MIDIEvents.EVENT_META) {
        bufferLength++;
      }
      // Adding necessary bytes to encode the length
      bufferLength +=
        events[i].length >>> 21
          ? 4
          : events[i].length >>> 14
            ? 3
            : events[i].length >>> 7
              ? 2
              : 1;
      // Adding bytes corresponding to the event length
      bufferLength += events[i].length;
    }
  }
  return bufferLength;
};

module.exports = MIDIEvents;
