/**
 * @file
 * @brief Local entity management.
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

#include "../client.h"
#include "cl_localentity.h"
#include "../sound/s_main.h"
#include "../sound/s_sample.h"
#include "cl_particle.h"
#include "cl_actor.h"
#include "cl_hud.h"
#include "../renderer/r_mesh_anim.h"
#include "../renderer/r_draw.h"
#include "../../common/tracing.h"
#include "../../common/grid.h"
#include "../../shared/moveclip.h"

cvar_t* cl_le_debug;
cvar_t* cl_trace_debug;
cvar_t* cl_map_draw_rescue_zone;

/*===========================================================================
Local Model (LM) handling
=========================================================================== */

static inline void LE_GenerateInlineModelList (void)
{
	le_t* le = nullptr;
	int i = 0;

	while ((le = LE_GetNextInUse(le))) {
		if (le->model1 && le->inlineModelName[0] == '*')
			cl.leInlineModelList[i++] = le->inlineModelName;
	}
	cl.leInlineModelList[i] = nullptr;
}

static void CL_GridRecalcRouting (const le_t* le)
{
	/* We ALWAYS check against a model, even if it isn't in use.
	 * An unused model is NOT included in the inline list, so it doesn't get
	 * traced against. */
	if (!le->model1 || le->inlineModelName[0] != '*')
		return;

	if (Com_ServerState())
		return;

	const cBspModel_t* model = CM_InlineModel(cl.mapTiles, le->inlineModelName);
	if (!model) {
		return;
	}
	AABB absBox(model->cbmBox);
	absBox.shift(model->origin);
	GridBox rerouteBox(absBox);

	Grid_RecalcRouting(cl.mapTiles, cl.mapData->routing, le->inlineModelName, rerouteBox, cl.leInlineModelList);
}

/**
 * @sa G_CompleteRecalcRouting
 */
void CL_CompleteRecalcRouting (void)
{
	const double start = time(nullptr);	/* stopwatch */

	LE_GenerateInlineModelList();

	int i = 0;
	for (const le_t* le = cl.LEs; i < cl.numLEs; i++, le++)
		CL_GridRecalcRouting(le);

	Com_Printf("Rerouted for %i LEs in %5.2fs\n", i, time(nullptr) - start);
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

static void LM_AddToSceneOrder (bool parents)
{
	for (int i = 0; i < cl.numLMs; i++) {
		localModel_t& lm = cl.LMs[i];
		if (!lm.inuse)
			continue;

		/* check for visibility */
		if (!((1 << cl_worldlevel->integer) & lm.levelflags))
			continue;

		/* if we want to render the parents and this is a child (has a parent assigned)
		 * then skip it */
		if (parents && lm.parent)
			continue;

		/* if we want to render the children and this is a parent (no further parent
		 * assigned), then skip it. */
		if (!parents && lm.parent == nullptr)
			continue;

		/* set entity values */
		entity_t ent(RF_NONE);
		assert(lm.model);
		ent.model = lm.model;
		ent.skinnum = lm.skin;
		ent.lighting = &lm.lighting;
		ent.setScale(lm.scale);

		if (lm.parent) {
			/** @todo what if the tagent is not rendered due to different level flags? */
			ent.tagent = R_GetEntity(lm.parent->renderEntityNum);
			if (ent.tagent == nullptr)
				Com_Error(ERR_DROP, "Invalid parent entity num for local model (%s/%s): %i",
						lm.model->name, lm.id, lm.parent->renderEntityNum);
			ent.tagname = lm.tagname;
		} else {
			R_EntitySetOrigin(&ent, lm.origin);
			VectorCopy(lm.origin, ent.oldorigin);
			VectorCopy(lm.angles, ent.angles);

			if (lm.animname[0] != '\0') {
				ent.as = lm.as;
				/* do animation */
				R_AnimRun(&lm.as, ent.model, cls.frametime * 1000);
			} else {
				ent.as.frame = lm.frame;
			}
		}

		/* renderflags like RF_PULSE */
		ent.flags = lm.renderFlags;

		/* add it to the scene */
		lm.renderEntityNum = R_AddEntity(&ent);
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
	LM_AddToSceneOrder(true);
	LM_AddToSceneOrder(false);
}

/**
 * @brief Checks whether a local model with the same entity number is already registered
 */
static inline localModel_t* LM_Find (int entnum)
{
	for (int i = 0; i < cl.numLMs; i++)
		if (cl.LMs[i].entnum == entnum)
			return &cl.LMs[i];

	return nullptr;
}

/**
 * @brief link any floor container into the actor temp floor container
 */
void LE_LinkFloorContainer (le_t* le)
{
	le_t* floorItem = LE_Find(ET_ITEM, le->pos);
	if (floorItem)
		le->setFloor(floorItem);
	else
		le->resetFloor();
}

/**
 * @brief Checks whether the given le is a living actor
 * @param[in] le The local entity to perform the check for
 * @sa G_IsLivingActor
 */
bool LE_IsActor (const le_t* le)
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
bool LE_IsLivingActor (const le_t* le)
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
bool LE_IsLivingAndVisibleActor (const le_t* le)
{
	assert(le);
	if (LE_IsInvisible(le))
		return false;

	assert(le->type != ET_ACTORHIDDEN);

	return LE_IsLivingActor(le);
}

/**
 * @brief Register misc_models
 * @sa CL_ViewLoadMedia
 */
void LM_Register (void)
{
	for (int i = 0; i < cl.numLMs; i++) {
		localModel_t& lm = cl.LMs[i];

		/* register the model */
		lm.model = R_FindModel(lm.name);
		if (lm.animname[0]) {
			R_AnimChange(&lm.as, lm.model, lm.animname);
			if (!lm.as.change)
				Com_Printf("LM_Register: Could not change anim of %s to '%s'\n",
						lm.name, lm.animname);
		}
		if (!lm.model)
			lm.inuse = false;
	}
}

void LE_SetThink (le_t* le, localEntityThinkFunc_t think)
{
	le->think = think;
}

localModel_t* LM_GetByID (const char* id)
{
	if (Q_strnull(id))
		return nullptr;

	for (int i = 0; i < cl.numLMs; i++) {
		localModel_t* lm = &cl.LMs[i];
		if (Q_streq(lm->id, id))
			return lm;
	}
	return nullptr;
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
localModel_t* LM_AddModel (const char* model, const vec3_t origin, const vec3_t angles, int entnum, int levelflags, int renderFlags, const vec3_t scale)
{
	if (cl.numLMs >= MAX_LOCALMODELS)
		Com_Error(ERR_DROP, "Too many local models\n");

	/* check whether there is already a model with that number */
	if (LM_Find(entnum))
		Com_Error(ERR_DROP, "Already a local model with the same id (%i) loaded\n", entnum);

	localModel_t* lm = &cl.LMs[cl.numLMs++];
	OBJZERO(*lm);
	Q_strncpyz(lm->name, model, sizeof(lm->name));
	VectorCopy(origin, lm->origin);
	VectorCopy(angles, lm->angles);
	lm->entnum = entnum;
	lm->levelflags = levelflags;
	lm->renderFlags = renderFlags;
	lm->inuse = true;
	lm->setScale(scale);

	return lm;
}

/*===========================================================================
LE thinking
=========================================================================== */

/**
 * @brief Call think function for the given local entity if its still in use
 */
void LE_ExecuteThink (le_t* le)
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
	if (cls.state != ca_active)
		return;

	le_t* le = nullptr;
	while ((le = LE_GetNext(le))) {
		LE_ExecuteThink(le);
		/* do animation - even for invisible entities */
		R_AnimRun(&le->as, le->model1, cls.frametime * 1000);
	}
}

void LM_Think (void)
{
	for (int i = 0; i < cl.numLMs; i++) {
		localModel_t& lm = cl.LMs[i];
		if (lm.think)
			lm.think(&lm);
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
const char* LE_GetAnim (const char* anim, int right, int left, int state)
{
	if (!anim)
		return "";

	static char retAnim[MAX_VAR];
	char* mod = retAnim;
	size_t length = sizeof(retAnim);

	/* add crouched flag */
	if (state & STATE_CROUCHED) {
		*mod++ = 'c';
		length--;
	}

	/* determine relevant data */
	char animationIndex;
	char const* type;
	if (right == NONE) {
		animationIndex = '0';
		if (left == NONE)
			type = "item";
		else {
			type = INVSH_GetItemByIDX(left)->type;
			/* left hand grenades look OK with default anim; others don't */
			if (!Q_streq(type, "grenade")) {
				type = "pistol_d";
			}
		}
	} else {
		const objDef_t* od = INVSH_GetItemByIDX(right);
		animationIndex = od->animationIndex;
		type = od->type;
		if (left != NONE && Q_streq(od->type, "pistol") && Q_streq(INVSH_GetItemByIDX(left)->type, "pistol")) {
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
void LET_StartIdle (le_t* le)
{
	/* hidden actors don't have models assigned, thus we can not change the
	 * animation for any model */
	if (le->type != ET_ACTORHIDDEN) {
		if (LE_IsDead(le))
			R_AnimChange(&le->as, le->model1, va("dead%i", LE_GetAnimationIndexForDeath(le)));
		else if (LE_IsPanicked(le))
			R_AnimChange(&le->as, le->model1, "panic0");
		else
			R_AnimChange(&le->as, le->model1, LE_GetAnim("stand", le->right, le->left, le->state));
	}

	le->pathPos = le->pathLength = 0;
	if (le->stepList != nullptr) {
		leStep_t* step = le->stepList->next;
		Mem_Free(le->stepList);
		le->stepList = step;
		if (step != nullptr) {
			le->stepIndex--;
		} else if (le->stepIndex != 0) {
			Com_Error(ERR_DROP, "stepindex for entnum %i is out of sync (%i should be 0)\n", le->entnum, le->stepIndex);
		}
	}

	/* keep this animation until something happens */
	LE_SetThink(le, nullptr);
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
	if (!LE_IsCrouched(le)) {
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
static void LE_PlaySoundFileAndParticleForSurface (le_t* le, const char* textureName)
{
	const terrainType_t* t = Com_GetTerrainType(textureName);
	if (!t)
		return;

	/* origin might not be up-to-date here - but pos should be */
	vec3_t origin;
	PosToVec(le->pos, origin);

	/** @todo use the Grid_Fall method (ACTOR_SIZE_NORMAL) to ensure, that the particle is
	 * drawn at the ground (if needed - maybe the origin is already ground aligned)*/
	if (t->particle) {
		/* check whether actor is visible */
		if (!LE_IsStunned(le) && LE_IsLivingAndVisibleActor(le))
			CL_ParticleSpawn(t->particle, 0, origin);
	}
	if (t->footstepSound) {
		Com_DPrintf(DEBUG_SOUND, "LE_PlaySoundFileAndParticleForSurface: volume %.2f\n", t->footstepVolume);
		S_LoadAndPlaySample(t->footstepSound, origin, SOUND_ATTN_STATIC, t->footstepVolume);
	}
}

/**
 * sqrt(2) for diagonal movement
 */
int LE_ActorGetStepTime (const le_t* le, const pos3_t pos, const pos3_t oldPos, const int dir, const int speed)
{
	if (dir != DIRECTION_FALL) {
		return (((dir & (CORE_DIRECTIONS - 1)) >= BASE_DIRECTIONS ? UNIT_SIZE * 1.41 : UNIT_SIZE) * 1000 / speed);
	} else {
		vec3_t start, dest;
		/* This needs to account for the distance of the fall. */
		Grid_PosToVec(cl.mapData->routing, le->fieldSize, oldPos, start);
		Grid_PosToVec(cl.mapData->routing, le->fieldSize, pos, dest);
		/* 1/1000th of a second per model unit in height change */
		return (start[2] - dest[2]);
	}
}

static void LE_PlayFootStepSound (le_t* le)
{
	if (Q_strvalid(le->teamDef->footstepSound)) {
		S_LoadAndPlaySample(le->teamDef->footstepSound, le->origin, SOUND_ATTN_NORM, SND_VOLUME_FOOTSTEPS);
		return;
	}
	/* walking in water will not play the normal footstep sounds */
	if (!le->pathContents[le->pathPos]) {
		vec3_t from, to;

		/* prepare trace vectors */
		PosToVec(le->pos, from);
		VectorCopy(from, to);
		/* we should really hit the ground with this */
		to[2] -= UNIT_HEIGHT;

		const trace_t trace = CL_Trace(Line(from, to), AABB::EMPTY, nullptr, nullptr, MASK_SOLID, cl_worldlevel->integer);
		if (trace.surface)
			LE_PlaySoundFileAndParticleForSurface(le, trace.surface->name);
	} else
		LE_PlaySoundFileForContents(le, le->pathContents[le->pathPos]);
}

static void LE_DoPathMove (le_t* le)
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
void LE_DoEndPathMove (le_t* le)
{
	/* Verify the current position */
	if (!VectorCompare(le->pos, le->newPos))
		Com_Error(ERR_DROP, "LE_DoEndPathMove: Actor movement is out of sync: %i:%i:%i should be %i:%i:%i (step %i of %i) (team %i)",
				le->pos[0], le->pos[1], le->pos[2], le->newPos[0], le->newPos[1], le->newPos[2], le->pathPos, le->pathLength, le->team);

	CL_ActorConditionalMoveCalc(le);
	/* if the moving actor was not the selected one, */
	/* recalc the pathing table for the selected one, too. */
	if (!LE_IsSelected(le)) {
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
static void LE_ActorBodyHit (const le_t* le, const vec3_t impact, int normal)
{
	if (le->teamDef) {
		/* Spawn "hit_particle" if defined in teamDef. */
		if (le->teamDef->hitParticle[0] != '\0')
			CL_ParticleSpawn(le->teamDef->hitParticle, 0, impact, bytedirs[normal]);
	}
}

/**
 * @brief Move the actor along the path to the given location
 * @note Think function
 * @sa CL_ActorDoMove
 */
static void LET_PathMove (le_t* le)
{
	/* check for start of the next step */
	if (cl.time < le->startTime)
		return;

	/* move ahead */
	while (cl.time >= le->endTime) {
		/* Ensure that we are displayed where we are supposed to be, in case the last frame came too quickly. */
		Grid_PosToVec(cl.mapData->routing, le->fieldSize, le->pos, le->origin);

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
	vec3_t start, dest, delta;
	Grid_PosToVec(cl.mapData->routing, le->fieldSize, le->oldPos, start);
	Grid_PosToVec(cl.mapData->routing, le->fieldSize, le->pos, dest);
	VectorSubtract(dest, start, delta);

	const float frac = (float) (cl.time - le->startTime) / (float) (le->endTime - le->startTime);

	/* calculate the new interpolated actor origin in the world */
	VectorMA(start, frac, delta, le->origin);
}

/**
 * @note Think function
 * @brief Change the actors animation to walking
 * @note See the *.anm files in the models dir
 */
void LET_StartPathMove (le_t* le)
{
	/* center view (if wanted) */
	if (!cls.isOurRound() && le->team != TEAM_CIVILIAN)
		LE_CenterView(le);

	/* initial animation or animation change */
	R_AnimChange(&le->as, le->model1, LE_GetAnim("walk", le->right, le->left, le->state));
	if (!le->as.change)
		Com_Printf("LET_StartPathMove: Could not change anim of le: %i, team: %i, pnum: %i\n",
			le->entnum, le->team, le->pnum);

	LE_SetThink(le, LET_PathMove);
	LE_ExecuteThink(le);
}

/**
 * @note Think function.
 * @brief Handle move for invisible actors.
 * @todo Is there something we should do here?
 */
void LET_HiddenMove (le_t* le)
{
	VectorCopy(le->newPos, le->pos);
	LE_SetThink(le, LET_StartIdle);
	LE_ExecuteThink(le);
	LE_Unlock(le);
}

/**
 * @note Think function
 */
static void LET_Projectile (le_t* le)
{
	if (cl.time >= le->endTime) {
		vec3_t impact;
		VectorCopy(le->origin, impact);
		CL_ParticleFree(le->ptl);
		/* don't run the think function again */
		le->inuse = false;
		if (Q_strvalid(le->ref1)) {
			VectorCopy(le->ptl->s, impact);
			le->ptl = CL_ParticleSpawn(le->ref1, 0, impact, bytedirs[le->angle]);
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
		le->inuse = false;
	}
}

/*===========================================================================
 LE Special Effects
=========================================================================== */

void LE_AddProjectile (const fireDef_t* fd, int flags, const vec3_t muzzle, const vec3_t impact, int normal, le_t* leVictim)
{
	/* add le */
	le_t* le = LE_Add(0);
	if (!le)
		return;
	LE_SetInvisible(le);
	/* bind particle */
	le->ptl = CL_ParticleSpawn(fd->projectile, 0, muzzle);
	if (!le->ptl) {
		le->inuse = false;
		return;
	}

	/* calculate parameters */
	vec3_t delta;
	VectorSubtract(impact, muzzle, delta);
	const float dist = VectorLength(delta);

	VecToAngles(delta, le->ptl->angles);
	/* direction - bytedirs index */
	le->angle = normal;
	le->fd = fd;

	/* infinite speed projectile? */
	if (!fd->speed) {
		le->inuse = false;
		le->ptl->size[0] = dist;
		VectorMA(muzzle, 0.5, delta, le->ptl->s);
		if ((flags & (SF_IMPACT | SF_BODY)) || (fd->splrad && !fd->bounce)) {
			ptl_t* ptl = nullptr;
			const float* dir = bytedirs[le->angle];
			if (flags & SF_BODY) {
				if (fd->hitBodySound != nullptr) {
					S_LoadAndPlaySample(fd->hitBodySound, le->origin, le->fd->impactAttenuation, SND_VOLUME_WEAPONS);
				}
				if (fd->hitBody != nullptr)
					ptl = CL_ParticleSpawn(fd->hitBody, 0, impact, dir);

				/* Spawn blood particles (if defined) if actor(-body) was hit. Even if actor is dead. */
				/** @todo Special particles for stun attack (mind you that there is
				 * electrical and gas/chemical stunning)? */
				if (leVictim) {
					if (fd->obj->dmgtype != csi.damStunGas)
						LE_ActorBodyHit(leVictim, impact, le->angle);
					if (fd->damage[0] >= 0)
						CL_ActorPlaySound(leVictim, SND_HURT);
				}
			} else {
				if (fd->impactSound != nullptr) {
					S_LoadAndPlaySample(fd->impactSound, le->origin, le->fd->impactAttenuation, SND_VOLUME_WEAPONS);
				}
				if (fd->impact != nullptr)
					ptl = CL_ParticleSpawn(fd->impact, 0, impact, dir);
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
		le->ref1 = nullptr;
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
 * @note Only call this for none empty Item
 */
static const objDef_t* LE_BiggestItem (const Item* ic)
{
	assert(ic);
	const objDef_t* max;
	int maxSize = 0;

	for (max = ic->def(); ic; ic = ic->getNext()) {
		const int size = INVSH_ShapeSize(ic->def()->shape);
		if (size > maxSize) {
			max = ic->def();
			maxSize = size;
		}
	}

	/* there must be an item in the Item */
	assert(max);
	return max;
}

/**
 * @sa CL_BiggestItem
 * @param[in] le The local entity (ET_ITEM) with the floor container
 */
void LE_PlaceItem (le_t* le)
{
	assert(LE_IsItem(le));

	/* search owners (there can be many, some of them dead) */
	le_t* actor = nullptr;
	while ((actor = LE_GetNextInUse(actor))) {
		if ((actor->type == ET_ACTOR || actor->type == ET_ACTOR2x2)
		 && VectorCompare(actor->pos, le->pos)) {
			if (le->getFloorContainer())
				actor->setFloor(le);
		}
	}

	/* the le is an ET_ITEM entity, this entity is there to render dropped items
	 * if there are no items in the floor container, this entity can be
	 * deactivated */
	Item* floorCont = le->getFloorContainer();
	if (floorCont) {
		const objDef_t* biggest = LE_BiggestItem(floorCont);
		le->model1 = cls.modelPool[biggest->idx];
		if (!le->model1)
			Com_Error(ERR_DROP, "Model for item %s is not precached in the cls.model_weapons array",
				biggest->id);
		Grid_PosToVec(cl.mapData->routing, le->fieldSize, le->pos, le->origin);
		VectorSubtract(le->origin, biggest->center, le->origin);
		le->angles[ROLL] = 90;
		/*le->angles[YAW] = 10*(int)(le->origin[0] + le->origin[1] + le->origin[2]) % 360; */
		le->origin[2] -= GROUND_DELTA;
	} else {
		/* If no items in floor inventory, don't draw this le - the container is
		 * maybe empty because an actor picked up the last items here */
		le->flags |= LE_REMOVE_NEXT_FRAME;
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
void LE_AddGrenade (const fireDef_t* fd, int flags, const vec3_t muzzle, const vec3_t v0, int dt, le_t* leVictim)
{
	/* add le */
	le_t* le = LE_Add(0);
	if (!le)
		return;
	LE_SetInvisible(le);

	/* bind particle */
	vec3_t accel;
	VectorSet(accel, 0, 0, -GRAVITY);
	le->ptl = CL_ParticleSpawn(fd->projectile, 0, muzzle, v0, accel);
	if (!le->ptl) {
		le->inuse = false;
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
		le->ref1 = nullptr;
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
bool LE_BrushModelAction (le_t* le, entity_t* ent)
{
	switch (le->type) {
	case ET_ROTATING:
	case ET_DOOR:
		/* These cause the model to render correctly */
		le->aabb.set(ent->eBox);
		VectorCopy(ent->origin, le->origin);
		VectorCopy(ent->angles, le->angles);
		break;
	case ET_DOOR_SLIDING:
		VectorCopy(le->origin, ent->origin);
		break;
	case ET_BREAKABLE:
		break;
	case ET_TRIGGER_RESCUE: {
		const int drawFlags = cl_map_draw_rescue_zone->integer;

		if (!((1 << cl_worldlevel->integer) & le->levelflags))
			return false;

		ent->flags = 0; /* Do not draw anything at all, if drawFlags set to 0 */
		enum { DRAW_TEXTURE = 0x1, DRAW_CIRCLES = 0x2 };
		ent->model = nullptr;
		ent->alpha = 0.3f;
		VectorSet(ent->color, 0.5f, 1.0f, 0.0f);
		if ((drawFlags & DRAW_TEXTURE) && ent->texture == nullptr) {
			ent->flags = RF_BOX;
			ent->texture = R_FindPics("sfx/misc/rescue");
			VectorSet(ent->color, 1, 1, 1);
		}
		ent->eBox.set(le->aabb);

		if (!(drawFlags & DRAW_CIRCLES))
			return false;

		/* The triggerbox seems to be 'off-by-one'. The '- UNIT_SIZE' compensates for that. */
		for (vec_t x = le->aabb.getMinX(); x < le->aabb.getMaxX() - UNIT_SIZE; x += UNIT_SIZE) {
			for (vec_t y = le->aabb.getMinY(); y < le->aabb.getMaxY() - UNIT_SIZE; y += UNIT_SIZE) {
				const vec3_t center = {x + UNIT_SIZE / 2, y + UNIT_SIZE / 2, le->aabb.getMinZ()};

				entity_t circle(RF_PATH);
				VectorCopy(center, circle.origin);
				circle.oldorigin[0] = circle.oldorigin[1] = circle.oldorigin[2] = UNIT_SIZE / 2.0f;
				VectorCopy(ent->color, circle.color);
				circle.alpha = ent->alpha;

				R_AddEntity(&circle);
			}
		}

		/* no other rendering entities should be added for the local entity */
		return false;
	}
	default:
		break;
	}

	return true;
}

void LET_BrushModel (le_t* le)
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
	localModel->think = nullptr;
}

/**
 * @brief Rotates a door in the given speed
 *
 * @param[in] le The local entity of the door to rotate
 * @param[in] speed The speed to rotate the door with
 */
void LET_RotateDoor (le_t* le, int speed)
{
	/** @todo lerp the rotation */
	const int angle = speed > 0 ? DOOR_ROTATION_ANGLE : -DOOR_ROTATION_ANGLE;
	if (le->dir & DOOR_OPEN_REVERSE)
		le->angles[le->dir & 3] -= angle;
	else
		le->angles[le->dir & 3] += angle;

	CM_SetInlineModelOrientation(cl.mapTiles, le->inlineModelName, le->origin, le->angles);
	CL_RecalcRouting(le);

	/* reset the think function as the movement finished */
	LE_SetThink(le, nullptr);
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
void LET_SlideDoor (le_t* le, int speed)
{
	vec3_t moveAngles, moveDir;

	/* get the movement angle vector */
	GET_SLIDING_DOOR_SHIFT_VECTOR(le->dir, speed, moveAngles);

	/* this origin is only an offset to the absolute mins/maxs for rendering */
	VectorAdd(le->origin, moveAngles, le->origin);

	/* get the direction vector from the movement angles that were set on the entity */
	AngleVectors(moveAngles, moveDir, nullptr, nullptr);
	moveDir[0] = fabsf(moveDir[0]);
	moveDir[1] = fabsf(moveDir[1]);
	moveDir[2] = fabsf(moveDir[2]);
	/* calculate the distance from the movement angles and the entity size */
	const int distance = DotProduct(moveDir, le->size);

	bool endPos = false;
	if (speed > 0) {
		/* check whether the distance the door may slide is slided already
		 * - if so, stop the movement of the door */
		if (fabs(le->origin[le->dir & 3]) >= distance)
			endPos = true;
	} else {
		/* the sliding door has not origin set - except when it is opened. This door type is no
		 * origin brush based bmodel entity. So whenever the origin vector is not the zero vector,
		 * the door is opened. */
		if (VectorEmpty(le->origin))
			endPos = true;
	}

	if (endPos) {
		vec3_t distanceVec;
		/* the door finished its move - either close or open, so make sure to recalc the routing
		 * data and set the mins/maxs for the inline brush model */
		cBspModel_t* model = CM_InlineModel(cl.mapTiles, le->inlineModelName);

		assert(model);

		/* we need the angles vector normalized */
		GET_SLIDING_DOOR_SHIFT_VECTOR(le->dir, (speed < 0) ? -1 : 1, moveAngles);

		/* the bounding box of the door is updated in one step - here is no lerping needed */
		VectorMul(distance, moveAngles, distanceVec);

		model->cbmBox.shift(distanceVec);
		CL_RecalcRouting(le);

		/* reset the think function as the movement finished */
		LE_SetThink(le, nullptr);
	} else
		le->thinkDelay = 1000;
}

/**
 * @brief Adds ambient sounds from misc_sound entities
 * @sa CL_SpawnParseEntitystring
 */
void LE_AddAmbientSound (const char* sound, const vec3_t origin, int levelflags, float volume, float attenuation)
{
	if (strstr(sound, "sound/"))
		sound += 6;

	int sampleIdx = S_LoadSampleIdx(sound);
	if (!sampleIdx)
		return;

	le_t* le = LE_Add(0);
	if (!le) {
		Com_Printf("Could not add ambient sound entity\n");
		return;
	}
	le->type = ET_SOUND;
	le->sampleIdx = sampleIdx;
	VectorCopy(origin, le->origin);
	LE_SetInvisible(le);
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
le_t* LE_Add (int entnum)
{
	le_t* le = nullptr;

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
	le->inuse = true;
	le->entnum = entnum;
	le->fieldSize = ACTOR_SIZE_NORMAL;
	return le;
}

void _LE_NotFoundError (int entnum, int type, const char* file, const int line)
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
void LE_CenterView (const le_t* le)
{
	if (!cl_centerview->integer)
		return;

	assert(le);
	if (le->team == cls.team) {
		const float minDistToMove = 4.0f * UNIT_SIZE;
		const float dist = Vector2Dist(cl.cam.origin, le->origin);
		if (dist < minDistToMove) {
			pos3_t currentCamPos;
			VecToPos(cl.cam.origin, currentCamPos);
			if (le->pos[2] != currentCamPos[2])
				Cvar_SetValue("cl_worldlevel", le->pos[2]);
			return;
		}

		VectorCopy(le->origin, cl.cam.origin);
	} else {
		pos3_t pos;
		VecToPos(cl.cam.origin, pos);
		CL_CheckCameraRoute(pos, le->pos);
	}
}

/**
 * @brief Searches all local entities for the one with the searched entnum
 * @param[in] entnum The entity number (server side)
 * @sa LE_Add
 */
le_t* LE_Get (int entnum)
{
	le_t* le = nullptr;

	if (entnum == SKIP_LOCAL_ENTITY)
		return nullptr;

	while ((le = LE_GetNextInUse(le))) {
		if (le->entnum == entnum)
			/* found the LE */
			return le;
	}

	/* didn't find it */
	return nullptr;
}

/**
 * @brief Checks if a given le_t structure is locked, i.e., used by another event at this time.
 * @param entnum the entnum of the le_t struct involved.
 * @return true if the le_t is locked (used by another event), false if it's not or if it doesn't exist.
 */
bool LE_IsLocked (int entnum)
{
	le_t* le = LE_Get(entnum);
	return (le != nullptr && (le->flags & LE_LOCKED));
}

/**
 * @brief Markes a le_t struct as locked.  Should be called at the
 *  beginning of an event handler on this le_t, and paired with a LE_Unlock at the end.
 * @param le The struct to be locked.
 * @note Always make sure you call LE_Unlock at the end of the event
 *  (might be in a different function), to allow other events on this le_t.
 */
void LE_Lock (le_t* le)
{
	if (le->flags & LE_LOCKED)
		Com_Error(ERR_DROP, "LE_Lock: Trying to lock %i which is already locked\n", le->entnum);

	le->flags |= LE_LOCKED;
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
void LE_Unlock (le_t* le)
{
	if (!(le->flags & LE_LOCKED))
		Com_Error(ERR_DROP, "LE_Unlock: Trying to unlock %i which is already unlocked\n", le->entnum);

	le->flags &= ~LE_LOCKED;
}

/**
 * @brief Searches a local entity on a given grid field
 * @param[in] pos The grid pos to search for an item of the given type
 */
le_t* LE_GetFromPos (const pos3_t pos)
{
	le_t* le = nullptr;

	while ((le = LE_GetNextInUse(le))) {
		if (VectorCompare(le->pos, pos))
			return le;
	}

	/* didn't find it */
	return nullptr;
}

/**
 * @brief Iterate through the list of entities
 * @param lastLE The entity found in the previous iteration; if nullptr, we start at the beginning
 */
le_t* LE_GetNext (le_t* lastLE)
{
	if (!cl.numLEs)
		return nullptr;

	if (!lastLE)
		return cl.LEs;

	le_t* endOfLEs = &cl.LEs[cl.numLEs];

	assert(lastLE >= cl.LEs);
	assert(lastLE < endOfLEs);

	le_t* le = lastLE;

	le++;
	if (le >= endOfLEs)
		return nullptr;
	else
		return le;
}

/**
 * @brief Iterate through the entities that are in use
 * @note we can hopefully get rid of this function once we know when it makes sense
 * to iterate through entities that are NOT in use
 * @param lastLE The entity found in the previous iteration; if nullptr, we start at the beginning
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
 * @param[in] from The entity to start the search from. @c nullptr will start from the beginning.
 * @param[in] org The origin that is the center of the circle.
 * @param[in] rad radius to search an edict in.
 * @param[in] type Type of local entity. @c ET_NULL to ignore the type.
 */
le_t* LE_FindRadius (le_t* from, const vec3_t org, float rad, entity_type_t type)
{
	le_t* le = from;

	while ((le = LE_GetNextInUse(le))) {
		if (type != ET_NULL && le->type != type)
			continue;
		vec3_t eorg;
		vec3_t leCenter;
		le->aabb.getCenter(leCenter);
		for (int j = 0; j < 3; j++)
			eorg[j] = org[j] - (le->origin[j] + leCenter[j]);
		if (VectorLength(eorg) > rad)
			continue;
		return le;
	}

	return nullptr;
}

/**
 * @brief Searches a local entity on a given grid field
 * @param[in] type Entity type
 * @param[in] pos The grid pos to search for an item of the given type
 */
le_t* LE_Find (entity_type_t type, const pos3_t pos)
{
	le_t* le = nullptr;

	while ((le = LE_GetNextInUse(le))) {
		if (le->type == type && VectorCompare(le->pos, pos))
			/* found the LE */
			return le;
	}

	/* didn't find it */
	return nullptr;
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
static inline bool LE_IsOriginBrush (const le_t* const le)
{
	return (le->type == ET_DOOR || le->type == ET_ROTATING);
}

/**
 * @brief Adds a box that highlights the current active door
 */
static void LE_AddEdictHighlight (const le_t* le)
{
	const cBspModel_t* model = LE_GetClipModel(le);

	entity_t ent(RF_BOX);
	VectorSet(ent.color, 1, 1, 1);
	ent.alpha = (sin(cl.time * 6.28) + 1.0) / 2.0;
	CalculateMinsMaxs(le->angles, model->cbmBox, le->origin, ent.eBox);
	R_AddEntity(&ent);
}

/**
 * @sa CL_ViewRender
 * @sa CL_AddUGV
 * @sa CL_AddActor
 */
void LE_AddToScene (void)
{
	for (int i = 0; i < cl.numLEs; i++) {
		le_t& le = cl.LEs[i];
		if (le.flags & LE_REMOVE_NEXT_FRAME) {
			le.inuse = false;
			le.flags &= ~LE_REMOVE_NEXT_FRAME;
		}
		if (le.inuse && !LE_IsInvisible(&le)) {
			if (le.flags & LE_CHECK_LEVELFLAGS) {
				if (!((1 << cl_worldlevel->integer) & le.levelflags))
					continue;
			} else if (le.flags & LE_ALWAYS_VISIBLE) {
				/* show them always */
			} else if (le.pos[2] > cl_worldlevel->integer)
				continue;

			entity_t ent(RF_NONE);
			ent.alpha = le.alpha;

			VectorCopy(le.angles, ent.angles);
			ent.model = le.model1;
			ent.skinnum = le.bodySkin;
			ent.lighting = &le.lighting;

			switch (le.contents) {
			/* Only breakables do not use their origin; func_doors and func_rotating do!!!
			 * But none of them have animations. */
			case CONTENTS_SOLID:
			case CONTENTS_DETAIL: /* they use mins/maxs */
				break;
			default:
				/* set entity values */
				R_EntitySetOrigin(&ent, le.origin);
				VectorCopy(le.origin, ent.oldorigin);
				/* store animation values */
				ent.as = le.as;
				break;
			}

			if (LE_IsOriginBrush(&le)) {
				ent.isOriginBrushModel = true;
				R_EntitySetOrigin(&ent, le.origin);
				VectorCopy(le.origin, ent.oldorigin);
			}

			/* Offset the model to be inside the cursor box */
			switch (le.fieldSize) {
			case ACTOR_SIZE_NORMAL:
			case ACTOR_SIZE_2x2:
				vec3_t modelOffset;
				ModelOffset(le.fieldSize, modelOffset);
				R_EntityAddToOrigin(&ent, modelOffset);
				VectorAdd(ent.oldorigin, modelOffset, ent.oldorigin);
				break;
			default:
				break;
			}

			if (LE_IsSelected(&le) && le.clientAction != nullptr) {
				const le_t* action = le.clientAction;
				if (action->inuse && action->type > ET_NULL && action->type < ET_MAX)
					LE_AddEdictHighlight(action);
			}

			/* call add function */
			/* if it returns false, don't draw */
			if (le.addFunc)
				if (!le.addFunc(&le, &ent))
					continue;

			/* add it to the scene */
			R_AddEntity(&ent);

			if (cl_le_debug->integer)
				CL_ParticleSpawn("cross", 0, le.origin);
		}
	}
}

/**
 * @brief Cleanup unused LE inventories that the server sent to the client
 * also free some unused LE memory
 */
void LE_Cleanup (void)
{
	Com_DPrintf(DEBUG_CLIENT, "LE_Cleanup: Clearing up to %i unused LE inventories\n", cl.numLEs);
	for (int i = cl.numLEs - 1; i >= 0; i--) {
		le_t* le = &cl.LEs[i];
		if (!le->inuse)
			continue;
		if (LE_IsActor(le))
			CL_ActorCleanup(le);
		else if (LE_IsItem(le))
			cls.i.emptyContainer(&le->inv, CID_FLOOR);

		le->inuse = false;
	}
}

#ifdef DEBUG
/**
 * @brief Shows a list of current know local entities with type and status
 */
void LE_List_f (void)
{
	Com_Printf("number | entnum | type | inuse | invis | pnum | team | size |  HP | state | level | model/ptl\n");
	for (int i = 0; i < cl.numLEs; i++) {
		le_t& le = cl.LEs[i];
		Com_Printf("#%5i | #%5i | %4i | %5i | %5i | %4i | %4i | %4i | %3i | %5i | %5i | ",
			i, le.entnum, le.type, le.inuse, LE_IsInvisible(&le), le.pnum, le.team,
			le.fieldSize, le.HP, le.state, le.levelflags);
		if (le.type == ET_PARTICLE) {
			if (le.ptl)
				Com_Printf("%s\n", le.ptl->ctrl->name);
			else
				Com_Printf("no ptl\n");
		} else if (le.model1)
			Com_Printf("%s\n", le.model1->name);
		else
			Com_Printf("no mdl\n");
	}
}

/**
 * @brief Shows a list of current know local models
 */
void LM_List_f (void)
{
	Com_Printf("number | entnum | skin | frame | lvlflg | renderflags | origin          | name\n");
	for (int i = 0; i < cl.numLMs; i++) {
		localModel_t& lm = cl.LMs[i];
		Com_Printf("#%5i | #%5i | #%3i | #%4i | %6i | %11i | %5.0f:%5.0f:%3.0f | %s\n",
			i, lm.entnum, lm.skin, lm.frame, lm.levelflags, lm.renderFlags,
			lm.origin[0], lm.origin[1], lm.origin[2], lm.name);
	}
}

#endif

/*===========================================================================
 LE Tracing
=========================================================================== */

/** @brief Client side moveclip */
class MoveClipCL : public MoveClip
{
public:
	trace_t trace;
	const le_t* passle, *passle2;		/**< ignore these for clipping */
};

const cBspModel_t* LE_GetClipModel (const le_t* le)
{
	const cBspModel_t* model;
	const unsigned int index = le->modelnum1;
	if (index > lengthof(cl.model_clip))
		Com_Error(ERR_DROP, "Clip model index out of bounds");
	model = cl.model_clip[index];
	if (!model)
		Com_Error(ERR_DROP, "LE_GetClipModel: Could not find inline model %u", index);
	return model;
}

model_t* LE_GetDrawModel (unsigned int index)
{
	if (index == 0 || index > lengthof(cl.model_draw))
		Com_Error(ERR_DROP, "Draw model index out of bounds");
	model_t* model = cl.model_draw[index];
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
static int32_t CL_HullForEntity (const le_t* le, int* tile, vec3_t rmaShift, vec3_t angles)
{
	/* special case for bmodels */
	if (le->contents & CONTENTS_SOLID) {
		const cBspModel_t* model = LE_GetClipModel(le);
		/* special value for bmodel */
		if (!model)
			Com_Error(ERR_DROP, "CL_HullForEntity: Error - le with nullptr bmodel (%i)\n", le->type);
		*tile = model->tile;
		VectorCopy(le->angles, angles);
		VectorCopy(model->shift, rmaShift);
		return model->headnode;
	} else {
		/* might intersect, so do an exact clip */
		*tile = 0;
		VectorCopy(vec3_origin, angles);
		VectorCopy(vec3_origin, rmaShift);
		return CM_HeadnodeForBox(cl.mapTiles->mapTiles[*tile], le->aabb);
	}
}

/**
 * @brief Clip against solid entities
 * @sa CL_Trace
 * @sa SV_ClipMoveToEntities
 */
static void CL_ClipMoveToLEs (MoveClipCL* clip)
{
	if (clip->trace.allsolid)
		return;

	le_t* le = nullptr;
	while ((le = LE_GetNextInUse(le))) {
		int tile = 0;

		if (!(le->contents & clip->contentmask))
			continue;
		if (le == clip->passle || le == clip->passle2)
			continue;

		vec3_t angles, shift;
		const int32_t headnode = CL_HullForEntity(le, &tile, shift, angles);
		assert(headnode < MAX_MAP_NODES);

		vec3_t origin;
		VectorCopy(le->origin, origin);

		trace_t trace = CM_HintedTransformedBoxTrace(cl.mapTiles->mapTiles[tile], clip->moveLine, clip->objBox,
				headnode, clip->contentmask, 0, origin, angles, shift, 1.0);

		if (trace.fraction < clip->trace.fraction) {
			/* make sure we keep a startsolid from a previous trace */
			const bool oldStart = clip->trace.startsolid;
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
			clip->trace.startsolid = true;
		}
	}
}

/**
 * @brief Moves the given mins/maxs volume through the world from start to end.
 * @note Passedict and edicts owned by passedict are explicitly not checked.
 * @sa CL_ClipMoveToLEs
 * @sa SV_Trace
 * @param[in] traceLine Start and end vector of the trace
 * @param[in] box The box we move through the world
 * @param[in] passle Ignore this local entity in the trace (might be nullptr)
 * @param[in] passle2 Ignore this local entity in the trace (might be nullptr)
 * @param[in] contentmask Searched content the trace should watch for
 * @param[in] worldLevel The worldlevel (0-7) to calculate the levelmask for the trace from
 */
trace_t CL_Trace (const Line& traceLine, const AABB& box, const le_t* passle, le_t* passle2, int contentmask, int worldLevel)
{
	if (cl_trace_debug->integer)
		R_DrawBoundingBoxBatched(box);

	/* clip to world */
	MoveClipCL clip;
	clip.trace = CM_CompleteBoxTrace(cl.mapTiles, traceLine, box, (1 << (worldLevel + 1)) - 1, contentmask, 0);
	clip.trace.le = nullptr;
	if (clip.trace.fraction == 0)
		return clip.trace;		/* blocked by the world */

	clip.contentmask = contentmask;
	clip.moveLine.set(traceLine);
	clip.objBox.set(box);
	clip.passle = passle;
	clip.passle2 = passle2;

	/* create the bounding box of the entire move */
	clip.calcBounds();

	/* clip to other solid entities */
	CL_ClipMoveToLEs(&clip);

	return clip.trace;
}
