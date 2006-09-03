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
BUILD_GLX=YES		# X11 GLX driver
BUILD_DEDICATED=YES	# build a dedicated ufo server
BUILD_ALSA=YES		# build alsa sound driver
BUILD_OSS=YES		# build oss sound driver
BUILD_ARTS=NO		# build arts sound driver
BUILD_WITH_SHADER=YES	# build with shader support
HAVE_IPV6=NO		# Compile with IPv6 (protocol independent API). Tested on FreeBSD
HAVE_GETTEXT=YES	# Compile with multilanguage-support?
BUILD_WITH_DGA=NO	# dga support compiled in
BUILD_WITH_VIDMODE=YES	# vidmode support
BUILD_WITH_MMX=NO	# MMX support
BUILD_PARANOID=YES	# paranoid mode for debug
BUILD_FOR_PROFILING=NO  # build with -pg option for profiling via gprof
BUILD_COMPILETOOLS=YES	# build ufo2map bsp compile tool

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
ARCH := $(shell uname -m | sed -e s/i.86/i386/ -e s/sun4u/sparc/ -e s/sparc64/sparc/ -e s/arm.*/arm/ -e s/sa110/arm/ -e s/alpha/axp/ -e 's/Power Macintosh/ppc/' )

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

SVN_REV=
ifeq ($(wildcard .svn),.svn)
	SVN_REV=$(shell LANG=C svnversion .)
endif

MOUNT_DIR=.

BUILD_DEBUG_DIR=../debug$(ARCH)
BUILD_RELEASE_DIR=../release$(ARCH)
TOOLS_DIR=$(MOUNT_DIR)/tools
CLIENT_DIR=$(MOUNT_DIR)/client
SERVER_DIR=$(MOUNT_DIR)/server
REF_GL_DIR=$(MOUNT_DIR)/ref_gl
COMMON_DIR=$(MOUNT_DIR)/qcommon
GAME_DIR=$(MOUNT_DIR)/game
LINUX_DIR=$(MOUNT_DIR)/ports/linux
MACOSX_DIR=$(MOUNT_DIR)/ports/macosx
UNIX_DIR=$(MOUNT_DIR)/ports/unix
NULL_DIR=$(MOUNT_DIR)/ports/null
# TODO: Add targets for these plattforms
WIN32_DIR=$(MOUNT_DIR)/ports/win32
SOLARIS_DIR=$(MOUNT_DIR)/ports/solaris
IRIX_DIR=$(MOUNT_DIR)/ports/irix

BASE_CFLAGS=-Wall -pipe -Dstricmp=strcasecmp -D_BSD_SOURCE -D_XOPEN_SOURCE -std=c89
# this may fix prediction errors
# -ffloat-store -mfpmath=sse
# Put this in when it is fixed
# -ansi -pedantic
TOOLSCFLAGS=
TOOLSLDFLAGS=-lm
PROFILER_CFLAGS=

MAKE_FLAGS=--stop --silent

ifneq ($(ARCH),i386)
	BASE_CFLAGS+=-DC_ONLY
endif

ifeq ($(OSTYPE),FreeBSD)
	LDFLAGS=-lm -lvorbisfile -lvorbis -lintl
	BUILD_ALSA=NO
endif

ifeq ($(OSTYPE),Linux)
	LDFLAGS=-ldl -lm -lvorbisfile -lvorbis
endif

ifeq ($(OSTYPE),Darwin)
	# Turn off about everything, until we get the support right...
	BUILD_SDLUFO=NO	# use SDL for cdrom, sound and maybe renderer (selectable)
	BUILD_GLX=NO		# X11 GLX driver
	BUILD_DEDICATED=YES	# build a dedicated ufo server
	BUILD_ALSA=YES		# build alsa sound driver
	BUILD_OSS=YES		# build oss sound driver
	BUILD_ARTS=NO		# build arts sound driver
	BUILD_WITH_SHADER=NO	# build with shader support
	HAVE_IPV6=NO		# Compile with IPv6 (protocol independent API). Tested on FreeBSD
	HAVE_GETTEXT=YES	# Compile with multilanguage-support?
	BUILD_WITH_DGA=NO	# dga support compiled in
	BUILD_WITH_VIDMODE=NO	# vidmode support
	BUILD_WITH_MMX=NO	# MMX support
	BUILD_FOR_PROFILING=NO # build with -pg option for profiling via gprof
	BUILD_COMPILETOOLS=NO	# build ufo2map bsp compile tool

	LDFLAGS=-L/sw/lib -ldl -lm -lvorbisfile -lvorbis -lintl -framework IOKit -framework AppKit -framework QuickTime -framework CoreAudio -lGL
	BASE_CFLAGS+=-I/sw/include
endif

ifeq ($(OSTYPE),FreeBSD)
	ifeq ($(strip $(BUILD_SDLUFO)),YES)
		LDFLAGS+=$(shell sdl11-config --libs)
		BASE_CFLAGS+=$(shell sdl11-config --cflags)
	endif
	SDLLDFLAGS=$(shell sdl11-config --libs) -lSDL_ttf
	GLXCFLAGS=-I/usr/X11R6/include $(shell sdl11-config --cflags)
	GLXLDFLAGS=-L/usr/X11R6/lib$(_LIB) -lX11 -lXext $(shell sdl11-config --libs) -lSDL_ttf
else
	ifeq ($(strip $(BUILD_SDLUFO)),YES)
		LDFLAGS+=$(shell sdl-config --libs)
		BASE_CFLAGS+=$(shell sdl-config --cflags)
	endif
	SDLLDFLAGS=$(shell sdl-config --libs) -lSDL_ttf
	GLXCFLAGS=-I/usr/X11R6/include $(shell sdl-config --cflags)
	GLXLDFLAGS=-L/usr/X11R6/lib$(_LIB) -lX11 -lXext $(shell sdl-config --libs) -lSDL_ttf
endif

ifeq ($(strip $(BUILD_ALSA)),YES)
	BASE_CFLAGS+=-DUSE_ALSA
	LDFLAGS+=-lasound
endif

ifeq ($(strip $(BUILD_WITH_DGA)),YES)
	BASE_CFLAGS+=-DHAVE_DGA
endif

ifeq ($(strip $(BUILD_WITH_MMX)),YES)
	BASE_CFLAGS+=-DQ2_MMX_ENABLED -msse
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

ifeq ($(strip $(BUILD_FOR_PROFILING)),YES)
# -pg for gprof
# -fprofile-arcs -ftest-coverage for gcov
	PROFILER_CFLAGS+=-pg -DPROFILING -fprofile-arcs -ftest-coverage
endif

ifeq ($(ARCH),axp)
	RELEASE_CFLAGS=$(BASE_CFLAGS) -ffast-math -funroll-loops -fomit-frame-pointer -fexpensive-optimizations -DNDEBUG
endif

ifeq ($(ARCH),ppc)
	RELEASE_CFLAGS=$(BASE_CFLAGS) -O2 -ffast-math -funroll-loops -fomit-frame-pointer -fexpensive-optimizations -DNDEBUG
endif

ifeq ($(ARCH),sparc)
	RELEASE_CFLAGS=$(BASE_CFLAGS) -ffast-math -funroll-loops -fomit-frame-pointer -fexpensive-optimizations -DNDEBUG
endif

ifeq ($(ARCH),i386)
	RELEASE_CFLAGS=$(BASE_CFLAGS) -O2 -ffast-math -funroll-loops -falign-loops=2 -falign-jumps=2 -falign-functions=2 -fno-strict-aliasing -DNDEBUG
endif

ifeq ($(ARCH),x86_64)
	_LIB := 64
	RELEASE_CFLAGS=$(BASE_CFLAGS) -O2 -ffast-math -funroll-loops -fomit-frame-pointer -fexpensive-optimizations -fno-strict-aliasing -DNDEBUG
endif

#ifeq ($(ARCH),mipsEE)
#	GLCFLAGS=-I$(MESA_DIR)/include -I/usr/X11R6/include
#	GLLDFLAGS=-L$(MESA_DIR)/lib -L/usr/X11R6/lib -L/usr/local/lib -lm -lGL -lGLU -lps2dev
#endif
#ifeq ($(ARCH),i386)
#	GLCFLAGS=-I$(MESA_DIR)/include -I/usr/include/glide -I/usr/X11R6/include
#	GLLDFLAGS=-L/usr/X11R6/lib -L/usr/local/lib -L$(MESA_DIR)/lib -lX11 -lXext -lvga -lm
#endif

ifeq ($(strip $(BUILD_WITH_DGA)),YES)
	GLXLDFLAGS+=-lXxf86dga
endif

ifeq ($(strip $(BUILD_WITH_VIDMODE)),YES)
	GLXLDFLAGS+=-lXxf86vm
endif

ifeq ($(OSTYPE),Darwin)
SHLIBEXT=dylib
SHLIBCFLAGS=-fPIC -fno-common
SHLIBLDFLAGS=-dynamiclib $(LDFLAGS)
NOTSHLIBCFLAGS=-mdynamic-no-pic
else
SHLIBEXT=so
SHLIBCFLAGS=-fPIC
SHLIBLDFLAGS=-shared
NOTSHLIBCFLAGS=
endif

GLXLDFLAGS+=-ljpeg
SDLLDFLAGS+=-ljpeg

DEBUG_CFLAGS=$(BASE_CFLAGS) -ggdb -O0 -D_FORTIFY_SOURCE=2 -DDEBUG $(PROFILER_CFLAGS) -fno-inline

ifeq ($(strip $(BUILD_PARANOID)),YES)
	DEBUG_CFLAGS+=-DPARANOID
endif

DO_CC=echo " * [UFO] $@"; $(CC) $(NOTSHLIBCFLAGS) $(CFLAGS) -o $@ -c $<
DO_DED_CC=echo " * [DED] $@"; $(CC) $(CFLAGS) -DDEDICATED_ONLY -o $@ -c $<
DO_TOOLS_CC=echo " * [MAP] $@"; $(CC) $(CFLAGS) -o $@ -c $<
DO_JPEG_CC=echo " * [JPG] $@"; $(CC) $(NOTSHLIBCFLAGS) $(CFLAGS) -o $@ -c $<
DO_O_CC=echo " * [OPT] $@"; $(CC) $(CFLAGS) -O -o $@ -c $<
DO_SHLIB_CC=echo " * [LIB] $@"; $(CC) $(CFLAGS) $(SHLIBCFLAGS) -o $@ -c $<
DO_GL_SHLIB_CC=echo " * [REF] $@"; $(CC) $(CFLAGS) $(SHLIBCFLAGS) $(GLXCFLAGS) -o $@ -c $<
DO_SND_SHLIB_CC=echo " * [SND] $@"; $(CC) $(CFLAGS) $(SHLIBCFLAGS) -o $@ -c $<
DO_AS=echo " * [AS.] $@"; $(CC) $(CFLAGS) -DELF -x assembler-with-cpp -o $@ -c $<
DO_SHLIB_AS=echo " * [ASL] $@"; $(CC) $(CFLAGS) $(SHLIBCFLAGS) -DELF -x assembler-with-cpp -o $@ -c $<

#############################################################################
# gettext stuff
#############################################################################
WORKSPACE = $(shell pwd)
POFILES = $(wildcard $(WORKSPACE)/po/*.po)
#############################################################################

#############################################################################
# SETUP AND BUILD
#############################################################################

.PHONY : targets build_debug build_release clean clean-debug clean-release clean2 docs echo_info

TARGETS=$(BUILDDIR)/ufo $(BUILDDIR)/game.$(SHLIBEXT)

ifeq ($(strip $(BUILD_DEDICATED)),YES)
	TARGETS += $(BUILDDIR)/ufoded
endif

ifeq ($(strip $(BUILD_ALSA)),YES)
	TARGETS += $(BUILDDIR)/snd_alsa.$(SHLIBEXT)
endif

ifeq ($(strip $(BUILD_OSS)),YES)
	TARGETS += $(BUILDDIR)/snd_oss.$(SHLIBEXT)
endif

ifeq ($(strip $(BUILD_ARTS)),YES)
	TARGETS += $(BUILDDIR)/snd_arts.$(SHLIBEXT)
endif

ifeq ($(strip $(BUILD_SDLUFO)),YES)
	TARGETS += $(BUILDDIR)/snd_sdl.$(SHLIBEXT)
endif

ifeq ($(ARCH),axp)
	ifeq ($(strip $(BUILD_GLX)),YES)
		$(warning Warning: GLX support not supported for $(ARCH))
	endif
	ifeq ($(strip $(BUILD_SDLUFO)),YES)
		$(warning Warning: SDL support not supported for $(ARCH))
	endif
endif # ARCH axp

ifeq ($(ARCH),ppc)
	ifeq ($(strip $(BUILD_GLX)),YES)
		TARGETS += $(BUILDDIR)/ref_glx.$(SHLIBEXT)
	endif
	ifeq ($(strip $(BUILD_SDLUFO)),YES)
		TARGETS += $(BUILDDIR)/ref_sdl.$(SHLIBEXT)
	endif
endif # ARCH ppc

ifeq ($(ARCH),sparc)
	ifeq ($(strip $(BUILD_GLX)),YES)
		$(warning Warning: GLX support not supported for $(ARCH))
	endif
	ifeq ($(strip $(BUILD_SDLUFO)),YES)
		$(warning Warning: SDL support not supported for $(ARCH))
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

ifeq ($(strip $(BUILD_COMPILETOOLS)),YES)
	TARGETS += $(BUILDDIR)/ufo2map
endif

debug: build_debug copy_debug echo_info
all: build_debug build_release lang
lazy: lang build_debug copy_debug maps
installer: win32installer linuxinstaller sourcearchive
mappack:
	tar -cvjp --exclude-from=$(LINUX_DIR)/tar.ex -f ufoai-$(VERSION)-mappack.tar.bz2 ../base/maps

win32installer:
	makensis ports/win32/installer.nsi

linuxinstaller:
	tar -cvjp --exclude-from=$(LINUX_DIR)/tar.ex -f ufoai-$(VERSION)-linux.tar.bz2 ../

sourcearchive:
	tar -cvjp --exclude-from=$(LINUX_DIR)/tar.ex -f ufoai-$(VERSION)-source.tar.bz2 ./

deb:
	debuild binary

copy_debug:
	@rm -f ../ufo ../ufo2map ../ref_glx.$(SHLIBEXT) ../ref_sdl.$(SHLIBEXT) ../base/game.$(SHLIBEXT) ../ufoded ../snd_sdl.$(SHLIBEXT) ../snd_alsa.$(SHLIBEXT) ../snd_oss.$(SHLIBEXT) ../snd_arts.$(SHLIBEXT)
	@if test -f $(BUILD_DEBUG_DIR)/ufoded; then cp $(BUILD_DEBUG_DIR)/ufoded ../ufoded; fi
	@if test -f $(BUILD_DEBUG_DIR)/ufo; then cp $(BUILD_DEBUG_DIR)/ufo ../ufo; fi
	@if test -f $(BUILD_DEBUG_DIR)/ufo2map; then cp $(BUILD_DEBUG_DIR)/ufo2map ../ufo2map; fi
	@if test -f $(BUILD_DEBUG_DIR)/ref_sdl.$(SHLIBEXT); then cp $(BUILD_DEBUG_DIR)/ref_sdl.$(SHLIBEXT) ../ref_sdl.$(SHLIBEXT); fi
	@if test -f $(BUILD_DEBUG_DIR)/ref_glx.$(SHLIBEXT); then cp $(BUILD_DEBUG_DIR)/ref_glx.$(SHLIBEXT) ../ref_glx.$(SHLIBEXT); fi
	@if test -f $(BUILD_DEBUG_DIR)/snd_alsa.$(SHLIBEXT); then cp $(BUILD_DEBUG_DIR)/snd_alsa.$(SHLIBEXT) ../snd_alsa.$(SHLIBEXT); fi
	@if test -f $(BUILD_DEBUG_DIR)/snd_sdl.$(SHLIBEXT); then cp $(BUILD_DEBUG_DIR)/snd_sdl.$(SHLIBEXT) ../snd_sdl.$(SHLIBEXT); fi
	@if test -f $(BUILD_DEBUG_DIR)/snd_oss.$(SHLIBEXT); then cp $(BUILD_DEBUG_DIR)/snd_oss.$(SHLIBEXT) ../snd_oss.$(SHLIBEXT); fi
	@if test -f $(BUILD_DEBUG_DIR)/snd_arts.$(SHLIBEXT); then cp $(BUILD_DEBUG_DIR)/snd_arts.$(SHLIBEXT) ../snd_arts.$(SHLIBEXT); fi
	@if test -f $(BUILD_DEBUG_DIR)/game.$(SHLIBEXT); then cp $(BUILD_DEBUG_DIR)/game.$(SHLIBEXT) ../base/game.$(SHLIBEXT); fi

release: build_release

build_debug:
	@-mkdir -p $(BUILD_DEBUG_DIR) \
		$(BUILD_DEBUG_DIR)/client \
		$(BUILD_DEBUG_DIR)/ded \
		$(BUILD_DEBUG_DIR)/ref_gl \
		$(BUILD_DEBUG_DIR)/game \
		$(BUILD_DEBUG_DIR)/tools \
		$(BUILD_DEBUG_DIR)/tools/ufo2map \
		$(BUILD_DEBUG_DIR)/tools/ufo2map/common
	@$(MAKE) $(MAKE_FLAGS) targets BUILDDIR=$(BUILD_DEBUG_DIR) CFLAGS="$(DEBUG_CFLAGS) -DSVN='\"$(SVN_REV)\"'"

echo_info:
	@echo
	@echo "CFLAGS"
	@echo "-----------------------"
	@echo $(DEBUG_CFLAGS)
	@echo
	@echo "LDFLAGS"
	@echo "-----------------------"
	@echo $(LDFLAGS)
	@echo
	@echo "Options"
	@echo "-----------------------"
	@echo "BUILD_SDLUFO = $(BUILD_SDLUFO)"
	@echo "BUILD_GLX = $(BUILD_GLX)"
	@echo "BUILD_DEDICATED = $(BUILD_DEDICATED)"
	@echo "BUILD_ALSA = $(BUILD_ALSA)"
	@echo "BUILD_WITH_SHADER = $(BUILD_WITH_SHADER)"
	@echo "HAVE_IPV6 = $(HAVE_IPV6)"
	@echo "HAVE_GETTEXT = $(HAVE_GETTEXT)"
	@echo "BUILD_WITH_DGA = $(BUILD_WITH_DGA)"
	@echo "BUILD_WITH_VIDMODE = $(BUILD_WITH_VIDMODE)"
	@echo "BUILD_WITH_MMX = $(BUILD_WITH_MMX)"
	@echo "BUILD_FOR_PROFILING = $(BUILD_FOR_PROFILING)"
	@echo "BUILD_COMPILETOOLS = $(BUILD_COMPILETOOLS)"
	@echo "BUILD_PARANOID = $(BUILD_PARANOID)"
	@echo
	@echo "Note"
	@echo "-----------------------"
	@echo "This is the debug version."
	@echo "For better performace type 'make release' to build optimzed binaries."
	@echo "To debug this version type 'gdb ufo'"
	@echo "Enter 'run +set vid_fullscreen 0 +set vid_grabmouse 0'"
	@echo "After a crash type 'bt' to gdb console"
	@echo
	@echo "Gettext"
	@echo "-----------------------"
	@echo "Type 'make lang' to compile the gettext translation files. Type 'make update-po' in src/po to update with newest strings"
	@echo
	@echo "Maps"
	@echo "-----------------------"
	@echo "Type 'make' in base/maps to compile the maps"
	@echo
	@echo Build for $(ARCH)
	@echo

build_release:
	@-mkdir -p $(BUILD_RELEASE_DIR) \
		$(BUILD_RELEASE_DIR)/client \
		$(BUILD_RELEASE_DIR)/ded \
		$(BUILD_RELEASE_DIR)/ref_gl \
		$(BUILD_RELEASE_DIR)/game \
		$(BUILD_RELEASE_DIR)/tools \
		$(BUILD_RELEASE_DIR)/tools/ufo2map \
		$(BUILD_RELEASE_DIR)/tools/ufo2map/common
	@$(MAKE) $(MAKE_FLAGS) targets BUILDDIR=$(BUILD_RELEASE_DIR) CFLAGS="$(RELEASE_CFLAGS) -DSVN='\"$(SVN_REV)\"'"
	@echo
	@echo "CFLAGS"
	@echo "-----------------------"
	@echo $(RELEASE_CFLAGS)
	@echo
	@echo "LDFLAGS"
	@echo "-----------------------"
	@echo $(LDFLAGS)
	@echo
	@echo "Options"
	@echo "-----------------------"
	@echo "BUILD_SDLUFO = $(BUILD_SDLUFO)"
	@echo "BUILD_GLX = $(BUILD_GLX)"
	@echo "BUILD_DEDICATED = $(BUILD_DEDICATED)"
	@echo "BUILD_ALSA = $(BUILD_ALSA)"
	@echo "BUILD_WITH_SHADER = $(BUILD_WITH_SHADER)"
	@echo "HAVE_IPV6 = $(HAVE_IPV6)"
	@echo "HAVE_GETTEXT = $(HAVE_GETTEXT)"
	@echo "BUILD_WITH_DGA = $(BUILD_WITH_DGA)"
	@echo "BUILD_WITH_VIDMODE = $(BUILD_WITH_VIDMODE)"
	@echo "BUILD_WITH_MMX = $(BUILD_WITH_MMX)"
	@echo "BUILD_FOR_PROFILING = $(BUILD_FOR_PROFILING)"
	@echo "BUILD_COMPILETOOLS = $(BUILD_COMPILETOOLS)"
	@echo "BUILD_PARANOID = $(BUILD_PARANOID) (only in debug mode)"
	@echo
	@echo "Gettext"
	@echo "-----------------------"
	@echo "Type 'make lang' to compile the gettext translation files. Type 'make update-po' in src/po to update with newest strings"
	@echo
	@echo "Maps"
	@echo "-----------------------"
	@echo "Type 'make' in base/maps to compile the maps"
	@echo

targets: $(TARGETS)

#############################################################################
# TOOLS
#############################################################################

TOOLS_OBJS = \
	$(BUILDDIR)/tools/ufo2map/ufo2map.o \
	$(BUILDDIR)/tools/ufo2map/qrad3.o \
	$(BUILDDIR)/tools/ufo2map/qbsp3.o \
	$(BUILDDIR)/tools/ufo2map/brushbsp.o \
	$(BUILDDIR)/tools/ufo2map/csg.o \
	$(BUILDDIR)/tools/ufo2map/faces.o \
	$(BUILDDIR)/tools/ufo2map/glfile.o \
	$(BUILDDIR)/tools/ufo2map/levels.o \
	$(BUILDDIR)/tools/ufo2map/lightmap.o \
	$(BUILDDIR)/tools/ufo2map/map.o \
	$(BUILDDIR)/tools/ufo2map/nodraw.o \
	$(BUILDDIR)/tools/ufo2map/patches.o \
	$(BUILDDIR)/tools/ufo2map/portals.o \
	$(BUILDDIR)/tools/ufo2map/routing.o \
	$(BUILDDIR)/tools/ufo2map/textures.o \
	$(BUILDDIR)/tools/ufo2map/tree.o \
	$(BUILDDIR)/tools/ufo2map/writebsp.o \
	$(BUILDDIR)/tools/ufo2map/common/bspfile.o \
	$(BUILDDIR)/tools/ufo2map/common/cmdlib.o \
	$(BUILDDIR)/tools/ufo2map/common/l3dslib.o \
	$(BUILDDIR)/tools/ufo2map/common/lbmlib.o \
	$(BUILDDIR)/tools/ufo2map/common/mathlib.o \
	$(BUILDDIR)/tools/ufo2map/common/polylib.o \
	$(BUILDDIR)/tools/ufo2map/common/scriplib.o \
	$(BUILDDIR)/tools/ufo2map/common/threads.o \
	$(BUILDDIR)/tools/ufo2map/common/trace.o \
	$(BUILDDIR)/tools/ufo2map/common/trilib.o

$(BUILDDIR)/ufo2map : $(TOOLS_OBJS)
	@echo " * [MAP] ...linking";
	$(CC) $(CFLAGS) $(TOOLSCFLAGS) -o $@ $(TOOLS_OBJS) $(TOOLSLDFLAGS)

$(BUILDDIR)/tools/ufo2map/ufo2map.o :    $(TOOLS_DIR)/ufo2map/ufo2map.c
	$(DO_TOOLS_CC)

$(BUILDDIR)/tools/ufo2map/qbsp3.o :    $(TOOLS_DIR)/ufo2map/qbsp3.c
	$(DO_TOOLS_CC)

$(BUILDDIR)/tools/ufo2map/qrad3.o :    $(TOOLS_DIR)/ufo2map/qrad3.c
	$(DO_TOOLS_CC)

$(BUILDDIR)/tools/ufo2map/brushbsp.o :    $(TOOLS_DIR)/ufo2map/brushbsp.c
	$(DO_TOOLS_CC)

$(BUILDDIR)/tools/ufo2map/csg.o :    $(TOOLS_DIR)/ufo2map/csg.c
	$(DO_TOOLS_CC)

$(BUILDDIR)/tools/ufo2map/faces.o :    $(TOOLS_DIR)/ufo2map/faces.c
	$(DO_TOOLS_CC)

$(BUILDDIR)/tools/ufo2map/gldraw.o :    $(TOOLS_DIR)/ufo2map/gldraw.c
	$(DO_TOOLS_CC)

$(BUILDDIR)/tools/ufo2map/glfile.o :    $(TOOLS_DIR)/ufo2map/glfile.c
	$(DO_TOOLS_CC)

$(BUILDDIR)/tools/ufo2map/leakfile.o :    $(TOOLS_DIR)/ufo2map/leakfile.c
	$(DO_TOOLS_CC)

$(BUILDDIR)/tools/ufo2map/levels.o :    $(TOOLS_DIR)/ufo2map/levels.c
	$(DO_TOOLS_CC)

$(BUILDDIR)/tools/ufo2map/lightmap.o :    $(TOOLS_DIR)/ufo2map/lightmap.c
	$(DO_TOOLS_CC)

$(BUILDDIR)/tools/ufo2map/map.o :    $(TOOLS_DIR)/ufo2map/map.c
	$(DO_TOOLS_CC)

$(BUILDDIR)/tools/ufo2map/nodraw.o :    $(TOOLS_DIR)/ufo2map/nodraw.c
	$(DO_TOOLS_CC)

$(BUILDDIR)/tools/ufo2map/patches.o :    $(TOOLS_DIR)/ufo2map/patches.c
	$(DO_TOOLS_CC)

$(BUILDDIR)/tools/ufo2map/portals.o :    $(TOOLS_DIR)/ufo2map/portals.c
	$(DO_TOOLS_CC)

$(BUILDDIR)/tools/ufo2map/prtfile.o :    $(TOOLS_DIR)/ufo2map/prtfile.c
	$(DO_TOOLS_CC)

$(BUILDDIR)/tools/ufo2map/routing.o :    $(TOOLS_DIR)/ufo2map/routing.c
	$(DO_TOOLS_CC)

$(BUILDDIR)/tools/ufo2map/textures.o :    $(TOOLS_DIR)/ufo2map/textures.c
	$(DO_TOOLS_CC)

$(BUILDDIR)/tools/ufo2map/tree.o :    $(TOOLS_DIR)/ufo2map/tree.c
	$(DO_TOOLS_CC)

$(BUILDDIR)/tools/ufo2map/writebsp.o :    $(TOOLS_DIR)/ufo2map/writebsp.c
	$(DO_TOOLS_CC)

$(BUILDDIR)/tools/ufo2map/common/bspfile.o :    $(TOOLS_DIR)/ufo2map/common/bspfile.c
	$(DO_TOOLS_CC)

$(BUILDDIR)/tools/ufo2map/common/cmdlib.o :    $(TOOLS_DIR)/ufo2map/common/cmdlib.c
	$(DO_TOOLS_CC)

$(BUILDDIR)/tools/ufo2map/common/l3dslib.o :    $(TOOLS_DIR)/ufo2map/common/l3dslib.c
	$(DO_TOOLS_CC)

$(BUILDDIR)/tools/ufo2map/common/lbmlib.o :    $(TOOLS_DIR)/ufo2map/common/lbmlib.c
	$(DO_TOOLS_CC)

$(BUILDDIR)/tools/ufo2map/common/mathlib.o :    $(TOOLS_DIR)/ufo2map/common/mathlib.c
	$(DO_TOOLS_CC)

$(BUILDDIR)/tools/ufo2map/common/polylib.o :    $(TOOLS_DIR)/ufo2map/common/polylib.c
	$(DO_TOOLS_CC)

$(BUILDDIR)/tools/ufo2map/common/scriplib.o :    $(TOOLS_DIR)/ufo2map/common/scriplib.c
	$(DO_TOOLS_CC)

$(BUILDDIR)/tools/ufo2map/common/threads.o :    $(TOOLS_DIR)/ufo2map/common/threads.c
	$(DO_TOOLS_CC)

$(BUILDDIR)/tools/ufo2map/common/trace.o :    $(TOOLS_DIR)/ufo2map/common/trace.c
	$(DO_TOOLS_CC)

$(BUILDDIR)/tools/ufo2map/common/trilib.o :    $(TOOLS_DIR)/ufo2map/common/trilib.c
	$(DO_TOOLS_CC)


#############################################################################
# CLIENT/SERVER
#############################################################################

QUAKE2_OBJS = \
	$(BUILDDIR)/client/cl_actor.o \
	$(BUILDDIR)/client/cl_basemanagement.o \
	$(BUILDDIR)/client/cl_research.o \
	$(BUILDDIR)/client/cl_market.o \
	$(BUILDDIR)/client/cl_campaign.o \
	$(BUILDDIR)/client/cl_ufo.o \
	$(BUILDDIR)/client/cl_radar.o \
	$(BUILDDIR)/client/cl_popup.o \
	$(BUILDDIR)/client/cl_produce.o \
	$(BUILDDIR)/client/cl_employee.o \
	$(BUILDDIR)/client/cl_map.o \
	$(BUILDDIR)/client/cl_aircraft.o \
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
	$(BUILDDIR)/client/cl_inventory.o \
	$(BUILDDIR)/client/cl_ufopedia.o \
	$(BUILDDIR)/client/cl_view.o \
	$(BUILDDIR)/client/console.o \
	$(BUILDDIR)/client/keys.o \
	$(BUILDDIR)/client/snd_ref.o \
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
	$(BUILDDIR)/client/q_shared.o

ifeq ($(OSTYPE),Linux)
	QUAKE2_OBJS+= \
	$(BUILDDIR)/client/q_shlinux.o \
	$(BUILDDIR)/client/vid_so.o \
	$(BUILDDIR)/client/sys_linux.o \
	$(BUILDDIR)/client/glob.o \
	$(BUILDDIR)/client/$(NET_UDP).o
endif

ifeq ($(OSTYPE),Darwin)
	QUAKE2_OBJS+= \
	$(BUILDDIR)/client/sys_osx.o \
	$(BUILDDIR)/client/vid_osx.o \
	$(BUILDDIR)/client/in_osx.o \
	$(BUILDDIR)/client/snddma_osx.o \
	$(BUILDDIR)/client/glob.o \
	$(BUILDDIR)/client/$(NET_UDP).o \
	$(BUILDDIR)/client/q_shosx.o
	# FIXME Add more objects
endif

ifeq ($(strip $(BUILD_SDLUFO)),YES)
	QUAKE2_OBJS+=$(BUILDDIR)/client/cd_sdl.o
else
	ifeq ($(OSTYPE),Linux)
		QUAKE2_OBJS+=$(BUILDDIR)/client/cd_linux.o
	endif
	ifeq ($(OSTYPE),Darwin)
		QUAKE2_OBJS+=$(BUILDDIR)/client/cd_osx.o
	endif
endif

$(BUILDDIR)/ufo : $(QUAKE2_OBJS) $(QUAKE2_LNX_OBJS)
	@echo " * [UFO] ...linking";
	$(CC) $(CFLAGS) -o $@ $(QUAKE2_OBJS) $(QUAKE2_LNX_OBJS) $(LDFLAGS)

$(BUILDDIR)/client/cl_actor.o :    $(CLIENT_DIR)/cl_actor.c
	$(DO_CC)

$(BUILDDIR)/client/cl_campaign.o : $(CLIENT_DIR)/cl_campaign.c
	$(DO_CC)

$(BUILDDIR)/client/cl_ufo.o : $(CLIENT_DIR)/cl_ufo.c
	$(DO_CC)

$(BUILDDIR)/client/cl_radar.o : $(CLIENT_DIR)/cl_radar.c
	$(DO_CC)

$(BUILDDIR)/client/cl_popup.o : $(CLIENT_DIR)/cl_popup.c
	$(DO_CC)

$(BUILDDIR)/client/cl_produce.o : $(CLIENT_DIR)/cl_produce.c
	$(DO_CC)

$(BUILDDIR)/client/cl_employee.o : $(CLIENT_DIR)/cl_employee.c
	$(DO_CC)

$(BUILDDIR)/client/cl_map.o : $(CLIENT_DIR)/cl_map.c
	$(DO_CC)

$(BUILDDIR)/client/cl_aircraft.o : $(CLIENT_DIR)/cl_aircraft.c
	$(DO_CC)

$(BUILDDIR)/client/cl_basemanagement.o : $(CLIENT_DIR)/cl_basemanagement.c
	$(DO_CC)

$(BUILDDIR)/client/cl_research.o : $(CLIENT_DIR)/cl_research.c
	$(DO_CC)

$(BUILDDIR)/client/cl_market.o : $(CLIENT_DIR)/cl_market.c
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

$(BUILDDIR)/client/cl_inventory.o :     $(CLIENT_DIR)/cl_inventory.c
	$(DO_CC)

$(BUILDDIR)/client/cl_ufopedia.o : $(CLIENT_DIR)/cl_ufopedia.c
	$(DO_CC)

$(BUILDDIR)/client/cl_view.o :     $(CLIENT_DIR)/cl_view.c
	$(DO_CC)

$(BUILDDIR)/client/console.o :     $(CLIENT_DIR)/console.c
	$(DO_CC)

$(BUILDDIR)/client/keys.o :        $(CLIENT_DIR)/keys.c
	$(DO_CC)

$(BUILDDIR)/client/snd_ref.o :     $(CLIENT_DIR)/snd_ref.c
	$(DO_CC)

$(BUILDDIR)/client/snd_mem.o :     $(CLIENT_DIR)/snd_mem.c
	$(DO_CC)

$(BUILDDIR)/client/snd_mix.o :     $(CLIENT_DIR)/snd_mix.c
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

$(BUILDDIR)/client/sys_linux.o :   $(LINUX_DIR)/sys_linux.c
	$(DO_CC)

$(BUILDDIR)/client/glob.o :        $(UNIX_DIR)/glob.c
	$(DO_CC)

$(BUILDDIR)/client/q_shared.o :    $(GAME_DIR)/q_shared.c
	$(DO_CC)

$(BUILDDIR)/client/net_udp.o :    $(UNIX_DIR)/net_udp.c
	$(DO_CC)

$(BUILDDIR)/client/net_udp6.o :    $(UNIX_DIR)/net_udp6.c
	$(DO_CC)

$(BUILDDIR)/client/cd_sdl.o :     $(UNIX_DIR)/cd_sdl.c
	$(DO_CC)

$(BUILDDIR)/client/cd_osx.o :    $(MACOSX_DIR)/cd_osx.m
	$(DO_CC)

$(BUILDDIR)/client/sys_osx.o :    $(MACOSX_DIR)/sys_osx.m
	$(DO_CC)

$(BUILDDIR)/client/in_osx.o :    $(MACOSX_DIR)/in_osx.m
	$(DO_CC)

$(BUILDDIR)/client/vid_osx.o :    $(MACOSX_DIR)/vid_osx.m
	$(DO_CC)

$(BUILDDIR)/client/snddma_osx.o :    $(MACOSX_DIR)/snddma_osx.c
	$(DO_CC)

$(BUILDDIR)/client/q_shosx.o :    $(MACOSX_DIR)/q_shosx.c
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
	$(BUILDDIR)/ded/q_shared.o \
	\
	$(BUILDDIR)/ded/cl_null.o \
	$(BUILDDIR)/ded/cd_null.o

ifeq ($(OSTYPE),Linux)
	Q2DED_OBJS += \
	$(BUILDDIR)/ded/q_shlinux.o \
	$(BUILDDIR)/ded/sys_linux.o \
	$(BUILDDIR)/ded/glob.o \
	$(BUILDDIR)/ded/$(NET_UDP).o
endif

ifeq ($(OSTYPE),Darwin)
	Q2DED_OBJS += \
	$(BUILDDIR)/ded/sys_osx.o \
	$(BUILDDIR)/ded/q_shosx.o \
	$(BUILDDIR)/ded/glob.o \
	$(BUILDDIR)/ded/$(NET_UDP).o
endif

$(BUILDDIR)/ufoded : $(Q2DED_OBJS)
	@echo " * [DED] ...linking";
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

$(BUILDDIR)/ded/glob.o :	$(UNIX_DIR)/glob.c
	$(DO_DED_CC)

$(BUILDDIR)/ded/net_udp.o :    $(UNIX_DIR)/net_udp.c
	$(DO_DED_CC)

$(BUILDDIR)/ded/net_udp6.o :    $(UNIX_DIR)/net_udp6.c
	$(DO_DED_CC)

$(BUILDDIR)/ded/cd_null.o :    $(NULL_DIR)/cd_null.c
	$(DO_DED_CC)

$(BUILDDIR)/ded/cl_null.o :    $(NULL_DIR)/cl_null.c
	$(DO_DED_CC)

$(BUILDDIR)/ded/sys_osx.o :    $(MACOSX_DIR)/sys_osx.m
	$(DO_DED_CC)

$(BUILDDIR)/ded/q_shosx.o :    $(MACOSX_DIR)/q_shosx.c
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
	@echo " * [LIB] ...linking";
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
	$(BUILDDIR)/ref_gl/gl_font.o \
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

REF_GLX_OBJS = \
	$(BUILDDIR)/ref_gl/gl_glx.o

REF_SDL_OBJS = \
	$(BUILDDIR)/ref_gl/gl_sdl.o

REF_PS2_OBJS = \
	$(BUILDDIR)/ref_gl/gl_ps2mesa.o

$(BUILDDIR)/ref_glx.$(SHLIBEXT) : $(REF_GL_OBJS) $(REF_GLX_OBJS)
	@echo " * [GLX] ...linking";
	$(CC) $(CFLAGS) $(SHLIBLDFLAGS) -o $@ $(REF_GL_OBJS) $(REF_GLX_OBJS) $(GLXLDFLAGS)

$(BUILDDIR)/ref_sdl.$(SHLIBEXT) : $(REF_GL_OBJS) $(REF_SDL_OBJS)
	@echo " * [SDL] ...linking";
	$(CC) $(CFLAGS) $(SHLIBLDFLAGS) -o $@ $(REF_GL_OBJS) $(REF_SDL_OBJS) $(SDLLDFLAGS)

ifeq ($(ARCH),mipsEE)
$(BUILDDIR)/ref_sdl.$(SHLIBEXT) : $(REF_GL_OBJS) $(REF_PS2_OBJS)
	@echo " * [PSP] ...linking";
	$(CC) $(CFLAGS) $(SHLIBLDFLAGS) -o $@ $(REF_GL_OBJS) $(REF_PS2_OBJS) $(SDLLDFLAGS)
endif

$(BUILDDIR)/ref_gl/gl_anim.o :        $(REF_GL_DIR)/gl_anim.c
	$(DO_GL_SHLIB_CC)

$(BUILDDIR)/ref_gl/gl_draw.o :        $(REF_GL_DIR)/gl_draw.c
	$(DO_GL_SHLIB_CC)

$(BUILDDIR)/ref_gl/gl_font.o :        $(REF_GL_DIR)/gl_font.c
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

$(BUILDDIR)/ref_gl/glob.o :           $(UNIX_DIR)/glob.c
	$(DO_GL_SHLIB_CC)

$(BUILDDIR)/ref_gl/gl_glx.o :         $(LINUX_DIR)/gl_glx.c
	$(DO_GL_SHLIB_CC) $(GLXCFLAGS)

$(BUILDDIR)/ref_gl/gl_sdl.o :         $(LINUX_DIR)/gl_sdl.c
	$(DO_GL_SHLIB_CC) $(SDLCFLAGS)

$(BUILDDIR)/ps2/gl_ps2mesa.o :        $(LINUX_DIR)/gl_ps2mesa.c
	$(DO_GL_SHLIB_CC)


#############################################################################
# MISC
#############################################################################

SND_SDL_OBJS = $(BUILDDIR)/client/snd_sdl.o

SND_ALSA_OBJS = $(BUILDDIR)/client/snd_alsa.o

SND_OSS_OBJS = $(BUILDDIR)/client/snd_oss.o

SND_ARTS_OBJS = $(BUILDDIR)/client/snd_arts.o

$(BUILDDIR)/client/snd_oss.o :   $(LINUX_DIR)/snd_oss.c
	$(DO_SND_SHLIB_CC)

$(BUILDDIR)/client/snd_arts.o :   $(LINUX_DIR)/snd_arts.c
	$(DO_SND_SHLIB_CC) $(shell artsc-config --cflags)

$(BUILDDIR)/client/snd_sdl.o :     $(LINUX_DIR)/snd_sdl.c
	$(DO_SND_SHLIB_CC)

$(BUILDDIR)/client/snd_alsa.o :     $(LINUX_DIR)/snd_alsa.c
	$(DO_SND_SHLIB_CC)

$(BUILDDIR)/snd_oss.$(SHLIBEXT) : $(SND_OSS_OBJS)
	@echo " * [OSS] ...linking";
	$(CC) $(CFLAGS) $(SHLIBLDFLAGS) -o $@ $(SND_OSS_OBJS)

$(BUILDDIR)/snd_arts.$(SHLIBEXT) : $(SND_ARTS_OBJS)
	@echo " * [ART] ...linking";
	$(CC) $(CFLAGS) $(SHLIBLDFLAGS) $(shell artsc-config --libs) -o $@ $(SND_ARTS_OBJS)

$(BUILDDIR)/snd_alsa.$(SHLIBEXT) : $(SND_ALSA_OBJS)
	@echo " * [ALS] ...linking";
	$(CC) $(CFLAGS) $(SHLIBLDFLAGS) -o $@ $(SND_ALSA_OBJS)

$(BUILDDIR)/snd_sdl.$(SHLIBEXT) : $(SND_SDL_OBJS)
	@echo " * [SDL] ...linking";
	$(CC) $(CFLAGS) $(SHLIBLDFLAGS) -o $@ $(SND_SDL_OBJS)

#############################################################################
# MISC
#############################################################################

clean: clean-debug clean-release

cleanrenderer:
	$(MAKE) cleanrenderer2 BUILDDIR=$(BUILD_DEBUG_DIR)

cleanrenderer2:
	rm -f \
	$(REF_GL_OBJS) \
	$(REF_GLX_OBJS) \
	$(REF_SDL_OBJS) \
	$(SND_SDL_OBJS) \
	$(SND_OSS_OBJS) \
	$(SND_ALSA_OBJS) \
	$(BUILDDIR)/ref_sdl.$(SHLIBEXT) \
	$(BUILDDIR)/ref_glx.$(SHLIBEXT) \
	$(BUILDDIR)/snd_sdl.$(SHLIBEXT) \
	$(BUILDDIR)/snd_oss.$(SHLIBEXT) \
	$(BUILDDIR)/snd_alsa.$(SHLIBEXT)

clean-debug:
	$(MAKE) clean2 BUILDDIR=$(BUILD_DEBUG_DIR)

clean-release:
	$(MAKE) clean2 BUILDDIR=$(BUILD_RELEASE_DIR)

clean2:
	rm -f \
	$(TOOLS_OBJS) \
	$(QUAKE2_OBJS) \
	$(QUAKE2_AS_OBJS) \
	$(GAME_OBJS) \
	$(REF_GL_OBJS) \
	$(REF_GLX_OBJS) \
	$(REF_SDL_OBJS) \
	$(Q2DED_OBJS) \
	$(SND_SDL_OBJS) \
	$(SND_OSS_OBJS) \
	$(SND_ALSA_OBJS) \
	$(BUILDDIR)/ufo \
	$(BUILDDIR)/ufo2map \
	$(BUILDDIR)/game.$(SHLIBEXT) \
	$(BUILDDIR)/ref_sdl.$(SHLIBEXT) \
	$(BUILDDIR)/ref_glx.$(SHLIBEXT) \
	$(BUILDDIR)/snd_sdl.$(SHLIBEXT) \
	$(BUILDDIR)/snd_oss.$(SHLIBEXT) \
	$(BUILDDIR)/snd_alsa.$(SHLIBEXT) \
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
	  mkdir -p ../base/i18n/$$dir/LC_MESSAGES; \
	  msgfmt -v -o ../base/i18n/$$dir/LC_MESSAGES/ufoai.mo po/$$po; \
	done

docs:
	doxygen docs/doxyall

maps:
	cd ../base/maps; PATH=$(PATH):../../debugi386/; make

rsync:
	export RSYNC_PROXY=rsync-svn.sourceforge.net:80; rsync -a rsync-svn-u::svn/ufoai/* /var/lib/svn/