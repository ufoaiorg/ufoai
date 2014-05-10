/**
 * @file
 * @brief Local definitions for game module
 */

/*
All original material Copyright (C) 2002-2014 UFO: Alien Invasion.

Original file from Quake 2 v3.21: quake2-2.31/game/g_local.h
Copyright (C) 1997-2001 Id Software, Inc.

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

#include "../shared/ufotypes.h"

/**
 * @brief Everything that is not in the bsp tree is an edict, the spawnpoints,
 * the actors, the misc_models, the weapons and so on.
 */
#define INH 0
/* This is an attempt to inherit Edict from the part known to the server.
 * This fails because the server knows about parent/child relations between Edicts. */
#if INH
#include "srvedict.h"
class Edict : public SrvEdict {
public:
#else
class Edict {
public:
	bool inuse;
	int linkcount;		/**< count the amount of server side links - if a link was called,
						 * something on the position or the size of the entity was changed */

	int number;			/**< the number in the global edict array */

	vec3_t origin;		/**< the position in the world */
	vec3_t angles;		/**< the rotation in the world (pitch, yaw, roll) */
	pos3_t pos;			/**< the position on the grid @sa @c UNIT_SIZE */

	/** tracing info SOLID_BSP, SOLID_BBOX, ... */
	solid_t solid;

	AABB entBox;		/**< position of min and max points - relative to origin */
	AABB absBox;		/**< position of min and max points - relative to world's origin */
	vec3_t size;

	Edict* _child;		/**< e.g. the trigger for this edict */
	Edict* _owner;		/**< e.g. the door model in case of func_door */
	int modelindex;		/**< inline model index */
	const char* classname;
#endif
	/*================================ */
	/* don't change anything above here - the server expects the fields in that order */
	/*================================ */

	int mapNum;					/**< entity number in the map file */
	const char* model;			/**< inline model (start with * and is followed by the model number)
								 * or misc_model model name (e.g. md2) */

	/** only used locally in game, not by server */

	Edict* particleLink;
	const Edict* link;			/**< can be used to store another edict that e.g. interacts with the current one */
	entity_type_t type;
	teammask_t visflags;		/**< bitmask of teams that can see this edict */

	int contentFlags;			/**< contents flags of the brush the actor is walking in */

	byte dir;					/**< direction the player looks at */

	int TU;						/**< remaining timeunits for actors or timeunits needed to 'use' this entity */
	int HP;						/**< remaining healthpoints */
	int STUN;					/**< The stun damage received in a mission. */
	int morale;					/**< the current morale value */

	int state;					/**< the player state - dead, shaken.... */

	int team;					/**< player of which team? */
	int pnum;					/**< the actual player slot */
	/** the model indices */
	unsigned int body;
	unsigned int head;
	int frame;					/**< frame of the model to show */

	char* group;				/**< this can be used to trigger a group of entities
								 * e.g. for two-part-doors - set the group to the same
								 * string for each door part and they will open both
								 * if you open one */

	bool inRescueZone;			/**< the actor is standing in a rescue zone if this is true - this means that
								 * when the mission is aborted the actor will not die */

	/** client actions - interact with the world */
	Edict* clientAction;

	/** here are the character values */
	character_t chr;

	int spawnflags;				/**< set via mapeditor */

	float angle;				/**< entity yaw - (0-360 degree) set via mapeditor - sometimes used for movement direction,
								 * then -1=up; -2=down is used additionally */

	int radius;					/**< this is used to extend the bounding box of a trigger_touch for e.g. misc_mission */
	int speed;					/**< speed of entities - e.g. rotating or actors */
	const char* target;			/**< name of the entity to trigger or move towards - this name is stored in the target edicts targetname value */
	const char* targetname;		/**< name pointed to by target - see the target of the parent edict */
	const char* item;			/**< the item id that must be placed to e.g. the func_mission to activate the use function */
	const char* particle;
	const char* nextmap;
	const char* message;		/**< misc_message */
	const char* noise;			/**< sounds - e.g. for func_door */
	edictMaterial_t material;	/**< material value (e.g. for func_breakable) */
	camera_edict_data_t camera;
	int count;					/**< general purpose 'amount' variable - set via mapeditor often */
	int time;					/**< general purpose 'rounds' variable - set via mapeditor often */
	int sounds;					/**< type of sounds to play - e.g. doors */
	int dmg;					/**< damage done by entity */
	byte dmgtype;				/**< damage type done by the entity */
	/** @sa memcpy in Grid_CheckForbidden */
	actorSizeEnum_t fieldSize;	/**< ACTOR_SIZE_* */
	bool hiding;				/**< for ai actors - when they try to hide after the performed their action */

	/** function to call when triggered - this function should only return true when there is
	 * a client action associated with it */
	bool (*touch)(Edict* self, Edict* activator);
	/** reset function that is called before the touch triggers are called */
	void (*reset)(Edict* self, Edict* activator);
	float nextthink;
	void (*think)(Edict* self);
	/** general use function that is called when the triggered client action is executed
	 * or when the server has to 'use' the entity
	 * @param activator Might be @c nullptr if there is no activator */
	bool (*use)(Edict* self, Edict* activator);
	bool (*destroy)(Edict* self);

	Edict* touchedNext;			/**< entity list of edict that are currently touching the trigger_touch */
	int doorState;				/**< open or closed */

	moveinfo_t		moveinfo;

	/** flags will have FL_GROUPSLAVE set when the edict is part of a chain,
	 * but not the master - you can use the groupChain pointer to get all the
	 * edicts in the particular chain - and start out for the one that doesn't
	 * have the above mentioned flag set.
	 * @sa G_FindEdictGroups */
	Edict* groupChain;
	Edict* groupMaster;			/**< first entry in the list */
	int flags;					/**< FL_* */

	AI_t AI; 					/**< The character's artificial intelligence */

	pos3_t* forbiddenListPos;	/**< this is used for e.g. misc_models with the solid flag set - this will
								 * hold a list of grid positions that are blocked by the aabb of the model */
	int forbiddenListSize;		/**< amount of entries in the forbiddenListPos */

	bool active;				/** only used by camera */


	/*==================
	 *		ctors
	 *==================*/
	inline void init (int idx) {
		OBJZERO(*this);
		number = idx;
		chr.inv.init();
	}
	/*==================
	 *		setters
	 *==================*/
	inline void setChild (Edict* child) {
		_child = child;
	}
	inline void nativeReset () {	/* strange name, but there is also a complex function named 'reset' */
		int idx = this->number;
		OBJZERO(*this);				/* zero everything, but ... */
		chr.inv.init();
		this->number = idx;			/* ... preserve the number */
	}
	inline void setActive() {
		active = true;
	}
	inline void toggleActive() {
		active ^= true;
	}
	inline void resetContainer (const containerIndex_t idx) {
		chr.inv.resetContainer(idx);
	}
	inline void setFloor (const Edict* other) {
		chr.inv.setFloorContainer(other->getFloor());
	}
	inline void resetFloor () {
		chr.inv.setFloorContainer(nullptr);
	}
/**
 * @brief Calculate the edict's origin vector from it's grid position
 */
	inline void calcOrigin () {
		gi.GridPosToVec(fieldSize, pos, origin);
	}
/**
 * @brief Set the edict's pos and origin vector to the given grid position
 * @param newPos The new grid position
 */
	inline void setOrigin (const pos3_t newPos) {
		VectorCopy(newPos, pos);
		calcOrigin();
	}


	/*==================
	 *		getters
	 *==================*/
	inline int getIdNum() const {
		return number;
	}
	inline Edict* child () {
		return _child;
	}
	inline Edict* owner () {
		return _owner;
	}
	inline Item* getContainer (const containerIndex_t idx) const {
		return chr.inv.getContainer2(idx);
	}
	inline Item* getArmour () const {
		return chr.inv.getArmour();
	}
	inline Item* getRightHandItem () const {
		return chr.inv.getRightHandContainer();
	}
	inline Item* getLeftHandItem () const {
		return chr.inv.getLeftHandContainer();
	}
	inline Item* getHandItem (actorHands_t hand) const {
		if (hand == ACTOR_HAND_RIGHT)
			return chr.inv.getRightHandContainer();
		else if (hand == ACTOR_HAND_LEFT)
			return chr.inv.getLeftHandContainer();
		else return nullptr;
	}
	inline Item* getFloor () const {
		return chr.inv.getFloorContainer();
	}
	inline Player& getPlayer () const {
		return game.players[this->pnum];
	}
	/* also used by camera ! */
	inline int getTeam() const {
		return team;
	}
	/*==================
	 *		checkers
	 *==================*/
	inline bool isSameAs (const Edict* other) const {
		return this->getIdNum() == other->getIdNum();
	}
/**
 * @brief Check whether the edict is on the given position
 * @param cmpPos The grid position to compare to
 * @return true if positions are equal
 */
	inline bool isSamePosAs (const pos3_t cmpPos) {
		return VectorCompare(cmpPos, this->pos);
	}

	/*==================
	 *	manipulators
	 *==================*/

	/*
	 *	A set of unsorted functions that are to be moved to class Actor later
	 */
	inline void setMorale(int mor) {
		morale = mor;
	}
	/* only Actors should have TUs */
	inline void setTus(int tus) {
		TU = tus;
	}
	inline int getTus() const {
		return TU;
	}
};

/**
 * @brief An Edict of type Actor
 * @note Let's try to split the 'god-class' Edict into smaller chunks. Class Actor is intended to receive
 * members and functions from Edict that are only used with actors.
 * There will be other classes (eg. Trigger, Field, Item,...) in the future.
 */
class Actor : public Edict {
public:
	inline int getMorale() const {
		return morale;
	}
};

Actor* makeActor (Edict* ent);Actor* makeActor (Edict* ent);
