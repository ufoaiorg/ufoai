# Configure extensions.
EXT_SHLIB=.dll
EXT_BIN=.exe

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