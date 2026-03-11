import promisify from "./promisify-xhr";
import shuffle from 'lodash/shuffle';
import EventEmitter from 'events';
import autoBind from 'auto-bind';
import { getUrlFromFilepath } from './util';

export const REPEAT_OFF = 0;
export const REPEAT_ALL = 1;
export const REPEAT_ONE = 2;
export const NUM_REPEAT_MODES = 3;
export const REPEAT_LABELS = ['Off', 'All', 'One'];

export const SHUFFLE_OFF = 0;
export const SHUFFLE_ON = 1;
export const NUM_SHUFFLE_MODES = 2;
export const SHUFFLE_LABELS = ['Off', 'On '];
const ERROR_DELAY_MS = 1000;

export default class Sequencer extends EventEmitter {
  constructor(players, localFilesManager, getSettings) {
    super();
    autoBind(this);

    this.player = null;
    this.players = players;
    this.localFilesManager = localFilesManager;
    this.getSettings = getSettings;
    // this.onSequencerStateUpdate = onSequencerStateUpdate;
    // this.onPlayerError = onError;

    this.currIdx = 0;
    this.context = null;
    this.currSongPath = null;
    this.shuffle = SHUFFLE_OFF;
    this.shuffleOrder = [];
    this.songRequest = null;
    this.repeat = REPEAT_OFF;
    this.playerErrorAdvanceTimer = null;

    this.players.forEach(player => {
      player.on('playerStateUpdate', this.handlePlayerStateUpdate);
      player.on('playerError', this.handlePlayerError);
    });
  }

  handlePlayerError(e) {
    this.emit('playerError', e);
    if (this.context && this.player) {
      this.playerErrorAdvanceTimer = setTimeout(this.nextSong, ERROR_DELAY_MS);
    } else {
      this.emit('sequencerStateUpdate', { isEjected: true });
    }
  }

  handlePlayerStateUpdate(playerState) {
    const { isStopped } = playerState;
    console.debug('Sequencer.handlePlayerStateUpdate(isStopped=%s)', isStopped);
    if (this.playerErrorAdvanceTimer) {
      console.debug('Cancelling auto-advance due to playerState update.');
      clearTimeout(this.playerErrorAdvanceTimer);
      this.playerErrorAdvanceTimer = null;
    }

    if (isStopped) {
      this.currSongPath = null;
      if (this.context) {
        this.nextSong();
      }
    } else {
      this.emit('sequencerStateUpdate', {
        songPath: this.currSongPath,
        hasPlayer: true,
        // TODO: combine isEjected and hasPlayer
        isEjected: false,
        ...playerState,
      });
    }
  }

  playContext(context, index = 0, subtune = 0) {
    this.currIdx = index;
    this.context = context;
    if (this.shuffle === SHUFFLE_ON) {
      this.setShuffle(this.shuffle);
    }
    this.playCurrentSong(subtune);
  }

  playCurrentSong(subtune = 0) {
    let idx = this.currIdx;
    if (this.shuffle === SHUFFLE_ON) {
      idx = this.shuffleOrder[idx];
      console.log('Shuffle (%s): %s', this.currIdx, idx);
    }
    this.playSong(this.context[idx], subtune);
  }

  playSonglist(urls) {
    this.playContext(urls, 0);
  }

  toggleShuffle() {
    this.setShuffle(!this.shuffle);
  }

  setShuffle(shuff) {
    this.shuffle = shuff;
    if (this.shuffle === SHUFFLE_ON && this.context) {
      // Generate a new shuffle order.
      // Insert current play index at the beginning.
      this.shuffleOrder = [this.currIdx, ...shuffle(this.context.map((_, i) => i).filter(i => i !== this.currIdx))];
      this.currIdx = 0;
    } else if (this.shuffleOrder) {
      // Restore linear play sequence at current shuffle position.
      if (this.shuffleOrder[this.currIdx] !== null) {
        this.currIdx = this.shuffleOrder[this.currIdx];
      }
    }
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
        this.playCurrentSong();
      } else {
        console.debug('Sequencer.advanceSong(direction=%s) %s passed end of context length %s',
          direction, this.currIdx, this.context.length);
        this.currIdx = 0;
        this.context = null;
        this.player.stop();
        this.player = null;
        this.emit('sequencerStateUpdate', { isEjected: true });
      }
    } else {
      this.playCurrentSong();
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
    return this.shuffle ? this.shuffleOrder[this.currIdx] : this.currIdx;
  }

  getCurrSongPath() {
    return this.currSongPath;
  }

  getSubtune() {
    return this.player.getSubtune();
  }

  playSong(filepath, subtune = 0) {
    if (this.player !== null) {
      this.player.suspend();
    }

    // Find a player that can play this filetype
    const ext = filepath.slice(filepath.lastIndexOf('.') + 1).toLowerCase();
    let player = this.players.find(p => p.canPlay(ext));
    if (player === null) {
      this.emit('playerError', `The file format ".${ext}" was not recognized.`);
      return;
    }
    this.player = player;

    if (filepath.startsWith('local/')) {
      const buffer = this.localFilesManager.read(filepath);
      this.currSongPath = filepath;
      this.playSongBuffer(player, filepath, buffer, subtune);
    } else {
      // Normalize url - paths are assumed to live under CATALOG_PREFIX
      const url = filepath.startsWith('http') ? filepath : getUrlFromFilepath(filepath);

      // Fetch the song file (cancelable request)
      // Cancel any outstanding request so that playback doesn't happen out of order
      if (this.songRequest) this.songRequest.abort();
      this.songRequest = promisify(new XMLHttpRequest());
      this.songRequest.responseType = 'arraybuffer';
      this.songRequest.open('GET', url);
      this.songRequest.send()
        .then(xhr => xhr.response)
        .then(buffer => {
          this.currSongPath = filepath;
          this.playSongBuffer(player, filepath, buffer, subtune)
        })
        .catch(e => {
          this.handlePlayerError(e.message || `HTTP ${e.status} ${e.statusText} ${filepath}`);
        });
    }
  }

  async playSongBuffer(player, filepath, buffer, subtune = 0) {
    let uint8Array;
    uint8Array = new Uint8Array(buffer);
    const persistedSettings = this.getSettings();
    try {
      await player.loadData(uint8Array, filepath, persistedSettings, subtune);
    } catch (e) {
      this.handlePlayerError(`Unable to play ${filepath} (${e.message}).`);
      return;
    }
    const numVoices = player.getNumVoices();
    player.setVoiceMask([...Array(numVoices)].fill(true));
  }
}
