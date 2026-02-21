import React, { createContext, useCallback, useEffect, useMemo, useState } from 'react';
import axios from 'axios';
import { debounce } from 'lodash';
import { getAuth, onAuthStateChanged, signInWithPopup, signOut, GoogleAuthProvider } from 'firebase/auth';
import { initializeApp as firebaseInitializeApp } from 'firebase/app';
import firebaseConfig from '../config/firebaseConfig';
import { API_BASE, CATALOG_PREFIX } from '../config';
import { pathJoin } from '../util';

const UserContext = createContext({
  user: null,
  loadingUser: true,
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

const getWithAuth = async (user, path) => {
  if (!user) return;

  const token = await user.getIdToken();
  return axios.get(path, {
    headers: { 'Authorization': `Bearer ${token}`, },
  }).then(res => res.data);
}

const postWithAuth = async (user, path, json) => {
  if (!user) return;

  const token = await user.getIdToken();
  return axios.post(path, json, {
    headers: { 'Authorization': `Bearer ${token}`, },
  }).then(res => res.data);
}

const UserProvider = ({ children }) => {
  // Use authState hook for user state
  // const [authUser, userLoading] = useAuthState(firebase.auth());
  const [user, setUser] = useState(null); // Local state for user data
  const [faves, setFaves] = useState(() => {
    // Restore favorites from localStorage.
    try {
      const faves = window.localStorage.getItem('faves');
      return faves ? JSON.parse(faves) : [];
    } catch (e) {
      console.error('Could not load faves from localStorage', e);
      return [];
    }
  });
  const [loadingUser, setLoadingUser] = useState(true); // Manage loading state
  const [settings, setSettings] = useState(() => {
    // Restore settings from localStorage.
    try {
      const settings = window.localStorage.getItem('settings');
      return settings ? JSON.parse(settings) : DEFAULT_SETTINGS;
    } catch (e) {
      console.error('Could not load settings from localStorage', e);
      return DEFAULT_SETTINGS;
    }
  });

  useEffect(() => {
    // Initialize Firebase
    const firebaseApp = firebaseInitializeApp(firebaseConfig);
    const auth = getAuth(firebaseApp);
    onAuthStateChanged(auth, async user => {
      setUser(user);
      console.log('Firebase auth state changed, user:', user);
      setLoadingUser(false);
    });
  }, []);

  // Copy favorites to localStorage whenever they change, so they persist across page reloads.
  useEffect(() => {
    try {
      window.localStorage.setItem('faves', JSON.stringify(faves));
    } catch (e) {
      console.error('Could not save faves to localStorage', e);
    }
  }, [faves]);

  // Copy settings to localStorage whenever they change, so they persist across page reloads.
  useEffect(() => {
    try {
      window.localStorage.setItem('settings', JSON.stringify(settings));
    } catch (e) {
      console.error('Could not save settings to localStorage', e);
    }
  }, [settings]);

  useEffect(() => {
    if (!user) return;

    try {
      getWithAuth(user, `${API_BASE}/user/favorites`).then(favesRes => {
        setFaves(favesRes.favorites);
      });

      getWithAuth(user, `${API_BASE}/user/settings`).then(settingsRes => {
        // Merge server settings with defaults to ensure all keys exist
        setSettings(prev => ({ ...prev, ...settingsRes }));
      });
    } catch (e) {
      console.error('Error fetching user data:', e);
    }
  }, [user]);


  const handleLogin = async () => {
    const auth = getAuth();
    const provider = new GoogleAuthProvider();
    try {
      const result = await signInWithPopup(auth, provider);
      console.log('Firebase auth result:', result);
    } catch (error) {
      console.log('Firebase auth error:', error);
    }
  };

  const handleLogout = async () => {
    const auth = getAuth();
    try {
      await signOut(auth);
      setUser(null);
      setFaves([]);
    } catch (error) {
      console.log('Firebase logout error:', error);
    }
  };

  const handleToggleFavorite = async (path, songId) => {
    if (user) {
      const oldFaves = faves;
      const isFavorite = faves.find(fave => fave.path === path);

      const fave = {
        path,
        songId,
        href: pathJoin(CATALOG_PREFIX, encodeURIComponent(path)),
        mtime: Math.floor(Date.now() / 1000),
      };

      const newFaves = isFavorite
        ? faves.filter(fave => fave.path !== path)
        : [...faves, fave];
      setFaves(newFaves);

      try {
        if (isFavorite) {
          await removeFavorite(fave);
        } else {
          await addFavorite(fave);
        }
      } catch (e) {
        setFaves(oldFaves);
        console.log('Couldn\'t update favorites in Firebase.', e);
      }
    }
  };

  const addFavorite = useCallback(async (fave) => {
    return postWithAuth(user, `${API_BASE}/user/favorites/add`, fave);
  }, [user]);

  const removeFavorite = useCallback(async (fave) => {
    return postWithAuth(user, `${API_BASE}/user/favorites/remove`, fave);
  }, [user]);

  const saveSettings = useMemo(() => {
    return debounce((user, newSettings) => {
      if (!user) return;
      postWithAuth(user, `${API_BASE}/user/settings`, newSettings)
        .then(res => {
          console.log('Saved settings to server:', res);
        })
        .catch(e => {
          console.log('Couldn\'t save settings to server.', e);
        });
    }, 1000);
  }, []);

  const updateSettings = useCallback((partialSettings) => {
    const newSettings = { ...settings, ...partialSettings };
    setSettings(newSettings);
    saveSettings(user, newSettings);
  }, [user, settings, saveSettings]);

  const replaceSettings = useCallback((newSettings) => {
    setSettings(newSettings);
    saveSettings(user, newSettings);
  }, [user, saveSettings]);

  // We need to derive a list of paths to use as the play context.
  const favesContext = useMemo(() => {
    return faves.map(fave => fave.path);
  }, [faves]);

  return (
    <UserContext.Provider
      value={{
        user,
        loadingUser,
        faves,
        favesContext,
        handleLogin,
        handleLogout,
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
