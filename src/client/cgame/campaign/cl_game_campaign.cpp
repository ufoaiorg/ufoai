/**
 * @file
 * @brief Singleplayer campaign game type code
 */

/*
Copyright (C) 2002-2011 UFO: Alien Invasion.

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

*/

#include "../../cl_shared.h"
#include "../cl_game.h"
#include "cp_cgame_callbacks.h"
#include "cl_game_campaign.h"

extern void MAP_DrawMap(geoscapeData_t* data);
extern void MAP_DrawMapMarkers(const uiNode_t* node);
extern void MAP_MapClick(const uiNode_t* node, int x, int y, const vec2_t pos);

CGAME_HARD_LINKED_FUNCTIONS

#ifndef HARD_LINKED_CGAME
const cgame_export_t *GetCGameAPI (const cgame_import_t *import)
#else
const cgame_export_t *GetCGameCampaignAPI (const cgame_import_t *import)
#endif
{
	static cgame_export_t e;

	OBJZERO(e);

	e.name = "Campaign mode";
	e.menu = "campaign";
	e.Init = GAME_CP_InitStartup;
	e.Shutdown = GAME_CP_Shutdown;
	e.Spawn = GAME_CP_Spawn;
	e.Results = GAME_CP_Results;
	e.IsItemUseable = GAME_CP_ItemIsUseable;
	e.GetModelForItem = GAME_CP_GetItemModel;
	e.GetEquipmentDefinition = GAME_CP_GetEquipmentDefinition;
	e.UpdateCharacterValues = GAME_CP_CharacterCvars;
	e.IsTeamKnown = GAME_CP_TeamIsKnown;
	e.GetSelectedChr = GAME_CP_GetSelectedChr;
	e.Drop = GAME_CP_Drop;
	e.InitializeBattlescape = GAME_CP_InitializeBattlescape;
	e.InitMissionBriefing = GAME_CP_InitMissionBriefing;
	e.RunFrame = GAME_CP_Frame;
	e.HandleBaseClick = GAME_CP_HandleBaseClick;
	e.DrawBase = GAME_CP_DrawBase;
	e.DrawBaseLayout = GAME_CP_DrawBaseLayout;
	e.DrawBaseTooltip = GAME_CP_DrawBaseTooltip;
	e.GetTeamDef = GAME_CP_GetTeamDef;
	e.MapDraw = MAP_DrawMap;
	e.MapDrawMarkers = MAP_DrawMapMarkers;
	e.MapClick = MAP_MapClick;

	cgi = import;

	return &e;
}
