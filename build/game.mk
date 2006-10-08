GAME_SRCS=\
	game/q_shared.c \
	game/g_ai.c \
	game/g_client.c \
	game/g_cmds.c \
	game/g_main.c \
	game/g_spawn.c \
	game/g_svcmds.c \
	game/g_utils.c

GAME_OBJS=$(GAME_SRCS:%.c=$(_BUILDDIR)/game/%.o)
GAME_DEPS=$(GAME_OBJS:%.o=%.d)
GAME_TARGET=base/game.$(SHARED_EXT)

# Add the list of all project object files and dependencies
ALL_OBJS += $(GAME_OBJS)
ALL_DEPS += $(GAME_DEPS)
TARGETS +=$(GAME_TARGET)

# Say how about to build the target
$(GAME_TARGET) : $(GAME_OBJS) $(BUILDDIR)/.dirs
	@echo " * [GAM] ... linking"; \
		$(CC) $(LDFLAGS) $(SHARED_LDFLAGS) -o $@ $(GAME_OBJS) $(LIBS)

# Say how to build .o files from .c files for this module
$(BUILDDIR)/game/%.o: $(SRCDIR)/%.c $(BUILDDIR)/.dirs
	@echo " * [GAM] $<"; \
		$(CC) $(CFLAGS) $(SHARED_CFLAGS) -o $@ -c $<

# Say how to build the dependencies
ifdef BUILDDIR
$(BUILDDIR)/game/%.d: $(SRCDIR)/%.c $(BUILDDIR)/.dirs
	@echo " * [DEP] $<"; $(DEP)
endif


