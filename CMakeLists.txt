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

# Project settings
project(ufoai)
set(UFOAI_PROJECT_UFOSLICER "ufoslicer")

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

set(UFOAI_SRC "${CMAKE_SOURCE_DIR}/src")

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
endif()

## Projects we want to build
option(UFOAI_BUILD_UFOSLICER "Build the tool for creating fancy map overviews" ON)

## We check for dependencies
set(UFOAI_DEPENDENCIES_DIR "$ENV{UFOAI_DEPS}" CACHE PATH "Path to prebuilt UFOAI dependencies")
if(WIN32 AND MINGW)
   set(UFOAI_ARCH_SUFIX "/mingw32")
elseif(WIN32 AND MSVC)
   set(UFOAI_ARCH_SUFIX "/msvc")
elseif()
   set(UFOAI_ARCH_SUFIX "")
endif()
set(UFOAI_DEP_SEARCH_PATH
      ${UFOAI_DEPENDENCIES_DIR}/ogg/include
      ${UFOAI_DEPENDENCIES_DIR}/ogg/lib${UFOAI_ARCH_SUFIX}
      ${UFOAI_DEPENDENCIES_DIR}/SDL2/include
      ${UFOAI_DEPENDENCIES_DIR}/SDL2/lib${UFOAI_ARCH_SUFIX}
      ${UFOAI_DEPENDENCIES_DIR}/vorbis/include
      ${UFOAI_DEPENDENCIES_DIR}/vorbis/lib${UFOAI_ARCH_SUFIX}
)

message(STATUS "Search path dependencies: ${UFOAI_DEP_SEARCH_PATH}")

# hints to the find_package calls
set(CMAKE_PREFIX_PATH ${UFOAI_DEP_SEARCH_PATH} ${CMAKE_PREFIX_PATH})

## Packages
if(UFOAI_BUILD_UFOSLICER)
   find_package(JPEG)
   find_package(X11)
   set(OpenGL_GL_PREFERENCE "LEGACY")
   find_package(OpenGL)
   find_package(PNG)
endif()

if(UFOAI_BUILD_UFOSLICER)
   find_package(SDL)
   find_package(ZLIB)
endif()

# -std=c++11 is bugged on cygwin/mingw
if(WIN32 AND MINGW)
   set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=gnu++0x")
elseif(!MSVC)
   set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++0x")
endif()

###############################################################################
# game
###############################################################################

add_subdirectory("src/game")

###############################################################################
# ufo2map
###############################################################################

add_subdirectory("src/tools/ufo2map")

###############################################################################
# ufoded
###############################################################################

add_subdirectory("src/server")

###############################################################################
# ufo
###############################################################################

add_subdirectory("src/client")

###############################################################################
# ufomodel
###############################################################################

add_subdirectory("src/tools/ufomodel")

###############################################################################
# ufoslicer
###############################################################################

if(UFOAI_BUILD_UFOSLICER)
   set(UFOAI_SOURCEFILES_UFOSLICER
      ${UFOAI_SRC}/common/bspslicer.cpp
      ${UFOAI_SRC}/common/files.cpp
      ${UFOAI_SRC}/common/ioapi.cpp
      ${UFOAI_SRC}/common/list.cpp
      ${UFOAI_SRC}/common/mem.cpp
      ${UFOAI_SRC}/common/unzip.cpp

      ${UFOAI_SRC}/shared/aabb.cpp
      ${UFOAI_SRC}/shared/byte.cpp
      ${UFOAI_SRC}/shared/images.cpp
      ${UFOAI_SRC}/shared/mathlib.cpp
      ${UFOAI_SRC}/shared/parse.cpp
      ${UFOAI_SRC}/shared/shared.cpp
      ${UFOAI_SRC}/shared/utf8.cpp

      ${UFOAI_SRC}/tools/ufo2map/common/bspfile.cpp
      ${UFOAI_SRC}/tools/ufo2map/common/scriplib.cpp
      ${UFOAI_SRC}/tools/ufoslicer.cpp
   )

   # platform specific sources
   if(WIN32)
      SET(UFOAI_SOURCEFILES_UFOSLICER ${UFOAI_SOURCEFILES_UFOSLICER}
         ${UFOAI_SRC}/ports/windows/win_shared.cpp
      )
   elseif(UNIX)
      SET(UFOAI_SOURCEFILES_UFOSLICER ${UFOAI_SOURCEFILES_UFOSLICER}
         ${UFOAI_SRC}/ports/unix/unix_files.cpp
         ${UFOAI_SRC}/ports/unix/unix_shared.cpp
         ${UFOAI_SRC}/ports/unix/unix_main.cpp
      )
   elseif(SOLARIS)
      SET(UFOAI_SOURCEFILES_UFOSLICER ${UFOAI_SOURCEFILES_UFOSLICER}
         ${UFOAI_SRC}/ports/unix/unix_files.cpp
         ${UFOAI_SRC}/ports/unix/unix_shared.cpp
         ${UFOAI_SRC}/ports/unix/unix_main.cpp
      )
   elseif(APPLE)
      SET(UFOAI_SOURCEFILES_UFOSLICER ${UFOAI_SOURCEFILES_UFOSLICER}
         ${UFOAI_SRC}/ports/unix/unix_files.cpp
         ${UFOAI_SRC}/ports/unix/unix_shared.cpp
         ${UFOAI_SRC}/ports/unix/unix_main.cpp
      )
   endif()

   # ufoslicer is executable
   add_executable(${UFOAI_PROJECT_UFOSLICER} ${UFOAI_SOURCEFILES_UFOSLICER})

   # For generic no-config case (e.g. with mingw, gcc, ...)
   set_target_properties(${UFOAI_PROJECT_UFOSLICER} PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
   # For multi-config builds like msvc
   foreach(OUTPUTCONFIG ${CMAKE_CONFIGURATION_TYPES})
      string(TOUPPER ${OUTPUTCONFIG} OUTPUTCONFIG)
      set_target_properties(${UFOAI_PROJECT_UFOSLICER} PROPERTIES RUNTIME_OUTPUT_DIRECTORY_${OUTPUTCONFIG}  ${CMAKE_BINARY_DIR})
   endforeach(OUTPUTCONFIG CMAKE_CONFIGURATION_TYPES)

   target_compile_definitions(${UFOAI_PROJECT_UFOSLICER} PRIVATE -DCOMPILE_MAP)

   target_include_directories(${UFOAI_PROJECT_UFOSLICER}
      PRIVATE ${SDL_INCLUDE_DIR}
      PRIVATE ${JPEG_INCLUDE_DIRS}
      PRIVATE ${PNG_INCLUDE_DIRS}
      PRIVATE ${ZLIB_INCLUDE_DIRS}
   )

   if(MINGW)
      target_link_libraries(${UFOAI_PROJECT_UFOSLICER}
         mingw32
      )
   endif()

   target_link_libraries(${UFOAI_PROJECT_UFOSLICER}
      ${SDL_LIBRARY_MAIN}
      ${SDL_LIBRARY}
      ${JPEG_LIBRARIES}
      ${PNG_LIBRARIES}
      ${ZLIB_LIBRARIES}
   )

   if(WIN32)
      target_link_libraries(${UFOAI_PROJECT_UFOSLICER}
         imm32
         version
         winmm
      )
   endif()
   if(CMAKE_SYSTEM_NAME MATCHES "BSD")
      target_link_libraries(${UFOAI_PROJECT_UFOSLICER}
        execinfo
      )
   endif()
endif()

###############################################################################
# ufotestall
###############################################################################

add_subdirectory("src/tests/")

###############################################################################
# uforadiant
###############################################################################

add_subdirectory("src/tools/radiant/")
