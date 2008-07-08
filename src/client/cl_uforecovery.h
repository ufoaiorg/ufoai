/**
 * @file cl_uforecovery.h
 * @brief UFO recovery
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

#ifndef CLIENT_CL_UFORECOVERY_H
#define CLIENT_CL_UFORECOVERY_H

/* UFO Recoveries stuff. */
#define MAX_RECOVERIES 32

/** @brief Structure of UFO recoveries (all of them).
 * @sa ufoRecovery_t */
typedef struct ufoRecoveries_s {
	qboolean active;			/**< True if the recovery is under processing. */
	base_t *base;				/**< Base where the recovery will be processed. */
	aircraft_t *ufoTemplate;	/**< Entry of UFO in aircraftTemplates array. */
	date_t event;				/**< When the process will start (UFO got transported to base). */
} ufoRecoveries_t;

void UR_UpdateUFOHangarCapForAll(base_t *base);
void UR_Prepare(base_t *base);
void UR_ProcessActive(void);
qboolean UR_ConditionsForStoring(const base_t *base, const aircraft_t *ufocraft);
void UR_InitStartup(void);
void UR_SendMail(const aircraft_t *ufocraft, const base_t *base);

#endif
