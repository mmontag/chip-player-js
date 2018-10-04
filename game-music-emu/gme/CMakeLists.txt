#include_directories(../fex)
add_definitions(-DHAVE_STDINT_H)
include_directories(~/src/emsdk/emscripten/1.38.11/system/include/emscripten)

# was only used while trying to find things that still needed linking in the
# port back to CMake

#SET ( CMAKE_CXX_FLAGS "-Wl,-z,defs" CACHE STRING "compile flags" FORCE)
# List of source files required by libgme and any emulators
# This is not 100% accurate (Fir_Resampler for instance) but
# you'll be OK.
set(libgme_SRCS Blip_Buffer.cpp
                Classic_Emu.cpp
                Data_Reader.cpp
                Dual_Resampler.cpp
                Effects_Buffer.cpp
                Fir_Resampler.cpp
                Resampler.cpp
                Rom_Data.cpp
                gme.cpp
                Gme_Loader.cpp
                Gme_File.cpp
                M3u_Playlist.cpp
                Multi_Buffer.cpp
                Music_Emu.cpp
                blargg_errors.cpp
                blargg_common.cpp
                Track_Filter.cpp
                )

# static builds need to find static zlib (and static forms of other needed
# libraries.  Ensure CMake looks only for static libs if we're doing a static
# build.  See https://stackoverflow.com/a/44738756
if(NOT BUILD_SHARED_LIBS)
    set(CMAKE_FIND_LIBRARY_SUFFIXES ".a")
endif()

find_package(ZLIB QUIET)

# Ay_Apu is very popular around here
if (USE_GME_AY OR USE_GME_KSS)
    set(libgme_SRCS ${libgme_SRCS}
                Ay_Apu.cpp
                Ay_Core.cpp
        )
endif()

# so is Ym2612_Emu
if (USE_GME_VGM OR USE_GME_GYM)
    set(libgme_SRCS ${libgme_SRCS}
#                Ym2612_Emu.cpp
                fm2612.c
                fm.c
                fmopl.cpp
                ymdeltat.cpp
                Ym2612_Emu_MAME.cpp
                Ymf262_Emu.cpp
                dbopl.cpp
                # not sure this is the optimal location for this:
                # it might belong in some special "OPL" definition or something
                Ym3812_Emu.cpp
                Ym2610b_Emu.cpp
                Ym2608_Emu.cpp
                # these two probably should really have a special case, since
                # I know this is used in the NES VRC7 chip as well as the
                # Japanese SMS/Mark III FM module
                Ym2413_Emu.cpp
                ym2413.c
                Sms_Fm_Apu.cpp
                Ym2151_Emu.cpp
                ym2151.c
                Ym2203_Emu.cpp
                Ymz280b_Emu.cpp
                ymz280b.c
# done in MAME
#                Ym2612_Emu.cpp
                Z80_Cpu.cpp
                # Extremely unsure this goes here
                SegaPcm_Emu.cpp
                segapcm.c
        )
endif()

# Less accurate than the other implementation, only one should be defined at a time.
if (USE_GENS)
  set(libgme_SRCS ${libgme_SRCS}
                Ym2612_Emu_Gens.cpp
    )
endif()

# But none are as popular as Sms_Apu
if (USE_GME_VGM OR USE_GME_GYM OR USE_GME_KSS)
    set(libgme_SRCS ${libgme_SRCS}
                Sms_Apu.cpp
        )
endif()

if (USE_GME_AY)
    set(libgme_SRCS ${libgme_SRCS}
              # Ay_Apu.cpp included earlier
                Ay_Cpu.cpp
                Ay_Emu.cpp
        )
endif()

if (USE_GME_GBS)
    set(libgme_SRCS ${libgme_SRCS}
                Gb_Apu.cpp
                Gb_Cpu.cpp
                Gbs_Cpu.cpp
                Gb_Oscs.cpp
                Gbs_Emu.cpp
                Gbs_Core.cpp
        )
endif()

if (USE_GME_GYM)
    set(libgme_SRCS ${libgme_SRCS}
              # Sms_Apu.cpp included earlier
              # Ym2612_Emu.cpp included earlier
                Gym_Emu.cpp
        )
endif()

if (USE_GME_HES)
    set(libgme_SRCS ${libgme_SRCS}
                Hes_Apu.cpp
                Hes_Cpu.cpp
                Hes_Emu.cpp
                Hes_Core.cpp
        )
endif()

if (USE_GME_KSS)
    set(libgme_SRCS ${libgme_SRCS}
              # Ay_Apu.cpp included earlier
              # Sms_Apu.cpp included earlier
                Kss_Cpu.cpp
                Kss_Emu.cpp
                Kss_Scc_Apu.cpp
                Kss_Core.cpp
        )
endif()

if (USE_GME_NSF OR USE_GME_NSFE)
    set(libgme_SRCS ${libgme_SRCS}
                Nes_Apu.cpp
                Nes_Cpu.cpp
                Nes_Fme7_Apu.cpp
                Nes_Namco_Apu.cpp
                Nes_Oscs.cpp
                Nes_Vrc6_Apu.cpp
                Nsf_Emu.cpp
                Nsf_Core.cpp
                Nsf_Impl.cpp
                Nsf_Cpu.cpp
                Nes_Fds_Apu.cpp
                Nes_Vrc7_Apu.cpp
        )
endif()

if (USE_GME_NSFE)
    set(libgme_SRCS ${libgme_SRCS}
                Nsfe_Emu.cpp
        )
endif()

if (USE_GME_SAP)
    set(libgme_SRCS ${libgme_SRCS}
                Sap_Apu.cpp
                Sap_Cpu.cpp
                Sap_Emu.cpp
        )
endif()

if (USE_GME_SPC)
    set(libgme_SRCS ${libgme_SRCS}
                Spc_Emu.h
                Spc_Emu.cpp
                Spc_Filter.cpp
                Spc_Sfm.cpp
                higan/smp/smp.cpp
                higan/dsp/dsp.cpp
                higan/dsp/SPC_DSP.cpp
                higan/smp/timing.cpp
                higan/smp/memory.cpp
                higan/processor/spc700/spc700.cpp
#                higan/processor/spc700/algorithms.cpp
#                higan/processor/spc700/disassembler.cpp
#                higan/processor/spc700/instructions.cpp
                Bml_Parser.cpp
        )
endif()

if (USE_GME_VGM)
    set(libgme_SRCS ${libgme_SRCS}
              # Sms_Apu.cpp included earlier
              # Ym2612_Emu.cpp included earlier
                Vgm_Emu.cpp
                Vgm_Core.cpp
                dac_control.c
#                Vgm_Emu.cpp
#                Vgm_Emu_Impl.cpp
#                Ym2413_Emu.cpp
        )
endif()

# ADDED ON LATER:
# SGC - Hope I'm doing this right
if (USE_GME_SGC)
    set(libgme_SRCS ${libgme_SRCS}
                Sgc_Emu.cpp
                Sgc_Cpu.cpp
                Sgc_Impl.cpp
                Sgc_Core.cpp
        )
endif()

# MISC: SHOULD REALLY GO IN THEIR OWN SECTION, BUT I AM TIRED OF FIXING THIS CRAP:
set(libgme_SRCS ${libgme_SRCS}
  #C140 Namco System 2
  C140_Emu.cpp
  c140.c
  #Other
  # sega CD I think?
  Rf5C68_Emu.cpp
  rf5c68.c
  scd_pcm.c
  Rf5C164_Emu.cpp
  
  Pwm_Emu.cpp
  pwm.c
  
  Okim6258_Emu.cpp
  okim6258.c
  Okim6295_Emu.cpp
  okim6295.c
  
  K051649_Emu.cpp
  k051649.c
  
  K053260_Emu.cpp
  k053260.c
  
  K054539_Emu.cpp
  k054539.c
  
  Qsound_Apu.cpp
  qmix.c
  
  # And the ones that still escaped my attention
  # (discovered via some shell script magic):
  Downsampler.cpp
  Hes_Apu_Adpcm.cpp
  Opl_Apu.cpp
  Sap_Core.cpp
  Upsampler.cpp
  )

# These headers are part of the generic gme interface.
set (EXPORTED_HEADERS gme.h)

# On some platforms we may need to change headers or whatnot based on whether
# we're building the library or merely using the library. The following is
# only defined when building the library to allow us to tell which is which.
add_definitions(-DBLARGG_BUILD_DLL)

# For the gme_types.h
include_directories(${CMAKE_CURRENT_BINARY_DIR})

# Add library to be compiled.
add_library(gme ${libgme_SRCS})

if(ZLIB_FOUND)
    message(" ** ZLib library located, compressed file formats will be supported")
    target_compile_definitions(gme PRIVATE -DHAVE_ZLIB_H)
    target_include_directories(gme PRIVATE ${ZLIB_INCLUDE_DIRS})
    target_link_libraries(gme ${ZLIB_LIBRARIES})
    # Is not to be installed though

    set(PKG_CONFIG_ZLIB -lz) # evaluated in libgme.pc.in
else()
    message("ZLib library not found, disabling support for compressed formats such as VGZ")
endif()

# The version is the release.  The "soversion" is the API version.  As long
# as only build fixes are performed (i.e. no backwards-incompatible changes
# to the API), the SOVERSION should be the same even when bumping up VERSION.
# The way gme.h is designed, SOVERSION should very rarely be bumped, if ever.
# Hopefully the API can stay compatible with old versions.
set_target_properties(gme
    PROPERTIES VERSION ${GME_VERSION}
               SOVERSION 0)

install(TARGETS gme LIBRARY DESTINATION lib${LIB_SUFFIX}
                    RUNTIME DESTINATION bin  # DLL platforms
                    ARCHIVE DESTINATION lib) # DLL platforms

# Run during cmake phase, so this is available during make
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/gme_types.h.in
    ${CMAKE_CURRENT_BINARY_DIR}/gme_types.h)

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/libgme.pc.in
    ${CMAKE_CURRENT_BINARY_DIR}/libgme.pc @ONLY)

install(FILES ${EXPORTED_HEADERS} DESTINATION include/gme)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/libgme.pc DESTINATION lib${LIB_SUFFIX}/pkgconfig)

