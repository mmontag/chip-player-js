set(CMAKE_CROSSCOMPILING TRUE)
set(CMAKE_SYSTEM_NAME MSDOS)
SET(CMAKE_SYSTEM_VERSION 6)

set (OPENWATTCOM TRUE)
set (MSDOS TRUE)

set (WATCOM_PREFIX "/opt/watcom")

#add_definitions(-D__WATCOMC__ -D__DOS__ -D__DOS4G__)

set (ENV{PATH} ${WATCOM_PREFIX}/binl:$ENV{PATH})
set (ENV{WATCOM} ${WATCOM_PREFIX})
set (ENV{INCLUDE} "${WATCOM_PREFIX}/h:${CMAKE_CURRENT_LIST_DIR}/custom-h")
set (ENV{EDPATH} ${WATCOM_PREFIX}/eddat)
set (ENV{WIPFC} ${WATCOM_PREFIX}/wipfc)

# specify the cross compiler
set (CMAKE_C_COMPILER ${WATCOM_PREFIX}/binl/owcc)
set (CMAKE_CXX_COMPILER ${WATCOM_PREFIX}/binl/owcc)

set (WATCOM_FLAGS "-bdos4g -march=i386")
set (CMAKE_C_FLAGS "${WATCOM_FLAGS} -x c -std=wc")
set (CMAKE_CXX_FLAGS "${WATCOM_FLAGS} -xc++ -xs -feh -frtti -std=c++98")
set (CMAKE_EXE_LINKER_FLAGS "")
# export CFLAGS="$WATCOM_FLAGS -x c -std=wc"
# export CXXFLAGS="$WATCOM_FLAGS -x c++ -xs -feh -frtti -std=c++98"

# where is the target environment
set (CMAKE_FIND_ROOT_PATH ${WATCOM_PREFIX})

# search for programs in the build host directories
set (CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
# for libraries and headers in the target directories
set (CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set (CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set (CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)

include_directories(${WATCOM_PREFIX}/h ${CMAKE_CURRENT_LIST_DIR}/custom-h)

# Make sure Qt can be detected by CMake
set (QT_BINARY_DIR ${WATCOM_PREFIX}/binl /usr/bin)
set (QT_INCLUDE_DIRS_NO_SYSTEM ON)

# These are needed for compiling lapack (RHBZ #753906)
#set (CMAKE_Fortran_COMPILER /home/wohlstand/Qt/Tools/ow-snapshot-2.0/binl/wfc)
#set (CMAKE_AR:FILEPATH /usr/local/djgpp/bin/i586-pc-msdosdjgpp-ar)
#set (CMAKE_RANLIB:FILEPATH /usr/local/djgpp/bin/i586-pc-msdosdjgpp-ranlib)

# include(Linux-OpenWatcom.cmake)

