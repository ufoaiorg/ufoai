/**
 * @file
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

#include "../../../../client.h"
#include "../../../../cgame/cl_game.h"
#include "e_event_results.h"

int CL_ParseResultsTime (const struct eventRegister_s* self, dbuffer* msg, eventTiming_t* eventTiming)
{
	return eventTiming->nextTime + 1400;
}

/**
 * @brief Reads mission result data from server
 * @sa EV_RESULTS
 * @sa G_MatchSendResults
 * @sa GAME_CP_Results_f
 */
void CL_ParseResults (const eventRegister_t* self, dbuffer* msg)
{
	/* get number of teams */
	const int num = NET_ReadByte(msg);
	if (num > MAX_TEAMS)
		Com_Error(ERR_DROP, "Too many teams in result message");

	Com_DPrintf(DEBUG_CLIENT, "Receiving results with %i teams.\n", num);

	/* get winning team */
	const int winner = NET_ReadByte(msg);
	const bool nextmap = NET_ReadByte(msg);

	if (cls.team > num)
		Com_Error(ERR_DROP, "Team number %d too high (only %d teams)", cls.team, num);

	/* get spawn and alive count */
	int num_spawned[MAX_TEAMS];
	int num_alive[MAX_TEAMS];
	OBJZERO(num_spawned);
	OBJZERO(num_alive);
	for (int i = 0; i < num; i++) {
		num_spawned[i] = NET_ReadByte(msg);
		num_alive[i] = NET_ReadByte(msg);
	}

	/* get kills */
	/* the first dimension contains the attacker team, the second the victim team */
	int num_kills[MAX_TEAMS + 1][MAX_TEAMS];
	OBJZERO(num_kills);
	for (int i = 0; i <= num; i++)
		for (int j = 0; j < num; j++)
			num_kills[i][j] = NET_ReadByte(msg);

	/* get stuns */
	int num_stuns[MAX_TEAMS + 1][MAX_TEAMS];
	OBJZERO(num_stuns);
	for (int i = 0; i <= num; i++)
		for (int j = 0; j < num; j++)
			num_stuns[i][j] = NET_ReadByte(msg);

	GAME_HandleResults(msg, winner, num_spawned, num_alive, num_kills, num_stuns, nextmap);
}
