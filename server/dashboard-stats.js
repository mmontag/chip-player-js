const path = require('path');
const { LRUCache } = require('lru-cache');
const { db } = require('./database');

// Force process timezone to Pacific Time if not already set
const TIMEZONE = 'America/Los_Angeles';
process.env.TZ = process.env.TZ || TIMEZONE;

// Version of dashboard aggregation & caching schema.
// Incrementing this invalidates stale cached metrics and recalculates fresh records.
const DASHBOARD_VERSION = 3;

// Dedicated SQLite database file for dashboard cache.
// Kept separate from users.db and excluded from backup.js.
const DASHBOARD_DB_PATH = path.resolve(__dirname, 'dashboard-cache.db');

// Attach dashboard-cache.db and initialize tables if they don't exist yet
try {
  db.exec(`ATTACH DATABASE '${DASHBOARD_DB_PATH}' AS dashboard_db`);
  db.exec(`PRAGMA dashboard_db.journal_mode = WAL;`);
  db.exec(`
    CREATE TABLE IF NOT EXISTS dashboard_db.dashboard_daily_stats (
      date TEXT NOT NULL,
      version INTEGER NOT NULL,
      start_time INTEGER NOT NULL,
      end_time INTEGER NOT NULL,
      total_plays INTEGER NOT NULL DEFAULT 0,
      logged_in_plays INTEGER NOT NULL DEFAULT 0,
      anon_plays INTEGER NOT NULL DEFAULT 0,
      unique_listeners INTEGER NOT NULL DEFAULT 0,
      active_users INTEGER NOT NULL DEFAULT 0,
      listen_hours REAL NOT NULL DEFAULT 0,
      new_users INTEGER NOT NULL DEFAULT 0,
      favorites_added INTEGER NOT NULL DEFAULT 0,
      cumulative_users INTEGER NOT NULL DEFAULT 0,
      cumulative_favorites INTEGER NOT NULL DEFAULT 0,
      top_systems_json TEXT NOT NULL DEFAULT '{}',
      top_formats_json TEXT NOT NULL DEFAULT '{}',
      total_sessions INTEGER NOT NULL DEFAULT 0,
      total_session_plays INTEGER NOT NULL DEFAULT 0,
      total_session_duration_sec INTEGER NOT NULL DEFAULT 0,
      bounce_count INTEGER NOT NULL DEFAULT 0,
      play_bins_json TEXT NOT NULL DEFAULT '{}',
      duration_bins_json TEXT NOT NULL DEFAULT '{}',
      updated_at INTEGER NOT NULL,
      PRIMARY KEY (date, version)
    );
    CREATE INDEX IF NOT EXISTS dashboard_db.idx_dashboard_daily_stats_date_ver 
      ON dashboard_daily_stats(date, version);

    CREATE TABLE IF NOT EXISTS dashboard_db.dashboard_cache (
      cache_key TEXT NOT NULL,
      version INTEGER NOT NULL,
      data_json TEXT NOT NULL,
      created_at INTEGER NOT NULL,
      is_frozen INTEGER NOT NULL DEFAULT 0,
      PRIMARY KEY (cache_key, version)
    );
    CREATE INDEX IF NOT EXISTS dashboard_db.idx_dashboard_cache_key_ver 
      ON dashboard_cache(cache_key, version);
  `);

  // Ensure new columns exist if table was created in an earlier version
  const extraCols = [
    "top_systems_json TEXT NOT NULL DEFAULT '{}'",
    "top_formats_json TEXT NOT NULL DEFAULT '{}'",
    "total_sessions INTEGER NOT NULL DEFAULT 0",
    "total_session_plays INTEGER NOT NULL DEFAULT 0",
    "total_session_duration_sec INTEGER NOT NULL DEFAULT 0",
    "bounce_count INTEGER NOT NULL DEFAULT 0",
    "play_bins_json TEXT NOT NULL DEFAULT '{}'",
    "duration_bins_json TEXT NOT NULL DEFAULT '{}'",
  ];
  for (const colDef of extraCols) {
    try {
      db.exec(`ALTER TABLE dashboard_db.dashboard_daily_stats ADD COLUMN ${colDef}`);
    } catch (e) {
      // Column already exists
    }
  }
} catch (e) {
  // If already attached, ignore error
  if (!String(e.message).includes('already in use')) {
    console.error('Error attaching dashboard_db:', e);
  }
}

// 1-hour in-memory cache for aggregate dashboard stats
const statsCache = new LRUCache({
  max: 50,
  ttl: 1000 * 60 * 60, // 1 hour
});

// --- Pacific Time Formatters ---
const ptDateFormatter = new Intl.DateTimeFormat('en-CA', {
  timeZone: TIMEZONE,
  year: 'numeric',
  month: '2-digit',
  day: '2-digit',
});

const ptHourFormatter = new Intl.DateTimeFormat('en-CA', {
  timeZone: TIMEZONE,
  year: 'numeric',
  month: '2-digit',
  day: '2-digit',
  hour: '2-digit',
  hour12: false,
});

const ptMonthFormatter = new Intl.DateTimeFormat('en-CA', {
  timeZone: TIMEZONE,
  year: 'numeric',
  month: '2-digit',
});

/**
 * Format a Date object or Unix timestamp to YYYY-MM-DD in Pacific Time
 */
function formatDatePt(dOrSec) {
  const d = typeof dOrSec === 'number' ? new Date(dOrSec * 1000) : dOrSec;
  return ptDateFormatter.format(d);
}

/**
 * Format a Date object or Unix timestamp to YYYY-MM-DD HH:00 in Pacific Time
 */
function formatHourPt(dOrSec) {
  const d = typeof dOrSec === 'number' ? new Date(dOrSec * 1000) : dOrSec;
  const parts = Object.fromEntries(ptHourFormatter.formatToParts(d).map(p => [p.type, p.value]));
  return `${parts.year}-${parts.month}-${parts.day} ${parts.hour}:00`;
}

/**
 * Format a Date object or Unix timestamp to YYYY-MM in Pacific Time
 */
function formatMonthPt(dOrSec) {
  const d = typeof dOrSec === 'number' ? new Date(dOrSec * 1000) : dOrSec;
  return ptMonthFormatter.format(d);
}

// Predefined histogram bucket definitions for session analytics
const PLAY_BUCKETS_DEF = [
  { name: '1 play', min: 1, max: 1 },
  { name: '2–5', min: 2, max: 5 },
  { name: '6–10', min: 6, max: 10 },
  { name: '11–25', min: 11, max: 25 },
  { name: '26–50', min: 26, max: 50 },
  { name: '51–100', min: 51, max: 100 },
  { name: '100+', min: 101, max: 150 },
];

const DURATION_BUCKETS_DEF = [
  { name: '< 1 min', min: 0.1, max: 1.0 },
  { name: '1–5 min', min: 1.0, max: 5.0 },
  { name: '5–15 min', min: 5.0, max: 15.0 },
  { name: '15–30 min', min: 15.0, max: 30.0 },
  { name: '30–60 min', min: 30.0, max: 60.0 },
  { name: '1–2 hrs', min: 60.0, max: 120.0 },
  { name: '2+ hrs', min: 120.0, max: 180.0 },
];

/**
 * Approximate median value from discrete histogram buckets via linear interpolation
 */
function interpolateMedian(bucketsDef, bins, total) {
  if (total <= 0) return 0;
  const target = total / 2;
  let cum = 0;
  for (const b of bucketsDef) {
    const count = bins[b.name] || 0;
    if (cum + count >= target) {
      if (count === 0) return b.min;
      const frac = (target - cum) / count;
      return Number((b.min + frac * (b.max - b.min)).toFixed(1));
    }
    cum += count;
  }
  return bucketsDef[bucketsDef.length - 1].max;
}

/**
 * Convert a Pacific calendar date string (YYYY-MM-DD) to Unix epoch seconds
 */
function pacificDayToUnix(dateStr, timeStr = '00:00:00') {
  const [y, m, d] = dateStr.split('-').map(Number);
  const [h, min, s] = timeStr.split(':').map(Number);
  let guessMs = Date.UTC(y, m - 1, d, h + 8, min, s);
  const parts = Object.fromEntries(
    new Intl.DateTimeFormat('en-US', {
      timeZone: TIMEZONE,
      year: 'numeric', month: 'numeric', day: 'numeric',
      hour: 'numeric', minute: 'numeric', second: 'numeric',
      hour12: false,
    }).formatToParts(new Date(guessMs)).map(p => [p.type, p.value])
  );
  let ph = Number(parts.hour);
  if (ph === 24) ph = 0;
  const diffHours = h - ph;
  guessMs += diffHours * 3600000;
  return Math.floor(guessMs / 1000);
}

/**
 * Return the next day string in YYYY-MM-DD in Pacific Time
 */
function getNextPacificDayStr(dateStr) {
  const [y, m, d] = dateStr.split('-').map(Number);
  const nextDate = new Date(Date.UTC(y, m - 1, d + 1, 12, 0, 0));
  return ptDateFormatter.format(nextDate);
}

/**
 * Return startSec (00:00:00 PT) and endSec (next day 00:00:00 PT) for a Pacific date
 */
function getPacificDayBounds(dateStr) {
  const startSec = pacificDayToUnix(dateStr, '00:00:00');
  const nextDayStr = getNextPacificDayStr(dateStr);
  const endSec = pacificDayToUnix(nextDayStr, '00:00:00');
  return { startSec, endSec };
}

/**
 * Return the earliest activity timestamp across users, playlists, and playbacks.
 */
function getGlobalEarliestSec() {
  try {
    const minUser = db.prepare('SELECT MIN(created_at) as m FROM user_db.users').get()?.m;
    const minFav = db.prepare(`
      SELECT MIN(cast(json_extract(je.value, '$.mtime') as integer)) as m 
      FROM user_db.playlists p, json_each(p.items) je 
      WHERE p.type = 'favorites' AND json_extract(je.value, '$.mtime') IS NOT NULL
    `).get()?.m;
    const minPlay = db.prepare('SELECT MIN(played_at) as m FROM user_db.playbacks').get()?.m;

    const valid = [minUser, minFav, minPlay].filter(t => typeof t === 'number' && t > 0);
    return valid.length > 0 ? Math.min(...valid) : 1544404726; // Default to Dec 10, 2018
  } catch (e) {
    return 1544404726;
  }
}

/**
 * Generate contiguous bucket keys from start to end in Pacific Time
 */
function generateBuckets(startSec, endSec, resolution) {
  const buckets = [];
  if (resolution === 'hour') {
    const startHourSec = Math.floor(startSec / 3600) * 3600;
    let curr = startHourSec;
    while (curr <= endSec) {
      buckets.push({
        key: formatHourPt(curr),
        timestamp: curr,
      });
      curr += 3600;
    }
  } else if (resolution === 'day') {
    let currDayStr = formatDatePt(startSec);
    const endDayStr = formatDatePt(Math.max(startSec, endSec - 1));
    while (currDayStr <= endDayStr) {
      const bounds = getPacificDayBounds(currDayStr);
      buckets.push({
        key: currDayStr,
        timestamp: bounds.startSec,
      });
      currDayStr = getNextPacificDayStr(currDayStr);
    }
  } else if (resolution === 'month') {
    let currDayStr = formatDatePt(startSec).slice(0, 7) + '-01';
    const endMonthStr = formatDatePt(Math.max(startSec, endSec - 1)).slice(0, 7);
    while (currDayStr.slice(0, 7) <= endMonthStr) {
      const monthKey = currDayStr.slice(0, 7);
      const bounds = getPacificDayBounds(currDayStr);
      buckets.push({
        key: monthKey,
        timestamp: bounds.startSec,
      });
      const [y, m] = monthKey.split('-').map(Number);
      const nextMonthDate = new Date(Date.UTC(y, m, 1, 12, 0, 0));
      currDayStr = ptDateFormatter.format(nextMonthDate);
    }
  }
  return buckets;
}

/**
 * Resolve start, end, resolution from query parameters in Pacific Time
 */
function parseTimeRange(params) {
  const CACHE_WINDOW_SEC = 3600;
  const rawNow = Math.floor(Date.now() / 1000);
  const now = Math.floor(rawNow / CACHE_WINDOW_SEC) * CACHE_WINDOW_SEC;
  const todayPt = formatDatePt(now);
  const todayBounds = getPacificDayBounds(todayPt);
  const todayStartSec = todayBounds.startSec;

  let range = params.range || '30d';
  let start;
  let end = now;
  let resolution = params.resolution || 'auto';

  if (params.start && params.end) {
    range = 'custom';
    if (typeof params.start === 'string' && params.start.includes('-')) {
      start = getPacificDayBounds(params.start).startSec;
    } else {
      start = Number(params.start);
      if (isNaN(start)) start = Math.floor(new Date(params.start).getTime() / 1000);
    }

    if (typeof params.end === 'string' && params.end.includes('-')) {
      end = getPacificDayBounds(params.end).endSec;
    } else {
      end = Number(params.end);
      if (isNaN(end)) end = Math.floor(new Date(params.end).getTime() / 1000);
    }

    if (isNaN(start) || isNaN(end) || start >= end) {
      start = todayStartSec - 29 * 86400;
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
        start = todayStartSec - 29 * 86400;
        if (resolution === 'auto') resolution = 'day';
        break;
      case '90d':
        start = todayStartSec - 89 * 86400;
        if (resolution === 'auto') resolution = 'day';
        break;
      case '1y':
        start = todayStartSec - 364 * 86400;
        if (resolution === 'auto') resolution = 'day';
        break;
      case 'all': {
        const earliestSec = getGlobalEarliestSec();
        const earliestDayStr = formatDatePt(earliestSec);
        start = getPacificDayBounds(earliestDayStr).startSec;
        if (resolution === 'auto') resolution = 'month';
        break;
      }
      default:
        range = '30d';
        start = todayStartSec - 29 * 86400;
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

  return { range, start, end, resolution, todayPt, todayStartSec };
}

/**
 * Get SQLite strftime pattern for a resolution in Pacific Time (localtime)
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
 * Synchronize daily stats in dashboard_cache.db for all completed Pacific days up to yesterday
 */
function syncDailyStats(forceRebuild = false) {
  const now = Math.floor(Date.now() / 1000);
  const yesterdayPt = formatDatePt(now - 86400);

  // Prune any obsolete version records
  try {
    db.prepare('DELETE FROM dashboard_db.dashboard_daily_stats WHERE version < ?').run(DASHBOARD_VERSION);
    db.prepare('DELETE FROM dashboard_db.dashboard_cache WHERE version < ?').run(DASHBOARD_VERSION);
  } catch (e) {
    // Ignore
  }

  if (forceRebuild) {
    db.prepare('DELETE FROM dashboard_db.dashboard_daily_stats WHERE version = ?').run(DASHBOARD_VERSION);
  }

  const maxCachedRow = db.prepare(`
    SELECT MAX(date) as max_d 
    FROM dashboard_db.dashboard_daily_stats 
    WHERE version = ?
  `).get(DASHBOARD_VERSION);

  const lastCachedDate = maxCachedRow ? maxCachedRow.max_d : null;
  if (lastCachedDate && lastCachedDate >= yesterdayPt) {
    return; // Already up to date through yesterday
  }

  const globalMinSec = getGlobalEarliestSec();
  const minDate = formatDatePt(globalMinSec);
  const startDate = (lastCachedDate && lastCachedDate >= minDate) ? getNextPacificDayStr(lastCachedDate) : minDate;
  if (startDate > yesterdayPt) {
    return;
  }

  // Aggregate completed days up to yesterday
  const playRows = db.prepare(`
    SELECT 
      strftime('%Y-%m-%d', played_at, 'unixepoch', 'localtime') AS day,
      COUNT(*) AS total_plays,
      COUNT(user_id) AS logged_in_plays,
      COUNT(DISTINCT COALESCE(user_id, ip_address)) AS unique_listeners,
      COUNT(DISTINCT user_id) AS active_users,
      ROUND(SUM(duration_ms) / 3600000.0, 2) AS listen_hours
    FROM user_db.playbacks
    WHERE strftime('%Y-%m-%d', played_at, 'unixepoch', 'localtime') >= ?
      AND strftime('%Y-%m-%d', played_at, 'unixepoch', 'localtime') <= ?
    GROUP BY day
  `).all(startDate, yesterdayPt);
  const playMap = new Map(playRows.map(r => [r.day, r]));

  const userRows = db.prepare(`
    SELECT 
      strftime('%Y-%m-%d', created_at, 'unixepoch', 'localtime') AS day,
      COUNT(*) AS new_users
    FROM user_db.users
    WHERE strftime('%Y-%m-%d', created_at, 'unixepoch', 'localtime') >= ?
      AND strftime('%Y-%m-%d', created_at, 'unixepoch', 'localtime') <= ?
    GROUP BY day
  `).all(startDate, yesterdayPt);
  const userMap = new Map(userRows.map(r => [r.day, r.new_users]));

  const favRows = db.prepare(`
    SELECT 
      strftime('%Y-%m-%d', cast(json_extract(je.value, '$.mtime') as integer), 'unixepoch', 'localtime') AS day,
      COUNT(*) AS favorites_added
    FROM user_db.playlists p, json_each(p.items) je
    WHERE p.type = 'favorites' 
      AND strftime('%Y-%m-%d', cast(json_extract(je.value, '$.mtime') as integer), 'unixepoch', 'localtime') >= ?
      AND strftime('%Y-%m-%d', cast(json_extract(je.value, '$.mtime') as integer), 'unixepoch', 'localtime') <= ?
    GROUP BY day
  `).all(startDate, yesterdayPt);
  const favMap = new Map(favRows.map(r => [r.day, r.favorites_added]));

  // Batch aggregate system & format counts for days that have playbacks
  const startSec = getPacificDayBounds(startDate).startSec;
  const endSec = getPacificDayBounds(yesterdayPt).endSec;

  const dailySystemsMap = new Map();
  const dailyFormatsMap = new Map();

  if (playRows.length > 0) {
    const metaRows = db.prepare(`
      SELECT 
        strftime('%Y-%m-%d', p.played_at, 'unixepoch', 'localtime') AS day,
        m.system,
        m.extension,
        SUM(p.plays) as plays
      FROM (
        SELECT 
          played_at,
          song_id,
          COUNT(*) as plays
        FROM user_db.playbacks
        WHERE played_at >= ? AND played_at < ?
        GROUP BY strftime('%Y-%m-%d', played_at, 'unixepoch', 'localtime'), song_id
      ) p
      JOIN music m ON m.song_id = p.song_id
      GROUP BY day, m.system, m.extension
    `).all(startSec, endSec);

    for (const r of metaRows) {
      if (r.system) {
        let sysObj = dailySystemsMap.get(r.day);
        if (!sysObj) { sysObj = {}; dailySystemsMap.set(r.day, sysObj); }
        sysObj[r.system] = (sysObj[r.system] || 0) + r.plays;
      }
      if (r.extension) {
        let fmtObj = dailyFormatsMap.get(r.day);
        if (!fmtObj) { fmtObj = {}; dailyFormatsMap.set(r.day, fmtObj); }
        const ext = r.extension.toUpperCase();
        fmtObj[ext] = (fmtObj[ext] || 0) + r.plays;
      }
    }
  }

  // Prepared statement for session clustering per day
  const dailySessionStmt = db.prepare(`
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
  `);

  // Get cumulative baseline before startDate
  let cumulativeUsers = 0;
  let cumulativeFavs = 0;

  if (lastCachedDate) {
    const prevRow = db.prepare(`
      SELECT cumulative_users, cumulative_favorites 
      FROM dashboard_db.dashboard_daily_stats 
      WHERE date = ? AND version = ?
    `).get(lastCachedDate, DASHBOARD_VERSION);
    if (prevRow) {
      cumulativeUsers = prevRow.cumulative_users;
      cumulativeFavs = prevRow.cumulative_favorites;
    }
  } else {
    const initUsers = db.prepare(`
      SELECT COUNT(*) as c FROM user_db.users 
      WHERE strftime('%Y-%m-%d', created_at, 'unixepoch', 'localtime') < ?
    `).get(startDate);
    cumulativeUsers = initUsers ? initUsers.c : 0;

    const initFavs = db.prepare(`
      SELECT COUNT(*) as c FROM user_db.playlists p, json_each(p.items) je 
      WHERE p.type = 'favorites' 
        AND strftime('%Y-%m-%d', cast(json_extract(je.value, '$.mtime') as integer), 'unixepoch', 'localtime') < ?
    `).get(startDate);
    cumulativeFavs = initFavs ? initFavs.c : 0;
  }

  // Insert day by day within a single transaction
  const insertStmt = db.prepare(`
    INSERT OR REPLACE INTO dashboard_db.dashboard_daily_stats (
      date, version, start_time, end_time, total_plays, logged_in_plays, anon_plays,
      unique_listeners, active_users, listen_hours, new_users, favorites_added,
      cumulative_users, cumulative_favorites,
      top_systems_json, top_formats_json,
      total_sessions, total_session_plays, total_session_duration_sec, bounce_count,
      play_bins_json, duration_bins_json,
      updated_at
    ) VALUES (
      @date, @version, @start_time, @end_time, @total_plays, @logged_in_plays, @anon_plays,
      @unique_listeners, @active_users, @listen_hours, @new_users, @favorites_added,
      @cumulative_users, @cumulative_favorites,
      @top_systems_json, @top_formats_json,
      @total_sessions, @total_session_plays, @total_session_duration_sec, @bounce_count,
      @play_bins_json, @duration_bins_json,
      @updated_at
    )
  `);

  const syncTx = db.transaction(() => {
    let currDay = startDate;
    const updatedAt = Math.floor(Date.now() / 1000);

    while (currDay <= yesterdayPt) {
      const bounds = getPacificDayBounds(currDay);
      const p = playMap.get(currDay);
      const totalPlays = p ? p.total_plays : 0;
      const loggedInPlays = p ? p.logged_in_plays : 0;
      const anonPlays = Math.max(0, totalPlays - loggedInPlays);
      const uniqueListeners = p ? p.unique_listeners : 0;
      const activeUsers = p ? p.active_users : 0;
      const listenHours = p ? p.listen_hours : 0;

      const newUsers = userMap.get(currDay) || 0;
      const favoritesAdded = favMap.get(currDay) || 0;

      cumulativeUsers += newUsers;
      cumulativeFavs += favoritesAdded;

      let topSystemsJson = '{}';
      let topFormatsJson = '{}';
      let totalSessions = 0;
      let totalSessionPlays = 0;
      let totalSessionDurationSec = 0;
      let bounceCount = 0;
      let playBinsJson = '{}';
      let durationBinsJson = '{}';

      if (totalPlays > 0) {
        const sysObj = dailySystemsMap.get(currDay) || {};
        const fmtObj = dailyFormatsMap.get(currDay) || {};
        topSystemsJson = JSON.stringify(sysObj);
        topFormatsJson = JSON.stringify(fmtObj);

        const sessRows = dailySessionStmt.all(bounds.startSec, bounds.endSec, 3600);
        totalSessions = sessRows.length;
        const pBins = {
          '1 play': 0,
          '2–5': 0,
          '6–10': 0,
          '11–25': 0,
          '26–50': 0,
          '51–100': 0,
          '100+': 0,
        };
        const dBins = {
          '< 1 min': 0,
          '1–5 min': 0,
          '5–15 min': 0,
          '15–30 min': 0,
          '30–60 min': 0,
          '1–2 hrs': 0,
          '2+ hrs': 0,
        };

        for (const row of sessRows) {
          const pc = row.playCount;
          const d = row.durationSeconds;
          totalSessionPlays += pc;
          totalSessionDurationSec += d;
          if (pc === 1) {
            bounceCount++;
            pBins['1 play']++;
          } else if (pc <= 5) pBins['2–5']++;
          else if (pc <= 10) pBins['6–10']++;
          else if (pc <= 25) pBins['11–25']++;
          else if (pc <= 50) pBins['26–50']++;
          else if (pc <= 100) pBins['51–100']++;
          else pBins['100+']++;

          const dm = d / 60.0;
          if (dm < 1) dBins['< 1 min']++;
          else if (dm < 5) dBins['1–5 min']++;
          else if (dm < 15) dBins['5–15 min']++;
          else if (dm < 30) dBins['15–30 min']++;
          else if (dm < 60) dBins['30–60 min']++;
          else if (dm < 120) dBins['1–2 hrs']++;
          else dBins['2+ hrs']++;
        }

        playBinsJson = JSON.stringify(pBins);
        durationBinsJson = JSON.stringify(dBins);
      }

      insertStmt.run({
        date: currDay,
        version: DASHBOARD_VERSION,
        start_time: bounds.startSec,
        end_time: bounds.endSec,
        total_plays: totalPlays,
        logged_in_plays: loggedInPlays,
        anon_plays: anonPlays,
        unique_listeners: uniqueListeners,
        active_users: activeUsers,
        listen_hours: listenHours,
        new_users: newUsers,
        favorites_added: favoritesAdded,
        cumulative_users: cumulativeUsers,
        cumulative_favorites: cumulativeFavs,
        top_systems_json: topSystemsJson,
        top_formats_json: topFormatsJson,
        total_sessions: totalSessions,
        total_session_plays: totalSessionPlays,
        total_session_duration_sec: totalSessionDurationSec,
        bounce_count: bounceCount,
        play_bins_json: playBinsJson,
        duration_bins_json: durationBinsJson,
        updated_at: updatedAt,
      });

      currDay = getNextPacificDayStr(currDay);
    }
  });

  syncTx();
}

/**
 * Compute all dashboard stats for the given time range
 */
function getDashboardStats(params = {}) {
  const { range, start, end, resolution, todayPt, todayStartSec } = parseTimeRange(params);
  const cacheKey = `${range}_${start}_${end}_${resolution}`;

  // 1. Check in-memory LRU cache first (if not force refresh)
  if (!params.refresh) {
    const memoryCached = statsCache.get(cacheKey);
    if (memoryCached) {
      return memoryCached;
    }

    // 2. Check persistent SQLite cache in dashboard_cache.db
    try {
      const dbCachedRow = db.prepare(`
        SELECT data_json, is_frozen, created_at 
        FROM dashboard_db.dashboard_cache 
        WHERE cache_key = ? AND version = ?
      `).get(cacheKey, DASHBOARD_VERSION);

      if (dbCachedRow) {
        // Frozen historical ranges never expire!
        if (dbCachedRow.is_frozen === 1) {
          const parsed = JSON.parse(dbCachedRow.data_json);
          statsCache.set(cacheKey, parsed);
          return parsed;
        }

        // Active preset windows check TTL (1 hour)
        const ageSec = Math.floor(Date.now() / 1000) - dbCachedRow.created_at;
        if (ageSec < 3600) {
          const parsed = JSON.parse(dbCachedRow.data_json);
          statsCache.set(cacheKey, parsed);
          return parsed;
        }
      }
    } catch (e) {
      console.error('Error reading dashboard_cache:', e);
    }
  }

  // Ensure daily stats cache is up to date through yesterday
  syncDailyStats(params.rebuild_cache === '1');

  const sqlDatePattern = getSqlDatePattern(resolution);
  const duration = end - start;
  const prevStart = start - duration;
  const prevEnd = start;

  let timeSeries = [];
  let cumulativeUsers = 0;
  let cumulativeFavorites = 0;
  let pastDayRows = [];

  if (resolution === 'day') {
    // Read past completed days from dashboard_daily_stats
    const startDayStr = formatDatePt(start);
    const endDayStr = formatDatePt(Math.max(start, end - 1));

    pastDayRows = db.prepare(`
      SELECT * 
      FROM dashboard_db.dashboard_daily_stats 
      WHERE date >= ? AND date <= ? AND version = ?
      ORDER BY date ASC
    `).all(startDayStr, endDayStr, DASHBOARD_VERSION);

    const pastDayMap = new Map(pastDayRows.map(r => [r.date, r]));

    // Check if range covers "today"
    let todayData = null;
    if (endDayStr >= todayPt && start <= (todayStartSec + 86400)) {
      const todayPlays = db.prepare(`
        SELECT 
          COUNT(*) AS total_plays,
          COUNT(user_id) AS logged_in_plays,
          COUNT(DISTINCT COALESCE(user_id, ip_address)) AS unique_listeners,
          COUNT(DISTINCT user_id) AS active_users,
          ROUND(SUM(duration_ms) / 3600000.0, 2) AS listen_hours
        FROM user_db.playbacks
        WHERE played_at >= ? AND played_at < ?
      `).get(todayStartSec, end);

      const todayUsers = db.prepare(`
        SELECT COUNT(*) AS new_users 
        FROM user_db.users 
        WHERE created_at >= ? AND created_at < ?
      `).get(todayStartSec, end).new_users;

      const todayFavs = db.prepare(`
        SELECT COUNT(*) AS favorites_added
        FROM user_db.playlists p, json_each(p.items) je
        WHERE p.type = 'favorites' 
          AND cast(json_extract(je.value, '$.mtime') as integer) >= ? 
          AND cast(json_extract(je.value, '$.mtime') as integer) < ?
      `).get(todayStartSec, end).favorites_added;

      todayData = {
        totalPlays: todayPlays ? todayPlays.total_plays : 0,
        loggedInPlays: todayPlays ? todayPlays.logged_in_plays : 0,
        uniqueListeners: todayPlays ? todayPlays.unique_listeners : 0,
        activeUsers: todayPlays ? todayPlays.active_users : 0,
        listenHours: todayPlays ? todayPlays.listen_hours || 0 : 0,
        newUsers: todayUsers,
        favoritesAdded: todayFavs,
      };
    }

    // Determine initial cumulative baseline before startDayStr
    const prevDayRow = db.prepare(`
      SELECT cumulative_users, cumulative_favorites 
      FROM dashboard_db.dashboard_daily_stats 
      WHERE date < ? AND version = ?
      ORDER BY date DESC LIMIT 1
    `).get(startDayStr, DASHBOARD_VERSION);

    if (prevDayRow) {
      cumulativeUsers = prevDayRow.cumulative_users;
      cumulativeFavorites = prevDayRow.cumulative_favorites;
    } else {
      const initU = db.prepare(`
        SELECT COUNT(*) as c FROM user_db.users 
        WHERE strftime('%Y-%m-%d', created_at, 'unixepoch', 'localtime') < ?
      `).get(startDayStr);
      cumulativeUsers = initU ? initU.c : 0;

      const initF = db.prepare(`
        SELECT COUNT(*) as c FROM user_db.playlists p, json_each(p.items) je 
        WHERE p.type = 'favorites' 
          AND strftime('%Y-%m-%d', cast(json_extract(je.value, '$.mtime') as integer), 'unixepoch', 'localtime') < ?
      `).get(startDayStr);
      cumulativeFavorites = initF ? initF.c : 0;
    }

    // Build contiguous daily time series
    const bucketList = generateBuckets(start, end, 'day');
    timeSeries = bucketList.map(({ key, timestamp }) => {
      let d = pastDayMap.get(key);
      if (key === todayPt && todayData) {
        d = todayData;
      }

      const totalPlays = d ? d.totalPlays || d.total_plays || 0 : 0;
      const loggedInPlays = d ? d.loggedInPlays || d.logged_in_plays || 0 : 0;
      const anonPlays = Math.max(0, totalPlays - loggedInPlays);
      const uniqueListeners = d ? d.uniqueListeners || d.unique_listeners || 0 : 0;
      const activeUsers = d ? d.activeUsers || d.active_users || 0 : 0;
      const listenHours = d ? d.listenHours || d.listen_hours || 0 : 0;
      const newUsers = d ? d.newUsers || d.new_users || 0 : 0;
      const favoritesAdded = d ? d.favoritesAdded || d.favorites_added || 0 : 0;

      cumulativeUsers += newUsers;
      cumulativeFavorites += favoritesAdded;

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
        favoritesAdded,
        cumulativeFavorites,
      };
    });
  } else if (resolution === 'month') {
    // Read past completed days grouped by month
    const startDayStr = formatDatePt(start);
    const endDayStr = formatDatePt(Math.max(start, end - 1));

    pastDayRows = db.prepare(`
      SELECT * 
      FROM dashboard_db.dashboard_daily_stats 
      WHERE date >= ? AND date <= ? AND version = ?
      ORDER BY date ASC
    `).all(startDayStr, endDayStr, DASHBOARD_VERSION);

    const monthRows = db.prepare(`
      SELECT 
        strftime('%Y-%m', date) AS month,
        SUM(total_plays) AS total_plays,
        SUM(logged_in_plays) AS logged_in_plays,
        SUM(anon_plays) AS anon_plays,
        MAX(unique_listeners) AS unique_listeners,
        MAX(active_users) AS active_users,
        ROUND(SUM(listen_hours), 2) AS listen_hours,
        SUM(new_users) AS new_users,
        SUM(favorites_added) AS favorites_added
      FROM dashboard_db.dashboard_daily_stats 
      WHERE date >= ? AND date <= ? AND version = ?
      GROUP BY month
      ORDER BY month ASC
    `).all(startDayStr, endDayStr, DASHBOARD_VERSION);
    const monthMap = new Map(monthRows.map(r => [r.month, r]));

    // Check if end covers today
    const currentMonthStr = todayPt.slice(0, 7);
    if (endDayStr >= todayPt) {
      const todayMonthPlays = db.prepare(`
        SELECT 
          COUNT(*) AS total_plays,
          COUNT(user_id) AS logged_in_plays,
          ROUND(SUM(duration_ms) / 3600000.0, 2) AS listen_hours
        FROM user_db.playbacks
        WHERE played_at >= ? AND played_at < ?
      `).get(todayStartSec, end);

      const todayMonthUsers = db.prepare(`
        SELECT COUNT(*) AS new_users FROM user_db.users WHERE created_at >= ? AND created_at < ?
      `).get(todayStartSec, end).new_users;

      const todayMonthFavs = db.prepare(`
        SELECT COUNT(*) AS favorites_added
        FROM user_db.playlists p, json_each(p.items) je
        WHERE p.type = 'favorites' 
          AND cast(json_extract(je.value, '$.mtime') as integer) >= ? 
          AND cast(json_extract(je.value, '$.mtime') as integer) < ?
      `).get(todayStartSec, end).favorites_added;

      const m = monthMap.get(currentMonthStr) || {
        total_plays: 0,
        logged_in_plays: 0,
        anon_plays: 0,
        unique_listeners: 0,
        active_users: 0,
        listen_hours: 0,
        new_users: 0,
        favorites_added: 0,
      };

      m.total_plays += (todayMonthPlays ? todayMonthPlays.total_plays : 0);
      m.logged_in_plays += (todayMonthPlays ? todayMonthPlays.logged_in_plays : 0);
      m.anon_plays = Math.max(0, m.total_plays - m.logged_in_plays);
      m.listen_hours = Number(((m.listen_hours || 0) + (todayMonthPlays ? todayMonthPlays.listen_hours || 0 : 0)).toFixed(2));
      m.new_users += todayMonthUsers;
      m.favorites_added += todayMonthFavs;
      monthMap.set(currentMonthStr, m);
    }

    // Cumulative baseline before start
    const initU = db.prepare(`
      SELECT COUNT(*) as c FROM user_db.users 
      WHERE strftime('%Y-%m', created_at, 'unixepoch', 'localtime') < ?
    `).get(startDayStr.slice(0, 7));
    cumulativeUsers = initU ? initU.c : 0;

    const initF = db.prepare(`
      SELECT COUNT(*) as c FROM user_db.playlists p, json_each(p.items) je 
      WHERE p.type = 'favorites' 
        AND strftime('%Y-%m', cast(json_extract(je.value, '$.mtime') as integer), 'unixepoch', 'localtime') < ?
    `).get(startDayStr.slice(0, 7));
    cumulativeFavorites = initF ? initF.c : 0;

    const bucketList = generateBuckets(start, end, 'month');
    timeSeries = bucketList.map(({ key, timestamp }) => {
      const m = monthMap.get(key);
      const totalPlays = m ? m.total_plays : 0;
      const loggedInPlays = m ? m.logged_in_plays : 0;
      const anonPlays = m ? m.anon_plays : 0;
      const uniqueListeners = m ? m.unique_listeners : 0;
      const activeUsers = m ? m.active_users : 0;
      const listenHours = m ? m.listen_hours : 0;
      const newUsers = m ? m.new_users : 0;
      const favoritesAdded = m ? m.favorites_added : 0;

      cumulativeUsers += newUsers;
      cumulativeFavorites += favoritesAdded;

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
        favoritesAdded,
        cumulativeFavorites,
      };
    });
  } else {
    // resolution === 'hour' (used for 24h and 7d, query raw playbacks with index on played_at)
    const playbackRows = db.prepare(`
      SELECT 
        strftime('${sqlDatePattern}', played_at, 'unixepoch', 'localtime') AS bucket,
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

    const newUserRows = db.prepare(`
      SELECT 
        strftime('${sqlDatePattern}', created_at, 'unixepoch', 'localtime') AS bucket,
        COUNT(*) AS new_users
      FROM user_db.users
      WHERE created_at >= ? AND created_at < ?
      GROUP BY bucket
      ORDER BY bucket ASC
    `).all(start, end);

    const favoriteRows = db.prepare(`
      SELECT 
        strftime('${sqlDatePattern}', cast(json_extract(je.value, '$.mtime') as integer), 'unixepoch', 'localtime') AS bucket,
        COUNT(*) AS favorites_added
      FROM user_db.playlists p, json_each(p.items) je
      WHERE p.type = 'favorites' 
        AND cast(json_extract(je.value, '$.mtime') as integer) >= ? 
        AND cast(json_extract(je.value, '$.mtime') as integer) < ?
      GROUP BY bucket
      ORDER BY bucket ASC
    `).all(start, end);

    const initialUsersRow = db.prepare(`
      SELECT COUNT(*) as count FROM user_db.users WHERE created_at < ?
    `).get(start);
    cumulativeUsers = initialUsersRow ? initialUsersRow.count : 0;

    const initialFavsRow = db.prepare(`
      SELECT COUNT(*) as count 
      FROM user_db.playlists p, json_each(p.items) je 
      WHERE p.type = 'favorites' AND cast(json_extract(je.value, '$.mtime') as integer) < ?
    `).get(start);
    cumulativeFavorites = initialFavsRow ? initialFavsRow.count : 0;

    const playbackMap = new Map(playbackRows.map(r => [r.bucket, r]));
    const newUserMap = new Map(newUserRows.map(r => [r.bucket, r.new_users]));
    const favoriteMap = new Map(favoriteRows.map(r => [r.bucket, r.favorites_added]));

    const bucketList = generateBuckets(start, end, 'hour');
    timeSeries = bucketList.map(({ key, timestamp }) => {
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
  }

  // Summary totals for current and previous period
  const daysInPeriod = Math.max(1, (end - start) / 86400);

  // Current period totals derived directly from timeSeries buckets (instant, 0 scans)
  const totalPlays = timeSeries.reduce((acc, b) => acc + (b.totalPlays || 0), 0);
  const loggedInPlays = timeSeries.reduce((acc, b) => acc + (b.loggedInPlays || 0), 0);
  const currentNewUsers = timeSeries.reduce((acc, b) => acc + (b.newUsers || 0), 0);
  const currentFavs = timeSeries.reduce((acc, b) => acc + (b.favoritesAdded || 0), 0);
  const listenHours = Number(timeSeries.reduce((acc, b) => acc + (b.listenHours || 0), 0).toFixed(1));

  let uniqueListeners = 0;
  if (totalPlays > 0) {
    const ulRow = db.prepare(`
      SELECT COUNT(DISTINCT COALESCE(user_id, ip_address)) AS c
      FROM user_db.playbacks
      WHERE played_at >= ? AND played_at < ?
    `).get(start, end);
    uniqueListeners = ulRow ? ulRow.c : 0;
  }

  // Previous period comparison for KPI delta calculation
  let prevTotalPlays = 0;
  let prevNewUsers = 0;
  let prevFavs = 0;
  let prevUniqueListeners = 0;

  if (resolution === 'hour') {
    const pSum = db.prepare(`
      SELECT 
        COUNT(*) AS total_plays,
        COUNT(DISTINCT COALESCE(user_id, ip_address)) AS unique_listeners
      FROM user_db.playbacks
      WHERE played_at >= ? AND played_at < ?
    `).get(prevStart, prevEnd);
    prevTotalPlays = pSum ? pSum.total_plays : 0;
    prevUniqueListeners = pSum ? pSum.unique_listeners : 0;

    prevNewUsers = db.prepare(`
      SELECT COUNT(*) AS count FROM user_db.users WHERE created_at >= ? AND created_at < ?
    `).get(prevStart, prevEnd).count;

    prevFavs = db.prepare(`
      SELECT COUNT(*) AS count
      FROM user_db.playlists p, json_each(p.items) je
      WHERE p.type = 'favorites' 
        AND cast(json_extract(je.value, '$.mtime') as integer) >= ? 
        AND cast(json_extract(je.value, '$.mtime') as integer) < ?
    `).get(prevStart, prevEnd).count;
  } else {
    // For daily/monthly resolutions, query pre-aggregated daily stats table (0.05ms)
    const prevStartDayStr = formatDatePt(prevStart);
    const prevEndDayStr = formatDatePt(Math.max(prevStart, prevEnd - 1));

    const pRow = db.prepare(`
      SELECT 
        COALESCE(SUM(total_plays), 0) AS total_plays,
        COALESCE(SUM(new_users), 0) AS new_users,
        COALESCE(SUM(favorites_added), 0) AS favorites_added
      FROM dashboard_db.dashboard_daily_stats
      WHERE date >= ? AND date <= ? AND version = ?
    `).get(prevStartDayStr, prevEndDayStr, DASHBOARD_VERSION);

    if (pRow) {
      prevTotalPlays = pRow.total_plays;
      prevNewUsers = pRow.new_users;
      prevFavs = pRow.favorites_added;
    }

    if (prevTotalPlays > 0) {
      const pUl = db.prepare(`
        SELECT COUNT(DISTINCT COALESCE(user_id, ip_address)) AS c
        FROM user_db.playbacks
        WHERE played_at >= ? AND played_at < ?
      `).get(prevStart, prevEnd);
      prevUniqueListeners = pUl ? pUl.c : 0;
    }
  }

  function calcDelta(curr, prev) {
    if (!prev || prev === 0) return 0;
    return Number((((curr - prev) / prev) * 100).toFixed(1));
  }

  const summary = {
    totalPlays,
    prevTotalPlays,
    playsDeltaPercent: calcDelta(totalPlays, prevTotalPlays),

    uniqueListeners,
    prevUniqueListeners,
    listenersDeltaPercent: calcDelta(uniqueListeners, prevUniqueListeners),

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

  // Top Tracks in Period
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

  // Top Favorited Tracks in Period
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

  // Top Systems & Formats
  const systemMap = new Map();
  const formatMap = new Map();
  let totalMetaPlays = 0;

  if (resolution === 'hour') {
    // For 24h / 7d hourly windows, run fast bounded query on raw playbacks
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
  } else {
    // Merge pre-aggregated daily JSON for past days (< 1ms)
    for (const row of pastDayRows) {
      if (row.top_systems_json && row.top_systems_json !== '{}') {
        try {
          const sys = JSON.parse(row.top_systems_json);
          for (const [k, v] of Object.entries(sys)) {
            systemMap.set(k, (systemMap.get(k) || 0) + v);
            totalMetaPlays += v;
          }
        } catch (e) {}
      }
      if (row.top_formats_json && row.top_formats_json !== '{}') {
        try {
          const fmt = JSON.parse(row.top_formats_json);
          for (const [k, v] of Object.entries(fmt)) {
            formatMap.set(k, (formatMap.get(k) || 0) + v);
          }
        } catch (e) {}
      }
    }

    // If range covers today, query today's partial plays only
    const endDayStr = formatDatePt(Math.max(start, end - 1));
    if (endDayStr >= todayPt && end > todayStartSec) {
      const todayMeta = db.prepare(`
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
      `).all(todayStartSec, end);

      for (const row of todayMeta) {
        totalMetaPlays += row.plays;
        if (row.system) {
          systemMap.set(row.system, (systemMap.get(row.system) || 0) + row.plays);
        }
        if (row.extension) {
          const ext = row.extension.toUpperCase();
          formatMap.set(ext, (formatMap.get(ext) || 0) + row.plays);
        }
      }
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

  // Session Analytics
  let totalSessions = 0;
  let totalSessionPlays = 0;
  let totalSessionDurationSec = 0;
  let bounceCount = 0;
  let medianPlaysPerSession = 0;
  let medianDurationMins = 0;

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

  if (resolution === 'hour') {
    // For 24h / 7d hourly windows, compute exact clusters from raw plays
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

    totalSessions = sessionRows.length;
    const playCounts = [];
    const sessionDurations = [];

    for (const row of sessionRows) {
      const p = row.playCount;
      const d = row.durationSeconds;
      totalSessionPlays += p;
      totalSessionDurationSec += d;
      playCounts.push(p);
      sessionDurations.push(d);

      if (p === 1) {
        bounceCount++;
        playBins['1 play']++;
      } else if (p <= 5) playBins['2–5']++;
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
    medianPlaysPerSession = totalSessions > 0 ? playCounts[Math.floor(totalSessions / 2)] : 0;
    medianDurationMins = totalSessions > 0 ? Number((sessionDurations[Math.floor(totalSessions / 2)] / 60.0).toFixed(1)) : 0;
  } else {
    // Merge pre-aggregated daily session metrics from pastDayRows (< 1ms)
    for (const row of pastDayRows) {
      totalSessions += row.total_sessions || 0;
      totalSessionPlays += row.total_session_plays || 0;
      totalSessionDurationSec += row.total_session_duration_sec || 0;
      bounceCount += row.bounce_count || 0;

      if (row.play_bins_json && row.play_bins_json !== '{}') {
        try {
          const bins = JSON.parse(row.play_bins_json);
          for (const [k, v] of Object.entries(bins)) {
            if (playBins[k] !== undefined) playBins[k] += v;
          }
        } catch (e) {}
      }
      if (row.duration_bins_json && row.duration_bins_json !== '{}') {
        try {
          const bins = JSON.parse(row.duration_bins_json);
          for (const [k, v] of Object.entries(bins)) {
            if (durationBins[k] !== undefined) durationBins[k] += v;
          }
        } catch (e) {}
      }
    }

    // If range covers today, compute session metrics for today's partial plays only
    const endDayStr = formatDatePt(Math.max(start, end - 1));
    if (endDayStr >= todayPt && end > todayStartSec) {
      const todaySessions = db.prepare(`
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
      `).all(todayStartSec, end, 3600);

      totalSessions += todaySessions.length;
      for (const row of todaySessions) {
        const p = row.playCount;
        const d = row.durationSeconds;
        totalSessionPlays += p;
        totalSessionDurationSec += d;

        if (p === 1) {
          bounceCount++;
          playBins['1 play']++;
        } else if (p <= 5) playBins['2–5']++;
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
    }

    if (totalSessions > 0) {
      medianPlaysPerSession = Math.max(1, Math.round(interpolateMedian(PLAY_BUCKETS_DEF, playBins, totalSessions)));
      medianDurationMins = Number(interpolateMedian(DURATION_BUCKETS_DEF, durationBins, totalSessions).toFixed(1));
    }
  }

  const avgPlaysPerSession = totalSessions > 0 ? Number((totalSessionPlays / totalSessions).toFixed(1)) : 0;
  const avgDurationMins = totalSessions > 0 ? Number(((totalSessionDurationSec / totalSessions) / 60.0).toFixed(1)) : 0;
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

  const isFrozen = (end <= todayStartSec) ? 1 : 0;
  const result = {
    range,
    resolution,
    start,
    end,
    cachedAt: Math.floor(Date.now() / 1000),
    ttlSeconds: isFrozen ? 86400 * 365 : 3600,
    summary,
    timeSeries,
    sessionAnalytics,
    topSystems,
    topFormats,
    topTracks,
    topFavorites,
  };

  // Cache in-memory
  statsCache.set(cacheKey, result);

  // Cache in SQLite dashboard_cache.db
  try {
    db.prepare(`
      INSERT OR REPLACE INTO dashboard_db.dashboard_cache (
        cache_key, version, data_json, created_at, is_frozen
      ) VALUES (?, ?, ?, ?, ?)
    `).run(cacheKey, DASHBOARD_VERSION, JSON.stringify(result), Math.floor(Date.now() / 1000), isFrozen);
  } catch (e) {
    console.error('Error saving to dashboard_cache:', e);
  }

  return result;
}

/**
 * Clear the dashboard cache
 */
function clearDashboardCache(targetVersion = DASHBOARD_VERSION) {
  statsCache.clear();
  try {
    db.prepare('DELETE FROM dashboard_db.dashboard_daily_stats WHERE version <= ?').run(targetVersion);
    db.prepare('DELETE FROM dashboard_db.dashboard_cache WHERE version <= ?').run(targetVersion);
  } catch (e) {
    console.error('Error clearing dashboard cache:', e);
  }
}

module.exports = {
  getDashboardStats,
  syncDailyStats,
  clearDashboardCache,
  statsCache,
  DASHBOARD_VERSION,
  TIMEZONE,
  parseTimeRange,
  formatDatePt,
  formatHourPt,
  formatMonthPt,
  getPacificDayBounds,
  getGlobalEarliestSec,
};
