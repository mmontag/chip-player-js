'use strict';

// MIDIFileTrack : Read and edit a MIDI track chunk in a given ArrayBuffer
function MIDIFileTrack(buffer, start) {
  let a;
  let trackLength;

  // no buffer, creating him
  if (!buffer) {
    a = new Uint8Array(12);
    // Adding the empty track header (MTrk)
    a[0] = 0x4d;
    a[1] = 0x54;
    a[2] = 0x72;
    a[3] = 0x6b;
    // Adding the empty track size (4)
    a[4] = 0x00;
    a[5] = 0x00;
    a[6] = 0x00;
    a[7] = 0x04;
    // Adding the track end event
    a[8] = 0x00;
    a[9] = 0xff;
    a[10] = 0x2f;
    a[11] = 0x00;
    // Saving the buffer
    this.datas = new DataView(a.buffer, 0, MIDIFileTrack.HDR_LENGTH + 4);
    // parsing the given buffer
  } else {
    if (!(buffer instanceof ArrayBuffer)) {
      throw new Error('Invalid buffer received.');
    }
    // Buffer length must size at least like an  empty track (8+3bytes)
    if (12 > buffer.byteLength - start) {
      throw new Error(
        'Invalid MIDIFileTrack (0x' +
          start.toString(16) +
          ') :' +
          ' Buffer length must size at least 12bytes'
      );
    }
    // Creating a temporary view to read the track header
    this.datas = new DataView(buffer, start, MIDIFileTrack.HDR_LENGTH);
    // Reading MIDI track header chunk
    if (
      !(
        'M' === String.fromCharCode(this.datas.getUint8(0)) &&
        'T' === String.fromCharCode(this.datas.getUint8(1)) &&
        'r' === String.fromCharCode(this.datas.getUint8(2)) &&
        'k' === String.fromCharCode(this.datas.getUint8(3))
      )
    ) {
      throw new Error(
        'Invalid MIDIFileTrack (0x' +
          start.toString(16) +
          ') :' +
          ' MTrk prefix not found'
      );
    }
    // Reading the track length
    trackLength = this.getTrackLength();
    if (buffer.byteLength - start < trackLength) {
      throw new Error(
        'Invalid MIDIFileTrack (0x' +
          start.toString(16) +
          ') :' +
          ' The track size exceed the buffer length.'
      );
    }
    // Creating the final DataView
    this.datas = new DataView(
      buffer,
      start,
      MIDIFileTrack.HDR_LENGTH + trackLength
    );
    // Trying to find the end of track event
    if (
      !(
        0xff ===
          this.datas.getUint8(MIDIFileTrack.HDR_LENGTH + (trackLength - 3)) &&
        0x2f ===
          this.datas.getUint8(MIDIFileTrack.HDR_LENGTH + (trackLength - 2)) &&
        0x00 ===
          this.datas.getUint8(MIDIFileTrack.HDR_LENGTH + (trackLength - 1))
      )
    ) {
      throw new Error(
        'Invalid MIDIFileTrack (0x' +
          start.toString(16) +
          ') :' +
          ' No track end event found at the expected index' +
          ' (' +
          (MIDIFileTrack.HDR_LENGTH + (trackLength - 1)).toString(16) +
          ').'
      );
    }
  }
}

// Static constants
MIDIFileTrack.HDR_LENGTH = 8;

// Track length
MIDIFileTrack.prototype.getTrackLength = function() {
  return this.datas.getUint32(4);
};

MIDIFileTrack.prototype.setTrackLength = function(trackLength) {
  return this.datas.setUint32(4, trackLength);
};

// Read track contents
MIDIFileTrack.prototype.getTrackContent = function() {
  return new DataView(
    this.datas.buffer,
    this.datas.byteOffset + MIDIFileTrack.HDR_LENGTH,
    this.datas.byteLength - MIDIFileTrack.HDR_LENGTH
  );
};

// Set track content
MIDIFileTrack.prototype.setTrackContent = function(dataView) {
  let origin;
  let destination;
  let i;
  let j;
  // Calculating the track length
  const trackLength = dataView.byteLength - dataView.byteOffset;

  // Track length must size at least like an  empty track (4bytes)
  if (4 > trackLength) {
    throw new Error('Invalid track length, must size at least 4bytes');
  }
  this.datas = new DataView(
    new Uint8Array(MIDIFileTrack.HDR_LENGTH + trackLength).buffer
  );
  // Adding the track header (MTrk)
  this.datas.setUint8(0, 0x4d); // M
  this.datas.setUint8(1, 0x54); // T
  this.datas.setUint8(2, 0x72); // r
  this.datas.setUint8(3, 0x6b); // k
  // Adding the track size
  this.datas.setUint32(4, trackLength);
  // Copying the content
  origin = new Uint8Array(
    dataView.buffer,
    dataView.byteOffset,
    dataView.byteLength
  );
  destination = new Uint8Array(
    this.datas.buffer,
    MIDIFileTrack.HDR_LENGTH,
    trackLength
  );
  for (i = 0, j = origin.length; i < j; i++) {
    destination[i] = origin[i];
  }
};

module.exports = MIDIFileTrack;
