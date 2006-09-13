# =================================
# Game objects
# =================================

# =================================
# Configure build directory
# =================================

build_game?=$(BUILDDIR)/game
mkdir_dir_list+=$(build_game)

# =================================
# Configure common game headers
# =================================

hdr_game+=$(src_game)/g_local.h
hdr_game+=$(src_game)/game.h

# =================================
# Objects
# =================================

obj_game+=$(build_game)/g_client.o
obj_game+=$(build_game)/g_cmds.o
obj_game+=$(build_game)/g_main.o
obj_game+=$(build_game)/g_spawn.o
obj_game+=$(build_game)/g_svcmds.o
obj_game+=$(build_game)/g_utils.o

obj_common+=$(build_common)/q_shared.o

# =================================
# Implicit Rules
# =================================

$(build_game)/%.o: $(src_game)/%.c $(src_game)/%.h $(HDR_COMMON) $(hdr_game)
	$(CC_SHLIB)

$(build_game)/%.o: $(src_game)/%.c $(HDR_COMMON) $(hdr_game)
	$(CC_SHLIB)

$(build_common)/%.o: $(src_game)/%.c $(src_game)/%.h $(HDR_COMMON)
	$(CC_COMMON)

$(build_common)/%.o: $(src_game)/%.c $(HDR_COMMON)
	$(CC_COMMON)

# =================================
# Target-specific Rules
# =================================

$(target_game): $(obj_game)
	$(CC) $(CFLAGS) $(LDFLAGS_SHLIB) $(LDLIBS) -o $@ $(GAME_OBJS)
