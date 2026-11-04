import React from 'react';
import autoBindReact from 'auto-bind/react';
import { CHANNEL_COLORS } from '../piano-roll/config';

export default class PlayerParams extends React.PureComponent {
  constructor(props) {
    super(props);
    autoBindReact(this);
  }

  handleVoiceToggle(e, index) {
    const { onVoiceMaskChange, voiceGroups } = this.props;
    if (!onVoiceMaskChange) return;

    const voiceMask = [...this.props.voiceMask];
    e = e.nativeEvent || e || {};
    if (e.altKey || e.shiftKey || e.metaKey) {
      const allActiveVoiceIndices = voiceGroups && voiceGroups.length > 0
        ? voiceGroups.flatMap(group => group.voices.map(voice => voice.idx))
        : voiceMask.map((_, i) => i);

      const isCurrentlySoloed = allActiveVoiceIndices.every(idx =>
        (idx === index) ? Boolean(voiceMask[idx]) : !voiceMask[idx]
      );

      if (isCurrentlySoloed) {
        allActiveVoiceIndices.forEach(idx => {
          voiceMask[idx] = true;
        });
      } else {
        allActiveVoiceIndices.forEach(idx => {
          voiceMask[idx] = (idx === index);
        });
      }
    } else {
      voiceMask[index] = !voiceMask[index];
    }
    onVoiceMaskChange(voiceMask);
  }

  handleVoiceGroupToggle(e, group) {
    const { onVoiceMaskChange, voiceGroups } = this.props;
    if (!onVoiceMaskChange) return;

    const voiceMask = [...this.props.voiceMask];
    const event = e.nativeEvent || e || {};

    if (event.altKey || event.shiftKey || event.metaKey) {
      const allActiveVoiceIndices = voiceGroups && voiceGroups.length > 0
        ? voiceGroups.flatMap(g => g.voices.map(v => v.idx))
        : group.voices.map(v => v.idx);
      const groupVoiceIndices = new Set(group.voices.map(v => v.idx));

      // Check if this group is already the only active unmuted group (i.e. already soloed)
      const isCurrentlySoloed = allActiveVoiceIndices.every(idx =>
        groupVoiceIndices.has(idx) ? Boolean(voiceMask[idx]) : !voiceMask[idx]
      );

      if (isCurrentlySoloed) {
        // Unmute all active voices across all groups
        allActiveVoiceIndices.forEach(idx => {
          voiceMask[idx] = true;
        });
      } else {
        // Solo this group: enable all voices in this group, mute all voices outside this group
        allActiveVoiceIndices.forEach(idx => {
          voiceMask[idx] = groupVoiceIndices.has(idx);
        });
      }
    } else {
      // New state should be the opposite of current average voice state
      const numEnabled = group.voices.reduce((acc, cur) => acc + (voiceMask[cur.idx] ? 1 : 0), 0);
      const newState = numEnabled < (group.voices.length / 2);
      group.voices.forEach(voice => {
        voiceMask[voice.idx] = newState;
      });
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
      voiceMask,
    } = this.props;

    const { isPinned } = this;
    const isMidi = playerKey === 'midi';

    const groups = (voiceGroups && voiceGroups.length > 0)
      ? voiceGroups
      : (voiceMask && voiceMask.length > 0
          ? [{
              name: isMidi ? 'Channels' : 'Voices',
              voices: voiceMask.map((_, i) => ({
                idx: i,
                name: `Voice ${i + 1}`,
                color: isMidi ? CHANNEL_COLORS[i % CHANNEL_COLORS.length] : undefined,
              })).filter(v => !isMidi || (v.name && v.name.trim() !== '--'))
            }]
          : []);

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
        {groups.map((group, i) => {
          return (
            <span className='PlayerParams-param PlayerParams-group' key={group.name + i}>
              <label
                className="PlayerParams-group-title"
                title="Click to toggle group. Alt+click to solo."
                onClick={(e) => this.handleVoiceGroupToggle(e, group)}
              >
                {group.icon && <span className='inline-icon dim-icon icon-chip'/>}{' '}
                {group.name}:
              </label>
              <div className="PlayerParams-voiceList">
                {group.voices.map((voice) => {
                  const color = voice.color || (isMidi ? CHANNEL_COLORS[voice.idx % CHANNEL_COLORS.length] : null);
                  const displayName = voice.name ? voice.name.trim() : '';
                  return (
                    <div key={voice.idx} className='App-voice-label' title={displayName}>
                      <input
                        title='Alt+click to solo. Alt+click again to unmute all.'
                        type='checkbox'
                        id={'v_' + voice.idx}
                        onChange={(e) => this.handleVoiceToggle(e, voice.idx)}
                        checked={!!voiceMask[voice.idx]}
                      />
                      <label htmlFor={'v_' + voice.idx}>
                        {color && <span className="PlayerParams-color-chip" style={{ color }}>■</span>}
                        {displayName}
                      </label>
                    </div>
                  );
                })}
              </div>
            </span>
          );
        })}

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
                         onDoubleClick={this.getResetHandler(param.id, param.defaultValue)}
                         onContextMenu={this.getResetHandler(param.id, param.defaultValue)}
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
                         checked={!!value}/>
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
