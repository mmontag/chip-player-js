import React, {PureComponent} from 'react';
import Slider from './Slider'
import './App.css';
import songData from './song-data';
import ChipPlayer from './chipplayer';
import GMEPlayer from './players/GMEPlayer';
import XMPPlayer from './players/XMPPlayer';
import MIDIPlayer from './players/MIDIPlayer';

const MAX_VOICES = 32;

class App extends PureComponent {
  constructor(props) {
    super(props);

    this.togglePause = this.togglePause.bind(this);
    this.play = this.play.bind(this);
    this.playSong = this.playSong.bind(this);
    this.handleSliderDrag = this.handleSliderDrag.bind(this);
    this.handleSliderChange = this.handleSliderChange.bind(this);
    this.handleTempoChange = this.handleTempoChange.bind(this);
    this.getSongPos = this.getSongPos.bind(this);
    this.prevSubtune = this.prevSubtune.bind(this);
    this.nextSubtune = this.nextSubtune.bind(this);
    this.playSubtune = this.playSubtune.bind(this);
    this.getTimeLabel = this.getTimeLabel.bind(this);
    this.displayLoop = this.displayLoop.bind(this);
    this.getFadeMs = this.getFadeMs.bind(this);

    const audioContext = new (window.AudioContext || window.webkitAudioContext)();
    console.log('Sample rate: %d hz', audioContext.sampleRate);

    const emscriptenRuntime = new ChipPlayer({
      // Look for .wasm file in web root, not the same location as the app bundle (static/js).
      locateFile: (path, prefix) => {
        if (path.endsWith('.wasm') || path.endsWith('.wast')) return './' + path;
        return prefix + path;
      },
      onRuntimeInitialized: () => {
        this.players = [
          new GMEPlayer(audioContext, emscriptenRuntime),
          new XMPPlayer(audioContext, emscriptenRuntime),
          new MIDIPlayer(audioContext, emscriptenRuntime),
        ];
        this.setState({loading: false});
      },
    });

    this.player = null;

    this.lastTime = (new Date()).getTime();
    this.startedFadeOut = false;
    this.state = {
      loading: true,
      paused: false,
      currentSongMetadata: {},
      currentSongNumVoices: 0,
      currentSongNumSubtunes: 0,
      currentSongSubtune: 0,
      extractedTunes: [],
      currentSongDurationMs: 1000,
      currentSongPositionMs: 0,
      draggedSongPositionMs: -1,
      tempo: 1,
      voices: Array(MAX_VOICES).fill(true),
      voiceNames: Array(MAX_VOICES).fill(''),
    };

    this.displayLoop();
  }

  displayLoop(currentTime) {
    const deltaTime = currentTime - this.lastTime;
    this.lastTime = currentTime;
    if (this.player) {
      const playerPositionMs = this.player.getPositionMs();
      if (this.player.isPlaying() && playerPositionMs) {
        if (this.state.currentSongPositionMs >= this.state.currentSongDurationMs) {
          if (!this.startedFadeOut) {
            // Fade out right now
            console.log('Starting fadeout at', playerPositionMs);
            this.startedFadeOut = true;
            this.player.setFadeout(playerPositionMs);
          }
        } else {
          const currMs = this.state.currentSongPositionMs;
          this.setState({
            currentSongPositionMs: currMs + deltaTime * this.state.tempo,
            currentSongDurationMs: this.player.getDurationMs(),
          });
        }
      }
    }
    requestAnimationFrame(this.displayLoop);
  }

  play() {
    if (this.player !== null) {
      this.player.restart();
      this.setState({
        paused: false,
        currentSongPositionMs: 0,
      });
    }
  }

  playSong(filename) {
    if (this.player !== null) {
      this.player.stop();
    }
    this.player = null;
    const ext = filename.split('.').pop().toLowerCase();
    for (let i = 0; i < this.players.length; i++) {
      if (this.players[i].canPlay(ext)) {
        this.player = this.players[i];
        break;
      }
    }
    if (this.player === null) {
      console.error(`None of the Player engines can handle the ".${ext}" file format.`);
      return;
    }

    fetch(filename).then(response => response.arrayBuffer()).then(buffer => {
      let uint8Array;
      let extractedTunes = [];
      // if (filename.endsWith('.rsn')) {
      //   // Unrar RSN files
      //   const extractedTunes = unrar.createExtractorFromData(buffer)
      //     .extractAll()[1].files
      //     .map(file => file.extract[1])
      //     .filter(arr => arr.byteLength >= 8192);
      //   uint8Array = extractedTunes[0];
      // } else {
      uint8Array = new Uint8Array(buffer);
      // }

      this.player.loadData(uint8Array);
      this.player.setTempo(this.state.tempo);
      this.player.setVoices(this.state.voices);
      this.player.resume();
      const numSubtunes = this.player.getNumSubtunes();
      const numVoices = this.player.getNumVoices();

      this.startedFadeOut = false;
      this.setState({
        paused: false,
        currentSongMetadata: this.player.getMetadata(),
        currentSongDurationMs: this.player.getDurationMs(),
        currentSongNumVoices: numVoices,
        currentSongNumSubtunes: extractedTunes.length || numSubtunes,
        currentSongPositionMs: 0,
        currentSongSubtune: 0,
        extractedTunes: extractedTunes,
        voiceNames: [...Array(numVoices)].map((_, i) => this.player.getVoiceName(i)),
      });
    });
  }

  playSubtune(subtune) {
    // if (this.state.extractedTunes.length) {
    //   // TODO: factor with playSong()
    //   this.player.LoadData(this.state.extractedTunes[subtune]);
    //   this.player.SetTempo(this.state.tempo);
    //   this.player.SetVoices(this.state.voices);
    //   const metadata = parseMetadata();
    //   this.setState({
    //     currentSongSubtune: subtune,
    // currentSongDurationMs: this.player.getDurationMs(),
    //   currentSongMetadata: this.player.getMetadata(),
    //     currentSongPositionMs: 0,
    //   });
    // } else {
    if (this.player.playSubtune(subtune) !== 0) {
      console.error("Could not load track");
      return;
    }
    this.setState({
      currentSongSubtune: subtune,
      currentSongDurationMs: this.player.getDurationMs(),
      currentSongMetadata: this.player.getMetadata(),
      currentSongPositionMs: 0,
    });
    // }
  }

  songEnded() {
    // update state with the song's exact duration
    this.setState({
      currentSongDurationMs: this.state.currentSongPositionMs
    });
  }

  prevSubtune() {
    const subtune = this.state.currentSongSubtune - 1;
    if (subtune < 0) return;
    this.playSubtune(subtune);
  }

  nextSubtune() {
    const subtune = this.state.currentSongSubtune + 1;
    if (subtune >= this.state.currentSongNumSubtunes) return;
    this.playSubtune(subtune);
  }

  togglePause() {
    this.setState({paused: this.player.togglePause()});
  }

  handleSliderDrag(event) {
    const pos = event.target ? event.target.value : event;
    // Update current time position label
    this.setState({
      draggedSongPositionMs: pos * this.state.currentSongDurationMs,
    });
  }

  handleSliderChange(event) {
    const pos = event.target ? event.target.value : event;
    // Seek in song
    const seekMs = Math.floor(pos * this.state.currentSongDurationMs) / this.state.tempo;
    // this.player.setFadeout(this.getFadeMs(this.state.currentSongMetadata, this.state.tempo));
    this.player.seekMs(seekMs);
    this.startedFadeOut = false;
    this.setState({
      draggedSongPositionMs: -1,
      currentSongPositionMs: pos * this.state.currentSongDurationMs, // Smooth
    });
    setTimeout(() => {
      this.setState({
        currentSongPositionMs: this.player.getPositionMs(), // Accurate
      });
    }, 100);
  }

  handleVoiceToggle(index) {
    const voices = [...this.state.voices];
    voices[index] = !voices[index];
    this.player.setVoices(voices);
    this.setState({voices: voices});
  }

  handleTempoChange(event) {
    const tempo = (event.target ? event.target.value : event) || 1.0;
    // const newDurationMs = this.state.currentSongMetadata.play_length / tempo + FADE_OUT_DURATION_MS;
    this.player.setTempo(tempo);
    // this.player.setFadeout(this.getFadeMs(this.state.currentSongMetadata, tempo));
    this.setState({
      tempo: tempo
    });
  }

  getSongPos() {
    return this.state.currentSongPositionMs / this.state.currentSongDurationMs;
  }

  getTimeLabel() {
    const val = this.state.draggedSongPositionMs >= 0 ?
      this.state.draggedSongPositionMs :
      this.state.currentSongPositionMs;
    return this.getTime(val);
  }

  getTime(val) {
    const pad = n => n < 10 ? '0' + n : n;
    const min = Math.floor(val / 60000);
    const sec = Math.floor((val / 1000) % 60);
    return `${min}:${pad(sec)}`;
  }

  getFadeMs(metadata, tempo) {
    return Math.floor(metadata.play_length / tempo);
  }

  render() {
    const onFileClick = (filename) => {
      return (e) => {
        e.preventDefault();
        this.playSong(filename);
      }
    };
    return (
      <div className="App">
        <header className="App-header">
          <p className="App-title">Chip Player JS</p>
          <small>
            powered by <a href="https://bitbucket.org/mpyne/game-music-emu/wiki/Home">Game Music Emu</a>, <a href="https://github.com/cmatsuoka/libxmp">LibXMP</a>, and <a href="https://github.com/schellingb/TinySoundFont">TinySoundFont</a>
          </small>
        </header>
        {this.state.loading ?
          <p>Loading...</p>
          :
          <div className="App-intro">
            <button onClick={this.play}>
              Play
            </button>
            <button onClick={this.togglePause}>
              {this.state.paused ? 'Resume' : 'Pause'}
            </button>
            <Slider
              pos={this.getSongPos()}
              onDrag={this.handleSliderDrag}
              onChange={this.handleSliderChange}/>
            {this.state.currentSongMetadata &&
            <div className="Song-details">
              Time: {this.getTimeLabel()} / {this.getTime(this.state.currentSongDurationMs)}<br/>
              Speed: <input
              type="range" value={this.state.tempo}
              min="0.3" max="2.0" step="0.1"
              onInput={this.handleTempoChange}
              onChange={this.handleTempoChange}/>
              {this.state.tempo}<br/>
              {this.state.currentSongNumSubtunes > 1 &&
              <span>
                  Subtune: {this.state.currentSongSubtune + 1} of {this.state.currentSongNumSubtunes}&nbsp;
                <button onClick={this.prevSubtune}>Prev</button>
                &nbsp;
                <button onClick={this.nextSubtune}>Next</button><br/>
                </span>
              }
              Voices:
              {[...Array(this.state.currentSongNumVoices)].map((_, i) => {
                return (
                  <label key={i}>
                    <input type="checkbox" onChange={() => {
                      this.handleVoiceToggle(i)
                    }} checked={this.state.voices[i]}/>
                    {this.state.voiceNames[i]}
                  </label>
                )
              })}<br/>
              Title: {this.state.currentSongMetadata.title || '--'}<br/>
              System: {this.state.currentSongMetadata.system || '--'}<br/>
              Game: {this.state.currentSongMetadata.game || '--'}<br/>
              Song: {this.state.currentSongMetadata.song || '--'}<br/>
              Author: {this.state.currentSongMetadata.author || '--'}<br/>
              Copyright: {this.state.currentSongMetadata.copyright || '--'}<br/>
              Comment: {this.state.currentSongMetadata.comment || '--'}
            </div>
            }
            {songData.map(group => {
              return (
                <div key={group.title}>
                  <h4>{group.title}</h4>
                  {group.files.map(file => {
                    const href = group.url_prefix + file;
                    return (
                      <div key={file}>
                        <a onClick={onFileClick(href)} href={href}>{unescape(file)}</a>
                      </div>
                    )
                  })}
                </div>
              )
            })}
          </div>
        }
      </div>
  );
  }
  }

  export default App;
