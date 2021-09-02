function(find_mingw_dll _FieldName _FileName _DestList _SearchPaths)
    find_file(MINGWDLL_${_FieldName} ${_FileName} PATHS "${_SearchPaths}")
    if(MINGWDLL_${_FieldName})
        list(APPEND ${_DestList} "${MINGWDLL_${_FieldName}}")
        set(${_DestList} ${${_DestList}} PARENT_SCOPE)
    endif()
endfunction()

set(MINGW_BIN_PATH $ENV{MinGW})

if(NOT MINGW_BIN_PATH)
    set(MINGW_BIN_PATH "${QT_BINLIB_DIR}")
else()
    string(REPLACE "\\" "/" MINGW_BIN_PATH $ENV{MinGW})
endif()

set(MINGW_DLLS)
find_mingw_dll(LIBGCCDW         "libgcc_s_dw2-1.dll" MINGW_DLLS "${MINGW_BIN_PATH}")
find_mingw_dll(LIBGCCSJLJ       "libgcc_s_sjlj-1.dll" MINGW_DLLS "${MINGW_BIN_PATH}")
find_mingw_dll(LIBGCCSEC        "libgcc_s_seh-1.dll" MINGW_DLLS "${MINGW_BIN_PATH}")
find_mingw_dll(MINGWEX          "libmingwex-0.dll" MINGW_DLLS "${MINGW_BIN_PATH}")
find_mingw_dll(WINPTHREAD       "libwinpthread-1.dll" MINGW_DLLS "${MINGW_BIN_PATH}")
find_mingw_dll(WINPTHREADGC3    "pthreadGC-3.dll" MINGW_DLLS "${MINGW_BIN_PATH}")
find_mingw_dll(STDCPP           "libstdc++-6.dll" MINGW_DLLS "${MINGW_BIN_PATH}")

message("MinGW DLLs: [${MINGW_DLLS}]")

install(FILES
    ${MINGW_DLLS}
    DESTINATION "${PGE_INSTALL_DIRECTORY}/"
)

add_custom_target(copy_mingw_dlls DEPENDS pge_windeploy)
foreach(MingwRuntimeDll ${MINGW_DLLS})
    add_custom_command(TARGET copy_mingw_dlls POST_BUILD
                       COMMAND ${CMAKE_COMMAND} -E
                       copy ${MingwRuntimeDll} "${CMAKE_INSTALL_PREFIX_ORIG}/${PGE_INSTALL_DIRECTORY}"
    )
endforeach()

