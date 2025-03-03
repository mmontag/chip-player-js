import React, { useContext } from 'react';
import autoBindReact from 'auto-bind/react';
import isMobile from 'ismobilejs';
import clamp from 'lodash/clamp';
import shuffle from 'lodash/shuffle';
import path from 'path';
import queryString from 'querystring';
import { NavLink, Route, Switch, withRouter } from 'react-router-dom';
import Dropzone from 'react-dropzone';

import ChipCore from '../chip-core';
import {
  API_BASE,
  CATALOG_PREFIX,
  MAX_VOICES,
  REPLACE_STATE_ON_SEEK,
  SOUNDFONT_MOUNTPOINT,
  MAX_SAMPLE_RATE,
  FORMATS,
} from '../config';
import {
  ensureEmscFileWithData,
  getMetadataUrlForCatalogUrl,
  pathJoin,
  titlesFromMetadata,
  unlockAudioContext
} from '../util';
import requestCache from '../RequestCache';
import LocalFilesManager from '../LocalFilesManager';
import Sequencer, { NUM_REPEAT_MODES, NUM_SHUFFLE_MODES, REPEAT_OFF, SHUFFLE_OFF } from '../Sequencer';

import GMEPlayer from '../players/GMEPlayer';
import MIDIPlayer from '../players/MIDIPlayer';
import V2MPlayer from '../players/V2MPlayer';
import XMPPlayer from '../players/XMPPlayer';
import N64Player from '../players/N64Player';
import MDXPlayer from '../players/MDXPlayer';
import VGMPlayer from '../players/VGMPlayer';

import AppFooter from './AppFooter';
import AppHeader from './AppHeader';
import Browse from './Browse';
import DropMessage from './DropMessage';
import Favorites from './Favorites';
import Search from './Search';
import Visualizer from './Visualizer';
import Toast, { ToastLevels } from './Toast';
import MessageBox from './MessageBox';
import Settings from './Settings';
import LocalFiles from './LocalFiles';
import { UserContext } from './UserProvider';
import { ToastContext } from './ToastProvider';
import Announcements from './Announcements';

const BASE_URL = process.env.PUBLIC_URL || document.location.origin;

class App extends React.Component {
  constructor(props) {
    super(props);
    autoBindReact(this);

    this.attachMediaKeyHandlers();
    this.contentAreaRef = React.createRef();
    this.listRef = React.createRef(); // react-virtualized List component ref
    this.playContexts = {};
    this.midiPlayer = null; // Need a reference to MIDIPlayer to handle SoundFont loading.
    window.ChipPlayer = this;


    // Initialize audio graph
    // ┌────────────┐      ┌────────────┐      ┌─────────────┐
    // │ playerNode ├─────>│  gainNode  ├─────>│ destination │
    // └────────────┘      └────────────┘      └─────────────┘
    let audioCtx = this.audioCtx = window.audioCtx = new (window.AudioContext || window.webkitAudioContext)({
      latencyHint: 'playback'
    });

    // Limit the sample rate if needed
    if (audioCtx.sampleRate > MAX_SAMPLE_RATE) {
      console.warn("AudioContext default sample rate was too high (%s). Limiting to %s.", audioCtx.sampleRate, MAX_SAMPLE_RATE);
      let targetRate = audioCtx.sampleRate;
      while (targetRate > MAX_SAMPLE_RATE) {
        targetRate /= 2;
      }
      audioCtx = this.audioCtx = window.audioCtx = new (window.AudioContext || window.webkitAudioContext)({
        latencyHint: 'playback',
        sampleRate: targetRate,
      });
    }

    const bufferSize = Math.max( // Make sure script node bufferSize is at least baseLatency
      Math.pow(2, Math.ceil(Math.log2((audioCtx.baseLatency || 0.001) * audioCtx.sampleRate))), 2048);
    const gainNode = this.gainNode = audioCtx.createGain();
    gainNode.gain.value = 1;
    gainNode.connect(audioCtx.destination);
    const playerNode = this.playerNode = audioCtx.createScriptProcessor(bufferSize, 0, 2);
    playerNode.connect(gainNode);

    unlockAudioContext(audioCtx);
    console.log('Sample rate: %d hz. Base latency: %d. Buffer size: %d.',
      audioCtx.sampleRate, audioCtx.baseLatency * audioCtx.sampleRate, bufferSize);

    this.state = {
      loading: true,
      loadingLocalFiles: true,
      paused: true,
      ejected: true,
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
      songUrl: null,
      volume: 100,
      repeat: REPEAT_OFF,
      shuffle: SHUFFLE_OFF,
      directories: {},
      hasPlayer: false,
      paramDefs: [],
      // Special playable contexts
      localFiles: [],
    };

    this.initChipCore(audioCtx, playerNode, bufferSize);
  }

  async initChipCore(audioCtx, playerNode, bufferSize) {
    // Load the chip-core Emscripten runtime
    try {
      this.chipCore = await new ChipCore({
        // Look for .wasm file in web root, not the same location as the app bundle (static/js).
        locateFile: (path, prefix) => {
          if (path.endsWith('.wasm') || path.endsWith('.wast'))
            return `${BASE_URL}/${path}`;
          return prefix + path;
        },
        print: (msg) => console.debug('[stdout] ' + msg),
        printErr: (msg) => console.debug('[stderr] ' + msg),
      });
    } catch (e) {
      // Browser doesn't support WASM (Safari in iOS Simulator)
      this.setState({ loading: false });
      this.props.toastContext.enqueueToast({ message: 'Error loading player engine. Old browser?', level: ToastLevels.ERROR });
      return;
    }

    // Get debug from location.search
    const debug = queryString.parse(window.location.search.substring(1)).debug;
    // Create all the players. Players will set up IDBFS mount points.
    const players = [
      MIDIPlayer,
      GMEPlayer,
      XMPPlayer,
      V2MPlayer,
      N64Player,
      MDXPlayer,
      VGMPlayer,
    ].map(P => new P(this.chipCore, audioCtx.sampleRate, bufferSize, debug));
    players.forEach(p => {
      p.audioNode = this.playerNode;
    });
    this.midiPlayer = players[0];

    // Set up the central audio processing callback. This is where the magic happens.
    playerNode.onaudioprocess = (e) => {
      const channels = [];
      for (let i = 0; i < e.outputBuffer.numberOfChannels; i++) {
        channels.push(e.outputBuffer.getChannelData(i));
      }
      for (let player of players) {
        if (player.stopped) continue;
        player.processAudio(channels);
      }
    }

    // Create localFilesManager before performing the syncfs.
    this.localFilesManager = new LocalFilesManager(this.chipCore.FS, 'local');

    // Populate all mounted IDBFS file systems from IndexedDB.
    this.chipCore.FS.syncfs(true, (err) => {
      if (err) {
        console.log('Error populating FS from indexeddb.', err);
      }
      players.forEach(player => player.handleFileSystemReady());
      this.updateLocalFiles();
      this.setState({ loadingLocalFiles: false });
    });

    this.sequencer = new Sequencer(players, this.localFilesManager);
    this.sequencer.on('sequencerStateUpdate', this.handleSequencerStateUpdate);
    this.sequencer.on('playerError', (message) => this.props.toastContext.enqueueToast(message, ToastLevels.ERROR));

    // TODO: Move to separate processUrlParams method.
    const urlParams = queryString.parse(window.location.search.substring(1));
    if (urlParams.play) {
      // Treat play params as "transient command" and strip them after starting playback.
      // See comment in Browse.js for more about why a sticky play param is not a good idea.
      const playPath = urlParams.play;
      const subtune = urlParams.subtune ? parseInt(urlParams.subtune, 10) : 0;
      const time = urlParams.t ? parseInt(urlParams.t, 10) : 0;
      delete urlParams.play;
      delete urlParams.subtune;
      delete urlParams.t;
      const qs = queryString.stringify(urlParams);
      const search = qs ? `?${qs}` : '';
      // Navigate to song's containing folder. History comes from withRouter().
      const dirname = path.dirname(playPath);
      this.fetchDirectory(dirname).then(() => {
        this.props.history.replace(`${pathJoin('/browse', dirname)}${search}`);
        // Convert play path to href (context contains full hrefs)
        const playHref = CATALOG_PREFIX + playPath;
        const index = this.playContexts[dirname].indexOf(playHref);

        this.playContext(this.playContexts[dirname], index, subtune);

        if (time) {
          setTimeout(() => {
            if (this.sequencer.getPlayer()) {
              this.sequencer.getPlayer().seekMs(time);
            }
          }, 100);
        }
      });
    }

    this.setState({ loading: false });
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
      voiceGroups: 'voiceGroups',
      songUrl: 'url',
      hasPlayer: 'hasPlayer',
      // TODO: add param values? move to paramStateUpdate?
      paramDefs: 'paramDefs',
      infoTexts: 'infoTexts',
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

  attachMediaKeyHandlers() {
    if ('mediaSession' in navigator) {
      console.log('Attaching Media Key event handlers.');

      // Limitations of MediaSession: there must always be an active audio element.
      // See https://bugs.chromium.org/p/chromium/issues/detail?id=944538
      //     https://github.com/GoogleChrome/samples/issues/637
      this.mediaSessionAudio = document.createElement('audio');
      this.mediaSessionAudio.src = BASE_URL + '/5-seconds-of-silence.mp3';
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

  playContext(context, index = 0, subtune = 0) {
    this.sequencer.playContext(context, index, subtune);
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
    console.debug('App.handleSequencerStateUpdate(isEjected=%s)', isEjected);

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
      const player = this.sequencer.getPlayer();
      const url = this.sequencer.getCurrUrl();
      // TODO: this is messy. imageUrl comes asynchronously from the /metadata request.
      //       Title, artist, etc. come synchronously from player.getMetadata().
      //       ...but these are also emitted with playerStateUpdate.
      //       It would be better to incorporate imageUrl into playerStateUpdate.
      if (!url) {
        this.setState({ imageUrl: null });
      } else if (url !== this.state.songUrl) {
        const metadataUrl = getMetadataUrlForCatalogUrl(url);
        // TODO: Disabled to support scroll restoration.
        // const filepath = url.replace(CATALOG_PREFIX, '');
        // updateQueryString({ play: filepath, t: undefined });
        // TODO: move fetch metadata to Player when it becomes event emitter
        requestCache.fetchCached(metadataUrl).then(response => {
          const { imageUrl, infoTexts } = response;
          const newInfoTexts = [ ...infoTexts, ...this.state.infoTexts ];
          this.setState({ imageUrl, infoTexts: newInfoTexts });

          if ('mediaSession' in navigator) {
            // Clear artwork if imageUrl is null.
            navigator.mediaSession.metadata.artwork = (imageUrl == null) ? [] : [{
              src: imageUrl,
              sizes: '512x512',
            }];
          }
        }).catch(e => {
          this.setState({ imageUrl: null });
        });
      }

      const metadata = player.getMetadata();

      if ('mediaSession' in navigator) {
        this.mediaSessionAudio.play();

        if ('MediaMetadata' in window) {
          navigator.mediaSession.metadata = new window.MediaMetadata({
            title: metadata.title || metadata.formatted?.title,
            artist: metadata.artist || metadata.formatted?.subtitle,
            album: metadata.game,
            artwork: []
          });
        }
      }

      this.setState({
        ...App.mapSequencerStateToAppState(sequencerState),
        // TODO: sparse state update
        showInfo: this.state.showInfo && player.getInfoTexts()?.length > 0,
      });
    }
  }

  togglePause() {
    if (this.state.ejected || !this.sequencer.getPlayer()) return;

    const paused = this.sequencer.getPlayer().togglePause();
    if ('mediaSession' in navigator) {
      if (paused) {
        this.mediaSessionAudio.pause();
      } else {
        this.mediaSessionAudio.play();
      }
    }
    this.setState({ paused: paused });
  }

  handleTimeSliderChange(event) {
    if (!this.sequencer.getPlayer()) return;

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
    if (!this.sequencer.getPlayer()) return;

    const durationMs = this.state.currentSongDurationMs;
    const seekMs = clamp(this.sequencer.getPlayer().getPositionMs() + ms, 0, durationMs);

    this.seekRelativeInner(seekMs);
  }

  seekRelativeInner(seekMs) {
    this.sequencer.getPlayer().seekMs(seekMs);
    this.setState({
      currentSongPositionMs: seekMs, // Smooth
    });
    setTimeout(() => {
      if (this.sequencer.getPlayer().isPlaying()) {
        this.setState({
          currentSongPositionMs: this.sequencer.getPlayer().getPositionMs(), // Accurate
        });
      }
    }, 100);
  }

  handleSetVoiceMask(voiceMask) {
    if (!this.sequencer.getPlayer()) return;

    this.sequencer.getPlayer().setVoiceMask(voiceMask);
    this.setState({ voiceMask: [...voiceMask] });
  }

  handleTempoChange(event) {
    if (!this.sequencer.getPlayer()) return;

    const tempo = parseFloat((event.target ? event.target.value : event)) || 1.0;
    this.sequencer.getPlayer().setTempo(tempo);
    this.setState({
      tempo: tempo
    });
  }

  setSpeedRelative(delta) {
    if (!this.sequencer.getPlayer()) return;

    const tempo = clamp(this.state.tempo + delta, 0.1, 2);
    this.sequencer.getPlayer().setTempo(tempo);
    this.setState({
      tempo: tempo
    });
  }

  handleShufflePlay(path) {
    if (path === 'favorites') {
      this.sequencer.playContext(shuffle(this.props.userContext.favesContext));
    } else if (path === 'local') {
      this.sequencer.playContext(shuffle(this.playContexts['local']));
    } else {
      // This is more like a synthetic recursive shuffle.
      // Response of this API is an array of *paths*.
      fetch(`${API_BASE}/shuffle?path=${encodeURI(path)}&limit=100`)
        .then(response => response.json())
        .then(json => json.items.map(this.pathToHref))
        .then(items => this.sequencer.playContext(items));
    }
  }

  handleCycleShuffle() {
    const shuffle = (this.state.shuffle + 1) % NUM_SHUFFLE_MODES;
    this.setState({ shuffle });
    this.sequencer.setShuffle(shuffle);
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
    this.gainNode.gain.value = Math.max(0, Math.min(2, volume * 0.01));
  }

  handleCycleRepeat() {
    // TODO: Handle dropped file repeat
    const repeat = (this.state.repeat + 1) % NUM_REPEAT_MODES;
    this.setState({ repeat });
    this.sequencer.setRepeat(repeat);
  }

  toggleInfo() {
    this.setState({
      showInfo: !this.state.showInfo,
    });
  }

  directoryListingToContext(items) {
    return items
      .filter(item => item.type === 'file')
      .map(item => this.pathToHref(item.path));
  }

  pathToHref(path) {
    return pathJoin(CATALOG_PREFIX, path.replace('%', '%25').replace('#', '%23'));
  }

  fetchDirectory(path) {
    const slashPath = pathJoin('/', path);
    return fetch(`${API_BASE}/browse?path=${encodeURIComponent(slashPath)}`)
      .then(response => response.json())
      .then(items => {
        this.playContexts[path] = this.directoryListingToContext(items);
        items.forEach(item => {
          // Convert timestamp 1704067200 to ISO date 2024-01-01
          item.mtime = new Date(item.mtime * 1000).toISOString().split('T')[0];
          item.name = item.path.split('/').pop();
          // XXX: Escape immediately: the escaped URL is considered canonical.
          //      The URL must be decoded for display from here on out.
          item.path.replace('%', '%25').replace('#', '%23');
          if (item.type === 'file')
            item.href = pathJoin(CATALOG_PREFIX, item.path);
          else
            item.href = pathJoin('/browse', item.path);
        });

        if (path !== '') { // No '..' at top level browse path.
          // Use substring, not slice, to pass through strings that don't contain any '/'.
          const parentPath = path.substring(0, path.lastIndexOf('/'));
          items.unshift({
            type: 'directory',
            path: parentPath,
            href: pathJoin('/browse', parentPath),
            name: '..',
          });
        }

        const directories = {
          ...this.state.directories,
          [path]: items,
        };
        this.setState({ directories });
      });
  }

  getCurrentSongLink(withSubtune = false) {
    const url = this.sequencer?.getCurrUrl();
    if (!url) return null;
    let link = BASE_URL + '/?play=' +
      encodeURIComponent(pathJoin('/', url.replace(CATALOG_PREFIX, '')));
    if (withSubtune) {
      const subtune = this.sequencer?.getSubtune();
      if (subtune !== 0) {
        link += '&subtune=' + subtune;
      }
    }
    return link;
  }

  updateLocalFiles() {
    const localFiles = this.localFilesManager.readAll();
    // Convert timestamp 1704067200 to ISO date 2024-01-01
    localFiles.forEach(item => item.mtime = new Date(item.mtime * 1000).toISOString().split('T')[0]);
    this.playContexts['local'] = localFiles.map(item => item.path);
    this.setState({ localFiles });
  }

  onDrop = (droppedFiles) => {
    const promises = droppedFiles.map(file => {
      return new Promise((resolve, reject) => {
        // TODO: refactor, avoid creating new reader/handlers for every dropped file.
        const reader = new FileReader();
        reader.onerror = (event) => reject(event.target.error);
        reader.onload = async () => {
          const ext = path.extname(file.name).toLowerCase().substring(1);
          if (ext === 'sf2') {
            // Handle dropped Soundfont
            if (!this.midiPlayer) {
              reject('MIDIPlayer has not been created - unable to load SoundFont.');
            } else if (droppedFiles.length !== 1) {
              reject('Soundfonts must be added one at a time, separate from other files.');
            } else {
              const sf2Path = `user/${file.name}`;
              await ensureEmscFileWithData(this.chipCore, `${SOUNDFONT_MOUNTPOINT}/${sf2Path}`, new Uint8Array(reader.result), /*forceWrite=*/true);
              this.midiPlayer.updateSoundfontParamDefs();
              this.midiPlayer.setParameter('soundfont', sf2Path, /*isTransient=*/false);
              // TODO: emit "paramDefsChanged" from player.
              // See https://reactjs.org/docs/integrating-with-other-libraries.html#integrating-with-model-layers
              this.forceUpdate();
              resolve(0);
            }
          } else if (FORMATS.includes(ext)) {
            // Handle dropped song file
            const songData = reader.result;
            this.localFilesManager.write(path.join('local', file.name), songData);
            resolve(1);
          } else {
            reject(`The file format ".${ext}" was not recognized.`);
          }
        };
        reader.readAsArrayBuffer(file);
      });
    });

    Promise.allSettled(promises).then(results => {
      const numSongsAdded = results
        .filter(result => result.status === 'fulfilled')
        .reduce((acc, result) => acc + result.value, 0);
      if (numSongsAdded > 0) {
        const currContextIsLocalFiles = this.sequencer?.getCurrContext() === this.playContexts['local'];
        this.updateLocalFiles();
        this.props.history.push('/local');
        if (currContextIsLocalFiles) this.sequencer.context = this.playContexts['local'];
        this.forceUpdate();
      }
      // Display all rejection reasons with duplicate reasons removed.
      results.filter(result => result.status === 'rejected')
        .reduce((acc, result) => acc.includes(result.reason) ? acc : [ ...acc, result.reason ], [])
        .forEach((reason, i) => setTimeout(() => this.props.toastContext.enqueueToast(reason, ToastLevels.ERROR), i * 1500));
    });
  };

  handleLocalFileDelete = (filePaths) => {
    if (!Array.isArray(filePaths)) filePaths = [filePaths];
    const currContextIsLocalFiles = this.sequencer?.getCurrContext() === this.playContexts['local'];
    let currIndexWasDeleted = false;
    filePaths.forEach(filePath => {
      const deleted = this.localFilesManager.delete(filePath);

      if (deleted && currContextIsLocalFiles) {
        const index = this.playContexts['local'].indexOf(filePath);
        if (index === this.sequencer.currIdx) currIndexWasDeleted = true;
        if (index <= this.sequencer.currIdx) {
          this.sequencer.currIdx--;
        }
      }
    });

    this.updateLocalFiles();
    if (currContextIsLocalFiles) this.sequencer.context = this.playContexts['local'];
    if (currIndexWasDeleted)     this.sequencer.nextSong();
  }

  handleCopyLink = (url) => {
    navigator.clipboard.writeText(url);
    this.props.toastContext.enqueueToast('Copied song link to clipboard.', ToastLevels.INFO);
  }

  render() {
    const { title, subtitle } = titlesFromMetadata(this.state.currentSongMetadata);
    const currContext = this.sequencer?.getCurrContext();
    const currIdx = this.sequencer?.getCurrIdx();
    const search = { search: window.location.search };
    const { showPlayerSettings, handleToggleSettings } = this.props.userContext;

    return (
      <Dropzone
        disableClick
        style={{}} // Required to clear Dropzone styles
        onDrop={this.onDrop}>{dropzoneProps => (
        <div className="App">
          <DropMessage dropzoneProps={dropzoneProps}/>
          <MessageBox showInfo={this.state.showInfo}
                      infoTexts={this.state.infoTexts}
                      toggleInfo={this.toggleInfo}/>
          <Toast/>
          <AppHeader/>
          <div className="App-main">
            <div className="App-main-inner">
              <div className="tab-container">
                <NavLink className="tab" activeClassName="tab-selected" to={{ pathname: "/", ...search }}
                         exact>Search</NavLink>
                <NavLink className="tab" activeClassName="tab-selected"
                         to={{ pathname: "/browse", ...search }}>Browse</NavLink>
                <NavLink className="tab" activeClassName="tab-selected"
                         to={{ pathname: "/favorites", ...search }}>Favorites</NavLink>
                <NavLink className="tab" activeClassName="tab-selected"
                         to={{ pathname: "/local", ...search }}>Local</NavLink>
                {/* this.sequencer?.players?.map((p, i) => `p${i}:${p.stopped?'off':'on'}`).join(' ') */}
                <button className={`tab tab-settings ${showPlayerSettings ? 'tab-selected' : ''}`}
                        onClick={handleToggleSettings}>Settings</button>
              </div>
              <div className="App-main-content-and-settings">
              <div className="App-main-content-area"
                   ref={this.contentAreaRef}>
                <Switch>
                  <Route path="/favorites" render={() => (
                    <Favorites
                      scrollContainerRef={this.contentAreaRef}
                      handleShufflePlay={this.handleShufflePlay}
                      onSongClick={this.handleSongClick}
                      currContext={currContext}
                      currIdx={currIdx}
                      listRef={this.listRef}/>
                  )}/>
                  <Route path="/browse/:browsePath*" render={({ history, match, location }) => {
                    // Undo the react-router-dom double-encoded % workaround - see DirectoryLink.js
                    const browsePath = match.params?.browsePath?.replace('%25', '%') || '';
                    return (
                      this.contentAreaRef.current &&
                      <Browse currContext={currContext}
                              currIdx={currIdx}
                              history={history}
                              locationKey={location.key}
                              browsePath={browsePath}
                              listing={this.state.directories[browsePath]}
                              playContext={this.playContexts[browsePath]}
                              fetchDirectory={this.fetchDirectory}
                              onSongClick={this.handleSongClick}
                              handleShufflePlay={this.handleShufflePlay}
                              scrollContainerRef={this.contentAreaRef}
                              listRef={this.listRef}
                      />
                    );
                  }}/>
                  <Route path="/local" render={() => (
                    <LocalFiles
                      loading={this.state.loadingLocalFiles}
                      handleShufflePlay={this.handleShufflePlay}
                      onSongClick={this.handleSongClick}
                      onDelete={this.handleLocalFileDelete}
                      playContext={this.playContexts['local']}
                      currContext={currContext}
                      currIdx={currIdx}
                      listing={this.state.localFiles}/>
                  )}/>>
                  <Route path="/settings" render={() => (
                    <Settings
                      ejected={this.state.ejected}
                      tempo={this.state.tempo}
                      currentSongNumVoices={this.state.currentSongNumVoices}
                      voiceMask={this.state.voiceMask}
                      voiceNames={this.state.voiceNames}
                      handleSetVoiceMask={this.handleSetVoiceMask}
                      handleTempoChange={this.handleTempoChange}
                      sequencer={this.sequencer}
                      />
                  )}/>
                  {/* Catch-all route */}
                  <Route render={({history}) => (
                    this.contentAreaRef.current &&
                    <Search
                      history={history}
                      currContext={currContext}
                      currIdx={currIdx}
                      onSongClick={this.handleSongClick}
                      scrollContainerRef={this.contentAreaRef}
                      listRef={this.listRef}
                    >
                      {this.state.loading && <p>Loading player engine...</p>}
                      <Announcements/>
                    </Search>
                  )}/>
                </Switch>
              </div>
                { showPlayerSettings &&
                <div className="App-main-content-area settings">
                  <Settings
                    ejected={this.state.ejected}
                    tempo={this.state.tempo}
                    currentSongNumVoices={this.state.currentSongNumVoices}
                    voiceMask={this.state.voiceMask}
                    voiceNames={this.state.voiceNames}
                    voiceGroups={this.state.voiceGroups}
                    handleSetVoiceMask={this.handleSetVoiceMask}
                    handleTempoChange={this.handleTempoChange}
                    sequencer={this.sequencer}
                  />
                </div>
                }
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
            getCurrentSongLink={this.getCurrentSongLink}
            handleCopyLink={this.handleCopyLink}
            handleCycleRepeat={this.handleCycleRepeat}
            handleCycleShuffle={this.handleCycleShuffle}
            handleSetVoiceMask={this.handleSetVoiceMask}
            handleTempoChange={this.handleTempoChange}
            handleTimeSliderChange={this.handleTimeSliderChange}
            handleVolumeChange={this.handleVolumeChange}
            imageUrl={this.state.imageUrl}
            infoTexts={this.state.infoTexts}
            nextSong={this.nextSong}
            nextSubtune={this.nextSubtune}
            paused={this.state.paused}
            prevSong={this.prevSong}
            prevSubtune={this.prevSubtune}
            repeat={this.state.repeat}
            shuffle={this.state.shuffle}
            sequencer={this.sequencer}
            songUrl={this.state.songUrl}
            subtitle={subtitle}
            tempo={this.state.tempo}
            title={title}
            toggleInfo={this.toggleInfo}
            togglePause={this.togglePause}
            voiceNames={this.state.voiceNames}
            voiceMask={this.state.voiceMask}
            volume={this.state.volume}
          />
        </div>
      )}</Dropzone>
    );
  }
}

// TODO: convert App to a function component and remove this.
// Inject contexts as props since class components only support a single context.
const AppWithContext = (props) => {
  const userContext = useContext(UserContext);
  const toastContext = useContext(ToastContext);
  return (<App {...props} userContext={userContext} toastContext={toastContext} />);
}

export default withRouter(AppWithContext);
