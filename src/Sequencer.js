import axios from 'redaxios';
import autoBind from 'auto-bind';
import EventEmitter from 'events';
import shuffle from 'lodash/shuffle';
import { getUrlFromFilepath, songRef } from './util';

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

    this.currIdx = 0;
    this.context = null;
    this.currSongPath = null;
    this.currSongRef = null;
    this.shuffle = SHUFFLE_OFF;
    this.shuffleOrder = [];
    this.repeat = REPEAT_OFF;
    this.playerErrorAdvanceTimer = null;
    this.currSongBuffer = null;

    this.players.forEach(player => {
      player.on('playerStateUpdate', state => {
        if (player === this.player) this.handlePlayerStateUpdate(state);
      });
      player.on('playerError', error => {
        if (player === this.player) this.handlePlayerError(error);
      });
      player.on('playerSongEnd', () => {
        if (player === this.player) this.handlePlayerSongEnd();
      });
    });
  }

  eject() {
    this.currIdx = 0;
    this.context = null;
    this.currSongPath = null;
    this.currSongRef = null;
    this.currSongBuffer = null;
    if (this.player) {
      this.player.stop();
      this.player = null;
    }
    this.emit('sequencerStateUpdate', {
      songPath: null,
      songBuffer: null,
      ejected: true,
    });
  }

  isEjected() {
    return this.player === null;
  }

  handlePlayerSongEnd() {
    this.currSongPath = null;
    this.currSongRef = null;
    this.currSongBuffer = null;
    if (!this.context) return;
    // Repeat One is a transport mode for an *automatic* end: stay on this
    // entry and reload it (the stop-and-reload engines). Manual Next/Previous
    // must still navigate; advanceSong() no longer special-cases the mode.
    if (this.repeat === REPEAT_ONE) {
      this.playCurrentSong();
    } else {
      this.nextSong();
    }
  }

  handlePlayerError(e) {
    this.emit('playerError', e);
    if (this.context && this.player) {
      clearTimeout(this.playerErrorAdvanceTimer);
      this.playerErrorAdvanceTimer = setTimeout(this.nextSong, ERROR_DELAY_MS);
    } else {
      this.eject();
    }
  }

  handlePlayerStateUpdate(playerState) {
    if (this.playerErrorAdvanceTimer) {
      console.debug('Cancelling auto-advance due to playerState update.');
      clearTimeout(this.playerErrorAdvanceTimer);
      this.playerErrorAdvanceTimer = null;
    }

    // Keep the transient SongRef in sync for metadata/share/favorites if a
    // player ever reports a different sub-tune than the one we asked for
    // (a tail restart, or an engine that advances internally), but never
    // mutate the context: its entries are the navigation list.
    const { subtune } = playerState;
    if (this.currSongRef && subtune != null && this.currSongRef.subtune !== subtune) {
      this.currSongRef = { ...this.currSongRef, subtune };
    }
    this.emit('sequencerStateUpdate', {
      songPath: this.currSongPath,
      songRef: this.currSongRef,
      songBuffer: this.currSongBuffer,
      ejected: false,
      ...playerState,
    });
  }

  /**
   * Begin playing a context (an ordered list of SongRefs).
   * `subtune` is an optional override for the first entry, used by share links
   * (?play=...&subtune=N); normally the sub-tune comes from the SongRef.
   */
  playContext(context, index = 0, subtune = null) {
    if (!context || context.length === 0) {
      this.emit('playerError', 'No songs found to play.');
      return;
    }
    this.currIdx = index;
    this.context = context.map(item => songRef(item));
    if (this.shuffle === SHUFFLE_ON) {
      this.setShuffle(this.shuffle);
    }
    const ref = this.currContextRef();
    if (ref && subtune != null) {
      ref.subtune = subtune;
    }
    this.playCurrentSong();
  }

  currContextRef() {
    let idx = this.currIdx;
    if (this.shuffle === SHUFFLE_ON) {
      idx = this.shuffleOrder[idx];
    }
    return this.context ? this.context[idx] : null;
  }

  playCurrentSong() {
    this.playSong(this.currContextRef());
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
    if (this.player) this.player.setLooping(repeat === REPEAT_ONE);
  }

  advanceSong(direction) {
    if (this.context == null) return;
    this.currIdx += direction;

    if (this.currIdx < 0 || this.currIdx >= this.context.length) {
      // An empty context has nothing to wrap to: REPEAT_ALL's modulo would be
      // NaN, and playSong(undefined) returns without stopping the player, so a
      // deleted song would keep playing against an empty list. Eject instead.
      if (this.repeat === REPEAT_ALL && this.context.length > 0) {
        this.currIdx = (this.currIdx + this.context.length) % this.context.length;
        this.playCurrentSong();
      } else {
        console.debug('Sequencer.advanceSong(direction=%s) %s passed end of context length %s',
          direction, this.currIdx, this.context.length);
        this.eject();
      }
    } else {
      this.playCurrentSong();
    }
  }

  nextSong() {
    this.advanceSong(1);
  }

  prevSong() {
    if (this.context == null) return;
    // At the first entry there is nothing before the current song to play.
    // advanceSong(-1) would walk off the context and eject it (except under
    // Repeat All, which wraps), tearing down the player for a control that
    // should restart what is already loaded. Keep the context and replay the
    // first entry; Repeat All retains its wrap to the last entry.
    if (this.currIdx <= 0 && this.repeat !== REPEAT_ALL) {
      if (this.context.length === 0) {
        this.eject();
      } else {
        this.playCurrentSong();
      }
      return;
    }
    this.advanceSong(-1);
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

  getCurrSongRef() {
    return this.currSongRef;
  }

  getCurrSongBuffer() {
    return this.currSongBuffer;
  }

  /**
   * Play a single song. Accepts a SongRef (preferred) or a bare path string.
   */
  playSong(songRefOrPath) {
    const ref = songRef(songRefOrPath);
    if (!ref) return;
    const { path: filepath, subtune } = ref;

    this.currSongRef = ref;
    this.currSongBuffer = null;
    if (this.player !== null) {
      this.player.stop();
    }

    // Find a player that can play this filetype
    const ext = filepath.slice(filepath.lastIndexOf('.') + 1).toLowerCase();
    let player = this.players.find(p => p.canPlay(ext));
    if (!player) {
      this.emit('playerError', `The file format ".${ext}" was not recognized.`);
      return;
    }
    this.player = player;
    this.player.setLooping(this.repeat === REPEAT_ONE);
    const loadSignal = player.beginLoad();
    player.fetchCatalogMetadata(filepath, loadSignal, subtune);

    if (filepath.startsWith('local/')) {
      const buffer = this.localFilesManager.read(filepath);
      this.currSongPath = filepath;
      this.playSongBuffer(player, filepath, buffer, subtune, loadSignal);
    } else {
      // Normalize url - paths are assumed to live under CATALOG_PREFIX
      const url = filepath.startsWith('http') ? filepath : getUrlFromFilepath(filepath);

      // Fetch the song file (cancelable request)
      axios.get(url, {
          responseType: 'arrayBuffer',
          // redaxios does not forward signal to fetch.
          fetch: (url, options) => fetch(url, { ...options, signal: loadSignal }),
        })
        .then(res => {
          if (loadSignal.aborted) return;
          this.currSongPath = filepath;
          this.playSongBuffer(player, filepath, res.data, subtune, loadSignal);
        })
        .catch(e => {
          if (loadSignal.aborted) return;
          console.error(e);
          this.handlePlayerError(`${e.message}: ${filepath}`);
        });
    }
  }

  async playSongBuffer(player, filepath, buffer, subtune = 0, loadSignal = player.beginLoad()) {
    this.currSongBuffer = buffer;
    const uint8Array = new Uint8Array(buffer);
    const persistedSettings = this.getSettings();
    try {
      await player.loadData(uint8Array, filepath, persistedSettings, subtune);
    } catch (e) {
      if (loadSignal.aborted) return;
      console.error(`Unable to play ${filepath}.`, e);
      this.handlePlayerError(`Unable to play ${filepath} (${e.message}).`);
      return;
    }
    if (loadSignal.aborted) return;
    // A superseded load can resolve without initialising a voice mask; never
    // let that crash the load path.
    const voiceMask = player.getVoiceMask();
    if (voiceMask) voiceMask.fill(true);
  }
}
