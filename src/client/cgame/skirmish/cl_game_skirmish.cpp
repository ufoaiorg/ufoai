/**
 * @file
 * @brief Skirmish game type implementation
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
#include "cl_game_skirmish.h"
#include "../../ui/ui_data.h"

#define DROPSHIP_MAX INTERCEPTOR_STILETTO

static cvar_t* cl_equip;
static const cgame_import_t* cgi;

CGAME_HARD_LINKED_FUNCTIONS

static void GAME_SK_InitMissionBriefing (const char** title, linkedList_t** victoryConditionsMsgIDs, linkedList_t** missionBriefingMsgIDs)
{
	const mapDef_t* md = cgi->GAME_GetCurrentSelectedMap();
	if (Q_strvalid(md->victoryCondition)) {
		cgi->LIST_AddString(victoryConditionsMsgIDs, md->victoryCondition);
	}
	if (Q_strvalid(md->missionBriefing)) {
		cgi->LIST_AddString(missionBriefingMsgIDs, md->missionBriefing);
	}
	if (Q_strvalid(md->description)) {
		*title = _(md->description);
	}
}

static inline const char* GAME_SK_GetRandomMapAssemblyNameForCraft (const char* name)
{
	return cgi->Com_GetRandomMapAssemblyNameForCraft(name);
}

/**
 * @brief Register some data in the shared client/server structs to ensure that e.g. every known
 * alien race is used in a skirmish game
 */
static void GAME_SK_SetMissionParameters (const mapDef_t* md)
{
	cgi->Cvar_SetValue("ai_numcivilians", 8);
	if (md->civTeam != nullptr)
		cgi->Cvar_Set("ai_civilianteam", "%s", md->civTeam);
	else
		cgi->Cvar_Set("ai_civilianteam", "europe");

	cgi->Cvar_Set("sv_hurtaliens", "0");

	/* now store the alien teams in the shared csi struct to let the game dll
	 * have access to this data, too */
	cgi->csi->numAlienTeams = 0;
	for (int i = 0; i < cgi->csi->numTeamDefs; i++) {
		const teamDef_t* td = &cgi->csi->teamDef[i];
		if (CHRSH_IsTeamDefAlien(td))
			cgi->csi->alienTeams[cgi->csi->numAlienTeams++] = td;
		if (cgi->csi->numAlienTeams >= MAX_TEAMS_PER_MISSION)
			break;
	}
}

/**
 * @brief Starts a new skirmish game
 */
static void GAME_SK_Start_f (void)
{
	if (cgi->GAME_IsTeamEmpty()) {
		cgi->GAME_LoadDefaultTeam(false);
	}

	if (cgi->GAME_IsTeamEmpty()) {
		/** @todo make the teamdef configurable */
		const char* ugvTeamDefID = "phalanx_ugv_phoenix";
		const char* name = cgi->Cvar_GetString("cl_equip");
		const equipDef_t* ed = cgi->INV_GetEquipmentDefinitionByID(name);
		const size_t size = cgi->GAME_GetCharacterArraySize();
		uint32_t maxSoldiers = cgi->Cvar_GetInteger("sv_maxsoldiersperplayer");
		uint32_t ugvs = cgi->Cvar_GetInteger("cl_ugvs");

		if (maxSoldiers <= 0)
			maxSoldiers = size;

		ugvs = std::min(ugvs, (uint32_t)(size - maxSoldiers));
		cgi->Com_Printf("Starting skirmish with %i soldiers and %i ugvs\n", maxSoldiers, ugvs);
		cgi->GAME_AutoTeam(name, maxSoldiers);
		for (unsigned int i = 0; i < ugvs; i++)
			cgi->GAME_AppendTeamMember(i + maxSoldiers, ugvTeamDefID, ed);
	} else {
		cgi->Com_Printf("Using already loaded team\n");
	}

	const mapDef_t* md = cgi->GAME_GetCurrentSelectedMap();
	if (!md)
		return;

	GAME_SK_SetMissionParameters(md);

	assert(md->mapTheme);

	cgi->Cbuf_AddText("map %s %s %s;", cgi->Cvar_GetInteger("mn_serverday") ? "day" : "night", md->mapTheme, md->params ? (const char*)cgi->LIST_GetRandom(md->params) : "");
}

static void GAME_SK_Restart_f (void)
{
	cgi->GAME_ReloadMode();
	GAME_SK_Start_f();
}

/**
 * @brief Changed the given cvar to the next/prev equipment definition
 */
static void GAME_SK_ChangeEquip_f (void)
{
	if (cgi->Cmd_Argc() < 2) {
		cgi->Com_Printf("Usage: %s <cvarname>\n", cgi->Cmd_Argv(0));
		return;
	}

	const char* command = cgi->Cmd_Argv(0);
	const char* cvarName = cgi->Cmd_Argv(1);

	changeEquipType_t type;
	if (Q_streq(command, "sk_prevequip")) {
		type = BACKWARD;
	} else if (Q_streq(command, "sk_nextequip")) {
		type = FORWARD;
	} else {
		type = INIT;
	}

	const equipDef_t* ed = cgi->GAME_ChangeEquip(cgi->cgameType->equipmentList, type, cgi->Cvar_GetString(cvarName));

	char cvarBuf[MAX_VAR];
	Com_sprintf(cvarBuf, sizeof(cvarBuf), "%sname", cvarName);

	cgi->Cvar_Set(cvarName, "%s", ed->id);
	cgi->Cvar_Set(cvarBuf, "_%s", ed->name);
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
 * @param nextmap Indicates if there is another map to follow within the same msission
 */
static void GAME_SK_Results (dbuffer* msg, int winner, int* numSpawned, int* numAlive, int numKilled[][MAX_TEAMS], int numStunned[][MAX_TEAMS], bool nextmap)
{
	if (nextmap)
		return;

	/* HACK: Change to the main menu now so cgame shutdown won't kill the results popup doing it */
	cgi->UI_InitStack("main", "");

	if (winner == -1) {
		cgi->UI_Popup(_("Game Drawn!"), "%s\n", _("The game was a draw!\n\nEnemies escaped."));
		return;
	}

	if (winner == 0) {
		cgi->UI_Popup(_("Game Drawn!"), "%s\n", _("The game was a draw!\n\nNo survivors left on any side."));
		return;
	}

	const int team = cgi->GAME_GetCurrentTeam();
	const int ownFriendlyFire = numKilled[team][team] + numKilled[TEAM_CIVILIAN][team];
	const int ownLost = numSpawned[team] - numAlive[team] - ownFriendlyFire;
	const int civFriendlyFire = numKilled[team][TEAM_CIVILIAN] + numKilled[TEAM_CIVILIAN][TEAM_CIVILIAN];
	const int civLost = numSpawned[TEAM_CIVILIAN] - numAlive[TEAM_CIVILIAN] - civFriendlyFire;

	int enemiesStunned = 0;
	for (int i = 0; i <= MAX_TEAMS; ++i) {
			enemiesStunned += numStunned[i][TEAM_ALIEN];
	}
	const int enemiesKilled = numSpawned[TEAM_ALIEN] - numAlive[TEAM_ALIEN] - enemiesStunned;

	char resultText[1024];
	Com_sprintf(resultText, sizeof(resultText),
			_("\n%i of %i enemies killed, %i stunned, %i survived.\n"
			  "%i of %i team members survived, %i lost in action, %i friendly fire loses.\n"
			  "%i of %i civilians saved, %i civilian loses, %i friendly fire loses."),
			enemiesKilled, numSpawned[TEAM_ALIEN], enemiesStunned, numAlive[TEAM_ALIEN],
			numAlive[team], numSpawned[team], ownLost, ownFriendlyFire,
			numAlive[TEAM_CIVILIAN], numSpawned[TEAM_CIVILIAN], civLost, civFriendlyFire);
	if (winner == team) {
		cgi->UI_Popup(_("Congratulations"), "%s\n%s\n", _("You won the game!"), resultText);
	} else {
		cgi->UI_Popup(_("Better luck next time"), "%s\n%s\n", _("You've lost the game!"), resultText);
	}

	/* Ask the game mode to shutdown -- don't execute the shutdown here or we will crash! */
	cgi->Cbuf_AddText("game_exit");
}

/**
 * @brief Hide the dropship selection or show it with the dropship given in the parameter
 * @param dropships if @c nullptr, the dropship selection panel will be hidden, otherwise it
 * will be shown with the given list entries as content.
 */
static inline void GAME_SK_HideDropships (const linkedList_t* dropships)
{
	const bool hide = (dropships == nullptr);
	if (hide) {
		cgi->UI_ExecuteConfunc("skirmish_hide_dropships true");
		cgi->Cvar_Set("rm_drop", "");
	} else {
		const char* rma = GAME_SK_GetRandomMapAssemblyNameForCraft((const char*)dropships->data);
		cgi->Cvar_Set("rm_drop", "%s", rma);
		cgi->UI_UpdateInvisOptions(cgi->UI_GetOption(OPTION_DROPSHIPS), dropships);
		cgi->UI_RegisterOption(OPTION_DROPSHIPS, cgi->UI_GetOption(OPTION_DROPSHIPS));

		cgi->UI_ExecuteConfunc("skirmish_hide_dropships false");
	}
}

/**
 * @brief Hide the ufo selection or show it with the ufos given in the parameter
 * @param ufos if @c nullptr, the ufo selection panel will be hidden, otherwise it
 * will be shown with the given list entries as content.
 */
static inline void GAME_SK_HideUFOs (const linkedList_t* ufos)
{
	const bool hide = (ufos == nullptr);
	if (hide) {
		cgi->UI_ExecuteConfunc("skirmish_hide_ufos true");
		cgi->Cvar_Set("rm_ufo", "");
	} else {
		const char* rma = GAME_SK_GetRandomMapAssemblyNameForCraft((const char*)ufos->data);
		cgi->Cvar_Set("rm_ufo", "%s", rma);
		cgi->UI_UpdateInvisOptions(cgi->UI_GetOption(OPTION_UFOS), ufos);
		cgi->UI_RegisterOption(OPTION_UFOS, cgi->UI_GetOption(OPTION_UFOS));

		cgi->UI_ExecuteConfunc("skirmish_hide_ufos false");
	}
	cgi->Cvar_Set("rm_crashed", "");
}

static const mapDef_t* GAME_SK_MapInfo (int step)
{
	int i = 0;

	while (!cgi->GAME_GetCurrentSelectedMap()->singleplayer) {
		i++;
		cgi->GAME_SwitchCurrentSelectedMap(step ? step : 1);
		if (i > 100000)
			cgi->Com_Error(ERR_DROP, "no singleplayer map found");
	}

	const mapDef_t* md = cgi->GAME_GetCurrentSelectedMap();

	cgi->Cvar_SetValue("ai_singleplayeraliens", md->maxAliens);

	if (md->mapTheme[0] == '.')
		return nullptr;

	if (md->mapTheme[0] == '+') {
		GAME_SK_HideUFOs(md->ufos);
		GAME_SK_HideDropships(md->aircraft);
	} else {
		GAME_SK_HideUFOs(nullptr);
		GAME_SK_HideDropships(nullptr);
	}

	return md;
}

static void GAME_InitMenuOptions (void)
{
	uiNode_t* ufoOptions = nullptr;
	uiNode_t* aircraftOptions = nullptr;

	for (int i = 0; i < UFO_MAX; i++) {
		const char* shortName = cgi->Com_UFOTypeToShortName((ufoType_t)i);
		cgi->UI_AddOption(&ufoOptions, shortName, shortName, GAME_SK_GetRandomMapAssemblyNameForCraft(shortName));
	}
	for (int i = 0; i < UFO_MAX; i++) {
		const char* shortName = cgi->Com_UFOCrashedTypeToShortName((ufoType_t)i);
		cgi->UI_AddOption(&ufoOptions, shortName, shortName, GAME_SK_GetRandomMapAssemblyNameForCraft(shortName));
	}
	cgi->UI_RegisterOption(OPTION_UFOS, ufoOptions);

	for (int i = 0; i < DROPSHIP_MAX; i++) {
		const char* shortName = cgi->Com_DropShipTypeToShortName((humanAircraftType_t)i);
		cgi->UI_AddOption(&aircraftOptions, shortName, shortName, GAME_SK_GetRandomMapAssemblyNameForCraft(shortName));
	}
	cgi->UI_RegisterOption(OPTION_DROPSHIPS, aircraftOptions);
}

static const cmdList_t skirmishCmds[] = {
	{"sk_start", GAME_SK_Start_f, "Start the new skirmish game"},
	{"sk_prevequip", GAME_SK_ChangeEquip_f, "Previous equipment definition"},
	{"sk_nextequip", GAME_SK_ChangeEquip_f, "Next equipment definition"},
	{"sk_initequip", GAME_SK_ChangeEquip_f, "Init equipment definition"},
	{"game_go", GAME_SK_Restart_f, "Restart the skirmish mission"},
	{nullptr, nullptr, nullptr}
};
static void GAME_SK_InitStartup (void)
{
	cgi->Cvar_ForceSet("sv_maxclients", "1");
	cl_equip = cgi->Cvar_Get("cl_equip", "multiplayer_initial", 0, "Equipment that is used for skirmish mode games");

	cgi->Cmd_TableAddList(skirmishCmds);

	GAME_InitMenuOptions();
}

static void GAME_SK_Shutdown (void)
{
	cgi->Cmd_TableRemoveList(skirmishCmds);
	/* You really don't want this with campaign */
	cgi->Cvar_ForceSet("g_endlessaliens", "0");

	cgi->UI_ResetData(OPTION_DROPSHIPS);
	cgi->UI_ResetData(OPTION_UFOS);

	cgi->SV_Shutdown("Quitting server.", false);
}

#ifndef HARD_LINKED_CGAME
const cgame_export_t* GetCGameAPI (const cgame_import_t* import)
#else
const cgame_export_t* GetCGameSkirmishAPI (const cgame_import_t* import)
#endif
{
	static cgame_export_t e;

	OBJZERO(e);

	e.name ="Skirmish mode";
	e.menu = "skirmish";
	e.Init = GAME_SK_InitStartup;
	e.Shutdown = GAME_SK_Shutdown;
	e.MapInfo = GAME_SK_MapInfo;
	e.Results = GAME_SK_Results;
	e.InitMissionBriefing = GAME_SK_InitMissionBriefing;

	cgi = import;

	return &e;
}
