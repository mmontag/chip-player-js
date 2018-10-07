import Spectrogram from './Spectrogram';
import React, {Component} from 'react';

const SPECTROGRAM_MODES = [
  'Linear FFT', 'Log FFT', 'Constant Q'
];

const FFT_SIZES = [
  512, 1024, 2048, 4096, 8192, 16384
];

export default class Visualizer extends Component {
  constructor(props) {
    super(props);

    this.freqCanvasRef = React.createRef();
    this.specCanvasRef = React.createRef();
  }

  componentDidMount() {
    this.spectrogram = new Spectrogram(
      this.props.chipCore,
      this.props.audioCtx,
      this.freqCanvasRef.current,
      this.specCanvasRef.current,
    );
    this.props.sourceNode.connect(this.spectrogram.analyserNode);
  }

  shouldComponentUpdate(nextProps, nextState) {
    this.spectrogram.setPaused(nextProps.paused);
    return false;
  }

  render() {
    const handleModeClick    = (e) => this.spectrogram.setMode(parseInt(e.target.value));
    const handleFFTSizeClick = (e) => this.spectrogram.setFFTSize(parseInt(e.target.value));
    return (
      <section className='Visualizer'>
        <h3>Visualizer</h3>
        Mode: {
          SPECTROGRAM_MODES.map((mode, i) =>
            <label key={i}><input onClick={handleModeClick}
                                  type='radio'
                                  name='spectrogram-mode'
                                  value={i}/>{mode}</label>
          )
        }
        <br/>
        FFT Size: {
          FFT_SIZES.map((size, i) =>
            <label key={i}><input onClick={handleFFTSizeClick}
                                  type='radio'
                                  name='fft-size'
                                  value={size}/>{size}</label>
          )
        }
        <canvas className='Visualizer-canvas' width={1024} height={60} ref={this.freqCanvasRef}/>
        <canvas className='Visualizer-canvas' width={1024} height={400} ref={this.specCanvasRef}/>
      </section>
    );
  }
}