/**
 * @file cl_transfer.c
 * @brief Deals with the Transfer stuff.
 * @note Transfer menu functions prefix: TR_
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

#include "../client.h"
#include "../cl_global.h"
#include "../cl_team.h"
#include "../menu/m_popup.h"
#include "cl_uforecovery.h"
#include "cl_map.h"
#include "cl_aliencont.h"

/** @brief Current selected aircraft for transfer (if transfer started from mission). */
static aircraft_t *transferStartAircraft = NULL;

/** @brief Current selected base for transfer. */
static base_t *transferBase = NULL;

/** @brief Current transfer type (item, employee, alien, aircraft). */
static transferType_t currentTransferType = TRANS_TYPE_ITEM;

/** @brief Current cargo onboard. */
static transferCargo_t cargo[MAX_CARGO];

/** @brief Current item cargo. */
static int trItemsTmp[MAX_OBJDEFS];

/** @brief Current alien cargo. [0] alive [1] dead */
static int trAliensTmp[MAX_TEAMDEFS][TRANS_ALIEN_MAX];

/** @brief Current personnel cargo. */
static employee_t *trEmployeesTmp[MAX_EMPL][MAX_EMPLOYEES];

/** @brief Current aircraft for transfer. */
static int trAircraftsTmp[MAX_AIRCRAFT];

/** @brief Current cargo type count (updated in TR_CargoList) */
static int trCargoCountTmp = 0;

/** @brief Max values for transfer factors. */
static const int MAX_TR_FACTORS = 500;

/**
 * @brief Checks condition for item transfer.
 * @param[in] od Pointer to object definition.
 * @param[in] destbase Pointer to destination base.
 * @param[in] amount Number of items to transfer.
 * @return Number of items that can be transfered.
 */
static int TR_CheckItem (const objDef_t *od, const base_t *destbase, int amount)
{
	int i, intransfer = 0, amtransfer = 0;
	int smallufotransfer = 0, bigufotransfer = 0;

	assert(od);
	assert(destbase);

	/* Count size of all items already on the transfer list. */
	for (i = 0; i < csi.numODs; i++) {
		if (trItemsTmp[i] > 0) {
			if (!Q_strncmp(csi.ods[i].id, "antimatter", 10))
				amtransfer = ANTIMATTER_SIZE * trItemsTmp[i];
			if (od->tech->type == RS_CRAFT) {
				aircraft_t *ufocraft = AIR_GetAircraft(od->tech->provides);
				assert(ufocraft);
				if (ufocraft->size == AIRCRAFT_LARGE)
					bigufotransfer++;
				else
					smallufotransfer++;
			} else
				intransfer += csi.ods[i].size * trItemsTmp[i];
		}
	}

	/* Is this antimatter and destination base has enough space in Antimatter Storage? */
	if (!Q_strncmp(od->id, "antimatter", 10)) {
		/* Give some meaningful feedback to the player if the player clicks on an a.m. item but base doesn't have am storage. */
		if (!B_GetBuildingStatus(destbase, B_ANTIMATTER) && B_GetBuildingStatus(destbase, B_STORAGE)) {
			MN_Popup(_("Missing storage"), _("Destination base does not have an Antimatter Storage.\n"));
			return qfalse;
		} else if (!B_GetBuildingStatus(destbase, B_ANTIMATTER)) {	/* Return if the target base doesn't have antimatter storage or power. */
			return 0;
		}
		amount = min(amount, destbase->capacities[CAP_ANTIMATTER].max - destbase->capacities[CAP_ANTIMATTER].cur - amtransfer / ANTIMATTER_SIZE);
		if (amount <= 0) {
			MN_Popup(_("Not enough space"), _("Destination base does not have enough\nAntimatter Storage space to store more antimatter.\n"));
			return 0;
		} else {
			/* amount to transfer can't be bigger than what we have */
			amount = min(amount, (destbase->capacities[CAP_ANTIMATTER].max - destbase->capacities[CAP_ANTIMATTER].cur - amtransfer) / ANTIMATTER_SIZE);
		}
	} else if (od->tech->type == RS_CRAFT) { /* This is UFO craft */
		aircraft_t *ufocraft = AIR_GetAircraft(od->tech->provides);
		assert(ufocraft);
		if (ufocraft->size == AIRCRAFT_LARGE) {
			if (!B_GetBuildingStatus(destbase, B_UFO_HANGAR)) {
				MN_Popup(_("Missing Large UFO Hangar"), _("Destination base does not have functional Large UFO Hangar.\n"));
				return 0;
			} else {
				amount = min(amount, destbase->capacities[CAP_UFOHANGARS_LARGE].max - destbase->capacities[CAP_UFOHANGARS_LARGE].cur - bigufotransfer);
				if (amount <= 0) {
					MN_Popup(_("Missing Large UFO Hangar"), _("Destination base does not have enough Large UFO Hangar.\n"));
					return 0;
				}
			}
		} else if (ufocraft->size == AIRCRAFT_SMALL) {
			if (!B_GetBuildingStatus(destbase, B_UFO_SMALL_HANGAR)) {
				MN_Popup(_("Missing Small UFO Hangar"), _("Destination base does not have functional Small UFO Hangar.\n"));
				return qfalse;
			} else {
				amount = min(amount, destbase->capacities[CAP_UFOHANGARS_SMALL].max - destbase->capacities[CAP_UFOHANGARS_SMALL].cur - smallufotransfer);
				if (amount <= 0) {
					MN_Popup(_("Missing Small UFO Hangar"), _("Destination base does not have enough Small UFO Hangar.\n"));
					return 0;
				}
			}
		}
	} else {	/*This is not antimatter*/
		if (!B_GetBuildingStatus(destbase, B_STORAGE))	/* Return if the target base doesn't have storage or power. */
			return 0;

		/* Does the destination base has enough space in storage? */
		amount = min(amount, destbase->capacities[CAP_ITEMS].max - destbase->capacities[CAP_ITEMS].cur - intransfer / od->size);
		if (amount <= 0) {
			MN_Popup(_("Not enough space"), _("Destination base does not have enough\nStorage space to store this item.\n"));
			return 0;
		}
	}

	return amount;
}

/**
 * @brief Checks condition for employee transfer.
 * @param[in] employee Pointer to employee for transfer.
 * @param[in] destbase Pointer to destination base.
 * @return qtrue if transfer of this type of employee is possible.
 */
static qboolean TR_CheckEmployee (const employee_t *employee, const base_t *destbase)
{
	int i, intransfer = 0;
	employeeType_t emplType;

	assert(employee && destbase);

	/* Count amount of all employees already on the transfer list. */
	for (emplType = 0; emplType < MAX_EMPL; emplType++) {
		for (i = 0; i < gd.numEmployees[emplType]; i++) {
			if (trEmployeesTmp[emplType][i])
				intransfer++;
		}
	}

	/* Does the destination base has enough space in living quarters? */
	if (destbase->capacities[CAP_EMPLOYEES].max - destbase->capacities[CAP_EMPLOYEES].cur - intransfer < 1) {
		MN_Popup(_("Not enough space"), _("Destination base does not have enough space\nin Living Quarters.\n"));
		return qfalse;
	}

	switch (employee->type) {
	case EMPL_SOLDIER:
		/* Is this a soldier assigned to aircraft? */
		if (AIR_IsEmployeeInAircraft(employee, NULL)) {
			Com_sprintf(popupText, sizeof(popupText), _("%s %s is assigned to aircraft and cannot be\ntransfered to another base.\n"),
				_(gd.ranks[employee->chr.score.rank].shortname), employee->chr.name);
			MN_Popup(_("Soldier in aircraft"), popupText);
			return qfalse;
		}
		break;
	case EMPL_PILOT:
		/* Is this a pilot assigned to aircraft? */
		if (AIR_IsEmployeeInAircraft(employee, NULL)) {
			Com_sprintf(popupText, sizeof(popupText), _("%s is assigned to aircraft and cannot be\ntransfered to another base.\n"),
				employee->chr.name);
			MN_Popup(_("Pilot in aircraft"), popupText);
			return qfalse;
		}
		break;
	default:
		break;
	}

	return qtrue;
}

/**
 * @brief Checks condition for live alien transfer.
 * @param[in] alienidx Index of an alien type.
 * @param[in] destbase Pointer to destination base.
 * @return qtrue if transfer of this type of alien is possible.
 */
static qboolean TR_CheckAlien (int alienidx, base_t *destbase)
{
	int i, intransfer = 0;

	assert(destbase);

	/* Count amount of live aliens already on the transfer list. */
	for (i = 0; i < gd.numAliensTD; i++) {
		if (trAliensTmp[i][TRANS_ALIEN_ALIVE] > 0)
			intransfer += trAliensTmp[i][TRANS_ALIEN_ALIVE];
	}

	/* add the alien we are trying to transfer */
	intransfer++;

	/* Does the destination base has enough space in alien containment? */
	if (!AL_CheckAliveFreeSpace(destbase, NULL, intransfer)) {
		MN_Popup(_("Not enough space"), _("Destination base does not have enough space\nin Alien Containment.\n"));
		return qfalse;
	}

	return qtrue;
}

/**
 * @brief Checks condition for aircraft transfer.
 * @param[in] aircraft Pointer to aircraft which is going to be added to transferlist.
 * @param[in] destbase Pointer to destination base.
 * @return qtrue if transfer of this aircraft is possible.
 */
static qboolean TR_CheckAircraft (const aircraft_t *aircraft, const base_t *destbase)
{
	int i, hangarStorage, numAircraftTransfer = 0;
	assert(aircraft);
	assert(destbase);

	/* Count weight and number of all aircraft already on the transfer list that goes
	 * into the same hangar type than aircraft. */
	for (i = 0; i < MAX_AIRCRAFT; i++)
		if (trAircraftsTmp[i] > TRANS_LIST_EMPTY_SLOT) {
			const aircraft_t *aircraftTemp = AIR_AircraftGetFromIDX(i);
			assert(aircraftTemp);
			if (aircraftTemp->size == aircraft->size)
				numAircraftTransfer++;
		}

	/* Hangars in destbase functional? */
	if (!B_GetBuildingStatus(destbase, B_POWER)) {
		MN_Popup(_("Hangars not ready"), _("Destination base does not have hangars ready.\nProvide power supplies.\n"));
		return qfalse;
	} else if (!B_GetBuildingStatus(destbase, B_COMMAND)) {
		MN_Popup(_("Hangars not ready"), _("Destination base does not have command centre.\nHangars not functional.\n"));
		return qfalse;
	} else if (!B_GetBuildingStatus(destbase, B_HANGAR) && !B_GetBuildingStatus(destbase, B_SMALL_HANGAR)) {
		MN_Popup(_("Hangars not ready"), _("Destination base does not have any hangar."));
		return qfalse;
	}
	/* Is there a place for this aircraft in destination base? */
	hangarStorage = AIR_CalculateHangarStorage(aircraft->tpl, destbase, numAircraftTransfer);
	if (hangarStorage == 0) {
		MN_Popup(_("Not enough space"), _("Destination base does not have enough space\nin hangars.\n"));
		return qfalse;
	} else if (hangarStorage > 0) {
		return qtrue;
	}

	return qtrue;
}

/**
 * @brief Display cargo list.
 */
static void TR_CargoList (void)
{
	int i = 0;
	employeeType_t emplType;
	int trempl[MAX_EMPL];
	linkedList_t *cargoList = NULL;
	char str[128];

	trCargoCountTmp = 0;
	memset(cargo, 0, sizeof(cargo));
	memset(trempl, 0, sizeof(trempl));

	/* Show items. */
	for (i = 0; i < csi.numODs; i++) {
		if (trItemsTmp[i] > 0) {
			Com_sprintf(str, sizeof(str), _("%s (%i for transfer)"),
				_(csi.ods[i].name), trItemsTmp[i]);
			LIST_AddString(&cargoList, str);
			cargo[trCargoCountTmp].type = CARGO_TYPE_ITEM;
			cargo[trCargoCountTmp].itemidx = i;
			trCargoCountTmp++;
			if (trCargoCountTmp >= MAX_CARGO) {
				Com_DPrintf(DEBUG_CLIENT, "TR_CargoList: Cargo is full\n");
				break;
			}
		}
	}

	/* Show employees. */
	for (emplType = 0; emplType < MAX_EMPL; emplType++) {
		for (i = 0; i < gd.numEmployees[emplType]; i++) {
			if (trEmployeesTmp[emplType][i]) {
				if (emplType == EMPL_SOLDIER || emplType == EMPL_PILOT) {
					employee_t *employee = trEmployeesTmp[emplType][i];
					Com_sprintf(str, sizeof(str), (emplType == EMPL_SOLDIER) ? _("Soldier %s %s") : _("Pilot %s %s"),
						_(gd.ranks[employee->chr.score.rank].shortname), employee->chr.name);
					LIST_AddString(&cargoList, str);
					cargo[trCargoCountTmp].type = CARGO_TYPE_EMPLOYEE;
					cargo[trCargoCountTmp].itemidx = employee->idx;
					trCargoCountTmp++;
					if (trCargoCountTmp >= MAX_CARGO) {
						Com_DPrintf(DEBUG_CLIENT, "TR_CargoList: Cargo is full\n");
						break;
					}
				}
				trempl[emplType]++;
			}
		}
	}
	for (emplType = 0; emplType < MAX_EMPL; emplType++) {
		if (emplType == EMPL_SOLDIER || emplType == EMPL_PILOT)
			continue;
		if (trempl[emplType] > 0) {
			Com_sprintf(str, sizeof(str), _("%s (%i for transfer)"),
				E_GetEmployeeString(emplType), trempl[emplType]);
			LIST_AddString(&cargoList, str);
			cargo[trCargoCountTmp].type = CARGO_TYPE_EMPLOYEE;
			trCargoCountTmp++;
			if (trCargoCountTmp >= MAX_CARGO) {
				Com_DPrintf(DEBUG_CLIENT, "TR_CargoList: Cargo is full\n");
				break;
			}
		}
	}

	/* Show aliens. */
	for (i = 0; i < gd.numAliensTD; i++) {
		if (trAliensTmp[i][TRANS_ALIEN_DEAD] > 0) {
			Com_sprintf(str, sizeof(str), _("Corpse of %s (%i for transfer)"),
				_(AL_AlienTypeToName(AL_GetAlienGlobalIDX(i))), trAliensTmp[i][TRANS_ALIEN_DEAD]);
			LIST_AddString(&cargoList, str);
			cargo[trCargoCountTmp].type = CARGO_TYPE_ALIEN_DEAD;
			cargo[trCargoCountTmp].itemidx = i;
			trCargoCountTmp++;
			if (trCargoCountTmp >= MAX_CARGO) {
				Com_DPrintf(DEBUG_CLIENT, "TR_CargoList: Cargo is full\n");
				break;
			}
		}
	}
	for (i = 0; i < gd.numAliensTD; i++) {
		if (trAliensTmp[i][TRANS_ALIEN_ALIVE] > 0) {
			Com_sprintf(str, sizeof(str), _("%s (%i for transfer)"),
				_(AL_AlienTypeToName(AL_GetAlienGlobalIDX(i))), trAliensTmp[i][TRANS_ALIEN_ALIVE]);
			LIST_AddString(&cargoList, str);
			cargo[trCargoCountTmp].type = CARGO_TYPE_ALIEN_ALIVE;
			cargo[trCargoCountTmp].itemidx = i;
			trCargoCountTmp++;
			if (trCargoCountTmp >= MAX_CARGO) {
				Com_DPrintf(DEBUG_CLIENT, "TR_CargoList: Cargo is full\n");
				break;
			}
		}
	}

	/* Show all aircraft. */
	for (i = 0; i < MAX_AIRCRAFT; i++) {
		if (trAircraftsTmp[i] > TRANS_LIST_EMPTY_SLOT) {
			aircraft_t *aircraft = AIR_AircraftGetFromIDX(trAircraftsTmp[i]);
			assert(aircraft);
			Com_sprintf(str, sizeof(str), _("Aircraft %s"), _(aircraft->name));
			LIST_AddString(&cargoList, str);
			cargo[trCargoCountTmp].type = CARGO_TYPE_AIRCRAFT;
			cargo[trCargoCountTmp].itemidx = i;
			trCargoCountTmp++;
			if (trCargoCountTmp >= MAX_CARGO) {
				Com_DPrintf(DEBUG_CLIENT, "TR_CargoList: Cargo is full\n");
				break;
			}
		}
	}

	MN_MenuTextReset(TEXT_CARGO_LIST);
	MN_RegisterLinkedListText(TEXT_CARGO_LIST, cargoList);
}

/**
 * @brief Check if an aircraft should be displayed for transfer.
 * @param[in] i global idx of the aircraft
 * @return qtrue if the aircraft should be displayed, qfalse else.
 */
static qboolean TR_AircraftListSelect (int i)
{
	aircraft_t *aircraft;

	if (trAircraftsTmp[i] > TRANS_LIST_EMPTY_SLOT)	/* Already on transfer list. */
		return qfalse;

	aircraft = AIR_AircraftGetFromIDX(i);
	if (!AIR_IsAircraftInBase(aircraft))	/* Aircraft is not in base. */
		return qfalse;

	return qtrue;
}

/**
 * @brief Function gives the user friendly name of a transfer category
 */
static inline const char *TR_CategoryName (const transferType_t cat)
{
	switch (cat) {
	case TRANS_TYPE_ITEM:
		return _("Equipment");
	case TRANS_TYPE_EMPLOYEE:
		return _("Employees");
	case TRANS_TYPE_ALIEN:
		return _("Aliens");
	case TRANS_TYPE_AIRCRAFT:
		return _("Aircraft");
	case TRANS_TYPE_MAX:
	case TRANS_TYPE_INVALID:
		/* shouldn't happen */
		break;
	}
	Sys_Error("TR_CategoryName: invalid transfer type %i", cat);
}

/**
 * @brief Reset scrolling of node containing item-in-base list.
 */
static void TR_ResetScrolling_f (void)
{
	/* for reseting the scrolling */
	static menuNode_t *trans_list = NULL;
	static menuNode_t *trans_list_scroll = NULL;

	if (!trans_list) {
		trans_list = MN_GetNodeFromCurrentMenu("trans_list");
	}
	if (!trans_list_scroll) {
		trans_list_scroll = MN_GetNodeFromCurrentMenu("trans_list_scroll");
	}

	/* maybe we call this function and transfer menu is not on the menu stack */
	if (!trans_list || !trans_list_scroll) {
		return;
	}

	trans_list->u.text.textScroll = 0;
	trans_list_scroll->u.abstractscrollbar.pos = 0;
}

static void TR_TransferSelect (base_t *srcbase, base_t *destbase, transferType_t transferType)
{
	linkedList_t *transferList = NULL;
	int numempl[MAX_EMPL], trempl[MAX_EMPL];
	int i, j, cnt = 0;
	char str[128];

	/* reset for every new call */
	MN_MenuTextReset(TEXT_TRANSFER_LIST);

	/* Reset and fill temp employees arrays. */
	for (i = 0; i < MAX_EMPL; i++) {
		trempl[i] = numempl[i] = 0;
		for (j = 0; j < MAX_EMPLOYEES; j++) {
			if (trEmployeesTmp[i][j])
				trempl[i]++;
		}
	}

	switch (transferType) {
	case TRANS_TYPE_ITEM:
		if (B_GetBuildingStatus(destbase, B_STORAGE)) {
			for (i = 0; i < csi.numODs; i++)
				if (srcbase->storage.num[i]) {
					if (trItemsTmp[i] > 0)
						Com_sprintf(str, sizeof(str), _("%s (%i for transfer, %i left)"), _(csi.ods[i].name), trItemsTmp[i], srcbase->storage.num[i]);
					else
						Com_sprintf(str, sizeof(str), _("%s (%i available)"), _(csi.ods[i].name), srcbase->storage.num[i]);
					LIST_AddString(&transferList, str);
					cnt++;
				}
			if (!cnt)
				LIST_AddString(&transferList, _("Storage is empty."));
		} else if (B_GetBuildingStatus(destbase, B_POWER)) {
			LIST_AddString(&transferList, _("Transfer is not possible - the base doesn't have a Storage."));
		} else {
			LIST_AddString(&transferList, _("Transfer is not possible - the base does not have power supplies."));
		}
		break;
	case TRANS_TYPE_EMPLOYEE:
		if (B_GetBuildingStatus(destbase, B_QUARTERS)) {
			employeeType_t emplType;
			for (emplType = 0; emplType < MAX_EMPL; emplType++) {
				for (i = 0; i < gd.numEmployees[emplType]; i++) {
					const employee_t *employee = &gd.employees[emplType][i];
					if (!E_IsInBase(employee, srcbase))
						continue;
					if (trEmployeesTmp[emplType][i])	/* Already on transfer list. */
						continue;
					if (emplType == EMPL_SOLDIER || emplType == EMPL_PILOT) {
						Com_sprintf(str, sizeof(str), (emplType == EMPL_SOLDIER) ? _("Soldier %s %s") : _("Pilot %s %s"), _(gd.ranks[employee->chr.score.rank].shortname), employee->chr.name);
						LIST_AddString(&transferList, str);
						cnt++;
					}
					numempl[emplType]++;
				}
			}
			for (i = 0; i < MAX_EMPL; i++) {
				if (i == EMPL_SOLDIER || i == EMPL_PILOT)
					continue;
				if ((trempl[i] > 0) && (numempl[i] > 0))
					Com_sprintf(str, sizeof(str), _("%s (%i for transfer, %i left)"), E_GetEmployeeString(i), trempl[i], numempl[i]);
				else if (numempl[i] > 0)
					Com_sprintf(str, sizeof(str), _("%s (%i available)"), E_GetEmployeeString(i), numempl[i]);
				if (numempl[i] > 0) {
					LIST_AddString(&transferList, str);
					cnt++;
				}
			}
			if (!cnt)
				LIST_AddString(&transferList, _("Living Quarters empty."));
		} else
			LIST_AddString(&transferList, _("Transfer is not possible - the base doesn't have Living Quarters."));
		break;
	case TRANS_TYPE_ALIEN:
		if (B_GetBuildingStatus(destbase, B_ALIEN_CONTAINMENT)) {
			for (i = 0; i < gd.numAliensTD; i++) {
				if (srcbase->alienscont[i].teamDef && srcbase->alienscont[i].amount_dead > 0) {
					if (trAliensTmp[i][TRANS_ALIEN_DEAD] > 0)
						Com_sprintf(str, sizeof(str), _("Corpse of %s (%i for transfer, %i left)"),
						_(AL_AlienTypeToName(AL_GetAlienGlobalIDX(i))), trAliensTmp[i][TRANS_ALIEN_DEAD],
						srcbase->alienscont[i].amount_dead);
					else
						Com_sprintf(str, sizeof(str), _("Corpse of %s (%i available)"),
						_(AL_AlienTypeToName(AL_GetAlienGlobalIDX(i))), srcbase->alienscont[i].amount_dead);
					LIST_AddString(&transferList, str);
					cnt++;
				}
				if (srcbase->alienscont[i].teamDef && srcbase->alienscont[i].amount_alive > 0) {
					if (trAliensTmp[i][TRANS_ALIEN_ALIVE] > 0)
						Com_sprintf(str, sizeof(str), _("Alive %s (%i for transfer, %i left)"),
						_(AL_AlienTypeToName(AL_GetAlienGlobalIDX(i))), trAliensTmp[i][TRANS_ALIEN_ALIVE],
						srcbase->alienscont[i].amount_alive);
					else
						Com_sprintf(str, sizeof(str), _("Alive %s (%i available)"),
						_(AL_AlienTypeToName(AL_GetAlienGlobalIDX(i))), srcbase->alienscont[i].amount_alive);
					LIST_AddString(&transferList, str);
					cnt++;
				}
			}
			if (!cnt)
				LIST_AddString(&transferList, _("Alien Containment is empty."));
		} else if (B_GetBuildingStatus(destbase, B_POWER)) {
			LIST_AddString(&transferList, _("Transfer is not possible - the base doesn't have an Alien Containment."));
		} else {
			LIST_AddString(&transferList, _("Transfer is not possible - the base does not have power supplies."));
		}
		break;
	case TRANS_TYPE_AIRCRAFT:
		if (B_GetBuildingStatus(destbase, B_HANGAR) || B_GetBuildingStatus(destbase, B_SMALL_HANGAR)) {
			for (i = 0; i < MAX_AIRCRAFT; i++) {
				aircraft_t *aircraft = AIR_AircraftGetFromIDX(i);
				if (aircraft) {
					if ((aircraft->homebase == srcbase) && TR_AircraftListSelect(i)) {
						Com_sprintf(str, sizeof(str), _("Aircraft %s"), _(aircraft->name));
						LIST_AddString(&transferList, str);
						cnt++;
					}
				}
			}
			if (!cnt)
				LIST_AddString(&transferList, _("No aircraft available for transfer."));
		} else {
			LIST_AddString(&transferList, _("Transfer is not possible - the base doesn't have a functional hangar."));
		}
		break;
	default:
		Com_Printf("TR_TransferSelect: Unknown transferType id %i\n", transferType);
		return;
	}

	/* Update cargo list. */
	TR_CargoList();

	currentTransferType = transferType;
	Cvar_Set("mn_transcat_name", TR_CategoryName(currentTransferType));
	MN_RegisterLinkedListText(TEXT_TRANSFER_LIST, transferList);
}

/**
 * @brief Fills the items-in-base list with stuff available for transfer.
 * @note Filling the transfer list with proper stuff (items/employees/aliens/aircraft) is being done here.
 * @sa TR_TransferStart_f
 * @sa TR_TransferInit_f
 */
static void TR_TransferSelect_f (void)
{
	int type;

	if (!transferBase || !baseCurrent)
		return;

	if (Cmd_Argc() < 2)
		type = currentTransferType;
	else
		type = atoi(Cmd_Argv(1));

	if (type < TRANS_TYPE_ITEM || type >= TRANS_TYPE_MAX)
		return;

	TR_TransferSelect(baseCurrent, transferBase, type);
}

/**
 * @brief Rolls Category left in Transfer menu.
 */
static void TR_Prev_Category_f (void)
{
	currentTransferType--;

	if (currentTransferType < TRANS_TYPE_ITEM)
		currentTransferType = TRANS_TYPE_MAX - 1;

	Cbuf_AddText(va("trans_resetscroll; trans_select %i;\n", currentTransferType));
}

/**
 * @brief Rolls Category right in Transfer menu.
 */
static void TR_Next_Category_f (void)
{
	currentTransferType++;

	if (currentTransferType >= TRANS_TYPE_MAX)
		currentTransferType = TRANS_TYPE_ITEM;

	Cbuf_AddText(va("trans_resetscroll;trans_select %i;\n", currentTransferType));
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
				B_ManageAntimatter(baseCurrent, trItemsTmp[i], qtrue);
			else if (csi.ods[i].tech->type == RS_CRAFT) { /* This is UFO craft */
				const aircraft_t *ufocraft = AIR_GetAircraft(csi.ods[i].tech->provides);
				assert(ufocraft);
				/* don't use B_UpdateStorageAndCapacity: UFO are not stored in storage */
				baseCurrent->storage.num[i] += trItemsTmp[i];
				if (ufocraft->size == AIRCRAFT_LARGE)
					baseCurrent->capacities[CAP_UFOHANGARS_LARGE].cur++;
				else
					baseCurrent->capacities[CAP_UFOHANGARS_SMALL].cur++;
			} else
				B_UpdateStorageAndCapacity(baseCurrent, &csi.ods[i], trItemsTmp[i], qfalse, qfalse);
		}
	}
	for (i = 0; i < gd.numAliensTD; i++) {	/* Return aliens. */
		if (trAliensTmp[i][TRANS_ALIEN_ALIVE] > 0)
			AL_ChangeAliveAlienNumber(baseCurrent, &(baseCurrent->alienscont[i]), trAliensTmp[i][TRANS_ALIEN_ALIVE]);
		if (trAliensTmp[i][TRANS_ALIEN_DEAD] > 0)
			baseCurrent->alienscont[i].amount_dead += trAliensTmp[i][TRANS_ALIEN_DEAD];
	}

	/* Clear temporary cargo arrays. */
	memset(trItemsTmp, 0, sizeof(trItemsTmp));
	memset(trAliensTmp, 0, sizeof(trAliensTmp));
	memset(trEmployeesTmp, 0, sizeof(trEmployeesTmp));
	memset(trAircraftsTmp, TRANS_LIST_EMPTY_SLOT, sizeof(trAircraftsTmp));
	/* Update cargo list and items list. */
	TR_CargoList();
	TR_TransferSelect(baseCurrent, transferBase, currentTransferType);
	TR_ResetScrolling_f();
}

/**
 * @brief Unloads transfer cargo when finishing the transfer or destroys it when no buildings/base.
 * @param[in,out] destination The destination base - might be NULL in case the base
 * is already destroyed
 * @param[in] transfer Pointer to transfer in gd.alltransfers.
 * @param[in] success True if the transfer reaches dest base, false if the base got destroyed.
 * @sa TR_TransferEnd
 */
static void TR_EmptyTransferCargo (base_t *destination, transfer_t *transfer, qboolean success)
{
	int i, j;

	assert(transfer);

	if (transfer->hasItems && success) {	/* Items. */
		if (!B_GetBuildingStatus(destination, B_STORAGE)) {
			Com_sprintf(cp_messageBuffer, sizeof(cp_messageBuffer), _("Base %s does not have Storage, items are removed!"), destination->name);
			MSO_CheckAddNewMessage(NT_TRANSFER_LOST,_("Transport mission"), cp_messageBuffer, qfalse, MSG_TRANSFERFINISHED, NULL);
			/* Items cargo is not unloaded, will be destroyed in TR_TransferCheck(). */
		} else {
			for (i = 0; i < csi.numODs; i++) {
				if (transfer->itemAmount[i] > 0) {
					if (!Q_strncmp(csi.ods[i].id, "antimatter", 10))
						B_ManageAntimatter(destination, transfer->itemAmount[i], qtrue);
					else if (csi.ods[i].tech->type == RS_CRAFT) { /* This is UFO craft */
						const aircraft_t *ufocraft = AIR_GetAircraft(csi.ods[i].tech->provides);
						assert(ufocraft);
						for (j = 0; j < transfer->itemAmount[i]; j++) {
							if (UR_ConditionsForStoring(destination, ufocraft)) {
								/* don't use B_UpdateStorageAndCapacity: UFO are not stored in storage */
								destination->storage.num[i]++;
								if (ufocraft->size == AIRCRAFT_LARGE)
									destination->capacities[CAP_UFOHANGARS_LARGE].cur++;
								else
									destination->capacities[CAP_UFOHANGARS_SMALL].cur++;
							}
						}
					} else
						B_UpdateStorageAndCapacity(destination, &csi.ods[i], transfer->itemAmount[i], qfalse, qtrue);
				}
			}
		}
	}

	if (transfer->hasEmployees && transfer->srcBase) {	/* Employees. (cannot come from a mission) */
		if (!success || !B_GetBuildingStatus(destination, B_QUARTERS)) {	/* Employees will be unhired. */
			if (success) {
				Com_sprintf(cp_messageBuffer, sizeof(cp_messageBuffer), _("Base %s does not have Living Quarters, employees got unhired!"), destination->name);
				MSO_CheckAddNewMessage(NT_TRANSFER_LOST,_("Transport mission"), cp_messageBuffer, qfalse, MSG_TRANSFERFINISHED, NULL);
			}
			for (i = 0; i < MAX_EMPL; i++) {
				for (j = 0; j < gd.numEmployees[i]; j++) {
					if (transfer->trEmployees[i][j]) {
						employee_t *employee = transfer->trEmployees[i][j];
						employee->baseHired = transfer->srcBase;	/* Restore back the original baseid. */
						/* every employee that we have transfered should have a
						 * base he is hire in - otherwise we couldn't have transfered him */
						assert(employee->baseHired);
						employee->transfer = qfalse;
						E_UnhireEmployee(employee);
					}
				}
			}
		} else {
			for (i = 0; i < MAX_EMPL; i++) {
				for (j = 0; j < gd.numEmployees[i]; j++) {
					if (transfer->trEmployees[i][j]) {
						employee_t *employee = transfer->trEmployees[i][j];
						employee->baseHired = transfer->srcBase;	/* Restore back the original baseid. */
						/* every employee that we have transfered should have a
						 * base he is hire in - otherwise we couldn't have transfered him */
						assert(employee->baseHired);
						employee->transfer = qfalse;
						E_UnhireEmployee(employee);
						E_HireEmployee(destination, employee);
					}
				}
			}
		}
	}

	if (transfer->hasAliens && success) {	/* Aliens. */
		if (!B_GetBuildingStatus(destination, B_ALIEN_CONTAINMENT)) {
			Com_sprintf(cp_messageBuffer, sizeof(cp_messageBuffer), _("Base %s does not have Alien Containment, Aliens are removed!"), destination->name);
			MSO_CheckAddNewMessage(NT_TRANSFER_LOST,_("Transport mission"), cp_messageBuffer, qfalse, MSG_TRANSFERFINISHED, NULL);
			/* Aliens cargo is not unloaded, will be destroyed in TR_TransferCheck(). */
		} else {
			for (i = 0; i < gd.numAliensTD; i++) {
				if (transfer->alienAmount[i][TRANS_ALIEN_ALIVE] > 0) {
					AL_ChangeAliveAlienNumber(destination, &(destination->alienscont[i]), transfer->alienAmount[i][TRANS_ALIEN_ALIVE]);
				}
				if (transfer->alienAmount[i][TRANS_ALIEN_DEAD] > 0)
					destination->alienscont[i].amount_dead += transfer->alienAmount[i][TRANS_ALIEN_DEAD];
			}
		}
	}

	/** @todo If source base is destroyed during transfer, aircraft doesn't exist anymore.
	 * aircraftArray should contain pointers to aircraftTemplates to avoid this problem, and be removed from
	 * source base as soon as transfer starts */
	if (transfer->hasAircraft && success && transfer->srcBase) {	/* Aircraft. Cannot come from mission */
		/* reverse loop: aircraft are deleted in the loop: idx change */
		for (i = MAX_AIRCRAFT - 1; i >= 0; i--) {
			if (transfer->aircraftArray[i] > TRANS_LIST_EMPTY_SLOT) {
				aircraft_t *aircraft = AIR_AircraftGetFromIDX(i);
				assert(aircraft);

				if (AIR_CalculateHangarStorage(aircraft->tpl, destination, 0) > 0) {
					/** @todo This doesn't work for assigned weapons and other
					 * assigned stuff like employees and so on */
					/* Aircraft relocated to new base, just add new one. */
					AIR_NewAircraft(destination, aircraft->id);
					/* Remove aircraft from old base. */
					AIR_DeleteAircraft(transfer->srcBase, aircraft);
				} else {
					/* No space, aircraft will be lost. */
					Com_sprintf(cp_messageBuffer, sizeof(cp_messageBuffer), _("Base %s does not have enough free space in hangars. Aircraft is lost!"), destination->name);
					MSO_CheckAddNewMessage(NT_TRANSFER_LOST,_("Transport mission"), cp_messageBuffer, qfalse, MSG_TRANSFERFINISHED, NULL);
					AIR_DeleteAircraft(transfer->srcBase, aircraft);
				}
			}
		}
	}
}

/**
 * @brief Starts alien bodies transfer between mission and base.
 * @param[in] base Pointer to the base to send the alien bodies.
 * @sa TR_TransferBaseListClick_f
 * @sa TR_TransferStart_f
 */
static void TR_TransferAlienAfterMissionStart (const base_t *base)
{
	int i, j;
	transfer_t *transfer;
	float time;
	char message[256];
	int alienCargoTypes;
	aliensTmp_t *cargo;

	if (!base) {
		Com_Printf("TR_TransferAlienAfterMissionStart_f: No base selected!\n");
		return;
	}

	transfer = NULL;
	/* Find free transfer slot. */
	for (i = 0; i < MAX_TRANSFERS; i++) {
		if (!gd.alltransfers[i].active) {
			transfer = &gd.alltransfers[i];
			break;
		}
	}

	if (!transfer)
		return;

	/* Initialize transfer.
	 * calculate time to go from 1 base to another : 1 day for one quarter of the globe*/
	time = MAP_GetDistance(base->pos, transferStartAircraft->pos) / 90.0f;
	transfer->event.day = ccs.date.day + floor(time);	/* add day */
	time = (time - floor(time)) * SECONDS_PER_DAY;	/* convert remaining time in second */
	transfer->event.sec = ccs.date.sec + round(time);
	/* check if event is not the followinf day */
	if (transfer->event.sec > SECONDS_PER_DAY) {
		transfer->event.sec -= SECONDS_PER_DAY;
		transfer->event.day++;
	}
	transfer->destBase = B_GetFoundedBaseByIDX(base->idx);	/* Destination base. */
	transfer->srcBase = NULL;	/* Source base. */
	transfer->active = qtrue;

	alienCargoTypes = AL_GetAircraftAlienCargoTypes(transferStartAircraft);
	cargo = AL_GetAircraftAlienCargo(transferStartAircraft);
	for (i = 0; i < alienCargoTypes; i++, cargo++) {		/* Aliens. */
		if (cargo->amount_alive > 0) {
			for (j = 0; j < gd.numAliensTD; j++) {
				if (!AL_IsTeamDefAlien(&csi.teamDef[j]))
					continue;
				if (base->alienscont[j].teamDef == cargo->teamDef) {
					transfer->hasAliens = qtrue;
					transfer->alienAmount[j][TRANS_ALIEN_ALIVE] = cargo->amount_alive;
					cargo->amount_alive = 0;
					break;
				}
			}
		}
		if (cargo->amount_dead > 0) {
			for (j = 0; j < gd.numAliensTD; j++) {
				if (!AL_IsTeamDefAlien(&csi.teamDef[j]))
					continue;
				if (base->alienscont[j].teamDef == cargo->teamDef) {
					transfer->hasAliens = qtrue;
					transfer->alienAmount[j][TRANS_ALIEN_DEAD] = cargo->amount_dead;
					cargo->amount_dead = 0;
					break;
				}
			}
		}
	}

	/* Make sure that we don't use transferStartAircraft after this point
	 * (Could be a fake aircraft in case of base attack) */
	transferStartAircraft = NULL;

	Com_sprintf(message, sizeof(message), _("Transport mission started, cargo is being transported to base %s"), transfer->destBase->name);
	MSO_CheckAddNewMessage(NT_TRANSFER_ALIENBODIES_DEFERED,_("Transport mission"), message, qfalse, MSG_TRANSFERFINISHED, NULL);
#if 0
	MN_PopMenu(qfalse);
#else
	/** @todo Why don't we use MN_PopMenu directly here? */
	Cbuf_AddText("mn_pop\n");
#endif
}

/**
 * @brief Action to realize when clicking on Transfer Menu.
 * @note This menu is used when a dropship ending a mission collected alien bodies, but there's no alien cont. in home base
 * @sa TR_TransferAircraftMenu
 */
static void TR_TransferBaseListClick_f (void)
{
	int num, baseIdx;

	if (Cmd_Argc() < 2) {
		Com_Printf("Usage: %s <arg>\n", Cmd_Argv(0));
		return;
	}

	num = atoi(Cmd_Argv(1));

	/* skip base not displayed (see TR_TransferAircraftMenu) */
	for (baseIdx = 0; baseIdx < MAX_BASES; baseIdx++) {
		const base_t *base = B_GetFoundedBaseByIDX(baseIdx);
		if (!base)
			continue;

		if (!B_GetBuildingStatus(base, B_ALIEN_CONTAINMENT))
			continue;

		num--;
		if (num <= 0) {
			TR_TransferAlienAfterMissionStart(base);
			break;
		}
	}

	if (baseIdx == MAX_BASES) {
		Com_Printf("TR_TransferBaseListClick_f: baseIdx %i doesn't exist.\n", num);
		return;
	}
}

/**
 * @brief Shows available bases to collect dead aliens after a mission is finished.
 * @param[in] aircraft Pointer to aircraft used in transfer.
 * @sa TR_TransferBaseListClick_f
 */
void TR_TransferAircraftMenu (aircraft_t* aircraft)
{
	int i, num;
	static char transferBaseSelectPopup[512];

	transferBaseSelectPopup[0] = '\0';

	/* store the aircraft to be able to remove alien bodies */
	transferStartAircraft = aircraft;

	/* make sure that all tests here are the same than in TR_TransferBaseListClick_f */
	for (i = 0; i < MAX_BASES; i++) {
		const base_t *base = B_GetFoundedBaseByIDX(i);
		if (!base)
			continue;
		/* don't display bases without Alien Containment */
		if (!base->hasBuilding[B_ALIEN_CONTAINMENT])
			continue;

		num = (base->capacities[CAP_ALIENS].max - base->capacities[CAP_ALIENS].cur);
		Q_strcat(transferBaseSelectPopup, va("  %s ", base->name), sizeof(transferBaseSelectPopup));
		Q_strcat(transferBaseSelectPopup, va(ngettext("(can host %i live alien)\n", "(can host %i live aliens)\n", num), num), sizeof(transferBaseSelectPopup));
	}
	MN_RegisterText(TEXT_LIST, transferBaseSelectPopup);
	MN_PushMenu("popup_transferbaselist", NULL);
}

/**
 * @brief Ends the transfer.
 * @param[in] transfer Pointer to transfer in gd.alltransfers
 */
static void TR_TransferEnd (transfer_t *transfer)
{
	base_t* destination = transfer->destBase;
	assert(destination);

	if (!destination->founded) {
		TR_EmptyTransferCargo(NULL, transfer, qfalse);
		MSO_CheckAddNewMessage(NT_TRANSFER_LOST,_("Transport mission"), _("The destination base no longer exists! Transfer cargo are lost, personel got unhired."), qfalse, MSG_TRANSFERFINISHED, NULL);
		/** @todo what if source base is lost? we won't be able to unhire transfered personel. */
	} else {
		char message[256];
		TR_EmptyTransferCargo(destination, transfer, qtrue);
		Com_sprintf(message, sizeof(message), _("Transport mission ended, unloading cargo in base %s"), destination->name);
		MSO_CheckAddNewMessage(NT_TRANSFER_COMPLETED_SUCCESS,_("Transport mission"), message, qfalse, MSG_TRANSFERFINISHED, NULL);
	}
	transfer->active = qfalse;
}

/**
 * @brief Starts the transfer.
 * @sa TR_TransferSelect_f
 * @sa TR_TransferInit_f
 */
static void TR_TransferStart_f (void)
{
	int i, j;
	transfer_t *transfer;
	float time;
	char message[256];

	if (currentTransferType == TRANS_TYPE_INVALID) {
		Com_Printf("TR_TransferStart_f: currentTransferType is wrong!\n");
		return;
	}

	if (!transferBase || !baseCurrent) {
		Com_Printf("TR_TransferStart_f: No base selected!\n");
		return;
	}

	/* don't start any empty transport */
	if (!trCargoCountTmp) {
		return;
	}

	transfer = NULL;
	/* Find free transfer slot. */
	for (i = 0; i < MAX_TRANSFERS; i++) {
		if (!gd.alltransfers[i].active) {
			transfer = &gd.alltransfers[i];
			break;
		}
	}

	if (!transfer)
		return;

	/* Initialize transfer. */
	/* calculate time to go from 1 base to another : 1 day for one quarter of the globe*/
	time = MAP_GetDistance(transferBase->pos, baseCurrent->pos) / 90.0f;
	transfer->event.day = ccs.date.day + floor(time);	/* add day */
	time = (time - floor(time)) * SECONDS_PER_DAY;	/* convert remaining time in second */
	transfer->event.sec = ccs.date.sec + round(time);
	/* check if event is not the followinf day */
	if (transfer->event.sec > SECONDS_PER_DAY) {
		transfer->event.sec -= SECONDS_PER_DAY;
		transfer->event.day++;
	}
	transfer->destBase = transferBase;	/* Destination base. */
	assert(transfer->destBase);
	transfer->srcBase = baseCurrent;	/* Source base. */
	transfer->active = qtrue;
	for (i = 0; i < csi.numODs; i++) {	/* Items. */
		if (trItemsTmp[i] > 0) {
			transfer->hasItems = qtrue;
			transfer->itemAmount[i] = trItemsTmp[i];
		}
	}
	/* Note that personel remains hired in source base during the transfer, that is
	 * it takes Living Quarters capacity, etc, but it cannot be used anywhere. */
	for (i = 0; i < MAX_EMPL; i++) {		/* Employees. */
		for (j = 0; j < gd.numEmployees[i]; j++) {
			if (trEmployeesTmp[i][j]) {
				employee_t *employee = trEmployeesTmp[i][j];
				transfer->hasEmployees = qtrue;

				assert(employee->baseHired == baseCurrent);

				E_ResetEmployee(employee);
				transfer->trEmployees[i][j] = employee;
				employee->baseHired = NULL;
				employee->transfer = qtrue;
			}
		}
	}
	for (i = 0; i < gd.numAliensTD; i++) {		/* Aliens. */
		if (!AL_IsTeamDefAlien(&csi.teamDef[i]))
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
			aircraft_t *aircraft = AIR_AircraftGetFromIDX(i);
			aircraft->status = AIR_TRANSFER;
			AIR_RemoveEmployees(aircraft);
			transfer->hasAircraft = qtrue;
			transfer->aircraftArray[i] = i;
		}
	}

	/* Clear temporary cargo arrays. */
	memset(trItemsTmp, 0, sizeof(trItemsTmp));
	memset(trAliensTmp, 0, sizeof(trAliensTmp));
	memset(trEmployeesTmp, 0, sizeof(trEmployeesTmp));
	memset(trAircraftsTmp, TRANS_LIST_EMPTY_SLOT, sizeof(trAircraftsTmp));

	/* Recheck if production/research can be done on srcbase (if there are workers/scientists) */
	PR_ProductionAllowed(baseCurrent);
	RS_ResearchAllowed(baseCurrent);

	Com_sprintf(message, sizeof(message), _("Transport mission started, cargo is being transported to base %s"), transferBase->name);
	MSO_CheckAddNewMessage(NT_TRANSFER_STARTED,_("Transport mission"), message, qfalse, MSG_TRANSFERFINISHED, NULL);
	MN_PopMenu(qfalse);
}

/**
 * @brief Set the number of item to transfer.
 */
static int TR_GetTransferFactor (void)
{
	return 1;
}

/**
 * @brief Adds a thing to transfercargo by left mouseclick.
 * @sa TR_TransferSelect_f
 * @sa TR_TransferInit_f
 */
static void TR_TransferListSelect_f (void)
{
	int num, cnt = 0, i;
	employeeType_t emplType;
	qboolean added = qfalse;
	int numempl[MAX_EMPL];

	if (Cmd_Argc() < 2)
		return;

	if (!baseCurrent)
		return;

	if (!transferBase) {
		MN_Popup(_("No target base selected"), _("Please select the target base from the list"));
		return;
	}

	num = atoi(Cmd_Argv(1));
	if (num < 0 || num >= csi.numODs)
		return;

	switch (currentTransferType) {
	case TRANS_TYPE_INVALID:	/**< No list was inited before you call this. */
		return;
	case TRANS_TYPE_ITEM:
		for (i = 0; i < csi.numODs; i++) {
			if (baseCurrent->storage.num[i]) {
				if (cnt == num) {
					int amount = TR_GetTransferFactor();
					const objDef_t *od = &csi.ods[i];
					/* you can't transfer more item than you have */
					amount = min(amount, baseCurrent->storage.num[i]);
					/* you can only transfer items that destination base can accept */
					amount = TR_CheckItem(od, transferBase, amount);
					if (amount) {
						trItemsTmp[i] += amount;
						if (!Q_strncmp(od->id, "antimatter", 10))
							B_ManageAntimatter(baseCurrent, amount, qfalse);
						else if (od->tech->type == RS_CRAFT) { /* This is UFO craft */
							const aircraft_t *ufocraft = AIR_GetAircraft(csi.ods[i].tech->provides);
							assert(ufocraft);
							/* don't use B_UpdateStorageAndCapacity: UFO are not stored in storage */
							baseCurrent->storage.num[i] -= amount;
							if (ufocraft->size == AIRCRAFT_LARGE)
								baseCurrent->capacities[CAP_UFOHANGARS_LARGE].cur -= amount;
							else
								baseCurrent->capacities[CAP_UFOHANGARS_SMALL].cur -= amount;
						} else
							B_UpdateStorageAndCapacity(baseCurrent, od, -amount, qfalse, qfalse);
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
			employee_t *employee = &gd.employees[EMPL_SOLDIER][i];
			if (!E_IsInBase(employee, baseCurrent))
				continue;
			if (trEmployeesTmp[EMPL_SOLDIER][i])
				continue;
			if (cnt == num) {
				if (TR_CheckEmployee(employee, transferBase)) {
					trEmployeesTmp[EMPL_SOLDIER][employee->idx] = employee;
					added = qtrue;
					break;
				} else
					return;
			}
			cnt++;
		}
		for (i = 0; i < gd.numEmployees[EMPL_PILOT]; i++) {
			employee_t *employee = &gd.employees[EMPL_PILOT][i];
			if (!E_IsInBase(employee, baseCurrent))
				continue;
			if (trEmployeesTmp[EMPL_PILOT][i])
				continue;
			if (cnt == num) {
				if (TR_CheckEmployee(employee, transferBase)) {
					trEmployeesTmp[EMPL_PILOT][employee->idx] = employee;
					added = qtrue;
					break;
				} else
					return;
			}
			cnt++;
		}

		if (added) /* We already added a soldier, so break. */
			break;

		/* Reset and fill temp employees arrays. */
		for (emplType = 0; emplType < MAX_EMPL; emplType++) {
			numempl[emplType] = E_CountHired(baseCurrent, emplType);
			for (i = 0; i < MAX_EMPLOYEES; i++) {
				if (trEmployeesTmp[emplType][i])
					numempl[emplType]--;
			}
		}

		for (emplType = 0; emplType < MAX_EMPL; emplType++) {
			if (emplType == EMPL_SOLDIER || emplType == EMPL_PILOT)
				continue;
			/* no employee in base or all employees already in the transfer list */
			if (numempl[emplType] < 1)
				continue;
			if (cnt == num) {
				int amount = min(E_CountHired(baseCurrent, emplType), TR_GetTransferFactor());
				for (i = 0; i < gd.numEmployees[emplType]; i++) {
					employee_t *employee = &gd.employees[emplType][i];
					if (!E_IsInBase(employee, baseCurrent))
						continue;
					if (trEmployeesTmp[emplType][employee->idx])	/* Already on transfer list. */
						continue;
					if (TR_CheckEmployee(employee, transferBase)) {
						trEmployeesTmp[emplType][employee->idx] = employee;
						amount--;
						if (amount == 0)
							break;
					} else
						return;
				}
			}
			cnt++;
		}
		break;
	case TRANS_TYPE_ALIEN:
		if (!B_GetBuildingStatus(transferBase, B_ALIEN_CONTAINMENT))
			return;
		for (i = 0; i < gd.numAliensTD; i++) {
			if (baseCurrent->alienscont[i].teamDef && baseCurrent->alienscont[i].amount_dead > 0) {
				if (cnt == num) {
					trAliensTmp[i][TRANS_ALIEN_DEAD]++;
					/* Remove the corpse from Alien Containment. */
					baseCurrent->alienscont[i].amount_dead--;
					break;
				}
				cnt++;
			}
			if (baseCurrent->alienscont[i].teamDef && baseCurrent->alienscont[i].amount_alive > 0) {
				if (cnt == num) {
					if (TR_CheckAlien(i, transferBase)) {
						trAliensTmp[i][TRANS_ALIEN_ALIVE]++;
						/* Remove an alien from Alien Containment. */
						AL_ChangeAliveAlienNumber(baseCurrent, &(baseCurrent->alienscont[i]), -1);
						break;
					} else
						return;
				}
				cnt++;
			}
		}
		break;
	case TRANS_TYPE_AIRCRAFT:
		if (!B_GetBuildingStatus(transferBase, B_HANGAR) && !B_GetBuildingStatus(transferBase, B_SMALL_HANGAR))
			return;
		for (i = 0; i < MAX_AIRCRAFT; i++) {
			const aircraft_t *aircraft = AIR_AircraftGetFromIDX(i);
			if (!aircraft)
				return;
			if (aircraft->homebase == baseCurrent && TR_AircraftListSelect(i)) {
				if (cnt == num) {
					if (TR_CheckAircraft(aircraft, transferBase)) {
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

	TR_TransferSelect(baseCurrent, transferBase, currentTransferType);
}

/**
 * @brief Callback for base list click.
 * @note transferBase is being set here.
 * @param[in] srcbase
 * @param[in] destbase Pointer to base which will be transferBase.
 */
static void TR_TransferBaseSelect (base_t *srcbase, base_t *destbase)
{
	static char baseInfo[1024];
	qboolean powercomm = qfalse;

	if (!destbase || !srcbase)
		return;

	Com_sprintf(baseInfo, sizeof(baseInfo), "%s\n\n", destbase->name);

	powercomm = B_GetBuildingStatus(destbase, B_POWER);

	/* if there is no power supply facility this check will fail, too */
	if (B_GetBuildingStatus(destbase, B_STORAGE)) {
		Q_strcat(baseInfo, _("You can transfer equipment - this base has a Storage.\n"), sizeof(baseInfo));
	} else if (powercomm) {
		/* if there is a power supply facility we really don't have a storage */
		Q_strcat(baseInfo, _("No Storage in this base.\n"), sizeof(baseInfo));
	}

	if (B_GetBuildingStatus(destbase, B_QUARTERS)) {
		Q_strcat(baseInfo, _("You can transfer employees - this base has Living Quarters.\n"), sizeof(baseInfo));
	} else {
		Q_strcat(baseInfo, _("No Living Quarters in this base.\n"), sizeof(baseInfo));
	}

	if (B_GetBuildingStatus(destbase, B_ALIEN_CONTAINMENT)) {
		Q_strcat(baseInfo, _("You can transfer Aliens - this base has an Alien Containment.\n"), sizeof(baseInfo));
	} else if (powercomm) {
		Q_strcat(baseInfo, _("No Alien Containment in this base.\n"), sizeof(baseInfo));
	}

	if (B_GetBuildingStatus(destbase, B_ANTIMATTER)) {
		Q_strcat(baseInfo, _("You can transfer antimatter - this base has an Antimatter Storage.\n"), sizeof(baseInfo));
	} else if (powercomm) {
		Q_strcat(baseInfo, _("No Antimatter Storage in this base.\n"), sizeof(baseInfo));
	}

	if (B_GetBuildingStatus(destbase, B_HANGAR) || B_GetBuildingStatus(destbase, B_SMALL_HANGAR)) {
		Q_strcat(baseInfo, _("You can transfer aircraft - this base has a Hangar.\n"), sizeof(baseInfo));
	} else if (!B_GetBuildingStatus(destbase, B_COMMAND)) {
		Q_strcat(baseInfo, _("Aircraft transfer not possible - this base does not have a Command Centre.\n"), sizeof(baseInfo));
	} else if (powercomm) {
		Q_strcat(baseInfo, _("No Hangar in this base.\n"), sizeof(baseInfo));
	}

	if (!powercomm)
		Q_strcat(baseInfo, _("No power supplies in this base.\n"), sizeof(baseInfo));

	MN_RegisterText(TEXT_BASE_INFO, baseInfo);

	/* Set global pointer to current selected base. */
	transferBase = destbase;
	Cvar_Set("mn_trans_base_name", destbase->name);

	/* Update stuff-in-base list. */
	TR_TransferSelect(srcbase, destbase, currentTransferType);
}

/**
 * @brief Selects next base.
 * @sa TR_PrevBase_f
 * @note Command to call this: trans_nextbase
 */
static void TR_NextBase_f (void)
{
	int baseIdx, counter;

	if (!baseCurrent)
		return;

	if (!transferBase)
		counter = baseCurrent->idx;
	else
		counter = transferBase->idx;

	for (baseIdx = counter + 1; baseIdx < MAX_BASES; baseIdx++) {
		base_t *base = B_GetFoundedBaseByIDX(baseIdx);
		if (!base)
			continue;
		if (base == baseCurrent)
			continue;
		TR_TransferBaseSelect(baseCurrent, base);
		return;
	}
	/* At this point we are at "last" base, so we will select first. */
	for (baseIdx = 0; baseIdx < MAX_BASES; baseIdx++) {
		base_t *base = B_GetFoundedBaseByIDX(baseIdx);
		if (!base)
			continue;
		if (base == baseCurrent)
			continue;
		TR_TransferBaseSelect(baseCurrent, base);
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
	int baseIdx, counter;

	if (!baseCurrent)
		return;

	if (!transferBase)
		counter = baseCurrent->idx;
	else
		counter = transferBase->idx;

	for (baseIdx = counter - 1; baseIdx >= 0; baseIdx--) {
		base_t *base = B_GetFoundedBaseByIDX(baseIdx);
		if (!base)
			continue;
		if (base == baseCurrent)
			continue;
		TR_TransferBaseSelect(baseCurrent, base);
		return;
	}
	/* At this point we are at "first" base, so we will select last. */
	for (baseIdx = MAX_BASES - 1; baseIdx >= 0; baseIdx--) {
		base_t *base = B_GetFoundedBaseByIDX(baseIdx);
		if (!base)
			continue;
		if (base == baseCurrent)
			continue;
		TR_TransferBaseSelect(baseCurrent, base);
		return;
	}
}

/**
 * @brief Removes items from cargolist by click.
 */
static void TR_CargoListSelect_f (void)
{
	int num, cnt = 0, entries = 0, i, j;
	qboolean removed = qfalse;
	int numempl[MAX_EMPL];
	employeeType_t emplType;

	if (Cmd_Argc() < 2)
		return;

	if (!baseCurrent)
		return;

	num = atoi(Cmd_Argv(1));
	if (num < 0 || num >= MAX_CARGO)
		return;

	switch (cargo[num].type) {
	case CARGO_TYPE_ITEM:
		for (i = 0; i < csi.numODs; i++) {
			if (trItemsTmp[i] > 0) {
				if (cnt == num) {
					int amount;
					amount = TR_GetTransferFactor();
					/* you can't transfer more item than there are in current tranfer */
					amount = min(amount, trItemsTmp[i]);
					trItemsTmp[i] -= amount;
					if (!Q_strncmp(csi.ods[i].id, "antimatter", 10))
						B_ManageAntimatter(baseCurrent, amount, qfalse);
					else if (csi.ods[i].tech->type == RS_CRAFT) { /* This is UFO craft */
						const aircraft_t *ufocraft = AIR_GetAircraft(csi.ods[i].tech->provides);
						assert(ufocraft);
						/* don't use B_UpdateStorageAndCapacity: UFO are not stored in storage */
						baseCurrent->storage.num[i] += amount;
						if (ufocraft->size == AIRCRAFT_LARGE)
							baseCurrent->capacities[CAP_UFOHANGARS_LARGE].cur += amount;
						else
							baseCurrent->capacities[CAP_UFOHANGARS_SMALL].cur += amount;
					} else
						B_UpdateStorageAndCapacity(baseCurrent, &csi.ods[i], amount, qfalse, qfalse);
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
			if (trEmployeesTmp[EMPL_SOLDIER][i]) {
				if (cnt == num) {
					trEmployeesTmp[EMPL_SOLDIER][i] = NULL;
					removed = qtrue;
					break;
				}
				cnt++;
			}
		}
		for (i = 0; i < gd.numEmployees[EMPL_PILOT]; i++) {
			if (trEmployeesTmp[EMPL_PILOT][i]) {
				if (cnt == num) {
					trEmployeesTmp[EMPL_PILOT][i] = NULL;
					removed = qtrue;
					break;
				}
				cnt++;
			}
		}
		if (removed)	/* We already removed soldier, break here. */
			break;

		Com_DPrintf(DEBUG_CLIENT, "TR_CargoListSelect_f: cnt: %i, num: %i\n", cnt, num);

		/* Reset and fill temp employees arrays. */
		for (emplType = 0; emplType < MAX_EMPL; emplType++) {
			numempl[emplType] = 0;
			for (i = 0; i < MAX_EMPLOYEES; i++) {
				if (trEmployeesTmp[emplType][i])
					numempl[emplType]++;
			}
		}

		for (emplType = 0; emplType < MAX_EMPL; emplType++) {
			if ((numempl[emplType] < 1) || (emplType == EMPL_SOLDIER) || (emplType == EMPL_PILOT))
				continue;
			if (cnt == num) {
				int amount;
				amount = TR_GetTransferFactor();
				amount = min(amount, E_CountHired(baseCurrent, emplType));
				for (j = 0; j < gd.numEmployees[emplType]; j++) {
					if (trEmployeesTmp[emplType][j]) {
						trEmployeesTmp[emplType][j] = NULL;
						amount--;
						removed = qtrue;
						if (amount == 0)
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
					AL_ChangeAliveAlienNumber(baseCurrent, &(baseCurrent->alienscont[i]), 1);
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

	Cbuf_AddText(va("trans_select %i\n", currentTransferType));
}

/**
 * @brief Notify that an aircraft has been removed.
 * @sa AIR_DeleteAircraft
 */
void TR_NotifyAircraftRemoved (const aircraft_t *aircraft)
{
	int i;

	assert(aircraft->idx >= 0 && aircraft->idx < MAX_AIRCRAFT);

	for (i = 0; i < MAX_TRANSFERS; i++) {
		transfer_t *transfer = &gd.alltransfers[i];
		size_t n = MAX_AIRCRAFT;

		/* skip non active transfer */
		if (!transfer->active)
			continue;
		if (!transfer->hasAircraft)
			continue;
		REMOVE_ELEM(transfer->aircraftArray, aircraft->idx, n);
	}
}

/**
 * @brief Checks whether given transfer should be processed.
 * @sa CL_CampaignRun
 */
void TR_TransferCheck (void)
{
	int i;

	for (i = 0; i < MAX_TRANSFERS; i++) {
		transfer_t *transfer = &gd.alltransfers[i];
		if (!transfer->active)
			continue;
		if (transfer->event.day == ccs.date.day && ccs.date.sec >= transfer->event.sec) {
			assert(transfer->destBase);
			TR_TransferEnd(transfer);
			/* Reset this transfer. */
			memset(transfer, 0, sizeof(*transfer));
			memset(transfer->aircraftArray, TRANS_LIST_EMPTY_SLOT, sizeof(transfer->aircraftArray));
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

	/* Clear employees temp array. */
	memset(trEmployeesTmp, 0, sizeof(trEmployeesTmp));

	/* Clear aircraft temp array. */
	memset(trAircraftsTmp, TRANS_LIST_EMPTY_SLOT, sizeof(trAircraftsTmp));

	/* Select first available item */
	TR_TransferSelect_f();

	/* Select first available base. */
	TR_NextBase_f();

	/* Set up cvar used to display transferBase. */
	if (transferBase)
		Cvar_Set("mn_trans_base_name", transferBase->name);
	/* Reset scrolling for item-in-base list */
	TR_ResetScrolling_f();
}

/**
 * @brief Closes Transfer Menu and resets temp arrays.
 */
static void TR_TransferClose_f (void)
{
	if (!baseCurrent)
		return;

	/* Unload what was loaded. */
	TR_TransferListClear_f();

	/* Clear temporary cargo arrays. */
	memset(trItemsTmp, 0, sizeof(trItemsTmp));
	memset(trAliensTmp, 0, sizeof(trAliensTmp));
	memset(trEmployeesTmp, 0, sizeof(trEmployeesTmp));
	memset(trAircraftsTmp, TRANS_LIST_EMPTY_SLOT, sizeof(trAircraftsTmp));
}

/**
 * @brief Save callback for savegames
 * @sa TR_Load
 * @sa SAV_GameSave
 */
qboolean TR_Save (sizebuf_t* sb, void* data)
{
	int i, j, k;

	for (i = 0; i < presaveArray[PRE_MAXTRA]; i++) {
		const transfer_t *transfer = &gd.alltransfers[i];
		for (j = 0; j < presaveArray[PRE_MAXOBJ]; j++)
			MSG_WriteByte(sb, transfer->itemAmount[j]);
		for (j = 0; j < presaveArray[PRE_NUMALI]; j++) {
			MSG_WriteByte(sb, transfer->alienAmount[j][TRANS_ALIEN_ALIVE]);
			MSG_WriteByte(sb, transfer->alienAmount[j][TRANS_ALIEN_DEAD]);
		}
		for (j = 0; j < presaveArray[PRE_EMPTYP]; j++) {
			for (k = 0; k < presaveArray[PRE_MAXEMP]; k++)
				MSG_WriteShort(sb, (transfer->trEmployees[j][k] ? transfer->trEmployees[j][k]->idx : -1));
		}
		for (j = 0; j < presaveArray[PRE_MAXAIR]; j++)
			MSG_WriteShort(sb, transfer->aircraftArray[j]);

		MSG_WriteByte(sb, (transfer->destBase ? transfer->destBase->idx : BYTES_NONE));
		MSG_WriteByte(sb, (transfer->srcBase ? transfer->srcBase->idx : BYTES_NONE));
		if (transfer->active && !transfer->destBase) {
			Com_Printf("Could not save transfer, active is true, but no destBase is set\n");
			return qfalse;
		}
		MSG_WriteByte(sb, transfer->active);
		MSG_WriteByte(sb, transfer->hasItems);
		MSG_WriteByte(sb, transfer->hasEmployees);
		MSG_WriteByte(sb, transfer->hasAliens);
		MSG_WriteByte(sb, transfer->hasAircraft);
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
	byte destBase, srcBase;

	for (i = 0; i < presaveArray[PRE_MAXTRA]; i++) {
		transfer_t *transfer = &gd.alltransfers[i];
		for (j = 0; j < presaveArray[PRE_MAXOBJ]; j++)
			transfer->itemAmount[j] = MSG_ReadByte(sb);
		for (j = 0; j < presaveArray[PRE_NUMALI]; j++) {
			transfer->alienAmount[j][TRANS_ALIEN_ALIVE] = MSG_ReadByte(sb);
			transfer->alienAmount[j][TRANS_ALIEN_DEAD] = MSG_ReadByte(sb);
		}
		for (j = 0; j < presaveArray[PRE_EMPTYP]; j++) {
			for (k = 0; k < presaveArray[PRE_MAXEMP]; k++) {
				const int emplIdx = MSG_ReadShort(sb);
				transfer->trEmployees[j][k] = ((emplIdx >= 0) ? &gd.employees[j][emplIdx] : NULL);
			}
		}
		for (j = 0; j < presaveArray[PRE_MAXAIR]; j++)
			transfer->aircraftArray[j] = MSG_ReadShort(sb);
		assert(gd.numBases);
		destBase = MSG_ReadByte(sb);
		transfer->destBase = ((destBase != BYTES_NONE) ? B_GetBaseByIDX(destBase) : NULL);
		/** @todo Can (or should) destBase be NULL? If not, check against a null pointer
		 * for transfer->destbase and return qfalse here */
		srcBase = MSG_ReadByte(sb);
		transfer->srcBase = ((srcBase != BYTES_NONE) ? B_GetBaseByIDX(srcBase) : NULL);
		transfer->active = MSG_ReadByte(sb);
		transfer->hasItems = MSG_ReadByte(sb);
		transfer->hasEmployees = MSG_ReadByte(sb);
		transfer->hasAliens = MSG_ReadByte(sb);
		transfer->hasAircraft = MSG_ReadByte(sb);
		transfer->event.day = MSG_ReadLong(sb);
		transfer->event.sec = MSG_ReadLong(sb);
	}
	/* Restore transfer flag if an employee is currently in progress. */
	for (i = 0; i < presaveArray[PRE_MAXTRA]; i++) {
		transfer_t *transfer = &gd.alltransfers[i];

		if (!transfer->active)
			continue;

		for (j = 0; j < presaveArray[PRE_EMPTYP]; j++) {
			for (k = 0; k < presaveArray[PRE_MAXEMP]; k++) {
				gd.employees[j][k].transfer = (transfer->trEmployees[j][k]>=0) ? qtrue : qfalse;
			}
		}
	}
	return qtrue;
}

/**
 * @brief Defines commands and cvars for the Transfer menu(s).
 * @sa MN_InitStartup
 */
void TR_InitStartup (void)
{
	/* add commands */
	Cmd_AddCommand("trans_init", TR_Init_f, "Init function for Transfer menu");
	Cmd_AddCommand("trans_start", TR_TransferStart_f, "Starts the tranfer");
	Cmd_AddCommand("trans_select", TR_TransferSelect_f, "Switch between transfer types (employees, techs, items)");
	Cmd_AddCommand("trans_resetscroll", TR_ResetScrolling_f, "Reset scrolling items-in-base list");
	Cmd_AddCommand("trans_emptyairstorage", TR_TransferListClear_f, "Unload everything from transfer cargo back to base");
	Cmd_AddCommand("trans_list_click", TR_TransferListSelect_f, "Callback for transfer list node click");
	Cmd_AddCommand("trans_cargolist_click", TR_CargoListSelect_f, "Callback for cargo list node click");
	Cmd_AddCommand("trans_close", TR_TransferClose_f, "Callback for closing Transfer Menu");
	Cmd_AddCommand("trans_nextbase", TR_NextBase_f, "Callback for selecting next base");
	Cmd_AddCommand("trans_prevbase", TR_PrevBase_f, "Callback for selecting previous base");
	Cmd_AddCommand("trans_baselist_click", TR_TransferBaseListClick_f, "Callback for choosing base while recovering alien after mission");
	Cmd_AddCommand("trans_prevcat", TR_Prev_Category_f, "Callback for selecting previous transfer category");
	Cmd_AddCommand("trans_nextcat", TR_Next_Category_f, "Callback for selecting next transfer category");
}
