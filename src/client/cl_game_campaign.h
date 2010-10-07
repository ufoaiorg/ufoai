/**
 * @file cl_game_campaign.h
 * @brief Singleplayer campaign game type headers
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

#ifndef CL_GAME_CAMPAIGN_H
#define CL_GAME_CAMPAIGN_H

#include "ui/ui_nodes.h"

void GAME_CP_DisplayItemInfo(uiNode_t *node, const char *string);
const mapDef_t* GAME_CP_MapInfo(int step);
void GAME_CP_InitStartup(void);
void GAME_CP_Shutdown(void);
qboolean GAME_CP_ItemIsUseable(const objDef_t *od);
void GAME_CP_Results(struct dbuffer *msg, int winner, int *numSpawned, int *numAlive, int numKilled[][MAX_TEAMS], int numStunned[][MAX_TEAMS]);
qboolean GAME_CP_Spawn(void);
int GAME_CP_GetTeam(void);
equipDef_t *GAME_CP_GetEquipmentDefinition(void);
void GAME_CP_CharacterCvars(const character_t *chr);
qboolean GAME_CP_TeamIsKnown(const teamDef_t *teamDef);
void GAME_CP_Drop(void);
void GAME_CP_InitializeBattlescape(const chrList_t *team);
void GAME_CP_Frame(void);
const char* GAME_CP_GetTeamDef(void);
const char* GAME_CP_GetModelForItem(const objDef_t *od);

#endif
