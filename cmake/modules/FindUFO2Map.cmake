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

# Find Ufo2Map binary

include(FindPackageHandleStandardArgs)

# Add prefered search path to builded ufo2map binary
set(CMAKE_PROGRAM_PATH ${CMAKE_BINARY_DIR})

find_program(UFO2Map_EXECUTABLE NAMES ufo2map)

find_package_handle_standard_args(UFO2Map REQUIRED_VARS UFO2Map_EXECUTABLE)

if (UFO2Map_FOUND)
   mark_as_advanced(UFO2Map_EXECUTABLE)
endif()

if (UFO2Map_FOUND AND NOT TARGET ufo2map)
   add_executable(ufo2map IMPORTED)
   set_property(TARGET ufo2map PROPERTY IMPORTED_LOCATION ${UFO2Map_EXECUTABLE})
endif()
