import React, { memo, useCallback, useContext } from "react";
import FavoriteButton from "./FavoriteButton";
import { UserContext } from "./UserProvider";

export default memo(Favorites);
function Favorites(props) {
  const {
    currContext,
    currIdx,
    onSongClick,
    handleShufflePlay,
  } = props;

  const {
    user,
    loadingUser,
    faves: favorites,
    handleLogin,
    handleToggleFavorite: toggleFavorite,
  } = useContext(UserContext);

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
