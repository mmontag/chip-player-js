const API_BASE = 'https://gifx.co/chip';
const CATALOG_PREFIX = 'https://gifx.co/music/';
const SOUNDFONT_URL_PATH = 'https://gifx.co/soundfonts/';
// const API_BASE = 'http://localhost:8080';
// const CATALOG_PREFIX = 'http://localhost:9000/catalog/';
// const SOUNDFONT_URL_PATH = 'http://localhost:3000/soundfonts/';
const MAX_VOICES = 64;
const REPLACE_STATE_ON_SEEK = false;
const FORMATS =  [
  'ay',
  'it',
  'mdx',
  'mid',
  'miniusf',
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
