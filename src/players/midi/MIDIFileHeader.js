'use strict';

// MIDIFileHeader : Read and edit a MIDI header chunk in a given ArrayBuffer
function MIDIFileHeader(buffer) {
  let a;
  // No buffer creating him
  if (!buffer) {
    a = new Uint8Array(MIDIFileHeader.HEADER_LENGTH);
    // Adding the header id (MThd)
    a[0] = 0x4d;
    a[1] = 0x54;
    a[2] = 0x68;
    a[3] = 0x64;
    // Adding the header chunk size
    a[4] = 0x00;
    a[5] = 0x00;
    a[6] = 0x00;
    a[7] = 0x06;
    // Adding the file format (1 here cause it's the most commonly used)
    a[8] = 0x00;
    a[9] = 0x01;
    // Adding the track count (1 cause it's a new file)
    a[10] = 0x00;
    a[11] = 0x01;
    // Adding the time division (192 ticks per beat)
    a[12] = 0x00;
    a[13] = 0xc0;
    // saving the buffer
    this.datas = new DataView(a.buffer, 0, MIDIFileHeader.HEADER_LENGTH);
    // Parsing the given buffer
  } else {
    if (!(buffer instanceof ArrayBuffer)) {
      throw Error('Invalid buffer received.');
    }
    this.datas = new DataView(buffer, 0, MIDIFileHeader.HEADER_LENGTH);
    // Reading MIDI header chunk
    if (
      !(
        'M' === String.fromCharCode(this.datas.getUint8(0)) &&
        'T' === String.fromCharCode(this.datas.getUint8(1)) &&
        'h' === String.fromCharCode(this.datas.getUint8(2)) &&
        'd' === String.fromCharCode(this.datas.getUint8(3))
      )
    ) {
      throw new Error('Invalid MIDIFileHeader : MThd prefix not found');
    }
    // Reading chunk length
    if (6 !== this.datas.getUint32(4)) {
      throw new Error('Invalid MIDIFileHeader : Chunk length must be 6');
    }
  }
}

// Static constants
MIDIFileHeader.HEADER_LENGTH = 14;
MIDIFileHeader.FRAMES_PER_SECONDS = 1;
MIDIFileHeader.TICKS_PER_BEAT = 2;

// MIDI file format
MIDIFileHeader.prototype.getFormat = function() {
  const format = this.datas.getUint16(8);
  if (0 !== format && 1 !== format && 2 !== format) {
    throw new Error(
      'Invalid MIDI file : MIDI format (' +
        format +
        '),' +
        ' format can be 0, 1 or 2 only.'
    );
  }
  return format;
};

MIDIFileHeader.prototype.setFormat = function(format) {
  if (0 !== format && 1 !== format && 2 !== format) {
    throw new Error(
      'Invalid MIDI format given (' +
        format +
        '),' +
        ' format can be 0, 1 or 2 only.'
    );
  }
  this.datas.setUint16(8, format);
};

// Number of tracks
MIDIFileHeader.prototype.getTracksCount = function() {
  return this.datas.getUint16(10);
};

MIDIFileHeader.prototype.setTracksCount = function(n) {
  return this.datas.setUint16(10, n);
};

// Tick compute
MIDIFileHeader.prototype.getTickResolution = function(tempo) {
  // Frames per seconds
  if (this.datas.getUint16(12) & 0x8000) {
    return 1000000 / (this.getSMPTEFrames() * this.getTicksPerFrame());
    // Ticks per beat
  }
  // Default MIDI tempo is 120bpm, 500ms per beat
  tempo = tempo || 500000;
  return tempo / this.getTicksPerBeat();
};

// Time division type
MIDIFileHeader.prototype.getTimeDivision = function() {
  if (this.datas.getUint16(12) & 0x8000) {
    return MIDIFileHeader.FRAMES_PER_SECONDS;
  }
  return MIDIFileHeader.TICKS_PER_BEAT;
};

// Ticks per beat
MIDIFileHeader.prototype.getTicksPerBeat = function() {
  var divisionWord = this.datas.getUint16(12);
  if (divisionWord & 0x8000) {
    throw new Error('Time division is not expressed as ticks per beat.');
  }
  return divisionWord;
};

MIDIFileHeader.prototype.setTicksPerBeat = function(ticksPerBeat) {
  this.datas.setUint16(12, ticksPerBeat & 0x7fff);
};

// Frames per seconds
MIDIFileHeader.prototype.getSMPTEFrames = function() {
  const divisionWord = this.datas.getUint16(12);
  let smpteFrames;

  if (!(divisionWord & 0x8000)) {
    throw new Error('Time division is not expressed as frames per seconds.');
  }
  smpteFrames = divisionWord & 0x7f00;
  if (-1 === [24, 25, 29, 30].indexOf(smpteFrames)) {
    throw new Error('Invalid SMPTE frames value (' + smpteFrames + ').');
  }
  return 29 === smpteFrames ? 29.97 : smpteFrames;
};

MIDIFileHeader.prototype.getTicksPerFrame = function() {
  const divisionWord = this.datas.getUint16(12);

  if (!(divisionWord & 0x8000)) {
    throw new Error('Time division is not expressed as frames per seconds.');
  }
  return divisionWord & 0x00ff;
};

MIDIFileHeader.prototype.setSMTPEDivision = function(
  smpteFrames,
  ticksPerFrame
) {
  if (29.97 === smpteFrames) {
    smpteFrames = 29;
  }
  if (-1 === [24, 25, 29, 30].indexOf(smpteFrames)) {
    throw new Error('Invalid SMPTE frames value given (' + smpteFrames + ').');
  }
  if (0 > ticksPerFrame || 0xff < ticksPerFrame) {
    throw new Error(
      'Invalid ticks per frame value given (' + smpteFrames + ').'
    );
  }
  this.datas.setUint8(12, 0x80 | smpteFrames);
  this.datas.setUint8(13, ticksPerFrame);
};

module.exports = MIDIFileHeader;
