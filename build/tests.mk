TESTS_CFLAGS+=-DCOMPILE_UFO -DCOMPILE_UNITTESTS -ggdb -O0
TESTS_LIBS+=-lcunit

TESTS_SRCS = \
	tests/test_all.c \
	tests/test_routing.c \
	tests/test_generic.c \
	tests/test_inventory.c \
	tests/test_rma.c \
	tests/test_shared.c \
	tests/test_ui.c \
	tests/test_campaign.c \
	tests/test_parser.c \
	\
	common/cmd.c \
	common/http.c \
	common/ioapi.c \
	common/unzip.c \
	common/bsp.c \
	common/grid.c \
	common/cmodel.c \
	common/common.c \
	common/cvar.c \
	common/files.c \
	common/list.c \
	common/md4.c \
	common/md5.c \
	common/mem.c \
	common/msg.c \
	common/dbuffer.c \
	common/net.c \
	common/netpack.c \
	common/pqueue.c \
	common/scripts.c \
	common/tracing.c \
	common/routing.c \
	\
	server/sv_ccmds.c \
	server/sv_init.c \
	server/sv_game.c \
	server/sv_main.c \
	server/sv_mapcyle.c \
	server/sv_rma.c \
	server/sv_send.c \
	server/sv_user.c \
	server/sv_world.c \
	server/sv_clientstub.c \
	\
	shared/byte.c \
	shared/infostring.c \
	shared/mathlib.c \
	shared/parse.c \
	shared/shared.c \
	shared/utf8.c \
	\
	game/q_shared.c \
	game/inv_shared.c \
	game/chr_shared.c \
	game/inventory.c \
	\
	client/ui/ui_timer.c

ifeq ($(HARD_LINKED_GAME),1)
	TESTS_SRCS+=$(GAME_SRCS)
endif

ifneq ($(findstring $(TARGET_OS), netbsd freebsd linux-gnu),)
	TESTS_SRCS += \
		ports/unix/unix_console.c \
		ports/unix/unix_curses.c \
		ports/unix/unix_main.c
endif

ifeq ($(TARGET_OS),mingw32)
	TESTS_SRCS+=\
		ports/windows/win_console.c \
		ports/windows/win_shared.c \
		ports/windows/ufo.rc
endif

ifeq ($(TARGET_OS),darwin)
	TESTS_SRCS+=\
		ports/unix/unix_console.c \
		ports/unix/unix_curses.c \
		ports/unix/unix_main.c
endif

ifeq ($(TARGET_OS),solaris)
	TESTS_SRCS += \
		ports/unix/unix_console.c \
		ports/unix/unix_main.c
endif

TESTS_OBJS= \
	$(patsubst %.c, $(BUILDDIR)/tests/%.o, $(filter %.c, $(TESTS_SRCS))) \
	$(patsubst %.m, $(BUILDDIR)/tests/%.o, $(filter %.m, $(TESTS_SRCS))) \
	$(patsubst %.rc, $(BUILDDIR)/tests/%.o, $(filter %.rc, $(TESTS_SRCS)))

TESTS_TARGET=testall$(EXE_EXT)

ifeq ($(BUILD_TESTS),1)
	ALL_OBJS+=$(TESTS_OBJS)
	TARGETS+=$(TESTS_TARGET)
endif

# Say how to link the exe
$(TESTS_TARGET): $(TESTS_OBJS)
	@echo " * [TEST] ... linking $(LNKFLAGS) ($(TESTS_LIBS) $(SERVER_LIBS) $(CLIENT_LIBS) $(SDL_LIBS))"; \
		$(CC) $(LDFLAGS) -o $@ $(TESTS_OBJS) $(LNKFLAGS) $(TESTS_LIBS) $(SDL_LIBS) $(CLIENT_LIBS) $(SERVER_LIBS)

# Say how to build .o files from .c files for this module
$(BUILDDIR)/tests/%.o: $(SRCDIR)/%.c
	@echo " * [TEST] $<"; \
		$(CC) $(CFLAGS) $(TESTS_CFLAGS) $(SDL_CFLAGS) -o $@ -c $< $(CFLAGS_M_OPTS)

# Say how to build .o files from .m files for this module
$(BUILDDIR)/tests/%.o: $(SRCDIR)/%.m
	@echo " * [TEST] $<"; \
		$(CC) $(CFLAGS) $(TESTS_CFLAGS) $(SDL_CFLAGS) -o $@ -c $< $(CFLAGS_M_OPTS)

ifeq ($(TARGET_OS),mingw32)
# Say how to build .o files from .rc files for this module
$(BUILDDIR)/tests/%.o: $(SRCDIR)/%.rc
	@echo " * [RC ] $<"; \
		$(WINDRES) -DCROSSBUILD -i $< -o $@
endif
