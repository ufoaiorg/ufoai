/**
 * @file cl_game_team.h
 * @brief cgame team management headers.
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

See the GNU General Public License for more details.m

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

*/

#ifndef CL_GAME_TEAM_H
#define CL_GAME_TEAM_H

#include "../../common/xml.h"

#define NO_TEAM_SLOT_LOADED -1

void GAME_SaveTeam_f(void);
void GAME_LoadTeam_f(void);
void GAME_TeamSlotComments_f(void);
void GAME_SaveTeamState_f(void);
void GAME_ToggleActorForTeam_f(void);
void GAME_AutoTeam_f(void);
void GAME_AutoTeam(const char *equipmentDefinitionID, int teamMembers);
void GAME_UpdateInventory(inventory_t *inv, const equipDef_t *ed);
void GAME_UpdateTeamMenuParameters_f(void);
void GAME_ActorSelect_f(void);
qboolean GAME_LoadDefaultTeam(qboolean force);
qboolean GAME_SaveCharacter(xmlNode_t *p, const character_t *chr);
qboolean GAME_LoadCharacter(xmlNode_t *p, character_t *chr);

#endif
