import React, {Component} from "react";

export class VolumeSlider extends Component {
  render() {
    return <div className="VolumeSlider">
      <input type='range'
             title={"Volume"}
             min={0} max={150} step={1}
             onChange={this.props.onChange}
             onDoubleClick={this.props.handleReset}
             onContextMenu={this.props.handleReset}
             value={this.props.value}>
      </input>
      <div className="VolumeSlider-labels">
        <div>Volume</div>
        <div>{this.props.value}%</div>
      </div>
    </div>;
  }
}
