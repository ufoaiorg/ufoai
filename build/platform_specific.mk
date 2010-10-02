#########################################################################################################################
# Special libs
#########################################################################################################################

# Defaults
SHARED_EXT=so
SHARED_LDFLAGS=-shared
SHARED_CFLAGS=-fPIC
JPEG_CFLAGS=
CFLAGS+=
LDFLAGS+=
CFLAGS_M_OPTS=-MD -MT $@ -MP
USEWINDOWSCMD=

# MinGW32
ifeq ($(TARGET_OS),mingw32)
	CLIENT_LIBS+=-lws2_32 -lwinmm -lopengl32
	SERVER_LIBS+=-lws2_32 -lwinmm
	ifeq ($(PROFILING),1)
		SERVER_LIBS += -lgmon
		CLIENT_LIBS += -lgmon
	endif
	RADIANT_LIBS+=-lglib-2.0 -lgtk-win32-2.0 -lgobject-2.0
	SHARED_EXT=dll
	SHARED_LDFLAGS=-shared
	SHARED_CFLAGS=-shared
	JPEG_CFLAGS=-DDONT_TYPEDEF_INT32
	CFLAGS+=-DGETTEXT_STATIC
	# Windows XP is the minimum we need
	CFLAGS+=-DWINVER=0x501
#	GAME_LIBS+=
	TOOLS_LIBS+=-lwinmm
	# To use Windows CMD
	ifeq ($(MSYS),1)
	else
		USEWINDOWSCMD=1
	endif
else
	LNKFLAGS+=-rdynamic
	ifeq ($(TARGET_OS),darwin)
		LDFLAGS+=-framework IOKit -framework Foundation -framework Cocoa
		RADIANT_LIBS+=-headerpad_max_install_names
	else
		CLIENT_LIBS+=-lGL
	endif
endif

# Linux like
ifeq ($(TARGET_OS),linux-gnu)
	CFLAGS+=-D_GNU_SOURCE -D_BSD_SOURCE -D_XOPEN_SOURCE
#	-Wunsafe-loop-optimizations
endif

# FreeBSD like
ifeq ($(TARGET_OS),freebsd)
	CFLAGS+=-D_BSD_SOURCE -D_XOPEN_SOURCE
	LDFLAGS+=-lexecinfo
endif

ifeq ($(TARGET_OS),netbsd)
	CFLAGS+=-D_BSD_SOURCE -D_NETBSD_SOURCE
endif

# Darwin
ifeq ($(TARGET_OS),darwin)
	SHARED_EXT=dylib
	SHARED_CFLAGS=-fPIC -fno-common
	SHARED_LDFLAGS=-dynamiclib
	CFLAGS+=-D_BSD_SOURCE -D_XOPEN_SOURCE
	SERVER_LIBS+=

	ifeq ($(TARGET_CPU),universal)
		CFLAGS_M_OPTS=
	endif
endif

# Solaris
ifeq ($(TARGET_OS),solaris)
	#TODO
	CFLAGS+=
	CLIENT_LIBS+=-lsocket -lnsl
	SERVER_LIBS+=-lsocket -lnsl
endif

#########################################################################################################################
# Release flags
#########################################################################################################################

RELEASE_CFLAGS=-ffast-math -funroll-loops -D_FORTIFY_SOURCE=2

CFLAGS+=-DSHARED_EXT=\"$(SHARED_EXT)\"

#ifeq ($(TARGET_CPU),axp)
#	RELEASE_CFLAGS+=-fomit-frame-pointer -fexpensive-optimizations
#endif

#ifeq ($(TARGET_CPU),sparc)
#	RELEASE_CFLAGS+=-fomit-frame-pointer -fexpensive-optimizations
#endif

ifeq ($(TARGET_CPU),powerpc64)
	RELEASE_CFLAGS+=-O2 -fomit-frame-pointer -fexpensive-optimizations
endif

ifeq ($(TARGET_CPU),powerpc)
	RELEASE_CFLAGS+=-O2 -fomit-frame-pointer -fexpensive-optimizations
endif

ifeq ($(TARGET_CPU),i386)
	ifeq ($(TARGET_OS),freebsd)
		RELEASE_CFLAGS+=-falign-loops=2 -falign-jumps=2 -falign-functions=2 -fno-strict-aliasing
	else
		RELEASE_CFLAGS+=-O2 -falign-loops=2 -falign-jumps=2 -falign-functions=2 -fno-strict-aliasing
	endif
endif

ifeq ($(TARGET_CPU),x86_64)
	ifeq ($(TARGET_OS),freebsd)
		RELEASE_CFLAGS+=-fomit-frame-pointer -fexpensive-optimizations -fno-strict-aliasing
	else
		RELEASE_CFLAGS+=-O2 -fomit-frame-pointer -fexpensive-optimizations -fno-strict-aliasing
	endif
endif
