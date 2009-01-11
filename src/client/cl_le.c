/**
 * @file cl_le.c
 * @brief Local entity managament.
 */

/*
Copyright (C) 2002-2007 UFO: Alien Invasion team.

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

#include "client.h"
#include "cl_sound.h"
#include "cl_particle.h"
#include "cl_actor.h"
#include "renderer/r_mesh_anim.h"
#include "../common/tracing.h"

localModel_t LMs[MAX_LOCALMODELS];
int numLMs;

le_t LEs[MAX_EDICTS];
int numLEs;

/*===========================================================================
Local Model (LM) handling
=========================================================================== */

static const char *leInlineModelList[MAX_EDICTS + 1];

static inline void LE_GenerateInlineModelList (void)
{
	le_t *le;
	int i, l;

	l = 0;
	for (i = 0, le = LEs; i < numLEs; i++, le++)
		if (le->inuse && le->model1 && le->inlineModelName[0] == '*')
			leInlineModelList[l++] = le->inlineModelName;
	leInlineModelList[l] = NULL;
}

/**
 * @sa G_CompleteRecalcRouting
 */
void CL_CompleteRecalcRouting (void)
{
	le_t* le;
	int i;

	LE_GenerateInlineModelList();

	for (i = 0, le = LEs; i < numLEs; i++, le++)
		/**
		 * We ALWAYS check against a model, even if it isn't in use.
		 * An unused model is NOT included in the inline list, so it doesn't get
		 * traced against.
		 */
		if (le->model1 && le->inlineModelName[0] == '*')
			Grid_RecalcRouting(clMap, le->inlineModelName, leInlineModelList);
}

/**
 * @sa LE_Explode
 */
void CL_RecalcRouting (const le_t* le)
{
	LE_GenerateInlineModelList();
	/**
	 * We ALWAYS check against a model, even if it isn't in use.
	 * An unused model is NOT included in the inline list, so it doesn't get
	 * traced against.
	 */
	if (le->model1 && le->inlineModelName[0] == '*')
		Grid_RecalcRouting(clMap, le->inlineModelName, leInlineModelList);

	CL_ConditionalMoveCalcForCurrentSelectedActor();
}

/**
 * @brief Add the local models to the scene
 * @sa V_RenderView
 * @sa LE_AddToScene
 * @sa LM_AddModel
 */
void LM_AddToScene (void)
{
	localModel_t *lm;
	entity_t ent;
	int i;

	for (i = 0, lm = LMs; i < numLMs; i++, lm++) {
		/* check for visibility */
		if (!((1 << cl_worldlevel->integer) & lm->levelflags))
			continue;

		/* set entity values */
		memset(&ent, 0, sizeof(ent));
		VectorCopy(lm->origin, ent.origin);
		VectorCopy(lm->origin, ent.oldorigin);
		VectorCopy(lm->angles, ent.angles);
		VectorCopy(lm->scale, ent.scale);
		assert(lm->model);
		ent.model = lm->model;
		ent.skinnum = lm->skin;
		ent.as.frame = lm->frame;
		if (lm->animname[0]) {
			ent.as = lm->as;
			/* do animation */
			R_AnimRun(&lm->as, ent.model, cls.frametime * 1000);
			lm->lighting.dirty = qtrue;
		}

		/* renderflags like RF_PULSE */
		ent.flags = lm->renderFlags;
		ent.lighting = &lm->lighting;

		/* add it to the scene */
		R_AddEntity(&ent);
	}
}

/**
 * @brief Checks whether a local model with the same entity number is already registered
 */
static inline localModel_t *LM_Find (int entnum)
{
	int i;

	for (i = 0; i < numLMs; i++)
		if (LMs[i].entnum == entnum)
			return &LMs[i];

	return NULL;
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
 * @brief Checks whether the given le is a living actor
 * @param[in] le The local entity to perform the check for
 * @sa G_IsLivingActor
 * @sa LE_IsActor
 */
qboolean LE_IsLivingActor (const le_t *le)
{
	assert(le);
	return LE_IsActor(le) && !LE_IsDead(le);
}

/**
 * @sa EV_DOOR_CLOSE
 * @sa EV_DOOR_OPEN
 * @sa G_ClientUseEdict
 */
static inline void LE_DoorAction (struct dbuffer *msg, qboolean openDoor)
{
	/* get local entity */
	const int entNum = NET_ReadShort(msg);
	le_t *le = LE_Get(entNum);
	if (!le) {
		Com_Printf("Can't close the door - le (%p) for entnum %i not found\n", le, entNum);
		return;
	}

	if (openDoor) {
		/** @todo YAW should be the orientation of the door */
		le->angles[YAW] += DOOR_ROTATION_ANGLE;
	} else {
		/** @todo YAW should be the orientation of the door */
		le->angles[YAW] -= DOOR_ROTATION_ANGLE;
	}

	Com_DPrintf(DEBUG_CLIENT, "Client processed door movement.\n");

	CM_SetInlineModelOrientation(le->inlineModelName, le->origin, le->angles);
	CL_RecalcRouting(le);
}

/**
 * @brief Callback for EV_DOOR_CLOSE event - rotates the inline model and recalc routing
 * @sa Touch_DoorTrigger
 */
void LE_DoorClose (struct dbuffer *msg)
{
	LE_DoorAction(msg, qfalse);
}

/**
 * @brief Callback for EV_DOOR_OPEN event - rotates the inline model and recalc routing
 * @sa Touch_DoorTrigger
 */
void LE_DoorOpen (struct dbuffer *msg)
{
	LE_DoorAction(msg, qtrue);
}

/**
 * @note e.g. func_breakable or func_door with health
 * @sa EV_MODEL_EXPLODE
 */
void LE_Explode (struct dbuffer *msg)
{
	const int num = NET_ReadShort(msg);
	le_t *le = LE_Get(num);
	if (!le)
		Com_Error(ERR_DROP, "LE_Explode: Could not find le with entnum %i", num);

	le->inuse = qfalse;

	/* Recalc the client routing table because this le (and the inline model) is now gone */
	CL_RecalcRouting(le);
}

/**
 * @brief Register misc_models
 * @sa CL_LoadMedia
 */
void LM_Register (void)
{
	localModel_t *lm;
	int i;

	for (i = 0, lm = LMs; i < numLMs; i++, lm++) {
		/* register the model */
		lm->model = R_RegisterModelShort(lm->name);
		if (lm->animname[0]) {
			R_AnimChange(&lm->as, lm->model, lm->animname);
			if (!lm->as.change)
				Com_Printf("LM_Register: Could not change anim of model '%s'\n", lm->animname);
		}
	}
}


/**
 * @brief Adds local (not known or handled by the server) models to the map
 * @note misc_model
 * @sa CL_ParseEntitystring
 * @param[in] entnum Entity number
 * @sa LM_AddToScene
 */
localModel_t *LM_AddModel (const char *model, const char *particle, const vec3_t origin, const vec3_t angles, int entnum, int levelflags, int renderFlags, const vec3_t scale)
{
	localModel_t *lm;

	lm = &LMs[numLMs++];

	if (numLMs >= MAX_LOCALMODELS)
		Sys_Error("Too many local models\n");

	memset(lm, 0, sizeof(*lm));
	Q_strncpyz(lm->name, model, sizeof(lm->name));
	Q_strncpyz(lm->particle, particle, sizeof(lm->particle));
	VectorCopy(origin, lm->origin);
	VectorCopy(angles, lm->angles);
	/* check whether there is already a model with that number */
	if (LM_Find(entnum))
		Com_Error(ERR_DROP, "Already a local model with the same id (%i) loaded\n", entnum);
	lm->entnum = entnum;
	lm->levelflags = levelflags;
	lm->renderFlags = renderFlags;
	lm->lighting.dirty = qtrue;
	VectorCopy(scale, lm->scale);

	return lm;
}

static const float mapZBorder = -(UNIT_HEIGHT * 5);
/**
 * @brief Checks whether give position is still inside the map borders
 */
qboolean CL_OutsideMap (const vec3_t impact, const float delta)
{
	if (impact[0] < map_min[0] - delta || impact[0] > map_max[0] + delta)
		return qtrue;

	if (impact[1] < map_min[1] - delta || impact[1] > map_max[1] + delta)
		return qtrue;

	/* if a le is deeper than 5 levels below the latest walkable level (0) then
	 * we can assume that it is outside the world
	 * This is needed because some maps (e.g. the dam map) has unwalkable levels
	 * that just exists for detail reasons */
	if (impact[2] < mapZBorder)
		return qtrue;

	/* still inside the map borders */
	return qfalse;
}

/*===========================================================================
LE thinking
=========================================================================== */

/**
 * @brief Calls the le think function
 * @sa LET_StartIdle
 * @sa LET_PathMove
 * @sa LET_StartPathMove
 * @sa LET_Projectile
 */
void LE_Think (void)
{
	le_t *le;
	int i;

	if (cls.state != ca_active)
		return;

	for (i = 0, le = LEs; i < numLEs; i++, le++) {
		if (le->inuse && le->think)
			/* call think function */
			le->think(le);
	}
}


/*===========================================================================
 LE think functions
=========================================================================== */

static char retAnim[MAX_VAR];

/**
 * @brief Get the correct animation for the given actor state and weapons
 * @param[in] right ods index to determine the weapon in the actors right hand
 * @param[in] left ods index to determine the weapon in the actors left hand
 * @param[in] state the actors state - e.g. STATE_CROUCHED (crounched animations)
 * have a 'c' in front of their animation definitions (see *.anm files for
 * characters)
 */
const char *LE_GetAnim (const char *anim, int right, int left, int state)
{
	char *mod;
	qboolean akimbo;
	char animationIndex;
	const char *type;
	size_t length = sizeof(retAnim);

	if (!anim)
		return "";

	mod = retAnim;

	/* add crouched flag */
	if (state & STATE_CROUCHED) {
		*mod++ = 'c';
		length--;
	}

	/* determine relevant data */
	akimbo = qfalse;
	if (right == NONE) {
		animationIndex = '0';
		if (left == NONE)
			type = "item";
		else {
			/* left hand grenades look OK with default anim; others don't */
			if (Q_strncmp(csi.ods[left].type, "grenade", 7))
				akimbo = qtrue;
			type = csi.ods[left].type;
		}
	} else {
		animationIndex = csi.ods[right].animationIndex;
		type = csi.ods[right].type;
		if (left != NONE && !Q_strncmp(csi.ods[right].type, "pistol", 6) && !Q_strncmp(csi.ods[left].type, "pistol", 6))
			akimbo = qtrue;
	}

	if (!Q_strncmp(anim, "stand", 5) || !Q_strncmp(anim, "walk", 4)) {
		Q_strncpyz(mod, anim, length);
		mod += strlen(anim);
		*mod++ = animationIndex;
		*mod++ = 0;
	} else {
		Com_sprintf(mod, length, "%s_%s", anim, akimbo ? "pistol_d" : type);
	}

	return retAnim;
}

/**
 * @sa LE_AddAmbientSound
 */
void LET_PlayAmbientSound (le_t * le)
{
	assert(le->sfx);

	if (!((1 << cl_worldlevel->integer) & le->levelflags)) {
		S_StopSound(le->sfx);
		le->sfx->channel = -1;
		return;
	}

	if (le->sfx->channel == -1) {
		S_SetVolume(le->sfx, le->sfx->volume);
		S_StartSound(NULL, le->sfx, -1.0f);
	}

	/* could not start the sound */
	if (le->sfx->channel == -1) {
		Com_DPrintf(DEBUG_SOUND, "Could not play the soundfile '%s'\n", le->sfx->name);
		return;
	}

	/* find the total contribution of all sounds of this type */
	if (S_Playing(le->sfx)) {
		float dist = VectorDist(cl.cam.camorg, le->origin);
		float volume = le->volume;
		if (dist >= SOUND_FULLVOLUME) {
			dist = 1.0 - (dist / SOUND_MAX_DISTANCE);
			if (dist < 0.)
				/* too far away */
				volume = 0;
			else {
				/* close enough to hear it, but apply a distance effect though
				 * because it's farer than SOUND_FULLVOLUME */
				volume *= dist;
			}
		}
		S_SetVolume(le->sfx, volume);
	} else {
		le->sfx->channel = -1;
	}
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
		else if (le->state & STATE_PANIC)
			R_AnimChange(&le->as, le->model1, "panic0");
		else
			R_AnimChange(&le->as, le->model1, LE_GetAnim("stand", le->right, le->left, le->state));
		if (!le->as.change)
			Com_Printf("LET_StartIdle: Could not change anim of le: %i, team: %i, pnum: %i\n",
				le->entnum, le->team, le->pnum);
	}

#ifdef DEBUG
	/* idle actors don't move */
	if (le->pathPos != le->pathLength)
		Com_Printf("LET_StartIdle: Looks like the actor has some steps to do. length: %i, pos: %i\n",
			le->pathLength, le->pathPos);
#endif
	le->pathPos = le->pathLength = 0;

	/* keep this animation until something happens */
	le->think = NULL;
}

/**
 * @param[in] contents The contents flash of the brush we are currently in
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
				S_StartSound(le->origin, cls.sound_pool[SOUND_WATER_OUT], DEFAULT_SOUND_PACKET_VOLUME);
			} else {
				/* play water entering sound */
				S_StartSound(le->origin, cls.sound_pool[SOUND_WATER_IN], DEFAULT_SOUND_PACKET_VOLUME);
			}
			return;
		}

		if (le->positionContents & CONTENTS_WATER) {
			/* play water leaving sound */
			S_StartSound(le->origin, cls.sound_pool[SOUND_WATER_MOVE], DEFAULT_SOUND_PACKET_VOLUME);
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
		/* check whether actor is visibile */
		if (le->invis != qfalse)
			CL_ParticleSpawn(t->particle, 0, origin, NULL, NULL);
	}
	if (t->footStepSound) {
		sfx_t *sfx = S_RegisterSound(t->footStepSound);
		Com_DPrintf(DEBUG_SOUND, "LE_PlaySoundFileAndParticleForSurface: volume %.2f\n", t->footStepVolume);
		S_StartSound(origin, sfx, t->footStepVolume);
	}
}

/**
 * @brief Searches the closest actor to the given world vector
 * @param[in] origin World position to get the closest actor to
 * @note Only your own team is searched
 */
le_t* LE_GetClosestActor (const vec3_t origin)
{
	int i, tmp;
	int dist = 999999;
	le_t *actor = NULL, *le;
	vec3_t leOrigin;

	for (i = 0, le = LEs; i < numLEs; i++, le++) {
		if (!le->inuse || le->pnum != cl.pnum)
			continue;
		if (!LE_IsLivingActor(le))
			continue;
		VectorSubtract(origin, le->origin, leOrigin);
		tmp = VectorLength(leOrigin);
		if (tmp < dist) {
			actor = le;
			dist = tmp;
		}
	}

	return actor;
}

/**
 * @brief Move the actor along the path to the given location
 * @note Think function
 * @sa CL_ActorDoMove
 */
static void LET_PathMove (le_t * le)
{
	float frac;
	int tuCost = 0;
	vec3_t start, dest, delta;

	/* check for start */
	if (cl.time <= le->startTime)
		return;

	/* move ahead */
	while (cl.time > le->endTime) {
		/* Ensure that we are displayed where we are supposed to be, in case the last frame came too quickly. */
		Grid_PosToVec(clMap, le->fieldSize, le->pos, le->origin);

		/* Record the last postition of movement calculations. */
		VectorCopy(le->pos, le->oldPos);

		if (le->pathPos < le->pathLength) {
			/* next part */
			const byte fulldv = le->path[le->pathPos];
			const byte dir = getDVdir(fulldv);
			const int crouching_state = le->state & STATE_CROUCHED ? 1 : 0;
			/** @note new_crouching_state needs to be set to the current crouching state and is possibly updated by PosAddDV. */
			int new_crouching_state = crouching_state;
			PosAddDV(le->pos, new_crouching_state, fulldv);
			Com_DPrintf(DEBUG_PATHING, "Moved in dir %i to (%i, %i, %i)\n", dir, le->pos[0], le->pos[1], le->pos[2]);
			/** @note we no longer need to adjust the value from Grid_MoveLength for crouching:
			 *  the function now accounts for crouching. */
			tuCost = Grid_MoveLength(&clPathMap, le->pos, new_crouching_state, qfalse) - Grid_MoveLength(&clPathMap, le->oldPos, crouching_state, qfalse);

			le->TU -= tuCost;
			if (le == selActor)
				actorMoveLength -= tuCost;

			/* walking in water will not play the normal footstep sounds */
			if (!le->pathContents[le->pathPos]) {
				trace_t trace;
				vec3_t from, to;

				/* prepare trace vectors */
				PosToVec(le->pos, from);
				VectorCopy(from, to);
				/* we should really hit the ground with this */
				to[2] -= UNIT_HEIGHT;

				trace = CL_Trace(from, to, vec3_origin, vec3_origin, NULL, NULL, MASK_SOLID);
				if (trace.surface)
					LE_PlaySoundFileAndParticleForSurface(le, trace.surface->name);
			} else
				LE_PlaySoundFileForContents(le, le->pathContents[le->pathPos]);

			/* only change the direction if the actor moves horizontally. */
			if (dir < CORE_DIRECTIONS || dir >= FLYING_DIRECTIONS)
				le->dir = dir & (CORE_DIRECTIONS - 1);
			le->angles[YAW] = dangle[le->dir];
			le->startTime = le->endTime;
			/* check for straight movement or diagonal movement */
			assert(le->speed);
			if (dir != DIRECTION_FALL) {
				/* sqrt(2) for diagonal movement */
				le->endTime += (le->dir >= BASE_DIRECTIONS ? UNIT_SIZE * 1.41 : UNIT_SIZE) * 1000 / le->speed;
			} else {
				/* This needs to account for the distance of the fall. */
				Grid_PosToVec(clMap, le->fieldSize, le->oldPos, start);
				Grid_PosToVec(clMap, le->fieldSize, le->pos, dest);
				/* 1/1000th of a second per model unit in height change */
				le->endTime += (start[2] - dest[2]);
			}

			Com_DPrintf(DEBUG_CLIENT, "LET_PathMove: moving from (%i %i %i) to (%i %i %i).\n"
				, le->oldPos[0], le->oldPos[1], le->oldPos[2]
				, le->pos[0], le->pos[1], le->pos[2]);

			le->positionContents = le->pathContents[le->pathPos];
			le->pathPos++;
		} else {
			/* end of move */
			le_t *floor;
#ifdef DEBUG
			{
				pos3_t gridPos;

				/* HACK: Attempt to correct actor positions */
				VecToPos(le->origin, gridPos);
				if (!Vector2Compare(gridPos, le->pos)) {
					Com_DPrintf(DEBUG_CLIENT, "LET_PathMove: Warning: Actor positions doesn't match "
						"(origin grid: %i:%i:%i, le->pos: %i:%i:%i)\n",
						gridPos[0], gridPos[1], gridPos[2], le->pos[0], le->pos[1], le->pos[2]);
					VectorCopy(gridPos, le->pos);
				}
			}
#endif
			/* Verify the current position */
			if (!VectorCompare(le->pos, le->newPos)) {
				/* Output an error message */
				Com_DPrintf(DEBUG_CLIENT, "The actor's end position is not what the server has. o(%i %i %i) n(%i %i %i)\n"
					, le->pos[0], le->pos[1], le->pos[2]
					, le->newPos[0], le->newPos[1], le->newPos[2]);
				/* Fix the location */
				VectorCopy(le->newPos, le->pos);
				Grid_PosToVec(clMap, le->fieldSize, le->pos, le->origin);
			}

			if (le == selActor)
				CL_ConditionalMoveCalcForCurrentSelectedActor();

			/* link any floor container into the actor temp floor container */
			floor = LE_Find(ET_ITEM, le->pos);
			if (floor)
				FLOOR(le) = FLOOR(floor);

			CL_UnblockEvents();

			le->think = LET_StartIdle;
			le->think(le);
			/* maybe there are some other EV_ACTOR_MOVE events following */
			return;
		}
	}

	/* interpolate the position */
	Grid_PosToVec(clMap, le->fieldSize, le->oldPos, start);
	Grid_PosToVec(clMap, le->fieldSize, le->pos, dest);
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

	le->think = LET_PathMove;
	le->think(le);
}

/**
 * @brief Autohides a projectile particle if no actor of your team can see it
 * @note Think function
 * @sa LET_Projectile
 * @sa CM_TestLine
 * @sa FrustumVis
 */
void LET_ProjectileAutoHide (le_t *le)
{
	int i;
	le_t *actors;

	/* only compute this every 20 frames */
	if (le->thinkDelay <= 0) {
		le->thinkDelay = 20;
		return;
	} else {
		le->thinkDelay--;
	}

	/* check whether any of our actors can see this le */
	for (i = 0, actors = LEs; i < numLEs; i++, actors++) {
		if (actors->team != cls.team)
			continue;
		if (LE_IsLivingActor(actors)) {
			/* at least one of our actors can see this */
			if (FrustumVis(actors->origin, actors->dir, le->origin)) {
				if (TR_TestLine(actors->origin, le->origin, TL_FLAG_NONE)) {
					if (!le->ptl) {
						le->ptl = CL_ParticleSpawn(le->particleID, le->levelflags, le->origin, NULL, NULL);
					}
					return;
				}
			}
		}
	}

	/* hide the particle */
	if (le->ptl) {
		CL_ParticleFree(le->ptl);
		le->ptl = NULL;
	}
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
		if (le->ref1 && le->ref1[0]) {
			VectorCopy(le->ptl->s, impact);
			/** @todo Why are we using le->state here, not le->dir? */
			le->ptl = CL_ParticleSpawn(le->ref1, 0, impact, bytedirs[le->state], NULL);
			VecToAngles(bytedirs[le->state], le->ptl->angles);
		}
		if (le->ref2 && le->ref2[0]) {
			sfx_t *sfx = S_RegisterSound(le->ref2);
			S_StartSound(impact, sfx, le->fd->relImpactVolume);
		}
	} else if (CL_OutsideMap(le->ptl->s, UNIT_SIZE * 10)) {
		le->endTime = cl.time;
		CL_ParticleFree(le->ptl);
		/* don't run the think function again */
		le->inuse = qfalse;
		/* we have to reset impact time here */
		CL_UnblockEvents();
	}
}

/*===========================================================================
 LE Special Effects
=========================================================================== */

void LE_AddProjectile (const fireDef_t *fd, int flags, const vec3_t muzzle, const vec3_t impact, int normal, qboolean autohide)
{
	le_t *le;
	vec3_t delta;
	float dist;

	/* add le */
	le = LE_Add(0);
	if (!le)
		return;
	le->invis = !cl_leshowinvis->integer;
	le->autohide = autohide;
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
	/* direction */
	/** @todo Why are we not using le->dir here?
	 * @sa LE_AddGrenade */
	le->state = normal;
	le->fd = fd;

	if (!fd->speed) {
		/* infinite speed projectile */
		ptl_t *ptl;

		le->inuse = qfalse;
		le->ptl->size[0] = dist;
		VectorMA(muzzle, 0.5, delta, le->ptl->s);
		if (flags & (SF_IMPACT | SF_BODY) || (fd->splrad && !fd->bounce)) {
			ptl = NULL;
			if (flags & SF_BODY) {
				if (fd->hitBodySound[0]) {
					sfx_t *sfx = S_RegisterSound(fd->hitBodySound);
					S_StartSound(le->origin, sfx, le->fd->relImpactVolume);
				}
				if (fd->hitBody[0])
					ptl = CL_ParticleSpawn(fd->hitBody, 0, impact, bytedirs[normal], NULL);
			} else {
				if (fd->impactSound[0]) {
					sfx_t *sfx = S_RegisterSound(fd->impactSound);
					S_StartSound(le->origin, sfx, le->fd->relImpactVolume);
				}
				if (fd->impact[0])
					ptl = CL_ParticleSpawn(fd->impact, 0, impact, bytedirs[normal], NULL);
			}
			if (ptl)
				VecToAngles(bytedirs[normal], ptl->angles);
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
	} else if (flags & SF_IMPACT || (fd->splrad && !fd->bounce)) {
		le->ref1 = fd->impact;
		le->ref2 = fd->impactSound;
	} else {
		le->ref1 = NULL;
		if (flags & SF_BOUNCING)
			le->ref2 = fd->bounceSound;
	}

	le->think = LET_Projectile;
	le->think(le);
}

/**
 * @param[in] fd The grenade fire definition
 * @param[in] flags bitmask: SF_BODY, SF_IMPACT, SF_BOUNCING, SF_BOUNCED
 * @param[in] muzzle starting/location vector
 * @param[in] v0 velocity vector
 * @param[in] dt delta seconds
 */
void LE_AddGrenade (const fireDef_t *fd, int flags, const vec3_t muzzle, const vec3_t v0, int dt)
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
	} else if (flags & SF_IMPACT || (fd->splrad && !fd-> bounce)) {
		le->ref1 = fd->impact;
		le->ref2 = fd->impactSound;
	} else {
		le->ref1 = NULL;
		if (flags & SF_BOUNCING)
			le->ref2 = fd->bounceSound;
	}

	le->endTime = cl.time + dt;
	/** @todo Why are we not using le->dir here?
	 @sa LE_AddProjectile */
	le->state = 5;				/* direction (0,0,1) */
	le->fd = fd;
	assert(fd);
	le->think = LET_Projectile;
	le->think(le);
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
	case ET_BREAKABLE:
		break;
	}

	return qtrue;
}

void LET_BrushModel (le_t *le)
{
	if (cl.time - le->thinkDelay < le->speed) {
		le->thinkDelay = cl.time;
		return;
	}

	if (le->type == ET_ROTATING) {
		float angle = le->angles[le->dir] + (1.0 / le->rotationSpeed);
		le->angles[le->dir] = (angle >= 360 ? angle - 360 : angle);
	}
}

/**
 * @brief Adds ambient sounds from misc_sound entities
 * @sa CL_ParseEntitystring
 */
void LE_AddAmbientSound (const char *sound, const vec3_t origin, float volume, int levelflags)
{
	le_t* le;
	sfx_t* sfx;

	sfx = S_RegisterSound(sound);
	if (!sfx) {
		Com_Printf("LE_AddAmbientSound: can't cache %s\n", sound);
		return;
	}

	le = LE_Add(0);
	if (!le) {
		Com_Printf("Could not add ambient sound entity\n");
		return;
	}
	le->type = ET_SOUND;
	/** @todo What if we call a snd_restart during this is played? */
	le->sfx = sfx;
	le->sfx->channel = -1;
	le->sfx->volume = -1; /* at least one volume change */
	le->volume = min(volume, MIX_MAX_VOLUME);
	VectorCopy(origin, le->origin);
	le->invis = !cl_leshowinvis->integer;
	le->levelflags = levelflags;
	le->think = LET_PlayAmbientSound;
	Com_DPrintf(DEBUG_SOUND, "Add ambient sound '%s'\n", sound);
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
	int i;
	le_t *le;

	for (i = 0, le = LEs; i < numLEs; i++, le++)
		if (!le->inuse)
			/* found a free LE */
			break;

	/* list full, try to make list longer */
	if (i == numLEs) {
		if (numLEs >= MAX_EDICTS) {
			/* no free LEs */
			Com_Error(ERR_DROP, "Too many LEs");
			return NULL;
		}

		/* list isn't too long */
		numLEs++;
	}

	/* initialize the new LE */
	memset(le, 0, sizeof(*le));
	le->inuse = qtrue;
	le->entnum = entnum;
	return le;
}

/**
 * @brief Searches all local entities for the one with the searched entnum
 * @param[in] entnum The entity number (server side)
 * @sa LE_Add
 */
le_t *LE_Get (int entnum)
{
	int i;
	le_t *le;

	for (i = 0, le = LEs; i < numLEs; i++, le++)
		if (le->inuse && le->entnum == entnum)
			/* found the LE */
			return le;

	/* didn't find it */
	return NULL;
}

/**
 * @brief Searches a local entity on a given grid field
 * @param[in] type Entity type
 * @param[in] pos The grid pos to search for an item of the given type
 */
le_t *LE_Find (int type, pos3_t pos)
{
	int i;
	le_t *le;

	for (i = 0, le = LEs; i < numLEs; i++, le++)
		if (le->inuse && le->type == type && VectorCompare(le->pos, pos))
			/* found the LE */
			return le;

	/* didn't find it */
	return NULL;
}

/** @sa BoxOffset in cl_actor.c */
#define ModelOffset(i, target) (target[0]=(i-1)*(UNIT_SIZE+BOX_DELTA_WIDTH)/2, target[1]=(i-1)*(UNIT_SIZE+BOX_DELTA_LENGTH)/2, target[2]=0)

/**
 * @sa V_RenderView
 * @sa CL_AddUGV
 * @sa CL_AddActor
 */
void LE_AddToScene (void)
{
	le_t *le;
	entity_t ent;
	vec3_t modelOffset;
	int i;

	for (i = 0, le = LEs; i < numLEs; i++, le++) {
		if (le->inuse && !le->invis) {
			if (le->contents & CONTENTS_SOLID) {
				if (!((1 << cl_worldlevel->integer) & le->levelflags))
					continue;
			} else if (le->contents & CONTENTS_DETAIL) {
				/* show them always */
			} else if (le->pos[2] > cl_worldlevel->integer)
				continue;

			memset(&ent, 0, sizeof(ent));

			ent.alpha = le->alpha;
			ent.state = le->state;

			VectorCopy(le->angles, ent.angles);
			ent.model = le->model1;
			ent.skinnum = le->skinnum;

			switch (le->contents) {
			/* Only breakables do not use their origin; func_doors and func_rotating do!!!
			 * But none of them have animations. */
			case CONTENTS_SOLID:
			case CONTENTS_DETAIL: /* they use mins/maxs */
				break;
			default:
				/* set entity values */
				VectorCopy(le->origin, ent.origin);
				VectorCopy(le->origin, ent.oldorigin);

				/* do animation */
				R_AnimRun(&le->as, ent.model, cls.frametime * 1000);
				ent.as = le->as;
				break;
			}

			if (le->type == ET_DOOR || le->type == ET_ROTATING) {
				VectorCopy(le->angles, ent.angles);
				VectorCopy(le->origin, ent.origin);
				VectorCopy(le->origin, ent.oldorigin);
			}

			/**
			 * Offset the model to be inside the cursor box
			 * @todo Dunno if this is the best place to do it - what happens to
			 * shot-origin and stuff? le->origin is never changed.
			 */
			switch (le->fieldSize) {
			case ACTOR_SIZE_NORMAL:
			case ACTOR_SIZE_2x2:
				ModelOffset(le->fieldSize, modelOffset);
				VectorAdd(ent.origin, modelOffset, ent.origin);
				VectorAdd(ent.oldorigin, modelOffset, ent.oldorigin);
				break;
			default:
				break;
			}

			ent.lighting = &le->lighting;
			ent.lighting->dirty = qtrue;

			/* call add function */
			/* if it returns false, don't draw */
			if (le->addFunc)
				if (!le->addFunc(le, &ent))
					continue;

			/* add it to the scene */
			R_AddEntity(&ent);
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
	inventory_t inv;

	Com_DPrintf(DEBUG_CLIENT, "LE_Cleanup: Clearing up to %i unused LE inventories\n", numLEs);
	for (i = numLEs - 1, le = &LEs[numLEs - 1]; i >= 0; i--, le--) {
		switch (le->type) {
		case ET_ACTOR:
		case ET_ACTOR2x2:
			inv = le->i;
			INVSH_DestroyInventory(&inv);
			break;
		case ET_ITEM:
			INVSH_EmptyContainer(&le->i, &csi.ids[csi.idFloor]);
			break;
		case ET_PARTICLE:
			if (le->ptl) {
				CL_ParticleFree(le->ptl);
				le->ptl = NULL;
			}
			break;
		default:
			break;
		}
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
	for (i = 0, le = LEs; i < numLEs; i++, le++) {
		Com_Printf("#%5i | #%5i | %4i | %5i | %5i | %4i | %4i | %4i | %3i | %5i | %5i | ",
			i, le->entnum, le->type, le->inuse, le->invis, le->pnum, le->team,
			le->fieldSize, le->HP, le->state, le->levelflags);
		if (le->type == ET_PARTICLE) {
			if (le->ptl)
				Com_Printf("%s\n", le->ptl->ctrl->name);
			else
				Com_Printf("no ptl\n");
		} else if (le->model1)
			Com_Printf("%s\n", (char*)le->model1);
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
	for (i = 0, lm = LMs; i < numLMs; i++, lm++) {
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
	float *start, *end;
	trace_t trace;
	le_t *passle, *passle2;		/**< ignore these for clipping */
	int contentmask;			/**< search these in your trace - see MASK_* */
} moveclip_t;


/**
 * @brief Clip against solid entities
 * @sa CL_Trace
 * @sa SV_ClipMoveToEntities
 */
static void CL_ClipMoveToLEs (moveclip_t * clip)
{
	int i, tile = 0;
	le_t *le;
	trace_t trace;
	int headnode;
	cBspModel_t *model;
	const float *angles;
	vec3_t origin;

	if (clip->trace.allsolid)
		return;

	for (i = 0, le = LEs; i < numLEs; i++, le++) {
		if (!le->inuse || !(le->contents & clip->contentmask))
			continue;
		if (le == clip->passle || le == clip->passle2)
			continue;

		/* special case for bmodels */
		if (le->contents & CONTENTS_SOLID) {
			/* special value for bmodel */
			assert(le->modelnum1 < MAX_MODELS);
			model = cl.model_clip[le->modelnum1];
			if (!model) {
				Com_Printf("CL_ClipMoveToLEs: Error - le with no NULL bmodel (%i)\n", le->type);
				continue;
			}
			headnode = model->headnode;
			tile = model->tile;
			angles = le->angles;
		} else {
			/* might intersect, so do an exact clip */
			/** @todo make headnode = HullForLe(le, &tile) ... the counterpart of SV_HullForEntity in server/sv_world.c */
			headnode = CM_HeadnodeForBox(0, le->mins, le->maxs);
			angles = vec3_origin;
		}
		VectorCopy(le->origin, origin);

		assert(headnode < MAX_MAP_NODES);
		trace = CM_TransformedBoxTrace(clip->start, clip->end, clip->mins, clip->maxs, tile, headnode, clip->contentmask, 0, origin, angles);

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
 * @todo cl_worldlevel->integer should be a function parameter to eliminate sideeffects like e.g. in the particles code
 */
trace_t CL_Trace (vec3_t start, vec3_t end, const vec3_t mins, const vec3_t maxs, le_t * passle, le_t * passle2, int contentmask)
{
	moveclip_t clip;

	/* clip to world */
	clip.trace = TR_CompleteBoxTrace(start, end, mins, maxs, (1 << (cl_worldlevel->integer + 1)) - 1, contentmask, 0);
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
