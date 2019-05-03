#!/usr/bin/env node

/**
 *
 * Chip Player JS
 * ᴍ ɪ ᴄ ʀ ᴏ s ᴇ ʀ ᴠ ᴇ ʀ
 *
 * Matt Montag · March 2019
 *
 */

const http = require('http');
const URL = require('url');
const TrieSearch = require('trie-search');
const { performance } = require('perf_hooks');

const CATALOG_PATH = './catalog.json';
const catalog = require(CATALOG_PATH);
const DIRECTORIES_PATH = './directories.json';
const directories = require(DIRECTORIES_PATH);

console.log('Found %s entries in %s.', Object.entries(directories).length, DIRECTORIES_PATH);

if (!Array.isArray(catalog)) {
  console.log('%s must be a json file with root-level array.', CATALOG_PATH);
  process.exit(1);
}

const trie = new TrieSearch('file', {
  indexField: 'id',
  idFieldOrFunction: 'id',
  splitOnRegEx: /[^A-Za-z0-9]/g,
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
};

http.createServer(function (req, res) {
  const url = URL.parse(req.url, true);
  const lastPathComponent = url.pathname.split('/').pop();
  const route = routes[lastPathComponent];
  if (route) {
    const params = url.query || {};
    try {
      const json = route(params);
      if (json) {
        res.writeHead(200, {
          // If running behind a proxy such as nginx,
          // configure it to ignore this CORS header
          'Access-Control-Allow-Origin': '*',
          'Content-Type': 'application/json',
        });
        res.end(JSON.stringify(json));

        return;
      } else {
        res.writeHead(404);
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
}).listen(8080, 'localhost');

console.log('Server running at http://localhost:8080/.');