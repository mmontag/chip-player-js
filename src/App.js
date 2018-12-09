import './App.css';

import React, {PureComponent} from 'react';
import isMobile from 'ismobilejs';
import queryString from 'querystring';

import ChipCore from './chip-core';
import GMEPlayer from './players/GMEPlayer';
import MIDIPlayer from './players/MIDIPlayer';
import XMPPlayer from './players/XMPPlayer';
import promisify from './promisifyXhr';
import {trieToList} from './ResigTrie';

import PlayerParams from './PlayerParams';
import Search from './Search';
import TimeSlider from "./TimeSlider";
import Visualizer from './Visualizer';

const MAX_VOICES = 32;
const CATALOG_PREFIX = 'https://gifx.co/music/';

class App extends PureComponent {
  constructor(props) {
    super(props);

    this.togglePause = this.togglePause.bind(this);
    this.toggleSettings = this.toggleSettings.bind(this);
    this.play = this.play.bind(this);
    this.playSong = this.playSong.bind(this);
    this.handleTimeSliderChange = this.handleTimeSliderChange.bind(this);
    this.handleTempoChange = this.handleTempoChange.bind(this);
    this.handleVoiceToggle = this.handleVoiceToggle.bind(this);
    this.handlePlayerStateUpdate = this.handlePlayerStateUpdate.bind(this);
    this.prevSubtune = this.prevSubtune.bind(this);
    this.nextSubtune = this.nextSubtune.bind(this);
    this.playSubtune = this.playSubtune.bind(this);
    this.displayLoop = this.displayLoop.bind(this);
    this.getFadeMs = this.getFadeMs.bind(this);
    this.loadCatalog = this.loadCatalog.bind(this);
    this.handlePlayRandom = this.handlePlayRandom.bind(this);
    this.handleFileClick = this.handleFileClick.bind(this);

    // Initialize audio graph
    const audioCtx = this.audioCtx = new (window.AudioContext || window.webkitAudioContext)();
    const gainNode = audioCtx.createGain();
    gainNode.gain.value = 1;

    // const compressorNode = audioCtx.createDynamicsCompressor();
    // compressorNode.threshold.value = -60;
    // compressorNode.knee.value = 40;
    // compressorNode.ratio.value = 16;
    // compressorNode.attack.value = 0;
    // compressorNode.release.value = 4;
    // gainNode.connect(compressorNode);
    // compressorNode.connect(audioCtx.destination);
    // this.playerNode = compressorNode;

    gainNode.connect(audioCtx.destination);
    var playerNode = this.playerNode = gainNode;

    this._safariAudioUnlock(audioCtx);
    console.log('Sample rate: %d hz', audioCtx.sampleRate);

    // Load the chip-core Emscripten runtime
    const chipCore = this.chipCore = new ChipCore({
      // Look for .wasm file in web root, not the same location as the app bundle (static/js).
      locateFile: (path, prefix) => {
        if (path.endsWith('.wasm') || path.endsWith('.wast')) return './' + path;
        return prefix + path;
      },
      onRuntimeInitialized: () => {
        this.players = [
          new GMEPlayer(audioCtx, playerNode, chipCore, this.handlePlayerStateUpdate),
          new XMPPlayer(audioCtx, playerNode, chipCore, this.handlePlayerStateUpdate),
          new MIDIPlayer(audioCtx, playerNode, chipCore, this.handlePlayerStateUpdate),
        ];
        this.setState({loading: false});

        const urlParams = queryString.parse(window.location.search.substr(1));
        if (urlParams.q) {
          this.setState({initialQuery: urlParams.q});
        }
        if (urlParams.play) {
          // Allow a little time for initial page render before starting the song.
          // This is not absolutely necessary but helps prevent stuttering.
          setTimeout(() => {
            this.playSong(CATALOG_PREFIX + urlParams.play);
            if (urlParams.t) {
              //
              setTimeout(() => {
                if (this.player) {
                  this.player.seekMs(parseInt(urlParams.t, 10));
                }
              }, 100);
            }
          }, 500);
        }
      },
    });

    this.player = null;
    this.songRequest = null;
    this.state = {
      catalog: null,
      loading: true,
      paused: true,
      ejected: true,
      playerError: null,
      currentSongMetadata: {},
      currentSongNumVoices: 0,
      currentSongNumSubtunes: 0,
      currentSongSubtune: 0,
      currentSongDurationMs: 1,
      currentSongPositionMs: 0,
      tempo: 1,
      voices: Array(MAX_VOICES).fill(true),
      voiceNames: Array(MAX_VOICES).fill(''),
      initialQuery: null,
      imageUrl: null,
      showSettings: false,
    };

    // Load the song catalog
    this.loadCatalog();

    // Start display loop
    setInterval(this.displayLoop, 46); //  46 ms = 2048/44100 sec or 21.5 fps
                                       // 400 ms = 2.5 fps
  }

  _safariAudioUnlock(context) {
    // https://hackernoon.com/unlocking-web-audio-the-smarter-way-8858218c0e09
    if (context.state === 'suspended') {
      const events = ['touchstart', 'touchend', 'mousedown', 'mouseup'];
      const unlock = () => context.resume()
        .then(() => events.forEach(event => document.body.removeEventListener(event, unlock)));
      events.forEach(event => document.body.addEventListener(event, unlock, false));
    }
  }

  displayLoop() {
    if (this.player) {
      this.setState({
        currentSongPositionMs: Math.min(this.player.getPositionMs(), this.state.currentSongDurationMs),
      });
    }
    // requestAnimationFrame(this.displayLoop);
  }

  loadCatalog() {
    fetch('./catalog.json')
      .then(response => response.json())
      .then(trie => trieToList(trie))
      .then(list => {
        console.log('Loaded catalog.json (%d items).', list.length);
        this.setState({catalog: list});
      });
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

  playSong(url) {
    this.setState({playerError: null});
    if (this.player !== null) {
      this.player.stop();
    }
    this.player = null;
    const filepath = url.replace(CATALOG_PREFIX, '');
    const ext = url.split('.').pop().toLowerCase();
    const parts = url.split('/');
    const filename = parts.pop();
    url = [...parts, encodeURIComponent(filename)].join('/'); // escape #, which appears in some filenames

    const urlParams = {
      ...queryString.parse(window.location.search.substr(1)),
      play: filepath,
    };
    delete urlParams.t;
    const stateUrl = '?' + queryString.stringify(urlParams)
      .replace(/%20/g, '+') // I don't care about escaping these characters
      .replace(/%2C/g, ',')
      .replace(/%2F/g, '/');
    window.history.replaceState(null, '', stateUrl);

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

    const imageUrl = [...parts, 'image.jpg'].join('/');
    if (this.imageRequest) this.imageRequest.abort();
    this.imageRequest = promisify(new XMLHttpRequest());
    this.imageRequest.open('HEAD', imageUrl);
    console.log('requesting image', imageUrl);
    this.imageRequest.send()
      .then(xhr => {
        console.log(xhr.status);
        if (xhr.status >= 200 && xhr.status < 400) {
          console.log('set state', {imageUrl: imageUrl});
          this.setState({imageUrl: imageUrl});
        }
      })
      .catch(e => {
        this.setState({imageUrl: null});
      });

    // Cancel any outstanding request so that playback doesn't happen out of order
    if (this.songRequest) this.songRequest.abort();
    this.songRequest = promisify(new XMLHttpRequest());
    this.songRequest.responseType = 'arraybuffer';
    this.songRequest.open('GET', url);
    this.songRequest.send()
      // .then(response => response.arrayBuffer())
      .then(xhr => xhr.response)
      .then(buffer => {
        let uint8Array;
        uint8Array = new Uint8Array(buffer);

        try {
          this.player.loadData(uint8Array, filepath);
        } catch (e) {
          this.setState({
            playerError: e.message,
          });
          return;
        }

        this.player.setTempo(this.state.tempo);
        this.player.setVoices(this.state.voices);
        this.player.resume();
        const numVoices = this.player.getNumVoices();

        this.setState({
          paused: false,
          currentSongMetadata: this.player.getMetadata(),
          currentSongDurationMs: this.player.getDurationMs(),
          currentSongNumVoices: numVoices,
          currentSongNumSubtunes: this.player.getNumSubtunes(),
          currentSongPositionMs: 0,
          currentSongSubtune: 0,
          voiceNames: [...Array(numVoices)].map((_, i) => this.player.getVoiceName(i)),
        });
      });
  }

  playSubtune(subtune) {
    this.player.playSubtune(subtune);
  }

  handlePlayerStateUpdate(isStopped) {
    if (isStopped) {
      this.setState({
        ejected: true,
        currentSongSubtune: 0,
        currentSongMetadata: {},
        currentSongNumVoices: 0,
        currentSongPositionMs: 0,
        currentSongDurationMs: 1,
        currentSongNumSubtunes: 0,
        imageUrl: null,
      });
    } else {
      this.setState({
        ejected: false,
        paused: this.player.isPaused(),
        currentSongSubtune: this.player.getSubtune(),
        currentSongMetadata: this.player.getMetadata(),
        currentSongNumVoices: this.player.getNumVoices(),
        currentSongPositionMs: this.player.getPositionMs(),
        currentSongDurationMs: this.player.getDurationMs(),
        currentSongNumSubtunes: this.player.getNumSubtunes(),
        // voiceNames: [...Array(this.player.getNumVoices())].map((_, i) => this.player.getVoiceName(i)),
      });
    }
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

  togglePause() {
    if (!this.player) return;

    this.setState({paused: this.player.togglePause()});
  }

  toggleSettings() {
    this.setState({showSettings: !this.state.showSettings});
  }

  handleTimeSliderChange(event) {
    if (!this.player) return;

    const pos = event.target ? event.target.value : event;

    const seekMs = Math.floor(pos * this.state.currentSongDurationMs);
    const urlParams = {
      ...queryString.parse(window.location.search.substr(1)),
      t: seekMs,
    };
    const stateUrl = '?' + queryString.stringify(urlParams).replace(/%20/g, '+').replace(/%2F/g, '/');
    window.history.replaceState(null, '', stateUrl);

    // Seek in song
    this.player.seekMs(seekMs);
    this.setState({
      currentSongPositionMs: pos * this.state.currentSongDurationMs, // Smooth
    });
    setTimeout(() => {
      if (this.player.isPlaying()) {
        this.setState({
          currentSongPositionMs: this.player.getPositionMs(), // Accurate
        });
      }
    }, 100);
  }

  handleVoiceToggle(index) {
    if (!this.player) return;

    const voices = [...this.state.voices];
    voices[index] = !voices[index];
    this.player.setVoices(voices);
    this.setState({voices: voices});
  }

  handleTempoChange(event) {
    if (!this.player) return;

    const tempo = parseFloat((event.target ? event.target.value : event)) || 1.0;
    this.player.setTempo(tempo);
    this.setState({
      tempo: tempo
    });
  }

  handlePlayRandom() {
    const catalog = this.state.catalog;
    if (catalog) {
      const idx = (Math.random() * catalog.length) | 0;
      this.playSong(CATALOG_PREFIX + catalog[idx]);
    }
  }

  handleFileClick(filename) {
    return (e) => {
      e.preventDefault();
      this.playSong(filename);
    }
  }

  getFadeMs(metadata, tempo) {
    return Math.floor(metadata.play_length / tempo);
  }

  render() {
    const metadata = this.state.currentSongMetadata;
    return (
      <div className="App">
        <header className="App-header">
          <h2 className="App-title">Chip Player JS</h2>
          {!isMobile.phone &&
          <p className="App-subtitle">
            <span className="App-byline">Feedback:&nbsp;
              <a href="https://twitter.com/matthewmontag">@matthewmontag</a>
            </span>
            Powered by&nbsp;
            <a href="https://bitbucket.org/mpyne/game-music-emu/wiki/Home">Game Music Emu</a>,&nbsp;
            <a href="https://github.com/cmatsuoka/libxmp">LibXMP</a>, and&nbsp;
            <a href="https://github.com/schellingb/TinySoundFont">TinySoundFont</a>.
          </p>}
        </header>
        {this.state.loading ?
          <p>Loading...</p>
          :
          <div className="App-content-area">
            <Search
              initialQuery={this.state.initialQuery}
              catalog={this.state.catalog}
              onResultClick={this.handleFileClick}/>
            {!isMobile.phone &&
            <Visualizer audioCtx={this.audioCtx}
                        sourceNode={this.playerNode}
                        chipCore={this.chipCore}
                        paused={this.state.ejected || this.state.paused}/>}
          </div>
        }
        <div className="App-footer">
          <div className="App-footer-main">
            <button onClick={this.togglePause}
                    disabled={this.state.ejected}>
              {this.state.paused ? 'Resume' : 'Pause'}
            </button>
            &nbsp;
            <button onClick={this.handlePlayRandom}>
              I'm Feeling Lucky
            </button>
            {!this.state.showSettings &&
            <button className="App-player-settings-button" onClick={this.toggleSettings}>
              Player Settings >
            </button>}
            {this.state.playerError &&
            <div className="App-error">ERROR: {this.state.playerError}</div>
            }
            <TimeSlider
              currentSongPositionMs={this.state.currentSongPositionMs}
              currentSongDurationMs={this.state.currentSongDurationMs}
              onChange={this.handleTimeSliderChange}/>
            {this.state.currentSongNumSubtunes > 1 &&
            <span>
                Subtune: {this.state.currentSongSubtune + 1} of {this.state.currentSongNumSubtunes}&nbsp;
              <button
                disabled={this.state.ejected}
                onClick={this.prevSubtune}>Prev</button>
              &nbsp;
              <button
                disabled={this.state.ejected}
                onClick={this.nextSubtune}>Next</button><br/>
              </span>}
            {metadata &&
            <div className="SongDetails">
              <div className="SongDetails-title">{metadata.artist} - {metadata.title}</div>
              <div className="SongDetails-subtitle">{metadata.game} - {metadata.system} ({metadata.copyright})</div>
            </div>}
          </div>
          {this.state.showSettings &&
          <div className="App-footer-settings">
            <PlayerParams
              ejected={this.state.ejected}
              tempo={this.state.tempo}
              numVoices={this.state.currentSongNumVoices}
              voices={this.state.voices}
              voiceNames={this.state.voiceNames}
              handleTempoChange={this.handleTempoChange}
              handleVoiceToggle={this.handleVoiceToggle}
              toggleSettings={this.toggleSettings}
              getParameter={this.player.getParameter}
              setParameter={this.player.setParameter}
              params={this.player.getParameters()}/>
          </div>}
          {this.state.imageUrl &&
          <div className="App-footer-art" style={{backgroundImage: `url("${this.state.imageUrl}")`}}/>}
        </div>
      </div>
    );
  }
}

export default App;
