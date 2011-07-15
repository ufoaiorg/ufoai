/**
 * @file cp_rank.h
 */

/*
Copyright (C) 2002-2011 UFO: Alien Invasion.

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

#ifndef CL_RANK_H
#define CL_RANK_H

#define MAX_RANKS	32

/** @brief Describes a rank that a recruit can gain */
typedef struct rank_s {
	const char *id;		/**< Unique identifier as parsed from the ufo files. */
	const char *name;	/**< Rank name (Captain, Squad Leader) */
	const char *shortname;	/**< Rank shortname (Cpt, Sqd Ldr) */
	const char *image;		/**< Image to show in menu */
	int type;			/**< employeeType_t */
	int mind;			/**< character mind attribute needed */
	int killedEnemies;		/**< needed amount of enemies killed */
	int killedOthers;		/**< needed amount of other actors killed */
	float factor;		/**< a factor that is used to e.g. increase the win
						 * probability for auto missions */
} rank_t;

void CL_ParseRanks(const char *name, const char **text);
int CL_GetRankIdx(const char* rankID);
rank_t *CL_GetRankByIdx(const int index);

#endif
