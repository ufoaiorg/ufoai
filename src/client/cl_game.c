/**
 * @file cl_game.c
 * @brief Shared game type code
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

#include "cl_game.h"
#include "cgame.h"
#include "battlescape/cl_localentity.h"
#include "ui/ui_main.h"
#include "ui/ui_nodes.h"
#include "cl_team.h"
#include "battlescape/events/e_main.h"
#include "cl_inventory.h"
#include "ui/node/ui_node_model.h"

static invList_t invList[MAX_INVLIST];

static const cgame_export_t gameTypeList[] = {
	{"Multiplayer mode", "multiplayer", GAME_MULTIPLAYER, GAME_MP_InitStartup, GAME_MP_Shutdown, NULL, GAME_MP_GetTeam, GAME_MP_MapInfo, GAME_MP_Results, NULL, NULL, GAME_MP_GetEquipmentDefinition, NULL, NULL, NULL, NULL, NULL, NULL, GAME_MP_EndRoundAnnounce, GAME_MP_StartBattlescape},
	{"Campaign mode", "campaigns", GAME_CAMPAIGN, GAME_CP_InitStartup, GAME_CP_Shutdown, GAME_CP_Spawn, GAME_CP_GetTeam, GAME_CP_MapInfo, GAME_CP_Results, GAME_CP_ItemIsUseable, GAME_CP_DisplayItemInfo, GAME_CP_GetEquipmentDefinition, GAME_CP_CharacterCvars, GAME_CP_TeamIsKnown, GAME_CP_Drop, GAME_CP_InitializeBattlescape, GAME_CP_Frame, GAME_CP_GetModelForItem, NULL, NULL},
	{"Skirmish mode", "skirmish", GAME_SKIRMISH, GAME_SK_InitStartup, GAME_SK_Shutdown, NULL, GAME_SK_GetTeam, GAME_SK_MapInfo, GAME_SK_Results, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL},

	{NULL, NULL, 0, NULL, NULL}
};

/**
 * @brief static character array that can be used by a game mode to store the needed character values.
 */
static character_t characters[MAX_ACTIVETEAM];

/**
 * @brief Returns a character that can be used to store the game type specific character values
 * @note The returned pointer is a reference to static memory
 * @param index The index of the character array. This value must be greater than 0 and not bigger than the
 * value @c GAME_GetCharacterArraySize returned
 * @sa GAME_GetCharacterArraySize
 * @sa GAME_ResetCharacters
 * @return A character slot
 */
character_t* GAME_GetCharacter (int index)
{
	if (index < 0 || index >= lengthof(characters))
		Com_Error(ERR_DROP, "Out of bounds character access");

	return &characters[index];
}

/**
 * @return The size of the static character array
 * @sa GAME_GetCharacter
 * @sa GAME_ResetCharacters
 */
size_t GAME_GetCharacterArraySize (void)
{
	return lengthof(characters);
}

/**
 * @brief Will reset (memset) all characters in the static character array
 * @sa GAME_GetCharacterArraySize
 * @sa GAME_GetCharacter
 */
void GAME_ResetCharacters (void)
{
	memset(&characters, 0, sizeof(characters));
	chrDisplayList.num = 0;
}

void GAME_AppendTeamMember (int memberIndex, const char *teamDefID, const equipDef_t *ed)
{
	character_t *chr;

	if (ed == NULL)
		Com_Error(ERR_DROP, "No equipment definition given");

	chr = GAME_GetCharacter(memberIndex);

	CL_GenerateCharacter(chr, teamDefID);
	/* pack equipment */
	cls.i.EquipActor(&cls.i, &chr->i, ed, chr->teamDef);

	chrDisplayList.chr[memberIndex] = chr;
	chrDisplayList.num++;
}

void GAME_GenerateTeam (const char *teamDefID, const equipDef_t *ed, int teamMembers)
{
	int i;

	if (teamMembers > GAME_GetCharacterArraySize())
		Com_Error(ERR_DROP, "More than the allowed amount of team members");

	if (ed == NULL)
		Com_Error(ERR_DROP, "No equipment definition given");

	GAME_ResetCharacters();

	for (i = 0; i < teamMembers; i++)
		GAME_AppendTeamMember(i, teamDefID, ed);
}

static const cgame_export_t *GAME_GetCurrentType (void)
{
	const cgame_export_t *list = gameTypeList;

	if (cls.gametype == GAME_NONE)
		return NULL;

	while (list->name) {
		if (list->gametype == cls.gametype)
			return list;
		list++;
	}

	return NULL;
}

void GAME_ReloadMode (void)
{
	const cgame_export_t *list = GAME_GetCurrentType();
	if (list != NULL) {
		const int gametype = list->gametype;
		GAME_SetMode(GAME_NONE);
		GAME_SetMode(gametype);
	}
}

/**
 * @brief Called when the server sends the @c EV_START event.
 * @param isTeamPlay @c true if the game is a teamplay round. This can be interesting for
 * multiplayer based game types
 * @sa GAME_EndBattlescape
 */
void GAME_StartBattlescape (qboolean isTeamPlay)
{
	const cgame_export_t *list = GAME_GetCurrentType();
	if (list != NULL && list->StartBattlescape) {
		list->StartBattlescape(isTeamPlay);
	} else {
		UI_InitStack(mn_hud->string, NULL, qtrue, qtrue);
	}
	Com_Printf("Free inventory slots: %i\n", cls.i.GetFreeSlots(&cls.i));
}

/**
 * @brief This is called when a client quits the battlescape
 * @sa GAME_StartBattlescape
 */
void GAME_EndBattlescape (void)
{
	Com_Printf("Free inventory slots after battle: %i\n", cls.i.GetFreeSlots(&cls.i));
}

/**
 * @brief Send end round announcements
 * @param playerNum The server player number of the player that has ended the round
 * @param team The team the player is in
 */
void GAME_EndRoundAnnounce (int playerNum, int team)
{
	/** @todo do we need the team number here? isn't the playernum enough to get the team? */
	const cgame_export_t *list = GAME_GetCurrentType();
	if (list != NULL && list->EndRoundAnnounce)
		list->EndRoundAnnounce(playerNum, team);
}

/**
 * @brief Shows game type specific item information (if it's not resolvable via @c objDef_t).
 * @param[in] node The menu node to show the information in.
 * @param[in] string The id of the 'thing' to show information for.
 */
void GAME_DisplayItemInfo (uiNode_t *node, const char *string)
{
	const cgame_export_t *list = GAME_GetCurrentType();
	if (list != NULL && list->DisplayItemInfo)
		list->DisplayItemInfo(node, string);
}

#if 0
/**
 * @todo Implement the usage of this function and update the game modes to use the callbacks
 */
static cgame_import_t* GAME_GetImportData (void)
{
	static cgame_import_t gameImport;
	static cgame_import_t *cgi = NULL;

	if (cgi == NULL) {
		memset(&gameImport, 0, sizeof(gameImport));
		cgi = &gameImport;

		cgi->csi = &csi;
		cgi->cls = &cls;
		cgi->cl = &cl;

		cgi->Cmd_AddCommand = Cmd_AddCommand;
		cgi->Cmd_Argc = Cmd_Argc;
		cgi->Cmd_Args = Cmd_Args;
		cgi->Cmd_Argv = Cmd_Argv;
		cgi->Cmd_ExecuteString = Cmd_ExecuteString;
		cgi->Cmd_RemoveCommand = Cmd_RemoveCommand;
		cgi->Cvar_Delete = Cvar_Delete;
		cgi->Cvar_Get = Cvar_Get;
		cgi->Cvar_Integer = Cvar_GetInteger;
		cgi->Cvar_Set = Cvar_Set;
		cgi->Cvar_SetValue = Cvar_SetValue;
		cgi->Cvar_String = Cvar_GetString;
		cgi->FS_FreeFile = FS_FreeFile;
		cgi->FS_LoadFile = FS_LoadFile;
		cgi->UI_AddOption = UI_AddOption;
		cgi->UI_ExecuteConfunc = UI_ExecuteConfunc;
		cgi->UI_InitStack = UI_InitStack;
		cgi->UI_Popup = UI_Popup;
		cgi->UI_PopupList = UI_PopupList;
		cgi->UI_PopWindow = UI_PopWindow;
		cgi->UI_PushWindow = UI_PushWindow;
		cgi->UI_RegisterLinkedListText = UI_RegisterLinkedListText;
		cgi->UI_RegisterOption = UI_RegisterOption;
		cgi->UI_RegisterText = UI_RegisterText;
		cgi->UI_ResetData = UI_ResetData;
		cgi->UI_TextNodeSelectLine = UI_TextNodeSelectLine;
		cgi->mxml_AddBool = mxml_AddBool;
		cgi->mxml_AddBoolValue = mxml_AddBoolValue;
		cgi->mxml_AddByte = mxml_AddByte;
		cgi->mxml_AddByteValue = mxml_AddByteValue;
		cgi->mxml_AddDate = mxml_AddDate;
		cgi->mxml_AddDouble = mxml_AddDouble;
		cgi->mxml_AddDoubleValue = mxml_AddDoubleValue;
		cgi->mxml_AddFloat = mxml_AddFloat;
		cgi->mxml_AddFloatValue = mxml_AddFloatValue;
		cgi->mxml_AddInt = mxml_AddInt;
		cgi->mxml_AddIntValue = mxml_AddIntValue;
		cgi->mxml_AddLong = mxml_AddLong;
		cgi->mxml_AddLongValue = mxml_AddLongValue;
		cgi->mxml_AddNode = mxml_AddNode;
		cgi->mxml_AddPos2 = mxml_AddPos2;
		cgi->mxml_AddPos3 = mxml_AddPos3;
		cgi->mxml_AddShort = mxml_AddShort;
		cgi->mxml_AddShortValue = mxml_AddShortValue;
		cgi->mxml_AddString = mxml_AddString;
		cgi->mxml_AddStringValue = mxml_AddStringValue;
		cgi->R_LoadImage = R_LoadImage;
		cgi->R_LoadImageData = R_LoadImageData;
		cgi->R_SoftenTexture = R_SoftenTexture;
		cgi->R_UploadAlpha = R_UploadAlpha;
		cgi->S_SetSampleRepeatRate = S_SetSampleRepeatRate;
		cgi->S_StartLocalSample = S_StartLocalSample;
	}

	return cgi;
}
#endif

void GAME_SetMode (int gametype)
{
	const cgame_export_t *list;

	if (gametype < 0 || gametype > GAME_MAX) {
		Com_Printf("Invalid gametype %i given\n", gametype);
		return;
	}

	if (cls.gametype == gametype)
		return;

	list = GAME_GetCurrentType();
	if (list) {
		Com_Printf("Shutdown gametype '%s'\n", list->name);
		list->Shutdown();

		/* we dont need to go back to "main" stack if we are already on this stack */
		if (!UI_IsWindowOnStack("main"))
			UI_InitStack("main", "", qtrue, qtrue);
	}

	cls.gametype = gametype;

	CL_Disconnect();

	list = GAME_GetCurrentType();
	if (list) {
		Com_Printf("Change gametype to '%s'\n", list->name);
		memset(&invList, 0, sizeof(invList));
		/* inventory structure switched/initialized */
		INV_InitInventory(list->name, &cls.i, &csi, invList, lengthof(invList));
		list->Init();
	}
}

static void UI_MapInfoGetNext (int step)
{
	const mapDef_t *md;

	cls.currentSelectedMap += step;

	if (cls.currentSelectedMap < 0)
		cls.currentSelectedMap = cls.numMDs - 1;

	cls.currentSelectedMap %= cls.numMDs;

	md = Com_GetMapDefByIDX(cls.currentSelectedMap);

	/* special purpose maps are not startable without the specific context */
	if (md->map[0] == '.')
		UI_MapInfoGetNext(step);
}

/**
 * @brief Prints the map info for the server creation dialogue
 * @todo Skip special map that start with a '.' (e.g. .baseattack)
 */
static void UI_MapInfo (int step)
{
	const char *mapname;
	const mapDef_t *md;
	const cgame_export_t *list = GAME_GetCurrentType();

	if (!list)
		return;

	if (!cls.numMDs)
		return;

	UI_MapInfoGetNext(step);

	md = list->MapInfo(step);
	if (!md)
		return;

	mapname = md->map;
	/* skip random map char */
	if (mapname[0] == '+') {
		Cvar_Set("mn_svmapname", va("%s %s", md->map, md->param ? md->param : ""));
		mapname++;
	} else {
		Cvar_Set("mn_svmapname", md->map);
	}

	if (FS_CheckFile("pics/maps/shots/%s.jpg", mapname) != -1)
		Cvar_Set("mn_mappic", va("maps/shots/%s", mapname));
	else
		Cvar_Set("mn_mappic", "maps/shots/default");

	if (FS_CheckFile("pics/maps/shots/%s_2.jpg", mapname) != -1)
		Cvar_Set("mn_mappic2", va("maps/shots/%s_2", mapname));
	else
		Cvar_Set("mn_mappic2", "maps/shots/default");

	if (FS_CheckFile("pics/maps/shots/%s_3.jpg", mapname) != -1)
		Cvar_Set("mn_mappic3", va("maps/shots/%s_3", mapname));
	else
		Cvar_Set("mn_mappic3", "maps/shots/default");
}

static void UI_GetMaps_f (void)
{
	UI_MapInfo(0);
}

static void UI_ChangeMap_f (void)
{
	if (!strcmp(Cmd_Argv(0), "mn_nextmap"))
		UI_MapInfo(1);
	else
		UI_MapInfo(-1);
}

static void UI_SelectMap_f (void)
{
	const char *mapname;
	int i;

	if (Cmd_Argc() != 2) {
		Com_Printf("Usage: %s <mapname>\n", Cmd_Argv(0));
		return;
	}

	if (!cls.numMDs)
		return;

	mapname = Cmd_Argv(1);

	for (i = 0; i < cls.numMDs; i++) {
		const mapDef_t *md = Com_GetMapDefByIDX(i);
		if (strcmp(md->map, mapname))
			continue;
		cls.currentSelectedMap = i;
		UI_MapInfo(0);
		return;
	}

	for (i = 0; i < cls.numMDs; i++) {
		const mapDef_t *md = Com_GetMapDefByIDX(i);
		if (strcmp(md->id, mapname))
			continue;
		cls.currentSelectedMap = i;
		UI_MapInfo(0);
		return;
	}

	Com_Printf("Could not find map %s\n", mapname);
}

/**
 * @brief Decides with game mode should be set - takes the menu as reference
 */
static void GAME_SetMode_f (void)
{
	const char *modeName;
	const cgame_export_t *list = gameTypeList;

	if (Cmd_Argc() == 2)
		modeName = Cmd_Argv(1);
	else
		modeName = UI_GetActiveWindowName();

	if (modeName[0] == '\0')
		return;

	while (list->name) {
		if (!strcmp(list->menu, modeName)) {
			GAME_SetMode(list->gametype);
			return;
		}
		list++;
	}
	Com_Printf("GAME_SetMode_f: Mode '%s' not found\n", modeName);
}

static qboolean GAME_IsArmourUseableForTeam (const objDef_t *od, const teamDef_t *teamDef)
{
	if (teamDef != NULL && teamDef->armour && INV_IsArmour(od)) {
		if (CHRSH_IsTeamDefAlien(teamDef))
			return od->useable == TEAM_ALIEN;
		else if (teamDef->race == RACE_PHALANX_HUMAN)
			return od->useable == TEAM_PHALANX;
		else if (teamDef->race == RACE_CIVILIAN)
			return od->useable == TEAM_CIVILIAN;
		else
			return qfalse;
	}

	return qtrue;
}

qboolean GAME_ItemIsUseable (const objDef_t *od)
{
	const cgame_export_t *list = GAME_GetCurrentType();
	const char *teamDefID = GAME_GetTeamDef();
	const teamDef_t *teamDef = Com_GetTeamDefinitionByID((teamDefID));

	/* Don't allow armour for other teams */
	if (!GAME_IsArmourUseableForTeam(od, teamDef))
		return qfalse;

	if (list && list->IsItemUseable)
		return list->IsItemUseable(od);

	return qtrue;
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
void GAME_HandleResults (struct dbuffer *msg, int winner, int *numSpawned, int *numAlive, int numKilled[][MAX_TEAMS], int numStunned[][MAX_TEAMS])
{
	const cgame_export_t *list = GAME_GetCurrentType();
	if (list)
		list->Results(msg, winner, numSpawned, numAlive, numKilled, numStunned);
	else
		CL_Drop();
}

/**
 * @sa G_WriteItem
 * @sa G_ReadItem
 * @note The amount of the item_t struct should not be needed here - because
 * the amount is only valid for idFloor and idEquip
 */
static void CL_NetSendItem (struct dbuffer *buf, item_t item, containerIndex_t container, int x, int y)
{
	const int ammoIdx = item.m ? item.m->idx : NONE;
	const eventRegister_t *eventData = CL_GetEvent(EV_INV_TRANSFER);
	assert(item.t);
	Com_DPrintf(DEBUG_CLIENT, "CL_NetSendItem: Add item %s to container %i (t=%i:a=%i:m=%i) (x=%i:y=%i)\n",
		item.t->id, container, item.t->idx, item.a, ammoIdx, x, y);
	NET_WriteFormat(buf, eventData->formatString, item.t->idx, item.a, ammoIdx, container, x, y, item.rotated, item.amount);
}

/**
 * @sa G_SendInventory
 */
static void CL_NetSendInventory (struct dbuffer *buf, const inventory_t *i)
{
	containerIndex_t container;
	int nr = 0;
	const invList_t *ic;

	for (container = 0; container < csi.numIDs; container++) {
		for (ic = i->c[container]; ic; ic = ic->next)
			nr++;
	}

	NET_WriteShort(buf, nr * INV_INVENTORY_BYTES);
	for (container = 0; container < csi.numIDs; container++) {
		for (ic = i->c[container]; ic; ic = ic->next)
			CL_NetSendItem(buf, ic->item, container, ic->x, ic->y);
	}
}

/**
 * @brief Send the character information to the server that is needed to spawn the soldiers of the player.
 * @param[out] buf The net channel buffer to write the character data into.
 * @param[in] chr The character to get the data from.
 */
static void GAME_NetSendCharacter (struct dbuffer * buf, const character_t *chr)
{
	int j;

	if (!chr)
		Com_Error(ERR_DROP, "No character given");
	if (chr->fieldSize != ACTOR_SIZE_2x2 && chr->fieldSize != ACTOR_SIZE_NORMAL)
		Com_Error(ERR_DROP, "Invalid character size given for character '%s': %i",
				chr->name, chr->fieldSize);
	if (chr->teamDef == NULL)
		Com_Error(ERR_DROP, "Character with no teamdef set (%s)", chr->name);

	NET_WriteByte(buf, chr->fieldSize);
	NET_WriteShort(buf, chr->ucn);
	NET_WriteString(buf, chr->name);

	/* model */
	NET_WriteString(buf, chr->path);
	NET_WriteString(buf, chr->body);
	NET_WriteString(buf, chr->head);
	NET_WriteByte(buf, chr->skin);

	NET_WriteShort(buf, chr->HP);
	NET_WriteShort(buf, chr->maxHP);
	NET_WriteByte(buf, chr->teamDef->idx);
	NET_WriteByte(buf, chr->gender);
	NET_WriteByte(buf, chr->STUN);
	NET_WriteByte(buf, chr->morale);

	for (j = 0; j < SKILL_NUM_TYPES + 1; j++)
		NET_WriteLong(buf, chr->score.experience[j]);
	for (j = 0; j < SKILL_NUM_TYPES; j++)
		NET_WriteByte(buf, chr->score.skills[j]);
	for (j = 0; j < SKILL_NUM_TYPES + 1; j++)
		NET_WriteByte(buf, chr->score.initialSkills[j]);
	for (j = 0; j < KILLED_NUM_TYPES; j++)
		NET_WriteShort(buf, chr->score.kills[j]);
	for (j = 0; j < KILLED_NUM_TYPES; j++)
		NET_WriteShort(buf, chr->score.stuns[j]);
	NET_WriteShort(buf, chr->score.assignedMissions);
}

/**
 * @brief Stores a team-list (chr-list) info to buffer (which might be a network buffer, too).
 * @sa G_ClientTeamInfo
 * @sa MP_SaveTeamMultiplayerInfo
 * @note Called in CL_Precache_f to send the team info to server
 */
static void GAME_SendCurrentTeamSpawningInfo (struct dbuffer * buf, chrList_t *team)
{
	int i;

	/* header */
	NET_WriteByte(buf, clc_teaminfo);
	NET_WriteByte(buf, team->num);

	Com_DPrintf(DEBUG_CLIENT, "GAME_SendCurrentTeamSpawningInfo: Upload information about %i soldiers to server\n", team->num);
	for (i = 0; i < team->num; i++) {
		character_t *chr = team->chr[i];

		GAME_NetSendCharacter(buf, chr);

		CL_NetSendInventory(buf, &chr->i);
	}
}

const char* GAME_GetTeamDef (void)
{
	const char *teamDefID = Cvar_GetString("cl_teamdef");
	if (teamDefID[0] == '\0')
		teamDefID = "phalanx";
	return teamDefID;
}

static qboolean GAME_Spawn (void)
{
	const size_t size = GAME_GetCharacterArraySize();
	int i;

	/* If there is no active gametype we create a team with default values.
	 * This is e.g. the case when someone starts a map with the map command */
	if (GAME_GetCurrentType() == NULL || chrDisplayList.num == 0) {
		const char *teamDefID = GAME_GetTeamDef();
		const equipDef_t *ed = INV_GetEquipmentDefinitionByID("multiplayer_initial");

		memset(&invList, 0, sizeof(invList));

		/* inventory structure switched/initialized */
		INV_InitInventory("client", &cls.i, &csi, invList, lengthof(invList));
		GAME_GenerateTeam(teamDefID, ed, size);
	}

	for (i = 0; i < size; i++)
		cl.chrList.chr[i] = chrDisplayList.chr[i];
	cl.chrList.num = chrDisplayList.num;

	return qtrue;
}

/**
 * @brief This is called if actors are spawned (or at least the spawning commands were send to
 * the server). This callback can e.g. be used to set initial actor states. E.g. request crouch and so on.
 * These events are executed without consuming time
 */
static void GAME_InitializeBattlescape (chrList_t *team)
{
	int i;
	const cgame_export_t *list = GAME_GetCurrentType();

	for (i = 0; i < lengthof(cl.teamList); i++) {
		UI_ExecuteConfunc("huddisable %i", i);
	}

	if (list && list->InitializeBattlescape)
		list->InitializeBattlescape(team);
}

/**
 * @brief Called during startup of mission to send team info
 */
void GAME_SpawnSoldiers (void)
{
	const cgame_export_t *list = GAME_GetCurrentType();
	qboolean spawnStatus;

	/* this callback is responsible to set up the cl.chrList */
	if (list && list->Spawn)
		spawnStatus = list->Spawn();
	else
		spawnStatus = GAME_Spawn();

	if (spawnStatus && cl.chrList.num > 0) {
		struct dbuffer *msg;

		/* send team info */
		msg = new_dbuffer();
		GAME_SendCurrentTeamSpawningInfo(msg, &cl.chrList);
		NET_WriteMsg(cls.netStream, msg);

		msg = new_dbuffer();
		NET_WriteByte(msg, clc_stringcmd);
		NET_WriteString(msg, "spawn\n");
		NET_WriteMsg(cls.netStream, msg);

		GAME_InitializeBattlescape(&cl.chrList);
	}
}

int GAME_GetCurrentTeam (void)
{
	const cgame_export_t *list = GAME_GetCurrentType();

	if (list && list->GetTeam != NULL)
		return list->GetTeam();

	return TEAM_DEFAULT;
}

equipDef_t *GAME_GetEquipmentDefinition (void)
{
	const cgame_export_t *list = GAME_GetCurrentType();

	if (list && list->GetEquipmentDefinition != NULL)
		return list->GetEquipmentDefinition();
	return NULL;
}

qboolean GAME_TeamIsKnown (const teamDef_t *teamDef)
{
	const cgame_export_t *list = GAME_GetCurrentType();

	if (!teamDef)
		return qfalse;

	if (list && list->IsTeamKnown != NULL)
		return list->IsTeamKnown(teamDef);
	return qtrue;
}

void GAME_CharacterCvars (const character_t *chr)
{
	const cgame_export_t *list = GAME_GetCurrentType();
	if (list && list->UpdateCharacterValues != NULL)
		list->UpdateCharacterValues(chr);
}

/**
 * @brief Let the aliens win the match
 */
static void GAME_Abort_f (void)
{
	/* aborting means letting the aliens win */
	Cbuf_AddText(va("sv win %i\n", TEAM_ALIEN));
}

void GAME_Drop (void)
{
	const cgame_export_t *list = GAME_GetCurrentType();

	if (list && list->Drop) {
		list->Drop();
	} else {
		SV_Shutdown("Drop", qfalse);
		GAME_SetMode(GAME_NONE);
		UI_InitStack("main", NULL, qfalse, qtrue);
	}
}

/**
 * @brief Quits the current running game by calling the @c shutdown callback
 */
static void GAME_Exit_f (void)
{
	GAME_SetMode(GAME_NONE);
}

/**
 * @brief Called every frame and allows us to hook into the current running game mode
 */
void GAME_Frame (void)
{
	const cgame_export_t *list;

	list = GAME_GetCurrentType();
	if (list && list->RunFrame != NULL)
		list->RunFrame();
}

/**
 * @brief Get a model for an item.
 * @param[in] od The object definition to get the model from.
 * @param[out] menuModel The menu model pointer.
 * @return The model path for the item. Never @c NULL.
 */
const char* GAME_GetModelForItem (const objDef_t *od, uiModel_t** uiModel)
{
	const cgame_export_t *list = GAME_GetCurrentType();
	if (list && list->GetModelForItem != NULL) {
		const char *model = list->GetModelForItem(od);
		if (model != NULL) {
			if (uiModel != NULL)
				*uiModel = UI_GetUIModel(model);
			return model;
		}
	}

	if (uiModel != NULL)
		*uiModel = NULL;
	return od->model;
}

mapDef_t* Com_GetMapDefinitionByID (const char *mapDefID)
{
	int i;

	assert(mapDefID);

	for (i = 0; i < cls.numMDs; i++) {
		mapDef_t *md = Com_GetMapDefByIDX(i);
		if (!strcmp(md->id, mapDefID))
			return md;
	}

	Com_DPrintf(DEBUG_CLIENT, "Com_GetMapDefinition: Could not find mapdef with id: '%s'\n", mapDefID);
	return NULL;
}

mapDef_t* Com_GetMapDefByIDX (int index)
{
	return &cls.mds[index];
}

void GAME_InitStartup (void)
{
	Cmd_AddCommand("game_setmode", GAME_SetMode_f, "Decides with game mode should be set - takes the menu as reference");
	Cmd_AddCommand("game_exit", GAME_Exit_f, "Abort the game and let the aliens/opponents win");
	Cmd_AddCommand("game_abort", GAME_Abort_f, "Abort the game and let the aliens/opponents win");
	Cmd_AddCommand("mn_getmaps", UI_GetMaps_f, "The initial map to show");
	Cmd_AddCommand("mn_nextmap", UI_ChangeMap_f, "Switch to the next valid map for the selected gametype");
	Cmd_AddCommand("mn_prevmap", UI_ChangeMap_f, "Switch to the previous valid map for the selected gametype");
	Cmd_AddCommand("mn_selectmap", UI_SelectMap_f, "Switch to the map given by the parameter - may be invalid for the current gametype");
}
