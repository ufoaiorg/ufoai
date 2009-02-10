/**
 * @file cl_global.h
 * @brief Defines global data structure.
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

#ifndef CLIENT_CL_GLOBAL_H
#define CLIENT_CL_GLOBAL_H

/**
 * @brief This struct/variable holds the global data for a game.
 * @sa CL_ReadSinglePlayerData
 * @todo Remove this completely by moving all those struct to the proper places (most is indeed campaign related)
 */
typedef struct globalData_s
{
	/* == Ranks == */
	/* Global list of all ranks defined in medals.ufo. */
	rank_t ranks[MAX_RANKS];
	/* The number of entries in the list above. */
	int numRanks;

	/* == UGVs == */
	ugv_t ugvs[MAX_UGV];
	int numUGV;
} globalData_t;


extern globalData_t gd;

globalData_t gd;


#endif /* CLIENT_CL_GLOBAL_H */
