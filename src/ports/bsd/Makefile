# =================================
# *BSD Specific Configuration
# =================================

# =================================
# Configure extensions.
# =================================

EXT_SHLIB=.so
EXT_BIN=

# =================================
# Configure arch-specific *FLAGS.
# =================================

ifeq ($(OSTYPE),FreeBSD)
	LDFLAGS_OS=-lintl
	BUILD_ALSA=NO
endif

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

# FreeBSD specific.
ifeq ($(ARCH),amd64)
  CFLAGS_ARCH_DEBUG=
  CFLAGS_ARCH_RELEASE=
endif

# =================================
# Include generic stuff for UNIX-like systems.
# =================================

include $(src_ports)/$(dirname_unix)/Makefile

# =================================
# Objects
# =================================

# Client main
obj_client_main=$(build_client)/$(dirname_linux)/sys_linux.o

# Server main
obj_server_main=$(build_server)/$(dirname_linux)/sys_linux.o

# Generic.
obj_system+=$(build_ports)/$(dirname_linux)/q_shlinux.o

# Client specific.
obj_system_client+=$(build_client)/$(dirname_linux)/vid_so.o
ifneq ($(strip $(BUILD_SDLUFO)),YES)
  obj_system_client+=$(build_client)/$(dirname_linux)/cd_linux.o
endif

# Server specific.
obj_system_server+=

# =================================
# Implicit Rules
# =================================

$(build_client)/$(dirname_linux)/%.o: %.c %.h $(HDR_COMMON) $(hdr_client)
	$(CC_CLIENT)

$(build_server)/$(dirname_linux)/%.o: %.c %.h $(HDR_COMMON) $(hdr_server)
	$(CC_SERVER)
	
$(build_ports)/$(dirname_linux)/%.o: %.c %.h $(HDR_COMMON)
	$(CC_COMMON)

# =================================
# Target-specific Rules
# =================================
