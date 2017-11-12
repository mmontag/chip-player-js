set(CMAKE_CROSSCOMPILING TRUE)
set(CMAKE_SYSTEM_NAME Linux)

set(OPENWATTCOM TRUE)
set(LINUX TRUE)

set(WATCOM_PREFIX "$ENV{HOME}/Qt/Tools/ow-snapshot-2.0")

set(ENV{PATH} ${WATCOM_PREFIX}/binl:$ENV{PATH})
set(ENV{WATCOM} ${WATCOM_PREFIX})
set(ENV{INCLUDE} "${WATCOM_PREFIX}/lh")
set(ENV{LIB} "${WATCOM_PREFIX}/lib386")
set(ENV{EDPATH} ${WATCOM_PREFIX}/eddat)
set(ENV{WIPFC} ${WATCOM_PREFIX}/wipfc)

# specify the cross compiler
set(CMAKE_C_COMPILER ${WATCOM_PREFIX}/binl/owcc)
set(CMAKE_CXX_COMPILER ${WATCOM_PREFIX}/binl/owcc)

# where is the target environment
set(CMAKE_FIND_ROOT_PATH ${WATCOM_PREFIX})

# search for programs in the build host directories
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
# for libraries and headers in the target directories
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)

include_directories(${WATCOM_PREFIX}/lh)

# Make sure Qt can be detected by CMake
set(QT_BINARY_DIR ${WATCOM_PREFIX}/binl /usr/bin)
set(QT_INCLUDE_DIRS_NO_SYSTEM ON)

# These are needed for compiling lapack (RHBZ #753906)
#set (CMAKE_Fortran_COMPILER /home/wohlstand/Qt/Tools/ow-snapshot-2.0/binl/wfc)
#set (CMAKE_AR:FILEPATH /usr/local/djgpp/bin/i586-pc-msdosdjgpp-ar)
#set (CMAKE_RANLIB:FILEPATH /usr/local/djgpp/bin/i586-pc-msdosdjgpp-ranlib)

# include(Linux-OpenWatcom.cmake)


