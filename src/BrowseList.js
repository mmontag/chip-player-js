import React, {Fragment} from 'react';
import bytes from 'bytes';
import {CATALOG_PREFIX} from './config';
import DirectoryLink from './DirectoryLink';
import FavoriteButton from './FavoriteButton';

const dirToken = <Fragment>&nbsp;&nbsp;&lt;DIR&gt;&nbsp;&nbsp;</Fragment>;

export default function BrowseList({ virtual, ...props }) {
  const {
    currContext,
    currIdx,
    favorites,
    toggleFavorite,
    handleSongClick,
    browsePath,
    contexts,
  } = props;
  const parentPath = browsePath.substr(0, browsePath.lastIndexOf('/'));

  return (
    <div style={{position: 'relative'}}>
      <div key={browsePath} style={css.row}>
        <div style={css.colName}>
          <DirectoryLink to={'/browse/' + parentPath}>..</DirectoryLink>
        </div>
        <div style={css.colDir}>{dirToken}</div>
        <div style={css.colSize}/>
      </div>
      <div style={virtual.style}>
        {virtual.items.map(item => {
          // XXX: Escape immediately: the escaped URL is considered canonical.
          //      The URL must be decoded for display from here on out.
          const path = item.path.replace('%', '%25').replace('#', '%23').replace(/^\//, '');
          const name = item.path.split('/').pop();
          const isPlaying = currContext === contexts[browsePath] && currIdx === item.idx;
          if (item.type === 'directory') {
            return (
              <div key={name} style={css.row}>
                <div style={css.colName}>
                  <DirectoryLink to={'/browse/' + path}>{name}</DirectoryLink>
                </div>
                <div style={css.colDir}>
                  {dirToken}
                </div>
                <div style={css.colSize}>
                  {bytes(item.size, {unitSeparator: ' '})}
                </div>
              </div>
            );
          } else {
            const href = CATALOG_PREFIX + path;
            return (
              <div key={name} style={css.row}>
                <div style={css.colName}>
                  {favorites &&
                  <FavoriteButton favorites={favorites}
                                  href={href}
                                  toggleFavorite={toggleFavorite}/>}
                  <a className={isPlaying ? 'Song-now-playing' : ''}
                     onClick={(e) => handleSongClick(href, contexts[browsePath], item.idx)(e)}
                     href='#'>
                    {name}
                  </a>
                </div>
                <div style={css.colSize}>
                  {bytes(item.size, {unitSeparator: ' '})}
                </div>
              </div>
            );
          }
        })}
      </div>
    </div>
  );
};

const css = {
  row: {
    display: 'flex',
  },
  colName: {
    overflowX: 'hidden',
    whiteSpace: 'nowrap',
    textOverflow: 'ellipsis',
    flexGrow: 1,
  },
  colDir: {
    flexShrink: 0,
  },
  colSize: {
    textAlign: 'right',
    textTransform: 'uppercase',
    whiteSpace: 'nowrap',
    flexShrink: 0,
    minWidth: '80px',
  },
};
