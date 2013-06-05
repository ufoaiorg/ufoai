TARGET             := ufo

# if the linking should be static
$(TARGET)_STATIC   ?= $(STATIC)
ifeq ($($(TARGET)_STATIC),1)
$(TARGET)_LDFLAGS  += -static
endif

$(TARGET)_LINKER   := $(CXX)
$(TARGET)_FILE     := $(TARGET)$(EXE_EXT)
$(TARGET)_CFLAGS   += -DCOMPILE_UFO $(BFD_CFLAGS) $(SDL_CFLAGS) $(SDL_TTF_CFLAGS) $(SDL_MIXER_CFLAGS) $(CURL_CFLAGS) $(THEORA_CFLAGS) $(XVID_CFLAGS) $(VORBIS_CFLAGS) $(OGG_CFLAGS) $(MXML_CFLAGS) $(MUMBLE_CFLAGS)
$(TARGET)_LDFLAGS  += -lpng -ljpeg $(BFD_LIBS) $(INTL_LIBS) $(SDL_TTF_LIBS) $(SDL_MIXER_LIBS) $(OPENGL_LIBS) $(SDL_LIBS) $(CURL_LIBS) $(THEORA_LIBS) $(XVID_LIBS) $(VORBIS_LIBS) $(OGG_LIBS) $(MXML_LIBS) $(MUMBLE_LIBS) $(SO_LIBS) -lz

$(TARGET)_SRCS      = \
	client/cl_console.cpp \
	client/cl_http.cpp \
	client/cl_inventory.cpp \
	client/cl_inventory_callbacks.cpp \
	client/cl_irc.cpp \
	client/cl_language.cpp \
	client/cl_main.cpp \
	client/cl_menu.cpp \
	client/cl_screen.cpp \
	client/cl_team.cpp \
	client/cl_tip.cpp \
	client/cl_tutorials.cpp \
	client/cl_video.cpp \
	\
	client/input/cl_input.cpp \
	client/input/cl_joystick.cpp \
	client/input/cl_keys.cpp \
	\
	client/cinematic/cl_cinematic.cpp \
	client/cinematic/cl_cinematic_roq.cpp \
	client/cinematic/cl_cinematic_ogm.cpp \
	client/cinematic/cl_sequence.cpp \
	\
	client/battlescape/cl_actor.cpp \
	client/battlescape/cl_battlescape.cpp \
	client/battlescape/cl_camera.cpp \
	client/battlescape/cl_hud.cpp \
	client/battlescape/cl_hud_callbacks.cpp \
	client/battlescape/cl_localentity.cpp \
	client/battlescape/cl_parse.cpp \
	client/battlescape/cl_particle.cpp \
	client/battlescape/cl_radar.cpp \
	client/battlescape/cl_ugv.cpp \
	client/battlescape/cl_view.cpp \
	client/battlescape/cl_spawn.cpp \
	\
	client/battlescape/events/e_main.cpp \
	client/battlescape/events/e_parse.cpp \
	client/battlescape/events/e_server.cpp \
	client/battlescape/events/event/actor/e_event_actoradd.cpp \
	client/battlescape/events/event/actor/e_event_actorappear.cpp \
	client/battlescape/events/event/actor/e_event_actorclientaction.cpp \
	client/battlescape/events/event/actor/e_event_actordie.cpp \
	client/battlescape/events/event/actor/e_event_actormove.cpp \
	client/battlescape/events/event/actor/e_event_actorresetclientaction.cpp \
	client/battlescape/events/event/actor/e_event_actorreservationchange.cpp \
	client/battlescape/events/event/actor/e_event_actorreactionfirechange.cpp \
	client/battlescape/events/event/actor/e_event_actorrevitalised.cpp \
	client/battlescape/events/event/actor/e_event_actorshoot.cpp \
	client/battlescape/events/event/actor/e_event_actorshoothidden.cpp \
	client/battlescape/events/event/actor/e_event_actorstartshoot.cpp \
	client/battlescape/events/event/actor/e_event_actorstatechange.cpp \
	client/battlescape/events/event/actor/e_event_actorstats.cpp \
	client/battlescape/events/event/actor/e_event_actorthrow.cpp \
	client/battlescape/events/event/actor/e_event_actorturn.cpp \
	client/battlescape/events/event/actor/e_event_actorwound.cpp \
	client/battlescape/events/event/inventory/e_event_invadd.cpp \
	client/battlescape/events/event/inventory/e_event_invammo.cpp \
	client/battlescape/events/event/inventory/e_event_invdel.cpp \
	client/battlescape/events/event/inventory/e_event_invreload.cpp \
	client/battlescape/events/event/player/e_event_centerview.cpp \
	client/battlescape/events/event/player/e_event_doendround.cpp \
	client/battlescape/events/event/player/e_event_endroundannounce.cpp \
	client/battlescape/events/event/player/e_event_reset.cpp \
	client/battlescape/events/event/player/e_event_results.cpp \
	client/battlescape/events/event/player/e_event_startgame.cpp \
	client/battlescape/events/event/world/e_event_addbrushmodel.cpp \
	client/battlescape/events/event/world/e_event_addedict.cpp \
	client/battlescape/events/event/world/e_event_cameraappear.cpp \
	client/battlescape/events/event/world/e_event_doorclose.cpp \
	client/battlescape/events/event/world/e_event_dooropen.cpp \
	client/battlescape/events/event/world/e_event_entappear.cpp \
	client/battlescape/events/event/world/e_event_entdestroy.cpp \
	client/battlescape/events/event/world/e_event_entperish.cpp \
	client/battlescape/events/event/world/e_event_explode.cpp \
	client/battlescape/events/event/world/e_event_particleappear.cpp \
	client/battlescape/events/event/world/e_event_particlespawn.cpp \
	client/battlescape/events/event/world/e_event_sound.cpp \
	\
	client/sound/s_music.cpp \
	client/sound/s_main.cpp \
	client/sound/s_mumble.cpp \
	client/sound/s_mix.cpp \
	client/sound/s_sample.cpp \
	\
	client/cgame/cl_game.cpp \
	client/cgame/cl_game_team.cpp \
	\
	client/ui/ui_actions.cpp \
	client/ui/ui_behaviour.cpp \
	client/ui/ui_components.cpp \
	client/ui/ui_data.cpp \
	client/ui/ui_dragndrop.cpp \
	client/ui/ui_draw.cpp \
	client/ui/ui_expression.cpp \
	client/ui/ui_font.cpp \
	client/ui/ui_sprite.cpp \
	client/ui/ui_input.cpp \
	client/ui/ui_main.cpp \
	client/ui/ui_node.cpp \
	client/ui/ui_nodes.cpp \
	client/ui/ui_parse.cpp \
	client/ui/ui_popup.cpp \
	client/ui/ui_render.cpp \
	client/ui/ui_sound.cpp \
	client/ui/ui_timer.cpp \
	client/ui/ui_tooltip.cpp \
	client/ui/ui_windows.cpp \
	client/ui/node/ui_node_abstractnode.cpp \
	client/ui/node/ui_node_abstractvalue.cpp \
	client/ui/node/ui_node_abstractoption.cpp \
	client/ui/node/ui_node_abstractscrollbar.cpp \
	client/ui/node/ui_node_abstractscrollable.cpp \
	client/ui/node/ui_node_bar.cpp \
	client/ui/node/ui_node_base.cpp \
	client/ui/node/ui_node_baseinventory.cpp \
	client/ui/node/ui_node_battlescape.cpp \
	client/ui/node/ui_node_button.cpp \
	client/ui/node/ui_node_checkbox.cpp \
	client/ui/node/ui_node_data.cpp \
	client/ui/node/ui_node_video.cpp \
	client/ui/node/ui_node_container.cpp \
	client/ui/node/ui_node_controls.cpp \
	client/ui/node/ui_node_editor.cpp \
	client/ui/node/ui_node_ekg.cpp \
	client/ui/node/ui_node_geoscape.cpp \
	client/ui/node/ui_node_image.cpp \
	client/ui/node/ui_node_item.cpp \
	client/ui/node/ui_node_keybinding.cpp \
	client/ui/node/ui_node_linechart.cpp \
	client/ui/node/ui_node_material_editor.cpp \
	client/ui/node/ui_node_model.cpp \
	client/ui/node/ui_node_messagelist.cpp \
	client/ui/node/ui_node_option.cpp \
	client/ui/node/ui_node_optionlist.cpp \
	client/ui/node/ui_node_optiontree.cpp \
	client/ui/node/ui_node_panel.cpp \
	client/ui/node/ui_node_radar.cpp \
	client/ui/node/ui_node_radiobutton.cpp \
	client/ui/node/ui_node_rows.cpp \
	client/ui/node/ui_node_selectbox.cpp \
	client/ui/node/ui_node_sequence.cpp \
	client/ui/node/ui_node_string.cpp \
	client/ui/node/ui_node_special.cpp \
	client/ui/node/ui_node_spinner.cpp \
	client/ui/node/ui_node_tab.cpp \
	client/ui/node/ui_node_tbar.cpp \
	client/ui/node/ui_node_text.cpp \
	client/ui/node/ui_node_text2.cpp \
	client/ui/node/ui_node_textlist.cpp \
	client/ui/node/ui_node_textentry.cpp \
	client/ui/node/ui_node_texture.cpp \
	client/ui/node/ui_node_timer.cpp \
	client/ui/node/ui_node_todo.cpp \
	client/ui/node/ui_node_vscrollbar.cpp \
	client/ui/node/ui_node_window.cpp \
	client/ui/node/ui_node_zone.cpp \
	\
	common/binaryexpressionparser.cpp \
	common/cmd.cpp \
	common/http.cpp \
	common/ioapi.cpp \
	common/unzip.cpp \
	common/bsp.cpp \
	common/grid.cpp \
	common/cmodel.cpp \
	common/common.cpp \
	common/cvar.cpp \
	common/files.cpp \
	common/list.cpp \
	common/md4.cpp \
	common/md5.cpp \
	common/mem.cpp \
	common/msg.cpp \
	common/net.cpp \
	common/netpack.cpp \
	common/dbuffer.cpp \
	common/pqueue.cpp \
	common/scripts.cpp \
	common/tracing.cpp \
	common/routing.cpp \
	common/xml.cpp \
	\
	server/sv_ccmds.cpp \
	server/sv_game.cpp \
	server/sv_init.cpp \
	server/sv_log.cpp \
	server/sv_main.cpp \
	server/sv_mapcycle.cpp \
	server/sv_rma.cpp \
	server/sv_send.cpp \
	server/sv_user.cpp \
	server/sv_world.cpp \
	\
	client/renderer/r_array.cpp \
	client/renderer/r_bsp.cpp \
	client/renderer/r_draw.cpp \
	client/renderer/r_corona.cpp \
	client/renderer/r_entity.cpp \
	client/renderer/r_font.cpp \
	client/renderer/r_flare.cpp \
	client/renderer/r_framebuffer.cpp \
	client/renderer/r_geoscape.cpp \
	client/renderer/r_grass.cpp \
	client/renderer/r_image.cpp \
	client/renderer/r_light.cpp \
	client/renderer/r_lightmap.cpp \
	client/renderer/r_main.cpp \
	client/renderer/r_material.cpp \
	client/renderer/r_matrix.cpp \
	client/renderer/r_misc.cpp \
	client/renderer/r_mesh.cpp \
	client/renderer/r_mesh_anim.cpp \
	client/renderer/r_model.cpp \
	client/renderer/r_model_alias.cpp \
	client/renderer/r_model_brush.cpp \
	client/renderer/r_model_md2.cpp \
	client/renderer/r_model_md3.cpp \
	client/renderer/r_model_obj.cpp \
	client/renderer/r_particle.cpp \
	client/renderer/r_program.cpp \
	client/renderer/r_sdl.cpp \
	client/renderer/r_surface.cpp \
	client/renderer/r_state.cpp \
	client/renderer/r_sphere.cpp \
	client/renderer/r_thread.cpp \
	\
	shared/bfd.cpp \
	shared/byte.cpp \
	shared/mathlib.cpp \
	shared/mathlib_extra.cpp \
	shared/aabb.cpp \
	shared/utf8.cpp \
	shared/images.cpp \
	shared/stringhunk.cpp \
	shared/infostring.cpp \
	shared/parse.cpp \
	shared/shared.cpp \
	\
	game/q_shared.cpp \
	game/chr_shared.cpp \
	game/inv_shared.cpp \
	game/inventory.cpp \
	\
	$(MXML_SRCS)\
	\
	$(MUMBLE_SRCS)

ifneq ($(findstring $(TARGET_OS), netbsd freebsd linux),)
	$(TARGET)_SRCS += \
		ports/linux/linux_main.cpp \
		ports/unix/unix_console.cpp \
		ports/unix/unix_files.cpp \
		ports/unix/unix_shared.cpp \
		ports/unix/unix_main.cpp
	$(TARGET)_LDFLAGS +=
endif

ifneq ($(findstring $(TARGET_OS), mingw32 mingw64),)
	$(TARGET)_SRCS +=\
		ports/windows/win_backtrace.cpp \
		ports/windows/win_console.cpp \
		ports/windows/win_shared.cpp \
		ports/windows/win_main.cpp \
		ports/windows/ufo.rc
	$(TARGET)_LDFLAGS +=
endif

ifeq ($(TARGET_OS),darwin)
	$(TARGET)_SRCS += \
		ports/macosx/osx_main.cpp \
		ports/macosx/osx_shared.cpp \
		ports/unix/unix_console.cpp \
		ports/unix/unix_files.cpp \
		ports/unix/unix_shared.cpp \
		ports/unix/unix_main.cpp
	$(TARGET)_LDFLAGS +=
endif

ifeq ($(TARGET_OS),solaris)
	$(TARGET)_SRCS += \
		ports/solaris/solaris_main.cpp \
		ports/unix/unix_console.cpp \
		ports/unix/unix_files.cpp \
		ports/unix/unix_shared.cpp \
		ports/unix/unix_main.cpp
	$(TARGET)_LDFLAGS +=
endif

ifeq ($(TARGET_OS),android)
	$(TARGET)_SRCS += \
		ports/android/android_console.cpp \
		ports/android/android_debugger.cpp \
		ports/android/android_main.cpp \
		ports/android/android_system.cpp \
		ports/unix/unix_shared.cpp \
		ports/unix/unix_files.cpp
endif

ifeq ($(HARD_LINKED_GAME),1)
	$(TARGET)_SRCS     += $(game_SRCS)
else
	$(TARGET)_DEPS     := game
endif

ifeq ($(HARD_LINKED_CGAME),1)
	$(TARGET)_SRCS     += \
		$(cgame-campaign_SRCS) \
		$(cgame-skirmish_SRCS) \
		$(cgame-multiplayer_SRCS) \
		$(cgame-staticcampaign_SRCS)
else
	$(TARGET)_DEPS     := \
		cgame-campaign \
		cgame-skirmish \
		cgame-multiplayer \
		cgame-staticcampaign
endif

$(TARGET)_OBJS     := $(call ASSEMBLE_OBJECTS,$(TARGET))
$(TARGET)_CXXFLAGS := $($(TARGET)_CFLAGS)
$(TARGET)_CCFLAGS  := $($(TARGET)_CFLAGS)
