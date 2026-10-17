import axios from 'redaxios';
import React, { useContext } from 'react';
import autoBindReact from 'auto-bind/react';
import isMobile from 'ismobilejs';
import clamp from 'lodash/clamp';
import shuffle from 'lodash/shuffle';
import pathe from 'pathe';
import { NavLink, Route, Switch, withRouter } from 'react-router-dom';
import Dropzone from 'react-dropzone';

// import ChipCore from '../chip-core';             // separate chunk in initChipCore
// import ChipCoreWasm from '../chip-core.wasm';    // found in chip-core.js by Webpack
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
  getMetadataUrlForFilepath,
  getUrlFromFilepath,
  isSongFolder,
  listingToContext,
  pathJoin,
  postWithOptionalAuth,
  songRef,
  songRefKey,
  songRefListsEqual,
  subtuneLabel,
  titlesFromMetadata,
  unlockAudioContext
} from '../util';
import LocalFilesManager from '../LocalFilesManager';
import { describeLocalSubtunes } from '../localFilesSubtunes';
import Sequencer, { NUM_REPEAT_MODES, NUM_SHUFFLE_MODES, REPEAT_OFF, SHUFFLE_OFF } from '../Sequencer';

import GMEPlayer from '../players/GMEPlayer';
import MIDIPlayer from '../players/MIDIPlayer';
import V2MPlayer from '../players/V2MPlayer';
import XMPPlayer from '../players/XMPPlayer';
import N64Player from '../players/N64Player';
import GSFPlayer from '../players/GSFPlayer';
import MDXPlayer from '../players/MDXPlayer';
import VGMPlayer from '../players/VGMPlayer';
import SIDPlayer from '../players/SIDPlayer';

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
import TopCharts from './TopCharts';
import { UserContext } from './UserProvider';
import { ToastContext } from './ToastProvider';
import Announcements from './Announcements';
import { getDomMetadataText } from '../metadata-inspector';

const BASE_URL = process.env.PUBLIC_URL || document.location.origin;

class App extends React.Component {
  constructor(props) {
    super(props);
    autoBindReact(this);

    this.attachMediaKeyHandlers();
    this.contentAreaRef = React.createRef();
    this.listRef = React.createRef(); // react-virtualized List component ref
    this.playContexts = {};
    // Guards updateLocalFiles against out-of-order sub-tune parses.
    this.localFilesGeneration = 0;
    this.midiPlayer = null; // Need a reference to MIDIPlayer to handle SoundFont loading.
    window.ChipPlayer = this;


    // Initialize audio graph
    // ┌────────────┐      ┌────────────┐      ┌─────────────┐
    // │ playerNode ├─────>│  gainNode  ├─────>│ destination │
    // └────────────┘      └────────────┘      └─────────────┘

    // Smaller buffer for mobile devices. 'interactive' yields 128 samples on iOS/Android.
    const latencyHint = isMobile.any ? 'interactive' : 'playback';
    let audioCtx = this.audioCtx = window.audioCtx = new (window.AudioContext || window.webkitAudioContext)({
      latencyHint,
    });

    // Limit the sample rate if needed
    if (audioCtx.sampleRate > MAX_SAMPLE_RATE) {
      console.warn("AudioContext default sample rate was too high (%s). Limiting to %s.", audioCtx.sampleRate, MAX_SAMPLE_RATE);
      let targetRate = audioCtx.sampleRate;
      while (targetRate > MAX_SAMPLE_RATE) {
        targetRate /= 2;
      }
      audioCtx = this.audioCtx = window.audioCtx = new (window.AudioContext || window.webkitAudioContext)({
        latencyHint,
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
      currentSongDurationMs: 1,
      currentSongPositionMs: 0,
      currentSongBuffer: null,
      tempo: 1,
      voiceMask: Array(MAX_VOICES).fill(true),
      voiceNames: Array(MAX_VOICES).fill(''),
      voiceGroups: [],
      imageUrl: null,
      infoTexts: [],
      showInfo: false,
      metadataText: null,
      songPath: null,
      songRef: null,
      songTitleKey: null,
      subtuneCount: null,
      subtuneTitle: null,
      songId: null,
      volume: 100,
      repeat: REPEAT_OFF,
      shuffle: SHUFFLE_OFF,
      directories: {},
      hasPlayer: false,
      paramDefs: [],
      paramValues: {},
      // Special playable contexts
      localFiles: [],
      theaterMode: false,
    };

    this.initChipCore(audioCtx, playerNode, bufferSize);
  }

  async initChipCore(audioCtx, playerNode, bufferSize) {
    // Load the chip-core Emscripten runtime
    const { default: ChipCore } = await import(/* webpackChunkName: "chip-core" */ '../chip-core');
    try {
      this.chipCore = await ChipCore({
        // Webpack performs automatic asset relinking on chip-core.wasm
        print: (msg) => console.debug('[stdout] ' + msg),
        printErr: (msg) => console.debug('[stderr] ' + msg),
      });
    } catch (e) {
      // Browser doesn't support WASM (Safari in iOS Simulator)
      this.setState({ loading: false });
      this.props.toastContext.enqueueToast('Error loading player engine. Old browser?', ToastLevels.ERROR);
      console.error(e);
      return;
    }

    // Get debug from location.search
    const searchParams = new URLSearchParams(window.location.search);
    const debug = searchParams.get('debug');
    // Create all the players. Players will set up IDBFS mount points.
    const players = [
      MIDIPlayer,
      GMEPlayer,
      XMPPlayer,
      V2MPlayer,
      N64Player,
      GSFPlayer,
      MDXPlayer,
      VGMPlayer,
      SIDPlayer,
    ].map(P => new P(this.chipCore, audioCtx.sampleRate, bufferSize, debug));
    const initialSilence = this.props.userContext?.settings?.silenceDuration ?? -1;
    players.forEach(p => {
      p.audioNode = this.playerNode;
      p.setSilenceDuration(initialSilence);
    });
    this.players = players;
    this.midiPlayer = players[0];

    // Set up the central audio processing callback. This is where the magic happens.
    playerNode.onaudioprocess = (e) => {
      const channels = [];
      for (let i = 0; i < e.outputBuffer.numberOfChannels; i++) {
        const chData = e.outputBuffer.getChannelData(i);
        chData.fill(0);
        channels.push(chData);
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
      // updateLocalFiles() owns the loading flag: parsing the sub-tune headers is
      // async, and clearing it here (before the parse) showed the empty-library
      // message to someone who had files.
      this.updateLocalFiles();
    });

    this.sequencer = new Sequencer(players, this.localFilesManager, () => this.props.userContext.settings);
    this.sequencer.on('sequencerStateUpdate', this.handleSequencerStateUpdate);
    this.sequencer.on('playerError', (message) => this.props.toastContext.enqueueToast(message, ToastLevels.ERROR));

    // TODO: Move to separate processUrlParams method.
    const urlParams = new URLSearchParams(window.location.search);
    // The server will read the ?play=... param and inject values in the __chipConfig object.
    if (window.__chipConfig?.songPath) {
      // Treat play params as "transient command" and strip them after starting playback.
      // See comment in Browse.js for more about why a sticky play param is not a good idea.
      const playPath = window.__chipConfig?.songPath;
      const subtuneParam = urlParams.get('subtune');
      const subtune = subtuneParam != null ? parseInt(subtuneParam, 10) : null;
      const tParam = urlParams.get('t');
      const time = tParam ? parseInt(tParam, 10) : 0;
      urlParams.delete('play');
      urlParams.delete('subtune');
      urlParams.delete('t');
      const qs = urlParams.toString();
      const search = qs ? `?${qs}` : '';
      const seekToTime = () => {
        if (time) {
          setTimeout(() => {
            if (this.sequencer.getPlayer()) {
              this.sequencer.getPlayer().seekMs(time);
            }
          }, 100);
        }
      };
      // A multi-song file is a virtual folder: land inside it with the
      // requested sub-tune selected, rather than on the parent directory.
      if (isSongFolder(window.__chipConfig)) {
        this.fetchDirectory(playPath).then(() => {
          this.props.history.replace(`${pathJoin('/browse', playPath)}${search}`);
          const folderContext = this.playContexts[playPath] || [];
          const target = subtune ?? 0;
          const index = folderContext.findIndex(ref => ref.path === playPath && ref.subtune === target);
          // No subtune override: each entry already carries its own sub-tune.
          this.playContext(folderContext, index < 0 ? 0 : index);
          seekToTime();
        });
      } else {
        // Navigate to song's containing folder. History comes from withRouter().
        const dirname = pathe.dirname(playPath);
        this.fetchDirectory(dirname).then(() => {
          this.props.history.replace(`${pathJoin('/browse', dirname)}${search}`);
          const context = this.playContexts[dirname] || [];
          // A directory context carries every sub-tune now (listingToContext
          // expands song folders), so match the whole SongRef; fall back to the
          // file's first entry when the link names a sub-tune it does not have.
          const target = subtune ?? 0;
          let index = context.findIndex(ref => ref.path === playPath && ref.subtune === target);
          if (index < 0) index = context.findIndex(ref => ref.path === playPath);

          this.playContext(context, index < 0 ? 0 : index, subtune);

          seekToTime();
        });
      }
    }

    this.setState({ loading: false });
  }

  componentWillUnmount() {
    clearInterval(this.mediaSessionTimer);
  }

  componentDidUpdate(prevProps) {
    const prevSilence = prevProps.userContext?.settings?.silenceDuration;
    const currSilence = this.props.userContext?.settings?.silenceDuration;
    if (prevSilence !== currSilence && this.players) {
      this.players.forEach(p => p.setSilenceDuration(currSilence ?? -1));
    }

    if (prevProps.location?.pathname !== this.props.location?.pathname && this.state.theaterMode) {
      const { updateSettings } = this.props.userContext;
      updateSettings({ showVisualizer: false });
    }
  }

  static mapSequencerStateToAppState(sequencerState) {
    const map = {
      ejected: 'isEjected',
      paused: 'isPaused',
      currentSongMetadata: 'metadata',
      currentSongPositionMs: 'positionMs',
      currentSongDurationMs: 'durationMs',
      currentSongBuffer: 'songBuffer',
      tempo: 'tempo',
      voiceNames: 'voiceNames',
      voiceMask: 'voiceMask',
      voiceGroups: 'voiceGroups',
      songPath: 'songPath',
      songRef: 'songRef',
      hasPlayer: 'hasPlayer',
      // TODO: Move to a separate paramStateUpdate?
      paramDefs: 'paramDefs',
      paramValues: 'paramValues',
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
      // Chrome seems to require volume > 0 to enable the session.
      this.mediaSessionAudio = document.createElement('audio');
      this.mediaSessionAudio.src = BASE_URL + '/silence-5sec.mp3';
      this.mediaSessionAudio.loop = true;
      this.mediaSessionAudio.volume = 0.01;

      navigator.mediaSession.setActionHandler('play', () => this.togglePause());
      navigator.mediaSession.setActionHandler('pause', () => this.togglePause());
      navigator.mediaSession.setActionHandler('previoustrack', () => this.prevSong());
      navigator.mediaSession.setActionHandler('nexttrack', () => this.nextSong());
      navigator.mediaSession.setActionHandler('seekbackward', () => this.seekRelative(-5000));
      navigator.mediaSession.setActionHandler('seekforward', () => this.seekRelative(5000));

      // The platform extrapolates the last position state with playbackRate, so
      // a position that folds inside a loop region would drift out of step
      // between updates; refresh it while playing.
      this.mediaSessionTimer = setInterval(() => {
        const player = this.sequencer && this.sequencer.getPlayer();
        if (player && player.isPlaying && player.isPlaying()) {
          this.updateMediaSessionPositionState();
        }
      }, 1000);
    }

    document.addEventListener('keydown', (e) => {
      // Keyboard shortcuts: tricky to get it just right and keep the browser behavior intact.
      // The order of switch-cases matters. More privileged keys appear at the top.
      // More restricted keys appear at the bottom, after various input focus states are filtered out.
      if (e.ctrlKey || e.metaKey) return; // avoid browser keyboard shortcuts

      switch (e.key) {
        case 'Escape':
          if (this.state.theaterMode) {
            this.setState({ theaterMode: false });
            e.preventDefault();
          }
          if (this.state.metadataText) {
            this.setState({ metadataText: null });
          }
          this.setState({ showInfo: false });
          e.target.blur();
          break;
        default:
      }

      if (e.target.tagName === 'INPUT' && e.target.type === 'text') return; // text input has focus

      if (e.shiftKey && (e.key === 'M' || e.key === 'm')) {
        this.setState(s => ({ metadataText: s.metadataText ? null : getDomMetadataText() }));
        e.preventDefault();
        return;
      }

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

  // Keep the OS media session in step with the app's transport: the position is
  // the player's "playlist" position (folded inside a loop region, raw
  // otherwise) and the length is the track's single-pass duration.
  updateMediaSessionPositionState() {
    if (!('mediaSession' in navigator)) return;
    const player = this.sequencer.getPlayer();
    if (!player) return;

    // No defined end -- a blind loop, or a length not known yet. The Media
    // Session spec spells that as Infinity; 0 would assert a zero-length track.
    // https://w3c.github.io/mediasession/#dom-mediasession-setpositionstate
    const durationMs = player.isBlindLoop() ? Infinity : this.state.currentSongDurationMs;
    const duration = durationMs > 0 ? durationMs / 1000 : Infinity;
    const position = Math.max(0, (player.getDisplayPositionMs() || 0) / 1000);
    navigator.mediaSession.setPositionState({
      duration,
      position: Math.min(position, duration),
      playbackRate: player.getTempo() || 1,
    });
  }

  playContext(context, index = 0, subtune = null) {
    this.sequencer.playContext(context, index, subtune);
  }

  prevSong() {
    this.sequencer.prevSong();
  }

  nextSong() {
    this.sequencer.nextSong();
  }

  handleSequencerStateUpdate(sequencerState) {
    const { isEjected } = sequencerState;
    console.debug('App.handleSequencerStateUpdate(isEjected=%s)', isEjected);

    if (isEjected) {
      this.setState({
        ejected: true,
        currentSongMetadata: {},
        currentSongPositionMs: 0,
        currentSongDurationMs: 1,
        currentSongBuffer: null,
        imageUrl: null,
        songPath: null,
        songRef: null,
        songTitleKey: null,
        subtuneCount: null,
        subtuneTitle: null,
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
      const songPath = this.sequencer.getCurrSongPath();
      const currSongRef = this.sequencer.getCurrSongRef();
      const songTitleKey = songRefKey(currSongRef);
      // TODO: this is messy. imageUrl comes asynchronously from the /metadata request.
      //       Title, artist, etc. come synchronously from player.getMetadata().
      //       ...but these are also emitted with playerStateUpdate.
      //       It would be better to incorporate imageUrl into playerStateUpdate.
      if (!songPath) {
        this.setState({ imageUrl: null });
      } else if (songTitleKey !== this.state.songTitleKey) {
        // Mark this song as loading so a burst of player state updates doesn't
        // fire duplicate metadata requests.
        this.setState({ songTitleKey, subtuneCount: null, subtuneTitle: null });
        const metadataUrl = getMetadataUrlForFilepath(songPath, currSongRef?.subtune);
        // XXX: fix this later
        // if (url.indexOf("%2") > -1 || url.indexOf("#") > -1) {
        //   console.warn("handleSequencerStateUpdate() url:", url);
        //   console.warn("handleSequencerStateUpdate() metadataUrl:", metadataUrl);
        // }
        // TODO: Disabled to support scroll restoration.
        // const filepath = url.replace(CATALOG_PREFIX, '');
        // updateQueryString({ play: filepath, t: undefined });
        // TODO: move fetch metadata to Player when it becomes event emitter
        axios.get(metadataUrl).then(response => {
          if (songTitleKey !== this.state.songTitleKey) return; // song changed while loading
          const { imageUrl: imagePath, infoTexts, md5, songId, subtuneTitle, subtuneCount } = response.data;
          const imageUrl = imagePath ? getUrlFromFilepath(imagePath) : null;
          const newInfoTexts = [...this.state.infoTexts, ...infoTexts ];
          const newShowInfo = this.state.showInfo && newInfoTexts.length > 0;
          this.setState({
            imageUrl, infoTexts: newInfoTexts, md5, showInfo: newShowInfo, songId,
            subtuneCount: subtuneCount ?? null,
            subtuneTitle: subtuneTitle ?? null,
          });

          // Playback logging
          clearTimeout(this.playbackTimer);
          this.playbackTimer = setTimeout(() => {
            // If still playing this song after 5 seconds, log a playback.
            if (this.state.songId === songId && this.state.songTitleKey === songTitleKey) {
              const subtune = currSongRef?.subtune ?? 0;
              postWithOptionalAuth(this.props.userContext.user, `${API_BASE}/playback`, { songId, subtune, durationMs: 5000 });
            }
          }, 5000);

          if ('mediaSession' in navigator) {
            // Clear artwork if imageUrl is null.
            navigator.mediaSession.metadata.artwork = (imageUrl == null) ? [] : [{
              src: imageUrl,
              sizes: '512x512',
            }];
            this.updateMediaSessionPositionState();
          }
        }).catch(e => {
          if (songTitleKey !== this.state.songTitleKey) return; // song changed while loading
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

  handleAuditionChord(pitches, isNoteOn) {
    if (this.audioCtx && this.audioCtx.state === 'suspended') {
      this.audioCtx.resume();
    }
    if (!this.midiPlayer) return;
    if (isNoteOn) {
      this.midiPlayer.auditionNoteOn(pitches);
    } else {
      this.midiPlayer.auditionNoteOff(pitches);
    }
  }

  handleTimeSliderChange(event) {
    if (!this.sequencer.getPlayer()) return;

    const pos = event.target ? event.target.value : event;
    const seekMs = Math.floor(pos * this.state.currentSongDurationMs);

    this.seekRelativeInner(seekMs);

    if (REPLACE_STATE_ON_SEEK) {
      const urlParams = new URLSearchParams(window.location.search);
      urlParams.set('t', seekMs);
      const stateUrl = '?' + urlParams.toString()
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
        this.updateMediaSessionPositionState();
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

    const value = parseFloat((event.target ? event.target.value : event)) || 1.0;
    this.sequencer.getPlayer().setTempo(value);
    this.setState({
      tempo: value
    });
    this.updateMediaSessionPositionState();

    const { settings, updateSettings } = this.props.userContext;
    const persistedKey = 'tempo';
    if (settings[persistedKey] != null) {
      updateSettings({ [persistedKey]: value });
    }
  }

  handleParamChange(id, value) {
    if (!this.sequencer.getPlayer()) return;
    const player = this.sequencer.getPlayer();
    player.setParameter(id, value);
    this.setState(prevState => ({
      paramValues: { ...prevState.paramValues, [id]: value },
    }));

    const { settings, updateSettings } = this.props.userContext;
    const persistedKey = `${player.playerKey}.${id}`;
    if (settings[persistedKey] != null) {
      updateSettings({ [persistedKey]: value });
    }
  }

  handlePinParam(persistedKey, currentValue) {
    const { settings, replaceSettings } = this.props.userContext;
    const newSettings = { ...settings };

    if (newSettings[persistedKey] != null) {
      delete newSettings[persistedKey];
    } else {
      newSettings[persistedKey] = currentValue;
    }

    replaceSettings(newSettings);
  }

  setSpeedRelative(delta) {
    if (!this.sequencer.getPlayer()) return;

    const tempo = clamp(this.state.tempo + delta, 0.1, 2);
    this.sequencer.getPlayer().setTempo(tempo);
    this.setState({
      tempo: tempo
    });
    this.updateMediaSessionPositionState();
  }

  handleShufflePlay(path, customContext) {
    if (path === 'favorites') {
      this.sequencer.playContext(shuffle(this.props.userContext.favesContext));
    } else if (path === 'local') {
      this.sequencer.playContext(shuffle(this.playContexts['local']));
    } else if (path === 'top') {
      this.sequencer.playContext(shuffle(customContext || this.playContexts['top'] || []));
    } else {
      // This is more like a synthetic recursive shuffle.
      // Response of this API is an array of { path, subtune } song refs.
      fetch(`${API_BASE}/shuffle?path=${encodeURI(path)}&limit=100`)
        .then(response => response.json())
        .then(json => json.items)
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
        this.sequencer.playSonglist([songRef(url)]);
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
    // One SongRef per playable song: a file is its own sub-tune, a songfolder is
    // expanded into all of them (see listingToContext), and directories are
    // dropped. The sub-tunes of one file are also their own rows when browsing
    // inside a song folder.
    return listingToContext(items);
  }

  pathToHref(path) {
    return pathJoin(CATALOG_PREFIX, path.replace(/%/g, '%25').replace(/#/g, '%23'));
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
          // Sub-tune rows carry a label from the server; don't clobber it.
          item.name = item.name || item.path.split('/').pop();
          // XXX: Escape immediately: the escaped URL is considered canonical.
          //      The URL must be decoded for display from here on out.
          // TODO: Replace `href` entirely with `url` field
          const href = item.path.replace(/%/g, '%25').replace(/#/g, '%23');
          if (item.type === 'file')
            item.href = pathJoin(CATALOG_PREFIX, href);
          else // item.type === 'directory' or 'songfolder'
            item.href = pathJoin('/browse', href);
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
    if (!this.state.songId) return null;

    let link = BASE_URL + '/?play=' + this.state.songId;
    if (withSubtune) {
      const subtune = this.sequencer?.getCurrSongRef()?.subtune ?? 0;
      if (subtune !== 0) {
        link += '&subtune=' + subtune;
      }
    }
    return link;
  }

  async updateLocalFiles() {
    const generation = ++this.localFilesGeneration;
    // The panel shows a loading state until applyLocalFiles() applies the listing
    // below. It has to be cleared there, in the same setState: the parse is async,
    // and clearing it a step earlier -- as the syncfs callback used to -- renders
    // once with no files and no loading state, i.e. the empty-library message, to
    // users who do have local files.
    this.setState({ loadingLocalFiles: true });
    // A read that throws leaves nothing to list, so the panel falls through to
    // the empty-library message rather than staying on the loading state.
    let localFiles = [];
    try {
      localFiles = this.localFilesManager.readAll();
    } catch (e) {
      console.error('Could not list local files:', e);
    }
    // Sub-tune counts come from the file headers, parsed by a lazily-loaded,
    // cached module (see ../localFilesSubtunes). On any failure we fall back to
    // one row per file, i.e. the old behavior.
    let byPath = null;
    try {
      byPath = await describeLocalSubtunes(localFiles, p => this.localFilesManager.read(p));
    } catch (e) {
      console.warn('Local file sub-tunes unavailable:', e && e.message);
    }
    if (generation !== this.localFilesGeneration) return false; // a newer update won
    // The sub-tune cache keys on (path, size, mtime), so it must see the real
    // timestamp; format it for display only here, after the parse. Truncating
    // to a date first made same-day replacements reuse stale sub-tunes.
    // Convert timestamp 1704067200 to ISO date 2024-01-01
    localFiles.forEach(item => item.mtime = new Date(item.mtime * 1000).toISOString().split('T')[0]);
    this.applyLocalFiles(localFiles, byPath);
    return true;
  }

  // Expand each local file into one playable row per sub-tune (single-song files
  // stay one row), so a local multi-song file plays, shuffles and advances like
  // a catalog song folder. Labels come from the file's own header.
  applyLocalFiles(files, byPath) {
    const prevRefs = this.playContexts['local'] || [];
    const isCurrent = this.sequencer && songRefListsEqual(this.sequencer.getCurrContext(), prevRefs);
    // Shuffle-aware: with Shuffle on, currIdx indexes shuffleOrder, not this
    // list, so take the player's own current SongRef rather than prevRefs[currIdx].
    const currRef = isCurrent ? this.sequencer.getCurrSongRef() : null;

    const listing = [];
    const refs = [];
    for (const item of files) {
      // Local paths are stored verbatim (not URL-encoded), so take the basename
      // as-is: decodeURIComponent here would throw on a name with a bare "%".
      const base = item.path.split('/').pop();
      const subtunes = (byPath && byPath.get(item.path)) || [];
      if (subtunes.length > 1) {
        const children = subtunes.map(st => {
          const ctxIdx = refs.length;
          refs.push(songRef(item.path, st.subtune));
          return { subtune: st.subtune, name: subtuneLabel(st.subtune, st.title), ctxIdx };
        });
        listing.push({ ...item, name: base, type: 'folder', children });
      } else {
        const ctxIdx = refs.length;
        refs.push(songRef(item.path, 0));
        listing.push({ ...item, name: base, type: 'file', subtune: 0, ctxIdx });
      }
    }
    this.playContexts['local'] = refs;
    // The listing and the loading state land together, so there is no render in
    // between where the panel is neither loading nor populated.
    this.setState({ localFiles: listing, loadingLocalFiles: false });

    // Keep the transport on the same song across the index shift the expansion
    // causes (sub-tune 0 of a file is still sub-tune 0 after expanding).
    if (isCurrent) {
      this.sequencer.context = refs;
      let idx = -1;
      if (currRef) idx = refs.findIndex(r => songRefKey(r) === songRefKey(currRef));
      if (currRef && idx < 0) {
        // The playing song is gone from the rebuilt list (its file was deleted).
        // Move to the song that followed it -- by identity, not by the old index:
        // a multi-song file is several rows, so removing them shifts every later
        // row left by the number removed, and clamping the old index landed past
        // the follower. If nothing after it survives (the follower's file went
        // too -- "Clear" removes several at once) step back to the nearest row
        // before it, or to the top if none of the old rows matched at all; an
        // empty context ejects below. This lives in the rebuild, not in
        // handleLocalFileDelete, so that whichever update wins handles it; a
        // superseded caller cannot strand the deleted song playing.
        if (refs.length > 0) {
          const at = new Map(refs.map((r, i) => [songRefKey(r), i]));
          const oldPos = prevRefs.findIndex(r => songRefKey(r) === songRefKey(currRef));
          let next = -1;
          for (let i = oldPos + 1; i < prevRefs.length && next < 0; i++) next = at.get(songRefKey(prevRefs[i])) ?? -1;
          for (let i = oldPos - 1; i >= 0 && next < 0; i--) next = at.get(songRefKey(prevRefs[i])) ?? -1;
          this.sequencer.currIdx = next >= 0 ? next : 0;
          if (this.sequencer.shuffle) this.sequencer.setShuffle(this.sequencer.shuffle);
          this.sequencer.playCurrentSong();
        } else {
          // Empty context: advanceSong stops the player and ejects, rather than
          // leaving the deleted buffer playing -- forever, under Repeat One.
          this.sequencer.nextSong();
        }
        return;
      }
      // If the current song survived, re-anchor on it; otherwise clamp. With
      // Shuffle on, currIdx indexes shuffleOrder, so rebuilding it for the new
      // list is required or advancing can pick a stale/out-of-range entry.
      this.sequencer.currIdx = idx >= 0
        ? idx
        : Math.max(0, Math.min(this.sequencer.currIdx, refs.length - 1));
      if (this.sequencer.shuffle) this.sequencer.setShuffle(this.sequencer.shuffle);
    }
  }

  handleFiles = (files) => {
    const promises = files.map(file => {
      return new Promise((resolve, reject) => {
        // TODO: refactor, avoid creating new reader/handlers for every dropped file.
        const reader = new FileReader();
        reader.onerror = (event) => reject(event.target.error);
        reader.onload = async () => {
          const ext = pathe.extname(file.name).toLowerCase().substring(1);
          if (ext === 'sf2') {
            // Handle dropped Soundfont
            if (!this.midiPlayer) {
              reject('MIDIPlayer has not been created - unable to load SoundFont.');
            } else if (files.length !== 1) {
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
            this.localFilesManager.write(pathe.join('local', file.name), songData);
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
        // updateLocalFiles() rebuilds the context (and keeps the current song
        // when it survives) once its async header parse finishes.
        this.updateLocalFiles();
        this.props.history.push('/local');
        this.forceUpdate();
      }
      // Display all rejection reasons with duplicate reasons removed.
      results.filter(result => result.status === 'rejected')
        .reduce((acc, result) => acc.includes(result.reason) ? acc : [ ...acc, result.reason ], [])
        .forEach((reason, i) => setTimeout(() => this.props.toastContext.enqueueToast(reason, ToastLevels.ERROR), i * 1500));
    });
  }

  onDrop = (droppedFiles) => {
    this.handleFiles(droppedFiles);
  };

  handleLocalFileDelete = async (filePaths) => {
    if (!Array.isArray(filePaths)) filePaths = [filePaths];
    // One file can span several sub-tune rows; delete each file once.
    filePaths = [...new Set(filePaths)];
    filePaths.forEach(filePath => this.localFilesManager.delete(filePath));
    // applyLocalFiles() rebuilds the context and, if the playing song was among
    // the deleted files, moves playback to the surviving song or ejects -- see
    // applyLocalFiles. Owning the removal in the rebuild, rather than in a
    // continuation here, is what makes a superseding update safe: whoever
    // rebuilds last handles it, and a cancelled continuation cannot leave the
    // deleted song playing.
    await this.updateLocalFiles();
  }

  handleCopyLink = (url) => {
    if (!url) return;
    const done = () => this.props.toastContext.enqueueToast('Copied song link to clipboard.', ToastLevels.INFO);
    const failed = () => this.props.toastContext.enqueueToast('Could not copy song link to clipboard.', ToastLevels.ERROR);
    // navigator.clipboard requires a secure context; fall back to execCommand
    // (e.g. remote dev over plain http) so the button still works there.
    if (navigator.clipboard?.writeText) {
      navigator.clipboard.writeText(url).then(done, failed);
    } else {
      try {
        const ta = document.createElement('textarea');
        ta.value = url;
        ta.style.position = 'fixed';
        ta.style.opacity = '0';
        document.body.appendChild(ta);
        ta.select();
        const ok = document.execCommand('copy');
        ta.remove();
        if (ok) done();
        else failed();
      } catch (e) {
        failed();
      }
    }
  }

  handleToggleSettings = (e) => {
    const { settings, updateSettings } = this.props.userContext;
    const showPlayerSettings = settings?.showPlayerSettings;
    updateSettings({ showPlayerSettings: !showPlayerSettings });
  }

  handleToggleTheaterMode = () => {
    this.setState(prevState => ({
      theaterMode: !prevState.theaterMode,
    }));
  }

  render() {
    // TODO: Consolidate imageUrl under metadata.
    const metadata = this.state.currentSongMetadata;
    const { title: metadataTitle, subtitle } = titlesFromMetadata(metadata);
    // A sub-tune's label from the catalog takes precedence over the player's
    // generic track title.
    const title = this.state.subtuneTitle || metadataTitle;
    const imageUrl = metadata.imageUrl || this.state.imageUrl;
    const currContext = this.sequencer?.getCurrContext();
    const currIdx = this.sequencer?.getCurrIdx();
    const search = { search: window.location.search };
    const { settings } = this.props.userContext;
    const showPlayerSettings = settings?.showPlayerSettings;
    const showVisualizer = settings?.showVisualizer;
    // The buffer comes from the sequencer, not from App state: it is the live
    // song's buffer, and the piano roll's loop gate is derived from the same live
    // source, so the two cannot disagree. App state lags a render behind, and a
    // stale buffer paired with the new song's gate paints the wrong file.
    const midiData = (
      (this.state.songPath && /\.(mid|midi|smf)$/i.test(this.state.songPath)) ||
      this.sequencer?.getPlayer()?.fileExtensions?.includes('mid')
    ) ? (this.sequencer?.getCurrSongBuffer() || this.state.currentSongBuffer) : null;
    const isMidi = Boolean(midiData);
    const activeTheaterMode = Boolean(isMidi && this.state.theaterMode && showVisualizer);
    // A multi-song file browses as a song folder, so the footer links into it.
    const songFolder = isSongFolder(this.state);

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
          <MessageBox showInfo={Boolean(this.state.metadataText)}
                      infoTexts={[this.state.metadataText || '']}
                      toggleInfo={() => this.setState({ metadataText: null })}/>
          <Toast/>
          <AppHeader/>
          <div className={`App-main ${activeTheaterMode ? 'theater-mode' : ''} ${showPlayerSettings ? 'has-settings' : ''}`}>
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
                <NavLink className="tab" activeClassName="tab-selected"
                         to={{ pathname: "/top", ...search }}>Top</NavLink>
                {/* this.sequencer?.players?.map((p, i) => `p${i}:${p.stopped?'off':'on'}`).join(' ') */}
                <button className={`tab tab-settings ${showPlayerSettings ? 'tab-selected' : ''}`}
                        onClick={this.handleToggleSettings}>Settings</button>
              </div>
              <div className="App-main-content-and-settings">
              <div className="App-main-content-area"
                   ref={this.contentAreaRef}>
                <Switch>
                  <Route path="/top" render={() => (
                    <TopCharts
                      scrollContainerRef={this.contentAreaRef}
                      handleShufflePlay={this.handleShufflePlay}
                      onSongClick={this.handleSongClick}
                      currContext={currContext}
                      currIdx={currIdx}/>
                  )}/>
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
                    const browsePath = match.params?.browsePath?.replace(/%25/g, '%') || '';
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
                      onAddFiles={this.handleFiles}
                      handleShufflePlay={this.handleShufflePlay}
                      onSongClick={this.handleSongClick}
                      onDelete={this.handleLocalFileDelete}
                      playContext={this.playContexts['local']}
                      currContext={currContext}
                      currIdx={currIdx}
                      listing={this.state.localFiles}/>
                  )}/>
                  {/* Catch-all route */}
                  <Route render={({history, location}) => (
                    <Search
                      history={history}
                      location={location}
                      currContext={currContext}
                      currIdx={currIdx}
                      onSongClick={this.handleSongClick}
                      scrollContainerRef={this.contentAreaRef}
                      listRef={this.listRef}
                    >
                      {/*{this.state.loading && <p>Loading player engine...</p>}*/}
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
                    voiceMask={this.state.voiceMask}
                    voiceNames={this.state.voiceNames}
                    voiceGroups={this.state.voiceGroups}
                    onVoiceMaskChange={this.handleSetVoiceMask}
                    onTempoChange={this.handleTempoChange}
                    paramDefs={this.state.paramDefs}
                    paramValues={this.state.paramValues}
                    onParamChange={this.handleParamChange}
                    onPinParam={this.handlePinParam}
                    persistedSettings={settings}
                    sequencer={this.sequencer}
                  />
                </div>
                }
              </div>
            </div>
            {!isMobile.phone && !this.state.loading && !!this.chipCore &&
              <Visualizer audioCtx={this.audioCtx}
                          sourceNode={this.playerNode}
                          chipCore={this.chipCore}
                          paused={this.state.ejected || this.state.paused}
                          songPath={this.state.songPath}
                          midiData={midiData}
                          getCurrentPositionMs={() => this.sequencer?.getPlayer()?.getPositionMs() || 0}
                          getPlaybackRate={() => this.sequencer?.getPlayer()?.getTempo?.() || 1.0}
                          getIsPlaying={() => this.sequencer?.getPlayer()?.isPlaying() ?? false}
                          voiceMask={this.state.voiceMask}
                          sequencer={this.sequencer}
                          visible={Boolean(showVisualizer)}
                          theaterMode={activeTheaterMode}
                          onToggleTheaterMode={this.handleToggleTheaterMode}
                          onAuditionChord={this.handleAuditionChord}/>}
          </div>
          <AppFooter
            currentSongDurationMs={this.state.currentSongDurationMs}
            currentSongMetadata={this.state.currentSongMetadata}
            ejected={this.state.ejected}
            getCurrentSongLink={this.getCurrentSongLink}
            handleCopyLink={this.handleCopyLink}
            handleCycleRepeat={this.handleCycleRepeat}
            handleCycleShuffle={this.handleCycleShuffle}
            handleSetVoiceMask={this.handleSetVoiceMask}
            handleTempoChange={this.handleTempoChange}
            handleTimeSliderChange={this.handleTimeSliderChange}
            handleVolumeChange={this.handleVolumeChange}
            imageUrl={imageUrl}
            infoTexts={this.state.infoTexts}
            isSongFolder={songFolder}
            md5={this.state.md5}
            nextSong={this.nextSong}
            paused={this.state.paused}
            prevSong={this.prevSong}
            repeat={this.state.repeat}
            shuffle={this.state.shuffle}
            sequencer={this.sequencer}
            songId={this.state.songId}
            songRef={this.state.songRef}
            songPath={this.state.songPath}
            subtitle={subtitle}
            // A sub-tune with no label of its own falls back to the file title,
            // so every tune in a song folder looks alike while it plays. Name it
            // the way Browse lists it. Labeled sub-tunes already say which tune
            // they are, so they get nothing extra here.
            subtuneLabel={songFolder && !this.state.subtuneTitle
              ? subtuneLabel(this.state.songRef?.subtune)
              : null}
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
