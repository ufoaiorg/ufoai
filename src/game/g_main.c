/**
 * @file g_main.c
 * @brief Main game functions.
 */

/*
All original materal Copyright (C) 2002-2006 UFO: Alien Invasion team.

26/06/06, Eddy Cullen (ScreamingWithNoSound):
	Reformatted to agreed style.
	Added doxygen file comment.
	Updated copyright notice.

Original file from Quake 2 v3.21: quake2-2.31/game/g_main.c
Copyright (C) 1997-2001 Id Software, Inc.

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

#include "g_local.h"

#if defined DEBUG && defined _MSC_VER
#include <intrin.h>
#endif

game_locals_t game;
level_locals_t level;
game_import_t gi;
game_export_t globals;

edict_t *g_edicts;

cvar_t *password;
cvar_t *spectator_password;
cvar_t *needpass;
cvar_t *maxplayers;
cvar_t *maxsoldiers;
cvar_t *maxsoldiersperplayer;
cvar_t *sv_enablemorale;
cvar_t *maxspectators;
cvar_t *maxentities;
cvar_t *dedicated;
cvar_t *developer;

cvar_t *filterban;

cvar_t *sv_gravity;

cvar_t *sv_cheats;

cvar_t *sv_ai;
cvar_t *sv_teamplay;
cvar_t *sv_maxclients;
cvar_t *sv_reaction_leftover;
cvar_t *sv_shot_origin;

cvar_t *ai_alien;
cvar_t *ai_civilian;
cvar_t *ai_equipment;
cvar_t *ai_numaliens;
cvar_t *ai_numcivilians;
cvar_t *ai_numactors;
cvar_t *ai_autojoin;

/* morale cvars */
cvar_t *mob_death;
cvar_t *mob_wound;
cvar_t *mof_watching;
cvar_t *mof_teamkill;
cvar_t *mof_civilian;
cvar_t *mof_enemy;
cvar_t *mor_pain;

/*everyone gets this times morale damage */
cvar_t *mor_default;

/*at this distance the following two get halfed (exponential scale) */
cvar_t *mor_distance;

/*at this distance the following two get halfed (exponential scale) */
cvar_t *mor_victim;

/*at this distance the following two get halfed (exponential scale) */
cvar_t *mor_attacker;

/* how much the morale depends on the size of the damaged team */
cvar_t *mon_teamfactor;

cvar_t *mor_regeneration;
cvar_t *mor_shaken;
cvar_t *mor_panic;

cvar_t *m_sanity;
cvar_t *m_rage;
cvar_t *m_rage_stop;
cvar_t *m_panic_stop;

cvar_t *g_nodamage;

cvar_t *difficulty;

extern void SpawnEntities(char *mapname, char *entities);
extern void G_RunFrame(void);

invList_t invChain[MAX_INVLIST];


/*=================================================================== */


/**
 * @brief This will be called when the dll is first loaded
 * @note only happens when a new game is started or a save game is loaded.
 */
void InitGame(void)
{
	gi.dprintf("==== InitGame ====\n");

	/* noset vars */
	dedicated = gi.cvar("dedicated", "0", CVAR_SERVERINFO | CVAR_NOSET, NULL);

	/* latched vars */
	sv_cheats = gi.cvar("cheats", "0", CVAR_SERVERINFO | CVAR_LATCH, NULL);
	gi.cvar("gamename", GAMEVERSION, CVAR_SERVERINFO | CVAR_LATCH, NULL);
	gi.cvar("gamedate", __DATE__, CVAR_SERVERINFO | CVAR_LATCH, NULL);
	developer = gi.cvar("developer", "0", 0, NULL);

	/* max. players per team (original quake) */
	maxplayers = gi.cvar("maxplayers", "8", CVAR_SERVERINFO | CVAR_LATCH, NULL);
	/* max. soldiers per team */
	maxsoldiers = gi.cvar("maxsoldiers", "4", CVAR_ARCHIVE | CVAR_SERVERINFO | CVAR_LATCH, NULL);
	/* max soldiers per player */
	maxsoldiersperplayer = gi.cvar("maxsoldiersperplayer", "8", CVAR_ARCHIVE | CVAR_SERVERINFO | CVAR_LATCH, NULL);
	/* enable moralestates in multiplayer */
	sv_enablemorale = gi.cvar("sv_enablemorale", "1", CVAR_ARCHIVE | CVAR_SERVERINFO | CVAR_LATCH, NULL);
	maxspectators = gi.cvar("maxspectators", "8", CVAR_SERVERINFO | CVAR_LATCH, NULL);
	maxentities = gi.cvar("maxentities", "1024", CVAR_LATCH, NULL);

	/* change anytime vars */
	password = gi.cvar("password", "", CVAR_USERINFO, NULL);
	spectator_password = gi.cvar("spectator_password", "", CVAR_USERINFO, NULL);
	needpass = gi.cvar("needpass", "0", CVAR_SERVERINFO, NULL);
	filterban = gi.cvar("filterban", "1", 0, NULL);
	sv_ai = gi.cvar("sv_ai", "1", 0, NULL);
	sv_teamplay = gi.cvar("sv_teamplay", "0", CVAR_ARCHIVE | CVAR_SERVERINFO, NULL);
	/* how many connected clients */
	sv_maxclients = gi.cvar("maxclients", "1", CVAR_SERVERINFO | CVAR_LATCH, NULL);
	/* reaction leftover is 0 for acceptance testing; should default to 13 */
	sv_reaction_leftover = gi.cvar("sv_reaction_leftover", "0", CVAR_LATCH, "Minimum TU left over by reaction fire");
	sv_shot_origin = gi.cvar("sv_shot_origin", "8", 0, "Assumed distance of muzzle from model");

	ai_alien = gi.cvar("ai_alien", "alien", 0, NULL);
	ai_civilian = gi.cvar("ai_civilian", "civilian", 0, NULL);
	ai_equipment = gi.cvar("ai_equipment", "human_phalanx_initial", 0, NULL);
	/* aliens in singleplayer (can differ each mission) */
	ai_numaliens = gi.cvar("ai_numaliens", "8", 0, NULL);
	/* civilians for singleplayer */
	ai_numcivilians = gi.cvar("ai_numcivilians", "8", 0, NULL);
	/* aliens in multiplayer */
	ai_numactors = gi.cvar("ai_numactors", "8", CVAR_ARCHIVE, NULL);
	/* autojoin aliens */
	ai_autojoin = gi.cvar("ai_autojoin", "0", 0, NULL);

	/* FIXME: Apply CVAR_NOSET after balancing */
	mob_death = gi.cvar("mob_death", "10", CVAR_LATCH, NULL);
	mob_wound = gi.cvar("mob_wound", "0.1", CVAR_LATCH, NULL);
	mof_watching = gi.cvar("mof_watching", "1.7", CVAR_LATCH, NULL);
	mof_teamkill = gi.cvar("mof_teamkill", "2.0", CVAR_LATCH, NULL);
	mof_civilian = gi.cvar("mof_civilian", "0.3", CVAR_LATCH, NULL);
	mof_enemy = gi.cvar("mof_ememy", "0.5", CVAR_LATCH, NULL);
	mor_pain = gi.cvar("mof_pain", "3.6", CVAR_LATCH, NULL);
	/*everyone gets this times morale damage */
	mor_default = gi.cvar("mor_default", "0.3", CVAR_LATCH, NULL);
	/*at this distance the following two get halfed (exponential scale) */
	mor_distance = gi.cvar("mor_distance", "120", CVAR_LATCH, NULL);
	/*at this distance the following two get halfed (exponential scale) */
	mor_victim = gi.cvar("mor_victim", "0.7", CVAR_LATCH, NULL);
	/*at this distance the following two get halfed (exponential scale) */
	mor_attacker = gi.cvar("mor_attacker", "0.3", CVAR_LATCH, NULL);
	/* how much the morale depends on the size of the damaged team */
	mon_teamfactor = gi.cvar("mon_teamfactor", "0.6", CVAR_LATCH, NULL);

	mor_regeneration = gi.cvar("mor_regeneration", "15", CVAR_LATCH, NULL);
	mor_shaken = gi.cvar("mor_shaken", "50", CVAR_LATCH, NULL);
	mor_panic = gi.cvar("mor_panic", "30", CVAR_LATCH, NULL);

	m_sanity = gi.cvar("m_sanity", "1.0", CVAR_LATCH, NULL);
	m_rage = gi.cvar("m_rage", "0.6", CVAR_LATCH, NULL);
	m_rage_stop = gi.cvar("r_rage_stop", "2.0", CVAR_LATCH, NULL);
	m_panic_stop = gi.cvar("m_panic_stop", "1.0", CVAR_LATCH, NULL);

	/* TODO: add CVAR_DEVELOPER flag which if !COM_CheckParm("-developer") acts like CVAR_NOSET and hides the cvar from the console */
	g_nodamage = gi.cvar("g_nodamage", "0", 0, NULL);

	difficulty = gi.cvar("difficulty", "0", CVAR_NOSET, NULL);

	*game.helpmessage1 = *game.helpmessage2 = '\0';

	game.maxentities = maxentities->value;
	game.maxplayers = maxplayers->value;

	/* initialize all entities for this game */
	g_edicts = gi.TagMalloc(game.maxentities * sizeof(g_edicts[0]), TAG_GAME);
	globals.edicts = g_edicts;
	globals.max_edicts = game.maxentities;
	globals.num_edicts = game.maxplayers;

	/* initialize all players for this game */
	game.players = gi.TagMalloc(game.maxplayers * 2 * sizeof(game.players[0]), TAG_GAME);
	globals.players = game.players;
	globals.max_players = game.maxplayers;

	/* init csi and inventory */
	Com_InitCSI(gi.csi);
	Com_InitInventory(invChain);
}


/*=================================================================== */


/**
 * @brief Free the tags TAG_LEVEL and TAG_GAME
 * @sa Mem_FreeTags
 */
void ShutdownGame(void)
{
	gi.dprintf("==== ShutdownGame ====\n");

	gi.FreeTags(TAG_LEVEL);
	gi.FreeTags(TAG_GAME);
}


/**
 * @brief Returns a pointer to the structure with all entry points and global variables
 */
game_export_t *GetGameAPI(game_import_t * import)
{
	gi = *import;
	srand(gi.seed);

	globals.apiversion = GAME_API_VERSION;
	globals.Init = InitGame;
	globals.Shutdown = ShutdownGame;
	globals.SpawnEntities = SpawnEntities;

	globals.ClientConnect = G_ClientConnect;
	globals.ClientUserinfoChanged = G_ClientUserinfoChanged;
	globals.ClientDisconnect = G_ClientDisconnect;
	globals.ClientBegin = G_ClientBegin;
	globals.ClientCommand = G_ClientCommand;
	globals.ClientAction = G_ClientAction;
	globals.ClientEndRound = G_ClientEndRound;
	globals.ClientTeamInfo = G_ClientTeamInfo;

	globals.RunFrame = G_RunFrame;

	globals.ServerCommand = ServerCommand;

	globals.edict_size = sizeof(edict_t);
	globals.player_size = sizeof(player_t);

	return &globals;
}

#ifndef GAME_HARD_LINKED
/* this is only here so the functions in q_shared.c and q_shwin.c can link */
void Sys_Error(char *error, ...)
{
	va_list argptr;
	char text[1024];

#if defined DEBUG && defined _MSC_VER
	__debugbreak();	/* break execution before game shutdown */
#endif

	va_start(argptr, error);
	vsprintf(text, error, argptr);
	va_end(argptr);

	gi.error(ERR_FATAL, "%s", text);
}

void Com_Printf(char *msg, ...)
{
	va_list argptr;
	char text[1024];

	va_start(argptr, msg);
	vsprintf(text, msg, argptr);
	va_end(argptr);

	gi.dprintf("%s", text);
}

void Com_DPrintf(char *msg, ...)
{
	va_list argptr;
	char text[1024];

	if (!developer->value)
		return;
	va_start(argptr, msg);
	vsprintf(text, msg, argptr);
	va_end(argptr);

	gi.dprintf("%s", text);
}
#endif

/*====================================================================== */


/**
 * @brief If password or spectator_password has changed, update needpass as needed
 */
void CheckNeedPass(void)
{
	int need;

	if (password->modified || spectator_password->modified) {
		password->modified = spectator_password->modified = qfalse;

		need = 0;

		if (*password->string && Q_stricmp(password->string, "none"))
			need |= 1;
		if (*spectator_password->string && Q_stricmp(spectator_password->string, "none"))
			need |= 2;

		gi.cvar_set("needpass", va("%d", need));
	}
}

/**
  * @brief Sends character stats like assigned missions and kills back to client
  *
  * @note first short is the ucn to allow the client to identify the character
  * @note parsed in CL_ParseCharacterData
  * @sa CL_ParseCharacterData
  * @sa G_EndGame
  */
static void G_SendCharacterData( edict_t* ent )
{
	int k;

	assert(ent);
#ifdef DEBUG
	if (!ent)
		return;	/* never reached. need for code analyst. */
#endif

	/* write character number */
	gi.WriteShort(ent->chr.ucn);

	gi.WriteShort(ent->HP);
	gi.WriteByte(ent->morale);

	/* scores */
	for (k = 0; k < KILLED_NUM_TYPES; k++)
		gi.WriteShort(ent->chr.kills[k]);
}

/**
  * @brief Handles the end of a game
  * @note Called by game_abort command (or sv win [team])
  * @sa G_RunFrame
  */
void G_EndGame(int team)
{
	edict_t *ent;
	int i, j = 0;
	int	number_of_teams;

	/* if aliens won, make sure every soldier dies */
	if (team == TEAM_ALIEN) {
		level.num_alive[TEAM_PHALANX] = 0;
		for (i = 0, ent = g_edicts; i < globals.num_edicts; i++, ent++)
			if ( ent->inuse && (ent->type == ET_ACTOR || ent->type == ET_UGV)
				 && !(ent->state & STATE_DEAD) && ent->team == TEAM_PHALANX ) {
				ent->state = STATE_DEAD;
				ent->HP=0;
				gi.AddEvent(PM_ALL, EV_ACTOR_STATECHANGE);
				gi.WriteShort(ent->number);
				gi.WriteShort(STATE_DEAD);
			}
	}

	/* Make everything visible to anyone who can't already see it */
	for (i = 0, ent = g_edicts; i < globals.num_edicts; ent++, i++)
		if (ent->inuse) {
			G_AppearPerishEvent(~G_VisToPM(ent->visflags), 1, ent);
			if ((ent->type == ET_ACTOR || ent->type == ET_UGV) && !(ent->state & STATE_DEAD))
				G_SendInventory(~G_TeamToPM(ent->team), ent);
		}

	/* send results */
	Com_DPrintf("Sending results for game won by team %i.\n", team);
	gi.AddEvent(PM_ALL, EV_RESULTS);
	number_of_teams = MAX_TEAMS;
	gi.WriteByte(number_of_teams);
	gi.WriteByte(team);

	gi.WriteShort(2 * number_of_teams);
	for (i = 0; i < number_of_teams; i++) {
		gi.WriteByte(level.num_spawned[i]);
		gi.WriteByte(level.num_alive[i]);
	}

	gi.WriteShort(number_of_teams * number_of_teams);
	for (i = 0; i < number_of_teams; i++)
		for (j = 0; j < number_of_teams; j++) {
			gi.WriteByte(level.num_kills[i][j]);
		}

	gi.WriteShort(number_of_teams * number_of_teams);
	for (i = 0; i < number_of_teams; i++)
		for (j = 0; j < number_of_teams; j++) {
			gi.WriteByte(level.num_stuns[i][j]);
		}

	/* how many actors */
	for (j = 0, i = 0, ent = g_edicts; i < globals.num_edicts; ent++, i++)
		if ( ent->inuse && (ent->type == ET_ACTOR || ent->type == ET_UGV)
			 && ent->team == TEAM_PHALANX )
			j++;

	Com_DPrintf("Sending results with %i actors.\n", j);
	/* this is (size of updateCharacter_t) * number of phalanx actors */
	gi.WriteShort(((KILLED_NUM_TYPES + 2) * 2 + 1) * j );

	if (j) {
		for (i = 0, ent = g_edicts; i < globals.num_edicts; ent++, i++)
			if ( ent->inuse && (ent->type == ET_ACTOR || ent->type == ET_UGV)
				 && ent->team == TEAM_PHALANX ) {
				Com_DPrintf("Sending results for actor %i.\n", i);
				G_SendCharacterData(ent);
			}
	}

	gi.EndEvents();
}


#if 0
/**
 * @brief checks for a mission objective
 * @param[in] activeTeams The number of teams with living actors
 * @return 1 if objective successful
 * @return 0 if objective unsuccessful
 * @return -1 if objective state unclear
 */
int G_MissionObjective (int activeTeams, int* winningTeam)
{
	/* TODO: put objective flag to level */
	switch (level.objective) {
	/* TODO: enum for objectives */
	case OBJ_RESCUE_CIVILIANS:
		if (!level.num_alive[TEAM_CIVILIAN])
			return 0;
		if (!level.num_alive[TEAM_ALIEN])
			return 1;
	/* TODO: More objectives */
	default:
		return -1;
	}
}
#endif

/**
 * @brief Checks whether there are still actors to fight with left
 * @sa G_EndGame
 */
void G_CheckEndGame(void)
{
	int activeTeams;
	int i, last;

	if (level.intermissionTime) /* already decided */
		return;

	/* FIXME: count from 0 to get the civilians for objectives */
	for (i = 1, activeTeams = 0, last = 0; i < MAX_TEAMS; i++)
		if (level.num_alive[i]) {
			last = i;
			activeTeams++;
		}

	/* TODO: < 2 does not work when we count civilians */
	/* prepare for sending results */
	if (activeTeams < 2 /* || G_MissionObjective(activeTeams, &level.winningTeam) != -1*/ ) {
		if (activeTeams == 0)
			level.winningTeam = 0;
		else if (activeTeams == 1)
			level.winningTeam = last;
		level.intermissionTime = level.time + (last == TEAM_ALIEN ? 10.0 : 3.0);
	}
}

/**
 * @brief
 * @sa SV_RunGameFrame
 * @sa G_EndGame
 * @sa AI_Run
 */
void G_RunFrame(void)
{
	level.framenum++;
	level.time = level.framenum * FRAMETIME;
/*	Com_Printf( "frame: %i   time: %f\n", level.framenum, level.time ); */

	/* check for intermission */
	if (level.intermissionTime && level.time > level.intermissionTime) {
		G_EndGame(level.winningTeam);
#if 0
/* It still happens that game resulsts are send twice --- dangerous */

		/* if the message gets lost, the game will not end
		   until you kill someone else, so we'll try again later,
		   but much later, so that the intermission animations can end */
		level.intermissionTime = level.time + 10.0;
#endif
		level.intermissionTime = 0.0;
		return;
	}

	/* run ai */
	AI_Run();
}
