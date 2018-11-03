import pianoKeys from './images/piano-keys.png';
import Spectrogram from './Spectrogram';
import React, {PureComponent} from 'react';

const SPECTROGRAM_MODES = [
  'Linear FFT', 'Log FFT', 'Constant Q'
];

const WEIGHTING_MODES = [
  {label: 'None', description: 'No attenuation'},
  {label: 'A Weighting', description: 'Darkens low and high frequencies to approximate sensitivity of human hearing.'}
];

const FFT_SIZES = [
  512, 1024, 2048, 4096, 8192, 16384
];

export default class Visualizer extends PureComponent {
  constructor(props) {
    super(props);

    this.state = {
      vizMode: 2,
      weightingMode: 1,
      fftSize: 2048,
      enabled: true,
    };

    this.freqCanvasRef = React.createRef();
    this.specCanvasRef = React.createRef();
    this.pianoKeysRef = React.createRef();
  }

  componentDidMount() {
    this.spectrogram = new Spectrogram(
      this.props.chipCore,
      this.props.audioCtx,
      this.freqCanvasRef.current,
      this.specCanvasRef.current,
      this.pianoKeysRef.current,
    );
    this.spectrogram.setMode(this.state.vizMode);
    this.spectrogram.setWeighting(this.state.weightingMode);
    this.props.sourceNode.connect(this.spectrogram.analyserNode);
  }

  componentDidUpdate(prevProps) {
    this.spectrogram.setPaused(this.state.enabled ? this.props.paused : true);
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
    const handleToggleVisualizer = (e) => {
      const enabled = e.target.value === 'true';
      this.setState({enabled: enabled});
    };
    const enabledStyle = {
      display: this.state.enabled ? 'block' : 'none',
    };
    return (
      <div className='Visualizer'>
        <h3 className='Visualizer-toggle'>
          Visualizer&nbsp;
          <label><input onClick={handleToggleVisualizer}
                        type='radio'
                        value={true}
                        defaultChecked={this.state.enabled === true}
                        name='visualizer-enabled'/>On</label>
          <label><input onClick={handleToggleVisualizer}
                        type='radio'
                        value={false}
                        defaultChecked={this.state.enabled === false}
                        name='visualizer-enabled'/>Off</label>
        </h3>
        <div className='Visualizer-options' style={enabledStyle}>
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
              <label title={mode.description} key={i}>
                <input onClick={handleWeightingModeClick}
                       type='radio'
                       name='weighting-mode'
                       defaultChecked={this.state.weightingMode === i}
                       value={i}/>{mode.label}</label>
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
        </div>
        <canvas style={enabledStyle} className='Visualizer-analyzer' width={448} height={60}
                ref={this.freqCanvasRef}/>
        <canvas style={enabledStyle} className='Visualizer-spectrogram' width={448} height={400}
                ref={this.specCanvasRef}/>
        <img src={pianoKeys}
             className='Visualizer-overlay'
             ref={this.pianoKeysRef}
             alt='Piano keys'
             style={{
               display: this.state.vizMode === 2 ? 'block' : 'none'
             }}/>
      </div>
    );
  }
}