
cmake_minimum_required(VERSION 2.8)

# Project settings
project(ufoai)
set(UFOAI_PROJECT_GAME "game")
set(UFOAI_PROJECT_UFO2MAP "ufo2map")
set(UFOAI_PROJECT_UFODED "ufoded")
set(UFOAI_PROJECT_UFO "ufo")
set(UFOAI_PROJECT_UFOMODEL "ufomodel")
set(UFOAI_PROJECT_UFOSLICER "ufoslicer")
set(UFOAI_PROJECT_UFORADIANT "uforadiant")
set(UFOAI_PROJECT_UFOTESTALL "ufotestall")

# cmake paths
set(CMAKE_CONFIG_DIR "${CMAKE_SOURCE_DIR}/cmake/config")
set(CMAKE_MODULE_PATH
   ${CMAKE_MODULE_PATH}
   ${CMAKE_SOURCE_DIR}/cmake/modules
)

if(NOT CMAKE_BUILD_TYPE)
   message(STATUS "CMake build type is not set, defaulting to 'RelWithDebInfo'")
   set(CMAKE_BUILD_TYPE "RelWithDebInfo")
endif()

if("${CMAKE_SIZEOF_VOID_P}" EQUAL "8")
   message(STATUS "Target is 64 bits")
elseif("${CMAKE_SIZEOF_VOID_P}" EQUAL "4")
   message(STATUS "Target is 32 bits")
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
   foreach(RES base contrib radiant)
      if(NOT EXISTS ${CMAKE_BINARY_DIR}/${RES})
         message(STATUS "Copying ${CMAKE_BINARY_DIR}/${RES}. This may take some time")
         file(COPY ${CMAKE_SOURCE_DIR}/${RES} DESTINATION ${CMAKE_BINARY_DIR})
      endif()
   endforeach(RES)
endif()

## Projects we want to build
option(UFOAI_BUILD_GAME "Build the library handling the battlescape" ON)
option(UFOAI_BUILD_UFO2MAP "Build the tool to compile maps" OFF)
option(UFOAI_BUILD_UFODED "Build the dedicated server" OFF)
option(UFOAI_BUILD_UFO "Build the main game" ON)
option(UFOAI_BUILD_UFOMODEL "Build the tool for pre-compiling lightmaps for models" OFF)
option(UFOAI_BUILD_UFOSLICER "Build the tool for creating fancy map overviews" OFF)
option(UFOAI_BUILD_UFORADIANT "Build the map editor (needs GTK)" OFF)
option(UFOAI_BUILD_UFOTESTALL "Build unitary tests" OFF)

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
      ${UFOAI_DEPENDENCIES_DIR}/curl/include
      ${UFOAI_DEPENDENCIES_DIR}/curl/lib${UFOAI_ARCH_SUFIX}
      ${UFOAI_DEPENDENCIES_DIR}/gtk/include
      ${UFOAI_DEPENDENCIES_DIR}/gtk/lib${UFOAI_ARCH_SUFIX}
      ${UFOAI_DEPENDENCIES_DIR}/jpeg/include
      ${UFOAI_DEPENDENCIES_DIR}/jpeg/lib${UFOAI_ARCH_SUFIX}
      ${UFOAI_DEPENDENCIES_DIR}/intl/include
      ${UFOAI_DEPENDENCIES_DIR}/intl/lib${UFOAI_ARCH_SUFIX}
      ${UFOAI_DEPENDENCIES_DIR}/ogg/include
      ${UFOAI_DEPENDENCIES_DIR}/ogg/lib${UFOAI_ARCH_SUFIX}
      ${UFOAI_DEPENDENCIES_DIR}/openal/include
      ${UFOAI_DEPENDENCIES_DIR}/openal/lib${UFOAI_ARCH_SUFIX}
      ${UFOAI_DEPENDENCIES_DIR}/png/include
      ${UFOAI_DEPENDENCIES_DIR}/png/lib${UFOAI_ARCH_SUFIX}
      ${UFOAI_DEPENDENCIES_DIR}/SDL2/include
      ${UFOAI_DEPENDENCIES_DIR}/SDL2/lib${UFOAI_ARCH_SUFIX}
      ${UFOAI_DEPENDENCIES_DIR}/vorbis/include
      ${UFOAI_DEPENDENCIES_DIR}/vorbis/lib${UFOAI_ARCH_SUFIX}
      ${UFOAI_DEPENDENCIES_DIR}/zlib/include
      ${UFOAI_DEPENDENCIES_DIR}/zlib/lib${UFOAI_ARCH_SUFIX}
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

if(UFOAI_BUILD_UFORADIANT)
   find_package(GTK)
endif()

if(UFOAI_BUILD_UFO2MAP OR
      UFOAI_BUILD_UFO OR
      UFOAI_BUILD_UFOMODEL OR
      UFOAI_BUILD_UFOSLICER OR
      UFOAI_BUILD_UFOTESTALL)
   find_package(JPEG)
   find_package(X11)
   find_package(OpenGL)
endif()

if(UFOAI_BUILD_UFO OR
      UFOAI_BUILD_UFORADIANT OR
      UFOAI_BUILD_UFOTESTALL)
   find_package(LibIntl)
   find_package(OGG)
endif()

if(UFOAI_BUILD_UFORADIANT)
   find_package(OpenAL)
endif()

if(UFOAI_BUILD_UFO2MAP OR
      UFOAI_BUILD_UFO OR
      UFOAI_BUILD_UFOMODEL OR
      UFOAI_BUILD_UFOSLICER OR
      UFOAI_BUILD_UFOTESTALL)
   find_package(PNG)
endif()

if(UFOAI_BUILD_GAME OR
      UFOAI_BUILD_UFO2MAP OR
      UFOAI_BUILD_UFODED OR
      UFOAI_BUILD_UFO OR
      UFOAI_BUILD_UFOMODEL OR
      UFOAI_BUILD_UFOSLICER OR
      UFOAI_BUILD_UFORADIANT OR
      UFOAI_BUILD_UFOTESTALL)
   find_package(SDL)
endif()

if(UFOAI_BUILD_UFO OR
      UFOAI_BUILD_UFOTESTALL)
   find_package(SDL_mixer)
endif()

if(UFOAI_BUILD_UFO OR
      UFOAI_BUILD_UFOTESTALL)
   find_package(SDL_ttf)
endif()

if(UFOAI_BUILD_UFO OR
      UFOAI_BUILD_UFORADIANT OR
      UFOAI_BUILD_UFOTESTALL)
   find_package(VORBIS)
endif()

if(UFOAI_BUILD_UFO2MAP OR
      UFOAI_BUILD_UFODED OR
      UFOAI_BUILD_UFO OR
      UFOAI_BUILD_UFOMODEL OR
      UFOAI_BUILD_UFOSLICER OR
      UFOAI_BUILD_UFORADIANT OR
      UFOAI_BUILD_UFOTESTALL)
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

if(UFOAI_BUILD_GAME)
   set(UFOAI_SOURCEFILES_GAME
      ${UFOAI_SRC}/shared/aabb.cpp
      ${UFOAI_SRC}/shared/infostring.cpp
      ${UFOAI_SRC}/shared/mathlib.cpp
      ${UFOAI_SRC}/shared/parse.cpp
      ${UFOAI_SRC}/shared/shared.cpp
      ${UFOAI_SRC}/shared/utf8.cpp

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

      ${UFOAI_SRC}/libs/lua/lapi.c
      ${UFOAI_SRC}/libs/lua/lauxlib.c
      ${UFOAI_SRC}/libs/lua/lbaselib.c
      ${UFOAI_SRC}/libs/lua/lcode.c
      ${UFOAI_SRC}/libs/lua/ldblib.c
      ${UFOAI_SRC}/libs/lua/ldebug.c
      ${UFOAI_SRC}/libs/lua/ldo.c
      ${UFOAI_SRC}/libs/lua/ldump.c
      ${UFOAI_SRC}/libs/lua/lfunc.c
      ${UFOAI_SRC}/libs/lua/lgc.c
      ${UFOAI_SRC}/libs/lua/linit.c
      ${UFOAI_SRC}/libs/lua/liolib.c
      ${UFOAI_SRC}/libs/lua/llex.c
      ${UFOAI_SRC}/libs/lua/lmathlib.c
      ${UFOAI_SRC}/libs/lua/lmem.c
      ${UFOAI_SRC}/libs/lua/loadlib.c
      ${UFOAI_SRC}/libs/lua/lobject.c
      ${UFOAI_SRC}/libs/lua/lopcodes.c
      ${UFOAI_SRC}/libs/lua/loslib.c
      ${UFOAI_SRC}/libs/lua/lparser.c
      ${UFOAI_SRC}/libs/lua/lstate.c
      ${UFOAI_SRC}/libs/lua/lstring.c
      ${UFOAI_SRC}/libs/lua/lstrlib.c
      ${UFOAI_SRC}/libs/lua/ltable.c
      ${UFOAI_SRC}/libs/lua/ltablib.c
      ${UFOAI_SRC}/libs/lua/ltm.c
      ${UFOAI_SRC}/libs/lua/lundump.c
      ${UFOAI_SRC}/libs/lua/lvm.c
      ${UFOAI_SRC}/libs/lua/lzio.c
      ${UFOAI_SRC}/libs/lua/print.c
   )

   # game is a shared library
   add_library(${UFOAI_PROJECT_GAME} SHARED ${UFOAI_SOURCEFILES_GAME})

   # we want the target library to be game.xxx and not libgame.xxx
   set_target_properties(${UFOAI_PROJECT_GAME} PROPERTIES PREFIX "")
   if(UNIX)
      set(UFOAI_OUTPUT_TYPE_GAME "LIBRARY_OUTPUT_DIRECTORY")
   else()
      set(UFOAI_OUTPUT_TYPE_GAME "RUNTIME_OUTPUT_DIRECTORY")
   endif()
   # For generic no-config case (e.g. with mingw, gcc, ...)
   set_target_properties(${UFOAI_PROJECT_GAME} PROPERTIES ${UFOAI_OUTPUT_TYPE_GAME} ${CMAKE_BINARY_DIR}/base)
   # For multi-config builds like msvc
   set(UFOAI_OUTPUT_TYPE_GAME ${UFOAI_OUTPUT_TYPE_GAME}_)
   foreach(OUTPUTCONFIG ${CMAKE_CONFIGURATION_TYPES})
      string(TOUPPER ${OUTPUTCONFIG} OUTPUTCONFIG)
      set_target_properties(${UFOAI_PROJECT_GAME} PROPERTIES ${UFOAI_OUTPUT_TYPE_GAME}${OUTPUTCONFIG}  ${CMAKE_BINARY_DIR}/base)
   endforeach(OUTPUTCONFIG CMAKE_CONFIGURATION_TYPES)

   target_compile_definitions(${UFOAI_PROJECT_GAME} PRIVATE -DCOMPILE_UFO)

   target_include_directories(${UFOAI_PROJECT_GAME}
      PRIVATE ${UFOAI_SRC}/libs/lua
      PRIVATE ${SDL_INCLUDE_DIR}
   )
endif()

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

   # plateform specific sources
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
      PRIVATE ${JPEG_INCLUDE_DIR}
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

   # plateform specific sources
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
      PRIVATE ${CURL_INCLUDE_DIR}
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

      ${UFOAI_SRC}/libs/lua/lapi.c
      ${UFOAI_SRC}/libs/lua/lauxlib.c
      ${UFOAI_SRC}/libs/lua/lbaselib.c
      ${UFOAI_SRC}/libs/lua/lcode.c
      ${UFOAI_SRC}/libs/lua/ldblib.c
      ${UFOAI_SRC}/libs/lua/ldebug.c
      ${UFOAI_SRC}/libs/lua/ldo.c
      ${UFOAI_SRC}/libs/lua/ldump.c
      ${UFOAI_SRC}/libs/lua/lfunc.c
      ${UFOAI_SRC}/libs/lua/lgc.c
      ${UFOAI_SRC}/libs/lua/linit.c
      ${UFOAI_SRC}/libs/lua/liolib.c
      ${UFOAI_SRC}/libs/lua/llex.c
      ${UFOAI_SRC}/libs/lua/lmathlib.c
      ${UFOAI_SRC}/libs/lua/lmem.c
      ${UFOAI_SRC}/libs/lua/loadlib.c
      ${UFOAI_SRC}/libs/lua/lobject.c
      ${UFOAI_SRC}/libs/lua/lopcodes.c
      ${UFOAI_SRC}/libs/lua/loslib.c
      ${UFOAI_SRC}/libs/lua/lparser.c
      ${UFOAI_SRC}/libs/lua/lstate.c
      ${UFOAI_SRC}/libs/lua/lstring.c
      ${UFOAI_SRC}/libs/lua/lstrlib.c
      ${UFOAI_SRC}/libs/lua/ltable.c
      ${UFOAI_SRC}/libs/lua/ltablib.c
      ${UFOAI_SRC}/libs/lua/ltm.c
      ${UFOAI_SRC}/libs/lua/lundump.c
      ${UFOAI_SRC}/libs/lua/lvm.c
      ${UFOAI_SRC}/libs/lua/lzio.c
      ${UFOAI_SRC}/libs/lua/print.c

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

   # plateform specific sources
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
      PRIVATE ${UFOAI_SRC}/libs/lua
      PRIVATE ${UFOAI_SRC}/libs/mumble
      PRIVATE ${UFOAI_SRC}/libs/mxml
      PRIVATE ${SDL_INCLUDE_DIR}
      PRIVATE ${SDLMIXER_INCLUDE_DIR}
      PRIVATE ${SDLTTF_INCLUDE_DIR}
      PRIVATE ${CURL_INCLUDE_DIR}
      PRIVATE ${VORBIS_INCLUDE_DIR}
      PRIVATE ${OGG_INCLUDE_DIR}
      PRIVATE ${JPEG_INCLUDE_DIR}
      PRIVATE ${PNG_INCLUDE_DIRS}
      PRIVATE ${LIBINTL_INCLUDE_DIR}
      PRIVATE ${ZLIB_INCLUDE_DIRS}
      PRIVATE ${OPENGL_INCLUDE_DIR}
   )

   if(MINGW)
      target_link_libraries(${UFOAI_PROJECT_UFO}
         mingw32
      )
   endif()

   target_link_libraries(${UFOAI_PROJECT_UFO}
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
         ${LIBINTL_LIBRARIES}
         winmm
         Ws2_32
      )
   elseif(WIN32)
      target_link_libraries(${UFOAI_PROJECT_UFO}
         opengl32
         imm32
         ${LIBINTL_LIBRARIES}
         version
         winmm
         Ws2_32
      )
   elseif(APPLE)
      target_link_libraries(${UFOAI_PROJECT_UFO}
         ${LIBINTL_LIBRARIES}
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
            ${LIBINTL_LIBRARIES}
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

   # plateform specific sources
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
      PRIVATE ${JPEG_INCLUDE_DIR}
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

   # plateform specific sources
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
      PRIVATE ${JPEG_INCLUDE_DIR}
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
# uforadiant
###############################################################################

if(UFOAI_BUILD_UFORADIANT)
   set(UFOAI_SOURCEFILES_UFORADIANT
      ${UFOAI_SRC}/shared/entitiesdef.cpp
      ${UFOAI_SRC}/shared/parse.cpp

      ${UFOAI_SRC}/tools/radiant/libs/Instance.cpp
      ${UFOAI_SRC}/tools/radiant/libs/gtkutil/MenuItemAccelerator.cpp
      ${UFOAI_SRC}/tools/radiant/libs/gtkutil/ModalProgressDialog.cpp
      ${UFOAI_SRC}/tools/radiant/libs/gtkutil/PanedPosition.cpp
      ${UFOAI_SRC}/tools/radiant/libs/gtkutil/RegistryConnector.cpp
      ${UFOAI_SRC}/tools/radiant/libs/gtkutil/SourceView.cpp
      ${UFOAI_SRC}/tools/radiant/libs/gtkutil/TextEntryDialog.cpp
      ${UFOAI_SRC}/tools/radiant/libs/gtkutil/TextPanel.cpp
      ${UFOAI_SRC}/tools/radiant/libs/gtkutil/Timer.cpp
      ${UFOAI_SRC}/tools/radiant/libs/gtkutil/TreeModel.cpp
      ${UFOAI_SRC}/tools/radiant/libs/gtkutil/VFSTreePopulator.cpp
      ${UFOAI_SRC}/tools/radiant/libs/gtkutil/WindowPosition.cpp
      ${UFOAI_SRC}/tools/radiant/libs/gtkutil/button.cpp
      ${UFOAI_SRC}/tools/radiant/libs/gtkutil/clipboard.cpp
      ${UFOAI_SRC}/tools/radiant/libs/gtkutil/cursor.cpp
      ${UFOAI_SRC}/tools/radiant/libs/gtkutil/dialog.cpp
      ${UFOAI_SRC}/tools/radiant/libs/gtkutil/filechooser.cpp
      ${UFOAI_SRC}/tools/radiant/libs/gtkutil/frame.cpp
      ${UFOAI_SRC}/tools/radiant/libs/gtkutil/glfont.cpp
      ${UFOAI_SRC}/tools/radiant/libs/gtkutil/glwidget.cpp
      ${UFOAI_SRC}/tools/radiant/libs/gtkutil/image.cpp
      ${UFOAI_SRC}/tools/radiant/libs/gtkutil/menu.cpp
      ${UFOAI_SRC}/tools/radiant/libs/gtkutil/menu/PopupMenu.cpp
      ${UFOAI_SRC}/tools/radiant/libs/gtkutil/messagebox.cpp
      ${UFOAI_SRC}/tools/radiant/libs/gtkutil/window.cpp
      ${UFOAI_SRC}/tools/radiant/libs/gtkutil/window/PersistentTransientWindow.cpp
      ${UFOAI_SRC}/tools/radiant/libs/picomodel/RenderablePicoModel.cpp
      ${UFOAI_SRC}/tools/radiant/libs/picomodel/RenderablePicoSurface.cpp
      ${UFOAI_SRC}/tools/radiant/libs/picomodel/model.cpp
      ${UFOAI_SRC}/tools/radiant/libs/xmlutil/Document.cpp
      ${UFOAI_SRC}/tools/radiant/libs/xmlutil/Node.cpp
      ${UFOAI_SRC}/tools/radiant/radiant/brush/Brush.cpp
      ${UFOAI_SRC}/tools/radiant/radiant/brush/BrushClipPlane.cpp
      ${UFOAI_SRC}/tools/radiant/radiant/brush/BrushInstance.cpp
      ${UFOAI_SRC}/tools/radiant/radiant/brush/BrushModule.cpp
      ${UFOAI_SRC}/tools/radiant/radiant/brush/BrushTokens.cpp
      ${UFOAI_SRC}/tools/radiant/radiant/brush/ContentsFlagsValue.cpp
      ${UFOAI_SRC}/tools/radiant/radiant/brush/Face.cpp
      ${UFOAI_SRC}/tools/radiant/radiant/brush/FaceInstance.cpp
      ${UFOAI_SRC}/tools/radiant/radiant/brush/FaceShader.cpp
      ${UFOAI_SRC}/tools/radiant/radiant/brush/FaceTexDef.cpp
      ${UFOAI_SRC}/tools/radiant/radiant/brush/TexDef.cpp
      ${UFOAI_SRC}/tools/radiant/radiant/brush/TextureProjection.cpp
      ${UFOAI_SRC}/tools/radiant/radiant/brush/brushmanip.cpp
      ${UFOAI_SRC}/tools/radiant/radiant/brush/construct/Cone.cpp
      ${UFOAI_SRC}/tools/radiant/radiant/brush/construct/Cuboid.cpp
      ${UFOAI_SRC}/tools/radiant/radiant/brush/construct/Prism.cpp
      ${UFOAI_SRC}/tools/radiant/radiant/brush/construct/Rock.cpp
      ${UFOAI_SRC}/tools/radiant/radiant/brush/construct/Sphere.cpp
      ${UFOAI_SRC}/tools/radiant/radiant/brush/construct/Terrain.cpp
      ${UFOAI_SRC}/tools/radiant/radiant/brush/csg/csg.cpp
      ${UFOAI_SRC}/tools/radiant/radiant/brush/winding.cpp
      ${UFOAI_SRC}/tools/radiant/radiant/brushexport/BrushExportOBJ.cpp
      ${UFOAI_SRC}/tools/radiant/radiant/camera/CamWnd.cpp
      ${UFOAI_SRC}/tools/radiant/radiant/camera/Camera.cpp
      ${UFOAI_SRC}/tools/radiant/radiant/camera/CameraSettings.cpp
      ${UFOAI_SRC}/tools/radiant/radiant/camera/GlobalCamera.cpp
      ${UFOAI_SRC}/tools/radiant/radiant/camera/view.cpp
      ${UFOAI_SRC}/tools/radiant/radiant/clipper/ClipPoint.cpp
      ${UFOAI_SRC}/tools/radiant/radiant/clipper/Clipper.cpp
      ${UFOAI_SRC}/tools/radiant/radiant/commands.cpp
      ${UFOAI_SRC}/tools/radiant/radiant/dialog.cpp
      ${UFOAI_SRC}/tools/radiant/radiant/entity/EntityCreator.cpp
      ${UFOAI_SRC}/tools/radiant/radiant/entity/EntitySettings.cpp
      ${UFOAI_SRC}/tools/radiant/radiant/entity/eclassmodel/EClassModel.cpp
      ${UFOAI_SRC}/tools/radiant/radiant/entity/eclassmodel/EClassModelInstance.cpp
      ${UFOAI_SRC}/tools/radiant/radiant/entity/eclassmodel/EClassModelNode.cpp
      ${UFOAI_SRC}/tools/radiant/radiant/entity/entity.cpp
      ${UFOAI_SRC}/tools/radiant/radiant/entity/entitydef/EntityClassManager.cpp
      ${UFOAI_SRC}/tools/radiant/radiant/entity/entitydef/EntityClassMgrModule.cpp
      ${UFOAI_SRC}/tools/radiant/radiant/entity/entitydef/EntityClassScanner.cpp
      ${UFOAI_SRC}/tools/radiant/radiant/entity/entitydef/EntityDefinitionModule.cpp
      ${UFOAI_SRC}/tools/radiant/radiant/entity/entitymodule.cpp
      ${UFOAI_SRC}/tools/radiant/radiant/entity/generic/GenericEntity.cpp
      ${UFOAI_SRC}/tools/radiant/radiant/entity/generic/GenericEntityInstance.cpp
      ${UFOAI_SRC}/tools/radiant/radiant/entity/generic/GenericEntityNode.cpp
      ${UFOAI_SRC}/tools/radiant/radiant/entity/group/Group.cpp
      ${UFOAI_SRC}/tools/radiant/radiant/entity/group/GroupInstance.cpp
      ${UFOAI_SRC}/tools/radiant/radiant/entity/group/GroupNode.cpp
      ${UFOAI_SRC}/tools/radiant/radiant/entity/light/Light.cpp
      ${UFOAI_SRC}/tools/radiant/radiant/entity/light/LightInstance.cpp
      ${UFOAI_SRC}/tools/radiant/radiant/entity/light/LightNode.cpp
      ${UFOAI_SRC}/tools/radiant/radiant/entity/model/MiscModel.cpp
      ${UFOAI_SRC}/tools/radiant/radiant/entity/model/MiscModelInstance.cpp
      ${UFOAI_SRC}/tools/radiant/radiant/entity/model/MiscModelNode.cpp
      ${UFOAI_SRC}/tools/radiant/radiant/entity/particle/MiscParticle.cpp
      ${UFOAI_SRC}/tools/radiant/radiant/entity/particle/MiscParticleInstance.cpp
      ${UFOAI_SRC}/tools/radiant/radiant/entity/particle/MiscParticleNode.cpp
      ${UFOAI_SRC}/tools/radiant/radiant/entity/sound/MiscSound.cpp
      ${UFOAI_SRC}/tools/radiant/radiant/entity/sound/MiscSoundInstance.cpp
      ${UFOAI_SRC}/tools/radiant/radiant/entity/sound/MiscSoundNode.cpp
      ${UFOAI_SRC}/tools/radiant/radiant/entity/targetable.cpp
      ${UFOAI_SRC}/tools/radiant/radiant/environment.cpp
      ${UFOAI_SRC}/tools/radiant/radiant/eventmanager/Accelerator.cpp
      ${UFOAI_SRC}/tools/radiant/radiant/eventmanager/Command.cpp
      ${UFOAI_SRC}/tools/radiant/radiant/eventmanager/EventManager.cpp
      ${UFOAI_SRC}/tools/radiant/radiant/eventmanager/Modifiers.cpp
      ${UFOAI_SRC}/tools/radiant/radiant/eventmanager/MouseEvents.cpp
      ${UFOAI_SRC}/tools/radiant/radiant/eventmanager/Toggle.cpp
      ${UFOAI_SRC}/tools/radiant/radiant/eventmanager/WidgetToggle.cpp
      ${UFOAI_SRC}/tools/radiant/radiant/exec.cpp
      ${UFOAI_SRC}/tools/radiant/radiant/filesystem/FileSystem.cpp
      ${UFOAI_SRC}/tools/radiant/radiant/filesystem/FileSystemModule.cpp
      ${UFOAI_SRC}/tools/radiant/radiant/filesystem/directory/DirectoryArchive.cpp
      ${UFOAI_SRC}/tools/radiant/radiant/filesystem/pk3/ZipArchive.cpp
      ${UFOAI_SRC}/tools/radiant/radiant/filetypes.cpp
      ${UFOAI_SRC}/tools/radiant/radiant/filters/BasicFilterSystem.cpp
      ${UFOAI_SRC}/tools/radiant/radiant/filters/LevelFilter.cpp
      ${UFOAI_SRC}/tools/radiant/radiant/filters/XMLFilter.cpp
      ${UFOAI_SRC}/tools/radiant/radiant/filters/filters.cpp
      ${UFOAI_SRC}/tools/radiant/radiant/image.cpp
      ${UFOAI_SRC}/tools/radiant/radiant/imagemodules.cpp
      ${UFOAI_SRC}/tools/radiant/radiant/log/console.cpp
      ${UFOAI_SRC}/tools/radiant/radiant/main.cpp
      ${UFOAI_SRC}/tools/radiant/radiant/map/AutoSaver.cpp
      ${UFOAI_SRC}/tools/radiant/radiant/map/MapCompiler.cpp
      ${UFOAI_SRC}/tools/radiant/radiant/map/MapFileChooserPreview.cpp
      ${UFOAI_SRC}/tools/radiant/radiant/map/RootNode.cpp
      ${UFOAI_SRC}/tools/radiant/radiant/map/algorithm/Traverse.cpp
      ${UFOAI_SRC}/tools/radiant/radiant/map/map.cpp
      ${UFOAI_SRC}/tools/radiant/radiant/map/mapmodule.cpp
      ${UFOAI_SRC}/tools/radiant/radiant/map/parse.cpp
      ${UFOAI_SRC}/tools/radiant/radiant/map/write.cpp
      ${UFOAI_SRC}/tools/radiant/radiant/material/LicenseParser.cpp
      ${UFOAI_SRC}/tools/radiant/radiant/material/MaterialModule.cpp
      ${UFOAI_SRC}/tools/radiant/radiant/material/MaterialShader.cpp
      ${UFOAI_SRC}/tools/radiant/radiant/material/MaterialShaderModule.cpp
      ${UFOAI_SRC}/tools/radiant/radiant/material/MaterialShaderSystem.cpp
      ${UFOAI_SRC}/tools/radiant/radiant/material/MaterialSystem.cpp
      ${UFOAI_SRC}/tools/radiant/radiant/model.cpp
      ${UFOAI_SRC}/tools/radiant/radiant/namespace/NameObserver.cpp
      ${UFOAI_SRC}/tools/radiant/radiant/namespace/Namespace.cpp
      ${UFOAI_SRC}/tools/radiant/radiant/namespace/NamespaceModule.cpp
      ${UFOAI_SRC}/tools/radiant/radiant/particles/ParticleParser.cpp
      ${UFOAI_SRC}/tools/radiant/radiant/particles/ParticleSystem.cpp
      ${UFOAI_SRC}/tools/radiant/radiant/pathfinding/Pathfinding.cpp
      ${UFOAI_SRC}/tools/radiant/radiant/pathfinding/PathfindingModule.cpp
      ${UFOAI_SRC}/tools/radiant/radiant/pathfinding/Routing.cpp
      ${UFOAI_SRC}/tools/radiant/radiant/pathfinding/RoutingLump.cpp
      ${UFOAI_SRC}/tools/radiant/radiant/pathfinding/RoutingLumpLoader.cpp
      ${UFOAI_SRC}/tools/radiant/radiant/pathfinding/RoutingRenderable.cpp
      ${UFOAI_SRC}/tools/radiant/radiant/plugin.cpp
      ${UFOAI_SRC}/tools/radiant/radiant/referencecache/nullmodel.cpp
      ${UFOAI_SRC}/tools/radiant/radiant/referencecache/referencecache.cpp
      ${UFOAI_SRC}/tools/radiant/radiant/render/OpenGLModule.cpp
      ${UFOAI_SRC}/tools/radiant/radiant/render/OpenGLRenderSystem.cpp
      ${UFOAI_SRC}/tools/radiant/radiant/render/backend/OpenGLShader.cpp
      ${UFOAI_SRC}/tools/radiant/radiant/render/backend/OpenGLShaderPass.cpp
      ${UFOAI_SRC}/tools/radiant/radiant/render/frontend/SphereRenderable.cpp
      ${UFOAI_SRC}/tools/radiant/radiant/scenegraph/CompiledGraph.cpp
      ${UFOAI_SRC}/tools/radiant/radiant/scenegraph/SceneGraphModule.cpp
      ${UFOAI_SRC}/tools/radiant/radiant/scenegraph/treemodel.cpp
      ${UFOAI_SRC}/tools/radiant/radiant/script/ScriptLibrary.cpp
      ${UFOAI_SRC}/tools/radiant/radiant/script/ScriptLibraryModule.cpp
      ${UFOAI_SRC}/tools/radiant/radiant/script/ScriptTokenWriter.cpp
      ${UFOAI_SRC}/tools/radiant/radiant/script/ScriptTokeniser.cpp
      ${UFOAI_SRC}/tools/radiant/radiant/select.cpp
      ${UFOAI_SRC}/tools/radiant/radiant/selection/BestPoint.cpp
      ${UFOAI_SRC}/tools/radiant/radiant/selection/Intersection.cpp
      ${UFOAI_SRC}/tools/radiant/radiant/selection/Manipulatables.cpp
      ${UFOAI_SRC}/tools/radiant/radiant/selection/Manipulators.cpp
      ${UFOAI_SRC}/tools/radiant/radiant/selection/Planes.cpp
      ${UFOAI_SRC}/tools/radiant/radiant/selection/RadiantSelectionSystem.cpp
      ${UFOAI_SRC}/tools/radiant/radiant/selection/RadiantWindowObserver.cpp
      ${UFOAI_SRC}/tools/radiant/radiant/selection/SelectionSystemModule.cpp
      ${UFOAI_SRC}/tools/radiant/radiant/selection/SelectionTest.cpp
      ${UFOAI_SRC}/tools/radiant/radiant/selection/TransformationVisitors.cpp
      ${UFOAI_SRC}/tools/radiant/radiant/selection/algorithm/Entity.cpp
      ${UFOAI_SRC}/tools/radiant/radiant/selection/algorithm/General.cpp
      ${UFOAI_SRC}/tools/radiant/radiant/selection/algorithm/Group.cpp
      ${UFOAI_SRC}/tools/radiant/radiant/selection/algorithm/Primitives.cpp
      ${UFOAI_SRC}/tools/radiant/radiant/selection/algorithm/Shader.cpp
      ${UFOAI_SRC}/tools/radiant/radiant/selection/algorithm/Transformation.cpp
      ${UFOAI_SRC}/tools/radiant/radiant/selection/selectionset/SelectionSet.cpp
      ${UFOAI_SRC}/tools/radiant/radiant/selection/selectionset/SelectionSetManager.cpp
      ${UFOAI_SRC}/tools/radiant/radiant/selection/selectionset/SelectionSetToolmenu.cpp
      ${UFOAI_SRC}/tools/radiant/radiant/selection/shaderclipboard/ShaderClipboard.cpp
      ${UFOAI_SRC}/tools/radiant/radiant/selection/shaderclipboard/Texturable.cpp
      ${UFOAI_SRC}/tools/radiant/radiant/server.cpp
      ${UFOAI_SRC}/tools/radiant/radiant/settings/GameDescription.cpp
      ${UFOAI_SRC}/tools/radiant/radiant/settings/GameManager.cpp
      ${UFOAI_SRC}/tools/radiant/radiant/settings/GameManagerModule.cpp
      ${UFOAI_SRC}/tools/radiant/radiant/settings/PreferenceDialog.cpp
      ${UFOAI_SRC}/tools/radiant/radiant/settings/Win32Registry.cpp
      ${UFOAI_SRC}/tools/radiant/radiant/sidebar/JobInfo.cpp
      ${UFOAI_SRC}/tools/radiant/radiant/sidebar/MapInfo.cpp
      ${UFOAI_SRC}/tools/radiant/radiant/sidebar/PrefabSelector.cpp
      ${UFOAI_SRC}/tools/radiant/radiant/sidebar/entityinspector/AddPropertyDialog.cpp
      ${UFOAI_SRC}/tools/radiant/radiant/sidebar/entityinspector/BooleanPropertyEditor.cpp
      ${UFOAI_SRC}/tools/radiant/radiant/sidebar/entityinspector/ColourPropertyEditor.cpp
      ${UFOAI_SRC}/tools/radiant/radiant/sidebar/entityinspector/EntityInspector.cpp
      ${UFOAI_SRC}/tools/radiant/radiant/sidebar/entityinspector/EntityPropertyEditor.cpp
      ${UFOAI_SRC}/tools/radiant/radiant/sidebar/entityinspector/FloatPropertyEditor.cpp
      ${UFOAI_SRC}/tools/radiant/radiant/sidebar/entityinspector/ModelPropertyEditor.cpp
      ${UFOAI_SRC}/tools/radiant/radiant/sidebar/entityinspector/ParticlePropertyEditor.cpp
      ${UFOAI_SRC}/tools/radiant/radiant/sidebar/entityinspector/PropertyEditor.cpp
      ${UFOAI_SRC}/tools/radiant/radiant/sidebar/entityinspector/PropertyEditorFactory.cpp
      ${UFOAI_SRC}/tools/radiant/radiant/sidebar/entityinspector/SkinChooser.cpp
      ${UFOAI_SRC}/tools/radiant/radiant/sidebar/entityinspector/SkinPropertyEditor.cpp
      ${UFOAI_SRC}/tools/radiant/radiant/sidebar/entityinspector/SoundPropertyEditor.cpp
      ${UFOAI_SRC}/tools/radiant/radiant/sidebar/entityinspector/SpawnflagsPropertyEditor.cpp
      ${UFOAI_SRC}/tools/radiant/radiant/sidebar/entityinspector/Vector3PropertyEditor.cpp
      ${UFOAI_SRC}/tools/radiant/radiant/sidebar/entitylist/EntityList.cpp
      ${UFOAI_SRC}/tools/radiant/radiant/sidebar/entitylist/GraphTreeModel.cpp
      ${UFOAI_SRC}/tools/radiant/radiant/sidebar/sidebar.cpp
      ${UFOAI_SRC}/tools/radiant/radiant/sidebar/surfaceinspector/surfaceinspector.cpp
      ${UFOAI_SRC}/tools/radiant/radiant/sidebar/texturebrowser.cpp
      ${UFOAI_SRC}/tools/radiant/radiant/sound/SoundManager.cpp
      ${UFOAI_SRC}/tools/radiant/radiant/sound/SoundManagerModule.cpp
      ${UFOAI_SRC}/tools/radiant/radiant/sound/SoundPlayer.cpp
      ${UFOAI_SRC}/tools/radiant/radiant/stacktrace.cpp
      ${UFOAI_SRC}/tools/radiant/radiant/textool/TexTool.cpp
      ${UFOAI_SRC}/tools/radiant/radiant/textool/TexToolItem.cpp
      ${UFOAI_SRC}/tools/radiant/radiant/textool/item/BrushItem.cpp
      ${UFOAI_SRC}/tools/radiant/radiant/textool/item/FaceItem.cpp
      ${UFOAI_SRC}/tools/radiant/radiant/textool/item/FaceVertexItem.cpp
      ${UFOAI_SRC}/tools/radiant/radiant/textures/Texture.cpp
      ${UFOAI_SRC}/tools/radiant/radiant/textures/TextureManipulator.cpp
      ${UFOAI_SRC}/tools/radiant/radiant/textures/TexturesMap.cpp
      ${UFOAI_SRC}/tools/radiant/radiant/textures/TexturesModule.cpp
      ${UFOAI_SRC}/tools/radiant/radiant/timer.cpp
      ${UFOAI_SRC}/tools/radiant/radiant/ufoscript/UFOScript.cpp
      ${UFOAI_SRC}/tools/radiant/radiant/ufoscript/common/DataBlock.cpp
      ${UFOAI_SRC}/tools/radiant/radiant/ufoscript/common/Parser.cpp
      ${UFOAI_SRC}/tools/radiant/radiant/ufoscript/common/ScriptValues.cpp
      ${UFOAI_SRC}/tools/radiant/radiant/ufoscript/mapdef/MapDef.cpp
      ${UFOAI_SRC}/tools/radiant/radiant/ufoscript/terrain/Terrain.cpp
      ${UFOAI_SRC}/tools/radiant/radiant/ui/about/AboutDialog.cpp
      ${UFOAI_SRC}/tools/radiant/radiant/ui/brush/QuerySidesDialog.cpp
      ${UFOAI_SRC}/tools/radiant/radiant/ui/colourscheme/ColourScheme.cpp
      ${UFOAI_SRC}/tools/radiant/radiant/ui/colourscheme/ColourSchemeEditor.cpp
      ${UFOAI_SRC}/tools/radiant/radiant/ui/colourscheme/ColourSchemeManager.cpp
      ${UFOAI_SRC}/tools/radiant/radiant/ui/commandlist/CommandList.cpp
      ${UFOAI_SRC}/tools/radiant/radiant/ui/commandlist/ShortcutChooser.cpp
      ${UFOAI_SRC}/tools/radiant/radiant/ui/common/MapPreview.cpp
      ${UFOAI_SRC}/tools/radiant/radiant/ui/common/MaterialDefinitionView.cpp
      ${UFOAI_SRC}/tools/radiant/radiant/ui/common/ModelPreview.cpp
      ${UFOAI_SRC}/tools/radiant/radiant/ui/common/RenderableAABB.cpp
      ${UFOAI_SRC}/tools/radiant/radiant/ui/common/ShaderChooser.cpp
      ${UFOAI_SRC}/tools/radiant/radiant/ui/common/ShaderSelector.cpp
      ${UFOAI_SRC}/tools/radiant/radiant/ui/common/SoundChooser.cpp
      ${UFOAI_SRC}/tools/radiant/radiant/ui/common/SoundPreview.cpp
      ${UFOAI_SRC}/tools/radiant/radiant/ui/common/TexturePreviewCombo.cpp
      ${UFOAI_SRC}/tools/radiant/radiant/ui/common/ToolbarCreator.cpp
      ${UFOAI_SRC}/tools/radiant/radiant/ui/common/UFOScriptDefinitionView.cpp
      ${UFOAI_SRC}/tools/radiant/radiant/ui/common/UMPDefinitionView.cpp
      ${UFOAI_SRC}/tools/radiant/radiant/ui/filterdialog/FilterDialog.cpp
      ${UFOAI_SRC}/tools/radiant/radiant/ui/filterdialog/FilterEditor.cpp
      ${UFOAI_SRC}/tools/radiant/radiant/ui/findbrush/findbrush.cpp
      ${UFOAI_SRC}/tools/radiant/radiant/ui/findshader/FindShader.cpp
      ${UFOAI_SRC}/tools/radiant/radiant/ui/lightdialog/LightDialog.cpp
      ${UFOAI_SRC}/tools/radiant/radiant/ui/mainframe/SplitPaneLayout.cpp
      ${UFOAI_SRC}/tools/radiant/radiant/ui/mainframe/mainframe.cpp
      ${UFOAI_SRC}/tools/radiant/radiant/ui/maptools/ErrorCheckDialog.cpp
      ${UFOAI_SRC}/tools/radiant/radiant/ui/materialeditor/MaterialEditor.cpp
      ${UFOAI_SRC}/tools/radiant/radiant/ui/menu/FiltersMenu.cpp
      ${UFOAI_SRC}/tools/radiant/radiant/ui/menu/UMPMenu.cpp
      ${UFOAI_SRC}/tools/radiant/radiant/ui/modelselector/ModelSelector.cpp
      ${UFOAI_SRC}/tools/radiant/radiant/ui/mru/MRU.cpp
      ${UFOAI_SRC}/tools/radiant/radiant/ui/mru/MRUMenuItem.cpp
      ${UFOAI_SRC}/tools/radiant/radiant/ui/ortho/EntityClassChooser.cpp
      ${UFOAI_SRC}/tools/radiant/radiant/ui/ortho/OrthoContextMenu.cpp
      ${UFOAI_SRC}/tools/radiant/radiant/ui/overlay/Overlay.cpp
      ${UFOAI_SRC}/tools/radiant/radiant/ui/particles/ParticleEditor.cpp
      ${UFOAI_SRC}/tools/radiant/radiant/ui/particles/ParticlePreview.cpp
      ${UFOAI_SRC}/tools/radiant/radiant/ui/particles/ParticleSelector.cpp
      ${UFOAI_SRC}/tools/radiant/radiant/ui/scripteditor/UFOScriptEditor.cpp
      ${UFOAI_SRC}/tools/radiant/radiant/ui/splash/Splash.cpp
      ${UFOAI_SRC}/tools/radiant/radiant/ui/textureoverview/TextureOverviewDialog.cpp
      ${UFOAI_SRC}/tools/radiant/radiant/ui/transform/TransformDialog.cpp
      ${UFOAI_SRC}/tools/radiant/radiant/ui/uimanager/MenuItem.cpp
      ${UFOAI_SRC}/tools/radiant/radiant/ui/uimanager/MenuManager.cpp
      ${UFOAI_SRC}/tools/radiant/radiant/ui/uimanager/UIManager.cpp
      ${UFOAI_SRC}/tools/radiant/radiant/ui/umpeditor/UMPEditor.cpp
      ${UFOAI_SRC}/tools/radiant/radiant/ump/UMPAssembly.cpp
      ${UFOAI_SRC}/tools/radiant/radiant/ump/UMPFile.cpp
      ${UFOAI_SRC}/tools/radiant/radiant/ump/UMPModule.cpp
      ${UFOAI_SRC}/tools/radiant/radiant/ump/UMPSystem.cpp
      ${UFOAI_SRC}/tools/radiant/radiant/ump/UMPTile.cpp
      ${UFOAI_SRC}/tools/radiant/radiant/undo/RadiantUndoSystem.cpp
      ${UFOAI_SRC}/tools/radiant/radiant/undo/RadiantUndoSystemModule.cpp
      ${UFOAI_SRC}/tools/radiant/radiant/undo/UndoStateTracker.cpp
      ${UFOAI_SRC}/tools/radiant/radiant/url.cpp
      ${UFOAI_SRC}/tools/radiant/radiant/windowobservers.cpp
      ${UFOAI_SRC}/tools/radiant/radiant/xmlregistry/RegistryTree.cpp
      ${UFOAI_SRC}/tools/radiant/radiant/xmlregistry/XMLRegistry.cpp
      ${UFOAI_SRC}/tools/radiant/radiant/xmlregistry/XMLRegistryModule.cpp
      ${UFOAI_SRC}/tools/radiant/radiant/xyview/GlobalXYWnd.cpp
      ${UFOAI_SRC}/tools/radiant/radiant/xyview/XYWnd.cpp
      ${UFOAI_SRC}/tools/radiant/radiant/xyview/grid/Grid.cpp

      ${UFOAI_SRC}/libs/picomodel/picointernal.c
      ${UFOAI_SRC}/libs/picomodel/picomodel.c
      ${UFOAI_SRC}/libs/picomodel/picomodules.c
      ${UFOAI_SRC}/libs/picomodel/pm_ase.c
      ${UFOAI_SRC}/libs/picomodel/pm_md2.c
      ${UFOAI_SRC}/libs/picomodel/pm_md3.c
      ${UFOAI_SRC}/libs/picomodel/pm_obj.c
   )

   # plateform specific sources
   if(WIN32)
      SET(UFOAI_SOURCEFILES_UFORADIANT ${UFOAI_SOURCEFILES_UFORADIANT}
         ${UFOAI_SRC}/tools/radiant/radiant/radiant.rc
      )
   endif()

   # uforadiant is executable
   add_executable(${UFOAI_PROJECT_UFORADIANT} ${UFOAI_SOURCEFILES_UFORADIANT})

   # For generic no-config case (e.g. with mingw, gcc, ...)
   set_target_properties(${UFOAI_PROJECT_UFORADIANT} PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/radiant)
   # For multi-config builds like msvc
   foreach(OUTPUTCONFIG ${CMAKE_CONFIGURATION_TYPES})
      string(TOUPPER ${OUTPUTCONFIG} OUTPUTCONFIG)
      set_target_properties(${UFOAI_PROJECT_UFORADIANT} PROPERTIES RUNTIME_OUTPUT_DIRECTORY_${OUTPUTCONFIG}  ${CMAKE_BINARY_DIR}/radiant)
   endforeach(OUTPUTCONFIG CMAKE_CONFIGURATION_TYPES)

   target_compile_definitions(${UFOAI_PROJECT_UFORADIANT} PRIVATE -DCOMPILE_MAP)
   target_compile_definitions(${UFOAI_PROJECT_UFORADIANT} PRIVATE -DPATH_UFORADIANTICON_FILE="${CMAKE_SOURCE_DIR}/build/projects/radiant.ico")

   target_include_directories(${UFOAI_PROJECT_UFORADIANT}
      PRIVATE ${UFOAI_SRC}/tools/radiant/include
      PRIVATE ${UFOAI_SRC}/tools/radiant/libs
      PRIVATE ${UFOAI_SRC}/libs/picomodel
      PRIVATE ${SDL_INCLUDE_DIR}
      PRIVATE ${GTK_INCLUDE_DIRS}
      PRIVATE ${OPENAL_INCLUDE_DIR}
      PRIVATE ${VORBIS_INCLUDE_DIR}
      PRIVATE ${OGG_INCLUDE_DIR}
      PRIVATE ${LIBINTL_INCLUDE_DIR}
      PRIVATE ${ZLIB_INCLUDE_DIRS}
   )

   target_link_libraries(${UFOAI_PROJECT_UFORADIANT}
      ${GTK_LIBRARIES}
      ${VORBIS_LIBRARY_FILE}
      ${VORBIS_LIBRARY}
      ${OGG_LIBRARIES}
      ${OPENAL_LIBRARIES}
      ${JPEG_LIBRARIES}
      ${PNG_LIBRARIES}
      ${ZLIB_LIBRARIES}
      ${CMAKE_DL_LIBS}
   )

   if(WIN32)
      target_link_libraries(${UFOAI_PROJECT_UFORADIANT}
         opengl32
         imm32
         ${LIBINTL_LIBRARIES}
         version
         winmm
      )
   elseif(APPLE)
      target_link_libraries(${UFOAI_PROJECT_UFORADIANT}
         ${LIBINTL_LIBRARIES}
      )
   elseif(UNIX)
      target_link_libraries(${UFOAI_PROJECT_UFORADIANT}
         GL
         X11
      )
      if(NOT CMAKE_SYSTEM_NAME MATCHES "OpenBSD")
          target_link_libraries(${UFOAI_PROJECT_UFORADIANT}
             rt
          )
      endif()
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
      ${UFOAI_SRC}/libs/lua/lapi.c
      ${UFOAI_SRC}/libs/lua/lauxlib.c
      ${UFOAI_SRC}/libs/lua/lbaselib.c
      ${UFOAI_SRC}/libs/lua/lcode.c
      ${UFOAI_SRC}/libs/lua/ldblib.c
      ${UFOAI_SRC}/libs/lua/ldebug.c
      ${UFOAI_SRC}/libs/lua/ldo.c
      ${UFOAI_SRC}/libs/lua/ldump.c
      ${UFOAI_SRC}/libs/lua/lfunc.c
      ${UFOAI_SRC}/libs/lua/lgc.c
      ${UFOAI_SRC}/libs/lua/linit.c
      ${UFOAI_SRC}/libs/lua/liolib.c
      ${UFOAI_SRC}/libs/lua/llex.c
      ${UFOAI_SRC}/libs/lua/lmathlib.c
      ${UFOAI_SRC}/libs/lua/lmem.c
      ${UFOAI_SRC}/libs/lua/loadlib.c
      ${UFOAI_SRC}/libs/lua/lobject.c
      ${UFOAI_SRC}/libs/lua/lopcodes.c
      ${UFOAI_SRC}/libs/lua/loslib.c
      ${UFOAI_SRC}/libs/lua/lparser.c
      ${UFOAI_SRC}/libs/lua/lstate.c
      ${UFOAI_SRC}/libs/lua/lstring.c
      ${UFOAI_SRC}/libs/lua/lstrlib.c
      ${UFOAI_SRC}/libs/lua/ltable.c
      ${UFOAI_SRC}/libs/lua/ltablib.c
      ${UFOAI_SRC}/libs/lua/ltm.c
      ${UFOAI_SRC}/libs/lua/lundump.c
      ${UFOAI_SRC}/libs/lua/lvm.c
      ${UFOAI_SRC}/libs/lua/lzio.c
      ${UFOAI_SRC}/libs/lua/print.c
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

   # plateform specific sources
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
      PRIVATE ${UFOAI_SRC}/libs/lua
      PRIVATE ${UFOAI_SRC}/libs/mumble
      PRIVATE ${UFOAI_SRC}/libs/mxml
      PRIVATE ${SDL_INCLUDE_DIR}
      PRIVATE ${SDLMIXER_INCLUDE_DIR}
      PRIVATE ${SDLTTF_INCLUDE_DIR}
      PRIVATE ${CURL_INCLUDE_DIR}
      PRIVATE ${VORBIS_INCLUDE_DIR}
      PRIVATE ${OGG_INCLUDE_DIR}
      PRIVATE ${JPEG_INCLUDE_DIR}
      PRIVATE ${PNG_INCLUDE_DIRS}
      PRIVATE ${LIBINTL_INCLUDE_DIR}
      PRIVATE ${ZLIB_INCLUDE_DIRS}
      PRIVATE ${X11_INCLUDE_DIR}
   )

   if(MINGW)
      target_link_libraries(${UFOAI_PROJECT_UFOTESTALL}
         mingw32
      )
   endif()

   target_link_libraries(${UFOAI_PROJECT_UFOTESTALL}
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
         ${LIBINTL_LIBRARIES}
         version
         winmm
         Ws2_32
      )
   elseif(APPLE)
      target_link_libraries(${UFOAI_PROJECT_UFORADIANT}
         ${LIBINTL_LIBRARIES}
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
