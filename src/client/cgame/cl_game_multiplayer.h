/**
 * @file cl_game_multiplayer.h
 * @brief Multiplayer game type headers
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

#ifndef CL_GAME_MULITPLAYER_H
#define CL_GAME_MULITPLAYER_H

const mapDef_t* GAME_MP_MapInfo(int step);
void GAME_MP_InitStartup(const cgame_import_t *import);
void GAME_MP_Shutdown(void);
void GAME_MP_Results(struct dbuffer *msg, int winner, int *numSpawned, int *numAlive, int numKilled[][MAX_TEAMS], int numStunned[][MAX_TEAMS]);
void GAME_MP_EndRoundAnnounce(int playerNum, int team);
void GAME_MP_StartBattlescape(qboolean isTeamPlay);
void GAME_MP_NotifyEvent(event_t eventType);

#endif
