#!/usr/bin/env node

/**
 *
 * Chip Player JS
 * ᴀ ᴘ ɪ  s ᴇ ʀ ᴠ ᴇ ʀ
 *
 * Matt Montag · March 2019
 *
 */
require('dotenv-flow').config({ path: __dirname });

const express = require('express');
const fs = require('fs').promises;
const { createProxyMiddleware } = require('http-proxy-middleware');
const { LRUCache } = require('lru-cache');
const path = require('path');
const { performance } = require('perf_hooks');
const { Canvas, loadImage } = require('skia-canvas');
const axios = require('axios');
const axiosRetry = require('axios-retry').default;
const { XMLParser } = require('fast-xml-parser');

const { dbStatements } = require('./database.js');
const { resolveDirectoryMetadata } = require('./directory-metadata');
const { requireAuth, optionalAuth } = require('./middleware/auth.js');
const { validate } = require('./middleware/validate');
const { SettingsSchema, FavoriteSchema, PlaybackSchema } = require('./schemas');

const {
  searchSongStmt,
  getDirIdStmt,
  getDirChildrenStmt,
  getSubtunesStmt,
  getMetadataStmt,
  getSubtuneMetadataStmt,
  getSidMetadataByHashStmt,
  getSidMetadataByPathStmt,
  getTextContentStmt,
  getShuffleStmt,
  getTotalStmt,
  getSongByPathStmt,
  getSubtuneCountBySongIdStmt,
  getSongByIdStmt,
  getSongImageByIdStmt,

  getFavoritesStmt,
  addFavoriteByPathStmt,
  removeFavoriteByPathStmt,
  getUserSettingsStmt,
  replaceUserSettingsStmt,
  insertPlaybackStmt,

  getCsdbXmlStmt,
  insertCsdbXmlStmt,

  getGlobalTopStmt,
  getUserTopStmt,
  getTopFavoritesStmt,
} = dbStatements;

// --- Configuration ---
const {
  LOCAL_CATALOG_ROOT,
  LOCAL_SOUNDFONT_ROOT,
  LOCAL_CLIENT_BUILD_ROOT,
  BROWSE_LOCAL_FILESYSTEM,
  NODE_ENV,
} = process.env;
const hostname = '0.0.0.0';
const port = process.env.PORT || 8080;
const browseLocalFilesystem = BROWSE_LOCAL_FILESYSTEM === 'true';
const isDev = NODE_ENV === 'development';
const WDS_PORT = 3000; // Port where Webpack Dev Server runs
const indexFilename = 'index.template.html';
const chipPlayerPng = 'chip-player-1200x640.png';
const logoPng = 'chip-player-logo.png';

if (!LOCAL_CATALOG_ROOT || !LOCAL_CLIENT_BUILD_ROOT) {
  console.error('Missing required environment variables. See .env file for details.');
  process.exit(1);
}

// Only used when browsing local filesystem
const { FORMATS } = browseLocalFilesystem ? require('../src/config/index.js') : [];

const app = express();
const router = express.Router();
app.set('trust proxy', 'loopback'); // Only trust X-Forwarded-For from localhost (Nginx)

// Pre-load index.html template (Production Only)
const indexPath = path.join(LOCAL_CLIENT_BUILD_ROOT, indexFilename);
let indexHtmlProd;

// --- Development Proxy Setup ---
if (isDev) {
  console.log(`Running in dev mode (proxy to localhost:${WDS_PORT})`);
  // Proxy Webpack static assets and Hot Module Replacement
  const devProxy = createProxyMiddleware({
    target: `http://localhost:${WDS_PORT}`,
    changeOrigin: true,
    ws: true, // Crucial for HMR WebSockets
    logger: console,
    pathFilter: (pathname, req) => {
      return (
        pathname.endsWith('.mp3') ||
        pathname.endsWith('.wasm') ||
        pathname.startsWith('/static') ||
        pathname.startsWith('/ws') ||
        pathname.startsWith('/sockjs-node') ||
        /\.hot-update\.(json|js)(\.map)?$/.test(pathname)
      );
    }
  });

  app.use(devProxy);

  // Serve static files (handled by Express as fallback to Nginx)
  // app.use(express.static(path.join(__dirname, LOCAL_CLIENT_BUILD_ROOT)));
}

// Build the FTS5 MATCH string for a user query. Each term is wrapped in double
// quotes (doubling any inside) and prefix-matched: a bare `-` or `:` is FTS5
// syntax, not text, so `in-game` or `zelda:link's` would otherwise error or
// change meaning -- and the catch below would silently return nothing.
function ftsQueryFor(query) {
  return (query || '').trim().split(/\s+/).filter(Boolean)
    .map(term => `"${term.replace(/"/g, '""')}"*`)
    .join(' ');
}

// Prepopulate searchMap for all 1- and 2-letter queries
const searchMap = new Map();
console.log('Pre-populating search map for 1- and 2-letter queries...');
(async () => {
  for (let char1 of [...'abcdefghijklmnopqrstuvwxyz']) {
    await new Promise(resolve => setTimeout(resolve, 100));
    process.stdout.write(char1);
    const query = char1;
    searchMap.set(query, searchSongStmt.all(ftsQueryFor(query), 50));
    for (let char2 of [...'abcdefghijklmnopqrstuvwxyz']) {
      const query = char1 + char2;
      searchMap.set(query, searchSongStmt.all(ftsQueryFor(query), 50));
    }
  }
  console.log(`...Done populating search map.`);
})();

// --- Production Asset Pre-loading ---
if (!isDev) {
  console.log('Running in production mode.');
  fs.readFile(indexPath, 'utf8').then(data => {
    indexHtmlProd = data;
    console.log(`Loaded ${indexFilename}.`);
  }).catch(e => {
    console.warn(`Could not load ${indexPath}. Ensure you have built the app.`);
  });

}

app.use((req, res, next) => {
  res.header('Access-Control-Allow-Origin', '*');
  next();
});

const cache1Hour = (req, res, next) => {
  res.header('Cache-Control', 'public, max-age=3600');
  next();
};

const fixSidMimeType = (req, res, next) => {
  if (req.path.endsWith('.sid')) res.type('audio/prs.sid');
  next();
};

// --- API Routes ---

app.use('/api', router);

// --- Preview Image Cache ---
const previewCache = new LRUCache({
  max: 25,
});

let bgImage = null;
async function getBgImage() {
  if (bgImage) return bgImage;
  
  let bgPath = path.join(LOCAL_CLIENT_BUILD_ROOT, chipPlayerPng);
  try {
    await fs.access(bgPath);
  } catch (e) {
    // Fallback for development or if build is missing
    bgPath = path.join(__dirname, `../public/${chipPlayerPng}`);
  }
  
  bgImage = await loadImage(bgPath);
  return bgImage;
}

let logoImage = null;
async function getLogoImage() {
  if (logoImage) return logoImage;

  let logoPath = path.join(LOCAL_CLIENT_BUILD_ROOT, logoPng);
  try {
    await fs.access(logoPath);
  } catch (e) {
    // Fallback for development or if build is missing
    logoPath = path.join(__dirname, `../public/${logoPng}`);
  }

  logoImage = await loadImage(logoPath);
  return logoImage;
}

app.get('/preview', cache1Hour, async (req, res) => {
  const { s: songId } = req.query;
  if (!songId) {
    return res.status(400).send('Missing song ID');
  }

  try {
    const song = getSongImageByIdStmt.get(`${songId}*`);
    let imageUrl = null;

    // getting crazy here ···
    if (song.image_path) {
      imageUrl = path.join(LOCAL_CATALOG_ROOT, song.image_path);
    }
    else if (song.path.endsWith('.sid')) {
      const sidMeta = getSidMetadataByPathStmt.get('_'+song.path);
      if (sidMeta) {
        imageUrl = await getCsdbImageUrl(sidMeta.csdbid);
      }
    }

    // Check cache if we have a song image path
    if (song && imageUrl) {
      const cachedBuffer = previewCache.get(imageUrl);
      if (cachedBuffer) {
        res.header('Content-Type', 'image/png');
        return res.send(cachedBuffer);
      }
    }

    const bg = await getBgImage();
    const logo = await getLogoImage();
    const canvas = new Canvas(bg.width, bg.height);
    const ctx = canvas.getContext('2d');
    
    // Draw background
    ctx.drawImage(bg, 0, 0);

    if (song && imageUrl) {
      try {
        const songImage = await loadImage(imageUrl);
        
        // Calculate dimensions to fit 80% of width or height
        const maxWidth = canvas.width * 0.8;
        const maxHeight = canvas.height * 0.8;
        
        let drawWidth = songImage.width;
        let drawHeight = songImage.height;
        
        const scaleX = maxWidth / drawWidth;
        const scaleY = maxHeight / drawHeight;
        const scale = Math.min(scaleX, scaleY);
        
        drawWidth *= scale;
        drawHeight *= scale;

        const logoWidth = 280;
        const logoOverlap = 36; // Amount of logo overlapping the song image
        const offset = logoWidth - logoOverlap;

        // Center the image + logo horizontally
        const x = (canvas.width - drawWidth - offset) / 2;
        const y = (canvas.height - drawHeight) / 2;

        // Draw semi-transparent overlay
        ctx.fillStyle = '#00008080';
        ctx.fillRect(0, 0, canvas.width, canvas.height);

        // Draw yellow outline with dropshadow
        ctx.fillStyle = '#ffffff';
        ctx.shadowColor = '#000040';
        ctx.shadowBlur = 60;
        ctx.shadowOffsetY = 15;
        const lw = 5;
        ctx.fillRect(x - lw + offset, y - lw, drawWidth + lw * 2, drawHeight + lw * 2);
        ctx.fillRect(x - lw + offset, y - lw, drawWidth + lw * 2, drawHeight + lw * 2);
        ctx.shadowColor = 'transparent';

        ctx.drawImage(songImage, x + offset, y, drawWidth, drawHeight);
        ctx.drawImage(logo, x, 180, 280, 280);
      } catch (e) {
        console.error('Error loading song image:', e);
      }
    }

    const buffer = await canvas.toBuffer('png');
    
    // Cache the result if we have a song image path
    if (song && imageUrl) {
      previewCache.set(imageUrl, buffer);
    }

    res.header('Content-Type', 'image/png');
    res.send(buffer);
  } catch (e) {
    console.error('Preview generation error:', e);
    res.status(500).send('Error generating preview');
  }
});

/**
 * Returns: { items: [ { file, song_id, subtune, title }, ... ], total }
 *
 * One query over the song index: every result is a playable song, so a file and
 * its sub-tunes are ranked in a single list and `limit` bounds the whole
 * response. `title` is the sub-tune's own label (null when it has none, and the
 * client falls back to `Tune N`); searching a file's name matches every song it
 * holds.
 */
router.get('/search', cache1Hour, (req, res) => {
  const { limit = 100, query } = req.query;
  const start = performance.now();
  const cacheKey = (query || '').trim();

  let items = [];
  const ftsQuery = ftsQueryFor(query);

  if (searchMap.has(cacheKey)) {
    // The map holds a fixed 50 per 1-2 letter query, so `limit` still caps it.
    items = [...searchMap.get(cacheKey)].slice(0, Number(limit) || 100);
  } else if (ftsQuery) {
    try {
      items = searchSongStmt.all(ftsQuery, limit);
    } catch (e) {
      console.error('Search error:', e.message);
    }
  }

  const time = (performance.now() - start).toFixed(1);
  console.log('Returned %s results for "%s" in %s ms.', items.length, query, time);

  res.json({
    items: items,
    total: items.length,
  });
});

/**
 * Returns: { total, unique }
 */
router.get('/total', cache1Hour, (req, res) => {
  const result = getTotalStmt.get();
  res.json({ total: result.total, unique: result.unique });
});

/**
 * Returns: { items: [ { path, subtune }, ... ], total }
 *
 * One item per playable song: a multi-song file is expanded into its sub-tunes,
 * so shuffle play treats a sub-tune exactly like a single-song file instead of
 * landing on one arbitrary sub-tune per file.
 */
function toSongRefs(rows) {
  return rows.map(({ path, subtune }) => ({ path, subtune }));
}

router.get('/random', (req, res) => {
  const limit = parseInt(req.query.limit, 10) || 1;
  const items = toSongRefs(getShuffleStmt.all('', '%', limit));
  res.json({
    items: items,
    total: items.length,
  });
});

/**
 * Escapes LIKE metacharacters so a path is matched literally.
 */
function escapeLike(value) {
  return value.replace(/[\\%_]/g, char => `\\${char}`);
}

/**
 * Returns: { items: [ { path, subtune }, ... ], total }
 *
 * `path` may name a directory, which is shuffled recursively, or a file, which
 * shuffles as itself (all of its sub-tunes, if it has any).
 */
router.get('/shuffle', (req, res) => {
  const limit = parseInt(req.query.limit, 10) || 100;
  const reqPath = (req.query.path || '').replace(/^\/+|\/+$/g, '');
  const items = toSongRefs(getShuffleStmt.all(
    reqPath,
    reqPath === '' ? '%' : `${escapeLike(reqPath)}/%`,
    limit
  ));

  res.json({
    items: items,
    total: items.length,
  });
});

/**
 * Convert an ISO date/timestamp string to Unix seconds (0 when unknown).
 */
function toUnixSeconds(value) {
  if (value == null) return 0;
  const time = new Date(value).getTime();
  return Number.isFinite(time) ? time / 1000 : 0;
}

/**
 * Returns: [ { path, type, size, mtime, idx, count }, ... ]
 *
 * `path` may name either a directory or a multi-song file. When it names a
 * multi-song file, this returns that file's sub-tunes as `type: 'file'` rows
 * (each with a `subtune` index), which is how the client drills into a
 * "song folder". A file with two or more sub-tunes appears in its parent
 * directory as `type: 'songfolder'`.
 */
router.get('/browse', cache1Hour, async (req, res) => {
  const { path: reqPath } = req.query;

  let idx = 0;
  if (browseLocalFilesystem) {
    try {
      const files = await fs.readdir(path.join(LOCAL_CATALOG_ROOT, reqPath), { withFileTypes: true });
      const result = files
        .filter(file => {
          const ext = path.extname(file.name).toLowerCase().slice(1);
          return (file.isDirectory() || (file.isFile() && FORMATS.includes(ext)));
        })
        .map((file, i) => {
          return {
            path: path.join(reqPath, file.name),
            size: 0,
            mtime: 0,
            type: file.isDirectory() ? 'directory' : 'file',
            idx: file.isDirectory() ? null : (idx++),
            count: 0,
          }
        });
      res.json(result);
    } catch (e) {
      res.status(404).json([]);
    }
  } else {
    const normalizedPath = reqPath ? reqPath.replace(/^\/+|\/+$/g, '') : '';

    const dirRow = getDirIdStmt.get(normalizedPath);
    if (dirRow) {
      const children = getDirChildrenStmt.all(dirRow.id, dirRow.id);

      const result = children.map((child) => {
        // A file containing multiple sub-tunes is presented as a folder.
        const isSongFolder = child.type === 'file' && child.subtune_count > 1;
        // A song folder is one row that the client expands into its sub-tunes
        // (`listingToContext` in src/util.js), so it owns `count` slots in the
        // play context: its idx is the first sub-tune's, and the counter runs
        // past the rest so the row after it still names its own slot.
        let childIdx = null;
        if (child.type !== 'directory') {
          childIdx = idx;
          idx += isSongFolder ? child.subtune_count : 1;
        }
        return {
          path: normalizedPath ? `${normalizedPath}/${child.path}` : child.path,
          type: isSongFolder ? 'songfolder' : child.type,
          size: child.size,
          // Prefer the parsed release date over the file system mtime.
          mtime: toUnixSeconds(child.release_date || child.mtime),
          idx: childIdx,
          count: isSongFolder ? child.subtune_count : (child.count || 0),
          url: child.type === 'directory' || isSongFolder
            ? null
            : `/?play=${encodeURIComponent(child.song_id)}`,
        };
      });

      res.json(result);
    } else if (getSubtunesStmt.all(normalizedPath).length > 0) {
      // The requested path is a multi-song file: return its sub-tunes.
      const song = getSongByPathStmt.get(normalizedPath);
      const subtunes = getSubtunesStmt.all(normalizedPath);

      const result = subtunes.map((sub) => {
        // A sub-tune lives inside its parent file, so reuse the file's size.
        // For the date, prefer per-sub-tune metadata, then file metadata, then
        // the file system mtime.
        const dateValue = sub.date || song?.release_date || song?.mtime;
        return {
          path: normalizedPath,
          type: 'file',
          name: sub.title || `Tune ${sub.subtune + 1}`,
          song_id: song ? song.song_id : null,
          subtune: sub.subtune,
          durationMs: sub.length_ms,
          size: song ? song.file_size : 0,
          mtime: toUnixSeconds(dateValue),
          idx: idx++,
          count: 0,
          url: song
            ? `/?play=${encodeURIComponent(song.song_id)}&subtune=${sub.subtune}`
            : null,
        };
      });

      res.json(result);
    } else {
      res.json([]);
    }
  }
});

/**
 * Returns entire row from hvsc_files table or 404 if SID not found.
 * {
 *   name: string,
 *   author: string,
 *   copyright: string,
 *   lengths: string
 * }
 */
router.get('/hvsc', cache1Hour, async (req, res) => {
  const { sidHash } = req.query;
  if (!sidHash) return res.status(500).send('Missing sidHash');
  const meta = getSidMetadataByHashStmt.get(sidHash);
  if (!meta) return res.status(404).send('SID not found. Wrong SID hash version?');
  meta.image_url = await getCsdbImageUrl(meta.csdbid);
  res.json(meta);
});

const axiosCsdb = axios.create();

axiosRetry(axiosCsdb, {
  retries: 3, // Number of retries
  retryDelay: () => 1000, // Fixed delay
  retryCondition: (error) => {
    // Only retry if the status code is 503
    return error.response?.status === 503;
  },
  // Ensure we don't retry on successful responses or other errors
  shouldResetTimeout: true,
});

async function getCsdbSidXml(csdbid) {
  let xml = getCsdbXmlStmt.pluck().get(csdbid);
  if (xml) {
    console.log(`Found cached CSdb entry for ${csdbid}.`);
  } else {
    const csdbUrl = `https://csdb.dk/webservice/?type=sid&id=${csdbid}`;
    console.log('Fetching CSdb SID URL:', csdbUrl);
    const response = await axiosCsdb.get(csdbUrl);
    xml = response.data;
    const now = Math.floor(Date.now() / 1000);
    insertCsdbXmlStmt.run({ csdbid, xml, now });
    console.log(`Wrote CSdb entry for ${csdbid}.`);
  }
  return xml;
}

async function getCsdbImageUrl(csdbid) {
  const releaseDate = (release) => {
    const month = release.ReleaseMonth || 12;
    const year = release.ReleaseYear;
    return year * 100 + month;
  };
  const xmlString = await getCsdbSidXml(csdbid);
  const jsonObj = new XMLParser({ ignoreAttributes: true }).parse(xmlString);
  let releases = jsonObj.CSDbData?.SID?.UsedIn?.Release;

  if (releases == null) return null;

  if (!Array.isArray(releases)) releases = [releases];
  const screenshotUrl = releases.sort((a, b) => {
    return releaseDate(a) - releaseDate(b);
  }).find(r => r.ScreenShot)?.ScreenShot;
  if (screenshotUrl && screenshotUrl.includes('csdb.dk')) return screenshotUrl;
}

/**
 * Returns: {
 *   songId: string,
 *   imageUrl: string|null,
 *   infoTexts: [ string, ... ],
 *   soundfont: string|null,
 *   md5: string|null,
 *   subtuneCount: number,
 *   subtuneTitle: string|null
 * }
 *
 * Pass `subtune=N` to get metadata for a specific sub-tune (its title).
 */
router.get('/metadata', cache1Hour, (req, res, next) => {
  const { path: reqPath, subtune } = req.query;
  if (!reqPath) return res.json({});

  const normalizedPath = reqPath.replace(/^\/+/, '');
  const meta = getMetadataStmt.get(normalizedPath);

  if (meta) {
    let infoTexts = [];
    if (meta.text_ids) {
      try {
        const ids = JSON.parse(meta.text_ids);
        infoTexts = ids.map(id => {
          const row = getTextContentStmt.get(id);
          return row ? row.content : null;
        }).filter(t => t !== null);
      } catch (e) {}
    }

    // TODO: these should probably be imagePath and soundfontPath, resolved to full URLs on the client.
    let imageUrl = null;
    if (meta.image_path) {
      const parts = meta.image_path.split('/');
      imageUrl = parts.map(encodeURIComponent).join('/');
    }

    let soundfont = null;
    if (meta.soundfont) {
      const parts = meta.soundfont.split('/');
      soundfont = parts.map(encodeURIComponent).join('/');
    }

    // Resolve the sub-tune title when a subtune index is supplied.
    let subtuneTitle = null;
    if (subtune !== undefined) {
      const sub = getSubtuneMetadataStmt.get(normalizedPath, parseInt(subtune, 10));
      if (sub) subtuneTitle = sub.title;
    }

    res.json({
      songId: meta.song_id,
      imageUrl: imageUrl,
      infoTexts: infoTexts,
      soundfont: soundfont,
      contributor: meta.contributor,
      md5: meta.md5,
      subtuneCount: meta.subtune_count || 1,
      subtuneTitle: subtuneTitle,
    });
  } else {
    res.json({});
  }
});

router.post('/playback',
  optionalAuth,
  express.json({ limit: '10kb' }),
  validate(PlaybackSchema),
  (req, res) => {
  const { songId, subtune, durationMs } = req.body;

  try {
    // Don't record a sub-tune the file doesn't have (single-song files only
    // have sub-tune 0); such rows can never be played from the charts.
    const song = getSubtuneCountBySongIdStmt.get(songId);
    if (song && subtune >= (song.subtune_count || 1)) {
      return res.status(400).json({ error: 'Invalid sub-tune' });
    }

    const now = Math.floor(Date.now() / 1000);
    insertPlaybackStmt.run(req.userId, req.ip, songId, subtune, now, durationMs);
    res.json({ success: true });
  } catch (e) {
    console.error('Error logging playback:', e);
    res.status(500).json({ error: 'Failed to log playback' });
  }
});

const topCache = new LRUCache({
  max: 10,
  ttl: 1000 * 60 * 60, // 60 minutes
});

/**
 * Returns: { items: [ { song_id, plays, title, artist, game, system, path, file_size, mtime }, ... ], total }
 */
router.get('/top', optionalAuth, (req, res) => {
  const metric = req.query.metric || 'plays';
  const scope = req.query.scope || 'global';
  const range = req.query.range || 'all';
  const limit = Math.min(Math.max(parseInt(req.query.limit, 10) || 10, 1), 100);

  const now = Math.floor(Date.now() / 1000);
  const sinceTimestamp = range === 'month' ? now - 30 * 24 * 60 * 60 : 0;
  const overFetch = limit + 50;

  if (metric === 'favorites') {
    const cacheKey = `favorites-${limit}`;
    const cached = topCache.get(cacheKey);
    if (cached) {
      return res.json(cached);
    }

    const items = getTopFavoritesStmt.all(overFetch, limit);
    const responseData = {
      items,
      total: items.length,
    };
    topCache.set(cacheKey, responseData);
    return res.json(responseData);
  }

  if (scope === 'user') {
    if (!req.userId) {
      return res.status(401).json({ error: 'Unauthorized' });
    }
    const items = getUserTopStmt.all(req.userId, sinceTimestamp, overFetch, limit);
    return res.json({
      items,
      total: items.length,
    });
  }

  const cacheKey = `${range}-${limit}`;
  const cached = topCache.get(cacheKey);
  if (cached) {
    return res.json(cached);
  }

  const items = getGlobalTopStmt.all(sinceTimestamp, overFetch, limit);
  const responseData = {
    items,
    total: items.length,
  };
  topCache.set(cacheKey, responseData);
  return res.json(responseData);
});

/**
 * Returns: { favorites: [ { songId, href, mtime, title, artist }, ... ] }
 */
router.get('/user/favorites', requireAuth, (req, res) => {
  const row = getFavoritesStmt.get(req.userId);
  let favorites = [];
  if (row && row.items) {
    try {
      favorites = JSON.parse(row.items);
    } catch (e) {
      console.error('Error parsing favorites JSON:', e);
    }
  }
  res.json({
    favorites,
  });
});

// Route to add a favorite. Requires auth middleware.
router.post(
  '/user/favorites/add',
  requireAuth,
  express.json({ limit: '10kb' }),
  validate(FavoriteSchema),
  (req, res) => {
    const { href, path, subtune, mtime } = req.body;

    try {
      let songPath = path || href
        .replace('https://gifx.co/music/', '')
        .replace('http://localhost:8080/catalog/', '');
      try { songPath = decodeURIComponent(songPath); } catch (e) {}

      const song = getSongByPathStmt.get(songPath);
      if (!song) {
        return res.status(404).json({ error: 'Song not found' });
      }

      if (subtune >= (song.subtune_count || 1)) {
        return res.status(400).json({ error: 'Sub-tune out of range' });
      }

      // Resolve the path to a song ID on insertion.
      addFavoriteByPathStmt.run({
        userId: req.userId,
        path: songPath,
        subtune,
        now: mtime,
      });
      // Return the entry already decorated, exactly as /user/favorites does, so
      // the client can splice this one item in instead of re-reading the whole
      // list (which could clobber a newer, still-pending toggle).
      res.json({
        success: true,
        favorite: {
          songId: song.song_id,
          mtime,
          path: song.path,
          subtune,
          href: `https://gifx.co/music/${song.path}`,
          size: song.file_size,
          subtuneCount: song.subtune_count || 1,
          subtuneTitle: getSubtuneMetadataStmt.get(song.path, subtune)?.title ?? null,
        },
      });
    } catch (e) {
      console.error('Error adding favorite:', e);
      res.status(500).json({ error: 'Failed to add favorite' });
    }
  });

// Route to remove a favorite. Requires auth middleware.
router.post(
  '/user/favorites/remove',
  requireAuth,
  express.json({ limit: '10kb' }),
  validate(FavoriteSchema),
  (req, res) => {
    const { href, path, subtune } = req.body;

    try {
      const now = Math.floor(Date.now() / 1000);
      let songPath = path || href
        .replace('https://gifx.co/music/', '')
        .replace('http://localhost:8080/catalog/', '');
      try { songPath = decodeURIComponent(songPath); } catch (e) {}
      removeFavoriteByPathStmt.run({
        userId: req.userId,
        path: songPath,
        subtune,
        now,
      });
      // Get rows affected
      const rowsAffected = removeFavoriteByPathStmt.changes;
      if (rowsAffected === 0) {
        return res.status(404).json({ error: 'Favorite not found' });
      }

      res.json({ success: true, removed: rowsAffected });
    } catch (e) {
      console.error('Error removing favorite:', e);
      res.status(500).json({ error: 'Failed to remove favorite' });
    }
  });

// Get Settings
router.get('/user/settings', requireAuth, (req, res) => {
  const row = getUserSettingsStmt.get(req.userId);
  const settings = JSON.parse(row.settings);
  res.json(settings);
});

// Update Settings
router.post(
  '/user/settings',
  requireAuth,
  express.json({ limit: '10kb' }),
  validate(SettingsSchema),
  (req, res) => {
    const settings = req.body;
    try {
      replaceUserSettingsStmt.run(JSON.stringify(settings), req.userId);
      res.json({ success: true });
    } catch (e) {
      console.error('Error updating settings:', e);
      res.status(500).json({ error: 'Failed to update settings' });
    }
});

// Chrome dev tools requests this for the "Workspace" feature.
app.get('/.well-known/appspecific/com.chrome.devtools.json', (req, res) => res.sendStatus(404));

// Static file fallback - should be handled by Nginx in production
app.use(express.static(LOCAL_CLIENT_BUILD_ROOT));
app.use('/catalog', cache1Hour, fixSidMimeType, express.static(LOCAL_CATALOG_ROOT));
app.use('/soundfonts', cache1Hour, express.static(LOCAL_SOUNDFONT_ROOT));

// Handle client-side routing, return all requests to index.html.
const clientRoutes = [
  '/',
  '/index.html',
  '/browse{/*path}',
  '/favorites',
  '/top',
  '/local'
];
app.get(clientRoutes, cache1Hour, async (req, res) => {
  let html = indexHtmlProd;

  if (isDev) {
    try {
      // Fetch the latest template from Webpack Dev Server
      const response = await fetch(`http://localhost:${WDS_PORT}/index.html`);
      html = await response.text();
    } catch (e) {
      return res.status(500).send('Webpack Dev Server is not responding. Is it running on port 3000?');
    }
  }

  if (!html) {
    return res.status(404).send(`${indexFilename} not found`);
  }

  const {
    title,
    description,
    url,
    image,
    songId,
    scriptTag,
    schemaJsonLd,
    noscriptContent,
  } = getHtmlInjectionsForRequest(req);

  const previewImage = songId ? `https://chiptune.app/preview?s=${encodeURIComponent(songId)}` : image;
  const defaultNoscript = 'You need to enable JavaScript to run this app.';

  const finalHtml = html
    .replace(/__TITLE__/g, title)
    .replace(/__DESCRIPTION__/g, description)
    .replace(/__URL__/g, url)
    .replace(/__IMAGE__/g, previewImage)
    .replace(/<chip-config><\/chip-config>/g, scriptTag)
    .replace(/__SCHEMA_JSON_LD__/g, schemaJsonLd || '')
    .replace(/__NOSCRIPT_CONTENT__/g, noscriptContent || defaultNoscript);

  res.send(finalHtml);
});

app.use((err, req, res, _next) => {
  console.error('Error processing request:', req.url, err);
  res.status(500).send('Server error');
});

app.listen(port, hostname, () => {
  console.log('Server running at http://%s:%s', hostname, port);
});

function getHtmlInjectionsForRequest(req) {
  // 1. Dedicated isolation for /top (prevent query param title/canonical leakage)
  if (req.path === '/top') {
    return {
      title: 'Top Charts - Most Played & Favorited Chiptunes',
      description: 'Explore the all-time and monthly most played and most favorited chiptune tracks and video game music on Chip Player JS.',
      url: 'https://chiptune.app/top',
      image: 'https://chiptune.app/chip-player-1200x640.png',
      songId: null,
      scriptTag: '',
      schemaJsonLd: '',
      noscriptContent: '<div class="seo-crawler-content"><h1>Top Charts</h1><p>Most played and most favorited chiptune tracks on Chip Player JS.</p></div>',
    };
  }

  // 2. Directory browse pages (/browse and /browse/*)
  if (req.path === '/browse' || req.path.startsWith('/browse/')) {
    const dirMeta = resolveDirectoryMetadata(req.path);
    return {
      title: dirMeta.title,
      description: dirMeta.description,
      url: dirMeta.url,
      image: dirMeta.image,
      songId: null,
      scriptTag: '',
      schemaJsonLd: dirMeta.schemaJsonLd,
      noscriptContent: dirMeta.noscriptContent,
    };
  }

  let url = `https://chiptune.app${req.path}`;
  let song;
  let image = 'https://chiptune.app/chip-player-1200x640.png';
  let title = 'Chip Player JS';
  let description = 'An online MIDI and VGM music player with SoundFont support. Over 380,000 songs, focused on performance and nostalgia. Play VGM, SPC, NSF, S3M, XM, MOD and more.';

  let songId = null;
  let scriptTag = '';
  let schemaJsonLd = '';
  let noscriptContent = '';

  // Extract 'play' param from query string
  const play = req.query.play;

  if (play) {
    if (/^[A-Za-z0-9_-]{3,8}$/.test(play)) {
      // play param is a song ID
      song = getSongByIdStmt.get(`${play}%`);

      // more HVSC hacks ···
      if (song && song.path.endsWith('.sid')) {
        const sidMeta = getSidMetadataByPathStmt.get('_' + song.path);
        if (sidMeta) {
          song.title = sidMeta.name;
          song.artist = sidMeta.author;
        }
      }

    } else {
      // play param is a path
      const reqPath = play.replace(/^\/+/, '');
      song = getSongByPathStmt.get(reqPath);
    }

    if (song) {
      // A sub-tune is a song of its own: a valid subtune > 0 gets its own
      // canonical, title and structured data, so it can be indexed and shared
      // on its own. Sub-tune 0 is the file page (the bare ?play= URL), which
      // keeps the two forms from duplicating each other.
      const subtuneCount = song.subtune_count || 1;
      const requestedSubtune = parseInt(req.query.subtune, 10);
      const subtune = Number.isInteger(requestedSubtune) && requestedSubtune > 0 && requestedSubtune < subtuneCount
        ? requestedSubtune
        : 0;

      url = `https://chiptune.app/?play=${encodeURIComponent(song.song_id)}`
        + (subtune > 0 ? `&subtune=${subtune}` : '');

      const chipConfig = {
        songId: song.song_id,
        songPath: song.path,
        subtune,
        // How many songs the file holds: > 1 means Browse presents it as a
        // "song folder", so a share link lands inside it instead of on the
        // containing directory. Sent for plain files too, so the client never
        // has to read meaning into an absent field.
        subtuneCount,
      };
      scriptTag = `<script>window.__chipConfig = ${JSON.stringify(chipConfig)};</script>`;

      // The sub-tune's own name when the format gives one, else "Tune N".
      const subtuneTitle = subtune > 0
        ? (getSubtuneMetadataStmt.get(song.path, subtune)?.title || `Tune ${subtune + 1}`)
        : null;
      const songTitle = subtuneTitle || song.title;

      if (songTitle) {
        title = songTitle;
        if (song.artist) title += ` - ${song.artist}`;
        else if (song.game) title += ` - ${song.game}`;
      } else {
        title = path.basename(song.path || play);
      }

      const parts = [];
      if (song.game) parts.push(song.game);
      if (song.system) parts.push(song.system);
      if (song.copyright) parts.push(song.copyright);
      if (parts.length > 0) description = parts.join(' · ');

      if (song.image_path) {
        const parts = song.image_path.split('/');
        const encodedPath = parts.map(encodeURIComponent).join('/');
        image = `https://chiptune.app/catalog/${encodedPath}`;
      }
      songId = song.song_id;

      // MusicRecording structured data
      const songSchema = {
        '@context': 'https://schema.org',
        '@type': 'MusicRecording',
        name: songTitle || path.basename(song.path || play),
        url: url,
      };
      if (song.artist) {
        songSchema.byArtist = { '@type': 'MusicGroup', name: song.artist };
      }
      if (song.game) {
        songSchema.inAlbum = { '@type': 'MusicAlbum', name: song.game };
      }
      schemaJsonLd = `<script type="application/ld+json">${JSON.stringify(songSchema)}</script>`;

      noscriptContent = `<div class="seo-crawler-content"><h1>${title}</h1><p>${description}</p><p><a href="${url}">Play track</a></p></div>`;
    }
  }

  return {
    title,
    description,
    url,
    image,
    songId,
    scriptTag,
    schemaJsonLd,
    noscriptContent,
  };
}
