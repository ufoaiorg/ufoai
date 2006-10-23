#
# The name of the project is UFOAI, and hence, then name of the source dir
# is ${UFOAI_SOURCE_DIR} and the binary dir is ${UFOAI_BUILD_DIR}
#
#

#
# TODOs: sdl-config --cflags doesn't get handled satisfactory by cmake standard include (if it matters)


PROJECT(UFOAI C)


#
# Configuration
#

OPTION(BUILD_SDLUFO "Use SDL for cdrom, sound and maybe renderer (selectable)" ON)
OPTION(BUILD_GLX "Build & use X11 GLX driver" ON)
OPTION(BUILD_DEDICATED "Build dedicated server \"ufoded\"" ON)
OPTION(BUILD_ALSA "Include support for ALSA" ON)
OPTION(BUILD_OSS "Include support for OSS" ON)
OPTION(BUILD_ARTS "Include support for arts" ON)
OPTION(BUILD_WITH_SHADER "build with shader support" ON)
OPTION(HAVE_IPV6 "Compile with IPv6 (protocol independent API). Tested on FreeBSD" OFF)
OPTION(HAVE_GETTEXT "Compile with multilanguage-support?" ON)
OPTION(BUILD_WITH_DGA, "dga support compiled in" OFF)
OPTION(BUILD_WITH_VIDMODE "vidmode support" ON)
OPTION(BUILD_WITH_MMX "MMX support" OFF)
OPTION(BUILD_PARANOID "paranoid mode (extra runtime checks) for debug" OFF)
OPTION(BUILD_FOR_PROFILING, "build with -pg option for profiling via gprof" OFF)
OPTION(BUILD_COMPILETOOLS "build ufo2map bsp compile tool" ON)

#
# Recurse into these subdirs
#
SUBDIRS(po)

SET(CMAKE_SHARED_LIBRARY_LINK_C_FLAGS "")

IF(HAVE_IPV6)
	ADD_DEFINITIONS(-DHAVE_IPV6)
	IF(${CMAKE_SYSTEM_NAME} STREQUAL FreeBSD)
		ADD_DEFINITIONS(-DHAVE_SIN6_LEN)
	ENDIF(${CMAKE_SYSTEM_NAME} STREQUAL FreeBSD)
	SET(NET_UDP "net_udp6")
ELSE(HAVE_IPV6)
	SET(NET_UDP "net_udp")
ENDIF(HAVE_IPV6)


#
# Sourcefiles for client
#

SET(UFOSOURCEFILES
	client/cl_actor.c
	client/cl_aircraft.c
	client/cl_basemanagement.c
	client/cl_campaign.c
	client/cl_employee.c
	client/cl_fx.c
	client/cl_input.c
	client/cl_inventory.c
	client/cl_le.c
	client/cl_main.c
	client/cl_map.c
	client/cl_market.c
	client/cl_menu.c
	client/cl_parse.c
	client/cl_particle.c
	client/cl_popup.c
	client/cl_produce.c
	client/cl_radar.c
	client/cl_research.c
	client/cl_scrn.c
	client/cl_sequence.c
	client/cl_shader.c
	client/cl_team.c
	client/cl_ufo.c
	client/cl_ufopedia.c
	client/cl_view.c
	client/console.c
	client/keys.c
	client/snd_mem.c
	client/snd_mix.c
	client/snd_ref.c
	client/x86.c

#	qcommon/cbuf.c
	qcommon/cmd.c
	qcommon/cmodel.c
	qcommon/common.c
	qcommon/crc.c
	qcommon/cvar.c
	qcommon/files.c
	qcommon/md4.c
	qcommon/net_chan.c
	qcommon/scripts.c

	server/sv_ccmds.c
	server/sv_game.c
	server/sv_init.c
	server/sv_main.c
#	server/sv_null.c
	server/sv_send.c
	server/sv_user.c
	server/sv_world.c

	game/q_shared.c

)

SET(GAMESOURCEFILES
	game/q_shared.c
	game/g_ai.c
	game/g_client.c
	game/g_cmds.c
	game/g_main.c
	game/g_spawn.c
	game/g_svcmds.c
	game/g_utils.c
)

SET(REF_GL_SOURCEFULES
	ref_gl/gl_anim.c
	ref_gl/gl_draw.c
	ref_gl/gl_font.c
	ref_gl/gl_image.c
	ref_gl/gl_light.c
	ref_gl/gl_mesh.c
	ref_gl/gl_model.c
	ref_gl/gl_rmain.c
	ref_gl/gl_rmisc.c
	ref_gl/gl_rsurf.c
	ref_gl/gl_warp.c
	ref_gl/gl_particle.c
	ref_gl/gl_arb_shader.c
	ref_gl/gl_shadows.c
	ref_gl/qgl.c

	ports/linux/qgl_linux.c
	game/q_shared.c
	ports/linux/q_shlinux.c
	ports/unix/glob.c # TODO: Different for macos.
)

SET(REF_GLX_SOURCEFILES
	ports/linux/gl_glx.c
)

SET(REF_SDL_SOURCEFILES
	ports/linux/gl_sdl.c
)

SET(UFODED_SOURCEFILES

	qcommon/cmd.c
	qcommon/cmodel.c
	qcommon/common.c
	qcommon/crc.c
	qcommon/cvar.c
	qcommon/files.c
	qcommon/md4.c
	qcommon/net_chan.c
	qcommon/scripts.c

	server/sv_ccmds.c
	server/sv_game.c
	server/sv_init.c
	server/sv_main.c
	server/sv_send.c
	server/sv_user.c
	server/sv_world.c

	game/q_shared.c

	ports/null/cl_null.c
	ports/null/cd_null.c
)

SET(TOOLS_SOURCEFILES
	tools/ufo2map/ufo2map.c
	tools/ufo2map/qrad3.c
	tools/ufo2map/qbsp3.c
	tools/ufo2map/brushbsp.c
	tools/ufo2map/csg.c
	tools/ufo2map/faces.c
	tools/ufo2map/glfile.c
	tools/ufo2map/levels.c
	tools/ufo2map/lightmap.c
	tools/ufo2map/map.c
	tools/ufo2map/nodraw.c
	tools/ufo2map/patches.c
	tools/ufo2map/portals.c
	tools/ufo2map/routing.c
	tools/ufo2map/textures.c
	tools/ufo2map/tree.c
	tools/ufo2map/writebsp.c
	tools/ufo2map/common/bspfile.c
	tools/ufo2map/common/cmdlib.c
	tools/ufo2map/common/l3dslib.c
	tools/ufo2map/common/lbmlib.c
	tools/ufo2map/common/mathlib.c
	tools/ufo2map/common/polylib.c
	tools/ufo2map/common/scriplib.c
	tools/ufo2map/common/threads.c
	tools/ufo2map/common/trace.c
	tools/ufo2map/common/trilib.c
)


EXEC_PROGRAM(uname . ARGS "-m  | sed -e s/i.86/i386/ -e s/sun4u/sparc/ -e s/sparc64/sparc/ -e s/arm.*/arm/ -e s/sa110/arm/ -e s/alpha/axp/ -e 's/Power Macintosh/ppc/'" OUTPUT_VARIABLE ARCH )

EXEC_PROGRAM(svnversion ARGS "." OUTPUT_VARIABLE SVN_OUTPUT)

SET(SOURCEDIRS client game server qcommon)
SET(EXTERNAL_LIBS "")
SET(CMAKE_C_FLAGS_DEBUG "-Wall -pipe -Dstricmp=strcasecmp -DC_ONLY -D_BSD_SOURCE -D_XOPEN_SOURCE -std=c89 -ggdb -O0 -D_FORTIFY_SOURCE=2 -DDEBUG  -fno-inline -DPARANOID")
SET(CMAKE_C_FLAGS_RELEASE "-Wall -pipe -Dstricmp=strcasecmp -DC_ONLY -D_BSD_SOURCE -D_XOPEN_SOURCE -std=c89 -ffast-math -funroll-loops -fomit-frame-pointer -fexpensive-optimizations -DNDEBUG")
SET(CMAKE_C_FLAGS ${CMAKE_C_FLAGS_DEBUG})

ADD_DEFINITIONS("-DSVN='\"${SVN_OUTPUT}\"'")


IF (${CMAKE_SYSTEM_NAME} STREQUAL FreeBSD)
	LIST(APPEND UFOSOURCEFILES
			ports/linux/q_shlinux.c
			ports/linux/vid_so.c
			ports/linux/sys_linux.c
			ports/unix/glob.c
			"ports/unix/${NET_UDP}.c"
	)
	LIST(APPEND UFODED_SOURCEFILES
			ports/linux/q_shlinux.c
			ports/linux/sys_linux.c
			ports/unix/glob.c
			"ports/unix/${NET_UDP}.c"
	)
	LIST(APPEND EXTERNAL_LIBS m vorbisfile vorbis intl)
ENDIF (${CMAKE_SYSTEM_NAME} STREQUAL FreeBSD)

IF (${CMAKE_SYSTEM_NAME} STREQUAL Linux)
	LIST(APPEND UFOSOURCEFILES
		ports/linux/q_shlinux.c
		ports/linux/vid_so.c
		ports/linux/sys_linux.c
		ports/unix/glob.c
		"ports/unix/${NET_UDP}.c"
	)
	LIST(APPEND UFODED_SOURCEFILES
		ports/linux/q_shlinux.c
		ports/linux/sys_linux.c
		ports/unix/glob.c
		"ports/unix/${NET_UDP}.c"
	)
	LIST(APPEND SOURCEDIRS ports/linux ports/unix)
	LIST(APPEND EXTERNAL_LIBS dl m vorbisfile vorbis asound)
	INCLUDE(FindX11)
	MESSAGE("X11_LIBRARIES=${X11_LIBRARIES}")
	IF (BUILD_GLX)
		ADD_LIBRARY(ref_glx SHARED  ${REF_GL_SOURCEFULES} ${REF_GLX_SOURCEFILES})
		SET_TARGET_PROPERTIES(ref_glx PROPERTIES PREFIX "")
		TARGET_LINK_LIBRARIES(ref_glx ${X11_LIBRARIES} ${SDL_LIBRARY} SDL_ttf jpeg)
		IF (X11_FOUND)
			TARGET_LINK_LIBRARIES(ref_glx ${X11_LIBRARIES})
			INCLUDE_DIRECTORIES(${X11_INCLUDE_DIR})
		ELSE (X11_FOUND)
			MESSAGE(FATAL_ERROR "X11 coult not be found")
			SET(BUILD_SDLUFO, OFF)
		ENDIF (X11_FOUND)
		IF (BUILD_WITH_DGA)
			FIND_LIBRARY(LIB_Xxf86dga Xxf86dga)
			TARGET_LINK_LIBRARIES(ref_glx ${LIB_Xxf86dga})
		ENDIF (BUILD_WITH_DGA)
		IF (BUILD_WITH_VIDMODE)
			FIND_LIBRARY(LIB_Xxf86vm Xxf86vm)
			TARGET_LINK_LIBRARIES(ref_glx ${LIB_Xxf86vm})
		ENDIF (BUILD_WITH_VIDMODE)
		INSTALL(TARGETS ref_glx DESTINATION ".")
	ENDIF(BUILD_GLX)
ENDIF(${CMAKE_SYSTEM_NAME} STREQUAL Linux)

# cd handling
IF (BUILD_SDLUFO)
   LIST(APPEND UFOSOURCEFILES ports/unix/cd_sdl.c)
ELSE(BUILD_SDLUFO)
	IF (${CMAKE_SYSTEM_NAME} STREQUAL Linux)
		LIST(APPEND UFOSOURCEFILES client/cd_linux.c)
	ENDIF(${CMAKE_SYSTEM_NAME} STREQUAL Linux)

	IF (${CMAKE_SYSTEM_NAME} STREQUAL FreeBSD)
		LIST(APPEND UFOSOURCEFILES client/cd_linux.c)
	ENDIF(${CMAKE_SYSTEM_NAME} STREQUAL FreeBSD)
#TODO: MacOs/Darwin/OSX
# 	ifeq ($(OSTYPE),Darwin)
# 		QUAKE2_OBJS+=$(BUILDDIR)/client/cd_osx.o
# 	endif
ENDIF(BUILD_SDLUFO)

IF (BUILD_WITH_SHADER)
	ADD_DEFINITIONS(-DSHADERS)
ENDIF (BUILD_WITH_SHADER)

IF (HAVE_GETTEXT)
	ADD_DEFINITIONS(-DHAVE_GETTEXT)
ENDIF (HAVE_GETTEXT)

IF (BUILD_WITH_DGA)
	ADD_DEFINITIONS(-DHAVE_DGA)
ENDIF (BUILD_WITH_DGA)

IF (BUILD_WITH_MMX)
	ADD_DEFINITIONS(-DQ2_MMX_ENABLED -msse)
ENDIF(BUILD_WITH_MMX)

IF (${ARCH} STREQUAL "i386")
	ADD_DEFINITIONS(-DC_ONLY)
ENDIF (${ARCH} STREQUAL "i386")

IF (BUILD_SDLUFO)
	INCLUDE(FindSDL)
	IF (SDL_FOUND)
		ADD_LIBRARY(snd_sdl SHARED ports/linux/snd_sdl.c)
		ADD_LIBRARY(ref_sdl SHARED ${REF_GL_SOURCEFULES} ${REF_SDL_SOURCEFILES})
		LIST(APPEND EXTERNAL_LIBS ${SDL_LIBRARY} jpeg	)
		ADD_DEFINITIONS(-DUSE_SDL)
		INCLUDE_DIRECTORIES(${SDL_INCLUDE_DIR})
		SET_TARGET_PROPERTIES(ref_sdl snd_sdl PROPERTIES PREFIX "" )
		INSTALL(TARGETS snd_sdl ref_sdl DESTINATION ".")
	ELSE(SDL_FOUND)
		MESSAGE("SDL could not be found, but BUILD_SDLUFO was specified. Turning off BUILD_SDLUFO")
		SET(BUILD_SDLUFO, OFF)
	ENDIF (SDL_FOUND)
ENDIF (BUILD_SDLUFO)

IF (BUILD_ALSA)
	LIST(APPEND EXTERNAL_LIBS asound)
	ADD_LIBRARY(snd_alsa SHARED ports/linux/snd_alsa.c)
	SET_TARGET_PROPERTIES(snd_alsa PROPERTIES PREFIX "" )
    INSTALL(TARGETS snd_alsa DESTINATION ".")
ENDIF (BUILD_ALSA)

IF (BUILD_OSS)
	ADD_LIBRARY(snd_oss SHARED ports/linux/snd_oss.c)
	SET_TARGET_PROPERTIES(snd_oss PROPERTIES PREFIX "" )
	INSTALL(TARGETS snd_oss DESTINATION ".")
ENDIF (BUILD_OSS)

IF (BUILD_ARTS)
	LIST(APPEND EXTERNAL_LIBS artsc)
	ADD_LIBRARY(snd_arts SHARED ports/linux/snd_arts.c)
	SET_TARGET_PROPERTIES(snd_arts PROPERTIES PREFIX "" )
	INSTALL(TARGETS snd_arts DESTINATION ".")
ENDIF (BUILD_ARTS)

IF (BUILD_WITH_VIDMODE)
	ADD_DEFINITIONS(-DHAVE_VIDMODE)
ENDIF (BUILD_WITH_VIDMODE)

IF (BUILD_PARANOID)
	ADD_DEFINITIONS(-DPARANOID)
ENDIF(BUILD_PARANOID)

IF (BUILD_FOR_PROFILING)
	ADD_DEFINITIONS(-pg -DPROFILING -fprofile-arcs -ftest-coverage)
ENDIF(BUILD_FOR_PROFILING)

SET(EXTERNAL_LIBS_FOUND "")
FOREACH(LIB ${EXTERNAL_LIBS})
	FIND_LIBRARY(LIB_${LIB} ${LIB})
	LIST(APPEND EXTERNAL_LIBS_FOUND ${LIB})
ENDFOREACH(LIB)

INCLUDE_DIRECTORIES(${SOURCEDIRS})

ADD_EXECUTABLE(ufo ${UFOSOURCEFILES})
INSTALL(TARGETS ufo DESTINATION ".")

TARGET_LINK_LIBRARIES(ufo ${EXTERNAL_LIBS})


ADD_LIBRARY(game SHARED ${GAMESOURCEFILES})
SET_TARGET_PROPERTIES(game PROPERTIES PREFIX "")
TARGET_LINK_LIBRARIES(game ${EXTERNAL_LIBS})
INSTALL(TARGETS game DESTINATION base/)

IF (BUILD_DEDICATED)
	ADD_EXECUTABLE(ufoded  ${UFODED_SOURCEFILES})
	INSTALL(TARGETS ufoded DESTINATION ".")
	TARGET_LINK_LIBRARIES(ufoded ${EXTERNAL_LIBS_FOUND})
	SET_TARGET_PROPERTIES(ufoded PROPERTIES  COMPILE_FLAGS -DDEDICATED_ONLY)
ENDIF (BUILD_DEDICATED)

IF (BUILD_COMPILETOOLS)
	ADD_EXECUTABLE(ufo2map ${TOOLS_SOURCEFILES})
	TARGET_LINK_LIBRARIES(ufo2map m)
	INSTALL(TARGETS ufo2map DESTINATION ".")
ENDIF(BUILD_COMPILETOOLS)

