const fs = require('fs');
const glob = require('glob');
const directoryTree = require('directory-tree');
const { FORMATS } = require('../src/config');
const { toArabic } = require('roman-numerals');

// Paths are relative to project root.
//
// Point this to the place where you keep all the music.
// this location is untracked, so put a symlink here.
// For example:
//     chip-player-js $ ln -s ~/Downloads/catalog catalog
const catalogPath = 'catalog/';
const outputPath = 'server/catalog.json';
const dirDictOutputPath = 'server/directories.json';
const formatsRegex = new RegExp(`\\.(${FORMATS.join('|')})$`);
const romanNumeralNineRegex = /\bix\b/i;
const romanNumeralRegex = /\b([IVXLC]+|[ivxlc]+)([-.,) ]|$)/; // All upper case or all lower case
const NUMERIC_COLLATOR = new Intl.Collator(undefined, { numeric: true, sensitivity: 'base' });

/*

Sample catalog.json output:

[
  "Classical MIDI/Balakirev/Islamey – Fantaisie Orientale (G. Giulimondi).mid",
  "Classical MIDI/Balakirev/Islamey – Fantaisie Orientale (W. Pepperdine).mid"
]

Sample directories.json output:

{
  "/Classical MIDI/Balakirev": [
    {
      "path": "/Classical MIDI/Balakirev/Islamey – Fantaisie Orientale (G. Giulimondi).mid",
      "size": 54602,
      "type": "file",
      "idx": 0
    },
    {
      "path": "/Classical MIDI/Balakirev/Islamey – Fantaisie Orientale (W. Pepperdine).mid",
      "size": 213866,
      "type": "file",
      "idx": 1
    }
  ]
}

*/

function replaceRomanWithArabic(str) {
  // Works up to 399 (CCCXCIX)
  try {
    return str.replace(romanNumeralRegex, (_, match) => String(toArabic(match)).padStart(4, '0'));
  } catch (e) {
    // Ignore false positives like 'mill.', 'did-', or 'mix,'
    return str;
  }
}

if (!fs.existsSync(catalogPath)) {
  console.log('Couldn\'t find a music folder for indexing. Create a folder or symlink at \'%s\'.', catalogPath);
  process.exit(1);
}

const files = glob.sync(`${catalogPath}**/*.{${FORMATS.join(',')}}`, { nocase: true },)
  .map(file => file.replace(catalogPath, ''));

const data = JSON.stringify(files, null, 2);
fs.writeSync(fs.openSync(outputPath, 'w+'), data);
console.log('Wrote %d entries in %s (%d bytes).', files.length, outputPath, data.length);

const dirDict = {};
const dirOptions = {
  extensions: formatsRegex,
  attributes: [ 'mtimeMs' ],
};
directoryTree(catalogPath, dirOptions, null, item => {
  if (item.children) {
    item.path = item.path.replace(catalogPath, '/');

    const children = item.children.map(child => {
      child.path = child.path.replace(catalogPath, '/');
      if (child.children) {
        child.numChildren = child.children.length;
        delete child.children;
      }
      child.mtime = Math.floor(child.mtimeMs / 1000);
      delete child.name;
      delete child.extension;
      delete child.mtimeMs;
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
