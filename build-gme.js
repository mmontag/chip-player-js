var glob = require('glob');
const { spawn, execSync } = require('child_process');
var tmp = require('tmp');
var fs = require('fs');
const paths = require('./config/paths');

var tmpObj = tmp.fileSync();
fs.writeSync(tmpObj.fd, "/*eslint-disable*/\n");

/**
 * Compile the C libraries with emscripten.
 */
var empp = process.env.EMPP_BIN || 'em++';
var gme_dir = './game-music-emu/gme';
var source_files = glob.sync(gme_dir + '/*.cpp');
var js_file = 'src/libgme.js';
var wasm_file = 'src/libgme.wasm';
var wasm_dir = paths.appPublic;
var exported_functions = [
  '_gme_open_data',
  '_gme_play',
  '_gme_delete',
  '_gme_mute_voices',
  '_gme_track_count',
  '_gme_track_ended',
  '_gme_voice_count',
  '_gme_track_info',
  '_gme_start_track',
  '_gme_open_data',
  '_gme_ignore_silence',
  '_gme_set_tempo',
  '_gme_seek',
  '_gme_tell',
  '_gme_set_fade',
];

var flags = [
  '-s', 'EXPORTED_FUNCTIONS=[' + exported_functions.join(',') + ']',
  '-s', 'EXTRA_EXPORTED_RUNTIME_METHODS=[allocate,ALLOC_NORMAL,ccall,getValue,Pointer_stringify]',
  '-s', 'ALLOW_MEMORY_GROWTH=1',
  '-s', 'ASSERTIONS=1',
  '-s', 'MODULARIZE=1',
  // '-s', 'MODULARIZE_INSTANCE=1',
  '-s', 'EXPORT_NAME=GME',
  // '-s', 'ENVIRONMENT=web',
  '-O1',
  '-I' + gme_dir,
  '-o', js_file,
  '--pre-js', tmpObj.name,

  // GCC/Clang arguments to shut up about warnings in code I didn't
  // write. :D
  '-Wno-deprecated',
  '-Qunused-arguments',
  '-DVGM_YM2612_NUKED=1',
  '-Wno-logical-op-parentheses',
  '-Wno-c++11-extensions',
  '-Wno-inconsistent-missing-override',
  '-std=c++11'
];
var args = [].concat(flags, source_files);

console.log('Compiling to %s...', js_file);
var begin = Date.now();
var build_proc = spawn(empp, args, {stdio: 'inherit'});
build_proc.on('exit', function () {
  console.log('Moving %s to %s.', wasm_file, wasm_dir);
  execSync(`mv ${wasm_file} ${wasm_dir}`);
});
