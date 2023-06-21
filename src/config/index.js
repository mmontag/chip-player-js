const API_BASE = 'https://gifx.co/chip';
const CATALOG_PREFIX = 'https://gifx.co/music/';
const SOUNDFONT_URL_PATH = 'https://gifx.co/soundfonts/';
// const API_BASE = 'http://localhost:8080';                       // npm run server - Node.js server on port 8080
// const CATALOG_PREFIX = 'http://localhost:8000/catalog/';        // python scripts/httpserver.py - Python file server
// const SOUNDFONT_URL_PATH = 'http://localhost:3000/soundfonts/'; // Webpack dev server
const MAX_VOICES = 64;
const REPLACE_STATE_ON_SEEK = false;
const ERROR_FLASH_DURATION_MS = 6000;
const FORMATS =  [
  'ay',
  'gbs',
  'it',
  'mdx',
  'mid',
  'miniusf',
  'mod',
  'nsf',
  'nsfe',
  'sgc',
  'spc',
  's3m',
  'smf',
  'v2m',
  'vgm',
  'vgz',
  'xm',
];
const SOUNDFONT_MOUNTPOINT = '/soundfonts';
const SOUNDFONTS = [
  {
    label: 'User SoundFonts (Drop .sf2 to add)',
    items: [],
  },
  {
    label: 'Small Soundfonts',
    items: [
      {label: 'GMGSx Plus (6.2 MB)', value: 'gmgsx-plus.sf2'},
      {label: 'Roland SC-55/SCC1 (3.3 MB)', value: 'Scc1t2.sf2'},
      {label: 'Yamaha DB50XG (3.9 MB)', value: 'Yamaha DB50XG.sf2'},
      {label: 'Gravis Ultrasound (5.9 MB)', value: 'Gravis_Ultrasound_Classic_PachSet_v1.6.sf2'},
      {label: 'Tim GM (6 MB)', value: 'TimGM6mb.sf2'},
      {label: 'Alan Chan 5MBGMGS (4.9 MB)', value: '5MBGMGS.SF2'},
      {label: 'E-mu 2MBGMGS (2.1 MB)', value: '2MBGMGS.SF2'},
      {label: 'E-mu 8MBGMGS (8.2 MB)', value: '8MBGMGS.SF2'},
    ],
  },
  {
    label: 'Large Soundfonts',
    items: [
      {label: 'Masquerade 55 v006 (18.4 MB)', value: 'masquerade55v006.sf2'},
      {label: 'GeneralUser GS v1.471 (31.3 MB)', value: 'generaluser.sf2'},
      {label: 'Chorium Revision A (28.9 MB)', value: 'choriumreva.sf2'},
      {label: 'Unison (29.3 MB)', value: 'Unison.SF2'},
      {label: 'Creative 28MBGM (29.7 MB)', value: '28MBGM.sf2'},
      {label: 'Musica Theoria 2 (30.5 MB)', value: 'mustheory2.sf2'},
      {label: 'Personal Copy Lite (31.4 MB)', value: 'PCLite.sf2'},
      {label: 'AnotherXG (31.4 MB)', value: 'bennetng_AnotherXG_v2-1.sf2'},
      {label: 'NTONYX 32Mb GM Stereo (32.5 MB)', value: '32MbGMStereo.sf2'},
      {label: 'Weeds GM 3 (54.9 MB)', value: 'weedsgm3.sf2'},
    ],
  },
  {
    label: 'Novelty Soundfonts',
    items: [
      {label: 'PC Beep (31 KB)', value: 'pcbeep.sf2'},
      {label: 'Nokia 6230i (227 KB)', value: 'Nokia_6230i_RM-72_.sf2'},
      {label: 'Kirby\'s Dream Land (271 KB)', value: 'Kirby\'s_Dream_Land_3.sf2'},
      {label: 'Vintage Waves v2 (315 KB)', value: 'Vintage Dreams Waves v2.sf2'},
      {label: 'Setzer\'s SPC Soundfont (1.2 MB)', value: 'Setzer\'s_SPC_Soundfont.sf2'},
      {label: 'SNES GM (1.9 MB)', value: 'Super_Nintendo_Unofficial_update.sf2'},
      {label: 'Nokia 30 (2.2 MB)', value: 'Nokia_30.sf2'},
      {label: 'LG Wink/Motorola ROKR (3.3 MB)', value: 'LG_Wink_Style_T310_Soundfont.sf2'},
      {label: 'Diddy Kong Racing DS (13.7 MB)', value: 'Diddy_Kong_Racing_DS_Soundfont.sf2'},
      {label: 'Regression FM v1.99g (14.4 MB)', value: 'R_FM_v1.99g-beta.sf2'},
      {label: 'Ultimate Megadrive (63.2 MB)', value: 'The Ultimate Megadrive Soundfont.sf2'},
    ],
  },
  {
    label: 'Piano Soundfonts',
    items: [
      {label: 'Yamaha Grand Lite v1.1 (21.8 MB)', value: 'Yamaha-Grand-Lite-v1.1.sf2'},
      {label: 'Abbey Steinway D v1.9 (40.2 MB)', value: 'Abbey-Steinway-D-v1.9.sf2'},
      {label: 'Chateau Grand Lite v1.0 (49.1 MB)', value: 'Chateau Grand Lite-v1.0.sf2'},
      {label: 'Steinway Grand v1.0 (145 MB)', value: 'Steinway Grand-SF4U-v1.sf2'},
    ],
  },
];

// needs to be a CommonJS module - used in node.js server
module.exports = {
  API_BASE,
  CATALOG_PREFIX,
  ERROR_FLASH_DURATION_MS,
  FORMATS,
  MAX_VOICES,
  REPLACE_STATE_ON_SEEK,
  SOUNDFONT_MOUNTPOINT,
  SOUNDFONT_URL_PATH,
  SOUNDFONTS,
};
