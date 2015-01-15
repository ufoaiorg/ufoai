/**
 * @file
 * @brief Singleplayer campaign game type code
 */

/*
Copyright (C) 2002-2015 UFO: Alien Invasion.

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

extern void GEO_Draw(geoscapeData_t* data);
extern void GEO_DrawMarkers(const uiNode_t* node);
extern void GEO_Click(const uiNode_t* node, int x, int y, const vec2_t pos);

CGAME_HARD_LINKED_FUNCTIONS

#ifndef HARD_LINKED_CGAME
/* These are here to allow common code to link */
linkedList_t* LIST_Add (linkedList_t** listDest, void const* data, size_t length)
{
	return cgi->LIST_Add(listDest, data, length);
}
void FS_CloseFile (qFILE*  f)
{
	cgi->FS_CloseFile(f);
}
/* Used by macros */
void LIST_Sort (linkedList_t** list, linkedListSort_t sorter, const void* userData)
{
	cgi->LIST_Sort(list, sorter, userData);
}
linkedList_t* LIST_CopyStructure (linkedList_t* src)
{
	return cgi->LIST_CopyStructure(src);
}
void* _Mem_Alloc (size_t size, bool zeroFill, memPool_t* pool, const int tagNum, const char* fileName, const int fileLine)
{
	return cgi->Alloc(size, zeroFill, pool, tagNum, fileName, fileLine);
}
int Com_GetMapDefNumber (void)
{
	return cgi->csi->numMDs;
}
mapDef_t* Com_GetMapDefByIDX (int index)
{
	return &cgi->csi->mds[index];
}

const cgame_export_t* GetCGameAPI (const cgame_import_t* import)
#else
const cgame_export_t* GetCGameCampaignAPI (const cgame_import_t* import)
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
	e.MapDraw = GEO_Draw;
	e.MapDrawMarkers = GEO_DrawMarkers;
	e.MapClick = GEO_Click;

	cgi = import;

	return &e;
}
