#!/usr/bin/env node

/**
 *
 * Chip Player JS
 * ᴀ ᴘ ɪ  s ᴇ ʀ ᴠ ᴇ ʀ
 *
 * Matt Montag · March 2019
 *
 */

const fs = require('fs');
const glob = require('glob');
const path = require('path');
const crypto = require('crypto');
const express = require('express');
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
const LOCAL_CATALOG_ROOT = process.env.DEV ?
  '/Users/montag/Music/Chip Archive' :
  '/var/www/gifx.co/public_html/music';

const sf2Regex = /SF2=(.+?)\.sf2/;

console.log('Local catalog at %s', LOCAL_CATALOG_ROOT);
console.log('Found %s entries in %s.', Object.entries(directories).length, DIRECTORIES_PATH);

if (!Array.isArray(catalog)) {
  console.log('%s must be a json file with root-level array.', CATALOG_PATH);
  process.exit(1);
}

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

const app = express();
app.use((req, res, next) => {
  res.header('Access-Control-Allow-Origin', '*');
  next();
});

app.get('/search', async (req, res) => {
  const { limit, query } = req.query;
  const start = performance.now();
  let items = trie.search(query, TrieSearch.UNION_REDUCER) || [];
  const total = items.length;
  if (limit) items = items.slice(0, parseInt(limit, 10));
  // Add directory depth to items for sorting
  items = items.map(item => { item.depth = item.file.split('/').length; return item; });
  items.sort((a, b) => {
    if (a.depth !== b.depth) return a.depth - b.depth;
    return a.file.localeCompare(b.file);
  });
  const time = (performance.now() - start).toFixed(1);
  console.log('Returned %s results in %s ms.', items.length, time);
  res.set('Cache-Control', 'public, max-age=3600');
  res.json({
    items: items,
    total: total,
  });
});

app.get('/total', async (req, res) => {
  res.set('Cache-Control', 'public, max-age=3600');
  res.json({ total: files.length });
});

app.get('/random', async (req, res) => {
  const limit = parseInt(req.query.limit, 10) || 1;
  const idx = Math.floor(Math.random() * files.length);
  const items = files.slice(idx, idx + limit);
  res.json({
    items: items,
    total: items.length,
  });
});

app.get('/shuffle', async (req, res) => {
  const limit = parseInt(req.query.limit, 10) || 100;
  let path = req.query.path || '';
  let items = catalog;
  if (path) {
    path = path.replace(/^\/+|\/+$/g, '') + '/';
    items = catalog.filter(file => file.startsWith(path));
  }
  const sampled = sampleSize(items, limit);
  res.json({
    items: sampled,
    total: sampled.length,
  });
});

app.get('/browse', async (req, res) => {
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
  const { path: reqPath } = req.query;
  res.set('Cache-Control', 'public, max-age=3600');
  if (process.env.DEV) {
    const files = fs.readdirSync(path.join(LOCAL_CATALOG_ROOT, reqPath), { withFileTypes: true });
    const result = files
      .filter(file => {
        // Get lowercase file extension, without the dot
        const ext = path.extname(file.name).toLowerCase().slice(1);
        return (file.isDirectory() || (file.isFile() && FORMATS.includes(ext)));
      })
      .map((file, i) => {
        return {
          path: path.join(reqPath, file.name),
          size: 999,
          mtime: 1665174068,
          type: file.isDirectory() ? 'directory' : 'file',
          idx: i,
        }
      });
    res.json(result);
  } else {
    res.json(directories[reqPath]);
  }
});

app.get('/metadata', async (req, res) => {
  const { path: reqPath } = req.query;
  let imageUrl = null;
  let soundfont = null;
  let infoTexts = [];
  let md5 = null;
  if (reqPath) {
    const { dir, name, ext } = path.parse(reqPath);
    if (['.it', '.s3m', '.xm', '.mod'].includes(ext.toLowerCase())) {
      // Calculate MD5 hash of file.
      // Used to generate a link for Mod Sample Master.
      const data = fs.readFileSync(path.join(LOCAL_CATALOG_ROOT, reqPath));
      const hash = crypto.createHash('md5');
      hash.update(data);
      md5 = hash.digest('hex');
    }

    // --- MIDI SoundFonts ---
    if (['.mid', '.midi'].includes(ext.toLowerCase())) {
      // 1. Check the file for SF2 meta text in first 1024 bytes (proprietary tag added by N64 MIDI script).
      const data = await new Promise((resolve, reject) => {
        const stream = fs.createReadStream(path.join(LOCAL_CATALOG_ROOT, reqPath), {
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
  res.set('Cache-Control', 'public, max-age=3600');
  res.json({
    imageUrl: imageUrl,
    infoTexts: infoTexts,
    soundfont: soundfont,
    md5: md5,
  });
});

app.use((err, req, res, next) => {
  console.error('Error processing request:', req.url, err);
  res.status(500).send('Server error');
});

app.listen(8080, 'localhost', () => {
  console.log('Server running at http://localhost:8080/.');
});
