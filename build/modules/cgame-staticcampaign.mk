TARGET             := cgame-staticcampaign

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
	client/cgame/staticcampaign/cl_game_staticcampaign.c \
	client/cgame/staticcampaign/scp_missions.c \
	client/cgame/staticcampaign/scp_parse.c

ifneq ($(HARD_LINKED_CGAME),1)
	$(TARGET)_SRCS += shared/mathlib.c \
		shared/shared.c \
		shared/utf8.c \
		shared/parse.c \
		shared/infostring.c \
		\
		common/binaryexpressionparser.c \
		\
		game/q_shared.c \
		game/chr_shared.c \
		game/inv_shared.c \
		game/inventory.c \
		\
		$(cgame-campaign_SRCS)
else
	$(TARGET)_IGNORE := yes
endif

$(TARGET)_OBJS     := $(call ASSEMBLE_OBJECTS,$(TARGET))
$(TARGET)_CXXFLAGS := $($(TARGET)_CFLAGS)
$(TARGET)_CCFLAGS  := $($(TARGET)_CFLAGS)
