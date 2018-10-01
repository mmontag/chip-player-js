var glob = require('glob');
const { spawn, execSync } = require('child_process');
var tmp = require('tmp');
var fs = require('fs');
const paths = require('../config/paths');

var tmpObj = tmp.fileSync();
fs.writeSync(tmpObj.fd, "/*eslint-disable*/\n");

/**
 * Compile the C libraries with emscripten.
 */
var empp = process.env.EMPP_BIN || 'em++';


var source_files = 'libxmp/lib/libxmp.a';

var js_file = 'src/libxmp.js';
var wasm_file = 'src/libxmp.wasm';
var wasm_dir = paths.appPublic;
var export_name = 'XMP';
var exported_functions = [
  '_xmp_create_context',
  // '_xmp_free_context',
  // '_xmp_test_module',
  // '_xmp_load_module',
  // '_xmp_scan_module',
  // '_xmp_release_module',
  '_xmp_start_player',
  // '_xmp_play_frame',
  '_xmp_play_buffer',
  '_xmp_get_frame_info',
  '_xmp_end_player',
  '_xmp_inject_event',
  '_xmp_get_module_info',
  '_xmp_get_format_list',
  // '_xmp_next_position',
  // '_xmp_prev_position',
  // '_xmp_set_position',
  '_xmp_stop_module',
  '_xmp_restart_module',
  '_xmp_seek_time',
  '_xmp_channel_mute',
  // '_xmp_channel_vol',
  // '_xmp_set_player',
  '_xmp_get_player',
  // '_xmp_set_instrument_path',
  '_xmp_load_module_from_memory',
  // '_xmp_load_module_from_file'
];

var flags = [
  '-s', 'EXPORTED_FUNCTIONS=[' + exported_functions.join(',') + ']',
  '-s', 'EXTRA_EXPORTED_RUNTIME_METHODS=[allocate,ALLOC_NORMAL,ccall,getValue,setValue,Pointer_stringify]',
  '-s', 'ALLOW_MEMORY_GROWTH=1',
  '-s', 'ASSERTIONS=1',
  '-s', 'MODULARIZE=1',
  '-s', 'EXPORT_NAME=' + export_name,
  '-O1',
  '-o', js_file,
  '--pre-js', tmpObj.name,

  // GCC/Clang arguments to shut up about warnings in code I didn't
  // write. :D
  '-Wno-deprecated',
  '-Qunused-arguments',
  '-Wno-logical-op-parentheses',
  '-Wno-c++11-extensions',
  '-Wno-inconsistent-missing-override',
  '-std=c++11'
];
var args = [].concat(flags, source_files);

console.log('Compiling to %s...', js_file);
var build_proc = spawn(empp, args, {stdio: 'inherit'});
build_proc.on('exit', function () {
  console.log('Moving %s to %s.', wasm_file, wasm_dir);
  execSync(`mv ${wasm_file} ${wasm_dir}`);
});
