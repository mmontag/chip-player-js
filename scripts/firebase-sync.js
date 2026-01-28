const { Command } = require('commander');
const fs = require('fs');
const admin = require('firebase-admin');
const Database = require('better-sqlite3');

// --- CONFIGURATION ---
const DB_FILENAME = '../server/users.db';
const CATALOG_DB_FILENAME = '../server/catalog.db';
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
        authUsers.set(userRecord.id, {
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

  // Attach Catalog DB for song_id resolution
  if (fs.existsSync(CATALOG_DB_FILENAME)) {
    console.log(`💿 Attaching Catalog DB (${CATALOG_DB_FILENAME})...`);
    db.exec(`ATTACH DATABASE '${CATALOG_DB_FILENAME}' AS catalog`);
  } else {
    console.warn(`⚠️ Catalog DB not found at ${CATALOG_DB_FILENAME}. Song IDs will not be resolved.`);
  }

  console.log('💿 Creating temporary index for fuzzy matching...');
  try {
    // Use reverse() function if available (SQLite 3.34.0+)
    db.exec(`
        CREATE TEMP TABLE music_reversed AS
        SELECT song_id, reverse(path) as path_reversed
        FROM catalog.music;

        CREATE INDEX idx_temp_music_reversed ON music_reversed(path_reversed COLLATE NOCASE);
    `);
    console.log('   ✅ Created temporary table with reverse() function.');
  } catch (e) {
    console.warn('   ⚠️ reverse() function not available. Falling back to slower method.');
    // Fallback if reverse() is not available
    db.exec(`
        CREATE TEMP TABLE music_reversed (
            song_id TEXT,
            path_reversed TEXT
        );
    `);
    const songs = db.prepare('SELECT song_id, path FROM catalog.music').all();
    const insertReversed = db.prepare('INSERT INTO music_reversed (song_id, path_reversed) VALUES (?, ?)');
    const populateReversed = db.transaction((songs) => {
      for (const song of songs) {
        const reversed = song.path.split('').reverse().join('');
        insertReversed.run(song.song_id, reversed);
      }
    });
    populateReversed(songs);
    db.exec('CREATE INDEX idx_temp_music_reversed ON music_reversed(path_reversed COLLATE NOCASE);');
    console.log('   ✅ Created temporary table with JS fallback.');
  }

  // Schema
  db.exec(`
    DROP TABLE IF EXISTS playbacks;
    DROP TABLE IF EXISTS playlists;
    DROP TABLE IF EXISTS favorites;
    DROP TABLE IF EXISTS users;

    CREATE TABLE users (
        id TEXT PRIMARY KEY,
        email TEXT,
        display_name TEXT,
        photo_url TEXT,
        created_at TEXT,
        last_login TEXT,
        settings TEXT, -- JSON Object
        raw_json TEXT
    );

    CREATE TABLE playlists (
        id INTEGER PRIMARY KEY AUTOINCREMENT,
        user_id TEXT,
        title TEXT,
        created_at INTEGER,
        modified_at INTEGER,
        type TEXT,
        items TEXT, -- JSON Array
        FOREIGN KEY(user_id) REFERENCES users(id)
    );
    
    CREATE TABLE playbacks (
        id INTEGER PRIMARY KEY AUTOINCREMENT,
        user_id TEXT,
        song_id TEXT,
        played_at INTEGER,
        duration_ms INTEGER,
        FOREIGN KEY(user_id) REFERENCES users(id)
    );
    
    CREATE INDEX idx_users_email ON users(email);
    CREATE INDEX idx_playlists_user ON playlists(user_id);
    CREATE UNIQUE INDEX idx_playlists_user_favorites ON playlists(user_id) WHERE type = 'favorites';
    CREATE INDEX idx_playbacks_user ON playbacks(user_id);
    CREATE INDEX idx_playbacks_song ON playbacks(song_id);
  `);

  const insertUser = db.prepare(`
      INSERT INTO users (
          id, email, display_name, photo_url, created_at, last_login,
          settings, raw_json
      ) VALUES (
                   @id, @email, @displayName, @photoURL, @createdAt, @lastLogin,
                   @settings, @rawJson
               )
  `);

  const insertPlaylist = db.prepare(`
      INSERT INTO playlists (user_id, title, created_at, modified_at, type, items)
      VALUES (@userId, @title, @createdAt, @modifiedAt, @type, @items)
  `);

  let lookupSongId = null;
  let lookupSongIdSuffix = null;
  try {
    lookupSongId = db.prepare("SELECT song_id FROM catalog.music WHERE path = ?");
    lookupSongIdSuffix = db.prepare("SELECT song_id FROM music_reversed WHERE path_reversed LIKE ? LIMIT 1");
  } catch (e) {
    console.warn("Could not prepare song lookup statement (catalog attached?)");
  }

  const stats = { exact: 0, fuzzy: 0, missing: 0 };

  const runTransaction = db.transaction((users) => {
    for (const user of users) {
      const auth = user._auth || {};

      insertUser.run({
        id: user._id,
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
        const items = [];
        let maxMtime = 0;

        favesList.forEach((item) => {
          let href = null;
          let mtime = null;

          if (typeof item === 'string') {
            href = item;
          } else if (item && typeof item === 'object') {
            href = item.href;
            mtime = item.mtime || null;
          }


          if (href) {
            if (href.startsWith('http://localhost')) return; // Skip a few odd localhost links
            let songId = null;
            if (lookupSongId) {
                // Strip prefix to get path
                let relativePath = href.replace('https://gifx.co/music/', '');

                // Fix known renames - repairs about 200 out of 300 broken favorites
                relativePath = relativePath.replace(/(Perfect Dark \(2000\)\(Rare\)\(Rare\)\/...?) - /, '$1 ');
                relativePath = relativePath.replace(/(Star Fox 64 \(1997\)\(Nintendo EAD\)\(Nintendo\)\/...?) - /, '$1 ');
                relativePath = relativePath.replace(/(Legend of Zelda Majora's Mask,The \(2000\)\(Nintendo EAD\)\(Nintendo\)\/...?) - /, '$1 ');
                relativePath = relativePath.replace(/NSFe_Collection_Part5_PAL Releases of NTSC Games \(Same Name\)/, 'PAL');
                relativePath = relativePath.replace(/.+Shovel_Knight_Music\.nsf/, 'Contemporary/Virt (Jake Kaufman)/Shovel Knight (2014) - Shovel of Hope.nsfe');
                relativePath = relativePath.replace(/.+VIRT_-_KEEP_SHREDDING_LITTLE_MAN\.S3M/, 'Contemporary/Virt (Jake Kaufman)/keep shredding little man.s3m');

                // Try exact match first
                const row = lookupSongId.get(relativePath);
                if (row) {
                    songId = row.song_id;
                    stats.exact++;
                } else {
                    // Try fuzzy match
                    // First, try decoding in case it was encoded
                    try {
                        relativePath = decodeURIComponent(relativePath);
                    } catch (e) {}

                    const parts = relativePath.split('/');
                    // Remove empty parts if any (e.g. leading slash)
                    if (parts[0] === '') parts.shift();

                    let found = false;
                    
                    // Loop: try full path with wildcard, then remove first component and try again
                    while (parts.length > 0) {
                        const suffix = parts.join('/');
                        if (lookupSongIdSuffix) {
                            const reversedSuffix = suffix.split('').reverse().join('');
                            const match = lookupSongIdSuffix.get(`${reversedSuffix}%`);
                            if (match) {
                                songId = match.song_id;
                                stats.fuzzy++;
                                found = true;
                                break;
                            }
                        }
                        parts.shift();
                    }
                    
                    if (!found) {
                        stats.missing++;
                        console.warn(`- Song not found: ${relativePath}`);
                    }
                }
            }

            // We store the item even if songId is null, to preserve the data.
            // But ideally we want songId.
            items.push({ songId, mtime, href });
            
            if (mtime && mtime > maxMtime) {
                maxMtime = mtime;
            }
          }
        });

        if (items.length > 0) {
            let createdAt = Date.now();
            if (auth.creationTime) {
                const parsed = Date.parse(auth.creationTime);
                if (!isNaN(parsed)) createdAt = parsed;
            }

            insertPlaylist.run({
                userId: user._id,
                title: 'Favorites',
                createdAt: createdAt,
                modifiedAt: maxMtime || createdAt,
                type: 'favorites',
                items: JSON.stringify(items)
            });
        }
      }
    }
  });

  console.log(`🚀 Processing ${rawData.length} user records...`);
  runTransaction(rawData);
  console.log('✅ Ingest complete.');
  console.log('📊 Migration Stats:');
  console.log(`   Exact matches: ${stats.exact}`);
  console.log(`   Fuzzy matches: ${stats.fuzzy}`);
  console.log(`   Missing songs: ${stats.missing}`);
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
  const topUsers = db.prepare(`
      SELECT
          COALESCE(u.email, u.display_name, u.id) as identifier,
          json_array_length(p.items) as fave_count
      FROM users u
               JOIN playlists p ON u.id = p.user_id
      WHERE p.type = 'favorites'
      ORDER BY fave_count DESC
      LIMIT 10
  `).all();
  console.table(topUsers);

  console.log('\n⭐ --- TOP 50 MOST FAVORITED SONGS ---');
  const topSongs = db.prepare(`
      SELECT 
          json_extract(value, '$.href') as href, 
          COUNT(*) as count
      FROM playlists, json_each(playlists.items)
      WHERE playlists.type = 'favorites'
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
               SELECT COALESCE(json_array_length(p.items), 0) as cnt
               FROM users u
                        LEFT JOIN playlists p ON u.id = p.user_id AND p.type = 'favorites'
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
              (SELECT SUM(json_array_length(items)) FROM playlists WHERE type='favorites') as total_faves
  `).get();

  console.log(`\nTotals: ${totals.total_users} Users, ${totals.total_faves || 0} Favorites`);
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
