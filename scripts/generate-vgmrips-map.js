const fs = require('fs');
const path = require('path');
const Database = require('better-sqlite3');
const chalk = require('chalk');

const USERS_DB_PATH = path.resolve(__dirname, '../server/users.db');
const NEW_CAT_PATH = path.resolve(__dirname, '../server/catalog.db');
const OLD_CAT_PATH = path.resolve(__dirname, '../server/catalog-old');

const usersDb = new Database(USERS_DB_PATH, { readonly: true });
const catDb = new Database(NEW_CAT_PATH, { readonly: true });
const oldCatDb = new Database(OLD_CAT_PATH, { readonly: true });

const playlists = usersDb.prepare('SELECT id, user_id, title, items FROM playlists').all();
const oldCatById = oldCatDb.prepare('SELECT song_id, path, filename, game, system, title, raw_meta FROM music WHERE song_id = ?');
const catById = catDb.prepare('SELECT song_id, path, filename, game, system, title, raw_meta FROM music WHERE song_id = ?');

const orphanMap = new Map();
for (const pl of playlists) {
  let items = [];
  try { items = JSON.parse(pl.items); } catch(e) { continue; }
  for (const item of items) {
    const songId = item.songId || item.id;
    const itemPath = item.path || '';
    const href = item.href || '';
    const isVgm = itemPath.startsWith('VGM Rips') || itemPath.includes('/VGM Rips/') || href.includes('VGM%20Rips') || href.includes('VGM Rips');
    const oldEntry = songId ? oldCatById.get(songId) : null;
    const isVgmOld = oldEntry && oldEntry.path.startsWith('VGM Rips');
    if (isVgm || isVgmOld) {
      if (!catById.get(songId)) {
        orphanMap.set(songId, oldEntry);
      }
    }
  }
}

const allNewVgm = catDb.prepare("SELECT song_id, path, filename, game, system, title, raw_meta FROM music WHERE path LIKE 'VGM Rips/%'").all();
const byFilename = new Map();
const byGamePack = new Map();
for (const s of allNewVgm) {
  if (!byFilename.has(s.filename)) byFilename.set(s.filename, []);
  byFilename.get(s.filename).push(s);
  const parts = s.path.split('/');
  const gameFolder = parts[parts.length - 2];
  if (!byGamePack.has(gameFolder)) byGamePack.set(gameFolder, []);
  byGamePack.get(gameFolder).push(s);
}

function normalize(str) {
  return (str || '')
    .toLowerCase()
    .replace(/['’"`]/g, '')
    .replace(/[_\-()[\].,~#]/g, ' ')
    .replace(/\s+/g, ' ')
    .trim();
}

function cleanTitle(title) {
  return normalize(title).replace(/^\d+\s*/, '').trim();
}

const manualMap = {
  // Providence
  'Nj9U5W6m': 'Jl_4oKQj', // 01 Purifimia -South- Theme -> 02 Field 1
  'JalnaxdJ': '6JRD2aml', // 06 Purifimia Country - Underground Cave -> 03 Dungeon 1
  'nh4mob-u': 'zXBiEL-8', // 11 Cave of Plateau -> 15 Dungeon 7
  // Star Cruiser
  'Fz9k5snO': 'iGFV-nmH', // 22 Barnard's Star (OPN)
  // Metal Black
  'lDN9LO2R': 'e9nh-ofU', // 07 Dancing Horming (Bonus Stage 1 -Over Ride-)
  'G-mdH29n': 'zZ3f7NmC', // 08 Dual Moon (Round 2 -Cry to the Moon-)
  // Namachuukei 68
  'ByE7uZin': '-LbwG2Pg', // 02 Intro -> 03 Opening
  'HHv7YDSr': 'w-MmN0Dv', // 04 End of round -> 15 Change
  '_wjyxs-8': 'uNAH18DQ', // 05 End of game 1 -> 16 Match End
  'qhHz-PS2': 'MuWyBzio', // 06 End of game 2 -> 17 Pro Baseball News Jingle
  '5kECMako': 'eqar09QF', // 07 Match Commentary -> 18 Pro Baseball News
  // Snatcher (Sega CD -> Sega Mega CD)
  '8RlotOrb': 'PC7GrSwe', // 01 Konami
  'H9aEGilU': '1MxTt4Sb', // 31 Faded Memories
  // Miwaku no Chousho
  '5S-17pOT': 'lxER84jf'  // 11 Our Statue of Liberty
};

const finalMapping = {};
const unmapped = [];

for (const [songId, old] of orphanMap) {
  if (manualMap[songId]) {
    const target = allNewVgm.find(s => s.song_id === manualMap[songId]);
    if (target) {
      finalMapping[songId] = {
        newSongId: target.song_id,
        newPath: target.path,
        oldPath: old.path,
        oldTitle: old.title,
        newTitle: target.title
      };
      continue;
    }
  }

  const oldFolder = old.path.split('/')[1] || '';
  const oldFilename = old.filename;
  const oldTitle = old.title || oldFilename.replace(/^\d+\s*/, '').replace(/\.vgz?$/i, '');
  let match = null;

  // 1. Filename match
  const fnCandidates = byFilename.get(oldFilename) || [];
  if (fnCandidates.length === 1) {
    match = fnCandidates[0];
  } else if (fnCandidates.length > 1) {
    const normOldFolder = normalize(oldFolder);
    const scored = fnCandidates.filter(c => normalize(c.path).includes(normOldFolder));
    if (scored.length === 1) match = scored[0];
  }

  // 2. Folder matching
  if (!match) {
    let normOldFolder = normalize(oldFolder).replace(/\bforce\b/g, 'forth');
    let candidatePacks = [];
    for (const [packName, songs] of byGamePack) {
      let normPack = normalize(packName);
      if (normPack === normOldFolder || normPack.includes(normOldFolder) || normOldFolder.includes(normPack)) {
        candidatePacks.push(songs);
      }
    }
    
    // Check filename in pack
    for (const packSongs of candidatePacks) {
      for (const s of packSongs) {
        if (s.filename === oldFilename) { match = s; break; }
      }
      if (match) break;
    }

    // Check title in pack
    if (!match) {
      const targetTitle = cleanTitle(oldTitle);
      for (const packSongs of candidatePacks) {
        for (const s of packSongs) {
          const sTitle = cleanTitle(s.title || s.filename.replace(/\.vgz?$/i, ''));
          if (sTitle === targetTitle) { match = s; break; }
        }
        if (match) break;
      }
    }

    // Relaxed title
    if (!match) {
      const targetTitle = cleanTitle(oldTitle);
      for (const packSongs of candidatePacks) {
        for (const s of packSongs) {
          const sTitle = cleanTitle(s.title || s.filename.replace(/\.vgz?$/i, ''));
          if (sTitle && targetTitle && (sTitle.includes(targetTitle) || targetTitle.includes(sTitle))) {
            match = s;
            break;
          }
        }
        if (match) break;
      }
    }
  }

  if (match) {
    finalMapping[songId] = {
      newSongId: match.song_id,
      newPath: match.path,
      oldPath: old.path,
      oldTitle: old.title,
      newTitle: match.title
    };
  } else {
    unmapped.push({ songId, old });
  }
}

console.log(`Total unique orphaned song IDs: ${orphanMap.size}`);
console.log(`Successfully mapped: ${Object.keys(finalMapping).length}`);
console.log(`Unmapped: ${unmapped.length}`);

if (unmapped.length > 0) {
  console.error('Unmapped items:', unmapped);
  process.exit(1);
}

const outputPath = path.resolve(__dirname, 'vgmrips-favorites-map.json');
fs.writeFileSync(outputPath, JSON.stringify(finalMapping, null, 2), 'utf8');
console.log(chalk.green(`\nWrote complete mapping dictionary to: ${outputPath}`));
