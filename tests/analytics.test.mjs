import { test } from 'node:test';
import assert from 'node:assert/strict';
import ReactGA from 'react-ga4';
import {
  logPlaySong,
  debouncedLogSearch,
  resetSearchHistory,
  logAddFavorite,
} from '../src/analytics.js';

test('logPlaySong formats parameters correctly for catalog song', (t) => {
  const events = [];
  const originalEvent = ReactGA.event;
  ReactGA.event = (name, params) => {
    events.push({ name, params });
  };
  t.after(() => {
    ReactGA.event = originalEvent;
  });

  logPlaySong({
    songId: 1234,
    songPath: 'Nintendo NES/Castlevania/01 - Vampire Killer.nsf',
    title: 'Vampire Killer',
    artist: 'Kinuyo Yamashita',
    game: 'Castlevania',
    system: 'Nintendo NES',
    subtune: 1,
    durationMs: 75400,
    playbackSource: 'catalog',
  });

  assert.equal(events.length, 1);
  assert.equal(events[0].name, 'play_song');
  assert.deepEqual(events[0].params, {
    song_id: '1234',
    song_path: 'Nintendo NES/Castlevania/01 - Vampire Killer.nsf',
    song_title: 'Vampire Killer',
    artist: 'Kinuyo Yamashita',
    game: 'Castlevania',
    system: 'Nintendo NES',
    subtune: 1,
    duration_seconds: 75,
    playback_source: 'catalog',
  });
});

test('logPlaySong derives system and title from songPath when metadata is missing', (t) => {
  const events = [];
  const originalEvent = ReactGA.event;
  ReactGA.event = (name, params) => {
    events.push({ name, params });
  };
  t.after(() => {
    ReactGA.event = originalEvent;
  });

  logPlaySong({
    songPath: 'Commodore 64/Hubbard_Rob/Commando.sid',
  });

  assert.equal(events.length, 1);
  assert.equal(events[0].name, 'play_song');
  assert.equal(events[0].params.song_title, 'Commando.sid');
  assert.equal(events[0].params.system, 'Commodore 64');
  assert.equal(events[0].params.playback_source, 'catalog');
  assert.equal(events[0].params.subtune, 0);
});

test('debouncedLogSearch debounces rapid queries and deduplicates consecutive calls', async (t) => {
  const events = [];
  const originalEvent = ReactGA.event;
  ReactGA.event = (name, params) => {
    events.push({ name, params });
  };
  t.after(() => {
    ReactGA.event = originalEvent;
    resetSearchHistory();
  });

  resetSearchHistory();

  // Rapid typing simulation
  debouncedLogSearch('m', 500);
  debouncedLogSearch('me', 200);
  debouncedLogSearch('meg', 80);
  debouncedLogSearch('megaman', 35);

  // Before debounce timer fires: no events
  assert.equal(events.length, 0);

  // Wait for 850ms to exceed debounce
  await new Promise(resolve => setTimeout(resolve, 850));

  assert.equal(events.length, 1);
  assert.equal(events[0].name, 'search');
  assert.deepEqual(events[0].params, {
    search_term: 'megaman',
    results_count: 35,
  });

  // Calling with exact same query should not duplicate
  debouncedLogSearch('megaman', 35);
  await new Promise(resolve => setTimeout(resolve, 850));
  assert.equal(events.length, 1, 'Duplicate consecutive search was suppressed');

  // Empty or whitespace query should be ignored
  debouncedLogSearch('   ', 0);
  await new Promise(resolve => setTimeout(resolve, 850));
  assert.equal(events.length, 1, 'Empty search was ignored');
});

test('resetSearchHistory cancels pending debounced search', async (t) => {
  const events = [];
  const originalEvent = ReactGA.event;
  ReactGA.event = (name, params) => {
    events.push({ name, params });
  };
  t.after(() => {
    ReactGA.event = originalEvent;
    resetSearchHistory();
  });

  resetSearchHistory();

  debouncedLogSearch('cancelled-query', 10);
  resetSearchHistory();

  await new Promise(resolve => setTimeout(resolve, 850));
  assert.equal(events.length, 0, 'Pending search was cancelled');
});

test('logAddFavorite formats parameters correctly', (t) => {
  const events = [];
  const originalEvent = ReactGA.event;
  ReactGA.event = (name, params) => {
    events.push({ name, params });
  };
  t.after(() => {
    ReactGA.event = originalEvent;
  });

  logAddFavorite({
    songId: 987,
    path: 'Sega Genesis/Sonic the Hedgehog/01 - Green Hill Zone.vgm',
    title: 'Green Hill Zone',
    artist: 'Masato Nakamura',
  });

  assert.equal(events.length, 1);
  assert.equal(events[0].name, 'add_favorite');
  assert.deepEqual(events[0].params, {
    item_id: '987',
    item_name: 'Green Hill Zone',
    song_id: '987',
    song_path: 'Sega Genesis/Sonic the Hedgehog/01 - Green Hill Zone.vgm',
    artist: 'Masato Nakamura',
    system: 'Sega Genesis',
  });
});
