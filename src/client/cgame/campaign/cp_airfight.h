/**
 * @file
 * @brief Header file for airfights
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

#pragma once

#define BULLETS_PER_SHOT	1
#define MAX_MULTIPLE_PROJECTILES	10

/**
 * @note 0 if the weapon can shoot
 * @note -1 if it can't shoot atm
 * @note -2 if it will never be able to shoot
 */
#define AIRFIGHT_WEAPON_CAN_SHOOT 0
#define AIRFIGHT_WEAPON_CAN_NOT_SHOOT_AT_THE_MOMENT -1
#define AIRFIGHT_WEAPON_CAN_NEVER_SHOOT -2

#define AIRFIGHT_BASE_CAN_T_FIRE -1

/** @brief projectile used during fight between two or more aircraft */
typedef struct aircraftProjectile_s {
	const objDef_t* aircraftItem;		/**< Corresponding ammo */
	int idx;				/**< self link of the idx in ccs.projectiles[] @todo: is this really needed? */
	vec3_t pos[MAX_MULTIPLE_PROJECTILES];	/**< array of positions of the projectile (latitude and longitude) - @todo why vec3_t this is long/lat */
	vec3_t projectedPos[MAX_MULTIPLE_PROJECTILES];	/**< Array if projected positions of the projectile (latitude and longitude). */
	vec3_t oldDrawPos[MAX_MULTIPLE_PROJECTILES];	/**< Array if old draw positions of the projectile (latitude and longitude). */
	int numProjectiles;		/**< Number of projectile positions used for this projectile. */
	bool hasMoved;			/**< Has the projectile been moved by the CampaignRunProjectiles function */
	int numInterpolationPoints;	/**< Number of points drawn so far during interpolation. */
	vec3_t idleTarget;		/**< target of the projectile
							 ** used only if the projectile will miss its target (that is if aimedAircraft is nullptr) */
	aircraft_t* attackingAircraft;	/**< Aircraft which shot the projectile. nullptr if aircraft is destroyed or base is shooting */
	vec3_t attackerPos;		/**< position of the attacker.
							 ** used only if base or samsite is shooting (attackingAircraft == nullptr) */
	aircraft_t* aimedAircraft;	/**< target of the projectile/
								 ** used only if the projectile will touch its target (otherwise it's nullptr) */
	int time;				/**< time since the projectile has been launched */
	float angle;			/**< angle of the missile on the geoscape */
	bool bullets;			/**< projectile has active bullets on geoscape */
	bool beam;				/**< projectile has an active (laser) beam on geoscape */
	bool rocket;
} aircraftProjectile_t;

void AIRFIGHT_ExecuteActions(const struct campaign_s* campaign, aircraft_t* air, aircraft_t* ufo);
void AIRFIGHT_ActionsAfterAirfight(const struct campaign_s* campaign, aircraft_t* shooter, aircraft_t* aircraft, bool phalanxWon);
void AIRFIGHT_CampaignRunProjectiles(const struct campaign_s* campaign, int dt);
void AIRFIGHT_CampaignRunBaseDefence(int dt);
int AIRFIGHT_CheckWeapon(const aircraftSlot_t* slot, float distance);
int AIRFIGHT_ChooseWeapon(const aircraftSlot_t* slot, int maxSlot, const vec2_t pos, const vec2_t targetPos);
bool AIRFIGHT_BaseCanTargetUFO(const struct base_s* base, const aircraft_t* ufo);
void AIRFIGHT_RemoveProjectileAimingAircraft(const aircraft_t* aircraft);

bool AIRFIGHT_SaveXML(xmlNode_t* parent);
bool AIRFIGHT_LoadXML(xmlNode_t* parent);
void AIRFIGHT_InitStartup(void);
