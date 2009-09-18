/**
 * @file e_event_actorshoot.c
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

#include "../../../../client.h"
#include "../../../cl_localentity.h"
#include "../../../cl_actor.h"
#include "../../../cl_particle.h"
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
	int weap_fds_idx, fd_idx, shootType;
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
 * @brief Spawns particle effects for a hit actor.
 * @param[in] le The actor to spawn the particles for.
 * @param[in] impact The impact location (where the particles are spawned).
 * @param[in] normal The index of the normal vector of the particles (think: impact angle).
 * @todo Get real impact location and direction?
 */
static void CL_ActorHit (const le_t * le, vec3_t impact, int normal)
{
	if (!le) {
		Com_DPrintf(DEBUG_CLIENT, "CL_ActorHit: Can't spawn particles, LE doesn't exist\n");
		return;
	}

	if (!LE_IsActor(le)) {
		Com_Printf("CL_ActorHit: Can't spawn particles, LE is not an actor (type: %i)\n", le->type);
		return;
	}

	if (le->teamDef) {
		/* Spawn "hit_particle" if defined in teamDef. */
		if (le->teamDef->hitParticle[0] != '\0')
			CL_ParticleSpawn(le->teamDef->hitParticle, 0, impact, bytedirs[normal], NULL);
	}
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
	le_t *leShooter;
	vec3_t muzzle, impact;
	int flags, normal, shooterEntnum, victimEntnum;
	int objIdx;
	const objDef_t *obj;
	int weapFdsIdx, fdIdx, surfaceFlags, shootType;

	/* read data */
	NET_ReadFormat(msg, self->formatString, &shooterEntnum, &victimEntnum, &objIdx, &weapFdsIdx, &fdIdx, &shootType, &flags, &surfaceFlags, &muzzle, &impact, &normal);

	if (victimEntnum != SKIP_LOCAL_ENTITY) {
		const le_t *leVictim = LE_Get(victimEntnum);
		if (!leVictim)
			LE_NotFoundError(victimEntnum);

		CL_PlayActorSound(leVictim, SND_HURT);
	}

	/* get shooter le */
	leShooter = LE_Get(shooterEntnum);

	/* get the fire def */
	obj = INVSH_GetItemByIDX(objIdx);
	fd = FIRESH_GetFiredef(obj, weapFdsIdx, fdIdx);

	/* add effect le */
	LE_AddProjectile(fd, flags, muzzle, impact, normal);

	/* start the sound */
	/** @todo handle fd->soundOnce */
	if (fd->fireSound[0] && !(flags & SF_BOUNCED))
		S_PlaySample(muzzle, S_LoadSample(fd->fireSound), fd->fireAttenuation, SND_VOLUME_WEAPONS);

	if (fd->irgoggles)
		refdef.rendererFlags |= RDF_IRGOGGLES;

	/* do actor related stuff */
	if (!leShooter)
		return; /* maybe hidden or inuse is false? */

	if (!LE_IsActor(leShooter)) {
		Com_Printf("Can't shoot, LE not an actor (type: %i)\n", leShooter->type);
		return;
	}

	/* no animations for hidden actors */
	if (leShooter->type == ET_ACTORHIDDEN)
		return;

	/** Spawn blood particles (if defined) if actor(-body) was hit. Even if actor is dead :)
	 * Don't do it if it's a stun-attack though.
	 * @todo Special particles for stun attack (mind you that there is electrical and gas/chemical stunning)? */
	if ((flags & SF_BODY) && fd->obj->dmgtype != csi.damStunGas) {	/**< @todo && !(flags & SF_BOUNCED) ? */
		CL_ActorHit(leShooter, impact, normal);
	}

	if (LE_IsDead(leShooter)) {
		Com_Printf("Can't shoot, actor dead or stunned.\n");
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
