/**
 * @file
 * @brief Artificial Intelligence functions.
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

/**
 * @brief AiAreaSearch class, used to get an area of the map around a certain position for the AI to check possible moving positions.
 */
class AiAreaSearch {
public:
	AiAreaSearch();
	AiAreaSearch(const pos3_t origin, int radius, bool flat = false);
	~AiAreaSearch(void);
	bool getNext(pos3_t pos);
private:
	/**
	 * @brief A simple queue class.
	 */
	class LQueue {
	public:
		/**
		 * @brief Initialize a LQueue object.
		 */
		LQueue(void) : _head(nullptr), _tail(nullptr), _count(0) { }
		~LQueue(void);
		void enqueue(const pos3_t data);
		bool dequeue(pos3_t data);
		/**
		 * @brief Gets the number of elements in the queue.
		 */
		int size(void) const { return _count; }
		/**
		 * @brief Checks if the queue is empty.
		 */
		bool isEmpty(void) const { return size() < 1; }
		void clear(void);
	private:
		struct qnode_s {
			pos3_t data;
			qnode_s* next;
		};
		qnode_s* _head;		/**< Start of the queue. */
		qnode_s* _tail;		/**< End of the queue. */
		int _count;			/**< Number of elements in the queue */
	};
	LQueue _area;			/**< Queue containing the positions in the search area */
	void plotArea(const pos3_t origin, int radius, bool flat = false);
	void plotCircle(const pos3_t origin, int radius);
	void plotPos(const pos3_t origin, int xOfs, int yOfs);
};

/*
 * AI functions
 */
void AI_Init(void);
void AI_CheckRespawn(int team);
extern Edict* ai_waypointList;
void G_AddToWayPointList(Edict* ent);
void AI_Run(void);
void AI_ActorRun(Player& player, Actor* actor);
Player* AI_CreatePlayer(int team);
bool AI_CheckUsingDoor(const Edict* ent, const Edict* door);

/*
 * Shared functions (between C AI and LUA AI)
 */
bool AI_HasLineOfFire (const Actor* actor, const Edict* target);
void AI_TurnIntoDirection(Actor* actor, const pos3_t pos);
bool AI_FindHidingLocation(int team, Actor* actor, const pos3_t from, int tuLeft);
bool AI_FindHerdLocation(Actor* actor, const pos3_t from, const vec3_t target, int tu, bool inverse);
int AI_GetHidingTeam(const Edict* ent);
const Item* AI_GetItemForShootType(shoot_types_t shootType, const Edict* ent);
bool AI_FighterCheckShoot(const Actor* actor, const Edict* check, const fireDef_t* fd, float* dist);
bool AI_CheckLineOfFire(const Actor* shooter, const Edict* target, const fireDef_t* fd, int shots);
float AI_CalcShotDamage(Actor* actor, Actor* target, const fireDef_t* fd, shoot_types_t shotType);
bool AI_TryToReloadWeapon(Actor* actor, containerIndex_t containerID);
bool AI_IsHostile(const Actor* actor, const Edict* target);
const invDef_t* AI_SearchGrenade(const Actor* actor, Item** ip);
bool AI_FindMissionLocation(Actor* actor, const pos3_t to, int tus);
bool AI_CheckPosition(const Actor* const ent, const pos3_t pos);
bool AI_HideNeeded(const Actor* actor);

/*
 * LUA functions
 */
bool AIL_TeamThink(Player& player);
void AIL_ActorThink(Player& player, Actor* actor);
int AIL_InitActor(Actor* actor);
void AIL_Cleanup(void);
void AIL_Init(void);
void AIL_Shutdown(void);
