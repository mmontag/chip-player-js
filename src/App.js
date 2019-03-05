import React, {PureComponent} from 'react';
import isMobile from 'ismobilejs';
import queryString from 'querystring';
import * as firebase from 'firebase/app';
import 'firebase/auth';
import 'firebase/firestore';
import firebaseConfig from './config/firebaseConfig';

import ChipCore from './chip-core';
import GMEPlayer from './players/GMEPlayer';
import MIDIPlayer from './players/MIDIPlayer';
import XMPPlayer from './players/XMPPlayer';
import {trieToList} from './ResigTrie';

import PlayerParams from './PlayerParams';
import Search from './Search';
import TimeSlider from "./TimeSlider";
import Visualizer from './Visualizer';
import FavoriteButton from "./FavoriteButton";
import AppHeader from "./AppHeader";
import Favorites from "./Favorites";
import Sequencer from "./Sequencer";

const MAX_VOICES = 64;

class App extends PureComponent {
  handleLogin() {
    const provider = new firebase.auth.FacebookAuthProvider();
    firebase.auth().signInWithPopup(provider).then(result => {
      console.log('firebase auth result:', result);
    }).catch(error => {
      console.log('firebase auth error:', error);
    });
  }

  handleLogout() {
    firebase.auth().signOut().then(() => {
      this.setState({
        user: null,
        favorites: null,
      });
    });
  }

  handleToggleFavorite(path) {
    const user = this.state.user;
    if (user) {
      const favorites = Object.assign({}, this.state.favorites);
      const id = favorites[path];
      const favoritesRef = this.db.collection('users').doc(user.uid).collection('favorites');
      if (id && id !== 'pending') {
        delete favorites[path];
        this.setState({favorites: favorites});
        favoritesRef.doc(id).delete().then(() => {
          console.log('Deleted favorite %s.', path);
        });
      } else {
        favorites[path] = 'pending';
        this.setState({favorites: favorites});
        favoritesRef.add({path: path}).then((ref) => {
          console.log('Added favorite %s.', path);
          favorites[path] = ref.id;
          this.setState({favorites: favorites});
        });
      }
    }
  }

  constructor(props) {
    super(props);

    this.togglePause = this.togglePause.bind(this);
    this.toggleSettings = this.toggleSettings.bind(this);
    this.displayLoop = this.displayLoop.bind(this);
    this.loadCatalog = this.loadCatalog.bind(this);
    this.playContext = this.playContext.bind(this);
    this.prevSong = this.prevSong.bind(this);
    this.nextSong = this.nextSong.bind(this);
    this.prevSubtune = this.prevSubtune.bind(this);
    this.nextSubtune = this.nextSubtune.bind(this);
    this.handleTimeSliderChange = this.handleTimeSliderChange.bind(this);
    this.handleTempoChange = this.handleTempoChange.bind(this);
    this.handleSetVoices = this.handleSetVoices.bind(this);
    this.handlePlayerStateUpdate = this.handlePlayerStateUpdate.bind(this);
    this.handlePlayerError = this.handlePlayerError.bind(this);
    this.handleDoSearch = this.handleDoSearch.bind(this);
    this.handlePlayRandom = this.handlePlayRandom.bind(this);
    this.handleSongClick = this.handleSongClick.bind(this);
    this.handleLogin = this.handleLogin.bind(this);
    this.handleLogout = this.handleLogout.bind(this);
    this.handleToggleFavorite = this.handleToggleFavorite.bind(this);

    // Initialize Firebase
    firebase.initializeApp(firebaseConfig);
    firebase.auth().onAuthStateChanged(user => {
      this.setState({user: user, loadingUser: !!user});
      if (user) {
        this.db.collection('users').doc(user.uid).collection('favorites').get().then(docs => {
          // invert the collection
          const favorites = {};
          docs.forEach(doc => {
            favorites[doc.data().path] = doc.id;
          });
          this.setState({favorites: favorites, loadingUser: false});
        });
      }
    });
    this.db = firebase.firestore();
    this.db.settings({timestampsInSnapshots: true});
    // Initialize audio graph
    const audioCtx = this.audioCtx = new (window.AudioContext || window.webkitAudioContext)();
    const gainNode = audioCtx.createGain();
    gainNode.gain.value = 1;
    gainNode.connect(audioCtx.destination);
    var playerNode = this.playerNode = gainNode;

    this._unlockAudioContext(audioCtx);
    console.log('Sample rate: %d hz', audioCtx.sampleRate);

    // Load the chip-core Emscripten runtime
    const chipCore = this.chipCore = new ChipCore({
      // Look for .wasm file in web root, not the same location as the app bundle (static/js).
      locateFile: (path, prefix) => {
        if (path.endsWith('.wasm') || path.endsWith('.wast')) return './' + path;
        return prefix + path;
      },
      onRuntimeInitialized: () => {
        this.sequencer.setPlayers([
          new GMEPlayer(audioCtx, playerNode, chipCore),
          new XMPPlayer(audioCtx, playerNode, chipCore),
          new MIDIPlayer(audioCtx, playerNode, chipCore),
        ]);
        this.setState({loading: false});

        const urlParams = queryString.parse(window.location.search.substr(1));
        if (urlParams.q) {
          this.setState({initialQuery: urlParams.q});
        }
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

    this.sequencer = new Sequencer([], this.handlePlayerStateUpdate, this.handlePlayerError);

    this.state = {
      catalog: null,
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
      initialQuery: null,
      imageUrl: null,
      showSettings: false,
      user: null,
      favorites: null,
      songUrl: null,
    };

    // Load the song catalog
    this.loadCatalog();

    // Start display loop
    setInterval(this.displayLoop, 46); //  46 ms = 2048/44100 sec or 21.5 fps
                                       // 400 ms = 2.5 fps
  }

  _unlockAudioContext(context) {
    // https://hackernoon.com/unlocking-web-audio-the-smarter-way-8858218c0e09
    if (context.state === 'suspended') {
      const events = ['touchstart', 'touchend', 'mousedown', 'mouseup'];
      const unlock = () => context.resume()
        .then(() => events.forEach(event => document.body.removeEventListener(event, unlock)));
      events.forEach(event => document.body.addEventListener(event, unlock, false));
    }
  }

  displayLoop() {
    if (this.sequencer && this.sequencer.getPlayer()) {
      this.setState({
        currentSongPositionMs: Math.min(this.sequencer.getPlayer().getPositionMs(), this.state.currentSongDurationMs),
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

  playContext(context, index = 0) {
    this.setState({playerError: null});
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
        songUrl: null,
      });
    } else {
      // // Update application URL (window.history API)
      // const filepath = url.replace(CATALOG_PREFIX, '');
      // const ext = url.split('.').pop().toLowerCase();
      // const pathParts = url.split('/');
      // pathParts.pop();
      // const urlParams = {
      //   ...queryString.parse(window.location.search.substr(1)),
      //   play: filepath,
      // };
      // delete urlParams.t;
      // const stateUrl = '?' + queryString.stringify(urlParams)
      //   .replace(/%20/g, '+') // I don't care about escaping these characters
      //   .replace(/%2C/g, ',')
      //   .replace(/%2F/g, '/');
      // window.history.replaceState(null, '', stateUrl);
      const player = this.sequencer.getPlayer();

      this.setState({
        ejected: false,
        paused: player.isPaused(),
        currentSongSubtune: player.getSubtune(),
        currentSongMetadata: player.getMetadata(),
        currentSongNumVoices: player.getNumVoices(),
        currentSongPositionMs: player.getPositionMs(),
        currentSongDurationMs: player.getDurationMs(),
        currentSongNumSubtunes: player.getNumSubtunes(),
        voiceNames: [...Array(player.getNumVoices())].map((_, i) => player.getVoiceName(i)),
        songUrl: this.sequencer.getCurrUrl(),
      });
    }
  }

  handlePlayerError(error) {
    this.setState({playerError: error});
  }

  togglePause() {
    if (!this.sequencer.getPlayer()) return;

    this.setState({paused: this.sequencer.getPlayer().togglePause()});
  }

  toggleSettings() {
    this.setState({showSettings: !this.state.showSettings});
  }

  handleTimeSliderChange(event) {
    if (!this.sequencer.getPlayer()) return;

    const pos = event.target ? event.target.value : event;

    const seekMs = Math.floor(pos * this.state.currentSongDurationMs);
    const urlParams = {
      ...queryString.parse(window.location.search.substr(1)),
      t: seekMs,
    };
    const stateUrl = '?' + queryString.stringify(urlParams).replace(/%20/g, '+').replace(/%2F/g, '/');
    window.history.replaceState(null, '', stateUrl);

    // Seek in song
    this.sequencer.getPlayer().seekMs(seekMs);
    this.setState({
      currentSongPositionMs: pos * this.state.currentSongDurationMs, // Smooth
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
    this.setState({voices: [...voices]});
  }

  handleTempoChange(event) {
    if (!this.sequencer.getPlayer()) return;

    const tempo = parseFloat((event.target ? event.target.value : event)) || 1.0;
    this.sequencer.getPlayer().setTempo(tempo);
    this.setState({
      tempo: tempo
    });
  }

  handlePlayRandom() {
    const catalog = this.state.catalog;
    if (catalog) {
      const idx = Math.floor(Math.random() * catalog.length);
      this.sequencer.playContext(catalog, idx);
    }
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

  handleDoSearch(e, query) {
    this.setState({initialQuery: query});
    e.preventDefault();
    return false;
  }

  static titlesFromMetadata(metadata) {
    if (metadata.formatted) {
      return metadata.formatted;
    }

    const title = App.allOrNone(metadata.artist, ' - ') + metadata.title;
    const subtitle = [metadata.game, metadata.system].filter(x => x).join(' - ') +
      App.allOrNone(' (', metadata.copyright, ')');
    return {title, subtitle};
  }

  static allOrNone(...args) {
    let str = '';
    for (let i = 0; i < args.length; i++) {
      if (!args[i]) return '';
      str += args[i];
    }
    return str;
  }

  render() {
    const {title, subtitle} = App.titlesFromMetadata(this.state.currentSongMetadata);
    const currContext = this.sequencer.getCurrContext();
    const currIdx = this.sequencer.getCurrIdx();
    return (
      <div className="App">
        <AppHeader user={this.state.user}
                   handleLogout={this.handleLogout}
                   handleLogin={this.handleLogin}
                   isPhone={isMobile.phone}/>
        {this.state.loading ?
          <p>Loading player engine...</p>
          :
          <div className="App-content-area">
            <Search
              initialQuery={this.state.initialQuery}
              catalog={this.state.catalog}
              currContext={currContext}
              currIdx={currIdx}
              toggleFavorite={this.handleToggleFavorite}
              favorites={this.state.favorites}
              onClick={this.handleSongClick}>
              {this.state.loadingUser ?
                <p>Loading user data...</p>
                :
                <Favorites
                  user={this.state.user}
                  handleLogin={this.handleLogin}
                  onClick={this.handleSongClick}
                  currContext={currContext}
                  currIdx={currIdx}
                  toggleFavorite={this.handleToggleFavorite}
                  favorites={this.state.favorites}/>}
              <h1>Top Level Folders</h1>
              {
                [
                  'Classical MIDI',
                  'Exotica',
                  'Famicompo',
                  'Game MIDI',
                  'Game Mods',
                  'MIDI',
                  'ModArchives',
                  'Nintendo',
                  'Nintendo SNES',
                  'Piano E-Competition MIDI',
                  'Project AY',
                  'Sega Game Gear',
                  'Sega Genesis',
                  'Sega Master System',
                  'Ubiktune',
                  'VGM Rips',
                ].map(t => <a key={t} href="#" onClick={(e) => this.handleDoSearch(e, t)}>{t}</a>)
              }
            </Search>
            {!isMobile.phone &&
            <Visualizer audioCtx={this.audioCtx}
                        sourceNode={this.playerNode}
                        chipCore={this.chipCore}
                        paused={this.state.ejected || this.state.paused}/>}
          </div>
        }
        <div className="App-footer">
          <div className="App-footer-main">
            <button onClick={this.prevSong}
                    className="box-button"
                    disabled={this.state.ejected}>
              &lt;
            </button>
            {' '}
            <button onClick={this.togglePause}
                    className="box-button"
                    disabled={this.state.ejected}>
              {this.state.paused ? 'Resume' : 'Pause'}
            </button>
            {' '}
            <button onClick={this.nextSong}
                    className="box-button"
                    disabled={this.state.ejected}>
              &gt;
            </button>
            {' '}
            {this.state.currentSongNumSubtunes > 1 &&
            <span style={{whiteSpace: 'nowrap'}}>
              Tune {this.state.currentSongSubtune + 1} of {this.state.currentSongNumSubtunes}{' '}
              <button
                className="box-button"
                disabled={this.state.ejected}
                onClick={this.prevSubtune}>&lt;
              </button>
              {' '}
              <button
                className="box-button"
                disabled={this.state.ejected}
                onClick={this.nextSubtune}>&gt;
              </button>
            </span>}
            <span style={{float: 'right'}}>
              <button className="box-button" onClick={this.handlePlayRandom}>
                I'm Feeling Lucky
              </button>
              {' '}
              {!this.state.showSettings &&
              <button className="box-button" onClick={this.toggleSettings}>
                Player Settings >
              </button>}
            </span>
            {this.state.playerError &&
            <div className="App-error">ERROR: {this.state.playerError}</div>
            }
            <TimeSlider
              currentSongPositionMs={this.state.currentSongPositionMs}
              currentSongDurationMs={this.state.currentSongDurationMs}
              onChange={this.handleTimeSliderChange}/>
            {!this.state.ejected &&
            <div className="SongDetails">
              {this.state.favorites &&
              <div style={{float: 'left', marginBottom: '58px'}}>
                <FavoriteButton favorites={this.state.favorites}
                                toggleFavorite={this.handleToggleFavorite}
                                href={this.state.songUrl}/>
              </div>}
              <div className="SongDetails-title">{title}</div>
              <div className="SongDetails-subtitle">{subtitle}</div>
            </div>}
          </div>
          {this.state.showSettings &&
          <div className="App-footer-settings">
            {this.sequencer.getPlayer() ?
              <PlayerParams
                ejected={this.state.ejected}
                tempo={this.state.tempo}
                numVoices={this.state.currentSongNumVoices}
                voices={this.state.voices}
                voiceNames={this.state.voiceNames}
                handleTempoChange={this.handleTempoChange}
                handleSetVoices={this.handleSetVoices}
                toggleSettings={this.toggleSettings}
                getParameter={this.sequencer.getPlayer().getParameter}
                setParameter={this.sequencer.getPlayer().setParameter}
                params={this.sequencer.getPlayer().getParameters()}/>
              :
              <div>--</div>}
          </div>}
          {this.state.imageUrl &&
          <div className="App-footer-art" style={{backgroundImage: `url("${this.state.imageUrl}")`}}/>}
        </div>
      </div>
    );
  }
}

export default App;
