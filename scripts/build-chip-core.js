const { spawn, execSync } = require('child_process');
const fs = require('fs');
const chalk = require('chalk');
const paths = require('../config/paths');
const path = require('path');

/**
 * Compile the C libraries with emscripten.
 */

const chipModules = [
  {
    name: 'visualizer',
    enabled: true,
    sourceFiles: [
      'src/showcqtbar.c',
    ],
    exportedFunctions: [
      // ---- Visualizer functions: ----
      '_cqt_init',
      '_cqt_calc',
      '_cqt_render_line',
      '_cqt_bin_to_freq',
    ],
    flags: [],
  },
  {
    // TODO: decouple from libADLMIDI and fluidlite,
    //       see also ../src/players/MIDIPlayer.js:207.
    name: 'player',
    enabled: true,
    sourceFiles: [
      'src/tinyplayer.c',
    ],
    exportedFunctions: [
      // ---- Tiny player functions: ----
      // '_tp_write_audio',
      // '_tp_open',
      '_tp_init',
      '_tp_unload_soundfont',
      '_tp_load_soundfont',
      '_tp_add_soundfont',
      // '_tp_stop',
      // '_tp_seek',
      // '_tp_set_speed',
      // '_tp_get_duration_ms',
      // '_tp_get_position_ms',
      '_tp_set_reverb',
      '_tp_get_polyphony',
      '_tp_set_polyphony',
      '_tp_get_channel_in_use',
      '_tp_get_channel_program',
      '_tp_set_channel_mute',
      '_tp_set_bank',
      '_tp_set_synth_engine',
      '_tp_set_ch10_melodic',
      '_tp_get_fluid_synth',
      // ---- MIDI synth functions: ----
      '_tp_note_on',
      '_tp_note_off',
      '_tp_program_change',
      '_tp_pitch_bend',
      '_tp_control_change',
      '_tp_channel_pressure',
      '_tp_render',
      '_tp_panic',
      '_tp_panic_channel',
      '_tp_reset',
    ],
    flags: [],
  },
  {
    name: 'gme',
    enabled: true,
    sourceFiles: [
      '../game-music-emu/build/gme/libgme.a',
    ],
    exportedFunctions: [
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
      '_gme_seek_scaled', // seek_scaled and tell_scaled exist in
      '_gme_tell_scaled', // github.com/mmontag/game-music-emu fork
      '_gme_set_fade',
      '_gme_voice_name',
      '_gme_set_stereo_depth',
    ],
    flags: [
      '-DHAVE_ZLIB_H',    // used by game_music_emu for vgz and lazyusf2 for psf
      '-DHAVE_STDINT_H',
    ],
  },
  {
    name: 'libxmp',
    enabled: true,
    sourceFiles: [
      // 'libxmp/lib/libxmp.a', // full libxmp build
      'libxmp/libxmp-lite-stagedir/lib/libxmp-lite.a',
    ],
    exportedFunctions: [
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
    ],
    flags: [],
  },
  {
    name: 'libvgm',
    enabled: true,
    sourceFiles: [
      'libvgm/build/bin/libvgm-emu.a',
      'libvgm/build/bin/libvgm-utils.a',
      'libvgm/build/bin/libvgm-player.a',
    ],
    exportedFunctions: [
      '_lvgm_init',
      '_lvgm_load_data',
      '_lvgm_start',
      '_lvgm_stop',
      '_lvgm_render',
      '_lvgm_get_position_ms',
      '_lvgm_get_duration_ms',
      '_lvgm_get_metadata',
      '_lvgm_get_voice_count',
      '_lvgm_get_voice_name',
      '_lvgm_get_voice_chip_name',
      '_lvgm_get_voice_mask',
      '_lvgm_set_voice_mask',
      '_lvgm_seek_ms',
      '_lvgm_set_playback_speed',
      '_lvgm_get_playback_speed',
      '_lvgm_reset',
    ],
    flags: [],
  },
  {
    name: 'fluidlite',
    enabled: true,
    sourceFiles: [
      'fluidlite/build/libfluidlite.a',
    ],
    exportedFunctions: [
      '_new_fluid_settings',
      '_new_fluid_synth',
      '_fluid_settings_setint',
      '_fluid_settings_setnum',
      '_fluid_settings_setstr',
      '_fluid_synth_set_interp_method',
      '_fluid_synth_sfload',
      '_fluid_synth_noteon',
      '_fluid_synth_noteoff',
      '_fluid_synth_all_notes_off',
      '_fluid_synth_all_sounds_off',
      '_fluid_synth_write_float',
      '_fluid_synth_set_reverb',
      '_fluid_synth_set_reverb_on',
      '_fluid_synth_set_chorus',
      '_fluid_synth_set_chorus_on',
      '_fluid_synth_get_polyphony',
      '_fluid_synth_set_polyphony',
      '_fluid_synth_bank_select',
    ],
    flags: [],
  },
  {
    name: 'libADLMIDI',
    enabled: true,
    sourceFiles: [
      'chips/dosbox_opl3.cpp',
      'chips/dosbox/dbopl.cpp',
      'wopl/wopl_file.c',
      'inst_db.cpp',
      'adlmidi.cpp',
      'adlmidi_load.cpp',
      'adlmidi_midiplay.cpp',
      'adlmidi_opl3.cpp',
      'adlmidi_private.cpp',
    ].map(file => 'libADLMIDI/src/' + file),
    exportedFunctions: [
      '_adl_init',
      '_adl_panic',
      '_adl_generate',
      '_adl_getBankNames',
      '_adl_getBanksCount',
      '_adl_setBank',
      '_adl_getNumChips',
      '_adl_setNumChips',
      '_adl_setSoftPanEnabled',
      '_adl_rt_controllerChange',
      '_adl_rt_channelAfterTouch',
      '_adl_rt_noteOff',
      '_adl_rt_noteOn',
      '_adl_rt_patchChange',
      '_adl_rt_pitchBend',
      '_adl_rt_resetState',
    ],
    flags: [
      '-IlibADLMIDI/include',
      '-DBWMIDI_DISABLE_XMI_SUPPORT',
      '-DBWMIDI_DISABLE_MUS_SUPPORT',
      '-DADLMIDI_DISABLE_MIDI_SEQUENCER',
      '-DADLMIDI_DISABLE_NUKED_EMULATOR',
      '-DADLMIDI_DISABLE_JAVA_EMULATOR',
      '-DADLMIDI_DISABLE_OPAL_EMULATOR',
      // '-DADLMIDI_DISABLE_DOSBOX_EMULATOR', // DOSBOX is recommended OPL3 core
    ],
  },
  {
    name: 'v2m',
    enabled: true,
    sourceFiles: [
      'ronan.cpp',
      'scope.cpp',
      'v2mplayer.cpp',
      'v2mconv.cpp',
      'synth_core.cpp',
      'sounddef.cpp',
      'v2mwrapper.cpp',
    ].map(file => 'farbrausch-v2m/' + file),
    exportedFunctions: [
      '_v2m_open',
      '_v2m_write_audio',
      '_v2m_get_position_ms',
      '_v2m_get_duration_ms',
      '_v2m_seek_ms',
      '_v2m_set_speed',
      '_v2m_close',
    ],
    flags: [
      '-flto',
      '-fno-asynchronous-unwind-tables',
      '-fno-stack-protector',
      '-ffunction-sections',
      '-fdata-sections',
      '-DRONAN',
      '-s', 'SAFE_HEAP=0',
    ],
  },
  {
    name: 'n64',
    enabled: true,
    sourceFiles: [
      'psflib/libpsflib.a',
      'lazyusf2/liblazyusf.a',
      'lazyusf2/_wothke/n64plug.cpp',
    ],
    exportedFunctions: [
      '_n64_load_file',
      '_n64_get_duration_ms',
      '_n64_get_position_ms',
      '_n64_seek_ms',
      '_n64_render_audio',
      '_n64_shutdown',
    ],
    flags: [
      '-Ipsflib',
      '-Ilazyusf2',
      '-DEMU_COMPILE',
      '-DEMU_LITTLE_ENDIAN',
      '-DNO_DEBUG_LOGS',
      '-Wno-pointer-sign',
      '-fno-strict-aliasing',
    ],
  },
  {
    name: 'mdx',
    enabled: true,
    sourceFiles: [
      'mdxmini/src/mdxmini.c',
      'mdxmini/src/mdx2151.c',
      'mdxmini/src/mdxmml_ym2151.c',
      'mdxmini/src/mdxfile.c',
      'mdxmini/src/nlg.c',
      'mdxmini/src/pcm8.c',
      'mdxmini/src/pdxfile.c',
      'mdxmini/src/ym2151.c', // TODO(montag): unify with gme; slightly different versions
    ],
    exportedFunctions: [
      '_mdx_set_rate',
      '_mdx_set_dir',
      '_mdx_open',
      '_mdx_close',
      '_mdx_next_frame',
      '_mdx_frame_length',
      '_mdx_calc_sample',
      '_mdx_calc_log',
      '_mdx_get_title',
      '_mdx_get_length',
      '_mdx_set_max_loop',
      '_mdx_get_tracks',
      '_mdx_get_current_notes',
      '_mdx_set_speed',
      '_mdx_get_position_ms',
      '_mdx_set_position_ms',
      '_mdx_create_context',
      '_mdx_get_pdx_filename',
      '_mdx_get_track_name',
      '_mdx_get_track_mask',
      '_mdx_set_track_mask',
    ],
    flags: [],
  },
];

const compiler = process.env.EMPP_BIN || 'em++';
const jsOutFile = 'src/chip-core.js';
const wasmOutFile = 'src/chip-core.wasm';
const wasmDir = paths.appPublic;
const wasmMapOutFile = wasmOutFile + '.map';
const wasmMapDir = path.resolve(paths.appPublic, '..');
const runtimeMethods = [
  'ALLOC_NORMAL',
  'FS',
  'UTF8ToString',
  'stringToNewUTF8',
  'ccall',
  'getValue',
  'setValue',
];
const exportedFns = [
  '_malloc',
  '_free',
].concat(...chipModules.filter(m => m.enabled).map(m => m.exportedFunctions));
const sourceFiles = [].concat(...chipModules.filter(m => m.enabled).map(m => m.sourceFiles));
const moduleFlags = [].concat(...chipModules.filter(m => m.enabled).map(m => m.flags));

const flags = [
  /*
  Build flags for Emscripten 3.1.39. Last updated July 15, 2024
  */
  // '--closure', '1',       // causes TypeError: lib.FS.mkdir is not a function
  // '--llvm-lto', '3',
  // '--clear-cache',        // sometimes Emscripten cache gets "poisoned"
  '--no-heap-copy',
  '-s', 'EXPORTED_FUNCTIONS=[' + exportedFns.join(',') + ']',
  '-s', 'EXPORTED_RUNTIME_METHODS=[' + runtimeMethods.join(',') + ']',
  '-s', 'ALLOW_MEMORY_GROWTH=1',
  '-s', 'ASSERTIONS=0',      // assertions increase runtime size about 100K
  '-s', 'STACK_OVERFLOW_CHECK=2',
  // '-s', 'STACK_SIZE=5MB', // support large VGM and XM files. default is 64KB
                             // disabled after allocating file data on heap
  '-s', 'MODULARIZE=1',
  '-s', 'EXPORT_NAME=CHIP_CORE',
  '-s', 'ENVIRONMENT=web',
  '-s', 'USE_ZLIB=1',
  '-s', 'EXPORT_ES6=1',
  '-s', 'USE_ES6_IMPORT_META=0',
  '-s', 'WASM_BIGINT',       // support passing 64 bit integers to/from JS
  '-lidbfs.js',
  '-Os',                     // set to O0 for fast compile during development
  // '-g',                   // include DWARF debug symbols. Increases size ~2.5x
  '-o', jsOutFile,

  /*
   WASM Source Maps

   These source maps require local fileserver running at chip-player-js root
   to expose C/C++ source files to browser; i.e. $ python -m http.server 9000
   Subproject static libraries must also be compiled with the emcc flags:
    `-g4 --source-map-base http://localhost:9000`.
   See lazyusf2/Makefile (for liblazyusf.a).
  */
  // '-g4',                     // include debug information
  // '--source-map-base', 'http://localhost:9000/',

  /*
  Warnings/misc.
   */
  '-Qunused-arguments',
  '-Wno-deprecated',
  '-Wno-logical-op-parentheses',
  '-Wno-c++11-extensions',
  '-Wno-inconsistent-missing-override',
  '-Wno-c++11-narrowing',
  '-std=c++11',

  ...moduleFlags,
];

console.log('Compiling to %s...', jsOutFile);
console.log(`Invocation:\n${compiler} ${chalk.blue(flags.join(' '))} ${chalk.gray(sourceFiles.join(' '))}\n`);
const preJs = `/*eslint-disable*/`;
const args = [].concat(flags, sourceFiles);
const build_proc = spawn(compiler, args, {stdio: 'inherit'});
build_proc.on('exit', function (code) {
  if (code === 0) {
    console.log('Moving %s to %s.', wasmOutFile, wasmDir);
    execSync(`mv ${wasmOutFile} ${wasmDir}`);

    if (fs.existsSync(wasmMapOutFile)) {
      console.log('Moving %s to %s.', wasmMapOutFile, wasmMapDir);
      execSync(`mv ${wasmMapOutFile} ${wasmMapDir}`);
    }

    // Don't use --pre-js because it can get stripped out by closure.
    console.log('Prepending %s: \n%s\n', jsOutFile, preJs.trim());
    execSync(`cat <<EOF > ${jsOutFile}\n${preJs}\n$(cat ${jsOutFile})\nEOF`);
  }
});
