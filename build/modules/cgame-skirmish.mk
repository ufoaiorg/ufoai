TARGET             := cgame-skirmish

# if the linking should be static
$(TARGET)_STATIC   ?= $(STATIC)
ifeq ($($(TARGET)_STATIC),1)
$(TARGET)_LDFLAGS  += -static
endif

$(TARGET)_LINKER   := $(CC)
$(TARGET)_CFLAGS   += -DCOMPILE_UFO $(SO_CFLAGS) $(MXML_CFLAGS)
$(TARGET)_LDFLAGS  += $(SO_LDFLAGS) $(MXML_LIBS) -lm
$(TARGET)_FILE     := base/$(TARGET).$(SO_EXT)

$(TARGET)_SRCS      = \
	client/cgame/skirmish/cl_game_skirmish.c

ifneq ($(HARD_LINKED_CGAME),1)
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
