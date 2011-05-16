/**
 * @file cp_alienbase.h
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

#ifndef CP_ALIENBASE_H
#define CP_ALIENBASE_H

/** @brief Alien Base */
typedef struct alienBase_s {
	int idx;				/**< idx of base in alienBases[] */
	vec2_t pos;			/**< position of the base (longitude, latitude) */
	int supply;			/**< Number of supply missions this base was already involved in */
	float stealth;		/**< How much PHALANX know this base. Decreases depending on PHALANX observation
						 * and base is known if stealth < 0 */
} alienBase_t;

#define AB_Foreach(var) LIST_Foreach(ccs.alienBases, alienBase_t, var)
alienBase_t* AB_GetByIDX(int baseIDX);

#define AB_Exists() (!LIST_IsEmpty(ccs.alienBases))

void AB_SetAlienBasePosition(vec2_t pos);
alienBase_t* AB_BuildBase(const vec2_t pos);
void AB_DestroyBase(alienBase_t *base);
void AB_UpdateStealthForAllBase(void);
void AB_BaseSearchedByNations(void);
qboolean AB_CheckSupplyMissionPossible(void);
alienBase_t* AB_ChooseBaseToSupply(void);
void AB_SupplyBase(alienBase_t *base, qboolean decreaseStealth);
int AB_GetAlienBaseNumber(void);
void CP_SpawnAlienBaseMission(alienBase_t *alienBase);

void AB_InitStartup(void);
void AB_Shutdown(void);

#endif	/* CP_ALIENBASE_H */
