set(CMAKE_C_STANDARD 99)

find_package(LIBVLC REQUIRED)

add_library(adlmidi_vlc_plugin MODULE libadlmidi.c)
set_target_properties(adlmidi_vlc_plugin PROPERTIES OUTPUT_NAME adlmidi_plugin)
target_compile_definitions(adlmidi_vlc_plugin PUBLIC
    "VLC_MODULE_COPYRIGHT=\"Copyright (c) Vitaly Novichkov\""
    "VLC_MODULE_LICENSE=\"GPLv3\""
    "MODULE_STRING=\"adlmidi\""
)

target_link_libraries(adlmidi_vlc_plugin PRIVATE ADLMIDI_static libvlc::plugin m)

if(WIN32 AND CMAKE_COMPILER_IS_GNUCXX)
    set_property(TARGET adlmidi_vlc_plugin APPEND_STRING PROPERTY LINK_FLAGS " -static-libgcc -static-libstdc++")
    set_property(TARGET adlmidi_vlc_plugin APPEND_STRING PROPERTY LINK_FLAGS " -Wl,-Bstatic,--whole-archive -lpthread -Wl,-Bdynamic,--no-whole-archive")
endif()

if(NOT VLC_PLUGIN_NOINSTALL)
    install(TARGETS adlmidi_vlc_plugin DESTINATION "${VLCPLUGIN_CODEC_INSTALL_PATH}")
else()
    install(TARGETS adlmidi_vlc_plugin DESTINATION "${CMAKE_INSTALL_LIBDIR}/vlc-codec/")
endif()
