# =================================
# Generic files for UNIX-like systems.
# =================================

# =================================
# Directories
# =================================

mkdir_dir_list+=$(build_server)/$(dirname_unix)
mkdir_dir_list+=$(build_client)/$(dirname_unix)
mkdir_dir_list+=$(build_ports)/$(dirname_unix)

# =================================
# Objects
# =================================

# Generic
obj_system+=$(build_ports)/$(dirname_unix)/glob.o

# Client specific

obj_system_client+=$(build_client)/$(dirname_unix)/$(NET_UDP).o
ifeq ($(strip $(BUILD_SDLUFO)),YES)
	obj_system_client+=$(build_client)/$(dirname_unix)/cd_sdl.o
endif

# Server specific
obj_system_server+=$(build_server)/$(dirname_linux)/$(NET_UDP).o

# =================================
# Implicit Rules
# =================================

# This is defined purely for prettification, don't rely on it outside this file!
$(src_port_unix)=$(src_ports)/$(dirname_unix)

$(build_client)/$(dirname_unix)/%.o: $(src_port_unix)/%.c $(src_port_unix)/%.h $(HDR_COMMON) $(hdr_client)
	$(CC_CLIENT)

$(build_client)/$(dirname_unix)/%.o: $(src_port_unix)/%.c $(HDR_COMMON) $(hdr_client)
	$(CC_CLIENT)
	
$(build_server)/$(dirname_unix)/%.o: $(src_port_unix)/%.c $(src_port_unix)/%.h $(HDR_COMMON) $(hdr_server)
	$(CC_SERVER)

$(build_server)/$(dirname_unix)/%.o:$(src_port_unix)/%.c $(HDR_COMMON) $(hdr_server)
	$(CC_SERVER)
	
$(build_ports)/$(dirname_unix)/%.o: $(src_port_unix)/%.c $(src_port_unix)/%.h $(HDR_COMMON)
	$(CC_COMMON)
	
$(build_ports)/$(dirname_unix)/%.o: $(src_port_unix)/%.c $(HDR_COMMON)
	$(CC_COMMON)

# =================================
# Target-specific rules.
# =================================
