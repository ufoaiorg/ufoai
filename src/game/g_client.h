/**
 * @file
 * @brief Interface for g_client.cpp.
 *
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

#pragma once

#include "g_local.h"

player_t* G_PlayerGetNextHuman(player_t *lastPlayer);
player_t* G_PlayerGetNextAI(player_t *lastPlayer);
player_t* G_PlayerGetNextActiveHuman(player_t *lastPlayer);
player_t* G_PlayerGetNextActiveAI(player_t *lastPlayer);
playermask_t G_TeamToPM(int team);
teammask_t G_PMToVis(playermask_t playerMask);
playermask_t G_VisToPM(teammask_t teamMask);
void G_ClientPrintf(const player_t *player, int printlevel, const char *fmt, ...) __attribute__((format(__printf__, 3, 4)));
void G_GiveTimeUnits(int team);
void G_AppearPerishEvent(playermask_t player_mask, bool appear, edict_t *check, const edict_t *ent);
void G_SendInvisible(const player_t *player);
int G_GetActiveTeam(void);
bool G_ActionCheckForCurrentTeam(const player_t *player, edict_t *ent, int TU);
bool G_ActionCheckForReaction(const player_t *player, edict_t *ent, int TU);
void G_ClientStateChange(const player_t* player, edict_t *ent, int reqState, bool checkaction);
bool G_ClientCanReload(edict_t *ent, containerIndex_t containerID);
void G_ClientGetWeaponFromInventory(edict_t *ent);
bool G_ClientUseEdict(const player_t *player, edict_t *actor, edict_t *door);
int G_ClientAction(player_t *player);
bool G_SetTeamForPlayer(player_t* player, const int team);
int G_ClientGetTeamNum(const player_t *player);
int G_ClientGetTeamNumPref(const player_t *player);
bool G_ClientIsReady(const player_t *player);
edict_t* G_ClientGetFreeSpawnPointForActorSize(const player_t *player, const actorSizeEnum_t actorSize);
void G_ClientInitActorStates(const player_t *player);
void G_ClientTeamInfo(const player_t *player);
bool G_ClientBegin(player_t *player);
void G_ClientStartMatch(player_t *player);
void G_ClientUserinfoChanged(player_t *player, const char *userinfo);
bool G_ClientConnect(player_t *player, char *userinfo, size_t userinfoSize);
void G_ClientDisconnect(player_t *player);
void G_ResetClientData(void);
