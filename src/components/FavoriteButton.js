import React, { useContext, useCallback } from "react";
import { UserContext } from './UserProvider';

const FavoriteButton = ({ href }) => {
  const {
    user,
    faves,
    handleToggleFavorite: toggleFavorite,
  } = useContext(UserContext);

  const handleClick = useCallback(() => {
    if (!user) {
      // TODO: prompt user with ToastManager.
      return;
    }

    toggleFavorite(href);
  }, [toggleFavorite, href, user]);

  const isFavorite = faves.find(fave => fave.href === href);
  const className = `Favorite-button ${isFavorite ? 'isFavorite' : ''}`;

  return (
    <button onClick={handleClick} className={className}>
      &hearts;
    </button>
  );
};

export default FavoriteButton;
