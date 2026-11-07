const { LRUCache } = require('lru-cache');
const { db } = require('./database');

// 1-hour in-memory cache for aggregate dashboard stats
const statsCache = new LRUCache({
  max: 50,
  ttl: 1000 * 60 * 60, // 1 hour
});

/**
 * Format a Date object to YYYY-MM-DD
 */
function formatDateUtc(d) {
  const y = d.getUTCFullYear();
  const m = String(d.getUTCMonth() + 1).padStart(2, '0');
  const day = String(d.getUTCDate()).padStart(2, '0');
  return `${y}-${m}-${day}`;
}

/**
 * Format a Date object to YYYY-MM-DD HH:00
 */
function formatHourUtc(d) {
  const dateStr = formatDateUtc(d);
  const h = String(d.getUTCHours()).padStart(2, '0');
  return `${dateStr} ${h}:00`;
}

/**
 * Format a Date object to YYYY-MM
 */
function formatMonthUtc(d) {
  const y = d.getUTCFullYear();
  const m = String(d.getUTCMonth() + 1).padStart(2, '0');
  return `${y}-${m}`;
}

/**
 * Generate contiguous bucket keys from start to end
 */
function generateBuckets(startSec, endSec, resolution) {
  const buckets = [];
  const startMs = startSec * 1000;
  const endMs = endSec * 1000;

  if (resolution === 'hour') {
    let curr = new Date(Math.floor(startMs / 3600000) * 3600000);
    while (curr.getTime() <= endMs) {
      buckets.push({
        key: formatHourUtc(curr),
        timestamp: Math.floor(curr.getTime() / 1000),
      });
      curr = new Date(curr.getTime() + 3600000);
    }
  } else if (resolution === 'day') {
    let curr = new Date(startMs);
    curr.setUTCHours(0, 0, 0, 0);
    while (curr.getTime() <= endMs) {
      buckets.push({
        key: formatDateUtc(curr),
        timestamp: Math.floor(curr.getTime() / 1000),
      });
      curr = new Date(curr.getTime() + 86400000);
    }
  } else if (resolution === 'month') {
    let curr = new Date(startMs);
    curr.setUTCDate(1);
    curr.setUTCHours(0, 0, 0, 0);
    while (curr.getTime() <= endMs) {
      buckets.push({
        key: formatMonthUtc(curr),
        timestamp: Math.floor(curr.getTime() / 1000),
      });
      curr.setUTCMonth(curr.getUTCMonth() + 1);
    }
  }

  return buckets;
}

/**
 * Resolve start, end, resolution from query parameters
 */
function parseTimeRange(params) {
  // Quantize relative "now" to 1-hour intervals (3600 seconds) so that relative
  // presets (24h, 7d, 30d, 90d, 1y) produce identical start/end timestamps and hit
  // the server-side LRU cache across all concurrent users during that window.
  const CACHE_WINDOW_SEC = 3600;
  const now = Math.floor(Date.now() / 1000 / CACHE_WINDOW_SEC) * CACHE_WINDOW_SEC;
  let range = params.range || '30d';
  let start;
  let end = now;
  let resolution = params.resolution || 'auto';

  if (params.start && params.end) {
    range = 'custom';
    start = Number(params.start);
    if (isNaN(start)) {
      start = Math.floor(new Date(params.start).getTime() / 1000);
    }
    end = Number(params.end);
    if (isNaN(end)) {
      end = Math.floor(new Date(params.end).getTime() / 1000);
    }
    if (isNaN(start) || isNaN(end) || start >= end) {
      start = now - 30 * 86400;
      end = now;
      range = '30d';
    }
  } else {
    switch (range) {
      case '24h':
        start = now - 86400;
        if (resolution === 'auto') resolution = 'hour';
        break;
      case '7d':
        start = now - 7 * 86400;
        if (resolution === 'auto') resolution = 'hour';
        break;
      case '30d':
        start = now - 30 * 86400;
        if (resolution === 'auto') resolution = 'day';
        break;
      case '90d':
        start = now - 90 * 86400;
        if (resolution === 'auto') resolution = 'day';
        break;
      case '1y':
        start = now - 365 * 86400;
        if (resolution === 'auto') resolution = 'day';
        break;
      case 'all':
        // Earliest playback timestamp or default to beginning of 2024
        start = 1704067200;
        if (resolution === 'auto') resolution = 'month';
        break;
      default:
        range = '30d';
        start = now - 30 * 86400;
        if (resolution === 'auto') resolution = 'day';
        break;
    }
  }

  if (resolution === 'auto') {
    const span = end - start;
    if (span <= 86400 * 7) resolution = 'hour';
    else if (span <= 86400 * 365) resolution = 'day';
    else resolution = 'month';
  }

  return { range, start, end, resolution };
}

/**
 * Get SQLite strftime pattern for a resolution
 */
function getSqlDatePattern(resolution) {
  switch (resolution) {
    case 'hour':
      return '%Y-%m-%d %H:00';
    case 'month':
      return '%Y-%m';
    case 'day':
    default:
      return '%Y-%m-%d';
  }
}

/**
 * Compute all dashboard stats for the given time range
 */
function getDashboardStats(params = {}) {
  const { range, start, end, resolution } = parseTimeRange(params);
  const cacheKey = `${range}_${start}_${end}_${resolution}`;

  if (!params.refresh) {
    const cached = statsCache.get(cacheKey);
    if (cached) {
      return cached;
    }
  }

  const sqlDatePattern = getSqlDatePattern(resolution);
  const duration = end - start;
  const prevStart = start - duration;
  const prevEnd = start;

  // 1. Playback aggregates by bucket
  const playbackRows = db.prepare(`
    SELECT 
      strftime('${sqlDatePattern}', played_at, 'unixepoch') AS bucket,
      COUNT(*) AS total_plays,
      COUNT(user_id) AS logged_in_plays,
      COUNT(DISTINCT COALESCE(user_id, ip_address)) AS unique_listeners,
      COUNT(DISTINCT user_id) AS active_users,
      ROUND(SUM(duration_ms) / 3600000.0, 2) AS listen_hours
    FROM user_db.playbacks
    WHERE played_at >= ? AND played_at < ?
    GROUP BY bucket
    ORDER BY bucket ASC
  `).all(start, end);

  // 2. New registered users by bucket
  const newUserRows = db.prepare(`
    SELECT 
      strftime('${sqlDatePattern}', created_at, 'unixepoch') AS bucket,
      COUNT(*) AS new_users
    FROM user_db.users
    WHERE created_at >= ? AND created_at < ?
    GROUP BY bucket
    ORDER BY bucket ASC
  `).all(start, end);

  // 3. Favorites added by bucket
  const favoriteRows = db.prepare(`
    SELECT 
      strftime('${sqlDatePattern}', cast(json_extract(je.value, '$.mtime') as integer), 'unixepoch') AS bucket,
      COUNT(*) AS favorites_added
    FROM user_db.playlists p, json_each(p.items) je
    WHERE p.type = 'favorites' 
      AND cast(json_extract(je.value, '$.mtime') as integer) >= ? 
      AND cast(json_extract(je.value, '$.mtime') as integer) < ?
    GROUP BY bucket
    ORDER BY bucket ASC
  `).all(start, end);

  // 4. Initial cumulative offsets
  const initialUsersRow = db.prepare(`
    SELECT COUNT(*) as count FROM user_db.users WHERE created_at < ?
  `).get(start);
  let cumulativeUsers = initialUsersRow ? initialUsersRow.count : 0;

  const initialFavsRow = db.prepare(`
    SELECT COUNT(*) as count 
    FROM user_db.playlists p, json_each(p.items) je 
    WHERE p.type = 'favorites' AND cast(json_extract(je.value, '$.mtime') as integer) < ?
  `).get(start);
  let cumulativeFavorites = initialFavsRow ? initialFavsRow.count : 0;

  // Map by bucket for O(1) lookup
  const playbackMap = new Map(playbackRows.map(r => [r.bucket, r]));
  const newUserMap = new Map(newUserRows.map(r => [r.bucket, r.new_users]));
  const favoriteMap = new Map(favoriteRows.map(r => [r.bucket, r.favorites_added]));

  // Build complete contiguous time-series
  const bucketList = generateBuckets(start, end, resolution);
  const timeSeries = bucketList.map(({ key, timestamp }) => {
    const p = playbackMap.get(key);
    const newUsers = newUserMap.get(key) || 0;
    const favs = favoriteMap.get(key) || 0;

    cumulativeUsers += newUsers;
    cumulativeFavorites += favs;

    const totalPlays = p ? p.total_plays : 0;
    const loggedInPlays = p ? p.logged_in_plays : 0;
    const anonPlays = Math.max(0, totalPlays - loggedInPlays);
    const uniqueListeners = p ? p.unique_listeners : 0;
    const activeUsers = p ? p.active_users : 0;
    const listenHours = p ? p.listen_hours : 0;

    return {
      bucket: key,
      timestamp,
      totalPlays,
      loggedInPlays,
      anonPlays,
      uniqueListeners,
      activeUsers,
      listenHours,
      newUsers,
      cumulativeUsers,
      favoritesAdded: favs,
      cumulativeFavorites,
    };
  });

  // 5. Current period summary totals
  const currentSummary = db.prepare(`
    SELECT 
      COUNT(*) AS total_plays,
      COUNT(user_id) AS logged_in_plays,
      COUNT(DISTINCT COALESCE(user_id, ip_address)) AS unique_listeners,
      COUNT(DISTINCT user_id) AS active_users,
      ROUND(SUM(duration_ms) / 3600000.0, 1) AS listen_hours
    FROM user_db.playbacks
    WHERE played_at >= ? AND played_at < ?
  `).get(start, end);

  const currentNewUsers = db.prepare(`
    SELECT COUNT(*) AS count FROM user_db.users WHERE created_at >= ? AND created_at < ?
  `).get(start, end).count;

  const currentFavs = db.prepare(`
    SELECT COUNT(*) AS count
    FROM user_db.playlists p, json_each(p.items) je
    WHERE p.type = 'favorites' 
      AND cast(json_extract(je.value, '$.mtime') as integer) >= ? 
      AND cast(json_extract(je.value, '$.mtime') as integer) < ?
  `).get(start, end).count;

  // 6. Previous period comparison for KPI delta calculation
  const prevSummary = db.prepare(`
    SELECT 
      COUNT(*) AS total_plays,
      COUNT(DISTINCT COALESCE(user_id, ip_address)) AS unique_listeners
    FROM user_db.playbacks
    WHERE played_at >= ? AND played_at < ?
  `).get(prevStart, prevEnd);

  const prevNewUsers = db.prepare(`
    SELECT COUNT(*) AS count FROM user_db.users WHERE created_at >= ? AND created_at < ?
  `).get(prevStart, prevEnd).count;

  const prevFavs = db.prepare(`
    SELECT COUNT(*) AS count
    FROM user_db.playlists p, json_each(p.items) je
    WHERE p.type = 'favorites' 
      AND cast(json_extract(je.value, '$.mtime') as integer) >= ? 
      AND cast(json_extract(je.value, '$.mtime') as integer) < ?
  `).get(prevStart, prevEnd).count;

  function calcDelta(curr, prev) {
    if (!prev || prev === 0) return 0;
    return Number((((curr - prev) / prev) * 100).toFixed(1));
  }

  const daysInPeriod = Math.max(1, (end - start) / 86400);
  const totalPlays = currentSummary ? currentSummary.total_plays : 0;
  const loggedInPlays = currentSummary ? currentSummary.logged_in_plays : 0;
  const uniqueListeners = currentSummary ? currentSummary.unique_listeners : 0;
  const listenHours = currentSummary ? currentSummary.listen_hours || 0 : 0;

  const summary = {
    totalPlays,
    prevTotalPlays: prevSummary ? prevSummary.total_plays : 0,
    playsDeltaPercent: calcDelta(totalPlays, prevSummary ? prevSummary.total_plays : 0),

    uniqueListeners,
    prevUniqueListeners: prevSummary ? prevSummary.unique_listeners : 0,
    listenersDeltaPercent: calcDelta(uniqueListeners, prevSummary ? prevSummary.unique_listeners : 0),

    newUsers: currentNewUsers,
    prevNewUsers,
    newUsersDeltaPercent: calcDelta(currentNewUsers, prevNewUsers),

    favoritesAdded: currentFavs,
    prevFavoritesAdded: prevFavs,
    favoritesDeltaPercent: calcDelta(currentFavs, prevFavs),

    totalListenHours: listenHours,
    avgDailyPlays: Math.round(totalPlays / daysInPeriod),
    loggedInPlaysPercent: totalPlays > 0 ? Number(((loggedInPlays / totalPlays) * 100).toFixed(1)) : 0,
    totalAccounts: cumulativeUsers,
    totalFavorites: cumulativeFavorites,
  };

  // 7. Top Tracks in Period (Grouped by song_id first for speed)
  const topTracks = db.prepare(`
    SELECT 
      top.song_id,
      top.plays,
      top.duration_sec,
      m.title,
      m.artist,
      m.game,
      m.system,
      m.extension,
      m.path
    FROM (
      SELECT 
        song_id, 
        COUNT(*) as plays, 
        SUM(duration_ms) / 1000 as duration_sec
      FROM user_db.playbacks
      WHERE played_at >= ? AND played_at < ?
      GROUP BY song_id
      ORDER BY plays DESC
      LIMIT 25
    ) top
    JOIN music m ON m.rowid = (
      SELECT rowid FROM music WHERE song_id = top.song_id ORDER BY mtime LIMIT 1
    )
    ORDER BY top.plays DESC
  `).all(start, end).map((track, i) => ({
    rank: i + 1,
    songId: track.song_id,
    title: track.title || (track.path ? track.path.split('/').pop() : 'Unknown'),
    artist: track.artist || track.game || 'Unknown',
    game: track.game || '',
    system: track.system || (track.path ? track.path.split('/')[0] : 'Unknown'),
    extension: track.extension ? track.extension.toUpperCase() : '',
    plays: track.plays,
    durationSec: track.duration_sec,
    url: `/?play=${encodeURIComponent(track.song_id)}`,
  }));

  // 8. Top Favorited Tracks in Period
  const topFavorites = db.prepare(`
    SELECT 
      top.song_id,
      top.fav_count,
      m.title,
      m.artist,
      m.game,
      m.system,
      m.extension,
      m.path
    FROM (
      SELECT 
        json_extract(je.value, '$.songId') as song_id,
        COUNT(*) as fav_count
      FROM user_db.playlists p, json_each(p.items) je
      WHERE p.type = 'favorites'
        AND cast(json_extract(je.value, '$.mtime') as integer) >= ? 
        AND cast(json_extract(je.value, '$.mtime') as integer) < ?
      GROUP BY song_id
      ORDER BY fav_count DESC
      LIMIT 25
    ) top
    JOIN music m ON m.rowid = (
      SELECT rowid FROM music WHERE song_id = top.song_id ORDER BY mtime LIMIT 1
    )
    ORDER BY top.fav_count DESC
  `).all(start, end).map((fav, i) => ({
    rank: i + 1,
    songId: fav.song_id,
    title: fav.title || (fav.path ? fav.path.split('/').pop() : 'Unknown'),
    artist: fav.artist || fav.game || 'Unknown',
    game: fav.game || '',
    system: fav.system || (fav.path ? fav.path.split('/')[0] : 'Unknown'),
    extension: fav.extension ? fav.extension.toUpperCase() : '',
    favoritesCount: fav.fav_count,
    url: `/?play=${encodeURIComponent(fav.song_id)}`,
  }));

  // 9. Top Systems & Formats in Period (Single combined pass for maximum performance)
  const metaDistribution = db.prepare(`
    SELECT 
      m.system,
      m.extension,
      SUM(top.plays) as plays
    FROM (
      SELECT song_id, COUNT(*) as plays
      FROM user_db.playbacks
      WHERE played_at >= ? AND played_at < ?
      GROUP BY song_id
    ) top
    JOIN music m ON m.song_id = top.song_id
    GROUP BY m.system, m.extension
  `).all(start, end);

  const systemMap = new Map();
  const formatMap = new Map();
  let totalMetaPlays = 0;

  for (const row of metaDistribution) {
    totalMetaPlays += row.plays;
    if (row.system) {
      systemMap.set(row.system, (systemMap.get(row.system) || 0) + row.plays);
    }
    if (row.extension) {
      const ext = row.extension.toUpperCase();
      formatMap.set(ext, (formatMap.get(ext) || 0) + row.plays);
    }
  }

  const topSystems = Array.from(systemMap.entries())
    .map(([system, plays]) => ({
      system,
      plays,
      sharePercent: totalMetaPlays > 0 ? Number(((plays / totalMetaPlays) * 100).toFixed(1)) : 0,
    }))
    .sort((a, b) => b.plays - a.plays)
    .slice(0, 10);

  const topFormats = Array.from(formatMap.entries())
    .map(([extension, plays]) => ({
      extension,
      plays,
      sharePercent: totalMetaPlays > 0 ? Number(((plays / totalMetaPlays) * 100).toFixed(1)) : 0,
    }))
    .sort((a, b) => b.plays - a.plays)
    .slice(0, 10);

  // 11. Session Analytics (Clustering with 1-hour timeout)
  const sessionTimeoutSec = 3600;
  const sessionRows = db.prepare(`
    WITH ordered_plays AS (
      SELECT 
        COALESCE(user_id, ip_address) AS listener_id,
        played_at,
        LAG(played_at) OVER (
          PARTITION BY COALESCE(user_id, ip_address) 
          ORDER BY played_at
        ) AS prev_played_at
      FROM user_db.playbacks
      WHERE played_at >= ? AND played_at < ?
    ),
    session_markers AS (
      SELECT 
        listener_id,
        played_at,
        CASE 
          WHEN prev_played_at IS NULL OR (played_at - prev_played_at) > ? THEN 1 
          ELSE 0 
        END AS is_new_session
      FROM ordered_plays
    ),
    session_groups AS (
      SELECT 
        listener_id,
        played_at,
        SUM(is_new_session) OVER (
          PARTITION BY listener_id 
          ORDER BY played_at
        ) AS session_num
      FROM session_markers
    )
    SELECT 
      COUNT(*) AS playCount,
      MAX(played_at) - MIN(played_at) AS durationSeconds
    FROM session_groups
    GROUP BY listener_id, session_num
  `).all(start, end, sessionTimeoutSec);

  const totalSessions = sessionRows.length;
  let totalSessionPlays = 0;
  let totalSessionDurationSec = 0;
  const playCounts = [];
  const sessionDurations = [];

  const playBins = {
    '1 play': 0,
    '2–5': 0,
    '6–10': 0,
    '11–25': 0,
    '26–50': 0,
    '51–100': 0,
    '100+': 0,
  };

  const durationBins = {
    '< 1 min': 0,
    '1–5 min': 0,
    '5–15 min': 0,
    '15–30 min': 0,
    '30–60 min': 0,
    '1–2 hrs': 0,
    '2+ hrs': 0,
  };

  for (const row of sessionRows) {
    const p = row.playCount;
    const d = row.durationSeconds;
    totalSessionPlays += p;
    totalSessionDurationSec += d;
    playCounts.push(p);
    sessionDurations.push(d);

    if (p === 1) playBins['1 play']++;
    else if (p <= 5) playBins['2–5']++;
    else if (p <= 10) playBins['6–10']++;
    else if (p <= 25) playBins['11–25']++;
    else if (p <= 50) playBins['26–50']++;
    else if (p <= 100) playBins['51–100']++;
    else playBins['100+']++;

    const dm = d / 60.0;
    if (dm < 1) durationBins['< 1 min']++;
    else if (dm < 5) durationBins['1–5 min']++;
    else if (dm < 15) durationBins['5–15 min']++;
    else if (dm < 30) durationBins['15–30 min']++;
    else if (dm < 60) durationBins['30–60 min']++;
    else if (dm < 120) durationBins['1–2 hrs']++;
    else durationBins['2+ hrs']++;
  }

  playCounts.sort((a, b) => a - b);
  sessionDurations.sort((a, b) => a - b);

  const medianPlaysPerSession = totalSessions > 0 ? playCounts[Math.floor(totalSessions / 2)] : 0;
  const medianDurationMins = totalSessions > 0 ? Number((sessionDurations[Math.floor(totalSessions / 2)] / 60.0).toFixed(1)) : 0;
  const avgPlaysPerSession = totalSessions > 0 ? Number((totalSessionPlays / totalSessions).toFixed(1)) : 0;
  const avgDurationMins = totalSessions > 0 ? Number(((totalSessionDurationSec / totalSessions) / 60.0).toFixed(1)) : 0;
  const bounceCount = playBins['1 play'];
  const bounceRatePercent = totalSessions > 0 ? Number(((bounceCount / totalSessions) * 100).toFixed(1)) : 0;

  const playBuckets = Object.entries(playBins).map(([bucket, count]) => ({
    bucket,
    count,
    sharePercent: totalSessions > 0 ? Number(((count / totalSessions) * 100).toFixed(1)) : 0,
  }));

  const durationBuckets = Object.entries(durationBins).map(([bucket, count]) => ({
    bucket,
    count,
    sharePercent: totalSessions > 0 ? Number(((count / totalSessions) * 100).toFixed(1)) : 0,
  }));

  const sessionAnalytics = {
    timeoutMinutes: 60,
    totalSessions,
    avgDurationMins,
    medianDurationMins,
    avgPlaysPerSession,
    medianPlaysPerSession,
    bounceRatePercent,
    playBuckets,
    durationBuckets,
  };

  const result = {
    range,
    resolution,
    start,
    end,
    cachedAt: Math.floor(Date.now() / 1000),
    ttlSeconds: 3600,
    summary,
    timeSeries,
    sessionAnalytics,
    topSystems,
    topFormats,
    topTracks,
    topFavorites,
  };

  statsCache.set(cacheKey, result);
  return result;
}

module.exports = {
  getDashboardStats,
  statsCache,
};
