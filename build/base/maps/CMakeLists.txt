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

# Project settings
project(maps DESCRIPTION "Compiles the maps" LANGUAGES NONE)

find_package(UFO2Map)

file(GLOB_RECURSE UMPS
	RELATIVE ${CMAKE_SOURCE_DIR}/base
	CONFIGURE_DEPENDS
	${CMAKE_CURRENT_SOURCE_DIR}/*.ump
)

foreach (UMP IN ITEMS ${UMPS})
	set(BLD_UMP ${CMAKE_BINARY_DIR}/base/${UMP})
	set(SRC_UMP ${CMAKE_SOURCE_DIR}/base/${UMP})
	add_custom_command(OUTPUT ${BLD_UMP}
		COMMAND ${CMAKE_COMMAND} -E copy ${SRC_UMP} ${BLD_UMP}
		DEPENDS ${SRC_UMP}
		COMMENT "${UMP} has been updated, copy it to the building root"
	)
	list(APPEND BLD_UMPS ${BLD_UMP})
endforeach(UMP)

add_custom_target(sync_ump DEPENDS ${BLD_UMPS})

file(GLOB_RECURSE MAPS_SRC
	RELATIVE ${CMAKE_SOURCE_DIR}/base
	CONFIGURE_DEPENDS
	${CMAKE_CURRENT_SOURCE_DIR}/*.map
)

foreach (MAP IN ITEMS ${MAPS_SRC})
	set(BLD_MAP ${CMAKE_BINARY_DIR}/base/${MAP})
	string(REPLACE .map .bsp BSP ${BLD_MAP})
	set(SRC_MAP ${CMAKE_SOURCE_DIR}/base/${MAP})
	# Sync *.map between source and build dirs
	add_custom_command(OUTPUT ${BLD_MAP}
		COMMAND ${CMAKE_COMMAND} -E copy ${SRC_MAP} ${BLD_MAP}
		DEPENDS ${SRC_MAP}
		COMMENT "${MAP} has been updated, copy it to the building root"
	)
	add_custom_command(OUTPUT ${BSP}
		COMMAND ufo2map -v 4 -nice 19 -quant 4 -soft ${MAP}
		DEPENDS ${BLD_MAP}
		WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
		COMMENT "Creating ${BSP}"
	)
	list(APPEND BSP_list ${BSP})
endforeach(MAP)

add_custom_target(maps DEPENDS ${BSP_list})
add_dependencies(maps ufo2map sync_ump)

set(BSPS ${MAPS_SRC})
list(TRANSFORM BSPS REPLACE .map .bsp)

add_custom_command(OUTPUT ${CMAKE_BINARY_DIR}/base/0maps.pk3
	COMMAND ${CMAKE_COMMAND} -E tar cf ${CMAKE_BINARY_DIR}/base/0maps.pk3 --format=zip ${BSPS} ${UMPS}
	DEPENDS ${BSP_list} ${BLD_UMPS}
	WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/base
	COMMENT "Creating maps pack..."
)

add_custom_target(0maps DEPENDS ${CMAKE_BINARY_DIR}/base/0maps.pk3)
add_dependencies(0maps maps)

if (TARGET pk3)
	add_dependencies(pk3 0maps)
endif()
