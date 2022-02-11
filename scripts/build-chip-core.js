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
      'Ay_Apu.cpp',
      'Ay_Core.cpp',
      'Ay_Cpu.cpp',
      'Ay_Emu.cpp',
      'blargg_common.cpp',
      'blargg_errors.cpp',
      'Blip_Buffer.cpp',
      'Bml_Parser.cpp',
      'c140.c',
      'C140_Emu.cpp',
      'Classic_Emu.cpp',
      'dac_control.c',
      'Data_Reader.cpp',
      // 'dbopl.cpp', // Currently using later version of dbopl.cpp from libADLMIDI
      'Downsampler.cpp',
      'Dual_Resampler.cpp',
      'Effects_Buffer.cpp',
      'Fir_Resampler.cpp',
      'fm.c',
      'fm2612.c',
      'fmopl.cpp',
      'Gb_Apu.cpp',
      'Gb_Cpu.cpp',
      'Gb_Oscs.cpp',
      'Gbs_Core.cpp',
      'Gbs_Cpu.cpp',
      'Gbs_Emu.cpp',
      'gme.cpp',
      'Gme_File.cpp',
      'Gme_Loader.cpp',
      'Gym_Emu.cpp',
      'Hes_Apu.cpp',
      'Hes_Apu_Adpcm.cpp',
      'Hes_Core.cpp',
      'Hes_Cpu.cpp',
      'Hes_Emu.cpp',
      'higan/dsp/dsp.cpp',
      'higan/dsp/SPC_DSP.cpp',
      'higan/processor/spc700/spc700.cpp',
      'higan/smp/memory.cpp',
      'higan/smp/smp.cpp',
      'higan/smp/timing.cpp',
      'k051649.c',
      'K051649_Emu.cpp',
      'k053260.c',
      'K053260_Emu.cpp',
      'k054539.c',
      'K054539_Emu.cpp',
      'Kss_Core.cpp',
      'Kss_Cpu.cpp',
      'Kss_Emu.cpp',
      'Kss_Scc_Apu.cpp',
      'M3u_Playlist.cpp',
      'Multi_Buffer.cpp',
      'Music_Emu.cpp',
      'Nes_Apu.cpp',
      'Nes_Cpu.cpp',
      'Nes_Fds_Apu.cpp',
      'Nes_Fme7_Apu.cpp',
      'Nes_Namco_Apu.cpp',
      'Nes_Oscs.cpp',
      'Nes_Vrc6_Apu.cpp',
      'Nes_Vrc7_Apu.cpp',
      'Nsf_Core.cpp',
      'Nsf_Cpu.cpp',
      'Nsf_Emu.cpp',
      'Nsf_Impl.cpp',
      'Nsfe_Emu.cpp',
      'okim6258.c',
      'Okim6258_Emu.cpp',
      'okim6295.c',
      'Okim6295_Emu.cpp',
      'Opl_Apu.cpp',
      'pwm.c',
      'Pwm_Emu.cpp',
      'qmix.c',
      'Qsound_Apu.cpp',
      'Resampler.cpp',
      'Rf5C164_Emu.cpp',
      'rf5c68.c',
      'Rf5C68_Emu.cpp',
      'Rom_Data.cpp',
      'Sap_Apu.cpp',
      'Sap_Core.cpp',
      'Sap_Cpu.cpp',
      'Sap_Emu.cpp',
      'scd_pcm.c',
      'segapcm.c',
      'SegaPcm_Emu.cpp',
      'Sgc_Core.cpp',
      'Sgc_Cpu.cpp',
      'Sgc_Emu.cpp',
      'Sgc_Impl.cpp',
      'Sms_Apu.cpp',
      'Sms_Fm_Apu.cpp',
      'Spc_Emu.cpp',
      'Spc_Filter.cpp',
      'Spc_Sfm.cpp',
      'Track_Filter.cpp',
      'Upsampler.cpp',
      'Vgm_Core.cpp',
      'Vgm_Emu.cpp',
      'ym2151.c',
      'Ym2151_Emu.cpp',
      'Ym2203_Emu.cpp',
      'ym2413.c',
      'Ym2413_Emu.cpp',
      'Ym2608_Emu.cpp',
      'Ym2610b_Emu.cpp',
      'Ym2612_Emu.cpp',
      // 'Ym2612_Emu_MAME.cpp',
      // 'Ym2612_Emu_Gens.cpp',
      'Ym3812_Emu.cpp',
      'ymdeltat.cpp',
      'Ymf262_Emu.cpp',
      'ymz280b.c',
      'Ymz280b_Emu.cpp',
      'Z80_Cpu.cpp',
    ].map(file => 'game-music-emu/gme/' + file),
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
      '_gme_seek_scaled',
      '_gme_tell_scaled',
      '_gme_set_fade',
      '_gme_voice_name',
    ],
    flags: [
      '-DVGM_YM2612_MAME=1',     // fast and accurate, but suffers on some GYM files
      // '-DVGM_YM2612_NUKED=1', // slow but very accurate
      // '-DVGM_YM2612_GENS=1',  // very fast but inaccurate
      '-DHAVE_ZLIB_H',           // used by game_music_emu for vgz and lazyusf2 for psf
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
    /*
    TODO: implement libvgm.
    Wait for libvgm player to get a C interface.
    https://github.com/ValleyBell/libvgm/blob/master/player/playera.hpp
    Or, use WebIDL Binder or Embind to interact with libvgm C++ player class.
    https://emscripten.org/docs/porting/connecting_cpp_and_javascript/WebIDL-Binder.html#a-quick-example
    https://emscripten.org/docs/porting/connecting_cpp_and_javascript/WebIDL-Binder.html#webidl-binder-type-name
    */
    name: 'libvgm',
    enabled: false,
    sourceFiles: [
      '../libvgm/build2/bin/libvgm-emu.a',
    ],
    exportedFunctions: [],
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
      '_fluid_synth_sfload',
      '_fluid_synth_noteon',
      '_fluid_synth_noteoff',
      '_fluid_synth_all_notes_off',
      '_fluid_synth_all_sounds_off',
      '_fluid_synth_write_float',
      '_fluid_synth_set_reverb',
      '_fluid_synth_get_polyphony',
      '_fluid_synth_set_polyphony',
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
  'allocate',
  'ccall',
  'getValue',
  'setValue',
];
const exportedFns = [].concat(...chipModules.filter(m => m.enabled).map(m => m.exportedFunctions));
const sourceFiles = [].concat(...chipModules.filter(m => m.enabled).map(m => m.sourceFiles));
const moduleFlags = [].concat(...chipModules.filter(m => m.enabled).map(m => m.flags));

const flags = [
  /*
  Build flags for Emscripten 1.39.11. Last updated March 22, 2020
  */
  // '--closure', '1',       // causes TypeError: lib.FS.mkdir is not a function
  // '--llvm-lto', '3',
  // '--clear-cache',        // sometimes Emscripten cache gets "poisoned"
  '--no-heap-copy',
  '-s', 'EXPORTED_FUNCTIONS=[' + exportedFns.join(',') + ']',
  '-s', 'EXPORTED_RUNTIME_METHODS=[' + runtimeMethods.join(',') + ']',
  '-s', 'ALLOW_MEMORY_GROWTH=1',
  '-s', 'ASSERTIONS=0',      // assertions increase runtime size about 100K
  '-s', 'MODULARIZE=1',
  '-s', 'EXPORT_NAME=CHIP_CORE',
  '-s', 'ENVIRONMENT=web',
  '-s', 'USE_ZLIB=1',
  '-s', 'EXPORT_ES6=1',
  '-s', 'USE_ES6_IMPORT_META=0',
  '-lidbfs.js',
  '-Os',                     // set to O0 for fast compile during development
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
