import React, { memo, useCallback, useContext } from 'react';
import FavoriteButton from './FavoriteButton';
import { UserContext } from './UserProvider';
import VirtualizedList from './VirtualizedList';

const FavoriteRow = (props) => {
  const {
    item, onPlay
  } = props;
  const { href, mtime } = item;
  const date = new Date(mtime * 1000).toISOString().split('T')[0];
  const name = decodeURIComponent(href.split('/').pop());

  return (
    <>
      <div className="BrowseList-colName">
        <FavoriteButton href={href}/>
        <a onClick={onPlay} href={href}>{name}</a>
      </div>
      <div className="BrowseList-colMtime">
        {date}
      </div>
    </>
  )
};

export default memo(Favorites);

function Favorites(props) {
  const {
    scrollContainerRef,
    currContext,
    currIdx,
    onSongClick,
    handleShufflePlay,
    listRef,
  } = props;

  const {
    user,
    loadingUser,
    faves,
    favesContext,
    handleLogin,
  } = useContext(UserContext);

  const handleShufflePlayFavorites = useCallback(() => {
    handleShufflePlay('favorites');
  }, [handleShufflePlay]);

  return (
    loadingUser ?
      <p>Loading user data...</p>
      :
      user ?
        <VirtualizedList
          {...{
            scrollContainerRef,
            currContext,
            currIdx,
            onSongClick,
            listRef,
            itemList: faves,
            songContext: favesContext,
            rowRenderer: FavoriteRow,
          }}
        >
          <h3 className="Browse-topRow">
            Favorite Songs ({faves.length})
            {faves.length > 1 &&
              <button
                className="box-button"
                title={`Shuffle all ${faves.length} favorites`}
                onClick={handleShufflePlayFavorites}>
                Shuffle Play
              </button>}
          </h3>
        </VirtualizedList>
        :
        faves.length > 0 && scrollContainerRef.current ?
          <div>
            You don't have any favorites yet.<br/>
            Click the &#003; heart icon next to any song to save a favorite.
          </div>
          :
          <span>
            You must <a href="#" onClick={handleLogin}>
            login or signup</a> to save favorites.
          </span>
  );
}
