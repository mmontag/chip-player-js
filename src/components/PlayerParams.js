import React from 'react';
import autoBindReact from 'auto-bind/react';

export default class PlayerParams extends React.PureComponent {
  constructor(props) {
    super(props);
    autoBindReact(this);
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
    const { onVoiceMaskChange } = this.props;
    if (!onVoiceMaskChange) return;

    const voiceMask = [...this.props.voiceMask];
    e = e.nativeEvent || {};
    if (e.altKey || e.shiftKey || e.metaKey) {
      if (voiceMask.every((enabled, i) => (i === index) === enabled)) {
        voiceMask.fill(true);
      } else {
        voiceMask.fill(false);
        voiceMask[index] = true;
      }
    } else {
      voiceMask[index] = !voiceMask[index];
    }
    onVoiceMaskChange(voiceMask);
  }

  getResetHandler(id, value) {
    return (e) => {
      this.props.onParamChange(id, value);
      e.preventDefault();
      e.stopPropagation();
    };
  }

  resetTempo() {
    this.props.onTempoChange(1.0);
  }

  isPinned(persistedKey) {
    return this.props.persistedSettings?.hasOwnProperty(persistedKey);
  }

  render() {
    const {
      paramDefs,
      paramValues,
      onParamChange,
      onPinParam,
      playerKey,
      tempo,
      onTempoChange,
      ejected,
      voiceGroups,
      numVoices,
      voiceMask,
      voiceNames,
    } = this.props;

    const { isPinned } = this;

    return (
      <div className='PlayerParams'>
        <span className='PlayerParams-param PlayerParams-group'>
          <button
            className="IconButton"
            title={isPinned('tempo') ? 'Un-pin this parameter' : 'Pin this parameter (retains value between songs)'}
            onClick={() => onPinParam('tempo', tempo)}>
            <span className={`inline-icon ${isPinned('tempo') ? 'icon-pin-down' : 'icon-pin-up'}`}/>
          </button>
          <label htmlFor='tempo' className="PlayerParams-label">
            Speed:{' '}
          </label>
          <input
            id='tempo'
            disabled={ejected}
            type='range' value={tempo}
            min='0.3' max='2.0' step='0.05'
            onInput={onTempoChange}
            onChange={onTempoChange}
            onDoubleClick={this.resetTempo}
            onContextMenu={this.resetTempo}
          />
          {' '}
          {tempo.toFixed(2)}
        </span>
        {voiceGroups.length > 0 ?
          voiceGroups.map((voiceGroup, i) => {
            return (
              <span className='PlayerParams-param PlayerParams-group' key={voiceGroup.name}>
                  <label className="PlayerParams-group-title" title="Sound chip">
                    {voiceGroup.icon && <span className='inline-icon dim-icon icon-chip'/>}{' '}
                    {voiceGroup.name}:
                  </label>
                  <div className="PlayerParams-voiceList">
                    {voiceGroup.voices.map((voice, j) => (
                      <div key={voice.idx} className='App-voice-label'><input
                        title='Alt+click to solo. Alt+click again to unmute all.'
                        type='checkbox'
                        id={'v_'+i+j}
                        onChange={(e) => this.handleVoiceToggle(e, voice.idx)}
                        checked={voiceMask[voice.idx]}/>
                      <label htmlFor={'v_'+i+j}>
                        {voice.name}
                      </label></div>
                    ))}
                  </div>
              </span>
            )
          })
          :
          numVoices > 0 &&
          <span className='PlayerParams-param PlayerParams-group'>
            <label className="PlayerParams-group-title">
              Voices:
            </label>
            <div className="PlayerParams-voiceList">
              {[...Array(numVoices)].map((_, i) => {
                return (
                  <div key={i} className='App-voice-label'><input
                    title='Alt+click to solo. Alt+click again to unmute all.'
                    type='checkbox'
                    id={'v_'+i}
                    onChange={(e) => this.handleVoiceToggle(e, i)}
                    checked={voiceMask[i]}/>
                  <label htmlFor={'v_'+i}>
                    {voiceNames[i]}
                  </label></div>
                )
              })}
            </div>
          </span>
        }

        {paramDefs && paramValues && paramDefs.map(param => {
          const value = paramValues[param.id];
          const dependsOn = param.dependsOn;
          if (dependsOn && paramValues[dependsOn.param] !== dependsOn.value) {
            return null;
          }

          const persistedKey = `${playerKey}.${param.id}`;
          const pinButton = (
            <button
              className="IconButton"
              title={isPinned(persistedKey) ? 'Un-pin this parameter' : 'Pin this parameter (retains value between songs)'}
              onClick={() => onPinParam(persistedKey, value)}>
              <span className={`inline-icon ${isPinned(persistedKey) ? 'icon-pin-down' : 'icon-pin-up'}`}/>
            </button>
          );

          switch (param.type) {
            case 'enum':
              return (
                <span key={param.id} className='PlayerParams-param'>
                  {pinButton}
                  <label htmlFor={param.id} title={param.hint} className="PlayerParams-label">
                  {param.label}:{' '}
                </label>
                  <select
                    id={param.id}
                    onChange={(e) => {
                      // TODO: make this explicit in the param def
                      const intVal = Number(e.target.value);
                      const value = isNaN(intVal) ? e.target.value : intVal;
                      onParamChange(param.id, value)
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
                </span>
              );
            case 'number':
              return (
                <span key={param.id} className='PlayerParams-param'>
                  {pinButton}
                  <label htmlFor={param.id} title={param.hint} className="PlayerParams-label">
                    {param.label}:{' '}
                  </label>
                  <input id={param.id}
                         type='range'
                         title={param.hint}
                         min={param.min} max={param.max} step={param.step}
                         onChange={(e) => onParamChange(param.id, parseFloat(e.target.value))}
                         onDoubleClick={this.getResetHandler(param.id, param.default)}
                         onContextMenu={this.getResetHandler(param.id, param.default)}
                         value={value}>
                  </input>{' '}
                  {value !== undefined && param.step >= 1 ? value : value.toFixed(2)}
                </span>
              );
            case 'toggle':
              return (
                <span key={param.id} className='PlayerParams-param'>
                  {pinButton}
                  <input type='checkbox'
                         id={param.id}
                         onChange={(e) => onParamChange(param.id, e.target.checked)}
                         checked={value}/>
                  <label htmlFor={param.id} title={param.hint}>
                    {param.label}
                  </label>
                </span>
              );
            case 'button':
              return (
                <button key={param.id} title={param.hint} className="box-button" onClick={() => onParamChange(param.id, true)}>
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
