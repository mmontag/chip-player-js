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
  }

  componentDidMount() {
    if (this.canvasRef.current) {
      this.engine = new PianoRollEngine(this.canvasRef.current, {
        getCurrentPositionMs: this.props.getCurrentPositionMs,
        getPlaybackRate: this.props.getPlaybackRate,
        getAudioLatencyMs: this.props.getAudioLatencyMs,
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

  componentDidUpdate(prevProps) {
    if (!this.engine) return;

    if (prevProps.getCurrentPositionMs !== this.props.getCurrentPositionMs) {
      this.engine.getCurrentPositionMs = this.props.getCurrentPositionMs;
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

    if (prevProps.width !== this.props.width || prevProps.height !== this.props.height) {
      this.engine.resize(this.props.width, this.props.height);
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
    const { width = 448, height = 800, style = {} } = this.props;

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
      </div>
    );
  }
}
