import React, { createContext, useEffect, useState } from 'react';
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
  showPlayerSettings: false,
  handleLogin: () => {},
  handleLogout: () => {},
  handleToggleFavorite: () => {},
  handleToggleSettings: () => {},
});

const UserProvider = ({ children }) => {
  // Use authState hook for user state
  // const [authUser, userLoading] = useAuthState(firebase.auth());
  const [user, setUser] = useState(null); // Local state for user data
  const [faves, setFaves] = useState([]);
  const [showPlayerSettings, setShowPlayerSettings] = useState(false);
  const [loadingUser, setLoadingUser] = useState(true); // Manage loading state

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
                settings: {},
              });
            } else {
              // Restore user
              const data = userSnapshot.data();
              setFaves(data.faves || []);
              setShowPlayerSettings(data.settings?.showPlayerSettings || false);
            }
          })
          .finally(() => {
            setLoadingUser(false);
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

  const handleToggleFavorite = async (path) => {
    // const user = authUser; // Use authUser from useAuthState
    if (user) {
      const userRef = doc(getFirestore(), 'users', user.uid);
      let newFaves, favesOp;
      const oldFaves = faves;
      const exists = oldFaves.includes(path);
      if (exists) {
        newFaves = oldFaves.filter(fave => fave !== path);
        favesOp = arrayRemove(path);
      } else {
        newFaves = [...oldFaves, path];
        favesOp = arrayUnion(path);
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

  const handleToggleSettings = () => {
    // Optimistic update
    const newShowPlayerSettings = !showPlayerSettings;
    setShowPlayerSettings(newShowPlayerSettings);

    if (user) {
      const userRef = doc(getFirestore(), 'users', user.uid);
      updateDoc(userRef, { settings: { showPlayerSettings: newShowPlayerSettings } })
        .catch((e) => {
          console.log('Couldn\'t update settings in Firebase.', e);
        });
    }
  }

  return (
    <UserContext.Provider
      value={{
        user,
        loadingUser,
        faves,
        showPlayerSettings,
        handleLogin,
        handleLogout,
        handleToggleFavorite,
        handleToggleSettings,
      }}>
      {children}
    </UserContext.Provider>
  );
};

export { UserContext, UserProvider };
