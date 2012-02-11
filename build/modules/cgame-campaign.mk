TARGET             := cgame-campaign

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
	client/cgame/campaign/cl_game_campaign.c \
	client/cgame/campaign/cp_aircraft.c \
	client/cgame/campaign/cp_aircraft_callbacks.c \
	client/cgame/campaign/cp_alien_interest.c \
	client/cgame/campaign/cp_base.c \
	client/cgame/campaign/cp_base_callbacks.c \
	client/cgame/campaign/cp_basedefence_callbacks.c \
	client/cgame/campaign/cp_building.c \
	client/cgame/campaign/cp_cgame_callbacks.c \
	client/cgame/campaign/cp_capacity.c \
	client/cgame/campaign/cp_hospital.c \
	client/cgame/campaign/cp_hospital_callbacks.c \
	client/cgame/campaign/cp_messages.c \
	client/cgame/campaign/cp_missions.c \
	client/cgame/campaign/cp_mission_triggers.c \
	client/cgame/campaign/cp_parse.c \
	client/cgame/campaign/cp_rank.c \
	client/cgame/campaign/cp_team.c \
	client/cgame/campaign/cp_team_callbacks.c \
	client/cgame/campaign/cp_time.c \
	client/cgame/campaign/cp_xvi.c \
	client/cgame/campaign/cp_alienbase.c \
	client/cgame/campaign/cp_aliencont.c \
	client/cgame/campaign/cp_aliencont_callbacks.c \
	client/cgame/campaign/cp_auto_mission.c \
	client/cgame/campaign/cp_mission_callbacks.c \
	client/cgame/campaign/cp_airfight.c \
	client/cgame/campaign/cp_campaign.c \
	client/cgame/campaign/cp_event.c \
	client/cgame/campaign/cp_employee.c \
	client/cgame/campaign/cp_employee_callbacks.c \
	client/cgame/campaign/cp_installation.c \
	client/cgame/campaign/cp_installation_callbacks.c \
	client/cgame/campaign/cp_market.c \
	client/cgame/campaign/cp_market_callbacks.c \
	client/cgame/campaign/cp_map.c \
	client/cgame/campaign/cp_mapfightequip.c \
	client/cgame/campaign/cp_nation.c \
	client/cgame/campaign/cp_produce.c \
	client/cgame/campaign/cp_produce_callbacks.c \
	client/cgame/campaign/cp_radar.c \
	client/cgame/campaign/cp_research.c \
	client/cgame/campaign/cp_research_callbacks.c \
	client/cgame/campaign/cp_save.c \
	client/cgame/campaign/cp_statistics.c \
	client/cgame/campaign/cp_transfer.c \
	client/cgame/campaign/cp_transfer_callbacks.c \
	client/cgame/campaign/cp_ufo.c \
	client/cgame/campaign/cp_ufopedia.c \
	client/cgame/campaign/cp_uforecovery.c \
	client/cgame/campaign/cp_uforecovery_callbacks.c \
	client/cgame/campaign/cp_messageoptions.c \
	client/cgame/campaign/cp_messageoptions_callbacks.c \
	client/cgame/campaign/cp_overlay.c \
	client/cgame/campaign/cp_popup.c \
	client/cgame/campaign/cp_fightequip_callbacks.c \
	\
	client/cgame/campaign/missions/cp_mission_baseattack.c \
	client/cgame/campaign/missions/cp_mission_buildbase.c \
	client/cgame/campaign/missions/cp_mission_harvest.c \
	client/cgame/campaign/missions/cp_mission_intercept.c \
	client/cgame/campaign/missions/cp_mission_recon.c \
	client/cgame/campaign/missions/cp_mission_rescue.c \
	client/cgame/campaign/missions/cp_mission_supply.c \
	client/cgame/campaign/missions/cp_mission_terror.c \
	client/cgame/campaign/missions/cp_mission_xvi.c

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
