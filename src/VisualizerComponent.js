import Viz from './Visualizer';
import React, {PureComponent} from 'react';

export default class Visualizer extends PureComponent {
  constructor(props) {
    super(props);
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

  render() {
    return (
      <section>
        <h3>Visualizer</h3>
        <canvas width={600} height={60} ref={this.freqCanvasRef}/>
        <canvas width={600} height={400} ref={this.specCanvasRef}/>
      </section>
    );
  }
}