
# CMake module to search for LIBVLC (VLC library)
# Authors: Rohit Yadav <rohityadav89@gmail.com>
#          Harald Sitter <apachelogger@ubuntu.com>
#          Alexander Grund <git@grundis.de>
#
# If it's found it defines the following targets:
#   - libvlc::libvlc 
#   - libvlc::plugin 

if(NOT WIN32)
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(PC_LIBVLC libvlc QUIET)
    set(LIBVLC_DEFINITIONS ${PC_LIBVLC_CFLAGS_OTHER})

    pkg_check_modules(PC_VLCPLUGIN vlc-plugin QUIET)
    set(VLCPLUGIN_DEFINITIONS ${PC_VLCPLUGIN_CFLAGS_OTHER})

    pkg_get_variable(PC_VLCPLUGIN_PLUGINS_PATH vlc-plugin pluginsdir)
    set(VLCPLUGIN_CODEC_INSTALL_PATH ${PC_VLCPLUGIN_PLUGINS_PATH}/codec)
else()
    set(LIBVLC_DEFINITIONS)
    # FIXME: Is "_FILE_OFFSET_BITS=64" correct for Windows?
    set(VLCPLUGIN_DEFINITIONS
        -D__PLUGIN__
        -D_FILE_OFFSET_BITS=64
        -D_REENTRANT
        -D_THREAD_SAFE
    )
    # FIXME: Put the proper install path here
    set(VLCPLUGIN_CODEC_INSTALL_PATH "C:/Program Files/VideoLAN/VLC/plugins/codec")
endif()

foreach(lib libvlc vlcplugin)
    string(TOUPPER ${lib} upperLib)
    if(lib STREQUAL "vlcplugin")
        set(headerFile vlc_common.h)
        set(suffixes vlc/plugins include/vlc/plugins)
    else()
        set(headerFile vlc/libvlc.h)
        set(suffixes )
    endif()
    find_path(${upperLib}_INCLUDE_DIR ${headerFile}
	    HINTS ${PC_${upperLib}_INCLUDEDIR} ${PC_${upperLib}_INCLUDE_DIRS} $ENV{LIBVLC_INCLUDE_PATH}
        PATH_SUFFIXES include ${suffixes}
        NO_DEFAULT_PATH
    )
    find_path(${upperLib}_INCLUDE_DIR ${headerFile}
        PATHS $ENV{LIB_DIR}
              C:/msys/local #mingw
        PATH_SUFFIXES include ${suffixes}
    )
endforeach()

foreach(lib vlc vlccore)
    string(TOUPPER ${lib} upperLib)
    find_library(LIB${upperLib}_LIBRARY ${lib} lib${lib}
        HINTS ${PC_LIBVLC_LIBDIR} ${PC_LIBVLC_LIBRARY_DIRS} $ENV{LIBVLC_LIBRARY_PATH}
        PATH_SUFFIXES lib
        NO_DEFAULT_PATH
    )
    find_library(LIB${upperLib}_LIBRARY ${lib} lib${lib}
        PATHS $ENV{LIB_DIR}
              C:/msys/local #mingw
        PATH_SUFFIXES lib
    )
endforeach()

if(LIBVLC_INCLUDE_DIR)
    set(vlcVersionFile ${LIBVLC_INCLUDE_DIR}/vlc/libvlc_version.h)
    if(EXISTS ${vlcVersionFile})
        file(STRINGS ${vlcVersionFile} versionStrings REGEX "#[ \t]*define[ \t]+LIBVLC_VERSION_(MAJOR|MINOR|REVISION|EXTRA)[ \t]+\\(?[0-9]+\\)?")
        if(NOT versionStrings)
            message(FATAL_ERROR "Could not read version from ${vlcVersionFile}")
        endif()
        foreach(item MAJOR MINOR REVISION EXTRA)
            string(REGEX REPLACE "#[ \t]*define[ \t]+LIBVLC_VERSION_${item}[ \t]+\\(?([0-9]+)\\)?" "\\1" LIBVLC_VERSION_${item} ${versionStrings})
        endforeach()
        set(LIBVLC_VERSION ${LIBVLC_VERSION_MAJOR}.${LIBVLC_VERSION_MINOR}.${LIBVLC_VERSION_REVISION}.${LIBVLC_VERSION_EXTRA})
    endif()
endif()

include(FindPackageHandleStandardArgs)

find_package_handle_standard_args(LIBVLC
    REQUIRED_VARS LIBVLC_INCLUDE_DIR VLCPLUGIN_INCLUDE_DIR LIBVLC_LIBRARY LIBVLCCORE_LIBRARY
    VERSION_VAR LIBVLC_VERSION
)

if(LIBVLC_FOUND)
    add_library(libvlc_libvlc INTERFACE)
    target_compile_definitions(libvlc_libvlc INTERFACE ${LIBVLC_DEFINITIONS})
    target_include_directories(libvlc_libvlc INTERFACE ${LIBVLC_INCLUDE_DIR})
    target_link_libraries(libvlc_libvlc INTERFACE ${LIBVLC_LIBRARY})
    
    add_library(libvlc_plugin INTERFACE)
    target_include_directories(libvlc_plugin INTERFACE ${LIBVLC_INCLUDE_DIR} ${VLCPLUGIN_INCLUDE_DIR})
    target_compile_definitions(libvlc_plugin INTERFACE ${VLCPLUGIN_DEFINITIONS})
    target_link_libraries(libvlc_plugin INTERFACE ${LIBVLCCORE_LIBRARY})

    add_library(libvlc::libvlc ALIAS libvlc_libvlc)
    add_library(libvlc::plugin ALIAS libvlc_plugin)
endif()
