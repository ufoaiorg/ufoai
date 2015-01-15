/**
 * @file
 * @brief Interface for g_client.cpp.
 *
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

#include "g_local.h"

Player* G_PlayerGetNextHuman(Player* lastPlayer);
Player* G_PlayerGetNextAI(Player* lastPlayer);
Player* G_PlayerGetNextActiveHuman(Player* lastPlayer);
Player* G_PlayerGetNextActiveAI(Player* lastPlayer);
playermask_t G_TeamToPM(int team);
teammask_t G_PMToVis(playermask_t playerMask);
playermask_t G_VisToPM(teammask_t teamMask);
void G_ClientPrintf(const Player& player, int printlevel, const char* fmt, ...) __attribute__((format(__printf__, 3, 4)));
void G_GiveTimeUnits(int team);
void G_AppearPerishEvent(playermask_t player_mask, bool appear, Edict& check, const Edict* ent);
void G_SendInvisible(const Player& player);
int G_GetActiveTeam(void);
bool G_ActionCheckForCurrentTeam(const Player& player, Actor* ent, int TU);
bool G_ActionCheckForCurrentTeam2(const Player& player, Edict* ent, int TU);
bool G_ActionCheckForReaction(const Player& player, Actor* actor, int TU);
void G_ClientStateChange(const Player& player, Actor* actor, int reqState, bool checkaction);
bool G_ClientCanReload(Actor* actor, containerIndex_t containerID);
bool G_ClientGetWeaponFromInventory(Actor* actor);
bool G_ClientUseEdict(const Player& player, Actor* actor, Edict* door);
int G_ClientAction(Player& player);
bool G_SetTeamForPlayer(Player& player, const int team);
int G_ClientGetTeamNum(const Player& player);
int G_ClientGetTeamNumPref(const Player& player);
bool G_ClientIsReady(const Player* player);
Actor* G_ClientGetFreeSpawnPointForActorSize(const Player& player, const actorSizeEnum_t actorSize);
void G_ClientInitActorStates(const Player& player);
void G_ClientTeamInfo(const Player& player);
bool G_ClientBegin(Player& player);
void G_ClientStartMatch(Player& player);
void G_ClientUserinfoChanged(Player& player, const char* userinfo);
bool G_ClientConnect(Player* player, char* userinfo, size_t userinfoSize);  /* can't change to Player&. Conflict with SrvPlayer ! */
void G_ClientDisconnect(Player& player);
void G_ResetClientData(void);
