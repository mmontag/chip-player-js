#### libvgm ####
cmake_minimum_required(VERSION 3.1)
if(POLICY CMP0042)
	cmake_policy(SET CMP0042 NEW)
endif()
if(POLICY CMP0048)
	cmake_policy(SET CMP0048 NEW)
endif()
if(POLICY CMP0074)
	cmake_policy(SET CMP0074 NEW)
endif()

project(libvgmMain VERSION 0.1 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 14)

list(APPEND CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/libs/cmake_modules/")

include(GNUInstallDirs)
include(CheckIncludeFile)
include("${PROJECT_SOURCE_DIR}/cmake/utils.cmake")

set(LIBVGM_NAME "libvgm")	# for use in subdirectories
set(LIBVGM_VERSION ${PROJECT_VERSION})	# for use in subdirectories
set(LIBVGM_SOURCE_DIR ${PROJECT_SOURCE_DIR})	# for use in subdirectories
set(LIBVGM_INSTALL_INCLUDE_DIR "${CMAKE_INSTALL_INCLUDEDIR}/vgm")
set(LIBVGM_INSTALL_TARGETS)	# will be filled in by sub-projects
set(LIBVGM_DEPS)	# will be filled in by sub-projects

set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}/bin")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}/bin")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}/bin")

if ("${CMAKE_VERSION}" VERSION_LESS "3.20")
	include(TestBigEndian)
	test_big_endian(ARCH_BIG_ENDIAN)
	if (ARCH_BIG_ENDIAN)
		set(CMAKE_C_BYTE_ORDER "BIG_ENDIAN")
	else()
		set(CMAKE_C_BYTE_ORDER "LITTLE_ENDIAN")
	endif()
endif()
# add defines for Little/Big Endian
if ("${CMAKE_C_BYTE_ORDER}" STREQUAL "BIG_ENDIAN")
	add_definitions("-D VGM_BIG_ENDIAN")
else()
	add_definitions("-D VGM_LITTLE_ENDIAN")
endif()

check_include_file("stdint.h" HAVE_STDINT)
if(HAVE_STDINT)
	add_definitions("-D HAVE_STDINT_H")
endif()


if(MSVC)
	set(CMAKE_DEBUG_POSTFIX "d")
	if(MSVC_VERSION LESS 1400)
		set(MSVC_POSTFIX "VC6")	# there are a few limitations for MSVC versions before 2005
	elseif(CMAKE_CL_64)
		set(MSVC_POSTFIX "Win64")
	else()
		set(MSVC_POSTFIX "Win32")
	endif()
	set(CMAKE_RELEASE_POSTFIX "_${MSVC_POSTFIX}${CMAKE_RELEASE_POSTFIX}")
	set(CMAKE_DEBUG_POSTFIX "_${MSVC_POSTFIX}${CMAKE_DEBUG_POSTFIX}")
	set(CMAKE_MINSIZEREL_POSTFIX "_${MSVC_POSTFIX}${CMAKE_MINSIZEREL_POSTFIX}")
	set(CMAKE_RELWITHDEBINFO_POSTFIX "_${MSVC_POSTFIX}${CMAKE_RELWITHDEBINFO_POSTFIX}")
	
	set(ZLIB_ROOT "${PROJECT_SOURCE_DIR}/libs" CACHE PATH "ZLib directory")
	set(Iconv_INCLUDE_DIR "${PROJECT_SOURCE_DIR}/libs/iconv/include" CACHE PATH "directory with iconv headers")
	set(Iconv_LIBRARY "${PROJECT_SOURCE_DIR}/libs/iconv/lib/libiconv.lib" CACHE FILEPATH "iconv library")
	
	if(NOT MSVC_VERSION LESS 1400)
		# Note: CMake 3.12 and up have add_compile_options()
		add_definitions("-D _CRT_SECURE_NO_WARNINGS")
	endif()
endif()

if(MINGW)
	option(LINK_STATIC_LIBS "link with static runtime libraries (MinGW only)" ON)
	if(LINK_STATIC_LIBS)
		set(CMAKE_FIND_LIBRARY_SUFFIXES .a ${CMAKE_FIND_LIBRARY_SUFFIXES})
		set(CMAKE_CXX_STANDARD_LIBRARIES "-static-libgcc -static-libstdc++ -lwsock32 -lws2_32 ${CMAKE_CXX_STANDARD_LIBRARIES}")
		set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,-Bstatic,--whole-archive -lwinpthread -Wl,--no-whole-archive")
	endif()
else()
	option(LINK_STATIC_LIBS "link with static runtime libraries (MinGW only)" OFF)
endif()

if(CMAKE_COMPILER_IS_GNUCC)

# assume Windows 2000 and later for GetConsoleWindow API call
if(WIN32)
	add_definitions("-D _WIN32_WINNT=0x500")
endif()
set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -D_DEBUG")

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall")
#set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wextra")
#set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wpedantic")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wno-unused-parameter -Wno-unused-but-set-variable -Wno-long-long -Wno-unused-value")

# silence typical sound core warnings
set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -Wno-unknown-pragmas")
set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -Wno-sign-compare")
set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -Wno-unused-variable -Wno-unused-const-variable -Wno-unused-function")

# additional warnings from http://blog.httrack.com/blog/2014/03/09/what-are-your-gcc-flags/
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wpointer-arith -Winit-self -Wstrict-aliasing")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wformat -Wformat-security -Wformat-nonliteral")
#set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fstack-protector")
#set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -fstack-protector")

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${CMAKE_C_FLAGS}")
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} ${CMAKE_C_FLAGS_DEBUG}")
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} ${CMAKE_C_FLAGS_RELEASE}")

endif(CMAKE_COMPILER_IS_GNUCC)

list(APPEND CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake/sanitizers")
find_package(Sanitizers)


option(BUILD_LIBAUDIO "build audio output library" ON)
option(BUILD_LIBEMU "build sound emulation library" ON)
option(BUILD_LIBPLAYER "build player library" ON)
option(BUILD_TESTS "build test programs" OFF)
option(BUILD_PLAYER "build player application" ON)
option(BUILD_VGM2WAV "build sample vgm2wav application" ON)
set(LIBRARY_TYPE "STATIC" CACHE STRING "library build type (static/shared)")
set_property(CACHE LIBRARY_TYPE PROPERTY STRINGS "SHARED;STATIC")
option(USE_SANITIZERS "use sanitizers" ON)


# --- main library ---
add_subdirectory(utils)
if(USE_SANITIZERS)
	add_sanitizers(utils)
endif(USE_SANITIZERS)

if(BUILD_LIBAUDIO)
	add_subdirectory(audio)
	if(USE_SANITIZERS)
		add_sanitizers(vgm-audio)
	endif(USE_SANITIZERS)
endif()

if(BUILD_LIBEMU)
	add_subdirectory(emu)
	if(USE_SANITIZERS)
		add_sanitizers(vgm-emu)
	endif(USE_SANITIZERS)
endif()

if(BUILD_LIBPLAYER)
	add_subdirectory(player)
	if(USE_SANITIZERS)
		add_sanitizers(vgm-player)
	endif(USE_SANITIZERS)
endif()


# --- additional stuff ---
if(BUILD_TESTS)

add_executable(audiotest audiotest.c)
target_include_directories(audiotest PRIVATE ${LIBVGM_SOURCE_DIR})
target_link_libraries(audiotest PRIVATE vgm-audio)
if(USE_SANITIZERS)
	add_sanitizers(audiotest)
endif(USE_SANITIZERS)

add_executable(emutest emutest.c)
target_include_directories(emutest PRIVATE ${LIBVGM_SOURCE_DIR})
target_link_libraries(emutest PRIVATE vgm-emu)
if(USE_SANITIZERS)
	add_sanitizers(emutest)
endif(USE_SANITIZERS)

add_executable(audemutest audemutest.c)
target_include_directories(audemutest PRIVATE ${LIBVGM_SOURCE_DIR})
target_link_libraries(audemutest PRIVATE vgm-audio vgm-emu)
if(USE_SANITIZERS)
	add_sanitizers(audemutest)
endif(USE_SANITIZERS)

find_package(ZLIB REQUIRED)

add_executable(vgmtest vgmtest.c player/dblk_compr.c)
target_include_directories(vgmtest PRIVATE ${LIBVGM_SOURCE_DIR})
target_link_libraries(vgmtest PRIVATE ZLIB::ZLIB vgm-audio vgm-emu)
if(USE_SANITIZERS)
	add_sanitizers(vgmtest)
endif(USE_SANITIZERS)

install(TARGETS audiotest emutest audemutest vgmtest DESTINATION "${CMAKE_INSTALL_BINDIR}")
endif(BUILD_TESTS)

if(BUILD_PLAYER)
add_executable(player player.cpp player/dblk_compr.c)
target_include_directories(player PRIVATE ${LIBVGM_SOURCE_DIR})
target_link_libraries(player PRIVATE vgm-audio vgm-player vgm-utils)
if(USE_SANITIZERS)
	add_sanitizers(player)
endif(USE_SANITIZERS)
install(TARGETS player DESTINATION "${CMAKE_INSTALL_BINDIR}")
endif(BUILD_PLAYER)

if(BUILD_VGM2WAV)
add_executable(vgm2wav vgm2wav.cpp)
target_include_directories(vgm2wav PRIVATE ${LIBVGM_SOURCE_DIR})
target_link_libraries(vgm2wav PRIVATE vgm-player vgm-emu vgm-utils)
if(USE_SANITIZERS)
	add_sanitizers(vgm2wav)
endif(USE_SANITIZERS)
install(TARGETS vgm2wav DESTINATION "${CMAKE_INSTALL_BINDIR}")
endif(BUILD_VGM2WAV)

set(COMMON_HEADERS
	common_def.h
	stdbool.h
	stdtype.h
)

install(FILES ${COMMON_HEADERS} DESTINATION "${LIBVGM_INSTALL_INCLUDE_DIR}")
cmake_cfg_install("${LIBVGM_SOURCE_DIR}/cmake/config.cmake.in"
	NAME "${LIBVGM_NAME}"
	VERSION "${LIBVGM_VERSION}"
	TARGETS "${LIBVGM_INSTALL_TARGETS}"
	DEPS "${LIBVGM_DEPS}"
	)
