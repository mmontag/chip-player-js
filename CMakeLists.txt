#### Audio Output Library ####
project(libVgmTest)
cmake_minimum_required(VERSION 2.8)

set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

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
	
	set(ZLIB_ROOT "${CMAKE_SOURCE_DIR}/libs" CACHE PATH "ZLib directory")
	
	if(NOT MSVC_VERSION LESS 1400)
		add_definitions("/D _CRT_SECURE_NO_WARNINGS")
	endif()
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
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wno-unused-parameter -Wno-unused-but-set-variable -Wno-long-long")

# silence typical sound core warnings
set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -Wno-unknown-pragmas")
set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -Wno-unused-value -Wno-sign-compare")
set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -Wno-unused-variable -Wno-unused-const-variable -Wno-unused-function")

# additional warnings from http://blog.httrack.com/blog/2014/03/09/what-are-your-gcc-flags/
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wpointer-arith -Winit-self -Wstrict-aliasing")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wformat -Wformat-security -Wformat-nonliteral")
#set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fstack-protector")
#set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -fstack-protector")

set(CMAKE_CXX_FLAGS "${CMAKE_C_FLAGS}")
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG}")
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE}")

endif(CMAKE_COMPILER_IS_GNUCC)


option(BUILD_LIBAUDIO "build audio output library" ON)
option(BUILD_LIBEMU "build sound emulation library" ON)
option(BUILD_TESTS "build test programs" ON)
option(BUILD_PLAYER "build player application" ON)

find_package(ZLIB)

if(BUILD_LIBAUDIO)
	add_subdirectory(audio)
endif()
if(BUILD_LIBEMU)
	add_subdirectory(emu)
endif()
if(BUILD_PLAYER)
	add_subdirectory(player)
endif()


if(BUILD_TESTS)

add_executable(audiotest audiotest.c)
target_include_directories(audiotest PRIVATE ${CMAKE_SOURCE_DIR})
target_link_libraries(audiotest PRIVATE vgm-audio)

add_executable(emutest emutest.c)
target_include_directories(emutest PRIVATE ${CMAKE_SOURCE_DIR})
target_link_libraries(emutest PRIVATE vgm-emu)

add_executable(audemutest audemutest.c)
target_include_directories(audemutest PRIVATE ${CMAKE_SOURCE_DIR})
target_link_libraries(audemutest PRIVATE vgm-audio vgm-emu)

add_executable(vgmtest vgmtest.c vgm/dblk_compr.c)
target_include_directories(vgmtest PRIVATE ${CMAKE_SOURCE_DIR} ${ZLIB_INCLUDE_DIRS})
target_link_libraries(vgmtest PRIVATE ${ZLIB_LIBRARIES} vgm-audio vgm-emu)

endif(BUILD_TESTS)

if(BUILD_PLAYER)
add_executable(player player.cpp vgm/dblk_compr.c)
target_include_directories(player PRIVATE ${CMAKE_SOURCE_DIR} ${ZLIB_INCLUDE_DIRS})
target_link_libraries(player PRIVATE ${ZLIB_LIBRARIES} vgm-audio vgm-player)
endif()
