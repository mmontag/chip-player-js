#### File Playback Library ####
if (POLICY CMP0048)
	cmake_policy(SET CMP0048 NEW)
endif ()

project(vgm-player VERSION 1.0)
cmake_minimum_required(VERSION 3.12)

if (POLICY CMP0042)
	cmake_policy(SET CMP0042 NEW)
endif ()

set(LIBVGM_SOURCE_DIR ${PROJECT_SOURCE_DIR}/..)

set(PLAYER_DEFS)
set(PLAYER_FILES
	dblk_compr.c
	helper.c
	playerbase.cpp
	droplayer.cpp
	s98player.cpp
	vgmplayer_cmdhandler.cpp
	vgmplayer.cpp
)
# export headers
set(PLAYER_HEADERS
	dblk_compr.h
	helper.h
	logging.h
	playerbase.hpp
	droplayer.hpp
	s98player.hpp
	vgmplayer.hpp
)
set(PLAYER_INCLUDES)
set(PLAYER_LIBS)
set(VGMPLAYER_PKGCONFIG_CFLAGS)
set(VGMPLAYER_PKGCONFIG_LDFLAGS)
set(VGMPLAYER_PKGCONFIG_VERSION ${PROJECT_VERSION})

add_library(${PROJECT_NAME}_obj OBJECT ${PLAYER_FILES})
set_property(TARGET ${PROJECT_NAME}_obj PROPERTY POSITION_INDEPENDENT_CODE ON)
target_link_libraries(${PROJECT_NAME}_obj PRIVATE ${PLAYER_LIBS})

target_compile_definitions(${PROJECT_NAME}_obj PUBLIC ${PLAYER_DEFS})

target_include_directories(${PROJECT_NAME}_obj
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
target_include_directories(${PROJECT_NAME}_obj PRIVATE ${PLAYER_INCLUDES})

add_library(${PROJECT_NAME} STATIC $<TARGET_OBJECTS:${PROJECT_NAME}_obj>)
SET_TARGET_PROPERTIES(${PROJECT_NAME} PROPERTIES OUTPUT_NAME ${PROJECT_NAME})
target_compile_definitions(${PROJECT_NAME} PUBLIC ${PLAYER_DEFS})
target_link_libraries(${PROJECT_NAME} PRIVATE ${PLAYER_LIBS} vgm-emu vgm-utils)

if(BUILD_SHARED)
add_library(${PROJECT_NAME}_shared SHARED $<TARGET_OBJECTS:${PROJECT_NAME}_obj>)
SET_TARGET_PROPERTIES(${PROJECT_NAME}_shared PROPERTIES OUTPUT_NAME ${PROJECT_NAME})
target_compile_definitions(${PROJECT_NAME}_shared PUBLIC ${PLAYER_DEFS})
target_link_libraries(${PROJECT_NAME}_shared PRIVATE ${PLAYER_LIBS} vgm-emu_shared vgm-utils_shared)
endif(BUILD_SHARED)


foreach(DEF ${PLAYER_DEFS})
	string(REPLACE " " "" DEF "${DEF}")
	set(VGMPLAYER_PKGCONFIG_CFLAGS ${VGMPLAYER_PKGCONFIG_CFLAGS} "-D${DEF}")
endforeach(DEF)

string(REPLACE ";" " " VGMPLAYER_PKGCONFIG_CFLAGS "${VGMPLAYER_PKGCONFIG_CFLAGS}")

configure_file(vgm-player.pc.in ${CMAKE_CURRENT_BINARY_DIR}/vgm-player.pc @ONLY)

install(TARGETS ${PROJECT_NAME}
		RUNTIME DESTINATION "bin"
		LIBRARY DESTINATION "lib"
		ARCHIVE DESTINATION "lib"
		)
install(FILES ${PLAYER_HEADERS} DESTINATION include/vgm/player)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/vgm-player.pc DESTINATION lib/pkgconfig)

if(BUILD_SHARED)
install(TARGETS ${PROJECT_NAME}_shared
		RUNTIME DESTINATION "bin"
		LIBRARY DESTINATION "lib"
		ARCHIVE DESTINATION "lib"
		)
endif(BUILD_SHARED)
