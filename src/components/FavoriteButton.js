import React, { useContext, useCallback, memo } from "react";
import { UserContext } from './UserProvider';

const FavoriteButton = ({ item }) => {
  const {
    user,
    faves,
    handleToggleFavorite: toggleFavorite,
  } = useContext(UserContext);

  const { path, songId, subtune = 0 } = item;

  const handleClick = useCallback((e) => {
    if (!user) {
      // TODO: prompt user "Login to save favorites" with ToastManager.
      return;
    }
    e.preventDefault();
    e.stopPropagation();
    toggleFavorite(path, subtune, songId);
  }, [toggleFavorite, path, subtune, songId, user]);

  const isFavorite = faves.find(fave => fave.path === path && (fave.subtune || 0) === subtune);
  const className = `FavoriteButton ${isFavorite ? 'isFavorite' : ''}`;

  return (
    <button onClick={handleClick} className={className} tabIndex="-1">
      &hearts;&#xFE0E;{/* Variation Selector-15 (U+FE0E) suppresses emoji */}
    </button>
  );
};

export default memo(FavoriteButton);
