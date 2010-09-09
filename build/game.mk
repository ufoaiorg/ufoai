GAME_CFLAGS+=-DCOMPILE_UFO

# Lua apicheck adds asserts to make sure stack is sane
ifeq ($(BUILD_DEBUG),1)
	CFLAGS+=-DLUA_USE_APICHECK
endif

GAME_SRCS=\
	game/g_actor.c \
	game/g_ai.c \
	game/g_ai_lua.c \
	game/g_client.c \
	game/g_combat.c \
	game/g_cmds.c \
	game/g_edicts.c \
	game/g_events.c \
	game/g_func.c \
	game/g_inventory.c \
	game/g_main.c \
	game/g_match.c \
	game/g_mission.c \
	game/g_morale.c \
	game/g_move.c \
	game/g_phys.c \
	game/g_reaction.c \
	game/g_round.c \
	game/g_stats.c \
	game/g_spawn.c \
	game/g_svcmds.c \
	game/g_trigger.c \
	game/g_utils.c \
	game/g_vis.c \
	\
	game/lua/lapi.c \
	game/lua/lauxlib.c \
	game/lua/lbaselib.c \
	game/lua/lcode.c \
	game/lua/ldblib.c \
	game/lua/ldebug.c \
	game/lua/ldo.c \
	game/lua/ldump.c \
	game/lua/lfunc.c \
	game/lua/lgc.c \
	game/lua/linit.c \
	game/lua/liolib.c \
	game/lua/llex.c \
	game/lua/lmathlib.c \
	game/lua/lmem.c \
	game/lua/loadlib.c \
	game/lua/lobject.c \
	game/lua/lopcodes.c \
	game/lua/loslib.c \
	game/lua/lparser.c \
	game/lua/lstate.c \
	game/lua/lstring.c \
	game/lua/lstrlib.c \
	game/lua/ltable.c \
	game/lua/ltablib.c \
	game/lua/ltm.c \
	game/lua/lundump.c \
	game/lua/lvm.c \
	game/lua/lzio.c \
	game/lua/print.c

ifneq ($(HARD_LINKED_GAME),1)
	GAME_SRCS+= \
		shared/mathlib.c \
		shared/shared.c \
		shared/utf8.c \
		shared/parse.c \
		shared/infostring.c \
		\
		game/q_shared.c \
		game/chr_shared.c \
		game/inv_shared.c \
		game/inventory.c
endif

GAME_OBJS=$(GAME_SRCS:%.c=$(_BUILDDIR)/game/%.o)
GAME_TARGET=base/game.$(SHARED_EXT)

ifneq ($(HARD_LINKED_GAME),1)
	# Add the list of all project object files and dependencies
	ALL_OBJS += $(GAME_OBJS)
	TARGETS +=$(GAME_TARGET)
endif

# Say how about to build the target
$(GAME_TARGET) : dirs $(GAME_OBJS)
	@echo " * [GAM] ... linking $(LNKFLAGS) ($(GAME_LIBS))"; \
		$(CC) $(LDFLAGS) $(SHARED_LDFLAGS) -o $@ $(GAME_OBJS) $(GAME_LIBS) $(LNKFLAGS)

# Say how to build .o files from .c files for this module
$(BUILDDIR)/game/%.o: $(SRCDIR)/%.c
	@echo " * [GAM] $<"; \
		$(CC) $(CFLAGS) $(SHARED_CFLAGS) $(GAME_CFLAGS) -o $@ -c $< $(CFLAGS_M_OPTS)
