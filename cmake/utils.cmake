include(CMakeParseArguments)
include(CMakePackageConfigHelpers)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/joinpaths")
include(JoinPaths)

# pkgcfg_configure - write a configured pkg-config.pc file
#	Required parameters:
#		- FILE_IN: pkg-config template (*.pc.in file)
#		- FILE_OUT: path to write the configured pkg-config file to (*.pc file)
#	Arguments:
#		- NAME: package name
#		- DESC: package description
#		- VERSION: package version
#		- DEFINES: C macros
#		- CFLAGS: compiler flags
#		- LDFLAGS_PRIV: link flags (PRIVATE)
#		- LDFLAGS_PUB: link flags (PUBLIC)
#		- PKGS_PRIV: required pkg-config packages (PRIVATE)
#		- PKGS_PUB: required pkg-config packages (PUBLIC)
function(pkgcfg_configure FILE_IN FILE_OUT)
	set(args_single NAME DESC VERSION)
	set(args_multi DEFINES CFLAGS LDFLAGS_PRIV LDFLAGS_PUB PKGS_PRIV PKGS_PUB)
	cmake_parse_arguments(PKGCFG "" "${args_single}" "${args_multi}" ${ARGN})
	
	foreach(DEF ${PKGCFG_DEFINES})
		string(REPLACE " " "" DEF "${DEF}")	# remove spaces from defines
		list(APPEND PKGCFG_CFLAGS "-D${DEF}")
	endforeach(DEF)
	
	# CMake uses ; to separate values, in pkg-config we use spaces.
	string(REPLACE ";" " " PKGCFG_CFLAGS "${PKGCFG_CFLAGS}")
	string(REPLACE ";" " " PKGCFG_LDFLAGS_PRIV "${PKGCFG_LDFLAGS_PRIV}")
	string(REPLACE ";" " " PKGCFG_LDFLAGS_PUB "${PKGCFG_LDFLAGS_PUB}")
	string(REPLACE ";" " " PKGCFG_PKGS_PRIV "${PKGCFG_PKGS_PRIV}")
	string(REPLACE ";" " " PKGCFG_PKGS_PUB "${PKGCFG_PKGS_PUB}")

	# JoinPaths does not support Windows yet, use previous behaviour there to avoid regressions
	if(WIN32)
		set(libdir_for_pc_file "\${prefix}/lib")
		set(includedir_for_pc_file "\${prefix}/include")
	else()
		join_paths(libdir_for_pc_file "\${exec_prefix}" "${CMAKE_INSTALL_LIBDIR}")
		join_paths(includedir_for_pc_file "\${prefix}" "${CMAKE_INSTALL_INCLUDEDIR}")
	endif(WIN32)

	configure_file("${FILE_IN}" "${FILE_OUT}" @ONLY)
endfunction()

# cmake_cfg_install - generate and install a CMake Package Configuration file
#	Required parameters:
#		- FILE_IN: cmake config template (config.cmake.in file)
#	Arguments:
#		- NAME: package name (used for folder and namespace)
#		- VERSION: package version
#		- TARGETS: CMake targets to install
#		- DEPS: dependencies
function(cmake_cfg_install CFG_TEMPLATE)
	set(args_single NAME VERSION)
	set(args_multi TARGETS DEPS)
	cmake_parse_arguments(CMCFG "" "${args_single}" "${args_multi}" ${ARGN})
	
	set(PACKAGE_DEPENDENCIES "")
	foreach(dep IN LISTS CMCFG_DEPS)
		string(APPEND PACKAGE_DEPENDENCIES "find_dependency(${dep})\n")
	endforeach()
	
	set(CONFIG_DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/${CMCFG_NAME}")
	set(TARGETS_FILENAME "${CMCFG_NAME}Targets.cmake")	# Note: This variable is used by config.cmake.in.
	set(CONFIG_FILENAME "${CMCFG_NAME}Config.cmake")
	set(CFGVER_FILENAME "${CMCFG_NAME}ConfigVersion.cmake")
	
	configure_package_config_file("${CFG_TEMPLATE}" "${CMAKE_CURRENT_BINARY_DIR}/${CONFIG_FILENAME}"
		INSTALL_DESTINATION "${CONFIG_DESTINATION}"
		)
	write_basic_package_version_file("${CMAKE_CURRENT_BINARY_DIR}/${CFGVER_FILENAME}"
		VERSION ${CMCFG_VERSION}
		COMPATIBILITY SameMajorVersion)
	
	install(EXPORT "${CMCFG_NAME}"
		DESTINATION "${CONFIG_DESTINATION}"
		NAMESPACE "${CMCFG_NAME}::"
		FILE "${TARGETS_FILENAME}")
	install(FILES "${CMAKE_CURRENT_BINARY_DIR}/${CONFIG_FILENAME}"
			"${CMAKE_CURRENT_BINARY_DIR}/${CFGVER_FILENAME}"
		DESTINATION "${CONFIG_DESTINATION}"
		)
	
	# register project in local user package registry
	export(TARGETS ${CMCFG_TARGETS}
		NAMESPACE "${CMCFG_NAME}::"
		FILE "${CMAKE_CURRENT_BINARY_DIR}/${TARGETS_FILENAME}")
	export(PACKAGE ${CMCFG_NAME})
endfunction()
