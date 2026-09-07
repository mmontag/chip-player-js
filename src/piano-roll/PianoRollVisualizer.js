import React, { PureComponent } from 'react';
import autoBind from 'auto-bind';
import PianoRollEngine from './PianoRollEngine';
import { parseMidiData } from './midi-parser';
import { PIANO_ROLL_CONFIG } from './config';

export default class PianoRollVisualizer extends PureComponent {
  constructor(props) {
    super(props);
    autoBind(this);

    this.canvasRef = React.createRef();
    this.engine = null;
    this.currentBuffer = null;

    this.state = {
      parsedMidi: null,
      hiddenChannels: new Set(),
      hiddenTracks: new Set(),
      listMode: PIANO_ROLL_CONFIG.LIST_MODE || 'channel',
      isOverlayExpanded: true,
    };
  }

  componentDidMount() {
    if (this.canvasRef.current) {
      this.engine = new PianoRollEngine(this.canvasRef.current, {
        getCurrentPositionMs: this.props.getCurrentPositionMs,
        isPaused: this.props.paused,
      });

      if (this.props.voiceMask) {
        this.engine.setVoiceMask(this.props.voiceMask);
      }

      if (this.props.midiData) {
        this.loadMidi(this.props.midiData);
      }

      if (!this.props.paused) {
        this.engine.start();
      }
    }
  }

  componentDidUpdate(prevProps, prevState) {
    if (!this.engine) return;

    if (prevProps.midiData !== this.props.midiData) {
      this.loadMidi(this.props.midiData);
    }

    if (prevProps.paused !== this.props.paused) {
      this.engine.setPaused(this.props.paused);
    } else if (!this.props.paused && this.engine.animFrameId === null) {
      this.engine.start();
    }

    if (prevProps.voiceMask !== this.props.voiceMask) {
      this.engine.setVoiceMask(this.props.voiceMask);
    }

    if (prevProps.width !== this.props.width || prevProps.height !== this.props.height) {
      this.engine.resize(this.props.width, this.props.height);
    }

    if (prevState.hiddenChannels !== this.state.hiddenChannels) {
      this.engine.setHiddenChannels(this.state.hiddenChannels);
    }

    if (prevState.hiddenTracks !== this.state.hiddenTracks) {
      this.engine.setHiddenTracks(this.state.hiddenTracks);
    }
  }

  componentWillUnmount() {
    if (this.engine) {
      this.engine.destroy();
      this.engine = null;
    }
  }

  loadMidi(buffer) {
    if (!buffer) {
      this.currentBuffer = null;
      this.setState({ parsedMidi: null, hiddenChannels: new Set(), hiddenTracks: new Set() });
      if (this.engine) this.engine.setMidiData(null);
      return;
    }

    this.currentBuffer = buffer;
    const parsedMidi = parseMidiData(buffer);
    if (!parsedMidi) {
      this.setState({ parsedMidi: null, hiddenChannels: new Set(), hiddenTracks: new Set() });
      if (this.engine) this.engine.setMidiData(null);
      return;
    }

    this.setState({
      parsedMidi,
      hiddenChannels: new Set(),
      hiddenTracks: new Set(),
    });

    if (this.engine) {
      this.engine.setMidiData(parsedMidi);
      if (!this.props.paused) {
        this.engine.start();
      }
    }
  }

  handleItemClick(type, id, event) {
    const isAltClick = event.altKey;
    const { parsedMidi, hiddenChannels, hiddenTracks } = this.state;
    if (!parsedMidi) return;

    if (type === 'channel') {
      const allChannels = parsedMidi.channels.map(c => c.channel);
      let newHidden = new Set(hiddenChannels);

      if (isAltClick) {
        // Option/Alt+Click: Solo or un-solo
        const isOnlyOneVisible = allChannels.every(ch => (ch === id ? !newHidden.has(ch) : newHidden.has(ch)));
        if (isOnlyOneVisible) {
          // Restore all
          newHidden.clear();
        } else {
          // Solo this item: hide everything except this id
          newHidden = new Set(allChannels.filter(ch => ch !== id));
        }
      } else {
        // Normal click: toggle visibility
        if (newHidden.has(id)) {
          newHidden.delete(id);
        } else {
          newHidden.add(id);
        }
      }
      this.setState({ hiddenChannels: newHidden });
    } else {
      // Track type
      const allTracks = parsedMidi.tracks.map(t => t.track);
      let newHidden = new Set(hiddenTracks);

      if (isAltClick) {
        const isOnlyOneVisible = allTracks.every(trk => (trk === id ? !newHidden.has(trk) : newHidden.has(trk)));
        if (isOnlyOneVisible) {
          newHidden.clear();
        } else {
          newHidden = new Set(allTracks.filter(trk => trk !== id));
        }
      } else {
        if (newHidden.has(id)) {
          newHidden.delete(id);
        } else {
          newHidden.add(id);
        }
      }
      this.setState({ hiddenTracks: newHidden });
    }
  }

  toggleOverlayExpanded() {
    this.setState(prev => ({ isOverlayExpanded: !prev.isOverlayExpanded }));
  }

  setListMode(mode) {
    this.setState({ listMode: mode });
  }

  render() {
    const { width = 448, height = 800, style = {} } = this.props;
    const { parsedMidi, hiddenChannels, hiddenTracks, listMode, isOverlayExpanded } = this.state;

    const showOverlay = PIANO_ROLL_CONFIG.SHOW_TRACK_CHANNEL_LIST && parsedMidi;

    return (
      <div
        className="PianoRoll-container"
        style={{
          width,
          minHeight: height,
          backgroundColor: PIANO_ROLL_CONFIG.BACKGROUND_COLOR,
          overflow: 'hidden',
          ...style,
        }}
      >
        <canvas
          ref={this.canvasRef}
          width={width}
          height={height}
          className="PianoRoll-canvas"
        />

        {showOverlay && (
          <div className="PianoRoll-overlay">
            <div className="PianoRoll-overlay-header" onClick={this.toggleOverlayExpanded}>
              <span className="PianoRoll-overlay-title">
                {listMode === 'channel' ? 'Channels' : 'Tracks'} ({listMode === 'channel' ? parsedMidi.channels.length : parsedMidi.tracks.length})
              </span>
              <span className="PianoRoll-overlay-toggle-icon">
                {isOverlayExpanded ? '▾' : '▸'}
              </span>
            </div>

            {isOverlayExpanded && (
              <div className="PianoRoll-overlay-body">
                <div className="PianoRoll-overlay-mode-select">
                  <button
                    type="button"
                    className={`PianoRoll-mode-btn ${listMode === 'channel' ? 'active' : ''}`}
                    onClick={() => this.setListMode('channel')}
                  >
                    Channels
                  </button>
                  <button
                    type="button"
                    className={`PianoRoll-mode-btn ${listMode === 'track' ? 'active' : ''}`}
                    onClick={() => this.setListMode('track')}
                  >
                    Tracks
                  </button>
                </div>

                <div className="PianoRoll-overlay-list">
                  {listMode === 'channel' ? (
                    parsedMidi.channels.map(item => {
                      const isHidden = hiddenChannels.has(item.channel);
                      const isExternallyMuted = this.props.voiceMask && this.props.voiceMask[item.channel] === false;
                      const color = PIANO_ROLL_CONFIG.CHANNEL_COLORS[item.channel % PIANO_ROLL_CONFIG.CHANNEL_COLORS.length];

                      return (
                        <div
                          key={`ch_${item.channel}`}
                          className={`PianoRoll-item ${isHidden || isExternallyMuted ? 'muted' : ''}`}
                          onClick={e => this.handleItemClick('channel', item.channel, e)}
                          title={`Click to toggle; Option+Click to solo.\n${item.instrumentName}`}
                        >
                          <span
                            className="PianoRoll-item-swatch"
                            style={{
                              backgroundColor: color,
                              opacity: isHidden || isExternallyMuted ? 0.3 : 1,
                            }}
                          />
                          <span className="PianoRoll-item-num">Ch {item.channel + 1}</span>
                          <span className="PianoRoll-item-name">{item.instrumentName}</span>
                        </div>
                      );
                    })
                  ) : (
                    parsedMidi.tracks.map(item => {
                      const isHidden = hiddenTracks.has(item.track);
                      const color = PIANO_ROLL_CONFIG.TRACK_COLORS[item.track % PIANO_ROLL_CONFIG.TRACK_COLORS.length];

                      return (
                        <div
                          key={`trk_${item.track}`}
                          className={`PianoRoll-item ${isHidden ? 'muted' : ''}`}
                          onClick={e => this.handleItemClick('track', item.track, e)}
                          title={`Click to toggle; Option+Click to solo.\n${item.name}`}
                        >
                          <span
                            className="PianoRoll-item-swatch"
                            style={{
                              backgroundColor: color,
                              opacity: isHidden ? 0.3 : 1,
                            }}
                          />
                          <span className="PianoRoll-item-num">Trk {item.track + 1}</span>
                          <span className="PianoRoll-item-name">{item.name}</span>
                        </div>
                      );
                    })
                  )}
                </div>
              </div>
            )}
          </div>
        )}
      </div>
    );
  }
}
