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
const fs = require('fs').promises;
const path = require('path');
const express = require('express');
const { performance } = require('perf_hooks');
const { createProxyMiddleware } = require('http-proxy-middleware');
const { dbStatements } = require('./database.js');
const { Canvas, loadImage } = require('skia-canvas');
const { LRUCache } = require('lru-cache');

const {
  searchStmt,
  getDirIdStmt,
  getDirChildrenStmt,
  getMetadataStmt,
  getTextContentStmt,
  getShuffleStmt,
  getTotalStmt,
  getSongInfoStmt,
  getSongImageByIdStmt,
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

if (!LOCAL_CATALOG_ROOT || !LOCAL_CLIENT_BUILD_ROOT) {
  console.error('Missing required environment variables. See .env file for details.');
  process.exit(1);
}

// Only used when browsing local filesystem
const { FORMATS } = browseLocalFilesystem ? require('../src/config/index.js') : [];

const app = express();
const router = express.Router();

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
        pathname.startsWith('/static') ||
        pathname.startsWith('/ws') ||
        pathname.startsWith('/sockjs-node') ||
        /\.hot-update\.(json|js)$/.test(pathname)
      );
    }
  });

  app.use(devProxy);

  // Serve static files (handled by Express as fallback to Nginx)
  // app.use(express.static(path.join(__dirname, LOCAL_CLIENT_BUILD_ROOT)));
}

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
  // Preserve for debugging purposes.
  // console.log('----', req.url);
  res.header('Access-Control-Allow-Origin', '*');
  next();
});

// --- API Routes ---

app.use('/api', router);

// --- Preview Image Cache ---
const previewCache = new LRUCache({
  max: 25,
});

let bgImage = null;

async function getBgImage() {
  if (bgImage) return bgImage;
  
  let bgPath = path.join(LOCAL_CLIENT_BUILD_ROOT, 'chip-player.png');
  try {
    await fs.access(bgPath);
  } catch (e) {
    // Fallback for development or if build is missing
    bgPath = path.join(__dirname, '../public/chip-player.png');
  }
  
  bgImage = await loadImage(bgPath);
  return bgImage;
}

app.get('/preview', async (req, res) => {
  const { s: songId } = req.query;
  if (!songId) {
    return res.status(400).send('Missing song ID');
  }

  try {
    const song = getSongImageByIdStmt.get(`${songId}*`);
    
    // Check cache if we have a song image path
    if (song && song.image_path) {
      const cachedBuffer = previewCache.get(song.image_path);
      if (cachedBuffer) {
        res.set('Content-Type', 'image/png');
        return res.send(cachedBuffer);
      }
    }

    const bg = await getBgImage();
    const canvas = new Canvas(bg.width, bg.height);
    const ctx = canvas.getContext('2d');
    
    // Draw background
    ctx.drawImage(bg, 0, 0);

    if (song && song.image_path) {
      const songImagePath = path.join(LOCAL_CATALOG_ROOT, song.image_path);
      try {
        const songImage = await loadImage(songImagePath);
        
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

        // Center the image
        const x = (canvas.width - drawWidth) / 2;
        const y = (canvas.height - drawHeight) / 2;

        // Draw yellow outline with dropshadow
        ctx.fillStyle = '#ffff00';
        ctx.shadowColor = '#000040';
        ctx.shadowBlur = 60;
        ctx.shadowOffsetY = 15;
        const lw = 3;
        ctx.fillRect(x - lw, y - lw, drawWidth + lw * 2, drawHeight + lw * 2);
        ctx.fillRect(x - lw, y - lw, drawWidth + lw * 2, drawHeight + lw * 2);
        ctx.shadowColor = 'transparent';

        ctx.drawImage(songImage, x, y, drawWidth, drawHeight);
      } catch (e) {
        console.error('Error loading song image:', e);
      }
    }

    const buffer = await canvas.toBuffer('png');
    
    // Cache the result if we have a song image path
    if (song && song.image_path) {
      previewCache.set(song.image_path, buffer);
    }

    res.set('Content-Type', 'image/png');
    res.send(buffer);
  } catch (e) {
    console.error('Preview generation error:', e);
    res.status(500).send('Error generating preview');
  }
});

router.get('/search', (req, res) => {
  const { limit = 100, query } = req.query;
  const start = performance.now();

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
  const result = getTotalStmt.get();
  res.set('Cache-Control', 'public, max-age=3600');
  res.json({ total: result.total });
});

router.get('/random', (req, res) => {
  const limit = parseInt(req.query.limit, 10) || 1;
  const items = getShuffleStmt.all('/%', limit);
  res.json({
    items: items,
    total: items.length,
  });
});

router.get('/shuffle', (req, res) => {
  const limit = parseInt(req.query.limit, 10) || 100;
  const reqPath = (req.query.path || '').replace(/^\/+|\/+$/g, '');
  items = getShuffleStmt.pluck().all(`${reqPath}%`, limit);

  res.json({
    items: items,
    total: items.length,
  });
});

router.get('/browse', async (req, res) => {
  const { path: reqPath } = req.query;
  res.set('Cache-Control', 'public, max-age=3600');

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

      const result = children.map((child) => ({
        path: normalizedPath ? `${normalizedPath}/${child.path}` : child.path,
        type: child.type,
        size: child.size,
        mtime: typeof child.mtime === 'string' ? new Date(child.mtime).getTime() / 1000 : 0,
        idx: child.type === 'directory' ? null : (idx++),
        count: child.count || 0
      }));

      res.json(result);
    } else {
      res.json([]);
    }
  }
});

router.get('/metadata', (req, res) => {
  const { path: reqPath } = req.query;
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

    let imageUrl = null;
    if (meta.image_path) {
      const parts = meta.image_path.split('/');
      const encodedPath = parts.map(encodeURIComponent).join('/');
      imageUrl = `/catalog/${encodedPath}`;
    }

    let soundfont = null;
    if (meta.soundfont) {
      const parts = meta.soundfont.split('/');
      const encodedPath = parts.map(encodeURIComponent).join('/');
      soundfont = `/catalog/${encodedPath}`;
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

// Chrome dev tools requests this for the "Workspace" feature.
app.get('/.well-known/appspecific/com.chrome.devtools.json', (req, res) => res.sendStatus(404));

// Static file fallback - should be handled by Nginx in production
app.use(express.static(LOCAL_CLIENT_BUILD_ROOT));
app.use('/catalog', express.static(LOCAL_CATALOG_ROOT));
app.use('/soundfonts', express.static(LOCAL_SOUNDFONT_ROOT));

// Handle client-side routing, return all requests to index.html.
// This must be the LAST route.
app.get('/{*splat}', async (req, res) => {
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

  const { title, description, url, image, songId } = getMetaTagsForRequest(req);
  const previewImage = songId ? `https://chiptune.app/preview?s=${encodeURIComponent(songId)}` : image;
  const finalHtml = html
    .replace(/__TITLE__/g, title)
    .replace(/__DESCRIPTION__/g, description)
    .replace(/__URL__/g, url)
    .replace(/__IMAGE__/g, previewImage);

  res.send(finalHtml);
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
  let image = 'https://chiptune.app/chip-player.png';
  let title = 'Chip Player JS';
  let description = 'Web-based music player for chiptune formats.';
  let songId = null;

  // Extract 'play' param from query string
  const playPath = req.query.play;

  if (playPath) {
    const reqPath = playPath.replace(/^\/+/, '');
    try {
      const song = getSongInfoStmt.get(reqPath);
      if (song) {
        if (song.title) {
          title = song.title;
          if (song.artist) title += ` - ${song.artist}`;
          else if (song.game) title += ` - ${song.game}`;
        } else {
          title = path.basename(reqPath);
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
      }
    } catch (e) {
      // Ignore DB errors for meta tags
    }
  }

  return {
    title,
    description,
    url,
    image,
    songId,
  };
}
