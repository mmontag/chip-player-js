#!/usr/bin/env node

/**
 * Build Sitemap Suite for Chip Player JS
 *
 * Generates:
 * 1. public/sitemap-directories.xml - All 31,000+ directories in the catalog
 * 2. public/sitemap-songs-top.xml - Top-N most played & favorited songs
 * 3. public/sitemap.xml - Sitemap index referencing directory and song sitemaps
 */

const fs = require('fs');
const path = require('path');
const Database = require('better-sqlite3');

const BASE_URL = process.env.BASE_URL || 'https://chiptune.app';
const OUT_DIR = path.resolve(__dirname, '../public');
const CATALOG_DB_PATH = path.resolve(__dirname, '../server/catalog.db');
const USER_DB_PATH = path.resolve(__dirname, '../server/users.db');

// Parse --top N CLI argument (default: 50000)
const topArgIndex = process.argv.indexOf('--top');
const TOP_N_SONGS = topArgIndex !== -1 && process.argv[topArgIndex + 1]
  ? parseInt(process.argv[topArgIndex + 1], 10)
  : 50000;

function escapeXml(unsafe) {
  return String(unsafe)
    .replace(/&/g, '&amp;')
    .replace(/</g, '&lt;')
    .replace(/>/g, '&gt;')
    .replace(/"/g, '&quot;')
    .replace(/'/g, '&apos;');
}

function encodePath(rawPath) {
  return rawPath.split('/').map(encodeURIComponent).join('/');
}

console.log('Building sitemap suite for %s...', BASE_URL);
console.log('Connecting to databases:\n  Catalog: %s\n  Users:   %s', CATALOG_DB_PATH, USER_DB_PATH);

const catalogDb = new Database(CATALOG_DB_PATH, { readonly: true });
const userDb = new Database(USER_DB_PATH, { readonly: true });

const todayIso = new Date().toISOString().split('T')[0];

// 1. Generate Directory Sitemap
console.log('\nQuerying directories...');
const dirs = catalogDb.prepare(`
  SELECT path, count, parent_id
  FROM directories
  WHERE path IS NOT NULL AND path != ''
  ORDER BY id
`).all();

console.log('Found %d directories.', dirs.length);

const dirXmlLines = [
  '<?xml version="1.0" encoding="UTF-8"?>',
  '<urlset xmlns="http://www.sitemaps.org/schemas/sitemap/0.9">',
  '  <url>',
  `    <loc>${BASE_URL}/</loc>`,
  `    <lastmod>${todayIso}</lastmod>`,
  '    <changefreq>daily</changefreq>',
  '    <priority>1.0</priority>',
  '  </url>',
  '  <url>',
  `    <loc>${BASE_URL}/top</loc>`,
  `    <lastmod>${todayIso}</lastmod>`,
  '    <changefreq>daily</changefreq>',
  '    <priority>0.9</priority>',
  '  </url>',
  '  <url>',
  `    <loc>${BASE_URL}/browse</loc>`,
  `    <lastmod>${todayIso}</lastmod>`,
  '    <changefreq>weekly</changefreq>',
  '    <priority>0.9</priority>',
  '  </url>',
];

for (const dir of dirs) {
  const isTopLevel = dir.parent_id === 1;
  const priority = isTopLevel ? '0.9' : (dir.count > 100 ? '0.8' : '0.7');
  const encodedUrl = `${BASE_URL}/browse/${encodePath(dir.path)}`;
  dirXmlLines.push('  <url>');
  dirXmlLines.push(`    <loc>${escapeXml(encodedUrl)}</loc>`);
  dirXmlLines.push(`    <lastmod>${todayIso}</lastmod>`);
  dirXmlLines.push('    <changefreq>weekly</changefreq>');
  dirXmlLines.push(`    <priority>${priority}</priority>`);
  dirXmlLines.push('  </url>');
}

dirXmlLines.push('</urlset>');

const dirSitemapPath = path.join(OUT_DIR, 'sitemap-directories.xml');
fs.writeFileSync(dirSitemapPath, dirXmlLines.join('\n'), 'utf8');
console.log('Wrote %s (%d URLs)', dirSitemapPath, dirs.length + 3);

// 2. Query and Score Top-N Songs
console.log('\nQuerying top songs (plays and favorites)...');
console.time('Song scoring');

const plays = userDb.prepare(`
  SELECT song_id, COUNT(*) as count
  FROM playbacks
  WHERE song_id IS NOT NULL AND song_id != ''
  GROUP BY song_id
  ORDER BY count DESC
  LIMIT 60000
`).all();

const faves = userDb.prepare(`
  SELECT json_extract(je.value, '$.songId') as song_id, COUNT(DISTINCT pl.user_id) as count
  FROM playlists pl, json_each(pl.items) je
  WHERE pl.type = 'favorites' AND json_extract(je.value, '$.songId') IS NOT NULL
  GROUP BY song_id
  ORDER BY count DESC
  LIMIT 60000
`).all();

const scoreMap = new Map();
plays.forEach(p => scoreMap.set(p.song_id, (scoreMap.get(p.song_id) || 0) + p.count));
faves.forEach(f => scoreMap.set(f.song_id, (scoreMap.get(f.song_id) || 0) + f.count * 5));

console.log('Verifying song IDs against catalog database...');
const validCatalogSongIds = new Set(
  catalogDb.prepare('SELECT DISTINCT song_id FROM music WHERE song_id IS NOT NULL').pluck().all()
);

const topSongs = Array.from(scoreMap.entries())
  .filter(([song_id]) => validCatalogSongIds.has(song_id))
  .map(([song_id, score]) => ({ song_id, score }))
  .sort((a, b) => b.score - a.score)
  .slice(0, TOP_N_SONGS);

console.timeEnd('Song scoring');
console.log('Selected top %d songs (score range: %d down to %d).',
  topSongs.length,
  topSongs[0]?.score || 0,
  topSongs[topSongs.length - 1]?.score || 0
);

// 3. Generate Song Sitemap
const songXmlLines = [
  '<?xml version="1.0" encoding="UTF-8"?>',
  '<urlset xmlns="http://www.sitemaps.org/schemas/sitemap/0.9">',
];

for (const song of topSongs) {
  const songUrl = `${BASE_URL}/?play=${encodeURIComponent(song.song_id)}`;
  songXmlLines.push('  <url>');
  songXmlLines.push(`    <loc>${escapeXml(songUrl)}</loc>`);
  songXmlLines.push(`    <lastmod>${todayIso}</lastmod>`);
  songXmlLines.push('    <changefreq>monthly</changefreq>');
  songXmlLines.push('    <priority>0.7</priority>');
  songXmlLines.push('  </url>');
}

songXmlLines.push('</urlset>');

const songSitemapPath = path.join(OUT_DIR, 'sitemap-songs-top.xml');
fs.writeFileSync(songSitemapPath, songXmlLines.join('\n'), 'utf8');
console.log('Wrote %s (%d URLs)', songSitemapPath, topSongs.length);

// 4. Generate Sitemap Index
const indexXmlLines = [
  '<?xml version="1.0" encoding="UTF-8"?>',
  '<sitemapindex xmlns="http://www.sitemaps.org/schemas/sitemap/0.9">',
  '  <sitemap>',
  `    <loc>${BASE_URL}/sitemap-directories.xml</loc>`,
  `    <lastmod>${todayIso}</lastmod>`,
  '  </sitemap>',
  '  <sitemap>',
  `    <loc>${BASE_URL}/sitemap-songs-top.xml</loc>`,
  `    <lastmod>${todayIso}</lastmod>`,
  '  </sitemap>',
  '</sitemapindex>',
];

const indexSitemapPath = path.join(OUT_DIR, 'sitemap.xml');
fs.writeFileSync(indexSitemapPath, indexXmlLines.join('\n'), 'utf8');
console.log('Wrote %s (index pointing to directories and songs)', indexSitemapPath);

console.log('\nSitemap generation complete!');
console.log('Total URLs indexed: %s', (dirs.length + 3 + topSongs.length).toLocaleString());
