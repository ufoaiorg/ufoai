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

cmake_minimum_required(VERSION 3.15)

# Project settings
project(ufoai LANGUAGES NONE)

# cmake paths
set(CMAKE_CONFIG_DIR "${CMAKE_SOURCE_DIR}/cmake/config")
list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake/modules")

if(NOT CMAKE_BUILD_TYPE)
   message(STATUS "CMake build type is not set, defaulting to 'RelWithDebInfo'")
   set(CMAKE_BUILD_TYPE "RelWithDebInfo")
endif()
message(STATUS "CMake build type is set to ${CMAKE_BUILD_TYPE}")

if("${CMAKE_BUILD_TYPE}" MATCHES "Debug" OR "${CMAKE_BUILD_TYPE}" MATCHES "RelWithDebInfo")
   message(STATUS "Adding #define DEBUG")
   add_definitions(-DDEBUG)
endif()

if(MSVC)
   add_definitions(-D_CRT_SECURE_NO_WARNINGS -D_CRT_NONSTDC_NO_DEPRECATE)
endif()

# For some reason, when building against win xp, WINVER and _WIN32_WINNT needs to be defined
if(WIN32 AND CMAKE_SYSTEM_VERSION)
   set(win_ver ${CMAKE_SYSTEM_VERSION})
   string(REPLACE "10" "A" win_ver ${win_ver})
   string(REPLACE "." "" win_ver ${win_ver})
   string(REGEX REPLACE "([A-F0-9])" "0\\1" win_ver ${win_ver})
   string(SUBSTRING ${win_ver} 0 4 win_ver)
   set(win_ver "0x${win_ver}")

   message(STATUS "Windows version: ${win_ver}")

   add_definitions(-DWINVER=${win_ver})
   add_definitions(-D_WIN32_WINNT=${win_ver})
endif()

# MSVC2013 conflicts std::min/std::max by including minwindef.h. To avoid that, we can define NOMINMAX
if(WIN32 AND MSVC)
   add_definitions(-DNOMINMAX)
endif()

# If the project is not built in the source directory, we copy needed data. Note that we do not create links because we assume
# that if one do not use the source directory to build, he doesn't want to have the generated files in his source either.
if(NOT CMAKE_BINARY_DIR STREQUAL CMAKE_SOURCE_DIR)
   message(STATUS "You are not building in the source dir. Needed files will be copied if not already present")
   foreach(RES base radiant)
      if(NOT EXISTS ${CMAKE_BINARY_DIR}/${RES})
         message(STATUS "Copying ${CMAKE_BINARY_DIR}/${RES}. This may take some time")
         file(COPY ${CMAKE_SOURCE_DIR}/${RES} DESTINATION ${CMAKE_BINARY_DIR})
      endif()
   endforeach(RES)
   file(GLOB CHAPTERS RELATIVE ${CMAKE_SOURCE_DIR} ${CMAKE_SOURCE_DIR}/src/docs/tex/chapters_*)
   foreach(CHAP ${CHAPTERS})
      if(NOT EXISTS ${CMAKE_BINARY_DIR}/${CHAP})
         file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/${CHAP})
      endif()
   endforeach(CHAP)
endif()

# -std=c++11 is bugged on cygwin/mingw
if(WIN32 AND MINGW)
   set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=gnu++0x")
elseif(!MSVC)
   set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++0x")
endif()

add_custom_target(tools) # Complex target for console developer tools

option(DISABLE_GAME "Disable tactical battle library building")
option(DISABLE_UFO "Disable standalone client building")
option(DISABLE_UFODED "Disable dedicated server building")
option(DISABLE_TESTS "Disable Google tests building")
option(DISABLE_TOOLS "Disable console developer tools building")
option(DISABLE_UFO2MAP "Disable ufo2map map compiler building")
option(DISABLE_UFOMODEL "Disable ufomodel lightmap pre-compiler building")
option(DISABLE_UFOSLICER "Disable ufoslicer 2D map overlays creator building")
option(DISABLE_UFORADIANT "Disable UFORadiant map editor building")
option(DISABLE_I18N "Disable game translations building")
option(DISABLE_MANUAL "Disable the user manual(s) building")
option(DISABLE_DOXYGEN_DOCS "Disable doxygen documentation generating")
option(DISABLE_BASE_PACKAGES "Disable compress game data packages")
option(DISABLE_MAPS_COMPILE "Disable the maps compile and pack")

if(NOT DISABLE_GAME)
   add_subdirectory("src/game")
   set(TEXTDOMAINS "${PROJECT_NAME}")
endif()
if(NOT DISABLE_UFO AND NOT DISABLE_GAME)
   add_subdirectory("src/client")
endif()
if(NOT DISABLE_UFODED AND NOT DISABLE_GAME) 
   add_subdirectory("src/server")
endif()
if(NOT DISABLE_UFO2MAP AND NOT DISABLE_TOOLS)
   add_subdirectory("src/tools/ufo2map")
endif()
if(NOT DISABLE_UFOMODEL AND NOT DISABLE_TOOLS) 
   add_subdirectory("src/tools/ufomodel")
endif()
if(NOT DISABLE_UFOSLICER AND NOT DISABLE_TOOLS)
   add_subdirectory("src/tools/ufoslicer")
endif()
if(NOT DISABLE_TESTS AND NOT DISABLE_GAME) 
   add_subdirectory("src/tests")
endif()
if(NOT DISABLE_UFORADIANT)
   add_subdirectory("src/tools/radiant")
   set(TEXTDOMAINS "${TEXTDOMAINS}" "uforadiant")
endif()
if(NOT DISABLE_I18N)
   add_subdirectory("src/po")
endif()
if(NOT DISABLE_MANUAL)
   add_subdirectory("src/docs/tex")
endif()
if(NOT DISABLE_DOXYGEN_DOCS)
   add_subdirectory("src/docs")
endif()
if(NOT DISABLE_BASE_PACKAGES)
   add_subdirectory("base")
endif()
if(NOT DISABLE_MAPS_COMPILE)
   add_subdirectory("base/maps")
endif()
