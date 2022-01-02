// import promisify from "./promisify-xhr";
import { CATALOG_PREFIX } from './config';
import EventEmitter from 'events';
import * as Comlink from 'comlink';

// import  { EventEmitter } from 'quackamole-event-emitter';
// import { EventEmitter } from '@billjs/event-emitter';
// import * as EventEmitter from 'event-emitter';
// import EventEmitter from 'eventemitter3';

export const REPEAT_OFF = 0;
export const REPEAT_ALL = 1;
export const REPEAT_ONE = 2;
export const NUM_REPEAT_MODES = 3;
export const REPEAT_LABELS = ['Off', 'All', 'One'];

export default class Sequencer extends EventEmitter {
  constructor(players, fetch) {
    super();

    this.playSong = this.playSong.bind(this);
    this.playSongBuffer = this.playSongBuffer.bind(this);
    this.playSongFile = this.playSongFile.bind(this);
    this.getPlayer = this.getPlayer.bind(this);
    this.onPlayerStateUpdate = this.onPlayerStateUpdate.bind(this);
    this.playContext = this.playContext.bind(this);
    this.advanceSong = this.advanceSong.bind(this);
    this.nextSong = this.nextSong.bind(this);
    this.prevSong = this.prevSong.bind(this);
    this.prevSubtune = this.prevSubtune.bind(this);
    this.nextSubtune = this.nextSubtune.bind(this);
    this.toggleShuffle = this.toggleShuffle.bind(this);
    this.getCurrUrl = this.getCurrUrl.bind(this);
    this.getCurrContext = this.getCurrContext.bind(this);
    this.getCurrIdx = this.getCurrIdx.bind(this);
    this.setShuffle = this.setShuffle.bind(this);

    this.fetch = fetch;
    this.player = null;
    this.players = players;

    this.currIdx = 0;
    this.context = null;
    this.currUrl = null;
    this.shuffle = false;
    this.songRequest = null;
    this.repeat = REPEAT_OFF;

    // Inside AudioWorklet
    // this.players.forEach(player => {
    //   player.on('playerStateUpdate', this.onPlayerStateUpdate);
    // });

    // Outside AudioWorklet
    setTimeout(() => {
      this.players.forEach(player => {
        player.on('playerStateUpdate', Comlink.proxy((e) => {
          this.onPlayerStateUpdate(e)
        }));
      });
    }, 2000);

    // EventEmitter + Comlink workaround: drop .on() return value
    // TODO: this can be removed when Sequencer is moved outside of the AudioWorklet
    const _on = this.on;
    this.on = (...args) => {
      _on.apply(this, args);
    };
  }

  onPlayerStateUpdate(playerState) {
    const { isStopped } = playerState;
    console.debug('Sequencer.onPlayerStateUpdate(isStopped=%s)', isStopped);
    if (isStopped) {
      this.currUrl = null;
      if (this.context) {
        this.nextSong();
      }
    } else {
      this.emit('sequencerStateUpdate', {
        // metadata: this.getMetadata(),
        // numVoices: this.getNumVoices(),
        // durationMs: this.getDurationMs(),
        // paramDefs: this.getParamDefs(),
        ...playerState,
        url: this.currUrl,
        isPaused: false,
        hasPlayer: true,
        // TODO: combine isEjected and hasPlayer
        isEjected: false,
      });
    }
  }

  playContext(context, index = 0) {
    this.currIdx = index;
    this.context = context;
    this.playSong(context[index]);
  }

  playSonglist(urls) {
    this.playContext(urls, 0);
  }

  toggleShuffle() {
    this.setShuffle(!this.shuffle);
  }

  setShuffle(shuffle) {
    this.shuffle = !!shuffle;
  }

  setRepeat(repeat) {
    this.repeat = repeat;
  }

  advanceSong(direction) {
    if (this.context == null) return;

    if (this.repeat !== REPEAT_ONE) {
      this.currIdx += direction;
    }

    if (this.currIdx < 0 || this.currIdx >= this.context.length) {
      if (this.repeat === REPEAT_ALL) {
        this.currIdx = (this.currIdx + this.context.length) % this.context.length;
        this.playSong(this.context[this.currIdx]);
      } else {
        console.debug('Sequencer.advanceSong(direction=%s) %s passed end of context length %s',
          direction, this.currIdx, this.context.length);
        this.currIdx = 0;
        this.context = null;
        this.player.stop();
        this.emit('sequencerStateUpdate', { isEjected: true });
      }
    } else {
      this.playSong(this.context[this.currIdx]);
    }
  }

  nextSong() {
    this.advanceSong(1);
  }

  prevSong() {
    this.advanceSong(-1);
  }

  playSubtune(subtune) {
    this.player.playSubtune(subtune);
  }

  prevSubtune() {
    const subtune = this.player.getSubtune() - 1;
    if (subtune < 0) return;
    this.playSubtune(subtune);
  }

  nextSubtune() {
    const subtune = this.player.getSubtune() + 1;
    if (subtune >= this.player.getNumSubtunes()) return;
    this.playSubtune(subtune);
  }

  getPlayer() {
    return this.player;
  }

  getCurrContext() {
    return this.context;
  }

  getCurrIdx() {
    return this.currIdx;
  }

  getCurrUrl() {
    return this.currUrl;
  }

  async playSong(url) {
    if (this.player !== null) {
      this.player.suspend();
    }

    // Normalize url - paths are assumed to live under CATALOG_PREFIX
    url = url.startsWith('http') ? url : CATALOG_PREFIX + url;

    // Find a player that can play this filetype
    const ext = url.split('.').pop().toLowerCase();
    for (let i = 0; i < this.players.length; i++) {
      if (await this.players[i].canPlay(ext)) {
        this.player = this.players[i];
        break;
      }
    }
    if (this.player === null) {
      this.emit('playerError', `The file format ".${ext}" was not recognized.`);
      return;
    }

    // Fetch the song file (cancelable request)
    // Cancel any outstanding request so that playback doesn't happen out of order
    // if (this.songRequest) this.songRequest.abort();
    // this.songRequest = promisify(new XMLHttpRequest());
    // this.songRequest.responseType = 'arraybuffer';
    // this.songRequest.open('GET', url);
    // this.songRequest.send()
    //   .then(xhr => xhr.response)
    this.fetch(url)
      .then(buffer => {
        this.currUrl = url;
        const filepath = url.replace(CATALOG_PREFIX, '');
        this.playSongBuffer(filepath, buffer);
        // TODO: listen for playerstateupdate and bubble as seqStateUpd.
        // this.emit('sequencerStateUpdate', {
        //   url,
        //   isPaused: false,
        //   metadata: this.getMetadata(),
        //   numVoices: this.getNumVoices(),
        //   durationMs: this.getDurationMs(),
        //   paramDefs: this.getParamDefs(),
        //   hasPlayer: true,
        //   // TODO: combine isEjected and hasPlayer
        //   isEjected: false,
        // });
      })
      .catch(e => {
        // TODO: recover from this error
        this.emit('sequencerStateUpdate', { isEjected: true });
        this.player = null;
        const message = e.message || `${e.status} ${e.statusText}`;
        console.error(e);
        this.emit('playerError', message);
      });
  }

  playSongFile(filepath, songData) {
    if (this.player !== null) {
      this.player.suspend();
    }

    const ext = filepath.split('.').pop().toLowerCase();

    // Find a player that can play this filetype
    for (let i = 0; i < this.players.length; i++) {
      if (this.players[i].canPlay(ext)) {
        this.player = this.players[i];
        break;
      }
    }
    if (this.player === null) {
      this.emit('playerError', `The file format ".${ext}" was not recognized.`);
      return;
    }

    this.context = [];
    this.currUrl = null;
    this.playSongBuffer(filepath, songData);
  }

  playSongBuffer(filepath, buffer) {
    let uint8Array;
    uint8Array = new Uint8Array(buffer);
    this.player.setTempo(1);
    this.player.loadData(uint8Array, filepath);
    const numVoices = this.player.getNumVoices();
    this.player.setVoiceMask([...Array(numVoices)].fill(true));

    console.debug('Sequencer.playSong(...) song request completed');
  }

  hasPlayer = () => {
    return this.player !== null;
  }
  getDurationMs = () => {
    if (this.player) return Promise.resolve(this.player.getDurationMs());
  }
  getMetadata = () => {
    if (this.player) return Promise.resolve(this.player.getMetadata());
  }
  getNumSubtunes = () => {
    if (this.player) return Promise.resolve(this.player.getNumSubtunes());
  }
  getNumVoices = () => {
    if (this.player) return Promise.resolve(this.player.getNumVoices());
  }
  getParamDefs = () => {
    if (this.player) return Promise.resolve(this.player.getParamDefs());
  }
  getParameter = (id) => {
    if (this.player) return Promise.resolve(this.player.getParameter(id));
  }
  getPositionMs = () => {
    if (this.player) return Promise.resolve(this.player.getPositionMs());
  }
  getSubtune = () => {
    if (this.player) return Promise.resolve(this.player.getSubtune());
  }
  getTempo = () => {
    if (this.player) return Promise.resolve(this.player.getTempo());
  }
  getVoiceName = (idx) => {
    if (this.player) return Promise.resolve(this.player.getVoiceName(idx));
  }
  getVoiceMask = () => {
    if (this.player) return Promise.resolve(this.player.getVoiceMask());
  }
  isPaused = () => {
    if (this.player) return Promise.resolve(this.player.isPaused());
  }
  isPlaying = () => {
    if (this.player) return Promise.resolve(this.player.isPlaying());
  }

  process = (output) => {
    if (this.player) return Promise.resolve(this.player.process(output));
  }

  seekMs = (ms) => {
    if (this.player) return Promise.resolve(this.player.seekMs(ms));
  }
  setParameter = (id, value) => {
    if (this.player) return Promise.resolve(this.player.setParameter(id, value));
  }
  setTempo = (tempo) => {
    if (this.player) return Promise.resolve(this.player.setTempo(tempo));
  }
  setVoiceMask = (mask) => {
    if (this.player) return Promise.resolve(this.player.setVoiceMask(mask));
  }
  togglePause = () => {
    if (this.player) return Promise.resolve(this.player.togglePause());
  }
}
