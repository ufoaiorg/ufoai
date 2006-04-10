#
# Quake2 Makefile for Linux 2.0
#
# Nov '97 by Zoid <zoid@idsoftware.com>
#
# ELF only
#

# start of configurable options

# Here are your build options:
# (Note: not all options are available for all platforms).
# ufo (uses OSS for sound, cdrom ioctls for cd audio) is automatically built.
# game.so is automatically built.
BUILD_SDLUFO=YES	# use SDL for cdrom, sound and maybe renderer (selectable)
BUILD_GLX=YES		# X11 GLX driver. Works somewhat ok.
BUILD_DEDICATED=YES	# build a dedicated ufo server
BUILD_ALSA=YES		# build with alsa-support
BUILD_WITH_SHADER=NO	# build with shader support
HAVE_IPV6=NO		# Compile with IPv6 (protocol independent API). Tested on FreeBSD
HAVE_GETTEXT=YES	# Compile with multilanguage-support?
BUILD_JPEG=NO		# jpeg-6 compile in or link in?
BUILD_FREETYPE=NO	# build with freetype support?
BUILD_WITH_DGA=NO	# dga support compiled in
BUILD_WITH_VIDMODE=YES	# vidmode support
BUILD_WITH_JOYSTICK=NO	# joystick support

# (hopefully) end of configurable options

# Check OS type.
OSTYPE := $(shell uname -s)

ifneq ($(OSTYPE),Linux)
	ifneq ($(OSTYPE),FreeBSD)
		ifneq ($(OSTYPE),Darwin)
			ifeq ($(OSTYPE),SunOS)
				$(error OS $(OSTYPE) detected, use "Makefile.Solaris" instead.)
			else
				$(error OS $(OSTYPE) is currently not supported)
			endif
		endif
	endif
endif


# this nice line comes from the linux kernel makefile
ARCH := $(shell uname -m | sed -e s/i.86/i386/ -e s/sun4u/sparc/ -e s/sparc64/sparc/ -e s/arm.*/arm/ -e s/sa110/arm/ -e s/alpha/axp/)

ifneq ($(ARCH),x86_64)
	ifneq ($(ARCH),i386)
		ifneq ($(ARCH),axp)
			ifneq ($(OSTYPE),ppc)
				ifneq ($(OSTYPE),Darwin)
					ifneq ($(ARCH),sparc)
						$(error arch $(ARCH) is currently not supported)
					endif
				endif
			endif
		endif
	endif
endif

CC=gcc

VERSION=$(shell grep UFO_VERSION $(MOUNT_DIR)/qcommon/qcommon.h | sed -e 's/.*UFO_VERSION\s*\(.*\)/\1/')

MOUNT_DIR=..

BUILD_DEBUG_DIR=debug$(ARCH)
BUILD_RELEASE_DIR=release$(ARCH)
CLIENT_DIR=$(MOUNT_DIR)/client
SERVER_DIR=$(MOUNT_DIR)/server
REF_GL_DIR=$(MOUNT_DIR)/ref_gl
COMMON_DIR=$(MOUNT_DIR)/qcommon
LINUX_DIR=$(MOUNT_DIR)/linux
GAME_DIR=$(MOUNT_DIR)/game
NULL_DIR=$(MOUNT_DIR)/null
JPDIR=$(MOUNT_DIR)/jpeg-6

BASE_CFLAGS=-Wall -pipe -Dstricmp=strcasecmp

ifneq ($(ARCH),i386)
	BASE_CFLAGS+=-DC_ONLY
endif

ifeq ($(OSTYPE),FreeBSD)
	LDFLAGS=-lm -lvorbisfile -lvorbis -lasound
endif

ifeq ($(OSTYPE),Linux)
	LDFLAGS=-ldl -lm -lvorbisfile -lvorbis -lasound
endif

ifeq ($(OSTYPE),Darwin)
	LDFLAGS=-ldl -lm -lvorbisfile -lvorbis -lasound -llibintl
endif

ifeq ($(strip $(BUILD_SDLUFO)),YES)
	LDFLAGS+=$(shell sdl-config --libs)
	BASE_CFLAGS+=$(shell sdl-config --cflags) -DUSE_SDL
endif

ifeq ($(strip $(BUILD_ALSA)),YES)
	BASE_CFLAGS+=-DUSE_ALSA
	LDFLAGS+=-lasound
endif

ifeq ($(strip $(BUILD_WITH_JOYSTICK)),YES)
	BASE_CFLAGS+=-DJoystick
endif

ifeq ($(strip $(BUILD_WITH_DGA)),YES)
	BASE_CFLAGS+=-DHAVE_DGA
endif

ifeq ($(strip $(BUILD_WITH_VIDMODE)),YES)
	BASE_CFLAGS+=-DHAVE_VIDMODE
endif

ifeq ($(strip $(HAVE_GETTEXT)),YES)
	BASE_CFLAGS+=-DHAVE_GETTEXT
endif

ifeq ($(strip $(BUILD_WITH_SHADER)),YES)
	BASE_CFLAGS+=-DSHADERS
endif

ifeq ($(strip $(HAVE_IPV6)),YES)
	BASE_CFLAGS+=-DHAVE_IPV6
	ifeq ($(OSTYPE),FreeBSD)
		BASE_CFLAGS+=-DHAVE_SIN6_LEN
	endif
	NET_UDP=net_udp6
else
	NET_UDP=net_udp
endif

ifeq ($(ARCH),axp)
	RELEASE_CFLAGS=$(BASE_CFLAGS) -ffast-math -funroll-loops -fomit-frame-pointer -fexpensive-optimizations
endif

ifeq ($(ARCH),ppc)
	RELEASE_CFLAGS=$(BASE_CFLAGS) -O2 -ffast-math -funroll-loops -fomit-frame-pointer -fexpensive-optimizations
endif

ifeq ($(ARCH),sparc)
	RELEASE_CFLAGS=$(BASE_CFLAGS) -ffast-math -funroll-loops -fomit-frame-pointer -fexpensive-optimizations
endif

ifeq ($(ARCH),i386)
	RELEASE_CFLAGS=$(BASE_CFLAGS) -O2 -ffast-math -funroll-loops -falign-loops=2 -falign-jumps=2 -falign-functions=2 -fno-strict-aliasing
endif

ifeq ($(ARCH),x86_64)
	_LIB := 64
	RELEASE_CFLAGS=$(BASE_CFLAGS) -O2 -ffast-math -funroll-loops -fomit-frame-pointer -fexpensive-optimizations -fno-strict-aliasing
endif

GLXCFLAGS=-I/usr/X11R6/include
GLXLDFLAGS=-L/usr/X11R6/lib$(_LIB) -lX11 -lXext -lXxf86dga -lXxf86vm
SDLLDFLAGS=$(shell sdl-config --libs)

SHLIBEXT=so

SHLIBCFLAGS=-fPIC
SHLIBLDFLAGS=-shared

ifeq ($(strip $(BUILD_JPEG)),YES)
	BASE_CFLAGS+=-DBUILD_JPEG
else
	GLXLDFLAGS+=-ljpeg
	SDLLDFLAGS+=-ljpeg
endif

ifeq ($(strip $(BUILD_FREETYPE)),YES)
	LDFLAGS+=$(shell freetype-config --libs)
	BASE_CFLAGS+=$(shell freetype-config --cflags) -DBUILD_FREETYPE
endif

DEBUG_CFLAGS=$(BASE_CFLAGS) -g -D_FORTIFY_SOURCE=2

DO_CC=$(CC) $(CFLAGS) -o $@ -c $<
DO_DEBUG_CC=$(CC) $(DEBUG_CFLAGS) -o $@ -c $<
DO_DED_CC=$(CC) $(CFLAGS) -DDEDICATED_ONLY -o $@ -c $<
DO_DED_DEBUG_CC=$(CC) $(DEBUG_CFLAGS) -DDEDICATED_ONLY -o $@ -c $<
DO_O_CC=$(CC) $(CFLAGS) -O -o $@ -c $<
DO_SHLIB_CC=$(CC) $(CFLAGS) $(SHLIBCFLAGS) -o $@ -c $<
DO_GL_SHLIB_CC=$(CC) $(CFLAGS) $(SHLIBCFLAGS) $(GLCFLAGS) -o $@ -c $<
DO_AS=$(CC) $(CFLAGS) -DELF -x assembler-with-cpp -o $@ -c $<
DO_SHLIB_AS=$(CC) $(CFLAGS) $(SHLIBCFLAGS) -DELF -x assembler-with-cpp -o $@ -c $<

#############################################################################
# gettext stuff
#############################################################################
WORKSPACE = $(shell pwd)
POFILES = $(wildcard $(WORKSPACE)/../po/*.po)
#############################################################################

#############################################################################
# SETUP AND BUILD
#############################################################################

.PHONY : targets build_debug build_release clean clean-debug clean-release clean2

TARGETS=$(BUILDDIR)/ufo $(BUILDDIR)/game.$(SHLIBEXT)

ifeq ($(strip $(BUILD_DEDICATED)),YES)
	TARGETS += $(BUILDDIR)/ufoded
endif

ifeq ($(ARCH),axp)
	ifeq ($(strip $(BUILD_GLX)),YES)
		$(warning Warning: support not supported for $(ARCH))
	endif
endif # ARCH axp

ifeq ($(ARCH),ppc)
	ifeq ($(strip $(BUILD_GLX)),YES)
		TARGETS += $(BUILDDIR)/ref_glx.$(SHLIBEXT)
	endif
endif # ARCH ppc

ifeq ($(ARCH),sparc)
	ifeq ($(strip $(BUILD_GLX)),YES)
		$(warning Warning: GLX support not supported for $(ARCH))
	endif
endif # ARCH sparc

ifeq ($(ARCH),x86_64)
	ifeq ($(strip $(BUILD_GLX)),YES)
		TARGETS += $(BUILDDIR)/ref_glx.$(SHLIBEXT)
	endif
	ifeq ($(strip $(BUILD_SDLUFO)),YES)
		TARGETS += $(BUILDDIR)/ref_sdl.$(SHLIBEXT)
	endif
endif # ARCH x86_64

ifeq ($(ARCH),i386)
	ifeq ($(strip $(BUILD_GLX)),YES)
		TARGETS += $(BUILDDIR)/ref_glx.$(SHLIBEXT)
	endif
	ifeq ($(strip $(BUILD_SDLUFO)),YES)
		TARGETS += $(BUILDDIR)/ref_sdl.$(SHLIBEXT)
	endif
endif # ARCH i386

debug: build_debug
all: build_debug build_release lang
release: build_release

build_debug:
	@-mkdir -p $(BUILD_DEBUG_DIR) \
		$(BUILD_DEBUG_DIR)/client \
		$(BUILD_DEBUG_DIR)/ded \
		$(BUILD_DEBUG_DIR)/ref_gl \
		$(BUILD_DEBUG_DIR)/game
	$(MAKE) targets BUILDDIR=$(BUILD_DEBUG_DIR) CFLAGS="$(DEBUG_CFLAGS) -DLINUX_VERSION='\"$(VERSION) Debug\"'"

build_release:
	@-mkdir -p $(BUILD_RELEASE_DIR) \
		$(BUILD_RELEASE_DIR)/client \
		$(BUILD_RELEASE_DIR)/ded \
		$(BUILD_RELEASE_DIR)/ref_gl \
		$(BUILD_RELEASE_DIR)/game
	$(MAKE) targets BUILDDIR=$(BUILD_RELEASE_DIR) CFLAGS="$(RELEASE_CFLAGS) -DLINUX_VERSION='\"$(VERSION)\"'"

targets: $(TARGETS)

#############################################################################
# CLIENT/SERVER
#############################################################################

QUAKE2_OBJS = \
	$(BUILDDIR)/client/cl_actor.o \
	$(BUILDDIR)/client/cl_basemanagement.o \
	$(BUILDDIR)/client/cl_research.o \
	$(BUILDDIR)/client/cl_campaign.o \
	$(BUILDDIR)/client/cl_cin.o \
	$(BUILDDIR)/client/cl_fx.o \
	$(BUILDDIR)/client/cl_input.o \
	$(BUILDDIR)/client/cl_le.o \
	$(BUILDDIR)/client/cl_main.o \
	$(BUILDDIR)/client/cl_menu.o \
	$(BUILDDIR)/client/cl_parse.o \
	$(BUILDDIR)/client/cl_particle.o \
	$(BUILDDIR)/client/cl_shader.o \
	$(BUILDDIR)/client/cl_scrn.o \
	$(BUILDDIR)/client/cl_sequence.o \
	$(BUILDDIR)/client/cl_team.o \
	$(BUILDDIR)/client/cl_ufopedia.o \
	$(BUILDDIR)/client/cl_view.o \
	$(BUILDDIR)/client/console.o \
	$(BUILDDIR)/client/keys.o \
	$(BUILDDIR)/client/snd_dma.o \
	$(BUILDDIR)/client/snd_mem.o \
	$(BUILDDIR)/client/snd_mix.o \
	\
	$(BUILDDIR)/client/cmd.o \
	$(BUILDDIR)/client/cmodel.o \
	$(BUILDDIR)/client/common.o \
	$(BUILDDIR)/client/crc.o \
	$(BUILDDIR)/client/cvar.o \
	$(BUILDDIR)/client/files.o \
	$(BUILDDIR)/client/md4.o \
	$(BUILDDIR)/client/net_chan.o \
	$(BUILDDIR)/client/scripts.o \
	\
	$(BUILDDIR)/client/sv_ccmds.o \
	$(BUILDDIR)/client/sv_game.o \
	$(BUILDDIR)/client/sv_init.o \
	$(BUILDDIR)/client/sv_main.o \
	$(BUILDDIR)/client/sv_send.o \
	$(BUILDDIR)/client/sv_user.o \
	$(BUILDDIR)/client/sv_world.o \
	\
	$(BUILDDIR)/client/q_shlinux.o \
	$(BUILDDIR)/client/vid_so.o \
	$(BUILDDIR)/client/sys_linux.o \
	$(BUILDDIR)/client/glob.o \
	$(BUILDDIR)/client/net_udp.o \
	$(BUILDDIR)/client/snd_linux.o \
	$(BUILDDIR)/client/snd_oss.o \
	\
	$(BUILDDIR)/client/q_shared.o

ifeq ($(strip $(BUILD_SDLUFO)),YES)
	QUAKE2_OBJS+=$(BUILDDIR)/client/snd_sdl.o $(BUILDDIR)/client/cd_sdl.o
else
	QUAKE2_OBJS+=$(BUILDDIR)/client/cd_linux.o
endif

ifeq ($(strip $(BUILD_ALSA)),YES)
	QUAKE2_OBJS+=$(BUILDDIR)/client/snd_alsa.o
endif

$(BUILDDIR)/ufo : $(QUAKE2_OBJS) $(QUAKE2_LNX_OBJS)
	$(CC) $(CFLAGS) -o $@ $(QUAKE2_OBJS) $(QUAKE2_LNX_OBJS) $(LDFLAGS)

$(BUILDDIR)/client/cl_actor.o :    $(CLIENT_DIR)/cl_actor.c
	$(DO_CC)

$(BUILDDIR)/client/cl_campaign.o : $(CLIENT_DIR)/cl_campaign.c
	$(DO_CC)

$(BUILDDIR)/client/cl_basemanagement.o : $(CLIENT_DIR)/cl_basemanagement.c
	$(DO_CC)

$(BUILDDIR)/client/cl_research.o : $(CLIENT_DIR)/cl_research.c
	$(DO_CC)

$(BUILDDIR)/client/cl_cin.o :     $(CLIENT_DIR)/cl_cin.c
	$(DO_CC)

$(BUILDDIR)/client/cl_fx.o :      $(CLIENT_DIR)/cl_fx.c
	$(DO_CC)

$(BUILDDIR)/client/cl_input.o :   $(CLIENT_DIR)/cl_input.c
	$(DO_CC)

$(BUILDDIR)/client/cl_le.o :     $(CLIENT_DIR)/cl_le.c
	$(DO_CC)

$(BUILDDIR)/client/cl_main.o :    $(CLIENT_DIR)/cl_main.c
	$(DO_CC)

$(BUILDDIR)/client/cl_menu.o :    $(CLIENT_DIR)/cl_menu.c
	$(DO_CC)

$(BUILDDIR)/client/cl_parse.o :   $(CLIENT_DIR)/cl_parse.c
	$(DO_CC)

$(BUILDDIR)/client/cl_particle.o : $(CLIENT_DIR)/cl_particle.c
	$(DO_CC)

$(BUILDDIR)/client/cl_shader.o : $(CLIENT_DIR)/cl_shader.c
	$(DO_CC)

$(BUILDDIR)/client/cl_scrn.o :     $(CLIENT_DIR)/cl_scrn.c
	$(DO_CC)

$(BUILDDIR)/client/cl_sequence.o : $(CLIENT_DIR)/cl_sequence.c
	$(DO_CC)

$(BUILDDIR)/client/cl_team.o :     $(CLIENT_DIR)/cl_team.c
	$(DO_CC)

$(BUILDDIR)/client/cl_ufopedia.o : $(CLIENT_DIR)/cl_ufopedia.c
	$(DO_CC)

$(BUILDDIR)/client/cl_view.o :     $(CLIENT_DIR)/cl_view.c
	$(DO_CC)

$(BUILDDIR)/client/console.o :     $(CLIENT_DIR)/console.c
	$(DO_CC)

$(BUILDDIR)/client/keys.o :        $(CLIENT_DIR)/keys.c
	$(DO_CC)

$(BUILDDIR)/client/snd_dma.o :     $(CLIENT_DIR)/snd_dma.c
	$(DO_CC)

$(BUILDDIR)/client/snd_mem.o :     $(CLIENT_DIR)/snd_mem.c
	$(DO_CC)

$(BUILDDIR)/client/snd_mix.o :     $(CLIENT_DIR)/snd_mix.c
	$(DO_CC)

$(BUILDDIR)/client/snd_alsa.o :     $(LINUX_DIR)/snd_alsa.c
	$(DO_CC)

$(BUILDDIR)/client/cmd.o :         $(COMMON_DIR)/cmd.c
	$(DO_CC)

$(BUILDDIR)/client/cmodel.o :      $(COMMON_DIR)/cmodel.c
	$(DO_CC)

$(BUILDDIR)/client/common.o :      $(COMMON_DIR)/common.c
	$(DO_CC)

$(BUILDDIR)/client/crc.o :         $(COMMON_DIR)/crc.c
	$(DO_CC)

$(BUILDDIR)/client/cvar.o :        $(COMMON_DIR)/cvar.c
	$(DO_CC)

$(BUILDDIR)/client/files.o :       $(COMMON_DIR)/files.c
	$(DO_CC)

$(BUILDDIR)/client/md4.o :         $(COMMON_DIR)/md4.c
	$(DO_CC)

$(BUILDDIR)/client/net_chan.o :    $(COMMON_DIR)/net_chan.c
	$(DO_CC)

$(BUILDDIR)/client/scripts.o :     $(COMMON_DIR)/scripts.c
	$(DO_CC)

$(BUILDDIR)/client/sv_ccmds.o :    $(SERVER_DIR)/sv_ccmds.c
	$(DO_CC)

$(BUILDDIR)/client/sv_game.o :     $(SERVER_DIR)/sv_game.c
	$(DO_CC)

$(BUILDDIR)/client/sv_init.o :     $(SERVER_DIR)/sv_init.c
	$(DO_CC)

$(BUILDDIR)/client/sv_main.o :     $(SERVER_DIR)/sv_main.c
	$(DO_CC)

$(BUILDDIR)/client/sv_send.o :     $(SERVER_DIR)/sv_send.c
	$(DO_CC)

$(BUILDDIR)/client/sv_user.o :     $(SERVER_DIR)/sv_user.c
	$(DO_CC)

$(BUILDDIR)/client/sv_world.o :    $(SERVER_DIR)/sv_world.c
	$(DO_CC)

$(BUILDDIR)/client/cd_linux.o :    $(LINUX_DIR)/cd_linux.c
	$(DO_CC)

$(BUILDDIR)/client/q_shlinux.o :   $(LINUX_DIR)/q_shlinux.c
	$(DO_O_CC)

$(BUILDDIR)/client/vid_so.o :      $(LINUX_DIR)/vid_so.c
	$(DO_CC)

$(BUILDDIR)/client/snd_linux.o :   $(LINUX_DIR)/snd_linux.c
	$(DO_CC)

$(BUILDDIR)/client/snd_oss.o :   $(LINUX_DIR)/snd_oss.c
	$(DO_CC)

#$(BUILDDIR)/client/snd_mixa.o :    $(LINUX_DIR)/snd_mixa.s
#	$(DO_AS)

$(BUILDDIR)/client/sys_linux.o :   $(LINUX_DIR)/sys_linux.c
	$(DO_CC)

$(BUILDDIR)/client/glob.o :        $(LINUX_DIR)/glob.c
	$(DO_CC)

$(BUILDDIR)/client/q_shared.o :    $(GAME_DIR)/q_shared.c
	$(DO_CC)

$(BUILDDIR)/client/net_udp.o :    $(LINUX_DIR)/net_udp.c
	$(DO_CC)

$(BUILDDIR)/client/net_udp6.o :    $(LINUX_DIR)/net_udp6.c
	$(DO_CC)

$(BUILDDIR)/client/cd_sdl.o :     $(LINUX_DIR)/cd_sdl.c
	$(DO_CC)

$(BUILDDIR)/client/snd_sdl.o :     $(LINUX_DIR)/snd_sdl.c
	$(DO_CC)

#############################################################################
# DEDICATED SERVER
#############################################################################

Q2DED_OBJS = \
	\
	$(BUILDDIR)/ded/cmd.o \
	$(BUILDDIR)/ded/cmodel.o \
	$(BUILDDIR)/ded/common.o \
	$(BUILDDIR)/ded/crc.o \
	$(BUILDDIR)/ded/cvar.o \
	$(BUILDDIR)/ded/files.o \
	$(BUILDDIR)/ded/md4.o \
	$(BUILDDIR)/ded/net_chan.o \
	$(BUILDDIR)/ded/scripts.o \
	\
	$(BUILDDIR)/ded/sv_ccmds.o \
	$(BUILDDIR)/ded/sv_game.o \
	$(BUILDDIR)/ded/sv_init.o \
	$(BUILDDIR)/ded/sv_main.o \
	$(BUILDDIR)/ded/sv_send.o \
	$(BUILDDIR)/ded/sv_user.o \
	$(BUILDDIR)/ded/sv_world.o \
	\
	$(BUILDDIR)/ded/q_shlinux.o \
	$(BUILDDIR)/ded/sys_linux.o \
	$(BUILDDIR)/ded/glob.o \
	$(BUILDDIR)/ded/net_udp.o \
	\
	$(BUILDDIR)/ded/q_shared.o \
	\
	$(BUILDDIR)/ded/cl_null.o \
	$(BUILDDIR)/ded/cd_null.o

$(BUILDDIR)/ufoded : $(Q2DED_OBJS)
	$(CC) $(CFLAGS) -o $@ $(Q2DED_OBJS) $(LDFLAGS)

$(BUILDDIR)/ded/cmd.o :        $(COMMON_DIR)/cmd.c
	$(DO_DED_CC)

$(BUILDDIR)/ded/cmodel.o :     $(COMMON_DIR)/cmodel.c
	$(DO_DED_CC)

$(BUILDDIR)/ded/common.o :     $(COMMON_DIR)/common.c
	$(DO_DED_CC)

$(BUILDDIR)/ded/crc.o :	 $(COMMON_DIR)/crc.c
	$(DO_DED_CC)

$(BUILDDIR)/ded/cvar.o :	$(COMMON_DIR)/cvar.c
	$(DO_DED_CC)

$(BUILDDIR)/ded/files.o :      $(COMMON_DIR)/files.c
	$(DO_DED_CC)

$(BUILDDIR)/ded/md4.o :	 $(COMMON_DIR)/md4.c
	$(DO_DED_CC)

$(BUILDDIR)/ded/net_chan.o :   $(COMMON_DIR)/net_chan.c
	$(DO_DED_CC)

$(BUILDDIR)/ded/scripts.o :    $(COMMON_DIR)/scripts.c
	$(DO_DED_CC)

$(BUILDDIR)/ded/q_shared.o :   $(GAME_DIR)/q_shared.c
	$(DO_DED_CC)

$(BUILDDIR)/ded/pmove.o :      $(COMMON_DIR)/pmove.c
	$(DO_DED_CC)

$(BUILDDIR)/ded/sv_ccmds.o :   $(SERVER_DIR)/sv_ccmds.c
	$(DO_DED_CC)

$(BUILDDIR)/ded/sv_ents.o :    $(SERVER_DIR)/sv_ents.c
	$(DO_DED_CC)

$(BUILDDIR)/ded/sv_game.o :    $(SERVER_DIR)/sv_game.c
	$(DO_DED_CC)

$(BUILDDIR)/ded/sv_init.o :    $(SERVER_DIR)/sv_init.c
	$(DO_DED_CC)

$(BUILDDIR)/ded/sv_main.o :    $(SERVER_DIR)/sv_main.c
	$(DO_DED_CC)

$(BUILDDIR)/ded/sv_send.o :    $(SERVER_DIR)/sv_send.c
	$(DO_DED_CC)

$(BUILDDIR)/ded/sv_user.o :    $(SERVER_DIR)/sv_user.c
	$(DO_DED_CC)

$(BUILDDIR)/ded/sv_world.o :   $(SERVER_DIR)/sv_world.c
	$(DO_DED_CC)

$(BUILDDIR)/ded/q_shlinux.o :  $(LINUX_DIR)/q_shlinux.c
	$(DO_DED_CC)

$(BUILDDIR)/ded/sys_linux.o :  $(LINUX_DIR)/sys_linux.c
	$(DO_DED_CC)

$(BUILDDIR)/ded/glob.o :	$(LINUX_DIR)/glob.c
	$(DO_DED_CC)

$(BUILDDIR)/ded/net_udp.o :    $(LINUX_DIR)/net_udp.c
	$(DO_DED_CC)

$(BUILDDIR)/ded/cd_null.o :    $(NULL_DIR)/cd_null.c
	$(DO_DED_CC)

$(BUILDDIR)/ded/cl_null.o :    $(NULL_DIR)/cl_null.c
	$(DO_DED_CC)


#############################################################################
# GAME
#############################################################################

GAME_OBJS = \
	$(BUILDDIR)/game/q_shared.o \
	$(BUILDDIR)/game/g_ai.o \
	$(BUILDDIR)/game/g_client.o \
	$(BUILDDIR)/game/g_cmds.o \
	$(BUILDDIR)/game/g_main.o \
	$(BUILDDIR)/game/g_spawn.o \
	$(BUILDDIR)/game/g_svcmds.o \
	$(BUILDDIR)/game/g_utils.o

$(BUILDDIR)/game.$(SHLIBEXT) : $(GAME_OBJS)
	$(CC) $(CFLAGS) $(SHLIBLDFLAGS) -o $@ $(GAME_OBJS)

$(BUILDDIR)/game/q_shared.o :    $(GAME_DIR)/q_shared.c
	$(DO_SHLIB_CC)

$(BUILDDIR)/game/g_ai.o :        $(GAME_DIR)/g_ai.c
	$(DO_SHLIB_CC)

$(BUILDDIR)/game/g_client.o :    $(GAME_DIR)/g_client.c
	$(DO_SHLIB_CC)

$(BUILDDIR)/game/g_cmds.o :      $(GAME_DIR)/g_cmds.c
	$(DO_SHLIB_CC)

$(BUILDDIR)/game/g_main.o :      $(GAME_DIR)/g_main.c
	$(DO_SHLIB_CC)

$(BUILDDIR)/game/g_spawn.o :     $(GAME_DIR)/g_spawn.c
	$(DO_SHLIB_CC)

$(BUILDDIR)/game/g_svcmds.o :    $(GAME_DIR)/g_svcmds.c
	$(DO_SHLIB_CC)

$(BUILDDIR)/game/g_utils.o :     $(GAME_DIR)/g_utils.c
	$(DO_SHLIB_CC)

#############################################################################
# REF_GL
#############################################################################

REF_GL_OBJS = \
	$(BUILDDIR)/ref_gl/gl_anim.o \
	$(BUILDDIR)/ref_gl/gl_draw.o \
	$(BUILDDIR)/ref_gl/gl_image.o \
	$(BUILDDIR)/ref_gl/gl_light.o \
	$(BUILDDIR)/ref_gl/gl_mesh.o \
	$(BUILDDIR)/ref_gl/gl_model.o \
	$(BUILDDIR)/ref_gl/gl_rmain.o \
	$(BUILDDIR)/ref_gl/gl_rmisc.o \
	$(BUILDDIR)/ref_gl/gl_rsurf.o \
	$(BUILDDIR)/ref_gl/gl_warp.o \
	$(BUILDDIR)/ref_gl/gl_particle.o \
	$(BUILDDIR)/ref_gl/gl_arb_shader.o \
	$(BUILDDIR)/ref_gl/gl_shadows.o \
	\
	$(BUILDDIR)/ref_gl/qgl_linux.o \
	$(BUILDDIR)/ref_gl/q_shared.o \
	$(BUILDDIR)/ref_gl/q_shlinux.o \
	$(BUILDDIR)/ref_gl/glob.o

ifeq ($(strip $(BUILD_JPEG)),YES)
REF_GL_OBJS += \
	$(BUILDDIR)/ref_gl/jcapimin.o \
	$(BUILDDIR)/ref_gl/jchuff.o   \
	$(BUILDDIR)/ref_gl/jcinit.o \
	$(BUILDDIR)/ref_gl/jccoefct.o  \
	$(BUILDDIR)/ref_gl/jccolor.o \
	$(BUILDDIR)/ref_gl/jfdctflt.o \
	$(BUILDDIR)/ref_gl/jcdctmgr.o \
	$(BUILDDIR)/ref_gl/jcphuff.o \
	$(BUILDDIR)/ref_gl/jcmainct.o \
	$(BUILDDIR)/ref_gl/jcmarker.o \
	$(BUILDDIR)/ref_gl/jcmaster.o \
	$(BUILDDIR)/ref_gl/jcomapi.o \
	$(BUILDDIR)/ref_gl/jcparam.o \
	$(BUILDDIR)/ref_gl/jcprepct.o \
	$(BUILDDIR)/ref_gl/jcsample.o \
	$(BUILDDIR)/ref_gl/jdapimin.o \
	$(BUILDDIR)/ref_gl/jdapistd.o \
	$(BUILDDIR)/ref_gl/jdatasrc.o \
	$(BUILDDIR)/ref_gl/jdcoefct.o \
	$(BUILDDIR)/ref_gl/jdcolor.o \
	$(BUILDDIR)/ref_gl/jddctmgr.o \
	$(BUILDDIR)/ref_gl/jdhuff.o \
	$(BUILDDIR)/ref_gl/jdinput.o \
	$(BUILDDIR)/ref_gl/jdmainct.o \
	$(BUILDDIR)/ref_gl/jdmarker.o \
	$(BUILDDIR)/ref_gl/jdmaster.o \
	$(BUILDDIR)/ref_gl/jdpostct.o \
	$(BUILDDIR)/ref_gl/jdsample.o \
	$(BUILDDIR)/ref_gl/jdtrans.o \
	$(BUILDDIR)/ref_gl/jerror.o \
	$(BUILDDIR)/ref_gl/jidctflt.o \
	$(BUILDDIR)/ref_gl/jmemmgr.o \
	$(BUILDDIR)/ref_gl/jmemnobs.o \
	$(BUILDDIR)/ref_gl/jutils.o
endif

REF_GLX_OBJS = \
	$(BUILDDIR)/ref_gl/gl_glx.o

REF_SDL_OBJS = \
	$(BUILDDIR)/ref_gl/gl_sdl.o

$(BUILDDIR)/ref_gl/jcapimin.o : $(JPDIR)/jcapimin.c; $(DO_GL_SHLIB_CC)
$(BUILDDIR)/ref_gl/jchuff.o : $(JPDIR)/jchuff.c; $(DO_GL_SHLIB_CC)
$(BUILDDIR)/ref_gl/jcinit.o : $(JPDIR)/jcinit.c; $(DO_GL_SHLIB_CC)
$(BUILDDIR)/ref_gl/jccoefct.o : $(JPDIR)/jccoefct.c; $(DO_GL_SHLIB_CC)
$(BUILDDIR)/ref_gl/jccolor.o : $(JPDIR)/jccolor.c; $(DO_GL_SHLIB_CC)
$(BUILDDIR)/ref_gl/jfdctflt.o : $(JPDIR)/jfdctflt.c; $(DO_GL_SHLIB_CC)
$(BUILDDIR)/ref_gl/jcdctmgr.o : $(JPDIR)/jcdctmgr.c; $(DO_GL_SHLIB_CC)
$(BUILDDIR)/ref_gl/jcmainct.o : $(JPDIR)/jcmainct.c; $(DO_GL_SHLIB_CC)
$(BUILDDIR)/ref_gl/jcmarker.o : $(JPDIR)/jcmarker.c; $(DO_GL_SHLIB_CC)
$(BUILDDIR)/ref_gl/jcmaster.o : $(JPDIR)/jcmaster.c; $(DO_GL_SHLIB_CC)
$(BUILDDIR)/ref_gl/jcomapi.o : $(JPDIR)/jcomapi.c; $(DO_GL_SHLIB_CC)
$(BUILDDIR)/ref_gl/jcparam.o : $(JPDIR)/jcparam.c;  $(DO_GL_SHLIB_CC)
$(BUILDDIR)/ref_gl/jcprepct.o : $(JPDIR)/jcprepct.c; $(DO_GL_SHLIB_CC)
$(BUILDDIR)/ref_gl/jcsample.o : $(JPDIR)/jcsample.c; $(DO_GL_SHLIB_CC)

$(BUILDDIR)/ref_gl/jdapimin.o : $(JPDIR)/jdapimin.c; $(DO_GL_SHLIB_CC)
$(BUILDDIR)/ref_gl/jdapistd.o : $(JPDIR)/jdapistd.c; $(DO_GL_SHLIB_CC)
$(BUILDDIR)/ref_gl/jdatasrc.o : $(JPDIR)/jdatasrc.c; $(DO_GL_SHLIB_CC)
$(BUILDDIR)/ref_gl/jdcoefct.o : $(JPDIR)/jdcoefct.c; $(DO_GL_SHLIB_CC)
$(BUILDDIR)/ref_gl/jdcolor.o : $(JPDIR)/jdcolor.c; $(DO_GL_SHLIB_CC)
$(BUILDDIR)/ref_gl/jcphuff.o : $(JPDIR)/jcphuff.c; $(DO_GL_SHLIB_CC)
$(BUILDDIR)/ref_gl/jddctmgr.o : $(JPDIR)/jddctmgr.c; $(DO_GL_SHLIB_CC)
$(BUILDDIR)/ref_gl/jdhuff.o : $(JPDIR)/jdhuff.c; $(DO_GL_SHLIB_CC)
$(BUILDDIR)/ref_gl/jdinput.o : $(JPDIR)/jdinput.c; $(DO_GL_SHLIB_CC)
$(BUILDDIR)/ref_gl/jdmainct.o : $(JPDIR)/jdmainct.c; $(DO_GL_SHLIB_CC)
$(BUILDDIR)/ref_gl/jdmarker.o : $(JPDIR)/jdmarker.c; $(DO_GL_SHLIB_CC)
$(BUILDDIR)/ref_gl/jdmaster.o : $(JPDIR)/jdmaster.c; $(DO_GL_SHLIB_CC)
$(BUILDDIR)/ref_gl/jdpostct.o : $(JPDIR)/jdpostct.c; $(DO_GL_SHLIB_CC)
$(BUILDDIR)/ref_gl/jdsample.o : $(JPDIR)/jdsample.c; $(DO_GL_SHLIB_CC)
$(BUILDDIR)/ref_gl/jdtrans.o : $(JPDIR)/jdtrans.c; $(DO_GL_SHLIB_CC)
$(BUILDDIR)/ref_gl/jerror.o : $(JPDIR)/jerror.c; $(DO_GL_SHLIB_CC)
$(BUILDDIR)/ref_gl/jidctflt.o : $(JPDIR)/jidctflt.c; $(DO_GL_SHLIB_CC)
$(BUILDDIR)/ref_gl/jmemmgr.o : $(JPDIR)/jmemmgr.c; $(DO_GL_SHLIB_CC)
$(BUILDDIR)/ref_gl/jmemnobs.o : $(JPDIR)/jmemnobs.c; $(DO_GL_SHLIB_CC)
$(BUILDDIR)/ref_gl/jutils.o : $(JPDIR)/jutils.c; $(DO_GL_SHLIB_CC)

$(BUILDDIR)/ref_glx.$(SHLIBEXT) : $(REF_GL_OBJS) $(REF_GLX_OBJS)
	$(CC) $(CFLAGS) $(SHLIBLDFLAGS) -o $@ $(REF_GL_OBJS) $(REF_GLX_OBJS) $(GLXLDFLAGS)

$(BUILDDIR)/ref_sdl.$(SHLIBEXT) : $(REF_GL_OBJS) $(REF_SDL_OBJS)
	$(CC) $(CFLAGS) $(SHLIBLDFLAGS) -o $@ $(REF_GL_OBJS) $(REF_SDL_OBJS) $(SDLLDFLAGS)

$(BUILDDIR)/ref_gl/gl_anim.o :        $(REF_GL_DIR)/gl_anim.c
	$(DO_GL_SHLIB_CC)

$(BUILDDIR)/ref_gl/gl_draw.o :        $(REF_GL_DIR)/gl_draw.c
	$(DO_GL_SHLIB_CC)

$(BUILDDIR)/ref_gl/gl_image.o :       $(REF_GL_DIR)/gl_image.c
	$(DO_GL_SHLIB_CC)

$(BUILDDIR)/ref_gl/gl_particle.o :    $(REF_GL_DIR)/gl_particle.c
	$(DO_GL_SHLIB_CC)

$(BUILDDIR)/ref_gl/gl_arb_shader.o :    $(REF_GL_DIR)/gl_arb_shader.c
	$(DO_GL_SHLIB_CC)

$(BUILDDIR)/ref_gl/gl_shadows.o :    $(REF_GL_DIR)/gl_shadows.c
	$(DO_GL_SHLIB_CC)

$(BUILDDIR)/ref_gl/gl_light.o :       $(REF_GL_DIR)/gl_light.c
	$(DO_GL_SHLIB_CC)

$(BUILDDIR)/ref_gl/gl_mesh.o :        $(REF_GL_DIR)/gl_mesh.c
	$(DO_GL_SHLIB_CC)

$(BUILDDIR)/ref_gl/gl_model.o :       $(REF_GL_DIR)/gl_model.c
	$(DO_GL_SHLIB_CC)

$(BUILDDIR)/ref_gl/gl_rmain.o :       $(REF_GL_DIR)/gl_rmain.c
	$(DO_GL_SHLIB_CC)

$(BUILDDIR)/ref_gl/gl_rmisc.o :       $(REF_GL_DIR)/gl_rmisc.c
	$(DO_GL_SHLIB_CC)

$(BUILDDIR)/ref_gl/gl_rsurf.o :       $(REF_GL_DIR)/gl_rsurf.c
	$(DO_GL_SHLIB_CC)

$(BUILDDIR)/ref_gl/gl_warp.o :        $(REF_GL_DIR)/gl_warp.c
	$(DO_GL_SHLIB_CC)

$(BUILDDIR)/ref_gl/qgl_linux.o :      $(LINUX_DIR)/qgl_linux.c
	$(DO_GL_SHLIB_CC)

$(BUILDDIR)/ref_gl/q_shared.o :       $(GAME_DIR)/q_shared.c
	$(DO_GL_SHLIB_CC)

$(BUILDDIR)/ref_gl/q_shlinux.o :      $(LINUX_DIR)/q_shlinux.c
	$(DO_GL_SHLIB_CC)

$(BUILDDIR)/ref_gl/glob.o :           $(LINUX_DIR)/glob.c
	$(DO_GL_SHLIB_CC)

$(BUILDDIR)/ref_gl/gl_glx.o :         $(LINUX_DIR)/gl_glx.c
	$(DO_GL_SHLIB_CC) $(GLXCFLAGS)

$(BUILDDIR)/ref_gl/gl_sdl.o :         $(LINUX_DIR)/gl_sdl.c
	$(DO_GL_SHLIB_CC) $(SDLCFLAGS)


#############################################################################
# MISC
#############################################################################

clean: clean-debug clean-release

clean-debug:
	$(MAKE) clean2 BUILDDIR=$(BUILD_DEBUG_DIR) CFLAGS="$(DEBUG_CFLAGS)"

clean-release:
	$(MAKE) clean2 BUILDDIR=$(BUILD_RELEASE_DIR) CFLAGS="$(DEBUG_CFLAGS)"

clean2:
	rm -f \
	$(QUAKE2_OBJS) \
	$(QUAKE2_AS_OBJS) \
	$(GAME_OBJS) \
	$(REF_GL_OBJS) \
	$(REF_GLX_OBJS) \
	$(REF_SDL_OBJS) \
	$(Q2DED_OBJS) \
	$(BUILDDIR)/ufo \
	$(BUILDDIR)/game.$(SHLIBEXT) \
	$(BUILDDIR)/ref_sdl.$(SHLIBEXT) \
	$(BUILDDIR)/ref_glx.$(SHLIBEXT) \
	$(BUILDDIR)/ufoded

distclean:
	@-rm -rf $(BUILD_DEBUG_DIR) $(BUILD_RELEASE_DIR)
	@-rm -f `find . \( -not -type d \) -and \
		\( -name '*~' \) -type f -print`

lang:
	pofiles='$(POFILES)'; \
	for po in $$pofiles; do \
	  po=`basename $$po`; \
	  dir=`echo $$po | sed -e 's,\.po,,'`; \
	  mkdir -p ../../base/i18n/$$dir/LC_MESSAGES; \
	  msgfmt -v -o ../../base/i18n/$$dir/LC_MESSAGES/ufoai.mo ../po/$$po; \
	done
