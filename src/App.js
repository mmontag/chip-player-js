import React from 'react';
import isMobile from 'ismobilejs';
import clamp from 'lodash/clamp';
import queryString from 'querystring';
import * as firebase from 'firebase/app';
import 'firebase/auth';
import 'firebase/firestore';
import { BrowserRouter as Router, NavLink, Route, Switch } from 'react-router-dom';
import Dropzone from 'react-dropzone';

import ChipCore from './chip-core';
import firebaseConfig from './config/firebaseConfig';
import promisify from './promisifyXhr';
import { API_BASE, CATALOG_PREFIX, MAX_VOICES, REPLACE_STATE_ON_SEEK } from './config';
import { replaceRomanWithArabic, titlesFromMetadata, unlockAudioContext, updateQueryString } from './util';

import GMEPlayer from './players/GMEPlayer';
import MIDIPlayer from './players/MIDIPlayer';
import V2MPlayer from './players/V2MPlayer';
import XMPPlayer from './players/XMPPlayer';

import AppFooter from './AppFooter';
import AppHeader from './AppHeader';
import Browse from './Browse';
import DropMessage from './DropMessage';
import Favorites from './Favorites';
import Search from './Search';
import Sequencer from './Sequencer';
import Visualizer from './Visualizer';

const NUMERIC_COLLATOR = new Intl.Collator(undefined, { numeric: true, sensitivity: 'base' });

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
    this.handleVolumeChange = this.handleVolumeChange.bind(this);
    this.handleTimeSliderChange = this.handleTimeSliderChange.bind(this);
    this.handleTempoChange = this.handleTempoChange.bind(this);
    this.handleSetVoices = this.handleSetVoices.bind(this);
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
    const audioCtx = this.audioCtx = new (window.AudioContext || window.webkitAudioContext)();
    const gainNode = audioCtx.createGain();
    gainNode.gain.value = 1;
    gainNode.connect(audioCtx.destination);
    const playerNode = this.playerNode = gainNode;

    unlockAudioContext(audioCtx);
    console.log('Sample rate: %d hz', audioCtx.sampleRate);

    // Initialize sequencer with empty players array
    this.sequencer = new Sequencer([], this.handleSequencerStateUpdate, this.handlePlayerError);

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
      voices: Array(MAX_VOICES).fill(true),
      voiceNames: Array(MAX_VOICES).fill(''),
      imageUrl: null,
      infoTexts: [],
      showInfo: false,
      showPlayerSettings: false,
      user: null,
      faves: [],
      songUrl: null,
      volume: 100,

      directories: {},
    };

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
          this.sequencer.setPlayers([
            new GMEPlayer(audioCtx, playerNode, chipCore),
            new XMPPlayer(audioCtx, playerNode, chipCore),
            new MIDIPlayer(audioCtx, playerNode, chipCore),
            new V2MPlayer(audioCtx, playerNode, chipCore),
          ]);
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

          const urlParams = queryString.parse(window.location.search.substr(1));
          if (urlParams.play) {
            // Allow a little time for initial page render before starting the song.
            // This is not absolutely necessary but helps prevent stuttering.
            setTimeout(() => {
              this.sequencer.playSonglist([urlParams.play]);
              if (urlParams.t) {
                setTimeout(() => {
                  if (this.sequencer.getPlayer()) {
                    this.sequencer.getPlayer().seekMs(parseInt(urlParams.t, 10));
                  }
                }, 100);
              }
            }, 500);
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
      if (e.key !== 'Escape' && (e.target.tagName === 'INPUT' || e.target.tagName === 'SELECT')) return;

      switch (e.key) {
        case 'Escape':
          this.setState({ showInfo: false });
          e.target.blur();
          break;
        case 'ArrowLeft':
          this.seekRelative(-5000);
          break;
        case 'ArrowRight':
          this.seekRelative(5000);
          break;
        case ' ':
          if (e.target.tagName !== 'BUTTON') {
            this.togglePause();
            e.preventDefault();
          }
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

  handleSequencerStateUpdate(isEjected) {
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
      updateQueryString({ play: undefined });

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
      if (url && url !== this.state.songUrl) {
        const path = url.replace(CATALOG_PREFIX, '/');
        const getMetadataUrl = `${API_BASE}/metadata?path=${encodeURIComponent(path)}`;
        const pathParts = url.split('/');
        pathParts.pop();

        const filepath = url.replace(CATALOG_PREFIX, '');
        updateQueryString({ play: filepath, t: undefined });

        // Fetch artwork/info for this file (cancelable request)
        if (this.metaRequest) this.metaRequest.abort();
        this.metaRequest = promisify(new XMLHttpRequest());
        this.metaRequest.responseType = 'json';
        this.metaRequest.open('GET', getMetadataUrl);
        this.metaRequest.send()
          .then(xhr => {
            if (xhr.status >= 200 && xhr.status < 400) {
              const { imageUrl, infoTexts } = xhr.response;
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
            }
          })
          .catch(e => {
            this.setState({ imageUrl: null, infoTexts: [], showInfo: false });
          });
      } else {
        // Drag & dropped files reach this branch
        this.setState({ imageUrl: null, infoTexts: [], showInfo: false });
      }

      const metadata = player.getMetadata();

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

      // Wrap in blob player state?
      this.setState({
        ejected: false,
        paused: player.isPaused(),
        currentSongSubtune: player.getSubtune(),
        currentSongMetadata: player.getMetadata(),
        currentSongNumVoices: player.getNumVoices(),
        currentSongPositionMs: player.getPositionMs(),
        currentSongDurationMs: player.getDurationMs(),
        currentSongNumSubtunes: player.getNumSubtunes(),
        tempo: 1,
        voiceNames: [...Array(player.getNumVoices())].map((_, i) => player.getVoiceName(i)),
        voices: [...Array(player.getNumVoices())].fill(true),
        songUrl: url,
      });
    }
  }

  handlePlayerError(error) {
    this.setState({ playerError: error });
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

  handleSetVoices(voices) {
    if (!this.sequencer.getPlayer()) return;

    this.sequencer.getPlayer().setVoices(voices);
    this.setState({ voices: [...voices] });
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
    this.playerNode.gain.value = Math.max(0, Math.min(1, volume * 0.01));
  }

  toggleInfo() {
    this.setState({
      showInfo: !this.state.showInfo,
    });
  }

  fetchDirectory(path) {
    fetch(`${API_BASE}/browse?path=%2F${encodeURIComponent(path)}`)
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
        const directories = {
          ...this.state.directories,
          [path]: items,
        };
        this.setState({ directories });
      });
  }

  getCurrentSongLink() {
    const url = this.sequencer.getCurrUrl();
    return url ? '/?play=' + encodeURIComponent(url.replace(CATALOG_PREFIX, '')) : '#';
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
    const currContext = this.sequencer.getCurrContext();
    const currIdx = this.sequencer.getCurrIdx();
    const search = { search: window.location.search };
    return (
      <Router basename={process.env.PUBLIC_URL}>
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
                    <Route path="/browse/:browsePath*" render={({ match }) => {
                      // Undo the react-router-dom double-encoded % workaround - see DirectoryLink.js
                      if (match.params.browsePath)
                        match.params.browsePath = match.params.browsePath.replace('%25', '%');
                      return (
                        this.contentAreaRef.current &&
                        <Browse currContext={currContext}
                                currIdx={currIdx}
                                browsePath={match.params.browsePath || ''}
                                directories={this.state.directories}
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
              handlePlayRandom={this.handlePlayRandom}
              handleSetVoices={this.handleSetVoices}
              handleTempoChange={this.handleTempoChange}
              handleTimeSliderChange={this.handleTimeSliderChange}
              handleToggleFavorite={this.handleToggleFavorite}
              handleVolumeChange={this.handleVolumeChange}
              imageUrl={this.state.imageUrl}
              infoTexts={this.state.infoTexts}
              nextSong={this.nextSong}
              nextSubtune={this.nextSubtune}
              paused={this.state.paused}
              playerError={this.state.playerError}
              prevSong={this.prevSong}
              prevSubtune={this.prevSubtune}
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
              voices={this.state.voices}
              volume={this.state.volume}
            />
          </div>
        )}</Dropzone>
      </Router>
    );
  }
}

export default App;
