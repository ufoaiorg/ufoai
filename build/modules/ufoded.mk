TARGET             := ufoded

# if the linking should be static
$(TARGET)_STATIC   ?= $(STATIC)
ifeq ($($(TARGET)_STATIC),1)
$(TARGET)_LDFLAGS  += -static
endif

$(TARGET)_LINKER   := $(CXX)
$(TARGET)_FILE     := $(TARGET)$(EXE_EXT)
$(TARGET)_CFLAGS   += -DCOMPILE_UFO -DDEDICATED_ONLY $(BFD_CFLAGS) $(INTL_CFLAGS) $(SDL_CFLAGS) $(CURL_CFLAGS) $(LUA_CFLAGS)
$(TARGET)_LDFLAGS  += $(BFD_LIBS) $(INTL_LIBS) $(SDL_LIBS) $(CURL_LIBS) $(SO_LIBS) $(LUA_LIBS) -lz -lm

$(TARGET)_SRCS      = \
	common/cmd.cpp \
	common/http.cpp \
	common/ioapi.cpp \
	common/unzip.cpp \
	common/bsp.cpp \
	common/grid.cpp \
	common/cmodel.cpp \
	common/common.cpp \
	common/cvar.cpp \
	common/files.cpp \
	common/list.cpp \
	common/md4.cpp \
	common/md5.cpp \
	common/mem.cpp \
	common/msg.cpp \
	common/dbuffer.cpp \
	common/net.cpp \
	common/netpack.cpp \
	common/pqueue.cpp \
	common/scripts.cpp \
	common/sha1.cpp \
	common/sha2.cpp \
	common/tracing.cpp \
	common/routing.cpp \
	\
	server/sv_ccmds.cpp \
	server/sv_game.cpp \
	server/sv_init.cpp \
	server/sv_log.cpp \
	server/sv_main.cpp \
	server/sv_mapcycle.cpp \
	server/sv_rma.cpp \
	server/sv_send.cpp \
	server/sv_user.cpp \
	server/sv_world.cpp \
	server/sv_clientstub.cpp \
	\
	shared/bfd.cpp \
	shared/byte.cpp \
	shared/stringhunk.cpp \
	shared/infostring.cpp \
	shared/mathlib.cpp \
	shared/aabb.cpp \
	shared/parse.cpp \
	shared/shared.cpp \
	shared/utf8.cpp \
	\
	game/q_shared.cpp \
	game/inv_shared.cpp \
	game/chr_shared.cpp \
	\
	$(CURL_SRCS)

ifneq ($(findstring $(TARGET_OS), openbsd netbsd freebsd linux),)
	$(TARGET)_SRCS += \
		ports/linux/linux_main.cpp \
		ports/unix/unix_console.cpp \
		ports/unix/unix_files.cpp \
		ports/unix/unix_shared.cpp \
		ports/unix/unix_main.cpp
endif

ifneq ($(findstring $(TARGET_OS), mingw32 mingw64),)
	$(TARGET)_SRCS += \
		ports/windows/win_backtrace.cpp \
		ports/windows/win_console.cpp \
		ports/windows/win_shared.cpp \
		ports/windows/win_main.cpp \
		ports/windows/ufo.rc
endif

ifeq ($(TARGET_OS),darwin)
	$(TARGET)_SRCS += \
		ports/macosx/osx_main.cpp \
		ports/macosx/osx_shared.cpp \
		ports/unix/unix_console.cpp \
		ports/unix/unix_files.cpp \
		ports/unix/unix_shared.cpp \
		ports/unix/unix_main.cpp
endif

ifeq ($(TARGET_OS),solaris)
	$(TARGET)_SRCS += \
		ports/solaris/solaris_main.cpp \
		ports/unix/unix_console.cpp \
		ports/unix/unix_files.cpp \
		ports/unix/unix_shared.cpp \
		ports/unix/unix_main.cpp
endif

ifeq ($(HARD_LINKED_GAME),1)
	$(TARGET)_SRCS     += $(game_SRCS) \
		game/inventory.cpp
else
	$(TARGET)_DEPS     := game
endif

$(TARGET)_OBJS     := $(call ASSEMBLE_OBJECTS,$(TARGET))
$(TARGET)_CXXFLAGS := $($(TARGET)_CFLAGS)
$(TARGET)_CCFLAGS  := $($(TARGET)_CFLAGS)
