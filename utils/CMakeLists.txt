#### miscellaneous utility functions/classes ####
if (POLICY CMP0048)
	cmake_policy(SET CMP0048 NEW)
endif ()

project(vgm-utils VERSION 1.0)
cmake_minimum_required(VERSION 3.1)

if (POLICY CMP0042)
	cmake_policy(SET CMP0042 NEW)
endif ()

set(LIBVGM_SOURCE_DIR ${PROJECT_SOURCE_DIR}/..)

set(UTIL_DEFS)
set(UTIL_FILES)
# export headers
set(UTIL_HEADERS
	DataLoader.h
	FileLoader.h
	MemoryLoader.h
	OSMutex.h
	OSSignal.h
	OSThread.h
	StrUtils.h
)
set(UTIL_INCLUDES)
set(UTIL_LIBS)
set(VGMUTILS_PKGCONFIG_CFLAGS)
set(VGMUTILS_PKGCONFIG_LDFLAGS)
set(VGMUTILS_PKGCONFIG_REQUIRES)
set(VGMUTILS_PKGCONFIG_VERSION ${PROJECT_VERSION})



# File Functions
# --------------
find_package(ZLIB REQUIRED)
set(UTIL_FILES ${UTIL_FILES} MemoryLoader.c FileLoader.c DataLoader.c)
set(UTIL_LIBS ${UTIL_LIBS} ZLIB::ZLIB)
set(VGMUTILS_PKGCONFIG_REQUIRES ${VGMUTILS_PKGCONFIG_REQUIRES} "zlib")



# Threads and Synchronization
# ---------------------------
find_package(Threads REQUIRED)

if(CMAKE_USE_WIN32_THREADS_INIT)
	set(UTIL_FILES ${UTIL_FILES}
		OSMutex_Win.c
		OSSignal_Win.c
		OSThread_Win.c
	)
elseif(CMAKE_USE_PTHREADS_INIT)
	set(UTIL_FILES ${UTIL_FILES}
		OSMutex_POSIX.c
		OSSignal_POSIX.c
		OSThread_POSIX.c
	)
endif()
set(UTIL_LIBS ${UTIL_LIBS} Threads::Threads)
set(VGMUTILS_PKGCONFIG_LDFLAGS ${VGMUTILS_PKGCONFIG_LDFLAGS} ${CMAKE_THREAD_LIBS_INIT})



# String Codepage Conversion
# --------------------------
find_package(Iconv QUIET)
if(WIN32)
	if(${ICONV_FOUND})
		set(CAN_WINAPI OFF)
	else()
		set(CAN_WINAPI ON)
	endif()
else()
	set(CAN_WINAPI OFF)
endif()


option(UTIL_CHARCNV_ICONV "Charset Conversion: iconv" ${ICONV_FOUND})
option(UTIL_CHARCNV_WINAPI "Charset Conversion: WinAPI" ${CAN_WINAPI})


if(UTIL_CHARCNV_WINAPI)
	set(UTIL_FILES ${UTIL_FILES} StrUtils-CPConv_Win.c)
elseif(UTIL_CHARCNV_ICONV)
	find_package(Iconv REQUIRED)
	set(UTIL_FILES ${UTIL_FILES} StrUtils-CPConv_IConv.c)
	set(UTIL_LIBS ${UTIL_LIBS} Iconv::Iconv)
    if(NOT Iconv_IS_BUILT_IN)
    	set(VGMUTILS_PKGCONFIG_LDFLAGS ${VGMUTILS_PKGCONFIG_LDFLAGS} "-liconv")
    endif()
endif()



add_library(${PROJECT_NAME}_obj OBJECT ${UTIL_FILES})
set_property(TARGET ${PROJECT_NAME}_obj PROPERTY POSITION_INDEPENDENT_CODE ON)
target_link_libraries(${PROJECT_NAME}_obj PRIVATE ${UTIL_LIBS})

target_compile_definitions(${PROJECT_NAME}_obj PUBLIC ${UTIL_DEFS})

target_include_directories(${PROJECT_NAME}_obj PRIVATE
	PUBLIC ${LIBVGM_SOURCE_DIR}
	PRIVATE ${LIBVGM_SOURCE_DIR}/libs/include
)
if(WIN32 AND NOT MSVC)
	target_include_directories(${PROJECT_NAME}_obj PRIVATE
		${LIBVGM_SOURCE_DIR}/libs/include_mingw
	)
elseif(MSVC)
	target_include_directories(${PROJECT_NAME}_obj PRIVATE
		${LIBVGM_SOURCE_DIR}/libs/include_vc
	)
	if(MSVC_VERSION LESS 1400)
		target_include_directories(${PROJECT_NAME}_obj PRIVATE
			${LIBVGM_SOURCE_DIR}/libs/include_vc6
		)
	endif()
endif()
target_include_directories(${PROJECT_NAME}_obj PRIVATE ${UTIL_INCLUDES})

add_library(${PROJECT_NAME} STATIC $<TARGET_OBJECTS:${PROJECT_NAME}_obj>)

SET_TARGET_PROPERTIES(${PROJECT_NAME} PROPERTIES OUTPUT_NAME ${PROJECT_NAME})
target_compile_definitions(${PROJECT_NAME} PUBLIC ${UTIL_DEFS})
target_link_libraries(${PROJECT_NAME} PRIVATE ${UTIL_LIBS})

if(BUILD_SHARED)
add_library(${PROJECT_NAME}_shared SHARED $<TARGET_OBJECTS:${PROJECT_NAME}_obj>)
SET_TARGET_PROPERTIES(${PROJECT_NAME}_shared PROPERTIES OUTPUT_NAME ${PROJECT_NAME})
target_compile_definitions(${PROJECT_NAME}_shared PUBLIC ${UTIL_DEFS})
target_link_libraries(${PROJECT_NAME}_shared PRIVATE ${UTIL_LIBS})
endif(BUILD_SHARED)

foreach(DEF ${UTIL_DEFS})
	string(REPLACE " " "" DEF "${DEF}")
	set(VGMUTILS_PKGCONFIG_CFLAGS ${VGMUTILS_PKGCONFIG_CFLAGS} "-D${DEF}")
endforeach(DEF)

string(REPLACE ";" " " VGMUTILS_PKGCONFIG_CFLAGS "${VGMUTILS_PKGCONFIG_CFLAGS}")
string(REPLACE ";" " " VGMUTILS_PKGCONFIG_LDFLAGS "${VGMUTILS_PKGCONFIG_LDFLAGS}")
string(REPLACE ";" " " VGMUTILS_PKGCONFIG_REQUIRES "${VGMUTILS_PKGCONFIG_REQUIRES}")

configure_file(vgm-utils.pc.in ${CMAKE_CURRENT_BINARY_DIR}/vgm-utils.pc @ONLY)

install(TARGETS ${PROJECT_NAME}
		RUNTIME DESTINATION "bin"
		LIBRARY DESTINATION "lib"
		ARCHIVE DESTINATION "lib"
		)
install(FILES ${UTIL_HEADERS} DESTINATION include/vgm/utils)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/vgm-utils.pc DESTINATION lib/pkgconfig)

if(BUILD_SHARED)
install(TARGETS ${PROJECT_NAME}_shared
		RUNTIME DESTINATION "bin"
		LIBRARY DESTINATION "lib"
		ARCHIVE DESTINATION "lib"
		)
endif(BUILD_SHARED)
