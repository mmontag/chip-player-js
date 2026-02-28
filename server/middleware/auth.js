// Firebase auth middleware for Express 5
const { dbStatements } = require('../database.js');
const admin = require('firebase-admin');
const serviceAccount = require('../untracked/chip-player-js-c57327916be6.json');

const { getUserStmt, insertUserStmt, updateUserProfileStmt } = dbStatements;

const safe = async (promise) => {
  try {
    return [null, await promise];
  } catch (err) {
    return [err, null];
  }
};

// Check if app is already initialized to avoid error
if (admin.apps.length === 0) {
  admin.initializeApp({
    credential: admin.credential.cert(serviceAccount)
  });
}

function syncUser(decodedToken) {
  // Check if user exists in SQLite
  const user = getUserStmt.get(decodedToken.uid);

  if (!user) {
    // Lazy create: The user is valid in Firebase, but new to SQLite.
    // This can happen when a user signs in for the first time.
    const now = Math.floor(Date.now() / 1000);
    insertUserStmt.run(decodedToken.uid, decodedToken.email, decodedToken.name, decodedToken.picture, now, now);
    console.log(`Created new user: ${(decodedToken.uid)} (${decodedToken.name})`);
  } else {
    // Update profile and last login time
    const now = Math.floor(Date.now() / 1000);
    updateUserProfileStmt.run(
      decodedToken.email,
      decodedToken.name,
      decodedToken.picture,
      now,
      decodedToken.uid
    );
  }
}

const requireAuth = async (req, res, next) => {
  const token = req.headers.authorization?.split('Bearer ')[1];

  if (!token) {
    return res.status(401).send('Unauthorized');
  }

  // Verify Token with Firebase
  const [authErr, decodedToken] = await safe(admin.auth().verifyIdToken(token));

  if (authErr) {
    console.error('Authentication error:', authErr);
    return res.status(401).send('Unauthorized');
  }

  syncUser(decodedToken);

  // Attach uid to request for the route handler to use
  req.userId = decodedToken.uid;
  next();
};

const optionalAuth = async (req, res, next) => {
  const token = req.headers.authorization?.split('Bearer ')[1];

  if (!token) {
    req.userId = null;
    return next();
  }

  // Verify Token with Firebase
  const [authErr, decodedToken] = await safe(admin.auth().verifyIdToken(token));

  if (authErr) {
    // If token is invalid, treat as anonymous
    req.userId = null;
    return next();
  }

  // Attach uid to request for the route handler to use
  req.userId = decodedToken.uid;
  next();
};

module.exports = { requireAuth, optionalAuth };
