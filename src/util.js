import queryString from 'querystring';
import React from 'react';
import { toArabic } from 'roman-numerals';

import DirectoryLink from './DirectoryLink';

const ROMAN_NUMERAL_REGEX = /\b([IVXLC]+|[ivxlc]+)[-.,)]/; // All upper case or all lower case
const CATALOG_PREFIX_REGEX = /^https?:\/\/[a-z0-9\-.:]+\/(music|catalog)\//;

export function updateQueryString(newParams) {
  // Merge new params with current query string
  const params = {
    ...queryString.parse(window.location.search.substr(1)),
    ...newParams,
  };
  // Delete undefined properties
  Object.keys(params).forEach(key => params[key] === undefined && delete params[key]);
  // Object.keys(params).forEach(key => params[key] = decodeURIComponent(params[key]));
  const stateUrl = '?' + queryString.stringify(params).replace(/%20/g, '+');
  // Update address bar URL
  window.history.replaceState(null, '', stateUrl);
}

export function replaceRomanWithArabic(str) {
  // Works up to 399 (CCCXCIX)
  try {
    return str.replace(ROMAN_NUMERAL_REGEX, (_, match) => String(toArabic(match)).padStart(4, '0'));
  } catch (e) {
    // Ignore false positives like 'mill.', 'did-', or 'mix,'
    return str;
  }
}

export function unlockAudioContext(context) {
  // https://hackernoon.com/unlocking-web-audio-the-smarter-way-8858218c0e09
  console.log('AudioContext initial state is %s.', context.state);
  if (context.state === 'suspended') {
    const events = ['touchstart', 'touchend', 'mousedown', 'mouseup'];
    const unlock = () => context.resume()
      .then(() => events.forEach(event => document.body.removeEventListener(event, unlock)));
    events.forEach(event => document.body.addEventListener(event, unlock, false));
  }
}

export function titlesFromMetadata(metadata) {
  if (metadata.formatted) {
    return metadata.formatted;
  }

  const title = allOrNone(metadata.artist, ' - ') + metadata.title;
  const subtitle = [metadata.game, metadata.system].filter(x => x).join(' - ') +
    allOrNone(' (', metadata.copyright, ')');
  return { title, subtitle };
}

export function allOrNone(...args) {
  let str = '';
  for (let i = 0; i < args.length; i++) {
    if (!args[i]) return '';
    str += args[i];
  }
  return str;
}

export function pathToLinks(path) {
  if (!path) return null;

  path = path
    .replace(CATALOG_PREFIX_REGEX, '/')
    .split('/').slice(0, -1).join('/') + '/';
  return <DirectoryLink dim to={'/browse' + path}>{decodeURI(path)}</DirectoryLink>;
}
