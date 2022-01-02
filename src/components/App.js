import React from 'react';
import isMobile from 'ismobilejs';
import clamp from 'lodash/clamp';
import path from 'path';
import queryString from 'querystring';
import * as firebase from 'firebase/app';
import 'firebase/auth';
import 'firebase/firestore';
import { NavLink, Route, Switch, withRouter } from 'react-router-dom';
import Dropzone from 'react-dropzone';
/* eslint import/no-webpack-loader-syntax: off */
import workletUrl from 'worklet-loader!../players/ChipWorkletProcessor';
import * as Comlink from 'comlink';

import ChipCore from '../chip-core';
import firebaseConfig from '../config/firebaseConfig';
import { API_BASE, CATALOG_PREFIX, MAX_VOICES, REPLACE_STATE_ON_SEEK } from '../config';
import { replaceRomanWithArabic, titlesFromMetadata, unlockAudioContext } from '../util';
import requestCache from '../RequestCache';
import Sequencer, { NUM_REPEAT_MODES, REPEAT_OFF } from '../Sequencer';

import GMEPlayer from '../players/GMEPlayer';
import MIDIPlayer from '../players/MIDIPlayer';
import V2MPlayer from '../players/V2MPlayer';
import XMPPlayer from '../players/XMPPlayer';
import N64Player from '../players/N64Player';
import MDXPlayer from '../players/MDXPlayer';

import AppFooter from './AppFooter';
import AppHeader from './AppHeader';
import Browse from './Browse';
import DropMessage from './DropMessage';
import Favorites from './Favorites';
import Search from './Search';
import Visualizer from './Visualizer';

const NUMERIC_COLLATOR = new Intl.Collator(undefined, { numeric: true, sensitivity: 'base' });

// Here we have the first of many gross hacks for AudioWorklets. I'm abandoning the whole idea.
// After all this work, there were still audio glitches because the emulation cores are running
// on the AudioWorklet thread. Thus in order to do a seek, the code blocks for 2 seconds and freezes out the
// audio buffer, and nothing was solved.
//
// The only way to avoid it is to fill a ring buffer on a *worker* thread that is also readable from
// AudioWorklet thread. And then getting into the world of shared array buffers which are still poorly
// supported, compounding the browser issues.
//
// So, no thanks. Maybe next year.
const portableFetch = (url) => {
  return fetch(url).then(response => {
    if (typeof registerProcessor === 'function') {
      return Comlink.transfer(response.arrayBuffer());
    } else {
      return response.arrayBuffer();
    }
  });
}

class App extends React.Component {
  constructor(props) {
    super(props);

    this.togglePause = this.togglePause.bind(this);
    this.toggleSettings = this.toggleSettings.bind(this);
    this.toggleInfo = this.toggleInfo.bind(this);
    this.playContext = this.playContext.bind(this);
    this.prevSong = this.prevSong.bind(this);
    this.nextSong = this.nextSong.bind(this);
    this.prevSubtune = this.prevSubtune.bind(this);
    this.nextSubtune = this.nextSubtune.bind(this);
    this.handleCycleRepeat = this.handleCycleRepeat.bind(this);
    this.handleVolumeChange = this.handleVolumeChange.bind(this);
    this.handleTimeSliderChange = this.handleTimeSliderChange.bind(this);
    this.handleTempoChange = this.handleTempoChange.bind(this);
    this.handleSetVoiceMask = this.handleSetVoiceMask.bind(this);
    this.handleSequencerStateUpdate = this.handleSequencerStateUpdate.bind(this);
    this.handlePlayerError = this.handlePlayerError.bind(this);
    this.handlePlayRandom = this.handlePlayRandom.bind(this);
    this.handleShufflePlay = this.handleShufflePlay.bind(this);
    this.handleSongClick = this.handleSongClick.bind(this);
    this.handleLogin = this.handleLogin.bind(this);
    this.handleLogout = this.handleLogout.bind(this);
    this.handleToggleFavorite = this.handleToggleFavorite.bind(this);
    this.attachMediaKeyHandlers = this.attachMediaKeyHandlers.bind(this);
    this.fetchDirectory = this.fetchDirectory.bind(this);
    this.setSpeedRelative = this.setSpeedRelative.bind(this);
    this.getCurrentSongLink = this.getCurrentSongLink.bind(this);

    this.attachMediaKeyHandlers();
    this.contentAreaRef = React.createRef();
    this.playContexts = {};
    window.ChipPlayer = this;

    // Initialize Firebase
    if (firebase.apps.length === 0) firebase.initializeApp(firebaseConfig);
    this.db = firebase.firestore();
    firebase.auth().onAuthStateChanged(user => {
      this.setState({ user: user, loadingUser: !!user });
      if (user) {
        this.db
          .collection('users')
          .doc(user.uid)
          .get()
          .then(userSnapshot => {
            if (!userSnapshot.exists) {
              // Create user
              this.db.collection('users').doc(user.uid).set({
                faves: [],
                settings: {},
              });
            } else {
              // Restore user
              const data = userSnapshot.data();
              this.setState({
                faves: data.faves || [],
                showPlayerSettings: data.settings ? data.settings.showPlayerSettings : false,
              });
            }
          })
          .finally(() => {
            this.setState({ loadingUser: false });
          });
      }
    });

    // Initialize audio graph
    const audioCtx = this.audioCtx = window.audioCtx = new (window.AudioContext || window.webkitAudioContext)({
      latencyHint: 'playback'
    });
    const bufferSize = Math.max( // Make sure script node bufferSize is at least baseLatency
      Math.pow(2, Math.ceil(Math.log2((audioCtx.baseLatency || 0.001) * audioCtx.sampleRate))), 2048);
    const gainNode = audioCtx.createGain();
    gainNode.gain.value = 1;
    gainNode.connect(audioCtx.destination);
    const audioNode = audioCtx.createScriptProcessor(bufferSize, 2, 2);
    audioNode.connect(gainNode);
    const playerNode = this.playerNode = gainNode;

    unlockAudioContext(audioCtx);
    console.log('Sample rate: %d hz. Base latency: %d. Buffer size: %d.',
      audioCtx.sampleRate, audioCtx.baseLatency * audioCtx.sampleRate, bufferSize);

    this.state = {
      loading: true,
      loadingUser: true,
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
      voiceMask: Array(MAX_VOICES).fill(true),
      voiceNames: Array(MAX_VOICES).fill(''),
      imageUrl: null,
      infoTexts: [],
      showInfo: false,
      showPlayerSettings: false,
      user: null,
      faves: [],
      songUrl: null,
      volume: 100,
      repeat: REPEAT_OFF,
      directories: {},
      hasPlayer: false,
      paramDefs: [],
    };

    const urlParams = queryString.parse(window.location.search.substr(1));
    if (urlParams.worker) {
      // const workletUrl = '../players/ChipWorkletProcessor';
      audioCtx.audioWorklet.addModule(workletUrl).then(() => {
        const chipWorkletNode = new AudioWorkletNode(audioCtx, 'chip-worklet-processor', {
          numberOfInputs: 0,
          numberOfOutputs: 1,
          outputChannelCount: [2],
        });
        WebAssembly.compileStreaming(fetch(`${process.env.PUBLIC_URL}/chip-core.wasm`))
          .then(wasmData => chipWorkletNode.port.postMessage({ type: 'WASM_DATA', payload: wasmData }));
        chipWorkletNode.connect(playerNode);

        Comlink.expose({
          fetch: portableFetch,
        }, chipWorkletNode.port);

        // const sequencer = Comlink.wrap(chipWorkletNode.port).sequencer;
        // setTimeout(() => {
        //   console.log('Playing song on AudioWorklet...');
        //   sequencer.on('sequencerStateUpdate', Comlink.proxy((e) => {
        //     console.log('Got sequencerStateUpdate', e);
        //     const newState = App.mapSequencerStateToAppState(e);
        //     console.log('New State', newState);
        //     this.setState({...newState, loading: false, ejected: false });
        //   }));
        //   sequencer.playSong("https://gifx.co/music/Nintendo%20SNES/Last%20Bible%20III/31%20Boss%20Battle%202.spc");
        //   this.sequencer = sequencer;
        // }, 2000);
        const chipWorkletRPC = Comlink.wrap(chipWorkletNode.port);
        const players = [
          chipWorkletRPC.gmePlayer,
          chipWorkletRPC.xmpPlayer,
          chipWorkletRPC.mdxPlayer,
          chipWorkletRPC.v2mPlayer,
          chipWorkletRPC.midiPlayer,
          chipWorkletRPC.n64Player,
        ];
        const sequencer = new Sequencer(players, portableFetch);
        sequencer.on('sequencerStateUpdate', (e) => {
          console.log('Got sequencerStateUpdate', e);
          const newState = App.mapSequencerStateToAppState(e);
          console.log('New State', newState);
          this.setState({...newState, loading: false, ejected: false });
        });
        setTimeout(() => {
          console.log('Playing song on AudioWorklet...');
          sequencer.playSong("https://gifx.co/music/Nintendo%20SNES/Last%20Bible%20III/31%20Boss%20Battle%202.spc");
          this.sequencer = sequencer;
        }, 2000);

        // const loadSong = (url) => {
        //   this.songRequest = promisify(new XMLHttpRequest());
        //   this.songRequest.responseType = 'arraybuffer';
        //   this.songRequest.open('GET', url);
        //   this.songRequest.send()
        //     .then(xhr => xhr.response)
        //     .then(buffer => {
        //       this.currUrl = url;
        //       const filepath = url.replace(CATALOG_PREFIX, '');
        //       chipWorkletNode.port.postMessage({ type: 'SEQUENCER_RPC', payload: ['playSongFile', filepath, buffer] });
        //     })
        //     .catch(e => console.error(e));
        // }
        // loadSong("https://gifx.co/music/VGM%20Rips/Snatcher_(Sega_CD_WIP)/03%20Junker%20HQ.vgm");
        // setTimeout(() => loadSong("https://gifx.co/music/VGM%20Rips/Snatcher_(Sega_CD_WIP)/02%20Evil%20Ripple.vgm"),        5000);
        // setTimeout(() => loadSong("https://gifx.co/music/Nintendo%20SNES/Last%20Bible%20III/31%20Boss%20Battle%202.spc"),   10000);
        // setTimeout(() => loadSong("https://gifx.co/music/VGM%20Rips/Snatcher_(Sega_CD_WIP)/25%20Blow%20Up%20Tricycle.vgm"), 15000);
        //
        // // Dummy sequencer
        this.sequencer = new Sequencer([]);
      });
      return;
    }
    // Load the chip-core Emscripten runtime
    try {
      const chipCore = this.chipCore = new ChipCore({
        // Look for .wasm file in web root, not the same location as the app bundle (static/js).
        locateFile: (path, prefix) => {
          if (path.endsWith('.wasm') || path.endsWith('.wast'))
            return `${process.env.PUBLIC_URL}/${path}`;
          return prefix + path;
        },
        onRuntimeInitialized: () => {
          const players = [
            new GMEPlayer(chipCore, audioCtx.sampleRate),
            new XMPPlayer(chipCore, audioCtx.sampleRate),
            new MIDIPlayer(chipCore, audioCtx.sampleRate),
            new V2MPlayer(chipCore, audioCtx.sampleRate),
            new N64Player(chipCore, audioCtx.sampleRate),
            new MDXPlayer(chipCore, audioCtx.sampleRate),
          ];
          this.sequencer = new Sequencer(players, portableFetch);
          this.sequencer.on('sequencerStateUpdate', this.handleSequencerStateUpdate);
          this.sequencer.on('playerError', this.handlePlayerError);

          audioNode.onaudioprocess = (e) => {
            const channels = [];
            for (let channel = 0; channel < e.outputBuffer.numberOfChannels; channel++) {
              channels[channel] = e.outputBuffer.getChannelData(channel);
            }
            for (let player of players) {
              // if (player.isPaused()) continue;
              player.process(channels);
            }
            // players[4].process(channels);
          }

          this.setState({ loading: false });

          // Experimental: Split Module Support
          //
          // chipCore.loadDynamicLibrary('./xmp.wasm', {
          //     loadAsync: true,
          //     global: true,
          //     nodelete: true,
          //   })
          //   .then(() => {
          //     return this.sequencer.setPlayers([new XMPPlayer(audioCtx, playerNode, chipCore)]);
          //   });

          if (urlParams.play) {
            const play = urlParams.play;
            const dirname = path.dirname(urlParams.play);
            // We treat play param as a "transient command" and strip it away after starting playback.
            // See comment in Browse.js for more about why a stateful play param is not a good idea.
            delete urlParams.play;
            const qs = queryString.stringify(urlParams);
            const search = qs ? `?${qs}` : '';
            // Navigate to song's containing folder. History comes from withRouter().
            this.fetchDirectory(dirname).then(() => {
              this.props.history.replace(`/browse/${dirname}${search}`);
              const index = this.playContexts[dirname].indexOf(play);
              this.playContext(this.playContexts[dirname], index);

              if (urlParams.t) {
                setTimeout(() => {
                  if (this.state.hasPlayer) {
                    this.sequencer.seekMs(parseInt(urlParams.t, 10));
                  }
                }, 100);
              }
            });
          }
        },
      });
    } catch (e) {
      // Browser doesn't support WASM (Safari in iOS Simulator)
      Object.assign(this.state, {
        playerError: 'Error loading player engine.',
        loading: false
      });
    }
  }

  static mapSequencerStateToAppState(sequencerState) {
    const map = {
      ejected: 'isEjected',
      paused: 'isPaused',
      currentSongSubtune: 'subtune',
      currentSongMetadata: 'metadata',
      currentSongNumVoices: 'numVoices',
      currentSongPositionMs: 'positionMs',
      currentSongDurationMs: 'durationMs',
      currentSongNumSubtunes: 'numSubtunes',
      tempo: 'tempo',
      voiceNames: 'voiceNames',
      voiceMask: 'voiceMask',
      songUrl: 'url',
      hasPlayer: 'hasPlayer',
      // TODO: add param values? move to paramStateUpdate?
      paramDefs: 'paramDefs',
    };
    const appState = {};
    for (let prop in map) {
      const seqProp = map[prop];
      if (seqProp in sequencerState) {
        appState[prop] = sequencerState[seqProp];
      }
    }
    return appState;
  }

  handleLogin() {
    const provider = new firebase.auth.GoogleAuthProvider();
    firebase.auth().signInWithPopup(provider).then(result => {
      console.log('Firebase auth result:', result);
    }).catch(error => {
      console.log('Firebase auth error:', error);
    });
  }

  handleLogout() {
    firebase.auth().signOut().then(() => {
      this.setState({
        user: null,
        faves: [],
      });
    });
  }

  handleToggleFavorite(path) {
    const user = this.state.user;
    if (user) {
      const userRef = this.db.collection('users').doc(user.uid);
      let newFaves, favesOp;
      const oldFaves = this.state.faves;
      const exists = oldFaves.includes(path);
      if (exists) {
        newFaves = oldFaves.filter(fave => fave !== path);
        favesOp = firebase.firestore.FieldValue.arrayRemove(path);
      } else {
        newFaves = [...oldFaves, path];
        favesOp = firebase.firestore.FieldValue.arrayUnion(path);
      }
      // Optimistic update
      this.setState({ faves: newFaves });
      userRef.update({ faves: favesOp }).catch((e) => {
        this.setState({ faves: oldFaves });
        console.log('Couldn\'t update favorites in Firebase.', e);
      });
    }
  }

  attachMediaKeyHandlers() {
    if ('mediaSession' in navigator) {
      console.log('Attaching Media Key event handlers.');

      // Limitations of MediaSession: there must always be an active audio element :(
      this.mediaSessionAudio = document.createElement('audio');
      this.mediaSessionAudio.src = process.env.PUBLIC_URL + '/5-seconds-of-silence.mp3';
      this.mediaSessionAudio.loop = true;
      this.mediaSessionAudio.volume = 0;

      navigator.mediaSession.setActionHandler('play', () => this.togglePause());
      navigator.mediaSession.setActionHandler('pause', () => this.togglePause());
      navigator.mediaSession.setActionHandler('previoustrack', () => this.prevSong());
      navigator.mediaSession.setActionHandler('nexttrack', () => this.nextSong());
      navigator.mediaSession.setActionHandler('seekbackward', () => this.seekRelative(-5000));
      navigator.mediaSession.setActionHandler('seekforward', () => this.seekRelative(5000));
    }

    document.addEventListener('keydown', (e) => {
      // Keyboard shortcuts: tricky to get it just right and keep the browser behavior intact.
      // The order of switch-cases matters. More privileged keys appear at the top.
      // More restricted keys appear at the bottom, after various input focus states are filtered out.
      if (e.ctrlKey || e.metaKey) return; // avoid browser keyboard shortcuts

      switch (e.key) {
        case 'Escape':
          this.setState({ showInfo: false });
          e.target.blur();
          break;
        default:
      }

      if (e.target.tagName === 'INPUT' && e.target.type === 'text') return; // text input has focus

      switch (e.key) {
        case ' ':
          this.togglePause();
          e.preventDefault();
          break;
        case '-':
          this.setSpeedRelative(-0.1);
          break;
        case '_':
          this.setSpeedRelative(-0.01);
          break;
        case '=':
          this.setSpeedRelative(0.1);
          break;
        case '+':
          this.setSpeedRelative(0.01);
          break;
        default:
      }

      if (e.target.tagName === 'INPUT' && e.target.type === 'range') return; // a range slider has focus

      switch (e.key) {
        case 'ArrowLeft':
          this.seekRelative(-5000);
          e.preventDefault();
          break;
        case 'ArrowRight':
          this.seekRelative(5000);
          e.preventDefault();
          break;
        default:
      }
    });
  }

  playContext(context, index = 0) {
    this.sequencer.playContext(context, index);
  }

  prevSong() {
    this.sequencer.prevSong();
  }

  nextSong() {
    this.sequencer.nextSong();
  }

  prevSubtune() {
    this.sequencer.prevSubtune();
  }

  nextSubtune() {
    this.sequencer.nextSubtune();
  }

  handleSequencerStateUpdate(sequencerState) {
    const { isEjected } = sequencerState;
    console.debug('handleSequencerStateUpdate(isEjected=%s)', isEjected);

    if (isEjected) {
      this.setState({
        ejected: true,
        currentSongSubtune: 0,
        currentSongMetadata: {},
        currentSongNumVoices: 0,
        currentSongPositionMs: 0,
        currentSongDurationMs: 1,
        currentSongNumSubtunes: 0,
        imageUrl: null,
        songUrl: null,
      });
      // TODO: Disabled to support scroll restoration.
      // updateQueryString({ play: undefined });

      if ('mediaSession' in navigator) {
        this.mediaSessionAudio.pause();

        navigator.mediaSession.playbackState = 'none';
        if ('MediaMetadata' in window) {
          navigator.mediaSession.metadata = new window.MediaMetadata({});
        }
      }
    } else {
      const url = this.sequencer.getCurrUrl();
      if (url) {
        const path = url.replace(CATALOG_PREFIX, '/');
        const metadataUrl = `${API_BASE}/metadata?path=${encodeURIComponent(path)}`;
        // TODO: Disabled to support scroll restoration.
        // const filepath = url.replace(CATALOG_PREFIX, '');
        // updateQueryString({ play: filepath, t: undefined });
        requestCache.fetchCached(metadataUrl).then(response => {
          const { imageUrl, infoTexts } = response;
          this.setState({ imageUrl: imageUrl, infoTexts: infoTexts });

          if ('mediaSession' in navigator) {
            // Clear artwork if imageUrl is null.
            navigator.mediaSession.metadata.artwork = (imageUrl == null) ? [] : [{
              src: imageUrl,
              sizes: '512x512',
            }];
          }

          if (infoTexts.length === 0) {
            this.setState({ showInfo: false });
          }
        }).catch(e => {
          this.setState({ imageUrl: null, infoTexts: [], showInfo: false });
        });
      } else {
        // Drag & dropped files reach this branch
        this.setState({ imageUrl: null, infoTexts: [], showInfo: false });
      }

      const metadata = this.sequencer.getMetadata();

      if ('mediaSession' in navigator) {
        this.mediaSessionAudio.play();

        if ('MediaMetadata' in window) {
          console.log('metadata', metadata);
          navigator.mediaSession.metadata = new window.MediaMetadata({
            title: metadata.title || metadata.formatted?.title,
            artist: metadata.artist || metadata.formatted?.subtitle,
            album: metadata.game,
            artwork: []
          });
        }
      }

      this.setState(App.mapSequencerStateToAppState(sequencerState));
    }
  }

  handlePlayerError(error) {
    this.setState({ playerError: error });
  }

  togglePause() {
    if (this.state.ejected || !this.state.hasPlayer) return;

    const paused = this.sequencer.togglePause();
    if ('mediaSession' in navigator) {
      if (paused) {
        this.mediaSessionAudio.pause();
      } else {
        this.mediaSessionAudio.play();
      }
    }
    this.setState({ paused: paused });
  }

  toggleSettings() {
    let showPlayerSettings = !this.state.showPlayerSettings;
    // Optimistic update
    this.setState({ showPlayerSettings: showPlayerSettings });

    const user = this.state.user;
    if (user) {
      const userRef = this.db.collection('users').doc(user.uid);
      userRef
        .update({ settings: { showPlayerSettings: showPlayerSettings } })
        .catch((e) => {
          console.log('Couldn\'t update settings in Firebase.', e);
        });
    }
  }

  handleTimeSliderChange(event) {
    if (!this.state.hasPlayer) return;

    const pos = event.target ? event.target.value : event;
    const seekMs = Math.floor(pos * this.state.currentSongDurationMs);

    this.seekRelativeInner(seekMs);

    if (REPLACE_STATE_ON_SEEK) {
      const urlParams = {
        ...queryString.parse(window.location.search.substr(1)),
        t: seekMs,
      };
      const stateUrl = '?' + queryString.stringify(urlParams)
        .replace(/%20/g, '+')
        .replace(/%2F/g, '/');
      window.history.replaceState(null, '', stateUrl);
    }
  }

  seekRelative(ms) {
    if (!this.state.hasPlayer) return;

    const durationMs = this.state.currentSongDurationMs;
    const seekMs = clamp(this.sequencer.getPositionMs() + ms, 0, durationMs);

    this.seekRelativeInner(seekMs);
  }

  seekRelativeInner(seekMs) {
    this.sequencer.seekMs(seekMs);
    this.setState({
      currentSongPositionMs: seekMs, // Smooth
    });
    setTimeout(() => {
      if (this.sequencer.isPlaying()) {
        this.setState({
          currentSongPositionMs: this.sequencer.getPositionMs(), // Accurate
        });
      }
    }, 100);
  }

  handleSetVoiceMask(voiceMask) {
    if (!this.state.hasPlayer) return;

    this.sequencer.setVoiceMask(voiceMask);
    this.setState({ voiceMask: [...voiceMask] });
  }

  handleTempoChange(event) {
    if (!this.state.hasPlayer) return;

    const tempo = parseFloat((event.target ? event.target.value : event)) || 1.0;
    this.sequencer.setTempo(tempo);
    this.setState({
      tempo: tempo
    });
  }

  setSpeedRelative(delta) {
    if (!this.state.hasPlayer) return;

    const tempo = clamp(this.state.tempo + delta, 0.1, 2);
    this.sequencer.setTempo(tempo);
    this.setState({
      tempo: tempo
    });
  }

  handlePlayRandom() {
    fetch(`${API_BASE}/random?limit=100`)
      .then(response => response.json())
      .then(json => this.sequencer.playContext(json.items.map(item => item.file), 10));
  }

  handleShufflePlay(path) {
    fetch(`${API_BASE}/shuffle?path=${encodeURI(path)}&limit=100`)
      .then(response => response.json())
      .then(json => this.sequencer.playContext(json.items));
  }

  handleSongClick(url, context, index) {
    return (e) => {
      e.preventDefault();
      if (context) {
        this.playContext(context, index);
      } else {
        this.sequencer.playSonglist([url]);
      }
    }
  }

  handleVolumeChange(volume) {
    this.setState({ volume });
    this.playerNode.gain.value = Math.max(0, Math.min(2, volume * 0.01));
  }

  handleCycleRepeat() {
    const repeat = (this.state.repeat + 1) % NUM_REPEAT_MODES;
    this.setState({ repeat });
    this.sequencer.setRepeat(repeat);
  }

  toggleInfo() {
    this.setState({
      showInfo: !this.state.showInfo,
    });
  }

  fetchDirectory(path) {
    return fetch(`${API_BASE}/browse?path=%2F${encodeURIComponent(path)}`)
      .then(response => response.json())
      .then(json => {
        const arabicMap = {};
        const needsRomanNumeralSort = json.some(item => {
          // Only convert Roman numerals if the list sort could benefit from it.
          // Roman numerals less than 9 would be sorted incidentally.
          // This assumes that
          // - Roman numerals are formatted with a period.
          // - Roman numeral ranges don't have gaps.
          return item.path.toLowerCase().indexOf('ix') > -1;
        });
        if (needsRomanNumeralSort) {
          console.log("Roman numeral sort is active for this directory");
          // Movement IV. Wow => Movement 0004. Wow
          json.forEach(item => arabicMap[item.path] = replaceRomanWithArabic(item.path));
        }
        const items = json
          .sort((a, b) => {
            const [strA, strB] = needsRomanNumeralSort ?
              [arabicMap[a.path], arabicMap[b.path]] :
              [a.path, b.path];
            return NUMERIC_COLLATOR.compare(strA, strB);
          })
          .sort((a, b) => {
            if (a.type < b.type) return -1;
            if (a.type > b.type) return 1;
            return 0;
          })
          .map((item, i) => {
            item.idx = i;
            return item;
          });
        this.playContexts[path] = items.map(item =>
          item.path.replace('%', '%25').replace('#', '%23').replace(/^\//, '')
        );
        const directories = {
          ...this.state.directories,
          [path]: items,
        };
        this.setState({ directories });
      });
  }

  getCurrentSongLink() {
    const url = this.state.songUrl;
    return url ? process.env.PUBLIC_URL + '/?play=' + encodeURIComponent(url.replace(CATALOG_PREFIX, '')) : '#';
  }

  onDrop = (droppedFiles) => {
    const reader = new FileReader();
    const file = droppedFiles[0];
    reader.onload = () => {
      const songData = reader.result;
      this.sequencer.playSongFile(file.name, songData);
    };
    reader.readAsArrayBuffer(file);
  };

  render() {
    const {title, subtitle} = titlesFromMetadata(this.state.currentSongMetadata);
    const currContext = this.sequencer ? this.sequencer.getCurrContext() : null;
    const currIdx = this.sequencer ? this.sequencer.getCurrIdx() : 0;
    const search = { search: window.location.search };
    return (
        <Dropzone
          disableClick
          style={{}}
          onDrop={this.onDrop}>{dropzoneProps => (
          <div className="App">
            <DropMessage dropzoneProps={dropzoneProps}/>
            <div hidden={!this.state.showInfo} className="message-box-outer">
              <div hidden={!this.state.showInfo} className="message-box">
                <div className="message-box-inner">
              <pre style={{ maxHeight: '100%', margin: 0 }}>
                {this.state.infoTexts[0]}
              </pre>
                </div>
                <div className="message-box-footer">
                  <button className="box-button message-box-button" onClick={this.toggleInfo}>Close</button>
                </div>
              </div>
            </div>
            <AppHeader user={this.state.user}
                       handleLogout={this.handleLogout}
                       handleLogin={this.handleLogin}
                       isPhone={isMobile.phone}/>
            <div className="App-main">
              <div className="App-main-inner">
                <div className="tab-container">
                  <NavLink className="tab" activeClassName="tab-selected" to={{ pathname: "/", ...search }}
                           exact>Search</NavLink>
                  <NavLink className="tab" activeClassName="tab-selected"
                           to={{ pathname: "/browse", ...search }}>Browse</NavLink>
                  <NavLink className="tab" activeClassName="tab-selected"
                           to={{ pathname: "/favorites", ...search }}>Favorites</NavLink>
                </div>
                <div className="App-main-content-area" ref={this.contentAreaRef}>
                  <Switch>
                    <Route path="/" exact render={() => (
                      <Search
                        currContext={currContext}
                        currIdx={currIdx}
                        toggleFavorite={this.handleToggleFavorite}
                        favorites={this.state.faves}
                        onSongClick={this.handleSongClick}>
                        {this.state.loading && <p>Loading player engine...</p>}
                      </Search>
                    )}/>
                    <Route path="/favorites" render={() => (
                      <Favorites
                        user={this.state.user}
                        loadingUser={this.state.loadingUser}
                        handleLogin={this.handleLogin}
                        onSongClick={this.handleSongClick}
                        currContext={currContext}
                        currIdx={currIdx}
                        toggleFavorite={this.handleToggleFavorite}
                        favorites={this.state.faves}/>
                    )}/>
                    <Route path="/browse/:browsePath*" render={({ history, match, location }) => {
                      // Undo the react-router-dom double-encoded % workaround - see DirectoryLink.js
                      const browsePath = match.params?.browsePath?.replace('%25', '%') || '';
                      return (
                        this.contentAreaRef.current &&
                        <Browse currContext={currContext}
                                currIdx={currIdx}
                                historyAction={history.action}
                                locationKey={location.key}
                                browsePath={browsePath}
                                listing={this.state.directories[browsePath]}
                                playContext={this.playContexts[browsePath]}
                                fetchDirectory={this.fetchDirectory}
                                handleSongClick={this.handleSongClick}
                                handleShufflePlay={this.handleShufflePlay}
                                scrollContainerRef={this.contentAreaRef}
                                favorites={this.state.faves}
                                toggleFavorite={this.handleToggleFavorite}/>
                      );
                    }}/>
                  </Switch>
                </div>
              </div>
              {!isMobile.phone && !this.state.loading &&
              <Visualizer audioCtx={this.audioCtx}
                          sourceNode={this.playerNode}
                          chipCore={this.chipCore}
                          paused={this.state.ejected || this.state.paused}/>}
            </div>
            <AppFooter
              currentSongDurationMs={this.state.currentSongDurationMs}
              currentSongNumSubtunes={this.state.currentSongNumSubtunes}
              currentSongNumVoices={this.state.currentSongNumVoices}
              currentSongSubtune={this.state.currentSongSubtune}
              ejected={this.state.ejected}
              faves={this.state.faves}
              getCurrentSongLink={this.getCurrentSongLink}
              handleCycleRepeat={this.handleCycleRepeat}
              handlePlayerError={this.handlePlayerError}
              handlePlayRandom={this.handlePlayRandom}
              handleSetVoiceMask={this.handleSetVoiceMask}
              handleTempoChange={this.handleTempoChange}
              handleTimeSliderChange={this.handleTimeSliderChange}
              handleToggleFavorite={this.handleToggleFavorite}
              handleVolumeChange={this.handleVolumeChange}
              hasPlayer={this.state.hasPlayer}
              imageUrl={this.state.imageUrl}
              infoTexts={this.state.infoTexts}
              nextSong={this.nextSong}
              nextSubtune={this.nextSubtune}
              paramDefs={this.state.paramDefs}
              paused={this.state.paused}
              playerError={this.state.playerError}
              prevSong={this.prevSong}
              prevSubtune={this.prevSubtune}
              repeat={this.state.repeat}
              sequencer={this.sequencer}
              showPlayerSettings={this.state.showPlayerSettings}
              songUrl={this.state.songUrl}
              subtitle={subtitle}
              tempo={this.state.tempo}
              title={title}
              toggleInfo={this.toggleInfo}
              togglePause={this.togglePause}
              toggleSettings={this.toggleSettings}
              voiceNames={this.state.voiceNames}
              voiceMask={this.state.voiceMask}
              volume={this.state.volume}
            />
          </div>
        )}</Dropzone>
    );
  }
}

export default withRouter(App);
