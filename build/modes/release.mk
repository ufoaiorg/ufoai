CFLAGS += -O2 -ffast-math -funroll-loops -D_FORTIFY_SOURCE=2 -DNDEBUG

ifeq ($(TARGET_ARCH),powerpc64)
	CFLAGS += -fomit-frame-pointer -fexpensive-optimizations
endif

ifeq ($(TARGET_ARCH),powerpc)
	CFLAGS += -fomit-frame-pointer -fexpensive-optimizations
endif

ifeq ($(TARGET_ARCH),i386)
	CFLAGS += -falign-loops=2 -falign-jumps=2 -falign-functions=2 -fno-strict-aliasing
endif

ifeq ($(TARGET_ARCH),x86_64)
	CFLAGS += -fomit-frame-pointer -fexpensive-optimizations -fno-strict-aliasing
endif
