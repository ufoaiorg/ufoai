ifeq ($(OSTYPE),Darwin)
	# Turn off about everything, until we get the support right...
	BUILD_SDLUFO=NO	# use SDL for cdrom, sound and maybe renderer (selectable)
	BUILD_GLX=NO		# X11 GLX driver
	BUILD_DEDICATED=YES	# build a dedicated ufo server
	BUILD_ALSA=NO		# build alsa sound driver
	BUILD_OSS=NO		# build oss sound driver
	BUILD_ARTS=NO		# build arts sound driver
	BUILD_WITH_SHADER=NO	# build with shader support
	HAVE_IPV6=NO		# Compile with IPv6 (protocol independent API). Tested on FreeBSD
	HAVE_GETTEXT=YES	# Compile with multilanguage-support?
	BUILD_WITH_DGA=NO	# dga support compiled in
	BUILD_WITH_VIDMODE=NO	# vidmode support
	BUILD_WITH_MMX=NO	# MMX support
	BUILD_FOR_PROFILING=NO # build with -pg option for profiling via gprof
	BUILD_COMPILETOOLS=YES	# build ufo2map bsp compile tool

	LDFLAGS=-L/sw/lib -ldl -lm -lvorbisfile -lvorbis -lintl -framework IOKit -framework AppKit -framework QuickTime -framework CoreAudio
	BASE_CFLAGS+=-I/sw/include
endif

# Configure extensions.
EXT_SHLIB=.so
EXT_BIN=

# Configure arch-specific *FLAGS.
ifeq ($(ARCH),axp)
  CFLAGS_ARCH_DEBUG=
  CFLAGS_ARCH_RELEASE=
endif

ifeq ($(ARCH),ppc)
  CFLAGS_ARCH_DEBUG=
  CFLAGS_ARCH_RELEASE=-O2
endif

ifeq ($(ARCH),sparc)
  CFLAGS_ARCH_DEBUG=
  CFLAGS_ARCH_RELEASE=
endif

ifeq ($(ARCH),i386)
  CFLAGS_ARCH_DEBUG=-DC_ONLY
  CFLAGS_ARCH_RELEASE=-O2 -falign-loops=2 -falign-jumps=2 -falign-functions=2 -fno-strict-aliasing -DC_ONLY
endif

ifeq ($(ARCH),x86_64)
  _LIB := 64
  CFLAGS_ARCH_DEBUG=
  CFLAGS_ARCH_RELEASE=-O2 -fno-strict-aliasing
endif

# Configure build targets and rules

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