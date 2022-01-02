import EventEmitter from 'events';
//
// Player can be viewed as a state machine with
// 3 states (playing, paused, stopped) and 5 transitions
// (open, pause, unpause, seek, stop).
// If the player has reached end of the song,
// it is in the terminal "stopped" state and is
// no longer playable:
//                         ╭– (seek) –╮
//                         ^          v
//  ┌─ ─ ─ ─ ─┐         ┌─────────────────┐            ┌─────────┐
//  │ stopped │–(open)–>│     playing     │––(stop)–––>│ stopped │
//  └ ─ ─ ─ ─ ┘         └─────────────────┘            └─────────┘
//                        | ┌────────┐ ^
//                (pause) ╰>│ paused │–╯ (unpause)
//                          └────────┘
//
// In the "open" transition, the audioNode is connected.
// In the "stop" transition, it is disconnected.
// "stopped" is synonymous with closed/empty.
//
export default class Player extends EventEmitter {
  constructor(chipCore, sampleRate, bufferSize=2048, debug=false) {
    super();

    // this.process = this.process.bind(this);

    this.paused = true;
    this.fileExtensions = [];
    this.metadata = {};
    this.sampleRate = sampleRate;
    this.bufferSize = bufferSize;
    this._innerAudioProcess = null;
    this.debug = debug; // window.location.search.indexOf('debug=true') !== -1;
    this.timeCount = 0;
    this.renderTime = 0;
    this.perfLoggingInterval = 100;

    // EventEmitter + Comlink workaround: drop .on() return value
    const _on = this.on;
    this.on = (...args) => {
      _on.apply(this, args);
    };
  }

  togglePause() {
    this.paused = !this.paused;
    this.emit('playerStateUpdate', { paused: this.paused });
    return this.paused;
  }

  isPaused() {
    return this.paused;
  }

  resume() {
    this.paused = false;
  }

  canPlay(fileExtension) {
    return this.fileExtensions.indexOf(fileExtension.toLowerCase()) !== -1;
  }

  loadData(data, filename) {
    throw Error('Player.loadData() must be implemented.');
  }

  stop() {
    throw Error('Player.stop() must be implemented.');
  }

  isPlaying() {
    throw Error('Player.isPlaying() must be implemented.');
  }

  getTempo() { // TODO: rename all tempo to speed
    console.warn('Player.getTempo() not implemented for this player.');
    return 1;
  }

  setTempo() {
    console.warn('Player.setTempo() not implemented for this player.');
  }

  setFadeout(startMs) {
    console.warn('Player.setFadeout() not implemented for this player.');
  }

  getDurationMs() {
    console.warn('Player.getDurationMs() not implemented for this player.');
    return 5000;
  }

  getPositionMs() {
    console.warn('Player.getPositionMs() not implemented for this player.');
    return 0;
  }

  seekMs(ms) {
    console.warn('Player.seekMs() not implemented for this player.');
  }

  getVoiceName(index) {
    console.warn('Player.getVoiceName() not implemented for this player.');
  }

  getVoiceMask() {
    console.warn('Player.getVoiceMask() not implemented for this player.');
    return [];
  }

  setVoiceMask() {
    console.warn('Player.setVoiceMask() not implemented for this player.');
  }

  getNumVoices() {
    return 0;
  }

  getNumSubtunes() {
    return 1;
  }

  getSubtune() {
    return 0;
  }

  getMetadata() {
    return {
      title: null,
      author: null,
    }
  }

  getParamDefs() {
    return [];
  }

  getBasePlayerState() {
    return {
      metadata: this.getMetadata(),
      durationMs: this.getDurationMs(),
      positionMs: this.getPositionMs(),
      numVoices: this.getNumVoices(),
      voiceMask: this.getVoiceMask(),
      isStopped: false,
    };
  }

  suspend() {
    this.stopped = true;
    this.paused = true;
  }

  setAudioProcess(fn) {
    if (typeof fn !== 'function') {
      throw Error('AudioProcess must be a function.');
    }
    console.log('setAudioProcess this:', this);
    this._innerAudioProcess = fn;
  }

  process(output) {
    // TODO: AudioWorkletGlobalScope's currentTime doesn't work for performance measurement
    // TODO: replace all refs to "global" or "window" with "globalThis"
    const start = global.performance ? performance.now() : 0;
    this._innerAudioProcess(output);
    const end = global.performance ? performance.now() : 0;

    if (this.debug) {
      this.renderTime += end - start;
      this.timeCount++;
      if (this.timeCount >= this.perfLoggingInterval) {
        const cost = this.renderTime / this.timeCount;
        const budget = 1000 * this.bufferSize / this.sampleRate;
        console.log(
          '[%s] %s ms to render %d frames (%s ms) (%s% utilization)',
          this.constructor.name,
          cost.toFixed(2),
          this.bufferSize,
          budget.toFixed(1),
          (100 * cost / budget).toFixed(1),
        );
        this.renderTime = 0.0;
        this.timeCount = 0;
      }
    }
  }

  // muteAudioDuringCall(audioNode, fn) {
  //   if (audioNode && audioNode.context.state === 'running' && this.paused === false) {
  //     const audioprocess = audioNode.onaudioprocess;
  //     // Workaround to eliminate stuttering:
  //     // Temporarily swap the audio process callback, and do the
  //     // expensive operation only after buffer is filled with silence
  //     audioNode.onaudioprocess = function (e) {
  //       for (let i = 0; i < e.outputBuffer.numberOfChannels; i++) {
  //         e.outputBuffer.getChannelData(i).fill(0);
  //       }
  //       fn();
  //       audioNode.onaudioprocess = audioprocess;
  //     };
  //   } else {
  //     fn();
  //   }
  // }

  muteAudioDuringCall(fn) {
    if (typeof setTimeout != 'undefined') {
      // Workaround to eliminate stuttering:
      // Temporarily swap the audio process callback, and do the
      // expensive operation only after buffer is filled with silence
      console.log('stashing the audio process. this:', this);
      const audioprocess = this._innerAudioProcess;
      this.setAudioProcess((channels) => {
        console.log('filling buffer with silence');
        for (let ch = 0; ch < channels.length; ch++) {
          for (let i = 0; i < ch.length; i++) ch[i] = Math.random() - 0.5;
          // channels[ch].fill(Math.random() - 0.5);
        }
      });

      setTimeout(() => {
        console.log('running the expensive function');
        fn();
        console.log('restoring the audio process');
        this.setAudioProcess(audioprocess);
      }, 100); // Enough time to fill the buffer twice. Determined empirically.

    } else {
      fn();
    }
  }

  static metadataFromFilepath(filepath) {
    // Guess metadata from path/filename for MIDI files.
    // Assumes structure:  /Game MIDI/{game}/**/{title}
    //             ...or:  /MIDI/{artist}/**/{title}
    const parts = filepath.split('/');
    const meta = {};
    meta.title = parts.pop();
    meta.system = parts.shift();
    if (meta.system === 'Game MIDI') {
      meta.game = parts.shift();
    } else {
      meta.artist = parts.shift();
    }
    return meta;
  }
}
