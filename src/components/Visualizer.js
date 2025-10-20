import pianoKeys from '../images/piano-keys.png';
import Spectrogram from '../Spectrogram';
import React, { Fragment, PureComponent } from 'react';

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
const SPEEDS = [
  1, 2, 4
];
const SPEED_LABELS = [
  'Lo', 'Med', 'Hi'
];
const VIS_WIDTH = 448;

export default class Visualizer extends PureComponent {
  constructor(props) {
    super(props);

    this.state = {
      vizMode: 2,
      weightingMode: 1,
      fftSize: 2048,
      speed: 2,
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
    this.spectrogram.setSpeed(this.state.speed);
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

  handleSpeedClick = (e) => {
    const speed = parseInt(e.target.value, 10);
    this.setState({ speed: speed });
    this.spectrogram.setSpeed(speed);
  }

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
          <input onClick={this.handleToggleVisualizer}
                 id='vis-on'
                 type='radio'
                 value={true}
                 defaultChecked={this.state.enabled === true}
                 name='visualizer-enabled'/>
          <label htmlFor='vis-on' className='inline'>On</label>
          <input onClick={this.handleToggleVisualizer}
                 id='vis-off'
                 type='radio'
                 value={false}
                 defaultChecked={this.state.enabled === false}
                 name='visualizer-enabled'/>
          <label htmlFor='vis-off' className='inline'>Off</label>
        </h3>
        <div className='Visualizer-options' style={enabledStyle}>
          <div>
            <span className='VisualizerParams-label'>Mode:</span>
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
              <span className='VisualizerParams-label'>Weighting:</span>
              {
                WEIGHTING_MODES.map((mode, i) =>
                  <Fragment key={'w_'+i}>
                    <input onClick={this.handleWeightingModeClick}
                           type='radio'
                           id={'w_'+i}
                           name='weighting-mode'
                           defaultChecked={this.state.weightingMode === i}
                           value={i}/>
                    <label htmlFor={'w_'+i} title={mode.description} className='inline'>
                    {mode.label}</label>
                  </Fragment>
                )
              }
            </div>
            :
            <div>
              <span className='VisualizerParams-label'>FFT Size:</span>
              {
                FFT_SIZES.map((size, i) =>
                  <Fragment key={'f_'+i}>
                    <input onClick={this.handleFFTSizeClick}
                           type='radio'
                           id={'f_'+i}
                           name='fft-size'
                           defaultChecked={this.state.fftSize === size}
                           value={size}/>
                    <label htmlFor={'f_'+i} className='inline'>{FFT_LABELS[i]}</label>
                  </Fragment>
                )
              }
            </div>
          }
          <div title="Vertical scrolling speed (pixels per frame)">
            <span className='VisualizerParams-label'>Speed:</span>
            {
              SPEEDS.map((speed, i) =>
                <Fragment key={'s_'+i}>
                  <input onClick={this.handleSpeedClick}
                         type='radio'
                         id={'s_'+i}
                         name='speed'
                         defaultChecked={this.state.speed === speed}
                         value={speed}/>
                  <label htmlFor={'s_'+i} className='inline'>{SPEED_LABELS[i]}</label>
                </Fragment>
              )
            }
          </div>
        </div>
        <canvas style={enabledStyle} className='Visualizer-analyzer' width={VIS_WIDTH} height={60}
                ref={this.freqCanvasRef}/>
        <canvas style={enabledStyle} className='Visualizer-spectrogram' width={VIS_WIDTH} height={800}
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
