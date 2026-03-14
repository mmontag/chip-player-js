cmake_minimum_required(VERSION 3.14)

project(
    lazyusf2
    VERSION 0.1.0
    DESCRIPTION "N64 sound emulator library"
    HOMEPAGE_URL "https://bitbucket.org/losnoco/lazyusf2"
    LANGUAGES C
)

set(target lazyusf2)

# 1. Define common sources
set(SOURCES
    ai/ai_controller.c
    api/callbacks.c
    main/main.c
    main/rom.c
    main/savestates.c
    main/util.c
    memory/memory.c
    pi/cart_rom.c
    pi/pi_controller.c
    r4300/cached_interp.c
    r4300/cp0.c
    r4300/cp1.c
    r4300/exception.c
    r4300/interupt.c
    r4300/mi_controller.c
    r4300/pure_interp.c
    r4300/r4300.c
    r4300/r4300_core.c
    r4300/recomp.c
    r4300/reset.c
    r4300/tlb.c
    rdp/rdp_core.c
    ri/rdram.c
    ri/rdram_detection_hack.c
    ri/ri_controller.c
    rsp/rsp_core.c
    rsp_hle/alist.c
    rsp_hle/alist_audio.c
    rsp_hle/alist_naudio.c
    rsp_hle/alist_nead.c
    rsp_hle/audio.c
    rsp_hle/cicx105.c
    rsp_hle/hle.c
    rsp_hle/hvqm.c
    rsp_hle/jpeg.c
    rsp_hle/memory.c
    rsp_hle/mp3.c
    rsp_hle/musyx.c
    rsp_hle/plugin.c
    rsp_hle/re2.c
    rsp_lle/rsp.c
    si/cic.c
    si/game_controller.c
    si/n64_cic_nus_6105.c
    si/pif.c
    si/si_controller.c
    usf/usf.c
    usf/barray.c
    usf/resampler.c
    vi/vi_controller.c
)

# 2. Create the library with common sources
add_library(${target} STATIC ${SOURCES})
target_include_directories(${target} PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")

# 3. Apply platform-specific adjustments in a single block
if(EMSCRIPTEN)
    target_sources(${target} PRIVATE r4300/empty_dynarec.c)
    target_compile_definitions(${target} PRIVATE NDEBUG)
    target_compile_options(${target} PRIVATE -Oz -flto -UDEBUG_INFO)
else()
    target_compile_definitions(${target} PRIVATE DYNAREC ARCH_MIN_SSE2)
    target_sources(${target} PRIVATE
        debugger/dbg_decoder.c
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
endif()
