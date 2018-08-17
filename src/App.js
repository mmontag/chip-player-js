import React, {Component} from 'react';
import './App.css';
import songData from './song-data';

const LibGME = require('./libgme');
const libgme = new LibGME({
  // Look for .wasm file in web root, not the same location as the app bundle (static/js).
  locateFile: (path, prefix) => {
    if (path.endsWith('.wasm') || path.endsWith('.wast')) return './' + path;
    return prefix + path;
  }
});

let emu = null;
let node = null;
const audioContext = new AudioContext();
let paused = false;

function log(str) {
  console.log(str);
}

function playMusicData(payload, subtune) {
  subtune = subtune || 0;
  log("Subtune: " + subtune);

  const ref = libgme.allocate(1, "i32", libgme.ALLOC_NORMAL);
  libgme.allocate(1, "i32", libgme.ALLOC_NORMAL);

  const samplerate = audioContext.sampleRate;

  if (libgme.ccall("gme_open_data", "number", ["array", "number", "number", "number"], [payload, payload.length, ref, samplerate]) !== 0) {
    console.error("gme_open_data failed.");
    return;
  }
  emu = libgme.getValue(ref, "i32");

  var subtune_count = libgme._gme_track_count(emu);

  libgme._gme_ignore_silence(emu, 1);

  var voice_count = libgme._gme_voice_count(emu);
  log("Channel count: ", voice_count);
  log("Track count: ", subtune_count);

  // $("#tempo").val(1.0);
  // $("#tempolabel").html(1.0);

  if (libgme._gme_start_track(emu, subtune) !== 0)
    log("Could not load track");
  // setChannels();

  var bufferSize = 4096;
  var inputs = 2;
  var outputs = 2;

  if (!node && audioContext.createJavaScriptNode) node = audioContext.createJavaScriptNode(bufferSize, inputs, outputs);
  if (!node && audioContext.createScriptProcessor) node = audioContext.createScriptProcessor(bufferSize, inputs, outputs);

  var buffer = libgme.allocate(bufferSize * 16, "i16", libgme.ALLOC_NORMAL);

  var INT16_MAX = Math.pow(2, 16) - 1;

  node.onaudioprocess = function (e) {
    const channels = [e.outputBuffer.getChannelData(0), e.outputBuffer.getChannelData(1)];
    let i, channel;

    if (paused) {
      for (i = 0; i < bufferSize; i++)
        for (channel = 0; channel < e.outputBuffer.numberOfChannels; channel++)
          channels[channel][i] = 0;
      return;
    }

    // TODO: don't tear down audio when a track ends.
    if (libgme._gme_track_ended(emu) === 1) {
      node.disconnect();
      log("End of track.");
      return;
    }

    libgme._gme_play(emu, bufferSize * 2, buffer);
    for (i = 0; i < bufferSize; i++)
      for (channel = 0; channel < e.outputBuffer.numberOfChannels; channel++)
        channels[channel][i] = libgme.getValue(buffer +
          // Interleaved channel format
          // frame number * bytes per frame +
          i * 4 +
          // channel number * bytes per sample
          channel * 2,
          // 16-bit integer
          "i16") / INT16_MAX;
  };
  // node.connect(filterNode);
  node.connect(audioContext.destination);

  window.savedReferences = [audioContext, node];
}

class App extends Component {
  constructor(props) {
    super(props);

    this.togglePause = this.togglePause.bind(this);

    this.state = {
      paused: false
    };
  }

  playSong(filename, subtune) {
    filename = 'songs/wt-01.spc';
    if (node) {
      node.disconnect();
      node = null;
    }
    var xhr = new XMLHttpRequest();
    xhr.open("GET", filename, true);
    xhr.responseType = "arraybuffer";
    xhr.onerror = function (e) {
      log(e);
    };
    xhr.onload = function (e) {
      if (this.status === 404) {
        log("not found");
        return;
      }
      var payload = new Uint8Array(this.response);
      playMusicData(payload, subtune);
      //updateSongInfo(filename, subtune);
    };
    xhr.send();
  }

  togglePause() {
    paused = !this.state.paused;
    this.setState({
      paused: !this.state.paused
    });
  }

  render() {
    return (
      <div className="App">
        <header className="App-header">
          <h1 className="App-title">JavaScript Game Music Player</h1>
          <small>powered by <a href="https://bitbucket.org/mpyne/game-music-emu/wiki/Home">Game Music Emu</a></small>
        </header>
        <p className="App-intro">
          <button onClick={this.playSong}>
            Play Music
          </button>
          <button onClick={this.togglePause}>
            {this.state.paused ? 'Resume' : 'Pause'}
          </button>
        </p>
      </div>
    );
  }
}

export default App;
