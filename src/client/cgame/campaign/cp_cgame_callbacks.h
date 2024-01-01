/**
 * @file
 */

/*
Copyright (C) 2002-2024 UFO: Alien Invasion.

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

#include "../cl_game.h"

extern const cgame_import_t* cgi;

const char* GAME_CP_GetItemModel(const char* string);
void GAME_CP_InitStartup(void);
void GAME_CP_Shutdown(void);
bool GAME_CP_Spawn(linkedList_t** chrList);
void GAME_CP_Results(dbuffer* msg, int winner, int* numSpawned, int* numAlive, int numKilled[][MAX_TEAMS], int numStunned[][MAX_TEAMS], bool nextmap);
bool GAME_CP_ItemIsUseable(const objDef_t* od);
equipDef_t* GAME_CP_GetEquipmentDefinition(void);
void GAME_CP_CharacterCvars(const character_t* chr);
bool GAME_CP_TeamIsKnown(const teamDef_t* teamDef);
character_t* GAME_CP_GetSelectedChr(void);
void GAME_CP_Drop(void);
void GAME_CP_InitializeBattlescape(dbuffer* msg, const linkedList_t* team);
void GAME_CP_InitMissionBriefing(const char** title, linkedList_t** victoryConditionsMsgIDs, linkedList_t** missionBriefingMsgIDs);
void GAME_CP_Frame(float secondsSinceLastFrame);
const char* GAME_CP_GetTeamDef(void);
void GAME_CP_HandleBaseClick(int baseIdx, int key, int col, int row);
void GAME_CP_DrawBase(int baseIdx, int x, int y, int w, int h, int col, int row, bool hover, int overlap);
void GAME_CP_DrawBaseTooltip(int baseIdx, int x, int y, int col, int row);
void GAME_CP_DrawBaseLayout(int baseIdx, int x, int y, int totalMarge, int w, int h, int padding, const vec4_t bgcolor, const vec4_t color);
void GAME_CP_DrawBaseLayoutTooltip(int baseIdx, int x, int y);
