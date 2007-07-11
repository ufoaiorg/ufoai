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

	for (i = idx; i < gd.numProjectiles; i++) {
		gd.projectiles[i] = gd.projectiles[i+1];
	}
	gd.numProjectiles--;

	return qtrue;
}

/**
 * @brief Add a projectile in gd.projectiles
 * @param[in] idx of the ammo to add in array aircraftItems[]
 * @param[in] attacker Pointer to the attacking aircraft
 * @param[in] target Pointer to the target aircraft
 * @note we already checked in AIRFIGHT_ChooseWeapon that the weapon has still ammo
 */
static qboolean AIRFIGHT_AddProjectile (int idx, aircraft_t *attacker, aircraft_t *target, int slotIdx)
{
	aircraftProjectile_t *projectile;

	assert(attacker);
	assert(target);

	if (gd.numProjectiles >= MAX_PROJECTILESONGEOSCAPE) {
		Com_Printf("Too many projectiles on map\n");
		return qfalse;
	}

	assert(target);

	projectile = &gd.projectiles[gd.numProjectiles];

	projectile->aircraftItemsIdx = idx;
	projectile->idx = gd.numProjectiles;
	VectorSet(projectile->pos, attacker->pos[0], attacker->pos[1], 0);
	VectorSet(projectile->idleTarget, 0, 0, 0);
	projectile->aimedAircraft = target;
	projectile->time = 0;
	projectile->angle = 0.0f;

	attacker->weapons[slotIdx].ammoLeft -= 1;
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
	assert(oldTarget);

	VectorSet(projectile->idleTarget, oldTarget->pos[0] + 10 * frand() - 5, oldTarget->pos[1] + 10 * frand() - 5, 0);
	projectile->aimedAircraft = NULL;
}

/**
 * @brief Choose the weapon an attacking aircraft will use to fire on a target.
 * @param[in] shooter Pointer to the attacking aircraft.
 * @param[in] target Pointer to the aimed aircraft.
 * @return indice of the slot to use (in array weapons[]), -1 if no weapon to use atm, -2 if no weapon to use at all (no ammo left).
 */
static int AIRFIGHT_ChooseWeapon (aircraft_t *shooter, aircraft_t *target)
{
	int slotIdx = -2;
	int i, ammoIdx;
	float distance0, distance=99999.9f;

	distance0 = distance;

	/* We choose the usable weapon with the smaller range */
	for (i = 0; i < MAX_AIRCRAFTSLOT; i++) {
		/* check if there is a functional weapon in this slot */
		if (shooter->weapons[i].itemIdx < 0 || shooter->weapons[i].installationTime != 0)
			continue;

		/* check if there is still ammo in this weapon */
		if (shooter->weapons[slotIdx].ammoIdx < 0 || shooter->weapons[i].ammoLeft <= 0)
			continue;
		
		if (slotIdx == -2)
			slotIdx = -1;

		ammoIdx = shooter->weapons[i].ammoIdx;
		/* check if the target is within range of this weapon */
		distance = CP_GetDistance(shooter->pos, target->pos);
		if (distance > aircraftItems[ammoIdx].stats[AIR_STATS_WRANGE])
			continue;
		
		/* check if weapon is reloaded */
		if (shooter->weapons[i].delayNextShot > 0)
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
 * @param[in] shooter Pointer to the attacking aircraft.
 * @param[in] target Pointer to the aimed aircraft.
 * @return Probability to hit the target (0 when you don't have a chance, 1 (or more) when you're sure to hit).
 * @note that modifiers due to electronics, weapons, and shield are already taken into account in AII_UpdateAircraftStats
 * @sa AII_UpdateAircraftStats
 * @sa AIRFIGHT_ExecuteActions
 * @sa AIRFIGHT_ChooseWeapon
 * @pre slotIdx must have a weapon installed, with ammo available (see AIRFIGHT_ChooseWeapon)
 * @todo This probability should also depend on the pilot skills, when they will be implemented.
 */
static float AIRFIGHT_ProbabilityToHit (aircraft_t *shooter, aircraft_t *target, int slotIdx)
{
	int idx;
	float probability = 0.0f;

#ifdef DEBUG
	idx = shooter->weapons[slotIdx].itemIdx;
	if (idx < 0) {
		Com_Printf("AIRFIGHT_ProbabilityToHit: no weapon assigned to attacking aircraft\n");
		return probability;
	}

	idx = shooter->weapons[slotIdx].ammoIdx;
	if (idx < 0) {
		Com_Printf("AIRFIGHT_ProbabilityToHit: no ammo in weapon of attacking aircraft\n");
		return probability;
	}
#endif

	/* Take Base probability from the ammo of the attacking aircraft */
	probability = aircraftItems[idx].stats[AIR_STATS_ACCURACY];
	Com_DPrintf("Base probablity: %f\n", probability);

	/* Modify this probability by items of the attacking aircraft (stats is in percent) */
	probability *= shooter->stats[AIR_STATS_ACCURACY] / 100.0f;

	/* Modify this probability by items of the aimed aircraft (stats is in percent) */
	probability /= shooter->stats[AIR_STATS_ECM] / 100.0f;

	Com_DPrintf("Probability to hit: %f\n", probability);
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
	assert(target);

	/* Check if the attacking aircraft can shoot */
	slotIdx = AIRFIGHT_ChooseWeapon(shooter, target);

	if (slotIdx > -1) {
		ammoIdx = shooter->weapons[slotIdx].ammoIdx;

		/* shoot */
		if (AIRFIGHT_AddProjectile(ammoIdx, shooter, target, slotIdx)) {
			shooter->weapons[slotIdx].delayNextShot = aircraftItems[ammoIdx].weaponDelay;
			/* will we miss the target ? */
			probability = frand();
			if (probability > AIRFIGHT_ProbabilityToHit(shooter, target, slotIdx))
				AIRFIGHT_MissTarget(gd.numProjectiles - 1);
		}
	} else if (slotIdx > -2) {
		/* no ammo to fire atm (too far or reloading), pursue target */
		AIR_SendAircraftPurchasingUfo(shooter, target);
	} else {
		/* no ammo left, or no weapon, you should flee ! */
		AIR_AircraftReturnToBase(shooter);
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
 * @param[in] aircraft The aircraft which was destroyed (alien or phalanx).
 * @param[in] battleStatus qtrue if PHALANX won, qfalse if UFO won.
 * @note Some of these mission values are redone (and not reloaded) in CP_Load
 * @sa CP_Load
 */
void AIRFIGHT_ActionsAfterAirfight (aircraft_t* aircraft, qboolean phalanxWon)
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
		AIR_AircraftReturnToBase(aircraft);
	} else {
		/* @todo: destroy the aircraft and all soldiers in it */
		/* @todo: maybe rescue some of the soldiers */
		/* FIXME: remove this */
		AIR_AircraftReturnToBase(aircraft);

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

	/* base damage is given by the ammo */
	damage = aircraftItems[projectile->aircraftItemsIdx].weaponDamage;

	/* reduce damages with shield target */
	damage -= target->stats[AIR_STATS_SHIELD];

	/* apply resulting damages */
	if (damage > 0) {
		target->stats[AIR_STATS_DAMAGE] -= damage;
		if (target->stats[AIR_STATS_DAMAGE] <= 0)
			AIRFIGHT_ActionsAfterAirfight(projectile->aimedAircraft, qtrue);
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

	for (idx = 0, projectile = gd.projectiles; idx < gd.numProjectiles; projectile++, idx++) {
		projectile->time += dt;
		movement = (float) dt * aircraftItems[projectile->aircraftItemsIdx].weaponSpeed / 3600.0f;
		/* Check if the projectile reached its destination (aircraft or idle point) */
		if (AIRFIGHT_ProjectileReachedTarget(projectile, movement)) {
			/* check if it got the ennemy */
			if (projectile->aimedAircraft) {
				AIRFIGHT_ProjectileHits(projectile);
			}
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
		}
	}
}

