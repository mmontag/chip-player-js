import React, {Component} from 'react';
import Slider from './Slider'
import './App.css';
import songData from './song-data';

const LibGME = require('./libgme');
const unrar = require('node-unrar-js');

let emu = null;
let audioNode = null;
let libgme = null;
let audioCtx = null;
let paused = false;
const BUFFER_SIZE = 1024;
const MAX_VOICES = 8;
const INT16_MAX = Math.pow(2, 16) - 1;

function playerTogglePause() {
  paused = !paused;
  return paused;
}

function playerIsPaused() {
  return paused;
}

function playerResume() {
  paused = false;
}

function getDurationMs(metadata) {
  console.log(metadata);
  return metadata.play_length;
}

function playSubtune(subtune) {
  if (emu) return libgme._gme_start_track(emu, subtune)
}

function playMusicData(payload, callback = null) {
  let endSongCallback = callback;
  const subtune = 0;
  const trackEnded = libgme._gme_track_ended(emu) === 1;
  const buffer = libgme.allocate(BUFFER_SIZE * 16, "i16", libgme.ALLOC_NORMAL);
  const ref = libgme.allocate(1, "i32", libgme.ALLOC_NORMAL);

  if (!audioCtx) {
    audioCtx = new (window.AudioContext || window.webkitAudioContext)();
    audioNode = audioCtx.createScriptProcessor(BUFFER_SIZE, 2, 2);
    audioNode.connect(audioCtx.destination);
    audioNode.onaudioprocess = function (e) {
      let i, channel;
      const channels = [];
      for (channel = 0; channel < e.outputBuffer.numberOfChannels; channel++) {
        channels[channel] = e.outputBuffer.getChannelData(channel);
      }

      if (paused || trackEnded) {
        if (trackEnded && typeof endSongCallback === 'function') {
          endSongCallback();
          endSongCallback = null;
        }
        for (channel = 0; channel < channels.length; channel++) {
          channels[channel].fill(0);
        }
        return;
      }

      libgme._gme_play(emu, BUFFER_SIZE * 2, buffer);

      for (channel = 0; channel < channels.length; channel++) {
        for (i = 0; i < BUFFER_SIZE; i++) {
          channels[channel][i] = libgme.getValue(buffer +
            // Interleaved channel format
            i * 2 * 2 +             // frame offset   * bytes per sample * num channels +
            channel * 2,            // channel offset * bytes per sample
            "i16") / INT16_MAX;     // convert int16 to float
        }
      }
    };
    window.savedReferences = [audioCtx, audioNode];
  }

  if (libgme.ccall("gme_open_data", "number", ["array", "number", "number", "number"], [payload, payload.length, ref, audioCtx.sampleRate]) !== 0) {
    console.error("gme_open_data failed.");
    return;
  }
  emu = libgme.getValue(ref, "i32");
  // libgme._gme_ignore_silence(emu, 1); // causes crash in 2nd call to _gme_seek.
  if (libgme._gme_start_track(emu, subtune) !== 0) {
    console.error("gme_start_track failed.");
    return;
  }

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
    this.getTimeLabel = this.getTimeLabel.bind(this);
    this.displayLoop = this.displayLoop.bind(this);
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

  playSong(filename, subtune) {
    if (typeof filename !== 'string') {
      if (emu) {
        libgme._gme_seek(emu, 0);
        playerResume();
        this.setState({
          paused: false,
          currentSongPositionMs: 0,
        });
      }
      return;
    }
    // filename = corsPrefix + songData[Math.floor(Math.random() * songData.length)];
    fetch(filename).then(response => response.arrayBuffer()).then(buffer => {
      let uint8Array;
      let extractedTunes = [];
      if (filename.endsWith('.rsn')) {
        // Unrar RSN files
        const extractor = unrar.createExtractorFromData(buffer);
        const extracted = extractor.extractAll();
        extractedTunes = extracted[1].files.filter(file => {
          // Skip tiny files
          return file.extract[1].byteLength >= 8192;
        }).map(file => file.extract[1]);
        uint8Array = extractedTunes[0];
      } else {
        uint8Array = new Uint8Array(buffer);
      }

      playMusicData(uint8Array);
      playerSetTempo(this.state.tempo);
      playerSetVoices(this.state.voices);
      const numSubtunes = libgme._gme_track_count(emu);
      const metadata = parseMetadata(subtune);
      // playerSetFadeout(this.getFadeMs(metadata, this.state.tempo));

      this.startedFadeOut = false;
      this.setState({
        paused: playerIsPaused(),
        currentSongMetadata: metadata,
        currentSongDurationMs: getDurationMs(metadata),
        currentSongNumVoices: libgme._gme_voice_count(emu),
        currentSongNumSubtunes: extractedTunes.length || numSubtunes,
        currentSongPositionMs: 0,
        currentSongSubtune: 0,
        extractedTunes: extractedTunes,
      });
    });

  }

  displayLoop() {
    const currentTime = (new Date()).getTime();
    const deltaTime = currentTime - this.lastTime;
    this.lastTime = currentTime;
    if (emu) {
      if (!playerIsPaused() && libgme._gme_track_ended(emu) !== 1) {
        if (this.state.currentSongPositionMs >= this.state.currentSongDurationMs) {
          if (!this.startedFadeOut) {
            // Fade out right now
            console.log('Starting fadeout at ', playerGetPosition());
            this.startedFadeOut = true;
            playerSetFadeout(playerGetPosition());
          }
        } else {
          const currMs = this.state.currentSongPositionMs;
          this.setState({
            currentSongPositionMs: currMs + deltaTime * this.state.tempo,
          });
        }
      }
    }
    requestAnimationFrame(this.displayLoop);
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

  playSubtune(subtune) {
    if (this.state.extractedTunes.length) {
      // TODO: factor with playSong()
      playMusicData(this.state.extractedTunes[subtune]);
      playerSetTempo(this.state.tempo);
      playerSetVoices(this.state.voices);
      const metadata = parseMetadata();
      // playerSetFadeout(this.getFadeMs(metadata, this.state.tempo));
      this.setState({
        currentSongSubtune: subtune,
        currentSongDurationMs: getDurationMs(metadata),
        currentSongMetadata: metadata,
        currentSongPositionMs: 0,
      });
    } else {
      if (playSubtune(subtune) !== 0) {
        console.error("Could not load track");
        return;
      }
      const metadata = parseMetadata(subtune);
      // playerSetFadeout(this.getFadeMs(metadata, this.state.tempo));
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
      // playerSetFadeout(this.getFadeMs(this.state.currentSongMetadata, this.state.tempo));
      libgme._gme_seek(emu, seekMs);
      this.startedFadeOut = false;
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
    // playerSetFadeout(this.getFadeMs(this.state.currentSongMetadata, tempo));
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
              Time: {this.getTimeLabel()} / {this.getTime(this.state.currentSongDurationMs)}<br/>
              Speed: <input
              type="range" value={this.state.tempo}
              min="0.1" max="2.0" step="0.1"
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
