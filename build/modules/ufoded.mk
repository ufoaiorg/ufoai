TARGET             := ufoded

# if the linking should be static
$(TARGET)_STATIC   ?= $(STATIC)
ifeq ($($(TARGET)_STATIC),1)
$(TARGET)_LDFLAGS  += -static
endif

$(TARGET)_LINKER   := $(CC)
$(TARGET)_FILE     := $(TARGET)$(EXE_EXT)
$(TARGET)_LDFLAGS  += $(BFD_LIBS) $(INTL_LIBS) $(SDL_LIBS) $(CURL_LIBS) $(SO_LIBS) -lz -lm
$(TARGET)_CFLAGS   += -DCOMPILE_UFO -DDEDICATED_ONLY $(BFD_CFLAGS) $(INTL_CFLAGS) $(SDL_CFLAGS) $(CURL_CFLAGS)

$(TARGET)_SRCS      = \
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
	server/sv_log.c \
	server/sv_main.c \
	server/sv_mapcycle.c \
	server/sv_rma.c \
	server/sv_send.c \
	server/sv_user.c \
	server/sv_world.c \
	server/sv_clientstub.c \
	\
	shared/bfd.c \
	shared/byte.c \
	shared/stringhunk.c \
	shared/infostring.c \
	shared/mathlib.c \
	shared/mutex.c \
	shared/parse.c \
	shared/shared.c \
	shared/utf8.c \
	\
	game/q_shared.c \
	game/inv_shared.c \
	game/chr_shared.c

ifneq ($(findstring $(TARGET_OS), netbsd freebsd linux),)
	$(TARGET)_SRCS += \
		ports/linux/linux_main.c \
		ports/unix/unix_console.c \
		ports/unix/unix_files.c \
		ports/unix/unix_shared.c \
		ports/unix/unix_main.c
endif

ifeq ($(TARGET_OS),mingw32)
	$(TARGET)_SRCS += \
		ports/windows/win_backtrace.c \
		ports/windows/win_console.c \
		ports/windows/win_shared.c \
		ports/windows/win_main.c \
		ports/windows/ufo.rc
endif

ifeq ($(TARGET_OS),darwin)
	$(TARGET)_SRCS += \
		ports/macosx/osx_main.m \
		ports/unix/unix_console.c \
		ports/unix/unix_files.c \
		ports/unix/unix_shared.c \
		ports/unix/unix_main.c
endif

ifeq ($(TARGET_OS),solaris)
	$(TARGET)_SRCS += \
		ports/solaris/solaris_main.c \
		ports/unix/unix_console.c \
		ports/unix/unix_files.c \
		ports/unix/unix_shared.c \
		ports/unix/unix_main.c
endif

ifeq ($(HARD_LINKED_GAME),1)
	$(TARGET)_SRCS     += $(game_SRCS) \
		game/inventory.c
else
	$(TARGET)_DEPS     := game
endif

$(TARGET)_OBJS     := $(call ASSEMBLE_OBJECTS,$(TARGET))
$(TARGET)_CXXFLAGS := $($(TARGET)_CFLAGS)
$(TARGET)_CCFLAGS  := $($(TARGET)_CFLAGS)
