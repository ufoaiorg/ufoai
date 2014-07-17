TARGET             := ufo

# if the linking should be static
$(TARGET)_STATIC   ?= $(STATIC)
ifeq ($($(TARGET)_STATIC),1)
$(TARGET)_LDFLAGS  += -static
endif

$(TARGET)_LINKER   := $(CXX)
$(TARGET)_FILE     := $(TARGET)$(EXE_EXT)
$(TARGET)_CFLAGS   += -DCOMPILE_UFO $(BFD_CFLAGS) $(SDL_CFLAGS) $(SDL_TTF_CFLAGS) $(SDL_MIXER_CFLAGS) $(CURL_CFLAGS) $(THEORA_CFLAGS) $(XVID_CFLAGS) $(VORBIS_CFLAGS) $(OGG_CFLAGS) $(MXML_CFLAGS) $(MUMBLE_CFLAGS) $(PNG_CFLAGS) $(JPEG_CFLAGS) $(LUA_CFLAGS)
$(TARGET)_LDFLAGS  += $(PNG_LIBS) $(JPEG_LIBS) $(BFD_LIBS) $(INTL_LIBS) $(SDL_TTF_LIBS) $(SDL_MIXER_LIBS) $(OPENGL_LIBS) $(SDL_LIBS) $(CURL_LIBS) $(THEORA_LIBS) $(XVID_LIBS) $(VORBIS_LIBS) $(OGG_LIBS) $(MXML_LIBS) $(MUMBLE_LIBS) $(SO_LIBS) $(LUA_LIBS) -lz

$(TARGET)_SRCS      = $(subst $(SRCDIR)/,, \
	$(wildcard $(SRCDIR)/client/*.cpp) \
	\
	$(wildcard $(SRCDIR)/client/input/*.cpp) \
	\
	$(wildcard $(SRCDIR)/client/cinematic/*.cpp) \
	\
	$(wildcard $(SRCDIR)/client/battlescape/*.cpp) \
	\
	$(wildcard $(SRCDIR)/client/battlescape/events/*.cpp) \
	$(wildcard $(SRCDIR)/client/battlescape/events/event/actor/*.cpp) \
	$(wildcard $(SRCDIR)/client/battlescape/events/event/inventory/*.cpp) \
	$(wildcard $(SRCDIR)/client/battlescape/events/event/player/*.cpp) \
	$(wildcard $(SRCDIR)/client/battlescape/events/event/world/*.cpp) \
	\
	$(wildcard $(SRCDIR)/client/sound/*.cpp) \
	\
	$(wildcard $(SRCDIR)/client/cgame/*.cpp) \
	\
	$(wildcard $(SRCDIR)/client/web/*.cpp) \
	\
	$(wildcard $(SRCDIR)/client/ui/*.cpp) \
	\
	$(wildcard $(SRCDIR)/client/ui/node/*.cpp) \
	\
	$(wildcard $(SRCDIR)/client/renderer/*.cpp) \
	\
	common/binaryexpressionparser.cpp \
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
	common/net.cpp \
	common/netpack.cpp \
	common/dbuffer.cpp \
	common/pqueue.cpp \
	common/scripts.cpp \
	common/sha1.cpp \
	common/sha2.cpp \
	common/tracing.cpp \
	common/routing.cpp \
	common/xml.cpp \
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
	\
	shared/bfd.cpp \
	shared/byte.cpp \
	shared/mathlib.cpp \
	shared/mathlib_extra.cpp \
	shared/aabb.cpp \
	shared/utf8.cpp \
	shared/images.cpp \
	shared/stringhunk.cpp \
	shared/infostring.cpp \
	shared/parse.cpp \
	shared/shared.cpp \
	\
	game/q_shared.cpp \
	game/chr_shared.cpp \
	game/inv_shared.cpp \
	game/inventory.cpp ) \
	\
	$(MXML_SRCS)\
	\
	$(MUMBLE_SRCS) \
	\
	$(SDL_MIXER_SRCS) \
	\
	$(SDL_TTF_SRCS) \
	\
	$(PNG_SRCS) \
	\
	$(CURL_SRCS) \
	\
	$(JPEG_SRCS)

ifneq ($(findstring $(TARGET_OS), openbsd netbsd freebsd linux),)
	$(TARGET)_SRCS += \
		ports/linux/linux_main.cpp \
		ports/unix/unix_console.cpp \
		ports/unix/unix_files.cpp \
		ports/unix/unix_shared.cpp \
		ports/unix/unix_main.cpp
	$(TARGET)_LDFLAGS +=
endif

ifneq ($(findstring $(TARGET_OS), mingw32 mingw64),)
	$(TARGET)_SRCS +=\
		ports/windows/win_backtrace.cpp \
		ports/windows/win_console.cpp \
		ports/windows/win_shared.cpp \
		ports/windows/win_main.cpp \
		ports/windows/ufo.rc
	$(TARGET)_LDFLAGS +=
endif

ifeq ($(TARGET_OS),darwin)
	$(TARGET)_SRCS += \
		ports/macosx/osx_main.cpp \
		ports/macosx/osx_shared.cpp \
		ports/unix/unix_console.cpp \
		ports/unix/unix_files.cpp \
		ports/unix/unix_shared.cpp \
		ports/unix/unix_main.cpp
	$(TARGET)_LDFLAGS +=
endif

ifeq ($(TARGET_OS),solaris)
	$(TARGET)_SRCS += \
		ports/solaris/solaris_main.cpp \
		ports/unix/unix_console.cpp \
		ports/unix/unix_files.cpp \
		ports/unix/unix_shared.cpp \
		ports/unix/unix_main.cpp
	$(TARGET)_LDFLAGS +=
endif

ifeq ($(TARGET_OS),android)
	$(TARGET)_SRCS += \
		ports/android/android_console.cpp \
		ports/android/android_debugger.cpp \
		ports/android/android_main.cpp \
		ports/android/android_system.cpp \
		ports/unix/unix_shared.cpp \
		ports/unix/unix_files.cpp
endif

ifeq ($(HARD_LINKED_GAME),1)
	$(TARGET)_SRCS     += $(game_SRCS)
else
	$(TARGET)_DEPS     := game
endif

ifeq ($(HARD_LINKED_CGAME),1)
	$(TARGET)_SRCS     += \
		$(cgame-campaign_SRCS) \
		$(cgame-skirmish_SRCS) \
		$(cgame-multiplayer_SRCS)
else
	$(TARGET)_DEPS     := \
		cgame-campaign \
		cgame-skirmish \
		cgame-multiplayer
endif

$(TARGET)_OBJS     := $(call ASSEMBLE_OBJECTS,$(TARGET))
$(TARGET)_CXXFLAGS := $($(TARGET)_CFLAGS)
$(TARGET)_CCFLAGS  := $($(TARGET)_CFLAGS)
