const { test } = require('node:test');
const assert = require('node:assert/strict');
const fs = require('node:fs');
const vm = require('node:vm');
const source = fs.readFileSync('src/players/MIDIFilePlayer.js', 'utf8')
  .replace(/^import .*;\n/gm, '').replace('export default MIDIPlayer;', 'this.MIDIPlayer = MIDIPlayer;');
const MIDIEvents = { EVENT_MIDI_PROGRAM_CHANGE: 12, EVENT_MIDI_CONTROLLER: 11,
  EVENT_MIDI_PITCH_BEND: 14, EVENT_SYSEX: 240, EVENT_DIVSYSEX: 247 };
const context = { autoBind() {}, MIDIEvents, performance, setTimeout, clearTimeout, console };
vm.runInNewContext(source, context);

function fixture() {
  const bends = [], messages = [];
  const synth = { controlChange() {}, panic() {}, reset() {}, programChange() {},
    pitchBend: (channel, value) => bends.push([channel, value]) };
  const output = { send: message => messages.push(message) };
  const player = new context.MIDIPlayer({ synth, output, programChangeCb() {} });
  player.events = [
    { subtype: 14, channel: 0, param1: 0, param2: 96, playTime: 100 },
    { subtype: 14, channel: 0, param1: 0, param2: 80, playTime: 700 },
    { subtype: 0, playTime: 2000 },
  ];
  player.position = 1;
  player.elapsedTime = 500;
  return { player, bends, messages };
}

test('seek restores latest pitch bend even when the event precedes the current position', () => {
  const { player, bends } = fixture();
  player.setPosition(600);
  assert.deepEqual(bends.filter(([ch]) => ch === 0).map(([, v]) => v), [8192, 12288]);
  player.setPosition(1000);
  assert.equal(bends.filter(([ch]) => ch === 0).at(-1)[1], 10240);
  player.setPosition(50);
  assert.equal(bends.filter(([ch]) => ch === 0).at(-1)[1], 8192);
});

test('Web MIDI seek sends the bend as a pitch-bend message', () => {
  const { player, messages } = fixture();
  player.useWebMIDI = true;
  player.paused = true;
  player.setPosition(600);
  assert.ok(messages.some(m => m[0] === 0xe0 && m[1] === 0 && m[2] === 96));
});

test('switching tracks cancels the previous Web MIDI end callback', async () => {
  const { player } = fixture();
  player.useWebMIDI = true;
  player.events = [];
  player.position = 0;
  player.paused = false;
  let ended = 0;
  player.endCallback = () => { ended++; };
  player.processPlay();
  player.stop();
  player.events = [{ playTime: 2000 }];
  player.position = 0;
  player.play(() => {});
  await new Promise(resolve => setTimeout(resolve, 180));
  assert.equal(ended, 0);
  player.stop();
});

test('normal Web MIDI completion still calls the end callback', async () => {
  const { player } = fixture();
  player.useWebMIDI = true;
  player.events = [];
  player.position = 0;
  player.paused = false;
  let ended = 0;
  player.endCallback = () => { ended++; };
  player.processPlay();
  await new Promise(resolve => setTimeout(resolve, 180));
  assert.equal(ended, 1);
});
