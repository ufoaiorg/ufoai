GAME_SRCS=\
	game/q_shared.c \
	game/g_ai.c \
	game/g_client.c \
	game/g_combat.c \
	game/g_cmds.c \
	game/g_phys.c \
	game/g_main.c \
	game/g_spawn.c \
	game/g_svcmds.c \
	game/g_utils.c \
	game/inv_shared.c \
	shared/shared.c \
	shared/infostring.c

GAME_OBJS=$(GAME_SRCS:%.c=$(_BUILDDIR)/game/%.o)
GAME_TARGET=base/game.$(SHARED_EXT)

# Add the list of all project object files and dependencies
ALL_OBJS += $(GAME_OBJS)
TARGETS +=$(GAME_TARGET)

# Say how about to build the target
$(GAME_TARGET) : $(GAME_OBJS) $(BUILDDIR)/.dirs
	@echo " * [GAM] ... linking $(LNKFLAGS) ($(GAME_LIBS))"; \
		$(CC) $(LDFLAGS) $(SHARED_LDFLAGS) -o $@ $(GAME_OBJS) $(GAME_LIBS) $(LNKFLAGS)

# Say how to build .o files from .c files for this module
$(BUILDDIR)/game/%.o: $(SRCDIR)/%.c $(BUILDDIR)/.dirs
	@echo " * [GAM] $<"; \
		$(CC) $(CFLAGS) $(CPPFLAGS) $(SHARED_CFLAGS) -DNO_GETTEXT -o $@ -c $<
