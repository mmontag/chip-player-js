
ARCH := $(shell getconf LONG_BIT)

FLAGS_32 = -msse -mmmx -msse2
FLAGS_64 = -fPIC

# CFLAGS =  -c -g $(FLAGS_$(ARCH)) -I. $(OPTFLAGS)

# OBJS_RECOMPILER_32 = r4300/x86/assemble.o r4300/x86/gbc.o r4300/x86/gcop0.o r4300/x86/gcop1.o r4300/x86/gcop1_d.o r4300/x86/gcop1_l.o r4300/x86/gcop1_s.o r4300/x86/gcop1_w.o r4300/x86/gr4300.o r4300/x86/gregimm.o r4300/x86/gspecial.o r4300/x86/gtlb.o r4300/x86/regcache.o r4300/x86/rjump.o

# OBJS_RECOMPILER_64 = r4300/x86_64/assemble.o r4300/x86_64/gbc.o r4300/x86_64/gcop0.o r4300/x86_64/gcop1.o r4300/x86_64/gcop1_d.o r4300/x86_64/gcop1_l.o r4300/x86_64/gcop1_s.o r4300/x86_64/gcop1_w.o r4300/x86_64/gr4300.o r4300/x86_64/gregimm.o r4300/x86_64/gspecial.o r4300/x86_64/gtlb.o r4300/x86_64/regcache.o r4300/x86_64/rjump.o

# OBJS = ai/ai_controller.o api/callbacks.o debugger/dbg_decoder.o main/main.o main/rom.o main/savestates.o main/util.o memory/memory.o pi/cart_rom.o pi/pi_controller.o r4300/cached_interp.o r4300/cp0.o r4300/cp1.o r4300/exception.o r4300/interupt.o r4300/mi_controller.o r4300/pure_interp.o r4300/r4300.o r4300/r4300_core.o r4300/recomp.o r4300/reset.o r4300/tlb.o rdp/rdp_core.o ri/rdram.o ri/rdram_detection_hack.o ri/ri_controller.o rsp/rsp_core.o rsp_hle/alist.o rsp_hle/alist_audio.o rsp_hle/alist_naudio.o rsp_hle/alist_nead.o rsp_hle/audio.o rsp_hle/cicx105.o rsp_hle/hle.o rsp_hle/jpeg.o rsp_hle/memory.o rsp_hle/mp3.o rsp_hle/musyx.o rsp_hle/plugin.o rsp_lle/rsp.o si/cic.o si/game_controller.o si/n64_cic_nus_6105.o si/pif.o si/si_controller.o usf/usf.o usf/barray.o usf/resampler.o vi/vi_controller.o $(OBJS_RECOMPILER_$(ARCH))

# OPTS = -DDYNAREC
# ROPTS = -DARCH_MIN_SSE2

# Emscripten lazyusf2 release build
CFLAGS = -c -O3 -Wno-cast-align -fno-strict-aliasing -fno-exceptions -s VERBOSE=0 -s SAFE_HEAP=0 -DEMU_COMPILE -DEMU_LITTLE_ENDIAN -DHAVE_STDINT_H -DNO_DEBUG_LOGS -Wno-pointer-sign -I. $(OPTFLAGS)

# TODO: set up debug build with debugging flags
#
# Emscripten flags:
#
# -g4                generates source map using LLVM debug information
# -s VERBOSE=1       generates more verbose output
# --source-map-base  URL for the location where WebAssembly source maps will be published
#                    the .wasm file sourceMappingURL will be like <base-url><wasm-file-name>.map
#
# CFLAGS = -c -g4 --source-map-base http://localhost:9000/lazyusf2/ -Os -Wno-cast-align -fno-strict-aliasing -s VERBOSE=0 -s SAFE_HEAP=0 -s DISABLE_EXCEPTION_CATCHING=0  -DEMU_COMPILE -DEMU_LITTLE_ENDIAN -DHAVE_STDINT_H -DNO_DEBUG_LOGS -Wno-pointer-sign -I. $(OPTFLAGS)

OBJS = r4300/empty_dynarec.o ai/ai_controller.o api/callbacks.o debugger/dbg_decoder.o main/main.o main/rom.o main/savestates.o main/util.o memory/memory.o pi/cart_rom.o pi/pi_controller.o r4300/cached_interp.o r4300/cp0.o r4300/cp1.o r4300/exception.o r4300/interupt.o r4300/mi_controller.o r4300/pure_interp.o r4300/r4300.o r4300/r4300_core.o r4300/recomp.o r4300/reset.o r4300/tlb.o rdp/rdp_core.o ri/rdram.o ri/rdram_detection_hack.o ri/ri_controller.o rsp/rsp_core.o rsp_hle/alist.o rsp_hle/alist_audio.o rsp_hle/alist_naudio.o rsp_hle/alist_nead.o rsp_hle/audio.o rsp_hle/cicx105.o rsp_hle/hle.o rsp_hle/jpeg.o rsp_hle/memory.o rsp_hle/mp3.o rsp_hle/musyx.o rsp_hle/plugin.o rsp_lle/rsp.o si/cic.o si/game_controller.o si/n64_cic_nus_6105.o si/pif.o si/si_controller.o usf/usf.o usf/barray.o usf/resampler.o vi/vi_controller.o

all: liblazyusf.a bench dumpresampled

liblazyusf.a : $(OBJS) usf/usf_internal.h
	$(AR) rcs $@ $^

liblazyusf.so: $(OBJS)
	$(CC) $^ -shared -Wl,-soname -Wl,$@.2 -o $@.2.0

bench : test/bench.o liblazyusf.a
	$(CC) -o $@ $^ ../psflib/libpsflib.a -lz -lm

dumpresampled : test/dumpresampled.o liblazyusf.a
	$(CC) -o $@ $^ ../psflib/libpsflib.a -lz -lm

.c.o:
	$(CC) $(CFLAGS) $(OPTS) -o $@ $*.c

rsp_lle/rsp.o: rsp_lle/rsp.c
	$(CC) $(CFLAGS) $(ROPTS) -o $@ $^

test/bench.o: test/bench.c
	$(CC) $(CFLAGS) $(OPTS) -I../psflib -o $@ $^

test/dumpresampled.o: test/dumpresampled.c
	$(CC) $(CFLAGS) $(OPTS) -I../psflib -o $@ $^

clean:
	rm -f $(OBJS) liblazyusf.a liblazyusf.so* test/bench.o bench test/dumpresampled.o dumpresampled > /dev/null
