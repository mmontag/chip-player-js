import React, { memo, useCallback, useContext } from 'react';
import PlayerParams from './PlayerParams';
import { UserContext } from "./UserProvider";

const themes = [
  {
    value: 'msdos',
    label: 'MS-DOS',
  },
  {
    value: 'winamp',
    label: 'Winamp',
  }
];

const silenceOptions = [
  { value: -1, label: 'None' },
  { value: 0, label: '0 seconds (Gapless)' },
  { value: 1, label: '1 second' },
  { value: 2, label: '2 seconds' },
  { value: 3, label: '3 seconds' },
  { value: 5, label: '5 seconds' },
];

function Settings(props) {
  const {
    ejected,
    tempo,
    voiceMask,
    voiceNames,
    voiceGroups,
    onVoiceMaskChange,
    onTempoChange,
    paramDefs,
    paramValues,
    onParamChange,
    onPinParam,
    persistedSettings,
    sequencer,
  } = props;

  const { settings, updateSettings } = useContext(UserContext);
  const theme = settings?.theme;
  const silenceDuration = settings?.silenceDuration ?? -1;

  const handleThemeChange = useCallback((e) => {
    updateSettings({ theme: e.target.value });
  }, [updateSettings]);

  const handleSilenceDurationChange = useCallback((e) => {
    updateSettings({ silenceDuration: Number(e.target.value) });
  }, [updateSettings]);

  return (
    <div className='Settings'>
      <h3>{sequencer?.getPlayer()?.name || 'Player'} Settings</h3>
      {sequencer?.getPlayer() ?
        <PlayerParams
          ejected={ejected}
          tempo={tempo}
          voiceMask={voiceMask}
          voiceNames={voiceNames}
          voiceGroups={voiceGroups}
          onTempoChange={onTempoChange}
          onVoiceMaskChange={onVoiceMaskChange}
          paramDefs={paramDefs}
          paramValues={paramValues}
          onParamChange={onParamChange}
          onPinParam={onPinParam}
          persistedSettings={persistedSettings}
          playerKey={sequencer?.getPlayer()?.playerKey}
        />
        :
        <div>(No active player)</div>}
      <h3>Global Settings</h3>
      <span className='PlayerParams-param'>
        <label htmlFor='theme' className="PlayerParams-label-wide">
          Theme:{' '}
        </label>
        <select
          id='theme'
          onChange={handleThemeChange}
          value={theme}>
          {themes.map(option =>
            <option key={option.value} value={option.value}>{option.label}</option>
          )}
        </select>
      </span>
      <span className='PlayerParams-param'>
        <label htmlFor='silenceDuration' title='Silence between songs' className="PlayerParams-label-wide">
          Insert Silence:{' '}
        </label>
        <select
          id='silenceDuration'
          onChange={handleSilenceDurationChange}
          value={silenceDuration}>
          {silenceOptions.map(option =>
            <option key={option.value} value={option.value}>{option.label}</option>
          )}
        </select>
      </span>
    </div>
  );
}

export default memo(Settings);
