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

project(ufoded DESCRIPTION "UFO:AI Dedicated server.")

find_package(CURL)
find_package(SDL)
find_package(ZLIB)
find_package(Lua 5.3)

set(UFODED_SOURCES
	../common/bsp.cpp
	../common/cmd.cpp
	../common/cmodel.cpp
	../common/common.cpp
	../common/cvar.cpp
	../common/dbuffer.cpp
	../common/files.cpp
	../common/grid.cpp
	../common/http.cpp
	../common/ioapi.cpp
	../common/list.cpp
	../common/md4.cpp
	../common/md5.cpp
	../common/mem.cpp
	../common/msg.cpp
	../common/net.cpp
	../common/netpack.cpp
	../common/pqueue.cpp
	../common/routing.cpp
	../common/scripts.cpp
	../common/sha1.cpp
	../common/sha2.cpp
	../common/tracing.cpp
	../common/unzip.cpp

	../game/chr_shared.cpp
	../game/inv_shared.cpp
	../game/q_shared.cpp

	sv_ccmds.cpp
	sv_clientstub.cpp
	sv_game.cpp
	sv_init.cpp
	sv_log.cpp
	sv_main.cpp
	sv_mapcycle.cpp
	sv_rma.cpp
	sv_send.cpp
	sv_user.cpp
	sv_world.cpp

	../shared/aabb.cpp
	../shared/bfd.cpp
	../shared/byte.cpp
	../shared/infostring.cpp
	../shared/mathlib.cpp
	../shared/parse.cpp
	../shared/shared.cpp
	../shared/stringhunk.cpp
	../shared/utf8.cpp
)

# platform specific sources
if(WIN32)
	LIST(APPEND UFODED_SOURCES
		../ports/windows/win_backtrace.cpp
		../ports/windows/win_console.cpp
		../ports/windows/win_main.cpp
		../ports/windows/win_shared.cpp
		../ports/windows/ufoded.rc
	)
elseif(UNIX)
	LIST(APPEND UFODED_SOURCES
		../ports/linux/linux_main.cpp
		../ports/unix/unix_console.cpp
		../ports/unix/unix_files.cpp
		../ports/unix/unix_shared.cpp
		../ports/unix/unix_main.cpp
	)
elseif(SOLARIS)
	LIST(APPEND UFODED_SOURCES
		../ports/solaris/solaris_main.cpp
		../ports/unix/unix_console.cpp
		../ports/unix/unix_files.cpp
		../ports/unix/unix_shared.cpp
		../ports/unix/unix_main.cpp
	)
elseif(APPLE)
	LIST(APPEND UFODED_SOURCES
		../ports/macosx/osx_main.cpp
		../ports/macosx/osx_shared.cpp
		../ports/unix/unix_console.cpp
		../ports/unix/unix_files.cpp
		../ports/unix/unix_shared.cpp
		../ports/unix/unix_main.cpp
	)
endif()

# ufoded is executable
add_executable(${PROJECT_NAME} ${UFODED_SOURCES})
add_dependencies(${PROJECT_NAME} "game")

# For generic no-config case (e.g. with mingw, gcc, ...)
set_target_properties(${PROJECT_NAME} PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
# For multi-config builds like msvc
foreach(OUTPUTCONFIG ${CMAKE_CONFIGURATION_TYPES})
	string(TOUPPER ${OUTPUTCONFIG} OUTPUTCONFIG)
	set_target_properties(${PROJECT_NAME} PROPERTIES RUNTIME_OUTPUT_DIRECTORY_${OUTPUTCONFIG}  ${CMAKE_BINARY_DIR})
endforeach()

# since we embed our manifest, we should not generate a default one
if(WIN32 AND MSVC)
	set_target_properties(${PROJECT_NAME} PROPERTIES LINK_FLAGS "/MANIFEST:NO")
endif()

target_compile_definitions(${PROJECT_NAME} PRIVATE -DCOMPILE_UFO)
target_compile_definitions(${PROJECT_NAME} PRIVATE -DDEDICATED_ONLY)
target_compile_definitions(${PROJECT_NAME} PRIVATE -DPATH_MANIFEST_FILE="${CMAKE_SOURCE_DIR}/build/projects/ufo.exe.manifest")
target_compile_definitions(${PROJECT_NAME} PRIVATE -DPATH_UFOICON_FILE="${CMAKE_SOURCE_DIR}/build/projects/ufo.ico")
target_compile_definitions(${PROJECT_NAME} PRIVATE -DPATH_UFODEDICON_FILE="${CMAKE_SOURCE_DIR}/build/projects/ufoded.ico")

target_include_directories(${PROJECT_NAME}
	PRIVATE ${SDL_INCLUDE_DIR}
	PRIVATE ${CURL_INCLUDE_DIRS}
	PRIVATE ${ZLIB_INCLUDE_DIRS}
	PRIVATE ${LUA_INCLUDE_DIR}
)

target_link_libraries(${PROJECT_NAME}
	${SDL_LIBRARY}
	${CURL_LIBRARIES}
	${ZLIB_LIBRARIES}
	${LUA_LIBRARY}
)

if(WIN32)
	target_link_libraries(${PROJECT_NAME}
		imm32
		version
		winmm
		Ws2_32
		wldap32
		rtm
	)
endif()

if(CMAKE_SYSTEM_NAME MATCHES "BSD")
	target_link_libraries(${PROJECT_NAME}
		execinfo
	)
endif()
