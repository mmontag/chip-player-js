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
    toggleFavorite(item);
  }, [clearCurrentToast, enqueueToast, handleLogin, toggleFavorite, item, user]);

  const isFavorite = faves.find(fave => fave.path === item?.path);
  const className = `FavoriteButton ${isFavorite ? 'isFavorite' : ''}`;

  return (
    <button onClick={handleClick} className={className} tabIndex="-1">
      &hearts;&#xFE0E;{/* Variation Selector-15 (U+FE0E) suppresses emoji */}
    </button>
  );
};

export default memo(FavoriteButton);
