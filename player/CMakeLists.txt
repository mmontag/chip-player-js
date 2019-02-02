#### File Playback Library ####
project(vgm-player)
cmake_minimum_required(VERSION 2.8)

set(PLAYER_DEFS)
set(PLAYER_FILES
	helper.c
	FileLoader.cpp
	playerbase.cpp
	droplayer.cpp
	s98player.cpp
	vgmplayer_cmdhandler.cpp
	vgmplayer.cpp
)
# export headers
set(PLAYER_HEADERS
	helper.h
	FileLoader.hpp
	playerbase.hpp
	droplayer.hpp
	s98player.hpp
	vgmplayer.hpp
)
set(PLAYER_INCLUDES)
set(PLAYER_LIBS)

add_library(${PROJECT_NAME} STATIC ${PLAYER_FILES})

target_compile_definitions(${PROJECT_NAME} PUBLIC ${PLAYER_DEFS})

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
target_include_directories(${PROJECT_NAME} PUBLIC ${PLAYER_INCLUDES})

target_link_libraries(${PROJECT_NAME} PRIVATE ${PLAYER_LIBS} vgm-emu)

install(TARGETS ${PROJECT_NAME}
		RUNTIME DESTINATION "bin"
		LIBRARY DESTINATION "lib"
		ARCHIVE DESTINATION "lib"
		)
install(FILES ${PLAYER_HEADERS} DESTINATION include/player)
