const fs = require('fs');
const glob = require('glob');

const {listToTrie} = require('../src/ResigTrie');

const root = '/Users/montag/Music/Video Game Music/chip-archive/';
const files = glob.sync(
  // '{Nintendo,Sega Master System,Sega Genesis,Sega Game Gear,Nintendo SNES,MIDI}/' +
  root + '*/*/*.{spc,vgm,vgz,nsf,nsfe,mid,s3m,it,mod,xm}',
  {nocase: true},
).map(file => file.replace(root, ''));

const trie = listToTrie(files);
fs.writeSync(fs.openSync('src/catalog.json', 'w+'), JSON.stringify(trie));
