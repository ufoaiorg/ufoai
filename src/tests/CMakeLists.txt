# Copyright (C) 2002-2024 UFO: Alien Invasion.
#
# This program is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public License
# as published by the Free Software Foundation; either version 2
# of the License, or (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
#
# See the GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

cmake_minimum_required(VERSION 3.13)

project(ufotestall DESCRIPTION "Unittest suite for UFO:AI.")

find_package(CURL)
find_package(JPEG)
find_package(X11)
set(OpenGL_GL_PREFERENCE "LEGACY")
find_package(OpenGL)
find_package(PNG)
find_package(SDL)
find_package(ZLIB)
find_package(Intl)
find_package(OGG)
find_package(Lua 5.3)
find_package(SDL_mixer)
find_package(SDL_ttf)
find_package(VORBIS)
find_package(MXML)
find_package(GTest)

set(UFOTESTALL_SOURCES
	../client/battlescape/cl_actor.cpp
	../client/battlescape/cl_battlescape.cpp
	../client/battlescape/cl_camera.cpp
	../client/battlescape/cl_hud.cpp
	../client/battlescape/cl_hud_callbacks.cpp
	../client/battlescape/cl_localentity.cpp
	../client/battlescape/cl_parse.cpp
	../client/battlescape/cl_particle.cpp
	../client/battlescape/cl_radar.cpp
	../client/battlescape/cl_spawn.cpp
	../client/battlescape/cl_ugv.cpp
	../client/battlescape/cl_view.cpp
	../client/battlescape/events/e_main.cpp
	../client/battlescape/events/e_parse.cpp
	../client/battlescape/events/e_server.cpp
	../client/battlescape/events/event/actor/e_event_actoradd.cpp
	../client/battlescape/events/event/actor/e_event_actorappear.cpp
	../client/battlescape/events/event/actor/e_event_actorclientaction.cpp
	../client/battlescape/events/event/actor/e_event_actordie.cpp
	../client/battlescape/events/event/actor/e_event_actorendshot.cpp
	../client/battlescape/events/event/actor/e_event_actormove.cpp
	../client/battlescape/events/event/actor/e_event_actorreactionfireabortshot.cpp
	../client/battlescape/events/event/actor/e_event_actorreactionfireaddtarget.cpp
	../client/battlescape/events/event/actor/e_event_actorreactionfirechange.cpp
	../client/battlescape/events/event/actor/e_event_actorreactionfireremovetarget.cpp
	../client/battlescape/events/event/actor/e_event_actorreactionfiretargetupdate.cpp
	../client/battlescape/events/event/actor/e_event_actorreservationchange.cpp
	../client/battlescape/events/event/actor/e_event_actorresetclientaction.cpp
	../client/battlescape/events/event/actor/e_event_actorrevitalised.cpp
	../client/battlescape/events/event/actor/e_event_actorshoot.cpp
	../client/battlescape/events/event/actor/e_event_actorshoothidden.cpp
	../client/battlescape/events/event/actor/e_event_actorstartshoot.cpp
	../client/battlescape/events/event/actor/e_event_actorstatechange.cpp
	../client/battlescape/events/event/actor/e_event_actorstats.cpp
	../client/battlescape/events/event/actor/e_event_actorthrow.cpp
	../client/battlescape/events/event/actor/e_event_actorturn.cpp
	../client/battlescape/events/event/actor/e_event_actorwound.cpp
	../client/battlescape/events/event/inventory/e_event_invadd.cpp
	../client/battlescape/events/event/inventory/e_event_invammo.cpp
	../client/battlescape/events/event/inventory/e_event_invdel.cpp
	../client/battlescape/events/event/inventory/e_event_invreload.cpp
	../client/battlescape/events/event/player/e_event_centerview.cpp
	../client/battlescape/events/event/player/e_event_doendround.cpp
	../client/battlescape/events/event/player/e_event_endroundannounce.cpp
	../client/battlescape/events/event/player/e_event_reset.cpp
	../client/battlescape/events/event/player/e_event_results.cpp
	../client/battlescape/events/event/player/e_event_startgame.cpp
	../client/battlescape/events/event/world/e_event_addbrushmodel.cpp
	../client/battlescape/events/event/world/e_event_addedict.cpp
	../client/battlescape/events/event/world/e_event_cameraappear.cpp
	../client/battlescape/events/event/world/e_event_doorclose.cpp
	../client/battlescape/events/event/world/e_event_dooropen.cpp
	../client/battlescape/events/event/world/e_event_entappear.cpp
	../client/battlescape/events/event/world/e_event_entdestroy.cpp
	../client/battlescape/events/event/world/e_event_entperish.cpp
	../client/battlescape/events/event/world/e_event_explode.cpp
	../client/battlescape/events/event/world/e_event_particleappear.cpp
	../client/battlescape/events/event/world/e_event_particlespawn.cpp
	../client/battlescape/events/event/world/e_event_sound.cpp
	../client/cgame/campaign/aliencargo.cpp
	../client/cgame/campaign/itemcargo.cpp
	../client/cgame/campaign/aliencontainment.cpp
	../client/cgame/campaign/cl_game_campaign.cpp
	../client/cgame/campaign/cp_aircraft.cpp
	../client/cgame/campaign/cp_aircraft_callbacks.cpp
	../client/cgame/campaign/cp_airfight.cpp
	../client/cgame/campaign/cp_alien_interest.cpp
	../client/cgame/campaign/cp_alienbase.cpp
	../client/cgame/campaign/cp_aliencont.cpp
	../client/cgame/campaign/cp_aliencont_callbacks.cpp
	../client/cgame/campaign/cp_auto_mission.cpp
	../client/cgame/campaign/cp_base.cpp
	../client/cgame/campaign/cp_base_callbacks.cpp
	../client/cgame/campaign/cp_basedefence_callbacks.cpp
	../client/cgame/campaign/cp_building.cpp
	../client/cgame/campaign/cp_campaign.cpp
	../client/cgame/campaign/cp_capacity.cpp
	../client/cgame/campaign/cp_cgame_callbacks.cpp
	../client/cgame/campaign/cp_character.cpp
	../client/cgame/campaign/cp_component.cpp
	../client/cgame/campaign/cp_employee.cpp
	../client/cgame/campaign/cp_employee_callbacks.cpp
	../client/cgame/campaign/cp_event.cpp
	../client/cgame/campaign/cp_event_callbacks.cpp
	../client/cgame/campaign/cp_fightequip_callbacks.cpp
	../client/cgame/campaign/cp_geoscape.cpp
	../client/cgame/campaign/cp_hospital.cpp
	../client/cgame/campaign/cp_hospital_callbacks.cpp
	../client/cgame/campaign/cp_installation.cpp
	../client/cgame/campaign/cp_installation_callbacks.cpp
	../client/cgame/campaign/cp_mapfightequip.cpp
	../client/cgame/campaign/cp_market.cpp
	../client/cgame/campaign/cp_market_callbacks.cpp
	../client/cgame/campaign/cp_messageoptions.cpp
	../client/cgame/campaign/cp_messageoptions_callbacks.cpp
	../client/cgame/campaign/cp_messages.cpp
	../client/cgame/campaign/cp_mission_callbacks.cpp
	../client/cgame/campaign/cp_mission_triggers.cpp
	../client/cgame/campaign/cp_missions.cpp
	../client/cgame/campaign/cp_nation.cpp
	../client/cgame/campaign/cp_overlay.cpp
	../client/cgame/campaign/cp_parse.cpp
	../client/cgame/campaign/cp_popup.cpp
	../client/cgame/campaign/cp_produce.cpp
	../client/cgame/campaign/cp_produce_callbacks.cpp
	../client/cgame/campaign/cp_radar.cpp
	../client/cgame/campaign/cp_rank.cpp
	../client/cgame/campaign/cp_research.cpp
	../client/cgame/campaign/cp_research_callbacks.cpp
	../client/cgame/campaign/cp_save.cpp
	../client/cgame/campaign/cp_save_callbacks.cpp
	../client/cgame/campaign/cp_statistics.cpp
	../client/cgame/campaign/cp_team.cpp
	../client/cgame/campaign/cp_team_callbacks.cpp
	../client/cgame/campaign/cp_time.cpp
	../client/cgame/campaign/cp_transfer.cpp
	../client/cgame/campaign/cp_transfer_callbacks.cpp
	../client/cgame/campaign/cp_ufo.cpp
	../client/cgame/campaign/cp_ufo_callbacks.cpp
	../client/cgame/campaign/cp_ufopedia.cpp
	../client/cgame/campaign/cp_uforecovery.cpp
	../client/cgame/campaign/cp_uforecovery_callbacks.cpp
	../client/cgame/campaign/cp_xvi.cpp
	../client/cgame/campaign/missions/cp_mission_baseattack.cpp
	../client/cgame/campaign/missions/cp_mission_buildbase.cpp
	../client/cgame/campaign/missions/cp_mission_harvest.cpp
	../client/cgame/campaign/missions/cp_mission_intercept.cpp
	../client/cgame/campaign/missions/cp_mission_recon.cpp
	../client/cgame/campaign/missions/cp_mission_rescue.cpp
	../client/cgame/campaign/missions/cp_mission_supply.cpp
	../client/cgame/campaign/missions/cp_mission_terror.cpp
	../client/cgame/campaign/missions/cp_mission_ufocarrier.cpp
	../client/cgame/campaign/missions/cp_mission_xvi.cpp
	../client/cgame/cl_game.cpp
	../client/cgame/cl_game_team.cpp
	../client/cgame/cl_map_callbacks.cpp
	../client/cgame/multiplayer/cl_game_multiplayer.cpp
	../client/cgame/multiplayer/mp_callbacks.cpp
	../client/cgame/multiplayer/mp_serverlist.cpp
	../client/cgame/skirmish/cl_game_skirmish.cpp
	../client/cinematic/cl_cinematic.cpp
	../client/cinematic/cl_cinematic_ogm.cpp
	../client/cinematic/cl_cinematic_roq.cpp
	../client/cinematic/cl_sequence.cpp
	../client/cl_console.cpp
	../client/cl_http.cpp
	../client/cl_inventory.cpp
	../client/cl_inventory_callbacks.cpp
	../client/cl_irc.cpp
	../client/cl_language.cpp
	../client/cl_main.cpp
	../client/cl_menu.cpp
	../client/cl_screen.cpp
	../client/cl_team.cpp
	../client/cl_tip.cpp
	../client/cl_tutorials.cpp
	../client/cl_video.cpp
	../client/input/cl_input.cpp
	../client/input/cl_joystick.cpp
	../client/input/cl_keys.cpp
	../client/renderer/r_array.cpp
	../client/renderer/r_bsp.cpp
	../client/renderer/r_corona.cpp
	../client/renderer/r_draw.cpp
	../client/renderer/r_entity.cpp
	../client/renderer/r_flare.cpp
	../client/renderer/r_font.cpp
	../client/renderer/r_framebuffer.cpp
	../client/renderer/r_geoscape.cpp
	../client/renderer/r_grass.cpp
	../client/renderer/r_image.cpp
	../client/renderer/r_light.cpp
	../client/renderer/r_lightmap.cpp
	../client/renderer/r_main.cpp
	../client/renderer/r_material.cpp
	../client/renderer/r_matrix.cpp
	../client/renderer/r_mesh.cpp
	../client/renderer/r_mesh_anim.cpp
	../client/renderer/r_misc.cpp
	../client/renderer/r_model.cpp
	../client/renderer/r_model_alias.cpp
	../client/renderer/r_model_brush.cpp
	../client/renderer/r_model_md2.cpp
	../client/renderer/r_model_md3.cpp
	../client/renderer/r_model_obj.cpp
	../client/renderer/r_particle.cpp
	../client/renderer/r_program.cpp
	../client/renderer/r_sdl.cpp
	../client/renderer/r_sphere.cpp
	../client/renderer/r_state.cpp
	../client/renderer/r_surface.cpp
	../client/renderer/r_thread.cpp
	../client/renderer/r_weather.cpp
	../client/sound/s_main.cpp
	../client/sound/s_mix.cpp
	../client/sound/s_mumble.cpp
	../client/sound/s_music.cpp
	../client/sound/s_sample.cpp
	../client/ui/node/ui_node_abstractnode.cpp
	../client/ui/node/ui_node_abstractoption.cpp
	../client/ui/node/ui_node_abstractscrollable.cpp
	../client/ui/node/ui_node_abstractscrollbar.cpp
	../client/ui/node/ui_node_abstractvalue.cpp
	../client/ui/node/ui_node_bar.cpp
	../client/ui/node/ui_node_base.cpp
	../client/ui/node/ui_node_baseinventory.cpp
	../client/ui/node/ui_node_battlescape.cpp
	../client/ui/node/ui_node_button.cpp
	../client/ui/node/ui_node_checkbox.cpp
	../client/ui/node/ui_node_container.cpp
	../client/ui/node/ui_node_controls.cpp
	../client/ui/node/ui_node_data.cpp
	../client/ui/node/ui_node_editor.cpp
	../client/ui/node/ui_node_geoscape.cpp
	../client/ui/node/ui_node_image.cpp
	../client/ui/node/ui_node_item.cpp
	../client/ui/node/ui_node_linechart.cpp
	../client/ui/node/ui_node_material_editor.cpp
	../client/ui/node/ui_node_messagelist.cpp
	../client/ui/node/ui_node_model.cpp
	../client/ui/node/ui_node_option.cpp
	../client/ui/node/ui_node_optionlist.cpp
	../client/ui/node/ui_node_optiontree.cpp
	../client/ui/node/ui_node_panel.cpp
	../client/ui/node/ui_node_radar.cpp
	../client/ui/node/ui_node_radiobutton.cpp
	../client/ui/node/ui_node_rows.cpp
	../client/ui/node/ui_node_selectbox.cpp
	../client/ui/node/ui_node_sequence.cpp
	../client/ui/node/ui_node_special.cpp
	../client/ui/node/ui_node_spinner.cpp
	../client/ui/node/ui_node_string.cpp
	../client/ui/node/ui_node_tab.cpp
	../client/ui/node/ui_node_tbar.cpp
	../client/ui/node/ui_node_text.cpp
	../client/ui/node/ui_node_text2.cpp
	../client/ui/node/ui_node_textentry.cpp
	../client/ui/node/ui_node_textlist.cpp
	../client/ui/node/ui_node_texture.cpp
	../client/ui/node/ui_node_timer.cpp
	../client/ui/node/ui_node_video.cpp
	../client/ui/node/ui_node_vscrollbar.cpp
	../client/ui/node/ui_node_window.cpp
	../client/ui/node/ui_node_zone.cpp
	../client/ui/swig/ui_lua_shared.cpp
	../client/ui/ui_actions.cpp
	../client/ui/ui_behaviour.cpp
	../client/ui/ui_components.cpp
	../client/ui/ui_data.cpp
	../client/ui/ui_dragndrop.cpp
	../client/ui/ui_draw.cpp
	../client/ui/ui_expression.cpp
	../client/ui/ui_font.cpp
	../client/ui/ui_input.cpp
	../client/ui/ui_lua.cpp
	../client/ui/ui_main.cpp
	../client/ui/ui_node.cpp
	../client/ui/ui_nodes.cpp
	../client/ui/ui_parse.cpp
	../client/ui/ui_popup.cpp
	../client/ui/ui_render.cpp
	../client/ui/ui_sound.cpp
	../client/ui/ui_sprite.cpp
	../client/ui/ui_timer.cpp
	../client/ui/ui_tooltip.cpp
	../client/ui/ui_windows.cpp
	../client/web/web_cgame.cpp
	../client/web/web_main.cpp
	../client/cl_lua.cpp
	../client/DateTime.cpp

	../common/binaryexpressionparser.cpp
	../common/bsp.cpp
	../common/cmd.cpp
	../common/cmodel.cpp
	../common/common.cpp
	../common/cvar.cpp
	../common/dbuffer.cpp
	../common/files.cpp
	../common/grid.cpp
	../common/hashtable.cpp
	../common/http.cpp
	../common/ioapi.cpp
	../common/list.cpp
	../common/md4.cpp
	../common/md5.cpp
	../common/mem.cpp
	../common/msg.cpp
	../common/net.cpp
	../common/netpack.cpp
	../common/pqueue.cpp
	../common/routing.cpp
	../common/scripts.cpp
	../common/scripts_lua.cpp
	../common/sha1.cpp
	../common/sha2.cpp
	../common/tracing.cpp
	../common/unzip.cpp
	../common/xml.cpp

	../game/chr_shared.cpp
	../game/g_actor.cpp
	../game/g_ai.cpp
	../game/g_ai_lua.cpp
	../game/g_camera.cpp
	../game/g_client.cpp
	../game/g_cmds.cpp
	../game/g_combat.cpp
	../game/g_edicts.cpp
	../game/g_events.cpp
	../game/g_func.cpp
	../game/g_health.cpp
	../game/g_inventory.cpp
	../game/g_main.cpp
	../game/g_match.cpp
	../game/g_mission.cpp
	../game/g_morale.cpp
	../game/g_move.cpp
	../game/g_reaction.cpp
	../game/g_round.cpp
	../game/g_spawn.cpp
	../game/g_stats.cpp
	../game/g_svcmds.cpp
	../game/g_trigger.cpp
	../game/g_utils.cpp
	../game/g_vis.cpp
	../game/inv_shared.cpp
	../game/inventory.cpp
	../game/q_shared.cpp

	../server/sv_ccmds.cpp
	../server/sv_game.cpp
	../server/sv_init.cpp
	../server/sv_log.cpp
	../server/sv_main.cpp
	../server/sv_mapcycle.cpp
	../server/sv_rma.cpp
	../server/sv_send.cpp
	../server/sv_user.cpp
	../server/sv_world.cpp

	../shared/aabb.cpp
	../shared/bfd.cpp
	../shared/byte.cpp
	../shared/entitiesdef.cpp
	../shared/images.cpp
	../shared/infostring.cpp
	../shared/mathlib.cpp
	../shared/mathlib_extra.cpp
	../shared/parse.cpp
	../shared/shared.cpp
	../shared/stringhunk.cpp
	../shared/utf8.cpp

	test_all.cpp
	test_campaign.cpp
	test_character.cpp
	test_dbuffer.cpp
	test_events.cpp
	test_game.cpp
	test_generic.cpp
	test_inventory.cpp
	test_mapdef.cpp
	test_parser.cpp
	test_particles.cpp
	test_renderer.cpp
	test_rma.cpp
	test_routing.cpp
	test_scripts.cpp
	test_shared.cpp
	test_ui.cpp
	test_ui_level2.cpp
	test_webapi.cpp

	../libs/mumble/libmumblelink.c
)

# platform specific sources
if(WIN32)
	LIST(APPEND UFOTESTALL_SOURCES
		../ports/windows/win_backtrace.cpp
		../ports/windows/win_console.cpp
		../ports/windows/win_shared.cpp
	)
elseif(UNIX)
	LIST(APPEND UFOTESTALL_SOURCES
		../ports/unix/unix_console.cpp
		../ports/unix/unix_files.cpp
		../ports/unix/unix_shared.cpp
		../ports/unix/unix_main.cpp
	)
elseif(SOLARIS)
	LIST(APPEND UFOTESTALL_SOURCES
		../ports/unix/unix_console.cpp
		../ports/unix/unix_files.cpp
		../ports/unix/unix_shared.cpp
		../ports/unix/unix_main.cpp
	)
elseif(APPLE)
	LIST(APPEND UFOTESTALL_SOURCES
		../ports/unix/unix_console.cpp
		../ports/unix/unix_files.cpp
		../ports/unix/unix_shared.cpp
		../ports/unix/unix_main.cpp
	)
endif()

# ufotestall is executable
add_executable(${PROJECT_NAME} EXCLUDE_FROM_ALL ${UFOTESTALL_SOURCES})
add_dependencies(${PROJECT_NAME} "game")

# For generic no-config case (e.g. with mingw, gcc, ...)
set_target_properties(${PROJECT_NAME} PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
# For multi-config builds like msvc
foreach(OUTPUTCONFIG ${CMAKE_CONFIGURATION_TYPES})
	string(TOUPPER ${OUTPUTCONFIG} OUTPUTCONFIG)
	set_target_properties(${PROJECT_NAME} PROPERTIES RUNTIME_OUTPUT_DIRECTORY_${OUTPUTCONFIG}  ${CMAKE_BINARY_DIR})
endforeach()

target_compile_definitions(${PROJECT_NAME} PRIVATE -DCOMPILE_UFO)
target_compile_definitions(${PROJECT_NAME} PRIVATE -DCOMPILE_UNITTESTS)
target_compile_definitions(${PROJECT_NAME} PRIVATE -DHARD_LINKED_GAME)
target_compile_definitions(${PROJECT_NAME} PRIVATE -DHARD_LINKED_CGAME)

target_include_directories(${PROJECT_NAME}
	PRIVATE ../libs/mumble
	PRIVATE ${LUA_INCLUDE_DIR}
	PRIVATE ${SDL_INCLUDE_DIR}
	PRIVATE ${SDLMIXER_INCLUDE_DIR}
	PRIVATE ${SDLTTF_INCLUDE_DIR}
	PRIVATE ${CURL_INCLUDE_DIRS}
	PRIVATE ${VORBIS_INCLUDE_DIR}
	PRIVATE ${OGG_INCLUDE_DIR}
	PRIVATE ${JPEG_INCLUDE_DIRS}
	PRIVATE ${PNG_INCLUDE_DIRS}
	PRIVATE ${Intl_INCLUDE_DIRS}
	PRIVATE ${ZLIB_INCLUDE_DIRS}
	PRIVATE ${X11_INCLUDE_DIR}
	PRIVATE ${MXML_INCLUDE_DIR}
	PRIVATE ${GTEST_INCLUDE_DIRS}
)

if(MINGW)
	target_link_libraries(${PROJECT_NAME}
		mingw32
	)
endif()

target_link_libraries(${PROJECT_NAME}
	${LUA_LIBRARY}
	${SDL_LIBRARY_MAIN}
	${SDL_LIBRARY}
	${SDL_MIXER_LIBRARIES}
	${SDL_TTF_LIBRARIES}
	${CURL_LIBRARIES}
	${VORBIS_LIBRARY}
	${OGG_LIBRARIES}
	${JPEG_LIBRARIES}
	${PNG_LIBRARIES}
	${ZLIB_LIBRARIES}
	${X11_LIBRARIES}
	${MXML_LIBRARY}
	${GTEST_LIBRARIES}
)

if(WIN32)
	target_link_libraries(${PROJECT_NAME}
		opengl32
		imm32
		${Intl_LIBRARIES}
		version
		winmm
		Ws2_32
	)
elseif(APPLE)
	target_link_libraries(${PROJECT_NAME}
		${Intl_LIBRARIES}
	)
elseif(UNIX)
	target_link_libraries(${PROJECT_NAME}
		GL
	)
	if(NOT CMAKE_SYSTEM_NAME MATCHES "OpenBSD")
		target_link_libraries(${PROJECT_NAME}
			rt
		)
	endif()
endif()
