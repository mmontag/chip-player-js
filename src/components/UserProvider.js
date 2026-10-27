import React, { createContext, useCallback, useEffect, useMemo, useRef, useState } from 'react';
import { getAuth, onAuthStateChanged, signInWithPopup, signOut, GoogleAuthProvider } from 'firebase/auth';
import { initializeApp as firebaseInitializeApp } from 'firebase/app';
import firebaseConfig from '../config/firebaseConfig';
import { API_BASE, CATALOG_PREFIX } from '../config';
import { pathJoin } from '../util';
import { getWithAuth, postWithAuth } from '../util';
import SettingsSync from '../SettingsSync';

const UserContext = createContext({
  user: null,
  loadingUser: true,
  faves: [],
  favesContext: [],
  showPlayerSettings: false,
  showVisualizer: false,
  handleLogin: () => {},
  handleLogout: () => {},
  handleToggleFavorite: () => {},
  settings: {},
  updateSettings: () => {},
  replaceSettings: () => {},
});

const DEFAULT_SETTINGS = {
  showPlayerSettings: false,
  showVisualizer: false,
  theme: 'msdos',
  silenceDuration: -1,
};

const UserProvider = ({ children }) => {
  // Use authState hook for user state
  // const [authUser, userLoading] = useAuthState(firebase.auth());
  const [user, setUser] = useState(null); // Local state for user data
  const settingsSync = useRef(null);
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
    return onAuthStateChanged(auth, async user => {
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
    let active = true;
    const sync = new SettingsSync(user.uid, window.localStorage,
      settings => postWithAuth(user, `${API_BASE}/user/settings`, settings));
    settingsSync.current = sync;
    const onOnline = () => sync.schedule(0);
    window.addEventListener('online', onOnline);

    getWithAuth(user, `${API_BASE}/user/favorites`).then(favesRes => {
      if (active) {
        setFaves(favesRes.favorites);
      }
    }).catch(e => console.error('Error fetching favorites:', e));

    if (sync.pending) {
      setSettings(prev => ({ ...prev, ...sync.pending }));
      sync.schedule(0);
    } else {
      const revision = sync.revision;
      getWithAuth(user, `${API_BASE}/user/settings`).then(settingsRes => {
        if (active && sync.revision === revision) {
          setSettings(prev => ({ ...prev, ...settingsRes }));
        }
      }).catch(e => console.error('Error fetching settings:', e));
    }

    return () => {
      active = false;
      window.removeEventListener('online', onOnline);
      sync.dispose();
      if (settingsSync.current === sync) settingsSync.current = null;
    };
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

  const updateSettings = useCallback((partialSettings) => {
    const newSettings = { ...settings, ...partialSettings };
    setSettings(newSettings);
    if (user) settingsSync.current?.enqueue(newSettings);
  }, [user, settings]);

  const replaceSettings = useCallback((newSettings) => {
    setSettings(newSettings);
    if (user) settingsSync.current?.enqueue(newSettings);
  }, [user]);

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
