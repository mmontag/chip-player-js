import React, { createContext, useCallback, useEffect, useMemo, useRef, useState } from 'react';
import debounce from 'lodash/debounce';
import { getAuth, onAuthStateChanged, signInWithPopup, signOut, GoogleAuthProvider } from 'firebase/auth';
import { initializeApp as firebaseInitializeApp } from 'firebase/app';
import firebaseConfig from '../config/firebaseConfig';
import { API_BASE, CATALOG_PREFIX } from '../config';
import { pathJoin, songRef } from '../util';
import { getWithAuth, postWithAuth } from '../util';

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
  showLoopArea: true,
};


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
  // Monotonic token so stale favorites refreshes don't overwrite newer ones.
  const favesRefreshToken = useRef(0);
  // The account whose server list we already have, as its uid. Keyed by the
  // account rather than held as a page-lifetime boolean, so signing in as
  // someone else re-opens the cold path by itself: the check below simply stops
  // matching. A toggle before the initial GET lands bumps the token and
  // invalidates that GET, so a still-cold list is re-read once it completes.
  const [favesHydratedFor, setFavesHydratedFor] = useState(null);
  // Toggles whose request is still in flight, per account (uid -> count). A
  // whole-list refresh is deferred while any of *that account's* toggles is
  // outstanding, so the snapshot cannot drop a mutation that has not landed yet;
  // the last toggle to land then hydrates. Per account, not one global count: a
  // slow toggle from the account the user just left would otherwise defer the
  // new account's own hydration, and -- its late landing being declined by the
  // sameAccount guard -- leave that account's server favorites unread until
  // another toggle or a reload.
  const pendingFaveToggles = useRef(new Map());
  // The identity the favorites in state were last resolved against. The
  // localStorage seed is trusted on the first resolution -- it is what paints
  // favorites without waiting for the GET, and it is nearly always the same
  // user -- but once the identity changes underneath it, the old account's list
  // must not stand in for the new one.
  const lastUid = useRef(null);
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
      // A user change invalidates any favorites refresh started for the old user.
      favesRefreshToken.current++;
      // It also makes the list itself the wrong account's. Don't clear on the
      // *first* resolution (that is the localStorage seed doing its job); clear
      // when the identity actually changes, including to signed-out. The latch
      // goes with the list -- a later sign-in of the same account is a new
      // session, and a still-set latch would leave its favorites cold.
      const uid = user ? user.uid : null;
      if (lastUid.current && lastUid.current !== uid) {
        setFaves([]);
        setFavesHydratedFor(null);
      }
      lastUid.current = uid;
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
      const token = ++favesRefreshToken.current;
      getWithAuth(user, `${API_BASE}/user/favorites`).then(favesRes => {
        if (token === favesRefreshToken.current) {
          setFaves(favesRes.favorites);
          setFavesHydratedFor(user.uid);
        }
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
      favesRefreshToken.current++;
      setUser(null);
      setFaves([]);
    } catch (error) {
      console.log('Firebase logout error:', error);
    }
  };

  const handleToggleFavorite = async (path, subtune = 0, songId) => {
    if (user) {
      // Invalidate any read in flight: this toggle's optimistic state is newer
      // than anything a GET issued before it can report. The fallback refresh
      // below claims its own token when it is issued, so it is not affected.
      favesRefreshToken.current++;
      const oldFaves = faves;
      const isFavorite = faves.find(fave => fave.path === path && (fave.subtune || 0) === subtune);

      const fave = {
        path,
        subtune,
        songId,
        href: pathJoin(CATALOG_PREFIX, encodeURIComponent(path)),
        mtime: Math.floor(Date.now() / 1000),
      };

      const newFaves = isFavorite
        ? faves.filter(fave => !(fave.path === path && (fave.subtune || 0) === subtune))
        : [...faves, fave];
      setFaves(newFaves);

      let addResult;
      let committed = true;
      pendingFaveToggles.current.set(user.uid, (pendingFaveToggles.current.get(user.uid) || 0) + 1);
      try {
        if (isFavorite) {
          await removeFavorite(fave);
        } else {
          addResult = await addFavorite(fave);
        }
      } catch (e) {
        committed = false;
        // Only unwind if the list still belongs to the account that started the
        // toggle: after a sign-out or a switch it has already been cleared (or
        // replaced), and restoring this snapshot would paint the old account's
        // songs back onto the new session.
        if (lastUid.current === user.uid) setFaves(oldFaves);
        console.log('Couldn\'t update favorites in Firebase.', e);
      }
      pendingFaveToggles.current.set(user.uid, Math.max(0, (pendingFaveToggles.current.get(user.uid) || 1) - 1));
      // Everything below lands on `faves`, so establish once that it is still
      // the list of the account that started this toggle. A sign-out or a switch
      // clears it, and a toggle that resolves afterwards belongs to the account
      // that issued it -- applying it would put that account's songs (and their
      // localStorage copy) under the next one. The rejection path is guarded
      // above; the success path leaks just as well, because the refresh below
      // claims its token when it issues, so the sign-out's earlier invalidation
      // no longer covers it and its GET still runs with the captured user.
      const sameAccount = lastUid.current === user.uid;
      // The mutation is committed from here on, so a failed re-read must not
      // roll the optimistic state back -- that would show the song as un-favorited
      // until the next refresh, and a retry would then invert it. The add response
      // carries this entry already decorated (size, sub-tune title, ...), so splice
      // that one in optimistically to avoid a flicker.
      if (sameAccount && committed && addResult && addResult.favorite) {
        setFaves(prev => {
          // A newer toggle may have removed this exact entry already; if so,
          // leave it out instead of re-adding a stale one.
          if (!prev.some(f => f.path === path && (f.subtune || 0) === subtune)) return prev;
          // Replace in place. Appending instead moves the row to the end, so a second
          // add that lands while this one is still pending swaps them: the list (and the
          // order a directory plays in) reorders as responses arrive.
          return prev.map(f =>
            f.path === path && (f.subtune || 0) === subtune ? addResult.favorite : f);
        });
      }
      // Hydrate the list if it is still cold. This toggle invalidated the initial
      // GET, so the server favorites it would have brought -- and this toggle's
      // own result, for a removal -- are not in `faves` yet, and the splice above
      // cannot restore them. Deferred while another toggle *for this account* is
      // in flight, or the whole-list snapshot could drop a mutation that has not
      // landed; the last of this account's toggles to land then hydrates.
      if (sameAccount && favesHydratedFor !== user.uid && !pendingFaveToggles.current.get(user.uid)) {
        // Claim the token here, at issue, instead of reusing this toggle's. The
        // read is taken after every in-flight toggle has landed, so only a
        // toggle that starts *after* this point should invalidate it -- reusing
        // the older token dropped the response whenever a later-started toggle
        // had already finished, which left the list cold for the session.
        const token = ++favesRefreshToken.current;
        try {
          const refreshed = await getWithAuth(user, `${API_BASE}/user/favorites`);
          if (refreshed && token === favesRefreshToken.current) {
            setFaves(refreshed.favorites);
            setFavesHydratedFor(user.uid);
          }
        } catch (e) {
          console.log('Couldn\'t refresh favorites after updating Firebase.', e);
        }
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

  // We need to derive a list of SongRefs to use as the play context. Old
  // favorites have no subtune and resolve to the parent file (sub-tune 0).
  const favesContext = useMemo(() => {
    return faves.map(fave => songRef(fave.path, fave.subtune));
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
