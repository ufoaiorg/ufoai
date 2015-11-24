/**
 * @file
 * @brief Header file for character (soldier, alien) related campaign functions
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

#include "../../../game/chr_shared.h"

int CHAR_GetMaxExperiencePerMission(const abilityskills_t skill);
void CHAR_UpdateSkills(struct character_s* chr);
void CHAR_UpdateData(linkedList_t* updateCharacters);
void CHAR_UpdateStats(const struct base_s* base, const struct aircraft_s* aircraft);
void CHAR_ParseData(dbuffer* msg, linkedList_t** updateCharacters);

void CHAR_InitStartup(void);
void CHAR_Shutdown(void);
