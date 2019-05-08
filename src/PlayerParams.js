import {PureComponent} from 'react';
import React from 'react';

export default class PlayerParams extends PureComponent {
  constructor(props) {
    super(props);

    this.handleVoiceToggle = this.handleVoiceToggle.bind(this);
  }

  handleVoiceToggle(e, index) {
    const voices = this.props.voices;
    e = e.nativeEvent || {};
    if (e.altKey || e.shiftKey || e.metaKey) {
      // Behaves like Photoshop (toggling layer visibility with alt+click)
      if (voices.every((enabled, i) => (i === index) === enabled)) {
        // Alt-click on a single enabled channel unmutes everything
        voices.fill(true);
      } else {
        // Solo the channel
        voices.fill(false);
        voices[index] = true;
      }
    } else {
      // Regular toggle behavior
      voices[index] = !voices[index];
    }
    this.props.handleSetVoices(voices);
  }

  render() {
    return (
      <div className='PlayerParams'>
        Speed:{' '}
        <input
          disabled={this.props.ejected}
          type='range' value={this.props.tempo}
          min='0.3' max='2.0' step='0.05'
          onInput={this.props.handleTempoChange}
          onChange={this.props.handleTempoChange}/>{' '}
        {this.props.tempo.toFixed(2)}<br/>

        Voices:{' '}
        <div style={{display: 'flex', flexWrap: 'wrap', marginRight: '-8px'}}>
          {[...Array(this.props.numVoices)].map((_, i) => {
            return (
              <label className='App-voice-label' key={i}>
                <input
                  title='Alt+click to solo. Alt+click again to unmute all.'
                  type='checkbox'
                  onChange={(e) => this.handleVoiceToggle(e, i)}
                  checked={this.props.voices[i]}/>
                {this.props.voiceNames[i]}
              </label>
            )
          })}
        </div>

        {this.props.params.map(param => {
          switch (param.type) {
            case 'enum':
              return (
                <div key={param.id}>
                  {param.label}:{' '}
                  <select
                    onChange={(e) => {
                      this.props.setParameter(param.id, e.target.value);
                      this.forceUpdate();
                    }}
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
                  {param.label}:{' '}
                  <input type='range'
                         min={param.min} max={param.max} step={param.step}
                         value={this.props.getParameter(param.id)}
                         onChange={(e) => {
                           this.props.setParameter(param.id, e.target.value);
                           this.forceUpdate();
                         }}>
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