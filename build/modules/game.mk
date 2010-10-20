TARGET             := game

# if the linking should be static
$(TARGET)_STATIC   ?= $(STATIC)
ifeq ($($(TARGET)_STATIC),1)
$(TARGET)_LDFLAGS  += -static
endif

$(TARGET)_LINKER   := $(CC)
$(TARGET)_CFLAGS   += -DCOMPILE_UFO $(SO_CFLAGS)
$(TARGET)_LDFLAGS  += $(SO_LDFLAGS) -lm
$(TARGET)_FILE     := base/$(TARGET).$(SO_EXT)

# Lua apicheck adds asserts to make sure stack is sane
ifeq ($(DEBUG),1)
	$(TARGET)_CFLAGS += -DLUA_USE_APICHECK
endif

$(TARGET)_SRCS      = \
	game/g_actor.c \
	game/g_ai.c \
	game/g_ai_lua.c \
	game/g_client.c \
	game/g_combat.c \
	game/g_cmds.c \
	game/g_edicts.c \
	game/g_events.c \
	game/g_func.c \
	game/g_inventory.c \
	game/g_main.c \
	game/g_match.c \
	game/g_mission.c \
	game/g_morale.c \
	game/g_move.c \
	game/g_phys.c \
	game/g_reaction.c \
	game/g_round.c \
	game/g_stats.c \
	game/g_spawn.c \
	game/g_svcmds.c \
	game/g_trigger.c \
	game/g_utils.c \
	game/g_vis.c \
	\
	game/lua/lapi.c \
	game/lua/lauxlib.c \
	game/lua/lbaselib.c \
	game/lua/lcode.c \
	game/lua/ldblib.c \
	game/lua/ldebug.c \
	game/lua/ldo.c \
	game/lua/ldump.c \
	game/lua/lfunc.c \
	game/lua/lgc.c \
	game/lua/linit.c \
	game/lua/liolib.c \
	game/lua/llex.c \
	game/lua/lmathlib.c \
	game/lua/lmem.c \
	game/lua/loadlib.c \
	game/lua/lobject.c \
	game/lua/lopcodes.c \
	game/lua/loslib.c \
	game/lua/lparser.c \
	game/lua/lstate.c \
	game/lua/lstring.c \
	game/lua/lstrlib.c \
	game/lua/ltable.c \
	game/lua/ltablib.c \
	game/lua/ltm.c \
	game/lua/lundump.c \
	game/lua/lvm.c \
	game/lua/lzio.c \
	game/lua/print.c

ifneq ($(HARD_LINKED_GAME),1)
	$(TARGET)_SRCS += shared/mathlib.c \
		shared/shared.c \
		shared/utf8.c \
		shared/parse.c \
		shared/infostring.c \
		\
		game/q_shared.c \
		game/chr_shared.c \
		game/inv_shared.c \
		game/inventory.c
else
	$(TARGET)_IGNORE := yes
endif

$(TARGET)_OBJS     := $(call ASSEMBLE_OBJECTS,$(TARGET))
$(TARGET)_CXXFLAGS := $($(TARGET)_CFLAGS)
$(TARGET)_CCFLAGS  := $($(TARGET)_CFLAGS)
