import React, { useContext, useCallback, memo } from "react";
import { UserContext } from './UserProvider';
import { ToastContext } from './ToastProvider';
import { ToastLevels } from './Toast';

const FavoriteButton = ({ item }) => {
  const {
    user,
    faves,
    handleToggleFavorite: toggleFavorite,
    handleLogin,
  } = useContext(UserContext);
  const { enqueueToast, clearCurrentToast } = useContext(ToastContext);

  const { path, songId, subtune = 0, title } = item;

  const handleClick = useCallback((e) => {
    e.preventDefault();
    e.stopPropagation();
    if (!user) {
      enqueueToast?.(
        <span>
          <a
            href="#"
            onClick={(e) => {
              e.preventDefault();
              clearCurrentToast?.();
              handleLogin();
            }}
          >
            Login/Sign Up
          </a>{' '}
          to Save Favorites
        </span>,
        ToastLevels.INFO
      );
      return;
    }
    toggleFavorite(path, subtune, songId, title);
  }, [clearCurrentToast, enqueueToast, handleLogin, toggleFavorite, path, subtune, songId, title, user]);

  const isFavorite = faves.find(fave => fave.path === path && (fave.subtune || 0) === subtune);
  const className = `FavoriteButton ${isFavorite ? 'isFavorite' : ''}`;

  return (
    <button onClick={handleClick} className={className} tabIndex="-1">
      &hearts;&#xFE0E;{/* Variation Selector-15 (U+FE0E) suppresses emoji */}
    </button>
  );
};

export default memo(FavoriteButton);
