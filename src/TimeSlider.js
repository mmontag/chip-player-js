import React, {PureComponent} from 'react';
import Slider from "./Slider";

export default class TimeSlider extends PureComponent {
  constructor(props) {
    super(props);

    this.getSongPos = this.getSongPos.bind(this);
    this.getTimeLabel = this.getTimeLabel.bind(this);
    this.handlePositionDrag = this.handlePositionDrag.bind(this);
    this.handlePositionDrop = this.handlePositionDrop.bind(this);

    this.state = {
      draggedSongPositionMs: -1,
    };
  }

  getSongPos() {
    return this.props.currentSongPositionMs / this.props.currentSongDurationMs;
  }

  getTimeLabel() {
    const val = this.state.draggedSongPositionMs >= 0 ?
      this.state.draggedSongPositionMs :
      this.props.currentSongPositionMs;
    return this.getTime(val);
  }

  getTime(ms) {
    const pad = n => n < 10 ? '0' + n : n;
    const min = Math.floor(ms / 60000);
    const sec = (Math.floor((ms % 60000) / 100) / 10).toFixed(1);
    return `${min}:${pad(sec)}`;
  }

  handlePositionDrag(event) {
    const pos = event.target ? event.target.value : event;
    // Update current time position label
    this.setState({
      draggedSongPositionMs: pos * this.props.currentSongDurationMs,
    });
  }

  handlePositionDrop(event) {
    this.setState({
      draggedSongPositionMs: -1,
    });
    this.props.onChange(event);
  }

  render() {
    return (
      <div className='TimeSlider'>
        <Slider
          pos={this.getSongPos()}
          onDrag={this.handlePositionDrag}
          onChange={this.handlePositionDrop}/>
        Time: {this.getTimeLabel()} / {this.getTime(this.props.currentSongDurationMs)}
      </div>
    );
  }
}