CFLAGS += -ffast-math -funroll-loops -D_FORTIFY_SOURCE=2 -DNDEBUG

ifeq ($(filter -O0 -O1 -O2 -O3 -O4 -Os -Ofast,$(CFLAGS)),) # If you use multiple -O options, with or without level numbers, the last such option is the one that is effective
  CFLAGS += -O2
endif

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
