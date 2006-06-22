/*
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

game_locals_t	game;
level_locals_t	level;
game_import_t	gi;
game_export_t	globals;

int	snd_fry;
int meansOfDeath;

edict_t		*g_edicts;

cvar_t	*password;
cvar_t	*spectator_password;
cvar_t	*needpass;
cvar_t	*maxplayers;
cvar_t	*maxsoldiers;
cvar_t	*maxsoldiersperplayer;
cvar_t	*sv_enablemorale;
cvar_t	*maxspectators;
cvar_t	*maxentities;
cvar_t	*g_select_empty;
cvar_t	*dedicated;

cvar_t	*filterban;

cvar_t	*sv_gravity;

cvar_t	*sv_cheats;

cvar_t	*flood_msgs;
cvar_t	*flood_persecond;
cvar_t	*flood_waitdelay;

cvar_t	*sv_maplist;

cvar_t	*sv_ai;
cvar_t	*sv_teamplay;
cvar_t	*sv_maxclients;

cvar_t	*ai_alien;
cvar_t	*ai_civilian;
cvar_t	*ai_equipment;
cvar_t	*ai_numaliens;
cvar_t	*ai_numcivilians;
cvar_t	*ai_numactors;
cvar_t	*ai_autojoin;

cvar_t	*difficulty;

extern void SpawnEntities (char *mapname, char *entities);
extern void G_RunFrame (void);

invList_t	invChain[MAX_INVLIST];


/*=================================================================== */


/*
============
InitGame

This will be called when the dll is first loaded, which
only happens when a new game is started or a save game
is loaded.
============
*/
void InitGame (void)
{
	gi.dprintf ("==== InitGame ====\n");

	/* noset vars */
	dedicated = gi.cvar ("dedicated", "0", CVAR_SERVERINFO|CVAR_NOSET);

	/* latched vars */
	sv_cheats = gi.cvar ("cheats", "0", CVAR_SERVERINFO|CVAR_LATCH);
	gi.cvar ("gamename", GAMEVERSION , CVAR_SERVERINFO | CVAR_LATCH);
	gi.cvar ("gamedate", __DATE__ , CVAR_SERVERINFO | CVAR_LATCH);

	/* max. players per team (original quake) */
	maxplayers = gi.cvar ("maxplayers", "8", CVAR_SERVERINFO | CVAR_LATCH);
	/* max. soldiers per team */
	maxsoldiers = gi.cvar ("maxsoldiers", "4", CVAR_ARCHIVE | CVAR_SERVERINFO | CVAR_LATCH);
	/* max soldiers per player */
	maxsoldiersperplayer = gi.cvar ("maxsoldiersperplayer", "8", CVAR_ARCHIVE | CVAR_SERVERINFO | CVAR_LATCH);
	/* enable moralestates in multiplayer */
	sv_enablemorale = gi.cvar ("sv_enablemorale", "1", CVAR_ARCHIVE | CVAR_SERVERINFO | CVAR_LATCH);
	maxspectators = gi.cvar ("maxspectators", "8", CVAR_SERVERINFO|CVAR_LATCH);
	maxentities = gi.cvar ("maxentities", "1024", CVAR_LATCH);

	/* change anytime vars */
	password = gi.cvar ("password", "", CVAR_USERINFO);
	spectator_password = gi.cvar ("spectator_password", "", CVAR_USERINFO);
	needpass = gi.cvar ("needpass", "0", CVAR_SERVERINFO);
	filterban = gi.cvar ("filterban", "1", 0);
	sv_ai = gi.cvar ("sv_ai", "1", 0);
	sv_teamplay = gi.cvar ("sv_teamplay", "0", CVAR_ARCHIVE | CVAR_SERVERINFO);
	/* how many connected clients */
	sv_maxclients = gi.cvar ("maxclients", "1", CVAR_SERVERINFO | CVAR_LATCH);

	ai_alien = gi.cvar ("ai_alien", "alien", 0);
	ai_civilian = gi.cvar ("ai_civilian", "civilian", 0);
	ai_equipment = gi.cvar ("ai_equipment", "standard", 0);
	/* aliens in singleplayer (can differ each mission) */
	ai_numaliens = gi.cvar ("ai_numaliens", "8", 0 );
	/* civilians for singleplayer */
	ai_numcivilians = gi.cvar ("ai_numcivilians", "8", 0 );
	/* aliens in multiplayer */
	ai_numactors = gi.cvar ("ai_numactors", "8", CVAR_ARCHIVE );
	/* autojoin aliens */
	ai_autojoin = gi.cvar ("ai_autojoin", "0", 0 );

	difficulty = gi.cvar ("difficulty", "-1", CVAR_ARCHIVE | CVAR_LATCH);

	g_select_empty = gi.cvar ("g_select_empty", "0", CVAR_ARCHIVE);

	/* flood control */
	flood_msgs = gi.cvar ("flood_msgs", "4", 0);
	flood_persecond = gi.cvar ("flood_persecond", "4", 0);
	flood_waitdelay = gi.cvar ("flood_waitdelay", "10", 0);

	/* dm map list */
	sv_maplist = gi.cvar ("sv_maplist", "", 0);

	Com_sprintf (game.helpmessage1, sizeof(game.helpmessage1), "");

	Com_sprintf (game.helpmessage2, sizeof(game.helpmessage2), "");

	game.maxentities = maxentities->value;
	game.maxplayers = maxplayers->value;

	/* initialize all entities for this game */
	g_edicts =  gi.TagMalloc (game.maxentities * sizeof(g_edicts[0]), TAG_GAME);
	globals.edicts = g_edicts;
	globals.max_edicts = game.maxentities;
	globals.num_edicts = game.maxplayers;

	/* initialize all players for this game */
	game.players = gi.TagMalloc (game.maxplayers * 2 * sizeof(game.players[0]), TAG_GAME);
	globals.players = game.players;
	globals.max_players = game.maxplayers;

	/* init csi and inventory */
	Com_InitCSI( gi.csi );
	Com_InitInventory( invChain );
}


/*=================================================================== */


void ShutdownGame (void)
{
	gi.dprintf ("==== ShutdownGame ====\n");

	gi.FreeTags (TAG_LEVEL);
	gi.FreeTags (TAG_GAME);
}


/*
=================
GetGameAPI

Returns a pointer to the structure with all entry points
and global variables
=================
*/
game_export_t *GetGameAPI (game_import_t *import)
{
	gi = *import;
	srand( gi.seed );

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
void Sys_Error (char *error, ...)
{
	va_list		argptr;
	char		text[1024];

	va_start (argptr, error);
	vsprintf (text, error, argptr);
	va_end (argptr);

	gi.error (ERR_FATAL, "%s", text);
}

void Com_Printf (char *msg, ...)
{
	va_list		argptr;
	char		text[1024];

	va_start (argptr, msg);
	vsprintf (text, msg, argptr);
	va_end (argptr);

	gi.dprintf ("%s", text);
}
#endif

/*====================================================================== */


/*
=================
CheckNeedPass
=================
*/
void CheckNeedPass (void)
{
	int need;

	/* if password or spectator_password has changed, update needpass */
	/* as needed */
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

/*
=============
ExitLevel
=============
*/
void ExitLevel (void)
{
	char	command [256];

	Com_sprintf (command, sizeof(command), "gamemap \"%s\"\n", level.changemap);
	gi.AddCommandString (command);
	level.changemap = NULL;
}

/*
=================
G_EndGame
=================
*/
void G_EndGame( int team )
{
	edict_t *ent;
	int i, j;

	/* Make everything visible to anyone who can't already see it */
	for ( i = 0, ent = g_edicts; i < globals.num_edicts; ent++, i++)
		if ( ent->inuse ) {
			G_AppearPerishEvent( G_VisToPM( ~ent->visflags ), 1, ent );
			if ( (ent->type == ET_ACTOR || ent->type == ET_UGV) && !(ent->state & STATE_DEAD) )
				G_SendInventory( PM_ALL ^ G_TeamToPM( ent->team ), ent );
		}

	/* send results */
	gi.AddEvent( PM_ALL, EV_RESULTS );
	gi.WriteByte( MAX_TEAMS );
	gi.WriteByte( team );

	for ( i = 0; i < MAX_TEAMS; i++ ) {
		gi.WriteByte( level.num_spawned[i] );
		gi.WriteByte( level.num_alive[i] );
	}

	for ( i = 0; i < MAX_TEAMS; i++ )
		for ( j = 0; j < MAX_TEAMS; j++ )
			gi.WriteByte( level.num_kills[i][j] );

	gi.WriteByte( NONE );
	gi.EndEvents();
}

/*
=================
G_CheckEndGame
=================
*/
void G_CheckEndGame( void )
{
	int activeTeams;
	int i, last;

	for ( i = 1, activeTeams = 0, last = 0; i < MAX_TEAMS; i++ )
		if ( level.num_alive[i] ) {
			last = i;
			activeTeams++;
		}

	/* prepare for sending results */
	if ( activeTeams < 2 ) {
		level.intermissionTime = level.time + 4.0;
		if ( activeTeams == 0 ) level.winningTeam = 0;
		else if ( activeTeams == 1 ) level.winningTeam = last;
	}
}

/*
================
G_RunFrame
================
*/
void G_RunFrame (void)
{
	level.framenum++;
	level.time = level.framenum*FRAMETIME;
/*	Com_Printf( "frame: %i   time: %f\n", level.framenum, level.time ); */

	/* check for intermission */
	if ( level.intermissionTime && level.time > level.intermissionTime )
		G_EndGame( level.winningTeam );

	/* run ai */
	AI_Run();
}
