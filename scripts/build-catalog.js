const fs = require('fs');
const glob = require('glob');
const {listToTrie} = require('../src/ResigTrie');

const outputPath = 'public/catalog.json';
// Point this to the place where you keep all the music...
const root = '/Users/montag/Music/Chip Archive/';
const formats = 'spc,vgm,vgz,nsf,nsfe,mid,s3m,it,mod,xm,ay';

const files = glob.sync(`${root}**/*.{${formats}}`, {nocase: true},)
  .map(file => file.replace(root, ''));

const trie = listToTrie(files);
const data = JSON.stringify(trie);
fs.writeSync(fs.openSync(outputPath, 'w+'), data);
console.log('Wrote %d entries in %s (%d bytes).', files.length, outputPath, data.length);