#### Audio Output Library ####
project(vgm-audio)

set(AUDIO_DEFS)
set(AUDIO_FILES
	AudioStream.c
)
# export headers
set(AUDIO_HEADERS
	AudioStructs.h
	AudioStream.h
	AudioStream_SpcDrvFuns.h
)
set(AUDIO_INCLUDES)
set(AUDIO_LIBS)
set(AUDIO_DEPS)	# dependencies for CMake Config file

set(AUDIO_PC_CFLAGS)
set(AUDIO_PC_LDFLAGS)
set(AUDIO_PC_REQUIRES)


if(WIN32)
	set(ADRV_WIN_ALL TRUE)
	if(NOT MSVC OR MSVC_VERSION LESS 1400)
		# WASAPI doesn't work with VC6 or MinGW due to a lack of Windows Vista/7 header files
		set(ADRV_WIN_VISTA FALSE)
	else()
		set(ADRV_WIN_VISTA TRUE)
	endif()
	set(ADRV_LINUX FALSE)
	set(ADRV_APPLE FALSE)
	set(ADRV_NO_WIN FALSE)
else()
	set(ADRV_WIN_ALL FALSE)
	set(ADRV_WIN_VISTA FALSE)
	if(UNIX AND NOT APPLE)
		set(ADRV_LINUX TRUE)
		set(ADRV_APPLE FALSE)
	else()
		set(ADRV_APPLE TRUE)
		set(ADRV_LINUX FALSE)
	endif()
	set(ADRV_NO_WIN TRUE)
	
	find_package(ALSA QUIET)
	find_package(PulseAudio QUIET)
endif()
find_package(LibAO QUIET)

option(AUDIODRV_WAVEWRITE "Audio Driver: Wave Writer" ON)

option(AUDIODRV_WINMM "Audio Driver: WinMM [Windows]" ${ADRV_WIN_ALL})
option(AUDIODRV_DSOUND "Audio Driver: DirectSound [Windows]" ${ADRV_WIN_ALL})
option(AUDIODRV_XAUDIO2 "Audio Driver: XAudio2 [Windows]" ${ADRV_WIN_ALL})
option(AUDIODRV_WASAPI "Audio Driver: WASAPI [Windows]" ${ADRV_WIN_VISTA})

option(AUDIODRV_OSS "Audio Driver: OSS (Open Sound System) [Linux]" OFF)
option(AUDIODRV_SADA "Audio Driver: SADA (Solaris Audio Device Architecture) [Solaris/BSD]" OFF)
option(AUDIODRV_ALSA "Audio Driver: ALSA (Advanced Linux Sound Architecture) [Linux]" ${ALSA_FOUND})
option(AUDIODRV_PULSE "Audio Driver: PulseAudio [Linux]" ${PULSEAUDIO_FOUND})
option(AUDIODRV_APPLE "Audio Driver: Core Audio [macOS]" ${ADRV_APPLE})
if(LINK_STATIC_LIBS)
	# linking errors when using static library in MinGW
	option(AUDIODRV_LIBAO "Audio Driver: libao" OFF)
else()
	option(AUDIODRV_LIBAO "Audio Driver: libao" ${LIBAO_FOUND})
endif()

if(AUDIODRV_WAVEWRITE)
	set(AUDIO_DEFS ${AUDIO_DEFS} " AUDDRV_WAVEWRITE")
	set(AUDIO_FILES ${AUDIO_FILES} AudDrv_WaveWriter.c)
endif()

if(AUDIODRV_WINMM)
	set(AUDIO_DEFS ${AUDIO_DEFS} " AUDDRV_WINMM")
	set(AUDIO_FILES ${AUDIO_FILES} AudDrv_WinMM.c)
	set(AUDIO_LIBS ${AUDIO_LIBS} winmm)
	set(AUDIO_PC_LDFLAGS ${AUDIO_PC_LDFLAGS} "-lwinmm")
endif()

if(AUDIODRV_DSOUND)
	set(AUDIO_DEFS ${AUDIO_DEFS} " AUDDRV_DSOUND")
	set(AUDIO_FILES ${AUDIO_FILES} AudDrv_DSound.cpp)
	set(AUDIO_LIBS ${AUDIO_LIBS} ole32 dsound uuid)
	set(AUDIO_PC_LDFLAGS ${AUDIO_PC_LDFLAGS} "-ldsound")
	set(AUDIO_PC_LDFLAGS ${AUDIO_PC_LDFLAGS} "-luuid")
endif()

if(AUDIODRV_XAUDIO2)
	set(AUDIO_DEFS ${AUDIO_DEFS} " AUDDRV_XAUD2")
	set(AUDIO_FILES ${AUDIO_FILES} AudDrv_XAudio2.cpp)
	set(AUDIO_LIBS ${AUDIO_LIBS} ole32)
	set(AUDIO_PC_LDFLAGS ${AUDIO_PC_LDFLAGS} "-lole32")
endif()

if(AUDIODRV_WASAPI)
	set(AUDIO_DEFS ${AUDIO_DEFS} " AUDDRV_WASAPI")
	set(AUDIO_FILES ${AUDIO_FILES} AudDrv_WASAPI.cpp)
	set(AUDIO_LIBS ${AUDIO_LIBS} ole32)
	set(AUDIO_PC_LDFLAGS ${AUDIO_PC_LDFLAGS} "-lole32")
endif()

if(AUDIODRV_OSS)
	set(AUDIO_DEFS ${AUDIO_DEFS} " AUDDRV_OSS")
	set(AUDIO_FILES ${AUDIO_FILES} AudDrv_OSS.c)
endif()

if(AUDIODRV_SADA)
	set(AUDIO_DEFS ${AUDIO_DEFS} " AUDDRV_SADA")
	set(AUDIO_FILES ${AUDIO_FILES} AudDrv_SADA.c)
endif()

if(AUDIODRV_ALSA)
	find_package(ALSA REQUIRED)
	set(AUDIO_DEPS ${AUDIO_DEPS} "ALSA")
	set(AUDIO_DEFS ${AUDIO_DEFS} " AUDDRV_ALSA")
	set(AUDIO_FILES ${AUDIO_FILES} AudDrv_ALSA.c)
	set(AUDIO_INCLUDES ${AUDIO_INCLUDES} ${ALSA_INCLUDE_DIRS})
	set(AUDIO_LIBS ${AUDIO_LIBS} ${ALSA_LIBRARIES})
	set(AUDIO_PC_REQUIRES ${AUDIO_PC_REQUIRES} "alsa")
endif()

if(AUDIODRV_PULSE)
	find_package(PulseAudio REQUIRED)
	set(AUDIO_DEPS ${AUDIO_DEPS} "PulseAudio")
	set(AUDIO_DEFS ${AUDIO_DEFS} " AUDDRV_PULSE")
	set(AUDIO_FILES ${AUDIO_FILES} AudDrv_Pulse.c)
	set(AUDIO_INCLUDES ${AUDIO_INCLUDES} ${PULSEAUDIO_INCLUDE_DIR})
	# There seems to be no package or variable for pulse-simple.
	set(AUDIO_LIBS ${AUDIO_LIBS} pulse-simple ${PULSEAUDIO_LIBRARY})
	set(AUDIO_PC_REQUIRES ${AUDIO_PC_REQUIRES} "libpulse-simple")
endif()

if(AUDIODRV_APPLE)
	set(AUDIO_DEFS ${AUDIO_DEFS} " AUDDRV_CA")
	set(AUDIO_FILES ${AUDIO_FILES} AudDrv_CoreAudio.c)
	set(AUDIO_LIBS ${AUDIO_LIBS} "-framework AudioToolbox")
	set(AUDIO_PC_LDFLAGS ${AUDIO_PC_LDFLAGS} "-framework AudioToolbox")
endif()

if(AUDIODRV_LIBAO)
	find_package(LibAO REQUIRED)
	set(AUDIO_DEPS ${AUDIO_DEPS} "LibAO")
	set(AUDIO_DEFS ${AUDIO_DEFS} " AUDDRV_LIBAO")
	set(AUDIO_FILES ${AUDIO_FILES} AudDrv_libao.c)
	set(AUDIO_LIBS ${AUDIO_LIBS} libao::libao)
	set(AUDIO_PC_REQUIRES ${AUDIO_PC_REQUIRES} "ao")
endif()


add_library(${PROJECT_NAME} ${LIBRARY_TYPE} ${AUDIO_FILES})
set_target_properties(${PROJECT_NAME} PROPERTIES POSITION_INDEPENDENT_CODE ON)
target_compile_definitions(${PROJECT_NAME} PUBLIC ${AUDIO_DEFS})
target_include_directories(${PROJECT_NAME}
	PUBLIC $<BUILD_INTERFACE:${LIBVGM_SOURCE_DIR}> $<INSTALL_INTERFACE:${LIBVGM_INSTALL_INCLUDE_DIR}>
	PRIVATE ${AUDIO_INCLUDES}
)
target_link_libraries(${PROJECT_NAME} PRIVATE ${AUDIO_LIBS} vgm-utils)

if(WIN32)
	# for some Windows Audio APIs, we need WinAPI headers from special directories
	if(NOT MSVC)
		target_include_directories(${PROJECT_NAME} PRIVATE
			${LIBVGM_SOURCE_DIR}/libs/include
			${LIBVGM_SOURCE_DIR}/libs/include_mingw
		)
	elseif(MSVC)
		target_include_directories(${PROJECT_NAME} PRIVATE
			${LIBVGM_SOURCE_DIR}/libs/include
			${LIBVGM_SOURCE_DIR}/libs/include_vc
		)
		if(MSVC_VERSION LESS 1400)
			target_include_directories(${PROJECT_NAME} PRIVATE
				${LIBVGM_SOURCE_DIR}/libs/include_vc6
			)
		endif()
	endif()
endif()


pkgcfg_configure("${LIBVGM_SOURCE_DIR}/cmake/libvgm.pc.in" "${CMAKE_CURRENT_BINARY_DIR}/vgm-audio.pc"
		NAME "LibVGM Audio"
		DESC "LibVGM audio output library"
		VERSION ${LIBVGM_VERSION}
		DEFINES ${AUDIO_DEFS}
		CFLAGS ${AUDIO_PC_CFLAGS}
		LDFLAGS_PRIV ${AUDIO_PC_LDFLAGS}
		LDFLAGS_PUB "-l${PROJECT_NAME}"
		PKGS_PRIV ${AUDIO_PC_REQUIRES}
		PKGS_PUB vgm-utils
	)

install(TARGETS ${PROJECT_NAME}
		EXPORT "${LIBVGM_NAME}"
		RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}"
		LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}"
		ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}"
	)
set(LIBVGM_INSTALL_TARGETS ${LIBVGM_INSTALL_TARGETS} "${PROJECT_NAME}" PARENT_SCOPE)
install(FILES ${AUDIO_HEADERS} DESTINATION "${LIBVGM_INSTALL_INCLUDE_DIR}/audio")
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/vgm-audio.pc DESTINATION "${CMAKE_INSTALL_LIBDIR}/pkgconfig")
set(LIBVGM_DEPS ${LIBVGM_DEPS} "${AUDIO_DEPS}" PARENT_SCOPE)
