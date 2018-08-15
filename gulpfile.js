var argv = require('yargs').argv;
var glob = require('glob');
var gulp = require('gulp');
var gutil = require('gulp-util');
var path = require('path');
var spawn = require('child_process').spawn;

/**
 * Compile the C libraries with emscripten.
 */
gulp.task('build', function(done) {
    var empp = process.env.EMPP_BIN || argv.empp || 'em++';

    var gme_dir = path.join('emscripten', 'game_music_emu', 'gme');
    var gme_files = glob.sync(gme_dir + '/*.cpp');
    var json_dir = path.join('emscripten', 'json', 'ccan', 'json');
    var json_files = glob.sync(json_dir + '/*.c');
    var source_files = [].concat(gme_files, json_files);
    var outfile = 'libgme.js';
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
        '_gme_set_tempo'
    ];

    var flags = [
        '-s', 'EXPORTED_FUNCTIONS=['+exported_functions.join(',')+']',
        '-s', 'EXTRA_EXPORTED_RUNTIME_METHODS=[allocate,ALLOC_NORMAL,ccall,getValue,Pointer_stringify]',
        '-s', 'ALLOW_MEMORY_GROWTH=1',
        '-O3',
        '-I' + gme_dir,
        '-I' + json_dir,
        '-o', outfile,

        // GCC/Clang arguments to shut up about warnings in code I didn't
        // write. :D
        '-Wno-deprecated',
        '-Qunused-arguments',
        '-DVGM_YM2612_NUKED=1',
        '-Wno-logical-op-parentheses',
        '-Wno-c++11-extensions',
        '-Wno-inconsistent-missing-override'
    ];
    var args = [].concat(flags, source_files);

    gutil.log('Compiling via emscripten to ' + outfile);
    var build_proc = spawn(empp, args, {stdio: 'inherit'});
    build_proc.on('exit', function() {
        done();
    });
});
