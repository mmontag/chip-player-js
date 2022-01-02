import React, { PureComponent } from 'react';

export default class PlayerParams extends PureComponent {
  constructor(props) {
    super(props);

    this.handleVoiceToggle = this.handleVoiceToggle.bind(this);
  }

  // Voicename polling implementation left here for reference.
  //
  // Polling for voice updates is only useful for MIDIPlayer
  // because MIDI Player is the only player where voice names can update
  // in the middle of a song. Polling is probably a bad pattern.
  // This forces the player to keep a copy of voices in an array.
  //
  // Instead, the player should probably emit an event when voices have updated.
  //
  // Right now the only way for players to notify the UI is to invoke the
  // 'onPlayerStateUpdate' callback, which is really coarse and brittle.
  // It will trigger a setstate on the App level.
  //
  // A better way to bridge between the player engines and the
  // React UI is probably with an Event Bus (pubsub).
  //
  // The event bus can be passed down to children at arbitrary depth,
  // and each child can subscribe to events it cares about.
  //
  // componentDidMount() {
  //   const updateVoiceNames = () => {
  //     const voiceNames = this.props.getVoiceNames();
  //     if (voiceNames !== this.state.voiceNames) {
  //       console.debug("Updated voice names.");
  //       this.setState({
  //         voiceNames,
  //       });
  //     }
  //   };
  //   updateVoiceNames();
  //   this.timer = setInterval(updateVoiceNames, UPDATE_INTERVAL_MS);
  // }
  //
  // componentWillUnmount() {
  //   clearInterval(this.timer);
  // }

  handleVoiceToggle(e, index) {
    const voiceMask = this.props.voiceMask;
    e = e.nativeEvent || {};
    if (e.altKey || e.shiftKey || e.metaKey) {
      // Behaves like Photoshop (toggling layer visibility with alt+click)
      if (voiceMask.every((enabled, i) => (i === index) === enabled)) {
        // Alt-click on a single enabled channel unmutes everything
        voiceMask.fill(true);
      } else {
        // Solo the channel
        voiceMask.fill(false);
        voiceMask[index] = true;
      }
    } else {
      // Regular toggle behavior
      voiceMask[index] = !voiceMask[index];
    }
    this.props.handleSetVoiceMask(voiceMask);
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
        <div className="PlayerParams-voiceList">
          {[...Array(this.props.numVoices)].map((_, i) => {
            return (
              <label className='App-voice-label inline' key={i}>
                <input
                  title='Alt+click to solo. Alt+click again to unmute all.'
                  type='checkbox'
                  onChange={(e) => this.handleVoiceToggle(e, i)}
                  checked={this.props.voiceMask[i]}/>
                {this.props.voiceNames[i]}
              </label>
            )
          })}
        </div>

        {false && this.props.paramDefs.map(param => {
          const value = this.props.getParameter(param.id);
          const dependsOn = param.dependsOn;
          if (dependsOn && this.props.getParameter(dependsOn.param) !== dependsOn.value) {
              return null;
          }
          switch (param.type) {
            case 'enum':
              return (
                <label key={param.id} title={param.hint} className="PlayerParams-label">
                  {param.label}:{' '}
                  <select
                    onChange={(e) => {
                      this.props.setParameter(param.id, e.target.value);
                      this.forceUpdate();
                    }}
                    value={value}>
                    {param.options.map(optgroup =>
                      <optgroup key={optgroup.label} label={optgroup.label}>
                        {optgroup.items.map(option =>
                          <option key={option.value} value={option.value}>{option.label}</option>
                        )}
                      </optgroup>
                    )}
                  </select>
                </label>
              );
            case 'number':
              return (
                <label key={param.id} title={param.hint} className="PlayerParams-label">
                  {param.label}:{' '}
                  <input type='range'
                         title={param.hint}
                         min={param.min} max={param.max} step={param.step}
                         onChange={(e) => {
                           this.props.setParameter(param.id, e.target.value);
                           this.forceUpdate();
                         }}
                         value={value}>
                  </input>{' '}
                  {value !== undefined && param.step >= 1 ? value : value.toFixed(2)}
                </label>
              );
            case 'toggle':
              return (
                <label key={param.id} title={param.hint} className="PlayerParams-label">
                  <input type='checkbox'
                         onChange={(e) => {
                           this.props.setParameter(param.id, e.target.checked);
                           this.forceUpdate();
                         }}
                         checked={value}/>
                  {param.label}
                </label>
              );
            case 'button':
              return (
                <button key={param.id} title={param.hint} className="box-button" onClick={() => {
                   this.props.setParameter(param.id, true);
                }}>
                  {param.label}
                </button>
              );
            default:
              return null;
          }
        })}
      </div>
    );
  }
}
