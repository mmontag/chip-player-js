set(CMAKE_C_STANDARD 99)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/../../cmake")
find_package(LIBVLC REQUIRED)
message("Found ${LIBVLCCORE_LIBRARY} of version ${LIBVLC_VERSION} with includes ${LIBVLC_INCLUDE_DIR}")

set(adlmidi_vlc_src)
list(APPEND adlmidi_vlc_src libadlmidi.c)

add_library(adlmidi_vlc_plugin MODULE ${adlmidi_vlc_src})
set_target_properties(adlmidi_vlc_plugin PROPERTIES OUTPUT_NAME adlmidi_plugin)
target_compile_options(adlmidi_vlc_plugin PUBLIC
            "-DVLC_MODULE_COPYRIGHT=\"Copyright \(c\) Vitaly Novichkov\""
            "-DVLC_MODULE_LICENSE=\"GPLv3\""
            "-DMODULE_STRING=\"adlmidi\""
            ${VLCPLUGIN_DEFINITIONS})

target_include_directories(adlmidi_vlc_plugin PRIVATE
        ${LIBVLC_INCLUDE_DIR}/vlc/plugins
        ${LIBVLC_INCLUDE_DIR}/vlc)

target_link_libraries(adlmidi_vlc_plugin PRIVATE ADLMIDI_static ${LIBVLCCORE_LIBRARY} m)

if(WIN32 AND CMAKE_COMPILER_IS_GNUCXX)
    set_property(TARGET adlmidi_vlc_plugin APPEND_STRING PROPERTY LINK_FLAGS " -static-libgcc -static-libstdc++")
    set_property(TARGET adlmidi_vlc_plugin APPEND_STRING PROPERTY LINK_FLAGS " -Wl,-Bstatic,--whole-archive -lpthread -Wl,-Bdynamic,--no-whole-archive")
elseif(NOT WIN32)
    target_link_libraries(adlmidi_vlc_plugin PRIVATE stdc++)
endif()

# target_link_libraries(adlmidi_vlc_plugin INTERFACE "-export-symbol-regex ^vlc_entry")

if(NOT VLC_PLUGIN_NOINSTALL)
    install(TARGETS adlmidi_vlc_plugin DESTINATION "${VLCPLUGIN_CODEC_INSTALL_PATH}")
else()
    install(TARGETS adlmidi_vlc_plugin DESTINATION "${CMAKE_INSTALL_LIBDIR}/vlc-codec/")
endif()
