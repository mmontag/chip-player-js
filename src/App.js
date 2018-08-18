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
const MAX_VOICES = 8;
const INT16_MAX = Math.pow(2, 16) - 1;

function playerTogglePause() {
  paused = !paused;
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

    if (paused) {
      for (i = 0; i < BUFFER_SIZE; i++)
        for (channel = 0; channel < e.outputBuffer.numberOfChannels; channel++)
          channels[channel][i] = 0;
      return;
    }

    if (libgme._gme_track_ended(emu) === 1) {
      audioNode.disconnect();
      console.log("End of track.");
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

function parseMetadata(filename, subtune) {
  subtune = subtune || 0;
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
    this.getSongInfo = this.getSongInfo.bind(this);
    this.handleSliderDrag = this.handleSliderDrag.bind(this);
    this.handleSliderChange = this.handleSliderChange.bind(this);
    this.getSongPos = this.getSongPos.bind(this);

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

    this.sliderTimer = null;
    this.state = {
      loading: true,
      paused: false,
      currentSongMetadata: {},
      currentSongDurationMs: 1000,
      currentSongPositionMs: 0,
      draggedSongPositionMs: -1,
      currentSongNumVoices: 0,
      voices: Array(MAX_VOICES).fill(true),
    };
  }

  getSongInfo(filename, subtune) {
    const metadata = parseMetadata(filename, subtune);

    this.setState({
      currentSongMetadata: metadata,
      currentSongDurationMs: metadata.length,
    });
  }

  playSong(filename, subtune) {
    filename = corsPrefix + songData[Math.floor(Math.random() * songData.length)];
    fetch(filename).then(response => response.arrayBuffer()).then(buffer => {
      playMusicData(new Uint8Array(buffer));
      this.setState({
        paused: playerIsPaused(),
        currentSongNumVoices: libgme._gme_voice_count(emu),
      });
      this.getSongInfo(filename, subtune);
    });
    this.sliderTimer = setInterval(() => {
      this.setState({
        currentSongPositionMs: libgme._gme_tell(emu),
      });
    }, 1000);
  }

  togglePause() {
    playerTogglePause();
    this.setState({paused: playerIsPaused()});
  }

  handleSliderDrag(pos) {
    // Update current time position label
    this.setState({
      draggedSongPositionMs: Math.floor(pos * this.state.currentSongDurationMs),
    });
  }

  handleSliderChange(pos) {
    // Seek in song
    if (emu) {
      libgme._gme_seek(emu, Math.floor(pos * this.state.currentSongDurationMs));
      this.setState({
        draggedSongPositionMs: -1,
        currentSongPositionMs: libgme._gme_tell(emu),
      });
    }
  }

  handleVoiceToggle(index) {
    this.state.voices[index] = !this.state.voices[index];
    this.setState({
      voices: this.state.voices,
    });

    let mask = 0;
    this.state.voices.forEach((isEnabled, i) => {
      if (!isEnabled) {
        mask += 1 << i;
      }
    });
    if (emu) libgme._gme_mute_voices(emu, mask);
  }

  handleTempoChange(val) {

  }

  getSongPos() {
    return this.state.currentSongPositionMs / this.state.currentSongDurationMs;
  }

  getTimeLabel() {
    const pad = n => n < 10 ? '0' + n : n;
    const val = this.state.draggedSongPositionMs >= 0 ?
      this.state.draggedSongPositionMs :
      this.state.currentSongPositionMs;
    const min = Math.floor(val / 60000);
    const sec = Math.floor((val % 60000) / 1000);
    return `${min}:${pad(sec)}`;
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
            {this.getTimeLabel()}
            {this.state.currentSongMetadata &&
            <div className="Song-details">
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
          </div>
        }
      </div>
    );
  }
}

export default App;
