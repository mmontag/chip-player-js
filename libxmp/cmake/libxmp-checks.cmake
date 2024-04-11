# ======== Policies ========

if(POLICY CMP0075) # Include file check macros honor CMAKE_REQUIRED_LIBRARIES, CMake >= 3.12
    cmake_policy(SET CMP0075 NEW)
endif()

if(POLICY CMP0077) # Cache variables override since 3.12
    cmake_policy(SET CMP0077 NEW)
endif()


# ======== Compiler ========

if(CMAKE_C_COMPILER_ID MATCHES "Clang")
    set(HAVE_CLANG TRUE)
elseif(CMAKE_COMPILER_IS_GNUCC)
    set(HAVE_GCC TRUE)
endif()

message("-- Check for working linker: ${CMAKE_LINKER}")
execute_process(COMMAND ${CMAKE_LINKER} -v
                OUTPUT_VARIABLE LINKER_OUTPUT
                ERROR_VARIABLE  LINKER_OUTPUT)

string(REGEX REPLACE "[\r\n].*" "" LINKER_OUTPUT_LINE "${LINKER_OUTPUT}")
message(STATUS "Linker identification: ${LINKER_OUTPUT_LINE}")

if("${LINKER_OUTPUT}" MATCHES ".*GNU.*" OR "${LINKER_OUTPUT}" MATCHES ".*with BFD.*")
    set(HAVE_GNU_LD TRUE)
endif()


# ======== Includes ========

include(CheckSymbolExists)
include(CheckFunctionExists)
include(CheckIncludeFile)
include(CMakePushCheckState)
include(CheckCSourceCompiles)
include(CheckCCompilerFlag)
include(TestBigEndian)

# If platform is Emscripten
if(${CMAKE_SYSTEM_NAME} STREQUAL "Emscripten")
    set(EMSCRIPTEN 1)
endif()


# ======== Macros ========
macro(xmp_add_warning_flag WARNINGFLAG WARNING_VAR)
    check_c_compiler_flag("${WARNINGFLAG}" HAVE_W_${WARNING_VAR})
    if(HAVE_W_${WARNING_VAR})
       set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${WARNINGFLAG}")
    endif()
endmacro()

macro(xmp_disable_warning_flag WARNINGFLAG WARNING_VAR)
    check_c_compiler_flag("-W${WARNINGFLAG}" HAVE_W_${WARNING_VAR})
    if(HAVE_W_${WARNING_VAR})
        set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wno-${WARNINGFLAG}")
    endif()
endmacro()

macro(xmp_check_function SYMBOL HEADER VARNAME)
    check_function_exists(${SYMBOL} ${VARNAME}_F)
    check_symbol_exists(${SYMBOL} "${HEADER}" ${VARNAME})
    if(${VARNAME} AND ${VARNAME}_F)
        add_definitions(-D${VARNAME}=1)
    endif()
endmacro()

macro(xmp_check_include HEADER VARNAME)
    check_include_file(${HEADER} ${VARNAME})
    if(${VARNAME})
        add_definitions(-D${VARNAME}=1)
    endif()
endmacro()


# ======== Flags and warning ========

string(TOLOWER "${CMAKE_BUILD_TYPE}" CMAKE_BUILD_TYPE_LOWER)

set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -DNDEBUG")
set(CMAKE_C_FLAGS_MINSIZEREL "${CMAKE_C_FLAGS_MINSIZEREL} -DNDEBUG")
set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -DDEBUG -D_DEBUG")

if(MSVC)
    # Force to always compile with W4
    if(CMAKE_C_FLAGS MATCHES "/W[0-4]")
        string(REGEX REPLACE "/W[0-4]" "/W4" CMAKE_C_FLAGS "${CMAKE_C_FLAGS}")
    else()
        set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} /W4")
    endif()
    # Disable bogus MSVC warnings
    add_definitions(-D_CRT_SECURE_NO_WARNINGS)
    # Tune warnings
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} /wd4244 /wd4018 /wd4996 /wd4048 /wd4267 /wd4127")
else()
    xmp_add_warning_flag("-Wall" ALL)
    xmp_add_warning_flag("-Wwrite-strings" WRITE_STRINGS)
    xmp_add_warning_flag("-Wextra" EXTRA)
    if(NOT HAVE_W_EXTRA)
        xmp_add_warning_flag("-W" W)
    endif()
    xmp_disable_warning_flag("unknown-warning-option" NO_UNKNOWN_WARNING)
    xmp_disable_warning_flag("unused-parameter" NO_UNUSED_PARAMETER)
    xmp_disable_warning_flag("sign-compare" NO_SIGN_COMPARE)
endif()


# ======== Checks ========

if(UNIX AND NOT (WIN32 OR APPLE OR HAIKU OR EMSCRIPTEN OR BEOS))
    find_library(LIBM_LIBRARY m)
    if(LIBM_LIBRARY) # No need to link it by an absolute path
        set(LIBM_REQUIRED 1)
        set(LIBM_LIBRARY m)
        set(LIBM "-lm") # for pkgconfig file.
        list(APPEND CMAKE_REQUIRED_LIBRARIES m)
    endif()
    mark_as_advanced(LIBM_LIBRARY)
endif()

TEST_BIG_ENDIAN(WORDS_BIGENDIAN)
if(WORDS_BIGENDIAN)
    add_definitions(-DWORDS_BIGENDIAN=1)
endif()


cmake_push_check_state()
if(LIBM_REQUIRED)
    set(CMAKE_REQUIRED_LIBRARIES ${LIBM_LIBRARY})
endif()
xmp_check_function(powf "math.h" HAVE_POWF)
cmake_pop_check_state()

xmp_check_include(alloca.h HAVE_ALLOCA_H)
xmp_check_function(popen "stdio.h" HAVE_POPEN)
xmp_check_function(fnmatch "fnmatch.h" HAVE_FNMATCH)
xmp_check_function(umask "sys/stat.h" HAVE_UMASK)
xmp_check_function(wait "sys/wait.h" HAVE_WAIT)
xmp_check_function(mkstemp "stdlib.h" HAVE_MKSTEMP)

check_include_file(unistd.h HAVE_UNISTD_H)

if(HAVE_UNISTD_H)
    xmp_check_function(pipe "unistd.h" HAVE_PIPE)
    xmp_check_function(fork "unistd.h" HAVE_FORK)
    xmp_check_function(execvp "unistd.h" HAVE_EXECVP)
    xmp_check_function(dup2 "unistd.h" HAVE_DUP2)
endif()

if(AMIGA)
    xmp_check_include(proto/xfdmaster.h HAVE_PROTO_XFDMASTER_H)
endif()

check_symbol_exists(opendir "dirent.h" HAVE_DIRENT)
check_function_exists(opendir HAVE_OPENDIR)
check_function_exists(readdir HAVE_READDIR)
if(HAVE_DIRENT AND HAVE_OPENDIR AND HAVE_READDIR)
    add_definitions(-DHAVE_DIRENT=1)
endif()


# Symbol visibility attributes check
if(NOT (WIN32 OR CYGWIN OR AMIGA OR OS2))

    cmake_push_check_state()
    set(CMAKE_REQUIRED_FLAGS "-fvisibility=hidden -Werror")

    check_c_source_compiles("__attribute__((visibility(\"default\"))) int foo(void);
                             __attribute__((visibility(\"hidden\")))  int bar(void);
                             int foo (void) { return 0; }
                             int bar (void) { return 1; }
                             int main(void) { return 0; }" HAVE_VISIBILITY)

    check_c_source_compiles("__attribute__((visibility(\"default\"),externally_visible)) int foo(void);
                             int foo (void) { return 0; }
                             int main(void) { return 0; }" HAVE_EXTERNAL_VISIBILITY)

    set(CMAKE_REQUIRED_FLAGS "-Werror=attributes")
    check_c_source_compiles("void foo(void) __attribute__((__symver__(\"foo@bar\")));
                             int main(void) { return 0; }" HAVE_ATTRIBUTE_SYMVER)

    cmake_pop_check_state()

    if(HAVE_VISIBILITY)
        # Note: LIBXMP_DEFINES and LIBXMP_DEFINES should be defined externally before to include the "build_pros.cmake"
        list(APPEND LIBXMP_CFLAGS -fvisibility=hidden)
        list(APPEND LIBXMP_DEFINES -DXMP_SYM_VISIBILITY)

        if(HAVE_EXTERNAL_VISIBILITY)
            list(APPEND LIBXMP_DEFINES -DHAVE_EXTERNAL_VISIBILITY)
        endif()
    endif()

    if(HAVE_ATTRIBUTE_SYMVER)
        list(APPEND LIBXMP_DEFINES -DHAVE_ATTRIBUTE_SYMVER)
    endif()
endif()
