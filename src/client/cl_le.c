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

localModel_t LMs[MAX_LOCALMODELS];
int numLMs;


le_t LEs[MAX_EDICTS];
int numLEs;

static sfx_t* soundWaterIn;
static sfx_t* soundWaterOut;
static sfx_t* soundWaterMove;

/*===========================================================================
 LM handling
=========================================================================== */

static const char *lmList[MAX_LOCALMODELS + 1];


static void LM_GenerateList (void)
{
	localModel_t *lm;
	int i, l;

	l = 0;
	for (i = 0, lm = LMs; i < numLMs; i++, lm++)
		if (*lm->name == '*')
			lmList[l++] = lm->name;
	lmList[l] = NULL;
}

/**
 * @brief Add the local models (misc_model) to the scene
 * @sa V_RenderView
 * @sa LE_AddToScene
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
		memset(&ent, 0, sizeof(entity_t));
		VectorCopy(lm->origin, ent.origin);
		VectorCopy(lm->origin, ent.oldorigin);
		VectorCopy(lm->angles, ent.angles);
		ent.model = lm->model;
		ent.skinnum = lm->skin;
		ent.as.frame = lm->frame;
		if (lm->animname[0]) {
			ent.as = lm->as;
			/* do animation */
			R_AnimRun(&lm->as, ent.model, cls.frametime * 1000);
		}

		if (lm->flags & LMF_NOSMOOTH)
			ent.flags |= RF_NOSMOOTH;
		if (lm->flags & LMF_LIGHTFIXED) {
			ent.flags |= RF_LIGHTFIXED;
			ent.lightparam = lm->lightorigin;
			ent.lightcolor = lm->lightcolor;
			ent.lightambient = lm->lightambient;
		} else
			ent.lightparam = &lm->sunfrac;

		/* add it to the scene */
		V_AddEntity(&ent);
	}
}


static localModel_t *LM_Find (int num)
{
	int i;

	for (i = 0; i < numLMs; i++)
		if (LMs[i].num == num)
			return &LMs[i];

	return NULL;
}


static void LM_Delete (localModel_t * lm)
{
	localModel_t backup;

	backup = *lm;
	numLMs--;
	memcpy(lm, lm + 1, (numLMs - (lm - LMs)) * sizeof(localModel_t));

	LM_GenerateList();
	Grid_RecalcRouting(&clMap, backup.name, lmList);
	/* also recalc forbidden list for func_breakable and func_door e.g. */
	CL_ConditionalMoveCalc(&clMap, selActor, MAX_ROUTE);
}

/**
 * @brief Callback for EV_DOOR_CLOSE event
 */
void LM_DoorClose (struct dbuffer *msg)
{
	localModel_t *lm;
	le_t *le;
	int lmNum, entNum;
	vec3_t pos;
	vec3_t newOrigin;

	entNum = NET_ReadShort(msg);
	le = LE_Get(entNum);
	lmNum = NET_ReadShort(msg);
	lm = LM_Find(lmNum);
	if (!lm || !le) {
		NET_ReadPos(msg, pos);
		Com_Printf("Can't close the door - lm for mapNum %i not found or le (%i) not found\n", lmNum, entNum);
		return;
	} else {
		Com_Printf("Close the door\n");
	}
	le->contents = CONTENTS_SOLID;
	NET_ReadPos(msg, pos);
	VectorSubtract(pos, lm->origin, newOrigin);
	VectorCopy(newOrigin, lm->origin);
}

/**
 * @brief Callback for EV_DOOR_OPEN event
 */
void LM_DoorOpen (struct dbuffer *msg)
{
	localModel_t *lm;
	le_t *le;
	int lmNum, entNum;
	vec3_t pos;
	vec3_t newOrigin;

	entNum = NET_ReadShort(msg);
	le = LE_Get(entNum);
	lmNum = NET_ReadShort(msg);
	lm = LM_Find(lmNum);
	if (!lm || !le) {
		NET_ReadPos(msg, pos);
		Com_Printf("Can't open the door - lm for mapNum %i not found or le (%i) not found\n", lmNum, entNum);
		return;
	} else {
		Com_Printf("Open the door\n");
	}
	/* FIXME: don't trace against an opened door */
	/* NOTE: this is no func_door_rotating */
	le->contents = 0;
	/* FIXME: lm->origin is wrong here */
	NET_ReadPos(msg, pos);
	VectorSubtract(pos, lm->origin, newOrigin);
	VectorCopy(newOrigin, lm->origin);
	/*Print3Vector(lm->origin);*/
}

void LM_Perish (struct dbuffer *msg)
{
	localModel_t *lm;

	lm = LM_Find(NET_ReadShort(msg));
	if (!lm)
		return;

	LM_Delete(lm);
}


/**
 * @note e.g. func_breakable or func_door with health
 */
void LM_Explode (struct dbuffer *msg)
{
	localModel_t *lm;
	le_t *le;

	lm = LM_Find(NET_ReadShort(msg));
	if (!lm)
		return;

	/* now remove also the le to allow the tracing code to skip this le */
	/* func_breakable and func_door e.g. */
	le = LE_Get(NET_ReadShort(msg));
	if (le)
		le->inuse = qfalse;
	else
		Com_Error(ERR_DROP, "LM_Explode: Could not find le");

	if (lm->particle[0]) {
		cBspModel_t *mod;
		vec3_t center;

		/* create particles */
		mod = CM_InlineModel(lm->name);
		VectorAdd(mod->mins, mod->maxs, center);
		VectorScale(center, 0.5, center);
		CL_ParticleSpawn(lm->particle, 0, center, NULL, NULL);
	}

	LM_Delete(lm);
}


void CL_RegisterLocalModels (void)
{
	localModel_t *lm;
	vec3_t sunDir, sunOrigin;
	int i;

	VectorCopy(map_sun.dir, sunDir);

	for (i = 0, lm = LMs; i < numLMs; i++, lm++) {
		/* register the model and recalculate routing info */
		lm->model = R_RegisterModelShort(lm->name);
		if (lm->animname[0])
			R_AnimChange(&lm->as, lm->model, lm->animname);

		/* calculate sun lighting and register model if not yet done */
		VectorMA(lm->origin, 512, sunDir, sunOrigin);
		if (!CM_TestLine(lm->origin, sunOrigin))
			lm->sunfrac = 1.0f;
		else
			lm->sunfrac = 0.0f;
	}
}


/**
 * @sa CL_ParseEntitystring
 */
localModel_t *CL_AddLocalModel (const char *model, const char *particle, vec3_t origin, vec3_t angles, int num, int levelflags)
{
	localModel_t *lm;

	lm = &LMs[numLMs++];

	if (numLMs >= MAX_LOCALMODELS)
		Sys_Error("Too many local models\n");

	memset(lm, 0, sizeof(localModel_t));
	Q_strncpyz(lm->name, model, sizeof(lm->name));
	Q_strncpyz(lm->particle, particle, sizeof(lm->particle));
	VectorCopy(origin, lm->origin);
	VectorCopy(angles, lm->angles);
	lm->num = num;
	lm->levelflags = levelflags;

	LM_GenerateList();
	Grid_RecalcRouting(&clMap, lm->name, lmList);
/* 	Com_Printf("adding model %s %i\n", lm->name, numLMs); */

	return lm;
}

static const float mapBorder = UNIT_SIZE * 10;
static const float mapZBorder = -(UNIT_HEIGHT * 5);
/**
 * @brief Checks whether give position is still inside the map borders
 */
qboolean CL_OutsideMap (vec3_t impact)
{
	if (impact[0] < map_min[0] - mapBorder || impact[0] > map_max[0] + mapBorder)
		return qtrue;

	if (impact[1] < map_min[1] - mapBorder || impact[1] > map_max[1] + mapBorder)
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

	for (i = 0, le = LEs; i < numLEs; i++, le++)
		if (le->inuse && le->think)
			/* call think function */
			le->think(le);
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

	if (!anim)
		return "";

	mod = retAnim;

	/* add crouched flag */
	if (state & STATE_CROUCHED)
		*mod++ = 'c';

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
		Q_strncpyz(mod, anim, MAX_VAR);
		mod += strlen(anim);
		*mod++ = animationIndex;
		*mod++ = 0;
	} else {
		Q_strncpyz(mod, anim, MAX_VAR);
		Q_strcat(mod, "_", MAX_VAR);
		if (akimbo)
			Q_strcat(mod, "pistol_d", MAX_VAR);
		else
			Q_strcat(mod, type, MAX_VAR);
	}

	return retAnim;
}

/**
 * @sa LE_AddAmbientSound
 */
void LET_PlayAmbientSound (le_t * le)
{
	/* find the total contribution of all sounds of this type */
	/*int volume = S_SpatializeOrigin(le->origin, le->volume, le->attenuation);*/

}

/**
 * @brief Change the animation of an actor to the idle animation (which can be
 * panic, dead or stand)
 * @note Think function
 */
void LET_StartIdle (le_t * le)
{
	if (le->state & STATE_DEAD)
		R_AnimChange(&le->as, le->model1, va("dead%i", le->state & STATE_DEAD));
	else if (le->state & STATE_PANIC)
		R_AnimChange(&le->as, le->model1, "panic0");
	else
		R_AnimChange(&le->as, le->model1, LE_GetAnim("stand", le->right, le->left, le->state));

	/* keep this animation until something happens */
	le->think = NULL;
}

/**
 * @param[in] contents The contents flash of the brush we are currently in
 */
static void LE_PlaySoundFileForContents (le_t* le, int contents)
{
	if (!soundWaterIn)
		soundWaterIn = S_RegisterSound("footsteps/water_in");
	if (!soundWaterOut)
		soundWaterOut = S_RegisterSound("footsteps/water_out");
	if (!soundWaterMove)
		soundWaterOut = S_RegisterSound("footsteps/water_under");

	/* only play those water sounds when an actor jumps into the water - but not
	 * if he enters carefully in crouched mode */
	if (le->state & ~STATE_CROUCHED) {
		if (contents & CONTENTS_WATER) {
			/* were we already in the water? */
			if (le->positionContents & CONTENTS_WATER) {
				/* play water moving sound */
				S_StartSound(le->origin, soundWaterOut, DEFAULT_SOUND_PACKET_VOLUME, SOUND_DEFAULTATTENUATE);
			} else {
				/* play water entering sound */
				S_StartSound(le->origin, soundWaterIn, DEFAULT_SOUND_PACKET_VOLUME, SOUND_DEFAULTATTENUATE);
			}
			return;
		}

		if (le->positionContents & CONTENTS_WATER) {
			/* play water leaving sound */
			S_StartSound(le->origin, soundWaterMove, DEFAULT_SOUND_PACKET_VOLUME, SOUND_DEFAULTATTENUATE);
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
	sfx_t *sfx;
	const terrainType_t *t;
	vec3_t origin;

	t = Com_GetTerrainType(textureName);
	if (!t)
		return;

	/* origin might not be up-to-date here - but pos should be */
	PosToVec(le->pos, origin);

	/* @todo use the Grid_Fall method (ACTOR_SIZE_NORMAL) to ensure, that the particle is
	 * drawn at the ground (if needed - maybe the origin is already ground aligned)*/
	if (t->particle) {
		/* check whether actor is visibile */
		if (le->invis != qfalse)
			CL_ParticleSpawn(t->particle, 0, origin, NULL, NULL);
	}
	if (t->footStepSound) {
		sfx = S_RegisterSound(t->footStepSound);
		Com_DPrintf(DEBUG_SOUND, "LE_PlaySoundFileAndParticleForSurface: volume %.2f\n", t->footStepVolume);
		S_StartSound(origin, sfx, t->footStepVolume, t->footStepAttenuation);
	}
}

/**
 * @brief Searches the closest actor to the given world vector
 * @param[in] World position to get the closest actor to
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
		switch (le->type) {
		/* only visible actors */
		case ET_ACTOR:
		case ET_ACTOR2x2:
			VectorSubtract(origin, le->origin, leOrigin);
			tmp = VectorLength(leOrigin);
			if (tmp < dist) {
				actor = le;
				dist = tmp;
			}
			break;
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
	byte dv;
	float frac;
	int tuCost = 0;
	vec3_t start, dest, delta;
	trace_t trace;
	vec3_t from, to;

	/* check for start */
	if (cl.time <= le->startTime)
		return;

	/* move ahead */
	while (cl.time > le->endTime) {
		VectorCopy(le->pos, le->oldPos);

		if (le->pathPos < le->pathLength) {
			/* next part */
			dv = le->path[le->pathPos];
			PosAddDV(le->pos, dv);
			tuCost = Grid_MoveLength(&clMap, le->pos, qfalse) - Grid_MoveLength(&clMap, le->oldPos, qfalse);
			if (le->state & STATE_CROUCHED)
				tuCost *= 1.5;
			le->TU -= tuCost;
			if (le == selActor)
				actorMoveLength -= tuCost;

			/* walking in water will not play the normal footstep sounds */
			if (!le->pathContents[le->pathPos]) {
				/* prepare trace vectors */
				PosToVec(le->pos, from);
				VectorCopy(from, to);
				/* between these two we should really hit the ground */
				from[2] += UNIT_HEIGHT;
				to[2] -= UNIT_HEIGHT;

				trace = CL_Trace(from, to, vec3_origin, vec3_origin, NULL, NULL, MASK_SOLID);
				if (trace.surface)
					LE_PlaySoundFileAndParticleForSurface(le, trace.surface->name);
			} else
				LE_PlaySoundFileForContents(le, le->pathContents[le->pathPos]);

			le->dir = dv & (DIRECTIONS-1);
			le->angles[YAW] = dangle[le->dir];
			le->startTime = le->endTime;
			/* check for straight movement or diagonal movement */
			le->endTime += ((dv & (DIRECTIONS-1)) > 3 ? UNIT_SIZE * 1.41 : UNIT_SIZE) * 1000 / le->speed;

			le->positionContents = le->pathContents[le->pathPos];
			le->pathPos++;
		} else {
			/* end of move */
			le_t *floor;

			CL_ConditionalMoveCalc(&clMap, selActor, MAX_ROUTE);

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
	Grid_PosToVec(&clMap, le->pos, le->origin);
	Grid_PosToVec(&clMap, le->oldPos, start);
	Grid_PosToVec(&clMap, le->pos, dest);
	VectorSubtract(dest, start, delta);

	frac = (float) (cl.time - le->startTime) / (float) (le->endTime - le->startTime);

	VectorMA(start, frac, delta, le->origin);
}

/**
 * @note Think function
 */
void LET_StartPathMove (le_t * le)
{
	/* initial animation or animation change */
	R_AnimChange(&le->as, le->model1, LE_GetAnim("walk", le->right, le->left, le->state));

	le->think = LET_PathMove;
	le->think(le);
}

/**
 * @brief Autohides a projectile particle if no actor of your team can see it
 * @note Think function
 * @sa LET_Projectile
 * @sa CM_TestLine
 * @sa FrustomVis
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
		if (actors->team != cls.team || actors->state & STATE_DEAD)
			continue;
		if (actors->type == ET_ACTOR || actors->type == ET_ACTOR2x2) {
			/* at least one of our actors can see this */
			if (FrustomVis(actors->origin, actors->dir, le->origin)) {
				if (CM_TestLine(actors->origin, le->origin)) {
					if (!le->ptl)
						le->ptl = CL_ParticleSpawn(le->particleID, le->particleLevelFlags, le->origin, NULL, NULL);
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
			le->ptl = CL_ParticleSpawn(le->ref1, 0, impact, bytedirs[le->state], NULL);
			VecToAngles(bytedirs[le->state], le->ptl->angles);
		}
		if (le->ref2 && le->ref2[0]) {
			sfx_t *sfx = S_RegisterSound(le->ref2);
			S_StartSound(impact, sfx, le->fd->relImpactVolume, DEFAULT_SOUND_PACKET_ATTENUATION);
		}
	} else if (CL_OutsideMap(le->ptl->s)) {
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

void LE_AddProjectile (fireDef_t * fd, int flags, vec3_t muzzle, vec3_t impact, int normal, qboolean autohide)
{
	le_t *le;
	vec3_t delta;
	float dist;

	/* add le */
	le = LE_Add(0);
	le->invis = qtrue;
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
	le->state = normal;
	le->fd = fd;

	if (!fd->speed) {
		/* infinite speed projectile */
		ptl_t *ptl;
		sfx_t *sfx;

		le->inuse = qfalse;
		le->ptl->size[0] = dist;
		VectorMA(muzzle, 0.5, delta, le->ptl->s);
		if (flags & (SF_IMPACT | SF_BODY) || (fd->splrad && !fd->bounce)) {
			ptl = NULL;
			if (flags & SF_BODY) {
				if (fd->hitBodySound[0]) {
					sfx = S_RegisterSound(fd->hitBodySound);
					S_StartSound(le->origin, sfx, le->fd->relImpactVolume, DEFAULT_SOUND_PACKET_ATTENUATION);
				}
				if (fd->hitBody[0])
					ptl = CL_ParticleSpawn(fd->hitBody, 0, impact, bytedirs[normal], NULL);
			} else {
				if (fd->impactSound[0]) {
					sfx = S_RegisterSound(fd->impactSound);
					S_StartSound(le->origin, sfx, le->fd->relImpactVolume, DEFAULT_SOUND_PACKET_ATTENUATION);
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


void LE_AddGrenade (fireDef_t * fd, int flags, vec3_t muzzle, vec3_t v0, int dt)
{
	le_t *le;
	vec3_t accel;

	/* add le */
	le = LE_Add(0);
	le->invis = qtrue;

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
	le->state = 5;				/* direction (0,0,1) */
	le->fd = fd;
	assert(fd);
	le->think = LET_Projectile;
	le->think(le);
}

/**
 * @brief Adds ambient sounds from misc_sound entities
 * @sa CL_ParseEntitystring
 */
void LE_AddAmbientSound (const char *sound, vec3_t origin, float volume, float attenuation)
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
	le->sfx = sfx;
	le->attenuation = attenuation;
	le->volume = volume;
	le->inuse = qtrue;
	VectorCopy(origin, le->origin);
	le->invis = qtrue;
	le->think = LET_PlayAmbientSound;
	Com_DPrintf(DEBUG_SOUND, "Add ambient sound '%s'\n", sound);
}

/*===========================================================================
 LE Management functions
=========================================================================== */


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
		if (numLEs >= MAX_EDICTS - numLMs) {
			/* no free LEs */
			Com_Error(ERR_DROP, "Too many LEs");
			return NULL;
		}

		/* list isn't too long */
		numLEs++;
	}

	/* initialize the new LE */
	memset(le, 0, sizeof(le_t));
	le->inuse = qtrue;
	le->entnum = entnum;
	return le;
}

/**
 * @brief Searches all local entities for the one with the searched entnum
 * @param[in] entnum The entity number (server side)
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
	vec3_t sunOrigin;
	vec3_t modelOffset;
	int i;

	for (i = 0, le = LEs; i < numLEs; i++, le++) {
		if (le->inuse && !le->invis && le->pos[2] <= cl_worldlevel->value) {
			memset(&ent, 0, sizeof(entity_t));

			/* calculate sun lighting */
			if (!VectorCompare(le->origin, le->oldOrigin)) {
				VectorCopy(le->origin, le->oldOrigin);
				VectorMA(le->origin, 512, map_sun.dir, sunOrigin);
				if (!CM_TestLine(le->origin, sunOrigin))
					le->sunfrac = 1.0f;
				else
					le->sunfrac = 0.0f;
			}
			ent.lightparam = &le->sunfrac;
			ent.alpha = le->alpha;

			/* set entity values */
			VectorCopy(le->origin, ent.origin);
			VectorCopy(le->origin, ent.oldorigin);

			/**
			 * Offset the model to be inside the cursor box
			 * @todo Dunno if this is the best place to do it - what happens to shot-origin and stuff? le->origin is never changed.
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

			VectorCopy(le->angles, ent.angles);
			ent.model = le->model1;
			ent.skinnum = le->skinnum;

			/* do animation */
			R_AnimRun(&le->as, ent.model, cls.frametime * 1000);
			ent.as = le->as;

			/* call add function */
			/* if it returns false, don't draw */
			if (le->addFunc)
				if (!le->addFunc(le, &ent))
					continue;

			/* add it to the scene */
			V_AddEntity(&ent);
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
			INVSH_EmptyContainer(&le->i, csi.idFloor);
			break;
		case ET_PARTICLE:
			if (le->ptl)
				Mem_Free(le->ptl);
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

	Com_Printf("number | entnum | type | inuse | invis | pnum | team | size\n");
	for (i = 0, le = LEs; i < numLEs; i++, le++) {
		Com_Printf("#%5i | #%5i | %4i | %5i | %5i | %4i | %4i | %4i\n",
			i, le->entnum, le->type, le->inuse, le->invis, le->pnum, le->team, le->fieldSize);
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
	vec3_t mins2, maxs2;		/**< size when clipping against mosnters */
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
	cBspModel_t *cmodel;
	const float *angles;
	vec3_t origin;

	if (clip->trace.allsolid)
		return;

	for (i = 0, le = LEs; i < numLEs; i++, le++) {
		if (!le->inuse || !(le->contents & clip->contentmask))
			continue;
		if (le == clip->passle || le == clip->passle2)
			continue;

		if (le->type == ET_BREAKABLE || le->type == ET_DOOR) {
			/* special value for bmodel */
			assert(le->modelnum1 < MAX_MODELS);
			cmodel = cl.model_clip[le->modelnum1];
			if (!cmodel) {
				Com_Printf("CL_ClipMoveToLEs: Error - le with no NULL bmodel (%i)\n", le->type);
				continue;
			}
			headnode = cmodel->headnode;
			tile = cmodel->tile;
			angles = le->angles;
			/* bmodels don't have an origin (it's 0, 0, 0) - they use the pos instead */
			VectorCopy(le->pos, origin);
		} else {
			/* might intersect, so do an exact clip */
			/* @todo: make headnode = HullForLe(le, &tile) ... the counterpart of SV_HullForEntity in server/sv_world.c */
			headnode = CM_HeadnodeForBox(0, le->mins, le->maxs);
			angles = vec3_origin;
			VectorCopy(le->origin, origin);
		}

		assert(headnode < MAX_MAP_NODES);
		trace = CM_TransformedBoxTrace(clip->start, clip->end, clip->mins, clip->maxs, tile, headnode, clip->contentmask, origin, angles);

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
 * @sa CL_Trace
 */
static void CL_TraceBounds (const vec3_t start, const vec3_t mins, const vec3_t maxs, const vec3_t end, vec3_t boxmins, vec3_t boxmaxs)
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
 * @param[in] start Start vector to start the trace from
 * @param[in] end End vector to stop the trace at
 * @param[in] mins Bounding box used for tracing
 * @param[in] maxs Bounding box used for tracing
 * @param[in] passle Ignore this local entity in the trace (might be NULL)
 * @param[in] passle2 Ignore this local entity in the trace (might be NULL)
 * @param[in] contentmask Searched content the trace should watch for
 */
trace_t CL_Trace (vec3_t start, vec3_t end, const vec3_t mins, const vec3_t maxs, le_t * passle, le_t * passle2, int contentmask)
{
	moveclip_t clip;

	/* clip to world */
	clip.trace = CM_CompleteBoxTrace(start, end, mins, maxs, (1 << (cl_worldlevel->integer + 1)) - 1, contentmask);
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
