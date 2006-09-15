# =================================
# Common Objects
# =================================

# =================================
# Directories
# =================================

build_server?=$(DIR_BUILD)/server
mkdir_dir_list+=$(build_server)

# =================================
# Headers
# =================================

hdr_server+=server.h

# =================================
# Objects
# =================================

obj_common+=$(build_common)/sv_ccmds.o
obj_common+=$(build_common)/sv_game.o
obj_common+=$(build_common)/sv_init.o
obj_common+=$(build_common)/sv_main.o
# obj_common+=$(build_server)/sv_null.o # This object is not currently required.
obj_common+=$(build_common)/sv_send.o
obj_common+=$(build_common)/sv_user.o
obj_common+=$(build_common)/sv_world.o

# This is very hacky, but necessary for bad programming! (Damn you id!!!!)
obj_system_server+=$(build_server)/cl_null.o
obj_system_server+=$(build_server)/cd_null.o



# =================================
# Implicit Rules
# =================================

$(build_server)/%.o: $(src_server)/%.c $(src_server)/%.h $(HDR_COMMON) $(hdr_server)
	$(CC_SERVER)

$(build_server)/%.o: $(src_server)/%.c
	$(CC_SERVER)

$(build_common)/%.o: $(src_server)/%.c $(src_server)/%.h $(HDR_COMMON) $(hdr_server)
	$(CC_COMMON)

$(build_common)/%.o: $(src_server)/%.c
	$(CC_COMMON)

# =================================
# Target-specific rules.
# =================================

$(build_server)/cl_null.o: $(src_ports)/$(dirname_null)/cl_null.c
	$(CC_SERVER)
	
$(build_server)/cd_null.o: $(src_ports)/$(dirname_null)/cd_null.c
	$(CC_SERVER)

.PHONY: server

server: mkdirs $(target_server)

$(target_server): $(obj_system) $(obj_system_server) $(obj_common) $(obj_server) $(obj_server_main)
	@echo " * [LINK]   $@"; $(CC) $(CFLAGS) $(LDFLAGS) $(LDLIBS) $(obj_system) $(obj_system_server) $(obj_common) $(obj_server) $(obj_server_main) -o $(target_server)
