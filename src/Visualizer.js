import Spectrogram from './Spectrogram';
import React, {Component} from 'react';

const SPECTROGRAM_MODES = [
  'Linear FFT', 'Log FFT', 'Constant Q'
];

const WEIGHTING_MODES = [
  'None', 'A Weighting'
];

const FFT_SIZES = [
  512, 1024, 2048, 4096, 8192, 16384
];

export default class Visualizer extends Component {
  constructor(props) {
    super(props);

    this.state = {
      vizMode: 0,
      weightingMode: 0,
      fftSize: 2048,
    };

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

  componentDidUpdate(prevProps) {
    this.spectrogram.setPaused(this.props.paused);
  }

  render() {
    const handleModeClick = (e) => {
      const mode = parseInt(e.target.value, 10);
      this.setState({vizMode: mode});
      this.spectrogram.setMode(mode);
    };
    const handleWeightingModeClick = (e) => {
      const mode = parseInt(e.target.value, 10);
      this.setState({weightingMode: mode});
      this.spectrogram.setWeighting(mode);
    };
    const handleFFTSizeClick = (e) => {
      const size = parseInt(e.target.value, 10);
      this.setState({fftSize: size});
      this.spectrogram.setFFTSize(size);
    };
    return (
      <section className='Visualizer'>
        <h3>Visualizer</h3>
        Mode:&nbsp;
        {
          SPECTROGRAM_MODES.map((mode, i) =>
            <label key={i}><input onClick={handleModeClick}
                                  type='radio'
                                  name='spectrogram-mode'
                                  defaultChecked={this.state.vizMode === i}
                                  value={i}/>{mode}</label>
          )
        }
        <br/>
        Weighting:&nbsp;
        {
          WEIGHTING_MODES.map((mode, i) =>
            <label key={i}><input onClick={handleWeightingModeClick}
                                  type='radio'
                                  name='weighting-mode'
                                  defaultChecked={this.state.weightingMode === i}
                                  value={i}/>{mode}</label>
          )
        }
        <br/>
        {this.state.vizMode !== 2 &&
        <div>
          FFT Size:&nbsp;
          {
            FFT_SIZES.map((size, i) =>
              <label key={i}><input onClick={handleFFTSizeClick}
                                    type='radio'
                                    name='fft-size'
                                    defaultChecked={this.state.fftSize === size}
                                    value={size}/>{size}</label>
            )
          }
        </div>
        }
        <canvas className='Visualizer-canvas' width={1024} height={60} ref={this.freqCanvasRef}/>
        <canvas className='Visualizer-canvas' width={1024} height={400} ref={this.specCanvasRef}/>
      </section>
    );
  }
}