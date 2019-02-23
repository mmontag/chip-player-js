#### Audio Output Library ####
project(vgm-audio)
cmake_minimum_required(VERSION 2.8)

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

if(WIN32)
	set(ADRV_WIN_ALL TRUE)
	if(NOT MSVC OR MSVC_VERSION LESS 1400)
		# WASAPI doesn't work with VC6 or MinGW due to a lack of Windows Vista/7 header files
		set(ADRV_WIN_VISTA FALSE)
	else()
		set(ADRV_WIN_VISTA TRUE)
	endif()
	set(ADRV_LINUX FALSE)
	set(ADRV_NO_WIN FALSE)
else()
	set(ADRV_WIN_ALL FALSE)
	set(ADRV_WIN_VISTA FALSE)
	if(UNIX AND NOT APPLE)
		set(ADRV_LINUX TRUE)
	else()
		set(ADRV_LINUX FALSE)
	endif()
	set(ADRV_NO_WIN TRUE)
	
	find_package(ALSA QUIET)
	find_package(PulseAudio QUIET)
endif()

option(AUDIODRV_WAVEWRITE "Audio Driver: Wave Writer" ON)

option(AUDIODRV_WINMM "Audio Driver: WinMM [Windows]" ${ADRV_WIN_ALL})
option(AUDIODRV_DSOUND "Audio Driver: DirectSound [Windows]" ${ADRV_WIN_ALL})
option(AUDIODRV_XAUDIO2 "Audio Driver: XAudio2 [Windows]" ${ADRV_WIN_ALL})
option(AUDIODRV_WASAPI "Audio Driver: WASAPI [Windows]" ${ADRV_WIN_VISTA})

option(AUDIODRV_OSS "Audio Driver: OSS (Open Sound System) [Linux]" ${ADRV_LINUX})
option(AUDIODRV_SADA "Audio Driver: SADA (Solaris Audio Device Architecture) [Solaris/BSD]" OFF)
option(AUDIODRV_ALSA "Audio Driver: ALSA (Advanced Linux Sound Architecture) [Linux]" ${ALSA_FOUND})
option(AUDIODRV_PULSE "Audio Driver: PulseAudio [Linux]" ${PULSEAUDIO_FOUND})
option(AUDIODRV_LIBAO "Audio Driver: libao" ${ADRV_NO_WIN})


if(AUDIODRV_WAVEWRITE)
	set(AUDIO_DEFS ${AUDIO_DEFS} " AUDDRV_WAVEWRITE")
	set(AUDIO_FILES ${AUDIO_FILES} AudDrv_WaveWriter.c)
endif()

if(AUDIODRV_WINMM)
	set(AUDIO_DEFS ${AUDIO_DEFS} " AUDDRV_WINMM")
	set(AUDIO_FILES ${AUDIO_FILES} AudDrv_WinMM.c)
	set(AUDIO_LIBS ${AUDIO_LIBS} winmm)
endif()

if(AUDIODRV_DSOUND)
	set(AUDIO_DEFS ${AUDIO_DEFS} " AUDDRV_DSOUND")
	set(AUDIO_FILES ${AUDIO_FILES} AudDrv_DSound.cpp)
	set(AUDIO_LIBS ${AUDIO_LIBS} dsound uuid)
endif()

if(AUDIODRV_XAUDIO2)
	set(AUDIO_DEFS ${AUDIO_DEFS} " AUDDRV_XAUD2")
	set(AUDIO_FILES ${AUDIO_FILES} AudDrv_XAudio2.cpp)
	set(AUDIO_LIBS ${AUDIO_LIBS} ole32)
endif()

if(AUDIODRV_WASAPI)
	set(AUDIO_DEFS ${AUDIO_DEFS} " AUDDRV_WASAPI")
	set(AUDIO_FILES ${AUDIO_FILES} AudDrv_WASAPI.cpp)
	set(AUDIO_LIBS ${AUDIO_LIBS} ole32)
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
	set(AUDIO_DEFS ${AUDIO_DEFS} " AUDDRV_ALSA")
	set(AUDIO_FILES ${AUDIO_FILES} AudDrv_ALSA.c)
	set(AUDIO_INCLUDES ${AUDIO_INCLUDES} ${ALSA_INCLUDE_DIRS})
	set(AUDIO_LIBS ${AUDIO_LIBS} ${ALSA_LIBRARIES})
endif()

if(AUDIODRV_PULSE)
	find_package(PulseAudio REQUIRED)
	set(AUDIO_DEFS ${AUDIO_DEFS} " AUDDRV_PULSE")
	set(AUDIO_FILES ${AUDIO_FILES} AudDrv_Pulse.c)
	set(AUDIO_INCLUDES ${AUDIO_INCLUDES} ${PULSEAUDIO_INCLUDE_DIR})
	# There seems to be no package or variable for pulse-simple.
	set(AUDIO_LIBS ${AUDIO_LIBS} pulse-simple ${PULSEAUDIO_LIBRARY})
endif()

if(AUDIODRV_LIBAO)
	set(AUDIO_DEFS ${AUDIO_DEFS} " AUDDRV_LIBAO")
	set(AUDIO_FILES ${AUDIO_FILES} AudDrv_libao.c)
	set(AUDIO_LIBS ${AUDIO_LIBS} ao)
endif()


add_library(${PROJECT_NAME} STATIC ${AUDIO_FILES})

target_compile_definitions(${PROJECT_NAME} PUBLIC ${AUDIO_DEFS})

target_include_directories(${PROJECT_NAME} PUBLIC
	${CMAKE_SOURCE_DIR}
	${CMAKE_SOURCE_DIR}/libs/include
)
if(WIN32 AND NOT MSVC)
	target_include_directories(${PROJECT_NAME} PRIVATE
		${CMAKE_SOURCE_DIR}/libs/include_mingw
	)
elseif(MSVC)
	target_include_directories(${PROJECT_NAME} PRIVATE
		${CMAKE_SOURCE_DIR}/libs/include_vc
	)
	if(MSVC_VERSION LESS 1400)
		target_include_directories(${PROJECT_NAME} PRIVATE
			${CMAKE_SOURCE_DIR}/libs/include_vc6
		)
	endif()
endif()
target_include_directories(${PROJECT_NAME} PUBLIC ${AUDIO_INCLUDES})

target_link_libraries(${PROJECT_NAME} PRIVATE ${AUDIO_LIBS} vgm-utils)

install(TARGETS ${PROJECT_NAME}
		RUNTIME DESTINATION "bin"
		LIBRARY DESTINATION "lib"
		ARCHIVE DESTINATION "lib"
		)
install(FILES ${AUDIO_HEADERS} DESTINATION include/audio)
