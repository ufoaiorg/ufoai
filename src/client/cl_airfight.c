/**
 * @file cl_airfight.c
 * @brief Airfight related stuff.
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
#include "cl_global.h"

int numBullets = 0;			/**< Number of bunch of bullets on geoscape */
vec2_t bulletPos[MAX_BULLETS_ON_GEOSCAPE][BULLETS_PER_SHOT];	/**< Position of every bullets on geoscape */

/**
 * @brief Create bullets on geoscape.
 * @param[in] projectile Pointer to the projectile corresponding to this bullet.
 */
static void AIRFIGHT_CreateBullets (aircraftProjectile_t *projectile)
{
	int i;

	if (numBullets >= MAX_BULLETS_ON_GEOSCAPE) {
		Com_Printf("AIRFIGHT_CreateBullets: array bulletPos is full, no more bullets can be added on geoscape\n");
		return;
	}

	projectile->bulletIdx = numBullets;

	for (i = 0; i < BULLETS_PER_SHOT; i++) {
		bulletPos[numBullets][i][0] = projectile->pos[0] + (frand() - 0.5f) * 2;
		bulletPos[numBullets][i][1] = projectile->pos[1] + (frand() - 0.5f) * 2;
	}

	numBullets++;
}

/**
 * @brief Destroy bullets on geoscape.
 * @param[in] projectile Pointer to the projectile corresponding to this bullet.
 */
static void AIRFIGHT_DestroyBullets (aircraftProjectile_t *projectile)
{
	int i, k;

	if (projectile->bulletIdx < 0)
		return;

	for (k = projectile->bulletIdx; k < numBullets - 1; k++) {
		for (i = 0; i < BULLETS_PER_SHOT; i++) {
			bulletPos[k][i][0] = bulletPos[k + 1][i][0];
			bulletPos[k][i][1] = bulletPos[k + 1][i][1];
		}
	}

	for (k = 0; k < gd.numProjectiles; k++) {
		if (gd.projectiles[k].bulletIdx > projectile->bulletIdx)
			gd.projectiles[k].bulletIdx--;
	}

	numBullets--;
}

/**
 * @brief Run bullets on geoscape.
 * @param[in] projectile Pointer to the projectile corresponding to this bullet.
 * @param[in] ortogonalVector vector perpendicular to the movement of the projectile.
 * @param[in] movement how much each bullet should move toward its target.
 */
static void AIRFIGHT_RunBullets (aircraftProjectile_t *projectile, vec3_t ortogonalVector, float movement)
{
	int i;
	vec3_t startPoint, finalPoint;

	if (projectile->bulletIdx < 0)
		return;

	for (i = 0; i < BULLETS_PER_SHOT; i++) {
		PolarToVec(bulletPos[projectile->bulletIdx][i], startPoint);
		RotatePointAroundVector(finalPoint, ortogonalVector, startPoint, movement);
		VecToPolar(finalPoint, bulletPos[projectile->bulletIdx][i]);
	}
}

/**
 * @brief Remove a projectile from gd.projectiles
 * @param[in] idx of the projectile to remove in array gd.projectiles[]
 */
static qboolean AIRFIGHT_RemoveProjectile (int idx)
{
	int i = 0;

#ifdef DEBUG
	if (idx > gd.numProjectiles - 1) {
		Com_Printf("Tried to remove an unexisting idx in array gd.numProjectiles\n");
		return qfalse;
	}
#endif

	AIRFIGHT_DestroyBullets(gd.projectiles + idx);

	for (i = idx; i < gd.numProjectiles; i++) {
		gd.projectiles[i] = gd.projectiles[i+1];
	}
	gd.numProjectiles--;

	return qtrue;
}

/**
 * @brief Add a projectile in gd.projectiles
 * @param[in] attackingBaseIdx idx of the attacking base in gd.bases[]. -1 is the attacker is an aircraft.
 * @param[in] attacker Pointer to the attacking aircraft
 * @param[in] aimedBaseIdx idx of the aimed base (-1 if the target is an aircraft)
 * @param[in] target Pointer to the target aircraft
 * @param[in] weaponSlot Pointer to the weapon slot that fires the projectile.
 * @note we already checked in AIRFIGHT_ChooseWeapon that the weapon has still ammo
 */
static qboolean AIRFIGHT_AddProjectile (int attackingBaseIdx, aircraft_t *attacker, int aimedBaseIdx, aircraft_t *target, aircraftSlot_t *weaponSlot)
{
	aircraftProjectile_t *projectile;

	if (gd.numProjectiles >= MAX_PROJECTILESONGEOSCAPE) {
		Com_Printf("Too many projectiles on map\n");
		return qfalse;
	}

	projectile = &gd.projectiles[gd.numProjectiles];

	projectile->idx = gd.numProjectiles;
	projectile->aircraftItemsIdx = weaponSlot->ammoIdx;
	if (attackingBaseIdx == -1) {
		assert(attacker);
		projectile->attackingAircraft = attacker;
		VectorSet(projectile->pos, attacker->pos[0], attacker->pos[1], 0);
	} else {
		projectile->attackingAircraft = NULL;
		VectorSet(projectile->pos, gd.bases[attackingBaseIdx].pos[0], gd.bases[attackingBaseIdx].pos[1], 0);
	}
	if (aimedBaseIdx == -1) {
		assert(target);
		projectile->aimedAircraft = target;
		projectile->aimedBaseIdx = -1;
		VectorSet(projectile->idleTarget, 0, 0, 0);
	} else {
		projectile->aimedAircraft = NULL;
		projectile->aimedBaseIdx = aimedBaseIdx;
		VectorSet(projectile->idleTarget, gd.bases[aimedBaseIdx].pos[0], gd.bases[aimedBaseIdx].pos[1], 0);
	}
	projectile->time = 0;
	projectile->angle = 0.0f;
	projectile->bulletIdx = -1;

	/* @todo; imporve this test */
	if (!Q_strncmp(aircraftItems[weaponSlot->itemIdx].id, "craft_weapon_shiva", MAX_VAR))
		AIRFIGHT_CreateBullets(projectile);

	weaponSlot->ammoLeft -= 1;
	gd.numProjectiles++;

	return qtrue;
}

/**
 * @brief Change destination of projectile to an idle point of the map, close to its former target.
 * @param[in] idx idx of the projectile to update in gd.projectiles[].
 */
static void AIRFIGHT_MissTarget (int idx)
{
	aircraft_t *oldTarget;
	aircraftProjectile_t *projectile;

	projectile = &gd.projectiles[idx];
	assert(projectile);

	oldTarget = projectile->aimedAircraft;
	if (oldTarget) {
		VectorSet(projectile->idleTarget, oldTarget->pos[0] + 10 * frand() - 5, oldTarget->pos[1] + 10 * frand() - 5, 0);
		projectile->aimedAircraft = NULL;
	} else
		VectorSet(projectile->idleTarget, projectile->idleTarget[0] + 10 * frand() - 5, projectile->idleTarget[1] + 10 * frand() - 5, 0);
}

/**
 * @brief Choose the weapon an attacking aircraft will use to fire on a target.
 * @param[in] slot Pointer to the first weapon slot of attacking base or aircraft.
 * @param[in] maxSlot maximum number of weapon slots in attacking base or aircraft.
 * @param[in] pos position of attacking base or aircraft.
 * @param[in] targetpos Pointer to the aimed aircraft.
 * @return indice of the slot to use (in array weapons[]), -1 if no weapon to use atm, -2 if no weapon to use at all (no ammo left).
 */
static int AIRFIGHT_ChooseWeapon (aircraftSlot_t *slot, int maxSlot, vec3_t pos, vec3_t targetPos)
{
	int slotIdx = -2;
	int i, ammoIdx;
	float distance0, distance=99999.9f;

	distance0 = distance;

	assert(slot);

	distance = CP_GetDistance(pos, targetPos);

	/* We choose the usable weapon with the smaller range */
	for (i = 0; i < maxSlot; i++) {
		/* check if there is a functional weapon in this slot */
		if (slot[i].itemIdx < 0 || slot[i].installationTime != 0)
			continue;

		/* check if there is still ammo in this weapon */
		if (slot[i].ammoIdx < 0 || slot[i].ammoLeft <= 0)
			continue;

		if (slotIdx == -2)
			slotIdx = -1;

		ammoIdx = slot[i].ammoIdx;
		/* check if the target is within range of this weapon */
		if (distance > aircraftItems[ammoIdx].stats[AIR_STATS_WRANGE])
			continue;

		/* check if weapon is reloaded */
		if (slot[i].delayNextShot > 0)
			continue;

		if (distance < distance0) {
			slotIdx = i;
			distance0 = distance;
		}
	}
	return slotIdx;
}

/**
 * @brief Calculate the probability to hit the ennemy.
 * @param[in] shooter Pointer to the attacking aircraft (may be NULL if a base fires the projectile).
 * @param[in] target Pointer to the aimed aircraft (may be NULL if a target is a base).
 * @param[in] slot Slot containing the weapon firing.
 * @return Probability to hit the target (0 when you don't have a chance, 1 (or more) when you're sure to hit).
 * @note that modifiers due to electronics, weapons, and shield are already taken into account in AII_UpdateAircraftStats
 * @sa AII_UpdateAircraftStats
 * @sa AIRFIGHT_ExecuteActions
 * @sa AIRFIGHT_ChooseWeapon
 * @pre slotIdx must have a weapon installed, with ammo available (see AIRFIGHT_ChooseWeapon)
 * @todo This probability should also depend on the pilot skills, when they will be implemented.
 */
static float AIRFIGHT_ProbabilityToHit (aircraft_t *shooter, aircraft_t *target, aircraftSlot_t *slot)
{
	int idx;
	float probability = 0.0f;

	idx = slot->itemIdx;
	if (idx < 0) {
		Com_Printf("AIRFIGHT_ProbabilityToHit: no weapon assigned to attacking aircraft\n");
		return probability;
	}

	idx = slot->ammoIdx;
	if (idx < 0) {
		Com_Printf("AIRFIGHT_ProbabilityToHit: no ammo in weapon of attacking aircraft\n");
		return probability;
	}

	/* Take Base probability from the ammo of the attacking aircraft */
	probability = aircraftItems[idx].stats[AIR_STATS_ACCURACY];
	Com_DPrintf("AIRFIGHT_ProbabilityToHit: Base probablity: %f\n", probability);

	/* Modify this probability by items of the attacking aircraft (stats is in percent) */
	if (shooter)
		probability *= shooter->stats[AIR_STATS_ACCURACY] / 100.0f;

	/* Modify this probability by items of the aimed aircraft (stats is in percent) */
	if (target)
		probability /= target->stats[AIR_STATS_ECM] / 100.0f;

	Com_DPrintf("AIRFIGHT_ProbabilityToHit: Probability to hit: %f\n", probability);
	return probability;
}

/**
 * @brief Decide what an attacking aircraft can do.
 * @param[in] shooter The aircraft we attack with.
 * @param[in] target The ufo we are going to attack.
 * @todo Implement me and display an attack popup.
 */
void AIRFIGHT_ExecuteActions (aircraft_t* shooter, aircraft_t* target)
{
	int ammoIdx, slotIdx;
	float probability;

	/* some asserts */
	assert(shooter);

	if (shooter->baseTargetIdx == -1) {
		assert(target);

		/* Check if the attacking aircraft can shoot */
		slotIdx = AIRFIGHT_ChooseWeapon(shooter->weapons, shooter->maxWeapons, shooter->pos, target->pos);
	} else
		slotIdx = AIRFIGHT_ChooseWeapon(shooter->weapons, shooter->maxWeapons, shooter->pos, gd.bases[shooter->baseTargetIdx].pos);

	if (slotIdx > -1) {
		ammoIdx = shooter->weapons[slotIdx].ammoIdx;

		/* shoot */
		if (AIRFIGHT_AddProjectile(-1, shooter, shooter->baseTargetIdx, target, shooter->weapons + slotIdx)) {
			shooter->weapons[slotIdx].delayNextShot = aircraftItems[ammoIdx].weaponDelay;
			/* will we miss the target ? */
			probability = frand();
			if (probability > AIRFIGHT_ProbabilityToHit(shooter, target, shooter->weapons + slotIdx))
				AIRFIGHT_MissTarget(gd.numProjectiles - 1);
		}
	} else if (slotIdx > -2) {
		/* no ammo to fire atm (too far or reloading), pursue target */
		if (shooter->type == AIRCRAFT_UFO) {
			if (shooter->baseTargetIdx == -1)
				AIR_SendUfoPurchasingAircraft(shooter, target);
			else
				/* ufo is attacking a base */
				return;
		} else
			AIR_SendAircraftPurchasingUfo(shooter, target);
	} else {
		/* no ammo left, or no weapon, you should flee ! */
		if (shooter->type == AIRCRAFT_UFO) {
			if (shooter->baseTargetIdx == -1)
				UFO_FleePhalanxAircraft(shooter, target->pos);
			else if (gd.bases[shooter->baseTargetIdx].hasMissile || gd.bases[shooter->baseTargetIdx].hasLaser)
				UFO_FleePhalanxAircraft(shooter, gd.bases[shooter->baseTargetIdx].pos);
		} else
			AIR_AircraftReturnToBase(shooter);
	}
}

/**
 * @brief Check if a base can fire on an aircraft.
 * @note should be used against ufos.
 * @param[in] target Pointer to the aircraft that could be attacked.
 */
extern void AIRFIGHT_BaseShootUFO (aircraft_t *target) {
	base_t*		base;
	int slotIdx;

	for (base = gd.bases; base < (gd.bases + gd.numBases); base++) {
		slotIdx = AIRFIGHT_ChooseWeapon(base->batteries, base->maxBatteries, base->pos, target->pos);
		if (slotIdx > -1)
			if (AIRFIGHT_AddProjectile(base->idx, NULL, -1, target, base->batteries + slotIdx)) {
				base->batteries[slotIdx].delayNextShot = aircraftItems[base->batteries[slotIdx].ammoIdx].weaponDelay;
				/* will we miss the target ? */
				if (frand() > AIRFIGHT_ProbabilityToHit(NULL, target, base->batteries + slotIdx))
					AIRFIGHT_MissTarget(gd.numProjectiles - 1);
			}
	}
}

/**
 * @brief Set all projectile aiming a given aircraft to an idle destination.
 * @param[in] aircraft Pointer to the aimed aircraft.
 * @note This function is called when @c aircraft is destroyed.
 * @sa AIRFIGHT_ActionsAfterAirfight
 */
static void AIRFIGHT_RemoveProjectileAimingAircraft (aircraft_t * aircraft)
{
	aircraftProjectile_t *projectile;
	aircraft_t *target;
	int idx;

	for (idx = 0, projectile = gd.projectiles; idx < gd.numProjectiles; projectile++, idx++) {
		target = projectile->aimedAircraft;

		if (target && target->idx == aircraft->idx)
			AIRFIGHT_MissTarget(idx);
	}
}

/**
 * @brief Actions to execute when a fight is done.
 * @param[in] shooter Pointer to the aircraft that fired the projectile.
 * @param[in] aircraft Pointer to the aircraft which was destroyed (alien or phalanx).
 * @param[in] battleStatus qtrue if PHALANX won, qfalse if UFO won.
 * @note Some of these mission values are redone (and not reloaded) in CP_Load
 * @note shooter may be NULL
 * @sa UFO_DestroyAllUFOsOnGeoscape_f
 * @sa CP_Load
 */
void AIRFIGHT_ActionsAfterAirfight (aircraft_t *shooter, aircraft_t* aircraft, qboolean phalanxWon)
{
	mission_t* ms = NULL;
	actMis_t* mis = NULL;
	byte *color;
	const char *zoneType = NULL;
	char missionName[MAX_VAR];
	const nation_t *nation = NULL;

	if (phalanxWon) {
		/* get the color value of the map at the crash position */
		color = CL_GetMapColor(aircraft->pos, MAPTYPE_CLIMAZONE);
		/* if this color value is not the value for water ...
		 * and we hit the probability to spawn a crashsite mission */
		if (!MapIsWater(color)) {
			/* spawn new mission */
			/* some random data like alien race, alien count (also depends on ufo-size) */
			/* @todo: We should have a ufo crash theme for random map assembly */
			/* @todo: call the map assembly theme with the right parameter, e.g.: +ufocrash [climazone] */
			zoneType = MAP_GetZoneType(color);
			Com_sprintf(missionName, sizeof(missionName), "ufocrash%.0f:%.0f", aircraft->pos[0], aircraft->pos[1]);
			ms = CL_AddMission(missionName);
			if (!ms) {
				MN_AddNewMessage(_("Interception"), _("UFO interception succesful -- UFO lost."), qfalse, MSG_CRASHSITE, NULL);
				return;
			}
			ms->missionType = MIS_CRASHSITE;
			/* the size if somewhat random, because not all map tiles would have
			 * alien spawn points */
			ms->aliens = aircraft->size;
			/* 1-4 civilians */
			ms->civilians = (frand() * 10);
			ms->civilians %= 4;
			ms->civilians += 1;

			/* realPos is set below */
			Vector2Set(ms->pos, aircraft->pos[0], aircraft->pos[1]);

			nation = MAP_GetNation(ms->pos);
			Com_sprintf(ms->type, sizeof(ms->type), _("UFO crash site"));
			if (nation) {
				Com_sprintf(ms->location, sizeof(ms->location), _(nation->name));
			} else {
				Com_sprintf(ms->location, sizeof(ms->location), _("No nation"));
			}
			CL_GetNationTeamName(nation, ms->civTeam, sizeof(ms->civTeam));

			/* FIXME */
			Q_strncpyz(ms->alienTeam, "ortnok", sizeof(ms->alienTeam));

			Q_strncpyz(ms->loadingscreen, "crashsite", sizeof(ms->loadingscreen));
			Com_sprintf(ms->onwin, sizeof(ms->onwin), "cp_ufocrashed %i", aircraft->ufotype);
			/* use ufocrash.ump as random tile assembly */
			Com_sprintf(ms->map, sizeof(ms->map), "+ufocrash");
			Com_sprintf(ms->param, sizeof(ms->param), "%s-%s", UFO_TypeToShortName(aircraft->ufotype), MAP_GetZoneType(color));
			mis = CL_CampaignAddGroundMission(ms);
			if (mis) {
				Vector2Set(mis->realPos, ms->pos[0], ms->pos[1]);
				MN_AddNewMessage(_("Interception"), _("UFO interception successful -- New mission available."), qfalse, MSG_CRASHSITE, NULL);
			} else {
				/* no active stage - to decrement the mission counter */
				CL_RemoveLastMission();
				MN_AddNewMessage(_("Interception"), _("UFO interception succesful -- UFO lost."), qfalse, MSG_CRASHSITE, NULL);
			}
		} else {
			Com_Printf("zone: %s (%i:%i:%i)\n", MAP_GetZoneType(color), color[0], color[1], color[2]);
			MN_AddNewMessage(_("Interception"), _("UFO interception successful -- UFO lost to sea."), qfalse, MSG_STANDARD, NULL);
		}
		/* change destination of other projectiles aiming aircraft */
		AIRFIGHT_RemoveProjectileAimingAircraft(aircraft);
		/* now remove the ufo from geoscape */
		UFO_RemoveUfoFromGeoscape(aircraft);
		/* and send our aircraft back to base */
		/* @todo check that the aircraft that launched the projectile hasn't been destroyed by another aircraft */
		if (shooter)
			AIR_AircraftReturnToBase(shooter);
	} else {
		/* Destroy the aircraft and everything onboard */
		AIR_DestroyAircraft(aircraft);

		/* change destination of other projectiles aiming aircraft */
		AIRFIGHT_RemoveProjectileAimingAircraft(aircraft);

		MN_AddNewMessage(_("Interception"), _("You've lost the battle"), qfalse, MSG_STANDARD, NULL);
	}
}

/**
 * @brief Check if some projectiles on geoscape reached their destination.
 * @note Destination is not necessarily an aircraft, in case the projectile missed its initial target.
 * @param[in] projectile Pointer to the projectile
 * @param[in] movement distance that the projectile will do up to next draw of geoscape
 * @sa AIRFIGHT_CampaignRunProjectiles
 */
static qboolean AIRFIGHT_ProjectileReachedTarget (aircraftProjectile_t *projectile, float movement)
{
	float distance;

	if (!projectile->aimedAircraft)
		/* the target is idle, its position is in idleTarget*/
		distance = CP_GetDistance(projectile->idleTarget, projectile->pos);
	else {
		/* the target is moving, pointer to the other aircraft is aimedAircraft */
		distance = CP_GetDistance(projectile->aimedAircraft->pos, projectile->pos);
	}

	/* projectile reaches its target */
	if (distance < movement) {
		return qtrue;
	}

	/* check if the projectile went farther than it's range */
	distance = (float) projectile->time * aircraftItems[projectile->aircraftItemsIdx].weaponSpeed / 3600.0f;
	if (distance > aircraftItems[projectile->aircraftItemsIdx].stats[AIR_STATS_WRANGE]) {
		return qtrue;
	}

	return qfalse;
}

/**
 * @brief Solve the result of one projectile hitting an aircraft.
 * @param[in] projectile Pointer to the projectile.
 * @note the target loose (base damage - shield of target) hit points
 */
static void AIRFIGHT_ProjectileHits (aircraftProjectile_t *projectile)
{
	aircraft_t *target;
	int damage = 0;

	assert(projectile);
	target = projectile->aimedAircraft;
	assert(target);

	/* if the aircraft is not on geoscape anymore, do nothing (returned to base) */
	if (AIR_IsAircraftInBase(target))
		return;

	/* base damage is given by the ammo */
	damage = aircraftItems[projectile->aircraftItemsIdx].weaponDamage;

	/* reduce damages with shield target */
	damage -= target->stats[AIR_STATS_SHIELD];

	/* apply resulting damages */
	if (damage > 0) {
		target->stats[AIR_STATS_DAMAGE] -= damage;
		if (target->stats[AIR_STATS_DAMAGE] <= 0)
			AIRFIGHT_ActionsAfterAirfight(projectile->attackingAircraft, projectile->aimedAircraft, target->type == AIRCRAFT_UFO);
	}
}

/**
 * @brief Solve the result of one projectile hitting a base.
 * @param[in] projectile Pointer to the projectile.
 * @sa B_UpdateBaseData
 */
static void AIRFIGHT_ProjectileHitsBase (aircraftProjectile_t *projectile)
{
	base_t *base;
	int damage = 0;

	assert(projectile);
	base = gd.bases + projectile->aimedBaseIdx;
	assert(base);

	/* base damage is given by the ammo */
	damage = aircraftItems[projectile->aircraftItemsIdx].weaponDamage;

	/* damages are divided between defense system and base. */
	damage = frand() * damage;
	base->batteryDamage -= damage;
	base->baseDamage -= aircraftItems[projectile->aircraftItemsIdx].weaponDamage - damage;

	/* @todo implement me */
	if (base->batteryDamage <= 0) {
		Com_Printf("projectile destroyed turret\n");
		base->batteryDamage = MAX_BATTERY_DAMAGE;
	}

	if (base->baseDamage <= 0) {
		int rnd;
		Com_Printf("projectile destroyed a building\n");
		base->baseDamage = MAX_BASE_DAMAGE;
		rnd = frand() * gd.numBuildings[base->idx];
		/* Add message to message-system. */
		Com_sprintf(messageBuffer, sizeof(messageBuffer), _("You've lost a base facility (%s)."), _(gd.buildings[base->idx][rnd].name));
		MN_AddNewMessage(_("Base facility destroyed"), messageBuffer, qfalse, MSG_BASEATTACK, NULL);
		B_BuildingDestroy(base, &gd.buildings[base->idx][rnd]);
	}
}

/**
 * @brief Update values of projectiles.
 * @param[in] dt Time elapsed since last call of this function.
 */
void AIRFIGHT_CampaignRunProjectiles (int dt)
{
	aircraftProjectile_t *projectile;
	int idx;
	float angle;
	float movement;
	vec3_t ortogonalVector, finalPoint, startPoint;
	base_t*		base;

	/* update delay next shot of bases */
	/* @todo: this is probably not the best place for this update, but I don't know where to put it -- Kracken 230707 */
	for (base = gd.bases; base < (gd.bases + gd.numBases); base++) {
		for (idx = 0; idx < base->maxBatteries; idx++) {
			if (base->batteries[idx].delayNextShot > 0)
				base->batteries[idx].delayNextShot -= dt;
		}
	}

	for (idx = 0, projectile = gd.projectiles; idx < gd.numProjectiles; projectile++, idx++) {
		projectile->time += dt;
		movement = (float) dt * aircraftItems[projectile->aircraftItemsIdx].weaponSpeed / 3600.0f;
		/* Check if the projectile reached its destination (aircraft or idle point) */
		if (AIRFIGHT_ProjectileReachedTarget(projectile, movement)) {
			/* check if it got the ennemy */
			if (projectile->aimedBaseIdx > -1)
				AIRFIGHT_ProjectileHitsBase(projectile);
			else if (projectile->aimedAircraft)
				AIRFIGHT_ProjectileHits(projectile);

			/* remove the missile from gd.projectiles[] */
			AIRFIGHT_RemoveProjectile(idx);
		} else {
			/* missile is moving towards its target */
			if (projectile->aimedAircraft)
				angle = MAP_AngleOfPath(projectile->pos, projectile->aimedAircraft->pos, NULL, ortogonalVector, cl_3dmap->value);
			else
				angle = MAP_AngleOfPath(projectile->pos, projectile->idleTarget, NULL, ortogonalVector, cl_3dmap->value);
			/* udpate angle of the projectile */
			projectile->angle = angle;
			/* update position of the projectile */
			PolarToVec(projectile->pos, startPoint);
			RotatePointAroundVector(finalPoint, ortogonalVector, startPoint, movement);
			VecToPolar(finalPoint, projectile->pos);
			/* Update position of bullets if needed */
			AIRFIGHT_RunBullets(projectile, ortogonalVector, movement);
		}
	}
}



