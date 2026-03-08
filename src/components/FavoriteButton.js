import React, { useContext, useCallback, memo } from "react";
import { UserContext } from './UserProvider';

const FavoriteButton = ({ item }) => {
  const {
    user,
    faves,
    handleToggleFavorite: toggleFavorite,
  } = useContext(UserContext);

  const { path, songId } = item;

  const handleClick = useCallback((e) => {
    if (!user) {
      // TODO: prompt user "Login to save favorites" with ToastManager.
      return;
    }
    e.preventDefault();
    e.stopPropagation();
    toggleFavorite(path, songId);
  }, [toggleFavorite, path, songId, user]);

  const isFavorite = faves.find(fave => fave.path === path);
  const className = `FavoriteButton ${isFavorite ? 'isFavorite' : ''}`;

  return (
    <button onClick={handleClick} className={className} tabIndex="-1">
      &hearts;
    </button>
  );
};

export default memo(FavoriteButton);
