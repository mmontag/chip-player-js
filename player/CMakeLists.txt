#### File Playback Library ####
if (POLICY CMP0048)
	cmake_policy(SET CMP0048 NEW)
endif ()

project(vgm-player VERSION 1.0)

if (POLICY CMP0042)
	cmake_policy(SET CMP0042 NEW)
endif ()

set(LIBVGM_SOURCE_DIR ${PROJECT_SOURCE_DIR}/..)
cmake_minimum_required(VERSION 3.1)

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

add_library(${PROJECT_NAME} ${LIBRARY_TYPE} ${PLAYER_FILES})
set_target_properties(${PROJECT_NAME} PROPERTIES POSITION_INDEPENDENT_CODE ON)
target_compile_definitions(${PROJECT_NAME} PUBLIC ${PLAYER_DEFS})
target_include_directories(${PROJECT_NAME}
	PUBLIC ${LIBVGM_SOURCE_DIR}
	PRIVATE ${PLAYER_INCLUDES}
)
target_link_libraries(${PROJECT_NAME} PRIVATE ${PLAYER_LIBS} vgm-emu vgm-utils)


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
