import React, {Component} from 'react';
import Slider from './Slider'
import './App.css';
import songData from './song-data';

const LibGME = require('./libgme');

const corsPrefix = 'https://cors-anywhere.herokuapp.com/';

let emu = null;
let audioNode = null;
let libgme = null;
let audioCtx = null;
let paused = false;
const BUFFER_SIZE = 4096;
const FADE_OUT_DURATION_MS = 8000; // Default fade out duration in game-music-emu
const MAX_VOICES = 8;
const INT16_MAX = Math.pow(2, 16) - 1;

function playerTogglePause() {
  paused = !paused;
  return paused;
}

function playerIsPaused() {
  return paused;
}

function playerPause() {
  paused = true;
}

function playerResume() {
  paused = false;
}

function getDurationMs(metadata) {
  console.log(metadata);
  if (metadata.loop_length || metadata.intro_length) {
    return metadata.play_length + FADE_OUT_DURATION_MS;
  } else {
    return metadata.play_length;
  }
}

function playMusicData(payload, subtune) {
  subtune = subtune || 0;
  if (!audioCtx) {
    audioCtx = new (window.AudioContext || window.webkitAudioContext)();
    audioNode = audioCtx.createScriptProcessor(BUFFER_SIZE, 2, 2);
  }
  audioNode.connect(audioCtx.destination);

  const buffer = libgme.allocate(BUFFER_SIZE * 16, "i16", libgme.ALLOC_NORMAL);
  const ref = libgme.allocate(1, "i32", libgme.ALLOC_NORMAL);

  if (libgme.ccall("gme_open_data", "number", ["array", "number", "number", "number"], [payload, payload.length, ref, audioCtx.sampleRate]) !== 0) {
    console.error("gme_open_data failed.");
    return;
  }
  emu = libgme.getValue(ref, "i32");
  // libgme._gme_ignore_silence(emu, 1); // causes crash in 2nd call to _gme_seek.
  if (libgme._gme_start_track(emu, subtune) !== 0)
    console.error("Could not load track");

  audioNode.onaudioprocess = function (e) {
    const channels = [e.outputBuffer.getChannelData(0), e.outputBuffer.getChannelData(1)];
    let i, channel;

    if (paused || libgme._gme_track_ended(emu) === 1) {
      channels.forEach(channel => channel.fill(0));
      return;
    }

    libgme._gme_play(emu, BUFFER_SIZE * 2, buffer);
    for (i = 0; i < BUFFER_SIZE; i++) {
      for (channel = 0; channel < e.outputBuffer.numberOfChannels; channel++) {
        channels[channel][i] = libgme.getValue(buffer +
          // Interleaved channel format
          i * 2 * 2 +             // frame offset   * bytes per sample * num channels +
          channel * 2,            // channel offset * bytes per sample
          "i16") / INT16_MAX;     // convert int16 to float
      }
    }
  };
  window.savedReferences = [audioCtx, audioNode];
  playerResume();
}

function playerSetVoices(voices) {
  let mask = 0;
  voices.forEach((isEnabled, i) => {
    if (!isEnabled) {
      mask += 1 << i;
    }
  });
  if (emu) libgme._gme_mute_voices(emu, mask);
}

function playerSetTempo(val) {
  if (emu) libgme._gme_set_tempo(emu, val);
}

function playerSetFadeout(startMs) {
  if (emu) libgme._gme_set_fade(emu, startMs);
}

function playerGetPosition() {
  if (emu) return libgme._gme_tell(emu);
}

function parseMetadata(subtune = 0) {
  const metadataPtr = libgme.allocate(1, "i32", libgme.ALLOC_NORMAL);
  if (libgme._gme_track_info(emu, metadataPtr, subtune) !== 0)
    console.error("could not load metadata");
  const ref = libgme.getValue(metadataPtr, "*");

  let offset = 0;

  const readInt32 = function () {
    var value = libgme.getValue(ref + offset, "i32");
    offset += 4;
    return value;
  };

  const readString = function () {
    var value = libgme.Pointer_stringify(libgme.getValue(ref + offset, "i8*"));
    offset += 4;
    return value;
  };

  const res = {};

  res.length = readInt32();
  res.intro_length = readInt32();
  res.loop_length = readInt32();
  res.play_length = readInt32();

  offset += 4 * 12; // skip unused bytes

  res.system = readString();
  res.game = readString();
  res.song = readString();
  res.author = readString();
  res.copyright = readString();
  res.comment = readString();

  return res;
}


class App extends Component {
  constructor(props) {
    super(props);

    this.togglePause = this.togglePause.bind(this);
    this.playSong = this.playSong.bind(this);
    this.handleSliderDrag = this.handleSliderDrag.bind(this);
    this.handleSliderChange = this.handleSliderChange.bind(this);
    this.handleTempoChange = this.handleTempoChange.bind(this);
    this.getSongPos = this.getSongPos.bind(this);
    this.prevSubtune = this.prevSubtune.bind(this);
    this.nextSubtune = this.nextSubtune.bind(this);
    this.playSubtune = this.playSubtune.bind(this);
    this.getFadeMs = this.getFadeMs.bind(this);

    libgme = new LibGME({
      // Look for .wasm file in web root, not the same location as the app bundle (static/js).
      locateFile: (path, prefix) => {
        if (path.endsWith('.wasm') || path.endsWith('.wast')) return './' + path;
        return prefix + path;
      },
      onRuntimeInitialized: () => {
        this.setState({loading: false});
      },
    });

    this.lastTime = (new Date()).getTime();
    this.state = {
      loading: true,
      paused: false,
      currentSongMetadata: {},
      currentSongNumVoices: 0,
      currentSongNumSubtunes: 0,
      currentSongSubtune: 0,
      currentSongDurationMs: 1000,
      currentSongPositionMs: 0,
      draggedSongPositionMs: -1,
      tempo: 1,
      voices: Array(MAX_VOICES).fill(true),
    };

    this.displayLoop();
  }

  playSong(filename, subtune) {
    // filename = corsPrefix + songData[Math.floor(Math.random() * songData.length)];
    fetch(filename).then(response => response.arrayBuffer()).then(buffer => {
      playMusicData(new Uint8Array(buffer));
      playerSetTempo(this.state.tempo);
      playerSetVoices(this.state.voices);
      const numSubtunes = libgme._gme_track_count(emu);
      const metadata = parseMetadata(subtune);
      playerSetFadeout(this.getFadeMs(metadata, this.state.tempo));

      this.setState({
        paused: playerIsPaused(),
        currentSongMetadata: metadata,
        currentSongDurationMs: getDurationMs(metadata),
        currentSongNumVoices: libgme._gme_voice_count(emu),
        currentSongNumSubtunes: numSubtunes,
        currentSongPositionMs: 0,
        currentSongSubtune: 0,
      });
    });

  }

  displayLoop() {
    const currentTime = (new Date()).getTime();
    const deltaTime = currentTime - this.lastTime;
    this.lastTime = currentTime;
    if (emu) {
      if (!playerIsPaused() && libgme._gme_track_ended(emu) !== 1) {
        const currMs = this.state.currentSongPositionMs;
        this.setState({
          currentSongPositionMs: currMs + deltaTime * this.state.tempo,
        });
      }
    }
    requestAnimationFrame(this.displayLoop);
  }

  prevSubtune() {
    const subtune = this.state.currentSongSubtune - 1;
    if (subtune < 0) return;
    if (libgme._gme_start_track(emu, subtune) !== 0)
      console.error("Could not load track");
    else {
      const metadata = parseMetadata(subtune);
      this.setState({
        currentSongSubtune: subtune,
        currentSongMetadata: metadata,
        currentSongPositionMs: 0,
      });
    }
  }

  nextSubtune() {
    const subtune = this.state.currentSongSubtune + 1;
    if (subtune >= this.state.currentSongNumSubtunes) return;
    if (libgme._gme_start_track(emu, subtune) !== 0)
      console.error("Could not load track");
    else {
      const metadata = parseMetadata(subtune);
      playerSetFadeout(this.getFadeMs(metadata, this.state.tempo));
      this.setState({
        currentSongSubtune: subtune,
        currentSongDurationMs: getDurationMs(metadata),
        currentSongMetadata: metadata,
        currentSongPositionMs: 0,
      });
    }
  }

  togglePause() {
    const paused = playerTogglePause();
    this.setState({paused: paused});
  }

  handleSliderDrag(event) {
    const pos = event.target ? event.target.value : event;
    // Update current time position label
    this.setState({
      draggedSongPositionMs: Math.floor(pos * this.state.currentSongDurationMs),
    });
  }

  handleSliderChange(event) {
    const pos = event.target ? event.target.value : event;
    // Seek in song
    if (emu) {
      const seekMs = Math.floor(pos * this.state.currentSongDurationMs) / this.state.tempo;
      playerSetFadeout(this.getFadeMs(this.state.currentSongMetadata, this.state.tempo));
      libgme._gme_seek(emu, seekMs);
      this.setState({
        draggedSongPositionMs: -1,
        currentSongPositionMs: pos * this.state.currentSongDurationMs,
      });
    }
  }

  handleVoiceToggle(index) {
    const voices = [...this.state.voices];
    voices[index] = !voices[index];
    playerSetVoices(voices);
    this.setState({voices: voices});
  }

  handleTempoChange(event) {
    const tempo = (event.target ? event.target.value : event) || 1.0;
    // const newDurationMs = this.state.currentSongMetadata.play_length / tempo + FADE_OUT_DURATION_MS;
    playerSetTempo(tempo);
    playerSetFadeout(this.getFadeMs(this.state.currentSongMetadata, tempo));
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
    return (
      <div className="App">
        <header className="App-header">
          <h1 className="App-title">JavaScript Game Music Player</h1>
          <small>powered by <a href="https://bitbucket.org/mpyne/game-music-emu/wiki/Home">Game Music Emu</a></small>
        </header>
        {this.state.loading ?
          <p>Loading...</p>
          :
          <div className="App-intro">
            <button onClick={this.playSong}>
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
              Time: {this.getTimeLabel()}<br/>
              Speed: <input
              type="range" value={this.state.tempo}
              min="0.1" max="2.0" step="0.1"
              onInput={this.handleTempoChange}
              onChange={this.handleTempoChange}/>
              {this.state.tempo.toFixed(1)}<br/>
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
                return <input type="checkbox" onChange={() => {
                  this.handleVoiceToggle(i)
                }} checked={this.state.voices[i]}/>
              })}<br/>
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
                <div>
                  <h4>{group.title}</h4>
                  {group.files.map(file => {
                    const href = group.url_prefix + file;
                    return (
                      <div>
                        <a onClick={() => this.playSong(href)} href="javascript:void(0)">{unescape(file)}</a>
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
