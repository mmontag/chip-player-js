const { LRUCache } = require('lru-cache');
const path = require('path');
const { dbStatements } = require('./database.js');

const {
  getDirMetadataStmt,
  getDirSampleSongsStmt,
  getDirSampleSubdirsStmt,
  getDirDirectExtensionsStmt,
  getDirSubtreeExtensionsStmt,
} = dbStatements;

/**
 * Strategy mode configuration:
 * - 'structural': Intelligent structural hierarchy titles (e.g. "Famicompo mini5 Cover (NSF)")
 * - 'path': Simplified directory path titles (e.g. "Famicompo / mini5 / Cover (NSF)")
 *
 * Easily switchable here or via process.env.SEO_METADATA_MODE ('structural' | 'path').
 */
let STRATEGY_MODE = process.env.SEO_METADATA_MODE || 'path';

function setStrategyMode(mode) {
  if (mode === 'path' || mode === 'structural') {
    STRATEGY_MODE = mode;
    directoryMetaCache.clear();
  }
}

function getStrategyMode() {
  return STRATEGY_MODE;
}

/**
 * Mapping of top-level directory names to structural pattern strategies.
 * Based on the structural taxonomy of catalog folders (not musical genres).
 */
const PREFIX_STRUCTURE_MAP = {
  // 1. Competition & Event Archives
  'Famicompo': 'compo_event',
  'Famicompo (NSFE)': 'compo_event',
  'Battle of the Bits': 'compo_event',
  'Piano E-Competition MIDI': 'compo_event',

  // 2. Artist / Composer / Project Discographies
  'Contemporary': 'artist_discography',
  'Classical MIDI': 'artist_discography',
  'MIDI': 'artist_discography',
  'MIDI Datasets': 'artist_discography',
  'ModArchives': 'artist_discography',
  'Farbrausch V2': 'artist_discography',
  'OPLArchive': 'artist_discography',

  // 3. Partitioned Artist Archives
  'High Voltage SID Collection': 'partitioned_artist',
  'The Module Collection': 'partitioned_artist',
  'keygenmusic.org': 'partitioned_artist',

  // 4. Platform-Grouped Game Soundtracks
  'VGM Rips': 'platform_game_archive',
  'vgmusic.com MIDI': 'platform_game_archive',
  'Project AY': 'platform_game_archive',

  // 5. Flat Console Game Collections
  'Nintendo SNES': 'flat_game_collection',
  'Nintendo 64': 'flat_game_collection',
  'Nintendo 64 (SoundFont MIDI)': 'flat_game_collection',
  'Nintendo Game Boy': 'flat_game_collection',
  'Nintendo Game Boy Advance': 'flat_game_collection',
  'Nintendo (NSFE)': 'flat_game_collection',
  'Nintendo': 'flat_game_collection',
  'Sega Genesis': 'flat_game_collection',
  'Sega Master System': 'flat_game_collection',
  'Sega Game Gear': 'flat_game_collection',
  'Jazz MIDI': 'flat_game_collection',
  'OnlyMIDIs': 'flat_game_collection',

  // 6. Catalog Disc Numbered Archives
  'Roland SMF MIDI Disks': 'catalog_release_archive',
  'Tune 1000 SMF MIDI Disks': 'catalog_release_archive',

  // 7. Deep Thematic & Multi-Tier Archives
  'Game MIDI': 'deep_thematic_archive',
  'Game Mods': 'deep_thematic_archive',
  'Demo MIDI': 'deep_thematic_archive',
  'Sound Canvas MIDI Collection': 'deep_thematic_archive',
  'Sharp X68000 MDX': 'deep_thematic_archive',
  'Theme MIDI': 'deep_thematic_archive',
  'Ultimate Mod Collection': 'deep_thematic_archive',
  'Modland': 'deep_thematic_archive',
};

// Alias for backwards compatibility
const PREFIX_STRATEGY_MAP = PREFIX_STRUCTURE_MAP;

/**
 * Normalization map from file extensions to standard audio acronyms.
 */
const EXTENSION_MAP = {
  mid: 'MIDI',
  mus: 'MIDI',
  spc: 'SPC',
  nsf: 'NSF',
  nsfe: 'NSFE',
  vgm: 'VGM',
  vgz: 'VGM',
  miniusf: 'USF',
  usf: 'USF',
  minigsf: 'GSF',
  gsf: 'GSF',
  sid: 'SID',
  mdx: 'MDX',
  v2m: 'V2M',
  ay: 'AY',
  gbs: 'GBS',
  mod: 'MOD',
  xm: 'XM',
  s3m: 'S3M',
  it: 'IT',
  hes: 'HES',
  kss: 'KSS',
  sap: 'SAP',
};

const directoryMetaCache = new LRUCache({
  max: 1500,
});

function escapeHtml(str) {
  if (!str) return '';
  return String(str)
    .replace(/&/g, '&amp;')
    .replace(/</g, '&lt;')
    .replace(/>/g, '&gt;')
    .replace(/"/g, '&quot;')
    .replace(/'/g, '&#039;');
}

function truncateString(str, maxLen = 60) {
  if (!str || str.length <= maxLen) return str;
  return str.slice(0, maxLen - 1).trim() + '…';
}

function formatSongList(songs, count = 3) {
  const titles = songs
    .map(s => s.title || s.filename?.replace(/\.[^/.]+$/, ''))
    .filter(Boolean)
    .slice(0, count);
  if (titles.length === 0) return '';
  return titles.join(', ');
}

/**
 * Resolves exact formats contained under a directory by querying catalog.db.
 * Never guesses or assumes formats based on collection name.
 */
function resolveFormats(dirId, dirPath) {
  let rows = getDirDirectExtensionsStmt.all(dirId);
  if (rows.length === 0 && dirPath) {
    rows = getDirSubtreeExtensionsStmt.all(dirPath + '/', dirPath + '/\uffff');
  }
  const set = new Set();
  for (const r of rows) {
    if (r.extension) {
      const code = r.extension.toLowerCase();
      set.add(EXTENSION_MAP[code] || code.toUpperCase());
    }
  }
  return Array.from(set);
}

/**
 * Formats detected audio formats as a title suffix e.g. " (NSF)" or " (NSF, MIDI)".
 */
function formatTag(formats) {
  if (!formats || formats.length === 0) return '';
  if (formats.length > 3) return ''; // Omit on broad multi-format root containers
  return ` (${formats.join(', ')})`;
}

/**
 * Formats a simplified path title according to path depth and length:
 * 1 segment:
 *   AAA -> AAA (or A... if too long)
 * 2 segments:
 *   AAA / BBB -> AAA / B... (never an ellipsis in the middle)
 * 3 or more segments:
 *   Progressively remove middle segments starting from N - 1 and replace with ellipsis:
 *   AAA / BBB / CCC / DDD -> AAA / BBB / … / DDD -> AAA / … / DDD -> AAA / … / D...
 */
function formatSimplifiedPathTitle(pathSegments, maxLen = 60, options = {}) {
  const separator = options.separator || ' / ';
  const formats = options.formats || [];
  const fmtStr = options.includeFormat !== false ? formatTag(formats) : '';
  const targetLen = maxLen - fmtStr.length;

  if (!pathSegments || pathSegments.length === 0) return '';
  const N = pathSegments.length;

  // 1 segment: AAA -> AAA (or A... if too long)
  if (N === 1) {
    const s = pathSegments[0];
    const base = s.length <= targetLen ? s : s.slice(0, targetLen - 1).trim() + '…';
    return base + fmtStr;
  }

  // 2 segments: AAA / BBB -> AAA / B... (no middle ellipsis)
  if (N === 2) {
    const first = pathSegments[0];
    const second = pathSegments[1];
    const full = `${first}${separator}${second}`;
    if (full.length <= targetLen) {
      return full + fmtStr;
    }
    const prefix = `${first}${separator}`;
    const availForLeaf = targetLen - prefix.length;
    if (availForLeaf > 3) {
      const shortLeaf = second.slice(0, availForLeaf - 1).trim() + '…';
      return `${prefix}${shortLeaf}${fmtStr}`;
    }
    // If the first segment itself is too long:
    return full.slice(0, targetLen - 1).trim() + '…' + fmtStr;
  }

  // 3 or more segments:
  // AAA / BBB / CCC / DDD -> AAA / BBB / … / DDD -> AAA / … / DDD -> AAA / … / D...
  const full = pathSegments.join(separator);
  if (full.length <= targetLen) {
    return full + fmtStr;
  }

  const first = pathSegments[0];
  const last = pathSegments[N - 1];
  const middle = pathSegments.slice(1, N - 1); // intermediate segments (index 1 to N - 2)

  // Start removing middle segments from N - 1 (the one closest to the leaf)
  // keptCount goes from middle.length - 1 down to 0
  for (let keptCount = middle.length - 1; keptCount >= 0; keptCount--) {
    let candidate;
    if (keptCount === 0) {
      candidate = `${first}${separator}…${separator}${last}`;
    } else {
      const keptMiddle = middle.slice(0, keptCount).join(separator);
      candidate = `${first}${separator}${keptMiddle}${separator}…${separator}${last}`;
    }

    if (candidate.length <= targetLen) {
      return candidate + fmtStr;
    }
  }

  // If all middle segments are removed and it's still too long: AAA / … / D...
  const prefix = `${first}${separator}…${separator}`;
  const availForLeaf = targetLen - prefix.length;
  if (availForLeaf > 3) {
    const shortLeaf = last.slice(0, availForLeaf - 1).trim() + '…';
    return `${prefix}${shortLeaf}${fmtStr}`;
  }

  // Fallback if first segment + prefix exceeds targetLen
  return full.slice(0, targetLen - 1).trim() + '…' + fmtStr;
}

// Structural strategy handlers
const STRATEGIES = {
  // 1. Competition & Event Archives (Famicompo, Battle of the Bits, Piano E-Competition)
  compo_event({ dir, sampleSongs, sampleSubdirs, pathSegments, formats, fmtStr }) {
    const depth = pathSegments.length;
    const root = pathSegments[0];
    let title = '';

    if (depth === 1) {
      title = `${dir.name} Compo Archive${fmtStr}`;
    } else if (root.startsWith('Famicompo')) {
      // Combines event name + edition + category: e.g. "Famicompo mini5 Cover (NSF)"
      const compoPrefix = root.startsWith('Famicompo (NSFE)') ? 'Famicompo' : root;
      const relevantSegments = pathSegments.slice(1).join(' ');
      title = `${compoPrefix} ${relevantSegments}${fmtStr}`;
    } else if (root === 'Battle of the Bits') {
      title = `${dir.name} · BotB${fmtStr}`;
    } else if (root.startsWith('Piano E-Competition')) {
      const relevant = pathSegments.slice(1).join(' · ');
      title = `Piano E-Competition ${relevant}${fmtStr}`;
    } else {
      title = `${dir.name} · ${pathSegments[depth - 2]}${fmtStr}`;
    }
    title = truncateString(title, 60);

    const songList = formatSongList(sampleSongs, 3);
    const songsText = songList ? ` Entries include ${songList}, and more.` : '';
    const description = `Listen to ${dir.count.toLocaleString()} chiptune and competition entries in ${title}.${songsText} Online audio playback with interactive visualizer.`;
    return { title, description };
  },

  // 2. Artist Discographies & Projects (Contemporary, Classical MIDI, MIDI, Farbrausch V2, ModArchives, etc.)
  artist_discography({ dir, sampleSongs, sampleSubdirs, pathSegments, formats, fmtStr }) {
    const depth = pathSegments.length;
    const root = pathSegments[0];
    let title = '';

    if (depth === 1) {
      title = `${dir.name} Music Archive${fmtStr}`;
    } else if (depth === 2) {
      title = `${dir.name} · ${root}${fmtStr}`;
    } else {
      // Depth >= 3: e.g. Contemporary/Brad Smith/MOON8 -> "MOON8 · Brad Smith (NSFE)"
      // e.g. Contemporary/James Paddock/Doom Projects/Complete/Jenesis -> "Jenesis · James Paddock (Doom Projects) (MIDI)"
      const artist = pathSegments[1];
      const leaf = dir.name;
      const midSegments = pathSegments.slice(2, depth - 1).filter(s => !/^(Complete|others|Ports|Arrange)$/i.test(s));
      const extraContext = midSegments.length > 0 ? ` (${midSegments[midSegments.length - 1]})` : '';
      title = `${leaf} · ${artist}${extraContext}${fmtStr}`;
    }
    title = truncateString(title, 60);

    const songList = formatSongList(sampleSongs, 3);
    const songsText = songList ? ` Tracks include ${songList}, and more.` : '';
    const artist = depth >= 2 ? pathSegments[1] : dir.name;
    const description = `Listen to ${dir.count.toLocaleString()} tracks from ${dir.name} by ${artist}.${songsText} High-fidelity browser audio player with retro visualizer.`;
    return { title, description };
  },

  // 3. Partitioned Artist Archives (HVSC, The Module Collection, keygenmusic.org)
  partitioned_artist({ dir, sampleSongs, sampleSubdirs, pathSegments, formats, fmtStr }) {
    const depth = pathSegments.length;
    const root = pathSegments[0];
    let title = '';

    if (depth === 1) {
      title = `${dir.name}${fmtStr}`;
    } else if (root === 'High Voltage SID Collection') {
      // Filter navigational partition letters like "T", "0-9", "A-F"
      const nonPartitions = pathSegments.filter(s => !/^[A-Z0-9]$|^[0-9]-[0-9]$|^[A-Z]-[A-Z]$/.test(s));
      const leafName = nonPartitions[nonPartitions.length - 1];
      let artistName = leafName;
      if (artistName && artistName.includes('_')) {
        const parts = artistName.split('_');
        if (parts.length === 2) artistName = `${parts[1]} ${parts[0]} (${leafName})`;
      }

      if (leafName === 'MUSICIANS' || leafName === 'DEMOS' || leafName === 'GAMES') {
        title = `HVSC ${leafName}${fmtStr}`;
      } else if (nonPartitions.length === 1) {
        title = `HVSC ${pathSegments[depth - 1]}${fmtStr}`;
      } else {
        title = `${artistName} · HVSC${fmtStr}`;
      }
    } else if (root === 'The Module Collection') {
      if (dir.name.startsWith('ARTIST-')) {
        title = `The Module Collection: ${dir.name}${fmtStr}`;
      } else {
        title = `${dir.name} · The Module Collection${fmtStr}`;
      }
    } else {
      title = `${dir.name} · ${root}${fmtStr}`;
    }
    title = truncateString(title, 60);

    const songList = formatSongList(sampleSongs, 3);
    const songsText = songList ? ` Tracks include ${songList}, and more.` : '';
    const description = `Listen to ${dir.count.toLocaleString()} retro chiptune files in ${dir.name} from the ${root} archive.${songsText} Online emulated sound synthesis.`;
    return { title, description };
  },

  // 4. Platform-Grouped Game Soundtracks (VGM Rips, vgmusic.com MIDI, Project AY)
  platform_game_archive({ dir, sampleSongs, sampleSubdirs, pathSegments, formats, fmtStr }) {
    const depth = pathSegments.length;
    const root = pathSegments[0];
    let title = '';

    if (depth === 1) {
      title = `${dir.name} Archive${fmtStr}`;
    } else if (depth === 2) {
      title = `${dir.name} · ${root}${fmtStr}`;
    } else {
      const platform = pathSegments[1];
      const leaf = dir.name;
      const publisher = depth > 3 ? pathSegments[2] : null;
      if (publisher && !/^(Games|Demos)$/i.test(publisher)) {
        title = `${leaf} · ${publisher} ${platform}${fmtStr}`;
      } else {
        title = `${leaf} · ${platform}${fmtStr}`;
      }
    }
    title = truncateString(title, 60);

    const songList = formatSongList(sampleSongs, 3);
    const songsText = songList ? ` Features ${songList}, and more.` : '';
    const description = `Listen to ${dir.count.toLocaleString()} video game music tracks from ${dir.name}.${songsText} Hardware-accurate sound chip emulation and visualizer.`;
    return { title, description };
  },

  // 5. Flat Console Game Collections (Nintendo SNES, Sega Genesis, etc.)
  flat_game_collection({ dir, sampleSongs, sampleSubdirs, pathSegments, formats, fmtStr }) {
    const depth = pathSegments.length;
    let title = '';

    if (depth === 1) {
      title = `${dir.name} Soundtracks${fmtStr}`;
    } else if (depth === 2) {
      const composer = sampleSongs.find(s => s.artist)?.artist;
      let candidate = `${dir.name} OST${composer ? ` - ${composer}` : ''}${fmtStr}`;
      if (candidate.length > 60) candidate = `${dir.name} OST${fmtStr}`;
      title = candidate;
    } else {
      title = `${dir.name} · ${pathSegments[depth - 2]}${fmtStr}`;
    }
    title = truncateString(title, 60);

    const composer = sampleSongs.find(s => s.artist)?.artist;
    const composerText = composer ? ` composed by ${composer}` : '';
    const songList = formatSongList(sampleSongs, 3);
    const songsText = songList ? ` Features ${songList}, and more.` : '';
    const description = `Listen to the full ${dir.name} soundtrack${composerText}. ${dir.count.toLocaleString()} original tracks.${songsText} Interactive web playback.`;
    return { title, description };
  },

  // 6. Catalog Disc Archives (Roland SMF MIDI Disks, Tune 1000 SMF MIDI Disks)
  catalog_release_archive({ dir, sampleSongs, sampleSubdirs, pathSegments, formats, fmtStr }) {
    const depth = pathSegments.length;
    let title = '';

    if (depth === 1) {
      title = `${dir.name} Archive${fmtStr}`;
    } else {
      title = `${dir.name}${fmtStr}`;
    }
    title = truncateString(title, 60);

    const songList = formatSongList(sampleSongs, 3);
    const songsText = songList ? ` Includes ${songList}, and more.` : '';
    const description = `Listen to ${dir.count.toLocaleString()} tracks from ${dir.name}.${songsText} High-quality SoundFont rendering and interactive playback.`;
    return { title, description };
  },

  // 7. Deep Thematic & Multi-Tier Archives (Game MIDI, Sound Canvas, Sharp X68000, etc.)
  deep_thematic_archive({ dir, sampleSongs, sampleSubdirs, pathSegments, formats, fmtStr }) {
    const depth = pathSegments.length;
    let title = '';

    if (depth === 1) {
      title = `${dir.name} Music Collection${fmtStr}`;
    } else {
      // Find nearest non-generic ancestor
      const candidates = pathSegments.slice(1, depth - 1).filter(s => !/^(- Arranged -|- Soundtrack -|Game|Original)$/i.test(s));
      const nearest = candidates.length > 0 ? candidates[candidates.length - 1] : pathSegments[depth - 2];
      title = `${dir.name} · ${nearest}${fmtStr}`;
    }
    title = truncateString(title, 60);

    const songList = formatSongList(sampleSongs, 3);
    const songsText = songList ? ` Features ${songList}, and more.` : '';
    const description = `Listen to ${dir.count.toLocaleString()} music tracks from ${dir.name}.${songsText} Authentic synthesizer and chiptune audio in your browser.`;
    return { title, description };
  },

  // 8. Simplified Path Strategy (Literal directory path with middle ellipsis)
  simplified_path({ dir, sampleSongs, sampleSubdirs, pathSegments, formats, fmtStr }) {
    const title = formatSimplifiedPathTitle(pathSegments, 60, {
      includeFormat: true,
      formats,
      separator: ' / ',
    });

    const songList = formatSongList(sampleSongs, 3);
    const songsText = songList ? ` Features ${songList}, and more.` : '';
    const description = `Browse and listen to ${dir.count.toLocaleString()} tracks in ${pathSegments.join('/')}.${songsText} High-fidelity online audio playback.`;
    return { title, description };
  },

  // Adaptive Fallback
  default({ dir, sampleSongs, sampleSubdirs, pathSegments, formats, fmtStr }) {
    const depth = pathSegments.length;
    let title = '';
    if (depth === 1) {
      title = `${dir.name} Music Collection${fmtStr}`;
    } else {
      const parent = pathSegments[depth - 2];
      title = `${dir.name} · ${parent}${fmtStr}`;
    }
    title = truncateString(title, 60);

    const songList = formatSongList(sampleSongs, 3);
    const songsText = songList ? ` Tracks include ${songList}, and more.` : '';
    const description = `Browse ${dir.count.toLocaleString()} audio tracks in ${dir.name}.${songsText} Online chiptune player.`;
    return { title, description };
  },
};

function buildBreadcrumbJsonLd(pathSegments) {
  let currentPath = '';
  const itemListElement = [
    {
      '@type': 'ListItem',
      position: 1,
      name: 'Home',
      item: 'https://chiptune.app/',
    },
    {
      '@type': 'ListItem',
      position: 2,
      name: 'Browse',
      item: 'https://chiptune.app/browse',
    },
  ];

  pathSegments.forEach((segment, idx) => {
    currentPath += (currentPath ? '/' : '') + segment;
    const encoded = currentPath.split('/').map(encodeURIComponent).join('/');
    itemListElement.push({
      '@type': 'ListItem',
      position: idx + 3,
      name: segment,
      item: `https://chiptune.app/browse/${encoded}`,
    });
  });

  return {
    '@context': 'https://schema.org',
    '@type': 'BreadcrumbList',
    itemListElement,
  };
}

function buildCollectionPageJsonLd(title, description, url, image) {
  return {
    '@context': 'https://schema.org',
    '@type': 'CollectionPage',
    name: title,
    description: description,
    url: url,
    image: image,
  };
}

function buildNoscriptHtml(title, description, pathSegments, sampleSubdirs, sampleSongs, currentUrl) {
  const breadcrumbLinks = ['<a href="/">Home</a>', '<a href="/browse">Browse</a>'];
  let cur = '';
  pathSegments.forEach((seg, idx) => {
    cur += (cur ? '/' : '') + seg;
    const href = `/browse/${cur.split('/').map(encodeURIComponent).join('/')}`;
    if (idx === pathSegments.length - 1) {
      breadcrumbLinks.push(`<span>${escapeHtml(seg)}</span>`);
    } else {
      breadcrumbLinks.push(`<a href="${href}">${escapeHtml(seg)}</a>`);
    }
  });

  let subdirsHtml = '';
  if (sampleSubdirs.length > 0) {
    const listItems = sampleSubdirs
      .map(sub => {
        const subPath = pathSegments.concat([sub.name]).join('/');
        const href = `/browse/${subPath.split('/').map(encodeURIComponent).join('/')}`;
        return `<li><a href="${href}">${escapeHtml(sub.name)}</a> (${sub.count.toLocaleString()} tracks)</li>`;
      })
      .join('\n      ');
    subdirsHtml = `
    <h2>Subdirectories</h2>
    <ul>
      ${listItems}
    </ul>`;
  }

  let songsHtml = '';
  if (sampleSongs.length > 0) {
    const listItems = sampleSongs
      .map(song => {
        const songTitle = song.title || song.filename;
        const playUrl = song.song_id ? `/?play=${encodeURIComponent(song.song_id)}` : '#';
        const meta = [song.artist, song.game, song.extension ? song.extension.toUpperCase() : null]
          .filter(Boolean)
          .join(' · ');
        return `<li><a href="${playUrl}">${escapeHtml(songTitle)}</a>${meta ? ` <small>(${escapeHtml(meta)})</small>` : ''}</li>`;
      })
      .join('\n      ');
    songsHtml = `
    <h2>Tracks</h2>
    <ul>
      ${listItems}
    </ul>`;
  }

  return `
  <div class="seo-crawler-content">
    <nav aria-label="Breadcrumb" class="seo-breadcrumb">
      ${breadcrumbLinks.join(' / ')}
    </nav>
    <h1>${escapeHtml(title)}</h1>
    <p>${escapeHtml(description)}</p>
    ${subdirsHtml}
    ${songsHtml}
    <p><small>Requires JavaScript to play audio in the browser.</small></p>
  </div>`;
}

/**
 * Resolves full SEO metadata for a directory path.
 *
 * @param {string} rawBrowsePath - e.g. "Nintendo SNES/Super Mario World"
 * @param {object} [options] - Optional overrides: { mode: 'structural' | 'path' }
 * @returns {object} { title, description, url, image, schemaJsonLd, noscriptContent, formats }
 */
function resolveDirectoryMetadata(rawBrowsePath, options = {}) {
  let normalizedPath = (rawBrowsePath || '')
    .replace(/^\/?browse\/?/, '')
    .replace(/^\/+|\/+$/g, '');

  try {
    normalizedPath = decodeURIComponent(normalizedPath);
  } catch (e) {}

  if (!normalizedPath) {
    return {
      title: 'Browse Music Archives',
      description:
        'Browse over 380,000 video game music and chiptune tracks across 31,000 directories. Listen to SPC, SID, NSF, VGM, MIDI, MOD and more.',
      url: 'https://chiptune.app/browse',
      image: 'https://chiptune.app/chip-player-1200x640.png',
      schemaJsonLd: `<script type="application/ld+json">${JSON.stringify(buildBreadcrumbJsonLd([]))}</script>`,
      noscriptContent:
        '<div class="seo-crawler-content"><h1>Browse Music Archives</h1><p>Browse over 380,000 tracks.</p></div>',
    };
  }

  const activeMode = options.mode || STRATEGY_MODE;
  const cacheKey = `${activeMode}:${normalizedPath}`;
  const cached = directoryMetaCache.get(cacheKey);
  if (cached) {
    return cached;
  }

  const dir = getDirMetadataStmt.get(normalizedPath);
  if (!dir) {
    const segments = normalizedPath.split('/').filter(Boolean);
    const fallbackTitle = truncateString(decodeURIComponent(segments[segments.length - 1] || 'Directory'), 60);
    const result = {
      title: fallbackTitle,
      description: `Browse chiptune and video game music tracks in ${fallbackTitle}.`,
      url: `https://chiptune.app/browse/${normalizedPath.split('/').map(encodeURIComponent).join('/')}`,
      image: 'https://chiptune.app/chip-player-1200x640.png',
      schemaJsonLd: `<script type="application/ld+json">${JSON.stringify(buildBreadcrumbJsonLd(segments))}</script>`,
      noscriptContent: '',
    };
    directoryMetaCache.set(cacheKey, result);
    return result;
  }

  const sampleSongs = getDirSampleSongsStmt.all(dir.id);
  const sampleSubdirs = getDirSampleSubdirsStmt.all(dir.id);
  const pathSegments = normalizedPath.split('/').filter(Boolean);
  const topPrefix = pathSegments[0];

  const formats = resolveFormats(dir.id, dir.path);
  const fmtStr = formatTag(formats);

  let strategyKey;
  if (activeMode === 'path') {
    strategyKey = 'simplified_path';
  } else {
    strategyKey = PREFIX_STRUCTURE_MAP[topPrefix] || 'default';
  }

  const strategyFn = STRATEGIES[strategyKey] || STRATEGIES.default;

  const { title, description } = strategyFn({
    dir,
    sampleSongs,
    sampleSubdirs,
    pathSegments,
    normalizedPath,
    formats,
    fmtStr,
  });

  let image = 'https://chiptune.app/chip-player-1200x640.png';
  if (dir.image_path) {
    const encodedPath = dir.image_path.split('/').map(encodeURIComponent).join('/');
    image = `https://chiptune.app/catalog/${encodedPath}`;
  }

  const canonicalUrl = `https://chiptune.app/browse/${pathSegments.map(encodeURIComponent).join('/')}`;
  const breadcrumbSchema = buildBreadcrumbJsonLd(pathSegments);
  const collectionSchema = buildCollectionPageJsonLd(title, description, canonicalUrl, image);
  const combinedSchema = [breadcrumbSchema, collectionSchema];

  const schemaJsonLd = `<script type="application/ld+json">${JSON.stringify(combinedSchema)}</script>`;
  const noscriptContent = buildNoscriptHtml(
    title,
    description,
    pathSegments,
    sampleSubdirs,
    sampleSongs,
    canonicalUrl
  );

  const result = {
    title,
    description,
    url: canonicalUrl,
    image,
    schemaJsonLd,
    noscriptContent,
    formats,
  };

  directoryMetaCache.set(cacheKey, result);
  return result;
}

module.exports = {
  STRATEGY_MODE,
  setStrategyMode,
  getStrategyMode,
  PREFIX_STRUCTURE_MAP,
  PREFIX_STRATEGY_MAP,
  STRATEGIES,
  resolveFormats,
  formatTag,
  formatSimplifiedPathTitle,
  resolveDirectoryMetadata,
  buildBreadcrumbJsonLd,
  directoryMetaCache,
};
