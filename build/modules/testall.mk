TARGET             := testall

# if the linking should be static
$(TARGET)_STATIC   ?= $(STATIC)
ifeq ($($(TARGET)_STATIC),1)
$(TARGET)_LDFLAGS  += -static
endif

$(TARGET)_LINKER   := $(CXX)
$(TARGET)_FILE     := $(TARGET)$(EXE_EXT)
$(TARGET)_CFLAGS   += -DCOMPILE_UFO -DHARD_LINKED_GAME -DHARD_LINKED_CGAME -DCOMPILE_UNITTESTS $(GTEST_CFLAGS) $(BFD_CFLAGS) $(SDL_CFLAGS) $(CURL_CFLAGS) $(OGG_CFLAGS) $(MXML_CFLAGS) $(MUMBLE_CFLAGS) $(PNG_CFLAGS) $(JPEG_CFLAGS) $(SDL_MIXER_CFLAGS) $(SDL_TTF_CFLAGS) $(LUA_CFLAGS)
$(TARGET)_LDFLAGS  += $(GTEST_LIBS) $(PNG_LIBS) $(JPEG_LIBS) $(BFD_LIBS) $(INTL_LIBS) $(SDL_TTF_LIBS) $(SDL_MIXER_LIBS) $(OPENGL_LIBS) $(SDL_LIBS) $(CURL_LIBS) $(THEORA_LIBS) $(XVID_LIBS) $(VORBIS_LIBS) $(OGG_LIBS) $(MXML_LIBS) $(MUMBLE_LIBS) $(SO_LIBS) $(LUA_LIBS) -lz

ifneq ($(findstring $(TARGET_OS), openbsd netbsd freebsd),)
    $(TARGET)_LDFLAGS += -lexecinfo
endif

$(TARGET)_SRCS_TMP      = $(subst $(SRCDIR)/,, \
	$(wildcard $(SRCDIR)/tests/*.cpp) \
	\
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
	client/ui/swig/ui_lua_shared.cpp \
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
	common/hashtable.cpp \
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
	common/scripts_lua.cpp \
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
	shared/mathlib_extra.cpp \
	shared/bfd.cpp \
	shared/entitiesdef.cpp \
	shared/stringhunk.cpp \
	shared/byte.cpp \
	shared/images.cpp \
	shared/mathlib.cpp \
	shared/aabb.cpp \
	shared/shared.cpp \
	shared/utf8.cpp \
	shared/parse.cpp \
	shared/infostring.cpp \
	\
	game/q_shared.cpp \
	game/chr_shared.cpp \
	game/inv_shared.cpp \
	game/inventory.cpp ) \
	\
	$(game_SRCS) \
	\
	$(MXML_SRCS) \
	\
	$(MUMBLE_SRCS) \
	\
	$(PNG_SRCS) \
	\
	$(GTEST_SRCS) \
	\
	$(LUA_SRCS)

ifneq ($(findstring $(TARGET_OS), mingw32 mingw64),)
	$(TARGET)_SRCS_TMP += \
		ports/windows/win_backtrace.cpp \
		ports/windows/win_console.cpp \
		ports/windows/win_shared.cpp \
		ports/windows/ufo.rc
else
	$(TARGET)_SRCS_TMP += \
		ports/unix/unix_console.cpp \
		ports/unix/unix_files.cpp \
		ports/unix/unix_shared.cpp \
		ports/unix/unix_main.cpp
endif

$(TARGET)_SRCS_TMP     += \
	$(cgame-campaign_SRCS) \
	$(cgame-skirmish_SRCS) \
	$(cgame-multiplayer_SRCS) \
	$(cgame-staticcampaign_SRCS)

$(TARGET)_SRCS = $(sort $($(TARGET)_SRCS_TMP))

TESTMAPSRC := $(wildcard unittest/maps/*.map)
TESTBSPS := $(TESTMAPSRC:.map=.bsp)
$(TESTBSPS): %.bsp: %.map
	./$(UFO2MAP)$(EXE_EXT) -gamedir unittest $(UFO2MAPFLAGS) $(<:unittest/%=%)

$(TARGET)_OBJS     := $(call ASSEMBLE_OBJECTS,$(TARGET))
$(TARGET)_CXXFLAGS := $($(TARGET)_CFLAGS)
$(TARGET)_CCFLAGS  := $($(TARGET)_CFLAGS)

testall: ufo2map unittest/maps/test_routing.bsp unittest/maps/test_game.bsp
