/**
 * @file
 * @brief Serverlist menu callbacks headers for multiplayer
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

#pragma once

#define MAX_MESSAGE_TEXT 256

typedef struct teamData_s {
	int teamCount[MAX_TEAMS];	/**< team counter - parsed from server data 'teaminfo' */
	int maxteams;
	int maxPlayersPerTeam;		/**< max players per team */
} teamData_t;

extern teamData_t teamData;
extern cvar_t* rcon_client_password;
extern cvar_t* cl_maxsoldiersperteam;
extern cvar_t* cl_maxsoldiersperplayer;
extern cvar_t* cl_roundtimelimit;

struct cgame_import_s;

void GAME_MP_CallbacksInit(const struct cgame_import_s* import);
void GAME_MP_CallbacksShutdown(void);
bool GAME_MP_Rcon(const char* password, const char* command);
