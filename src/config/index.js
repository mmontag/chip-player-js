export const API_BASE = 'https://gifx.co/chip';
export const CATALOG_PREFIX = 'https://gifx.co/music/';
export const SOUNDFONT_URL_PATH = 'https://gifx.co/soundfonts/';
// export const API_BASE = 'http://localhost:8080';
// export const CATALOG_PREFIX = 'http://localhost:3000/catalog/';
// export const SOUNDFONT_URL_PATH = 'http://localhost:3000/soundfonts/';
export const MAX_VOICES = 64;
export const REPLACE_STATE_ON_SEEK = false;
export const FORMATS =  [
  'ay',
  'it',
  'mid',
  'mod',
  'nsf',
  'nsfe',
  'sgc',
  'spc',
  'kss',
  's3m',
  'v2m',
  'vgm',
  'vgz',
  'xm',
];

// needs to be a CommonJS module - used in node.js server
module.exports = {
  API_BASE,
  CATALOG_PREFIX,
  SOUNDFONT_URL_PATH,
  MAX_VOICES,
  REPLACE_STATE_ON_SEEK,
  FORMATS,
};
