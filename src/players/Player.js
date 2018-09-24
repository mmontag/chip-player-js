//
// Player can be viewed as a state machine with
// 3 states (playing, paused, stopped) and 5 transitions
// (open, pause, unpause, seek, close).
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

  getSubtune() {
    return 0;
  }

  getMetadata() {
    return {
      title: '[Not implemented]',
      author: '[Not implemented]',
    }
  }

  getParameters() {
    return [];
  }

  static muteAudioDuringCall(audioNode, fn) {
    if (audioNode) {
      const audioprocess = audioNode.onaudioprocess;
      // Workaround to eliminate stuttering:
      // Temporarily swap the audio process callback, and do the
      // expensive operation only after buffer is filled with silence
      audioNode.onaudioprocess = function(e) {
        for (let i = 0; i < e.outputBuffer.numberOfChannels; i++) {
          e.outputBuffer.getChannelData(i).fill(0);
        }
        fn();
        audioNode.onaudioprocess = audioprocess;
      };
    } else {
      fn();
    }
  }
}