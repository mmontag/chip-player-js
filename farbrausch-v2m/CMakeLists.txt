CMAKE_MINIMUM_REQUIRED(VERSION 2.8)

PROJECT(v2mplayer)

SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DNDEBUG -Os -flto -Wall -fno-asynchronous-unwind-tables -fno-stack-protector -ffunction-sections -fdata-sections -Wl,--gc-sections")
SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DNDEBUG -DRONAN -Os -flto -Wall -fno-asynchronous-unwind-tables -fno-stack-protector -ffunction-sections -fdata-sections -Wl,--gc-sections")

SET(CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/CMake)
SET(MAJOR_VERSION 0)
SET(DATE_VERSION 20180108)
SET(V2M_VERSION ${MAJOR_VERSION}.${DATE_VERSION})

find_package(SDL2 REQUIRED)
include_directories(${SDL2_INCLUDE_DIRS})
include_directories(~/src/emsdk/emscripten/1.38.11/system/include/emscripten)

ADD_EXECUTABLE(v2mplayer
#    tinyplayer.cpp
    ronan.cpp
    v2mplayer.cpp
    v2mconv.cpp
    synth_core.cpp
    sounddef.cpp
        v2mwrapper.cpp
)
SET_TARGET_PROPERTIES(v2mplayer PROPERTIES VERSION ${V2M_VERSION} SOVERSION ${MAJOR_VERSION})

TARGET_LINK_LIBRARIES(v2mplayer ${SDL2_LIBRARIES})
