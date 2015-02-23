CFLAGS = -c -fPIC

OBJS = ai/ai_controller.o api/callbacks.o main/main.o main/rom.o main/savestates.o main/util.o memory/memory.o pi/cart_rom.o pi/pi_controller.o r4300/cached_interp.o r4300/cp0.o r4300/cp1.o r4300/exception.o r4300/interupt.o r4300/mi_controller.o r4300/pure_interp.o r4300/r4300.o r4300/r4300_core.o r4300/recomp.o r4300/reset.o r4300/tlb.o r4300/x86_64/assemble.o r4300/x86_64/gbc.o r4300/x86_64/gcop0.o r4300/x86_64/gcop1.o r4300/x86_64/gcop1_d.o r4300/x86_64/gcop1_l.o r4300/x86_64/gcop1_s.o r4300/x86_64/gcop1_w.o r4300/x86_64/gr4300.o r4300/x86_64/gregimm.o r4300/x86_64/gspecial.o r4300/x86_64/gtlb.o r4300/x86_64/regcache.o r4300/x86_64/rjump.o rdp/rdp_core.o ri/rdram.o ri/rdram_detection_hack.o ri/ri_controller.o rsp/rsp_core.o rsp_hle/alist.o rsp_hle/alist_audio.o rsp_hle/alist_naudio.o rsp_hle/alist_nead.o rsp_hle/audio.o rsp_hle/cicx105.o rsp_hle/hle.o rsp_hle/jpeg.o rsp_hle/memory.o rsp_hle/mp3.o rsp_hle/musyx.o rsp_hle/plugin.o rsp_lle/rsp.o si/cic.o si/game_controller.o si/n64_cic_nus_6105.o si/pif.o si/si_controller.o usf/usf.o usf/resampler.o vi/vi_controller.o

OPTS = -O3 -DDYNAREC -I.
ROPTS = -O3 -DARCH_MIN_SSE2 -I.

all: liblazyusf.a

liblazyusf.a : $(OBJS)
	$(AR) rcs $@ $^

.c.o:
	$(CC) $(CFLAGS) $(OPTS) -o $@ $*.c

rsp_lle/rsp.o: rsp_lle/rsp.c
	$(CC) $(CFLAGS) $(ROPTS) -o $@ $^

clean:
	rm -f $(OBJS) liblazyusf.a > /dev/null

