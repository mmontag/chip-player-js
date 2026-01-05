#!/usr/bin/env node

/**
 *
 * Chip Player JS
 * ᴀ ᴘ ɪ  s ᴇ ʀ ᴠ ᴇ ʀ
 *
 * Matt Montag · March 2019
 *
 */
require('dotenv').config();

const fs = require('fs').promises;
const path = require('path');
const express = require('express');
const { performance } = require('perf_hooks');
const Database = require('better-sqlite3');

// --- Configuration ---
const {
  PUBLIC_CATALOG_URL,
  LOCAL_CATALOG_ROOT,
  BROWSE_LOCAL_FILESYSTEM,
} = process.env;
const hostname = '127.0.0.1';
const port = process.env.PORT || 8080;
const browseLocalFilesystem = BROWSE_LOCAL_FILESYSTEM === 'true';

if (!LOCAL_CATALOG_ROOT || !PUBLIC_CATALOG_URL) {
  console.error(`
    The following environment variables must be set:

    LOCAL_CATALOG_ROOT=/path/to/your/music/archive
    PUBLIC_CATALOG_URL=http://your-website.com/catalog

    See .env.example for more details.
  `);
  process.exit(1);
}

// Only used when browsing local filesystem
const { FORMATS } = browseLocalFilesystem ? require('../src/config/index.js') : [];

// Initialize DB
const DB_PATH = path.resolve(__dirname, 'catalog.db');
let db;
try {
  db = new Database(DB_PATH, { readonly: true });
  console.log(`Connected to database at ${DB_PATH}`);
} catch (e) {
  console.warn('Could not connect to database. Ensure you have run "npm run build-music-db".');
}

// Pre-load index.html template
const indexFile = path.join(__dirname, 'index.html');
let indexHtml;
fs.readFile(indexFile, 'utf8').then(data => {
  indexHtml = data;
  console.log('Loaded index.html template.');
});

const app = express();
const router = express.Router();

router.use((req, res, next) => {
  res.header('Access-Control-Allow-Origin', '*');
  next();
});

// --- Prepared Statements ---
const searchStmt = db ? db.prepare(`
  SELECT path as file, title, artist, game, system 
  FROM music_fts 
  WHERE music_fts MATCH ? 
  ORDER BY rank 
  LIMIT ?
`) : null;

// Browse: Get directory ID by path
const getDirIdStmt = db ? db.prepare('SELECT id FROM directories WHERE path = ?') : null;

// Browse: Get children (subdirectories and files)
const getDirChildrenStmt = db ? db.prepare(`
  SELECT name as path, 'directory' as type, sort_order, total_size as size, mtime, count 
  FROM directories WHERE parent_id = ?
  UNION ALL
  SELECT filename as path, 'file' as type, sort_order, file_size as size, mtime, 0 as count
  FROM music WHERE directory_id = ?
  ORDER BY sort_order ASC
`) : null;

// Metadata: Get file info
const getMetadataStmt = db ? db.prepare(`
  SELECT m.image_id, m.text_ids, m.soundfont, m.md5, i.path as image_path
  FROM music m
  LEFT JOIN images i ON m.image_id = i.id
  WHERE m.path = ?
`) : null;

// SEO: Get full song info
const getSongInfoStmt = db ? db.prepare(`
  SELECT m.title, m.artist, m.game, m.system, m.copyright, i.path as image_path
  FROM music m
  LEFT JOIN images i ON m.image_id = i.id
  WHERE m.path = ?
`) : null;

const getTextContentStmt = db ? db.prepare('SELECT content FROM texts WHERE id = ?') : null;
const getRandomStmt = db ? db.prepare('SELECT path as file FROM music ORDER BY RANDOM() LIMIT ?') : null;
const getTotalStmt = db ? db.prepare('SELECT COUNT(*) as total FROM music') : null;

// --- Routes ---

router.get('/search', (req, res) => {
  if (!db) return res.status(503).json({ error: 'Database not available' });

  const { limit = 100, query } = req.query;
  const start = performance.now();
  
  // FTS5 Prefix Search: Append '*' to ALL terms to allow partial matches on any word.
  // e.g. "moto take" -> "moto* take*"
  const sanitizedQuery = query.replace(/"/g, '""'); 
  const ftsQuery = sanitizedQuery.trim().split(/\s+/).map(term => `${term}*`).join(' ');

  let items = [];
  try {
    items = searchStmt.all(ftsQuery, limit);
  } catch (e) {
    console.error('Search error:', e.message);
  }

  const time = (performance.now() - start).toFixed(1);
  console.log('Returned %s results for "%s" in %s ms.', items.length, query, time);
  
  res.set('Cache-Control', 'public, max-age=3600');
  res.json({
    items: items,
    total: items.length, 
  });
});

router.get('/total', (req, res) => {
  if (!db) return res.status(503).json({ error: 'Database not available' });
  const result = getTotalStmt.get();
  res.set('Cache-Control', 'public, max-age=3600');
  res.json({ total: result.total });
});

router.get('/random', (req, res) => {
  if (!db) return res.status(503).json({ error: 'Database not available' });
  const limit = parseInt(req.query.limit, 10) || 1;
  const items = getRandomStmt.all(limit);
  res.json({
    items: items,
    total: items.length,
  });
});

router.get('/shuffle', (req, res) => {
  if (!db) return res.status(503).json({ error: 'Database not available' });
  
  const limit = parseInt(req.query.limit, 10) || 100;
  let reqPath = req.query.path || '';
  
  let items;
  if (reqPath) {
    reqPath = reqPath.replace(/^\/+|\/+$/g, ''); 
    // We need to find all songs under this folder.
    // Since we store full relative path in 'path', we can use LIKE.
    const stmt = db.prepare('SELECT path as file FROM music WHERE path LIKE ? OR path = ? ORDER BY RANDOM() LIMIT ?');
    items = stmt.all(`${reqPath}/%`, reqPath, limit);
  } else {
    items = getRandomStmt.all(limit);
  }

  res.json({
    items: items,
    total: items.length,
  });
});

router.get('/browse', async (req, res) => {
  const { path: reqPath } = req.query;
  res.set('Cache-Control', 'public, max-age=3600');

  if (browseLocalFilesystem) {
    // Legacy filesystem browsing
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
            idx: i,
          }
        });
      res.json(result);
    } catch (e) {
      res.status(404).json([]);
    }
  } else {
    // Database browsing
    if (!db) return res.status(503).json({ error: 'Database not available' });
    
    const normalizedPath = reqPath ? reqPath.replace(/^\/+|\/+$/g, '') : '';
    
    const dirRow = getDirIdStmt.get(normalizedPath);
    if (dirRow) {
      const children = getDirChildrenStmt.all(dirRow.id, dirRow.id);
      
      const result = children.map((child, idx) => ({
        path: normalizedPath ? `${normalizedPath}/${child.path}` : child.path,
        type: child.type,
        size: child.size,
        mtime: typeof child.mtime === 'string' ? new Date(child.mtime).getTime() / 1000 : 0, 
        idx: idx,
        count: child.count || 0 // Include recursive count for directories
      }));
      
      res.json(result);
    } else {
      res.json([]);
    }
  }
});

router.get('/metadata', (req, res) => {
  if (!db) return res.status(503).json({ error: 'Database not available' });
  
  const { path: reqPath } = req.query;
  if (!reqPath) return res.json({});

  const normalizedPath = reqPath.replace(/^\/+/, ''); 
  const meta = getMetadataStmt.get(normalizedPath);

  if (meta) {
    // Resolve Text IDs
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

    // Resolve Image URL
    let imageUrl = null;
    if (meta.image_path) {
      const parts = meta.image_path.split('/');
      const encodedPath = parts.map(encodeURIComponent).join('/');
      imageUrl = `${PUBLIC_CATALOG_URL}/${encodedPath}`;
    }

    // Resolve Soundfont URL
    let soundfont = null;
    if (meta.soundfont) {
      const parts = meta.soundfont.split('/');
      const encodedPath = parts.map(encodeURIComponent).join('/');
      soundfont = `${PUBLIC_CATALOG_URL}/${encodedPath}`;
    }

    res.json({
      imageUrl: imageUrl,
      infoTexts: infoTexts,
      soundfont: soundfont,
      md5: meta.md5,
    });
  } else {
    res.json({});
  }
});

app.use('/', router);

// Handle client-side routing, return all requests to index.html.
app.get('/{*splat}', (req, res) => {
  const { title, description, url, image } = getMetaTagsForRequest(req);
  const html = indexHtml
    .replace(/__TITLE__/g, title)
    .replace(/__DESCRIPTION__/g, description)
    .replace(/__URL__/g, url)
    .replace(/__IMAGE__/g, image);
    res.send(html);
});

app.use((err, req, res, next) => {
  console.error('Error processing request:', req.url, err);
  res.status(500).send('Server error');
});

app.listen(port, hostname, () => {
  console.log('Server running at http://%s:%s', hostname, port);
});

function getMetaTagsForRequest(req) {
  const url = `${req.protocol}://${req.hostname}${req.originalUrl}`;
  const image = 'https://chiptune.app/chip-player.png';
  // TODO: get title and description from catalog
  const meta = {
    title: 'Chip Player JS',
    description: 'Web-based music player for chiptune formats.',
    url,
    image,
  };

  return meta;
}
