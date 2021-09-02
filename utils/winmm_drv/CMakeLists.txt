
if(NOT WIN32)
    message(FATAL_ERROR "WinDriver: This component is for Windows Platform only")
endif()

if(WIN32 AND CMAKE_COMPILER_IS_GNUCXX)
    option(WITH_WINMMDRV_PTHREADS    "Link libwinpthreads statically (when using pthread-based builds)" ON)
    option(WITH_WINMMDRV_MINGWEX     "Link libmingwex statically (when using vanilla MinGW builds)" OFF)
endif()

#====================================
#          Driver module
#====================================

set(ADLMIDI_DRIVER_SRC
    src/winmm_drv.def
    src/winmm_drv.cpp
    src/MidiSynth.cpp
    config/regconfig.c
)

add_library(adlmididrv MODULE ${ADLMIDI_DRIVER_SRC})
set_target_properties(adlmididrv PROPERTIES PREFIX "")
target_link_libraries(adlmididrv PRIVATE winmm ADLMIDI_static)
target_include_directories(adlmididrv PRIVATE config)
target_compile_definitions(adlmididrv PRIVATE
    -D_USRDLL
    -D_WINDLL
    -DMM_WIN32DRV_EXPORTS
    -DENABLE_REG_SERVER
)

if(WIN32 AND CMAKE_COMPILER_IS_GNUCXX)
    set_property(TARGET adlmididrv APPEND_STRING PROPERTY LINK_FLAGS " -static-libgcc -static-libstdc++ -Wl,--enable-stdcall-fixup")
    if(WITH_WINMMDRV_PTHREADS)
        set_property(TARGET adlmididrv APPEND_STRING PROPERTY LINK_FLAGS " -Wl,-Bstatic,--whole-archive -lpthread -Wl,-Bdynamic,--no-whole-archive")
    endif()
    if(WITH_WINMMDRV_MINGWEX)
        set_property(TARGET adlmididrv APPEND_STRING PROPERTY LINK_FLAGS " -Wl,-Bstatic,--whole-archive -lmingwex -Wl,-Bdynamic,--no-whole-archive")
    endif()
endif()

if(NOT MSVC)
    target_compile_options(adlmididrv PRIVATE "-Wno-cast-function-type")
endif()



#====================================
#        Install utility
#====================================

set(ADLMIDI_INSTALLER_SRC
    installer/drvinst.c
)

if(NOT MSVC)
    list(APPEND ADLMIDI_INSTALLER_SRC
        installer/drvinst.rc
    )
endif()

add_executable(drvsetup ${ADLMIDI_INSTALLER_SRC})

target_compile_definitions(drvsetup PRIVATE
    -D_CRT_SECURE_CPP_OVERLOAD_STANDARD_NAMES=1
    -D_CRT_SECURE_CPP_OVERLOAD_STANDARD_NAMES_COUNT=1
    -D_CRT_SECURE_NO_WARNINGS
)

if(WIN32 AND CMAKE_COMPILER_IS_GNUCXX)
    set_property(TARGET drvsetup APPEND_STRING PROPERTY LINK_FLAGS " -static-libgcc")
    if(WITH_WINMMDRV_MINGWEX)
        set_property(TARGET drvsetup APPEND_STRING PROPERTY LINK_FLAGS " -Wl,-Bstatic,--whole-archive -lmingwex -Wl,-Bdynamic,--no-whole-archive")
    endif()
endif()

configure_file(installer/install.bat ${CMAKE_BINARY_DIR}/install.bat COPYONLY)
configure_file(installer/uninstall.bat ${CMAKE_BINARY_DIR}/uninstall.bat COPYONLY)


#====================================
#  A unit test to verify the driver
#====================================

add_executable(drvtest test/test.c)
target_link_libraries(drvtest PRIVATE winmm)
target_compile_options(drvtest PRIVATE "-Wno-cast-function-type")
if(WITH_WINMMDRV_MINGWEX)
    set_property(TARGET drvtest APPEND_STRING PROPERTY LINK_FLAGS " -Wl,-Bstatic,--whole-archive -lmingwex -Wl,-Bdynamic,--no-whole-archive")
endif()


#====================================
#       Control panel applet
#====================================

set(ADLMIDI_DRIVER_SRC
    cpl/adlconfig.def
    cpl/adlconfig.rc
    cpl/config_dialog.c
    config/regconfig.c
)

add_library(adlmidiconfig MODULE ${ADLMIDI_DRIVER_SRC} cpl/adlconfig.c cpl/res-cpl.rc)
set_target_properties(adlmidiconfig PROPERTIES PREFIX "" OUTPUT_NAME "libadlconfig" SUFFIX ".cpl")
target_include_directories(adlmidiconfig PRIVATE config)
target_compile_definitions(adlmidiconfig PRIVATE
    -D_CRT_SECURE_CPP_OVERLOAD_STANDARD_NAMES=1
    -D_CRT_SECURE_CPP_OVERLOAD_STANDARD_NAMES_COUNT=1
    -D_CRT_SECURE_NO_WARNINGS
    -D_UNICODE
    -DUNICODE
    -DADL_IS_CPL_APPLET
)

target_link_libraries(adlmidiconfig PRIVATE comctl32 gdi32 user32)


add_executable(adlmidiconfigtool ${ADLMIDI_DRIVER_SRC} cpl/adlconfig-tool.c cpl/res-tool.rc)
set_target_properties(adlmidiconfigtool PROPERTIES WIN32_EXECUTABLE ON)
target_include_directories(adlmidiconfigtool PRIVATE config)
target_compile_definitions(adlmidiconfigtool PRIVATE
    -D_CRT_SECURE_CPP_OVERLOAD_STANDARD_NAMES=1
    -D_CRT_SECURE_CPP_OVERLOAD_STANDARD_NAMES_COUNT=1
    -D_CRT_SECURE_NO_WARNINGS
    -D_UNICODE
    -DUNICODE
)

target_link_libraries(adlmidiconfigtool PRIVATE comctl32 gdi32 user32)

if(WIN32 AND CMAKE_COMPILER_IS_GNUCXX)
    target_compile_options(adlmidiconfig PRIVATE "-Wno-cast-function-type")
    target_compile_options(adlmidiconfigtool PRIVATE "-Wno-cast-function-type")
    set_property(TARGET adlmidiconfig APPEND_STRING PROPERTY LINK_FLAGS " -static-libgcc -Wl,--enable-stdcall-fixup")
    set_property(TARGET adlmidiconfigtool APPEND_STRING PROPERTY LINK_FLAGS " -static-libgcc -Wl,--enable-stdcall-fixup")
    if(WITH_WINMMDRV_MINGWEX)
        set_property(TARGET adlmidiconfig APPEND_STRING PROPERTY LINK_FLAGS " -Wl,-Bstatic,--whole-archive -lmingwex -Wl,-Bdynamic,--no-whole-archive")
        set_property(TARGET adlmidiconfigtool APPEND_STRING PROPERTY LINK_FLAGS " -Wl,-Bstatic,--whole-archive -lmingwex -Wl,-Bdynamic,--no-whole-archive")
    endif()
endif()


#install(TARGETS adlmididrv
#        RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}")
