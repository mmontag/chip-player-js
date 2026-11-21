// Tail-end detector for engines that cannot report the end of a tune.
//
// GME detects a silent tail inside its own engine and hands us the track end
// directly. SID and N64 do not, so a player that loops one of them under
// Repeat One watches its rendered output here and restarts the tune once the
// tail has been quiet AND unchanging for a full window -- the JS stand-in for
// the loop API those cores lack.
//
// It works out of the box with the defaults below. An importing player can
// override any of them for its own sense of "tail" by passing `tuning`; a
// field left out keeps the default:
//
//   this.endDetector = new EndDetector({
//     sampleRate: this.sampleRate,
//     bufferSize: this.bufferSize,
//     tuning: { windowSec: 8 },
//   });
//
// Hook-up: call reset() on load / seek / sub-tune change, and fold each
// rendered buffer in with fold(left, right). A trip means the last `windowSec`
// seconds of per-second means are all below `quietMean` and within
// `staticRange` of each other. Run fold() only once the position reaches
// getTripAtMs(duration), so quiet intros and mid-song breakdowns stay gated.
//
// The values in force are the defaults, overridden by the player's `tuning`,
// and overridden again by the dev-only tuning store (see setTuning below),
// which the Settings panel drives. getTuning() reports what is actually used.

// The settings hint shared by every player that uses this detector. The
// toggle's label carries a React glyph, so each player keeps its own paramDef;
// only the text is central here.
export const DETECT_SONG_END_HINT =
  'When Repeat One is active, restart the song when silence is detected.';

const DEFAULT_TUNING = {
  // Per-second mean |sample| below which a second counts as quiet. Music
  // bodies run 0.03-0.16; ended tails sit at or below ~0.001.
  quietMean: 0.004,
  // Max spread of the window's per-second means for the window to count as
  // unchanging. The level gate alone cannot separate a quiet-but-alive
  // passage (which moves); this can.
  staticRange: 0.001,
  // Seconds of tail to require before tripping (mirrors GME's 6s silence rule).
  windowSec: 6,
  // Sample every Nth frame: the detector needs a level, not the waveform.
  tapStep: 7,
};

export default class EndDetector {
  constructor({ sampleRate, bufferSize, tuning = null } = {}) {
    this.sampleRate = sampleRate;
    this.bufferSize = bufferSize;
    this.tuning = tuning; // player overrides; unspecified fields use the defaults
    this.reset();
  }

  // The values in force: the defaults, overridden by the importing player.
  getTuning() {
    return { ...DEFAULT_TUNING, ...this.tuning };
  }

  reset() {
    this.endSecMeans = [];
    this.endSecSum = 0;
    this.endSecFrames = 0;
    this.endDetectTripAtMs = null;
  }

  // Start of the trip window, one window before the expected end. Cached,
  // because the duration only changes on load (which resets the detector).
  getTripAtMs(durationMs) {
    if (this.endDetectTripAtMs == null) {
      this.endDetectTripAtMs = Math.max(0, (durationMs || 0) - this.getTuning().windowSec * 1000);
    }
    return this.endDetectTripAtMs;
  }

  // Fold one rendered buffer into the detector. Returns true once a full
  // window of per-second means is both quiet and unchanging.
  fold(left, right = left) {
    const { quietMean, staticRange, windowSec, tapStep } = this.getTuning();
    let sum = 0, n = 0;
    for (let i = 0; i < this.bufferSize; i += tapStep) {
      sum += Math.abs(left[i]) + Math.abs(right[i]);
      n += 2;
    }
    this.endSecSum += (sum / n) * this.bufferSize;
    this.endSecFrames += this.bufferSize;
    if (this.endSecFrames < this.sampleRate) return false;
    this.endSecMeans.push(this.endSecSum / this.endSecFrames);
    if (this.endSecMeans.length > windowSec) this.endSecMeans.shift();
    this.endSecSum = 0;
    this.endSecFrames = 0;
    if (this.endSecMeans.length < windowSec) return false;
    let lo = Infinity, hi = -Infinity;
    for (const m of this.endSecMeans) {
      if (m >= quietMean) return false;
      if (m < lo) lo = m;
      if (m > hi) hi = m;
    }
    return hi - lo < staticRange;
  }

}
