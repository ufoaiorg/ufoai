/**
 * @file cl_game_skirmish.c
 * @brief Skirmish game type implementation
 */

/*
Copyright (C) 2002-2007 UFO: Alien Invasion team.

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

#include "client.h"
#include "cl_global.h"
#include "cl_game_skirmish.h"
#include "renderer/r_overlay.h"

#define MAX_CREDITS 10000000

/**
 * @brief Starts a new skirmish game
 * @todo Check the stuff in this function - maybe not every function call
 * is needed here or maybe some are missing
 */
static void CL_GameSkirmish_f (void)
{
	base_t* base;
	char map[MAX_VAR];
	mapDef_t *md;
	int i;

	assert(GAME_IsSkirmish());
	assert(cls.currentSelectedMap >= 0);
	assert(cls.currentSelectedMap < MAX_MAPDEFS);

	md = &csi.mds[cls.currentSelectedMap];
	if (!md)
		return;

	assert(md->map);
	Com_sprintf(map, sizeof(map), "map %s %s %s;", mn_serverday->integer ? "day" : "night", md->map, md->param ? md->param : "");

	/* get campaign - they are already parsed here */
	curCampaign = CL_GetCampaign(cl_campaign->string);
	if (!curCampaign) {
		Com_Printf("CL_GameSkirmish_f: Could not find campaign '%s'\n", cl_campaign->string);
		return;
	}

	Cvar_Set("cl_team", curCampaign->team);

	memset(&ccs, 0, sizeof(ccs));
	ccs.gametype = GAME_SKIRMISH;
	CL_ReadSinglePlayerData();

	/* create employees and clear bases */
	B_NewBases();

	base = B_GetBaseByIDX(0);
	baseCurrent = base;
	gd.numAircraft = 0;

	CL_GameInit(qfalse);
	RS_MarkResearchedAll();

	gd.numBases = 1;

	/* even in skirmish mode we need a little money to build the base */
	CL_UpdateCredits(MAX_CREDITS);

	/* only needed for base dummy creation */
	R_CreateRadarOverlay();

	/* build our pseudo base */
	B_SetUpBase(base, qtrue, qtrue);

	if (!base->numAircraftInBase) {
		Com_Printf("CL_GameSkirmish_f: Error - there is no dropship in base\n");
		return;
	}

	/* we have to set this manually here */
	for (i = 0; i < base->numAircraftInBase; i++) {
		if (base->aircraft[i].type == AIRCRAFT_TRANSPORTER) {
			cls.missionaircraft = &base->aircraft[i];
			break;
		}
	}

	base->aircraftCurrent = cls.missionaircraft;

	if (!cls.missionaircraft || !cls.missionaircraft->homebase) {
		Com_Printf("CL_GameSkirmish_f: Error - could not set the mission aircraft: %i\n", base->numAircraftInBase);
		return;
	}

	/* prepare */
	MN_PopMenu(qtrue);
	Cvar_Set("mn_main_afterdrop", "singleplayermission");

	/* this is no real campaign - but we need the pointer for some of
	 * the previous actions */
	curCampaign = NULL;
	Cbuf_AddText(map);
}

void GAME_SK_InitStartup (void)
{
	Cvar_ForceSet("sv_maxclients", "1");
	Cmd_AddCommand("game_skirmish", CL_GameSkirmish_f, "Start the new skirmish game");
}

void GAME_SK_Shutdown (void)
{
	Cmd_RemoveCommand("game_skirmish");
}
