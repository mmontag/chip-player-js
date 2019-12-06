import promisify from "./promisifyXhr";
import {CATALOG_PREFIX} from "./config";

export default class Sequencer {
  constructor(players, onSequencerStateUpdate, onError) {
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
    this.setPlayers = this.setPlayers.bind(this);
    this.setShuffle = this.setShuffle.bind(this);

    this.player = null;
    this.players = players;
    this.onSequencerStateUpdate = onSequencerStateUpdate;
    this.onPlayerError = onError;

    this.currIdx = 0;
    this.context = null;
    this.currUrl = null;
    this.tempo = 1;
    this.shuffle = false;
    this.songRequest = null;
  }

  setPlayers(players) {
    this.players = players;
    this.players.forEach(player => {
      player.setOnPlayerStateUpdate(this.onPlayerStateUpdate);
    });
  }

  onPlayerStateUpdate(isStopped) {
    console.debug('Sequencer.onPlayerStateUpdate(isStopped=%s)', isStopped);
    if (isStopped) {
      this.currUrl = null;
      if (this.context) {
        this.nextSong();
      }
    } else {
      this.onSequencerStateUpdate(false);
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

  advanceSong(direction) {
    this.currIdx += direction;

    if (this.currIdx < 0 || this.currIdx >= this.context.length) {
      console.debug('Sequencer.advanceSong(direction=%s) %s passed end of context length %s',
        direction, this.currIdx, this.context.length);
      this.currIdx = 0;
      this.context = null;
      this.player.stop();
      this.onSequencerStateUpdate(true);
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

  playSong(url) {
    if (this.player !== null) {
      this.player.suspend();
    }

    // Normalize url - paths are assumed to live under CATALOG_PREFIX
    url = url.startsWith('http') ? url : CATALOG_PREFIX + url;

    // Find a player that can play this filetype
    const ext = url.split('.').pop().toLowerCase();
    for (let i = 0; i < this.players.length; i++) {
      if (this.players[i].canPlay(ext)) {
        this.player = this.players[i];
        break;
      }
    }
    if (this.player === null) {
      this.onPlayerError(`The file format ".${ext}" was not recognized.`);
      return;
    }

    // Fetch the song file (cancelable request)
    // Cancel any outstanding request so that playback doesn't happen out of order
    if (this.songRequest) this.songRequest.abort();
    this.songRequest = promisify(new XMLHttpRequest());
    this.songRequest.responseType = 'arraybuffer';
    this.songRequest.open('GET', url);
    this.songRequest.send()
      .then(xhr => xhr.response)
      .then(buffer => {
        this.currUrl = url;
        const filepath = url.replace(CATALOG_PREFIX, '');
        this.playSongBuffer(filepath, buffer)
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
      this.onPlayerError(`The file format ".${ext}" was not recognized.`);
      return;
    }

    this.context = [];
    this.currUrl = null;
    this.playSongBuffer(filepath, songData);
  }

  playSongBuffer(filepath, buffer) {
    let uint8Array;
    uint8Array = new Uint8Array(buffer);

    try {
      this.player.loadData(uint8Array, filepath);
    } catch (e) {
      this.onPlayerError(e.message);
      return;
    }
    this.onPlayerError(null);

    const numVoices = this.player.getNumVoices();
    this.player.setTempo(this.tempo);
    this.player.setVoices([...Array(numVoices)].fill(true));

    console.debug('Sequencer.playSong(...) song request completed');
  }
}
