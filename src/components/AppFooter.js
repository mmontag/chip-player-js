import React, { memo, useCallback } from 'react';
import diceImage from '../images/dice.png';
import downloadImage from '../images/download.png';
import linkImage from '../images/link.png';
import repeatImage from '../images/repeat.png';
import TimeSlider from './TimeSlider';
import VolumeSlider from './VolumeSlider';
import FavoriteButton from './FavoriteButton';
import PlayerParams from './PlayerParams';
import { pathToLinks } from '../util';
import { REPEAT_LABELS, SHUFFLE_LABELS } from '../Sequencer';

export default memo(AppFooter);
function AppFooter(props) {
  const {
    // this.state.
    currentSongDurationMs,
    currentSongNumSubtunes,
    currentSongNumVoices,
    currentSongSubtune,
    ejected,
    faves,
    imageUrl,
    infoTexts,
    paused,
    repeat,
    shuffle,
    showPlayerSettings,
    songUrl,
    subtitle,
    tempo,
    title,
    voiceNames,
    voiceMask,
    volume,

    // this.
    getCurrentSongLink,
    handleCopyLink,
    handleCycleRepeat,
    handleCycleShuffle,
    handleSetVoiceMask,
    handleTempoChange,
    handleTimeSliderChange,
    handleToggleFavorite,
    handleVolumeChange,
    nextSong,
    nextSubtune,
    prevSong,
    prevSubtune,
    sequencer,
    toggleInfo,
    togglePause,
    toggleSettings,
  } = props;

  const pathLinks = pathToLinks(songUrl);
  const subtuneText = `Tune ${currentSongSubtune + 1} of ${currentSongNumSubtunes}`;

  const handleToggleInfo = useCallback((e) => {
    e.preventDefault();
    toggleInfo();
  }, [toggleInfo]);

  const handleCopySongLink = useCallback((e) => {
    e.preventDefault();
    handleCopyLink(getCurrentSongLink());
  });

  const handleCopySubtuneLink = useCallback((e) => {
    e.preventDefault();
    handleCopyLink(getCurrentSongLink(/*withSubtune=*/true));
  });

  return (
    <div className="AppFooter">
      <div className="AppFooter-main">
        <div className="AppFooter-main-inner">
          <button onClick={prevSong}
                  title="Previous"
                  className="box-button"
                  disabled={ejected}>
            ⏮
          </button>
          {' '}
          <button onClick={togglePause}
                  title={paused ? 'Resume' : 'Pause'}
                  className="box-button"
                  disabled={ejected}>
            {paused ? ' ► ' : ' ⏸ '}
          </button>
          {' '}
          <button onClick={nextSong}
                  title="Next"
                  className="box-button"
                  disabled={ejected}>
            ⏭
          </button>
          {' '}
          {currentSongNumSubtunes > 1 &&
            <span style={{ whiteSpace: 'nowrap' }}>
              { songUrl ?
                <a style={{ color: 'var(--neutral4)' }} href={getCurrentSongLink(/*subtune=*/true)}
                   title="Copy subtune link to clipboard"
                   onClick={handleCopySubtuneLink}>
                  {subtuneText}
                  {' '}
                  <img alt="Copy link" src={linkImage} style={{ verticalAlign: 'bottom' }}/>
                </a>
                :
                subtuneText
              }
              {' '}
              <button
                className="box-button"
                disabled={ejected}
                onClick={prevSubtune}>&lt;
              </button>
              {' '}
              <button
                className="box-button"
                disabled={ejected}
                onClick={nextSubtune}>&gt;
              </button>
            </span>}
          <span className="AppFooter-more-buttons">
              <button title="Cycle Repeat (repeat off, repeat all songs in the context, or repeat one song)"
                      className="box-button" onClick={handleCycleRepeat}>
                <img alt="Repeat" src={repeatImage} style={{ verticalAlign: 'bottom' }}/>
                {REPEAT_LABELS[repeat]}
              </button>
            {' '}
            <button title="Toggle shuffle mode" className="box-button" onClick={handleCycleShuffle}>
                <img alt="Roll the dice" src={diceImage} style={{ verticalAlign: 'bottom' }}/>
              {SHUFFLE_LABELS[shuffle]}
              </button>
            {' '}
            {(!showPlayerSettings && false) && // TODO: Remove footer settings
              <button className="box-button" onClick={toggleSettings}>
                Settings &gt;
              </button>}
            </span>
          <div style={{ display: 'flex', flexDirection: 'row' }}>
            <TimeSlider
              paused={paused}
              currentSongDurationMs={currentSongDurationMs}
              getCurrentPositionMs={() => {
                // TODO: reevaluate this approach
                if (sequencer && sequencer.getPlayer()) {
                  return sequencer.getPlayer().getPositionMs();
                }
                return 0;
              }}
              onChange={handleTimeSliderChange}/>
            <VolumeSlider
              onChange={(e) => {
                handleVolumeChange(e.target.value);
              }}
              handleReset={(e) => {
                handleVolumeChange(100);
                e.preventDefault();
                e.stopPropagation();
              }}
              title="Double-click or right-click to reset to 100%."
              value={volume}/>
          </div>
          {!ejected &&
            <div className="SongDetails">
              {faves && songUrl &&
                <div style={{ float: 'left', marginBottom: '58px' }}>
                  <FavoriteButton isFavorite={faves.includes(songUrl)}
                                  toggleFavorite={handleToggleFavorite}
                                  href={songUrl}/>
                </div>}
              <div className="SongDetails-title">
                { songUrl ?
                  <>
                    <a style={{ color: 'var(--neutral4)' }} href={getCurrentSongLink()}
                       title="Copy song link to clipboard"
                       onClick={handleCopySongLink}
                    >
                      {title}{' '}
                      <img alt="Copy link" src={linkImage} style={{ verticalAlign: 'bottom' }}/>
                    </a>
                    {' '}
                    <a style={{ color: 'var(--neutral4)' }} href={songUrl}>
                      <img alt="Download" src={downloadImage} style={{ verticalAlign: 'bottom' }}/>
                    </a>
                  </>
                  :
                  title
                }
                {' '}
                {infoTexts.length > 0 &&
                  <a onClick={handleToggleInfo} href='#'>
                    тхт
                  </a>
                }
              </div>
              <div className="SongDetails-subtitle">{subtitle}</div>
              <div className="SongDetails-filepath">{pathLinks}</div>
            </div>}
        </div>
      </div>
      {(showPlayerSettings && false) && // TODO: Remove footer settings
        <div className="AppFooter-settings">
          <div style={{
            display: 'flex',
            justifyContent: 'space-between',
            alignItems: 'start',
            marginBottom: '19px'
          }}>
            <h3 style={{ margin: '0 8px 0 0' }}>Player Settings</h3>
            <button className='box-button' onClick={toggleSettings}>
              Close
            </button>
          </div>
          {sequencer.getPlayer() ?
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
        </div>}
      {imageUrl &&
        <img alt="Cover art" className="AppFooter-art" src={imageUrl}/>}
    </div>
  );
}
