#### Audio Output Library ####
project(libaudio)
cmake_minimum_required(VERSION 2.8)

set(AUDIO_DEFS)
set(AUDIO_FILES
	AudioStream.c
)
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
endif()

option(AUDIODRV_WAVEWRITE "Audio Driver: Wave Writer" ON)

option(AUDIODRV_WINMM "Audio Driver: WinMM [Windows]" ${ADRV_WIN_ALL})
option(AUDIODRV_DSOUND "Audio Driver: DirectSound [Windows]" ${ADRV_WIN_ALL})
option(AUDIODRV_XAUDIO2 "Audio Driver: XAudio2 [Windows]" ${ADRV_WIN_ALL})
option(AUDIODRV_WASAPI "Audio Driver: WASAPI [Windows]" ${ADRV_WIN_VISTA})

option(AUDIODRV_OSS "Audio Driver: OSS (Open Sound System) [Linux]" ${ADRV_LINUX})
option(AUDIODRV_SADA "Audio Driver: SADA (Solaris Audio Device Architecture) [Solaris/BSD]" OFF)
option(AUDIODRV_ALSA "Audio Driver: ALSA (Advanced Linux Sound Architecture) [Linux]" ${ADRV_LINUX})
option(AUDIODRV_PULSE "Audio Driver: PulseAudio [Linux]" ${ADRV_LINUX})
option(AUDIODRV_LIBAO "Audio Driver: libao" ${ADRV_NO_WIN})


if(AUDIODRV_WAVEWRITE)
	set(AUDIO_DEFS ${AUDIO_DEFS} " AUDDRV_WAVEWRITE")
	set(AUDIO_FILES ${AUDIO_FILES} AudDrv_WaveWriter.c)
endif()

if(AUDIODRV_WINMM)
	set(AUDIO_DEFS ${AUDIO_DEFS} " AUDDRV_WINMM")
	set(AUDIO_FILES ${AUDIO_FILES} AudDrv_WinMM.c)
	set(AUDIO_LIBS ${AUDIO_LIBS} kernel32 winmm)
endif()

if(AUDIODRV_DSOUND)
	set(AUDIO_DEFS ${AUDIO_DEFS} " AUDDRV_DSOUND")
	set(AUDIO_FILES ${AUDIO_FILES} AudDrv_DSound.cpp)
	set(AUDIO_LIBS ${AUDIO_LIBS} kernel32 dsound uuid)
endif()

if(AUDIODRV_XAUDIO2)
	set(AUDIO_DEFS ${AUDIO_DEFS} " AUDDRV_XAUD2")
	set(AUDIO_FILES ${AUDIO_FILES} AudDrv_XAudio2.cpp)
	set(AUDIO_LIBS ${AUDIO_LIBS} kernel32 ole32)
endif()

if(AUDIODRV_WASAPI)
	set(AUDIO_DEFS ${AUDIO_DEFS} " AUDDRV_WASAPI")
	set(AUDIO_FILES ${AUDIO_FILES} AudDrv_WASAPI.cpp)
	set(AUDIO_LIBS ${AUDIO_LIBS} kernel32 ole32)
endif()

if(AUDIODRV_OSS)
	set(AUDIO_DEFS ${AUDIO_DEFS} " AUDDRV_OSS")
	set(AUDIO_FILES ${AUDIO_FILES} AudDrv_OSS.c)
	set(AUDIO_LIBS ${AUDIO_LIBS} pthread)
endif()

if(AUDIODRV_SADA)
	set(AUDIO_DEFS ${AUDIO_DEFS} " AUDDRV_SADA")
	set(AUDIO_FILES ${AUDIO_FILES} AudDrv_SADA.c)
	set(AUDIO_LIBS ${AUDIO_LIBS} pthread)
endif()

if(AUDIODRV_ALSA)
	set(AUDIO_DEFS ${AUDIO_DEFS} " AUDDRV_ALSA")
	set(AUDIO_FILES ${AUDIO_FILES} AudDrv_ALSA.c)
	set(AUDIO_LIBS ${AUDIO_LIBS} pthread asound)
endif()

if(AUDIODRV_ALSA)
	set(AUDIO_DEFS ${AUDIO_DEFS} " AUDDRV_PULSE")
	set(AUDIO_FILES ${AUDIO_FILES} AudDrv_Pulse.c)
	set(AUDIO_LIBS ${AUDIO_LIBS} pulse-simple pulse)
endif()

if(AUDIODRV_LIBAO)
	set(AUDIO_DEFS ${AUDIO_DEFS} " AUDDRV_LIBAO")
	set(AUDIO_FILES ${AUDIO_FILES} AudDrv_libao.c)
	set(AUDIO_LIBS ${AUDIO_LIBS} pthread ao)
endif()


add_library(${PROJECT_NAME} STATIC ${AUDIO_FILES})

target_compile_definitions(${PROJECT_NAME} PUBLIC ${AUDIO_DEFS})

target_include_directories(${PROJECT_NAME} PUBLIC
	${CMAKE_SOURCE_DIR}
)
if(WIN32 AND NOT MSVC)
target_include_directories(${PROJECT_NAME} PUBLIC
	${CMAKE_SOURCE_DIR}/libs/include_mingw
)
endif()

target_link_libraries(${PROJECT_NAME} PRIVATE ${AUDIO_LIBS})
