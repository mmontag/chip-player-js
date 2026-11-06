/**
 * Chiptune.app Analytics Dashboard Frontend Controller
 * Clean Tailwind / Bootstrap UI style
 */
(function() {
  'use strict';

  // Global State
  let currentRange = '30d';
  let customStart = null;
  let customEnd = null;
  let currentData = null;
  let autoRefreshTimer = null;
  let autoRefreshEnabled = false;

  // Chart instances
  const charts = {
    playbacks: null,
    users: null,
    favorites: null,
    systems: null,
    formats: null,
  };

  // Clean Tailwind Palette
  const COLORS = {
    primary: '#2563eb', // blue-600
    primaryLight: 'rgba(37, 99, 235, 0.08)',
    success: '#059669', // emerald-600
    indigo: '#4f46e5',  // indigo-600
    amber: '#d97706',   // amber-600
    rose: '#e11d48',    // rose-600
    slate: '#64748b',   // slate-500
    grid: '#f1f5f9',    // slate-100
    text: '#64748b',
    border: '#e2e8f0',
  };

  // Utility: number formatters
  const fmtNumber = (n) => (n == null ? '0' : Number(n).toLocaleString());
  const fmtCompact = (n) => {
    if (n == null) return '0';
    if (n >= 1e6) return (n / 1e6).toFixed(2) + 'M';
    if (n >= 1e3) return (n / 1e3).toFixed(1) + 'k';
    return Number(n).toLocaleString();
  };

  // Default Chart.js Config Helpers (Clean Tailwind style)
  function getCommonChartOptions(extra = {}) {
    return {
      responsive: true,
      maintainAspectRatio: false,
      animation: { duration: 300 },
      interaction: {
        mode: 'index',
        intersect: false,
      },
      plugins: {
        legend: {
          display: true,
          position: 'top',
          align: 'end',
          labels: {
            boxWidth: 8,
            boxHeight: 8,
            usePointStyle: true,
            color: COLORS.text,
            font: { family: '-apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, sans-serif', size: 11, weight: '500' },
            padding: 10,
          },
        },
        tooltip: {
          backgroundColor: '#ffffff',
          borderColor: '#e2e8f0',
          borderWidth: 1,
          padding: 10,
          titleFont: { family: '-apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, sans-serif', size: 12, weight: '600' },
          bodyFont: { family: 'ui-monospace, SFMono-Regular, Menlo, monospace', size: 11 },
          titleColor: '#0f172a',
          bodyColor: '#334155',
          boxPadding: 4,
          usePointStyle: true,
          displayColors: true,
          shadowOffsetX: 0,
          shadowOffsetY: 4,
          shadowBlur: 6,
          shadowColor: 'rgba(0, 0, 0, 0.08)',
        },
      },
      scales: {
        x: {
          grid: { color: COLORS.grid, drawBorder: false },
          ticks: {
            color: COLORS.text,
            font: { family: '-apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, sans-serif', size: 11 },
            maxRotation: 0,
            autoSkip: true,
            maxTicksLimit: 12,
          },
        },
        y: {
          grid: { color: COLORS.grid, drawBorder: false },
          ticks: {
            color: COLORS.text,
            font: { family: 'ui-monospace, SFMono-Regular, Menlo, monospace', size: 11 },
            callback: (val) => fmtCompact(val),
          },
        },
      },
      ...extra,
    };
  }

  // Fetch Stats from Express API
  async function fetchStats() {
    const spinner = document.getElementById('loadingIndicator');
    if (spinner) spinner.style.display = 'inline-flex';

    try {
      let url = '/api/dashboard/stats';
      const params = new URLSearchParams();

      if (currentRange === 'custom' && customStart && customEnd) {
        params.set('start', customStart);
        params.set('end', customEnd);
      } else {
        params.set('range', currentRange);
      }

      url += '?' + params.toString();
      const res = await fetch(url);
      if (!res.ok) throw new Error('API returned ' + res.status);

      const data = await res.json();
      currentData = data;
      renderDashboard(data);
    } catch (err) {
      console.error('Error loading dashboard stats:', err);
    } finally {
      if (spinner) spinner.style.display = 'none';
    }
  }

  // Render Dashboard
  function renderDashboard(data) {
    updateKPIs(data.summary, data.range);
    renderPlaybacksChart(data.timeSeries);
    renderUsersChart(data.timeSeries);
    renderFavoritesChart(data.timeSeries);
    renderSystemsChart(data.topSystems);
    renderFormatsChart(data.topFormats);
    renderTopTracksTable(data.topTracks);
    renderTopFavoritesTable(data.topFavorites);

    const cachedTimeEl = document.getElementById('cachedTimestamp');
    if (cachedTimeEl && data.cachedAt) {
      cachedTimeEl.textContent = new Date(data.cachedAt * 1000).toLocaleTimeString();
    }
  }

  // Update KPI Cards
  function updateKPIs(sum, range) {
    if (!sum) return;

    document.getElementById('kpiTotalPlays').textContent = fmtNumber(sum.totalPlays);
    renderDelta('kpiPlaysDelta', sum.playsDeltaPercent);
    document.getElementById('kpiPlaysSub').textContent = `${sum.loggedInPlaysPercent}% registered users`;

    document.getElementById('kpiUniqueListeners').textContent = fmtNumber(sum.uniqueListeners);
    renderDelta('kpiListenersDelta', sum.listenersDeltaPercent);
    document.getElementById('kpiListenersSub').textContent = `Avg ${fmtNumber(sum.avgDailyPlays)} plays / day`;

    document.getElementById('kpiListenHours').textContent = sum.totalListenHours + ' hrs';
    document.getElementById('kpiListenHoursSub').textContent = 'Based on ≥5s listen logs';

    document.getElementById('kpiNewUsers').textContent = '+' + fmtNumber(sum.newUsers);
    renderDelta('kpiNewUsersDelta', sum.newUsersDeltaPercent);
    document.getElementById('kpiTotalAccounts').textContent = `${fmtNumber(sum.totalAccounts)} total accounts`;

    document.getElementById('kpiFavoritesAdded').textContent = '+' + fmtNumber(sum.favoritesAdded);
    renderDelta('kpiFavoritesDelta', sum.favoritesDeltaPercent);
    document.getElementById('kpiTotalFavorites').textContent = `${fmtNumber(sum.totalFavorites)} total catalog favs`;
  }

  function renderDelta(elemId, val) {
    const el = document.getElementById(elemId);
    if (!el) return;

    if (val > 0) {
      el.className = 'kpi-badge pos';
      el.textContent = `+${val}%`;
    } else if (val < 0) {
      el.className = 'kpi-badge neg';
      el.textContent = `${val}%`;
    } else {
      el.className = 'kpi-badge neutral';
      el.textContent = `0%`;
    }
  }

  // Chart 1: Playbacks Over Time
  function renderPlaybacksChart(series) {
    const ctx = document.getElementById('chartPlaybacks').getContext('2d');
    const labels = series.map(s => s.bucket);

    const totalPlays = series.map(s => s.totalPlays);
    const loggedInPlays = series.map(s => s.loggedInPlays);
    const anonPlays = series.map(s => s.anonPlays);

    if (charts.playbacks) {
      charts.playbacks.data.labels = labels;
      charts.playbacks.data.datasets[0].data = totalPlays;
      charts.playbacks.data.datasets[1].data = loggedInPlays;
      charts.playbacks.data.datasets[2].data = anonPlays;
      charts.playbacks.update();
      return;
    }

    charts.playbacks = new Chart(ctx, {
      type: 'line',
      data: {
        labels,
        datasets: [
          {
            label: 'Total Plays',
            data: totalPlays,
            borderColor: COLORS.primary,
            backgroundColor: COLORS.primaryLight,
            borderWidth: 2,
            pointRadius: series.length > 60 ? 0 : 2,
            pointHoverRadius: 4,
            fill: true,
            tension: 0.15,
          },
          {
            label: 'Registered Users',
            data: loggedInPlays,
            borderColor: COLORS.success,
            backgroundColor: 'transparent',
            borderWidth: 1.5,
            pointRadius: 0,
            pointHoverRadius: 3,
            tension: 0.15,
          },
          {
            label: 'Anonymous Listeners',
            data: anonPlays,
            borderColor: COLORS.slate,
            backgroundColor: 'transparent',
            borderWidth: 1.5,
            borderDash: [3, 3],
            pointRadius: 0,
            pointHoverRadius: 3,
            tension: 0.15,
          },
        ],
      },
      options: getCommonChartOptions(),
    });
  }

  // Chart 2: Active Users & New Registrations
  function renderUsersChart(series) {
    const ctx = document.getElementById('chartUsers').getContext('2d');
    const labels = series.map(s => s.bucket);

    const uniqueListeners = series.map(s => s.uniqueListeners);
    const activeUsers = series.map(s => s.activeUsers);
    const newUsers = series.map(s => s.newUsers);
    const cumulativeUsers = series.map(s => s.cumulativeUsers);

    if (charts.users) {
      charts.users.data.labels = labels;
      charts.users.data.datasets[0].data = uniqueListeners;
      charts.users.data.datasets[1].data = activeUsers;
      charts.users.data.datasets[2].data = newUsers;
      charts.users.data.datasets[3].data = cumulativeUsers;
      charts.users.update();
      return;
    }

    const options = getCommonChartOptions();
    options.scales.y1 = {
      position: 'right',
      grid: { drawOnChartArea: false },
      ticks: {
        color: COLORS.indigo,
        font: { family: 'ui-monospace, SFMono-Regular, Menlo, monospace', size: 10 },
        callback: (val) => fmtCompact(val),
      },
    };

    charts.users = new Chart(ctx, {
      type: 'bar',
      data: {
        labels,
        datasets: [
          {
            label: 'Active Listeners',
            data: uniqueListeners,
            backgroundColor: '#3b82f6',
            borderRadius: 3,
            order: 2,
          },
          {
            label: 'Active Registered',
            data: activeUsers,
            backgroundColor: '#10b981',
            borderRadius: 3,
            order: 2,
          },
          {
            label: 'New Registrations',
            data: newUsers,
            backgroundColor: '#f59e0b',
            borderRadius: 3,
            order: 2,
          },
          {
            label: 'Cumulative Accounts',
            data: cumulativeUsers,
            type: 'line',
            yAxisID: 'y1',
            borderColor: COLORS.indigo,
            borderWidth: 2,
            pointRadius: 0,
            pointHoverRadius: 3,
            order: 1,
          },
        ],
      },
      options,
    });
  }

  // Chart 3: Favorites Added Over Time
  function renderFavoritesChart(series) {
    const ctx = document.getElementById('chartFavorites').getContext('2d');
    const labels = series.map(s => s.bucket);

    const favsAdded = series.map(s => s.favoritesAdded);
    const cumulativeFavs = series.map(s => s.cumulativeFavorites);

    if (charts.favorites) {
      charts.favorites.data.labels = labels;
      charts.favorites.data.datasets[0].data = favsAdded;
      charts.favorites.data.datasets[1].data = cumulativeFavs;
      charts.favorites.update();
      return;
    }

    const options = getCommonChartOptions();
    options.scales.y1 = {
      position: 'right',
      grid: { drawOnChartArea: false },
      ticks: {
        color: COLORS.amber,
        font: { family: 'ui-monospace, SFMono-Regular, Menlo, monospace', size: 10 },
        callback: (val) => fmtCompact(val),
      },
    };

    charts.favorites = new Chart(ctx, {
      type: 'bar',
      data: {
        labels,
        datasets: [
          {
            label: 'Favorites Added',
            data: favsAdded,
            backgroundColor: '#f43f5e',
            borderRadius: 3,
            order: 2,
          },
          {
            label: 'Cumulative Favorites',
            data: cumulativeFavs,
            type: 'line',
            yAxisID: 'y1',
            borderColor: COLORS.amber,
            borderWidth: 2,
            pointRadius: 0,
            pointHoverRadius: 3,
            order: 1,
          },
        ],
      },
      options,
    });
  }

  // Chart 4: Top Systems
  function renderSystemsChart(systems) {
    const ctx = document.getElementById('chartSystems').getContext('2d');
    const labels = systems.map(s => s.system);
    const data = systems.map(s => s.plays);

    if (charts.systems) {
      charts.systems.data.labels = labels;
      charts.systems.data.datasets[0].data = data;
      charts.systems.update();
      return;
    }

    charts.systems = new Chart(ctx, {
      type: 'bar',
      data: {
        labels,
        datasets: [
          {
            label: 'Plays',
            data,
            backgroundColor: '#3b82f6',
            borderRadius: 4,
          },
        ],
      },
      options: {
        indexAxis: 'y',
        responsive: true,
        maintainAspectRatio: false,
        plugins: {
          legend: { display: false },
          tooltip: {
            backgroundColor: '#ffffff',
            borderColor: '#e2e8f0',
            borderWidth: 1,
            titleColor: '#0f172a',
            bodyColor: '#334155',
            callbacks: {
              label: (ctx) => `${fmtNumber(ctx.raw)} plays (${systems[ctx.dataIndex].sharePercent}%)`,
            },
          },
        },
        scales: {
          x: {
            grid: { color: COLORS.grid },
            ticks: {
              color: COLORS.text,
              font: { family: 'ui-monospace, SFMono-Regular, Menlo, monospace', size: 10 },
              callback: (v) => fmtCompact(v),
            },
          },
          y: {
            grid: { display: false },
            ticks: {
              color: '#0f172a',
              font: { family: '-apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, sans-serif', size: 11, weight: '500' },
            },
          },
        },
      },
    });
  }

  // Chart 5: Top Audio Formats
  function renderFormatsChart(formats) {
    const ctx = document.getElementById('chartFormats').getContext('2d');
    const labels = formats.map(f => f.extension);
    const data = formats.map(f => f.plays);

    if (charts.formats) {
      charts.formats.data.labels = labels;
      charts.formats.data.datasets[0].data = data;
      charts.formats.update();
      return;
    }

    charts.formats = new Chart(ctx, {
      type: 'bar',
      data: {
        labels,
        datasets: [
          {
            label: 'Plays',
            data,
            backgroundColor: '#f59e0b',
            borderRadius: 4,
          },
        ],
      },
      options: {
        indexAxis: 'y',
        responsive: true,
        maintainAspectRatio: false,
        plugins: {
          legend: { display: false },
          tooltip: {
            backgroundColor: '#ffffff',
            borderColor: '#e2e8f0',
            borderWidth: 1,
            titleColor: '#0f172a',
            bodyColor: '#334155',
            callbacks: {
              label: (ctx) => `${fmtNumber(ctx.raw)} plays (${formats[ctx.dataIndex].sharePercent}%)`,
            },
          },
        },
        scales: {
          x: {
            grid: { color: COLORS.grid },
            ticks: {
              color: COLORS.text,
              font: { family: 'ui-monospace, SFMono-Regular, Menlo, monospace', size: 10 },
              callback: (v) => fmtCompact(v),
            },
          },
          y: {
            grid: { display: false },
            ticks: {
              color: '#0f172a',
              font: { family: 'ui-monospace, SFMono-Regular, Menlo, monospace', size: 11, weight: '600' },
            },
          },
        },
      },
    });
  }

  // Table 1: Top Played Tracks
  function renderTopTracksTable(tracks, filterQuery = '') {
    const tbody = document.getElementById('topTracksBody');
    if (!tbody) return;

    const q = filterQuery.toLowerCase().trim();
    const filtered = q
      ? tracks.filter(t => (t.title && t.title.toLowerCase().includes(q)) || (t.artist && t.artist.toLowerCase().includes(q)) || (t.system && t.system.toLowerCase().includes(q)))
      : tracks;

    if (filtered.length === 0) {
      tbody.innerHTML = '<tr><td colspan="6" style="text-align:center;color:var(--text-muted);padding:24px;">No tracks found matching filter</td></tr>';
      return;
    }

    tbody.innerHTML = filtered.map(t => `
      <tr>
        <td class="col-rank">#${t.rank}</td>
        <td>
          <div class="track-box">
            <span class="track-name" title="${escapeHtml(t.title)}">${escapeHtml(t.title)}</span>
            <span class="track-meta">${escapeHtml(t.artist)}</span>
          </div>
        </td>
        <td><span class="tag tag-system">${escapeHtml(t.system)}</span></td>
        <td><span class="tag tag-format">${escapeHtml(t.extension)}</span></td>
        <td class="col-num">${fmtNumber(t.plays)}</td>
        <td style="text-align:right;">
          <a href="${t.url}" target="_blank" rel="noopener noreferrer" class="btn-play-link">▶ Play</a>
        </td>
      </tr>
    `).join('');
  }

  // Table 2: Top Favorited Tracks
  function renderTopFavoritesTable(favorites, filterQuery = '') {
    const tbody = document.getElementById('topFavoritesBody');
    if (!tbody) return;

    const q = filterQuery.toLowerCase().trim();
    const filtered = q
      ? favorites.filter(t => (t.title && t.title.toLowerCase().includes(q)) || (t.artist && t.artist.toLowerCase().includes(q)) || (t.system && t.system.toLowerCase().includes(q)))
      : favorites;

    if (filtered.length === 0) {
      tbody.innerHTML = '<tr><td colspan="6" style="text-align:center;color:var(--text-muted);padding:24px;">No favorites found matching filter</td></tr>';
      return;
    }

    tbody.innerHTML = filtered.map(t => `
      <tr>
        <td class="col-rank">#${t.rank}</td>
        <td>
          <div class="track-box">
            <span class="track-name" title="${escapeHtml(t.title)}">${escapeHtml(t.title)}</span>
            <span class="track-meta">${escapeHtml(t.artist)}</span>
          </div>
        </td>
        <td><span class="tag tag-system">${escapeHtml(t.system)}</span></td>
        <td><span class="tag tag-format">${escapeHtml(t.extension)}</span></td>
        <td class="col-num" style="color:#e11d48;">♥ ${fmtNumber(t.favoritesCount)}</td>
        <td style="text-align:right;">
          <a href="${t.url}" target="_blank" rel="noopener noreferrer" class="btn-play-link">▶ Play</a>
        </td>
      </tr>
    `).join('');
  }

  function escapeHtml(str) {
    if (!str) return '';
    return String(str)
      .replace(/&/g, '&amp;')
      .replace(/</g, '&lt;')
      .replace(/>/g, '&gt;')
      .replace(/"/g, '&quot;')
      .replace(/'/g, '&#39;');
  }

  // Sync UI and variables from URL search params
  function applyRangeFromUrl() {
    const urlParams = new URLSearchParams(window.location.search);
    const rangeParam = urlParams.get('range');
    const startParam = urlParams.get('start');
    const endParam = urlParams.get('end');

    const rangeButtons = document.querySelectorAll('.btn-range');

    if (rangeParam === 'custom' && startParam && endParam) {
      currentRange = 'custom';
      customStart = startParam;
      customEnd = endParam;
      const startInput = document.getElementById('startDate');
      const endInput = document.getElementById('endDate');
      if (startInput) startInput.value = startParam;
      if (endInput) endInput.value = endParam;
      rangeButtons.forEach(b => b.classList.remove('active'));
    } else {
      currentRange = rangeParam || '30d';
      customStart = null;
      customEnd = null;
      rangeButtons.forEach(b => {
        if (b.getAttribute('data-range') === currentRange) {
          b.classList.add('active');
        } else {
          b.classList.remove('active');
        }
      });
    }
  }

  // Event Listeners Setup
  function initEvents() {
    // Range Buttons
    const rangeButtons = document.querySelectorAll('.btn-range');
    rangeButtons.forEach(btn => {
      btn.addEventListener('click', () => {
        const newRange = btn.getAttribute('data-range');
        if (currentRange === newRange && !customStart && !customEnd) {
          return; // No-op if clicking already active range
        }

        rangeButtons.forEach(b => b.classList.remove('active'));
        btn.classList.add('active');
        currentRange = newRange;
        customStart = null;
        customEnd = null;

        // Push range change to browser history stack
        const url = new URL(window.location);
        url.searchParams.set('range', currentRange);
        url.searchParams.delete('start');
        url.searchParams.delete('end');
        window.history.pushState({ range: currentRange, start: null, end: null }, '', url);

        fetchStats();
      });
    });

    // Custom Date Form
    const customForm = document.getElementById('customRangeForm');
    if (customForm) {
      customForm.addEventListener('submit', (e) => {
        e.preventDefault();
        const startVal = document.getElementById('startDate').value;
        const endVal = document.getElementById('endDate').value;
        if (!startVal || !endVal) return;

        if (currentRange === 'custom' && customStart === startVal && customEnd === endVal) {
          return; // No-op if same dates
        }

        rangeButtons.forEach(b => b.classList.remove('active'));
        currentRange = 'custom';
        customStart = startVal;
        customEnd = endVal;

        // Push custom range to browser history stack
        const url = new URL(window.location);
        url.searchParams.set('range', 'custom');
        url.searchParams.set('start', customStart);
        url.searchParams.set('end', customEnd);
        window.history.pushState({ range: 'custom', start: customStart, end: customEnd }, '', url);

        fetchStats();
      });
    }

    // Handle browser Back / Forward navigation
    window.addEventListener('popstate', () => {
      applyRangeFromUrl();
      fetchStats();
    });

    // Refresh Button
    const refreshBtn = document.getElementById('btnRefresh');
    if (refreshBtn) {
      refreshBtn.addEventListener('click', () => fetchStats());
    }

    // Auto-Refresh Button
    const autoRefreshBtn = document.getElementById('btnAutoRefresh');
    if (autoRefreshBtn) {
      autoRefreshBtn.addEventListener('click', () => {
        autoRefreshEnabled = !autoRefreshEnabled;
        if (autoRefreshEnabled) {
          autoRefreshBtn.classList.add('active');
          autoRefreshBtn.innerHTML = '↻ Auto: 60s';
          autoRefreshTimer = setInterval(fetchStats, 60000);
        } else {
          autoRefreshBtn.classList.remove('active');
          autoRefreshBtn.innerHTML = '↻ Auto: Off';
          clearInterval(autoRefreshTimer);
          autoRefreshTimer = null;
        }
      });
    }

    // Search filters for tables
    const tracksFilter = document.getElementById('filterTracks');
    if (tracksFilter) {
      tracksFilter.addEventListener('input', (e) => {
        if (currentData && currentData.topTracks) {
          renderTopTracksTable(currentData.topTracks, e.target.value);
        }
      });
    }

    const favoritesFilter = document.getElementById('filterFavorites');
    if (favoritesFilter) {
      favoritesFilter.addEventListener('input', (e) => {
        if (currentData && currentData.topFavorites) {
          renderTopFavoritesTable(currentData.topFavorites, e.target.value);
        }
      });
    }
  }

  // Initialization
  document.addEventListener('DOMContentLoaded', () => {
    applyRangeFromUrl();
    // Replace initial state in history with current params for clean back navigation
    const initialUrl = new URL(window.location);
    window.history.replaceState({ range: currentRange, start: customStart, end: customEnd }, '', initialUrl);
    initEvents();
    fetchStats();
  });
})();
