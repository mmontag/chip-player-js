const fs = require('fs');
const glob = require('glob');
const {listToTrie} = require('../src/ResigTrie');
const directoryTree = require('directory-tree');

// Point this to the place where you keep all the music.
// this location is untracked, so put a symlink here.
// For example:
//     chip-player-js $ ln -s ~/Downloads/catalog catalog
const catalogRoot = 'catalog/';
const outputPath = 'public/catalog.json';
const trieOutputPath = 'public/catalog-trie.json';
const dirDictOutputPath = 'public/directories.json';
const formats = 'spc,vgm,vgz,nsf,nsfe,mid,s3m,it,mod,xm,ay,sgc,kss,v2m,gbs';
const formatsRegex = new RegExp(`\\.(${formats.split(',').join('|')})$`);

if(!fs.existsSync(catalogRoot)) {
    console.log('Couldn\'t find a music folder for indexing. Create a folder or symlink at \'%s\'.', catalogRoot);
    process.exit(1);
}

const files = glob.sync(`${catalogRoot}**/*.{${formats}}`, {nocase: true},)
  .map(file => file.replace(catalogRoot, ''));

const data = JSON.stringify(files, null, 2);
fs.writeSync(fs.openSync(outputPath, 'w+'), data);
console.log('Wrote %d entries in %s (%d bytes).', files.length, outputPath, data.length);

const trie = listToTrie(files);

const trieData = JSON.stringify(trie);
fs.writeSync(fs.openSync(trieOutputPath, 'w+'), trieData);
console.log('Wrote %d entries in %s (%d bytes).', files.length, trieOutputPath, trieData.length);

const dirDict = {};
directoryTree(catalogRoot, { extensions: formatsRegex }, null, item => {
    if (item.children) {
        item.children = item.children.map(child => {
            child.path = child.path.replace(catalogRoot, '/');
            if (child.children) {
                child.numChildren = child.children.length;
                delete child.children;
            }
            delete child.name;
            delete child.extension;
            return child;
        });
        item.path = item.path.replace(catalogRoot, '/');
        dirDict[item.path] = item.children;
    }
});

const dirDictData = JSON.stringify(dirDict, null, 2);
fs.writeSync(fs.openSync(dirDictOutputPath, 'w+'), dirDictData);
console.log('Wrote %d entries in %s (%d bytes).', Object.keys(dirDict).length, dirDictOutputPath, dirDictData.length);
