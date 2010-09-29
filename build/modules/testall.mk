TARGET             := testall

$(TARGET)_FILE     := $(TARGET)$(EXE_EXT)
$(TARGET)_CFLAGS   += -DCOMPILE_UFO -DCOMPILE_UNITTESTS $(SDL_CFLAGS) $(CURL_CFLAGS)
$(TARGET)_LDFLAGS  += -lcunit $(INTL_LIBS) $(SDL_LIBS) $(CURL_LIBS) -lz
$(TARGET)_SRCS      = \
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
	shared/threads.c \
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

ifeq ($(TARGET_OS),mingw32)
	$(TARGET)_SRCS += \
		ports/windows/win_console.c \
		ports/windows/win_shared.c \
		ports/windows/ufo.rc
else
	$(TARGET)_SRCS += \
		ports/unix/unix_console.c \
		ports/unix/unix_main.c
endif

ifeq ($(HARD_LINKED_GAME),1)
	$(TARGET)_SRCS     += $(game_SRCS)
endif

ifneq ($(HAVE_CUNIT_BASIC_H), 1)
	$(TARGET)_IGNORE := yes
endif

$(TARGET)_OBJS     := $(call ASSEMBLE_OBJECTS,$(TARGET))
$(TARGET)_CXXFLAGS := $($(TARGET)_CFLAGS)
$(TARGET)_CCFLAGS  := $($(TARGET)_CFLAGS)
