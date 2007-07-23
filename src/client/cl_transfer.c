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
static int transferType = -1;

/** @brief Current cargo onboard. */
static transferCargo_t cargo[MAX_CARGO];

/** @brief Current item cargo. */
static int trItemsTmp[MAX_OBJDEFS];

/** @brief Current alien cargo. [0] alive [1] dead */
static int trAliensTmp[MAX_TEAMDEFS][2];

/** @brief Current personel cargo. */
static int trEmployeesTmp[MAX_EMPL][MAX_EMPLOYEES];

/**
 * @brief Checks condition for item transfer.
 * @param[in] *od Pointer to object definition.
 * @param[in] *srcbase Pointer to current base.
 * @param[in] *destbase Pointer to destination base.
 * @return qtrue if transfer of this item is possible.
 */
qboolean TR_CheckItem (objDef_t *od, base_t *srcbase, base_t *destbase)
{
	assert (od && srcbase && destbase);

	/* Is this antimatter and destination base has enough space in Antimatter Storage? */
	if (!Q_strncmp(od->id, "antimatter", 10)) {
		if (destbase->capacities[CAP_ANTIMATTER].max - destbase->capacities[CAP_ANTIMATTER].cur < ANTIMATTER_SIZE) {
			MN_Popup(_("Not enough space"), _("Destination base does not have enough\nAntimatter Storage space to store more antimatter.\n"));
			return qfalse;
		}
	}

	/* Does the destination base has enough space in storage? */
	if ((destbase->capacities[CAP_ITEMS].max - destbase->capacities[CAP_ITEMS].cur < od->size) && Q_strncmp(od->id, "antimatter", 10)) {
		MN_Popup(_("Not enough space"), _("Destination base does not have enough\nStorage space to store this item.\n"));
		return qfalse;
	}
	/* Is the item under research at this moment? */
	/* @todo: implement me */

	return qtrue;
}

/**
 * @brief Checks condition for employee transfer.
 * @param[in] *employee Pointer to employee for transfer.
 * @param[in] *srcbase Pointer to current base.
 * @param[in] *destbase Pointer to destination base.
 * @return qtrue if transfer of this type of alien is possible.
 */
qboolean TR_CheckEmployee (employee_t *employee, base_t *srcbase, base_t *destbase)
{
	assert (employee && srcbase && destbase);

	/* Does the destination base has enough space in living quarters? */
	if (destbase->capacities[CAP_EMPLOYEES].max - destbase->capacities[CAP_EMPLOYEES].cur < 1) {
		MN_Popup(_("Not enough space"), _("Destination base does not have enough space\nin Living Quarters.\n"));
		return qfalse;
	}

	/* @todo: check capacities also with current personel on transferlist. */

	/* Is this a soldier assigned to aircraft? */
	if ((employee->type == EMPL_SOLDIER) && CL_SoldierInAircraft(employee->idx, -1)) {
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
 * @param[in] *srcbase Pointer to current base.
 * @param[in] *destbase Pointer to destination base.
 * @return qtrue if transfer of this type of alien is possible.
 */
qboolean TR_CheckAlien (int alienidx, base_t *srcbase, base_t *destbase)
{
	assert (srcbase && destbase);

	/* Does the destination base has enough space in alien containment? */
	if (destbase->capacities[CAP_ALIENS].max - destbase->capacities[CAP_ALIENS].cur < 1) {
		MN_Popup(_("Not enough space"), _("Destination base does not have enough space\nin Alien Containment.\n"));
		return qfalse;
	}
	/* Is the alien under research at this moment? */
	/* @todo: implement me */

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
	static char cargoList[1024];
	char str[128];

	cargoList[0] = '\0';
	memset(&cargo, 0, sizeof(cargo));

	menuText[TEXT_CARGO_LIST] = cargoList;
	for (i = 0; i < MAX_EMPL; i++)
		trempl[i] = 0;

	if (!baseCurrent)
		return;

	/* Show items. */
	for (i = 0; i < csi.numODs; i++) {
		if (trItemsTmp[i] > 0) {
			Com_sprintf(str, sizeof(str), _("%s (%i for transfer)\n"),
			csi.ods[i].name, trItemsTmp[i]);
			Q_strcat(cargoList, str, sizeof(cargoList));
			cargo[cnt].type = 1;
			cargo[cnt].itemidx = i;
			cnt++;
		}
	}

	/* Show employees. */
	for (empltype = 0; empltype < MAX_EMPL; empltype++) {
		for (i = 0; i < gd.numEmployees[empltype]; i++) {
			if (trEmployeesTmp[empltype][i] > -1) {
				if (empltype == EMPL_SOLDIER) {
					employee = &gd.employees[empltype][i];		
					Com_sprintf(str, sizeof(str), _("Soldier %s %s\n"), gd.ranks[employee->chr.rank].shortname, employee->chr.name);
					Q_strcat(cargoList, str, sizeof(cargoList));
					cargo[cnt].type = 2;
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
			cargo[cnt].type = 2;
			cnt++;
		}		
	}

	/* Show aliens. */
	for (i = 0; i < numTeamDesc; i++) {
		if (trAliensTmp[i][1] > 0) {
			Com_sprintf(str, sizeof(str), _("Corpse of %s (%i for transfer)\n"),
			_(AL_AlienTypeToName(i)), trAliensTmp[i][1]);
			Q_strcat(cargoList, str, sizeof(cargoList));
			cargo[cnt].type = 3;
			cargo[cnt].itemidx = i;
			cnt++;
		}
	}
	for (i = 0; i < numTeamDesc; i++) {
		if (trAliensTmp[i][0]) {
			Com_sprintf(str, sizeof(str), _("%s (%i for transfer)\n"),
			_(AL_AlienTypeToName(i)), trAliensTmp[i][0]);
			Q_strcat(cargoList, str, sizeof(cargoList));
			cargo[cnt].type = 4;
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
	static char transferList[1024];
	int type, empltype;
	int numempl[MAX_EMPL], trempl[MAX_EMPL];
	int i, j, cnt = 0;
	employee_t* employee = NULL;
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
			if (trEmployeesTmp[i][j] > -1)
				trempl[i]++;
		}
	}

	switch (type) { /**< 0 - items, 1 - employees, 2 - aliens, 3 - aircrafts */
	case 0:		/**< items */
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
		} else {
			Q_strcat(transferList, _("Transfer is not possible - the base doesn't have a Storage."), sizeof(transferList));
		}
		break;
	case 1:		/**< humans */
		if (transferBase->hasQuarters) {
			for (empltype = 0; empltype < MAX_EMPL; empltype++) {
				for (i = 0; i < gd.numEmployees[empltype]; i++) {
					employee = &gd.employees[empltype][i];
					if (!E_IsInBase(employee, baseCurrent))
						continue;
					if (trEmployeesTmp[empltype][i] > -1)	/* Already on transfer list. */
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
	case 2:		/**< aliens */
		if (transferBase->hasAlienCont) {
			for (i = 0; i < numTeamDesc; i++) {
				if (baseCurrent->alienscont[i].alientype && baseCurrent->alienscont[i].amount_dead > 0) {
					if (trAliensTmp[i][1] > 0)
						Com_sprintf(str, sizeof(str), _("Corpse of %s (%i for transfer, %i left)\n"),
						_(AL_AlienTypeToName(i)), trAliensTmp[i][1],
						baseCurrent->alienscont[i].amount_dead);
					else
						Com_sprintf(str, sizeof(str), _("Corpse of %s (%i available)\n"),
						_(AL_AlienTypeToName(i)), baseCurrent->alienscont[i].amount_dead);
					Q_strcat(transferList, str, sizeof(transferList));
					cnt++;
				}
				if (baseCurrent->alienscont[i].alientype && baseCurrent->alienscont[i].amount_alive > 0) {
					if (trAliensTmp[i][0] > 0)
						Com_sprintf(str, sizeof(str), _("Alive %s (%i for transfer, %i left)\n"),
						_(AL_AlienTypeToName(i)), trAliensTmp[i][0],
						baseCurrent->alienscont[i].amount_alive);
					else
						Com_sprintf(str, sizeof(str), _("Alive %s (%i available)\n"),
						_(AL_AlienTypeToName(i)), baseCurrent->alienscont[i].amount_alive);
					Q_strcat(transferList, str, sizeof(transferList));
					cnt++;
				}
			}
			if (!cnt)
				Q_strncpyz(transferList, _("Alien Containment is empty.\n"), sizeof(transferList));
		} else {
			Q_strcat(transferList, _("Transfer is not possible - the base doesn't have an Alien Containment."), sizeof(transferList));
		}
		break;
	case 3:			/**< aircrafts */
		if (transferBase->hasHangar) {
			Q_strcat(transferList, "@todo: aircrafts", sizeof(transferList));
		} else {
			Q_strcat(transferList, _("Transfer is not possible - the base doesn't have any hangar."), sizeof(transferList));
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
				B_UpdateStorageAndCapacity(baseCurrent, i, trItemsTmp[i], qfalse);
		}
 	}
	for (i = 0; i < numTeamDesc; i++) {	/* Return aliens. */
		if (trAliensTmp[i][0] > 0)
			baseCurrent->alienscont[i].amount_alive += trAliensTmp[i][0];
		if (trAliensTmp[i][1] > 0)
			baseCurrent->alienscont[i].amount_dead += trAliensTmp[i][1];
	}

	/* Clear temporary cargo arrays. */
	memset(trItemsTmp, 0, sizeof(trItemsTmp));
	memset(trAliensTmp, 0, sizeof(trAliensTmp));
	memset(trEmployeesTmp, -1, sizeof(trEmployeesTmp));
	/* Update cargo list and items list. */
	TR_CargoList();
 	TR_TransferSelect_f();
}

/**
 * @brief Unload transfer cargo when finishing the transfer.
 * @param[in] *transfer Pointer to transfer in gd.alltransfers.
 * @note This should be called only for unloading stuff, not TR_AIRCRAFT.
 * @sa TR_TransferEnd
 */
void TR_EmptyTransferCargo (transferlist_t *transfer)
{
	int i, j;
	base_t *destination = NULL;
	base_t *source = NULL;
	employee_t *employee;

	assert (transfer);
	destination = &gd.bases[transfer->destBase];
	source = &gd.bases[transfer->srcBase];
	assert (destination && source);

	/* Unload items. @todo: check building status and limits. */
	if (transfer->hasItems) {
		if (!destination->hasStorage) {
			/* @todo: destroy items in transfercargo and inform an user. */
		} else {
			for (i = 0; i < csi.numODs; i++) {
				if (transfer->itemAmount[i] > 0) {
					if (!Q_strncmp(csi.ods[i].id, "antimatter", 10))
						INV_ManageAntimatter(destination, transfer->itemAmount[i], qtrue);
					else
						B_UpdateStorageAndCapacity(destination, i, transfer->itemAmount[i], qfalse);
				}
			}
		}
	}

	/* Unload personel. @todo: check building status and limits. */
	if (transfer->hasEmployees) {
		if (!destination->hasQuarters) {
			/* @todo: what will we do here in such case? */
		} else {
			for (i = 0; i < MAX_EMPL; i++) {		/* Employees. */
				for (j = 0; j < gd.numEmployees[i]; j++) {
					if (transfer->employeesArray[i][j] > -1) {
						employee = &gd.employees[i][j];
						employee->baseIDHired = transfer->srcBase;	/* Restore back the original baseid. */
						E_UnhireEmployee (source, i, employee->idx);
						E_HireEmployee (destination, i, employee->idx);
					}
				}
			}
		}
	}

	/* Unload aliens. @todo: check building status and limits. */
	if (transfer->hasAliens) {
		if (!destination->hasAlienCont) {
			/* @todo: destroy aliens in transfercargo and inform an user. */
		} else {
			for (i = 0; i < numTeamDesc; i++) {
				if (transfer->alienAmount[i][0] > 0) {
					destination->alienscont[i].amount_alive += transfer->alienAmount[i][0];
					destination->capacities[CAP_ALIENS].cur += transfer->alienAmount[i][0];
				}
				if (transfer->alienAmount[i][1] > 0)
					destination->alienscont[i].amount_dead += transfer->alienAmount[i][1];
			}
		}
	}
}

/**
 * @brief Shows available bases in transfer menu.
 * @param[in] *aircraft Pointer to aircraft used in transfer.
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
 * @param[in] *transfer Pointer to transfer in gd.alltransfers
 */
void TR_TransferEnd (transferlist_t *transfer)
{
	base_t* destination = NULL;
	char message[256];

	assert (transfer);
	destination = &gd.bases[transfer->destBase];
	assert(destination);

	TR_EmptyTransferCargo (transfer);
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
	transferlist_t *transfer = NULL;
	char message[256];

	if (transferType < 0) {
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
			memset (&gd.alltransfers[i].employeesArray, -1, sizeof(gd.alltransfers[i].employeesArray));
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
	   it takes Living Quarters capacity, etc, but it cannot be used anywhere. */
	for (i = 0; i < MAX_EMPL; i++) {		/* Employees. */
		for (j = 0; j < gd.numEmployees[i]; j++) {
			if (trEmployeesTmp[i][j] > -1) {
				transfer->hasEmployees = qtrue;
				employee = &gd.employees[i][j];
				E_RemoveEmployeeFromBuilding(employee);		/* Remove from building. */
				Com_DestroyInventory(&employee->inv);		/* Remove inventory. */
				HOS_RemoveFromList(employee, baseCurrent);	/* Remove from hospital. */
				transfer->employeesArray[i][j] = trEmployeesTmp[i][j];
				employee->baseIDHired = 42;	/* Hack to take them as hired but not in any base. */
			}
		}
	}
	for (i = 0; i < numTeamDesc; i++) {		/* Aliens. */
		if (!teamDesc[i].alien)
			continue;
		if (trAliensTmp[i][0] > 0) {
			transfer->hasAliens = qtrue;
			transfer->alienAmount[i][0] = trAliensTmp[i][0];
		}
		if (trAliensTmp[i][1] > 0) {
			transfer->hasAliens = qtrue;
			transfer->alienAmount[i][1] = trAliensTmp[i][1];
		}
	}

	/* Clear temporary cargo arrays. */
	memset(trItemsTmp, 0, sizeof(trItemsTmp));
	memset(trAliensTmp, 0, sizeof(trAliensTmp));
	memset(trEmployeesTmp, -1, sizeof(trEmployeesTmp));

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
	case -1:	/**< No list was inited before you call this. */
		return;
	case 0:		/**< items */
		for (i = 0; i < csi.numODs; i++) {
			if (baseCurrent->storage.num[i]) {
				if (cnt == num) {
					od = &csi.ods[i];
					if (TR_CheckItem(od, baseCurrent, transferBase)) {
						trItemsTmp[i]++;
						if (!Q_strncmp(od->id, "antimatter", 10))
							INV_ManageAntimatter(baseCurrent, 1, qfalse);
						else
							B_UpdateStorageAndCapacity(baseCurrent, i, -1, qfalse);
						break;
					} else
						return;
				}
				cnt++;
			}
		}
		break;
	case 1:		/**< employees */
		for (i = 0; i < gd.numEmployees[EMPL_SOLDIER]; i++) {
			employee = &gd.employees[EMPL_SOLDIER][i];
			if (!E_IsInBase(employee, baseCurrent))
				continue;
			if (trEmployeesTmp[EMPL_SOLDIER][i] > -1)
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
					if (trEmployeesTmp[empltype][employee->idx] > -1)	/* Already on transfer list. */
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
	case 2:		/**< aliens */
		for (i = 0; i < numTeamDesc; i++) {
			if (baseCurrent->alienscont[i].alientype && baseCurrent->alienscont[i].amount_dead > 0) {
				if (cnt == num) {
					trAliensTmp[i][1]++;
					/* Remove the corpse from Alien Containment. */
					baseCurrent->alienscont[i].amount_dead--;
					break;
				}
				cnt++;
			}
			if (baseCurrent->alienscont[i].alientype && baseCurrent->alienscont[i].amount_alive > 0) {
				if (cnt == num) {
					if (TR_CheckAlien(i, baseCurrent, transferBase)) {
						trAliensTmp[i][0]++;
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
	case 3:		/**< aircrafts */
		/* @todo: aircrafts here. */
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
 * @brief Callback for aircraft list click.
 * @todo REMOVEME
 */
static void TR_TransferAircraftListClick_f (void)
{
	int i, j = -1, num;
	/* initialize - maybe there are no aircraft in base */
	aircraft_t* aircraft = NULL;

	if (!baseCurrent)
		return;

	if (Cmd_Argc() < 2)
		return;

	num = atoi(Cmd_Argv(1));

	for (i = 0; i < baseCurrent->numAircraftInBase; i++) {
		aircraft = &baseCurrent->aircraft[i];
		if (aircraft->status == AIR_HOME) {
			j++;
			if (j == num)
				break;
		}
	}

	if (j < 0)
		return;
}

/**
 * @brief Finds the destination base.
 */
static void TR_TransferBaseSelectPopup_f (void)
{
	int i, j = -1, num;
	base_t* base;

	if (!baseCurrent)
		return;

	if (Cmd_Argc() < 2) {
		Com_Printf("usage: trans_baselist_click <type>\n");
		return;
	}

	num = atoi(Cmd_Argv(1));

	for (i = 0, base = gd.bases; i < gd.numBases; i++, base++) {
		if (base->founded == qfalse || base == baseCurrent)
			continue;
		j++;
		if (j == num) {
			break;
		}
	}

	/* no base founded */
	if (j < 0 || i == gd.numBases)
		return;

	transferBase = base;
}

/**
 * @brief Callback for base list click.
 * @note transferBase is being set here.
 */
static void TR_TransferBaseSelect_f (void)
{
	static char baseInfo[1024];
	/*char str[128];*/
	int j = -1, num, i;
	base_t* base;

	if (!baseCurrent)
		return;

	if (Cmd_Argc() < 2) {
		Com_Printf("usage: trans_bases_click <type>\n");
		return;
	}

	num = atoi(Cmd_Argv(1));

	for (i = 0, base = gd.bases; i < gd.numBases; i++, base++) {
		if (base->founded == qfalse || base == baseCurrent)
			continue;
		j++;
		if (j == num) {
			break;
		}
	}

	/* No base founded. */
	if (j < 0 || i == gd.numBases)
		return;

	Com_sprintf(baseInfo, sizeof(baseInfo), "%s\n\n", base->name);

	if (base->hasStorage) {
		Q_strcat(baseInfo, _("You can transfer equipment - this base has a Storage.\n"), sizeof(baseInfo));
	} else {
		Q_strcat(baseInfo, _("No Storage in this base.\n"), sizeof(baseInfo));
	}
	if (base->hasQuarters) {
		Q_strcat(baseInfo, _("You can transfer employees - this base has Living Quarters.\n"), sizeof(baseInfo));
	} else {
		Q_strcat(baseInfo, _("No Living Quarters in this base.\n"), sizeof(baseInfo));
	}
	if (base->hasAlienCont) {
		Q_strcat(baseInfo, _("You can transfer Aliens - this base has an Alien Containment.\n"), sizeof(baseInfo));
	} else {
		Q_strcat(baseInfo, _("No Alien Containment in this base.\n"), sizeof(baseInfo));
	}
	if (base->hasAmStorage) {
		Q_strcat(baseInfo, _("You can transfer antimatter - this base has an Antimatter Storage.\n"), sizeof(baseInfo));
	} else {
		Q_strcat(baseInfo, _("No Antimatter Storage in this base.\n"), sizeof(baseInfo));
	}

	/* @todo: check hangar (for aircraft transfer) */

	menuText[TEXT_BASE_INFO] = baseInfo;

	/* Set global pointer to current selected base. */
	transferBase = base;

	Cvar_Set("mn_trans_base_name", transferBase->name);

	/* Update stuff-in-base list. */
	TR_TransferSelect_f();
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
	case 1:		/**< items */
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
	case 2:		/**< employees */
		for (i = 0; i < MAX_CARGO; i++) {
			/* Count previous types on the list. */
			if (cargo[i].type == 1)
				entries++;
		}
		/* Start increasing cnt from the amount of previous entries. */
		cnt = entries;
		for (i = 0; i < gd.numEmployees[EMPL_SOLDIER]; i++) {
			if (trEmployeesTmp[EMPL_SOLDIER][i] > -1) {
				if (cnt == num) {
					trEmployeesTmp[EMPL_SOLDIER][i] = -1;
					removed = qtrue;
					break;
				}
				cnt++;
			}
		}
		if (removed)	/* We already removed soldier, break here. */
			break;
		Com_Printf("cnt: %i, num: %i\n", cnt, num);
		for (i = 0; i < MAX_EMPL; i++) {
			if ((E_CountHired(baseCurrent, i) < 1) || (i == EMPL_SOLDIER))
				continue;
			if (cnt == num) {
				for (j = 0; j < gd.numEmployees[i]; j++) {
					if (trEmployeesTmp[i][j] > -1) {
						trEmployeesTmp[i][j] = -1;
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
	case 3:		/**< alien bodies */
		for (i = 0; i < MAX_CARGO; i++) {
			/* Count previous types on the list. */
			if (cargo[i].type == 1 || cargo[i].type == 2)
				entries++;
		}
		/* Start increasing cnt from the amount of previous entries. */
		cnt = entries;
		for (i = 0; i < numTeamDesc; i++) {
			if (trAliensTmp[i][1] > 0) {
				if (cnt == num) {
					trAliensTmp[i][1]--;
					baseCurrent->alienscont[i].amount_dead++;
					break;
				}
				cnt++;
			}
		}
		break;
	case 4:		/**< alive aliens */
		for (i = 0; i < MAX_CARGO; i++) {
			/* Count previous types on the list. */
			if (cargo[i].type == 1 || cargo[i].type == 2 || cargo[i].type == 3)
				entries++;
		}
		/* Start increasing cnt from the amount of previous entries. */
		cnt = entries;
		for (i = 0; i < numTeamDesc; i++) {
			if (trAliensTmp[i][0] > 0) {
				if (cnt == num) {
					trAliensTmp[i][0]--;
					baseCurrent->alienscont[i].amount_alive--;
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
	transferlist_t *transfer;

	for (i = 0; i < MAX_TRANSFERS; i++) {
		transfer = &gd.alltransfers[i];
		if (transfer->event.day == ccs.date.day) {	/* @todo make it checking hour also, not only day */
			base = &gd.bases[transfer->destBase];
			if (!base->founded) {
				/* Destination base was destroyed meanwhile. */
				/* Transfer is lost, send proper message to the user.*/
				/* @todo: prepare MN_AddNewMessage() here */

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
	static char baseList[1024];
	static char aircraftList[1024];
	int i, j;
	base_t* base = NULL;
	aircraft_t* aircraft = NULL;

	if (!baseCurrent)
		return;

	transferBase = NULL;

	baseList[0] = '\0';
	aircraftList[0] = '\0';

	if (Cmd_Argc() < 2)
		Com_Printf("warning: you should call trans_init with parameter 0\n");

	/* Fill in the bases node with every founded base except current. */
	for (i = 0, base = gd.bases; i < gd.numBases; i++, base++) {
		if (base->founded == qfalse || base == baseCurrent)
			continue;
		Q_strcat(baseList, base->name, sizeof(baseList));
		Q_strcat(baseList, "\n", sizeof(baseList));
	}

	/* Clear employees temp array. */
	for (i = 0; i < MAX_EMPL; i++) {
		for (j = 0; j < MAX_EMPLOYEES; j++)
			trEmployeesTmp[i][j] = -1;
	}

	/* Select first available item. */
	TR_TransferSelect_f();

	/* Fill in the aircrafts node with every aircraft in current base. */
	for (i = 0; i < baseCurrent->numAircraftInBase; i++) {
		aircraft = &baseCurrent->aircraft[i];
		if (aircraft->status == AIR_HOME) {
			Q_strcat(aircraftList, aircraft->shortname, sizeof(aircraftList));
			Q_strcat(aircraftList, "\n", sizeof(aircraftList));
		}
	}

	/* Fill up the nodes in case of lack of aircraft (none at all or none suitable). */
	if (!aircraft) {
		Q_strcat(aircraftList, _("None"), sizeof(aircraftList));
		Cvar_Set("mn_trans_aircraft_name", _("None"));
	}

	/* Select first available base. */
	TR_TransferBaseSelect_f();

	/* Set up cvar used to display transferBase. */
	if (transferBase)
		Cvar_Set("mn_trans_base_name", transferBase->name);

	menuText[TEXT_BASE_LIST] = baseList;
	menuText[TEXT_AIRCRAFT_LIST] = aircraftList;
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
	Cmd_AddCommand("trans_baselist_click", TR_TransferBaseSelectPopup_f, "Callback for transfer base list popup");
	Cmd_AddCommand("trans_bases_click", TR_TransferBaseSelect_f, "Callback for base list node click");
	Cmd_AddCommand("trans_list_click", TR_TransferListSelect_f, "Callback for transfer list node click");
	Cmd_AddCommand("trans_aircraft_click", TR_TransferAircraftListClick_f, "Callback for aircraft list node click");
	Cmd_AddCommand("trans_cargolist_click", TR_CargoListSelect_f, "Callback for cargo list node click");
}


