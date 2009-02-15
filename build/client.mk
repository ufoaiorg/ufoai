CLIENT_CFLAGS+=-DCOMPILE_UFO

CLIENT_SRCS = \
	client/cl_actor.c \
	client/cl_cinematic.c \
	client/cl_console.c \
	client/cl_game.c \
	client/cl_game_campaign.c \
	client/cl_game_multiplayer.c \
	client/cl_game_skirmish.c \
	client/cl_http.c \
	client/cl_hud.c \
	client/cl_inventory.c \
	client/cl_input.c \
	client/cl_irc.c \
	client/cl_joystick.c \
	client/cl_keys.c \
	client/cl_language.c \
	client/cl_le.c \
	client/cl_main.c \
	client/cl_menu.c \
	client/cl_parse.c \
	client/cl_particle.c \
	client/cl_rank.c \
	client/cl_screen.c \
	client/cl_sound.c \
	client/cl_sequence.c \
	client/cl_team.c \
	client/cl_tip.c \
	client/cl_tutorials.c \
	client/cl_ugv.c \
	client/cl_video.c \
	client/cl_view.c \
	\
	client/campaign/cp_aircraft.c \
	client/campaign/cp_aircraft_callbacks.c \
	client/campaign/cp_alien_interest.c \
	client/campaign/cp_base.c \
	client/campaign/cp_base_callbacks.c \
	client/campaign/cp_geoscape_actions.c \
	client/campaign/cp_hospital.c \
	client/campaign/cp_hospital_callbacks.c \
	client/campaign/cp_messages.c \
	client/campaign/cp_missions.c \
	client/campaign/cp_mission_triggers.c \
	client/campaign/cp_nations.c \
	client/campaign/cp_parse.c \
	client/campaign/cp_team.c \
	client/campaign/cp_team_callbacks.c \
	client/campaign/cp_time.c \
	client/campaign/cp_xvi.c \
	client/campaign/cl_alienbase.c \
	client/campaign/cl_aliencont.c \
	client/campaign/cl_aliencont_callbacks.c \
	client/campaign/cl_airfight.c \
	client/campaign/cl_airfightmap.c \
	client/campaign/cl_campaign.c \
	client/campaign/cl_event.c \
	client/campaign/cl_employee.c \
	client/campaign/cp_employee_callbacks.c \
	client/campaign/cl_installation.c \
	client/campaign/cl_market.c \
	client/campaign/cp_market_callbacks.c \
	client/campaign/cl_map.c \
	client/campaign/cl_mapfightequip.c \
	client/campaign/cl_nation.c \
	client/campaign/cl_produce.c \
	client/campaign/cl_produce_callbacks.c \
	client/campaign/cl_radar.c \
	client/campaign/cl_research.c \
	client/campaign/cl_research_callbacks.c \
	client/campaign/cl_save.c \
	client/campaign/cl_transfer.c \
	client/campaign/cl_ufo.c \
	client/campaign/cl_ufopedia.c \
	client/campaign/cl_uforecovery.c \
	client/campaign/cl_messageoptions.c \
	client/campaign/cl_messageoptions_callbacks.c \
	client/campaign/cl_popup.c \
	client/campaign/cl_fightequip_callbacks.c \
	\
	client/campaign/missions/cp_mission_baseattack.c \
	client/campaign/missions/cp_mission_buildbase.c \
	client/campaign/missions/cp_mission_harvest.c \
	client/campaign/missions/cp_mission_intercept.c \
	client/campaign/missions/cp_mission_recon.c \
	client/campaign/missions/cp_mission_supply.c \
	client/campaign/missions/cp_mission_terror.c \
	client/campaign/missions/cp_mission_xvi.c \
	\
	client/multiplayer/mp_callbacks.c \
	client/multiplayer/mp_serverlist.c \
	client/multiplayer/mp_chatmessages.c \
	client/multiplayer/mp_team.c \
	\
	client/menu/m_actions.c \
	client/menu/m_condition.c \
	client/menu/m_data.c \
	client/menu/m_dragndrop.c \
	client/menu/m_draw.c \
	client/menu/m_font.c \
	client/menu/m_icon.c \
	client/menu/m_input.c \
	client/menu/m_main.c \
	client/menu/m_nodes.c \
	client/menu/m_parse.c \
	client/menu/m_popup.c \
	client/menu/m_timer.c \
	client/menu/m_tooltip.c \
	client/menu/node/m_node_abstractnode.c \
	client/menu/node/m_node_abstractvalue.c \
	client/menu/node/m_node_abstractoption.c \
	client/menu/node/m_node_abstractscrollbar.c \
	client/menu/node/m_node_airfightmap.c \
	client/menu/node/m_node_bar.c \
	client/menu/node/m_node_base.c \
	client/menu/node/m_node_button.c \
	client/menu/node/m_node_checkbox.c \
	client/menu/node/m_node_cinematic.c \
	client/menu/node/m_node_container.c \
	client/menu/node/m_node_controls.c \
	client/menu/node/m_node_custombutton.c \
	client/menu/node/m_node_editor.c \
	client/menu/node/m_node_image.c \
	client/menu/node/m_node_item.c \
	client/menu/node/m_node_linestrip.c \
	client/menu/node/m_node_map.c \
	client/menu/node/m_node_model.c \
	client/menu/node/m_node_optionlist.c \
	client/menu/node/m_node_panel.c \
	client/menu/node/m_node_radar.c \
	client/menu/node/m_node_radiobutton.c \
	client/menu/node/m_node_selectbox.c \
	client/menu/node/m_node_string.c \
	client/menu/node/m_node_special.c \
	client/menu/node/m_node_spinner.c \
	client/menu/node/m_node_tab.c \
	client/menu/node/m_node_tbar.c \
	client/menu/node/m_node_text.c \
	client/menu/node/m_node_textentry.c \
	client/menu/node/m_node_todo.c \
	client/menu/node/m_node_vscrollbar.c \
	client/menu/node/m_node_window.c \
	client/menu/node/m_node_zone.c \
	\
	client/mxml/mxml-attr.c \
	client/mxml/mxml-entity.c \
	client/mxml/mxml-file.c \
	client/mxml/mxml-index.c \
	client/mxml/mxml-node.c \
	client/mxml/mxml-private.c \
	client/mxml/mxml-search.c \
	client/mxml/mxml-set.c \
	client/mxml/mxml-string.c \
	client/mxml/mxml_ufoai.c \
	\
	common/cmd.c \
	common/http.c \
	common/ioapi.c \
	common/unzip.c \
	common/cmodel.c \
	common/common.c \
	common/cvar.c \
	common/files.c \
	common/md4.c \
	common/md5.c \
	common/mem.c \
	common/msg.c \
	common/net.c \
	common/netpack.c \
	common/dbuffer.c \
	common/pqueue.c \
	common/scripts.c \
	common/tracing.c \
	common/routing.c \
	\
	server/sv_ccmds.c \
	server/sv_game.c \
	server/sv_init.c \
	server/sv_main.c \
	server/sv_send.c \
	server/sv_user.c \
	server/sv_world.c \
	\
	game/q_shared.c \
	game/inv_shared.c \
	\
	client/renderer/r_array.c \
	client/renderer/r_bsp.c \
	client/renderer/r_draw.c \
	client/renderer/r_entity.c \
	client/renderer/r_font.c \
	client/renderer/r_image.c \
	client/renderer/r_light.c \
	client/renderer/r_lightmap.c \
	client/renderer/r_main.c \
	client/renderer/r_material.c \
	client/renderer/r_misc.c \
	client/renderer/r_mesh.c \
	client/renderer/r_mesh_anim.c \
	client/renderer/r_model.c \
	client/renderer/r_model_alias.c \
	client/renderer/r_model_brush.c \
	client/renderer/r_model_dpm.c \
	client/renderer/r_model_md2.c \
	client/renderer/r_model_md3.c \
	client/renderer/r_overlay.c \
	client/renderer/r_particle.c \
	client/renderer/r_program.c \
	client/renderer/r_sdl.c \
	client/renderer/r_surface.c \
	client/renderer/r_state.c \
	client/renderer/r_sphere.c \
	client/renderer/r_thread.c \
	\
	shared/byte.c \
	shared/mathlib.c \
	shared/infostring.c \
	shared/parse.c \
	shared/shared.c

ifneq ($(findstring $(TARGET_OS), netbsd freebsd linux-gnu),)
	CLIENT_SRCS+= \
		ports/linux/linux_main.c \
		ports/unix/unix_console.c \
		ports/unix/unix_main.c \
		ports/unix/unix_glob.c
endif

ifeq ($(TARGET_OS),mingw32)
	CLIENT_SRCS+=\
		ports/windows/win_console.c \
		ports/windows/win_shared.c \
		ports/windows/win_main.c \
		ports/windows/ufo.rc
endif

ifeq ($(TARGET_OS),darwin)
	CLIENT_SRCS+= \
		ports/macosx/osx_main.m \
		ports/unix/unix_glob.c \
		ports/unix/unix_console.c \
		ports/unix/unix_main.c
endif

ifeq ($(TARGET_OS),solaris)
	CLIENT_SRCS+= \
		ports/solaris/solaris_main.c \
		ports/unix/unix_console.c \
		ports/unix/unix_main.c \
		ports/unix/unix_glob.c
endif

CLIENT_OBJS= \
	$(patsubst %.c, $(BUILDDIR)/client/%.o, $(filter %.c, $(CLIENT_SRCS))) \
	$(patsubst %.m, $(BUILDDIR)/client/%.o, $(filter %.m, $(CLIENT_SRCS))) \
	$(patsubst %.rc, $(BUILDDIR)/client/%.o, $(filter %.rc, $(CLIENT_SRCS)))

CLIENT_TARGET=ufo$(EXE_EXT)

ifeq ($(BUILD_CLIENT),1)
	ALL_OBJS+=$(CLIENT_OBJS)
	TARGETS+=$(CLIENT_TARGET)
endif

# Say how to link the exe
$(CLIENT_TARGET): $(CLIENT_OBJS) $(BUILDDIR)/.dirs
	@echo " * [UFO] ... linking $(LNKFLAGS) ($(CLIENT_LIBS) $(SDL_LIBS))"; \
		$(CC) $(LDFLAGS) -o $@ $(CLIENT_OBJS) $(LNKFLAGS) $(CLIENT_LIBS) $(SDL_LIBS)

# Say how to build .o files from .c files for this module
$(BUILDDIR)/client/%.o: $(SRCDIR)/%.c $(BUILDDIR)/.dirs
	@echo " * [UFO] $<"; \
		$(CC) $(CFLAGS) $(CLIENT_CFLAGS) $(SDL_CFLAGS) -o $@ -c $< $(CFLAGS_M_OPTS)

# Say how to build .o files from .m files for this module
$(BUILDDIR)/client/%.o: $(SRCDIR)/%.m $(BUILDDIR)/.dirs
	@echo " * [UFO] $<"; \
		$(CC) $(CFLAGS) $(CLIENT_CFLAGS) $(SDL_CFLAGS) -o $@ -c $< $(CFLAGS_M_OPTS)

ifeq ($(TARGET_OS),mingw32)
# Say how to build .o files from .rc files for this module
$(BUILDDIR)/client/%.o: $(SRCDIR)/%.rc $(BUILDDIR)/.dirs
	@echo " * [RC ] $<"; \
		$(WINDRES) -DCROSSBUILD -i $< -o $@
endif
