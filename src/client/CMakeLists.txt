# Copyright (C) 2002-2023 UFO: Alien Invasion.
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

project(ufo DESCRIPTION "UFO:AI main game client and server.")

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
find_package(Lua51)
find_package(SDL_mixer)
find_package(SDL_ttf)
find_package(VORBIS)

set(UFOAI_SOURCES
	battlescape/cl_actor.cpp
	battlescape/cl_battlescape.cpp
	battlescape/cl_camera.cpp
	battlescape/cl_hud.cpp
	battlescape/cl_hud_callbacks.cpp
	battlescape/cl_localentity.cpp
	battlescape/cl_parse.cpp
	battlescape/cl_particle.cpp
	battlescape/cl_radar.cpp
	battlescape/cl_spawn.cpp
	battlescape/cl_ugv.cpp
	battlescape/cl_view.cpp
	battlescape/events/e_main.cpp
	battlescape/events/e_parse.cpp
	battlescape/events/e_server.cpp
	battlescape/events/event/actor/e_event_actoradd.cpp
	battlescape/events/event/actor/e_event_actorappear.cpp
	battlescape/events/event/actor/e_event_actorclientaction.cpp
	battlescape/events/event/actor/e_event_actordie.cpp
	battlescape/events/event/actor/e_event_actorendshot.cpp
	battlescape/events/event/actor/e_event_actormove.cpp
	battlescape/events/event/actor/e_event_actorreactionfireabortshot.cpp
	battlescape/events/event/actor/e_event_actorreactionfireaddtarget.cpp
	battlescape/events/event/actor/e_event_actorreactionfirechange.cpp
	battlescape/events/event/actor/e_event_actorreactionfireremovetarget.cpp
	battlescape/events/event/actor/e_event_actorreactionfiretargetupdate.cpp
	battlescape/events/event/actor/e_event_actorreservationchange.cpp
	battlescape/events/event/actor/e_event_actorresetclientaction.cpp
	battlescape/events/event/actor/e_event_actorrevitalised.cpp
	battlescape/events/event/actor/e_event_actorshoot.cpp
	battlescape/events/event/actor/e_event_actorshoothidden.cpp
	battlescape/events/event/actor/e_event_actorstartshoot.cpp
	battlescape/events/event/actor/e_event_actorstatechange.cpp
	battlescape/events/event/actor/e_event_actorstats.cpp
	battlescape/events/event/actor/e_event_actorthrow.cpp
	battlescape/events/event/actor/e_event_actorturn.cpp
	battlescape/events/event/actor/e_event_actorwound.cpp
	battlescape/events/event/inventory/e_event_invadd.cpp
	battlescape/events/event/inventory/e_event_invammo.cpp
	battlescape/events/event/inventory/e_event_invdel.cpp
	battlescape/events/event/inventory/e_event_invreload.cpp
	battlescape/events/event/player/e_event_centerview.cpp
	battlescape/events/event/player/e_event_doendround.cpp
	battlescape/events/event/player/e_event_endroundannounce.cpp
	battlescape/events/event/player/e_event_reset.cpp
	battlescape/events/event/player/e_event_results.cpp
	battlescape/events/event/player/e_event_startgame.cpp
	battlescape/events/event/world/e_event_addbrushmodel.cpp
	battlescape/events/event/world/e_event_addedict.cpp
	battlescape/events/event/world/e_event_cameraappear.cpp
	battlescape/events/event/world/e_event_doorclose.cpp
	battlescape/events/event/world/e_event_dooropen.cpp
	battlescape/events/event/world/e_event_entappear.cpp
	battlescape/events/event/world/e_event_entdestroy.cpp
	battlescape/events/event/world/e_event_entperish.cpp
	battlescape/events/event/world/e_event_explode.cpp
	battlescape/events/event/world/e_event_particleappear.cpp
	battlescape/events/event/world/e_event_particlespawn.cpp
	battlescape/events/event/world/e_event_sound.cpp
	cgame/campaign/aliencargo.cpp
	cgame/campaign/itemcargo.cpp
	cgame/campaign/aliencontainment.cpp
	cgame/campaign/cl_game_campaign.cpp
	cgame/campaign/cp_aircraft.cpp
	cgame/campaign/cp_aircraft_callbacks.cpp
	cgame/campaign/cp_airfight.cpp
	cgame/campaign/cp_alien_interest.cpp
	cgame/campaign/cp_alienbase.cpp
	cgame/campaign/cp_aliencont.cpp
	cgame/campaign/cp_aliencont_callbacks.cpp
	cgame/campaign/cp_auto_mission.cpp
	cgame/campaign/cp_base.cpp
	cgame/campaign/cp_base_callbacks.cpp
	cgame/campaign/cp_basedefence_callbacks.cpp
	cgame/campaign/cp_building.cpp
	cgame/campaign/cp_campaign.cpp
	cgame/campaign/cp_capacity.cpp
	cgame/campaign/cp_cgame_callbacks.cpp
	cgame/campaign/cp_character.cpp
	cgame/campaign/cp_component.cpp
	cgame/campaign/cp_employee.cpp
	cgame/campaign/cp_employee_callbacks.cpp
	cgame/campaign/cp_event.cpp
	cgame/campaign/cp_event_callbacks.cpp
	cgame/campaign/cp_fightequip_callbacks.cpp
	cgame/campaign/cp_geoscape.cpp
	cgame/campaign/cp_hospital.cpp
	cgame/campaign/cp_hospital_callbacks.cpp
	cgame/campaign/cp_installation.cpp
	cgame/campaign/cp_installation_callbacks.cpp
	cgame/campaign/cp_mapfightequip.cpp
	cgame/campaign/cp_market.cpp
	cgame/campaign/cp_market_callbacks.cpp
	cgame/campaign/cp_messageoptions.cpp
	cgame/campaign/cp_messageoptions_callbacks.cpp
	cgame/campaign/cp_messages.cpp
	cgame/campaign/cp_mission_callbacks.cpp
	cgame/campaign/cp_mission_triggers.cpp
	cgame/campaign/cp_missions.cpp
	cgame/campaign/cp_nation.cpp
	cgame/campaign/cp_overlay.cpp
	cgame/campaign/cp_parse.cpp
	cgame/campaign/cp_popup.cpp
	cgame/campaign/cp_produce.cpp
	cgame/campaign/cp_produce_callbacks.cpp
	cgame/campaign/cp_radar.cpp
	cgame/campaign/cp_rank.cpp
	cgame/campaign/cp_research.cpp
	cgame/campaign/cp_research_callbacks.cpp
	cgame/campaign/cp_save.cpp
	cgame/campaign/cp_save_callbacks.cpp
	cgame/campaign/cp_statistics.cpp
	cgame/campaign/cp_team.cpp
	cgame/campaign/cp_team_callbacks.cpp
	cgame/campaign/cp_time.cpp
	cgame/campaign/cp_transfer.cpp
	cgame/campaign/cp_transfer_callbacks.cpp
	cgame/campaign/cp_ufo.cpp
	cgame/campaign/cp_ufo_callbacks.cpp
	cgame/campaign/cp_ufopedia.cpp
	cgame/campaign/cp_uforecovery.cpp
	cgame/campaign/cp_uforecovery_callbacks.cpp
	cgame/campaign/cp_xvi.cpp
	cgame/campaign/missions/cp_mission_baseattack.cpp
	cgame/campaign/missions/cp_mission_buildbase.cpp
	cgame/campaign/missions/cp_mission_harvest.cpp
	cgame/campaign/missions/cp_mission_intercept.cpp
	cgame/campaign/missions/cp_mission_recon.cpp
	cgame/campaign/missions/cp_mission_rescue.cpp
	cgame/campaign/missions/cp_mission_supply.cpp
	cgame/campaign/missions/cp_mission_terror.cpp
	cgame/campaign/missions/cp_mission_ufocarrier.cpp
	cgame/campaign/missions/cp_mission_xvi.cpp
	cgame/cl_game.cpp
	cgame/cl_game_team.cpp
	cgame/cl_map_callbacks.cpp
	cgame/multiplayer/cl_game_multiplayer.cpp
	cgame/multiplayer/mp_callbacks.cpp
	cgame/multiplayer/mp_serverlist.cpp
	cgame/skirmish/cl_game_skirmish.cpp
	cinematic/cl_cinematic.cpp
	cinematic/cl_cinematic_ogm.cpp
	cinematic/cl_cinematic_roq.cpp
	cinematic/cl_sequence.cpp
	cl_console.cpp
	cl_http.cpp
	cl_inventory.cpp
	cl_inventory_callbacks.cpp
	cl_irc.cpp
	cl_language.cpp
	cl_main.cpp
	cl_menu.cpp
	cl_screen.cpp
	cl_team.cpp
	cl_tip.cpp
	cl_tutorials.cpp
	cl_video.cpp
	input/cl_input.cpp
	input/cl_joystick.cpp
	input/cl_keys.cpp
	renderer/r_array.cpp
	renderer/r_bsp.cpp
	renderer/r_corona.cpp
	renderer/r_draw.cpp
	renderer/r_entity.cpp
	renderer/r_flare.cpp
	renderer/r_font.cpp
	renderer/r_framebuffer.cpp
	renderer/r_geoscape.cpp
	renderer/r_grass.cpp
	renderer/r_image.cpp
	renderer/r_light.cpp
	renderer/r_lightmap.cpp
	renderer/r_main.cpp
	renderer/r_material.cpp
	renderer/r_matrix.cpp
	renderer/r_mesh.cpp
	renderer/r_mesh_anim.cpp
	renderer/r_misc.cpp
	renderer/r_model.cpp
	renderer/r_model_alias.cpp
	renderer/r_model_brush.cpp
	renderer/r_model_md2.cpp
	renderer/r_model_md3.cpp
	renderer/r_model_obj.cpp
	renderer/r_particle.cpp
	renderer/r_program.cpp
	renderer/r_sdl.cpp
	renderer/r_sphere.cpp
	renderer/r_state.cpp
	renderer/r_surface.cpp
	renderer/r_thread.cpp
	renderer/r_weather.cpp
	sound/s_main.cpp
	sound/s_mix.cpp
	sound/s_mumble.cpp
	sound/s_music.cpp
	sound/s_sample.cpp
	ui/node/ui_node_abstractnode.cpp
	ui/node/ui_node_abstractoption.cpp
	ui/node/ui_node_abstractscrollable.cpp
	ui/node/ui_node_abstractscrollbar.cpp
	ui/node/ui_node_abstractvalue.cpp
	ui/node/ui_node_bar.cpp
	ui/node/ui_node_base.cpp
	ui/node/ui_node_baseinventory.cpp
	ui/node/ui_node_battlescape.cpp
	ui/node/ui_node_button.cpp
	ui/node/ui_node_checkbox.cpp
	ui/node/ui_node_container.cpp
	ui/node/ui_node_controls.cpp
	ui/node/ui_node_data.cpp
	ui/node/ui_node_editor.cpp
	ui/node/ui_node_geoscape.cpp
	ui/node/ui_node_image.cpp
	ui/node/ui_node_item.cpp
	ui/node/ui_node_linechart.cpp
	ui/node/ui_node_material_editor.cpp
	ui/node/ui_node_messagelist.cpp
	ui/node/ui_node_model.cpp
	ui/node/ui_node_option.cpp
	ui/node/ui_node_optionlist.cpp
	ui/node/ui_node_optiontree.cpp
	ui/node/ui_node_panel.cpp
	ui/node/ui_node_radar.cpp
	ui/node/ui_node_radiobutton.cpp
	ui/node/ui_node_rows.cpp
	ui/node/ui_node_selectbox.cpp
	ui/node/ui_node_sequence.cpp
	ui/node/ui_node_special.cpp
	ui/node/ui_node_spinner.cpp
	ui/node/ui_node_string.cpp
	ui/node/ui_node_tab.cpp
	ui/node/ui_node_tbar.cpp
	ui/node/ui_node_text.cpp
	ui/node/ui_node_text2.cpp
	ui/node/ui_node_textentry.cpp
	ui/node/ui_node_textlist.cpp
	ui/node/ui_node_texture.cpp
	ui/node/ui_node_timer.cpp
	ui/node/ui_node_video.cpp
	ui/node/ui_node_vscrollbar.cpp
	ui/node/ui_node_window.cpp
	ui/node/ui_node_zone.cpp
	ui/swig/ui_lua_shared.cpp
	ui/ui_actions.cpp
	ui/ui_behaviour.cpp
	ui/ui_components.cpp
	ui/ui_data.cpp
	ui/ui_dragndrop.cpp
	ui/ui_draw.cpp
	ui/ui_expression.cpp
	ui/ui_font.cpp
	ui/ui_input.cpp
	ui/ui_lua.cpp
	ui/ui_main.cpp
	ui/ui_node.cpp
	ui/ui_nodes.cpp
	ui/ui_parse.cpp
	ui/ui_popup.cpp
	ui/ui_render.cpp
	ui/ui_sound.cpp
	ui/ui_sprite.cpp
	ui/ui_timer.cpp
	ui/ui_tooltip.cpp
	ui/ui_windows.cpp
	web/web_cgame.cpp
	web/web_main.cpp
	cl_lua.cpp
	DateTime.cpp

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
	../shared/images.cpp
	../shared/infostring.cpp
	../shared/mathlib.cpp
	../shared/mathlib_extra.cpp
	../shared/parse.cpp
	../shared/shared.cpp
	../shared/stringhunk.cpp
	../shared/utf8.cpp

	../libs/mumble/libmumblelink.c

	../libs/mxml/mxml-attr.c
	../libs/mxml/mxml-entity.c
	../libs/mxml/mxml-file.c
	../libs/mxml/mxml-index.c
	../libs/mxml/mxml-node.c
	../libs/mxml/mxml-private.c
	../libs/mxml/mxml-search.c
	../libs/mxml/mxml-set.c
	../libs/mxml/mxml-string.c
)

# platform specific sources
if(WIN32)
	LIST(APPEND UFOAI_SOURCES
		../ports/windows/win_backtrace.cpp
		../ports/windows/win_console.cpp
		../ports/windows/win_main.cpp
		../ports/windows/win_shared.cpp
		../ports/windows/ufo.rc
	)
elseif(UNIX)
	LIST(APPEND UFOAI_SOURCES
		../ports/linux/linux_main.cpp
		../ports/unix/unix_console.cpp
		../ports/unix/unix_files.cpp
		../ports/unix/unix_shared.cpp
		../ports/unix/unix_main.cpp
	)
elseif(SOLARIS)
	LIST(APPEND UFOAI_SOURCES
		../ports/solaris/solaris_main.cpp
		../ports/unix/unix_console.cpp
		../ports/unix/unix_files.cpp
		../ports/unix/unix_shared.cpp
		../ports/unix/unix_main.cpp
	)
elseif(APPLE)
	LIST(APPEND UFOAI_SOURCES
		../ports/macosx/osx_main.cpp
		../ports/macosx/osx_shared.cpp
		../ports/unix/unix_console.cpp
		../ports/unix/unix_files.cpp
		../ports/unix/unix_shared.cpp
		../ports/unix/unix_main.cpp
	)
endif()

# ufo is executable
add_executable(${PROJECT_NAME} ${UFOAI_SOURCES})
add_dependencies(${PROJECT_NAME} "game")

# For generic no-config case (e.g. with mingw, gcc, ...)
set_target_properties(${PROJECT_NAME} PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
# For multi-config builds like msvc
foreach(OUTPUTCONFIG ${CMAKE_CONFIGURATION_TYPES})
	string(TOUPPER ${OUTPUTCONFIG} OUTPUTCONFIG)
	set_target_properties(${PROJECT_NAME} PROPERTIES RUNTIME_OUTPUT_DIRECTORY_${OUTPUTCONFIG}  ${CMAKE_BINARY_DIR})
endforeach()

# since we embed our manifest, we should not generate a default one
if(WIN32 AND MSVC)
	set_target_properties(${PROJECT_NAME} PROPERTIES LINK_FLAGS "/MANIFEST:NO")
endif()

target_compile_definitions(${PROJECT_NAME} PRIVATE -DCOMPILE_UFO)
target_compile_definitions(${PROJECT_NAME} PRIVATE -DHARD_LINKED_CGAME)
target_compile_definitions(${PROJECT_NAME} PRIVATE -DPATH_MANIFEST_FILE="${CMAKE_SOURCE_DIR}/build/projects/ufo.exe.manifest")
target_compile_definitions(${PROJECT_NAME} PRIVATE -DPATH_UFOICON_FILE="${CMAKE_SOURCE_DIR}/build/projects/ufo.ico")
target_compile_definitions(${PROJECT_NAME} PRIVATE -DPATH_UFODEDICON_FILE="${CMAKE_SOURCE_DIR}/build/projects/ufoded.ico")

target_include_directories(${PROJECT_NAME}
	PRIVATE ../libs/mumble
	PRIVATE ../libs/mxml
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
	PRIVATE ${OPENGL_INCLUDE_DIR}
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
	${OPENGL_LIBRARY}
)

if(WIN32 AND MSVC)
	target_link_libraries(${PROJECT_NAME}
		opengl32
		${Intl_LIBRARIES}
		winmm
		Ws2_32
	)
elseif(WIN32)
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
	if(CMAKE_SYSTEM_NAME MATCHES "BSD")
		target_link_libraries(${PROJECT_NAME}
			${Intl_LIBRARIES}
		)
	endif()
endif()
