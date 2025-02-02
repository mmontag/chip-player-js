import React, { memo, useCallback, useContext } from 'react';
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

  const playPauseTitle = paused ? 'Play' : 'Pause';
  const playPauseClass = paused ? 'icon-play' : 'icon-pause';

  return (
    <div className="AppFooter">
      <div className="AppFooter-main">
        <div className="AppFooter-main-inner">
          <button onClick={prevSong}
                  title="Previous"
                  className="box-button"
                  disabled={ejected}>
            <span className="inline-icon icon-prev"/>
          </button>
          {' '}
          <button onClick={togglePause}
                  title={playPauseTitle}
                  className="box-button"
                  disabled={ejected}>
            <span className={`inline-icon ${playPauseClass}`}/>
          </button>
          {' '}
          <button onClick={nextSong}
                  title="Next"
                  className="box-button"
                  disabled={ejected}>
            <span className="inline-icon icon-next"/>
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
                  <span className="inline-icon icon-copy"/>
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
              <span className="inline-icon icon-repeat"/>
              {REPEAT_LABELS[repeat]}
            </button>
            {' '}
            <button title="Toggle shuffle mode" className="box-button" onClick={handleCycleShuffle}>
              <span className="inline-icon icon-shuffle"/>
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
                       onClick={handleCopySongLink}>
                      {title}{' '}
                      <span className="inline-icon icon-copy"/>
                    </a>
                    {' '}
                    <a style={{ color: 'var(--neutral4)' }} href={songUrl}
                       title="Download song">
                      <span className="inline-icon icon-download"/>
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
