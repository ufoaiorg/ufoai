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
	client/cgame/staticcampaign/cl_game_staticcampaign.cpp \
	client/cgame/staticcampaign/scp_missions.cpp \
	client/cgame/staticcampaign/scp_parse.cpp

ifneq ($(HARD_LINKED_CGAME),1)
	$(TARGET)_SRCS += shared/mathlib.cpp \
		shared/shared.cpp \
		shared/utf8.cpp \
		shared/parse.cpp \
		shared/infostring.cpp \
		\
		common/binaryexpressionparser.cpp \
		\
		game/q_shared.cpp \
		game/chr_shared.cpp \
		game/inv_shared.cpp \
		game/inventory.cpp \
		\
		$(cgame-campaign_SRCS)
else
	$(TARGET)_IGNORE := yes
endif

$(TARGET)_OBJS     := $(call ASSEMBLE_OBJECTS,$(TARGET))
$(TARGET)_CXXFLAGS := $($(TARGET)_CFLAGS)
$(TARGET)_CCFLAGS  := $($(TARGET)_CFLAGS)
