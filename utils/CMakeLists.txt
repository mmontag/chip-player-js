#### miscellaneous utility functions/classes ####
project(vgm-utils)
cmake_minimum_required(VERSION 2.8)

set(UTIL_DEFS)
set(UTIL_FILES
	FileLoader.cpp
)
# export headers
set(UTIL_HEADERS
	FileLoader.hpp
	StrUtils.h
)
set(UTIL_INCLUDES)
set(UTIL_LIBS)


find_package(Iconv REQUIRED)

set(UTIL_LIBS ${UTIL_LIBS} Iconv::Iconv)
set(UTIL_FILES ${UTIL_FILES} StrUtils-CPConv_IConv.c)


add_library(${PROJECT_NAME} STATIC ${UTIL_FILES})

target_compile_definitions(${PROJECT_NAME} PUBLIC ${UTIL_DEFS})

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
target_include_directories(${PROJECT_NAME} PUBLIC ${UTIL_INCLUDES})
target_link_libraries(${PROJECT_NAME} PRIVATE ${UTIL_LIBS})

install(TARGETS ${PROJECT_NAME}
		RUNTIME DESTINATION "bin"
		LIBRARY DESTINATION "lib"
		ARCHIVE DESTINATION "lib"
		)
install(FILES ${UTIL_HEADERS} DESTINATION include/utils)
