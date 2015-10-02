include(CMakeParseArguments)
include(FindPackageHandleStandardArgs)

#
# put a variable into the global namespace
#
macro(ufo_var_global VARIABLES)
	foreach(VAR ${VARIABLES})
		set(${VAR} ${${VAR}} CACHE STRING "" FORCE)
		mark_as_advanced(${VAR})
	endforeach()
endmacro()

#
# Add external dependency. It will trigger a find_package and use the system wide install if found, otherwise the bundled version
# If you set USE_BUILTIN the system wide is ignored.
#
# parameters:
# LIB:
# CFLAGS:
# LINKERFLAGS:
# SRCS: the list of source files for the bundled lib
# DEFINES: a list of defines (without -D or /D)
#
macro(ufo_add_library)
	set(_OPTIONS_ARGS)
	set(_ONE_VALUE_ARGS LIB CFLAGS LINKERFLAGS)
	set(_MULTI_VALUE_ARGS SRCS DEFINES)

	cmake_parse_arguments(_ADDLIB "${_OPTIONS_ARGS}" "${_ONE_VALUE_ARGS}" "${_MULTI_VALUE_ARGS}" ${ARGN} )

	if (NOT _ADDLIB_LIB)
		message(FATAL_ERROR "ufo_add_library requires the LIB argument")
	endif()
	if (NOT _ADDLIB_SRCS)
		message(FATAL_ERROR "ufo_add_library requires the SRCS argument")
	endif()

	find_package(${_ADDLIB_LIB})
	string(TOUPPER ${_ADDLIB_LIB} PREFIX)
	if (${PREFIX}_FOUND)
		message(STATUS "System wide installation found for ${_ADDLIB_LIB}")
		ufo_var_global(${PREFIX}_FOUND)
	else()
		message(STATUS "No system wide installation found, use built-in version of ${_ADDLIB_LIB}")
		set(${PREFIX}_LIBRARIES ${_ADDLIB_LIB})
		set(${PREFIX}_LINKERFLAGS ${_ADDLIB_LINKERFLAGS})
		#set(${PREFIX}_COMPILERFLAGS "${_ADDLIB_DEFINES} ${_ADDLIB_CFLAGS}")
		set(${PREFIX}_DEFINITIONS ${_ADDLIB_DEFINES})
		set(${PREFIX}_INCLUDE_DIRS ${ROOT_DIR}/src/libs/${_ADDLIB_LIB})
		add_library(${_ADDLIB_LIB} STATIC ${_ADDLIB_SRCS})
		include_directories(${${PREFIX}_INCLUDE_DIRS})
		set_target_properties(${_ADDLIB_LIB} PROPERTIES COMPILE_DEFINITIONS "${${PREFIX}_DEFINITIONS}")
		if (NOT MSVC)
			set_target_properties(${_ADDLIB_LIB} PROPERTIES COMPILE_FLAGS "${_ADDLIB_CFLAGS}")
		endif()
		set_target_properties(${_ADDLIB_LIB} PROPERTIES FOLDER ${_ADDLIB_LIB})
		ufo_var_global(${PREFIX}_LIBRARIES)
		ufo_var_global(${PREFIX}_LINKERFLAGS)
		ufo_var_global(${PREFIX}_INCLUDE_DIRS)
		ufo_var_global(${PREFIX}_DEFINITIONS)
	endif()
	set(${PREFIX}_EXTERNAL ON)
	ufo_var_global(${PREFIX}_EXTERNAL)
endmacro()

#
# macro for the FindLibName.cmake files. If USE_BUILTIN is set we don't search for system wide installs at all.
#
# parameters:
# LIB: the library we are trying to find
# HEADER: the header we are trying to find
# SUFFIX: suffix for the include dir
#
# Example: ufo_find(SDL2_image SDL_image.h include/SDL2)
#
macro(ufo_find LIB HEADER SUFFIX)
	string(TOUPPER ${LIB} PREFIX_)
	STRING(REGEX REPLACE "^LIB" "" PREFIX ${PREFIX_})
	message(STATUS "Checking for ${PREFIX} (${LIB})")
	if (NOT USE_BUILTIN)
		if(CMAKE_SIZEOF_VOID_P EQUAL 8)
			set(_PROCESSOR_ARCH "x64")
		else()
			set(_PROCESSOR_ARCH "x86")
		endif()
		set(_SEARCH_PATHS
			~/Library/Frameworks
			/Library/Frameworks
			/usr/local
			/usr
			/sw # Fink
			/opt/local # DarwinPorts
			/opt/csw # Blastwave
			/opt
		)
		find_package(PkgConfig)
		if (LINK_STATIC_LIBS)
			set(PKG_PREFIX _${PREFIX}_STATIC)
		else()
			set(PKG_PREFIX _${PREFIX})
		endif()
		if (PKG_CONFIG_FOUND)
			pkg_check_modules(_${PREFIX} ${LIB})
			if (_${PREFIX}_FOUND)
				set(${PREFIX}_COMPILERFLAGS ${${PKG_PREFIX}_CFLAGS_OTHER})
				ufo_var_global(${PREFIX}_COMPILERFLAGS)
				set(${PREFIX}_LINKERFLAGS ${${PKG_PREFIX}_LDFLAGS_OTHER})
				ufo_var_global(${PREFIX}_LINKERFLAGS)
				set(${PREFIX}_FOUND ${${PKG_PREFIX}_FOUND})
				ufo_var_global(${PREFIX}_FOUND)
				set(${PREFIX}_INCLUDE_DIRS ${${PKG_PREFIX}_INCLUDE_DIRS})
				ufo_var_global(${PREFIX}_INCLUDE_DIRS)
				set(${PREFIX}_LIBRARY_DIRS ${${PKG_PREFIX}_LIBRARY_DIRS})
				ufo_var_global(${PREFIX}_LIBRARY_DIRS)
				set(${PREFIX}_LIBRARIES ${${PKG_PREFIX}_LIBRARIES})
				ufo_var_global(${PREFIX}_LIBRARIES)
			endif()
		endif()
		if (NOT ${PKG_PREFIX}_FOUND)
			find_path(${PREFIX}_INCLUDE_DIRS ${HEADER}
				HINTS ENV ${PREFIX}DIR
				PATH_SUFFIXES include ${SUFFIX}
				PATHS
					${${PKG_PREFIX}_INCLUDE_DIRS}}
					${_SEARCH_PATHS}
			)
			find_library(${PREFIX}_LIBRARIES
				${LIB}
				HINTS ENV ${PREFIX}DIR
				PATH_SUFFIXES lib64 lib lib/${_PROCESSOR_ARCH}
				PATHS
					${${PKG_PREFIX}_LIBRARY_DIRS}}
					${_SEARCH_PATHS}
			)
			find_package_handle_standard_args(${LIB} DEFAULT_MSG ${PREFIX}_LIBRARIES ${PREFIX}_INCLUDE_DIRS)
			ufo_var_global(${PREFIX}_INCLUDE_DIRS)
			ufo_var_global(${PREFIX}_LIBRARIES)
			ufo_var_global(${PREFIX}_FOUND)
		endif()
		if (${PREFIX}_FOUND)
			message(STATUS "LIBS: ${${PREFIX}_LIBRARIES}")
			message(STATUS "INCLUDE_DIRS: ${${PREFIX}_INCLUDE_DIRS}")
		else()
			message(STATUS "${LIB} not found")
		endif()
	endif()
endmacro()

macro(ufo_recurse_resolve_dependencies LIB DEPS)
	list(APPEND ${DEPS} ${LIB})
	get_property(_DEPS GLOBAL PROPERTY ${LIB}_DEPS)
	foreach(DEP ${_DEPS})
		ufo_recurse_resolve_dependencies(${DEP} ${DEPS})
	endforeach()
endmacro()

macro(ufo_resolve_dependencies LIB DEPS)
	get_property(_DEPS GLOBAL PROPERTY ${LIB}_DEPS)
	list(APPEND ${DEPS} ${LIB})
	foreach(DEP ${_DEPS})
		ufo_recurse_resolve_dependencies(${DEP} ${DEPS})
	endforeach()
endmacro()

macro(ufo_unique INPUTSTR OUT)
	string(REPLACE " " ";" TMP "${INPUTSTR}")
	list(REVERSE TMP)
	list(REMOVE_DUPLICATES TMP)
	list(REVERSE TMP)
	string(REPLACE ";" " " ${OUT} "${TMP}")
endmacro()

#
# Specify libraries or flags to use when linking a given target
#
# Example:
# ufo_target_link_libraries(TARGET ufoded LIBS shared common some_external_lib)
# Note: This automatically resolved all dependencies for you. If e.g. common would rely on
# shared, you don't have to specify shared here if you don't directly depend on it.
#
macro(ufo_target_link_libraries)
	set(_OPTIONS_ARGS)
	set(_ONE_VALUE_ARGS TARGET)
	set(_MULTI_VALUE_ARGS LIBS)

	cmake_parse_arguments(_LINKLIBS "${_OPTIONS_ARGS}" "${_ONE_VALUE_ARGS}" "${_MULTI_VALUE_ARGS}" ${ARGN} )

	if (NOT _LINKLIBS_TARGET)
		message(FATAL_ERROR "ufo_target_link_libraries requires the TARGET argument")
	endif()
	if (NOT _LINKLIBS_LIBS)
		message(FATAL_ERROR "ufo_target_link_libraries requires the LIBS argument")
	endif()
	set(LINK_LIBS)
	foreach(LIB ${_LINKLIBS_LIBS})
		ufo_resolve_dependencies(${LIB} LINK_LIBS)
	endforeach()
	if (LINK_LIBS)
		list(REVERSE LINK_LIBS)
		list(REMOVE_DUPLICATES LINK_LIBS)
		list(REVERSE LINK_LIBS)
	endif()

	string(TOUPPER ${_LINKLIBS_TARGET} TARGET)

	set(LINKERFLAGS)
	set(COMPILERFLAGS)
	set(LIBRARIES)
	set(DEFINITIONS)
	set(INCLUDE_DIRS)
	if (${_LINKLIBS_TARGET}_INCLUDE_DIRS)
		list(APPEND INCLUDE_DIRS ${${_LINKLIBS_TARGET}_INCLUDE_DIRS})
	endif()

	foreach(LIB ${LINK_LIBS})
		string(TOUPPER ${LIB} PREFIX)
		if (${PREFIX}_LINKERFLAGS)
			list(APPEND LINKERFLAGS ${${PREFIX}_LINKERFLAGS})
		endif()
		if (${PREFIX}_COMPILERFLAGS)
			list(APPEND COMPILERFLAGS ${${PREFIX}_COMPILERFLAGS})
		endif()
		if (${PREFIX}_DEFINITIONS)
			list(APPEND DEFINITIONS ${${PREFIX}_DEFINITIONS})
		endif()
		if (NOT ${PREFIX}_EXTERNAL)
			list(APPEND LIBRARIES ${LIB})
		endif()
		if (${PREFIX}_LIBRARIES)
			list(APPEND LIBRARIES ${${PREFIX}_LIBRARIES})
		endif()
		if (${PREFIX}_INCLUDE_DIRS)
			list(APPEND INCLUDE_DIRS ${${PREFIX}_INCLUDE_DIRS})
		endif()
	endforeach()
	if (LINKERFLAGS)
		ufo_unique("${LINKERFLAGS}" ${TARGET}_LINKERFLAGS)
		ufo_var_global(${TARGET}_LINKERFLAGS)
		set_target_properties(${_LINKLIBS_TARGET} PROPERTIES LINK_FLAGS "${${TARGET}_LINKERFLAGS}")
	endif()
	if (COMPILERFLAGS)
		ufo_unique("${COMPILERFLAGS}" ${TARGET}_COMPILERFLAGS)
		ufo_var_global(${TARGET}_COMPILERFLAGS)
		set_target_properties(${_LINKLIBS_TARGET} PROPERTIES COMPILE_FLAGS "${${TARGET}_COMPILERFLAGS}")
	endif()
	if (DEFINITIONS)
		set(${TARGET}_DEFINITIONS ${DEFINITIONS})
		list(REMOVE_DUPLICATES ${TARGET}_DEFINITIONS)
		ufo_var_global(${TARGET}_DEFINITIONS)
		set_target_properties(${_LINKLIBS_TARGET} PROPERTIES COMPILE_DEFINITIONS "${DEFINITIONS}")
	endif()

	set_property(GLOBAL PROPERTY ${_LINKLIBS_TARGET}_DEPS ${_LINKLIBS_LIBS})
	if (LIBRARIES)
		list(REVERSE LIBRARIES)
		list(REMOVE_DUPLICATES LIBRARIES)
		list(REVERSE LIBRARIES)
		target_link_libraries(${_LINKLIBS_TARGET} ${LIBRARIES})
	endif()
	if (INCLUDE_DIRS)
		list(REMOVE_DUPLICATES INCLUDE_DIRS)
		include_directories(${INCLUDE_DIRS})
	endif()
endmacro()

#
# set up the binary for the application. This will also set up platform specific stuff for you
#
# Example: ufo_add_executable(TARGET SomeTargetName SRCS Source.cpp Main.cpp WINDOWED VERSION 1.0)
#
macro(ufo_add_executable)
	set(_OPTIONS_ARGS WINDOWED)
	set(_ONE_VALUE_ARGS TARGET VERSION)
	set(_MULTI_VALUE_ARGS SRCS)

	cmake_parse_arguments(_EXE "${_OPTIONS_ARGS}" "${_ONE_VALUE_ARGS}" "${_MULTI_VALUE_ARGS}" ${ARGN} )

	if (WINDOWED)
		if (WINDOWS)
			add_executable(${_EXE_TARGET} WIN32 ${_EXE_SRCS})
		elseif (DARWIN OR IOS)
			add_executable(${_EXE_TARGET} MACOSX_BUNDLE ${_EXE_SRCS})
		else()
			add_executable(${_EXE_TARGET} ${_EXE_SRCS})
		endif()
	else()
		add_executable(${_EXE_TARGET} ${_EXE_SRCS})
	endif()

	set_target_properties(${_EXE_TARGET} PROPERTIES FOLDER ${_EXE_TARGET})
endmacro()
