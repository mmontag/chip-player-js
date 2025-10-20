import React, { createContext, useCallback, useEffect, useMemo, useState } from 'react';

const UserContext = createContext({
  user: null,
  loadingUser: false,
  faves: [],
  favesContext: [],
  showPlayerSettings: false,
  handleLogin: () => {},
  handleLogout: () => {},
  handleToggleFavorite: () => {},
  settings: {},
  updateSettings: () => {},
  replaceSettings: () => {},
});

const DEFAULT_SETTINGS = {
  showPlayerSettings: false,
  theme: 'msdos',
};

const UserProvider = ({ children }) => {
  const [user, setUser] = useState(null);
  const [faves, setFaves] = useState(() => {
    try {
      const saved = window.localStorage.getItem('faves');
      return saved ? JSON.parse(saved) : [];
    } catch (e) {
      console.error('Could not load faves from localStorage', e);
      return [];
    }
  });
  const [settings, setSettings] = useState(() => {
    try {
      const saved = window.localStorage.getItem('settings');
      return saved ? JSON.parse(saved) : DEFAULT_SETTINGS;
    } catch (e) {
      console.error('Could not load settings from localStorage', e);
      return DEFAULT_SETTINGS;
    }
  });

  useEffect(() => {
    try {
      window.localStorage.setItem('faves', JSON.stringify(faves));
    } catch (e) {
      console.error('Could not save faves to localStorage', e);
    }
  }, [faves]);

  useEffect(() => {
    try {
      window.localStorage.setItem('settings', JSON.stringify(settings));
    } catch (e) {
      console.error('Could not save settings to localStorage', e);
    }
  }, [settings]);

  const handleToggleFavorite = useCallback((href) => {
    const existingIdx = faves.findIndex(f => f === href || f.href === href);
    if (existingIdx === -1) {
      const newFave = {
        href,
        mtime: Math.floor(Date.now() / 1000),
      };
      setFaves([...faves, newFave]);
    } else {
      setFaves(faves.filter((_, idx) => idx !== existingIdx));
    }
  }, [faves]);

  const updateSettings = useCallback((partialSettings) => {
    const newSettings = { ...settings, ...partialSettings };
    setSettings(newSettings);
  }, [settings]);

  const replaceSettings = useCallback((newSettings) => {
    setSettings(newSettings);
  }, []);

  const favesContext = useMemo(() => {
    return faves.map(fave => fave.href || fave);
  }, [faves]);

  return (
    <UserContext.Provider
      value={{
        user,
        loadingUser: false,
        faves,
        favesContext,
        handleLogin: () => setUser(null),
        handleLogout: () => setUser(null),
        handleToggleFavorite,
        settings,
        updateSettings,
        replaceSettings,
      }}>
      {children}
    </UserContext.Provider>
  );
};

export { UserContext, UserProvider };
