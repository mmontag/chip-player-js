const { Command } = require('commander');
const fs = require('fs');
const admin = require('firebase-admin');
const Database = require('better-sqlite3');
const path = require('path');

// --- CONFIGURATION ---
const DB_FILENAME = 'chipplayer-users.db';
const SNAPSHOT_FILENAME = 'snapshot.json';
const SERVICE_ACCOUNT_PATH = './untracked/chip-player-js-c57327916be6.json'

const program = new Command();

program
  .name('sync')
  .description('Tool to snapshot Firestore users + Auth, ingest into SQLite, and summarize')
  .version('1.3.0');

/**
 * MODE: SNAPSHOT
 * Connects to Firestore & Auth, merges data, saves to JSON.
 */
async function takeSnapshot() {
  console.log('🔌 Connecting to Firebase...');

  if (!fs.existsSync(SERVICE_ACCOUNT_PATH)) {
    console.error(`❌ Error: ${SERVICE_ACCOUNT_PATH} not found.`);
    process.exit(1);
  }

  const serviceAccount = require(SERVICE_ACCOUNT_PATH);

  try {
    admin.initializeApp({
      credential: admin.credential.cert(serviceAccount)
    });
  } catch (e) {
    if (e.code !== 'app/already-exists') throw e;
  }

  const db = admin.firestore();
  const auth = admin.auth();

  // 1. Fetch all Auth Users (Emails, Metadata)
  console.log('🔑 Downloading Auth Users (Emails)...');
  const authUsers = new Map();
  let nextPageToken;

  try {
    do {
      const result = await auth.listUsers(1000, nextPageToken);
      result.users.forEach(userRecord => {
        authUsers.set(userRecord.uid, {
          email: userRecord.email,
          displayName: userRecord.displayName,
          photoURL: userRecord.photoURL,
          creationTime: userRecord.metadata.creationTime,
          lastSignInTime: userRecord.metadata.lastSignInTime
        });
      });
      nextPageToken = result.pageToken;
      process.stdout.write(`   Fetched ${authUsers.size} accounts...\r`);
    } while (nextPageToken);
    console.log(`\n   ✅ Found ${authUsers.size} total accounts.`);
  } catch (error) {
    console.error('\n❌ Auth Fetch Error:', error.message);
    process.exit(1);
  }

  // 2. Fetch Firestore Documents
  console.log('📥 Downloading "users" collection from Firestore...');

  try {
    const snapshot = await db.collection('users').get();
    const data = [];

    snapshot.forEach(doc => {
      // Merge Auth data with Firestore data
      const authData = authUsers.get(doc.id) || {};

      data.push({
        _id: doc.id,
        _auth: authData, // Store auth info in a reserved property
        ...doc.data()
      });
    });

    fs.writeFileSync(SNAPSHOT_FILENAME, JSON.stringify(data, null, 2));

    console.log(`✅ Saved ${data.length} merged records to ${SNAPSHOT_FILENAME}`);
    process.exit(0);
  } catch (error) {
    console.error('❌ Firestore Read Error:', error.message);
    process.exit(1);
  }
}

/**
 * MODE: INGEST
 * Reads local JSON, rebuilds SQLite DB.
 */
function ingestSnapshot() {
  if (!fs.existsSync(SNAPSHOT_FILENAME)) {
    console.error(`❌ Error: ${SNAPSHOT_FILENAME} not found.`);
    console.error('   Run "node sync.js snapshot" first.');
    process.exit(1);
  }

  console.log(`📖 Reading ${SNAPSHOT_FILENAME}...`);
  let rawData;
  try {
    rawData = JSON.parse(fs.readFileSync(SNAPSHOT_FILENAME, 'utf8'));
  } catch (e) {
    console.error('❌ JSON Parse Error:', e.message);
    process.exit(1);
  }

  console.log(`💿 Initializing SQLite (${DB_FILENAME})...`);
  const db = new Database(DB_FILENAME);
  db.pragma('journal_mode = WAL');
  db.pragma('synchronous = NORMAL');

  // Schema
  db.exec(`
    DROP TABLE IF EXISTS favorites;
    DROP TABLE IF EXISTS users;

    CREATE TABLE users (
        uid TEXT PRIMARY KEY,
        email TEXT,
        display_name TEXT,
        photo_url TEXT,
        created_at TEXT,
        last_login TEXT,
        settings TEXT, -- JSON Object
        raw_json TEXT
    );

    CREATE TABLE favorites (
        id INTEGER PRIMARY KEY AUTOINCREMENT,
        user_uid TEXT,
        sort_index INTEGER,
        href TEXT,
        mtime INTEGER,
        FOREIGN KEY(user_uid) REFERENCES users(uid)
    );
    
    CREATE INDEX idx_favorites_user ON favorites(user_uid);
    CREATE INDEX idx_users_email ON users(email);
  `);

  const insertUser = db.prepare(`
      INSERT INTO users (
          uid, email, display_name, photo_url, created_at, last_login,
          settings, raw_json
      ) VALUES (
                   @uid, @email, @displayName, @photoURL, @createdAt, @lastLogin,
                   @settings, @rawJson
               )
  `);

  const insertFave = db.prepare(`
      INSERT INTO favorites (user_uid, sort_index, href, mtime)
      VALUES (@uid, @idx, @href, @mtime)
  `);

  const runTransaction = db.transaction((users) => {
    for (const user of users) {
      const auth = user._auth || {};

      insertUser.run({
        uid: user._id,
        email: auth.email || null,
        displayName: auth.displayName || null,
        photoURL: auth.photoURL || null,
        createdAt: auth.creationTime || null,
        lastLogin: auth.lastSignInTime || null,
        settings: user.settings ? JSON.stringify(user.settings) : '{}',
        rawJson: JSON.stringify(user)
      });

      if (user.faves) {
        const favesList = Array.isArray(user.faves) ? user.faves : Object.values(user.faves);
        favesList.forEach((item, index) => {
          let href = null;
          let mtime = null;

          if (typeof item === 'string') {
            href = item;
          } else if (item && typeof item === 'object') {
            href = item.href;
            mtime = item.mtime || null;
          }

          if (href) {
            insertFave.run({ uid: user._id, idx: index, href: href, mtime: mtime });
          }
        });
      }
    }
  });

  console.log(`🚀 Processing ${rawData.length} user records...`);
  runTransaction(rawData);
  console.log('✅ Ingest complete.');
}

/**
 * MODE: SUMMARY
 * Runs analytical SQL queries.
 */
function runSummary() {
  if (!fs.existsSync(DB_FILENAME)) {
    console.error(`❌ Error: ${DB_FILENAME} not found.`);
    process.exit(1);
  }

  const db = new Database(DB_FILENAME, { readonly: true });

  console.log('\n📊 --- TOP 10 USERS BY FAVORITES ---');
  // Now fetching email/display_name instead of just UID
  const topUsers = db.prepare(`
      SELECT
          COALESCE(u.email, u.display_name, u.uid) as identifier,
          COUNT(f.id) as fave_count
      FROM users u
               LEFT JOIN favorites f ON u.uid = f.user_uid
      GROUP BY u.uid
      ORDER BY fave_count DESC
      LIMIT 10
  `).all();
  console.table(topUsers);

  console.log('\n⭐ --- TOP 50 MOST FAVORITED SONGS ---');
  const topSongs = db.prepare(`
      SELECT href, COUNT(*) as count
      FROM favorites
      GROUP BY href
      ORDER BY count DESC
      LIMIT 50
  `).all();
  console.table(topSongs);

  console.log('\n📈 --- FAVORITES DISTRIBUTION ---');
  const distribution = db.prepare(`
      SELECT
          CASE
              WHEN cnt = 0 THEN '0'
              WHEN cnt = 1 THEN '1'
              WHEN cnt BETWEEN 2 AND 5 THEN '2 - 5'
              WHEN cnt BETWEEN 6 AND 10 THEN '6 - 10'
              WHEN cnt BETWEEN 11 AND 20 THEN '11 - 20'
              WHEN cnt BETWEEN 21 AND 50 THEN '21 - 50'
              WHEN cnt BETWEEN 51 AND 100 THEN '51 - 100'
              WHEN cnt BETWEEN 101 AND 500 THEN '101 - 500'
              WHEN cnt BETWEEN 501 AND 1000 THEN '501 - 1000'
              ELSE '1000+'
              END as range,
          COUNT(*) as count,
          MIN(cnt) as sort_key
      FROM (
               SELECT COUNT(f.id) as cnt
               FROM users u
                        LEFT JOIN favorites f ON u.uid = f.user_uid
               GROUP BY u.uid
           )
      GROUP BY range
      ORDER BY sort_key ASC
  `).all();

  const cleanDistribution = distribution.map(row => ({
    'Favorites Range': row.range,
    'Num Users': row.count
  }));

  console.table(cleanDistribution);

  const totals = db.prepare(`
      SELECT
              (SELECT COUNT(*) FROM users) as total_users,
              (SELECT COUNT(*) FROM favorites) as total_faves
  `).get();

  console.log(`\nTotals: ${totals.total_users} Users, ${totals.total_faves} Favorites`);
}

// --- COMMANDER SETUP ---

program
  .command('snapshot')
  .description('Download "users" from Firestore + Auth to local JSON')
  .action(takeSnapshot);

program
  .command('ingest', { isDefault: true })
  .description('Import local JSON snapshot into SQLite')
  .action(ingestSnapshot);

program
  .command('summary')
  .description('Show analysis of the local data')
  .action(runSummary);

program.parse(process.argv);
