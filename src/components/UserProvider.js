import React, { createContext, useCallback, useEffect, useMemo, useState } from 'react';
import { getAuth, onAuthStateChanged, signInWithPopup, signOut, GoogleAuthProvider } from 'firebase/auth';
// import { useAuthState } from 'react-firebase-hooks/auth'; // Consider using react-firebase-hooks
// import firebase from 'firebase/app'; // Assuming you have firebase configured
import { initializeApp as firebaseInitializeApp } from 'firebase/app';
import {
  getFirestore,
  doc,
  getDoc,
  updateDoc,
  setDoc,
  arrayRemove,
  arrayUnion
} from 'firebase/firestore/lite';
import firebaseConfig from '../config/firebaseConfig';

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
});

const DEFAULT_SETTINGS = {
  showPlayerSettings: false,
  theme: 'msdos',
};

/**
 * Convert favorites from list of path strings to list of objects.
 * As of July 2024, the converted objects are not persisted to Firebase.
 * Only new favorites are saved to Firebase in the object form.
 *
 * {
 *   href: 'https://web.site/music/game/song.vgm',
 *   date: 1650000000,
 * }
 *
 * @param faves
 */
function migrateFaves(faves) {
  if (faves.length > 0) {
    return faves.map(fave => {
      return typeof fave === 'string' ? {
        href: fave,
        mtime: Math.floor(Date.parse('2024-01-01') / 1000),
      } : fave;
    });
  }

  return faves;
}

function migrateSettings(settings) {
  const migratedSettings = { ...settings };
  Object.keys(DEFAULT_SETTINGS).forEach(key => {
    if (settings[key] === undefined) {
      migratedSettings[key] = DEFAULT_SETTINGS[key];
    }
  });
  return migratedSettings;
}

const UserProvider = ({ children }) => {
  // Use authState hook for user state
  // const [authUser, userLoading] = useAuthState(firebase.auth());
  const [user, setUser] = useState(null); // Local state for user data
  const [faves, setFaves] = useState([]);
  const [loadingUser, setLoadingUser] = useState(true); // Manage loading state
  const [settings, setSettings] = useState(DEFAULT_SETTINGS);

  useEffect(() => {
    // Initialize Firebase
    const firebaseApp = firebaseInitializeApp(firebaseConfig);
    const auth = getAuth(firebaseApp);
    const db = getFirestore(firebaseApp);
    onAuthStateChanged(auth, user => {
      setUser(user);
      setLoadingUser(false);
      if (user) {
        const docRef = doc(db, 'users', user.uid);
        getDoc(docRef)
          .then(userSnapshot => {
            if (!userSnapshot.exists()) {
              // Create user
              console.debug('Creating user document', user.uid);
              setDoc(docRef, {
                faves: [],
                settings: DEFAULT_SETTINGS,
              });
            } else {
              // Restore user
              const data = userSnapshot.data();
              const faves = migrateFaves(data.faves || []);
              setFaves(faves);
              const settings = migrateSettings(data.settings);
              setSettings(settings);
            }
          });
      }
    });
  }, []);

  const handleLogin = async () => {
    const auth = getAuth();
    const provider = new GoogleAuthProvider();
    try {
      const result = await signInWithPopup(auth, provider);
      console.log('Firebase auth result:', result);
      // Update user state based on result
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

  const handleToggleFavorite = async (href) => {
    if (user) {
      const userRef = doc(getFirestore(), 'users', user.uid);
      let newFaves, favesOp;
      const oldFaves = faves;
      const existingIdx = oldFaves.findLastIndex(f => f === href || f.href === href);
      if (existingIdx === -1) {
        // ADD
        const newFave = {
          href,
          mtime: Math.floor(Date.now() / 1000),
        }
        newFaves = [...oldFaves, newFave];
        favesOp = arrayUnion(newFave);
      } else {
        // REMOVE
        // Firebase cannot remove from array by index, only by value.
        // Remove both the object and href (for legacy favorites).
        const element = oldFaves[existingIdx];
        newFaves = oldFaves.toSpliced(existingIdx, 1);
        favesOp = arrayRemove(element, element.href);
      }
      // Optimistic update
      setFaves(newFaves);
      try {
        await updateDoc(userRef, { faves: favesOp });
      } catch (e) {
        setFaves(oldFaves);
        console.log('Couldn\'t update favorites in Firebase.', e);
      }
    }
  };

  const updateSettings = useCallback((partialSettings) => {
    const newSettings = { ...settings, ...partialSettings };
    setSettings(newSettings);

    if (user) {
      const userRef = doc(getFirestore(), 'users', user.uid);
      updateDoc(userRef, { settings: newSettings })
        .catch((e) => {
          console.log('Couldn\'t update settings in Firebase.', e);
        });
    }
  }, [user, settings]);

  // We need to derive a list of hrefs to use as the play context.
  const favesContext = useMemo(() => {
    return faves.map(fave => fave.href);
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
      }}>
      {children}
    </UserContext.Provider>
  );
};

export { UserContext, UserProvider };
