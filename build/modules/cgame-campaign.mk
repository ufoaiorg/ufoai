TARGET             := cgame-campaign

# if the linking should be static
$(TARGET)_STATIC   ?= $(STATIC)
ifeq ($($(TARGET)_STATIC),1)
$(TARGET)_LDFLAGS  += -static
endif

$(TARGET)_LINKER   := $(CXX)
$(TARGET)_CFLAGS   += -DCOMPILE_UFO $(SO_CFLAGS) $(MXML_CFLAGS)
$(TARGET)_LDFLAGS  += $(SO_LDFLAGS) $(MXML_LIBS) -lm
$(TARGET)_FILE     := base/$(TARGET).$(SO_EXT)

$(TARGET)_SRCS      = \
	client/cgame/campaign/cl_game_campaign.cpp \
	client/cgame/campaign/cp_aircraft.cpp \
	client/cgame/campaign/cp_aircraft_callbacks.cpp \
	client/cgame/campaign/cp_alien_interest.cpp \
	client/cgame/campaign/cp_base.cpp \
	client/cgame/campaign/cp_base_callbacks.cpp \
	client/cgame/campaign/cp_basedefence_callbacks.cpp \
	client/cgame/campaign/cp_building.cpp \
	client/cgame/campaign/cp_cgame_callbacks.cpp \
	client/cgame/campaign/cp_capacity.cpp \
	client/cgame/campaign/cp_hospital.cpp \
	client/cgame/campaign/cp_hospital_callbacks.cpp \
	client/cgame/campaign/cp_messages.cpp \
	client/cgame/campaign/cp_missions.cpp \
	client/cgame/campaign/cp_mission_triggers.cpp \
	client/cgame/campaign/cp_parse.cpp \
	client/cgame/campaign/cp_rank.cpp \
	client/cgame/campaign/cp_team.cpp \
	client/cgame/campaign/cp_team_callbacks.cpp \
	client/cgame/campaign/cp_time.cpp \
	client/cgame/campaign/cp_xvi.cpp \
	client/cgame/campaign/cp_alienbase.cpp \
	client/cgame/campaign/cp_aliencont.cpp \
	client/cgame/campaign/cp_aliencont_callbacks.cpp \
	client/cgame/campaign/cp_auto_mission.cpp \
	client/cgame/campaign/cp_mission_callbacks.cpp \
	client/cgame/campaign/cp_airfight.cpp \
	client/cgame/campaign/cp_campaign.cpp \
	client/cgame/campaign/cp_event.cpp \
	client/cgame/campaign/cp_event_callbacks.cpp \
	client/cgame/campaign/cp_employee.cpp \
	client/cgame/campaign/cp_employee_callbacks.cpp \
	client/cgame/campaign/cp_installation.cpp \
	client/cgame/campaign/cp_installation_callbacks.cpp \
	client/cgame/campaign/cp_market.cpp \
	client/cgame/campaign/cp_market_callbacks.cpp \
	client/cgame/campaign/cp_map.cpp \
	client/cgame/campaign/cp_mapfightequip.cpp \
	client/cgame/campaign/cp_nation.cpp \
	client/cgame/campaign/cp_produce.cpp \
	client/cgame/campaign/cp_produce_callbacks.cpp \
	client/cgame/campaign/cp_radar.cpp \
	client/cgame/campaign/cp_research.cpp \
	client/cgame/campaign/cp_research_callbacks.cpp \
	client/cgame/campaign/cp_save.cpp \
	client/cgame/campaign/cp_statistics.cpp \
	client/cgame/campaign/cp_transfer.cpp \
	client/cgame/campaign/cp_transfer_callbacks.cpp \
	client/cgame/campaign/cp_ufo.cpp \
	client/cgame/campaign/cp_ufopedia.cpp \
	client/cgame/campaign/cp_uforecovery.cpp \
	client/cgame/campaign/cp_uforecovery_callbacks.cpp \
	client/cgame/campaign/cp_messageoptions.cpp \
	client/cgame/campaign/cp_messageoptions_callbacks.cpp \
	client/cgame/campaign/cp_overlay.cpp \
	client/cgame/campaign/cp_popup.cpp \
	client/cgame/campaign/cp_fightequip_callbacks.cpp \
	\
	client/cgame/campaign/missions/cp_mission_baseattack.cpp \
	client/cgame/campaign/missions/cp_mission_buildbase.cpp \
	client/cgame/campaign/missions/cp_mission_harvest.cpp \
	client/cgame/campaign/missions/cp_mission_intercept.cpp \
	client/cgame/campaign/missions/cp_mission_recon.cpp \
	client/cgame/campaign/missions/cp_mission_rescue.cpp \
	client/cgame/campaign/missions/cp_mission_supply.cpp \
	client/cgame/campaign/missions/cp_mission_terror.cpp \
	client/cgame/campaign/missions/cp_mission_ufocarrier.cpp \
	client/cgame/campaign/missions/cp_mission_xvi.cpp

ifneq ($(HARD_LINKED_CGAME),1)
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
