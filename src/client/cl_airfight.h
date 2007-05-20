/**
 * @file cl_airfight.h
 * @brief Header file for airfights
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

#ifndef CLIENT_CL_AIRFIGHT_H
#define CLIENT_CL_AIRFIGHT_H

/** @brief projectile used during fight between aircrafts */
typedef struct aircraftProjectile_s {
	int aircraftItemsIdx;	/**< idx of the corresponding ammo in the array aircraftItems[] */
	int idx;				/**< self link of the idx in gd.projectiles[] */
	vec3_t pos;				/**< position of the projectile (latitude and longitude) */
	vec3_t idleTarget;		/**< target of the projectile
							 ** used only if the projectile will miss its target (that is if aimedAircraft is NULL) */
	aircraft_t * aimedAircraft;	/**< target of the projectile/
							 	 ** used only if the projectile will touch its target (otherwise it's NULL) */
	float distance;			/**< distance already done by the projectile */
	float angle;			/**< angle of the missile on the geoscape */
} aircraftProjectile_t;

extern void AIRFIGHT_ExecuteActions(aircraft_t* air, aircraft_t* ufo);
extern void AIRFIGHT_ActionsAfterAirfight(aircraft_t* aircraft, qboolean phalanxWon);
extern void AIRFIGHT_ProjectileReachedTarget(void);
extern void AIRFIGHT_CampaignRunProjectiles(int dt);

#endif /* CLIENT_CL_AIRFIGHT_H */
