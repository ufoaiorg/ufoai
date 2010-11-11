/**
 * @file e_event_actorshoot.c
 */

/*
Copyright (C) 2002-2010 UFO: Alien Invasion.

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

#include "../../../../client.h"
#include "../../../cl_localentity.h"
#include "../../../../renderer/r_mesh.h"
#include "../../../../renderer/r_mesh_anim.h"
#include "e_event_actorshoot.h"

int CL_ActorDoShootTime (const eventRegister_t *self, struct dbuffer *msg, const int dt)
{
#if 0
	const fireDef_t	*fd;
	static int impactTime;
	int flags, dummy;
	int objIdx, surfaceFlags;
	objDef_t *obj;
	int weap_fds_idx, fd_idx;
	shoot_types_t shootType;
	vec3_t muzzle, impact;

	if (impactTime < cl.time)
		impactTime = cl.time;

	/* read data */
	NET_ReadFormat(msg, self->formatString, &dummy, &dummy, &objIdx, &weap_fds_idx, &fd_idx, &shootType, &flags, &surfaceFlags, &muzzle, &impact, &dummy);

	obj = INVSH_GetItemByIDX(objIdx);
	fd = FIRESH_GetFiredef(obj, weap_fds_idx, fd_idx);

	if (!(flags & SF_BOUNCED)) {
		/* shooting */
		if (fd->speed && !CL_OutsideMap(impact, UNIT_SIZE * 10)) {
			impactTime = shootTime + 1000 * VectorDist(muzzle, impact) / fd->speed;
		} else {
			impactTime = shootTime;
		}
		if (cl.actTeam != cls.team)
			nextTime = impactTime + 1400;
		else
			nextTime = impactTime + 400;
		if (fd->delayBetweenShots)
			shootTime += 1000 / fd->delayBetweenShots;
	} else {
		/* only a bounced shot */
		eventTime = impactTime;
		if (fd->speed) {
			impactTime += 1000 * VectorDist(muzzle, impact) / fd->speed;
			nextTime = impactTime;
		}
	}
#else
	return cl.time;
#endif
}

/**
 * @brief Calculates the muzzle for the current weapon the actor is shooting with
 * @param[in] actor The actor that is shooting. Might not be @c NULL
 * @param[out] muzzle The muzzle vector to spawn the particle at. Might not be @c NULL. This is not
 * modified if there is no tag for the muzzle found for the weapon or item the actor has
 * in the hand (also see the given shoot type)
 * @param[in] shootType The shoot type to determine which tag of the actor should be used
 * to resolve the world coordinates. Also used to determine which item (or better which hand)
 * should be used to resolve the actor's item.
 */
static void CL_ActorGetMuzzle (const le_t* actor, vec3_t muzzle, shoot_types_t shootType)
{
	const struct model_s *model;
	const char *tag;
	const float *shooterTag, *muzzleTag;
	float matrix[16], mc[16], modifiedMatrix[16];
	const objDef_t* od;
	const invList_t *invlistWeapon;

	if (actor == NULL)
		return;

	if (IS_SHOT_RIGHT(shootType)) {
		tag = "tag_rweapon";
		invlistWeapon = RIGHT(actor);
	} else {
		tag = "tag_lweapon";
		invlistWeapon = LEFT(actor);
	}

	if (!invlistWeapon || !invlistWeapon->item.t)
		return;

	od = invlistWeapon->item.t;

	model = cls.modelPool[od->idx];
	if (!model)
		Com_Error(ERR_DROP, "Model for item %s is not precached", od->id);

	/* not every weapon has a muzzle tag assigned */
	muzzleTag = R_GetTagMatrix(model, "tag_muzzle");
	if (!muzzleTag)
		return;

	shooterTag = R_GetTagMatrix(actor->model1, tag);
	if (!shooterTag)
		Com_Error(ERR_DROP, "Could not find tag %s for actor model %s", tag, actor->model1->name);

	GLMatrixAssemble(actor->origin, actor->angles, mc);

	memcpy(modifiedMatrix, shooterTag, sizeof(modifiedMatrix));
	modifiedMatrix[12] = -modifiedMatrix[12];
	GLMatrixMultiply(mc, modifiedMatrix, matrix);

	memcpy(modifiedMatrix, muzzleTag, sizeof(modifiedMatrix));
	modifiedMatrix[12] = -modifiedMatrix[12];
	GLMatrixMultiply(matrix, modifiedMatrix, mc);

	muzzle[0] = mc[12];
	muzzle[1] = mc[13];
	muzzle[2] = mc[14];
}

/**
 * @brief Shoot with weapon.
 * @sa CL_ActorShoot
 * @sa CL_ActorShootHidden
 * @todo Improve detection of left- or right animation.
 * @sa EV_ACTOR_SHOOT
 */
void CL_ActorDoShoot (const eventRegister_t *self, struct dbuffer *msg)
{
	const fireDef_t *fd;
	le_t *leShooter, *leVictim;
	vec3_t muzzle, impact;
	int flags, normal, shooterEntnum, victimEntnum;
	int objIdx;
	int first;
	const objDef_t *obj;
	weaponFireDefIndex_t weapFdsIdx;
	fireDefIndex_t fdIdx;
	int surfaceFlags;
	shoot_types_t shootType;

	/* read data */
	NET_ReadFormat(msg, self->formatString, &shooterEntnum, &victimEntnum, &first, &objIdx, &weapFdsIdx, &fdIdx, &shootType, &flags, &surfaceFlags, &muzzle, &impact, &normal);

	if (victimEntnum != SKIP_LOCAL_ENTITY) {
		leVictim = LE_Get(victimEntnum);
		if (!leVictim)
			LE_NotFoundError(victimEntnum);
	} else {
		leVictim = NULL;
	}

	/* get shooter le */
	leShooter = LE_Get(shooterEntnum);

	/* get the fire def */
	obj = INVSH_GetItemByIDX(objIdx);
	fd = FIRESH_GetFiredef(obj, weapFdsIdx, fdIdx);

	CL_ActorGetMuzzle(leShooter, muzzle, shootType);

	/* add effect le */
	LE_AddProjectile(fd, flags, muzzle, impact, normal, leVictim);

	/* start the sound */
	if ((first || !fd->soundOnce) && fd->fireSound[0] && !(flags & SF_BOUNCED))
		S_LoadAndPlaySample(fd->fireSound, muzzle, fd->fireAttenuation, SND_VOLUME_WEAPONS);

	if (fd->irgoggles)
		refdef.rendererFlags |= RDF_IRGOGGLES;

	/* do actor related stuff */
	if (!leShooter)
		return; /* maybe hidden or inuse is false? */

	if (!LE_IsActor(leShooter))
		Com_Error(ERR_DROP, "Can't shoot, LE not an actor (type: %i)", leShooter->type);

	/* no animations for hidden actors */
	if (leShooter->type == ET_ACTORHIDDEN)
		return;

	if (LE_IsDead(leShooter)) {
		Com_DPrintf(DEBUG_CLIENT, "Can't shoot, actor dead or stunned.\n");
		return;
	}

	/* Animate - we have to check if it is right or left weapon usage. */
	if (IS_SHOT_RIGHT(shootType)) {
		R_AnimChange(&leShooter->as, leShooter->model1, LE_GetAnim("shoot", leShooter->right, leShooter->left, leShooter->state));
		R_AnimAppend(&leShooter->as, leShooter->model1, LE_GetAnim("stand", leShooter->right, leShooter->left, leShooter->state));
	} else if (IS_SHOT_LEFT(shootType)) {
		R_AnimChange(&leShooter->as, leShooter->model1, LE_GetAnim("shoot", leShooter->left, leShooter->right, leShooter->state));
		R_AnimAppend(&leShooter->as, leShooter->model1, LE_GetAnim("stand", leShooter->left, leShooter->right, leShooter->state));
	} else if (!IS_SHOT_HEADGEAR(shootType)) {
		/* no animation for headgear (yet) */
		Com_Error(ERR_DROP, "CL_ActorDoShoot: Invalid shootType given (entnum: %i, shootType: %i).\n", shootType, shooterEntnum);
	}
}
