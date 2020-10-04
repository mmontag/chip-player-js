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



add_library(${PROJECT_NAME} ${LIBRARY_TYPE} ${UTIL_FILES})
set_target_properties(${PROJECT_NAME} PROPERTIES POSITION_INDEPENDENT_CODE ON)
target_compile_definitions(${PROJECT_NAME} PUBLIC ${UTIL_DEFS})
target_include_directories(${PROJECT_NAME}
	PUBLIC ${LIBVGM_SOURCE_DIR}
	PRIVATE ${UTIL_INCLUDES}
)
target_link_libraries(${PROJECT_NAME} PUBLIC ${UTIL_LIBS})


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
