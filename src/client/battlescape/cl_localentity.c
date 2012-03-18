/**
 * @file cl_localentity.c
 * @brief Local entity management.
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

#include "../client.h"
#include "cl_localentity.h"
#include "../sound/s_main.h"
#include "cl_particle.h"
#include "cl_actor.h"
#include "cl_hud.h"
#include "../renderer/r_mesh_anim.h"
#include "../renderer/r_draw.h"
#include "../../common/tracing.h"
#include "../../common/grid.h"

cvar_t *cl_le_debug;
cvar_t *cl_trace_debug;
cvar_t* cl_map_draw_rescue_zone;

/*===========================================================================
Local Model (LM) handling
=========================================================================== */

static inline void LE_GenerateInlineModelList (void)
{
	le_t *le = NULL;
	int i = 0;

	while ((le = LE_GetNextInUse(le))) {
		if (le->model1 && le->inlineModelName[0] == '*')
			cl.leInlineModelList[i++] = le->inlineModelName;
	}
	cl.leInlineModelList[i] = NULL;
}

static void CL_GridRecalcRouting (const le_t *le)
{
	/* We ALWAYS check against a model, even if it isn't in use.
	 * An unused model is NOT included in the inline list, so it doesn't get
	 * traced against. */
	if (!le->model1 || le->inlineModelName[0] != '*')
		return;

	if (Com_ServerState())
		return;

	Com_DPrintf(DEBUG_ROUTING, "Rerouting le %i client side\n", le->entnum);

	Grid_RecalcRouting(cl.mapTiles, cl.mapData->map, le->inlineModelName, cl.leInlineModelList);
}

/**
 * @sa G_CompleteRecalcRouting
 */
void CL_CompleteRecalcRouting (void)
{
	le_t* le;
	int i;

	LE_GenerateInlineModelList();

	Com_DPrintf(DEBUG_ROUTING, "Rerouting everything client side\n");

	for (i = 0, le = cl.LEs; i < cl.numLEs; i++, le++)
		CL_GridRecalcRouting(le);
}

/**
 * @sa CL_Explode
 * @param[in] le A local entity of a brush model (func_breakable, func_door, ...)
 */
void CL_RecalcRouting (const le_t* le)
{
	LE_GenerateInlineModelList();

	CL_GridRecalcRouting(le);

	CL_ActorConditionalMoveCalc(selActor);
}

static void LM_AddToSceneOrder (qboolean parents)
{
	localModel_t *lm;
	entity_t ent;
	int i;

	for (i = 0, lm = cl.LMs; i < cl.numLMs; i++, lm++) {
		if (!lm->inuse)
			continue;

		/* check for visibility */
		if (!((1 << cl_worldlevel->integer) & lm->levelflags))
			continue;

		/* if we want to render the parents and this is a child (has a parent assigned)
		 * then skip it */
		if (parents && lm->parent)
			continue;

		/* if we want to render the children and this is a parent (no further parent
		 * assigned), then skip it. */
		if (!parents && lm->parent == NULL)
			continue;

		/* set entity values */
		OBJZERO(ent);
		assert(lm->model);
		ent.model = lm->model;
		ent.skinnum = lm->skin;
		ent.lighting = &lm->lighting;
		VectorCopy(lm->scale, ent.scale);

		if (lm->parent) {
			/** @todo what if the tagent is not rendered due to different level flags? */
			ent.tagent = R_GetEntity(lm->parent->renderEntityNum);
			if (ent.tagent == NULL)
				Com_Error(ERR_DROP, "Invalid entity num for local model (%s/%s): %i",
						lm->model->name, lm->id, lm->parent->renderEntityNum);
			ent.tagname = lm->tagname;
		} else {
			R_EntitySetOrigin(&ent, lm->origin);
			VectorCopy(lm->origin, ent.oldorigin);
			VectorCopy(lm->angles, ent.angles);

			if (lm->animname[0] != '\0') {
				ent.as = lm->as;
				/* do animation */
				R_AnimRun(&lm->as, ent.model, cls.frametime * 1000);
			} else {
				ent.as.frame = lm->frame;
			}
		}

		/* renderflags like RF_PULSE */
		ent.flags = lm->renderFlags;

		/* add it to the scene */
		lm->renderEntityNum = R_AddEntity(&ent);
	}
}

/**
 * @brief Add the local models to the scene
 * @sa CL_ViewRender
 * @sa LE_AddToScene
 * @sa LM_AddModel
 */
void LM_AddToScene (void)
{
	LM_AddToSceneOrder(qtrue);
	LM_AddToSceneOrder(qfalse);
}

/**
 * @brief Checks whether a local model with the same entity number is already registered
 */
static inline localModel_t *LM_Find (int entnum)
{
	int i;

	for (i = 0; i < cl.numLMs; i++)
		if (cl.LMs[i].entnum == entnum)
			return &cl.LMs[i];

	return NULL;
}

/**
 * @brief link any floor container into the actor temp floor container
 */
void LE_LinkFloorContainer (le_t *le)
{
	le_t *floor = LE_Find(ET_ITEM, le->pos);
	if (floor)
		FLOOR(le) = FLOOR(floor);
	else
		FLOOR(le) = NULL;
}

/**
 * @brief Checks whether the given le is a living actor
 * @param[in] le The local entity to perform the check for
 * @sa G_IsLivingActor
 */
qboolean LE_IsActor (const le_t *le)
{
	assert(le);
	return le->type == ET_ACTOR || le->type == ET_ACTOR2x2 || le->type == ET_ACTORHIDDEN;
}

/**
 * @brief Checks whether the given le is a living actor (but might be hidden)
 * @param[in] le The local entity to perform the check for
 * @sa G_IsLivingActor
 * @sa LE_IsActor
 */
qboolean LE_IsLivingActor (const le_t *le)
{
	assert(le);
	return LE_IsActor(le) && (LE_IsStunned(le) || !LE_IsDead(le));
}

/**
 * @brief Checks whether the given le is a living and visible actor
 * @param[in] le The local entity to perform the check for
 * @sa G_IsLivingActor
 * @sa LE_IsActor
 */
qboolean LE_IsLivingAndVisibleActor (const le_t *le)
{
	assert(le);
	if (le->invis)
		return qfalse;

	assert(le->type != ET_ACTORHIDDEN);

	return LE_IsLivingActor(le);
}

/**
 * @brief Register misc_models
 * @sa CL_ViewLoadMedia
 */
void LM_Register (void)
{
	localModel_t *lm;
	int i;

	for (i = 0, lm = cl.LMs; i < cl.numLMs; i++, lm++) {
		/* register the model */
		lm->model = R_FindModel(lm->name);
		if (lm->animname[0]) {
			R_AnimChange(&lm->as, lm->model, lm->animname);
			if (!lm->as.change)
				Com_Printf("LM_Register: Could not change anim of %s to '%s'\n",
						lm->name, lm->animname);
		}
		if (!lm->model)
			lm->inuse = qfalse;
	}
}

void LE_SetThink (le_t *le, localEntityThinkFunc_t think)
{
	le->think = think;
}

localModel_t *LM_GetByID (const char *id)
{
	int i;

	if (Q_strnull(id))
		return NULL;

	for (i = 0; i < cl.numLMs; i++) {
		localModel_t *lm = &cl.LMs[i];
		if (Q_streq(lm->id, id))
			return lm;
	}
	return NULL;
}

/**
 * @brief Prepares local (not known or handled by the server) models to the map, which will be added later in LM_AddToScene().
 * @param[in] model The model name.
 * @param[in] origin Origin of the model (position on map).
 * @param[in] angles Angles of the model (how it should be rotated after adding to map).
 * @param[in] scale Scaling of the model (how it should be scaled after adding to map).
 * @param[in] entnum Entity number.
 * @param[in] levelflags The levels in which the entity resides/is visible.
 * @param[in] renderFlags The flags for the renderer, eg. 'translucent'.
 * @note misc_model
 * @sa CL_SpawnParseEntitystring
 * @sa LM_AddToScene
 */
localModel_t *LM_AddModel (const char *model, const vec3_t origin, const vec3_t angles, int entnum, int levelflags, int renderFlags, const vec3_t scale)
{
	localModel_t *lm;

	lm = &cl.LMs[cl.numLMs++];

	if (cl.numLMs >= MAX_LOCALMODELS)
		Com_Error(ERR_DROP, "Too many local models\n");

	OBJZERO(*lm);
	Q_strncpyz(lm->name, model, sizeof(lm->name));
	VectorCopy(origin, lm->origin);
	VectorCopy(angles, lm->angles);
	/* check whether there is already a model with that number */
	if (LM_Find(entnum))
		Com_Error(ERR_DROP, "Already a local model with the same id (%i) loaded\n", entnum);
	lm->entnum = entnum;
	lm->levelflags = levelflags;
	lm->renderFlags = renderFlags;
	lm->inuse = qtrue;
	VectorCopy(scale, lm->scale);

	return lm;
}

/*===========================================================================
LE thinking
=========================================================================== */

/**
 * @brief Call think function for the given local entity if its still in use
 */
void LE_ExecuteThink (le_t *le)
{
	if (le->inuse && le->think) {
		le->think(le);
	}
}

/**
 * @brief Calls the le think function and updates the animation. The animation updated even if the
 * particular local entity is invisible for the client. This ensures, that an animation is always
 * lerped correctly and won't magically start again once the local entity gets visible again.
 * @sa LET_StartIdle
 * @sa LET_PathMove
 * @sa LET_StartPathMove
 * @sa LET_Projectile
 */
void LE_Think (void)
{
	le_t *le = NULL;

	if (cls.state != ca_active)
		return;

	while ((le = LE_GetNext(le))) {
		LE_ExecuteThink(le);
		/* do animation - even for invisible entities */
		R_AnimRun(&le->as, le->model1, cls.frametime * 1000);
	}
}

void LM_Think (void)
{
	int i;
	localModel_t *lm;

	for (i = 0, lm = cl.LMs; i < cl.numLMs; i++, lm++) {
		if (lm->think)
			lm->think(lm);
	}
}


/*===========================================================================
 LE think functions
=========================================================================== */

/**
 * @brief Get the correct animation for the given actor state and weapons
 * @param[in] anim Type of animation (for example "stand", "walk")
 * @param[in] right ods index to determine the weapon in the actors right hand
 * @param[in] left ods index to determine the weapon in the actors left hand
 * @param[in] state the actors state - e.g. STATE_CROUCHED (crouched animations)
 * have a 'c' in front of their animation definitions (see *.anm files for
 * characters)
 */
const char *LE_GetAnim (const char *anim, int right, int left, int state)
{
	if (!anim)
		return "";

	static char retAnim[MAX_VAR];
	char*       mod    = retAnim;
	size_t      length = sizeof(retAnim);

	/* add crouched flag */
	if (state & STATE_CROUCHED) {
		*mod++ = 'c';
		length--;
	}

	/* determine relevant data */
	char        animationIndex;
	char const* type;
	if (right == NONE) {
		animationIndex = '0';
		if (left == NONE)
			type = "item";
		else {
			type = INVSH_GetItemByIDX(left)->type;
			/* left hand grenades look OK with default anim; others don't */
			if (!Q_streq(type, "grenade"))
				goto akimbo;
		}
	} else {
		const objDef_t *od = INVSH_GetItemByIDX(right);
		animationIndex = od->animationIndex;
		type = od->type;
		if (left != NONE && Q_streq(od->type, "pistol") && Q_streq(INVSH_GetItemByIDX(left)->type, "pistol")) {
akimbo:
			type = "pistol_d";
		}
	}

	if (Q_strstart(anim, "stand") || Q_strstart(anim, "walk")) {
		Com_sprintf(mod, length, "%s%c", anim, animationIndex);
	} else {
		Com_sprintf(mod, length, "%s_%s", anim, type);
	}

	return retAnim;
}

/**
 * @brief Change the animation of an actor to the idle animation (which can be
 * panic, dead or stand)
 * @note We have more than one animation for dead - the index is given by the
 * state of the local entity
 * @note Think function
 * @note See the *.anm files in the models dir
 */
void LET_StartIdle (le_t * le)
{
	/* hidden actors don't have models assigned, thus we can not change the
	 * animation for any model */
	if (le->type != ET_ACTORHIDDEN) {
		if (LE_IsDead(le))
			R_AnimChange(&le->as, le->model1, va("dead%i", LE_GetAnimationIndexForDeath(le)));
		else if (LE_IsPaniced(le))
			R_AnimChange(&le->as, le->model1, "panic0");
		else
			R_AnimChange(&le->as, le->model1, LE_GetAnim("stand", le->right, le->left, le->state));
	}

	le->pathPos = le->pathLength = 0;

	/* keep this animation until something happens */
	LE_SetThink(le, NULL);
}

/**
 * @brief Plays sound of content for moving actor.
 * @param[in] le Pointer to local entity being an actor.
 * @param[in] contents The contents flag of the brush we are currently in
 * @note Currently it supports only CONTENTS_WATER, any other special contents
 * can be added here anytime.
 */
static void LE_PlaySoundFileForContents (le_t* le, int contents)
{
	/* only play those water sounds when an actor jumps into the water - but not
	 * if he enters carefully in crouched mode */
	if (le->state & ~STATE_CROUCHED) {
		if (contents & CONTENTS_WATER) {
			/* were we already in the water? */
			if (le->positionContents & CONTENTS_WATER) {
				/* play water moving sound */
				S_PlayStdSample(SOUND_WATER_MOVE, le->origin, SOUND_ATTN_IDLE, SND_VOLUME_FOOTSTEPS);
			} else {
				/* play water entering sound */
				S_PlayStdSample(SOUND_WATER_IN, le->origin, SOUND_ATTN_IDLE, SND_VOLUME_FOOTSTEPS);
			}
			return;
		}

		if (le->positionContents & CONTENTS_WATER) {
			/* play water leaving sound */
			S_PlayStdSample(SOUND_WATER_OUT, le->origin, SOUND_ATTN_IDLE, SND_VOLUME_FOOTSTEPS);
		}
	}
}

/**
 * @brief Plays step sounds and draw particles for different terrain types
 * @param[in] le The local entity to play the sound and draw the particle for
 * @param[in] textureName The name of the texture the actor is standing on
 * @sa LET_PathMove
 */
static void LE_PlaySoundFileAndParticleForSurface (le_t* le, const char *textureName)
{
	const terrainType_t *t;
	vec3_t origin;

	t = Com_GetTerrainType(textureName);
	if (!t)
		return;

	/* origin might not be up-to-date here - but pos should be */
	PosToVec(le->pos, origin);

	/** @todo use the Grid_Fall method (ACTOR_SIZE_NORMAL) to ensure, that the particle is
	 * drawn at the ground (if needed - maybe the origin is already ground aligned)*/
	if (t->particle) {
		/* check whether actor is visible */
		if (!LE_IsStunned(le) && LE_IsLivingAndVisibleActor(le))
			CL_ParticleSpawn(t->particle, 0, origin, NULL, NULL);
	}
	if (t->footStepSound) {
		Com_DPrintf(DEBUG_SOUND, "LE_PlaySoundFileAndParticleForSurface: volume %.2f\n", t->footStepVolume);
		S_LoadAndPlaySample(t->footStepSound, origin, SOUND_ATTN_STATIC, t->footStepVolume);
	}
}

/**
 * sqrt(2) for diagonal movement
 */
int LE_ActorGetStepTime (const le_t *le, const pos3_t pos, const pos3_t oldPos, const int dir, const int speed)
{
	if (dir != DIRECTION_FALL) {
		return (((dir & (CORE_DIRECTIONS - 1)) >= BASE_DIRECTIONS ? UNIT_SIZE * 1.41 : UNIT_SIZE) * 1000 / speed);
	} else {
		vec3_t start, dest;
		/* This needs to account for the distance of the fall. */
		Grid_PosToVec(cl.mapData->map, le->fieldSize, oldPos, start);
		Grid_PosToVec(cl.mapData->map, le->fieldSize, pos, dest);
		/* 1/1000th of a second per model unit in height change */
		return (start[2] - dest[2]);
	}
}

static void LE_PlayFootStepSound (le_t *le)
{
	/* walking in water will not play the normal footstep sounds */
	if (!le->pathContents[le->pathPos]) {
		trace_t trace;
		vec3_t from, to;

		/* prepare trace vectors */
		PosToVec(le->pos, from);
		VectorCopy(from, to);
		/* we should really hit the ground with this */
		to[2] -= UNIT_HEIGHT;

		trace = CL_Trace(from, to, vec3_origin, vec3_origin, NULL, NULL, MASK_SOLID, cl_worldlevel->integer);
		if (trace.surface)
			LE_PlaySoundFileAndParticleForSurface(le, trace.surface->name);
	} else
		LE_PlaySoundFileForContents(le, le->pathContents[le->pathPos]);
}

static void LE_DoPathMove (le_t *le)
{
	/* next part */
	const dvec_t dvec = le->dvtab[le->pathPos];
	const byte dir = getDVdir(dvec);
	const byte crouchingState = LE_IsCrouched(le) ? 1 : 0;
	/* newCrouchingState needs to be set to the current crouching state
	 * and is possibly updated by PosAddDV. */
	byte newCrouchingState = crouchingState;
	PosAddDV(le->pos, newCrouchingState, dvec);

	LE_PlayFootStepSound(le);

	/* only change the direction if the actor moves horizontally. */
	if (dir < CORE_DIRECTIONS || dir >= FLYING_DIRECTIONS)
		le->angle = dir & (CORE_DIRECTIONS - 1);
	le->angles[YAW] = directionAngles[le->angle];
	le->startTime = le->endTime;
	/* check for straight movement or diagonal movement */
	assert(le->speed[le->pathPos]);
	le->endTime += LE_ActorGetStepTime(le, le->pos, le->oldPos, dir, le->speed[le->pathPos]);

	le->positionContents = le->pathContents[le->pathPos];
	le->pathPos++;
}

/**
 * @brief Ends the move of an actor
 */
void LE_DoEndPathMove (le_t *le)
{
	/* Verify the current position */
	if (!VectorCompare(le->pos, le->newPos))
		Com_Error(ERR_DROP, "LE_DoEndPathMove: Actor movement is out of sync: %i:%i:%i should be %i:%i:%i (step %i of %i) (team %i)",
				le->pos[0], le->pos[1], le->pos[2], le->newPos[0], le->newPos[1], le->newPos[2], le->pathPos, le->pathLength, le->team);

	CL_ActorConditionalMoveCalc(le);
	/* if the moving actor was not the selected one, */
	/* recalc the pathing table for the selected one, too. */
	if (!le->selected) {
		CL_ActorConditionalMoveCalc(selActor);
	}

	LE_LinkFloorContainer(le);

	LE_SetThink(le, LET_StartIdle);
	LE_ExecuteThink(le);
	LE_Unlock(le);
}

/**
 * @brief Spawns particle effects for a hit actor.
 * @param[in] le The actor to spawn the particles for.
 * @param[in] impact The impact location (where the particles are spawned).
 * @param[in] normal The index of the normal vector of the particles (think: impact angle).
 */
static void LE_ActorBodyHit (const le_t * le, const vec3_t impact, int normal)
{
	if (le->teamDef) {
		/* Spawn "hit_particle" if defined in teamDef. */
		if (le->teamDef->hitParticle[0] != '\0')
			CL_ParticleSpawn(le->teamDef->hitParticle, 0, impact, bytedirs[normal], NULL);
	}
}

/**
 * @brief Move the actor along the path to the given location
 * @note Think function
 * @sa CL_ActorDoMove
 */
static void LET_PathMove (le_t * le)
{
	float frac;
	vec3_t start, dest, delta;

	/* check for start of the next step */
	if (cl.time < le->startTime)
		return;

	le->lighting.state = LIGHTING_DIRTY;

	/* move ahead */
	while (cl.time >= le->endTime) {
		/* Ensure that we are displayed where we are supposed to be, in case the last frame came too quickly. */
		Grid_PosToVec(cl.mapData->map, le->fieldSize, le->pos, le->origin);

		/* Record the last position of movement calculations. */
		VectorCopy(le->pos, le->oldPos);

		if (le->pathPos < le->pathLength) {
			LE_DoPathMove(le);
		} else {
			LE_DoEndPathMove(le);
			return;
		}
	}

	/* interpolate the position */
	Grid_PosToVec(cl.mapData->map, le->fieldSize, le->oldPos, start);
	Grid_PosToVec(cl.mapData->map, le->fieldSize, le->pos, dest);
	VectorSubtract(dest, start, delta);

	frac = (float) (cl.time - le->startTime) / (float) (le->endTime - le->startTime);

	/* calculate the new interpolated actor origin in the world */
	VectorMA(start, frac, delta, le->origin);
}

/**
 * @note Think function
 * @brief Change the actors animation to walking
 * @note See the *.anm files in the models dir
 */
void LET_StartPathMove (le_t *le)
{
	/* initial animation or animation change */
	R_AnimChange(&le->as, le->model1, LE_GetAnim("walk", le->right, le->left, le->state));
	if (!le->as.change)
		Com_Printf("LET_StartPathMove: Could not change anim of le: %i, team: %i, pnum: %i\n",
			le->entnum, le->team, le->pnum);

	LE_SetThink(le, LET_PathMove);
	LE_ExecuteThink(le);
}

/**
 * @note Think function
 */
static void LET_Projectile (le_t * le)
{
	if (cl.time >= le->endTime) {
		vec3_t impact;
		VectorCopy(le->origin, impact);
		CL_ParticleFree(le->ptl);
		/* don't run the think function again */
		le->inuse = qfalse;
		if (Q_strvalid(le->ref1)) {
			VectorCopy(le->ptl->s, impact);
			le->ptl = CL_ParticleSpawn(le->ref1, 0, impact, bytedirs[le->angle], NULL);
			VecToAngles(bytedirs[le->state], le->ptl->angles);
		}
		if (Q_strvalid(le->ref2)) {
			S_LoadAndPlaySample(le->ref2, impact, le->fd->impactAttenuation, SND_VOLUME_WEAPONS);
		}
		if (le->ref3) {
			/* Spawn blood particles (if defined) if actor(-body) was hit. Even if actor is dead. */
			/** @todo Special particles for stun attack (mind you that there is
			 * electrical and gas/chemical stunning)? */
			if (le->fd->obj->dmgtype != csi.damStunGas)
				LE_ActorBodyHit(le->ref3, impact, le->angle);
			CL_ActorPlaySound(le->ref3, SND_HURT);
		}
	} else if (CL_OutsideMap(le->ptl->s, UNIT_SIZE * 10)) {
		le->endTime = cl.time;
		CL_ParticleFree(le->ptl);
		/* don't run the think function again */
		le->inuse = qfalse;
	}
}

/*===========================================================================
 LE Special Effects
=========================================================================== */

void LE_AddProjectile (const fireDef_t *fd, int flags, const vec3_t muzzle, const vec3_t impact, int normal, le_t *leVictim)
{
	le_t *le;
	vec3_t delta;
	float dist;

	/* add le */
	le = LE_Add(0);
	if (!le)
		return;
	le->invis = !cl_leshowinvis->integer;
	/* bind particle */
	le->ptl = CL_ParticleSpawn(fd->projectile, 0, muzzle, NULL, NULL);
	if (!le->ptl) {
		le->inuse = qfalse;
		return;
	}

	/* calculate parameters */
	VectorSubtract(impact, muzzle, delta);
	dist = VectorLength(delta);

	VecToAngles(delta, le->ptl->angles);
	/* direction - bytedirs index */
	le->angle = normal;
	le->fd = fd;

	/* infinite speed projectile? */
	if (!fd->speed) {
		le->inuse = qfalse;
		le->ptl->size[0] = dist;
		VectorMA(muzzle, 0.5, delta, le->ptl->s);
		if ((flags & (SF_IMPACT | SF_BODY)) || (fd->splrad && !fd->bounce)) {
			ptl_t *ptl = NULL;
			const float *dir = bytedirs[le->angle];
			if (flags & SF_BODY) {
				if (fd->hitBodySound != NULL) {
					S_LoadAndPlaySample(fd->hitBodySound, le->origin, le->fd->impactAttenuation, SND_VOLUME_WEAPONS);
				}
				if (fd->hitBody != NULL)
					ptl = CL_ParticleSpawn(fd->hitBody, 0, impact, dir, NULL);

				/* Spawn blood particles (if defined) if actor(-body) was hit. Even if actor is dead. */
				/** @todo Special particles for stun attack (mind you that there is
				 * electrical and gas/chemical stunning)? */
				if (leVictim) {
					if (fd->obj->dmgtype != csi.damStunGas)
						LE_ActorBodyHit(leVictim, impact, le->angle);
					CL_ActorPlaySound(leVictim, SND_HURT);
				}
			} else {
				if (fd->impactSound != NULL) {
					S_LoadAndPlaySample(fd->impactSound, le->origin, le->fd->impactAttenuation, SND_VOLUME_WEAPONS);
				}
				if (fd->impact != NULL)
					ptl = CL_ParticleSpawn(fd->impact, 0, impact, dir, NULL);
			}
			if (ptl)
				VecToAngles(dir, ptl->angles);
		}
		return;
	}
	/* particle properties */
	VectorScale(delta, fd->speed / dist, le->ptl->v);
	le->endTime = cl.time + 1000 * dist / fd->speed;

	/* think function */
	if (flags & SF_BODY) {
		le->ref1 = fd->hitBody;
		le->ref2 = fd->hitBodySound;
		le->ref3 = leVictim;
	} else if ((flags & SF_IMPACT) || (fd->splrad && !fd->bounce)) {
		le->ref1 = fd->impact;
		le->ref2 = fd->impactSound;
	} else {
		le->ref1 = NULL;
		if (flags & SF_BOUNCING)
			le->ref2 = fd->bounceSound;
	}

	LE_SetThink(le, LET_Projectile);
	LE_ExecuteThink(le);
}

/**
 * @brief Returns the index of the biggest item in the inventory list
 * @note This item is the only one that will be drawn when lying at the floor
 * @sa LE_PlaceItem
 * @return the item index in the @c csi.ods array
 * @note Only call this for none empty invList_t - see FLOOR, LEFT, RIGHT and so on macros
 */
static const objDef_t *LE_BiggestItem (const invList_t *ic)
{
	const objDef_t *max;
	int maxSize = 0;

	for (max = ic->item.t; ic; ic = ic->next) {
		const int size = INVSH_ShapeSize(ic->item.t->shape);
		if (size > maxSize) {
			max = ic->item.t;
			maxSize = size;
		}
	}

	/* there must be an item in the invList_t */
	assert(max);
	return max;
}

/**
 * @sa CL_BiggestItem
 * @param[in] le The local entity (ET_ITEM) with the floor container
 */
void LE_PlaceItem (le_t *le)
{
	le_t *actor = NULL;

	assert(LE_IsItem(le));

	/* search owners (there can be many, some of them dead) */
	while ((actor = LE_GetNextInUse(actor))) {
		if ((actor->type == ET_ACTOR || actor->type == ET_ACTOR2x2)
		 && VectorCompare(actor->pos, le->pos)) {
			if (FLOOR(le))
				FLOOR(actor) = FLOOR(le);
		}
	}

	/* the le is an ET_ITEM entity, this entity is there to render dropped items
	 * if there are no items in the floor container, this entity can be
	 * deactivated */
	if (FLOOR(le)) {
		const objDef_t *biggest = LE_BiggestItem(FLOOR(le));
		le->model1 = cls.modelPool[biggest->idx];
		if (!le->model1)
			Com_Error(ERR_DROP, "Model for item %s is not precached in the cls.model_weapons array",
				biggest->id);
		Grid_PosToVec(cl.mapData->map, le->fieldSize, le->pos, le->origin);
		VectorSubtract(le->origin, biggest->center, le->origin);
		le->angles[ROLL] = 90;
		/*le->angles[YAW] = 10*(int)(le->origin[0] + le->origin[1] + le->origin[2]) % 360; */
		le->origin[2] -= GROUND_DELTA;
	} else {
		/* If no items in floor inventory, don't draw this le - the container is
		 * maybe empty because an actor picked up the last items here */
		le->removeNextFrame = qtrue;
	}
}

/**
 * @param[in] fd The grenade fire definition
 * @param[in] flags bitmask: SF_BODY, SF_IMPACT, SF_BOUNCING, SF_BOUNCED
 * @param[in] muzzle starting/location vector
 * @param[in] v0 velocity vector
 * @param[in] dt delta seconds
 * @param[in] leVictim The actor the grenade is thrown at (not yet supported)
 */
void LE_AddGrenade (const fireDef_t *fd, int flags, const vec3_t muzzle, const vec3_t v0, int dt, le_t* leVictim)
{
	le_t *le;
	vec3_t accel;

	/* add le */
	le = LE_Add(0);
	if (!le)
		return;
	le->invis = !cl_leshowinvis->integer;

	/* bind particle */
	VectorSet(accel, 0, 0, -GRAVITY);
	le->ptl = CL_ParticleSpawn(fd->projectile, 0, muzzle, v0, accel);
	if (!le->ptl) {
		le->inuse = qfalse;
		return;
	}
	/* particle properties */
	VectorSet(le->ptl->angles, 360 * crand(), 360 * crand(), 360 * crand());
	VectorSet(le->ptl->omega, 500 * crand(), 500 * crand(), 500 * crand());

	/* think function */
	if (flags & SF_BODY) {
		le->ref1 = fd->hitBody;
		le->ref2 = fd->hitBodySound;
		le->ref3 = leVictim;
	} else if ((flags & SF_IMPACT) || (fd->splrad && !fd->bounce)) {
		le->ref1 = fd->impact;
		le->ref2 = fd->impactSound;
	} else {
		le->ref1 = NULL;
		if (flags & SF_BOUNCING)
			le->ref2 = fd->bounceSound;
	}

	le->endTime = cl.time + dt;
	/* direction - bytedirs index (0,0,1) */
	le->angle = 5;
	le->fd = fd;
	LE_SetThink(le, LET_Projectile);
	LE_ExecuteThink(le);
}

/**
 * @brief Add function for brush models
 * @sa LE_AddToScene
 */
qboolean LE_BrushModelAction (le_t * le, entity_t * ent)
{
	switch (le->type) {
	case ET_ROTATING:
	case ET_DOOR:
		/* These cause the model to render correctly */
		VectorCopy(ent->mins, le->mins);
		VectorCopy(ent->maxs, le->maxs);
		VectorCopy(ent->origin, le->origin);
		VectorCopy(ent->angles, le->angles);
		break;
	case ET_DOOR_SLIDING:
		VectorCopy(le->origin, ent->origin);
		break;
	case ET_BREAKABLE:
		break;
	case ET_TRIGGER_RESCUE: {
		float x, y, z, xmax;
		const int drawFlags = cl_map_draw_rescue_zone->integer;

		if (!((1 << cl_worldlevel->integer) & le->levelflags))
			return qfalse;

		ent->flags = 0; /* Do not draw anything at all, if drawFlags set to 0 */
		enum { DRAW_TEXTURE = 0x1, DRAW_CIRCLES = 0x2 };
		ent->model = NULL;
		ent->alpha = 0.3f;
		VectorSet(ent->color, 0.5f, 1.0f, 0.0f);
		if ((drawFlags & DRAW_TEXTURE) && ent->texture == NULL) {
			ent->flags = RF_BOX;
			ent->texture = R_FindPics("sfx/misc/rescue");
			VectorSet(ent->color, 1, 1, 1);
		}
		VectorCopy(le->mins, ent->mins);
		VectorCopy(le->maxs, ent->maxs);

		if (!(drawFlags & DRAW_CIRCLES))
			return qfalse;

		/* There should be an easier way than calculating the grid coords back from the world coords */
		z = roundf(le->mins[2] / UNIT_HEIGHT) * UNIT_HEIGHT + UNIT_HEIGHT / 8.0f;
		xmax = roundf(le->maxs[0] / UNIT_SIZE) * UNIT_SIZE - 0.1f;
		for (x = roundf(le->mins[0] / UNIT_SIZE) * UNIT_SIZE; x < xmax; x += UNIT_SIZE) {
			const float ymax = roundf(le->maxs[1] / UNIT_SIZE) * UNIT_SIZE - 0.1f;
			for (y = roundf(le->mins[1] / UNIT_SIZE) * UNIT_SIZE; y < ymax; y += UNIT_SIZE) {
				const vec3_t pos = {x + UNIT_SIZE / 4.0f, y + UNIT_SIZE / 4.0f, z};
				entity_t circle;

				OBJZERO(circle);
				circle.flags = RF_PATH;
				VectorCopy(pos, circle.origin);
				circle.oldorigin[0] = circle.oldorigin[1] = circle.oldorigin[2] = UNIT_SIZE / 2.0f;
				VectorCopy(ent->color, circle.color);
				circle.alpha = ent->alpha;

				R_AddEntity(&circle);
			}
		}

		/* no other rendering entities should be added for the local entity */
		return qfalse;
	}
	default:
		break;
	}

	return qtrue;
}

void LET_BrushModel (le_t *le)
{
	const int delay = cl.time - le->thinkDelay;

	/* Updating model faster than 1000 times per second seems to be pretty much pointless */
	if (delay < 1)
		return;

	if (le->type == ET_ROTATING) {
		const float angle = le->angles[le->angle] + 0.001 * delay * le->rotationSpeed; /* delay is in msecs, speed in degrees per second */
		le->angles[le->angle] = (angle >= 360.0 ? angle - 360.0 : angle);
	}

	le->thinkDelay = cl.time;
}

void LMT_Init (localModel_t* localModel)
{
	if (localModel->target[0] != '\0') {
		localModel->parent = LM_GetByID(localModel->target);
		if (!localModel->parent)
			Com_Error(ERR_DROP, "Could not find local model entity with the id: '%s'.", localModel->target);
	}

	/* no longer needed */
	localModel->think = NULL;
}

/**
 * @brief Slides a door
 *
 * @note Though doors, sliding doors need a very different handling:
 * because it's movement is animated (unlike the rotating door),
 * the final position that is used to calculate the routing data
 * is set once the animation finished (because this recalculation
 * might be very expensive).
 *
 * @param[in,out] le The local entity of the inline model
 * @param[in] speed The speed to slide with - a negative value to close the door
 * @sa Door_SlidingUse
 */
void LET_SlideDoor (le_t *le, int speed)
{
	vec3_t moveAngles, moveDir;
	qboolean endPos = qfalse;
	int distance;

	/* get the movement angle vector */
	GET_SLIDING_DOOR_SHIFT_VECTOR(le->dir, speed, moveAngles);

	/* this origin is only an offset to the absolute mins/maxs for rendering */
	VectorAdd(le->origin, moveAngles, le->origin);

	/* get the direction vector from the movement angles that were set on the entity */
	AngleVectors(moveAngles, moveDir, NULL, NULL);
	moveDir[0] = fabsf(moveDir[0]);
	moveDir[1] = fabsf(moveDir[1]);
	moveDir[2] = fabsf(moveDir[2]);
	/* calculate the distance from the movement angles and the entity size */
	distance = DotProduct(moveDir, le->size);

	if (speed > 0) {
		/* check whether the distance the door may slide is slided already
		 * - if so, stop the movement of the door */
		if (fabs(le->origin[le->dir & 3]) >= distance)
			endPos = qtrue;
	} else {
		/* the sliding door has not origin set - except when it is opened. This door type is no
		 * origin brush based bmodel entity. So whenever the origin vector is not the zero vector,
		 * the door is opened. */
		if (VectorEmpty(le->origin))
			endPos = qtrue;
	}

	if (endPos) {
		vec3_t distanceVec;
		/* the door finished its move - either close or open, so make sure to recalc the routing
		 * data and set the mins/maxs for the inline brush model */
		cBspModel_t *model = CM_InlineModel(cl.mapTiles, le->inlineModelName);

		assert(model);

		/* we need the angles vector normalized */
		GET_SLIDING_DOOR_SHIFT_VECTOR(le->dir, (speed < 0) ? -1 : 1, moveAngles);

		/* the bounding box of the door is updated in one step - here is no lerping needed */
		VectorMul(distance, moveAngles, distanceVec);

		VectorAdd(model->mins, distanceVec, model->mins);
		VectorAdd(model->maxs, distanceVec, model->maxs);
		CL_RecalcRouting(le);

		/* reset the think function as the movement finished */
		LE_SetThink(le, NULL);
	} else
		le->thinkDelay = 1000;
}

/**
 * @brief Adds ambient sounds from misc_sound entities
 * @sa CL_SpawnParseEntitystring
 */
void LE_AddAmbientSound (const char *sound, const vec3_t origin, int levelflags, float volume, float attenuation)
{
	le_t* le;
	int sampleIdx;

	if (strstr(sound, "sound/"))
		sound += 6;

	sampleIdx = S_LoadSampleIdx(sound);
	if (!sampleIdx)
		return;

	le = LE_Add(0);
	if (!le) {
		Com_Printf("Could not add ambient sound entity\n");
		return;
	}
	le->type = ET_SOUND;
	le->sampleIdx = sampleIdx;
	VectorCopy(origin, le->origin);
	le->invis = !cl_leshowinvis->integer;
	le->levelflags = levelflags;
	le->attenuation = attenuation;

	if (volume < 0.0f || volume > 1.0f) {
		le->volume = SND_VOLUME_DEFAULT;
		Com_Printf("Invalid volume for local entity given - only values between 0.0 and 1.0 are valid\n");
	} else {
		le->volume = volume;
	}

	Com_DPrintf(DEBUG_SOUND, "Add ambient sound '%s' with volume %f\n", sound, volume);
}

/*===========================================================================
 LE Management functions
=========================================================================== */

/**
 * @brief Add a new local entity to the scene
 * @param[in] entnum The entity number (server side)
 * @sa LE_Get
 */
le_t *LE_Add (int entnum)
{
	le_t *le = NULL;

	while ((le = LE_GetNext(le))) {
		if (!le->inuse)
			/* found a free LE */
			break;
	}

	/* list full, try to make list longer */
	if (!le) {
		if (cl.numLEs >= MAX_EDICTS) {
			/* no free LEs */
			Com_Error(ERR_DROP, "Too many LEs");
		}

		/* list isn't too long */
		le = &cl.LEs[cl.numLEs];
		cl.numLEs++;
	}

	/* initialize the new LE */
	OBJZERO(*le);
	le->inuse = qtrue;
	le->entnum = entnum;
	le->fieldSize = ACTOR_SIZE_NORMAL;
	return le;
}

void _LE_NotFoundError (int entnum, int type, const char *file, const int line)
{
	Cmd_ExecuteString("debug_listle");
	Cmd_ExecuteString("debug_listedicts");
	if (type >= 0) {
		Com_Error(ERR_DROP, "LE_NotFoundError: Could not get LE with entnum %i of type: %i (%s:%i)\n", entnum, type, file, line);
	} else {
		Com_Error(ERR_DROP, "LE_NotFoundError: Could not get LE with entnum %i (%s:%i)\n", entnum, file, line);
	}
}

/**
 * @brief Center the camera on the local entity's origin
 * @param le The local entity which origin is used to center the camera
 * @sa CL_CenterView
 * @sa CL_ViewCenterAtGridPosition
 * @sa CL_CameraRoute
 */
void LE_CenterView (const le_t *le)
{
	/* if (cl_centerview->integer == 1 && cl.actTeam != cls.team) */
	if (!cl_centerview->integer)
		return;

	assert(le);
	Cvar_SetValue("cl_worldlevel", le->pos[2]);
	VectorCopy(le->origin, cl.cam.origin);
}

/**
 * @brief Searches all local entities for the one with the searched entnum
 * @param[in] entnum The entity number (server side)
 * @sa LE_Add
 */
le_t *LE_Get (int entnum)
{
	le_t *le = NULL;

	if (entnum == SKIP_LOCAL_ENTITY)
		return NULL;

	while ((le = LE_GetNextInUse(le))) {
		if (le->entnum == entnum)
			/* found the LE */
			return le;
	}

	/* didn't find it */
	return NULL;
}

/**
 * @brief Checks if a given le_t structure is locked, i.e., used by another event at this time.
 * @param entnum the entnum of the le_t struct involved.
 * @return true if the le_t is locked (used by another event), false if it's not or if it doesn't exist.
 */
qboolean LE_IsLocked (int entnum)
{
	le_t *le = LE_Get(entnum);
	return (le != NULL && le->locked);
}

/**
 * @brief Markes a le_t struct as locked.  Should be called at the
 *  beginning of an event handler on this le_t, and paired with a LE_Unlock at the end.
 * @param le The struct to be locked.
 * @note Always make sure you call LE_Unlock at the end of the event
 *  (might be in a different function), to allow other events on this le_t.
 */
void LE_Lock (le_t *le)
{
	if (le->locked)
		Com_Error(ERR_DROP, "LE_Lock: Trying to lock %i which is already locked\n", le->entnum);

	le->locked = qtrue;
}

/**
 * @brief Unlocks a previously locked le_t struct.
 * @param le The le_t to unlock.
 * @note Make sure that this is always paired with the corresponding
 *  LE_Lock around the conceptual beginning and ending of a le_t event.
 *  Should never be called by the handler(s) of a different event than
 *  the one that locked le.  The owner of the lock is currently not
 *  checked.
 * @todo If the event loop ever becomes multithreaded, this should
 *  be a real mutex lock.
 */
void LE_Unlock (le_t *le)
{
	if (!le->locked)
		Com_Error(ERR_DROP, "LE_Unlock: Trying to unlock %i which is already unlocked\n", le->entnum);

	le->locked = qfalse;
}

/**
 * @brief Searches a local entity on a given grid field
 * @param[in] pos The grid pos to search for an item of the given type
 */
le_t *LE_GetFromPos (const pos3_t pos)
{
	le_t *le = NULL;

	while ((le = LE_GetNextInUse(le))) {
		if (VectorCompare(le->pos, pos))
			return le;
	}

	/* didn't find it */
	return NULL;
}

/**
 * @brief Iterate through the list of entities
 * @param lastLE The entity found in the previous iteration; if NULL, we start at the beginning
 */
le_t* LE_GetNext (le_t* lastLE)
{
	le_t* endOfLEs = &cl.LEs[cl.numLEs];
	le_t* le;

	if (!cl.numLEs)
		return NULL;

	if (!lastLE)
		return cl.LEs;

	assert(lastLE >= cl.LEs);
	assert(lastLE < endOfLEs);

	le = lastLE;

	le++;
	if (le >= endOfLEs)
		return NULL;
	else
		return le;
}

/**
 * @brief Iterate through the entities that are in use
 * @note we can hopefully get rid of this function once we know when it makes sense
 * to iterate through entities that are NOT in use
 * @param lastLE The entity found in the previous iteration; if NULL, we start at the beginning
 */
le_t* LE_GetNextInUse (le_t* lastLE)
{
	le_t* le = lastLE;

	while ((le = LE_GetNext(le))) {
		if (le->inuse)
			break;
	}
	return le;
}

/**
 * @brief Returns entities that have origins within a spherical area.
 * @param[in] from The entity to start the search from. @c NULL will start from the beginning.
 * @param[in] org The origin that is the center of the circle.
 * @param[in] rad radius to search an edict in.
 * @param[in] type Type of local entity. @c ET_NULL to ignore the type.
 */
le_t *LE_FindRadius (le_t *from, const vec3_t org, float rad, entity_type_t type)
{
	le_t *le = from;

	while ((le = LE_GetNextInUse(le))) {
		int j;
		vec3_t eorg;
		for (j = 0; j < 3; j++)
			eorg[j] = org[j] - (le->origin[j] + (le->mins[j] + le->maxs[j]) * 0.5);
		if (VectorLength(eorg) > rad)
			continue;
		if (type != ET_NULL && le->type != type)
			continue;
		return le;
	}

	return NULL;
}

/**
 * @brief Searches a local entity on a given grid field
 * @param[in] type Entity type
 * @param[in] pos The grid pos to search for an item of the given type
 */
le_t *LE_Find (entity_type_t type, const pos3_t pos)
{
	le_t *le = NULL;

	while ((le = LE_GetNextInUse(le))) {
		if (le->type == type && VectorCompare(le->pos, pos))
			/* found the LE */
			return le;
	}

	/* didn't find it */
	return NULL;
}

/** @sa BoxOffset in cl_actor.c */
#define ModelOffset(i, target) (target[0]=(i-1)*(UNIT_SIZE+BOX_DELTA_WIDTH)/2, target[1]=(i-1)*(UNIT_SIZE+BOX_DELTA_LENGTH)/2, target[2]=0)

/**
 * Origin brush entities are bmodel entities that have their mins/maxs relative to the world origin.
 * The origin vector of the entity will be used to calculate e.g. the culling (and not the mins/maxs like
 * for other entities).
 * @param le The local entity to check
 * @return @c true if the given local entity is a func_door or func_rotating
 */
static inline qboolean LE_IsOriginBrush (const le_t *const le)
{
	return (le->type == ET_DOOR || le->type == ET_ROTATING);
}

/**
 * @brief Adds a box that highlights the current active door
 */
static void LE_AddEdictHighlight (const le_t *le)
{
	entity_t ent;
	const cBspModel_t *model = LE_GetClipModel(le);

	OBJZERO(ent);

	ent.flags = RF_BOX;
	VectorSet(ent.color, 1, 1, 1);
	ent.alpha = (sin(cl.time * 6.28) + 1.0) / 2.0;
	CalculateMinsMaxs(le->angles, model->mins, model->maxs, le->origin, ent.mins, ent.maxs);
	R_AddEntity(&ent);
}

/**
 * @sa CL_ViewRender
 * @sa CL_AddUGV
 * @sa CL_AddActor
 */
void LE_AddToScene (void)
{
	le_t *le;
	entity_t ent;
	vec3_t modelOffset;
	int i;

	for (i = 0, le = cl.LEs; i < cl.numLEs; i++, le++) {
		if (le->removeNextFrame) {
			le->inuse = qfalse;
			le->removeNextFrame = qfalse;
		}
		if (le->inuse && !le->invis) {
			if (le->contents & CONTENTS_SOLID) {
				if (!((1 << cl_worldlevel->integer) & le->levelflags))
					continue;
			} else if (le->contents & CONTENTS_DETAIL) {
				/* show them always */
			} else if (le->pos[2] > cl_worldlevel->integer)
				continue;

			OBJZERO(ent);

			ent.alpha = le->alpha;

			VectorCopy(le->angles, ent.angles);
			ent.model = le->model1;
			ent.skinnum = le->skinnum;
			ent.lighting = &le->lighting;

			switch (le->contents) {
			/* Only breakables do not use their origin; func_doors and func_rotating do!!!
			 * But none of them have animations. */
			case CONTENTS_SOLID:
			case CONTENTS_DETAIL: /* they use mins/maxs */
				break;
			default:
				/* set entity values */
				R_EntitySetOrigin(&ent, le->origin);
				VectorCopy(le->origin, ent.oldorigin);
				/* store animation values */
				ent.as = le->as;
				break;
			}

			if (LE_IsOriginBrush(le)) {
				ent.isOriginBrushModel = qtrue;
				R_EntitySetOrigin(&ent, le->origin);
				VectorCopy(le->origin, ent.oldorigin);
			}

			/* Offset the model to be inside the cursor box */
			switch (le->fieldSize) {
			case ACTOR_SIZE_NORMAL:
			case ACTOR_SIZE_2x2:
				ModelOffset(le->fieldSize, modelOffset);
				R_EntityAddToOrigin(&ent, modelOffset);
				VectorAdd(ent.oldorigin, modelOffset, ent.oldorigin);
				break;
			default:
				break;
			}

			if (le->selected && le->clientAction != NULL) {
				const le_t *action = le->clientAction;
				LE_AddEdictHighlight(action);
			}

			/* call add function */
			/* if it returns false, don't draw */
			if (le->addFunc)
				if (!le->addFunc(le, &ent))
					continue;

			/* add it to the scene */
			R_AddEntity(&ent);

			if (cl_le_debug->integer)
				CL_ParticleSpawn("cross", 0, le->origin, NULL, NULL);
		}
	}
}

/**
 * @brief Cleanup unused LE inventories that the server sent to the client
 * also free some unused LE memory
 */
void LE_Cleanup (void)
{
	int i;
	le_t *le;

	Com_DPrintf(DEBUG_CLIENT, "LE_Cleanup: Clearing up to %i unused LE inventories\n", cl.numLEs);
	for (i = cl.numLEs - 1, le = &cl.LEs[cl.numLEs - 1]; i >= 0; i--, le--) {
		if (!le->inuse)
			continue;
		if (LE_IsActor(le))
			CL_ActorCleanup(le);
		else if (LE_IsItem(le))
			cls.i.EmptyContainer(&cls.i, &le->i, INVDEF(csi.idFloor));

		le->inuse = qfalse;
	}
}

#ifdef DEBUG
/**
 * @brief Shows a list of current know local entities with type and status
 */
void LE_List_f (void)
{
	int i;
	le_t *le;

	Com_Printf("number | entnum | type | inuse | invis | pnum | team | size |  HP | state | level | model/ptl\n");
	for (i = 0, le = cl.LEs; i < cl.numLEs; i++, le++) {
		Com_Printf("#%5i | #%5i | %4i | %5i | %5i | %4i | %4i | %4i | %3i | %5i | %5i | ",
			i, le->entnum, le->type, le->inuse, le->invis, le->pnum, le->team,
			le->fieldSize, le->HP, le->state, le->levelflags);
		if (le->type == ET_PARTICLE) {
			if (le->ptl)
				Com_Printf("%s\n", le->ptl->ctrl->name);
			else
				Com_Printf("no ptl\n");
		} else if (le->model1)
			Com_Printf("%s\n", le->model1->name);
		else
			Com_Printf("no mdl\n");
	}
}

/**
 * @brief Shows a list of current know local models
 */
void LM_List_f (void)
{
	int i;
	localModel_t *lm;

	Com_Printf("number | entnum | skin | frame | lvlflg | renderflags | origin          | name\n");
	for (i = 0, lm = cl.LMs; i < cl.numLMs; i++, lm++) {
		Com_Printf("#%5i | #%5i | #%3i | #%4i | %6i | %11i | %5.0f:%5.0f:%3.0f | %s\n",
			i, lm->entnum, lm->skin, lm->frame, lm->levelflags, lm->renderFlags,
			lm->origin[0], lm->origin[1], lm->origin[2], lm->name);
	}
}

#endif

/*===========================================================================
 LE Tracing
=========================================================================== */

/** @brief Client side moveclip */
typedef struct {
	vec3_t boxmins, boxmaxs;	/**< enclose the test object along entire move */
	const float *mins, *maxs;	/**< size of the moving object */
	const float *start, *end;
	trace_t trace;
	const le_t *passle, *passle2;		/**< ignore these for clipping */
	int contentmask;			/**< search these in your trace - see MASK_* */
} moveclip_t;

const cBspModel_t *LE_GetClipModel (const le_t *le)
{
	const cBspModel_t *model;
	const unsigned int index = le->modelnum1;
	if (index > lengthof(cl.model_clip))
		Com_Error(ERR_DROP, "Clip model index out of bounds");
	model = cl.model_clip[index];
	if (!model)
		Com_Error(ERR_DROP, "LE_GetClipModel: Could not find inline model %u", index);
	return model;
}

model_t *LE_GetDrawModel (unsigned int index)
{
	model_t *model;
	if (index == 0 || index > lengthof(cl.model_draw))
		Com_Error(ERR_DROP, "Draw model index out of bounds");
	model = cl.model_draw[index];
	if (!model)
		Com_Error(ERR_DROP, "LE_GetDrawModel: Could not find model %u", index);
	return model;
}

/**
 * @brief Returns a headnode that can be used for testing or clipping an
 * object of mins/maxs size.
 * Offset is filled in to contain the adjustment that must be added to the
 * testing object's origin to get a point to use with the returned hull.
 * @param[in] le The local entity to get the bmodel from
 * @param[out] tile The maptile the bmodel belongs, too
 * @param[out] rmaShift the shift vector in case of an RMA (needed for doors)
 * @param[out] angles The rotation of the entity (in case of bmodels)
 * @return The headnode for the local entity
 * @sa SV_HullForEntity
 */
static int32_t CL_HullForEntity (const le_t *le, int *tile, vec3_t rmaShift, vec3_t angles)
{
	/* special case for bmodels */
	if (le->contents & CONTENTS_SOLID) {
		const cBspModel_t *model = LE_GetClipModel(le);
		/* special value for bmodel */
		if (!model)
			Com_Error(ERR_DROP, "CL_HullForEntity: Error - le with NULL bmodel (%i)\n", le->type);
		*tile = model->tile;
		VectorCopy(le->angles, angles);
		VectorCopy(model->shift, rmaShift);
		return model->headnode;
	} else {
		/* might intersect, so do an exact clip */
		*tile = 0;
		VectorCopy(vec3_origin, angles);
		VectorCopy(vec3_origin, rmaShift);
		return CM_HeadnodeForBox(&(cl.mapTiles->mapTiles[*tile]), le->mins, le->maxs);
	}
}

/**
 * @brief Clip against solid entities
 * @sa CL_Trace
 * @sa SV_ClipMoveToEntities
 */
static void CL_ClipMoveToLEs (moveclip_t * clip)
{
	le_t *le = NULL;

	if (clip->trace.allsolid)
		return;

	while ((le = LE_GetNextInUse(le))) {
		int tile = 0;
		trace_t trace;
		int32_t headnode;
		vec3_t angles;
		vec3_t origin, shift;

		if (!(le->contents & clip->contentmask))
			continue;
		if (le == clip->passle || le == clip->passle2)
			continue;

		headnode = CL_HullForEntity(le, &tile, shift, angles);
		assert(headnode < MAX_MAP_NODES);

		VectorCopy(le->origin, origin);

		trace = CM_HintedTransformedBoxTrace(&(cl.mapTiles->mapTiles[tile]), clip->start, clip->end, clip->mins, clip->maxs,
				headnode, clip->contentmask, 0, origin, angles, shift, 1.0);

		if (trace.fraction < clip->trace.fraction) {
			qboolean oldStart;

			/* make sure we keep a startsolid from a previous trace */
			oldStart = clip->trace.startsolid;
			trace.le = le;
			clip->trace = trace;
			clip->trace.startsolid |= oldStart;
		/* if true, plane is not valid */
		} else if (trace.allsolid) {
			trace.le = le;
			clip->trace = trace;
		/* if true, the initial point was in a solid area */
		} else if (trace.startsolid) {
			trace.le = le;
			clip->trace.startsolid = qtrue;
		}
	}
}


/**
 * @brief Create the bounding box for the entire move
 * @param[in] start Start vector to start the trace from
 * @param[in] mins Bounding box used for tracing
 * @param[in] maxs Bounding box used for tracing
 * @param[in] end End vector to stop the trace at
 * @param[out] boxmins The resulting box mins
 * @param[out] boxmaxs The resulting box maxs
 * @sa CL_Trace
 * @note Box is expanded by 1
 */
static inline void CL_TraceBounds (const vec3_t start, const vec3_t mins, const vec3_t maxs, const vec3_t end, vec3_t boxmins, vec3_t boxmaxs)
{
	int i;

	for (i = 0; i < 3; i++) {
		if (end[i] > start[i]) {
			boxmins[i] = start[i] + mins[i] - 1;
			boxmaxs[i] = end[i] + maxs[i] + 1;
		} else {
			boxmins[i] = end[i] + mins[i] - 1;
			boxmaxs[i] = start[i] + maxs[i] + 1;
		}
	}
}

/**
 * @brief Moves the given mins/maxs volume through the world from start to end.
 * @note Passedict and edicts owned by passedict are explicitly not checked.
 * @sa CL_TraceBounds
 * @sa CL_ClipMoveToLEs
 * @sa SV_Trace
 * @param[in] start Start vector to start the trace from
 * @param[in] end End vector to stop the trace at
 * @param[in] mins Bounding box used for tracing
 * @param[in] maxs Bounding box used for tracing
 * @param[in] passle Ignore this local entity in the trace (might be NULL)
 * @param[in] passle2 Ignore this local entity in the trace (might be NULL)
 * @param[in] contentmask Searched content the trace should watch for
 * @param[in] worldLevel The worldlevel (0-7) to calculate the levelmask for the trace from
 */
trace_t CL_Trace (const vec3_t start, const vec3_t end, const vec3_t mins, const vec3_t maxs, const le_t * passle, le_t * passle2, int contentmask, int worldLevel)
{
	moveclip_t clip;

	if (cl_trace_debug->integer)
		R_DrawBoundingBoxBatched(mins, maxs);

	/* clip to world */
	clip.trace = CM_CompleteBoxTrace(cl.mapTiles, start, end, mins, maxs, (1 << (worldLevel + 1)) - 1, contentmask, 0);
	clip.trace.le = NULL;
	if (clip.trace.fraction == 0)
		return clip.trace;		/* blocked by the world */

	clip.contentmask = contentmask;
	clip.start = start;
	clip.end = end;
	clip.mins = mins;
	clip.maxs = maxs;
	clip.passle = passle;
	clip.passle2 = passle2;

	/* create the bounding box of the entire move */
	CL_TraceBounds(start, mins, maxs, end, clip.boxmins, clip.boxmaxs);

	/* clip to other solid entities */
	CL_ClipMoveToLEs(&clip);

	return clip.trace;
}
