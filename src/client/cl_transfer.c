/**
 * @file cl_transfer.c
 * @brief Deals with the Transfer stuff.
 * @note Transfer menu functions prefix: TR_
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

/** @brief Current selected base for transfer. */
static base_t *transferBase = NULL;

/** @brief Current transfer type (item, employee, alien, aircraft). */
static int transferType = TRANS_TYPE_INVALID;

/** @brief Current cargo onboard. */
static transferCargo_t cargo[MAX_CARGO];

/** @brief Current item cargo. */
static int trItemsTmp[MAX_OBJDEFS];

/** @brief Current alien cargo. [0] alive [1] dead */
static int trAliensTmp[MAX_TEAMDEFS][TRANS_ALIEN_MAX];

/** @brief Current personel cargo. */
static int trEmployeesTmp[MAX_EMPL][MAX_EMPLOYEES];

/** @brief Current aircrafts for transfer. */
static int trAircraftsTmp[MAX_AIRCRAFT];

/**
 * @brief Checks condition for item transfer.
 * @param[in] od Pointer to object definition.
 * @param[in] srcbase Pointer to current base.
 * @param[in] destbase Pointer to destination base.
 * @return qtrue if transfer of this item is possible.
 */
static qboolean TR_CheckItem (objDef_t *od, base_t *srcbase, base_t *destbase)
{
	int i, intransfer = 0, amtransfer = 0;

	assert(od && srcbase && destbase);

	/* Count size of all items already on the transfer list. */
	for (i = 0; i < csi.numODs; i++) {
		if (trItemsTmp[i] > 0) {
			if (!Q_strncmp(csi.ods[i].id, "antimatter", 10))
				amtransfer = ANTIMATTER_SIZE * trItemsTmp[i];
			else
				intransfer += csi.ods[i].size * trItemsTmp[i];
		}
	}

	/* Is this antimatter and destination base has enough space in Antimatter Storage? */
	if (!Q_strncmp(od->id, "antimatter", 10)) {
		/* Give some meaningful feedback to the player if the player clicks on an a.m. item but base doesn't have am storage. */
		if (!destbase->hasAmStorage && destbase->hasStorage) {
			MN_Popup(_("Missing storage"), _("Destination base does not have an Antimatter Storage.\n"));
			return qfalse;
		} else if (!destbase->hasAmStorage) {	/* Return if the target base doesn't have antimatter storage or power. */
			return qfalse;
		}
		if (destbase->capacities[CAP_ANTIMATTER].max - destbase->capacities[CAP_ANTIMATTER].cur - amtransfer < ANTIMATTER_SIZE) {
			MN_Popup(_("Not enough space"), _("Destination base does not have enough\nAntimatter Storage space to store more antimatter.\n"));
			return qfalse;
		}
	} else {	/*This is not antimatter*/
		if (!transferBase->hasStorage)	/* Return if the target base doesn't have storage or power. */
			return qfalse;
	}

	/* Does the destination base has enough space in storage? */
	if ((destbase->capacities[CAP_ITEMS].max - destbase->capacities[CAP_ITEMS].cur - intransfer < od->size) && Q_strncmp(od->id, "antimatter", 10)) {
		MN_Popup(_("Not enough space"), _("Destination base does not have enough\nStorage space to store this item.\n"));
		return qfalse;
	}

	return qtrue;
}

/**
 * @brief Checks condition for employee transfer.
 * @param[in] employee Pointer to employee for transfer.
 * @param[in] srcbase Pointer to current base.
 * @param[in] destbase Pointer to destination base.
 * @return qtrue if transfer of this type of alien is possible.
 */
static qboolean TR_CheckEmployee (employee_t *employee, base_t *srcbase, base_t *destbase)
{
	int i, empltype, intransfer = 0;

	assert(employee && srcbase && destbase);

	/* Count amount of all employees already on the transfer list. */
	for (empltype = 0; empltype < MAX_EMPL; empltype++) {
		for (i = 0; i < gd.numEmployees[empltype]; i++) {
			if (trEmployeesTmp[empltype][i] > TRANS_LIST_EMPTY_SLOT)
				intransfer++;
		}
	}

	/* Does the destination base has enough space in living quarters? */
	if (destbase->capacities[CAP_EMPLOYEES].max - destbase->capacities[CAP_EMPLOYEES].cur - intransfer < 1) {
		MN_Popup(_("Not enough space"), _("Destination base does not have enough space\nin Living Quarters.\n"));
		return qfalse;
	}

	/* Is this a soldier assigned to aircraft? */
	if ((employee->type == EMPL_SOLDIER) && CL_SoldierInAircraft(employee->idx, employee->type, -1)) {
		Com_sprintf(popupText, sizeof(popupText), _("%s %s is assigned to aircraft and cannot be\ntransfered to another base.\n"),
		gd.ranks[employee->chr.rank].shortname, employee->chr.name);
		MN_Popup(_("Soldier in aircraft"), popupText);
		return qfalse;
	}

	return qtrue;
}

/**
 * @brief Checks condition for alive alien transfer.
 * @param[in] alienidx Index of an alien type.
 * @param[in] srcbase Pointer to current base.
 * @param[in] destbase Pointer to destination base.
 * @return qtrue if transfer of this type of alien is possible.
 */
static qboolean TR_CheckAlien (int alienidx, base_t *srcbase, base_t *destbase)
{
	int i, intransfer = 0;

	assert(srcbase && destbase);

	/* Count amount of alive aliens already on the transfer list. */
	for (i = 0; i < gd.numAliensTD; i++) {
		if (trAliensTmp[i][TRANS_ALIEN_ALIVE] > 0)
			intransfer += trAliensTmp[i][TRANS_ALIEN_ALIVE];
	}

	/* Does the destination base has enough space in alien containment? */
	if (destbase->capacities[CAP_ALIENS].max - destbase->capacities[CAP_ALIENS].cur - intransfer < 1) {
		MN_Popup(_("Not enough space"), _("Destination base does not have enough space\nin Alien Containment.\n"));
		return qfalse;
	}

	return qtrue;
}

/**
 * @brief Checks condition for aircraft transfer.
 * @param[in] aircraft Pointer to aircraft which is going to be added to transferlist.
 * @param[in] srcbase Pointer to current base.
 * @param[in] destbase Pointer to destination base.
 * @return qtrue if transfer of this aircraft is possible.
 */
static qboolean TR_CheckAircraft (aircraft_t *aircraft, base_t *srcbase, base_t *destbase)
{
	int i, intransfer = 0;
	aircraft_t *aircraftOnList = NULL;
	assert(aircraft && srcbase && destbase);

	/* Count weight of all aircrafts already on the transfer list. */
	for (i = 0; i < MAX_AIRCRAFT; i++) {
		if (trAircraftsTmp[i] > TRANS_LIST_EMPTY_SLOT) {
			aircraftOnList = AIR_AircraftGetFromIdx(i);
			intransfer += aircraftOnList->weight;
		}
	}

	/* Hangars in destbase functional? */
	if (!destbase->hasPower) {
		MN_Popup(_("Hangars not ready"), _("Destination base does not have hangars ready.\nProvide power supplies.\n"));
		return qfalse;
	} else if (!destbase->hasCommand) {
		MN_Popup(_("Hangars not ready"), _("Destination base does not have command centre.\nHangars not functional.\n"));
		return qfalse;
	} else if (!destbase->hasHangar && !destbase->hasHangarSmall) {
		MN_Popup(_("Hangars not ready"), _("Destination base does not have any hangar."));
		return qfalse;
	}
	/* Is there a place for this aircraft in destination base? */
	if (AIR_CalculateHangarStorage(aircraft->idx_sample, destbase, intransfer) == 0) {
		MN_Popup(_("Not enough space"), _("Destination base does not have enough space\nin hangars.\n"));
		return qfalse;
	} else if (AIR_CalculateHangarStorage(aircraft->idx_sample, destbase, intransfer) > 0) {
		return qtrue;
	}

	return qtrue;
}

/**
 * @brief Display cargo list.
 */
static void TR_CargoList (void)
{
	int i, cnt = 0, empltype;
	int trempl[MAX_EMPL];
	employee_t *employee;
	aircraft_t *aircraft;
	static char cargoList[1024];
	char str[128];

	cargoList[0] = '\0';
	memset(cargo, 0, sizeof(cargo));
	memset(trempl, 0, sizeof(trempl));

	menuText[TEXT_CARGO_LIST] = cargoList;

	if (!baseCurrent)
		return;

	/* Show items. */
	for (i = 0; i < csi.numODs; i++) {
		if (trItemsTmp[i] > 0) {
			Com_sprintf(str, sizeof(str), _("%s (%i for transfer)\n"),
			csi.ods[i].name, trItemsTmp[i]);
			Q_strcat(cargoList, str, sizeof(cargoList));
			cargo[cnt].type = CARGO_TYPE_ITEM;
			cargo[cnt].itemidx = i;
			cnt++;
		}
	}

	/* Show employees. */
	for (empltype = 0; empltype < MAX_EMPL; empltype++) {
		for (i = 0; i < gd.numEmployees[empltype]; i++) {
			if (trEmployeesTmp[empltype][i] > TRANS_LIST_EMPTY_SLOT) {
				if (empltype == EMPL_SOLDIER) {
					employee = &gd.employees[empltype][i];
					Com_sprintf(str, sizeof(str), _("Soldier %s %s\n"), gd.ranks[employee->chr.rank].shortname, employee->chr.name);
					Q_strcat(cargoList, str, sizeof(cargoList));
					cargo[cnt].type = CARGO_TYPE_EMPLOYEE;
					cargo[cnt].itemidx = employee->idx;
					cnt++;
				}
				trempl[empltype]++;
			}
		}
	}
	for (empltype = 0; empltype < MAX_EMPL; empltype++) {
		if (empltype == EMPL_SOLDIER)
			continue;
		if (trempl[empltype] > 0) {
			Com_sprintf(str, sizeof(str), _("%s (%i for transfer)\n"), E_GetEmployeeString(empltype), trempl[empltype]);
			Q_strcat(cargoList, str, sizeof(cargoList));
			cargo[cnt].type = CARGO_TYPE_EMPLOYEE;
			cnt++;
		}
	}

	/* Show aliens. */
	for (i = 0; i < gd.numAliensTD; i++) {
		if (trAliensTmp[i][TRANS_ALIEN_DEAD] > 0) {
			Com_sprintf(str, sizeof(str), _("Corpse of %s (%i for transfer)\n"),
			_(AL_AlienTypeToName(AL_GetAlienGlobalIdx(i))), trAliensTmp[i][TRANS_ALIEN_DEAD]);
			Q_strcat(cargoList, str, sizeof(cargoList));
			cargo[cnt].type = CARGO_TYPE_ALIEN_DEAD;
			cargo[cnt].itemidx = i;
			cnt++;
		}
	}
	for (i = 0; i < gd.numAliensTD; i++) {
		if (trAliensTmp[i][TRANS_ALIEN_ALIVE] > 0) {
			Com_sprintf(str, sizeof(str), _("%s (%i for transfer)\n"),
			_(AL_AlienTypeToName(AL_GetAlienGlobalIdx(i))), trAliensTmp[i][TRANS_ALIEN_ALIVE]);
			Q_strcat(cargoList, str, sizeof(cargoList));
			cargo[cnt].type = CARGO_TYPE_ALIEN_ALIVE;
			cargo[cnt].itemidx = i;
			cnt++;
		}
	}

	/* Show aircrafts. */
	for (i = 0; i < MAX_AIRCRAFT; i++) {
		if (trAircraftsTmp[i] > TRANS_LIST_EMPTY_SLOT) {
			aircraft = AIR_AircraftGetFromIdx(trAircraftsTmp[i]);
			Com_sprintf(str, sizeof(str), _("Aircraft %s\n"),
			aircraft->name);
			Q_strcat(cargoList, str, sizeof(cargoList));
			cargo[cnt].type = CARGO_TYPE_AIRCRAFT;
			cargo[cnt].itemidx = i;
			cnt++;
		}
	}

	menuText[TEXT_CARGO_LIST] = cargoList;
}

/**
 * @brief Fills the items-in-base list with stuff available for transfer.
 * @note Filling the transfer list with proper stuff (items/employees/aliens/aircrafts) is being done here.
 * @sa TR_TransferStart_f
 * @sa TR_TransferInit_f
 */
static void TR_TransferSelect_f (void)
{
	static char transferList[2048];
	int type, empltype;
	int numempl[MAX_EMPL], trempl[MAX_EMPL];
	int i, j, cnt = 0;
	employee_t* employee = NULL;
	aircraft_t *aircraft = NULL;
	char str[128];

	if (!transferBase)
		return;

	if (Cmd_Argc() < 2)
		type = transferType;
	else
		type = atoi(Cmd_Argv(1));

	transferList[0] = '\0';

	/* Reset and fill temp employees arrays. */
	for (i = 0; i < MAX_EMPL; i++) {
		trempl[i] = numempl[i] = 0;
		for (j = 0; j < MAX_EMPLOYEES; j++) {
			if (trEmployeesTmp[i][j] > TRANS_LIST_EMPTY_SLOT)
				trempl[i]++;
		}
	}

	switch (type) {
	case TRANS_TYPE_ITEM:
		if (transferBase->hasStorage) {
			for (i = 0; i < csi.numODs; i++)
				if (baseCurrent->storage.num[i]) {
					if (trItemsTmp[i] > 0)
						Com_sprintf(str, sizeof(str), _("%s (%i for transfer, %i left)\n"), csi.ods[i].name, trItemsTmp[i], baseCurrent->storage.num[i]);
					else
						Com_sprintf(str, sizeof(str), _("%s (%i available)\n"), csi.ods[i].name, baseCurrent->storage.num[i]);
					Q_strcat(transferList, str, sizeof(transferList));
					cnt++;
				}
			if (!cnt)
				Q_strncpyz(transferList, _("Storage is empty.\n"), sizeof(transferList));
		} else if (transferBase->hasPower) {
			Q_strcat(transferList, _("Transfer is not possible - the base doesn't have a Storage."), sizeof(transferList));
		} else {
			Q_strcat(transferList, _("Transfer is not possible - the base does not have power supplies."), sizeof(transferList));
		}
		break;
	case TRANS_TYPE_EMPLOYEE:
		if (transferBase->hasQuarters) {
			for (empltype = 0; empltype < MAX_EMPL; empltype++) {
				for (i = 0; i < gd.numEmployees[empltype]; i++) {
					employee = &gd.employees[empltype][i];
					if (!E_IsInBase(employee, baseCurrent))
						continue;
					if (trEmployeesTmp[empltype][i] > TRANS_LIST_EMPTY_SLOT)	/* Already on transfer list. */
						continue;
					if (empltype == EMPL_SOLDIER) {
						Com_sprintf(str, sizeof(str), _("Soldier %s %s\n"), gd.ranks[employee->chr.rank].shortname, employee->chr.name);
						Q_strcat(transferList, str, sizeof(transferList));
						cnt++;
					}
					numempl[empltype]++;
				}
			}
			for (i = 0; i < MAX_EMPL; i++) {
				if (i == EMPL_SOLDIER)
					continue;
				if ((trempl[i] > 0) && (numempl[i] > 0))
					Com_sprintf(str, sizeof(str), _("%s (%i for transfer, %i left)\n"), E_GetEmployeeString(i), trempl[i], numempl[i]);
				else if (numempl[i] > 0)
					Com_sprintf(str, sizeof(str), _("%s (%i available)\n"), E_GetEmployeeString(i), numempl[i]);
				if (numempl[i] > 0) {
					Q_strcat(transferList, str, sizeof(transferList));
					cnt++;
				}
			}
			if (!cnt)
				Q_strncpyz(transferList, _("Living Quarters empty.\n"), sizeof(transferList));
		} else
			Q_strcat(transferList, _("Transfer is not possible - the base doesn't have Living Quarters."), sizeof(transferList));
		break;
	case TRANS_TYPE_ALIEN:
		if (transferBase->hasAlienCont) {
			for (i = 0; i < gd.numAliensTD; i++) {
				if (*baseCurrent->alienscont[i].alientype && baseCurrent->alienscont[i].amount_dead > 0) {
					if (trAliensTmp[i][TRANS_ALIEN_DEAD] > 0)
						Com_sprintf(str, sizeof(str), _("Corpse of %s (%i for transfer, %i left)\n"),
						_(AL_AlienTypeToName(AL_GetAlienGlobalIdx(i))), trAliensTmp[i][TRANS_ALIEN_DEAD],
						baseCurrent->alienscont[i].amount_dead);
					else
						Com_sprintf(str, sizeof(str), _("Corpse of %s (%i available)\n"),
						_(AL_AlienTypeToName(AL_GetAlienGlobalIdx(i))), baseCurrent->alienscont[i].amount_dead);
					Q_strcat(transferList, str, sizeof(transferList));
					cnt++;
				}
				if (*baseCurrent->alienscont[i].alientype && baseCurrent->alienscont[i].amount_alive > 0) {
					if (trAliensTmp[i][TRANS_ALIEN_ALIVE] > 0)
						Com_sprintf(str, sizeof(str), _("Alive %s (%i for transfer, %i left)\n"),
						_(AL_AlienTypeToName(AL_GetAlienGlobalIdx(i))), trAliensTmp[i][TRANS_ALIEN_ALIVE],
						baseCurrent->alienscont[i].amount_alive);
					else
						Com_sprintf(str, sizeof(str), _("Alive %s (%i available)\n"),
						_(AL_AlienTypeToName(AL_GetAlienGlobalIdx(i))), baseCurrent->alienscont[i].amount_alive);
					Q_strcat(transferList, str, sizeof(transferList));
					cnt++;
				}
			}
			if (!cnt)
				Q_strncpyz(transferList, _("Alien Containment is empty.\n"), sizeof(transferList));
		} else if (transferBase->hasPower) {
			Q_strcat(transferList, _("Transfer is not possible - the base doesn't have an Alien Containment."), sizeof(transferList));
		} else {
			Q_strcat(transferList, _("Transfer is not possible - the base does not have power supplies."), sizeof(transferList));
		}
		break;
	case TRANS_TYPE_AIRCRAFT:
		if (transferBase->hasHangar || transferBase->hasHangarSmall) {
			for (i = 0; i < MAX_AIRCRAFT; i++) {
				if (trAircraftsTmp[i] > TRANS_LIST_EMPTY_SLOT)	/* Already on transfer list. */
					continue;
				aircraft = AIR_AircraftGetFromIdx(i);
				if (aircraft) {
					if (aircraft->homebase == baseCurrent) {
						Com_sprintf(str, sizeof(str), _("Aircraft %s\n"), aircraft->name);
						Q_strcat(transferList, str, sizeof(transferList));
						cnt++;
					}
				}
			}
			if (!cnt)
				Q_strncpyz(transferList, _("No aircrafts available for transfer.\n"), sizeof(transferList));
		} else {
			Q_strcat(transferList, _("Transfer is not possible - the base doesn't hangar functional."), sizeof(transferList));
		}
		break;
	default:
		Com_Printf("TR_TransferSelect_f: Unknown type id %i\n", type);
		return;
	}

	/* Update cargo list. */
	TR_CargoList();

	transferType = type;
	menuText[TEXT_TRANSFER_LIST] = transferList;
}

/**
 * @brief Unload everything from transfer cargo back to base.
 * @note This is being executed by pressing Unload button in menu.
 */
static void TR_TransferListClear_f (void)
{
	int i;

	if (!baseCurrent)
		return;

	for (i = 0; i < csi.numODs; i++) {	/* Return items. */
		if (trItemsTmp[i] > 0) {
			if (!Q_strncmp(csi.ods[i].id, "antimatter", 10))
				INV_ManageAntimatter(baseCurrent, trItemsTmp[i], qtrue);
			else
				B_UpdateStorageAndCapacity(baseCurrent, i, trItemsTmp[i], qfalse, qfalse);
		}
 	}
	for (i = 0; i < gd.numAliensTD; i++) {	/* Return aliens. */
		if (trAliensTmp[i][TRANS_ALIEN_ALIVE] > 0)
			baseCurrent->alienscont[i].amount_alive += trAliensTmp[i][TRANS_ALIEN_ALIVE];
		if (trAliensTmp[i][TRANS_ALIEN_DEAD] > 0)
			baseCurrent->alienscont[i].amount_dead += trAliensTmp[i][TRANS_ALIEN_DEAD];
	}

	/* Clear temporary cargo arrays. */
	memset(trItemsTmp, 0, sizeof(trItemsTmp));
	memset(trAliensTmp, 0, sizeof(trAliensTmp));
	memset(trEmployeesTmp, TRANS_LIST_EMPTY_SLOT, sizeof(trEmployeesTmp));
	memset(trAircraftsTmp, TRANS_LIST_EMPTY_SLOT, sizeof(trAircraftsTmp));
	/* Update cargo list and items list. */
	TR_CargoList();
 	TR_TransferSelect_f();
}

/**
 * @brief Unloads transfer cargo when finishing the transfer or destroys it when no buildings/base.
 * @param[in] transfer Pointer to transfer in gd.alltransfers.
 * @param[in] success True if the transfer reaches dest base, false if the base got destroyed.
 * @sa TR_TransferEnd
 */
void TR_EmptyTransferCargo (transfer_t *transfer, qboolean success)
{
	int i, j;
	base_t *destination = NULL;
	base_t *source = NULL;
	employee_t *employee;
	aircraft_t *aircraft;
	char message[256];

	assert(transfer);
	if (success) {
		destination = &gd.bases[transfer->destBase];
		assert(destination);
	}
	source = &gd.bases[transfer->srcBase];
	assert(source);

	if (transfer->hasItems && success) {	/* Items. */
		if (!destination->hasStorage) {
			Com_sprintf(message, sizeof(message), _("Base %s does not have Storage, items are removed!"), destination->name);
			MN_AddNewMessage(_("Transport mission"), message, qfalse, MSG_TRANSFERFINISHED, NULL);
			/* Items cargo is not unloaded, will be destroyed in TR_TransferCheck(). */
		} else {
			for (i = 0; i < csi.numODs; i++) {
				if (transfer->itemAmount[i] > 0) {
					if (!Q_strncmp(csi.ods[i].id, "antimatter", 10))
						INV_ManageAntimatter(destination, transfer->itemAmount[i], qtrue);
					else
						B_UpdateStorageAndCapacity(destination, i, transfer->itemAmount[i], qfalse, qtrue);
				}
			}
		}
	}

	if (transfer->hasEmployees) {	/* Employees. */
		if (!destination->hasQuarters || !success) {	/* Employees will be unhired. */
			if (success) {
				Com_sprintf(message, sizeof(message), _("Base %s does not have Living Quarters, employees got unhired!"), destination->name);
				MN_AddNewMessage(_("Transport mission"), message, qfalse, MSG_TRANSFERFINISHED, NULL);
			}
			for (i = 0; i < MAX_EMPL; i++) {
				for (j = 0; j < gd.numEmployees[i]; j++) {
					if (transfer->employeesArray[i][j] > TRANS_LIST_EMPTY_SLOT) {
						employee = &gd.employees[i][j];
						employee->baseIDHired = transfer->srcBase;	/* Restore back the original baseid. */
						E_UnhireEmployee(employee);
					}
				}
			}
		} else {
			for (i = 0; i < MAX_EMPL; i++) {
				for (j = 0; j < gd.numEmployees[i]; j++) {
					if (transfer->employeesArray[i][j] > TRANS_LIST_EMPTY_SLOT) {
						employee = &gd.employees[i][j];
						employee->baseIDHired = transfer->srcBase;	/* Restore back the original baseid. */
						E_UnhireEmployee(employee);
						E_HireEmployee(destination, employee);
					}
				}
			}
		}
	}

	if (transfer->hasAliens && success) {	/* Aliens. */
		if (!destination->hasAlienCont) {
			Com_sprintf(message, sizeof(message), _("Base %s does not have Alien Containment, Aliens are removed!"), destination->name);
			MN_AddNewMessage(_("Transport mission"), message, qfalse, MSG_TRANSFERFINISHED, NULL);
			/* Aliens cargo is not unloaded, will be destroyed in TR_TransferCheck(). */
		} else {
			for (i = 0; i < gd.numAliensTD; i++) {
				if (transfer->alienAmount[i][TRANS_ALIEN_ALIVE] > 0) {
					destination->alienscont[i].amount_alive += transfer->alienAmount[i][TRANS_ALIEN_ALIVE];
					destination->capacities[CAP_ALIENS].cur += transfer->alienAmount[i][TRANS_ALIEN_ALIVE];
				}
				if (transfer->alienAmount[i][TRANS_ALIEN_DEAD] > 0)
					destination->alienscont[i].amount_dead += transfer->alienAmount[i][TRANS_ALIEN_DEAD];
			}
		}
	}

	if (transfer->hasAircrafts && success) {	/* Aircrafts. */
		for (i = 0; i < MAX_AIRCRAFT; i++) {
			if (transfer->aircraftsArray[i] > TRANS_LIST_EMPTY_SLOT) {
				aircraft = AIR_AircraftGetFromIdx(i);
				assert (aircraft);
				if (AIR_CalculateHangarStorage(aircraft->idx_sample, destination, 0) > 0) {
					/* Aircraft relocated to new base, just add new one. */
					AIR_NewAircraft(destination, aircraft->id);
					/* Remove aircraft from old base. */
					AIR_DeleteAircraft(aircraft);
				} else {
					/* No space, aircraft will be lost. */
					Com_sprintf(message, sizeof(message), _("Base %s does not have enough free space in hangars. Aircraft is lost!"), destination->name);
					MN_AddNewMessage(_("Transport mission"), message, qfalse, MSG_TRANSFERFINISHED, NULL);
					AIR_DeleteAircraft(aircraft);
				}
			}
		}
	}
}

/**
 * @brief Shows available bases in transfer menu.
 * @param[in] aircraft Pointer to aircraft used in transfer.
 */
void TR_TransferAircraftMenu (aircraft_t* aircraft)
{
	int i;
	static char transferBaseSelectPopup[512];

	transferBaseSelectPopup[0] = '\0';

	for (i = 0; i < gd.numBases; i++) {
		if (!gd.bases[i].founded)
			continue;
		Q_strcat(transferBaseSelectPopup, gd.bases[i].name, sizeof(transferBaseSelectPopup));
	}
	menuText[TEXT_LIST] = transferBaseSelectPopup;
	MN_PushMenu("popup_transferbaselist");
}

/**
 * @brief Ends the transfer.
 * @param[in] transfer Pointer to transfer in gd.alltransfers
 */
void TR_TransferEnd (transfer_t *transfer)
{
	base_t* destination = NULL;
	char message[256];

	assert(transfer);
	destination = &gd.bases[transfer->destBase];
	assert(destination);

	TR_EmptyTransferCargo(transfer, qtrue);
	Com_sprintf(message, sizeof(message), _("Transport mission ended, unloading cargo in base %s"), gd.bases[transfer->destBase].name);
	MN_AddNewMessage(_("Transport mission"), message, qfalse, MSG_TRANSFERFINISHED, NULL);
}

/**
 * @brief Starts the transfer.
 * @sa TR_TransferSelect_f
 * @sa TR_TransferInit_f
 */
static void TR_TransferStart_f (void)
{
	int i, j;
	employee_t *employee;
	aircraft_t *aircraft;
	transfer_t *transfer = NULL;
	char message[256];

	if (transferType == TRANS_TYPE_INVALID) {
		Com_Printf("TR_TransferStart_f: transferType is wrong!\n");
 		return;
 	}

	if (!transferBase || !baseCurrent) {
		Com_Printf("TR_TransferStart_f: No base selected!\n");
		return;
	}

	/* Find free transfer slot. */
	for (i = 0; i < MAX_TRANSFERS; i++) {
		if (!gd.alltransfers[i].active) {
			/* Make sure it is empty here. */
			memset(&gd.alltransfers[i], 0, sizeof(gd.alltransfers[i]));
			memset(&gd.alltransfers[i].employeesArray, TRANS_LIST_EMPTY_SLOT, sizeof(gd.alltransfers[i].employeesArray));
			memset(&gd.alltransfers[i].aircraftsArray, TRANS_LIST_EMPTY_SLOT, sizeof(gd.alltransfers[i].aircraftsArray));
			transfer = &gd.alltransfers[i];
			break;
		}
 	}

	if (!transfer)
		return;

	/* Initialize transfer. */
	transfer->event.day = ccs.date.day + 1;	/* @todo: instead of +1 day calculate distance */
	transfer->event.sec = ccs.date.sec;
	transfer->destBase = transferBase->idx; /* Destination base index. */
	transfer->srcBase = baseCurrent->idx;	/* Source base index. */
	transfer->active = qtrue;
	for (i = 0; i < csi.numODs; i++) {		/* Items. */
		if (trItemsTmp[i] > 0) {
			transfer->hasItems = qtrue;
			transfer->itemAmount[i] = trItemsTmp[i];
		}
	}
	/* Note that personel remains hired in source base during the transfer, that is
	 * it takes Living Quarters capacity, etc, but it cannot be used anywhere. */
	for (i = 0; i < MAX_EMPL; i++) {		/* Employees. */
		for (j = 0; j < gd.numEmployees[i]; j++) {
			if (trEmployeesTmp[i][j] > TRANS_LIST_EMPTY_SLOT) {
				transfer->hasEmployees = qtrue;
				employee = &gd.employees[i][j];
				E_RemoveEmployeeFromBuilding(employee);		/* Remove from building. */
				INVSH_DestroyInventory(&employee->inv);		/* Remove inventory. */
				HOS_RemoveFromList(employee, baseCurrent);	/* Remove from hospital. */
				transfer->employeesArray[i][j] = trEmployeesTmp[i][j];
				/* FIXME: wouldn't it be better to use MAX_BASES + 1 here? */
				employee->baseIDHired = 42;	/* Hack to take them as hired but not in any base. */
			}
		}
	}
	for (i = 0; i < gd.numAliensTD; i++) {		/* Aliens. */
		if (!csi.teamDef[i].alien)
			continue;
		if (trAliensTmp[i][TRANS_ALIEN_ALIVE] > 0) {
			transfer->hasAliens = qtrue;
			transfer->alienAmount[i][TRANS_ALIEN_ALIVE] = trAliensTmp[i][TRANS_ALIEN_ALIVE];
		}
		if (trAliensTmp[i][TRANS_ALIEN_DEAD] > 0) {
			transfer->hasAliens = qtrue;
			transfer->alienAmount[i][TRANS_ALIEN_DEAD] = trAliensTmp[i][TRANS_ALIEN_DEAD];
		}
	}
	for (i = 0; i < MAX_AIRCRAFT; i++) {	/* Aircrafts. */
		if (trAircraftsTmp[i] > TRANS_LIST_EMPTY_SLOT) {
			aircraft = AIR_AircraftGetFromIdx(i);
			aircraft->status = AIR_TRANSFER;
			CL_RemoveSoldiersFromAircraft(aircraft->idx, baseCurrent);
			transfer->hasAircrafts = qtrue;
			transfer->aircraftsArray[i] = i;
		}
	}

	/* Clear temporary cargo arrays. */
	memset(trItemsTmp, 0, sizeof(trItemsTmp));
	memset(trAliensTmp, 0, sizeof(trAliensTmp));
	memset(trEmployeesTmp, TRANS_LIST_EMPTY_SLOT, sizeof(trEmployeesTmp));
	memset(trAircraftsTmp, TRANS_LIST_EMPTY_SLOT, sizeof(trAircraftsTmp));

	Com_sprintf(message, sizeof(message), _("Transport mission started, cargo is being transported to base %s"), gd.bases[transfer->destBase].name);
	MN_AddNewMessage(_("Transport mission"), message, qfalse, MSG_TRANSFERFINISHED, NULL);
	Cbuf_AddText("mn_pop\n");
}

/**
 * @brief Adds a thing to transfercargo by left mouseclick.
 * @sa TR_TransferSelect_f
 * @sa TR_TransferInit_f
 */
static void TR_TransferListSelect_f (void)
{
	int num, cnt = 0, i, empltype;
	objDef_t *od;
	employee_t* employee;
	aircraft_t *aircraft;
	qboolean added = qfalse;

	if (Cmd_Argc() < 2)
		return;

	if (!baseCurrent)
		return;

	if (!transferBase) {
		MN_Popup(_("No target base selected"), _("Please select the target base from the list"));
		return;
	}

	num = atoi(Cmd_Argv(1));

	switch (transferType) {
	case TRANS_TYPE_INVALID:	/**< No list was inited before you call this. */
		return;
	case TRANS_TYPE_ITEM:
		for (i = 0; i < csi.numODs; i++) {
			if (baseCurrent->storage.num[i]) {
				if (cnt == num) {
					od = &csi.ods[i];
					if (TR_CheckItem(od, baseCurrent, transferBase)) {
						trItemsTmp[i]++;
						if (!Q_strncmp(od->id, "antimatter", 10))
							INV_ManageAntimatter(baseCurrent, 1, qfalse);
						else
							B_UpdateStorageAndCapacity(baseCurrent, i, -1, qfalse, qfalse);
						break;
					} else
						return;
				}
				cnt++;
			}
		}
		break;
	case TRANS_TYPE_EMPLOYEE:
		for (i = 0; i < gd.numEmployees[EMPL_SOLDIER]; i++) {
			employee = &gd.employees[EMPL_SOLDIER][i];
			if (!E_IsInBase(employee, baseCurrent))
				continue;
			if (trEmployeesTmp[EMPL_SOLDIER][i] > TRANS_LIST_EMPTY_SLOT)
				continue;
			if (cnt == num) {
				if (TR_CheckEmployee(employee, baseCurrent, transferBase)) {
					trEmployeesTmp[EMPL_SOLDIER][employee->idx] = employee->idx;
					added = qtrue;
					break;
				} else
					return;
			}
			cnt++;
		}
		if (added) /* We already added a soldier, so break. */
			break;
		for (empltype = 0; empltype < MAX_EMPL; empltype++) {
			if (empltype == EMPL_SOLDIER)
				continue;
			if (E_CountHired(baseCurrent, empltype) < 1)
				continue;
			if (cnt == num) {
				for (i = 0; i < gd.numEmployees[empltype]; i++) {
					employee = &gd.employees[empltype][i];
					if (!E_IsInBase(employee, baseCurrent))
							continue;
					if (trEmployeesTmp[empltype][employee->idx] > TRANS_LIST_EMPTY_SLOT)	/* Already on transfer list. */
						continue;
					if (TR_CheckEmployee(employee, baseCurrent, transferBase)) {
						trEmployeesTmp[empltype][employee->idx] = employee->idx;
						break;
					} else
						return;
				}
			}
			cnt++;
		}
		break;
	case TRANS_TYPE_ALIEN:
		if (!transferBase->hasAlienCont)
			return;
		for (i = 0; i < gd.numAliensTD; i++) {
			if (*baseCurrent->alienscont[i].alientype && baseCurrent->alienscont[i].amount_dead > 0) {
				if (cnt == num) {
					trAliensTmp[i][TRANS_ALIEN_DEAD]++;
					/* Remove the corpse from Alien Containment. */
					baseCurrent->alienscont[i].amount_dead--;
					break;
				}
				cnt++;
			}
			if (*baseCurrent->alienscont[i].alientype && baseCurrent->alienscont[i].amount_alive > 0) {
				if (cnt == num) {
					if (TR_CheckAlien(i, baseCurrent, transferBase)) {
						trAliensTmp[i][TRANS_ALIEN_ALIVE]++;
						/* Remove an alien from Alien Containment. */
						baseCurrent->alienscont[i].amount_alive--;
						baseCurrent->capacities[CAP_ALIENS].cur--;
						break;
					} else
						return;
				}
				cnt++;
			}
		}
		break;
	case TRANS_TYPE_AIRCRAFT:
		if (!transferBase->hasHangar && !transferBase->hasHangarSmall)
			return;
		for (i = 0; i < MAX_AIRCRAFT; i++) {
			aircraft = AIR_AircraftGetFromIdx(i);
			if (!aircraft)
				return;
			if (aircraft->homebase == baseCurrent) {
				if (cnt == num) {
					if (TR_CheckAircraft(aircraft, baseCurrent, transferBase)) {
						trAircraftsTmp[i] = i;
						break;
					} else
						return;
				}
				cnt++;
			}
		}
		break;
	default:
		return;
	}

	/* clear the command buffer
	 * needed to erase all TR_TransferListSelect_f
	 * paramaters */
	Cmd_BufClear(); /* @todo: no need? */
	TR_TransferSelect_f();
}

/**
 * @brief Callback for base list click.
 * @param[in] base Pointer to base which will be transferBase.
 * @note transferBase is being set here.
 */
static void TR_TransferBaseSelect (base_t *base)
{
	static char baseInfo[1024];
	qboolean powercomm = qfalse;

	if (!baseCurrent)
		return;

	assert(base);

	Com_sprintf(baseInfo, sizeof(baseInfo), "%s\n\n", base->name);

	if (base->hasStorage) {
		Q_strcat(baseInfo, _("You can transfer equipment - this base has a Storage.\n"), sizeof(baseInfo));
	} else if (base->hasPower) {
		Q_strcat(baseInfo, _("No Storage in this base.\n"), sizeof(baseInfo));
	} else {
		powercomm = qtrue;
		Q_strcat(baseInfo, _("No power supplies in this base.\n"), sizeof(baseInfo));
	}
	if (base->hasQuarters) {
		Q_strcat(baseInfo, _("You can transfer employees - this base has Living Quarters.\n"), sizeof(baseInfo));
	} else {
		Q_strcat(baseInfo, _("No Living Quarters in this base.\n"), sizeof(baseInfo));
	}
	if (base->hasAlienCont) {
		Q_strcat(baseInfo, _("You can transfer Aliens - this base has an Alien Containment.\n"), sizeof(baseInfo));
	} else if (base->hasPower) {
		Q_strcat(baseInfo, _("No Alien Containment in this base.\n"), sizeof(baseInfo));
	} else if (!powercomm) {
		powercomm = qtrue;
		Q_strcat(baseInfo, _("No power supplies in this base.\n"), sizeof(baseInfo));
	}
	if (base->hasAmStorage) {
		Q_strcat(baseInfo, _("You can transfer antimatter - this base has an Antimatter Storage.\n"), sizeof(baseInfo));
	} else if (base->hasPower) {
		Q_strcat(baseInfo, _("No Antimatter Storage in this base.\n"), sizeof(baseInfo));
	} else if (!powercomm) {
		powercomm = qtrue;
		Q_strcat(baseInfo, _("No power supplies in this base.\n"), sizeof(baseInfo));
	}
	if (base->hasHangar || base->hasHangarSmall) {
		Q_strcat(baseInfo, _("You can transfer aircraft - this base has Hangar.\n"), sizeof(baseInfo));
	} else if (!base->hasCommand) {
		Q_strcat(baseInfo, _("Aircraft transfer not possible - this base does not have Command Centre.\n"), sizeof(baseInfo));
	} else if (base->hasPower) {
		Q_strcat(baseInfo, _("No Hangar in this base.\n"), sizeof(baseInfo));
	} else if (!powercomm) {
		powercomm = qtrue;
		Q_strcat(baseInfo, _("No power supplies in this base.\n"), sizeof(baseInfo));
	}

	menuText[TEXT_BASE_INFO] = baseInfo;

	/* Set global pointer to current selected base. */
	transferBase = base;

	Cvar_Set("mn_trans_base_name", transferBase->name);

	/* Update stuff-in-base list. */
	TR_TransferSelect_f();
}

/**
 * @brief Selects next base.
 * @sa TR_PrevBase_f
 * @note Command to call this: trans_nextbase
 */
static void TR_NextBase_f (void)
{
	int i, counter;
	base_t *base = NULL;

	if (!baseCurrent)
		return;
	if (!transferBase)
		counter = baseCurrent->idx;
	else
		counter = transferBase->idx;

	for (i = counter + 1; i < gd.numBases; i++) {
		base = &gd.bases[i];
		if (!base->founded)
			continue;
		if (base == baseCurrent)
			continue;
		TR_TransferBaseSelect(base);
		return;
	}
	/* At this point we are at "last" base, so we will select first. */
	for (i = 0; i < gd.numBases; i++) {
		base = &gd.bases[i];
		if (!base->founded)
			continue;
		if (base == baseCurrent)
			continue;
		TR_TransferBaseSelect(base);
		return;
	}
}

/**
 * @brief Selects previous base.
 * @sa TR_NextBase_f
 * @note Command to call this: trans_prevbase
 */
static void TR_PrevBase_f (void)
{
	int i, counter;
	base_t *base = NULL;

	if (!baseCurrent)
		return;
	if (!transferBase)
		counter = baseCurrent->idx;
	else
		counter = transferBase->idx;

	for (i = counter - 1; i >= 0; i--) {
		base = &gd.bases[i];
		if (!base->founded)
			continue;
		if (base == baseCurrent)
			continue;
		TR_TransferBaseSelect(base);
		return;
	}
	/* At this point we are at "first" base, so we will select last. */
	for (i = gd.numBases; i >= 0; i--) {
		base = &gd.bases[i];
		if (!base->founded)
			continue;
		if (base == baseCurrent)
			continue;
		TR_TransferBaseSelect(base);
		return;
	}
}

/**
 * @brief Removes single item from cargolist by click.
 */
static void TR_CargoListSelect_f (void)
{
	int num, cnt = 0, entries = 0, i, j;
	qboolean removed = qfalse;

	if (Cmd_Argc() < 2)
		return;

	if (!baseCurrent)
		return;

	num = atoi(Cmd_Argv(1));

	switch (cargo[num].type) {
	case CARGO_TYPE_ITEM:
		for (i = 0; i < csi.numODs; i++) {
			if (trItemsTmp[i] > 0) {
				if (cnt == num) {
					trItemsTmp[i]--;
					baseCurrent->storage.num[i]++;
					break;
				}
				cnt++;
			}
		}
		break;
	case CARGO_TYPE_EMPLOYEE:
		for (i = 0; i < MAX_CARGO; i++) {
			/* Count previous types on the list. */
			switch (cargo[i].type) {
			case CARGO_TYPE_ITEM:
				entries++;
			default:
				break;
			}
		}
		/* Start increasing cnt from the amount of previous entries. */
		cnt = entries;
		for (i = 0; i < gd.numEmployees[EMPL_SOLDIER]; i++) {
			if (trEmployeesTmp[EMPL_SOLDIER][i] > TRANS_LIST_EMPTY_SLOT) {
				if (cnt == num) {
					trEmployeesTmp[EMPL_SOLDIER][i] = TRANS_LIST_EMPTY_SLOT;
					removed = qtrue;
					break;
				}
				cnt++;
			}
		}
		if (removed)	/* We already removed soldier, break here. */
			break;

		Com_DPrintf(DEBUG_CLIENT, "TR_CargoListSelect_f: cnt: %i, num: %i\n", cnt, num);

		for (i = 0; i < MAX_EMPL; i++) {
			if ((E_CountHired(baseCurrent, i) < 1) || (i == EMPL_SOLDIER))
				continue;
			if (cnt == num) {
				for (j = 0; j < gd.numEmployees[i]; j++) {
					if (trEmployeesTmp[i][j] > TRANS_LIST_EMPTY_SLOT) {
						trEmployeesTmp[i][j] = TRANS_LIST_EMPTY_SLOT;
						removed = qtrue;
						break;
					} else
						continue;
				}
			}
			if (removed)	/* We already removed employee, break here. */
				break;
			cnt++;
		}
		break;
	case CARGO_TYPE_ALIEN_DEAD:
		for (i = 0; i < MAX_CARGO; i++) {
			/* Count previous types on the list. */
			switch (cargo[i].type) {
			case CARGO_TYPE_ITEM:
			case CARGO_TYPE_EMPLOYEE:
				entries++;
			default:
				break;
			}
		}
		/* Start increasing cnt from the amount of previous entries. */
		cnt = entries;
		for (i = 0; i < gd.numAliensTD; i++) {
			if (trAliensTmp[i][TRANS_ALIEN_DEAD] > 0) {
				if (cnt == num) {
					trAliensTmp[i][TRANS_ALIEN_DEAD]--;
					baseCurrent->alienscont[i].amount_dead++;
					break;
				}
				cnt++;
			}
		}
		break;
	case CARGO_TYPE_ALIEN_ALIVE:
		for (i = 0; i < MAX_CARGO; i++) {
			/* Count previous types on the list. */
			switch (cargo[i].type) {
			case CARGO_TYPE_ITEM:
			case CARGO_TYPE_EMPLOYEE:
			case CARGO_TYPE_ALIEN_DEAD:
				entries++;
			default:
				break;
			}
		}
		/* Start increasing cnt from the amount of previous entries. */
		cnt = entries;
		for (i = 0; i < gd.numAliensTD; i++) {
			if (trAliensTmp[i][TRANS_ALIEN_ALIVE] > 0) {
				if (cnt == num) {
					trAliensTmp[i][TRANS_ALIEN_ALIVE]--;
					baseCurrent->alienscont[i].amount_alive--;
					break;
				}
				cnt++;
			}
		}
		break;
	case CARGO_TYPE_AIRCRAFT:
		for (i = 0; i < MAX_CARGO; i++) {
			/* Count previous types on the list. */
			switch (cargo[i].type) {
			case CARGO_TYPE_ITEM:
			case CARGO_TYPE_EMPLOYEE:
			case CARGO_TYPE_ALIEN_DEAD:
			case CARGO_TYPE_ALIEN_ALIVE:
				entries++;
			default:
				break;
			}
		}
		/* Start increasing cnt from the amount of previous entries. */
		cnt = entries;
		for (i = 0; i < MAX_AIRCRAFT; i++) {
			if (trAircraftsTmp[i] > TRANS_LIST_EMPTY_SLOT) {
				if (cnt == num) {
					trAircraftsTmp[i] = TRANS_LIST_EMPTY_SLOT;
					break;
				}
				cnt++;
			}
		}
		break;
	default:
		return;
	}

	Cbuf_AddText(va("trans_select %i\n", transferType));
}

/**
 * @brief Checks whether given transfer should be processed.
 * @sa CL_CampaignRun
 */
void TR_TransferCheck (void)
{
	int i;
	base_t *base;
	transfer_t *transfer;

	for (i = 0; i < MAX_TRANSFERS; i++) {
		transfer = &gd.alltransfers[i];
		if (transfer->event.day == ccs.date.day) {	/* @todo make it checking hour also, not only day */
			base = &gd.bases[transfer->destBase];
			if (!base->founded) {
				TR_EmptyTransferCargo(transfer, qfalse);
				MN_AddNewMessage(_("Transport mission"), _("The destination base no longer exists! Transfer cargo are lost, personel got unhired."), qfalse, MSG_TRANSFERFINISHED, NULL);

				/* @todo: what if source base is lost? we won't be able to unhire transfered personel. */
			} else {
				TR_TransferEnd(transfer);
			}
			/* Reset this transfer. */
			memset(&gd.alltransfers[i], 0, sizeof(gd.alltransfers[i]));
			return;
		}
	}
}

/**
 * @brief Transfer menu init function.
 * @note Command to call this: trans_init
 * @note Should be called whenever the Transfer menu gets active.
 */
static void TR_Init_f (void)
{
	if (!baseCurrent)
		return;

	transferBase = NULL;

	if (Cmd_Argc() < 2)
		Com_Printf("TR_Init_f: warning: you should call trans_init with parameter 0\n");

	/* Clear employees temp array. */
	memset(trEmployeesTmp, TRANS_LIST_EMPTY_SLOT, sizeof(trEmployeesTmp));

	/* Clear aircrafts temp array. */
	memset(trAircraftsTmp, TRANS_LIST_EMPTY_SLOT, sizeof(trAircraftsTmp));

	/* Select first available item. */
	TR_TransferSelect_f();

	/* Select first available base. */
	TR_NextBase_f();

	/* Set up cvar used to display transferBase. */
	if (transferBase)
		Cvar_Set("mn_trans_base_name", transferBase->name);
}

/**
 * @brief Closes Transfer Menu and resets temp arrays.
 */
static void TR_TransferClose_f (void)
{
	int i;

	if (!baseCurrent)
		return;

	/* Unload what was loaded. */
	for (i = 0; i < csi.numODs; i++) {
		if (trItemsTmp[i] > 0)
			baseCurrent->storage.num[i] += trItemsTmp[i];
	}
	for (i = 0; i < gd.numAliensTD; i++) {
		if (trAliensTmp[i][TRANS_ALIEN_DEAD] > 0)
			baseCurrent->alienscont[i].amount_dead += trAliensTmp[i][TRANS_ALIEN_DEAD];
		if (trAliensTmp[i][TRANS_ALIEN_ALIVE])
			baseCurrent->alienscont[i].amount_alive += trAliensTmp[i][TRANS_ALIEN_ALIVE];
	}
	/* Clear temporary cargo arrays. */
	memset(trItemsTmp, 0, sizeof(trItemsTmp));
	memset(trAliensTmp, 0, sizeof(trAliensTmp));
	memset(trEmployeesTmp, TRANS_LIST_EMPTY_SLOT, sizeof(trEmployeesTmp));
	memset(trAircraftsTmp, TRANS_LIST_EMPTY_SLOT, sizeof(trAircraftsTmp));

	Cbuf_AddText("mn_pop\n");
}

/**
 * @brief Save callback for savegames
 * @sa TR_Load
 * @sa SAV_GameSave
 */
qboolean TR_Save (sizebuf_t* sb, void* data)
{
	int i, j, k;
	transfer_t *transfer;

	for (i = 0; i < presaveArray[PRE_MAXTRA]; i++) {
		transfer = &gd.alltransfers[i];
		for (j = 0; j < presaveArray[PRE_MAXOBJ]; j++)
			MSG_WriteByte(sb, transfer->itemAmount[j]);
		for (j = 0; j < presaveArray[PRE_NUMALI]; j++) {
			MSG_WriteByte(sb, transfer->alienAmount[j][TRANS_ALIEN_ALIVE]);
			MSG_WriteByte(sb, transfer->alienAmount[j][TRANS_ALIEN_DEAD]);
		}
		for (j = 0; j < presaveArray[PRE_EMPTYP]; j++) {
			for (k = 0; k < presaveArray[PRE_MAXEMP]; k++)
				MSG_WriteShort(sb, transfer->employeesArray[j][k]);
		}
		for (j = 0; j < presaveArray[PRE_MAXAIR]; j++)
			MSG_WriteShort(sb, transfer->aircraftsArray[j]);
		MSG_WriteByte(sb, transfer->destBase);
		MSG_WriteByte(sb, transfer->srcBase);
		MSG_WriteByte(sb, transfer->active);
		MSG_WriteByte(sb, transfer->hasItems);
		MSG_WriteByte(sb, transfer->hasEmployees);
		MSG_WriteByte(sb, transfer->hasAliens);
		MSG_WriteByte(sb, transfer->hasAircrafts);
		MSG_WriteLong(sb, transfer->event.day);
		MSG_WriteLong(sb, transfer->event.sec);
	}
	return qtrue;
}

/**
 * @brief Load callback for savegames
 * @sa TR_Save
 * @sa SAV_GameLoad
 */
qboolean TR_Load (sizebuf_t* sb, void* data)
{
	int i, j, k;
	transfer_t *transfer;

	for (i = 0; i < presaveArray[PRE_MAXTRA]; i++) {
		transfer = &gd.alltransfers[i];
		for (j = 0; j < presaveArray[PRE_MAXOBJ]; j++)
			transfer->itemAmount[j] = MSG_ReadByte(sb);
		for (j = 0; j < presaveArray[PRE_NUMALI]; j++) {
			transfer->alienAmount[j][TRANS_ALIEN_ALIVE] = MSG_ReadByte(sb);
			transfer->alienAmount[j][TRANS_ALIEN_DEAD] = MSG_ReadByte(sb);
		}
		for (j = 0; j < presaveArray[PRE_EMPTYP]; j++) {
			for (k = 0; k < presaveArray[PRE_MAXEMP]; k++)
				transfer->employeesArray[j][k] = MSG_ReadShort(sb);
		}
		for (j = 0; j < presaveArray[PRE_MAXAIR]; j++)
			transfer->aircraftsArray[j] = MSG_ReadShort(sb);
		transfer->destBase = MSG_ReadByte(sb);
		transfer->srcBase = MSG_ReadByte(sb);
		transfer->active = MSG_ReadByte(sb);
		transfer->hasItems = MSG_ReadByte(sb);
		transfer->hasEmployees = MSG_ReadByte(sb);
		transfer->hasAliens = MSG_ReadByte(sb);
		transfer->hasAircrafts = MSG_ReadByte(sb);
		transfer->event.day = MSG_ReadLong(sb);
		transfer->event.sec = MSG_ReadLong(sb);
	}
	return qtrue;
}

/**
 * @brief Defines commands and cvars for the Transfer menu(s).
 * @sa MN_ResetMenus
 */
void TR_Reset (void)
{
	/* add commands */
	Cmd_AddCommand("trans_init", TR_Init_f, "Init function for Transfer menu");
	Cmd_AddCommand("trans_start", TR_TransferStart_f, "Starts the tranfer");
	Cmd_AddCommand("trans_select", TR_TransferSelect_f, "Switch between transfer types (employees, techs, items)");
	Cmd_AddCommand("trans_emptyairstorage", TR_TransferListClear_f, "Unload everything from transfer cargo back to base");
	Cmd_AddCommand("trans_list_click", TR_TransferListSelect_f, "Callback for transfer list node click");
	Cmd_AddCommand("trans_cargolist_click", TR_CargoListSelect_f, "Callback for cargo list node click");
	Cmd_AddCommand("trans_close", TR_TransferClose_f, "Callback for closing Transfer Menu");
	Cmd_AddCommand("trans_nextbase", TR_NextBase_f, "Callback for selecting next base");
	Cmd_AddCommand("trans_prevbase", TR_PrevBase_f, "Callback for selecting previous base");
}


