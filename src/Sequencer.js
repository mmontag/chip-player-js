import promisify from "./promisifyXhr";
const CATALOG_PREFIX = 'https://gifx.co/music/';

export default class Sequencer {
  constructor(players, onStateUpdate, onError) {
    this.playSong = this.playSong.bind(this);
    this.getPlayer = this.getPlayer.bind(this);
    this.onPlayerStateUpdate = this.onPlayerStateUpdate.bind(this);
    this.playContext = this.playContext.bind(this);
    this.advanceSong = this.advanceSong.bind(this);
    this.nextSong = this.nextSong.bind(this);
    this.prevSong = this.prevSong.bind(this);
    this.prevSubtune = this.prevSubtune.bind(this);
    this.nextSubtune = this.nextSubtune.bind(this);
    this.toggleShuffle = this.toggleShuffle.bind(this);
    this.getCurrContext = this.getCurrContext.bind(this);
    this.getCurrIdx = this.getCurrIdx.bind(this);
    this.setPlayers = this.setPlayers.bind(this);

    this.player = null;
    this.players = players;
    this.onStateUpdate = onStateUpdate;
    this.onPlayerError = onError;

    this.currIdx = 0;
    this.context = null;
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
    if (isStopped && this.context) {
      this.nextSong();
    }
    // State update bubbles to owner
    // ignore stopped for now TODO
    this.onStateUpdate();
  }

  playContext(context, index) {
    this.currIdx = index || 0;
    this.context = context;
    this.playSong(context[index]);
  }

  toggleShuffle() {
    this.shuffle = !this.shuffle;
  }

  advanceSong(direction) {
    this.currIdx += direction;

    if (this.currIdx < 0 || this.currIdx >= this.context.length) {
      this.currIdx = 0;
      this.context = null;
      this.player.stop();
      this.onStateUpdate(true);
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

  playSong(url) {
    // Stop and unset current player
    if (this.player !== null) {
      this.player.suspend();
    }
    // this.player = null;
    //

    // Normalize url - paths are assumed to live under CATALOG_PREFIX
    url = url.startsWith('http') ? url : CATALOG_PREFIX + url;

    // Update application URL (window.history API)
    const filepath = url.replace(CATALOG_PREFIX, '');
    const ext = url.split('.').pop().toLowerCase();
    // const pathParts = url.split('/');
    // pathParts.pop();
    // const urlParams = {
    //   ...queryString.parse(window.location.search.substr(1)),
    //   play: filepath,
    // };
    // delete urlParams.t;
    // const stateUrl = '?' + queryString.stringify(urlParams)
    //   .replace(/%20/g, '+') // I don't care about escaping these characters
    //   .replace(/%2C/g, ',')
    //   .replace(/%2F/g, '/');
    // window.history.replaceState(null, '', stateUrl);

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

    // // Fetch artwork for this file (cancelable request)
    // const imageUrl = [...pathParts, 'image.jpg'].join('/');
    // if (this.imageRequest) this.imageRequest.abort();
    // this.imageRequest = promisify(new XMLHttpRequest());
    // this.imageRequest.open('HEAD', imageUrl);
    // console.log('requesting image', imageUrl);
    // this.imageRequest.send()
    //   .then(xhr => {
    //     console.log(xhr.status);
    //     if (xhr.status >= 200 && xhr.status < 400) {
    //       console.log('set state', {imageUrl: imageUrl});
    //       this.setState({imageUrl: imageUrl});
    //     }
    //   })
    //   .catch(e => {
    //     this.setState({imageUrl: null});
    //   });

    // Fetch the song file (cancelable request)
    // Cancel any outstanding request so that playback doesn't happen out of order
    if (this.songRequest) this.songRequest.abort();
    this.songRequest = promisify(new XMLHttpRequest());
    this.songRequest.responseType = 'arraybuffer';
    this.songRequest.open('GET', url);
    this.songRequest.send()
      .then(xhr => xhr.response)
      .then(buffer => {
        let uint8Array;
        uint8Array = new Uint8Array(buffer);

        try {
          this.player.loadData(uint8Array, filepath);
        } catch (e) {
          this.onPlayerError(e.message);
          return;
        }

        const numVoices = this.player.getNumVoices();
        this.player.setTempo(this.tempo);
        this.player.setVoices([...Array(numVoices)].fill(true));

        this.onPlayerStateUpdate();
      });
  }
}
