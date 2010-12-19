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
#include "battlescape/cl_localentity.h"
#include "menu/m_main.h"
#include "menu/m_nodes.h"
#include "cl_team.h"
#include "battlescape/events/e_main.h"
#include "cl_inventory.h"

static invList_t invList[MAX_INVLIST];

typedef struct gameTypeList_s {
	const char *name;
	const char *menu;
	int gametype;
	void (*init)(void);
	void (*shutdown)(void);
	/** soldier spawn functions may differ between the different gametypes */
	qboolean (*spawn)(void);
	/** each gametype can handle the current team in a different way */
	int (*getteam)(void);
	/** some gametypes only support special maps */
	const mapDef_t* (*mapinfo)(int step);
	/** some gametypes require extra data in the results parsing (like e.g. campaign mode) */
	void (*results)(struct dbuffer *msg, int, int*, int*, int[][MAX_TEAMS], int[][MAX_TEAMS]);
	/** check whether the given item is usable in the current game mode */
	qboolean (*itemIsUseable)(const objDef_t *od);
	/** shows item info if not resolvable via objDef_t */
	void (*displayiteminfo)(menuNode_t *node, const char *string);
	/** returns the equipment definition the game mode is using */
	equipDef_t * (*getequipdef)(void);
	/** update character display values for game type dependent stuff */
	void (*charactercvars)(const character_t *chr);
	/** checks whether the given team is known in the particular gamemode */
	qboolean (*teamisknown)(const teamDef_t *teamDef);
	/** called on errors */
	void (*drop)(void);
	/** called after the team spawn messages where send, can e.g. be used to set initial actor states */
	void (*initializebattlescape)(const chrList_t *team);
	/** callback that is executed every frame */
	void (*frame)(void);
	/** if you want to display a different model for the given object in your game mode, implement this function */
	const char* (*getmodelforitem)(const objDef_t*od, menuModel_t** menuModel);
} gameTypeList_t;

static const gameTypeList_t gameTypeList[] = {
	{"Multiplayer mode", "multiplayer", GAME_MULTIPLAYER, GAME_MP_InitStartup, GAME_MP_Shutdown, NULL, GAME_MP_GetTeam, GAME_MP_MapInfo, GAME_MP_Results, NULL, NULL, GAME_MP_GetEquipmentDefinition, NULL, NULL, NULL, NULL, NULL, NULL},
	{"Campaign mode", "campaigns", GAME_CAMPAIGN, GAME_CP_InitStartup, GAME_CP_Shutdown, GAME_CP_Spawn, GAME_CP_GetTeam, GAME_CP_MapInfo, GAME_CP_Results, GAME_CP_ItemIsUseable, GAME_CP_DisplayItemInfo, GAME_CP_GetEquipmentDefinition, GAME_CP_CharacterCvars, GAME_CP_TeamIsKnown, GAME_CP_Drop, GAME_CP_InitializeBattlescape, GAME_CP_Frame, GAME_CP_GetModelForItem},
	{"Skirmish mode", "skirmish", GAME_SKIRMISH, GAME_SK_InitStartup, GAME_SK_Shutdown, NULL, GAME_SK_GetTeam, GAME_SK_MapInfo, GAME_SK_Results, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL},

	{NULL, NULL, 0, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL}
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
}

void GAME_GenerateTeam (const char *teamDefID, const equipDef_t *ed, int teamMembers)
{
	int i;

	if (teamMembers > lengthof(characters))
		Com_Error(ERR_DROP, "More than the allowed amount of team members");

	if (ed == NULL)
		Com_Error(ERR_DROP, "No equipment definition given");

	GAME_ResetCharacters();

	for (i = 0; i < teamMembers; i++) {
		character_t *chr = GAME_GetCharacter(i);
		CL_GenerateCharacter(chr, teamDefID);
		/* pack equipment */
		cls.i.EquipActor(&cls.i, &chr->i, ed, chr);

		chrDisplayList.chr[i] = chr;
	}
	chrDisplayList.num = i;
}

static const gameTypeList_t *GAME_GetCurrentType (void)
{
	const gameTypeList_t *list = gameTypeList;

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
	const gameTypeList_t *list = GAME_GetCurrentType();
	if (list != NULL)
		GAME_RestartMode(list->gametype);
}

void GAME_RestartMode (int gametype)
{
	GAME_SetMode(GAME_NONE);
	GAME_SetMode(gametype);
}

/**
 * @brief Shows game type specific item information (if it's not resolvable via @c objDef_t).
 * @param[in] node The menu node to show the information in.
 * @param[in] string The id of the 'thing' to show information for.
 */
void GAME_DisplayItemInfo (menuNode_t *node, const char *string)
{
	const gameTypeList_t *list = GAME_GetCurrentType();
	if (list != NULL && list->displayiteminfo)
		list->displayiteminfo(node, string);
}

void GAME_SetMode (int gametype)
{
	const gameTypeList_t *list = gameTypeList;

	if (gametype < 0 || gametype > GAME_MAX) {
		Com_Printf("Invalid gametype %i given\n", gametype);
		return;
	}

	if (cls.gametype == gametype)
		return;

	list = GAME_GetCurrentType();
	if (list) {
		Com_Printf("Shutdown gametype '%s'\n", list->name);
		list->shutdown();

		/* we dont need to go back to "main" stack if we are already on this stack */
		if (!MN_IsWindowOnStack("main"))
			MN_InitStack("main", "", qtrue, qtrue);
	}

	cls.gametype = gametype;

	CL_Disconnect();

	list = GAME_GetCurrentType();
	if (list) {
		Com_Printf("Change gametype to '%s'\n", list->name);
		memset(&invList, 0, sizeof(invList));
		/* inventory structure switched/initialized */
		INV_InitInventory(&cls.i, &csi, invList, lengthof(invList));
		list->init();
	}
}

static void MN_MapInfoGetNext (int step)
{
	const mapDef_t *md;

	cls.currentSelectedMap += step;

	if (cls.currentSelectedMap < 0)
		cls.currentSelectedMap = csi.numMDs - 1;

	cls.currentSelectedMap %= csi.numMDs;

	md = &csi.mds[cls.currentSelectedMap];

	/* special purpose maps are not startable without the specific context */
	if (md->map[0] == '.')
		MN_MapInfoGetNext(step);
}

/**
 * @brief Prints the map info for the server creation dialogue
 * @todo Skip special map that start with a '.' (e.g. .baseattack)
 */
static void MN_MapInfo (int step)
{
	const char *mapname;
	const mapDef_t *md;
	const gameTypeList_t *list = gameTypeList;

	if (!csi.numMDs)
		return;

	MN_MapInfoGetNext(step);

	md = NULL;
	while (list->name) {
		if (list->gametype == cls.gametype) {
			md = list->mapinfo(step);
			break;
		}
		list++;
	}

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

static void MN_GetMaps_f (void)
{
	MN_MapInfo(0);
}

static void MN_ChangeMap_f (void)
{
	if (!strcmp(Cmd_Argv(0), "mn_nextmap"))
		MN_MapInfo(1);
	else
		MN_MapInfo(-1);
}

static void MN_SelectMap_f (void)
{
	const char *mapname;
	int i;

	if (Cmd_Argc() != 2) {
		Com_Printf("Usage: %s <mapname>\n", Cmd_Argv(0));
		return;
	}

	if (!csi.numMDs)
		return;

	mapname = Cmd_Argv(1);

	for (i = 0; i < csi.numMDs; i++) {
		const mapDef_t *md = &csi.mds[i];
		if (strcmp(md->map, mapname))
			continue;
		cls.currentSelectedMap = i;
		MN_MapInfo(0);
		return;
	}

	for (i = 0; i < csi.numMDs; i++) {
		const mapDef_t *md = &csi.mds[i];
		if (strcmp(md->id, mapname))
			continue;
		cls.currentSelectedMap = i;
		MN_MapInfo(0);
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
	const gameTypeList_t *list = gameTypeList;

	if (Cmd_Argc() == 2)
		modeName = Cmd_Argv(1);
	else
		modeName = MN_GetActiveWindowName();

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

qboolean GAME_ItemIsUseable (const objDef_t *od)
{
	const gameTypeList_t *list = GAME_GetCurrentType();

	if (list && list->itemIsUseable)
		return list->itemIsUseable(od);

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
	const gameTypeList_t *list = GAME_GetCurrentType();
	if (list)
		list->results(msg, winner, numSpawned, numAlive, numKilled, numStunned);
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
 * @param buf The net channel buffer to write the character data into.
 * @param chr The character to get the data from.
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

static qboolean GAME_Spawn (void)
{
	int i;

	/* If there is no active gametype we create a team with default values.
	 * This is e.g. the case when someone starts a map with the map command */
	if (GAME_GetCurrentType() == NULL || chrDisplayList.num == 0) {
		const char *teamDefID = cl_team->integer == TEAM_PHALANX ? "phalanx" : "taman";
		const equipDef_t *ed = INV_GetEquipmentDefinitionByID("multiplayer_initial");
		memset(&invList, 0, sizeof(invList));
		/* inventory structure switched/initialized */
		INV_InitInventory(&cls.i, &csi, invList, lengthof(invList));
		GAME_GenerateTeam(teamDefID, ed, MAX_ACTIVETEAM);
	}

	for (i = 0; i < MAX_ACTIVETEAM; i++)
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
	const gameTypeList_t *list = GAME_GetCurrentType();
	if (list && list->initializebattlescape)
		list->initializebattlescape(team);
}

/**
 * @brief Called during startup of mission to send team info
 */
void GAME_SpawnSoldiers (void)
{
	const gameTypeList_t *list = GAME_GetCurrentType();
	qboolean spawnStatus;

	/* this callback is responsible to set up the cl.chrList */
	if (list && list->spawn)
		spawnStatus = list->spawn();
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
	const gameTypeList_t *list = GAME_GetCurrentType();

	if (list && list->getteam != NULL)
		return list->getteam();

	return TEAM_DEFAULT;
}

equipDef_t *GAME_GetEquipmentDefinition (void)
{
	const gameTypeList_t *list = GAME_GetCurrentType();

	if (list && list->getequipdef != NULL)
		return list->getequipdef();
	return NULL;
}

qboolean GAME_TeamIsKnown (const teamDef_t *teamDef)
{
	const gameTypeList_t *list = GAME_GetCurrentType();

	if (!teamDef)
		return qfalse;

	if (list && list->teamisknown != NULL)
		return list->teamisknown(teamDef);
	return qtrue;
}

void GAME_CharacterCvars (const character_t *chr)
{
	const gameTypeList_t *list = GAME_GetCurrentType();
	if (list && list->charactercvars != NULL)
		list->charactercvars(chr);
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
	const gameTypeList_t *list = GAME_GetCurrentType();

	if (list && list->drop) {
		list->drop();
	} else {
		SV_Shutdown("Drop", qfalse);
		GAME_SetMode(GAME_NONE);
		MN_InitStack("main", NULL, qfalse, qtrue);
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
	const gameTypeList_t *list;

	list = GAME_GetCurrentType();
	if (list && list->frame != NULL)
		list->frame();
}

/**
 * @brief Get a model for an item.
 * @param[in] od The object definition to get the model from.
 * @param[out] menuModel The menu model pointer.
 * @return The model path for the item. Never @c NULL.
 */
const char* GAME_GetModelForItem (const objDef_t *od, menuModel_t** menuModel)
{
	const gameTypeList_t *list = GAME_GetCurrentType();
	if (list && list->getmodelforitem != NULL) {
		const char *model = list->getmodelforitem(od, menuModel);
		if (model != NULL)
			return model;
	}

	if (menuModel != NULL)
		*menuModel = NULL;
	return od->model;
}

void GAME_InitStartup (void)
{
	Cmd_AddCommand("game_setmode", GAME_SetMode_f, "Decides with game mode should be set - takes the menu as reference");
	Cmd_AddCommand("game_exit", GAME_Exit_f, "Abort the game and let the aliens/opponents win");
	Cmd_AddCommand("game_abort", GAME_Abort_f, "Abort the game and let the aliens/opponents win");
	Cmd_AddCommand("mn_getmaps", MN_GetMaps_f, "The initial map to show");
	Cmd_AddCommand("mn_nextmap", MN_ChangeMap_f, "Switch to the next valid map for the selected gametype");
	Cmd_AddCommand("mn_prevmap", MN_ChangeMap_f, "Switch to the previous valid map for the selected gametype");
	Cmd_AddCommand("mn_selectmap", MN_SelectMap_f, "Switch to the map given by the parameter - may be invalid for the current gametype");
}
