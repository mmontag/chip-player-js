// Firebase auth middleware for Express 5
const { db, dbStatements } = require('../database.js');
const admin = require('firebase-admin');
const SERVICE_ACCOUNT_PATH = '../../scripts/untracked/chip-player-js-c57327916be6.json'

const { getUserStmt, insertUserStmt } = dbStatements;

// Initialize Firebase Admin SDK
const serviceAccount = require(SERVICE_ACCOUNT_PATH);

admin.initializeApp({
  credential: admin.credential.cert(serviceAccount)
});

module.exports = async function authMiddleware(req, res, next) {
  const token = req.headers.authorization?.split('Bearer ')[1];

  try {
    // 1. Verify Token with Firebase
    const decodedToken = await admin.auth().verifyIdToken(token);
    const uid = decodedToken.uid;

    // 2. Check if user exists in SQLite
    const user = getUserStmt.get(uid);

    if (!user) {
      // 3. LAZY CREATE: The user is valid in Firebase, but new to SQLite.
      // We grab their email/pic from the token itself.
      // This can happen when a user signs in for the first time.
      insertUserStmt.run(uid, decodedToken.email, decodedToken.name, decodedToken.picture);
      console.log(`Created new user: ${uid} (${decodedToken.name})`);
      console.log('- full token: ', decodedToken);
    }

    // 4. Attach uid to request for the route handler to use
    req.userId = uid;
    next();

  } catch (error) {
    console.error('Authentication error:', error);
    res.status(401).send('Unauthorized');
  }
}
