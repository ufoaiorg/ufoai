/**
 * @file cp_uforecovery.h
 * @brief UFO recovery and storing
 */

/*
Copyright (C) 2002-2009 UFO: Alien Invasion team.

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
	installation_t *installation;	/**< UFO Yard where the recovery will be processed. */
	aircraft_t *ufoTemplate;	/**< Entry of UFO in aircraftTemplates array. */
	date_t event;				/**< When the process will start (UFO got transported to base). */
} ufoRecoveries_t;

#define MAX_STOREDUFOS 128

/* time the recovery takes in days */
#define RECOVERY_DELAY 2.0f

/** @brief Structure for stored UFOs */
typedef struct storedUFO_s {
	int idx;
	char id[MAX_VAR];
	components_t *comp;
	aircraft_t *ufoTemplate;

	/* arrival date (recovery/transfer) */
	date_t arrive;

	/* installation UFO is stored */
	installation_t *installation;

	/* link to disassembly item */
	production_t *disassembly;
} storedUFO_t;

void UR_ProcessActive(void);
void UR_InitStartup(void);

storedUFO_t *US_StoreUFO(aircraft_t *ufoTemplate, installation_t *installation, date_t date);
storedUFO_t *US_GetStoredUFOPlaceByIDX(const int idx);
storedUFO_t *US_GetStoredUFOByIDX(const int idx);
storedUFO_t *US_GetClosestStoredUFO(const aircraft_t *ufoTemplate, const base_t *base);
void US_RemoveStoredUFO(storedUFO_t *ufo);
int US_UFOsInStorage(const aircraft_t *ufoTemplate, const installation_t *installation);
void US_RemoveUFOsExceedingCapacity(installation_t *installation);

#endif
