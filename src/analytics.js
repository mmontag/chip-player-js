import ReactGA from 'react-ga4';
import debounce from 'lodash/debounce.js';

const GA_ID = process.env.REACT_APP_GOOGLE_ANALYTICS_ID;

function isBot() {
  if (typeof navigator === 'undefined') return true;
  if (navigator.webdriver) return true;
  const botRegex = /bot|crawler|spider|crawling|slurp|duckduckbot|baiduspider|yandex|bingbot|googlebot/i;
  return botRegex.test(navigator.userAgent || '');
}

export function initGA() {
  if (!GA_ID) {
    return;
  }

  if (isBot()) {
    return;
  }

  ReactGA.initialize(GA_ID, {
    testMode: process.env.NODE_ENV !== 'production',
  });
}

export function logEvent(name, params = {}) {
  if (isBot()) return;
  if (process.env.NODE_ENV !== 'production') {
    console.debug('[GA4]', name, params);
  }
  ReactGA.event(name, params);
}

export function logPlaySong({
  songId,
  songPath,
  title,
  artist,
  game,
  system,
  subtune = 0,
  durationMs,
  playbackSource = 'catalog',
} = {}) {
  const parts = (songPath || '').split('/');
  const derivedSystem = system || (parts.length > 1 ? parts[0] : undefined);
  const fileName = parts.pop() || '';

  const params = {
    song_title: title || fileName || '(unknown)',
    playback_source: playbackSource,
  };

  if (songId != null) params.song_id = String(songId);
  if (songPath) params.song_path = songPath;
  if (artist) params.artist = artist;
  if (game) params.game = game;
  if (derivedSystem) params.system = derivedSystem;
  if (subtune != null) params.subtune = Number(subtune);
  if (durationMs != null && durationMs > 0) {
    params.duration_seconds = Math.round(durationMs / 1000);
  }

  logEvent('play_song', params);
}

let lastLoggedSearch = null;

export const debouncedLogSearch = debounce((query, resultsCount) => {
  const trimmed = (query || '').trim();
  if (!trimmed || trimmed.toLowerCase() === lastLoggedSearch?.toLowerCase()) {
    return;
  }
  lastLoggedSearch = trimmed;

  const params = {
    search_term: trimmed,
  };
  if (resultsCount != null) {
    params.results_count = Number(resultsCount);
  }

  logEvent('search', params);
}, 800);

export function resetSearchHistory() {
  lastLoggedSearch = null;
  debouncedLogSearch.cancel();
}

export function logAddFavorite({
  songId,
  path,
  title,
  artist,
  system,
} = {}) {
  const parts = (path || '').split('/');
  const derivedSystem = system || (parts.length > 1 ? parts[0] : undefined);
  const fileName = parts.pop() || '';

  const params = {
    item_id: songId != null ? String(songId) : path,
    item_name: title || fileName || '(unknown)',
  };

  if (songId != null) params.song_id = String(songId);
  if (path) params.song_path = path;
  if (artist) params.artist = artist;
  if (derivedSystem) params.system = derivedSystem;

  logEvent('add_favorite', params);
}

export default ReactGA;
