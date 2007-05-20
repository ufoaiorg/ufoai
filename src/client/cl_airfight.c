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
 */
static qboolean AIRFIGHT_AddProjectile (int idx, vec3_t start, aircraft_t *target)
{
	if (gd.numProjectiles >= MAX_PROJECTILESONGEOSCAPE) {
		Com_Printf("Too many projectiles on map\n");
		return qfalse;
	}

	gd.projectiles[gd.numProjectiles].aircraftItemsIdx = idx;
	gd.projectiles[gd.numProjectiles].idx = gd.numProjectiles;
	VectorCopy(start, gd.projectiles[gd.numProjectiles].pos);
	gd.projectiles[gd.numProjectiles].aimedAircraft = target;
	gd.numProjectiles++;

	return qtrue;
}

/**
 * @brief Calculates the fight between aircraft and ufo.
 * @param[in] aircraft The aircraft we attack with.
 * @param[in] ufo The ufo we are going to attack.
 * @todo Implement me and display an attack popup.
 */
extern void AIRFIGHT_ExecuteActions (aircraft_t* air, aircraft_t* ufo)
{
	int idx;
	technology_t *tech;

	/* some asserts */
	assert(air);
	assert(ufo);

	tech = air->weapon;

	if (tech) {
		/* aircraft has a weapon */
		/* @todo we need the ammunition and not the weapon */
		idx = AII_GetAircraftItemByID(tech->provides);
		/* FIXME: we set a temporary value for craft_ammo_sparrowhawk */
		idx = 5;

		/* if we can shoot, shoot */
		/* FIXME: for now, you can only fire one missile at a time */
		/* we must add a rate of fire in aircraftmanagement.ufo */
		if (CP_GetDistance(ufo->pos, air->pos) < aircraftItems[idx].weaponRange / 2.0f && air->delayNextShot <= 0) {
			if (AIRFIGHT_AddProjectile(idx, air->pos, ufo))
				air->delayNextShot = aircraftItems[idx].weaponDelay;
		} else
			/* otherwise, pursue target */
			AIR_SendAircraftPurchasingUfo(air, ufo);
	} else {
		/* no weapon, you should flee ! */
		AIR_AircraftReturnToBase(air);
	}
}

/**
 * @brief Actions to execute when a fight is done.
 * @param[in] aircraft The aircraft which was destroyed (alien or phalanx).
 * @param[in] battleStatus qtrue if PHALANX won, qfalse if UFO won.
 */
extern void AIRFIGHT_ActionsAfterAirfight (aircraft_t* aircraft, qboolean phalanxWon)
{
	mission_t* ms = NULL;
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
			ms->missionType = MIS_INTERCEPT;
			/* the size if somewhat random, because not all map tiles would have
			 * alien spawn points */
			ms->aliens = aircraft->size;
			/* 1-4 civilians */
			ms->civilians = (frand() * 10);
			ms->civilians %= 4;
			ms->civilians += 1;
			Vector2Set(ms->pos, aircraft->pos[0], aircraft->pos[1]);
			nation = MAP_GetNation(ms->pos);
			Com_sprintf(ms->type, sizeof(ms->type), _("UFO crash site"));
			if (nation)
				Com_sprintf(ms->location, sizeof(ms->location), _(nation->name));
			else
				Com_sprintf(ms->location, sizeof(ms->location), _("No nation"));
			/* FIXME: teams + loadingscreen */
			Q_strncpyz(ms->alienTeam, "ortnok", sizeof(ms->alienTeam));
			Q_strncpyz(ms->civTeam, "european", sizeof(ms->civTeam));
			Q_strncpyz(ms->loadingscreen, "crashsite", sizeof(ms->civTeam));

			Com_sprintf(ms->onwin, sizeof(ms->onwin), "cp_uforecovery %s", aircraft->id); /* TODO: correct? */
			/* use ufocrash.ump as random tile assembly */
			Com_sprintf(ms->map, sizeof(ms->map), "+ufocrash");
			Com_sprintf(ms->param, sizeof(ms->param), "%s-%s", UFO_UfoTypeToShortName(aircraft->ufotype), MAP_GetZoneType(color));
			if (CL_CampaignAddGroundMission(ms))
				MN_AddNewMessage(_("Interception"), _("UFO interception successful -- New mission available."), qfalse, MSG_CRASHSITE, NULL);
			else
				MN_AddNewMessage(_("Interception"), _("UFO interception succesful -- UFO lost."), qfalse, MSG_CRASHSITE, NULL);
		} else {
			Com_Printf("zone: %s (%i:%i:%i)\n", MAP_GetZoneType(color), color[0], color[1], color[2]);
			MN_AddNewMessage(_("Interception"), _("UFO interception successful -- UFO lost to sea."), qfalse, MSG_STANDARD, NULL);
		}
		/* now remove the ufo from geoscape */
		UFO_RemoveUfoFromGeoscape(aircraft);
		/* and send our aircraft back to base */
		AIR_AircraftReturnToBase(aircraft);
	} else {
		/* @todo: destroy the aircraft and all soldiers in it */
		/* @todo: maybe rescue some of the soldiers */
		/* FIXME: remove this */
		AIR_AircraftReturnToBase(aircraft);

		MN_AddNewMessage(_("Interception"), _("You've lost the battle"), qfalse, MSG_STANDARD, NULL);
	}
}

/**
 * @brief Check if some projectiles on geoscape arrived at destination.
 */
extern void AIRFIGHT_ProjectileReachedTarget (void)
{
	aircraftProjectile_t *projectile;
	int idx;

	for (idx = 0, projectile = gd.projectiles; idx < gd.numProjectiles; projectile++, idx++) {
		if (!projectile->aimedAircraft)
			AIRFIGHT_RemoveProjectile(idx);
		if (CP_GetDistance(projectile->aimedAircraft->pos, projectile->pos) < 1.0f) {
			AIRFIGHT_ActionsAfterAirfight(projectile->aimedAircraft, qtrue);
			AIRFIGHT_RemoveProjectile(idx);
		} else if (projectile->distance > aircraftItems[gd.projectiles[idx].aircraftItemsIdx].weaponRange) {
			/* projectile went too far, delete it */
			AIRFIGHT_RemoveProjectile(idx);
		}
	}
}

/**
 * @brief Update values of projectiles.
 */
extern void AIRFIGHT_CampaignRunProjectiles (int dt)
{
	aircraftProjectile_t *projectile;
	int idx;
	float angle;
	
	for (idx = 0, projectile = gd.projectiles; idx < gd.numProjectiles; projectile++, idx++) {
		angle = MAP_AngleOfPath(projectile->pos, projectile->aimedAircraft->pos, NULL, projectile);
		projectile->angle = angle;
	}
	AIRFIGHT_ProjectileReachedTarget();
}

