const fs = require('fs');
const glob = require('glob');
const directoryTree = require('directory-tree');
const { listToTrie } = require('./ResigTrie');
const { FORMATS } = require('../src/config');
const { toArabic } = require('roman-numerals');

// Point this to the place where you keep all the music.
// this location is untracked, so put a symlink here.
// For example:
//     chip-player-js $ ln -s ~/Downloads/catalog catalog
const catalogRoot = 'catalog/';
const outputPath = 'public/catalog.json';
const trieOutputPath = 'public/catalog-trie.json';
const dirDictOutputPath = 'public/directories.json';
const formatsRegex = new RegExp(`\\.(${FORMATS.join('|')})$`);
const romanNumeralNineRegex = /\bix\b/i;
const romanNumeralRegex = /\b([IVXLC]+|[ivxlc]+)[-.,)]/; // All upper case or all lower case
const NUMERIC_COLLATOR = new Intl.Collator(undefined, { numeric: true, sensitivity: 'base' });

function replaceRomanWithArabic(str) {
  // Works up to 399 (CCCXCIX)
  try {
    return str.replace(romanNumeralRegex, (_, match) => String(toArabic(match)).padStart(4, '0'));
  } catch (e) {
    // Ignore false positives like 'mill.', 'did-', or 'mix,'
    return str;
  }
}

if (!fs.existsSync(catalogRoot)) {
  console.log('Couldn\'t find a music folder for indexing. Create a folder or symlink at \'%s\'.', catalogRoot);
  process.exit(1);
}

const files = glob.sync(`${catalogRoot}**/*.{${FORMATS.join(',')}}`, { nocase: true },)
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
    item.path = item.path.replace(catalogRoot, '/');

    const children = item.children.map(child => {
      child.path = child.path.replace(catalogRoot, '/');
      if (child.children) {
        child.numChildren = child.children.length;
        delete child.children;
      }
      delete child.name;
      delete child.extension;
      return child;
    });

    const arabicMap = {};
    const needsRomanNumeralSort = children.some(item => {
      // Only convert Roman numerals if the list sort could benefit from it.
      // Roman numerals less than 9 would be sorted incidentally.
      // This assumes that Roman numeral ranges don't have gaps.
      return item.path.match(romanNumeralNineRegex);
    });
    if (needsRomanNumeralSort) {
      console.log("Roman numeral sort is active for %s", item.path);
      // Movement IV. Wow => Movement 0004. Wow
      children.forEach(item => arabicMap[item.path] = replaceRomanWithArabic(item.path));
    }

    children
      .sort((a, b) => {
        const [strA, strB] = needsRomanNumeralSort ?
          [arabicMap[a.path], arabicMap[b.path]] :
          [a.path, b.path];
        return NUMERIC_COLLATOR.compare(strA, strB);
      })
      .sort((a, b) => {
        if (a.type < b.type) return -1;
        if (a.type > b.type) return 1;
        return 0;
      });

    // Add file idx property
    children.filter(child => child.type === 'file').forEach((item, idx) => item.idx = idx);
    dirDict[item.path] = children;
  }
});

const dirDictData = JSON.stringify(dirDict, null, 2);
fs.writeSync(fs.openSync(dirDictOutputPath, 'w+'), dirDictData);
console.log('Wrote %d entries in %s (%d bytes).', Object.keys(dirDict).length, dirDictOutputPath, dirDictData.length);
