/**
 * @file cp_transfer_callbacks.h
 * @brief Header file for menu related console command callbacks
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

#ifndef CP_TRANSFER_CALLBACKS_H
#define CP_TRANSFER_CALLBACKS_H

/**
 * @brief transfer types
 */
typedef enum {
	TRANS_TYPE_INVALID = -1,
	TRANS_TYPE_ITEM,
	TRANS_TYPE_EMPLOYEE,
	TRANS_TYPE_ALIEN,
	TRANS_TYPE_AIRCRAFT,

	TRANS_TYPE_MAX
} transferType_t;

typedef struct transferData_s {
	/** @brief Current selected aircraft for transfer (if transfer started from mission). */
	aircraft_t *transferStartAircraft;

	/** @brief Current selected base for transfer. */
	base_t *transferBase;

	/** @brief Current transfer type (item, employee, alien, aircraft). */
	transferType_t currentTransferType;

	/** @brief Current cargo onboard. */
	transferCargo_t cargo[MAX_CARGO];

	/** @brief Current item cargo. Amount of items for each object definition index. */
	int trItemsTmp[MAX_OBJDEFS];

	/** @brief Current alien cargo. [0] alive [1] dead */
	int trAliensTmp[MAX_TEAMDEFS][TRANS_ALIEN_MAX];

	/** @brief Current personnel cargo. */
	employee_t *trEmployeesTmp[MAX_EMPL][MAX_EMPLOYEES];

	/** @brief Current aircraft for transfer. */
	int trAircraftsTmp[MAX_AIRCRAFT];

	/** @brief Current cargo type count (updated in TR_CargoList) */
	int trCargoCountTmp;
} transferData_t;

void TR_InitCallbacks(void);
void TR_ShutdownCallbacks(void);

#endif
