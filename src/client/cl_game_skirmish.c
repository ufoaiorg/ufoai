/**
 * @file cl_game_skirmish.c
 * @brief Skirmish game type implementation
 */

/*
Copyright (C) 2002-2010 UFO: Alien Invasion.

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
#include "cl_game.h"
#include "cl_team.h"
#include "cl_inventory.h"
#include "ui/ui_main.h"
#include "ui/ui_popup.h"

#define DROPSHIP_MAX INTERCEPTOR_STILETTO

static const cgame_import_t *cgi;
static cvar_t *cl_equip;

/**
 * @brief Register some data in the shared client/server structs to ensure that e.g. every known
 * alien race is used in a skirmish game
 */
static void GAME_SK_SetMissionParameters (const mapDef_t *md)
{
	int i;

	Cvar_SetValue("ai_numcivilians", 8);
	Cvar_Set("ai_civilian", "europe");

	if (md->hurtAliens)
		Cvar_Set("sv_hurtaliens", "1");
	else
		Cvar_Set("sv_hurtaliens", "0");

	/* now store the alien teams in the shared csi struct to let the game dll
	 * have access to this data, too */
	csi.numAlienTeams = 0;
	for (i = 0; i < csi.numTeamDefs; i++) {
		const teamDef_t* td = &csi.teamDef[i];
		if (CHRSH_IsTeamDefAlien(td))
			csi.alienTeams[csi.numAlienTeams++] = td;
	}
}

/**
 * @brief Starts a new skirmish game
 */
static void GAME_SK_Start_f (void)
{
	char map[MAX_VAR];
	mapDef_t *md;
	const char *name = Cvar_GetString("cl_equip");
	const equipDef_t *ed = INV_GetEquipmentDefinitionByID(name);
	const char *teamDefID = GAME_GetTeamDef();
	const size_t size = GAME_GetCharacterArraySize();
	uint32_t maxSoldiers = Cvar_GetInteger("sv_maxsoldiersperplayer");
	uint32_t ugvs = Cvar_GetInteger("cl_ugvs");
	/** @todo make the teamdef configurable */
	const char *ugvTeamDefID = "phalanx_ugv_phoenix";
	unsigned int i;

	if (maxSoldiers <= 0)
		maxSoldiers = size;

	ugvs = min(ugvs, size - maxSoldiers);

	assert(cls.currentSelectedMap >= 0);
	assert(cls.currentSelectedMap < MAX_MAPDEFS);

	md = Com_GetMapDefByIDX(cls.currentSelectedMap);
	if (!md)
		return;

	GAME_SK_SetMissionParameters(md);

	Com_Printf("Starting skirmish with %i soldiers and %i ugvs\n", maxSoldiers, ugvs);
	GAME_GenerateTeam(teamDefID, ed, maxSoldiers);
	for (i = 0; i < ugvs; i++)
		GAME_AppendTeamMember(i + maxSoldiers, ugvTeamDefID, ed);

	assert(md->map);
	Com_sprintf(map, sizeof(map), "map %s %s %s;", Cvar_GetInteger("mn_serverday") ? "day" : "night", md->map, md->param ? md->param : "");

	/* prepare */
	UI_InitStack(NULL, "singleplayermission", qtrue, qfalse);

	Cbuf_AddText(map);
}

static void GAME_SK_Restart_f (void)
{
	GAME_ReloadMode();
	GAME_SK_Start_f();
}

/**
 * @brief Changed the given cvar to the next/prev equipment definition
 */
static void GAME_SK_ChangeEquip_f (void)
{
	const equipDef_t *ed;
	int index;
	const char *cvarName;

	if (Cmd_Argc() < 2) {
		Com_Printf("Usage: %s <cvarname>\n", Cmd_Argv(0));
		return;
	}

	cvarName = Cmd_Argv(1);
	ed = INV_GetEquipmentDefinitionByID(Cvar_GetString(cvarName));
	index = ed - csi.eds;

	if (!strcmp(Cmd_Argv(0), "sk_prevequip")) {
		index--;
		if (index < 0)
			index = csi.numEDs - 1;
		Cvar_Set(cvarName, csi.eds[index].name);
	} else {
		index++;
		if (index >= csi.numEDs)
			index = 0;
		Cvar_Set(cvarName, csi.eds[index].name);
	}
}

/**
 * @brief After a mission was finished this function is called
 * @param msg The network message buffer
 * @param winner The winning team
 * @param numSpawned The amounts of all spawned actors per team
 * @param numAlive The amount of survivors per team
 * @param numKilled The amount of killed actors for all teams. The first dimension contains
 * the attacker team, the second the victim team
 * @param numStunned The amount of stunned actors for all teams. The first dimension contains
 * the attacker team, the second the victim team
 */
void GAME_SK_Results (struct dbuffer *msg, int winner, int *numSpawned, int *numAlive, int numKilled[][MAX_TEAMS], int numStunned[][MAX_TEAMS])
{
	char resultText[UI_MAX_SMALLTEXTLEN];
	int enemiesKilled, enemiesStunned;
	int i;

	CL_Drop();

	if (winner == 0) {
		UI_Popup(_("Game Drawn!"), _("The game was a draw!\n\nNo survivors left on any side."));
		return;
	}

	enemiesKilled = enemiesStunned = 0;
	for (i = 0; i < MAX_TEAMS; i++) {
		if (i != cls.team && i != TEAM_CIVILIAN) {
			enemiesKilled += numKilled[cls.team][i];
			enemiesStunned += numStunned[cls.team][i];
		}
	}

	Com_sprintf(resultText, sizeof(resultText),
			_("Enemies killed:\t\t%i\n"
			  "Team survivors:\t\t%i\n"
			  "Enemy survivors:\t\t%i\n"
			  "Friendly fire:\t\t%i\n"
			  "Civilians killed:\t\t%i\n"
			  "Civilians killed by enemy:\t\t%i\n"),
			enemiesKilled + enemiesStunned, numAlive[cls.team], numAlive[TEAM_ALIEN],
			numKilled[cls.team][cls.team], numKilled[cls.team][TEAM_CIVILIAN], numKilled[TEAM_ALIEN][TEAM_CIVILIAN]);
	if (winner == cls.team) {
		Com_sprintf(popupText, lengthof(popupText), "%s\n%s", _("You won the game!"), resultText);
		UI_Popup(_("Congratulations"), popupText);
	} else {
		Com_sprintf(popupText, lengthof(popupText), "%s\n%s", _("You've lost the game!"), resultText);
		UI_Popup(_("Better luck next time"), popupText);
	}
}

/**
 * @brief Hide the dropship selection or show it with the dropship given in the parameter
 * @param dropships if @c NULL, the dropship selection panel will be hidden, otherwise it
 * will be shown with the given list entries as content.
 */
static inline void GAME_SK_HideDropships (const linkedList_t *dropships)
{
	const qboolean hide = (dropships == NULL);
	if (hide) {
		UI_ExecuteConfunc("skirmish_hide_dropships true");
		Cvar_Set("rm_drop", "");
	} else {
		const char *rma = Com_GetRandomMapAssemblyNameForCraft((const char *)dropships->data);
		Cvar_Set("rm_drop", rma);
		UI_UpdateInvisOptions(UI_GetOption(OPTION_DROPSHIPS), dropships);

		UI_ExecuteConfunc("skirmish_hide_dropships false");
	}
}

/**
 * @brief Hide the ufo selection or show it with the ufos given in the parameter
 * @param ufos if @c NULL, the ufo selection panel will be hidden, otherwise it
 * will be shown with the given list entries as content.
 */
static inline void GAME_SK_HideUFOs (const linkedList_t *ufos)
{
	const qboolean hide = (ufos == NULL);
	if (hide) {
		UI_ExecuteConfunc("skirmish_hide_ufos true");
		Cvar_Set("rm_ufo", "");
	} else {
		const char *rma = Com_GetRandomMapAssemblyNameForCraft((const char *)ufos->data);
		Cvar_Set("rm_ufo", rma);
		UI_UpdateInvisOptions(UI_GetOption(OPTION_UFOS), ufos);

		UI_ExecuteConfunc("skirmish_hide_ufos false");
	}
}

const mapDef_t* GAME_SK_MapInfo (int step)
{
	const mapDef_t *md = Com_GetMapDefByIDX(cls.currentSelectedMap);

	if (md->map[0] == '.')
		return NULL;

	if (md->map[0] == '+') {
		GAME_SK_HideUFOs(md->ufos);
		GAME_SK_HideDropships(md->aircraft);
	} else {
		GAME_SK_HideUFOs(NULL);
		GAME_SK_HideDropships(NULL);
	}

	return md;
}

int GAME_SK_GetTeam (void)
{
	return cl_team->integer;
}

static void GAME_InitMenuOptions (void)
{
	int i;
	uiNode_t* ufoOptions = NULL;
	uiNode_t* aircraftOptions = NULL;

	for (i = 0; i < UFO_MAX; i++) {
		const char *shortName = Com_UFOTypeToShortName(i);
		UI_AddOption(&ufoOptions, shortName, shortName, Com_GetRandomMapAssemblyNameForCraft(shortName));
	}
	for (i = 0; i < UFO_MAX; i++) {
		const char *shortName = Com_UFOCrashedTypeToShortName(i);
		UI_AddOption(&ufoOptions, shortName, shortName, Com_GetRandomMapAssemblyNameForCraft(shortName));
	}
	UI_RegisterOption(OPTION_UFOS, ufoOptions);

	for (i = 0; i < DROPSHIP_MAX; i++) {
		const char *shortName = Com_DropShipTypeToShortName(i);
		UI_AddOption(&aircraftOptions, shortName, shortName, Com_GetRandomMapAssemblyNameForCraft(shortName));
	}
	UI_RegisterOption(OPTION_DROPSHIPS, aircraftOptions);
}

void GAME_SK_InitStartup (const cgame_import_t *import)
{
	cgi = import;
	Cvar_ForceSet("sv_maxclients", "1");
	cl_equip = Cvar_Get("cl_equip", "multiplayer_initial", CVAR_ARCHIVE, "Equipment that is used for skirmish mode games");

	Cmd_AddCommand("sk_start", GAME_SK_Start_f, "Start the new skirmish game");
	Cmd_AddCommand("sk_prevequip", GAME_SK_ChangeEquip_f, "Previous equipment definition");
	Cmd_AddCommand("sk_nextequip", GAME_SK_ChangeEquip_f, "Next equipment definition");
	Cmd_AddCommand("game_go", GAME_SK_Restart_f, "Restart the skirmish mission");

	GAME_InitMenuOptions();
}

void GAME_SK_Shutdown (void)
{
	Cmd_RemoveCommand("sk_start");
	Cmd_RemoveCommand("sk_nextequip");
	Cmd_RemoveCommand("sk_prevequip");
	Cmd_RemoveCommand("game_go");

	UI_ResetData(OPTION_DROPSHIPS);
	UI_ResetData(OPTION_UFOS);

	SV_Shutdown("Quitting server.", qfalse);
}
