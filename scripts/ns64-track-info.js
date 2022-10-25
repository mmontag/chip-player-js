const fs = require('fs');
const glob = require('glob');
const path = require('path');
const { EVENT_META, EVENT_META_MARKER } = require('midievents');

let tracks = `{ u8"Kirby 64 - The Crystal Shards (U) 0000002D 002968E0.mid", u8"Kirby 64 - The Crystal Shards (U) 0000002D 002968E0.mid TrackParseDebug.txt", u8"Opening", 0 },
{ u8"Kirby 64 - The Crystal Shards (U) 00000028 00292654.mid", u8"Kirby 64 - The Crystal Shards (U) 00000028 00292654.mid TrackParseDebug.txt", u8"File Select", 0 },
{ u8"Kirby 64 - The Crystal Shards (U) 0000002E 00297D08.mid", u8"Kirby 64 - The Crystal Shards (U) 0000002E 00297D08.mid TrackParseDebug.txt", u8"Crystal Shards (Story Demo 1)", 0 },
{ u8"Kirby 64 - The Crystal Shards (U) 00000022 00287284.mid", u8"Kirby 64 - The Crystal Shards (U) 00000022 00287284.mid TrackParseDebug.txt", u8"Training", 0 },
{ u8"Kirby 64 - The Crystal Shards (U) 00000027 0028D42C.mid", u8"Kirby 64 - The Crystal Shards (U) 00000027 0028D42C.mid TrackParseDebug.txt", u8"World Map", 0 },
{ u8"Kirby 64 - The Crystal Shards (U) 0000001D 00283D74.mid", u8"Kirby 64 - The Crystal Shards (U) 0000001D 00283D74.mid TrackParseDebug.txt", u8"Pop Star - Map", 0 },
{ u8"Kirby 64 - The Crystal Shards (U) 0000000E 00268554.mid", u8"Kirby 64 - The Crystal Shards (U) 0000000E 00268554.mid TrackParseDebug.txt", u8"Pop Star (Plains Area)", 0 },
{ u8"Kirby 64 - The Crystal Shards (U) 00000024 0028AB94.mid", u8"Kirby 64 - The Crystal Shards (U) 00000024 0028AB94.mid TrackParseDebug.txt", u8"Room Guarder (Vs Sub-Boss)", 0 },
{ u8"Kirby 64 - The Crystal Shards (U) 00000019 00280D4C.mid", u8"Kirby 64 - The Crystal Shards (U) 00000019 00280D4C.mid TrackParseDebug.txt", u8"Stage Goal", 0 },
{ u8"Kirby 64 - The Crystal Shards (U) 0000002F 00298CA8.mid", u8"Kirby 64 - The Crystal Shards (U) 0000002F 00298CA8.mid TrackParseDebug.txt", u8"What! (Story Demo 2)", 0 },
{ u8"Kirby 64 - The Crystal Shards (U) 0000002A 00293EF4.mid", u8"Kirby 64 - The Crystal Shards (U) 0000002A 00293EF4.mid TrackParseDebug.txt", u8"Battle Among Friends 1 (Vs Waddle Dee)", 0 },
{ u8"Kirby 64 - The Crystal Shards (U) 00000030 00299970.mid", u8"Kirby 64 - The Crystal Shards (U) 00000030 00299970.mid TrackParseDebug.txt", u8"I’ll Go, Too (Story Demo 3)", 0 },
{ u8"Kirby 64 - The Crystal Shards (U) 00000013 00275FB8.mid", u8"Kirby 64 - The Crystal Shards (U) 00000013 00275FB8.mid TrackParseDebug.txt", u8"Quiet Forest (Forest Area)", 0 },
{ u8"Kirby 64 - The Crystal Shards (U) 00000031 0029A228.mid", u8"Kirby 64 - The Crystal Shards (U) 00000031 0029A228.mid TrackParseDebug.txt", u8"Eek! (Story Demo 4)", 0 },
{ u8"Kirby 64 - The Crystal Shards (U) 0000002B 002945FC.mid", u8"Kirby 64 - The Crystal Shards (U) 0000002B 002945FC.mid TrackParseDebug.txt", u8"Battle Among Friends 2 (Vs Adeleine)", 0 },
{ u8"Kirby 64 - The Crystal Shards (U) 00000032 0029AC94.mid", u8"Kirby 64 - The Crystal Shards (U) 00000032 0029AC94.mid TrackParseDebug.txt", u8"Mix Me In, Too (Story Demo 5)", 0 },
{ u8"Kirby 64 - The Crystal Shards (U) 00000009 0025EBD8.mid", u8"Kirby 64 - The Crystal Shards (U) 00000009 0025EBD8.mid TrackParseDebug.txt", u8"King Dedede’s Castle (Castle Area) [“Friends 3” from Kirby’s Dream Land 3]", 0 },
{ u8"Kirby 64 - The Crystal Shards (U) 00000033 0029BC60.mid", u8"Kirby 64 - The Crystal Shards (U) 00000033 0029BC60.mid TrackParseDebug.txt", u8"Whoa!! (Story Demo 6)", 0 },
{ u8"Kirby 64 - The Crystal Shards (U) 0000002C 00295704.mid", u8"Kirby 64 - The Crystal Shards (U) 0000002C 00295704.mid TrackParseDebug.txt", u8"Battle Among Friends 3 (Vs King Dedede)", 0 },
{ u8"Kirby 64 - The Crystal Shards (U) 00000034 0029CB50.mid", u8"Kirby 64 - The Crystal Shards (U) 00000034 0029CB50.mid TrackParseDebug.txt", u8"I’ll Come Along With You (Story Demo 7)", 0 },
{ u8"Kirby 64 - The Crystal Shards (U) 00000035 0029D630.mid", u8"Kirby 64 - The Crystal Shards (U) 00000035 0029D630.mid TrackParseDebug.txt", u8"Alright, On to the Next (Story Demo 8)", 0 },
{ u8"Kirby 64 - The Crystal Shards (U) 0000001C 00283750.mid", u8"Kirby 64 - The Crystal Shards (U) 0000001C 00283750.mid TrackParseDebug.txt", u8"Rock Star - Map", 0 },
{ u8"Kirby 64 - The Crystal Shards (U) 0000000D 00265FD0.mid", u8"Kirby 64 - The Crystal Shards (U) 0000000D 00265FD0.mid TrackParseDebug.txt", u8"Rock Star (Desert-Mountain Area)", 0 },
{ u8"Kirby 64 - The Crystal Shards (U) 00000014 00277458.mid", u8"Kirby 64 - The Crystal Shards (U) 00000014 00277458.mid TrackParseDebug.txt", u8"Ancient Ruins (Ruins-Deep Sea Area)", 0 },
{ u8"Kirby 64 - The Crystal Shards (U) 00000012 00274C84.mid", u8"Kirby 64 - The Crystal Shards (U) 00000012 00274C84.mid TrackParseDebug.txt", u8"Inside the Ruins (Spaceship Area)", 0 },
{ u8"Kirby 64 - The Crystal Shards (U) 00000036 0029E36C.mid", u8"Kirby 64 - The Crystal Shards (U) 00000036 0029E36C.mid TrackParseDebug.txt", u8"I’m Hungry (Story Demo 9)", 0 },
{ u8"Kirby 64 - The Crystal Shards (U) 0000001E 002844C0.mid", u8"Kirby 64 - The Crystal Shards (U) 0000001E 002844C0.mid TrackParseDebug.txt", u8"Aqua Star - Map", 0 },
{ u8"Kirby 64 - The Crystal Shards (U) 0000000B 002619AC.mid", u8"Kirby 64 - The Crystal Shards (U) 0000000B 002619AC.mid TrackParseDebug.txt", u8"Aqua Star (Sea Area)", 0 },
{ u8"Kirby 64 - The Crystal Shards (U) 00000010 0026E7E8.mid", u8"Kirby 64 - The Crystal Shards (U) 00000010 0026E7E8.mid TrackParseDebug.txt", u8"Down the Mountain Stream (River Area) [“Grass Land” from Kirby’s Dream Land 3]", 0 },
{ u8"Kirby 64 - The Crystal Shards (U) 00000037 0029FA40.mid", u8"Kirby 64 - The Crystal Shards (U) 00000037 0029FA40.mid TrackParseDebug.txt", u8"Idiot of the Sea (Story Demo 10)", 0 },
{ u8"Kirby 64 - The Crystal Shards (U) 0000001F 00284C28.mid", u8"Kirby 64 - The Crystal Shards (U) 0000001F 00284C28.mid TrackParseDebug.txt", u8"Neo Star - Map", 0 },
{ u8"Kirby 64 - The Crystal Shards (U) 0000000C 00263760.mid", u8"Kirby 64 - The Crystal Shards (U) 0000000C 00263760.mid TrackParseDebug.txt", u8"Neo Star (Volcano Area)", 0 },
{ u8"Kirby 64 - The Crystal Shards (U) 00000000 0025051C.mid", u8"Kirby 64 - The Crystal Shards (U) 00000000 0025051C.mid TrackParseDebug.txt", u8"Vs Boss", 0 },
{ u8"Kirby 64 - The Crystal Shards (U) 00000038 002A09CC.mid", u8"Kirby 64 - The Crystal Shards (U) 00000038 002A09CC.mid TrackParseDebug.txt", u8"Big Eruption (Story Demo 11)", 0 },
{ u8"Kirby 64 - The Crystal Shards (U) 00000020 00286224.mid", u8"Kirby 64 - The Crystal Shards (U) 00000020 00286224.mid TrackParseDebug.txt", u8"Shiver Star - Map", 0 },
{ u8"Kirby 64 - The Crystal Shards (U) 00000011 00271848.mid", u8"Kirby 64 - The Crystal Shards (U) 00000011 00271848.mid TrackParseDebug.txt", u8"Shiver Star (Snow Area)", 0 },
{ u8"Kirby 64 - The Crystal Shards (U) 0000000F 0026B8B8.mid", u8"Kirby 64 - The Crystal Shards (U) 0000000F 0026B8B8.mid TrackParseDebug.txt", u8"Above the Clouds (Sky Area) [“Butter Building” from Kirby’s Adventure]", 0 },
{ u8"Kirby 64 - The Crystal Shards (U) 00000016 0027B904.mid", u8"Kirby 64 - The Crystal Shards (U) 00000016 0027B904.mid TrackParseDebug.txt", u8"Factory Inspection (Factory Area)", 0 },
{ u8"Kirby 64 - The Crystal Shards (U) 00000039 002A1884.mid", u8"Kirby 64 - The Crystal Shards (U) 00000039 002A1884.mid TrackParseDebug.txt", u8"Overnight Detective (Story Demo 12)", 0 },
{ u8"Kirby 64 - The Crystal Shards (U) 00000026 0028BFE4.mid", u8"Kirby 64 - The Crystal Shards (U) 00000026 0028BFE4.mid TrackParseDebug.txt", u8"Ripple Star - Map", 0 },
{ u8"Kirby 64 - The Crystal Shards (U) 00000015 00279DB8.mid", u8"Kirby 64 - The Crystal Shards (U) 00000015 00279DB8.mid TrackParseDebug.txt", u8"Infiltration (Cistern Area)", 0 },
{ u8"Kirby 64 - The Crystal Shards (U) 00000017 0027D0DC.mid", u8"Kirby 64 - The Crystal Shards (U) 00000017 0027D0DC.mid TrackParseDebug.txt", u8"Ripple Star (Castle Area)", 0 },
{ u8"Kirby 64 - The Crystal Shards (U) 00000003 00259610.mid", u8"Kirby 64 - The Crystal Shards (U) 00000003 00259610.mid TrackParseDebug.txt", u8"Vs Miracle Matter", 0 },
{ u8"Kirby 64 - The Crystal Shards (U) 0000003B 002A35AC.mid", u8"Kirby 64 - The Crystal Shards (U) 0000003B 002A35AC.mid TrackParseDebug.txt", u8"Bye-Bye (Story Demo 13)", 0 },
{ u8"Kirby 64 - The Crystal Shards (U) 00000002 0025784C.mid", u8"Kirby 64 - The Crystal Shards (U) 00000002 0025784C.mid TrackParseDebug.txt", u8"Dark Star (Final Area)", 0 },
{ u8"Kirby 64 - The Crystal Shards (U) 0000003A 002A2280.mid", u8"Kirby 64 - The Crystal Shards (U) 0000003A 002A2280.mid TrackParseDebug.txt", u8"Final Battle (Story Demo 14)", 0 },
{ u8"Kirby 64 - The Crystal Shards (U) 00000001 002526F8.mid", u8"Kirby 64 - The Crystal Shards (U) 00000001 002526F8.mid TrackParseDebug.txt", u8"Vs 0²", 0 },
{ u8"Kirby 64 - The Crystal Shards (U) 0000003C 002A48BC.mid", u8"Kirby 64 - The Crystal Shards (U) 0000003C 002A48BC.mid TrackParseDebug.txt", u8"Grand Finale (Ending)", 0 },
{ u8"Kirby 64 - The Crystal Shards (U) 0000003D 002A5AC0.mid", u8"Kirby 64 - The Crystal Shards (U) 0000003D 002A5AC0.mid TrackParseDebug.txt", u8"Staff Roll", 0 },
{ u8"Kirby 64 - The Crystal Shards (U) 00000008 0025D89C.mid", u8"Kirby 64 - The Crystal Shards (U) 00000008 0025D89C.mid TrackParseDebug.txt", u8"Theater [“Friends 2” from Kirby’s Dream Land 3]", 0 },
{ u8"Kirby 64 - The Crystal Shards (U) 00000029 00292FC8.mid", u8"Kirby 64 - The Crystal Shards (U) 00000029 00292FC8.mid TrackParseDebug.txt", u8"Enemy Info [“Mt. Dedede” from Kirby’s Dream Land]", 0 },
{ u8"Kirby 64 - The Crystal Shards (U) 00000025 0028B5E8.mid", u8"Kirby 64 - The Crystal Shards (U) 00000025 0028B5E8.mid TrackParseDebug.txt", u8"Sub-Game Select", 0 },
{ u8"Kirby 64 - The Crystal Shards (U) 0000001A 00281120.mid", u8"Kirby 64 - The Crystal Shards (U) 0000001A 00281120.mid TrackParseDebug.txt", u8"100-Yard Hop", 0 },
{ u8"Kirby 64 - The Crystal Shards (U) 00000023 00289104.mid", u8"Kirby 64 - The Crystal Shards (U) 00000023 00289104.mid TrackParseDebug.txt", u8"Bumper-Crop Bump", 0 },
{ u8"Kirby 64 - The Crystal Shards (U) 0000001B 0028280C.mid", u8"Kirby 64 - The Crystal Shards (U) 0000001B 0028280C.mid TrackParseDebug.txt", u8"Checker Board Chase", 0 },
{ u8"Kirby 64 - The Crystal Shards (U) 0000000A 002606C8.mid", u8"Kirby 64 - The Crystal Shards (U) 0000000A 002606C8.mid TrackParseDebug.txt", u8"Sub-Game Results", 0 },
{ u8"Kirby 64 - The Crystal Shards (U) 00000021 00286994.mid", u8"Kirby 64 - The Crystal Shards (U) 00000021 00286994.mid TrackParseDebug.txt", u8"Invincible", 0 },
{ u8"Kirby 64 - The Crystal Shards (U) 00000006 0025BDE0.mid", u8"Kirby 64 - The Crystal Shards (U) 00000006 0025BDE0.mid TrackParseDebug.txt", u8"Lose Life", 0 },
{ u8"Kirby 64 - The Crystal Shards (U) 00000018 002806D8.mid", u8"Kirby 64 - The Crystal Shards (U) 00000018 002806D8.mid TrackParseDebug.txt", u8"Game Over", 0 },
{ u8"Kirby 64 - The Crystal Shards (U) 00000005 0025BA60.mid", u8"Kirby 64 - The Crystal Shards (U) 00000005 0025BA60.mid TrackParseDebug.txt", u8"Kirby Clear Dance 1 (Unused)", 0 },
{ u8"Kirby 64 - The Crystal Shards (U) 00000004 0025B518.mid", u8"Kirby 64 - The Crystal Shards (U) 00000004 0025B518.mid TrackParseDebug.txt", u8"Kirby Clear Dance 2 (Unused)", 0 },
{ u8"Kirby 64 - The Crystal Shards (U) 00000007 0025C274.mid", u8"Kirby 64 - The Crystal Shards (U) 00000007 0025C274.mid TrackParseDebug.txt", u8"Unused [“Kine’s Theme” from Kirby’s Dream Land 2]", 0 },
//{ u8"Kirby 64 - The Crystal Shards (U) 0000003E 002A8C38.mid", u8"Kirby 64 - The Crystal Shards (U) 0000003E 002A8C38.mid TrackParseDebug.txt", u8"0000003E 002A8C38", 0 },`;


const DIR = '/Users/montag/Music/kirbky';
let dryRun = true;
dryRun = false;

// const midiFiles = glob.sync(path.join(DIR, '*00C 00*.mid'));
const debugFiles = glob.sync(path.join(DIR, '*TrackParseDebug.txt')).sort((a, b) => a.localeCompare(b));
// const debugFiles = glob.sync(path.join(DIR, '*TrackParseDebug.txt'));
const midiFiles = debugFiles.map(f => f.replace(' TrackParseDebug.txt', ''));
const re = /([0-9A-F]{8} [0-9A-F]{8}?).+TrackParseDebug\.txt", u8"(.+?)",/;

const fileMap = {};

const lines = tracks.split('\n')
  .map((t, i) => {
    const match = t.match(re);
    fileMap[match[1]] = match[2];
    return {
      origIdx: i,
      hex: match[1],
      name: match[2],
    };
  })
  .sort((a, b) => a.hex.localeCompare(b.hex))
  .map((l, i) => {
    return {
      ...l, midiFile: midiFiles[i],
    }
  });


lines.forEach((l, i) => {
  const idx = (l.origIdx + 1).toString().padStart(2, '0');
  const destName = `${idx} - ${l.name}`;
  const doFn = dryRun ? console.log : fs.renameSync;
  doFn(l.midiFile, path.join(DIR, `${destName}.mid`));
  doFn(debugFiles[i], path.join(DIR, `${destName} TrackParseDebug.txt`));
});

console.log('~ end ~');
// const namesByHex = lines.sort((a, b) => a.hex.localeCompare(b.hex)); //.map(l => console.log('|' + l.name));


// console.log(namesByHex);
