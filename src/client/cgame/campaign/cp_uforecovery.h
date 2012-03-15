/**
 * @file cp_uforecovery.h
 * @brief UFO recovery and storing
 */

/*
Copyright (C) 2002-2011 UFO: Alien Invasion.

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

/* time the recovery takes in days */
#define RECOVERY_DELAY 2.0f

/**
 * @brief different statuses for a stored UFO
 * @note If you change/reorder this change ufostatus_strings (cp_uforecovery.c) as well
 */
typedef enum {
	SUFO_RECOVERED,				/**< UFO just got recovered, it's being transported to the UFO Yard */
	SUFO_STORED,				/**< UFO is in UFO Yard, nothing special */
	SUFO_TRANSFERED,			/**< UFO is being transfered to another UFO Yard */

	MAX_SUFO_STATUS
} storedUFOStatus_t;

/** @brief Structure for stored UFOs */
typedef struct storedUFO_s {
	int idx;
	char id[MAX_VAR];
	components_t *comp;
	const aircraft_t *ufoTemplate;

	storedUFOStatus_t status;
	/* arrival date (recovery/transfer) */
	date_t arrive;

	float condition;

	/* installation UFO is stored */
	installation_t *installation;

	/* link to disassembly item */
	production_t *disassembly;
} storedUFO_t;

void UR_ProcessActive(void);

#define US_Foreach(var) LIST_Foreach(ccs.storedUFOs, storedUFO_t, var)

storedUFO_t *US_StoreUFO(const aircraft_t *ufoTemplate, installation_t *installation, date_t date, float condition);
storedUFO_t *US_GetStoredUFOByIDX(const int idx);
storedUFO_t *US_GetClosestStoredUFO(const aircraft_t *ufoTemplate, const base_t *base);
void US_RemoveStoredUFO(storedUFO_t *ufo);
int US_UFOsInStorage(const aircraft_t *ufoTemplate, const installation_t *installation);
int US_StoredUFOCount(void);
void US_RemoveUFOsExceedingCapacity(installation_t *installation);
qboolean US_TransferUFO(storedUFO_t *ufo, installation_t *ufoyard);

/**
 * @brief returns if any UFOs are stored in UFO Yards
 */
#define US_UFOStored() (US_GetNext(NULL) != NULL)

void UR_InitStartup(void);
void UR_Shutdown(void);

#endif
