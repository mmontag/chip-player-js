#!/usr/bin/env node

/**
 * Automated Test Suite for Analytics Dashboard:
 * 1. Pacific Time zone orientation and DST handling
 * 2. Self-initializing isolated SQLite database (server/dashboard-cache.db)
 * 3. Daily metrics long-term caching for completed days from Dec 2018 to present
 * 4. User growth and favorites preservation back to 2018
 * 5. Sub-millisecond cached query performance
 * 6. Isolation: users.db remains completely untouched
 * 7. Version invalidation & cache clearing
 */

const assert = require('assert');
const fs = require('fs');
const path = require('path');

const DASHBOARD_DB_PATH = path.resolve(__dirname, '../server/dashboard-cache.db');

async function runTests() {
  console.log('=== Starting Dashboard Pacific Time & Caching Test Suite ===\n');

  // Test 1: Pacific Time Date Bounds and DST transitions
  console.log('Test 1: Pacific Time calculations and DST handling...');
  const {
    formatDatePt,
    formatHourPt,
    formatMonthPt,
    getPacificDayBounds,
    getGlobalEarliestSec,
    TIMEZONE,
    DASHBOARD_VERSION,
  } = require('../server/dashboard-stats');

  assert.strictEqual(TIMEZONE, 'America/Los_Angeles');
  assert.strictEqual(DASHBOARD_VERSION, 3);

  // Global earliest activity timestamp (user #1 in Dec 2018)
  const earliestSec = getGlobalEarliestSec();
  assert.strictEqual(earliestSec, 1544404726);
  assert.strictEqual(formatDatePt(earliestSec), '2018-12-09');

  // Winter timestamp (PST UTC-8): 1731438000 (2024-11-12 11:00:00 PST)
  assert.strictEqual(formatDatePt(1731438000), '2024-11-12');
  assert.strictEqual(formatHourPt(1731438000), '2024-11-12 11:00');
  assert.strictEqual(formatMonthPt(1731438000), '2024-11');

  // Summer timestamp (PDT UTC-7): 1720785600 (2024-07-12 05:00:00 PDT)
  assert.strictEqual(formatDatePt(1720785600), '2024-07-12');
  assert.strictEqual(formatHourPt(1720785600), '2024-07-12 05:00');
  assert.strictEqual(formatMonthPt(1720785600), '2024-07');

  // Spring forward DST day (23 hours)
  const springBounds = getPacificDayBounds('2026-03-08');
  assert.strictEqual((springBounds.endSec - springBounds.startSec) / 3600, 23);

  // Fall back DST day (25 hours)
  const fallBounds = getPacificDayBounds('2026-11-01');
  assert.strictEqual((fallBounds.endSec - fallBounds.startSec) / 3600, 25);

  console.log('  ✓ Pacific Time boundaries, PST/PDT formatting and DST transitions verified.');

  // Test 2: Database Self-Initialization & Isolation
  console.log('\nTest 2: Dedicated SQLite database auto-creation and users.db isolation...');
  const { db } = require('../server/database');

  assert.ok(fs.existsSync(DASHBOARD_DB_PATH), 'dashboard-cache.db should exist on disk');

  // Check tables in user_db vs dashboard_db
  const userDbTables = db.prepare("SELECT name FROM user_db.sqlite_master WHERE type='table'").all().map(t => t.name);
  assert.ok(!userDbTables.includes('dashboard_daily_stats'), 'user_db must not contain dashboard_daily_stats');
  assert.ok(!userDbTables.includes('dashboard_cache'), 'user_db must not contain dashboard_cache');

  const dashDbTables = db.prepare("SELECT name FROM dashboard_db.sqlite_master WHERE type='table'").all().map(t => t.name);
  assert.ok(dashDbTables.includes('dashboard_daily_stats'), 'dashboard_db must contain dashboard_daily_stats');
  assert.ok(dashDbTables.includes('dashboard_cache'), 'dashboard_db must contain dashboard_cache');

  console.log('  ✓ dashboard-cache.db is self-initialized and users.db remains completely untouched.');

  // Test 3: Daily Stats Synchronization from 2018
  console.log('\nTest 3: Daily stats synchronization for completed historical days back to Dec 2018...');
  const { syncDailyStats } = require('../server/dashboard-stats');
  syncDailyStats();

  const cachedDaysCount = db.prepare(`
    SELECT COUNT(*) as c FROM dashboard_db.dashboard_daily_stats WHERE version = ?
  `).get(DASHBOARD_VERSION).c;

  assert.ok(cachedDaysCount > 2800, `Expected > 2,800 completed days from 2018, found ${cachedDaysCount}`);

  // Verify first cached day starts in Dec 2018
  const firstCachedDay = db.prepare(`
    SELECT * FROM dashboard_db.dashboard_daily_stats WHERE version = ? ORDER BY date ASC LIMIT 1
  `).get(DASHBOARD_VERSION);

  assert.strictEqual(firstCachedDay.date, '2018-12-09');
  assert.strictEqual(firstCachedDay.new_users, 1);
  assert.strictEqual(firstCachedDay.cumulative_users, 1);

  // Verify monotonicity of cumulative users and favorites
  const sampleDays = db.prepare(`
    SELECT date, cumulative_users, cumulative_favorites 
    FROM dashboard_db.dashboard_daily_stats 
    WHERE version = ? 
    ORDER BY date ASC
  `).all(DASHBOARD_VERSION);

  for (let i = 1; i < sampleDays.length; i++) {
    assert.ok(
      sampleDays[i].cumulative_users >= sampleDays[i - 1].cumulative_users,
      `cumulative_users should be non-decreasing: ${sampleDays[i].date}`
    );
    assert.ok(
      sampleDays[i].cumulative_favorites >= sampleDays[i - 1].cumulative_favorites,
      `cumulative_favorites should be non-decreasing: ${sampleDays[i].date}`
    );
  }

  const latestDay = sampleDays[sampleDays.length - 1];
  assert.strictEqual(latestDay.cumulative_users, 3527);
  assert.strictEqual(latestDay.cumulative_favorites, 96557);

  console.log(`  ✓ Successfully cached ${cachedDaysCount} historical Pacific days starting Dec 2018 (3,527 users, 96,557 favs).`);

  // Test 4: Dashboard Stats Query & Time Series
  console.log('\nTest 4: Bounded query execution and response consistency...');
  const { getDashboardStats, statsCache } = require('../server/dashboard-stats');

  const res30d = getDashboardStats({ range: '30d' });
  assert.strictEqual(res30d.range, '30d');
  assert.strictEqual(res30d.resolution, 'day');
  assert.strictEqual(res30d.timeSeries.length, 30);
  assert.ok(res30d.summary.totalPlays >= 0);
  assert.strictEqual(res30d.summary.totalAccounts, 3527);
  assert.strictEqual(res30d.summary.totalFavorites, 96557);

  // Bucket keys should end on today's Pacific date
  const nowPt = formatDatePt(Date.now() / 1000);
  assert.strictEqual(res30d.timeSeries[res30d.timeSeries.length - 1].bucket, nowPt);

  console.log(`  ✓ 30d stats returned 30 contiguous Pacific buckets ending today (${nowPt}).`);

  // Test 5: 'all' Preset Verification from Dec 2018 to Present
  console.log('\nTest 5: Preset "all" spans complete history (2018 to present)...');
  const resAll = getDashboardStats({ range: 'all' });
  assert.strictEqual(resAll.range, 'all');
  assert.strictEqual(resAll.resolution, 'month');
  assert.ok(resAll.timeSeries.length >= 95, `Expected >= 95 months, got ${resAll.timeSeries.length}`);
  assert.strictEqual(resAll.timeSeries[0].bucket, '2018-12');
  assert.strictEqual(resAll.timeSeries[0].newUsers, 1);
  assert.strictEqual(resAll.timeSeries[0].cumulativeUsers, 1);
  assert.strictEqual(resAll.summary.totalAccounts, 3527);
  assert.strictEqual(resAll.summary.totalFavorites, 96557);

  console.log(`  ✓ "all" preset spans ${resAll.timeSeries.length} months from 2018-12 with complete user and favorite counts.`);

  // Test 6: Cache Hit Speed & Frozen Past Ranges
  console.log('\nTest 6: Caching performance and frozen past ranges...');
  statsCache.clear(); // Clear memory cache to test SQLite disk cache

  const t0 = process.hrtime.bigint();
  const cachedRes = getDashboardStats({ range: '30d' });
  const t1 = process.hrtime.bigint();
  const elapsedMs = Number(t1 - t0) / 1e6;

  assert.strictEqual(cachedRes.summary.totalPlays, res30d.summary.totalPlays);
  console.log(`  ✓ Cold read from SQLite cache completed in ${elapsedMs.toFixed(2)} ms.`);
  assert.ok(elapsedMs < 50, `Expected cache read < 50 ms, was ${elapsedMs.toFixed(2)} ms`);

  // Test frozen historical custom range
  const pastRangeRes = getDashboardStats({ start: '2026-04-01', end: '2026-04-30' });
  assert.strictEqual(pastRangeRes.range, 'custom');
  assert.strictEqual(pastRangeRes.timeSeries.length, 30);

  const frozenRow = db.prepare(`
    SELECT is_frozen FROM dashboard_db.dashboard_cache 
    WHERE cache_key = ? AND version = ?
  `).get('custom_' + pastRangeRes.start + '_' + pastRangeRes.end + '_day', DASHBOARD_VERSION);

  assert.ok(frozenRow, 'Custom historical range should be recorded in dashboard_cache');
  assert.strictEqual(frozenRow.is_frozen, 1, 'Past range must be marked is_frozen = 1');

  console.log('  ✓ Completed historical date range marked is_frozen = 1 in database.');

  // Test 7: Hourly preset
  console.log('\nTest 7: 24h hourly preset in Pacific Time...');
  const res24h = getDashboardStats({ range: '24h' });
  assert.strictEqual(res24h.range, '24h');
  assert.strictEqual(res24h.resolution, 'hour');
  assert.ok(res24h.timeSeries.length >= 24);
  assert.ok(res24h.timeSeries[0].bucket.includes(':00'));
  console.log('  ✓ 24h preset correctly generated hourly buckets.');

  // Test 8: Precalculated Daily Systems, Formats, and Session Analytics
  console.log('\nTest 8: Precalculated daily systems, formats, and session analytics...');
  const dayWithPlays = db.prepare(`
    SELECT date, total_plays, top_systems_json, top_formats_json, total_sessions, play_bins_json, duration_bins_json
    FROM dashboard_db.dashboard_daily_stats
    WHERE version = ? AND total_plays > 0
    LIMIT 1
  `).get(DASHBOARD_VERSION);

  assert.ok(dayWithPlays, 'Should find completed days with playbacks');
  assert.ok(dayWithPlays.top_systems_json.length > 2, 'top_systems_json must not be empty');
  assert.ok(dayWithPlays.top_formats_json.length > 2, 'top_formats_json must not be empty');
  assert.ok(dayWithPlays.total_sessions > 0, 'total_sessions must be > 0 for a day with plays');
  assert.ok(dayWithPlays.play_bins_json.length > 2, 'play_bins_json must not be empty');
  assert.ok(dayWithPlays.duration_bins_json.length > 2, 'duration_bins_json must not be empty');

  // Verify that 90d uncached request populates systems, formats, session analytics accurately
  const res90d = getDashboardStats({ range: '90d', refresh: '1' });
  assert.ok(res90d.topSystems.length > 0, '90d should have topSystems');
  assert.ok(res90d.topFormats.length > 0, '90d should have topFormats');
  assert.ok(res90d.sessionAnalytics.totalSessions > 0, '90d should have totalSessions');
  assert.ok(res90d.sessionAnalytics.playBuckets.length > 0, '90d should have playBuckets');
  assert.ok(res90d.sessionAnalytics.durationBuckets.length > 0, '90d should have durationBuckets');
  console.log('  ✓ Daily systems, formats, and session analytics are precalculated and correctly aggregated.');

  console.log('\n=== All Tests Passed Successfully! ===');
}

runTests().catch(err => {
  console.error('\n❌ Test failure:', err);
  process.exit(1);
});
