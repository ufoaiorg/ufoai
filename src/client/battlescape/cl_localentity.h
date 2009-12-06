/**
 * @file cl_localentity.h
 */

/*
Copyright (C) 2002-2009 UFO: Alien Invasion.

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

#ifndef CLIENT_CL_LOCALENTITY_H
#define CLIENT_CL_LOCALENTITY_H

#include "../renderer/r_entity.h"

#define MAX_LE_PATHLENGTH 32

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

#define IS_MODE_FIRE_RIGHT(x)	((x) == M_FIRE_R || (x) == M_PEND_FIRE_R)
#define IS_MODE_FIRE_LEFT(x)		((x) == M_FIRE_L || (x) == M_PEND_FIRE_L)
#define IS_MODE_FIRE_HEADGEAR(x)		((x) == M_FIRE_HEADGEAR)

/** @brief a local entity */
typedef struct le_s {
	qboolean inuse;
	qboolean removeNextFrame;	/**< will set the inuse value to false in the next frame */
	qboolean invis;
	qboolean selected;
	int type;				/**< the local entity type */
	int entnum;				/**< the server side edict num this le belongs to */

	vec3_t origin, oldOrigin;	/**< position given via world coordinates */
	pos3_t pos, oldPos, newPos;		/**< position on the grid */
	int dir;				/**< the current dir the le is facing into */

	int TU, maxTU;				/**< time units */
	int morale, maxMorale;		/**< morale value - used for soldier panic and the like */
	int HP, maxHP;				/**< health points */
	int STUN;					/**< if stunned - state STATE_STUN */
	int state;					/**< rf states, dead, crouched and so on */
	int oldstate;

	float angles[3];
	float alpha;

	int team;		/**< the team number this local entity belongs to */
	int pnum;		/**< the player number this local entity belongs to */

	int currentSelectedFiremode;

	actorModes_t actorMode;		/**< current selected action for the selected actor */
	/** for double-click movement and confirmations ... */
	pos3_t mousePendPos;
	/**
	 * @brief The TUs that the current selected actor needs to walk to the
	 * current grid position marked by the mouse cursor (mousePos)
	 * @sa CL_MoveLength
	 */
	byte actorMoveLength;

	int clientAction;		/**< entnum from server that is currently triggered */

	int contents;			/**< content flags for this LE - used for tracing */
	vec3_t mins, maxs;

	char inlineModelName[8];
	int modelnum1;	/**< the number of the body model in the cl.model_draw array */
	int modelnum2;	/**< the number of the head model in the cl.model_draw array */
	int skinnum;	/**< the skin number of the body and head model */
	model_t *model1, *model2;	/**< pointers to the cl.model_draw array
					 * that holds the models for body and head - model1 is body,
					 * model2 is head */

	/** is called every frame */
	void (*think) (struct le_s * le);
	/** number of frames to skip the think function for */
	int thinkDelay;

	/** various think function vars */
	byte path[MAX_LE_PATHLENGTH];
	int pathContents[MAX_LE_PATHLENGTH];	/**< content flags of the brushes the actor is walking in */
	int positionContents;					/**< content flags for the current brush the actor is standing in */
	int pathLength, pathPos;
	int startTime, endTime;
	int speed[MAX_LE_PATHLENGTH];			/**< the speed the le is moving with */
	float rotationSpeed;

	/** sound effects */
	struct s_sample_s* sample;
	float volume;			/**< loop sound volume - 0.0f-1.0f */

	/** gfx */
	animState_t as;	/**< holds things like the current active frame and so on */
	const char *particleID;
	int levelflags;	/**< the levels this particle should be visible at */
	ptl_t *ptl;				/**< particle pointer to display */
	const char *ref1, *ref2;
	inventory_t i;
	int left, right, extension, headgear;
	int fieldSize;				/**< ACTOR_SIZE_* */
	teamDef_t* teamDef;
	int gender;
	const fireDef_t *fd;	/**< in case this is a projectile */

	pathing_t *pathMap;	/**< This is where the data for TUS used to move and actor
								 * locations go - only available for human controlled actors */
	static_lighting_t lighting;

	/** is called before adding a le to scene */
	qboolean(*addFunc) (struct le_s * le, entity_t * ent);

	qboolean locked;	/**< true if there is an event going on involving
						 * this le_t.  Used to limit to one event per le_t struct at any time. */
} le_t;

#define MAX_LOCALMODELS		512

/** @brief local models */
typedef struct lm_s {
	char name[MAX_VAR];
	char particle[MAX_VAR];
	qboolean inuse;

	vec3_t origin;
	vec3_t angles;
	vec3_t scale;	/**< default is 1.0 - no scaling */

	int entnum;
	int skin;
	int renderFlags;	/**< effect flags */
	int frame;	/**< which frame to show */
	char animname[MAX_QPATH];	/**< is this an animated model */
	int levelflags;
	animState_t as;

	static_lighting_t lighting;

	struct model_s *model;
} localModel_t;							/* local models */

extern localModel_t LMs[MAX_LOCALMODELS];

extern le_t LEs[MAX_EDICTS];
extern const char *leInlineModelList[MAX_EDICTS + 1];

static const vec3_t player_mins = { -PLAYER_WIDTH, -PLAYER_WIDTH, PLAYER_MIN };
static const vec3_t player_maxs = { PLAYER_WIDTH, PLAYER_WIDTH, PLAYER_STAND };
static const vec3_t player_dead_maxs = { PLAYER_WIDTH, PLAYER_WIDTH, PLAYER_DEAD };

qboolean CL_OutsideMap(const vec3_t impact, const float delta);
const char *LE_GetAnim(const char *anim, int right, int left, int state);
void LE_AddProjectile(const fireDef_t *fd, int flags, const vec3_t muzzle, const vec3_t impact, int normal);
void LE_AddGrenade(const fireDef_t *fd, int flags, const vec3_t muzzle, const vec3_t v0, int dt);
void LE_AddAmbientSound(const char *sound, const vec3_t origin, int levelflags, float volume);
le_t *LE_GetClosestActor(const vec3_t origin);
int LE_ActorGetStepTime(const le_t *le, const pos3_t pos, const pos3_t oldPos, const int dir, const int sped);

#define LE_IsStunned(le)	(((le)->state & STATE_STUN) & ~STATE_DEAD)
/** @note This check also includes the IsStunned check - see the STATE_* bitmasks */
#define LE_IsDead(le)		(((le)->state & STATE_DEAD))

/** @brief Valid indices from 1 - MAX_DEATH */
#define LE_GetAnimationIndexForDeath(le)	((le)->state & MAX_DEATH)

void LE_SetThink(le_t *le, void (*think) (le_t *le));
void LE_ExecuteThink(le_t *le);
void LE_Think(void);
/* think functions */
void LET_StartIdle(le_t *le);
void LET_Appear(le_t *le);
void LET_StartPathMove(le_t *le);
void LET_BrushModel(le_t *le);
void LE_DoEndPathMove(le_t *le);

/* local model functions */
localModel_t *LM_AddModel(const char *model, const char *particle, const vec3_t origin, const vec3_t angles, int entnum, int levelflags, int flags, const vec3_t scale);
void LM_Perish(struct dbuffer *msg);
void LM_AddToScene(void);

qboolean LE_BrushModelAction(le_t *le, entity_t *ent);
void CL_RecalcRouting(const le_t *le);
void CL_CompleteRecalcRouting(void);

qboolean LE_IsLivingAndVisibleActor(const le_t *le);
qboolean LE_IsLivingActor(const le_t *le);
qboolean LE_IsActor(const le_t *le);
le_t *LE_Add(int entnum);
le_t *LE_Get(int entnum);
void LE_Lock(le_t *le);
void LE_Unlock(le_t *le);
qboolean LE_IsLocked(int entnum);
#define LE_NotFoundError(entnum) _LE_NotFoundError(entnum, __FILE__, __LINE__)
void _LE_NotFoundError(int entnum, const char *file, const int line) __attribute__((noreturn));
le_t *LE_Find(int type, pos3_t pos);
void LE_PlaceItem(le_t *le);
void LE_Cleanup(void);
void LE_AddToScene(void);
void LE_CenterView(const le_t *le);

trace_t CL_Trace(vec3_t start, vec3_t end, const vec3_t mins, const vec3_t maxs, le_t * passle, le_t * passle2, int contentmask);

void LM_Register(void);

#endif
