#FIXME: check for -ldl (mingw doesn't have this)
DEDICATED_CFLAGS+=-DCOMPILE_UFO -DDEDICATED_ONLY

SERVER_SRCS += \
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
	server/sv_game.c \
	server/sv_init.c \
	server/sv_main.c \
	server/sv_mapcycle.c \
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
	game/chr_shared.c

ifeq ($(HARD_LINKED_GAME),1)
	SERVER_SRCS+=$(GAME_SRCS) \
		game/inventory.c
	DEDICATED_CFLAGS+=$(GAME_CFLAGS)
endif

ifneq ($(findstring $(TARGET_OS), netbsd freebsd linux-gnu),)
	SERVER_SRCS += \
		ports/linux/linux_main.c \
		ports/unix/unix_console.c \
		ports/unix/unix_main.c
endif

ifeq ($(TARGET_OS),mingw32)
	SERVER_SRCS+=\
		ports/windows/win_console.c \
		ports/windows/win_shared.c \
		ports/windows/win_main.c \
		ports/windows/ufo.rc
endif

ifeq ($(TARGET_OS),darwin)
	SERVER_SRCS+=\
		ports/macosx/osx_main.m \
		ports/unix/unix_console.c \
		ports/unix/unix_main.c
endif

ifeq ($(TARGET_OS),solaris)
	SERVER_SRCS += \
		ports/solaris/solaris_main.c \
		ports/unix/unix_console.c \
		ports/unix/unix_main.c
endif

SERVER_OBJS= \
	$(patsubst %.c, $(BUILDDIR)/server/%.o, $(filter %.c, $(SERVER_SRCS))) \
	$(patsubst %.m, $(BUILDDIR)/server/%.o, $(filter %.m, $(SERVER_SRCS))) \
	$(patsubst %.rc, $(BUILDDIR)/server/%.o, $(filter %.rc, $(SERVER_SRCS)))

SERVER_TARGET=ufoded$(EXE_EXT)

ifeq ($(BUILD_DEDICATED),1)
	ALL_OBJS+=$(SERVER_OBJS)
	TARGETS+=$(SERVER_TARGET)
endif

# Say how to link the exe
$(SERVER_TARGET): dirs $(SERVER_OBJS)
	@echo " * [DED] ... linking $(LDFLAGS) ($(SERVER_LIBS) $(SDL_LIBS))"; \
		$(CC) $(LDFLAGS) -o $@ $(SERVER_OBJS) $(SERVER_LIBS) $(SDL_LIBS)

# Say how to build .o files from .c files for this module
$(BUILDDIR)/server/%.o: $(SRCDIR)/%.c
	@echo " * [DED] $<"; \
		$(CC) $(CFLAGS) $(DEDICATED_CFLAGS) $(SDL_CFLAGS) -o $@ -c $< $(CFLAGS_M_OPTS)

ifeq ($(TARGET_OS),mingw32)
# Say how to build .o files from .rc files for this module
$(BUILDDIR)/server/%.o: $(SRCDIR)/%.rc
	@echo " * [RC ] $<"; \
		$(WINDRES) -DCROSSBUILD -i $< -o $@
endif

# Say how to build .o files from .m files for this module
$(BUILDDIR)/server/%.o: $(SRCDIR)/%.m
	@echo " * [DED] $<"; \
		$(CC) $(CFLAGS) $(DEDICATED_CFLAGS) $(SDL_CFLAGS) -o $@ -c $< $(CFLAGS_M_OPTS)
