/**
 * @file
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

#include "../renderer/r_entity.h"

/** @brief Actor actions */
typedef enum {
	M_MOVE,		/**< We are currently in move-mode (destination selection). */
	M_FIRE_R,	/**< We are currently in fire-mode for the right weapon (target selection). */
	M_FIRE_L,	/**< We are currently in fire-mode for the left weapon (target selection). */
	M_FIRE_HEADGEAR, /**< We'll fire headgear item shortly. */
	M_PEND_MOVE,	/**< A move target has been selected, we are waiting for player-confirmation. */
	M_PEND_FIRE_R,	/**< A fire target (right weapon) has been selected, we are waiting for player-confirmation. */
	M_PEND_FIRE_L	/**< A fire target (left weapon) has been selected, we are waiting for player-confirmation. */
} actorModes_t;

#define IS_MODE_FIRE_RIGHT(x)		((x) == M_FIRE_R || (x) == M_PEND_FIRE_R)
#define IS_MODE_FIRE_LEFT(x)		((x) == M_FIRE_L || (x) == M_PEND_FIRE_L)
#define IS_MODE_FIRE_HEADGEAR(x)	((x) == M_FIRE_HEADGEAR)
#define IS_MODE_FIRE_PENDING(x)		((x) == M_PEND_FIRE_L || (x) == M_PEND_FIRE_R)

typedef bool (*localEntitiyAddFunc_t) (struct le_s*  le, entity_t* ent);
typedef void (*localEntityThinkFunc_t) (struct le_s*  le);

#define LE_CHECK_LEVELFLAGS		0x0001
#define LE_ALWAYS_VISIBLE		0x0002
#define LE_LOCKED				0x0004	/**< if there is an event going on involving
										 * this le_t.  Used to limit to one event per le_t struct at any time. */
#define LE_REMOVE_NEXT_FRAME	0x0008	/**< will set the inuse value to false in the next frame */
#define LE_INVISIBLE			0x0010
#define LE_SELECTED				0x0020	/**< used for actors - for the current selected actor this is true */

typedef struct leStep_s {
	int steps;
	int lastMoveTime;
	int lastMoveDuration;
	int stepTimes[MAX_ROUTE]; 	/**< the time each steps needs */
	struct leStep_s* next;
} leStep_t;

/** @brief a local entity */
typedef struct le_s {
	bool inuse;
	entity_type_t type;			/**< the local entity type */
	int entnum;					/**< the server side edict num this le belongs to */

	vec3_t origin, oldOrigin;	/**< position given via world coordinates */
	pos3_t pos, oldPos, newPos;	/**< position on the grid */
	int angle;					/**< the current dir the le is facing into. Beware, this can either
								* be an index in the bytedirs array or an index for the angles
								* array of the le */
	int dir;

	int TU, maxTU;				/**< time units */
	int morale, maxMorale;		/**< morale value - used for soldier panic and the like */
	int HP, maxHP;				/**< health points */
	int STUN;					/**< if stunned - state STATE_STUN */
	int state;					/**< rf states, dead, crouched and so on */
	woundInfo_t wounds;

	float angles[3];	/**< rotation angle, for actors only the YAW is used */
	float alpha;		/**< alpha color value for rendering */

	int team;		/**< the team number this local entity belongs to */
	int pnum;		/**< the player number this local entity belongs to */
	int ucn;		/**< the unique character number to match between server and client characters */

	int flags;

	fireDefIndex_t currentSelectedFiremode;

	actorModes_t actorMode;		/**< current selected action for this actor */
	/** for double-click movement and confirmations ... */
	pos3_t mousePendPos;
	/**
	 * @brief The TUs that the current selected actor needs to walk to the
	 * current grid position marked by the mouse cursor (mousePos)
	 * @sa CL_MoveLength
	 */
	byte actorMoveLength;

	struct le_s* clientAction;	/**< entity from server that is currently triggered and wait for client action */

	int contents;				/**< content flags for this LE - used for tracing */
	AABB aabb;					/**< the bounding box */
	vec3_t size;

	char inlineModelName[8];	/**< for bmodels */
	unsigned int modelnum1;		/**< the number of the body model in the cl.model_draw array */
	unsigned int modelnum2;		/**< the number of the head model in the cl.model_draw array */
	unsigned int bodySkin;		/**< the skin number of the body */
	unsigned int headSkin;		/**< the skin number of the head */
	model_t* model1, *model2;	/**< pointers to the cl.model_draw array
								* that holds the models for body and head - model1 is body,
								* model2 is head */

	/** is called every frame */
	localEntityThinkFunc_t think;
	/** number of milliseconds to skip the think function for */
	int thinkDelay;

	/** various think function vars */
	dvec_t dvtab[MAX_ROUTE];
	int pathContents[MAX_ROUTE];	/**< content flags of the brushes the actor is walking in */
	int positionContents;			/**< content flags for the current brush the actor is standing in */
	int pathLength, pathPos;
	leStep_t* stepList;				/**< list of steps - each new step is appended to this list (fifo) */
	int stepIndex;					/**< marks the current step in the @c stepList that should be used to get the
									 * event time for following step based events */
	int startTime, endTime;
	int speed[MAX_ROUTE];			/**< the speed the le is moving with */
	float rotationSpeed;
	int slidingSpeed;

	inline bool isMoving () const
	{
		return pathLength > 0;
	}

	/** sound effects */
	int sampleIdx;
	float attenuation;		/**< attenuation value for local entity sounds */
	float volume;			/**< loop sound volume - 0.0f-1.0f */

	/** gfx */
	animState_t as;			/**< holds things like the current active frame and so on */
	const char* particleID;
	int levelflags;			/**< the levels this local entity should be visible at */
	ptl_t* ptl;				/**< particle pointer to display */
	const char* ref1, *ref2;
	const struct le_s* ref3;
	Inventory inv;
	int left, right, headgear;	/**< item indices that the actor holds in his hands */
	actorSizeEnum_t fieldSize;	/**< ACTOR_SIZE_* */

	lighting_t lighting;

	teamDef_t* teamDef;
	int gender;				/**< @sa @c nametypes_t */
	const fireDef_t* fd;	/**< in case this is a projectile or an actor */

	/** is called before adding a le to scene */
	localEntitiyAddFunc_t addFunc;

	inline Item* getRightHandItem () const
	{
		return inv.getRightHandContainer();
	}
	inline Item* getLeftHandItem () const
	{
		return inv.getLeftHandContainer();
	}
	inline Item* getHandItem (actorHands_t hand) const
	{
		if (hand == ACTOR_HAND_RIGHT)
			return inv.getRightHandContainer();
		else if (hand == ACTOR_HAND_LEFT)
			return inv.getLeftHandContainer();
		return nullptr;
	}
	inline Item* getFloorContainer () const
	{
		return inv.getFloorContainer();
	}
	inline void setFloorContainer (Item* il)
	{
		inv.setFloorContainer(il);
	}
	inline void setFloor (le_s* other)
	{
		inv.setFloorContainer(other->getFloorContainer());
	}
	inline void resetFloor ()
	{
		inv.setFloorContainer(nullptr);
	}
} le_t;

#define MAX_LOCALMODELS		1024

/** @brief local models */
typedef struct localModel_s {
	char id[MAX_VAR];				/**< in case this local model is referenced by some other local
									 * model (e.g. for tags) - this id is set in the mapeditor */
	char name[MAX_QPATH];			/**< the name of the model file */
	char target[MAX_VAR];
	char tagname[MAX_VAR];			/**< in case a tag should be used to place the model */
	char animname[MAX_QPATH];		/**< is this an animated model */

	struct localModel_s* parent;	/**< in case a tag should be used to place the model a parent local model id must be given */
	bool inuse;

	vec3_t origin;
	vec3_t angles;
	vec3_t scale;			/**< default is 1.0 - no scaling */

	int entnum;				/**< entnum from the entity string (if available in the server, they match) */
	int renderEntityNum;	/**< entity number in the renderer entity array */
	int skin;
	int renderFlags;		/**< effect flags */
	int frame;				/**< which static frame to show (this can't be used if animname is set) */
	int levelflags;
	animState_t as;
	lighting_t lighting;

	/** is called every frame */
	void (*think) (struct localModel_s*  localModel);

	model_t* model;

	inline void setScale(const vec3_t scale_) {
		VectorCopy(scale_, scale);
	}
} localModel_t;

static const vec3_t player_mins = { -PLAYER_WIDTH, -PLAYER_WIDTH, PLAYER_MIN };
static const vec3_t player_maxs = { PLAYER_WIDTH, PLAYER_WIDTH, PLAYER_STAND };
static const vec3_t player_dead_maxs = { PLAYER_WIDTH, PLAYER_WIDTH, PLAYER_DEAD };

extern cvar_t* cl_le_debug;
extern cvar_t* cl_trace_debug;
extern cvar_t* cl_leshowinvis;
extern cvar_t* cl_map_draw_rescue_zone;

const char* LE_GetAnim(const char* anim, int right, int left, int state);
void LE_AddProjectile(const fireDef_t* fd, int flags, const vec3_t muzzle, const vec3_t impact, int normal, le_t* leVictim);
void LE_AddGrenade(const fireDef_t* fd, int flags, const vec3_t muzzle, const vec3_t v0, int dt, le_t* leVictim);
void LE_AddAmbientSound(const char* sound, const vec3_t origin, int levelflags, float volume, float attenuation);
int LE_ActorGetStepTime(const le_t* le, const pos3_t pos, const pos3_t oldPos, const int dir, const int sped);

#define LE_IsStunned(le)	(((le)->state & STATE_STUN) & ~STATE_DEAD)
/** @note This check also includes the IsStunned check - see the STATE_* bitmasks */
#define LE_IsDead(le)		((le)->state & STATE_DEAD)
#define LE_IsPanicked(le)	((le)->state & STATE_PANIC)
#define LE_IsCrouched(le)	((le)->state & STATE_CROUCHED)

#define LE_IsInvisible(le)	((le)->flags & LE_INVISIBLE)
#define LE_IsSelected(le)	((le)->flags & LE_SELECTED)

#define LE_SetInvisible(le) do { if (cl_leshowinvis->integer) le->flags &= ~LE_INVISIBLE; else le->flags |= LE_INVISIBLE; } while (0)

#define LE_IsItem(le)		((le)->type == ET_ITEM)
#define LE_IsCamera(le)		((le)->type == ET_CAMERA)
#define LE_IsCivilian(le)	((le)->team == TEAM_CIVILIAN)
#define LE_IsAlien(le)		((le)->team == TEAM_ALIEN)
#define LE_IsPhalanx(le)	((le)->team == TEAM_PHALANX)
#define LE_IsRotating(le)	((le)->type == ET_ROTATING)
#define LE_IsDoor(le)		((le)->type == ET_DOOR || (le)->type == ET_DOOR_SLIDING)
#define LE_IsBreakable(le)	((le)->type == ET_BREAKABLE)
#define LE_IsBrushModel(le)	(LE_IsBreakable(le) || LE_IsDoor(le) || LE_IsRotating(le))
#define LE_IsNotSolid(le)	((le)->type == ET_TRIGGER_RESCUE || (le)->type == ET_TRIGGER_NEXTMAP)

/** @brief Valid indices from 1 - MAX_DEATH */
#define LE_GetAnimationIndexForDeath(le)	((le)->state & MAX_DEATH)

#ifdef DEBUG
void LE_List_f(void);
void LM_List_f(void);
#endif

void LE_SetThink(le_t* le, localEntityThinkFunc_t think);
void LE_ExecuteThink(le_t* le);
void LE_Think(void);
/* think functions */
void LET_StartIdle(le_t* le);
void LET_Appear(le_t* le);
void LET_StartPathMove(le_t* le);
void LET_HiddenMove(le_t* le);
void LET_BrushModel(le_t* le);
void LE_DoEndPathMove(le_t* le);

/* local model functions */
void LM_Think(void);
void LMT_Init(localModel_t* localModel);
localModel_t* LM_AddModel(const char* model, const vec3_t origin, const vec3_t angles, int entnum, int levelflags, int flags, const vec3_t scale);
void LM_Perish(dbuffer* msg);
void LM_AddToScene(void);

bool LE_BrushModelAction(le_t* le, entity_t* ent);
void CL_RecalcRouting(const le_t* le);
void CL_CompleteRecalcRouting(void);

void LE_LinkFloorContainer(le_t* le);
bool LE_IsLivingAndVisibleActor(const le_t* le);
bool LE_IsLivingActor(const le_t* le);
bool LE_IsActor(const le_t* le);
le_t* LE_Add(int entnum);
le_t* LE_Get(int entnum);
le_t* LE_GetNextInUse(le_t* lastLE);
le_t* LE_GetNext(le_t* lastLE);
void LE_Lock(le_t* le);
void LE_Unlock(le_t* le);
bool LE_IsLocked(int entnum);
#define LE_NotFoundError(entnum) _LE_NotFoundError(entnum, -1, __FILE__, __LINE__)
#define LE_NotFoundWithTypeError(entnum, type) _LE_NotFoundError(entnum, type, __FILE__, __LINE__)
void _LE_NotFoundError(int entnum, int type, const char* file, const int line) __attribute__((noreturn));
le_t* LE_Find(entity_type_t type, const pos3_t pos);
le_t* LE_FindRadius(le_t* from, const vec3_t org, float rad, entity_type_t type);
le_t* LE_GetFromPos(const pos3_t pos);
void LE_PlaceItem(le_t* le);
void LE_Cleanup(void);
void LE_AddToScene(void);
void LE_CenterView(const le_t* le);
const cBspModel_t* LE_GetClipModel(const le_t* le);
model_t* LE_GetDrawModel(unsigned int modelIndex);
void LET_SlideDoor(le_t* le, int speed);
void LET_RotateDoor(le_t* le, int speed);

trace_t CL_Trace(const Line& traceLine, const AABB& box, const le_t* passle, le_t* passle2, int contentmask, int worldLevel);

void LM_Register(void);
localModel_t* LM_GetByID(const char* id);
