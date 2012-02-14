/**
 * @file cp_cgame_callbacks.h
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

#ifndef CP_CGAME_CALLBACKS_H
#define CP_CGAME_CALLBACKS_H

#include "../cl_game.h"

const cgame_import_t *cgImport;

const char* GAME_CP_GetItemModel(const char *string);
void GAME_CP_InitStartup(void);
void GAME_CP_Shutdown(void);
qboolean GAME_CP_Spawn(chrList_t *chrList);
void GAME_CP_Results(struct dbuffer *msg, int winner, int *numSpawned, int *numAlive, int numKilled[][MAX_TEAMS], int numStunned[][MAX_TEAMS], qboolean nextmap);
qboolean GAME_CP_ItemIsUseable(const objDef_t *od);
equipDef_t *GAME_CP_GetEquipmentDefinition(void);
void GAME_CP_CharacterCvars(const character_t *chr);
qboolean GAME_CP_TeamIsKnown(const teamDef_t *teamDef);
void GAME_CP_Drop(void);
struct dbuffer *GAME_CP_InitializeBattlescape(const chrList_t *team);
void GAME_CP_Frame(float secondsSinceLastFrame);
const char* GAME_CP_GetTeamDef(void);

#endif
