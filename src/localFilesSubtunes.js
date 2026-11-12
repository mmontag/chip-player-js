// Sub-tune headers for Local Files.
//
// A local file (dropped into the browser's IDBFS) has no catalog row, so the
// server's sub-tune listing cannot describe it. The sub-tune *count* and labels
// live in the file's own header, so we parse them here — but only for the
// multi-song formats, and only when there are local files to describe.
//
// The parser (`scripts/subtune-headers.js`) is loaded with a dynamic import, so
// it becomes its own webpack chunk: a user who never opens Local Files never
// downloads it, and the main bundle is untouched. Results are cached in
// localStorage keyed by (path, size, full timestamp), so re-renders and return
// visits skip the parse while a changed file is re-parsed -- including a
// same-size replacement inside one second, which whole seconds could not see.

const CACHE_KEY = 'cp.localSubtunes.v1';

let headersPromise = null;
function loadHeaders() {
  if (!headersPromise) {
    // webpackChunkName keeps the chunk name stable in build output.
    headersPromise = import(/* webpackChunkName: "subtune-headers" */ '../scripts/subtune-headers')
      .then((m) => (m && m.describeSubtunesInBuffer) ? m : (m && m.default) || m);
  }
  return headersPromise;
}

function readCache() {
  try {
    return JSON.parse(window.localStorage.getItem(CACHE_KEY)) || {};
  } catch (e) {
    return {};
  }
}

function writeCache(cache) {
  try {
    window.localStorage.setItem(CACHE_KEY, JSON.stringify(cache));
  } catch (e) {
    // Private mode / quota: play without a cache rather than fail.
  }
}

function extensionOf(path) {
  const dot = path.lastIndexOf('.');
  return dot === -1 ? '' : path.slice(dot + 1).toLowerCase();
}

// file: { path, size, mtime, mtimeMs }; read: (path) => Uint8Array.
//
// `mtime` is whole seconds (what the listing shows), which cannot tell a
// same-name, same-size replacement inside one second from the file it replaced --
// the old header (tune count and labels) would outlive the bytes. `mtimeMs` is
// the full timestamp; fall back to seconds for a caller that only has that.
function cacheKey(file) {
  const stamp = file.mtimeMs != null ? file.mtimeMs : file.mtime;
  return `${file.path}\u0000${file.size}\u0000${stamp}`;
}

async function subtunesForFile(mod, read, file, cache, fresh) {
  const key = cacheKey(file);
  if (cache[key]) return cache[key];
  const ext = extensionOf(file.path);
  let subtunes = [];
  // Only multi-song formats carry sub-tunes; skip reading the bytes otherwise
  // (a large .spc/.miniusf would be read for nothing).
  if (mod.MULTISONG_EXTENSIONS && mod.MULTISONG_EXTENSIONS.has(ext)) {
    try {
      subtunes = mod.describeSubtunesInBuffer(ext, read(file.path)) || [];
    } catch (e) {
      console.warn('Local file sub-tune parse failed:', file.path, e && e.message);
    }
  }
  cache[key] = subtunes;
  fresh.push(key);
  return subtunes;
}

/**
 * Describe every file's sub-tunes. Returns a Map of path -> subtunes[] (empty
 * for single-song or unparseable files). `read(path)` returns the file's bytes.
 */
export async function describeLocalSubtunes(files, read) {
  const cache = readCache();
  const fresh = [];
  const mod = await loadHeaders();
  const byPath = new Map();
  for (const file of files) {
    byPath.set(file.path, await subtunesForFile(mod, read, file, cache, fresh));
  }
  if (fresh.length > 0) {
    // Keep only entries still in use, so the cache cannot grow forever.
    const live = {};
    for (const file of files) {
      const key = cacheKey(file);
      if (cache[key]) live[key] = cache[key];
    }
    writeCache(live);
  }
  return byPath;
}
