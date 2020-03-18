import React, {Component} from "react";

export class VolumeSlider extends Component {
  render() {
    return <div
      style={{flexShrink: 0, marginTop: "var(--charH)", marginBottom: "var(--charH)", marginLeft: "var(--charW2)"}}>
      <input type='range'
             title={"Volume"}
             min={0} max={100} step={1}
             onChange={this.props.onChange}
             value={this.props.value}>
      </input>
      <div className="VolumeSlider-labels">
        <div>Volume</div>
        <div>{this.props.value}%</div>
      </div>
    </div>;
  }
}
