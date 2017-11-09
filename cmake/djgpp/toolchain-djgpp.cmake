set (CMAKE_SYSTEM_NAME linux-djgpp)

set (DJGPP TRUE)

# specify the cross compiler
set (CMAKE_C_COMPILER /usr/local/djgpp/bin/i586-pc-msdosdjgpp-gcc)
set (CMAKE_CXX_COMPILER /usr/local/djgpp/bin/i586-pc-msdosdjgpp-g++)

# where is the target environment
set (CMAKE_FIND_ROOT_PATH /usr/local/djgpp)

# search for programs in the build host directories
set (CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
# for libraries and headers in the target directories
set (CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set (CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set (CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)

# Make sure Qt can be detected by CMake
set (QT_BINARY_DIR /usr/local/djgpp/bin /usr/bin)
set (QT_INCLUDE_DIRS_NO_SYSTEM ON)

# These are needed for compiling lapack (RHBZ #753906)
set (CMAKE_Fortran_COMPILER /usr/local/djgpp/bin/i586-pc-msdosdjgpp-gfortran)
set (CMAKE_AR:FILEPATH /usr/local/djgpp/bin/i586-pc-msdosdjgpp-ar)
set (CMAKE_RANLIB:FILEPATH /usr/local/djgpp/bin/i586-pc-msdosdjgpp-ranlib)

