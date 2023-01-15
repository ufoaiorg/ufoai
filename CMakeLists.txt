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
set(UFOAI_PROJECT_UFO2MAP "ufo2map")
set(UFOAI_PROJECT_UFODED "ufoded")
set(UFOAI_PROJECT_UFO "ufo")
set(UFOAI_PROJECT_UFOMODEL "ufomodel")
set(UFOAI_PROJECT_UFOSLICER "ufoslicer")
set(UFOAI_PROJECT_UFOTESTALL "ufotestall")

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
option(UFOAI_BUILD_UFO2MAP "Build the tool to compile maps" ON)
option(UFOAI_BUILD_UFODED "Build the dedicated server" ON)
option(UFOAI_BUILD_UFO "Build the main game" ON)
option(UFOAI_BUILD_UFOMODEL "Build the tool for pre-compiling lightmaps for models" ON)
option(UFOAI_BUILD_UFOSLICER "Build the tool for creating fancy map overviews" ON)
option(UFOAI_BUILD_UFOTESTALL "Build unitary tests" ON)

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
if(UFOAI_BUILD_UFODED OR
      UFOAI_BUILD_UFO OR
      UFOAI_BUILD_UFOTESTALL)
   find_package(CURL)
endif()

if(UFOAI_BUILD_UFO2MAP OR
      UFOAI_BUILD_UFO OR
      UFOAI_BUILD_UFOMODEL OR
      UFOAI_BUILD_UFOSLICER OR
      UFOAI_BUILD_UFOTESTALL)
   find_package(JPEG)
   find_package(X11)
   set(OpenGL_GL_PREFERENCE "LEGACY")
   find_package(OpenGL)
   find_package(PNG)
endif()

if(UFOAI_BUILD_UFO2MAP OR
      UFOAI_BUILD_UFODED OR
      UFOAI_BUILD_UFO OR
      UFOAI_BUILD_UFOMODEL OR
      UFOAI_BUILD_UFOSLICER OR
      UFOAI_BUILD_UFOTESTALL)
   find_package(SDL)
   find_package(ZLIB)
endif()

if (UFOAI_BUILD_UFO OR
      UFOAI_BUILD_UFOTESTALL)
      find_package(Intl)
      find_package(OGG)
      find_package(Lua51)
   find_package(SDL_mixer)
   find_package(SDL_ttf)
   find_package(VORBIS)
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

if(UFOAI_BUILD_UFO2MAP)
   set(UFOAI_SOURCEFILES_UFO2MAP
      ${UFOAI_SRC}/common/files.cpp
      ${UFOAI_SRC}/common/ioapi.cpp
      ${UFOAI_SRC}/common/list.cpp
      ${UFOAI_SRC}/common/mem.cpp
      ${UFOAI_SRC}/common/routing.cpp
      ${UFOAI_SRC}/common/tracing.cpp
      ${UFOAI_SRC}/common/unzip.cpp

      ${UFOAI_SRC}/shared/aabb.cpp
      ${UFOAI_SRC}/shared/byte.cpp
      ${UFOAI_SRC}/shared/entitiesdef.cpp
      ${UFOAI_SRC}/shared/images.cpp
      ${UFOAI_SRC}/shared/mathlib.cpp
      ${UFOAI_SRC}/shared/parse.cpp
      ${UFOAI_SRC}/shared/shared.cpp
      ${UFOAI_SRC}/shared/utf8.cpp

      ${UFOAI_SRC}/tools/ufo2map/check/check.cpp
      ${UFOAI_SRC}/tools/ufo2map/check/checkentities.cpp
      ${UFOAI_SRC}/tools/ufo2map/check/checklib.cpp

      ${UFOAI_SRC}/tools/ufo2map/common/aselib.cpp
      ${UFOAI_SRC}/tools/ufo2map/common/bspfile.cpp
      ${UFOAI_SRC}/tools/ufo2map/common/polylib.cpp
      ${UFOAI_SRC}/tools/ufo2map/common/scriplib.cpp
      ${UFOAI_SRC}/tools/ufo2map/common/trace.cpp

      ${UFOAI_SRC}/tools/ufo2map/bsp.cpp
      ${UFOAI_SRC}/tools/ufo2map/bspbrush.cpp
      ${UFOAI_SRC}/tools/ufo2map/csg.cpp
      ${UFOAI_SRC}/tools/ufo2map/faces.cpp
      ${UFOAI_SRC}/tools/ufo2map/levels.cpp
      ${UFOAI_SRC}/tools/ufo2map/lighting.cpp
      ${UFOAI_SRC}/tools/ufo2map/lightmap.cpp
      ${UFOAI_SRC}/tools/ufo2map/map.cpp
      ${UFOAI_SRC}/tools/ufo2map/patches.cpp
      ${UFOAI_SRC}/tools/ufo2map/portals.cpp
      ${UFOAI_SRC}/tools/ufo2map/routing.cpp
      ${UFOAI_SRC}/tools/ufo2map/textures.cpp
      ${UFOAI_SRC}/tools/ufo2map/threads.cpp
      ${UFOAI_SRC}/tools/ufo2map/tree.cpp
      ${UFOAI_SRC}/tools/ufo2map/ufo2map.cpp
      ${UFOAI_SRC}/tools/ufo2map/writebsp.cpp
   )

   # platform specific sources
   if(WIN32)
      SET(UFOAI_SOURCEFILES_UFO2MAP ${UFOAI_SOURCEFILES_UFO2MAP}
         ${UFOAI_SRC}/ports/windows/win_shared.cpp
      )
   elseif(UNIX)
      SET(UFOAI_SOURCEFILES_UFO2MAP ${UFOAI_SOURCEFILES_UFO2MAP}
         ${UFOAI_SRC}/ports/unix/unix_files.cpp
         ${UFOAI_SRC}/ports/unix/unix_shared.cpp
         ${UFOAI_SRC}/ports/unix/unix_main.cpp
      )
   elseif(SOLARIS)
      SET(UFOAI_SOURCEFILES_UFO2MAP ${UFOAI_SOURCEFILES_UFO2MAP}
         ${UFOAI_SRC}/ports/unix/unix_files.cpp
         ${UFOAI_SRC}/ports/unix/unix_shared.cpp
         ${UFOAI_SRC}/ports/unix/unix_main.cpp
      )
   elseif(APPLE)
      SET(UFOAI_SOURCEFILES_UFO2MAP ${UFOAI_SOURCEFILES_UFO2MAP}
         ${UFOAI_SRC}/ports/unix/unix_files.cpp
         ${UFOAI_SRC}/ports/unix/unix_shared.cpp
         ${UFOAI_SRC}/ports/unix/unix_main.cpp
      )
   endif()

   # ufo2map is executable
   add_executable(${UFOAI_PROJECT_UFO2MAP} ${UFOAI_SOURCEFILES_UFO2MAP})

   # For generic no-config case (e.g. with mingw, gcc, ...)
   set_target_properties(${UFOAI_PROJECT_UFO2MAP} PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
   # For multi-config builds like msvc
   foreach(OUTPUTCONFIG ${CMAKE_CONFIGURATION_TYPES})
      string(TOUPPER ${OUTPUTCONFIG} OUTPUTCONFIG)
      set_target_properties(${UFOAI_PROJECT_UFO2MAP} PROPERTIES RUNTIME_OUTPUT_DIRECTORY_${OUTPUTCONFIG}  ${CMAKE_BINARY_DIR})
   endforeach(OUTPUTCONFIG CMAKE_CONFIGURATION_TYPES)

   target_compile_definitions(${UFOAI_PROJECT_UFO2MAP} PRIVATE -DCOMPILE_MAP)

   target_include_directories(${UFOAI_PROJECT_UFO2MAP}
      PRIVATE ${SDL_INCLUDE_DIR}
      PRIVATE ${JPEG_INCLUDE_DIRS}
      PRIVATE ${PNG_INCLUDE_DIRS}
      PRIVATE ${ZLIB_INCLUDE_DIRS}
   )

   if(MINGW)
      target_link_libraries(${UFOAI_PROJECT_UFO2MAP}
         mingw32
      )
   endif()

   target_link_libraries(${UFOAI_PROJECT_UFO2MAP}
      ${SDL_LIBRARY_MAIN}
      ${SDL_LIBRARY}
      ${JPEG_LIBRARIES}
      ${PNG_LIBRARIES}
      ${ZLIB_LIBRARIES}
   )

   if(WIN32)
      target_link_libraries(${UFOAI_PROJECT_UFO2MAP}
         imm32
         version
         winmm
      )
   endif()
   if(CMAKE_SYSTEM_NAME MATCHES "BSD")
     target_link_libraries(${UFOAI_PROJECT_UFO2MAP}
        execinfo
   )
   endif()
endif()

###############################################################################
# ufoded
###############################################################################

if(UFOAI_BUILD_UFODED)
   set(UFOAI_SOURCEFILES_UFODED
      ${UFOAI_SRC}/common/bsp.cpp
      ${UFOAI_SRC}/common/cmd.cpp
      ${UFOAI_SRC}/common/cmodel.cpp
      ${UFOAI_SRC}/common/common.cpp
      ${UFOAI_SRC}/common/cvar.cpp
      ${UFOAI_SRC}/common/dbuffer.cpp
      ${UFOAI_SRC}/common/files.cpp
      ${UFOAI_SRC}/common/grid.cpp
      ${UFOAI_SRC}/common/http.cpp
      ${UFOAI_SRC}/common/ioapi.cpp
      ${UFOAI_SRC}/common/list.cpp
      ${UFOAI_SRC}/common/md4.cpp
      ${UFOAI_SRC}/common/md5.cpp
      ${UFOAI_SRC}/common/mem.cpp
      ${UFOAI_SRC}/common/msg.cpp
      ${UFOAI_SRC}/common/net.cpp
      ${UFOAI_SRC}/common/netpack.cpp
      ${UFOAI_SRC}/common/pqueue.cpp
      ${UFOAI_SRC}/common/routing.cpp
      ${UFOAI_SRC}/common/scripts.cpp
      ${UFOAI_SRC}/common/sha1.cpp
      ${UFOAI_SRC}/common/sha2.cpp
      ${UFOAI_SRC}/common/tracing.cpp
      ${UFOAI_SRC}/common/unzip.cpp

      ${UFOAI_SRC}/game/chr_shared.cpp
      ${UFOAI_SRC}/game/inv_shared.cpp
      ${UFOAI_SRC}/game/q_shared.cpp

      ${UFOAI_SRC}/server/sv_ccmds.cpp
      ${UFOAI_SRC}/server/sv_clientstub.cpp
      ${UFOAI_SRC}/server/sv_game.cpp
      ${UFOAI_SRC}/server/sv_init.cpp
      ${UFOAI_SRC}/server/sv_log.cpp
      ${UFOAI_SRC}/server/sv_main.cpp
      ${UFOAI_SRC}/server/sv_mapcycle.cpp
      ${UFOAI_SRC}/server/sv_rma.cpp
      ${UFOAI_SRC}/server/sv_send.cpp
      ${UFOAI_SRC}/server/sv_user.cpp
      ${UFOAI_SRC}/server/sv_world.cpp

      ${UFOAI_SRC}/shared/aabb.cpp
      ${UFOAI_SRC}/shared/bfd.cpp
      ${UFOAI_SRC}/shared/byte.cpp
      ${UFOAI_SRC}/shared/infostring.cpp
      ${UFOAI_SRC}/shared/mathlib.cpp
      ${UFOAI_SRC}/shared/parse.cpp
      ${UFOAI_SRC}/shared/shared.cpp
      ${UFOAI_SRC}/shared/stringhunk.cpp
      ${UFOAI_SRC}/shared/utf8.cpp
   )

   # platform specific sources
   if(WIN32)
       SET(UFOAI_SOURCEFILES_UFODED ${UFOAI_SOURCEFILES_UFODED}
         ${UFOAI_SRC}/ports/windows/win_backtrace.cpp
         ${UFOAI_SRC}/ports/windows/win_console.cpp
         ${UFOAI_SRC}/ports/windows/win_main.cpp
         ${UFOAI_SRC}/ports/windows/win_shared.cpp
         ${UFOAI_SRC}/ports/windows/ufoded.rc
         )
   elseif(UNIX)
       SET(UFOAI_SOURCEFILES_UFODED ${UFOAI_SOURCEFILES_UFODED}
         ${UFOAI_SRC}/ports/linux/linux_main.cpp
         ${UFOAI_SRC}/ports/unix/unix_console.cpp
         ${UFOAI_SRC}/ports/unix/unix_files.cpp
         ${UFOAI_SRC}/ports/unix/unix_shared.cpp
         ${UFOAI_SRC}/ports/unix/unix_main.cpp
         )
   elseif(SOLARIS)
       SET(UFOAI_SOURCEFILES_UFODED ${UFOAI_SOURCEFILES_UFODED}
         ${UFOAI_SRC}/ports/solaris/solaris_main.cpp
         ${UFOAI_SRC}/ports/unix/unix_console.cpp
         ${UFOAI_SRC}/ports/unix/unix_files.cpp
         ${UFOAI_SRC}/ports/unix/unix_shared.cpp
         ${UFOAI_SRC}/ports/unix/unix_main.cpp
         )
   elseif(APPLE)
       SET(UFOAI_SOURCEFILES_UFODED ${UFOAI_SOURCEFILES_UFODED}
         ${UFOAI_SRC}/ports/macosx/osx_main.cpp
         ${UFOAI_SRC}/ports/macosx/osx_shared.cpp
         ${UFOAI_SRC}/ports/unix/unix_console.cpp
         ${UFOAI_SRC}/ports/unix/unix_files.cpp
         ${UFOAI_SRC}/ports/unix/unix_shared.cpp
         ${UFOAI_SRC}/ports/unix/unix_main.cpp
         )
   endif()

   # ufoded is executable
   add_executable(${UFOAI_PROJECT_UFODED} ${UFOAI_SOURCEFILES_UFODED})
   add_dependencies(${UFOAI_PROJECT_UFODED} "game")

   # For generic no-config case (e.g. with mingw, gcc, ...)
   set_target_properties(${UFOAI_PROJECT_UFODED} PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
   # For multi-config builds like msvc
   foreach(OUTPUTCONFIG ${CMAKE_CONFIGURATION_TYPES})
      string(TOUPPER ${OUTPUTCONFIG} OUTPUTCONFIG)
      set_target_properties(${UFOAI_PROJECT_UFODED} PROPERTIES RUNTIME_OUTPUT_DIRECTORY_${OUTPUTCONFIG}  ${CMAKE_BINARY_DIR})
   endforeach(OUTPUTCONFIG CMAKE_CONFIGURATION_TYPES)

   # since we embed our manifest, we should not generate a default one
   if(WIN32 AND MSVC)
      set_target_properties(${UFOAI_PROJECT_UFODED} PROPERTIES LINK_FLAGS "/MANIFEST:NO")
   endif()

   target_compile_definitions(${UFOAI_PROJECT_UFODED} PRIVATE -DCOMPILE_UFO)
   target_compile_definitions(${UFOAI_PROJECT_UFODED} PRIVATE -DDEDICATED_ONLY)
   target_compile_definitions(${UFOAI_PROJECT_UFODED} PRIVATE -DPATH_MANIFEST_FILE="${CMAKE_SOURCE_DIR}/build/projects/ufo.exe.manifest")
   target_compile_definitions(${UFOAI_PROJECT_UFODED} PRIVATE -DPATH_UFOICON_FILE="${CMAKE_SOURCE_DIR}/build/projects/ufo.ico")
   target_compile_definitions(${UFOAI_PROJECT_UFODED} PRIVATE -DPATH_UFODEDICON_FILE="${CMAKE_SOURCE_DIR}/build/projects/ufoded.ico")

   target_include_directories(${UFOAI_PROJECT_UFODED}
      PRIVATE ${SDL_INCLUDE_DIR}
      PRIVATE ${CURL_INCLUDE_DIRS}
      PRIVATE ${ZLIB_INCLUDE_DIRS}
   )

   target_link_libraries(${UFOAI_PROJECT_UFODED}
      ${SDL_LIBRARY}
      ${CURL_LIBRARIES}
      ${ZLIB_LIBRARIES}
   )

   if(WIN32)
      target_link_libraries(${UFOAI_PROJECT_UFODED}
         imm32
         version
         winmm
         Ws2_32
         wldap32
         rtm
      )
   endif()
   if(CMAKE_SYSTEM_NAME MATCHES "BSD")
      target_link_libraries(${UFOAI_PROJECT_UFODED}
        execinfo
      )
   endif()
endif()

###############################################################################
# ufo
###############################################################################

if(UFOAI_BUILD_UFO)
   set(UFOAI_SOURCEFILES_UFO
      ${UFOAI_SRC}/client/battlescape/cl_actor.cpp
      ${UFOAI_SRC}/client/battlescape/cl_battlescape.cpp
      ${UFOAI_SRC}/client/battlescape/cl_camera.cpp
      ${UFOAI_SRC}/client/battlescape/cl_hud.cpp
      ${UFOAI_SRC}/client/battlescape/cl_hud_callbacks.cpp
      ${UFOAI_SRC}/client/battlescape/cl_localentity.cpp
      ${UFOAI_SRC}/client/battlescape/cl_parse.cpp
      ${UFOAI_SRC}/client/battlescape/cl_particle.cpp
      ${UFOAI_SRC}/client/battlescape/cl_radar.cpp
      ${UFOAI_SRC}/client/battlescape/cl_spawn.cpp
      ${UFOAI_SRC}/client/battlescape/cl_ugv.cpp
      ${UFOAI_SRC}/client/battlescape/cl_view.cpp
      ${UFOAI_SRC}/client/battlescape/events/e_main.cpp
      ${UFOAI_SRC}/client/battlescape/events/e_parse.cpp
      ${UFOAI_SRC}/client/battlescape/events/e_server.cpp
      ${UFOAI_SRC}/client/battlescape/events/event/actor/e_event_actoradd.cpp
      ${UFOAI_SRC}/client/battlescape/events/event/actor/e_event_actorappear.cpp
      ${UFOAI_SRC}/client/battlescape/events/event/actor/e_event_actorclientaction.cpp
      ${UFOAI_SRC}/client/battlescape/events/event/actor/e_event_actordie.cpp
      ${UFOAI_SRC}/client/battlescape/events/event/actor/e_event_actorendshot.cpp
      ${UFOAI_SRC}/client/battlescape/events/event/actor/e_event_actormove.cpp
      ${UFOAI_SRC}/client/battlescape/events/event/actor/e_event_actorreactionfireabortshot.cpp
      ${UFOAI_SRC}/client/battlescape/events/event/actor/e_event_actorreactionfireaddtarget.cpp
      ${UFOAI_SRC}/client/battlescape/events/event/actor/e_event_actorreactionfirechange.cpp
      ${UFOAI_SRC}/client/battlescape/events/event/actor/e_event_actorreactionfireremovetarget.cpp
      ${UFOAI_SRC}/client/battlescape/events/event/actor/e_event_actorreactionfiretargetupdate.cpp
      ${UFOAI_SRC}/client/battlescape/events/event/actor/e_event_actorreservationchange.cpp
      ${UFOAI_SRC}/client/battlescape/events/event/actor/e_event_actorresetclientaction.cpp
      ${UFOAI_SRC}/client/battlescape/events/event/actor/e_event_actorrevitalised.cpp
      ${UFOAI_SRC}/client/battlescape/events/event/actor/e_event_actorshoot.cpp
      ${UFOAI_SRC}/client/battlescape/events/event/actor/e_event_actorshoothidden.cpp
      ${UFOAI_SRC}/client/battlescape/events/event/actor/e_event_actorstartshoot.cpp
      ${UFOAI_SRC}/client/battlescape/events/event/actor/e_event_actorstatechange.cpp
      ${UFOAI_SRC}/client/battlescape/events/event/actor/e_event_actorstats.cpp
      ${UFOAI_SRC}/client/battlescape/events/event/actor/e_event_actorthrow.cpp
      ${UFOAI_SRC}/client/battlescape/events/event/actor/e_event_actorturn.cpp
      ${UFOAI_SRC}/client/battlescape/events/event/actor/e_event_actorwound.cpp
      ${UFOAI_SRC}/client/battlescape/events/event/inventory/e_event_invadd.cpp
      ${UFOAI_SRC}/client/battlescape/events/event/inventory/e_event_invammo.cpp
      ${UFOAI_SRC}/client/battlescape/events/event/inventory/e_event_invdel.cpp
      ${UFOAI_SRC}/client/battlescape/events/event/inventory/e_event_invreload.cpp
      ${UFOAI_SRC}/client/battlescape/events/event/player/e_event_centerview.cpp
      ${UFOAI_SRC}/client/battlescape/events/event/player/e_event_doendround.cpp
      ${UFOAI_SRC}/client/battlescape/events/event/player/e_event_endroundannounce.cpp
      ${UFOAI_SRC}/client/battlescape/events/event/player/e_event_reset.cpp
      ${UFOAI_SRC}/client/battlescape/events/event/player/e_event_results.cpp
      ${UFOAI_SRC}/client/battlescape/events/event/player/e_event_startgame.cpp
      ${UFOAI_SRC}/client/battlescape/events/event/world/e_event_addbrushmodel.cpp
      ${UFOAI_SRC}/client/battlescape/events/event/world/e_event_addedict.cpp
      ${UFOAI_SRC}/client/battlescape/events/event/world/e_event_cameraappear.cpp
      ${UFOAI_SRC}/client/battlescape/events/event/world/e_event_doorclose.cpp
      ${UFOAI_SRC}/client/battlescape/events/event/world/e_event_dooropen.cpp
      ${UFOAI_SRC}/client/battlescape/events/event/world/e_event_entappear.cpp
      ${UFOAI_SRC}/client/battlescape/events/event/world/e_event_entdestroy.cpp
      ${UFOAI_SRC}/client/battlescape/events/event/world/e_event_entperish.cpp
      ${UFOAI_SRC}/client/battlescape/events/event/world/e_event_explode.cpp
      ${UFOAI_SRC}/client/battlescape/events/event/world/e_event_particleappear.cpp
      ${UFOAI_SRC}/client/battlescape/events/event/world/e_event_particlespawn.cpp
      ${UFOAI_SRC}/client/battlescape/events/event/world/e_event_sound.cpp
      ${UFOAI_SRC}/client/cgame/campaign/aliencargo.cpp
      ${UFOAI_SRC}/client/cgame/campaign/itemcargo.cpp
      ${UFOAI_SRC}/client/cgame/campaign/aliencontainment.cpp
      ${UFOAI_SRC}/client/cgame/campaign/cl_game_campaign.cpp
      ${UFOAI_SRC}/client/cgame/campaign/cp_aircraft.cpp
      ${UFOAI_SRC}/client/cgame/campaign/cp_aircraft_callbacks.cpp
      ${UFOAI_SRC}/client/cgame/campaign/cp_airfight.cpp
      ${UFOAI_SRC}/client/cgame/campaign/cp_alien_interest.cpp
      ${UFOAI_SRC}/client/cgame/campaign/cp_alienbase.cpp
      ${UFOAI_SRC}/client/cgame/campaign/cp_aliencont.cpp
      ${UFOAI_SRC}/client/cgame/campaign/cp_aliencont_callbacks.cpp
      ${UFOAI_SRC}/client/cgame/campaign/cp_auto_mission.cpp
      ${UFOAI_SRC}/client/cgame/campaign/cp_base.cpp
      ${UFOAI_SRC}/client/cgame/campaign/cp_base_callbacks.cpp
      ${UFOAI_SRC}/client/cgame/campaign/cp_basedefence_callbacks.cpp
      ${UFOAI_SRC}/client/cgame/campaign/cp_building.cpp
      ${UFOAI_SRC}/client/cgame/campaign/cp_campaign.cpp
      ${UFOAI_SRC}/client/cgame/campaign/cp_capacity.cpp
      ${UFOAI_SRC}/client/cgame/campaign/cp_cgame_callbacks.cpp
      ${UFOAI_SRC}/client/cgame/campaign/cp_character.cpp
      ${UFOAI_SRC}/client/cgame/campaign/cp_component.cpp
      ${UFOAI_SRC}/client/cgame/campaign/cp_employee.cpp
      ${UFOAI_SRC}/client/cgame/campaign/cp_employee_callbacks.cpp
      ${UFOAI_SRC}/client/cgame/campaign/cp_event.cpp
      ${UFOAI_SRC}/client/cgame/campaign/cp_event_callbacks.cpp
      ${UFOAI_SRC}/client/cgame/campaign/cp_fightequip_callbacks.cpp
      ${UFOAI_SRC}/client/cgame/campaign/cp_geoscape.cpp
      ${UFOAI_SRC}/client/cgame/campaign/cp_hospital.cpp
      ${UFOAI_SRC}/client/cgame/campaign/cp_hospital_callbacks.cpp
      ${UFOAI_SRC}/client/cgame/campaign/cp_installation.cpp
      ${UFOAI_SRC}/client/cgame/campaign/cp_installation_callbacks.cpp
      ${UFOAI_SRC}/client/cgame/campaign/cp_mapfightequip.cpp
      ${UFOAI_SRC}/client/cgame/campaign/cp_market.cpp
      ${UFOAI_SRC}/client/cgame/campaign/cp_market_callbacks.cpp
      ${UFOAI_SRC}/client/cgame/campaign/cp_messageoptions.cpp
      ${UFOAI_SRC}/client/cgame/campaign/cp_messageoptions_callbacks.cpp
      ${UFOAI_SRC}/client/cgame/campaign/cp_messages.cpp
      ${UFOAI_SRC}/client/cgame/campaign/cp_mission_callbacks.cpp
      ${UFOAI_SRC}/client/cgame/campaign/cp_mission_triggers.cpp
      ${UFOAI_SRC}/client/cgame/campaign/cp_missions.cpp
      ${UFOAI_SRC}/client/cgame/campaign/cp_nation.cpp
      ${UFOAI_SRC}/client/cgame/campaign/cp_overlay.cpp
      ${UFOAI_SRC}/client/cgame/campaign/cp_parse.cpp
      ${UFOAI_SRC}/client/cgame/campaign/cp_popup.cpp
      ${UFOAI_SRC}/client/cgame/campaign/cp_produce.cpp
      ${UFOAI_SRC}/client/cgame/campaign/cp_produce_callbacks.cpp
      ${UFOAI_SRC}/client/cgame/campaign/cp_radar.cpp
      ${UFOAI_SRC}/client/cgame/campaign/cp_rank.cpp
      ${UFOAI_SRC}/client/cgame/campaign/cp_research.cpp
      ${UFOAI_SRC}/client/cgame/campaign/cp_research_callbacks.cpp
      ${UFOAI_SRC}/client/cgame/campaign/cp_save.cpp
      ${UFOAI_SRC}/client/cgame/campaign/cp_save_callbacks.cpp
      ${UFOAI_SRC}/client/cgame/campaign/cp_statistics.cpp
      ${UFOAI_SRC}/client/cgame/campaign/cp_team.cpp
      ${UFOAI_SRC}/client/cgame/campaign/cp_team_callbacks.cpp
      ${UFOAI_SRC}/client/cgame/campaign/cp_time.cpp
      ${UFOAI_SRC}/client/cgame/campaign/cp_transfer.cpp
      ${UFOAI_SRC}/client/cgame/campaign/cp_transfer_callbacks.cpp
      ${UFOAI_SRC}/client/cgame/campaign/cp_ufo.cpp
      ${UFOAI_SRC}/client/cgame/campaign/cp_ufo_callbacks.cpp
      ${UFOAI_SRC}/client/cgame/campaign/cp_ufopedia.cpp
      ${UFOAI_SRC}/client/cgame/campaign/cp_uforecovery.cpp
      ${UFOAI_SRC}/client/cgame/campaign/cp_uforecovery_callbacks.cpp
      ${UFOAI_SRC}/client/cgame/campaign/cp_xvi.cpp
      ${UFOAI_SRC}/client/cgame/campaign/missions/cp_mission_baseattack.cpp
      ${UFOAI_SRC}/client/cgame/campaign/missions/cp_mission_buildbase.cpp
      ${UFOAI_SRC}/client/cgame/campaign/missions/cp_mission_harvest.cpp
      ${UFOAI_SRC}/client/cgame/campaign/missions/cp_mission_intercept.cpp
      ${UFOAI_SRC}/client/cgame/campaign/missions/cp_mission_recon.cpp
      ${UFOAI_SRC}/client/cgame/campaign/missions/cp_mission_rescue.cpp
      ${UFOAI_SRC}/client/cgame/campaign/missions/cp_mission_supply.cpp
      ${UFOAI_SRC}/client/cgame/campaign/missions/cp_mission_terror.cpp
      ${UFOAI_SRC}/client/cgame/campaign/missions/cp_mission_ufocarrier.cpp
      ${UFOAI_SRC}/client/cgame/campaign/missions/cp_mission_xvi.cpp
      ${UFOAI_SRC}/client/cgame/cl_game.cpp
      ${UFOAI_SRC}/client/cgame/cl_game_team.cpp
      ${UFOAI_SRC}/client/cgame/cl_map_callbacks.cpp
      ${UFOAI_SRC}/client/cgame/multiplayer/cl_game_multiplayer.cpp
      ${UFOAI_SRC}/client/cgame/multiplayer/mp_callbacks.cpp
      ${UFOAI_SRC}/client/cgame/multiplayer/mp_serverlist.cpp
      ${UFOAI_SRC}/client/cgame/skirmish/cl_game_skirmish.cpp
      ${UFOAI_SRC}/client/cinematic/cl_cinematic.cpp
      ${UFOAI_SRC}/client/cinematic/cl_cinematic_ogm.cpp
      ${UFOAI_SRC}/client/cinematic/cl_cinematic_roq.cpp
      ${UFOAI_SRC}/client/cinematic/cl_sequence.cpp
      ${UFOAI_SRC}/client/cl_console.cpp
      ${UFOAI_SRC}/client/cl_http.cpp
      ${UFOAI_SRC}/client/cl_inventory.cpp
      ${UFOAI_SRC}/client/cl_inventory_callbacks.cpp
      ${UFOAI_SRC}/client/cl_irc.cpp
      ${UFOAI_SRC}/client/cl_language.cpp
      ${UFOAI_SRC}/client/cl_main.cpp
      ${UFOAI_SRC}/client/cl_menu.cpp
      ${UFOAI_SRC}/client/cl_screen.cpp
      ${UFOAI_SRC}/client/cl_team.cpp
      ${UFOAI_SRC}/client/cl_tip.cpp
      ${UFOAI_SRC}/client/cl_tutorials.cpp
      ${UFOAI_SRC}/client/cl_video.cpp
      ${UFOAI_SRC}/client/input/cl_input.cpp
      ${UFOAI_SRC}/client/input/cl_joystick.cpp
      ${UFOAI_SRC}/client/input/cl_keys.cpp
      ${UFOAI_SRC}/client/renderer/r_array.cpp
      ${UFOAI_SRC}/client/renderer/r_bsp.cpp
      ${UFOAI_SRC}/client/renderer/r_corona.cpp
      ${UFOAI_SRC}/client/renderer/r_draw.cpp
      ${UFOAI_SRC}/client/renderer/r_entity.cpp
      ${UFOAI_SRC}/client/renderer/r_flare.cpp
      ${UFOAI_SRC}/client/renderer/r_font.cpp
      ${UFOAI_SRC}/client/renderer/r_framebuffer.cpp
      ${UFOAI_SRC}/client/renderer/r_geoscape.cpp
      ${UFOAI_SRC}/client/renderer/r_grass.cpp
      ${UFOAI_SRC}/client/renderer/r_image.cpp
      ${UFOAI_SRC}/client/renderer/r_light.cpp
      ${UFOAI_SRC}/client/renderer/r_lightmap.cpp
      ${UFOAI_SRC}/client/renderer/r_main.cpp
      ${UFOAI_SRC}/client/renderer/r_material.cpp
      ${UFOAI_SRC}/client/renderer/r_matrix.cpp
      ${UFOAI_SRC}/client/renderer/r_mesh.cpp
      ${UFOAI_SRC}/client/renderer/r_mesh_anim.cpp
      ${UFOAI_SRC}/client/renderer/r_misc.cpp
      ${UFOAI_SRC}/client/renderer/r_model.cpp
      ${UFOAI_SRC}/client/renderer/r_model_alias.cpp
      ${UFOAI_SRC}/client/renderer/r_model_brush.cpp
      ${UFOAI_SRC}/client/renderer/r_model_md2.cpp
      ${UFOAI_SRC}/client/renderer/r_model_md3.cpp
      ${UFOAI_SRC}/client/renderer/r_model_obj.cpp
      ${UFOAI_SRC}/client/renderer/r_particle.cpp
      ${UFOAI_SRC}/client/renderer/r_program.cpp
      ${UFOAI_SRC}/client/renderer/r_sdl.cpp
      ${UFOAI_SRC}/client/renderer/r_sphere.cpp
      ${UFOAI_SRC}/client/renderer/r_state.cpp
      ${UFOAI_SRC}/client/renderer/r_surface.cpp
      ${UFOAI_SRC}/client/renderer/r_thread.cpp
      ${UFOAI_SRC}/client/renderer/r_weather.cpp
      ${UFOAI_SRC}/client/sound/s_main.cpp
      ${UFOAI_SRC}/client/sound/s_mix.cpp
      ${UFOAI_SRC}/client/sound/s_mumble.cpp
      ${UFOAI_SRC}/client/sound/s_music.cpp
      ${UFOAI_SRC}/client/sound/s_sample.cpp
      ${UFOAI_SRC}/client/ui/node/ui_node_abstractnode.cpp
      ${UFOAI_SRC}/client/ui/node/ui_node_abstractoption.cpp
      ${UFOAI_SRC}/client/ui/node/ui_node_abstractscrollable.cpp
      ${UFOAI_SRC}/client/ui/node/ui_node_abstractscrollbar.cpp
      ${UFOAI_SRC}/client/ui/node/ui_node_abstractvalue.cpp
      ${UFOAI_SRC}/client/ui/node/ui_node_bar.cpp
      ${UFOAI_SRC}/client/ui/node/ui_node_base.cpp
      ${UFOAI_SRC}/client/ui/node/ui_node_baseinventory.cpp
      ${UFOAI_SRC}/client/ui/node/ui_node_battlescape.cpp
      ${UFOAI_SRC}/client/ui/node/ui_node_button.cpp
      ${UFOAI_SRC}/client/ui/node/ui_node_checkbox.cpp
      ${UFOAI_SRC}/client/ui/node/ui_node_container.cpp
      ${UFOAI_SRC}/client/ui/node/ui_node_controls.cpp
      ${UFOAI_SRC}/client/ui/node/ui_node_data.cpp
      ${UFOAI_SRC}/client/ui/node/ui_node_editor.cpp
      ${UFOAI_SRC}/client/ui/node/ui_node_geoscape.cpp
      ${UFOAI_SRC}/client/ui/node/ui_node_image.cpp
      ${UFOAI_SRC}/client/ui/node/ui_node_item.cpp
      ${UFOAI_SRC}/client/ui/node/ui_node_linechart.cpp
      ${UFOAI_SRC}/client/ui/node/ui_node_material_editor.cpp
      ${UFOAI_SRC}/client/ui/node/ui_node_messagelist.cpp
      ${UFOAI_SRC}/client/ui/node/ui_node_model.cpp
      ${UFOAI_SRC}/client/ui/node/ui_node_option.cpp
      ${UFOAI_SRC}/client/ui/node/ui_node_optionlist.cpp
      ${UFOAI_SRC}/client/ui/node/ui_node_optiontree.cpp
      ${UFOAI_SRC}/client/ui/node/ui_node_panel.cpp
      ${UFOAI_SRC}/client/ui/node/ui_node_radar.cpp
      ${UFOAI_SRC}/client/ui/node/ui_node_radiobutton.cpp
      ${UFOAI_SRC}/client/ui/node/ui_node_rows.cpp
      ${UFOAI_SRC}/client/ui/node/ui_node_selectbox.cpp
      ${UFOAI_SRC}/client/ui/node/ui_node_sequence.cpp
      ${UFOAI_SRC}/client/ui/node/ui_node_special.cpp
      ${UFOAI_SRC}/client/ui/node/ui_node_spinner.cpp
      ${UFOAI_SRC}/client/ui/node/ui_node_string.cpp
      ${UFOAI_SRC}/client/ui/node/ui_node_tab.cpp
      ${UFOAI_SRC}/client/ui/node/ui_node_tbar.cpp
      ${UFOAI_SRC}/client/ui/node/ui_node_text.cpp
      ${UFOAI_SRC}/client/ui/node/ui_node_text2.cpp
      ${UFOAI_SRC}/client/ui/node/ui_node_textentry.cpp
      ${UFOAI_SRC}/client/ui/node/ui_node_textlist.cpp
      ${UFOAI_SRC}/client/ui/node/ui_node_texture.cpp
      ${UFOAI_SRC}/client/ui/node/ui_node_timer.cpp
      ${UFOAI_SRC}/client/ui/node/ui_node_video.cpp
      ${UFOAI_SRC}/client/ui/node/ui_node_vscrollbar.cpp
      ${UFOAI_SRC}/client/ui/node/ui_node_window.cpp
      ${UFOAI_SRC}/client/ui/node/ui_node_zone.cpp
      ${UFOAI_SRC}/client/ui/swig/ui_lua_shared.cpp
      ${UFOAI_SRC}/client/ui/ui_actions.cpp
      ${UFOAI_SRC}/client/ui/ui_behaviour.cpp
      ${UFOAI_SRC}/client/ui/ui_components.cpp
      ${UFOAI_SRC}/client/ui/ui_data.cpp
      ${UFOAI_SRC}/client/ui/ui_dragndrop.cpp
      ${UFOAI_SRC}/client/ui/ui_draw.cpp
      ${UFOAI_SRC}/client/ui/ui_expression.cpp
      ${UFOAI_SRC}/client/ui/ui_font.cpp
      ${UFOAI_SRC}/client/ui/ui_input.cpp
      ${UFOAI_SRC}/client/ui/ui_lua.cpp
      ${UFOAI_SRC}/client/ui/ui_main.cpp
      ${UFOAI_SRC}/client/ui/ui_node.cpp
      ${UFOAI_SRC}/client/ui/ui_nodes.cpp
      ${UFOAI_SRC}/client/ui/ui_parse.cpp
      ${UFOAI_SRC}/client/ui/ui_popup.cpp
      ${UFOAI_SRC}/client/ui/ui_render.cpp
      ${UFOAI_SRC}/client/ui/ui_sound.cpp
      ${UFOAI_SRC}/client/ui/ui_sprite.cpp
      ${UFOAI_SRC}/client/ui/ui_timer.cpp
      ${UFOAI_SRC}/client/ui/ui_tooltip.cpp
      ${UFOAI_SRC}/client/ui/ui_windows.cpp
      ${UFOAI_SRC}/client/web/web_cgame.cpp
      ${UFOAI_SRC}/client/web/web_main.cpp
      ${UFOAI_SRC}/client/cl_lua.cpp
      ${UFOAI_SRC}/client/DateTime.cpp

      ${UFOAI_SRC}/common/binaryexpressionparser.cpp
      ${UFOAI_SRC}/common/bsp.cpp
      ${UFOAI_SRC}/common/cmd.cpp
      ${UFOAI_SRC}/common/cmodel.cpp
      ${UFOAI_SRC}/common/common.cpp
      ${UFOAI_SRC}/common/cvar.cpp
      ${UFOAI_SRC}/common/dbuffer.cpp
      ${UFOAI_SRC}/common/files.cpp
      ${UFOAI_SRC}/common/grid.cpp
      ${UFOAI_SRC}/common/hashtable.cpp
      ${UFOAI_SRC}/common/http.cpp
      ${UFOAI_SRC}/common/ioapi.cpp
      ${UFOAI_SRC}/common/list.cpp
      ${UFOAI_SRC}/common/md4.cpp
      ${UFOAI_SRC}/common/md5.cpp
      ${UFOAI_SRC}/common/mem.cpp
      ${UFOAI_SRC}/common/msg.cpp
      ${UFOAI_SRC}/common/net.cpp
      ${UFOAI_SRC}/common/netpack.cpp
      ${UFOAI_SRC}/common/pqueue.cpp
      ${UFOAI_SRC}/common/routing.cpp
      ${UFOAI_SRC}/common/scripts.cpp
      ${UFOAI_SRC}/common/scripts_lua.cpp
      ${UFOAI_SRC}/common/sha1.cpp
      ${UFOAI_SRC}/common/sha2.cpp
      ${UFOAI_SRC}/common/tracing.cpp
      ${UFOAI_SRC}/common/unzip.cpp
      ${UFOAI_SRC}/common/xml.cpp

      ${UFOAI_SRC}/game/chr_shared.cpp
      ${UFOAI_SRC}/game/inv_shared.cpp
      ${UFOAI_SRC}/game/inventory.cpp
      ${UFOAI_SRC}/game/q_shared.cpp

      ${UFOAI_SRC}/server/sv_ccmds.cpp
      ${UFOAI_SRC}/server/sv_game.cpp
      ${UFOAI_SRC}/server/sv_init.cpp
      ${UFOAI_SRC}/server/sv_log.cpp
      ${UFOAI_SRC}/server/sv_main.cpp
      ${UFOAI_SRC}/server/sv_mapcycle.cpp
      ${UFOAI_SRC}/server/sv_rma.cpp
      ${UFOAI_SRC}/server/sv_send.cpp
      ${UFOAI_SRC}/server/sv_user.cpp
      ${UFOAI_SRC}/server/sv_world.cpp

      ${UFOAI_SRC}/shared/aabb.cpp
      ${UFOAI_SRC}/shared/bfd.cpp
      ${UFOAI_SRC}/shared/byte.cpp
      ${UFOAI_SRC}/shared/images.cpp
      ${UFOAI_SRC}/shared/infostring.cpp
      ${UFOAI_SRC}/shared/mathlib.cpp
      ${UFOAI_SRC}/shared/mathlib_extra.cpp
      ${UFOAI_SRC}/shared/parse.cpp
      ${UFOAI_SRC}/shared/shared.cpp
      ${UFOAI_SRC}/shared/stringhunk.cpp
      ${UFOAI_SRC}/shared/utf8.cpp

      ${UFOAI_SRC}/libs/mumble/libmumblelink.c

      ${UFOAI_SRC}/libs/mxml/mxml-attr.c
      ${UFOAI_SRC}/libs/mxml/mxml-entity.c
      ${UFOAI_SRC}/libs/mxml/mxml-file.c
      ${UFOAI_SRC}/libs/mxml/mxml-index.c
      ${UFOAI_SRC}/libs/mxml/mxml-node.c
      ${UFOAI_SRC}/libs/mxml/mxml-private.c
      ${UFOAI_SRC}/libs/mxml/mxml-search.c
      ${UFOAI_SRC}/libs/mxml/mxml-set.c
      ${UFOAI_SRC}/libs/mxml/mxml-string.c
   )

   # platform specific sources
   if(WIN32)
      SET(UFOAI_SOURCEFILES_UFO ${UFOAI_SOURCEFILES_UFO}
         ${UFOAI_SRC}/ports/windows/win_backtrace.cpp
         ${UFOAI_SRC}/ports/windows/win_console.cpp
         ${UFOAI_SRC}/ports/windows/win_main.cpp
         ${UFOAI_SRC}/ports/windows/win_shared.cpp
         ${UFOAI_SRC}/ports/windows/ufo.rc
      )
   elseif(UNIX)
      SET(UFOAI_SOURCEFILES_UFO ${UFOAI_SOURCEFILES_UFO}
         ${UFOAI_SRC}/ports/linux/linux_main.cpp
         ${UFOAI_SRC}/ports/unix/unix_console.cpp
         ${UFOAI_SRC}/ports/unix/unix_files.cpp
         ${UFOAI_SRC}/ports/unix/unix_shared.cpp
         ${UFOAI_SRC}/ports/unix/unix_main.cpp
      )
   elseif(SOLARIS)
      SET(UFOAI_SOURCEFILES_UFO ${UFOAI_SOURCEFILES_UFO}
         ${UFOAI_SRC}/ports/solaris/solaris_main.cpp
         ${UFOAI_SRC}/ports/unix/unix_console.cpp
         ${UFOAI_SRC}/ports/unix/unix_files.cpp
         ${UFOAI_SRC}/ports/unix/unix_shared.cpp
         ${UFOAI_SRC}/ports/unix/unix_main.cpp
      )
   elseif(APPLE)
      SET(UFOAI_SOURCEFILES_UFO ${UFOAI_SOURCEFILES_UFO}
         ${UFOAI_SRC}/ports/macosx/osx_main.cpp
         ${UFOAI_SRC}/ports/macosx/osx_shared.cpp
         ${UFOAI_SRC}/ports/unix/unix_console.cpp
         ${UFOAI_SRC}/ports/unix/unix_files.cpp
         ${UFOAI_SRC}/ports/unix/unix_shared.cpp
         ${UFOAI_SRC}/ports/unix/unix_main.cpp
      )
   endif()

   # ufo is executable
   add_executable(${UFOAI_PROJECT_UFO} ${UFOAI_SOURCEFILES_UFO})
   add_dependencies(${UFOAI_PROJECT_UFO} "game")

   # For generic no-config case (e.g. with mingw, gcc, ...)
   set_target_properties(${UFOAI_PROJECT_UFO} PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
   # For multi-config builds like msvc
   foreach(OUTPUTCONFIG ${CMAKE_CONFIGURATION_TYPES})
      string(TOUPPER ${OUTPUTCONFIG} OUTPUTCONFIG)
      set_target_properties(${UFOAI_PROJECT_UFO} PROPERTIES RUNTIME_OUTPUT_DIRECTORY_${OUTPUTCONFIG}  ${CMAKE_BINARY_DIR})
   endforeach(OUTPUTCONFIG CMAKE_CONFIGURATION_TYPES)

   # since we embed our manifest, we should not generate a default one
   if(WIN32 AND MSVC)
      set_target_properties(${UFOAI_PROJECT_UFO} PROPERTIES LINK_FLAGS "/MANIFEST:NO")
   endif()

   target_compile_definitions(${UFOAI_PROJECT_UFO} PRIVATE -DCOMPILE_UFO)
   target_compile_definitions(${UFOAI_PROJECT_UFO} PRIVATE -DHARD_LINKED_CGAME)
   target_compile_definitions(${UFOAI_PROJECT_UFO} PRIVATE -DPATH_MANIFEST_FILE="${CMAKE_SOURCE_DIR}/build/projects/ufo.exe.manifest")
   target_compile_definitions(${UFOAI_PROJECT_UFO} PRIVATE -DPATH_UFOICON_FILE="${CMAKE_SOURCE_DIR}/build/projects/ufo.ico")
   target_compile_definitions(${UFOAI_PROJECT_UFO} PRIVATE -DPATH_UFODEDICON_FILE="${CMAKE_SOURCE_DIR}/build/projects/ufoded.ico")

   target_include_directories(${UFOAI_PROJECT_UFO}
      PRIVATE ${UFOAI_SRC}/libs/mumble
      PRIVATE ${UFOAI_SRC}/libs/mxml
      PRIVATE ${LUA_INCLUDE_DIR}
      PRIVATE ${SDL_INCLUDE_DIR}
      PRIVATE ${SDLMIXER_INCLUDE_DIR}
      PRIVATE ${SDLTTF_INCLUDE_DIR}
      PRIVATE ${CURL_INCLUDE_DIRS}
      PRIVATE ${VORBIS_INCLUDE_DIR}
      PRIVATE ${OGG_INCLUDE_DIR}
      PRIVATE ${JPEG_INCLUDE_DIRS}
      PRIVATE ${PNG_INCLUDE_DIRS}
      PRIVATE ${Intl_INCLUDE_DIRS}
      PRIVATE ${ZLIB_INCLUDE_DIRS}
      PRIVATE ${OPENGL_INCLUDE_DIR}
   )

   if(MINGW)
      target_link_libraries(${UFOAI_PROJECT_UFO}
         mingw32
      )
   endif()

   target_link_libraries(${UFOAI_PROJECT_UFO}
      ${LUA_LIBRARY}
      ${SDL_LIBRARY_MAIN}
      ${SDL_LIBRARY}
      ${SDL_MIXER_LIBRARIES}
      ${SDL_TTF_LIBRARIES}
      ${CURL_LIBRARIES}
      ${VORBIS_LIBRARY}
      ${OGG_LIBRARIES}
      ${JPEG_LIBRARIES}
      ${PNG_LIBRARIES}
      ${ZLIB_LIBRARIES}
      ${OPENGL_LIBRARY}
   )

   if(WIN32 AND MSVC)
      target_link_libraries(${UFOAI_PROJECT_UFO}
         opengl32
         ${Intl_LIBRARIES}
         winmm
         Ws2_32
      )
   elseif(WIN32)
      target_link_libraries(${UFOAI_PROJECT_UFO}
         opengl32
         imm32
         ${Intl_LIBRARIES}
         version
         winmm
         Ws2_32
      )
   elseif(APPLE)
      target_link_libraries(${UFOAI_PROJECT_UFO}
         ${Intl_LIBRARIES}
      )
   elseif(UNIX)
      target_link_libraries(${UFOAI_PROJECT_UFO}
         GL
      )
      if(NOT CMAKE_SYSTEM_NAME MATCHES "OpenBSD")
         target_link_libraries(${UFOAI_PROJECT_UFO}
            rt
         )
      endif()
      if(CMAKE_SYSTEM_NAME MATCHES "BSD")
         target_link_libraries(${UFOAI_PROJECT_UFO}
            ${Intl_LIBRARIES}
         )
      endif()
   endif()
endif()


###############################################################################
# ufomodel
###############################################################################

if(UFOAI_BUILD_UFOMODEL)
   set(UFOAI_SOURCEFILES_UFOMODEL
      ${UFOAI_SRC}/client/renderer/r_model.cpp
      ${UFOAI_SRC}/client/renderer/r_model_alias.cpp
      ${UFOAI_SRC}/client/renderer/r_model_md2.cpp
      ${UFOAI_SRC}/client/renderer/r_model_md3.cpp
      ${UFOAI_SRC}/client/renderer/r_model_obj.cpp

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

      ${UFOAI_SRC}/tools/ufomodel/md2.cpp
      ${UFOAI_SRC}/tools/ufomodel/ufomodel.cpp
   )

   # platform specific sources
   if(WIN32)
      SET(UFOAI_SOURCEFILES_UFOMODEL ${UFOAI_SOURCEFILES_UFOMODEL}
         ${UFOAI_SRC}/ports/windows/win_shared.cpp
      )
   elseif(UNIX)
      SET(UFOAI_SOURCEFILES_UFOMODEL ${UFOAI_SOURCEFILES_UFOMODEL}
         ${UFOAI_SRC}/ports/unix/unix_files.cpp
         ${UFOAI_SRC}/ports/unix/unix_shared.cpp
         ${UFOAI_SRC}/ports/unix/unix_main.cpp
      )
   elseif(SOLARIS)
      SET(UFOAI_SOURCEFILES_UFOMODEL ${UFOAI_SOURCEFILES_UFOMODEL}
         ${UFOAI_SRC}/ports/unix/unix_files.cpp
         ${UFOAI_SRC}/ports/unix/unix_shared.cpp
         ${UFOAI_SRC}/ports/unix/unix_main.cpp
      )
   elseif(APPLE)
      SET(UFOAI_SOURCEFILES_UFOMODEL ${UFOAI_SOURCEFILES_UFOMODEL}
         ${UFOAI_SRC}/ports/unix/unix_files.cpp
         ${UFOAI_SRC}/ports/unix/unix_shared.cpp
         ${UFOAI_SRC}/ports/unix/unix_main.cpp
      )
   endif()

   # ufomodel is executable
   add_executable(${UFOAI_PROJECT_UFOMODEL} ${UFOAI_SOURCEFILES_UFOMODEL})

   # For generic no-config case (e.g. with mingw, gcc, ...)
   set_target_properties(${UFOAI_PROJECT_UFOMODEL} PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
   # For multi-config builds like msvc
   foreach(OUTPUTCONFIG ${CMAKE_CONFIGURATION_TYPES})
      string(TOUPPER ${OUTPUTCONFIG} OUTPUTCONFIG)
      set_target_properties(${UFOAI_PROJECT_UFOMODEL} PROPERTIES RUNTIME_OUTPUT_DIRECTORY_${OUTPUTCONFIG}  ${CMAKE_BINARY_DIR})
   endforeach(OUTPUTCONFIG CMAKE_CONFIGURATION_TYPES)

   target_compile_definitions(${UFOAI_PROJECT_UFOMODEL} PRIVATE -DCOMPILE_MAP)

   target_include_directories(${UFOAI_PROJECT_UFOMODEL}
      PRIVATE ${SDL_INCLUDE_DIR}
      PRIVATE ${JPEG_INCLUDE_DIRS}
      PRIVATE ${PNG_INCLUDE_DIRS}
      PRIVATE ${ZLIB_INCLUDE_DIRS}
   )

   if(MINGW)
      target_link_libraries(${UFOAI_PROJECT_UFOMODEL}
         mingw32
      )
   endif()

   target_link_libraries(${UFOAI_PROJECT_UFOMODEL}
      ${SDL_LIBRARY_MAIN}
      ${SDL_LIBRARY}
      ${JPEG_LIBRARIES}
      ${PNG_LIBRARIES}
      ${ZLIB_LIBRARIES}
   )

   if(WIN32)
      target_link_libraries(${UFOAI_PROJECT_UFOMODEL}
         imm32
         version
         winmm
      )
   endif()
   if(CMAKE_SYSTEM_NAME MATCHES "BSD")
      target_link_libraries(${UFOAI_PROJECT_UFOMODEL}
        execinfo
      )
   endif()
endif()

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

if(UFOAI_BUILD_UFOTESTALL)
   set(UFOAI_SOURCEFILES_UFOTESTALL
      ${UFOAI_SRC}/client/battlescape/cl_actor.cpp
      ${UFOAI_SRC}/client/battlescape/cl_battlescape.cpp
      ${UFOAI_SRC}/client/battlescape/cl_camera.cpp
      ${UFOAI_SRC}/client/battlescape/cl_hud.cpp
      ${UFOAI_SRC}/client/battlescape/cl_hud_callbacks.cpp
      ${UFOAI_SRC}/client/battlescape/cl_localentity.cpp
      ${UFOAI_SRC}/client/battlescape/cl_parse.cpp
      ${UFOAI_SRC}/client/battlescape/cl_particle.cpp
      ${UFOAI_SRC}/client/battlescape/cl_radar.cpp
      ${UFOAI_SRC}/client/battlescape/cl_spawn.cpp
      ${UFOAI_SRC}/client/battlescape/cl_ugv.cpp
      ${UFOAI_SRC}/client/battlescape/cl_view.cpp
      ${UFOAI_SRC}/client/battlescape/events/e_main.cpp
      ${UFOAI_SRC}/client/battlescape/events/e_parse.cpp
      ${UFOAI_SRC}/client/battlescape/events/e_server.cpp
      ${UFOAI_SRC}/client/battlescape/events/event/actor/e_event_actoradd.cpp
      ${UFOAI_SRC}/client/battlescape/events/event/actor/e_event_actorappear.cpp
      ${UFOAI_SRC}/client/battlescape/events/event/actor/e_event_actorclientaction.cpp
      ${UFOAI_SRC}/client/battlescape/events/event/actor/e_event_actordie.cpp
      ${UFOAI_SRC}/client/battlescape/events/event/actor/e_event_actorendshot.cpp
      ${UFOAI_SRC}/client/battlescape/events/event/actor/e_event_actormove.cpp
      ${UFOAI_SRC}/client/battlescape/events/event/actor/e_event_actorreactionfireabortshot.cpp
      ${UFOAI_SRC}/client/battlescape/events/event/actor/e_event_actorreactionfireaddtarget.cpp
      ${UFOAI_SRC}/client/battlescape/events/event/actor/e_event_actorreactionfirechange.cpp
      ${UFOAI_SRC}/client/battlescape/events/event/actor/e_event_actorreactionfireremovetarget.cpp
      ${UFOAI_SRC}/client/battlescape/events/event/actor/e_event_actorreactionfiretargetupdate.cpp
      ${UFOAI_SRC}/client/battlescape/events/event/actor/e_event_actorreservationchange.cpp
      ${UFOAI_SRC}/client/battlescape/events/event/actor/e_event_actorresetclientaction.cpp
      ${UFOAI_SRC}/client/battlescape/events/event/actor/e_event_actorrevitalised.cpp
      ${UFOAI_SRC}/client/battlescape/events/event/actor/e_event_actorshoot.cpp
      ${UFOAI_SRC}/client/battlescape/events/event/actor/e_event_actorshoothidden.cpp
      ${UFOAI_SRC}/client/battlescape/events/event/actor/e_event_actorstartshoot.cpp
      ${UFOAI_SRC}/client/battlescape/events/event/actor/e_event_actorstatechange.cpp
      ${UFOAI_SRC}/client/battlescape/events/event/actor/e_event_actorstats.cpp
      ${UFOAI_SRC}/client/battlescape/events/event/actor/e_event_actorthrow.cpp
      ${UFOAI_SRC}/client/battlescape/events/event/actor/e_event_actorturn.cpp
      ${UFOAI_SRC}/client/battlescape/events/event/actor/e_event_actorwound.cpp
      ${UFOAI_SRC}/client/battlescape/events/event/inventory/e_event_invadd.cpp
      ${UFOAI_SRC}/client/battlescape/events/event/inventory/e_event_invammo.cpp
      ${UFOAI_SRC}/client/battlescape/events/event/inventory/e_event_invdel.cpp
      ${UFOAI_SRC}/client/battlescape/events/event/inventory/e_event_invreload.cpp
      ${UFOAI_SRC}/client/battlescape/events/event/player/e_event_centerview.cpp
      ${UFOAI_SRC}/client/battlescape/events/event/player/e_event_doendround.cpp
      ${UFOAI_SRC}/client/battlescape/events/event/player/e_event_endroundannounce.cpp
      ${UFOAI_SRC}/client/battlescape/events/event/player/e_event_reset.cpp
      ${UFOAI_SRC}/client/battlescape/events/event/player/e_event_results.cpp
      ${UFOAI_SRC}/client/battlescape/events/event/player/e_event_startgame.cpp
      ${UFOAI_SRC}/client/battlescape/events/event/world/e_event_addbrushmodel.cpp
      ${UFOAI_SRC}/client/battlescape/events/event/world/e_event_addedict.cpp
      ${UFOAI_SRC}/client/battlescape/events/event/world/e_event_cameraappear.cpp
      ${UFOAI_SRC}/client/battlescape/events/event/world/e_event_doorclose.cpp
      ${UFOAI_SRC}/client/battlescape/events/event/world/e_event_dooropen.cpp
      ${UFOAI_SRC}/client/battlescape/events/event/world/e_event_entappear.cpp
      ${UFOAI_SRC}/client/battlescape/events/event/world/e_event_entdestroy.cpp
      ${UFOAI_SRC}/client/battlescape/events/event/world/e_event_entperish.cpp
      ${UFOAI_SRC}/client/battlescape/events/event/world/e_event_explode.cpp
      ${UFOAI_SRC}/client/battlescape/events/event/world/e_event_particleappear.cpp
      ${UFOAI_SRC}/client/battlescape/events/event/world/e_event_particlespawn.cpp
      ${UFOAI_SRC}/client/battlescape/events/event/world/e_event_sound.cpp
      ${UFOAI_SRC}/client/cgame/campaign/aliencargo.cpp
      ${UFOAI_SRC}/client/cgame/campaign/itemcargo.cpp
      ${UFOAI_SRC}/client/cgame/campaign/aliencontainment.cpp
      ${UFOAI_SRC}/client/cgame/campaign/cl_game_campaign.cpp
      ${UFOAI_SRC}/client/cgame/campaign/cp_aircraft.cpp
      ${UFOAI_SRC}/client/cgame/campaign/cp_aircraft_callbacks.cpp
      ${UFOAI_SRC}/client/cgame/campaign/cp_airfight.cpp
      ${UFOAI_SRC}/client/cgame/campaign/cp_alien_interest.cpp
      ${UFOAI_SRC}/client/cgame/campaign/cp_alienbase.cpp
      ${UFOAI_SRC}/client/cgame/campaign/cp_aliencont.cpp
      ${UFOAI_SRC}/client/cgame/campaign/cp_aliencont_callbacks.cpp
      ${UFOAI_SRC}/client/cgame/campaign/cp_auto_mission.cpp
      ${UFOAI_SRC}/client/cgame/campaign/cp_base.cpp
      ${UFOAI_SRC}/client/cgame/campaign/cp_base_callbacks.cpp
      ${UFOAI_SRC}/client/cgame/campaign/cp_basedefence_callbacks.cpp
      ${UFOAI_SRC}/client/cgame/campaign/cp_building.cpp
      ${UFOAI_SRC}/client/cgame/campaign/cp_campaign.cpp
      ${UFOAI_SRC}/client/cgame/campaign/cp_capacity.cpp
      ${UFOAI_SRC}/client/cgame/campaign/cp_cgame_callbacks.cpp
      ${UFOAI_SRC}/client/cgame/campaign/cp_character.cpp
      ${UFOAI_SRC}/client/cgame/campaign/cp_component.cpp
      ${UFOAI_SRC}/client/cgame/campaign/cp_employee.cpp
      ${UFOAI_SRC}/client/cgame/campaign/cp_employee_callbacks.cpp
      ${UFOAI_SRC}/client/cgame/campaign/cp_event.cpp
      ${UFOAI_SRC}/client/cgame/campaign/cp_event_callbacks.cpp
      ${UFOAI_SRC}/client/cgame/campaign/cp_fightequip_callbacks.cpp
      ${UFOAI_SRC}/client/cgame/campaign/cp_geoscape.cpp
      ${UFOAI_SRC}/client/cgame/campaign/cp_hospital.cpp
      ${UFOAI_SRC}/client/cgame/campaign/cp_hospital_callbacks.cpp
      ${UFOAI_SRC}/client/cgame/campaign/cp_installation.cpp
      ${UFOAI_SRC}/client/cgame/campaign/cp_installation_callbacks.cpp
      ${UFOAI_SRC}/client/cgame/campaign/cp_mapfightequip.cpp
      ${UFOAI_SRC}/client/cgame/campaign/cp_market.cpp
      ${UFOAI_SRC}/client/cgame/campaign/cp_market_callbacks.cpp
      ${UFOAI_SRC}/client/cgame/campaign/cp_messageoptions.cpp
      ${UFOAI_SRC}/client/cgame/campaign/cp_messageoptions_callbacks.cpp
      ${UFOAI_SRC}/client/cgame/campaign/cp_messages.cpp
      ${UFOAI_SRC}/client/cgame/campaign/cp_mission_callbacks.cpp
      ${UFOAI_SRC}/client/cgame/campaign/cp_mission_triggers.cpp
      ${UFOAI_SRC}/client/cgame/campaign/cp_missions.cpp
      ${UFOAI_SRC}/client/cgame/campaign/cp_nation.cpp
      ${UFOAI_SRC}/client/cgame/campaign/cp_overlay.cpp
      ${UFOAI_SRC}/client/cgame/campaign/cp_parse.cpp
      ${UFOAI_SRC}/client/cgame/campaign/cp_popup.cpp
      ${UFOAI_SRC}/client/cgame/campaign/cp_produce.cpp
      ${UFOAI_SRC}/client/cgame/campaign/cp_produce_callbacks.cpp
      ${UFOAI_SRC}/client/cgame/campaign/cp_radar.cpp
      ${UFOAI_SRC}/client/cgame/campaign/cp_rank.cpp
      ${UFOAI_SRC}/client/cgame/campaign/cp_research.cpp
      ${UFOAI_SRC}/client/cgame/campaign/cp_research_callbacks.cpp
      ${UFOAI_SRC}/client/cgame/campaign/cp_save.cpp
      ${UFOAI_SRC}/client/cgame/campaign/cp_save_callbacks.cpp
      ${UFOAI_SRC}/client/cgame/campaign/cp_statistics.cpp
      ${UFOAI_SRC}/client/cgame/campaign/cp_team.cpp
      ${UFOAI_SRC}/client/cgame/campaign/cp_team_callbacks.cpp
      ${UFOAI_SRC}/client/cgame/campaign/cp_time.cpp
      ${UFOAI_SRC}/client/cgame/campaign/cp_transfer.cpp
      ${UFOAI_SRC}/client/cgame/campaign/cp_transfer_callbacks.cpp
      ${UFOAI_SRC}/client/cgame/campaign/cp_ufo.cpp
      ${UFOAI_SRC}/client/cgame/campaign/cp_ufo_callbacks.cpp
      ${UFOAI_SRC}/client/cgame/campaign/cp_ufopedia.cpp
      ${UFOAI_SRC}/client/cgame/campaign/cp_uforecovery.cpp
      ${UFOAI_SRC}/client/cgame/campaign/cp_uforecovery_callbacks.cpp
      ${UFOAI_SRC}/client/cgame/campaign/cp_xvi.cpp
      ${UFOAI_SRC}/client/cgame/campaign/missions/cp_mission_baseattack.cpp
      ${UFOAI_SRC}/client/cgame/campaign/missions/cp_mission_buildbase.cpp
      ${UFOAI_SRC}/client/cgame/campaign/missions/cp_mission_harvest.cpp
      ${UFOAI_SRC}/client/cgame/campaign/missions/cp_mission_intercept.cpp
      ${UFOAI_SRC}/client/cgame/campaign/missions/cp_mission_recon.cpp
      ${UFOAI_SRC}/client/cgame/campaign/missions/cp_mission_rescue.cpp
      ${UFOAI_SRC}/client/cgame/campaign/missions/cp_mission_supply.cpp
      ${UFOAI_SRC}/client/cgame/campaign/missions/cp_mission_terror.cpp
      ${UFOAI_SRC}/client/cgame/campaign/missions/cp_mission_ufocarrier.cpp
      ${UFOAI_SRC}/client/cgame/campaign/missions/cp_mission_xvi.cpp
      ${UFOAI_SRC}/client/cgame/cl_game.cpp
      ${UFOAI_SRC}/client/cgame/cl_game_team.cpp
      ${UFOAI_SRC}/client/cgame/cl_map_callbacks.cpp
      ${UFOAI_SRC}/client/cgame/multiplayer/cl_game_multiplayer.cpp
      ${UFOAI_SRC}/client/cgame/multiplayer/mp_callbacks.cpp
      ${UFOAI_SRC}/client/cgame/multiplayer/mp_serverlist.cpp
      ${UFOAI_SRC}/client/cgame/skirmish/cl_game_skirmish.cpp
      ${UFOAI_SRC}/client/cinematic/cl_cinematic.cpp
      ${UFOAI_SRC}/client/cinematic/cl_cinematic_ogm.cpp
      ${UFOAI_SRC}/client/cinematic/cl_cinematic_roq.cpp
      ${UFOAI_SRC}/client/cinematic/cl_sequence.cpp
      ${UFOAI_SRC}/client/cl_console.cpp
      ${UFOAI_SRC}/client/cl_http.cpp
      ${UFOAI_SRC}/client/cl_inventory.cpp
      ${UFOAI_SRC}/client/cl_inventory_callbacks.cpp
      ${UFOAI_SRC}/client/cl_irc.cpp
      ${UFOAI_SRC}/client/cl_language.cpp
      ${UFOAI_SRC}/client/cl_main.cpp
      ${UFOAI_SRC}/client/cl_menu.cpp
      ${UFOAI_SRC}/client/cl_screen.cpp
      ${UFOAI_SRC}/client/cl_team.cpp
      ${UFOAI_SRC}/client/cl_tip.cpp
      ${UFOAI_SRC}/client/cl_tutorials.cpp
      ${UFOAI_SRC}/client/cl_video.cpp
      ${UFOAI_SRC}/client/input/cl_input.cpp
      ${UFOAI_SRC}/client/input/cl_joystick.cpp
      ${UFOAI_SRC}/client/input/cl_keys.cpp
      ${UFOAI_SRC}/client/renderer/r_array.cpp
      ${UFOAI_SRC}/client/renderer/r_bsp.cpp
      ${UFOAI_SRC}/client/renderer/r_corona.cpp
      ${UFOAI_SRC}/client/renderer/r_draw.cpp
      ${UFOAI_SRC}/client/renderer/r_entity.cpp
      ${UFOAI_SRC}/client/renderer/r_flare.cpp
      ${UFOAI_SRC}/client/renderer/r_font.cpp
      ${UFOAI_SRC}/client/renderer/r_framebuffer.cpp
      ${UFOAI_SRC}/client/renderer/r_geoscape.cpp
      ${UFOAI_SRC}/client/renderer/r_grass.cpp
      ${UFOAI_SRC}/client/renderer/r_image.cpp
      ${UFOAI_SRC}/client/renderer/r_light.cpp
      ${UFOAI_SRC}/client/renderer/r_lightmap.cpp
      ${UFOAI_SRC}/client/renderer/r_main.cpp
      ${UFOAI_SRC}/client/renderer/r_material.cpp
      ${UFOAI_SRC}/client/renderer/r_matrix.cpp
      ${UFOAI_SRC}/client/renderer/r_mesh.cpp
      ${UFOAI_SRC}/client/renderer/r_mesh_anim.cpp
      ${UFOAI_SRC}/client/renderer/r_misc.cpp
      ${UFOAI_SRC}/client/renderer/r_model.cpp
      ${UFOAI_SRC}/client/renderer/r_model_alias.cpp
      ${UFOAI_SRC}/client/renderer/r_model_brush.cpp
      ${UFOAI_SRC}/client/renderer/r_model_md2.cpp
      ${UFOAI_SRC}/client/renderer/r_model_md3.cpp
      ${UFOAI_SRC}/client/renderer/r_model_obj.cpp
      ${UFOAI_SRC}/client/renderer/r_particle.cpp
      ${UFOAI_SRC}/client/renderer/r_program.cpp
      ${UFOAI_SRC}/client/renderer/r_sdl.cpp
      ${UFOAI_SRC}/client/renderer/r_sphere.cpp
      ${UFOAI_SRC}/client/renderer/r_state.cpp
      ${UFOAI_SRC}/client/renderer/r_surface.cpp
      ${UFOAI_SRC}/client/renderer/r_thread.cpp
      ${UFOAI_SRC}/client/renderer/r_weather.cpp
      ${UFOAI_SRC}/client/sound/s_main.cpp
      ${UFOAI_SRC}/client/sound/s_mix.cpp
      ${UFOAI_SRC}/client/sound/s_mumble.cpp
      ${UFOAI_SRC}/client/sound/s_music.cpp
      ${UFOAI_SRC}/client/sound/s_sample.cpp
      ${UFOAI_SRC}/client/ui/node/ui_node_abstractnode.cpp
      ${UFOAI_SRC}/client/ui/node/ui_node_abstractoption.cpp
      ${UFOAI_SRC}/client/ui/node/ui_node_abstractscrollable.cpp
      ${UFOAI_SRC}/client/ui/node/ui_node_abstractscrollbar.cpp
      ${UFOAI_SRC}/client/ui/node/ui_node_abstractvalue.cpp
      ${UFOAI_SRC}/client/ui/node/ui_node_bar.cpp
      ${UFOAI_SRC}/client/ui/node/ui_node_base.cpp
      ${UFOAI_SRC}/client/ui/node/ui_node_baseinventory.cpp
      ${UFOAI_SRC}/client/ui/node/ui_node_battlescape.cpp
      ${UFOAI_SRC}/client/ui/node/ui_node_button.cpp
      ${UFOAI_SRC}/client/ui/node/ui_node_checkbox.cpp
      ${UFOAI_SRC}/client/ui/node/ui_node_container.cpp
      ${UFOAI_SRC}/client/ui/node/ui_node_controls.cpp
      ${UFOAI_SRC}/client/ui/node/ui_node_data.cpp
      ${UFOAI_SRC}/client/ui/node/ui_node_editor.cpp
      ${UFOAI_SRC}/client/ui/node/ui_node_geoscape.cpp
      ${UFOAI_SRC}/client/ui/node/ui_node_image.cpp
      ${UFOAI_SRC}/client/ui/node/ui_node_item.cpp
      ${UFOAI_SRC}/client/ui/node/ui_node_linechart.cpp
      ${UFOAI_SRC}/client/ui/node/ui_node_material_editor.cpp
      ${UFOAI_SRC}/client/ui/node/ui_node_messagelist.cpp
      ${UFOAI_SRC}/client/ui/node/ui_node_model.cpp
      ${UFOAI_SRC}/client/ui/node/ui_node_option.cpp
      ${UFOAI_SRC}/client/ui/node/ui_node_optionlist.cpp
      ${UFOAI_SRC}/client/ui/node/ui_node_optiontree.cpp
      ${UFOAI_SRC}/client/ui/node/ui_node_panel.cpp
      ${UFOAI_SRC}/client/ui/node/ui_node_radar.cpp
      ${UFOAI_SRC}/client/ui/node/ui_node_radiobutton.cpp
      ${UFOAI_SRC}/client/ui/node/ui_node_rows.cpp
      ${UFOAI_SRC}/client/ui/node/ui_node_selectbox.cpp
      ${UFOAI_SRC}/client/ui/node/ui_node_sequence.cpp
      ${UFOAI_SRC}/client/ui/node/ui_node_special.cpp
      ${UFOAI_SRC}/client/ui/node/ui_node_spinner.cpp
      ${UFOAI_SRC}/client/ui/node/ui_node_string.cpp
      ${UFOAI_SRC}/client/ui/node/ui_node_tab.cpp
      ${UFOAI_SRC}/client/ui/node/ui_node_tbar.cpp
      ${UFOAI_SRC}/client/ui/node/ui_node_text.cpp
      ${UFOAI_SRC}/client/ui/node/ui_node_text2.cpp
      ${UFOAI_SRC}/client/ui/node/ui_node_textentry.cpp
      ${UFOAI_SRC}/client/ui/node/ui_node_textlist.cpp
      ${UFOAI_SRC}/client/ui/node/ui_node_texture.cpp
      ${UFOAI_SRC}/client/ui/node/ui_node_timer.cpp
      ${UFOAI_SRC}/client/ui/node/ui_node_video.cpp
      ${UFOAI_SRC}/client/ui/node/ui_node_vscrollbar.cpp
      ${UFOAI_SRC}/client/ui/node/ui_node_window.cpp
      ${UFOAI_SRC}/client/ui/node/ui_node_zone.cpp
      ${UFOAI_SRC}/client/ui/swig/ui_lua_shared.cpp
      ${UFOAI_SRC}/client/ui/ui_actions.cpp
      ${UFOAI_SRC}/client/ui/ui_behaviour.cpp
      ${UFOAI_SRC}/client/ui/ui_components.cpp
      ${UFOAI_SRC}/client/ui/ui_data.cpp
      ${UFOAI_SRC}/client/ui/ui_dragndrop.cpp
      ${UFOAI_SRC}/client/ui/ui_draw.cpp
      ${UFOAI_SRC}/client/ui/ui_expression.cpp
      ${UFOAI_SRC}/client/ui/ui_font.cpp
      ${UFOAI_SRC}/client/ui/ui_input.cpp
      ${UFOAI_SRC}/client/ui/ui_lua.cpp
      ${UFOAI_SRC}/client/ui/ui_main.cpp
      ${UFOAI_SRC}/client/ui/ui_node.cpp
      ${UFOAI_SRC}/client/ui/ui_nodes.cpp
      ${UFOAI_SRC}/client/ui/ui_parse.cpp
      ${UFOAI_SRC}/client/ui/ui_popup.cpp
      ${UFOAI_SRC}/client/ui/ui_render.cpp
      ${UFOAI_SRC}/client/ui/ui_sound.cpp
      ${UFOAI_SRC}/client/ui/ui_sprite.cpp
      ${UFOAI_SRC}/client/ui/ui_timer.cpp
      ${UFOAI_SRC}/client/ui/ui_tooltip.cpp
      ${UFOAI_SRC}/client/ui/ui_windows.cpp
      ${UFOAI_SRC}/client/web/web_cgame.cpp
      ${UFOAI_SRC}/client/web/web_main.cpp
      ${UFOAI_SRC}/client/cl_lua.cpp
      ${UFOAI_SRC}/client/DateTime.cpp

      ${UFOAI_SRC}/common/binaryexpressionparser.cpp
      ${UFOAI_SRC}/common/bsp.cpp
      ${UFOAI_SRC}/common/cmd.cpp
      ${UFOAI_SRC}/common/cmodel.cpp
      ${UFOAI_SRC}/common/common.cpp
      ${UFOAI_SRC}/common/cvar.cpp
      ${UFOAI_SRC}/common/dbuffer.cpp
      ${UFOAI_SRC}/common/files.cpp
      ${UFOAI_SRC}/common/grid.cpp
      ${UFOAI_SRC}/common/hashtable.cpp
      ${UFOAI_SRC}/common/http.cpp
      ${UFOAI_SRC}/common/ioapi.cpp
      ${UFOAI_SRC}/common/list.cpp
      ${UFOAI_SRC}/common/md4.cpp
      ${UFOAI_SRC}/common/md5.cpp
      ${UFOAI_SRC}/common/mem.cpp
      ${UFOAI_SRC}/common/msg.cpp
      ${UFOAI_SRC}/common/net.cpp
      ${UFOAI_SRC}/common/netpack.cpp
      ${UFOAI_SRC}/common/pqueue.cpp
      ${UFOAI_SRC}/common/routing.cpp
      ${UFOAI_SRC}/common/scripts.cpp
      ${UFOAI_SRC}/common/scripts_lua.cpp
      ${UFOAI_SRC}/common/sha1.cpp
      ${UFOAI_SRC}/common/sha2.cpp
      ${UFOAI_SRC}/common/tracing.cpp
      ${UFOAI_SRC}/common/unzip.cpp
      ${UFOAI_SRC}/common/xml.cpp

      ${UFOAI_SRC}/game/chr_shared.cpp
      ${UFOAI_SRC}/game/g_actor.cpp
      ${UFOAI_SRC}/game/g_ai.cpp
      ${UFOAI_SRC}/game/g_ai_lua.cpp
      ${UFOAI_SRC}/game/g_camera.cpp
      ${UFOAI_SRC}/game/g_client.cpp
      ${UFOAI_SRC}/game/g_cmds.cpp
      ${UFOAI_SRC}/game/g_combat.cpp
      ${UFOAI_SRC}/game/g_edicts.cpp
      ${UFOAI_SRC}/game/g_events.cpp
      ${UFOAI_SRC}/game/g_func.cpp
      ${UFOAI_SRC}/game/g_health.cpp
      ${UFOAI_SRC}/game/g_inventory.cpp
      ${UFOAI_SRC}/game/g_main.cpp
      ${UFOAI_SRC}/game/g_match.cpp
      ${UFOAI_SRC}/game/g_mission.cpp
      ${UFOAI_SRC}/game/g_morale.cpp
      ${UFOAI_SRC}/game/g_move.cpp
      ${UFOAI_SRC}/game/g_reaction.cpp
      ${UFOAI_SRC}/game/g_round.cpp
      ${UFOAI_SRC}/game/g_spawn.cpp
      ${UFOAI_SRC}/game/g_stats.cpp
      ${UFOAI_SRC}/game/g_svcmds.cpp
      ${UFOAI_SRC}/game/g_trigger.cpp
      ${UFOAI_SRC}/game/g_utils.cpp
      ${UFOAI_SRC}/game/g_vis.cpp
      ${UFOAI_SRC}/game/inv_shared.cpp
      ${UFOAI_SRC}/game/inventory.cpp
      ${UFOAI_SRC}/game/q_shared.cpp

      ${UFOAI_SRC}/server/sv_ccmds.cpp
      ${UFOAI_SRC}/server/sv_game.cpp
      ${UFOAI_SRC}/server/sv_init.cpp
      ${UFOAI_SRC}/server/sv_log.cpp
      ${UFOAI_SRC}/server/sv_main.cpp
      ${UFOAI_SRC}/server/sv_mapcycle.cpp
      ${UFOAI_SRC}/server/sv_rma.cpp
      ${UFOAI_SRC}/server/sv_send.cpp
      ${UFOAI_SRC}/server/sv_user.cpp
      ${UFOAI_SRC}/server/sv_world.cpp

      ${UFOAI_SRC}/shared/aabb.cpp
      ${UFOAI_SRC}/shared/bfd.cpp
      ${UFOAI_SRC}/shared/byte.cpp
      ${UFOAI_SRC}/shared/entitiesdef.cpp
      ${UFOAI_SRC}/shared/images.cpp
      ${UFOAI_SRC}/shared/infostring.cpp
      ${UFOAI_SRC}/shared/mathlib.cpp
      ${UFOAI_SRC}/shared/mathlib_extra.cpp
      ${UFOAI_SRC}/shared/parse.cpp
      ${UFOAI_SRC}/shared/shared.cpp
      ${UFOAI_SRC}/shared/stringhunk.cpp
      ${UFOAI_SRC}/shared/utf8.cpp

      ${UFOAI_SRC}/tests/test_all.cpp
      ${UFOAI_SRC}/tests/test_campaign.cpp
      ${UFOAI_SRC}/tests/test_character.cpp
      ${UFOAI_SRC}/tests/test_dbuffer.cpp
      ${UFOAI_SRC}/tests/test_events.cpp
      ${UFOAI_SRC}/tests/test_game.cpp
      ${UFOAI_SRC}/tests/test_generic.cpp
      ${UFOAI_SRC}/tests/test_inventory.cpp
      ${UFOAI_SRC}/tests/test_mapdef.cpp
      ${UFOAI_SRC}/tests/test_parser.cpp
      ${UFOAI_SRC}/tests/test_particles.cpp
      ${UFOAI_SRC}/tests/test_renderer.cpp
      ${UFOAI_SRC}/tests/test_rma.cpp
      ${UFOAI_SRC}/tests/test_routing.cpp
      ${UFOAI_SRC}/tests/test_scripts.cpp
      ${UFOAI_SRC}/tests/test_shared.cpp
      ${UFOAI_SRC}/tests/test_ui.cpp
      ${UFOAI_SRC}/tests/test_ui_level2.cpp
      ${UFOAI_SRC}/tests/test_webapi.cpp

      ${UFOAI_SRC}/libs/gtest/src/gtest-all.cc
      ${UFOAI_SRC}/libs/mumble/libmumblelink.c
      ${UFOAI_SRC}/libs/mxml/mxml-attr.c
      ${UFOAI_SRC}/libs/mxml/mxml-entity.c
      ${UFOAI_SRC}/libs/mxml/mxml-file.c
      ${UFOAI_SRC}/libs/mxml/mxml-index.c
      ${UFOAI_SRC}/libs/mxml/mxml-node.c
      ${UFOAI_SRC}/libs/mxml/mxml-private.c
      ${UFOAI_SRC}/libs/mxml/mxml-search.c
      ${UFOAI_SRC}/libs/mxml/mxml-set.c
      ${UFOAI_SRC}/libs/mxml/mxml-string.c
   )

   # platform specific sources
   if(WIN32)
      SET(UFOAI_SOURCEFILES_UFOTESTALL ${UFOAI_SOURCEFILES_UFOTESTALL}
         ${UFOAI_SRC}/ports/windows/win_backtrace.cpp
         ${UFOAI_SRC}/ports/windows/win_console.cpp
         ${UFOAI_SRC}/ports/windows/win_shared.cpp
      )
   elseif(UNIX)
       SET(UFOAI_SOURCEFILES_UFOTESTALL ${UFOAI_SOURCEFILES_UFOTESTALL}
         ${UFOAI_SRC}/ports/unix/unix_console.cpp
         ${UFOAI_SRC}/ports/unix/unix_files.cpp
         ${UFOAI_SRC}/ports/unix/unix_shared.cpp
         ${UFOAI_SRC}/ports/unix/unix_main.cpp
         )
   elseif(SOLARIS)
       SET(UFOAI_SOURCEFILES_UFOTESTALL ${UFOAI_SOURCEFILES_UFOTESTALL}
         ${UFOAI_SRC}/ports/unix/unix_console.cpp
         ${UFOAI_SRC}/ports/unix/unix_files.cpp
         ${UFOAI_SRC}/ports/unix/unix_shared.cpp
         ${UFOAI_SRC}/ports/unix/unix_main.cpp
         )
   elseif(APPLE)
       SET(UFOAI_SOURCEFILES_UFOTESTALL ${UFOAI_SOURCEFILES_UFOTESTALL}
         ${UFOAI_SRC}/ports/unix/unix_console.cpp
         ${UFOAI_SRC}/ports/unix/unix_files.cpp
         ${UFOAI_SRC}/ports/unix/unix_shared.cpp
         ${UFOAI_SRC}/ports/unix/unix_main.cpp
         )
   endif()

   # ufotestall is executable
   add_executable(${UFOAI_PROJECT_UFOTESTALL} ${UFOAI_SOURCEFILES_UFOTESTALL})
   add_dependencies(${UFOAI_PROJECT_UFOTESTALL} "game")

   # For generic no-config case (e.g. with mingw, gcc, ...)
   set_target_properties(${UFOAI_PROJECT_UFOTESTALL} PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
   # For multi-config builds like msvc
   foreach(OUTPUTCONFIG ${CMAKE_CONFIGURATION_TYPES})
      string(TOUPPER ${OUTPUTCONFIG} OUTPUTCONFIG)
      set_target_properties(${UFOAI_PROJECT_UFOTESTALL} PROPERTIES RUNTIME_OUTPUT_DIRECTORY_${OUTPUTCONFIG}  ${CMAKE_BINARY_DIR})
   endforeach(OUTPUTCONFIG CMAKE_CONFIGURATION_TYPES)

   target_compile_definitions(${UFOAI_PROJECT_UFOTESTALL} PRIVATE -DCOMPILE_UFO)
   target_compile_definitions(${UFOAI_PROJECT_UFOTESTALL} PRIVATE -DCOMPILE_UNITTESTS)
   target_compile_definitions(${UFOAI_PROJECT_UFOTESTALL} PRIVATE -DHARD_LINKED_GAME)
   target_compile_definitions(${UFOAI_PROJECT_UFOTESTALL} PRIVATE -DHARD_LINKED_CGAME)

   target_include_directories(${UFOAI_PROJECT_UFOTESTALL}
      PRIVATE ${UFOAI_SRC}/libs/gtest
      PRIVATE ${UFOAI_SRC}/libs/gtest/include
      PRIVATE ${UFOAI_SRC}/libs/mumble
      PRIVATE ${UFOAI_SRC}/libs/mxml
      PRIVATE ${LUA_INCLUDE_DIR}
      PRIVATE ${SDL_INCLUDE_DIR}
      PRIVATE ${SDLMIXER_INCLUDE_DIR}
      PRIVATE ${SDLTTF_INCLUDE_DIR}
      PRIVATE ${CURL_INCLUDE_DIRS}
      PRIVATE ${VORBIS_INCLUDE_DIR}
      PRIVATE ${OGG_INCLUDE_DIR}
      PRIVATE ${JPEG_INCLUDE_DIRS}
      PRIVATE ${PNG_INCLUDE_DIRS}
      PRIVATE ${Intl_INCLUDE_DIRS}
      PRIVATE ${ZLIB_INCLUDE_DIRS}
      PRIVATE ${X11_INCLUDE_DIR}
   )

   if(MINGW)
      target_link_libraries(${UFOAI_PROJECT_UFOTESTALL}
         mingw32
      )
   endif()

   target_link_libraries(${UFOAI_PROJECT_UFOTESTALL}
      ${LUA_LIBRARY}
      ${SDL_LIBRARY_MAIN}
      ${SDL_LIBRARY}
      ${SDL_MIXER_LIBRARIES}
      ${SDL_TTF_LIBRARIES}
      ${CURL_LIBRARIES}
      ${VORBIS_LIBRARY}
      ${OGG_LIBRARIES}
      ${JPEG_LIBRARIES}
      ${PNG_LIBRARIES}
      ${ZLIB_LIBRARIES}
      ${X11_LIBRARIES}
   )

   if(WIN32)
      target_link_libraries(${UFOAI_PROJECT_UFOTESTALL}
         opengl32
         imm32
         ${Intl_LIBRARIES}
         version
         winmm
         Ws2_32
      )
   elseif(APPLE)
      target_link_libraries(${UFOAI_PROJECT_UFOTESTALL}
         ${Intl_LIBRARIES}
      )
   elseif(UNIX)
      target_link_libraries(${UFOAI_PROJECT_UFOTESTALL}
         GL
      )
      if(NOT CMAKE_SYSTEM_NAME MATCHES "OpenBSD")
          target_link_libraries(${UFOAI_PROJECT_UFOTESTALL}
             rt
          )
      endif()
   endif()
endif()

add_subdirectory("src/tools/radiant/")
