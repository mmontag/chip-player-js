import React, { memo, useCallback, useContext } from 'react';
import shuffleImage from '../images/shuffle.png';
import downloadImage from '../images/download.png';
import copyImage from '../images/copy.png';
import repeatImage from '../images/repeat.png';
import TimeSlider from './TimeSlider';
import VolumeSlider from './VolumeSlider';
import FavoriteButton from './FavoriteButton';
import { pathToLinks } from '../util';
import { REPEAT_LABELS, SHUFFLE_LABELS } from '../Sequencer';
import { UserContext } from './UserProvider';

export default memo(AppFooter);
function AppFooter(props) {
  const {
    // this.state.
    currentSongDurationMs,
    currentSongNumSubtunes,
    currentSongSubtune,
    ejected,
    imageUrl,
    infoTexts,
    paused,
    repeat,
    shuffle,
    songUrl,
    subtitle,
    title,
    volume,

    // this.
    getCurrentSongLink,
    handleCopyLink,
    handleCycleRepeat,
    handleCycleShuffle,
    handleTimeSliderChange,
    handleVolumeChange,
    nextSong,
    nextSubtune,
    prevSong,
    prevSubtune,
    sequencer,
    toggleInfo,
    togglePause,
  } = props;

  const {
    faves,
    handleToggleFavorite,
  } = useContext(UserContext);

  const pathLinks = pathToLinks(songUrl);
  const subtuneText = `Tune ${currentSongSubtune + 1} of ${currentSongNumSubtunes}`;

  const handleToggleInfo = useCallback((e) => {
    e.preventDefault();
    toggleInfo();
  }, [toggleInfo]);

  const handleCopySongLink = useCallback((e) => {
    e.preventDefault();
    handleCopyLink(getCurrentSongLink());
  }, [getCurrentSongLink, handleCopyLink]);

  const handleCopySubtuneLink = useCallback((e) => {
    e.preventDefault();
    handleCopyLink(getCurrentSongLink(/*withSubtune=*/true));
  }, [getCurrentSongLink, handleCopyLink]);

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
                  <img alt="Copy link" src={copyImage} className="inline-icon"/>
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
              <img alt="Repeat" src={repeatImage} className="inline-icon"/>
              {REPEAT_LABELS[repeat]}
            </button>
            {' '}
            <button title="Toggle shuffle mode" className="box-button" onClick={handleCycleShuffle}>
              <img alt="Shuffle" src={shuffleImage} className="inline-icon"/>
              {SHUFFLE_LABELS[shuffle]}
            </button>
            {' '}
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
                      <img alt="Copy link" src={copyImage} className="inline-icon"/>
                    </a>
                    {' '}
                    <a style={{ color: 'var(--neutral4)' }} href={songUrl}>
                      <img alt="Download" src={downloadImage} className="inline-icon"/>
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
      {imageUrl && <img alt="Cover art" className="AppFooter-art" src={imageUrl}/>}
    </div>
  );
}
