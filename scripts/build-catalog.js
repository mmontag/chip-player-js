const fs = require('fs');
const glob = require('glob');
const {listToTrie} = require('../src/ResigTrie');

// Point this to the place where you keep all the music.
// this location is untracked, so put a symlink here.
// For example:
//     chip-player-js $ ln -s ~/Downloads/catalog catalog
const catalogRoot = 'catalog/';
const outputPath = 'public/catalog.json';
const formats = 'spc,vgm,vgz,nsf,nsfe,mid,s3m,it,mod,xm,ay';

if(!fs.existsSync(catalogRoot)) {
    console.log('Couldn\'t find a music folder for indexing. Create a folder or symlink at \'%s\'.', catalogRoot);
    process.exit(1);
}

const files = glob.sync(`${catalogRoot}**/*.{${formats}}`, {nocase: true},)
  .map(file => file.replace(catalogRoot, ''));

const trie = listToTrie(files);
const data = JSON.stringify(trie);
fs.writeSync(fs.openSync(outputPath, 'w+'), data);
console.log('Wrote %d entries in %s (%d bytes).', files.length, outputPath, data.length);