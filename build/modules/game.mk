TARGET             := game

# if the linking should be static
$(TARGET)_STATIC   ?= $(STATIC)
ifeq ($($(TARGET)_STATIC),1)
$(TARGET)_LDFLAGS  += -static
endif

$(TARGET)_LINKER   := $(CXX)
$(TARGET)_CFLAGS   += -DCOMPILE_UFO $(SO_CFLAGS) $(SDL_CFLAGS) $(LUA_CFLAGS)
$(TARGET)_LDFLAGS  += $(SO_LDFLAGS) -lm $(SO_LIBS) $(LUA_LIBS)
$(TARGET)_FILE     := base/$(TARGET).$(SO_EXT)

# Lua apicheck adds asserts to make sure stack is sane
ifeq ($(DEBUG),1)
	$(TARGET)_CFLAGS += -DLUA_USE_APICHECK
endif

$(TARGET)_SRCS      = $(subst $(SRCDIR)/,, \
	$(wildcard $(SRCDIR)/game/g_*.cpp) )
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
		game/inventory.cpp  \
		\
		$(LUA_SRCS)
else
	$(TARGET)_IGNORE := yes
endif

$(TARGET)_OBJS     := $(call ASSEMBLE_OBJECTS,$(TARGET))
$(TARGET)_CXXFLAGS := $($(TARGET)_CFLAGS)
$(TARGET)_CCFLAGS  := $($(TARGET)_CFLAGS)
