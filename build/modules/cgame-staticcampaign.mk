TARGET             := cgame-staticcampaign

# if the linking should be static
$(TARGET)_STATIC   ?= $(STATIC)
ifeq ($($(TARGET)_STATIC),1)
$(TARGET)_LDFLAGS  += -static
endif

$(TARGET)_LINKER   := $(CXX)
$(TARGET)_CFLAGS   += -DCOMPILE_UFO $(SO_CFLAGS) $(MXML_CFLAGS) $(SDL_CFLAGS)
$(TARGET)_LDFLAGS  += $(SO_LDFLAGS) $(MXML_LIBS) -lm
$(TARGET)_FILE     := base/$(TARGET).$(SO_EXT)

$(TARGET)_SRCS      = \
	client/cgame/staticcampaign/cl_game_staticcampaign.cpp \
	client/cgame/staticcampaign/scp_missions.cpp \
	client/cgame/staticcampaign/scp_parse.cpp

ifneq ($(HARD_LINKED_CGAME),1)
	$(TARGET)_SRCS   += $(call FILTER_OUT,cl_game_campaign,$(cgame-campaign_SRCS))
else
	$(TARGET)_IGNORE := yes
endif

$(TARGET)_OBJS     := $(call ASSEMBLE_OBJECTS,$(TARGET))
$(TARGET)_CXXFLAGS := $($(TARGET)_CFLAGS)
$(TARGET)_CCFLAGS  := $($(TARGET)_CFLAGS)
