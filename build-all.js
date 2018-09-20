var glob = require('glob');
const { spawn, execSync } = require('child_process');
var fs = require('fs');
const paths = require('./config/paths');

/**
 * Compile the C libraries with emscripten.
 */
var empp = process.env.EMPP_BIN || 'em++';

var gme_dir = './game-music-emu/gme';
// var unrar_dir = './unrar';
var source_files = [];
source_files = source_files.concat(glob.sync(gme_dir + '/*.cpp'));
source_files.push('libxmp/libxmp-lite-stagedir/lib/libxmp-lite.a');
source_files.push('fluidlite/build/libfluidlite.a');
source_files.push('TinySoundFont/tinyplayer.c');

var js_file = 'src/chipplayer.js';
var wasm_file = 'src/chipplayer.wasm';
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
  '_gme_voice_name',

  '_xmp_create_context',
  '_xmp_start_player',
  '_xmp_play_buffer',
  '_xmp_get_frame_info',
  '_xmp_end_player',
  '_xmp_inject_event',
  '_xmp_get_module_info',
  '_xmp_get_format_list',
  '_xmp_stop_module',
  '_xmp_restart_module',
  '_xmp_seek_time',
  '_xmp_channel_mute',
  '_xmp_get_player',
  '_xmp_load_module_from_memory',

  '_new_fluid_settings',
  '_new_fluid_synth',
  '_fluid_settings_setint',
  '_fluid_settings_setnum',
  '_fluid_settings_setstr',
  '_fluid_synth_sfload',
  '_fluid_synth_noteon',
  '_fluid_synth_noteoff',
  '_fluid_synth_all_notes_off',
  '_fluid_synth_all_sounds_off',
  '_fluid_synth_write_float',

  '_tp_write_audio',
  '_tp_open',
  '_tp_init',
  '_tp_load_soundfont',
  '_tp_stop',
  '_tp_seek',
  '_tp_set_speed',
  '_tp_get_duration_ms',
  '_tp_get_position_ms',
];

var runtime_methods = [
  'ALLOC_NORMAL',
  'FS',
  'Pointer_stringify',
  'allocate',
  'ccall',
  'getValue',
  'setValue',
];

var flags = [
  '-s', 'EXPORTED_FUNCTIONS=[' + exported_functions.join(',') + ']',
  '-s', 'EXPORTED_RUNTIME_METHODS=[' + runtime_methods.join(',') + ']',
  '-s', 'ALLOW_MEMORY_GROWTH=1',
  '-s', 'ASSERTIONS=1',
  '-s', 'MODULARIZE=1',
  '-s', 'EXPORT_NAME=CHIPPLAYER',
  '-s', 'ENVIRONMENT=web',
  '-O2',
  // '--closure', '1',
  // '--llvm-lto', '3',
  '-o', js_file,

  '-DVGM_YM2612_GENS=1', // the gme YM2612 GENS core is much faster than NUKED

  '-Qunused-arguments',
  '-Wno-deprecated',
  '-Wno-logical-op-parentheses',
  '-Wno-c++11-extensions',
  '-Wno-inconsistent-missing-override',
  '-std=c++11',
];
var args = [].concat(flags, source_files);

console.log('Compiling to %s...', js_file);
var build_proc = spawn(empp, args, {stdio: 'inherit'});
build_proc.on('exit', function (code) {
  if (code === 0) {
    console.log('Moving %s to %s.', wasm_file, wasm_dir);
    execSync(`mv ${wasm_file} ${wasm_dir}`);

    // Don't use --pre-js because it can get stripped out by closure.
    const eslint_disable = '/*eslint-disable*/\n';
    console.log('Prepending %s with %s.', js_file, eslint_disable.trim());
    const data = fs.readFileSync(js_file);
    const fd = fs.openSync(js_file, 'w+');
    const insert = new Buffer(eslint_disable);
    fs.writeSync(fd, insert, 0, insert.length, 0);
    fs.writeSync(fd, data, 0, data.length, insert.length);
    fs.close(fd, (err) => {
      if (err) throw err;
    });
  }
});
