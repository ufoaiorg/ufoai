TARGET             := game

# if the linking should be static
$(TARGET)_STATIC   ?= $(STATIC)
ifeq ($($(TARGET)_STATIC),1)
$(TARGET)_LDFLAGS  += -static
endif

$(TARGET)_LINKER   := $(CXX)
$(TARGET)_CFLAGS   += -DCOMPILE_UFO $(SO_CFLAGS) $(SDL_CFLAGS)
$(TARGET)_LDFLAGS  += $(SO_LDFLAGS) -lm $(SO_LIBS)
$(TARGET)_FILE     := base/$(TARGET).$(SO_EXT)

# Lua apicheck adds asserts to make sure stack is sane
ifeq ($(DEBUG),1)
	$(TARGET)_CFLAGS += -DLUA_USE_APICHECK
endif

$(TARGET)_SRCS      = \
	game/g_actor.cpp \
	game/g_ai.cpp \
	game/g_ai_lua.cpp \
	game/g_camera.cpp \
	game/g_client.cpp \
	game/g_combat.cpp \
	game/g_cmds.cpp \
	game/g_edicts.cpp \
	game/g_events.cpp \
	game/g_func.cpp \
	game/g_health.cpp \
	game/g_inventory.cpp \
	game/g_main.cpp \
	game/g_match.cpp \
	game/g_mission.cpp \
	game/g_morale.cpp \
	game/g_move.cpp \
	game/g_phys.cpp \
	game/g_reaction.cpp \
	game/g_round.cpp \
	game/g_stats.cpp \
	game/g_spawn.cpp \
	game/g_svcmds.cpp \
	game/g_trigger.cpp \
	game/g_utils.cpp \
	game/g_vis.cpp \
	\
	game/lua/lapi.cpp \
	game/lua/lauxlib.cpp \
	game/lua/lbaselib.cpp \
	game/lua/lcode.cpp \
	game/lua/ldblib.cpp \
	game/lua/ldebug.cpp \
	game/lua/ldo.cpp \
	game/lua/ldump.cpp \
	game/lua/lfunc.cpp \
	game/lua/lgc.cpp \
	game/lua/linit.cpp \
	game/lua/liolib.cpp \
	game/lua/llex.cpp \
	game/lua/lmathlib.cpp \
	game/lua/lmem.cpp \
	game/lua/loadlib.cpp \
	game/lua/lobject.cpp \
	game/lua/lopcodes.cpp \
	game/lua/loslib.cpp \
	game/lua/lparser.cpp \
	game/lua/lstate.cpp \
	game/lua/lstring.cpp \
	game/lua/lstrlib.cpp \
	game/lua/ltable.cpp \
	game/lua/ltablib.cpp \
	game/lua/ltm.cpp \
	game/lua/lundump.cpp \
	game/lua/lvm.cpp \
	game/lua/lzio.cpp \
	game/lua/print.cpp

ifneq ($(HARD_LINKED_GAME),1)
	$(TARGET)_SRCS += shared/mathlib.cpp \
		shared/aabb.cpp \
		shared/shared.cpp \
		shared/utf8.cpp \
		shared/parse.cpp \
		shared/infostring.cpp \
		\
		game/q_shared.cpp \
		game/chr_shared.cpp \
		game/inv_shared.cpp \
		game/inventory.cpp
else
	$(TARGET)_IGNORE := yes
endif

$(TARGET)_OBJS     := $(call ASSEMBLE_OBJECTS,$(TARGET))
$(TARGET)_CXXFLAGS := $($(TARGET)_CFLAGS)
$(TARGET)_CCFLAGS  := $($(TARGET)_CFLAGS)
