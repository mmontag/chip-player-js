const fs = require('fs');
const glob = require('glob');
const {listToTrie} = require('../src/ResigTrie');

// Point this to the place where you keep all the music...
const root = '/Users/montag/Music/Chip Archive/';
const formats = 'spc,vgm,vgz,nsf,nsfe,mid,s3m,it,mod,xm';

const files = glob.sync(`${root}**/*.{${formats}}`, {nocase: true},)
  .map(file => file.replace(root, ''));

const trie = listToTrie(files);
fs.writeSync(fs.openSync('src/catalog.json', 'w+'), JSON.stringify(trie));
