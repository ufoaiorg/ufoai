# Copyright (C) 2002-2023 UFO: Alien Invasion.
#
# This program is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public License
# as published by the Free Software Foundation; either version 2
# of the License, or (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
#
# See the GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

cmake_minimum_required(VERSION 3.13)

project(ufo2map DESCRIPTION "Map compiler for UFO:AI.")

find_package(JPEG)
find_package(X11)
set(OpenGL_GL_PREFERENCE "LEGACY")
find_package(OpenGL)
find_package(PNG)
find_package(SDL)
find_package(ZLIB)

set(UFO2MAP_SOURCES
	../../common/files.cpp
	../../common/ioapi.cpp
	../../common/list.cpp
	../../common/mem.cpp
	../../common/routing.cpp
	../../common/tracing.cpp
	../../common/unzip.cpp

	../../shared/aabb.cpp
	../../shared/byte.cpp
	../../shared/entitiesdef.cpp
	../../shared/images.cpp
	../../shared/mathlib.cpp
	../../shared/parse.cpp
	../../shared/shared.cpp
	../../shared/utf8.cpp

	check/check.cpp
	check/checkentities.cpp
	check/checklib.cpp

	common/aselib.cpp
	common/bspfile.cpp
	common/polylib.cpp
	common/scriplib.cpp
	common/trace.cpp

	bsp.cpp
	bspbrush.cpp
	csg.cpp
	faces.cpp
	levels.cpp
	lighting.cpp
	lightmap.cpp
	map.cpp
	patches.cpp
	portals.cpp
	routing.cpp
	textures.cpp
	threads.cpp
	tree.cpp
	ufo2map.cpp
	writebsp.cpp
)

# platform specific sources
if(WIN32)
	list(APPEND UFO2MAP_SOURCES
		../../ports/windows/win_shared.cpp
	)
elseif(UNIX)
	list(APPEND UFO2MAP_SOURCES
		../../ports/unix/unix_files.cpp
		../../ports/unix/unix_shared.cpp
		../../ports/unix/unix_main.cpp
	)
elseif(SOLARIS)
	list(APPEND UFO2MAP_SOURCES
		../../ports/unix/unix_files.cpp
		../../ports/unix/unix_shared.cpp
		../../ports/unix/unix_main.cpp
	)
elseif(APPLE)
	list(APPEND UFO2MAP_SOURCES
		../../ports/unix/unix_files.cpp
		../../ports/unix/unix_shared.cpp
		../../ports/unix/unix_main.cpp
	)
endif()

# ufo2map is executable
add_executable(${PROJECT_NAME} ${UFO2MAP_SOURCES})

# For generic no-config case (e.g. with mingw, gcc, ...)
set_target_properties(${PROJECT_NAME} PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
# For multi-config builds like msvc
foreach(OUTPUTCONFIG ${CMAKE_CONFIGURATION_TYPES})
	string(TOUPPER ${OUTPUTCONFIG} OUTPUTCONFIG)
	set_target_properties(${PROJECT_NAME} PROPERTIES RUNTIME_OUTPUT_DIRECTORY_${OUTPUTCONFIG} ${CMAKE_BINARY_DIR})
endforeach(OUTPUTCONFIG CMAKE_CONFIGURATION_TYPES)

target_compile_definitions(${PROJECT_NAME} PRIVATE -DCOMPILE_MAP)

target_include_directories(${PROJECT_NAME}
	PRIVATE ${SDL_INCLUDE_DIR}
	PRIVATE ${JPEG_INCLUDE_DIRS}
	PRIVATE ${PNG_INCLUDE_DIRS}
	PRIVATE ${ZLIB_INCLUDE_DIRS}
)

if(MINGW)
	target_link_libraries(${PROJECT_NAME}
	mingw32
	)
endif()

target_link_libraries(${PROJECT_NAME}
	${SDL_LIBRARY_MAIN}
	${SDL_LIBRARY}
	${JPEG_LIBRARIES}
	${PNG_LIBRARIES}
	${ZLIB_LIBRARIES}
)

if(WIN32)
	target_link_libraries(${PROJECT_NAME}
	imm32
	version
	winmm
	)
endif()

if(CMAKE_SYSTEM_NAME MATCHES "BSD")
	target_link_libraries(${PROJECT_NAME}
	execinfo
	)
endif()
