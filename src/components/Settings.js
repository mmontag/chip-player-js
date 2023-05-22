import React, { PureComponent } from 'react';
import PlayerParams from './PlayerParams';

export default class Settings extends PureComponent {
  render() {
    const {
      ejected,
      tempo,
      currentSongNumVoices,
      voiceMask,
      voiceNames,
      handleSetVoiceMask,
      handleTempoChange,
      sequencer,
    } = this.props;
    return (
      <div className='Settings'>
        <h3>{sequencer?.getPlayer()?.name || 'Player'} Settings</h3>
        {sequencer?.getPlayer() ?
          <PlayerParams
            ejected={ejected}
            tempo={tempo}
            numVoices={currentSongNumVoices}
            voiceMask={voiceMask}
            voiceNames={voiceNames}
            handleTempoChange={handleTempoChange}
            handleSetVoiceMask={handleSetVoiceMask}
            getParameter={sequencer.getPlayer().getParameter}
            setParameter={sequencer.getPlayer().setParameter}
            paramDefs={sequencer.getPlayer().getParamDefs()}/>
          :
          <div>(No active player)</div>}
      </div>
    );
  }
}
