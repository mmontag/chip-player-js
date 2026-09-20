import React, { PureComponent } from 'react';
import autoBind from 'auto-bind';
import PianoRollEngine from './PianoRollEngine';
import { parseMidiData } from './midi-parser';
import { PIANO_ROLL_CONFIG } from './config';

export default class PianoRollVisualizer extends PureComponent {
  constructor(props) {
    super(props);
    autoBind(this);

    this.containerRef = React.createRef();
    this.canvasRef = React.createRef();
    this.chordLabelRef = React.createRef();
    this.engine = null;
    this.currentBuffer = null;
    this.resizeObserver = null;

    this.state = {
      width: props.width || 448,
      height: props.height || 400,
    };
  }

  componentDidMount() {
    if (this.canvasRef.current) {
      this.engine = new PianoRollEngine(this.canvasRef.current, {
        getCurrentPositionMs: this.props.getCurrentPositionMs,
        getPlaybackRate: this.props.getPlaybackRate,
        getAudioLatencyMs: this.props.getAudioLatencyMs,
        isPaused: this.props.paused,
        ORIENTATION: this.props.theaterMode ? 'horizontal' : 'vertical',
        onChordChange: (chord) => {
          if (this.chordLabelRef.current) {
            this.chordLabelRef.current.textContent = chord;
          }
        },
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

      this.updateChordLabelPosition();
    }

    if (this.containerRef.current && typeof ResizeObserver !== 'undefined') {
      this.resizeObserver = new ResizeObserver((entries) => {
        for (const entry of entries) {
          const { width, height } = entry.contentRect;
          const w = Math.floor(width);
          const h = Math.floor(height);
          if (w > 0 && h > 0 && (w !== this.state.width || h !== this.state.height)) {
            this.setState({ width: w, height: h });
            if (this.engine) {
              this.engine.resize(w, h);
            }
          }
        }
      });
      this.resizeObserver.observe(this.containerRef.current);
    }
  }

  componentDidUpdate(prevProps, prevState) {
    if (!this.engine) return;

    if (prevState && (prevState.width !== this.state.width || prevState.height !== this.state.height)) {
      this.engine.resize(this.state.width, this.state.height);
    }

    if (prevProps.getCurrentPositionMs !== this.props.getCurrentPositionMs) {
      this.engine.getCurrentPositionMs = this.props.getCurrentPositionMs;
      if (this.props.paused) {
        this.engine.render();
      }
    }

    if (prevProps.getPlaybackRate !== this.props.getPlaybackRate) {
      this.engine.getPlaybackRate = this.props.getPlaybackRate;
    }

    if (prevProps.getAudioLatencyMs !== this.props.getAudioLatencyMs) {
      this.engine.getAudioLatencyMsCallback = this.props.getAudioLatencyMs;
    }

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

    if (prevProps.theaterMode !== this.props.theaterMode) {
      this.engine.updateConfig({
        ORIENTATION: this.props.theaterMode ? 'horizontal' : 'vertical',
      });
    }

    if (typeof this.props.width === 'number' && prevProps.width !== this.props.width) {
      if (this.props.width !== this.state.width) {
        this.setState({ width: this.props.width });
      }
    }

    this.updateChordLabelPosition();
  }

  getKeyboardHeight() {
    if (this.engine) {
      return this.engine.getKeyboardHeight();
    }
    const isVertical = !this.props.theaterMode;
    const isKeyboardVisible = PIANO_ROLL_CONFIG.SHOW_KEYBOARD !== false;
    const isForward = PIANO_ROLL_CONFIG.DIRECTION !== 'reverse' && PIANO_ROLL_CONFIG.DIRECTION !== 'bottom-to-top' && PIANO_ROLL_CONFIG.DIRECTION !== 'left-to-right';
    if (!isVertical || !isKeyboardVisible || !isForward) return 0;

    const width = this.state.width || this.props.width || 448;
    let totalPitchDimension = width;
    if (PIANO_ROLL_CONFIG.PITCH_ZOOM_MODE !== 'fill') {
      const totalUnits = 624 / 7;
      const unitScale = PIANO_ROLL_CONFIG.PIXELS_PER_NOTE || 5;
      totalPitchDimension = Math.round(totalUnits * unitScale);
    }
    const offset = typeof PIANO_ROLL_CONFIG.PLAYHEAD_OFFSET_PX === 'number' ? PIANO_ROLL_CONFIG.PLAYHEAD_OFFSET_PX : 2;
    const keyboardSize = Math.max(1, Math.round(totalPitchDimension * (PIANO_ROLL_CONFIG.KEYBOARD_ASPECT_RATIO || 0.125)));
    return keyboardSize + offset;
  }

  updateChordLabelPosition() {
    if (this.chordLabelRef.current) {
      const keyboardHeight = this.getKeyboardHeight();
      this.chordLabelRef.current.style.bottom = keyboardHeight > 0
        ? `calc(var(--charH) + ${keyboardHeight}px)`
        : 'var(--charH)';
    }
  }

  componentWillUnmount() {
    if (this.resizeObserver) {
      this.resizeObserver.disconnect();
      this.resizeObserver = null;
    }
    if (this.engine) {
      this.engine.destroy();
      this.engine = null;
    }
    if (this.chordLabelRef.current) {
      this.chordLabelRef.current.textContent = '';
    }
  }

  loadMidi(buffer) {
    if (!buffer) {
      this.currentBuffer = null;
      if (this.engine) this.engine.setMidiData(null);
      return;
    }

    this.currentBuffer = buffer;
    const parsedMidi = parseMidiData(buffer);
    if (!parsedMidi) {
      if (this.engine) this.engine.setMidiData(null);
      return;
    }

    if (this.engine) {
      this.engine.setMidiData(parsedMidi);
      if (!this.props.paused) {
        this.engine.start();
      }
    }
  }

  render() {
    const { width, height } = this.state;
    const { style = {}, theaterMode, onToggleTheaterMode } = this.props;
    const keyboardHeight = this.getKeyboardHeight();
    const chordLabelBottom = keyboardHeight > 0
      ? `calc(var(--charH) + ${keyboardHeight}px)`
      : 'var(--charH)';

    return (
      <div
        ref={this.containerRef}
        className={`PianoRoll-container ${theaterMode ? 'theater' : 'normal'}`}
        style={{
          width: theaterMode ? '100%' : (this.props.width || '100%'),
          backgroundColor: PIANO_ROLL_CONFIG.BACKGROUND_COLOR,
          ...style,
        }}
      >
        <canvas
          ref={this.canvasRef}
          width={width}
          height={height}
          className="PianoRoll-canvas"
        />
        {onToggleTheaterMode && (
          <button
            type="button"
            className="box-button PianoRoll-theater-toggle"
            onClick={onToggleTheaterMode}
            title={theaterMode ? 'Exit theater mode' : 'Theater mode'}
          >
            {theaterMode ? '→[]←' : '[←→]'}
          </button>
        )}
        <div
          ref={this.chordLabelRef}
          style={{
            position: 'absolute',
            bottom: chordLabelBottom,
            right: 'var(--charW2)',
            color: '#ffffff',
            pointerEvents: 'none',
            userSelect: 'none',
          }}
        />
      </div>
    );
  }
}
