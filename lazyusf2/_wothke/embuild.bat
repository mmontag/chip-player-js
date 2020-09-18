::  POOR MAN'S DOS PROMPT BUILD SCRIPT.. make sure to delete the respective built/*.bc files before building!
::  existing *.bc files will not be recompiled.

:: EMSCRIPTEN does no allow use of the features that might elsewhere be activated using -DNEW_DYNAREC -DDYNAREC -DARCH_MIN_SSE2
:: the respective "dynarec" specific files have therefore completely been trown out of this project..
setlocal enabledelayedexpansion

SET ERRORLEVEL
VERIFY > NUL

:: **** use the "-s WASM" switch to compile WebAssembly output. warning: the SINGLE_FILE approach does NOT currently work in Chrome 63.. ****
set "OPT= -s WASM=1 -s ASSERTIONS=2 -s FORCE_FILESYSTEM=1 -Wcast-align -fno-strict-aliasing -s VERBOSE=0 -s SAFE_HEAP=0 -s DISABLE_EXCEPTION_CATCHING=0  -DEMU_COMPILE -DEMU_LITTLE_ENDIAN -DHAVE_STDINT_H -DNO_DEBUG_LOGS -Wno-pointer-sign -I. -I.. -I../src -I../psflib  -I../zlib  -Os -O3 "

if not exist "built/extra.bc" (
	call emcc.bat %OPT%
	../psflib/psflib.c
	../psflib/psf2fs.c
	../zlib/adler32.c
	../zlib/compress.c
	../zlib/crc32.c
	../zlib/gzio.c
	../zlib/uncompr.c
	../zlib/deflate.c
	../zlib/trees.c
	../zlib/zutil.c
	../zlib/inflate.c
	../zlib/infback.c
	../zlib/inftrees.c
	../zlib/inffast.c
	-o built/extra.bc
	IF !ERRORLEVEL! NEQ 0 goto :END
)
if not exist "built/usf.bc" (
	call emcc.bat %OPT%
	../src/r4300/empty_dynarec.c
	../src/ai/ai_controller.c
	../src/api/callbacks.c
	../src/debugger/dbg_decoder.c
	../src/main/main.c
	../src/main/rom.c
	../src/main/savestates.c
	../src/main/util.c
	../src/memory/memory.c
	../src/pi/cart_rom.c
	../src/pi/pi_controller.c
	../src/r4300/cached_interp.c
	../src/r4300/cp0.c
	../src/r4300/cp1.c
	../src/r4300/exception.c
	../src/r4300/interupt.c
	../src/r4300/mi_controller.c
	../src/r4300/pure_interp.c
	../src/r4300/r4300.c
	../src/r4300/r4300_core.c
	../src/r4300/recomp.c
	../src/r4300/reset.c
	../src/r4300/tlb.c
	../src/rdp/rdp_core.c
	../src/ri/rdram.c
	../src/ri/rdram_detection_hack.c
	../src/ri/ri_controller.c
	../src/rsp/rsp_core.c
	../src/rsp_hle/alist.c
	../src/rsp_hle/alist_audio.c
	../src/rsp_hle/alist_naudio.c
	../src/rsp_hle/alist_nead.c
	../src/rsp_hle/audio.c
	../src/rsp_hle/cicx105.c
	../src/rsp_hle/hle.c
	../src/rsp_hle/jpeg.c
	../src/rsp_hle/memory.c
	../src/rsp_hle/mp3.c
	../src/rsp_hle/musyx.c
	../src/rsp_hle/plugin.c
	../src/rsp_lle/rsp.c
	../src/si/cic.c
	../src/si/game_controller.c
	../src/si/n64_cic_nus_6105.c
	../src/si/pif.c
	../src/si/si_controller.c
	../src/usf/usf.c
	../src/usf/barray.c
	../src/usf/resampler.c
	../src/vi/vi_controller.c
	-o built/usf.bc
	IF !ERRORLEVEL! NEQ 0 goto :END
)

call emcc.bat %OPT%
 built/extra.bc
 built/usf.bc
 n64plug.cpp
 adapter.cpp

 -s TOTAL_MEMORY=134217728
 --memory-init-file 0
 --closure 1
 --llvm-lto 1
 --js-library callback.js
 -s EXPORTED_FUNCTIONS="['_emu_setup', '_emu_init','_emu_teardown','_emu_get_current_position','_emu_seek_position','_emu_get_max_position','_emu_set_subsong','_emu_get_track_info','_emu_get_sample_rate','_emu_get_audio_buffer','_emu_get_audio_buffer_length','_emu_compute_audio_samples', '_malloc', '_free']"
 -o htdocs/n64.js
 -s SINGLE_FILE=0
 -s EXTRA_EXPORTED_RUNTIME_METHODS="['ccall', 'Pointer_stringify']"
 -s BINARYEN_ASYNC_COMPILATION=1
 -s BINARYEN_TRAP_MODE='clamp'
 && copy /b shell-pre.js + htdocs\n64.js + shell-post.js htdocs\web_n643.js
 && del htdocs\n64.js
 && copy /b htdocs\web_n643.js + n64_adapter.js htdocs\backend_n64.js
 && del htdocs\web_n643.js

:END
