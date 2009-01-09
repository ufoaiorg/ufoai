/**
 * @file cl_game.c
 * @brief Shared game type code
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
#include "cl_game.h"
#include "cl_team.h"

typedef struct gameTypeList_s {
	const char *name;
	const char *menu;
	int gametype;
	void (*init)(void);
	void (*shutdown)(void);
	qboolean (*spawn)(chrList_t *chrList);
	void (*results)(int, int*, int*, int[][MAX_TEAMS], int[][MAX_TEAMS]);
} gameTypeList_t;

static const gameTypeList_t gameTypeList[] = {
	{"Multiplayer mode", "multiplayer", GAME_MULTIPLAYER, GAME_MP_InitStartup, GAME_MP_Shutdown, GAME_MP_Spawn, GAME_MP_Results},
	{"Campaign mode", "campaigns", GAME_CAMPAIGN, GAME_CP_InitStartup, GAME_CP_Shutdown, GAME_CP_Spawn, GAME_CP_Results},
	{"Skirmish mode", "skirmish", GAME_SKIRMISH, GAME_SK_InitStartup, GAME_SK_Shutdown, GAME_SK_Spawn, GAME_SK_Results},

	{NULL, NULL, 0, NULL, NULL}
};

void GAME_RestartMode (int gametype)
{
	GAME_SetMode(GAME_NONE);
	GAME_SetMode(gametype);
}

void GAME_SetMode (int gametype)
{
	const gameTypeList_t *list = gameTypeList;
	const int currentGameType = cls.gametype;

	if (gametype < 0 || gametype > GAME_MAX) {
		Com_Printf("Invalid gametype %i given\n", gametype);
		return;
	}

	if (cls.gametype == gametype)
		return;

	cls.gametype = gametype;

	while (list->name) {
		if (list->gametype == gametype) {
			Com_Printf("Change gametype to '%s'\n", list->name);
			list->init();
		} else if (list->gametype == currentGameType) {
			Com_Printf("Shutdown gametype '%s'\n", list->name);
			list->shutdown();
			/* menu cvars are the same everywhere when shutting down a game type */
			Cvar_Set("mn_main", "main");
			Cvar_Set("mn_active", "");
		}
		list++;
	}
}

/**
 * @brief Prints the map info for the server creation dialogue
 * @todo Skip special map that start with a '.' (e.g. .baseattack)
 */
static void MN_MapInfo (int step)
{
	int i = 0;
	const mapDef_t *md;
	const char *mapname;

	if (!csi.numMDs)
		return;

	cls.currentSelectedMap += step;

	if (cls.currentSelectedMap < 0)
		cls.currentSelectedMap = csi.numMDs - 1;

	cls.currentSelectedMap %= csi.numMDs;

	if (GAME_IsMultiplayer()) {
		while (!csi.mds[cls.currentSelectedMap].multiplayer) {
			i++;
			cls.currentSelectedMap += (step ? step : 1);
			if (cls.currentSelectedMap < 0)
				cls.currentSelectedMap = csi.numMDs - 1;
			cls.currentSelectedMap %= csi.numMDs;
			if (i >= csi.numMDs)
				Sys_Error("MN_MapInfo: There is no multiplayer map in any mapdef\n");
		}
	}

	md = &csi.mds[cls.currentSelectedMap];

	mapname = md->map;
	/* skip random map char */
	if (mapname[0] == '+')
		mapname++;

	Cvar_Set("mn_svmapname", md->map);
	if (FS_CheckFile(va("pics/maps/shots/%s.jpg", mapname)) != -1)
		Cvar_Set("mn_mappic", va("maps/shots/%s", mapname));
	else
		Cvar_Set("mn_mappic", va("maps/shots/default"));

	if (FS_CheckFile(va("pics/maps/shots/%s_2.jpg", mapname)) != -1)
		Cvar_Set("mn_mappic2", va("maps/shots/%s_2", mapname));
	else
		Cvar_Set("mn_mappic2", va("maps/shots/default"));

	if (FS_CheckFile(va("pics/maps/shots/%s_3.jpg", mapname)) != -1)
		Cvar_Set("mn_mappic3", va("maps/shots/%s_3", mapname));
	else
		Cvar_Set("mn_mappic3", va("maps/shots/default"));

	if (GAME_IsMultiplayer()) {
		if (md->gameTypes) {
			const linkedList_t *list = md->gameTypes;
			char buf[256] = "";
			while (list) {
				Q_strcat(buf, va("%s ", (const char *)list->data), sizeof(buf));
				list = list->next;
			}
			Cvar_Set("mn_mapgametypes", buf);
			list = LIST_ContainsString(md->gameTypes, sv_gametype->string);
			/* the map doesn't support the current selected gametype - switch to the next valid */
#if 0
			/** @todo still needed? if yes - clean this up */
			if (!list)
				MN_ChangeGametype_f();
#endif
		} else {
			Cvar_Set("mn_mapgametypes", _("all"));
		}
	}
}

static void MN_GetMaps_f (void)
{
	MN_MapInfo(0);
}

static void MN_ChangeMap_f (void)
{
	if (!Q_strcmp(Cmd_Argv(0), "mn_nextmap"))
		MN_MapInfo(1);
	else
		MN_MapInfo(-1);
}

/**
 * @brief Decides with game mode should be set - takes the menu as reference
 */
static void GAME_SetMode_f (void)
{
	const menu_t *menu = MN_GetActiveMenu();
	const gameTypeList_t *list = gameTypeList;

	if (!menu)
		return;

	while (list->name) {
		if (!Q_strcmp(list->menu, menu->name)) {
			GAME_SetMode(list->gametype);
			return;
		}
		list++;
	}
}

/**
 * @param[in] load qtrue if we are loading game, qfalse otherwise
 */
void GAME_Init (qboolean load)
{
	/* @todo are all these needed on every load? */
	/* what about RS_InitTree? how often must this be done? */
	RS_InitTree(load);		/**< Initialise all data in the research tree. */

	/* now check the parsed values for errors that are not catched at parsing stage */
	if (!load)
		CL_ScriptSanityCheck();
}

/**
 * @brief After a mission was finished this function is called
 */
void GAME_HandleResults (int winner, int *numSpawned, int *numAlive, int numKilled[][MAX_TEAMS], int numStunned[][MAX_TEAMS])
{
	const gameTypeList_t *list = gameTypeList;

	while (list->name) {
		if (list->gametype == cls.gametype) {
			list->results(winner, numSpawned, numAlive, numKilled, numStunned);
			break;
		}
		list++;
	}
}

/**
 * @brief Stores a team-list (chr-list) info to buffer (which might be a network buffer, too).
 * @sa G_ClientTeamInfo
 * @sa CL_SaveTeamMultiplayerInfo
 * @note Called in CL_Precache_f to send the team info to server
 */
static void GAME_SendCurrentTeamSpawningInfo (struct dbuffer * buf, chrList_t *team)
{
	int i, j;

	/* header */
	NET_WriteByte(buf, clc_teaminfo);
	NET_WriteByte(buf, team->num);

	Com_DPrintf(DEBUG_CLIENT, "GAME_SendCurrentTeamSpawningInfo: Upload information about %i soldiers to server\n", team->num);
	for (i = 0; i < team->num; i++) {
		character_t *chr = team->chr[i];
		assert(chr);
		assert(chr->fieldSize > 0);
		/* send the fieldSize ACTOR_SIZE_* */
		NET_WriteByte(buf, chr->fieldSize);

		/* unique character number */
		NET_WriteShort(buf, chr->ucn);

		/* name */
		NET_WriteString(buf, chr->name);

		/* model */
		NET_WriteString(buf, chr->path);
		NET_WriteString(buf, chr->body);
		NET_WriteString(buf, chr->head);
		NET_WriteByte(buf, chr->skin);

		NET_WriteShort(buf, chr->HP);
		NET_WriteShort(buf, chr->maxHP);
		NET_WriteByte(buf, chr->teamDef ? chr->teamDef->idx : NONE);
		NET_WriteByte(buf, chr->gender);
		NET_WriteByte(buf, chr->STUN);
		NET_WriteByte(buf, chr->morale);

		/** Scores @sa inv_shared.h:chrScoreGlobal_t */
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
		NET_WriteByte(buf, chr->score.rank);

		/* Send user-defined (default) reaction-state. */
		NET_WriteShort(buf, chr->reservedTus.reserveReaction);

		/* inventory */
		CL_NetSendInventory(buf, chr->inv);
	}
}

/**
 * @brief After a mission was finished this function is called
 */
void GAME_SpawnSoldiers (chrList_t *chrList)
{
	const gameTypeList_t *list = gameTypeList;

	while (list->name) {
		if (list->gametype == cls.gametype) {
			const qboolean spawnStatus = list->spawn(chrList);
			if (spawnStatus && chrList->num > 0) {
				struct dbuffer *msg;

				/* send team info */
				msg = new_dbuffer();
				GAME_SendCurrentTeamSpawningInfo(msg, chrList);
				NET_WriteMsg(cls.netStream, msg);

				msg = new_dbuffer();
				NET_WriteByte(msg, clc_stringcmd);
				NET_WriteString(msg, va("spawn %i\n", cl.servercount));
				NET_WriteMsg(cls.netStream, msg);
			}
			break;
		}
		list++;
	}
}

/**
 * @brief Let the aliens win the match
 */
static void GAME_Abort_f (void)
{
	/* aborting means letting the aliens win */
	Cbuf_AddText(va("sv win %i\n", TEAM_ALIEN));
}

/**
 * @brief Quits the current running game by calling the @c shutdown callback
 */
static void GAME_Exit_f (void)
{
	GAME_SetMode(GAME_NONE);
}

void GAME_InitStartup (void)
{
	Cmd_AddCommand("game_setmode", GAME_SetMode_f, "Decides with game mode should be set - takes the menu as reference");
	Cmd_AddCommand("game_exit", GAME_Exit_f, "Abort the game and let the aliens/opponents win");
	Cmd_AddCommand("game_abort", GAME_Abort_f, "Abort the game and let the aliens/opponents win");
	Cmd_AddCommand("mn_getmaps", MN_GetMaps_f, "The initial map to show");
	Cmd_AddCommand("mn_nextmap", MN_ChangeMap_f, "Switch to the next multiplayer map");
	Cmd_AddCommand("mn_prevmap", MN_ChangeMap_f, "Switch to the previous multiplayer map");
}
