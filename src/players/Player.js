export default class Player {
  constructor() {
    this.paused = false;
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
}