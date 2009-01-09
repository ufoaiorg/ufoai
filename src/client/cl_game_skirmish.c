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
#include "cl_game.h"
#include "cl_team.h"
#include "menu/m_popup.h"

/**
 * @brief This fake aircraft is used to assign soldiers for a skirmish game
 */
static aircraft_t skirmishFakeAircraft;

/**
 * @brief Starts a new skirmish game
 * @todo Check the stuff in this function - maybe not every function call
 * is needed here or maybe some are missing
 */
static void GAME_SK_Start_f (void)
{
	char map[MAX_VAR];
	mapDef_t *md;
	int i;
	const char *name = Cvar_VariableString("cl_equip");

	assert(GAME_IsSkirmish());
	assert(cls.currentSelectedMap >= 0);
	assert(cls.currentSelectedMap < MAX_MAPDEFS);

	md = &csi.mds[cls.currentSelectedMap];
	if (!md)
		return;

	assert(md->map);
	Com_sprintf(map, sizeof(map), "map %s %s %s;", mn_serverday->integer ? "day" : "night", md->map, md->param ? md->param : "");

	memset(&ccs, 0, sizeof(ccs));
	CL_ReadSinglePlayerData();

	GAME_Init(qfalse);
	RS_MarkResearchedAll();

	for (i = 0; i < MAX_ACTIVETEAM; i++) {
		employee_t *employee = E_CreateEmployee(EMPL_SOLDIER, NULL, NULL);
		equipDef_t *ed = INV_GetEquipmentDefinitionByID(name);
		CL_AssignSoldierToAircraft(employee, cls.missionaircraft);
		B_PackInitialEquipment(cls.missionaircraft, ed);
	}

	/* prepare */
	MN_PopMenu(qtrue);
	Cvar_Set("mn_main_afterdrop", "singleplayermission");

	Cbuf_AddText(map);
}

/**
 * @brief Changed the cl_equip cvar to the next/prev equipment definition
 */
static void GAME_SK_ChangeEquip_f (void)
{
	equipDef_t *ed = INV_GetEquipmentDefinitionByID(cl_equip->string);
	int index = ed ? ed - csi.eds : 0;
	if (!Q_strcmp(Cmd_Argv(0), "mn_prevequip")) {
		index--;
		if (index < 0)
			index = csi.numEDs - 1;
		Cvar_Set("cl_equip", csi.eds[index].name);
	} else {
		index++;
		if (index >= csi.numEDs)
			index = 0;
		Cvar_Set("cl_equip", csi.eds[index].name);
	}
}

void GAME_SK_Results (int winner, int *numSpawned, int *numAlive, int numKilled[][MAX_TEAMS], int numStunned[][MAX_TEAMS])
{
	char resultText[MAX_SMALLMENUTEXTLEN];
	int their_killed, their_stunned;
	int i;

	if (winner == 0) {
		Q_strncpyz(popupText, _("The game was a draw!\n\nNo survivors left on any side."), sizeof(popupText));
		MN_Popup(_("Game Drawn!"), popupText);
		return;
	}

	their_killed = their_stunned = 0;
	for (i = 0; i < MAX_TEAMS; i++) {
		if (i != cls.team) {
			their_killed += numKilled[cls.team][i];
			their_stunned += numStunned[cls.team][i];
		}
	}

	Com_sprintf(resultText, sizeof(resultText), _("\n\nEnemies killed:  %i\nTeam survivors:  %i"), their_killed + their_stunned, numAlive[cls.team]);
	if (winner == cls.team) {
		Com_sprintf(popupText, lengthof(popupText), "%s%s", _("You won the game!"), resultText);
		MN_Popup(_("Congratulations"), popupText);
	} else {
		Com_sprintf(popupText, lengthof(popupText), "%s%s", _("You've lost the game!"), resultText);
		MN_Popup(_("Better luck next time"), popupText);
	}
}

qboolean GAME_SK_Spawn (chrList_t *chrList)
{
	return qtrue;
}

void GAME_SK_InitStartup (void)
{
	Cvar_ForceSet("sv_maxclients", "1");

	cls.missionaircraft = &skirmishFakeAircraft;
	memset(&skirmishFakeAircraft, 0, sizeof(skirmishFakeAircraft));
	skirmishFakeAircraft.maxTeamSize = MAX_ACTIVETEAM;
	gd.numAircraft = 1;

	Cmd_AddCommand("sk_start", GAME_SK_Start_f, "Start the new skirmish game");
	Cmd_AddCommand("sk_prevequip", GAME_SK_ChangeEquip_f, "Previous equipment definition");
	Cmd_AddCommand("sk_nextequip", GAME_SK_ChangeEquip_f, "Next equipment definition");
}

void GAME_SK_Shutdown (void)
{
	/* shutdown any running tactical mission */
	SV_Shutdown("Quitting skirmish.", qfalse);

	Cmd_RemoveCommand("sk_start");
	Cmd_RemoveCommand("sk_nextequip");
	Cmd_RemoveCommand("sk_prevequip");
}
