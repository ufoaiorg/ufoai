/**
 * @file cl_le.c
 * @brief Local entity managament.
 */

/*
Copyright (C) 2002-2006 UFO: Alien Invasion team.

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

lm_t LMs[MAX_LOCALMODELS];
int numLMs;


le_t LEs[MAX_EDICTS];
int numLEs;


/*===========================================================================
 LM handling
=========================================================================== */

static char *lmList[MAX_LOCALMODELS + 1];


/**
 * @brief
 */
static void LM_GenerateList(void)
{
	lm_t *lm;
	int i, l;

	l = 0;
	for (i = 0, lm = LMs; i < numLMs; i++, lm++)
		if (*lm->name == '*')
			lmList[l++] = lm->name;
	lmList[l] = NULL;
}


/**
 * @brief
 */
void LM_AddToScene(void)
{
	lm_t *lm;
	entity_t ent;
	int i;

	for (i = 0, lm = LMs; i < numLMs; i++, lm++) {
		/* check for visibility */
		if (!((1 << (int) cl_worldlevel->value) & lm->levelflags))
			continue;

		/* set entity values */
		memset(&ent, 0, sizeof(entity_t));
		VectorCopy(lm->origin, ent.origin);
		VectorCopy(lm->origin, ent.oldorigin);
		VectorCopy(lm->angles, ent.angles);
		ent.model = lm->model;
		ent.skinnum = lm->skin;
		if (lm->animname[0]) {
			ent.as = lm->as;
			/* do animation */
			re.AnimRun(&lm->as, ent.model, cls.frametime * 1000);
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


/**
 * @brief
 */
static lm_t *LM_Find(int num)
{
	int i;

	for (i = 0; i < numLMs; i++)
		if (LMs[i].num == num)
			return &LMs[i];

	Com_Printf("LM_Perish: Can't find model %i\n", num);
	return NULL;
}


/**
 * @brief
 */
static void LM_Delete(lm_t * lm)
{
	lm_t backup;

	backup = *lm;
	numLMs--;
	memcpy(lm, lm + 1, (numLMs - (lm - LMs)) * sizeof(lm_t));

	LM_GenerateList();
	Grid_RecalcRouting(&clMap, backup.name, lmList);
	/* TODO: before CL_ConditionalMoveCalc() was implemented, forbidden list wasn't recalculated here */
	CL_ConditionalMoveCalc(&clMap, selActor, MAX_ROUTE, fb_list, fb_length);
}


/**
 * @brief
 */
void LM_Perish(sizebuf_t * sb)
{
	lm_t *lm;

	lm = LM_Find(MSG_ReadShort(sb));
	if (!lm)
		return;

	LM_Delete(lm);
}


/**
 * @brief
 */
void LM_Explode(sizebuf_t * sb)
{
	lm_t *lm;

	lm = LM_Find(MSG_ReadShort(sb));
	if (!lm)
		return;

	if (lm->particle[0]) {
		cmodel_t *mod;
		vec3_t center;

		/* create particles */
		mod = CM_InlineModel(lm->name);
		VectorAdd(mod->mins, mod->maxs, center);
		VectorScale(center, 0.5, center);
		CL_ParticleSpawn(lm->particle, 0, center, NULL, NULL);
	}

	LM_Delete(lm);
}


/**
 * @brief
 */
void CL_RegisterLocalModels(void)
{
	lm_t *lm;
	vec3_t sunDir, sunOrigin;
	int i;

	VectorCopy(map_sun.dir, sunDir);

	for (i = 0, lm = LMs; i < numLMs; i++, lm++) {
		/* register the model and recalculate routing info */
		lm->model = re.RegisterModel(lm->name);
		if (lm->animname[0])
			re.AnimChange(&lm->as, lm->model, lm->animname);

		/* calculate sun lighting and register model if not yet done */
		VectorMA(lm->origin, 512, sunDir, sunOrigin);
		if (!CM_TestLine(lm->origin, sunOrigin))
			lm->sunfrac = 1.0f;
		else
			lm->sunfrac = 0.0f;
	}
}


/**
 * @brief
 */
lm_t *CL_AddLocalModel(char *model, char *particle, vec3_t origin, vec3_t angles, int num, int levelflags)
{
	lm_t *lm;

	lm = &LMs[numLMs++];

	if (numLMs >= MAX_LOCALMODELS)
		Sys_Error("Too many local models\n");

	memset(lm, 0, sizeof(lm_t));
	Q_strncpyz(lm->name, model, MAX_VAR);
	Q_strncpyz(lm->particle, particle, MAX_VAR);
	VectorCopy(origin, lm->origin);
	VectorCopy(angles, lm->angles);
	lm->num = num;
	lm->levelflags = levelflags;

	LM_GenerateList();
	Grid_RecalcRouting(&clMap, lm->name, lmList);
	/*  Com_Printf( "adding model %s %i\n", lm->name, numLMs ); */

	return lm;
}


/*===========================================================================
LE thinking
=========================================================================== */

/**
 * @brief Checks whether there are soldiers alive if not - end round automatically
 */
void LE_Status(void)
{
	le_t *le;
	int i;
	qboolean endRound = qtrue;

	if (!numLEs)
		return;

	/* only multiplayer - but maybe not our round? */
	if ((int) Cvar_VariableValue("maxclients") > 1 && cls.team != cl.actTeam)
		return;

	for (i = 0, le = LEs; i < numLEs; i++, le++)
		if (le->inuse && le->team == cls.team && !(le->state & STATE_DEAD))
			/* call think function */
			endRound = qfalse;

	/* ok, no players alive in multiplayer - end this round automatically */
	if (endRound) {
		CL_NextRound();
		Com_Printf("End round automatically - no soldiers left\n");
	}
}

/**
 * @brief Calls the le think function
 * @sa LET_StartIdle
 * @sa LET_PathMove
 * @sa LET_StartPathMove
 * @sa LET_Projectile
 */
void LE_Think(void)
{
	le_t *le;
	int i;

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
 * @brief
 */
char *LE_GetAnim(char *anim, int right, int left, int state)
{
	char *mod;
	qboolean akimbo;
	char category, *type;

	if (!anim)
		return "";

	mod = retAnim;

	/* add crouched flag */
	if (state & STATE_CROUCHED)
		*mod++ = 'c';

	/* determine relevant data */
	akimbo = qfalse;
	if (right == NONE) {
		category = '0';
		if (left == NONE)
			type = "item";
		else {
			/* left hand grenades look OK with deafult anim; others don't */
			if (Q_strncmp(csi.ods[left].type, "grenade", 7))
				akimbo = qtrue;
			type = csi.ods[left].type;
		}
	} else {
		category = csi.ods[right].category;
		type = csi.ods[right].type;
		if (left != NONE && !Q_strncmp(csi.ods[right].type, "pistol", 6) && !Q_strncmp(csi.ods[left].type, "pistol", 6))
			akimbo = qtrue;
	}

	if (!Q_strncmp(anim, "stand", 5) || !Q_strncmp(anim, "walk", 4)) {
		Q_strncpyz(mod, anim, MAX_VAR);
		mod += strlen(anim);
		*mod++ = category;
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
 * @brief
 * @note Think function
 */
void LET_StartIdle(le_t * le)
{
	if (le->state & STATE_DEAD)
		re.AnimChange(&le->as, le->model1, va("dead%i", le->state & STATE_DEAD));
	else if (le->state & STATE_PANIC)
		re.AnimChange(&le->as, le->model1, "panic0");
	else
		re.AnimChange(&le->as, le->model1, LE_GetAnim("stand", le->right, le->left, le->state));

	le->think = NULL;
}


/**
 * @brief
 * @note Think function
 */
static void LET_PathMove(le_t * le)
{
	byte dv;
	float frac;
	vec3_t start, dest, delta;

	/* check for start */
	if (cl.time <= le->startTime)
		return;

	/* move ahead */
	while (cl.time > le->endTime) {
		VectorCopy(le->pos, le->oldPos);

		if (le->pathPos < le->pathLength) {
			/* next part */
			dv = le->path[le->pathPos++];
			PosAddDV(le->pos, dv);
			le->dir = dv & 7;
			le->angles[YAW] = dangle[le->dir];
			le->startTime = le->endTime;
			le->endTime += ((dv & 7) > 3 ? UNIT_SIZE * 1.41 : UNIT_SIZE) * 1000 / le->speed;
			if (le->team == cls.team && le == selActor && (int) cl_worldlevel->value == le->oldPos[2] && le->pos[2] != le->oldPos[2]) {
				Cvar_SetValue("cl_worldlevel", le->pos[2]);
			}
		} else {
			/* end of move */
			le_t *floor;

			CL_ConditionalMoveCalc(&clMap, selActor, MAX_ROUTE, fb_list, fb_length);
			CL_ResetActorMoveLength();

			floor = LE_Find(ET_ITEM, le->pos);
			if (floor)
				FLOOR(le) = FLOOR(floor);

			blockEvents = qfalse;
			le->think = LET_StartIdle;
			le->think(le);
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
 * @brief
 * @note Think function
 */
void LET_StartPathMove(le_t * le)
{
	re.AnimChange(&le->as, le->model1, LE_GetAnim("walk", le->right, le->left, le->state));

	le->think = LET_PathMove;
	le->think(le);
}


/**
 * @brief
 * @note Think function
 */
static void LET_Projectile(le_t * le)
{
	if (cl.time >= le->endTime) {
		CL_ParticleFree(le->ptl);
		le->inuse = qfalse;
		if (le->ref1 && le->ref1[0]) {
			vec3_t impact;

			VectorCopy(le->ptl->s, impact);
			le->ptl = CL_ParticleSpawn(le->ref1, 0, impact, bytedirs[le->state], NULL);
			VecToAngles(bytedirs[le->state], le->ptl->angles);
		}
		if (le->ref2 && le->ref2[0])
			S_StartLocalSound(le->ref2);
	}
}

/*===========================================================================
 LE Special Effects
=========================================================================== */

/**
 * @brief
 */
void LE_AddProjectile(fireDef_t * fd, int flags, vec3_t muzzle, vec3_t impact, int normal)
{
	le_t *le;
	vec3_t delta;
	float dist;

	/* add le */
	le = LE_Add(0);
	le->invis = qtrue;

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

	if (!fd->speed) {
		/* infinite speed projectile */
		ptl_t *ptl;

		le->inuse = qfalse;
		le->ptl->size[0] = dist;
		VectorMA(muzzle, 0.5, delta, le->ptl->s);
		if (flags & (SF_IMPACT | SF_BODY) || (fd->splrad && !fd-> bounce)) {
			ptl = NULL;
			if (flags & SF_BODY) {
				if (fd->hitBodySound[0])
					S_StartLocalSound(fd->hitBodySound);
				if (fd->hitBody[0])
					ptl = CL_ParticleSpawn(fd->hitBody, 0, impact, bytedirs[normal], NULL);
			} else {
				if (fd->impactSound[0])
					S_StartLocalSound(fd->impactSound);
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
 * @brief
 */
void LE_AddGrenade(fireDef_t * fd, int flags, vec3_t muzzle, vec3_t v0, int dt)
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
	le->think = LET_Projectile;
	le->think(le);
}


/*===========================================================================
 LE Management functions
=========================================================================== */


/**
 * @brief
 */
le_t *LE_Add(int entnum)
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
			Com_Error(ERR_DROP, "Too many LEs\n");
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
 * @brief
 */
le_t *LE_Get(int entnum)
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
 * @brief
 */
le_t *LE_Find(int type, pos3_t pos)
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


/**
 * @brief
 */
void LE_AddToScene(void)
{
	le_t *le;
	entity_t ent;
	vec3_t sunOrigin;
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
			VectorCopy(le->angles, ent.angles);
			ent.model = le->model1;
			ent.skinnum = le->skinnum;

			/* do animation */
			re.AnimRun(&le->as, ent.model, cls.frametime * 1000);
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
 */
void LE_Cleanup(void)
{
    int i;
    le_t *le;
    inventory_t inv;

    Com_DPrintf("LE_Cleanup: Clearing up to %i unused LE inventories\n", numLEs);
    for (i = numLEs - 1, le = &LEs[numLEs - 1]; i >= 0; i--, le--) {
        switch (le->type) {
        case ET_ACTOR:
        case ET_UGV:
			inv = le->i;
            Com_DestroyInventory(&inv);
            break;
        case ET_ITEM:
			Com_EmptyContainer(&le->i, csi.idFloor);
            break;
        default:
            break;
        }
    }
}


/*===========================================================================
 LE Tracing
=========================================================================== */


typedef struct {
	vec3_t boxmins, boxmaxs;	/* enclose the test object along entire move */
	float *mins, *maxs;			/* size of the moving object */
	vec3_t mins2, maxs2;		/* size when clipping against mosnters */
	float *start, *end;
	trace_t trace;
	le_t *passle;
	int contentmask;
} moveclip_t;


/**
 * @brief
 */
static void CL_ClipMoveToLEs(moveclip_t * clip)
{
	int i;
	le_t *le;
	trace_t trace;
	int headnode;

	if (clip->trace.allsolid)
		return;

	for (i = 0, le = LEs; i < numLEs; i++, le++) {
		if (!le->inuse || !(le->contents & clip->contentmask))
			continue;
		if (le == clip->passle)
			continue;

		/* might intersect, so do an exact clip */
		/* TODO: make headnode = CM_HullForLe(le, &tile) ... the counterpart of SV_HullForEntity in server/sv_world.c */
		headnode = CM_HeadnodeForBox(0, le->mins, le->maxs);

		trace = CM_TransformedBoxTrace(clip->start, clip->end, clip->mins, clip->maxs, 0, headnode, clip->contentmask, le->origin, vec3_origin);

		if (trace.allsolid || trace.startsolid || trace.fraction < clip->trace.fraction) {
			trace.le = le;
			clip->trace = trace;
			if (clip->trace.startsolid)
				clip->trace.startsolid = qtrue;
		} else if (trace.startsolid)
			clip->trace.startsolid = qtrue;
	}
}


/**
 * @brief
 * @sa CL_Trace
 */
static void CL_TraceBounds(vec3_t start, vec3_t mins, vec3_t maxs, vec3_t end, vec3_t boxmins, vec3_t boxmaxs)
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
 */
trace_t CL_Trace(vec3_t start, vec3_t end, vec3_t mins, vec3_t maxs, le_t * passle, int contentmask)
{
	moveclip_t clip;

	/* clip to world */
	clip.trace = CM_CompleteBoxTrace(start, end, mins, maxs, (1 << ((int) cl_worldlevel->value + 1)) - 1, contentmask);
	clip.trace.le = NULL;
	if (clip.trace.fraction == 0)
		return clip.trace;		/* blocked by the world */

	clip.contentmask = contentmask;
	clip.start = start;
	clip.end = end;
	clip.mins = mins;
	clip.maxs = maxs;
	clip.passle = passle;

	/* create the bounding box of the entire move */
	CL_TraceBounds(start, mins, maxs, end, clip.boxmins, clip.boxmaxs);

	/* clip to other solid entities */
	CL_ClipMoveToLEs(&clip);

	return clip.trace;
}
