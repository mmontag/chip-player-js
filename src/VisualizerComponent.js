import Viz from './Visualizer';
import React, {Component} from 'react';

const VIZ_MODES = [
  'Linear', 'Log', 'Constant Q'
];
export default class Visualizer extends Component {
  constructor(props) {
    super(props);

    this.handleVizModeChange = this.handleVizModeChange.bind(this);

    this.freqCanvasRef = React.createRef();
    this.specCanvasRef = React.createRef();
  }

  componentDidMount() {
    // setTimeout(() => {
      this.viz = new Viz(
        this.props.audioCtx,
        this.freqCanvasRef.current,
        this.specCanvasRef.current,
      );
      this.props.sourceNode.connect(this.viz.analyserNode);
    // }, 1000);
  }

  shouldComponentUpdate(nextProps, nextState) {
    this.viz.setPaused(nextProps.paused);
    return false;
  }

  handleVizModeChange(val) {
    this.viz.setMode(val);
  }

  render() {
    const handleModeClick = (e) => {
      this.handleVizModeChange(parseInt(e.target.value));
    };
    return (
      <section className='Visualizer'>
        <h3>Visualizer</h3>
        {
          VIZ_MODES.map((mode, i) =>
            <label key={i}><input onClick={handleModeClick}
                                  type='radio'
                                  name='viz-mode'
                                  value={i}/>{mode}</label>
          )
        }
        <canvas className='Visualizer-canvas' width={1024} height={60} ref={this.freqCanvasRef}/>
        <canvas className='Visualizer-canvas' width={1024} height={400} ref={this.specCanvasRef}/>
      </section>
    );
  }
}