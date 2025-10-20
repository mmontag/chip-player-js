#!/usr/bin/env node

/**
 *
 * Chip Player JS
 * ᴍ ɪ ᴄ ʀ ᴏ s ᴇ ʀ ᴠ ᴇ ʀ
 *
 * Matt Montag · March 2019
 *
 */

const fs = require('fs');
const URL = require('url');
const glob = require('glob');
const path = require('path');
const http = require('http');
const crypto = require('crypto');
const TrieSearch = require('trie-search');
const { performance } = require('perf_hooks');
const { sampleSize } = require('lodash');
// Only used in DEV environment
const { FORMATS } = process.env.DEV ? require('../src/config/index.js') : [];

const CATALOG_PATH = './catalog.json';
const catalog = require(CATALOG_PATH);
const DIRECTORIES_PATH = './directories.json';
const directories = require(DIRECTORIES_PATH);

const PUBLIC_CATALOG_URL = process.env.DEV ?
  'http://localhost:8000/catalog' :
  'https://gifx.co/music';
function resolveCatalogRoot(p) {
  if (!p) return p;
  const absPath = path.resolve(p);
  try {
    return fs.realpathSync(absPath);
  } catch (e) {
    return absPath;
  }
}

const DEFAULT_LOCAL_CATALOG = resolveCatalogRoot(path.resolve(__dirname, '../catalog'));
const LOCAL_CATALOG_ROOT = process.env.DEV ?
  resolveCatalogRoot(process.env.LOCAL_CATALOG_ROOT || DEFAULT_LOCAL_CATALOG) :
  '/var/www/gifx.co/public_html/music';

const sf2Regex = /SF2=(.+?)\.sf2/;

console.log('Local catalog at %s', LOCAL_CATALOG_ROOT);
console.log('Found %s entries in %s.', Object.entries(directories).length, DIRECTORIES_PATH);

if (!Array.isArray(catalog)) {
  console.log('%s must be a json file with root-level array.', CATALOG_PATH);
  process.exit(1);
}

const HEADERS = {
  // If running behind a proxy such as nginx,
  // configure it to ignore this CORS header
  'Access-Control-Allow-Origin': '*',
  'Content-Type': 'application/json',
};

const trie = new TrieSearch('file', {
  indexField: 'id',
  idFieldOrFunction: 'id',
  splitOnRegEx: /[^a-zA-Z0-9]|(?<=[a-z])(?=[A-Z])/,
});
const start = performance.now();
const files = catalog.map((file, i) => ({id: i, file: file}));
trie.addAll(files);
const time = (performance.now() - start).toFixed(1);
console.log('Added %s items (%s tokens) to search trie in %s ms.', files.length, trie.size, time);

const routes = {
  'search': async (params) => {
    const limit = parseInt(params.limit, 10);
    const query = params.query;
    const start = performance.now();
    let items = trie.get(query, TrieSearch.UNION_REDUCER) || [];
    const total = items.length;
    if (limit) items = items.slice(0, limit);
    // Add directory depth to items for sorting
    items = items.map(item => { item.depth = item.file.split('/').length; return item; });
    items.sort((a, b) => {
      if (a.depth !== b.depth) return a.depth - b.depth;
      return a.file.localeCompare(b.file);
    });
    const time = (performance.now() - start).toFixed(1);
    console.log('Returned %s results in %s ms.', items.length, time);
    return {
      items: items,
      total: total,
    };
  },

  'total': async (params) => {
    return { total: files.length };
  },

  'random': async (params) => {
    const limit = parseInt(params.limit, 10) || 1;
    const idx = Math.floor(Math.random() * files.length);
    const items = files.slice(idx, idx + limit);
    return {
      items: items,
      total: items.length,
    };
  },

  'shuffle': async (params) => {
    const limit = parseInt(params.limit, 10) || 100;
    let path = params.path || '';
    let items = catalog;
    if (path) {
      path = path.replace(/^\/+|\/+$/g, '') + '/';
      items = catalog.filter(file => file.startsWith(path));
    }
    const sampled = sampleSize(items, limit);
    return {
      items: sampled,
      total: sampled.length,
    };
  },

  'browse': async (params) => {
    /*
      [
        {
          "path": "/Classical MIDI/Balakirev/Islamey – Fantaisie Orientale (G. Giulimondi).mid",
          "size": 54602,
          "type": "file",
          "idx": 0
        },
        {
          "path": "/Classical MIDI/Balakirev/Islamey – Fantaisie Orientale (W. Pepperdine).mid",
          "size": 213866,
          "type": "file",
          "idx": 1
        }
      ]
    */
    if (process.env.DEV) {
      const files = fs.readdirSync(path.join(LOCAL_CATALOG_ROOT, params.path), { withFileTypes: true });
      return files
        .filter(file => {
          // Get lowercase file extension, without the dot
          const ext = path.extname(file.name).toLowerCase().slice(1);
          return (file.isDirectory() || (file.isFile() && FORMATS.includes(ext)));
        })
        .map((file, i) => {
          return {
            path: path.join(params.path, file.name),
            size: 999,
            mtime: 1665174068,
            type: file.isDirectory() ? 'directory' : 'file',
            idx: i,
          }
        });
    } else {
      return directories[params.path];
    }
  },

  'metadata': async (params) => {
    let imageUrl = null;
    let soundfont = null;
    let infoTexts = [];
    let md5 = null;
    if (params.path) {
      const { dir, name, ext } = path.parse(params.path);
      if (['.it', '.s3m', '.xm', '.mod'].includes(ext.toLowerCase())) {
        // Calculate MD5 hash of file.
        // Used to generate a link for Mod Sample Master.
        const data = fs.readFileSync(path.join(LOCAL_CATALOG_ROOT, params.path));
        const hash = crypto.createHash('md5');
        hash.update(data);
        md5 = hash.digest('hex');
      }

      // --- MIDI SoundFonts ---
      if (['.mid', '.midi'].includes(ext.toLowerCase())) {
        // 1. Check the file for SF2 meta text in first 1024 bytes (proprietary tag added by N64 MIDI script).
        const data = await new Promise((resolve, reject) => {
          const stream = fs.createReadStream(path.join(LOCAL_CATALOG_ROOT, params.path), {
            encoding: 'UTF-8',
            start: 0,
            end: 256,
          });
          stream.on('data', data => resolve(data.toString()));
          stream.on('error', () => resolve(null));
        });
        const match = data ? data.match(sf2Regex) : null;
        if (match && match[1]) {
          soundfont = `${match[1]}.sf2`;
        } else {
          // 2. Check for a filename match.
          const soundfonts = glob.sync(`${LOCAL_CATALOG_ROOT}/${dir}/${name}.sf2`, { nocase: true });
          if (soundfonts.length > 0) {
            soundfont = soundfonts[0];
          } else {
            // 3. Check for any .sf2 file in current folder.
            const soundfonts = glob.sync(`${LOCAL_CATALOG_ROOT}/${dir}/*.sf2`, { nocase: true });
            if (soundfonts.length > 0) {
              soundfont = soundfonts[0];
            }
          }
        }

        if (soundfont !== null) {
          soundfont = `${PUBLIC_CATALOG_URL}/${path.join(dir, path.basename(soundfont))}`;
        }
      }

      // --- Image and Info Text ---
      // 1. Try matching same filename for info text.
      const infoFiles = glob.sync(`${LOCAL_CATALOG_ROOT}/${dir}/${name}.{text,txt,doc}`, {nocase: true});
      if (infoFiles.length > 0) {
        infoTexts.push(fs.readFileSync(infoFiles[0], 'utf8'));
      }

      // 2. Try matching same filename for image.
      const imageFiles = glob.sync(`${LOCAL_CATALOG_ROOT}/${dir}/${name}.{gif,png,jpg,jpeg}`, {nocase: true});
      if (imageFiles.length > 0) {
        const imageFile = encodeURI(path.basename(imageFiles[0]));
        const imageDir = encodeURI(dir);
        imageUrl = `${PUBLIC_CATALOG_URL}${imageDir}/${imageFile}`;
      }

      const segments = dir.split('/');
      // 3. Walk up parent directories to find image or text.
      while (segments.length) {
        const dir = segments.join('/');
        if (imageUrl === null) {
          const imageFiles = glob.sync(`${LOCAL_CATALOG_ROOT}/${dir}/*.{gif,png,jpg,jpeg}`, {nocase: true});
          if (imageFiles.length > 0) {
            const imageFile = encodeURI(path.basename(imageFiles[0]));
            const imageDir = encodeURI(dir);
            imageUrl = `${PUBLIC_CATALOG_URL}${imageDir}/${imageFile}`;
          }
        }
        if (infoTexts.length === 0) {
          const infoFiles = glob.sync(`${LOCAL_CATALOG_ROOT}/${dir}/*.{text,txt,doc}`, {nocase: true});
          infoTexts.push(...infoFiles.map(infoFile => fs.readFileSync(infoFile, 'utf8')));
        }
        if (imageUrl !== null && infoTexts.length > 0) {
          break;
        }
        segments.pop();
      }
    }
    return {
      imageUrl: imageUrl,
      infoTexts: infoTexts,
      soundfont: soundfont,
      md5: md5,
    };
  },
};

http.createServer(async function (req, res) {
  try {
    const url = URL.parse(req.url, true);
    const params = url.query || {};

    if (url.pathname.startsWith('/files/')) {
      const relative = decodeURIComponent(url.pathname.replace(/^\/files\//, ''));
      const normalized = path.normalize(relative).replace(/^(\.\.(\/|\\|$))+/, '');
      const absolutePath = path.resolve(LOCAL_CATALOG_ROOT, normalized);
      if (!absolutePath.startsWith(LOCAL_CATALOG_ROOT)) {
        res.writeHead(403, HEADERS);
        res.end('Access denied');
        return;
      }
      fs.stat(absolutePath, (err, stats) => {
        if (err || !stats.isFile()) {
          res.writeHead(404, HEADERS);
          res.end('File not found');
          return;
        }
        res.writeHead(200, {
          ...HEADERS,
          'Content-Type': 'application/octet-stream',
          'Cache-Control': 'public, max-age=3600',
        });
        fs.createReadStream(absolutePath).pipe(res);
      });
      return;
    }

    const lastPathComponent = url.pathname.split('/').pop();
    const route = routes[lastPathComponent];
    if (route) {
      try {
        const json = await route(params);
        const headers = {...HEADERS};
        if (!['random', 'shuffle'].includes(lastPathComponent)) {
          headers['Cache-Control'] = 'public, max-age=3600';
        }
        if (json) {
          res.writeHead(200, headers);
          res.end(JSON.stringify(json) + '\n');
          return;
        } else {
          res.writeHead(404, headers);
          res.end('Not found');
        }
      } catch (e) {
        res.writeHead(500);
        res.end('Server error');
        console.log('Error processing request:', req.url, e);
      }
    } else {
      res.writeHead(404);
      res.end('Route not found');
    }
  } catch (e) {
    res.writeHead(500);
    res.end(`Server error\n${e}\n`);
  }
}).listen(8080, 'localhost');

console.log('Server running at http://localhost:8080/.');
