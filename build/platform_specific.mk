
#########################################################################################################################
# Release flags
#########################################################################################################################

RELEASE_CFLAGS=-ffast-math -funroll-loops

#ifeq ($(TARGET_CPU),axp)
#	RELEASE_CFLAGS+=-fomit-frame-pointer -fexpensive-optimizations
#endif

#ifeq ($(TARGET_CPU),ppc)
#	RELEASE_CFLAGS+=-O2 -fomit-frame-pointer -fexpensive-optimizations
#endif

#ifeq ($(TARGET_CPU),sparc)
#	RELEASE_CFLAGS+=-fomit-frame-pointer -fexpensive-optimizations
#endif

ifeq ($(TARGET_CPU),i386)
#	ifeq ($(TARGET_OS),bsd)
#		RELEASE_CFLAGS+=-falign-loops=2 -falign-jumps=2 -falign-functions=2 -fno-strict-aliasing
#	else
		RELEASE_CFLAGS+=-O2 -falign-loops=2 -falign-jumps=2 -falign-functions=2 -fno-strict-aliasing
#	endif
endif

ifeq ($(TARGET_CPU),x86_64)
	RELEASE_CFLAGS+=-O2 -fomit-frame-pointer -fexpensive-optimizations -fno-strict-aliasing
endif

# FreeBSD only
#ifeq ($(TARGET_CPU),amd64)
#	RELEASE_CFLAGS+=-fomit-frame-pointer -fexpensive-optimizations -fno-strict-aliasing
#endif