import React, { useCallback } from "react";
import FavoriteButton from "./FavoriteButton";

export default function Favorites(props) {
  const {
    favorites,
    currContext,
    currIdx,
    onSongClick,
    toggleFavorite,
    user,
    loadingUser,
    handleLogin,
    handleShufflePlay,
  } = props;

  const handleShufflePlayFavorites = useCallback(() => {
    handleShufflePlay('favorites');
  }, [handleShufflePlay]);

  // Scroll Into View
  // ----------------
  // const playingRowRef = useRef(null);
  //
  // useEffect(() => {
  //   if (playingRowRef.current) {
  //     playingRowRef.current.scrollIntoViewIfNeeded();
  //   }
  // });

  return (
    loadingUser ?
      <p>Loading user data...</p>
      :
      <div>
        <h3 className="Browse-topRow">
          Favorite Songs ({favorites.length})
          {favorites.length > 1 &&
            <button
              className="box-button"
              title={`Shuffle all ${favorites.length} favorites`}
              onClick={handleShufflePlayFavorites}>
              Shuffle Play
            </button>}
        </h3>
        {user ?
          favorites.length > 0 ?
            <div>
              {
                favorites.map((href, i) => {
                  const title = decodeURIComponent(href.split('/').pop());
                  const isPlaying = currContext === favorites && currIdx === i;
                  return (
                    <div className={isPlaying ? 'Song-now-playing' : ''}
                      // Scroll Into View
                      // ref={isPlaying ? playingRowRef : null}
                         key={i}>
                      <FavoriteButton isFavorite={true}
                                      toggleFavorite={toggleFavorite}
                                      href={href}/>
                      <a onClick={onSongClick(href, favorites, i)} href={href}>{title}</a>
                    </div>
                  );
                })
              }
            </div>
            :
            <div>
              You don't have any favorites yet.<br/>
              Click the &#003; heart icon next to any song to save a favorite.
            </div>
          :
          <span>
              You must <a href="#" onClick={handleLogin}>
              login or signup</a> to save favorites.
            </span>
        }
      </div>
  );
}
