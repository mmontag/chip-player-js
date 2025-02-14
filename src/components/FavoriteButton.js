import React, { useContext, useCallback } from "react";
import { UserContext } from './UserProvider';

const FavoriteButton = ({ href }) => {
  const {
    user,
    faves,
    handleToggleFavorite: toggleFavorite,
  } = useContext(UserContext);

  const handleClick = useCallback((e) => {
    if (!user) {
      // TODO: prompt user with ToastManager.
      return;
    }
    e.preventDefault();
    e.stopPropagation();
    toggleFavorite(href);
  }, [toggleFavorite, href, user]);

  const isFavorite = faves.find(fave => fave.href === href);
  const className = `FavoriteButton ${isFavorite ? 'isFavorite' : ''}`;

  return (
    <button onClick={handleClick} className={className} tabIndex="-1">
      &hearts;
    </button>
  );
};

export default FavoriteButton;
