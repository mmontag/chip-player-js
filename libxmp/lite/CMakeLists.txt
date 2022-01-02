cmake_minimum_required(VERSION 3.2)
project(libxmplite VERSION 4.5.0 LANGUAGES C)

set(LIBXMP_DEFINES)
set(LIBXMP_DEFINES_PUBLIC)
set(LIBXMP_CFLAGS)

include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/libxmp-checks.cmake)

# -fPIC thing
if(NOT WIN32 AND NOT EMSCRIPTEN AND NOT VITA)
    option(LIBXMP_PIC   "Build the static library as position-independent" OFF)
else()
    set(LIBXMP_PIC OFF)
endif()
# -----------------------------------

if(EMSCRIPTEN OR VITA OR PSP)
    set(BUILD_SHARED_DEFAULT OFF)
else()
    set(BUILD_SHARED_DEFAULT ON)
endif()

option(BUILD_STATIC   "Build libXMP static library" ON)
option(BUILD_SHARED   "Build libXMP shared library" ${BUILD_SHARED_DEFAULT})
if(NOT BUILD_STATIC AND NOT BUILD_SHARED)
    message(FATAL_ERROR "Both static and shared builds got disabled. You must enable at least one of them.")
endif()

option(LIBXMP_DISABLE_IT            "Disable IT format support in libXMP-lite" OFF)

include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/libxmp-sources.cmake)

list(APPEND LIBXMP_DEFINES_PUBLIC
    -DLIBXMP_CORE_PLAYER
    -DLIBXMP_NO_PROWIZARD
    -DLIBXMP_NO_DEPACKERS
)

if(LIBXMP_DISABLE_IT)
    list(APPEND LIBXMP_DEFINES_PUBLIC -DLIBXMP_CORE_DISABLE_IT)
endif()

if(MSVC)
    set(LIBXMP_MSVC_DEFINES -D_USE_MATH_DEFINES)
    list(APPEND LIBXMP_DEFINES ${LIBXMP_MSVC_DEFINES})
endif()


set(XMP_INSTALLS)

if(BUILD_STATIC)
    add_library(xmp_lite_static STATIC ${LIBXMP_SRC_LIST})
    list(APPEND XMP_INSTALLS xmp_lite_static)
    set_target_properties(xmp_lite_static PROPERTIES C_STANDARD 90)
    if(MSVC)
        set_target_properties(xmp_lite_static PROPERTIES OUTPUT_NAME libxmp-lite-static)
    else()
        set_target_properties(xmp_lite_static PROPERTIES OUTPUT_NAME xmp-lite)
    endif()

    target_compile_options(xmp_lite_static PRIVATE ${LIBXMP_CFLAGS})
    target_compile_definitions(xmp_lite_static PRIVATE ${LIBXMP_DEFINES} -DBUILDING_STATIC)
    target_compile_definitions(xmp_lite_static PUBLIC ${LIBXMP_DEFINES_PUBLIC})

    if(LIBXMP_PIC)
        target_compile_options(xmp_lite_static PRIVATE -fPIC)
    endif()

    target_include_directories(xmp_lite_static PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include/libxmp-lite)

    if(LIBM_REQUIRED)
        target_link_libraries(xmp_lite_static PUBLIC ${LIBM_LIBRARY})
    endif()
endif()

if(BUILD_SHARED)
    add_library(xmp_lite_shared SHARED ${LIBXMP_SRC_LIST})
    list(APPEND XMP_INSTALLS xmp_lite_shared)
    set_target_properties(xmp_lite_shared PROPERTIES C_STANDARD 90)
    if(MSVC)
        set_target_properties(xmp_lite_shared PROPERTIES OUTPUT_NAME libxmp-lite)
    else()
        set_target_properties(xmp_lite_shared PROPERTIES OUTPUT_NAME xmp-lite)
    endif()

    if(HAVE_GCC OR HAVE_CLANG)
        if(APPLE)
            set_property(TARGET xmp_lite_shared APPEND_STRING PROPERTY
                         LINK_FLAGS " -Wl,-undefined,error")
        elseif(NOT CMAKE_SYSTEM_NAME STREQUAL "OpenBSD")
            cmake_push_check_state()
            set(CMAKE_REQUIRED_FLAGS "-Wl,--no-undefined")
            check_c_compiler_flag("" HAVE_NO_UNDEFINED)
            cmake_pop_check_state()
            if(HAVE_NO_UNDEFINED)
                set_property(TARGET xmp_lite_shared APPEND_STRING PROPERTY
                             LINK_FLAGS " -Wl,--no-undefined")
            endif()
        endif()
    endif()

    if(HAVE_VISIBILITY AND HAVE_GNU_LD AND UNIX)
        target_compile_definitions(xmp_lite_shared PRIVATE -DUSE_VERSIONED_SYMBOLS=1)
        set_property(TARGET xmp_lite_shared APPEND_STRING PROPERTY
                     LINK_FLAGS " -Wl,--version-script,${CMAKE_CURRENT_SOURCE_DIR}/libxmp.map")
        message(STATUS "Versioned symbols: ENABLED.")
    else()
        message(STATUS "Versioned symbols: DISABLED")
    endif()

    target_compile_definitions(xmp_lite_shared PRIVATE ${LIBXMP_DEFINES})
    if(WIN32)
        target_compile_definitions(xmp_lite_shared PRIVATE -DBUILDING_DLL)
    endif()

    target_compile_options(xmp_lite_shared PRIVATE ${LIBXMP_CFLAGS})
    target_compile_definitions(xmp_lite_shared PUBLIC ${LIBXMP_DEFINES_PUBLIC})

    target_include_directories(xmp_lite_shared PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include/libxmp-lite)

    set_target_properties(xmp_lite_shared PROPERTIES VERSION ${libxmplite_VERSION} SOVERSION ${libxmplite_VERSION_MAJOR})

    if(LIBM_REQUIRED)
        target_link_libraries(xmp_lite_shared PUBLIC ${LIBM_LIBRARY})
    endif()
endif()


add_library(XMP_IF INTERFACE)

if(NOT BUILD_SHARED)
    target_link_libraries(XMP_IF INTERFACE xmp_lite_static)
    target_compile_definitions(XMP_IF INTERFACE -DBUILDING_STATIC)
else()
    target_link_libraries(XMP_IF INTERFACE xmp_lite_shared)
endif()

# === Unit tests ====
option(WITH_UNIT_TESTS "Enable unit testing" OFF)
if(WITH_UNIT_TESTS)
    enable_testing()
    add_subdirectory(test)
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/build_examples.cmake")
    include(build_examples.cmake)
endif()


# === Install ====
include(GNUInstallDirs)

set(prefix ${CMAKE_INSTALL_PREFIX})
set(exec_prefix "\${prefix}")
set(includedir "${CMAKE_INSTALL_FULL_INCLUDEDIR}")
set(libdir     "${CMAKE_INSTALL_FULL_LIBDIR}")
set(bindir     "${CMAKE_INSTALL_FULL_BINDIR}")
configure_file("${CMAKE_CURRENT_SOURCE_DIR}/libxmp-lite.pc.in"
               "${CMAKE_CURRENT_BINARY_DIR}/libxmp-lite.pc" @ONLY
)
install(TARGETS ${XMP_INSTALLS}
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
)
install(FILES
        include/libxmp-lite/xmp.h
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/libxmp-lite
)
install(FILES "${CMAKE_CURRENT_BINARY_DIR}/libxmp-lite.pc"
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/pkgconfig
)
