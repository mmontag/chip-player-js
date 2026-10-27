const { test } = require('node:test');
const assert = require('node:assert/strict');
const fs = require('node:fs');
const vm = require('node:vm');
const source = fs.readFileSync('src/players/MIDIFilePlayer.js', 'utf8')
  .replace(/^import .*;\n/gm, '').replace('export default MIDIPlayer;', 'this.MIDIPlayer = MIDIPlayer;');
const context = { autoBind() {}, MIDIEvents: {}, performance };
vm.runInNewContext(source, context);

test('release-tail detection checks both polarities and both channels across the full buffer', () => {
  const frames = 2048;
  for (const mode of ['positive', 'negative', 'right', 'late', 'silent']) {
    const data = new Float32Array(frames * 2);
    const synth = {
      render(pointer, count) {
        for (let i = 0; i < count; i++) {
          const index = pointer / 4 + i;
          const frame = Math.floor(index / 2), channel = index % 2;
          data[index] = (mode === 'positive' && channel === 0) ? 0.1 :
            (mode === 'negative' && channel === 0) ? -0.1 :
            (mode === 'right' && channel === 1) ? 0.1 :
            (mode === 'late' && channel === 0 && frame >= frames / 2) ? 0.1 : 0;
        }
      },
      getValue: pointer => data[pointer / 4],
    };
    const player = new context.MIDIPlayer({ synth });
    player.events = [];
    player.position = 0;
    player.elapsedTime = 0;
    player.paused = false;
    const result = player.processPlaySynth(0, frames);
    assert.equal(result === 0, mode === 'silent', mode);
  }
});
