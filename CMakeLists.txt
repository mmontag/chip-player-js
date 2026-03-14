cmake_minimum_required(VERSION 3.14)

project(
    lazyusf2
    VERSION 0.1.0
    DESCRIPTION "N64 sound emulator library"
    HOMEPAGE_URL "https://bitbucket.org/losnoco/lazyusf2"
    LANGUAGES C
)
set(target lazyusf2)
add_library(${target} STATIC ${sources})

target_include_directories(${target} PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")

target_compile_definitions(${target} PRIVATE DYNAREC ARCH_MIN_SSE2)

target_sources(${target}
    PRIVATE
        ai/ai_controller.c
        ai/ai_controller.h
        api/callbacks.c
        api/callbacks.h
        debugger/dbg_decoder.c
        debugger/dbg_decoder.h
        main/main.c
        main/main.h
        main/rom.c
        main/rom.h
        main/savestates.c
        main/savestates.h
        main/util.c
        main/util.h
        main/version.h
        memory/memory.c
        memory/memory.h
        pi/cart_rom.c
        pi/cart_rom.h
        pi/pi_controller.c
        pi/pi_controller.h
        r4300/cached_interp.c
        r4300/cached_interp.h
        r4300/cp0.c
        r4300/cp0.h
        r4300/cp1.c
        r4300/cp1.h
        r4300/exception.c
        r4300/exception.h
        r4300/interupt.c
        r4300/interupt.h
        r4300/mi_controller.c
        r4300/mi_controller.h
        r4300/pure_interp.c
        r4300/pure_interp.h
        r4300/r4300.c
        r4300/r4300.h
        r4300/r4300_core.c
        r4300/r4300_core.h
        r4300/recomp.c
        r4300/recomp.h
        r4300/recomph.h
        r4300/reset.c
        r4300/reset.h
        r4300/tlb.c
        r4300/tlb.h
        rdp/rdp_core.c
        rdp/rdp_core.h
        ri/rdram.c
        ri/rdram.h
        ri/rdram_detection_hack.c
        ri/rdram_detection_hack.h
        ri/ri_controller.c
        ri/ri_controller.h
        rsp/rsp_core.c
        rsp/rsp_core.h
        rsp_hle/alist.c
        rsp_hle/alist.h
        rsp_hle/alist_audio.c
        rsp_hle/alist_naudio.c
        rsp_hle/alist_nead.c
        rsp_hle/audio.c
        rsp_hle/audio.h
        rsp_hle/cicx105.c
        rsp_hle/common.h
        rsp_hle/hle.c
        rsp_hle/hle.h
        rsp_hle/hle_external.h
        rsp_hle/hle_internal.h
        rsp_hle/hvqm.c
        rsp_hle/jpeg.c
        rsp_hle/memory.c
        rsp_hle/memory.h
        rsp_hle/mp3.c
        rsp_hle/musyx.c
        rsp_hle/plugin.c
        rsp_hle/re2.c
        rsp_hle/ucodes.h
        rsp_lle/rsp.c
        rsp_lle/config.h
        rsp_lle/execute.h
        rsp_lle/matrix.h
        rsp_lle/rsp_lle.h
        rsp_lle/rsp.h
        rsp_lle/su.h
        si/cic.c
        si/cic.h
        si/game_controller.c
        si/game_controller.h
        si/n64_cic_nus_6105.c
        si/n64_cic_nus_6105.h
        si/pif.c
        si/pif.h
        si/si_controller.c
        si/si_controller.h
        usf/usf.c
        usf/usf.h
        usf/usf_internal.h
        usf/barray.c
        usf/barray.h
        usf/resampler.c
        usf/resampler.h
        vi/vi_controller.c
        vi/vi_controller.h

        r4300/x86_64/assemble.c
        r4300/x86_64/gbc.c
        r4300/x86_64/gcop0.c
        r4300/x86_64/gcop1.c
        r4300/x86_64/gcop1_d.c
        r4300/x86_64/gcop1_l.c
        r4300/x86_64/gcop1_s.c
        r4300/x86_64/gcop1_w.c
        r4300/x86_64/gr4300.c
        r4300/x86_64/gregimm.c
        r4300/x86_64/gspecial.c
        r4300/x86_64/gtlb.c
        r4300/x86_64/regcache.c
        r4300/x86_64/rjump.c
)
