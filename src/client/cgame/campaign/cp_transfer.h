/**
 * @file
 * @brief Header file for Transfer stuff.
 */

/*
Copyright (C) 2002-2014 UFO: Alien Invasion.

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

struct transferData_s;

typedef enum {
	CARGO_TYPE_INVALID = 0,
	CARGO_TYPE_ITEM,
	CARGO_TYPE_EMPLOYEE,
	CARGO_TYPE_ALIEN_DEAD,
	CARGO_TYPE_ALIEN_ALIVE,
	CARGO_TYPE_AIRCRAFT
} transferCargoType_t;

enum {
	TRANS_ALIEN_ALIVE,
	TRANS_ALIEN_DEAD,

	TRANS_ALIEN_MAX
};

/** @brief Transfer information (they are being stored in ccs.transfers). */
typedef struct transfer_s {
	base_t* destBase;				/**< Pointer to destination base. May not be nullptr if active is true. */
	base_t* srcBase;				/**< Pointer to source base. May be nullptr if transfer comes from a mission (alien body recovery). */
	date_t event;					/**< When the transfer finish process should start. */

	int itemAmount[MAX_OBJDEFS];			/**< Amount of given item. */
	class AlienCargo* alienCargo;			/**< Alien cargo */
	linkedList_t* employees[MAX_EMPL];
	linkedList_t* aircraft;

	bool hasItems;				/**< Transfer of items. */
	bool hasEmployees;			/**< Transfer of employees. */
} transfer_t;

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

/** @brief Array of current cargo onboard. */
typedef struct transferCargo_s {
	union transferItem_t {
		const objDef_t* item;
		const aircraft_t* aircraft;
		const Employee* employee;
		const teamDef_t* alienTeam;
		const void* pointer;		/**< if you just wanna check whether a valid pointer was set */
	} data;
	transferCargoType_t type;			/**< Type of cargo (1 - items, 2 - employees, 3 - alien bodies, 4 - live aliens). */
} transferCargo_t;

typedef struct transferData_s {
	/** @brief Current selected aircraft for transfer (if transfer started from mission). */
	aircraft_t* transferStartAircraft;

	/** @brief Current selected base for transfer. */
	base_t* transferBase;

	/** @brief Current transfer type (item, employee, alien, aircraft). */
	transferType_t currentTransferType;

	/** @brief Current cargo onboard. */
	transferCargo_t cargo[MAX_CARGO];

	/** @brief Current item cargo. Amount of items for each object definition index. */
	int trItemsTmp[MAX_OBJDEFS];

	/** @brief Current alien cargo. */
	class AlienCargo* alienCargo;

	/** @brief Current personnel cargo. */
	linkedList_t* trEmployeesTmp[MAX_EMPL];

	/** @brief Current aircraft for transfer. */
	linkedList_t* aircraft;

	/** @brief Current cargo type count (updated in TR_CargoList) */
	int trCargoCountTmp;
} transferData_t;

#define TR_SetData(dataPtr, typeVal, ptr)  do { (dataPtr)->data.pointer = (ptr); (dataPtr)->type = (typeVal); } while (0);
#define TR_Foreach(var) LIST_Foreach(ccs.transfers, transfer_t, var)
#define TR_ForeachEmployee(var, transfer, employeeType) LIST_Foreach(transfer->employees[employeeType], Employee, var)
#define TR_ForeachAircraft(var, transfer) LIST_Foreach(transfer->aircraft, aircraft_t, var)

bool TR_AddData(transferData_t* transferData, transferCargoType_t type, const void* data);
void TR_TransferRun(void);
void TR_NotifyAircraftRemoved(const aircraft_t* aircraft);

transfer_t* TR_TransferStart(base_t* srcBase, transferData_t& transData);

void TR_InitStartup(void);
void TR_Shutdown(void);
