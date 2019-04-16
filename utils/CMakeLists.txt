#### miscellaneous utility functions/classes ####
project(vgm-utils)
cmake_minimum_required(VERSION 3.1)

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



# File Functions
# --------------
find_package(ZLIB REQUIRED)
set(UTIL_FILES ${UTIL_FILES} MemoryLoader.c FileLoader.c DataLoader.c)
set(UTIL_LIBS ${UTIL_LIBS} ZLIB::ZLIB)



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
endif()



add_library(${PROJECT_NAME} STATIC ${UTIL_FILES})
set_property(TARGET ${PROJECT_NAME} PROPERTY POSITION_INDEPENDENT_CODE ON)

target_compile_definitions(${PROJECT_NAME} PUBLIC ${UTIL_DEFS})

target_include_directories(${PROJECT_NAME} PRIVATE
	PUBLIC ${LIBVGM_SOURCE_DIR}
	PRIVATE ${LIBVGM_SOURCE_DIR}/libs/include
)
if(WIN32 AND NOT MSVC)
	target_include_directories(${PROJECT_NAME} PRIVATE
		${LIBVGM_SOURCE_DIR}/libs/include_mingw
	)
elseif(MSVC)
	target_include_directories(${PROJECT_NAME} PRIVATE
		${LIBVGM_SOURCE_DIR}/libs/include_vc
	)
	if(MSVC_VERSION LESS 1400)
		target_include_directories(${PROJECT_NAME} PRIVATE
			${LIBVGM_SOURCE_DIR}/libs/include_vc6
		)
	endif()
endif()
target_include_directories(${PROJECT_NAME} PRIVATE ${UTIL_INCLUDES})
target_link_libraries(${PROJECT_NAME} PRIVATE ${UTIL_LIBS})

install(TARGETS ${PROJECT_NAME}
		RUNTIME DESTINATION "bin"
		LIBRARY DESTINATION "lib"
		ARCHIVE DESTINATION "lib"
		)
install(FILES ${UTIL_HEADERS} DESTINATION include/utils)
