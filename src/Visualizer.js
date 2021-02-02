import pianoKeys from './images/piano-keys.png';
import Spectrogram from './Spectrogram';
import React, {PureComponent} from 'react';

const SPECTROGRAM_MODES = [
  'Linear FFT', 'Log FFT', 'Constant Q'
];
const WEIGHTING_MODES = [
  {label: 'None', description: 'No attenuation'},
  {label: 'A-Weighting', description: 'Darkens low and high frequencies to approximate sensitivity of human hearing.'}
];
const FFT_SIZES = [
  512, 1024, 2048, 4096, 8192
];
const FFT_LABELS = [
  '512', '1K', '2K', '4K', '8K', '16K'
];
const VIS_WIDTH = 448;

export default class Visualizer extends PureComponent {
  constructor(props) {
    super(props);

    this.state = {
      vizMode: 2,
      weightingMode: 1,
      fftSize: 2048,
      enabled: false,
    };

    this.freqCanvasRef = React.createRef();
    this.specCanvasRef = React.createRef();
    this.pianoKeysRef = React.createRef();
  }

  componentDidMount() {
    this.spectrogram = new Spectrogram(
      this.props.chipCore,
      this.props.audioCtx,
      this.props.sourceNode,
      this.freqCanvasRef.current,
      this.specCanvasRef.current,
      this.pianoKeysRef.current,
    );
    this.spectrogram.setMode(this.state.vizMode);
    this.spectrogram.setWeighting(this.state.weightingMode);
  }

  componentDidUpdate(prevProps) {
    this.spectrogram.setPaused(this.state.enabled ? this.props.paused : true);
  }

  handleModeClick = (e) => {
    const mode = parseInt(e.target.value, 10);
    this.setState({vizMode: mode});
    this.spectrogram.setMode(mode);
  };

  handleWeightingModeClick = (e) => {
    const mode = parseInt(e.target.value, 10);
    this.setState({weightingMode: mode});
    this.spectrogram.setWeighting(mode);
  };

  handleFFTSizeClick = (e) => {
    const size = parseInt(e.target.value, 10);
    this.setState({fftSize: size});
    this.spectrogram.setFFTSize(size);
  };

  handleToggleVisualizer = (e) => {
    const enabled = e.target.value === 'true';
    this.setState({enabled: enabled});
  };

  render() {
    const enabledStyle = {
      display: this.state.enabled ? 'block' : 'none',
      width: VIS_WIDTH,
      boxSizing: 'border-box',
    };
    return (
      <div className='Visualizer'>
        <h3 className='Visualizer-toggle'>
          Visualizer{' '}
          <label className='inline'><input onClick={this.handleToggleVisualizer}
                        type='radio'
                        value={true}
                        defaultChecked={this.state.enabled === true}
                        name='visualizer-enabled'/>On</label>
          <label className='inline'><input onClick={this.handleToggleVisualizer}
                        type='radio'
                        value={false}
                        defaultChecked={this.state.enabled === false}
                        name='visualizer-enabled'/>Off</label>
        </h3>
        <div className='Visualizer-options' style={enabledStyle}>
          <div>
            Mode:{' '}
            {
              SPECTROGRAM_MODES.map((mode, i) =>
                <label key={'m_'+i} className='inline'><input onClick={this.handleModeClick}
                                      type='radio'
                                      name='spectrogram-mode'
                                      defaultChecked={this.state.vizMode === i}
                                      value={i}/>{mode}</label>
              )
            }
          </div>
          {this.state.vizMode === 2 ?
            <div>
              Weighting:{' '}
              {
                WEIGHTING_MODES.map((mode, i) =>
                  <label title={mode.description} key={'w_'+i} className='inline'>
                    <input onClick={this.handleWeightingModeClick}
                           type='radio'
                           name='weighting-mode'
                           defaultChecked={this.state.weightingMode === i}
                           value={i}/>{mode.label}</label>
                )
              }
            </div>
            :
            <div>
              FFT Size:{' '}
              {
                FFT_SIZES.map((size, i) =>
                  <label key={'f_'+i} className='inline'><input onClick={this.handleFFTSizeClick}
                                        type='radio'
                                        name='fft-size'
                                        defaultChecked={this.state.fftSize === size}
                                        value={size}/>{FFT_LABELS[i]}</label>
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
               display: (this.state.enabled && this.state.vizMode === 2) ? 'block' : 'none',
               width: VIS_WIDTH,
             }}/>
      </div>
    );
  }
}