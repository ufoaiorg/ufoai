# Copyright (C) 2002-2024 UFO: Alien Invasion.
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

project(game DESCRIPTION "Tactical battle library of UFO:AI.")

find_package(SDL)
find_package(Lua 5.3)

set(UFOAI_GAMELIB_SOURCES
	../shared/aabb.cpp
	../shared/infostring.cpp
	../shared/mathlib.cpp
	../shared/parse.cpp
	../shared/shared.cpp
	../shared/utf8.cpp

	chr_shared.cpp
	g_actor.cpp
	g_ai.cpp
	g_ai_lua.cpp
	g_camera.cpp
	g_client.cpp
	g_cmds.cpp
	g_combat.cpp
	g_edicts.cpp
	g_events.cpp
	g_func.cpp
	g_health.cpp
	g_inventory.cpp
	g_main.cpp
	g_match.cpp
	g_mission.cpp
	g_morale.cpp
	g_move.cpp
	g_reaction.cpp
	g_round.cpp
	g_spawn.cpp
	g_stats.cpp
	g_svcmds.cpp
	g_trigger.cpp
	g_utils.cpp
	g_vis.cpp
	inv_shared.cpp
	inventory.cpp
	q_shared.cpp
)

# game is a shared library
add_library(${PROJECT_NAME} SHARED ${UFOAI_GAMELIB_SOURCES})

# we want the target library to be game.xxx and not libgame.xxx
set_target_properties(${PROJECT_NAME} PROPERTIES PREFIX "")
if(UNIX)
	set(UFOAI_GAMELIB_OUTPUT_TYPE "LIBRARY_OUTPUT_DIRECTORY")
else()
	set(UFOAI_GAMELIB_OUTPUT_TYPE "RUNTIME_OUTPUT_DIRECTORY")
endif()
# For generic no-config case (e.g. with mingw, gcc, ...)
set_target_properties(${PROJECT_NAME} PROPERTIES ${UFOAI_GAMELIB_OUTPUT_TYPE} ${CMAKE_BINARY_DIR}/base)
# For multi-config builds like msvc
set(UFOAI_GAMELIB_OUTPUT_TYPE ${UFOAI_GAMELIB_OUTPUT_TYPE}_)
foreach(OUTPUTCONFIG ${CMAKE_CONFIGURATION_TYPES})
	string(TOUPPER ${OUTPUTCONFIG} OUTPUTCONFIG)
	set_target_properties(${PROJECT_NAME} PROPERTIES ${UFOAI_GAMELIB_OUTPUT_TYPE}${OUTPUTCONFIG}  ${CMAKE_BINARY_DIR}/base)
endforeach()

target_compile_definitions(${PROJECT_NAME} PRIVATE -DCOMPILE_UFO)

target_include_directories(${PROJECT_NAME}
	PRIVATE ${LUA_INCLUDE_DIR}
	PRIVATE ${SDL_INCLUDE_DIR}
)

target_link_libraries(${PROJECT_NAME}
	${LUA_LIBRARY}
)
