import './App.css';

import React, {PureComponent} from 'react';
import Slider from './Slider';
import Search from './Search';
import Visualizer from './Visualizer';
import songData from './song-data';
import ChipCore from './chip-core';
import GMEPlayer from './players/GMEPlayer';
import XMPPlayer from './players/XMPPlayer';
import MIDIPlayer from './players/MIDIPlayer';
import promisify from './promisifyXhr';
import {trieToList} from './ResigTrie';
import queryString from 'querystring';

const MAX_VOICES = 32;
const CATALOG_PREFIX = 'https://gifx.co/music/';

class App extends PureComponent {
  constructor(props) {
    super(props);

    this.togglePause = this.togglePause.bind(this);
    this.play = this.play.bind(this);
    this.playSong = this.playSong.bind(this);
    this.handlePositionDrag = this.handlePositionDrag.bind(this);
    this.handlePositionDrop = this.handlePositionDrop.bind(this);
    this.handleTempoChange = this.handleTempoChange.bind(this);
    this.handlePlayerStateUpdate = this.handlePlayerStateUpdate.bind(this);
    this.getSongPos = this.getSongPos.bind(this);
    this.prevSubtune = this.prevSubtune.bind(this);
    this.nextSubtune = this.nextSubtune.bind(this);
    this.playSubtune = this.playSubtune.bind(this);
    this.getTimeLabel = this.getTimeLabel.bind(this);
    this.displayLoop = this.displayLoop.bind(this);
    this.getFadeMs = this.getFadeMs.bind(this);
    this.loadCatalog = this.loadCatalog.bind(this);
    this.handlePlayRandom = this.handlePlayRandom.bind(this);

    // Initialize audio graph
    const audioCtx = this.audioCtx = new (window.AudioContext || window.webkitAudioContext)();
    const gainNode = audioCtx.createGain();
    gainNode.gain.value = 1;

    gainNode.connect(audioCtx.destination);
    this.playerNode = gainNode;

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
          new GMEPlayer (audioCtx, gainNode, chipCore, this.handlePlayerStateUpdate),
          new XMPPlayer (audioCtx, gainNode, chipCore, this.handlePlayerStateUpdate),
          new MIDIPlayer(audioCtx, gainNode, chipCore, this.handlePlayerStateUpdate),
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
      playerError: null,
      currentSongMetadata: {},
      currentSongNumVoices: 0,
      currentSongNumSubtunes: 0,
      currentSongSubtune: 0,
      currentSongDurationMs: 1,
      currentSongPositionMs: 0,
      draggedSongPositionMs: -1,
      tempo: 1,
      voices: Array(MAX_VOICES).fill(true),
      voiceNames: Array(MAX_VOICES).fill(''),
      initialQuery: null,
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
    const stateUrl = '?' + queryString.stringify(urlParams).replace(/%20/g, '+').replace(/%2F/g, '/');
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
        currentSongSubtune: 0,
        currentSongMetadata: {},
        currentSongNumVoices: 0,
        currentSongPositionMs: 0,
        currentSongDurationMs: 1,
        draggedSongPositionMs: -1,
        currentSongNumSubtunes: 0,
      });
    } else {
      this.setState({
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

  handlePositionDrag(event) {
    const pos = event.target ? event.target.value : event;
    // Update current time position label
    this.setState({
      draggedSongPositionMs: pos * this.state.currentSongDurationMs,
    });
  }

  handlePositionDrop(event) {
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
      draggedSongPositionMs: -1,
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
    const sec = ((val / 1000) % 60).toFixed(1);
    return `${min}:${pad(sec)}`;
  }

  getFadeMs(metadata, tempo) {
    return Math.floor(metadata.play_length / tempo);
  }

  render() {
    const handleFileClick = (filename) => {
      return (e) => {
        e.preventDefault();
        this.playSong(filename);
      }
    };
    const playerParams = this.player ? this.player.getParameters() : [];
    const metadata = this.state.currentSongMetadata;
    return (
      <div className="App">
        <header className="App-header">
          <h2 className="App-title">Chip Player JS</h2>
          <p className="App-subtitle">
            powered by <a href="https://bitbucket.org/mpyne/game-music-emu/wiki/Home">Game Music Emu</a>, <a
            href="https://github.com/cmatsuoka/libxmp">LibXMP</a>, and <a
            href="https://github.com/schellingb/TinySoundFont">TinySoundFont</a>.
          </p>
        </header>
        <Search initialQuery={this.state.initialQuery} catalog={this.state.catalog} onResultClick={handleFileClick}/>
        {this.state.loading ?
          <p>Loading...</p>
          :
          <div>
            <Visualizer audioCtx={this.audioCtx}
                        sourceNode={this.playerNode}
                        chipCore={this.chipCore}
                        paused={this.state.paused}/>
            <button onClick={this.togglePause}
                    disabled={this.player ? null : true}>
              {this.state.paused ? 'Resume' : 'Pause'}
            </button>
            &nbsp;
            <button onClick={this.handlePlayRandom}>
              I'm Feeling Lucky
            </button>
            {this.state.playerError &&
            <div className="App-error">ERROR: {this.state.playerError}</div>
            }
            <Slider
              pos={this.getSongPos()}
              onDrag={this.handlePositionDrag}
              onChange={this.handlePositionDrop}/>
            {metadata &&
            <div className="Song-details">
              Time: {this.getTimeLabel()} / {this.getTime(this.state.currentSongDurationMs)}<br/>
              Speed: <input
              disabled={this.player ? null : true}
              type="range" value={this.state.tempo}
              min="0.3" max="2.0" step="0.05"
              onInput={this.handleTempoChange}
              onChange={this.handleTempoChange}/>&nbsp;
              {this.state.tempo.toFixed(2)}<br/>
              {this.state.currentSongNumSubtunes > 1 &&
              <span>
                  Subtune: {this.state.currentSongSubtune + 1} of {this.state.currentSongNumSubtunes}&nbsp;
                <button
                  disabled={this.player ? null : true}
                  onClick={this.prevSubtune}>Prev</button>
                &nbsp;
                <button
                  disabled={this.player ? null : true}
                  onClick={this.nextSubtune}>Next</button><br/>
                </span>
              }
              Voices:&nbsp;
              {[...Array(this.state.currentSongNumVoices)].map((_, i) => {
                return (
                  <label className="App-label" key={i}>
                    <input
                      type="checkbox" onChange={() => {
                      this.handleVoiceToggle(i)
                    }} checked={this.state.voices[i]}/>
                    {this.state.voiceNames[i]}
                  </label>
                )
              })}<br/><br/>
              Title: {metadata.title || '--'}<br/>
              Game: {metadata.game || '--'}<br/>
              Artist: {metadata.artist || '--'}<br/>
              System: {metadata.system || '--'}<br/>
              Copyright: {metadata.copyright || '--'}<br/>
              Comment: {metadata.comment || '--'}
              {playerParams.length > 0 &&
              <div>
                <h3>Player Settings</h3>
                {playerParams.map(param => {
                  switch (param.type) {
                    case 'enum':
                      return (
                        <div key={param.id}>
                          {param.label}:&nbsp;
                          <select
                            onChange={(e) => this.player.setParameter(param.id, e.target.value)}
                            defaultValue={this.player.getParameter(param.id)}>
                            {param.options.map(optgroup =>
                              <optgroup key={optgroup.label} label={optgroup.label}>
                                {optgroup.items.map(option =>
                                  <option key={option.value} value={option.value}>{option.label}</option>
                                )}
                              </optgroup>
                            )}
                          </select>
                        </div>
                      );
                    case 'number':
                      return (
                        <div key={param.id}>
                          {param.label}:&nbsp;
                          <input type="range"
                                 min={param.min} max={param.max} step={param.step}
                                 value={this.player.getParameter(param.id)}
                                 onChange={(e) => this.player.setParameter(param.id, e.target.value)}>
                          </input>
                        </div>
                      );
                    default:
                      return null;
                  }
                })}
              </div>
              }
            </div>
            }
            {window.location.search.indexOf('debug=true') !== -1 && songData.map(group => {
              return (
                <div key={group.title}>
                  <h4>{group.title}</h4>
                  {group.files.map(file => {
                    const href = group.url_prefix + file;
                    return (
                      <div key={file}>
                        <a onClick={handleFileClick(href)} href={href}>{unescape(file)}</a>
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
