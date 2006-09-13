# =================================
# Linux-specific Configuration
# =================================

# =================================
# Configure extensions.
# =================================

EXT_SHLIB=.so
EXT_BIN=

# =================================
# Configure arch-specific *FLAGS.
# =================================

LDLIBS_OS=-ldl

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

# =================================
# Include generic stuff for UNIX-like systems.
# =================================

include $(src_ports)/$(dirname_unix)/Makefile

# =================================
# Directories
# =================================

mkdir_dir_list+=$(build_server)/$(dirname_linux)
mkdir_dir_list+=$(build_client)/$(dirname_linux)
mkdir_dir_list+=$(build_ports)/$(dirname_linux)

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

# This is defined purely for prettification, don't rely on it outside this file!
src_port_linux=$(src_ports)/$(dirname_linux)

$(build_client)/$(dirname_linux)/%.o:  $(src_port_linux)/%.c $(src_port_linux)/%.h $(HDR_COMMON) $(hdr_client)
	$(CC_CLIENT)

$(build_client)/$(dirname_linux)/%.o:  $(src_port_linux)/%.c
	$(CC_CLIENT)

$(build_server)/$(dirname_linux)/%.o: $(src_port_linux)/%.c $(src_port_linux)/%.h $(HDR_COMMON) $(hdr_server)
	$(CC_SERVER)

$(build_server)/$(dirname_linux)/%.o: $(src_port_linux)/%.c $(HDR_COMMON) $(hdr_server)
	$(CC_SERVER)

$(build_ports)/$(dirname_linux)/%.o: $(src_port_linux)/%.c $(src_port_linux)/%.h $(HDR_COMMON)
	$(CC_COMMON)
	
$(build_ports)/$(dirname_linux)/%.o: $(src_port_linux)/%.c $(HDR_COMMON)
	$(CC_COMMON)

# =================================
# Target-specific rules.
# =================================

