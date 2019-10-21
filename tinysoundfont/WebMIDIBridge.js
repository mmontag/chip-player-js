const values = {
  noteoff: 0x8,           // 8
  noteon: 0x9,            // 9
  keyaftertouch: 0xA,     // 10
  controlchange: 0xB,     // 11
  channelmode: 0xB,       // 11
  nrpn: 0xB,              // 11
  programchange: 0xC,     // 12
  channelaftertouch: 0xD, // 13
  pitchbend: 0xE          // 14
};

mergeInto(LibraryManager.library, {
  webMidiNoteOn: function (channel, key, velocity) {
    const status = 0x90 + channel;
    window._webMidiBridgeOutput.send([status, key, velocity]);
  },
  webMidiNoteOff: function (channel, key) {
    const status = 0x80 + channel;
    window._webMidiBridgeOutput.send([status, key, 0x7F]);
  },
  webMidiProgramChange: function (channel, program) {
    const status = 0xC0 + channel;
    window._webMidiBridgeOutput.send([status, program]);
  },
  webMidiPitchBend: function (channel, pitch) {
    const status = 0xE0 + channel;
    const lsb = pitch && 0x7F;
    const msb = pitch >> 7;
    window._webMidiBridgeOutput.send([status, lsb, msb]);
  },
  webMidiControlChange: function (channel, control, value) {
    const status = 0xB0 + channel;
    window._webMidiBridgeOutput.send([status, control, value]);
  },
  webMidiChannelPressure: function (channel, value) {
    const status = 0xD0 + channel;
    window._webMidiBridgeOutput.send([status, value]);
  },
  webMidiPanic: function () {
    for (let channel = 0; channel < 16; channel++) {
      const status = 0x80 + channel;
      for (let key = 0; key < 128; key++) {
        window._webMidiBridgeOutput.send([status, key, 0x7F]);
      }
    }
  },
  webMidiPanicChannel: function (channel) {
    const status = 0x80 + channel;
    for (let key = 0; key < 128; key++) {
      window._webMidiBridgeOutput.send([status, key, 0x7F]);
    }
  },
  webMidiReset: function () {
    for (let channel = 0; channel < 16; channel++) {
      const status = 0x80 + channel;
      for (let key = 0; key < 128; key++) {
        window._webMidiBridgeOutput.send([status, key, 0x7F]);
      }
    }
  },
});
