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

  isReady() {
    return this.isReady;
  }

  canPlay(fileExtension) {
    return this.fileExtensions.indexOf(fileExtension.toLowerCase()) !== -1;
  }

  stop() {
    throw Error('Player.stop() must be implemented.');
  }
}