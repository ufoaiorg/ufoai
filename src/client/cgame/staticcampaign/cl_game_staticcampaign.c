/**
 * @file cl_game_staticcampaign.c
 * @brief Singleplayer static campaign game type code
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
#include "../campaign/cp_cgame_callbacks.h"
#include "../campaign/cp_campaign.h"
#include "cl_game_staticcampaign.h"
#include "scp_missions.h"
#include "scp_parse.h"
#include "scp_shared.h"

staticCampaignData_t* scd;

static void GAME_SCP_InitStartup (void)
{
	static saveSubsystems_t mission_subsystemXML = {"staticcampaign", SCP_Save, SCP_Load};

	GAME_CP_InitStartup();

	SAV_AddSubsystem(&mission_subsystemXML);

	ccs.missionSpawnCallback = SCP_SpawnNewMissions;
	ccs.missionResultCallback = SCP_CampaignProgress;
	scd = Mem_Alloc(sizeof(*scd));
}

static void GAME_SCP_Shutdown (void)
{
	GAME_CP_Shutdown();
	Mem_Free(scd);
}

static void GAME_SCP_Frame (float secondsSinceLastFrame)
{
	if (!scd->initialized && CP_IsRunning()) {
		SCP_Parse();

		SCP_CampaignActivateFirstStage();

		scd->initialized = qtrue;
	}

	GAME_CP_Frame(secondsSinceLastFrame);
}

#ifndef HARD_LINKED_CGAME
const cgame_export_t *GetCGameAPI (const cgame_import_t *import)
#else
const cgame_export_t *GetCGameStaticCampaignAPI (const cgame_import_t *import)
#endif
{
	static cgame_export_t e;

	OBJZERO(e);

	e.name = "Static Campaign mode";
	e.menu = "staticcampaigns";
	e.Init = GAME_SCP_InitStartup;
	e.Shutdown = GAME_SCP_Shutdown;
	e.Spawn = GAME_CP_Spawn;
	e.Results = GAME_CP_Results;
	e.IsItemUseable = GAME_CP_ItemIsUseable;
	e.GetModelForItem = GAME_CP_GetItemModel;
	e.GetEquipmentDefinition = GAME_CP_GetEquipmentDefinition;
	e.UpdateCharacterValues = GAME_CP_CharacterCvars;
	e.IsTeamKnown = GAME_CP_TeamIsKnown;
	e.Drop = GAME_CP_Drop;
	e.InitializeBattlescape = GAME_CP_InitializeBattlescape;
	e.RunFrame = GAME_SCP_Frame;
	e.GetTeamDef = GAME_CP_GetTeamDef;

	cgImport = import;

	return &e;
}
