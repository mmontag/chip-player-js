import React, { memo, useCallback } from "react";
import { FORMATS } from '../config';
import trash from '../images/trash.png';
import bytes from 'bytes';

const formatList = FORMATS.filter(f => f !== 'miniusf').map(f => `.${f}`);
const splitPoint = Math.floor(formatList.length / 2);
const formatsLine1 = formatList.slice(0, splitPoint).join(' ');
const formatsLine2 = formatList.slice(splitPoint).join(' ');

export default memo(LocalFiles);

function LocalFiles(props) {
  const {
    listing,
    currContext,
    playContext,
    currIdx,
    onSongClick,
    onDelete,
  } = props;

  const handleShufflePlayLocalFiles = useCallback(() => {
    handleShufflePlay('local');
  }, [handleShufflePlay]);

  const handleDelete = useCallback((event) => {
    const href = event.currentTarget.dataset.href;
    if (event.shiftKey) {
      onDelete(href);
    } else {
      if (window.confirm('Are you sure you want to remove this file from browser storage?\n(Shift-click to remove without confirmation.)'))
        onDelete(href);
    }
  }, [onDelete]);

  return (
    <div>
      <h3 className="Browse-topRow">
        Local Files ({listing.length})
      </h3>
      {listing.length > 0 ?
        <div>
          {
            listing.map((item, i) => {
              const href = item.path;
              const title = decodeURIComponent(href.split('/').pop());
              const isPlaying = currContext === playContext && currIdx === i;
              return (
                <div key={title} className={isPlaying ? 'Song-now-playing BrowseList-row' : 'BrowseList-row'}>
                  <button className='Trash-button' onClick={handleDelete} data-href={href}>
                    <img alt='trash' className='fileIcon' src={trash}/>
                  </button>
                  <div className="BrowseList-colName">
                    <a onClick={onSongClick(href, playContext, i)} href={href}>{title}</a>
                  </div>
                  <div className="BrowseList-colMtime">
                    {item.mtime}
                  </div>
                  <div className="BrowseList-colSize">
                    {bytes(item.size, { unitSeparator: ' ' })}
                  </div>
                </div>
              );
            })
          }
        </div>
        :
        <div>
          <p>
            You don't have any Local Files yet.
          </p>
          <p>
            Files dropped here will be persisted in your local browser.<br/>
            (Keep a copy - your browser might lose this data.)<br/>
            Local Files can't be added to Favorites (well, not yet).
          </p>
          <p>
            Supported formats:
          </p>
          <p>
            {formatsLine1}<br/>
            {formatsLine2}<br/>
          </p>
        </div>
      }
    </div>
  );
}
