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
const TrieSearch = require('trie-search');
const { performance } = require('perf_hooks');

const CATALOG_PATH = './catalog.json';
const catalog = require(CATALOG_PATH);
const DIRECTORIES_PATH = './directories.json';
const directories = require(DIRECTORIES_PATH);

const PUBLIC_CATALOG_URL = 'https://gifx.co/music';
const LOCAL_CATALOG_ROOT = process.env.DEV ?
  '/Users/montag/Music/Chip Archive' :
  '/var/www/gifx.co/public_html/music';

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
  'search': (params) => {
    const limit = parseInt(params.limit, 10);
    const query = params.query;
    const start = performance.now();
    let items = trie.get(query, TrieSearch.UNION_REDUCER) || [];
    const total = items.length;
    if (limit) items = items.slice(0, limit);
    const time = (performance.now() - start).toFixed(1);
    console.log('Returned %s results in %s ms.', items.length, time);
    return {
      items: items,
      total: total,
    };
  },

  'total': (params) => {
    return { total: files.length };
  },

  'random': (params) => {
    const limit = parseInt(params.limit, 10) || 1;
    const idx = Math.floor(Math.random() * files.length);
    const items = files.slice(idx, idx + limit);
    const total = items.length;
    return {
      items: items,
      total: total,
    }
  },

  'browse': (params) => {
    return directories[params.path];
  },

  'image': (params) => {
    const dir = params.path;
    const images = glob.sync(`${LOCAL_CATALOG_ROOT + dir}/*.{gif,png,jpg,jpeg}`, {nocase: true});
    let imageUrl = null;
    if (images.length > 0) {
      const imageFile = encodeURI(path.basename(images[0]));
      const imageDir = encodeURI(dir);
      imageUrl = `${PUBLIC_CATALOG_URL}${imageDir}/${imageFile}`;
    }
    return {
      imageUrl: imageUrl,
    };
  },
};

http.createServer(function (req, res) {
  try {
    const url = URL.parse(req.url, true);
    const params = url.query || {};
    const lastPathComponent = url.pathname.split('/').pop();
    const route = routes[lastPathComponent];
    if (route) {
      try {
        const json = route(params);
        const headers = { ...HEADERS };
        if (lastPathComponent !== 'random') {
          headers['Cache-Control'] = 'public, max-age=3600';
        }
        if (json) {
          res.writeHead(200, headers);
          res.end(JSON.stringify(json));
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
    }
    res.writeHead(404);
    res.end('Route not found');
  } catch (e) {
    res.writeHead(500);
    res.end(`Server error\n${e}\n`);
  }
}).listen(8080, 'localhost');

console.log('Server running at http://localhost:8080/.');