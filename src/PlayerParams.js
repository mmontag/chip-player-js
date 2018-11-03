import { Component } from 'react';
import React from "react";

export default class PlayerParams extends Component {
  constructor(props) {
    super(props);
  }

  render() {
    return (
      <div>
        <h3>Player Settings</h3>
        {this.props.params.map(param => {
          switch (param.type) {
            case 'enum':
              return (
                <div key={param.id}>
                  {param.label}:&nbsp;
                  <select
                    onChange={(e) => this.props.setParameter(param.id, e.target.value)}
                    defaultValue={this.props.getParameter(param.id)}>
                    {param.options.map(optgroup =>
                      <optgroup key={optgroup.label} label={optgroup.label}>
                        {optgroup.items.map(option =>
                          <option key={option.value} value={option.value}>{option.label}</option>
                        )}
                      </optgroup>
                    )}
                  </select>
                </div>
              );
            case 'number':
              return (
                <div key={param.id}>
                  {param.label}:&nbsp;
                  <input type="range"
                         min={param.min} max={param.max} step={param.step}
                         value={this.props.getParameter(param.id)}
                         onChange={(e) => this.props.setParameter(param.id, e.target.value)}>
                  </input>
                </div>
              );
            default:
              return null;
          }
        })}
      </div>

    );
  }
}