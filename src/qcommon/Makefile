# =================================
# Common Objects
# =================================

# =================================
# Directories
# =================================

build_common?=$(DIR_BUILD)/common
mkdir_dir_list+=$(build_common)

# =================================
# Headers
# =================================

HDR_COMMON+=$(src_common)/qcommon.h
HDR_COMMON+=$(src_common)/qfiles.h
HDR_COMMON+=$(src_common)/ufotypes.h

# =================================
# Objects
# =================================

obj_common+=$(build_common)/cmd.o
obj_common+=$(build_common)/cmodel.o
obj_common+=$(build_common)/crc.o
obj_common+=$(build_common)/cvar.o
obj_common+=$(build_common)/files.o
obj_common+=$(build_common)/md4.o
obj_common+=$(build_common)/net_chan.o
obj_common+=$(build_common)/scripts.o

obj_client+=$(build_client)/common.o
obj_server+=$(build_server)/common.o

# =================================
# Implicit Rules
# =================================

$(build_common)/%.o: $(src_common)/%.c $(src_common)/%.h $(HDR_COMMON)
	$(CC_COMMON)

$(build_common)/%.o: $(src_common)/%.c $(HDR_COMMON)
	$(CC_COMMON)
	
$(build_client)/%.o: $(src_common)/%.c $(src_common)/%.h $(HDR_COMMON)
	$(CC_CLIENT)

$(build_client)/%.o: $(src_common)/%.c $(HDR_COMMON)
	$(CC_CLIENT)

$(build_server)/%.o: $(src_common)/%.c $(src_common)/%.h $(HDR_COMMON)
	$(CC_SERVER)

$(build_server)/%.o: $(src_common)/%.c $(HDR_COMMON)
	$(CC_SERVER)

# =================================
# Target-specific rules.
# =================================
