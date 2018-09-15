export default class Player {
  constructor() {
    this.paused = false;
    this.fileExtensions = [];
  }

  togglePause() {
    this.paused = !this.paused;
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

  stop() {
    throw Error('Player.stop() must be implemented.');
  }

  restart() {
    throw Error('Player.restart() must be implemented.');
  }

  isPlaying() {
    throw Error('Player.isPlaying() must be implemented.');
  }

  setTempo() {
    console.warn('Player.setTempo() not implemented for this player.');
  }

  setVoices() {
    console.warn('Player.setVoices() not implemented for this player.');
  }

  setFadeout(startMs) {
    console.warn('Player.setFadeout() not implemented for this player.');
  }

  getNumVoices() {
    return 0;
  }

  getNumSubtunes() {
    return 1;
  }

  getMetadata() {
    return {
      title: '[Not implemented]',
      author: '[Not implemented]',
    }
  }
}