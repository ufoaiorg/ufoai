/**
 * @file cp_transfer.c
 * @brief Deals with the Transfer stuff.
 * @note Transfer menu functions prefix: TR_
 * @todo Remove direct access to nodes
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

#include "../client.h"
#include "../cl_team.h"
#include "../menu/m_main.h"
#include "../menu/m_popup.h"
#include "../menu/m_data.h"
#include "../mxml/mxml_ufoai.h"
#include "cp_campaign.h"
#include "cp_uforecovery.h"
#include "cp_map.h"
#include "cp_aliencont.h"
#include "save/save_transfer.h"

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

/**
 * @brief transfer typeID strings
 */
static const char* transferTypeIDs[] = {
	"item",
	"employee",
	"alien",
	"aircraft"
};
CASSERT(lengthof(transferTypeIDs) == TRANS_TYPE_MAX);

typedef struct transferData_s {
	/** @brief Current selected aircraft for transfer (if transfer started from mission). */
	aircraft_t *transferStartAircraft;

	/** @brief Current selected base for transfer. */
	base_t *transferBase;

	/** @brief Current transfer type (item, employee, alien, aircraft). */
	transferType_t currentTransferType;

	/** @brief Current cargo onboard. */
	transferCargo_t cargo[MAX_CARGO];

	/** @brief Current item cargo. */
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

static transferData_t td;

/** @brief Max values for transfer factors. */
static const int MAX_TR_FACTORS = 500;
/** @brief number of entries on menu */
static const int MAX_TRANSLIST_MENU_ENTRIES = 21;

/**
 * @brief Returns the transfer type
 * @param[in] id Transfer type Id
 * @sa transferType_t
 */
static transferType_t TR_GetTransferType (const char *id)
{
	int i;
	for (i = 0; i < TRANS_TYPE_MAX; i++) {
		if (!strcmp(transferTypeIDs[i], id))
			return i;
	}
	return TRANS_TYPE_INVALID;
}

/**
 * @brief Checks condition for item transfer.
 * @param[in] od Pointer to object definition.
 * @param[in] destbase Pointer to destination base.
 * @param[in] amount Number of items to transfer.
 * @return Number of items that can be transfered.
 */
static int TR_CheckItem (const objDef_t *od, const base_t *destbase, int amount)
{
	int i;
	int intransfer = 0;
	int amtransfer = 0;

	assert(od);
	assert(destbase);

	/* Count size of all items already on the transfer list. */
	for (i = 0; i < csi.numODs; i++) {
		if (td.trItemsTmp[i] > 0) {
			if (!strcmp(csi.ods[i].id, ANTIMATTER_TECH_ID))
				amtransfer = td.trItemsTmp[i];
			else
				intransfer += csi.ods[i].size * td.trItemsTmp[i];
		}
	}

	/* Is this antimatter and destination base has enough space in Antimatter Storage? */
	if (!strcmp(od->id, ANTIMATTER_TECH_ID)) {
		/* Give some meaningful feedback to the player if the player clicks on an a.m. item but base doesn't have am storage. */
		if (!B_GetBuildingStatus(destbase, B_ANTIMATTER) && B_GetBuildingStatus(destbase, B_STORAGE)) {
			MN_Popup(_("Missing storage"), _("Destination base does not have an Antimatter Storage.\n"));
			return qfalse;
		} else if (!B_GetBuildingStatus(destbase, B_ANTIMATTER)) {	/* Return if the target base doesn't have antimatter storage or power. */
			return 0;
		}
		amount = min(amount, destbase->capacities[CAP_ANTIMATTER].max - destbase->capacities[CAP_ANTIMATTER].cur - amtransfer);
		if (amount <= 0) {
			MN_Popup(_("Not enough space"), _("Destination base does not have enough\nAntimatter Storage space to store more antimatter.\n"));
			return 0;
		} else {
			/* amount to transfer can't be bigger than what we have */
			amount = min(amount, destbase->capacities[CAP_ANTIMATTER].max - destbase->capacities[CAP_ANTIMATTER].cur - amtransfer);
		}
	} else {	/*This is not antimatter */
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
		for (i = 0; i < ccs.numEmployees[emplType]; i++) {
			if (td.trEmployeesTmp[emplType][i])
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
			const rank_t *rank = CL_GetRankByIdx(employee->chr.score.rank);
			Com_sprintf(popupText, sizeof(popupText), _("%s %s is assigned to aircraft and cannot be\ntransfered to another base.\n"),
				_(rank->shortname), employee->chr.name);
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
	for (i = 0; i < ccs.numAliensTD; i++) {
		if (td.trAliensTmp[i][TRANS_ALIEN_ALIVE] > 0)
			intransfer += td.trAliensTmp[i][TRANS_ALIEN_ALIVE];
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
	for (i = 0; i < ccs.numAircraft; i++)
		if (td.trAircraftsTmp[i] > TRANS_LIST_EMPTY_SLOT) {
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
		MN_Popup(_("Not enough space"), _("Destination base does not have enough space in hangars.\n"));
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
	linkedList_t *cargoListAmount = NULL;
	char str[128];

	td.trCargoCountTmp = 0;
	memset(td.cargo, 0, sizeof(td.cargo));
	memset(trempl, 0, sizeof(trempl));

	/* Show items. */
	for (i = 0; i < csi.numODs; i++) {
		if (td.trItemsTmp[i] > 0) {
			LIST_AddString(&cargoList, _(csi.ods[i].name));
			LIST_AddString(&cargoListAmount, va("%i", td.trItemsTmp[i]));
			td.cargo[td.trCargoCountTmp].type = CARGO_TYPE_ITEM;
			td.cargo[td.trCargoCountTmp].itemidx = i;
			td.trCargoCountTmp++;
			if (td.trCargoCountTmp >= MAX_CARGO) {
				Com_DPrintf(DEBUG_CLIENT, "TR_CargoList: Cargo is full\n");
				break;
			}
		}
	}

	/* Show employees. */
	for (emplType = 0; emplType < MAX_EMPL; emplType++) {
		for (i = 0; i < ccs.numEmployees[emplType]; i++) {
			if (td.trEmployeesTmp[emplType][i]) {
				if (emplType == EMPL_SOLDIER || emplType == EMPL_PILOT) {
					employee_t *employee = td.trEmployeesTmp[emplType][i];
					if (emplType == EMPL_SOLDIER) {
						const rank_t *rank = CL_GetRankByIdx(employee->chr.score.rank);
						Com_sprintf(str, lengthof(str), _("Soldier %s %s"), _(rank->shortname), employee->chr.name);
					} else
						Com_sprintf(str, lengthof(str), _("Pilot %s"), employee->chr.name);
					LIST_AddString(&cargoList, str);
					LIST_AddString(&cargoListAmount, "1");
					td.cargo[td.trCargoCountTmp].type = CARGO_TYPE_EMPLOYEE;
					td.cargo[td.trCargoCountTmp].itemidx = employee->idx;
					td.trCargoCountTmp++;
					if (td.trCargoCountTmp >= MAX_CARGO) {
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
			LIST_AddString(&cargoList, E_GetEmployeeString(emplType));
			LIST_AddString(&cargoListAmount, va("%i", trempl[emplType]));
			td.cargo[td.trCargoCountTmp].type = CARGO_TYPE_EMPLOYEE;
			td.trCargoCountTmp++;
			if (td.trCargoCountTmp >= MAX_CARGO) {
				Com_DPrintf(DEBUG_CLIENT, "TR_CargoList: Cargo is full\n");
				break;
			}
		}
	}

	/* Show aliens. */
	for (i = 0; i < ccs.numAliensTD; i++) {
		if (td.trAliensTmp[i][TRANS_ALIEN_DEAD] > 0) {
			Com_sprintf(str, sizeof(str), _("Corpse of %s"),
				_(AL_AlienTypeToName(AL_GetAlienGlobalIDX(i))));
			LIST_AddString(&cargoList, str);
			LIST_AddString(&cargoListAmount, va("%i", td.trAliensTmp[i][TRANS_ALIEN_DEAD]));
			td.cargo[td.trCargoCountTmp].type = CARGO_TYPE_ALIEN_DEAD;
			td.cargo[td.trCargoCountTmp].itemidx = i;
			td.trCargoCountTmp++;
			if (td.trCargoCountTmp >= MAX_CARGO) {
				Com_DPrintf(DEBUG_CLIENT, "TR_CargoList: Cargo is full\n");
				break;
			}
		}
	}
	for (i = 0; i < ccs.numAliensTD; i++) {
		if (td.trAliensTmp[i][TRANS_ALIEN_ALIVE] > 0) {
			LIST_AddString(&cargoList, _(AL_AlienTypeToName(AL_GetAlienGlobalIDX(i))));
			LIST_AddString(&cargoListAmount, va("%i", td.trAliensTmp[i][TRANS_ALIEN_ALIVE]));
			td.cargo[td.trCargoCountTmp].type = CARGO_TYPE_ALIEN_ALIVE;
			td.cargo[td.trCargoCountTmp].itemidx = i;
			td.trCargoCountTmp++;
			if (td.trCargoCountTmp >= MAX_CARGO) {
				Com_DPrintf(DEBUG_CLIENT, "TR_CargoList: Cargo is full\n");
				break;
			}
		}
	}

	/* Show all aircraft. */
	for (i = 0; i < ccs.numAircraft; i++) {
		if (td.trAircraftsTmp[i] > TRANS_LIST_EMPTY_SLOT) {
			aircraft_t *aircraft = AIR_AircraftGetFromIDX(td.trAircraftsTmp[i]);
			assert(aircraft);
			Com_sprintf(str, lengthof(str), _("Aircraft %s"), aircraft->name);
			LIST_AddString(&cargoList, str);
			LIST_AddString(&cargoListAmount, "1");
			td.cargo[td.trCargoCountTmp].type = CARGO_TYPE_AIRCRAFT;
			td.cargo[td.trCargoCountTmp].itemidx = i;
			td.trCargoCountTmp++;
			if (td.trCargoCountTmp >= MAX_CARGO) {
				Com_DPrintf(DEBUG_CLIENT, "TR_CargoList: Cargo is full\n");
				break;
			}
		}
	}

	MN_RegisterLinkedListText(TEXT_CARGO_LIST, cargoList);
	MN_RegisterLinkedListText(TEXT_CARGO_LIST_AMOUNT, cargoListAmount);
}

/**
 * @brief Check if an aircraft should be displayed for transfer.
 * @param[in] i global idx of the aircraft
 * @return qtrue if the aircraft should be displayed, qfalse else.
 */
static qboolean TR_AircraftListSelect (int i)
{
	aircraft_t *aircraft;

	if (td.trAircraftsTmp[i] > TRANS_LIST_EMPTY_SLOT)	/* Already on transfer list. */
		return qfalse;

	aircraft = AIR_AircraftGetFromIDX(i);
	if (!AIR_IsAircraftInBase(aircraft))	/* Aircraft is not in base. */
		return qfalse;

	return qtrue;
}

/**
 * @brief Fills the items-in-base list with stuff available for transfer.
 * @note Filling the transfer list with proper stuff (items/employees/aliens/aircraft) is being done here.
 * @param[in] srcbase Pointer to the base the transfer starts from
 * @param[in] destbase Pointer to the base to transfer
 * @param[in] transferType Transfer category
 * @sa transferType_t
 */
static void TR_TransferSelect (base_t *srcbase, base_t *destbase, transferType_t transferType)
{
	linkedList_t *transferList = NULL;
	linkedList_t *transferListAmount = NULL;
	linkedList_t *transferListTransfered = NULL;
	int numempl[MAX_EMPL], trempl[MAX_EMPL];
	int i, j, cnt = 0;
	char str[128];

	/* reset for every new call */
	MN_ResetData(TEXT_TRANSFER_LIST);
	MN_ResetData(TEXT_TRANSFER_LIST_AMOUNT);
	MN_ResetData(TEXT_TRANSFER_LIST_TRANSFERED);

	/* Reset and fill temp employees arrays. */
	for (i = 0; i < MAX_EMPL; i++) {
		trempl[i] = numempl[i] = 0;
		for (j = 0; j < MAX_EMPLOYEES; j++) {
			if (td.trEmployeesTmp[i][j])
				trempl[i]++;
		}
	}

	switch (transferType) {
	case TRANS_TYPE_ITEM:
		if (B_GetBuildingStatus(destbase, B_STORAGE)) {
			for (i = 0; i < csi.numODs; i++)
				if ((srcbase->storage.numItems[i] || td.trItemsTmp[i]) && !csi.ods[i].isVirtual) {
					if (td.trItemsTmp[i] > 0)
						LIST_AddString(&transferListTransfered, va("%i", td.trItemsTmp[i]));
					else
						LIST_AddString(&transferListTransfered, "");
					Com_sprintf(str, sizeof(str), "%s", _(csi.ods[i].name));
					LIST_AddString(&transferList, str);
					LIST_AddString(&transferListAmount, va("%i", srcbase->storage.numItems[i]));
					cnt++;
				}
			if (!cnt) {
				LIST_AddString(&transferList, _("Storage is empty."));
				LIST_AddString(&transferListAmount, "");
				LIST_AddString(&transferListTransfered, "");
			}
		} else if (B_GetBuildingStatus(destbase, B_POWER)) {
			LIST_AddString(&transferList, _("Transfer is not possible - the base doesn't have a Storage."));
			LIST_AddString(&transferListAmount, "");
			LIST_AddString(&transferListTransfered, "");
		} else {
			LIST_AddString(&transferList, _("Transfer is not possible - the base does not have power supplies."));
			LIST_AddString(&transferListAmount, "");
			LIST_AddString(&transferListTransfered, "");
		}
		MN_ExecuteConfunc("trans_display_spinners %i", cnt);
		break;
	case TRANS_TYPE_EMPLOYEE:
		if (B_GetBuildingStatus(destbase, B_QUARTERS)) {
			employeeType_t emplType;
			for (emplType = 0; emplType < MAX_EMPL; emplType++) {
				for (i = 0; i < ccs.numEmployees[emplType]; i++) {
					const employee_t *employee = &ccs.employees[emplType][i];
					if (!E_IsInBase(employee, srcbase))
						continue;
					if (td.trEmployeesTmp[emplType][i])	/* Already on transfer list. */
						continue;
					if (emplType == EMPL_SOLDIER || emplType == EMPL_PILOT) {
						if (emplType == EMPL_SOLDIER) {
							const rank_t *rank = CL_GetRankByIdx(employee->chr.score.rank);
							Com_sprintf(str, sizeof(str), _("Soldier %s %s"), rank->shortname, employee->chr.name);
						} else
							Com_sprintf(str, sizeof(str), _("Pilot %s"), employee->chr.name);
						LIST_AddString(&transferList, str);
						LIST_AddString(&transferListAmount, "1");
						LIST_AddString(&transferListTransfered, "");
						cnt++;
					}
					numempl[emplType]++;
				}
			}
			for (i = 0; i < MAX_EMPL; i++) {
				if (i == EMPL_SOLDIER || i == EMPL_PILOT)
					continue;
				if (numempl[i] > 0) {
					LIST_AddString(&transferList, E_GetEmployeeString(i));
					LIST_AddString(&transferListAmount, va("%i", numempl[i]));
					if (trempl[i] > 0)
						LIST_AddString(&transferListTransfered, va("%i", trempl[i]));
					else
						LIST_AddString(&transferListTransfered, "");
					cnt++;
				}
			}
			if (!cnt) {
				LIST_AddString(&transferList, _("Living Quarters empty."));
				LIST_AddString(&transferListAmount, "");
				LIST_AddString(&transferListTransfered, "");
			}
		} else {
			LIST_AddString(&transferList, _("Transfer is not possible - the base doesn't have Living Quarters."));
			LIST_AddString(&transferListAmount, "");
			LIST_AddString(&transferListTransfered, "");
		}
		MN_ExecuteConfunc("trans_display_spinners 0");
		break;
	case TRANS_TYPE_ALIEN:
		if (B_GetBuildingStatus(destbase, B_ALIEN_CONTAINMENT)) {
			for (i = 0; i < ccs.numAliensTD; i++) {
				if (srcbase->alienscont[i].teamDef && srcbase->alienscont[i].amountDead > 0) {
					Com_sprintf(str, sizeof(str), _("Corpse of %s"),
					_(AL_AlienTypeToName(AL_GetAlienGlobalIDX(i))));
					LIST_AddString(&transferList, str);
					LIST_AddString(&transferListAmount, va("%i", srcbase->alienscont[i].amountDead));
					if (td.trAliensTmp[i][TRANS_ALIEN_DEAD] > 0)
						LIST_AddString(&transferListTransfered, va("%i", td.trAliensTmp[i][TRANS_ALIEN_DEAD]));
					else
						LIST_AddString(&transferListTransfered, "");
					cnt++;
				}
				if (srcbase->alienscont[i].teamDef && srcbase->alienscont[i].amountAlive > 0) {
					Com_sprintf(str, sizeof(str), _("Alive %s"),
						_(AL_AlienTypeToName(AL_GetAlienGlobalIDX(i))));
					LIST_AddString(&transferList, str);
					LIST_AddString(&transferListAmount, va("%i", srcbase->alienscont[i].amountAlive));
					if (td.trAliensTmp[i][TRANS_ALIEN_ALIVE] > 0)
						LIST_AddString(&transferListTransfered, va("%i", td.trAliensTmp[i][TRANS_ALIEN_ALIVE]));
					else
						LIST_AddString(&transferListTransfered, "");
					cnt++;
				}
			}
			if (!cnt) {
				LIST_AddString(&transferList, _("Alien Containment is empty."));
				LIST_AddString(&transferListAmount, "");
				LIST_AddString(&transferListTransfered, "");
			}
		} else if (B_GetBuildingStatus(destbase, B_POWER)) {
			LIST_AddString(&transferList, _("Transfer is not possible - the base doesn't have an Alien Containment."));
			LIST_AddString(&transferListAmount, "");
			LIST_AddString(&transferListTransfered, "");
		} else {
			LIST_AddString(&transferList, _("Transfer is not possible - the base does not have power supplies."));
			LIST_AddString(&transferListAmount, "");
			LIST_AddString(&transferListTransfered, "");
		}
		MN_ExecuteConfunc("trans_display_spinners 0");
		break;
	case TRANS_TYPE_AIRCRAFT:
		if (B_GetBuildingStatus(destbase, B_HANGAR) || B_GetBuildingStatus(destbase, B_SMALL_HANGAR)) {
			for (i = 0; i < MAX_AIRCRAFT; i++) {
				aircraft_t *aircraft = AIR_AircraftGetFromIDX(i);
				if (aircraft) {
					if (aircraft->homebase == srcbase && TR_AircraftListSelect(i)) {
						Com_sprintf(str, sizeof(str), _("Aircraft %s"), aircraft->name);
						LIST_AddString(&transferList, str);
						LIST_AddString(&transferListAmount, "1");
						LIST_AddString(&transferListTransfered, "");
						cnt++;
					}
				}
			}
			if (!cnt) {
				LIST_AddString(&transferList, _("No aircraft available for transfer."));
				LIST_AddString(&transferListAmount, "");
				LIST_AddString(&transferListTransfered, "");
			}
		} else {
			LIST_AddString(&transferList, _("Transfer is not possible - the base doesn't have a functional hangar."));
			LIST_AddString(&transferListAmount, "");
			LIST_AddString(&transferListTransfered, "");
		}
		MN_ExecuteConfunc("trans_display_spinners 0");
		break;
	default:
		Com_Printf("TR_TransferSelect: Unknown transferType id %i\n", transferType);
		MN_ExecuteConfunc("trans_display_spinners 0");
		return;
	}

	/* Update cargo list. */
	TR_CargoList();

	td.currentTransferType = transferType;
	MN_RegisterLinkedListText(TEXT_TRANSFER_LIST, transferList);
	MN_RegisterLinkedListText(TEXT_TRANSFER_LIST_AMOUNT, transferListAmount);
	MN_RegisterLinkedListText(TEXT_TRANSFER_LIST_TRANSFERED, transferListTransfered);
}

/**
 * @brief Function displays the transferable item/employee/aircraft/alien list
 * @sa TR_TransferStart_f
 * @sa TR_TransferInit_f
 */
static void TR_TransferSelect_f (void)
{
	int type;
	base_t *base = B_GetCurrentSelectedBase();

	if (!td.transferBase || !base)
		return;

	if (Cmd_Argc() < 2)
		type = td.currentTransferType;
	else
		type = TR_GetTransferType(Cmd_Argv(1));

	if (type < TRANS_TYPE_ITEM || type >= TRANS_TYPE_MAX)
		return;

	TR_TransferSelect(base, td.transferBase, type);
}

/**
 * @brief Unload everything from transfer cargo back to base.
 * @note This is being executed by pressing Unload button in menu.
 */
static void TR_TransferListClear_f (void)
{
	int i;
	base_t *base = B_GetCurrentSelectedBase();

	if (!base)
		return;

	for (i = 0; i < csi.numODs; i++) {	/* Return items. */
		if (td.trItemsTmp[i] > 0) {
			if (!strcmp(csi.ods[i].id, ANTIMATTER_TECH_ID))
				B_ManageAntimatter(base, td.trItemsTmp[i], qtrue);
			else
				B_UpdateStorageAndCapacity(base, &csi.ods[i], td.trItemsTmp[i], qfalse, qfalse);
		}
	}
	for (i = 0; i < ccs.numAliensTD; i++) {	/* Return aliens. */
		if (td.trAliensTmp[i][TRANS_ALIEN_ALIVE] > 0)
			AL_ChangeAliveAlienNumber(base, &(base->alienscont[i]), td.trAliensTmp[i][TRANS_ALIEN_ALIVE]);
		if (td.trAliensTmp[i][TRANS_ALIEN_DEAD] > 0)
			base->alienscont[i].amountDead += td.trAliensTmp[i][TRANS_ALIEN_DEAD];
	}

	/* Clear temporary cargo arrays. */
	memset(td.trItemsTmp, 0, sizeof(td.trItemsTmp));
	memset(td.trAliensTmp, 0, sizeof(td.trAliensTmp));
	memset(td.trEmployeesTmp, 0, sizeof(td.trEmployeesTmp));
	memset(td.trAircraftsTmp, TRANS_LIST_EMPTY_SLOT, sizeof(td.trAircraftsTmp));
	/* Update cargo list and items list. */
	TR_CargoList();
	TR_TransferSelect(base, td.transferBase, td.currentTransferType);
	MN_ExecuteConfunc("trans_resetscroll");
}

/**
 * @brief Unloads transfer cargo when finishing the transfer or destroys it when no buildings/base.
 * @param[in,out] destination The destination base - might be NULL in case the base
 * is already destroyed
 * @param[in] transfer Pointer to transfer in ccs.transfers.
 * @param[in] success True if the transfer reaches dest base, false if the base got destroyed.
 * @sa TR_TransferEnd
 */
static void TR_EmptyTransferCargo (base_t *destination, transfer_t *transfer, qboolean success)
{
	int i, j;

	assert(transfer);

	if (transfer->hasItems && success) {	/* Items. */
		if (!B_GetBuildingStatus(destination, B_STORAGE)) {
			Com_sprintf(cp_messageBuffer, sizeof(cp_messageBuffer), _("%s does not have Storage, items are removed!"), destination->name);
			MSO_CheckAddNewMessage(NT_TRANSFER_LOST, _("Transport mission"), cp_messageBuffer, qfalse, MSG_TRANSFERFINISHED, NULL);
			/* Items cargo is not unloaded, will be destroyed in TR_TransferCheck(). */
		} else {
			for (i = 0; i < csi.numODs; i++) {
				if (transfer->itemAmount[i] > 0) {
					if (!strcmp(csi.ods[i].id, ANTIMATTER_TECH_ID))
						B_ManageAntimatter(destination, transfer->itemAmount[i], qtrue);
					else
						B_UpdateStorageAndCapacity(destination, &csi.ods[i], transfer->itemAmount[i], qfalse, qtrue);
				}
			}
		}
	}

	if (transfer->hasEmployees && transfer->srcBase) {	/* Employees. (cannot come from a mission) */
		if (!success || !B_GetBuildingStatus(destination, B_QUARTERS)) {	/* Employees will be unhired. */
			if (success) {
				Com_sprintf(cp_messageBuffer, sizeof(cp_messageBuffer), _("%s does not have Living Quarters, employees got unhired!"), destination->name);
				MSO_CheckAddNewMessage(NT_TRANSFER_LOST, _("Transport mission"), cp_messageBuffer, qfalse, MSG_TRANSFERFINISHED, NULL);
			}
			for (i = 0; i < MAX_EMPL; i++) {
				for (j = 0; j < ccs.numEmployees[i]; j++) {
					if (transfer->employeeArray[i][j]) {
						employee_t *employee = transfer->employeeArray[i][j];
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
				for (j = 0; j < ccs.numEmployees[i]; j++) {
					if (transfer->employeeArray[i][j]) {
						employee_t *employee = transfer->employeeArray[i][j];
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
			Com_sprintf(cp_messageBuffer, sizeof(cp_messageBuffer), _("%s does not have Alien Containment, Aliens are removed!"), destination->name);
			MSO_CheckAddNewMessage(NT_TRANSFER_LOST, _("Transport mission"), cp_messageBuffer, qfalse, MSG_TRANSFERFINISHED, NULL);
			/* Aliens cargo is not unloaded, will be destroyed in TR_TransferCheck(). */
		} else {
			qboolean capacityWarning = qfalse;
			for (i = 0; i < ccs.numAliensTD; i++) {
				/* dead aliens */
				if (transfer->alienAmount[i][TRANS_ALIEN_DEAD] > 0) {
					destination->alienscont[i].amountDead += transfer->alienAmount[i][TRANS_ALIEN_DEAD];
				}
				/* alive aliens */
				if (transfer->alienAmount[i][TRANS_ALIEN_ALIVE] > 0) {
					int amount = min(transfer->alienAmount[i][TRANS_ALIEN_ALIVE], CAP_GetMax(destination, CAP_ALIENS) - CAP_GetCurrent(destination, CAP_ALIENS));

					if (transfer->alienAmount[i][TRANS_ALIEN_ALIVE] != amount)
						capacityWarning = qtrue;
					if (amount < 1)
						continue;

					AL_ChangeAliveAlienNumber(destination, &(destination->alienscont[i]), amount);
				}
			}
			if (capacityWarning) {
				Com_sprintf(cp_messageBuffer, sizeof(cp_messageBuffer), _("%s does not have enough Alien Containment space, some Aliens are removed!"), destination->name);
				MSO_CheckAddNewMessage(NT_TRANSFER_LOST, _("Transport mission"), cp_messageBuffer, qfalse, MSG_TRANSFERFINISHED, NULL);
			}
		}
	}

	/** @todo If source base is destroyed during transfer, aircraft doesn't exist anymore.
	 * aircraftArray should contain pointers to aircraftTemplates to avoid this problem, and be removed from
	 * source base as soon as transfer starts */
	if (transfer->hasAircraft && success && transfer->srcBase) {	/* Aircraft. Cannot come from mission */
		for (i = 0; i < ccs.numAircraft; i++) {
			if (transfer->aircraftArray[i] > TRANS_LIST_EMPTY_SLOT) {
				aircraft_t *aircraft = AIR_AircraftGetFromIDX(i);
				assert(aircraft);

				if ((AIR_CalculateHangarStorage(aircraft->tpl, destination, 0) <= 0) || !AIR_MoveAircraftIntoNewHomebase(aircraft, destination)) {
					/* No space, aircraft will be lost. */
					Com_sprintf(cp_messageBuffer, sizeof(cp_messageBuffer), _("%s does not have enough free space. Aircraft is lost!"), destination->name);
					MSO_CheckAddNewMessage(NT_TRANSFER_LOST, _("Transport mission"), cp_messageBuffer, qfalse, MSG_TRANSFERFINISHED, NULL);
					AIR_DeleteAircraft(aircraft);
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

	const technology_t *breathingTech;
	qboolean alienBreathing = qfalse;
	const objDef_t *alienBreathingObjDef;

	breathingTech = RS_GetTechByID(BREATHINGAPPARATUS_TECH);
	if (!breathingTech)
		Com_Error(ERR_DROP, "AL_AddAliens: Could not get breathing apparatus tech definition");
	alienBreathing = RS_IsResearched_ptr(breathingTech);
	alienBreathingObjDef = INVSH_GetItemByID(breathingTech->provides);
	if (!alienBreathingObjDef)
		Com_Error(ERR_DROP, "AL_AddAliens: Could not get breathing apparatus item definition");

	if (!base) {
		Com_Printf("TR_TransferAlienAfterMissionStart_f: No base selected!\n");
		return;
	}

	if (ccs.numTransfers >= MAX_TRANSFERS) {
		Com_DPrintf(DEBUG_CLIENT, "TR_TransferAlienAfterMissionStart: Max transfers reached.");
		return;
	}

	transfer = &ccs.transfers[ccs.numTransfers];

	if (transfer->active)
		Com_Error(ERR_DROP, "Transfer idx %i shouldn't be active.", ccs.numTransfers);

	/* Initialize transfer.
	 * calculate time to go from 1 base to another : 1 day for one quarter of the globe*/
	time = GetDistanceOnGlobe(base->pos, td.transferStartAircraft->pos) / 90.0f;
	transfer->event.day = ccs.date.day + floor(time);	/* add day */
	time = (time - floor(time)) * SECONDS_PER_DAY;	/* convert remaining time in second */
	transfer->event.sec = ccs.date.sec + round(time);
	/* check if event is not the following day */
	if (transfer->event.sec > SECONDS_PER_DAY) {
		transfer->event.sec -= SECONDS_PER_DAY;
		transfer->event.day++;
	}
	transfer->destBase = B_GetFoundedBaseByIDX(base->idx);	/* Destination base. */
	transfer->srcBase = NULL;	/* Source base. */
	transfer->active = qtrue;
	ccs.numTransfers++;

	alienCargoTypes = AL_GetAircraftAlienCargoTypes(td.transferStartAircraft);
	cargo = AL_GetAircraftAlienCargo(td.transferStartAircraft);
	for (i = 0; i < alienCargoTypes; i++, cargo++) {		/* Aliens. */
		if (!alienBreathing) {
			cargo->amountDead += cargo->amountAlive;
			cargo->amountAlive = 0;
		}
		if (cargo->amountAlive > 0) {
			for (j = 0; j < ccs.numAliensTD; j++) {
				if (!CHRSH_IsTeamDefAlien(&csi.teamDef[j]))
					continue;
				if (base->alienscont[j].teamDef == cargo->teamDef) {
					transfer->hasAliens = qtrue;
					transfer->alienAmount[j][TRANS_ALIEN_ALIVE] = cargo->amountAlive;
					cargo->amountAlive = 0;
					break;
				}
			}
		}
		if (cargo->amountDead > 0) {
			for (j = 0; j < ccs.numAliensTD; j++) {
				if (!CHRSH_IsTeamDefAlien(&csi.teamDef[j]))
					continue;
				if (base->alienscont[j].teamDef == cargo->teamDef) {
					transfer->hasAliens = qtrue;
					transfer->alienAmount[j][TRANS_ALIEN_DEAD] = cargo->amountDead;

					/* If we transfer aliens from battlefield add also breathing apparatur(s) */
					transfer->hasItems = qtrue;
					transfer->itemAmount[alienBreathingObjDef->idx] += cargo->amountDead;
					cargo->amountDead = 0;
					break;
				}
			}
		}
	}
	AL_SetAircraftAlienCargoTypes(td.transferStartAircraft, 0);

	/* Make sure that we don't use transferStartAircraft after this point
	 * (Could be a fake aircraft in case of base attack) */
	td.transferStartAircraft = NULL;

	Com_sprintf(message, sizeof(message), _("Transport mission started, cargo is being transported to %s"), transfer->destBase->name);
	MSO_CheckAddNewMessage(NT_TRANSFER_ALIENBODIES_DEFERED, _("Transport mission"), message, qfalse, MSG_TRANSFERFINISHED, NULL);
	MN_PopWindow(qfalse);
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
		Com_Printf("Usage: %s <base list index>\n", Cmd_Argv(0));
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
	linkedList_t *transfer = NULL;

	/* store the aircraft to be able to remove alien bodies */
	td.transferStartAircraft = aircraft;

	/* make sure that all tests here are the same than in TR_TransferBaseListClick_f */
	for (i = 0; i < MAX_BASES; i++) {
		const base_t *base = B_GetFoundedBaseByIDX(i);
		const char* string;
		if (!base)
			continue;
		/* don't display bases without Alien Containment */
		if (!base->hasBuilding[B_ALIEN_CONTAINMENT])
			continue;

		num = (base->capacities[CAP_ALIENS].max - base->capacities[CAP_ALIENS].cur);
		string = va(ngettext("(can host %i live alien)", "(can host %i live aliens)", num), num);
		string = va("%s %s", base->name, string);
		LIST_AddString(&transfer, string);
	}
	MN_RegisterLinkedListText(TEXT_LIST, transfer);
	MN_PushWindow("popup_transferbaselist", NULL);
}

/**
 * @brief Ends the transfer.
 * @param[in] transfer Pointer to transfer in ccs.transfers
 */
static void TR_TransferEnd (transfer_t *transfer)
{
	base_t* destination = transfer->destBase;
	assert(destination);

	if (!destination->founded) {
		TR_EmptyTransferCargo(NULL, transfer, qfalse);
		MSO_CheckAddNewMessage(NT_TRANSFER_LOST, _("Transport mission"), _("The destination base no longer exists! Transfer carge was lost, personnel has been discharged."), qfalse, MSG_TRANSFERFINISHED, NULL);
		/** @todo what if source base is lost? we won't be able to unhire transfered employees. */
	} else {
		char message[256];
		TR_EmptyTransferCargo(destination, transfer, qtrue);
		Com_sprintf(message, sizeof(message), _("Transport mission ended, unloading cargo in %s"), destination->name);
		MSO_CheckAddNewMessage(NT_TRANSFER_COMPLETED_SUCCESS, _("Transport mission"), message, qfalse, MSG_TRANSFERFINISHED, NULL);
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
	base_t *base = B_GetCurrentSelectedBase();

	if (td.currentTransferType == TRANS_TYPE_INVALID) {
		Com_Printf("TR_TransferStart_f: currentTransferType is wrong!\n");
		return;
	}

	if (!td.transferBase || !base) {
		Com_Printf("TR_TransferStart_f: No base selected!\n");
		return;
	}

	/* don't start any empty transport */
	if (!td.trCargoCountTmp) {
		return;
	}

	if (ccs.numTransfers >= MAX_TRANSFERS) {
		Com_DPrintf(DEBUG_CLIENT, "TR_TransferStart_f: Max transfers reached.");
		return;
	}

	transfer = &ccs.transfers[ccs.numTransfers];

	if (transfer->active)
		Com_Error(ERR_DROP, "Transfer idx %i shouldn't be active.", ccs.numTransfers);

	/* Initialize transfer. */
	/* calculate time to go from 1 base to another : 1 day for one quarter of the globe*/
	time = GetDistanceOnGlobe(td.transferBase->pos, base->pos) / 90.0f;
	transfer->event.day = ccs.date.day + floor(time);	/* add day */
	time = (time - floor(time)) * SECONDS_PER_DAY;	/* convert remaining time in second */
	transfer->event.sec = ccs.date.sec + round(time);
	/* check if event is not the following day */
	if (transfer->event.sec > SECONDS_PER_DAY) {
		transfer->event.sec -= SECONDS_PER_DAY;
		transfer->event.day++;
	}
	transfer->destBase = td.transferBase;	/* Destination base. */
	assert(transfer->destBase);
	transfer->srcBase = base;	/* Source base. */
	transfer->active = qtrue;
	ccs.numTransfers++;

	for (i = 0; i < csi.numODs; i++) {	/* Items. */
		if (td.trItemsTmp[i] > 0) {
			transfer->hasItems = qtrue;
			transfer->itemAmount[i] = td.trItemsTmp[i];
		}
	}
	/* Note that the employee remains hired in source base during the transfer, that is
	 * it takes Living Quarters capacity, etc, but it cannot be used anywhere. */
	for (i = 0; i < MAX_EMPL; i++) {		/* Employees. */
		for (j = 0; j < ccs.numEmployees[i]; j++) {
			if (td.trEmployeesTmp[i][j]) {
				employee_t *employee = td.trEmployeesTmp[i][j];
				transfer->hasEmployees = qtrue;

				assert(employee->baseHired == base);

				E_ResetEmployee(employee);
				transfer->employeeArray[i][j] = employee;
				employee->baseHired = NULL;
				employee->transfer = qtrue;
			}
		}
	}
	/** @todo This doesn't work - it would require to store the aliens as the
	 * first entries in the teamDef array - and this is not guaranteed. The
	 * alienAmount array may not contain more than numAliensTD entries though */
	for (i = 0; i < ccs.numAliensTD; i++) {		/* Aliens. */
		if (!CHRSH_IsTeamDefAlien(&csi.teamDef[i]))
			continue;
		if (td.trAliensTmp[i][TRANS_ALIEN_ALIVE] > 0) {
			transfer->hasAliens = qtrue;
			transfer->alienAmount[i][TRANS_ALIEN_ALIVE] = td.trAliensTmp[i][TRANS_ALIEN_ALIVE];
		}
		if (td.trAliensTmp[i][TRANS_ALIEN_DEAD] > 0) {
			transfer->hasAliens = qtrue;
			transfer->alienAmount[i][TRANS_ALIEN_DEAD] = td.trAliensTmp[i][TRANS_ALIEN_DEAD];
		}
	}
	memset(transfer->aircraftArray, TRANS_LIST_EMPTY_SLOT, sizeof(transfer->aircraftArray));
	for (i = 0; i < ccs.numAircraft; i++) {	/* Aircraft. */
		if (td.trAircraftsTmp[i] > TRANS_LIST_EMPTY_SLOT) {
			aircraft_t *aircraft = AIR_AircraftGetFromIDX(i);
			aircraft->status = AIR_TRANSFER;
			AIR_RemoveEmployees(aircraft);
			transfer->hasAircraft = qtrue;
			transfer->aircraftArray[i] = i;
		} else {
			transfer->aircraftArray[i] = TRANS_LIST_EMPTY_SLOT;
		}
	}

	/* Clear temporary cargo arrays. */
	memset(td.trItemsTmp, 0, sizeof(td.trItemsTmp));
	memset(td.trAliensTmp, 0, sizeof(td.trAliensTmp));
	memset(td.trEmployeesTmp, 0, sizeof(td.trEmployeesTmp));
	memset(td.trAircraftsTmp, TRANS_LIST_EMPTY_SLOT, sizeof(td.trAircraftsTmp));

	/* Recheck if production/research can be done on srcbase (if there are workers/scientists) */
	PR_ProductionAllowed(base);
	RS_ResearchAllowed(base);

	Com_sprintf(message, sizeof(message), _("Transport mission started, cargo is being transported to %s"), td.transferBase->name);
	MSO_CheckAddNewMessage(NT_TRANSFER_STARTED, _("Transport mission"), message, qfalse, MSG_TRANSFERFINISHED, NULL);
	MN_PopWindow(qfalse);
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
	base_t *base = B_GetCurrentSelectedBase();

	if (Cmd_Argc() < 2)
		return;

	if (!base)
		return;

	if (!td.transferBase) {
		MN_Popup(_("No target base selected"), _("Please select the target base from the list"));
		return;
	}

	num = atoi(Cmd_Argv(1));
	if (num < 0 || num >= csi.numODs)
		return;

	switch (td.currentTransferType) {
	case TRANS_TYPE_INVALID:	/**< No list was initialized before you call this. */
		return;
	case TRANS_TYPE_ITEM:
		for (i = 0; i < csi.numODs; i++) {
			if ((base->storage.numItems[i] || td.trItemsTmp[i]) && !csi.ods[i].isVirtual) {
				if (cnt == num) {
					int amount;
					const objDef_t *od = &csi.ods[i];

					if (Cmd_Argc() == 3)
						amount = atoi(Cmd_Argv(2));
					else
						amount = TR_GetTransferFactor();

					/* you can't transfer more item than you have */
					if (amount > 0) {
						amount = min(amount, base->storage.numItems[i]);
						if (amount == 0)
							return;
						/* you can only transfer items that destination base can accept */
						amount = TR_CheckItem(od, td.transferBase, amount);
					} else if (amount < 0) {
						amount = max(amount, - td.trItemsTmp[i]);
					}

					if (amount) {
						td.trItemsTmp[i] += amount;
						if (!strcmp(od->id, ANTIMATTER_TECH_ID))
							B_ManageAntimatter(base, amount, qfalse);
						else
							B_UpdateStorageAndCapacity(base, od, -amount, qfalse, qfalse);
						break;
					} else
						return;
				}
				cnt++;
			}
		}
		break;
	case TRANS_TYPE_EMPLOYEE:
		for (i = 0; i < ccs.numEmployees[EMPL_SOLDIER]; i++) {
			employee_t *employee = &ccs.employees[EMPL_SOLDIER][i];
			if (!E_IsInBase(employee, base))
				continue;
			if (td.trEmployeesTmp[EMPL_SOLDIER][i])
				continue;
			if (cnt == num) {
				if (TR_CheckEmployee(employee, td.transferBase)) {
					td.trEmployeesTmp[EMPL_SOLDIER][employee->idx] = employee;
					added = qtrue;
					break;
				} else
					return;
			}
			cnt++;
		}

		if (added) /* We already added a soldier, so break. */
			break;

		for (i = 0; i < ccs.numEmployees[EMPL_PILOT]; i++) {
			employee_t *employee = &ccs.employees[EMPL_PILOT][i];
			if (!E_IsInBase(employee, base))
				continue;
			if (td.trEmployeesTmp[EMPL_PILOT][i])
				continue;
			if (cnt == num) {
				if (TR_CheckEmployee(employee, td.transferBase)) {
					td.trEmployeesTmp[EMPL_PILOT][employee->idx] = employee;
					added = qtrue;
					break;
				} else
					return;
			}
			cnt++;
		}

		if (added) /* We already added a pilot, so break. */
			break;

		/* Reset and fill temp employees arrays. */
		for (emplType = 0; emplType < MAX_EMPL; emplType++) {
			numempl[emplType] = E_CountHired(base, emplType);
			for (i = 0; i < MAX_EMPLOYEES; i++) {
				if (td.trEmployeesTmp[emplType][i])
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
				int amount = min(E_CountHired(base, emplType), TR_GetTransferFactor());
				for (i = 0; i < ccs.numEmployees[emplType]; i++) {
					employee_t *employee = &ccs.employees[emplType][i];
					if (!E_IsInBase(employee, base))
						continue;
					if (td.trEmployeesTmp[emplType][employee->idx])	/* Already on transfer list. */
						continue;
					if (TR_CheckEmployee(employee, td.transferBase)) {
						td.trEmployeesTmp[emplType][employee->idx] = employee;
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
		if (!B_GetBuildingStatus(td.transferBase, B_ALIEN_CONTAINMENT))
			return;
		for (i = 0; i < ccs.numAliensTD; i++) {
			if (base->alienscont[i].teamDef && base->alienscont[i].amountDead > 0) {
				if (cnt == num) {
					td.trAliensTmp[i][TRANS_ALIEN_DEAD]++;
					/* Remove the corpse from Alien Containment. */
					base->alienscont[i].amountDead--;
					break;
				}
				cnt++;
			}
			if (base->alienscont[i].teamDef && base->alienscont[i].amountAlive > 0) {
				if (cnt == num) {
					if (TR_CheckAlien(i, td.transferBase)) {
						td.trAliensTmp[i][TRANS_ALIEN_ALIVE]++;
						/* Remove an alien from Alien Containment. */
						AL_ChangeAliveAlienNumber(base, &(base->alienscont[i]), -1);
						break;
					} else
						return;
				}
				cnt++;
			}
		}
		break;
	case TRANS_TYPE_AIRCRAFT:
		if (!B_GetBuildingStatus(td.transferBase, B_HANGAR) && !B_GetBuildingStatus(td.transferBase, B_SMALL_HANGAR))
			return;
		for (i = 0; i < MAX_AIRCRAFT; i++) {
			const aircraft_t *aircraft = AIR_AircraftGetFromIDX(i);
			if (!aircraft)
				return;
			if (aircraft->homebase == base && TR_AircraftListSelect(i)) {
				if (cnt == num) {
					if (TR_CheckAircraft(aircraft, td.transferBase)) {
						td.trAircraftsTmp[i] = i;
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

	TR_TransferSelect(base, td.transferBase, td.currentTransferType);
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

	baseInfo[0] = '\0';
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
	td.transferBase = destbase;
	Cvar_Set("mn_trans_base_name", destbase->name);
	Cvar_SetValue("mn_trans_base_id", destbase->idx);

	/* Update stuff-in-base list. */
	TR_TransferSelect(srcbase, destbase, td.currentTransferType);
}

/**
 * @brief Fills the optionlist with available bases to transfer to
 */
static void TR_InitBaseList (void)
{
	int baseIdx;
	const base_t const *currentBase = B_GetCurrentSelectedBase();
	menuOption_t *baseList = NULL;

	for (baseIdx = 0; baseIdx < ccs.numBases; baseIdx++) {
		const base_t const *base = B_GetFoundedBaseByIDX(baseIdx);
		if (!base)
			continue;
		if (base == currentBase)
			continue;

		MN_AddOption(&baseList, va("base%i", baseIdx), base->name, va("%i", baseIdx));
	}

	MN_RegisterOption(OPTION_BASELIST, baseList);
}

/**
 * @brief Callback to select destination base
 */
static void TR_SelectBase_f (void)
{
	int baseIdx;
	base_t *base = B_GetCurrentSelectedBase();
	base_t *destbase;

	if (Cmd_Argc() < 2) {
		Com_Printf("Usage: %s <baseIdx>\n", Cmd_Argv(0));
		return;
	}

	baseIdx = atoi(Cmd_Argv(1));
	destbase = B_GetFoundedBaseByIDX(baseIdx);

	TR_TransferBaseSelect(base, destbase);
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
	base_t *base = B_GetCurrentSelectedBase();

	if (Cmd_Argc() < 2)
		return;

	if (!base)
		return;

	num = atoi(Cmd_Argv(1));
	if (num < 0 || num >= MAX_CARGO)
		return;

	switch (td.cargo[num].type) {
	case CARGO_TYPE_ITEM:
		for (i = 0; i < csi.numODs; i++) {
			if (td.trItemsTmp[i] > 0) {
				if (cnt == num) {
					const int amount = min(TR_GetTransferFactor(), td.trItemsTmp[i]);
					/* you can't transfer more item than there are in current transfer */
					td.trItemsTmp[i] -= amount;
					if (!strcmp(csi.ods[i].id, ANTIMATTER_TECH_ID))
						B_ManageAntimatter(base, amount, qfalse);
					else
						B_UpdateStorageAndCapacity(base, &csi.ods[i], amount, qfalse, qfalse);
					break;
				}
				cnt++;
			}
		}
		break;
	case CARGO_TYPE_EMPLOYEE:
		for (i = 0; i < MAX_CARGO; i++) {
			/* Count previous types on the list. */
			switch (td.cargo[i].type) {
			case CARGO_TYPE_ITEM:
				entries++;
			default:
				break;
			}
		}
		/* Start increasing cnt from the amount of previous entries. */
		cnt = entries;
		for (i = 0; i < ccs.numEmployees[EMPL_SOLDIER]; i++) {
			if (td.trEmployeesTmp[EMPL_SOLDIER][i]) {
				if (cnt == num) {
					td.trEmployeesTmp[EMPL_SOLDIER][i] = NULL;
					removed = qtrue;
					break;
				}
				cnt++;
			}
		}
		if (removed)	/* We already removed soldier, break here. */
			break;
		for (i = 0; i < ccs.numEmployees[EMPL_PILOT]; i++) {
			if (td.trEmployeesTmp[EMPL_PILOT][i]) {
				if (cnt == num) {
					td.trEmployeesTmp[EMPL_PILOT][i] = NULL;
					removed = qtrue;
					break;
				}
				cnt++;
			}
		}
		if (removed)	/* We already removed pilot, break here. */
			break;

		Com_DPrintf(DEBUG_CLIENT, "TR_CargoListSelect_f: cnt: %i, num: %i\n", cnt, num);

		/* Reset and fill temp employees arrays. */
		for (emplType = 0; emplType < MAX_EMPL; emplType++) {
			numempl[emplType] = 0;
			/** @todo not ccs.numEmployees? isn't this (the td.trEmployeesTmp array)
			 * updated when an employee gets deleted? */
			for (i = 0; i < MAX_EMPLOYEES; i++) {
				if (td.trEmployeesTmp[emplType][i])
					numempl[emplType]++;
			}
		}

		for (emplType = 0; emplType < MAX_EMPL; emplType++) {
			if (numempl[emplType] < 1 || emplType == EMPL_SOLDIER || emplType == EMPL_PILOT)
				continue;
			if (cnt == num) {
				int amount = min(TR_GetTransferFactor(), E_CountHired(base, emplType));
				for (j = 0; j < ccs.numEmployees[emplType]; j++) {
					if (td.trEmployeesTmp[emplType][j]) {
						td.trEmployeesTmp[emplType][j] = NULL;
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
			switch (td.cargo[i].type) {
			case CARGO_TYPE_ITEM:
			case CARGO_TYPE_EMPLOYEE:
				entries++;
			default:
				break;
			}
		}
		/* Start increasing cnt from the amount of previous entries. */
		cnt = entries;
		for (i = 0; i < ccs.numAliensTD; i++) {
			if (td.trAliensTmp[i][TRANS_ALIEN_DEAD] > 0) {
				if (cnt == num) {
					td.trAliensTmp[i][TRANS_ALIEN_DEAD]--;
					base->alienscont[i].amountDead++;
					break;
				}
				cnt++;
			}
		}
		break;
	case CARGO_TYPE_ALIEN_ALIVE:
		for (i = 0; i < MAX_CARGO; i++) {
			/* Count previous types on the list. */
			switch (td.cargo[i].type) {
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
		for (i = 0; i < ccs.numAliensTD; i++) {
			if (td.trAliensTmp[i][TRANS_ALIEN_ALIVE] > 0) {
				if (cnt == num) {
					td.trAliensTmp[i][TRANS_ALIEN_ALIVE]--;
					AL_ChangeAliveAlienNumber(base, &(base->alienscont[i]), 1);
					break;
				}
				cnt++;
			}
		}
		break;
	case CARGO_TYPE_AIRCRAFT:
		for (i = 0; i < MAX_CARGO; i++) {
			/* Count previous types on the list. */
			switch (td.cargo[i].type) {
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
		for (i = 0; i < ccs.numAircraft; i++) {
			if (td.trAircraftsTmp[i] > TRANS_LIST_EMPTY_SLOT) {
				if (cnt == num) {
					td.trAircraftsTmp[i] = TRANS_LIST_EMPTY_SLOT;
					break;
				}
				cnt++;
			}
		}
		break;
	default:
		return;
	}

	TR_TransferSelect(base, td.transferBase, td.currentTransferType);
}

/**
 * @brief Notify that an aircraft has been removed.
 * @sa AIR_DeleteAircraft
 */
void TR_NotifyAircraftRemoved (const aircraft_t *aircraft)
{
	int i;

	if (aircraft->idx < 0 || aircraft->idx >= MAX_AIRCRAFT)
		return;

	for (i = 0; i < ccs.numTransfers; i++) {
		transfer_t *transfer = &ccs.transfers[i];
		int tmp = ccs.numAircraft;

		/* skip non active transfer */
		if (!transfer->active)
			continue;
		if (!transfer->hasAircraft)
			continue;
		REMOVE_ELEM_MEMSET(transfer->aircraftArray, aircraft->idx, tmp, TRANS_LIST_EMPTY_SLOT);
	}
}

/**
 * @brief Checks whether given transfer should be processed.
 * @sa CL_CampaignRun
 */
void TR_TransferCheck (void)
{
	int i;

	for (i = 0; i < ccs.numTransfers; i++) {
		transfer_t *transfer = &ccs.transfers[i];
		if (!transfer->active)
			continue;
		if (transfer->event.day < ccs.date.day || (transfer->event.day == ccs.date.day && ccs.date.sec >= transfer->event.sec)) {
			assert(transfer->destBase);
			TR_TransferEnd(transfer);
			REMOVE_ELEM(ccs.transfers, i, ccs.numTransfers);
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
	base_t *base = B_GetCurrentSelectedBase();

	/* Clear employees temp array. */
	memset(td.trEmployeesTmp, 0, sizeof(td.trEmployeesTmp));
	/* Clear aircraft temp array. */
	memset(td.trAircraftsTmp, TRANS_LIST_EMPTY_SLOT, sizeof(td.trAircraftsTmp));

	/* Update destination base list */
	TR_InitBaseList();

	/* Select first available base. */
	td.transferBase = B_GetFoundedBaseByIDX((base->idx + 1) % ccs.numBases);
	TR_TransferBaseSelect(base, td.transferBase);
	/* Set up cvar used to display transferBase. */
	if (td.transferBase) {
		Cvar_Set("mn_trans_base_name", td.transferBase->name);
		Cvar_SetValue("mn_trans_base_id", td.transferBase->idx);
	} else {
		Cvar_Set("mn_trans_base_id", "");
	}

	/* Set up cvar used with tabset */
	Cvar_Set("mn_itemtype", transferTypeIDs[0]);
	/* Select first available item */
	Cmd_ExecuteString(va("trans_type %s", transferTypeIDs[0]));

	/* Reset scrolling for item-in-base list */
	MN_ExecuteConfunc("trans_resetscroll");
}

/**
 * @brief Closes Transfer Menu and resets temp arrays.
 */
static void TR_TransferClose_f (void)
{
	base_t *base = B_GetCurrentSelectedBase();

	if (!base)
		return;

	/* Unload what was loaded. */
	TR_TransferListClear_f();

	/* Clear temporary cargo arrays. */
	memset(td.trItemsTmp, 0, sizeof(td.trItemsTmp));
	memset(td.trAliensTmp, 0, sizeof(td.trAliensTmp));
	memset(td.trEmployeesTmp, 0, sizeof(td.trEmployeesTmp));
	memset(td.trAircraftsTmp, TRANS_LIST_EMPTY_SLOT, sizeof(td.trAircraftsTmp));
}

#ifdef DEBUG
/**
 * @brief Lists an/all active transfer(s)
 */
static void TR_ListTransfers_f (void)
{
	int transIdx = -1;
	int i;

	if (Cmd_Argc() == 2) {
		transIdx = atoi(Cmd_Argv(1));
		if (transIdx < 0 || transIdx > MAX_TRANSFERS) {
			Com_Printf("Usage: %s [transferIDX]\nWithout parameter it lists all.\n", Cmd_Argv(0));
			return;
		}
	}

	if (!ccs.numTransfers)
		Com_Printf("No active transfers.\n");

	for (i = 0; i < ccs.numTransfers; i++) {
		const transfer_t *transfer = &ccs.transfers[i];
		dateLong_t date;

		if (transIdx >= 0 && i != transIdx)
			continue;
		if (!transfer->active)
			continue;

		/* @todo: we need a strftime feature to make this easier */
		CL_DateConvertLong(&transfer->event, &date);

		Com_Printf("Transfer #%d\n", i);
		Com_Printf("...From %d (%s) To %d (%s) Arrival: %04i-%02i-%02i %02i:%02i:%02i\n",
			(transfer->srcBase) ? transfer->srcBase->idx : -1,
			(transfer->srcBase) ? transfer->srcBase->name : "(null)",
			(transfer->destBase) ? transfer->destBase->idx : -1,
			(transfer->destBase) ? transfer->destBase->name : "(null)",
			date.year, date.month, date.day, date.hour, date.min, date.sec);

		/* ItemCargo */
		if (transfer->hasItems) {
			int j;
			Com_Printf("...ItemCargo:\n");
			for (j = 0; j < csi.numODs; j++) {
				if (transfer->itemAmount[j])
					Com_Printf("......%s: %i\n", csi.ods[j].id, transfer->itemAmount[j]);
			}
		}
		/* Carried Employees */
		if (transfer->hasEmployees) {
			int j;
			Com_Printf("...Carried Employee:\n");
			for (j = 0; j < MAX_EMPL; j++) {
				int k;
				for (k = 0; k < MAX_EMPLOYEES; k++) {
					const struct employee_s *employee = transfer->employeeArray[j][k];
					if (!employee)
						continue;
					if (employee->ugv) {
						/* @todo: improve ugv listing when they're implemented */
						Com_Printf("......ugv: %s [idx: %i]\n", employee->ugv->id, employee->idx);
					} else {
						Com_Printf("......%s (%s) / %s [idx: %i ucn: %i]\n", employee->chr.name,
							E_GetEmployeeString(employee->type),
							(employee->nation) ? employee->nation->id : "(nonation)",
							employee->idx, employee->chr.ucn);
						if (!employee->hired)
							Com_Printf("Warning: employee^ not hired!\n");
						if (!employee->transfer)
							Com_Printf("Warning: employee^ not marked as being transfered!\n");
					}
				}
			}
		}
		/* AlienCargo */
		if (transfer->hasAliens) {
			int j;
			Com_Printf("...AlienCargo:\n");
			for (j = 0; j < csi.numTeamDefs; j++) {
				if (transfer->alienAmount[j][TRANS_ALIEN_ALIVE] + transfer->alienAmount[j][TRANS_ALIEN_DEAD])
					Com_Printf("......%s alive: %i dead: %i\n", csi.teamDef[j].id, transfer->alienAmount[j][TRANS_ALIEN_ALIVE], transfer->alienAmount[j][TRANS_ALIEN_DEAD]);
			}
		}
		/* Transfered Aircraft */
		if (transfer->hasAircraft) {
			int j;
			Com_Printf("...Transfered Aircraft:\n");
			for (j = 0; j < ccs.numAircraft; j++) {
				const aircraft_t *aircraft;
				if (transfer->aircraftArray[j] == TRANS_LIST_EMPTY_SLOT)
					continue;
				aircraft = AIR_AircraftGetFromIDX(transfer->aircraftArray[j]);
				Com_Printf("......%s [idx: %i]\n", (aircraft) ? aircraft->id : "(null)", j);
			}
		}

	}
}
#endif

/**
 * @brief Save callback for xml savegames
 * @param[out] p XML Node structure, where we write the information to
 * @sa TR_LoadXML
 * @sa SAV_GameSaveXML
 */
qboolean TR_SaveXML (mxml_node_t *p)
{
	int i;
	mxml_node_t *n;
	n = mxml_AddNode(p, SAVE_TRANSFER_TRANSFERS);

	for (i = 0; i < ccs.numTransfers; i++) {
		int j;
		const transfer_t *transfer = &ccs.transfers[i];
		mxml_node_t *s;

		s = mxml_AddNode(n, SAVE_TRANSFER_TRANSFER);
		mxml_AddInt(s, SAVE_TRANSFER_DAY, transfer->event.day);
		mxml_AddInt(s, SAVE_TRANSFER_SEC, transfer->event.sec);
		if (!transfer->destBase) {
			Com_Printf("Could not save transfer, no destBase is set\n");
			return qfalse;
		}
		mxml_AddInt(s, SAVE_TRANSFER_DESTBASE, transfer->destBase->idx);
		/* scrBase can be NULL if this is alien (mission->base) transport 
		 * @sa TR_TransferAlienAfterMissionStart */
		if (transfer->srcBase)
			mxml_AddInt(s, SAVE_TRANSFER_SRCBASE, transfer->srcBase->idx);
		/* save items */
		if (transfer->hasItems) {
			for (j = 0; j < MAX_OBJDEFS; j++) {
				if (transfer->itemAmount[j] > 0) {
					const objDef_t *od = INVSH_GetItemByIDX(j);
					mxml_node_t *ss = mxml_AddNode(s, SAVE_TRANSFER_ITEM);

					assert(od);
					mxml_AddString(ss, SAVE_TRANSFER_ITEMID, od->id);
					mxml_AddInt(ss, SAVE_TRANSFER_AMOUNT, transfer->itemAmount[j]);
				}
			}
		}
		/* save aliens */
		if (transfer->hasAliens) {
			for (j = 0; j < ccs.numAliensTD; j++) {
				if (transfer->alienAmount[j][TRANS_ALIEN_ALIVE] > 0
				 || transfer->alienAmount[j][TRANS_ALIEN_DEAD] > 0)
				{
					teamDef_t *team = ccs.alienTeams[j];
					mxml_node_t *ss = mxml_AddNode(s, SAVE_TRANSFER_ALIEN);

					assert(team);
					mxml_AddString(ss, SAVE_TRANSFER_ALIENID, team->id);
					mxml_AddIntValue(ss, SAVE_TRANSFER_ALIVEAMOUNT, transfer->alienAmount[j][TRANS_ALIEN_ALIVE]);
					mxml_AddIntValue(ss, SAVE_TRANSFER_DEADAMOUNT, transfer->alienAmount[j][TRANS_ALIEN_DEAD]);
				}
			}
		}
		/* save employee */
		if (transfer->hasEmployees) {
			for (j = 0; j < MAX_EMPL; j++) {
				int k;
				for (k = 0; k < MAX_EMPLOYEES; k++) {
					const employee_t *empl = transfer->employeeArray[j][k];
					if (empl) {
						mxml_node_t *ss = mxml_AddNode(s, SAVE_TRANSFER_EMPLOYEE);

						mxml_AddInt(ss, SAVE_TRANSFER_UCN, empl->chr.ucn);
					}
				}
			}
		}
		/* save aircraft */
		if (transfer->hasAircraft) {
			for (j = 0; j < ccs.numAircraft; j++) {
				if (transfer->aircraftArray[j] > TRANS_LIST_EMPTY_SLOT) {
					mxml_node_t *ss = mxml_AddNode(s, SAVE_TRANSFER_AIRCRAFT);
					mxml_AddInt(ss, SAVE_TRANSFER_ID, j);
				}
			}
		}
	}
	return qtrue;
}

/**
 * @brief Load callback for xml savegames
 * @param[in] p XML Node structure, where we get the information from
 * @sa TR_SaveXML
 * @sa SAV_GameLoadXML
 */
qboolean TR_LoadXML (mxml_node_t *p)
{
	mxml_node_t *n, *s;

	n = mxml_GetNode(p, SAVE_TRANSFER_TRANSFERS);
	if (!n)
		return qfalse;

	assert(ccs.numBases);

	for (s = mxml_GetNode(n, SAVE_TRANSFER_TRANSFER); s && ccs.numTransfers < MAX_TRANSFERS; s = mxml_GetNextNode(s, n, SAVE_TRANSFER_TRANSFER)) {
		mxml_node_t *ss;
		transfer_t *transfer = &ccs.transfers[ccs.numTransfers];

		transfer->destBase = B_GetBaseByIDX(mxml_GetInt(s, SAVE_TRANSFER_DESTBASE, BYTES_NONE));
		if (!transfer->destBase) {
			Com_Printf("Error: Transfer has no destBase set\n");
			return qfalse;
		}
		transfer->srcBase = B_GetBaseByIDX(mxml_GetInt(s, SAVE_TRANSFER_SRCBASE, BYTES_NONE));

		transfer->event.day = mxml_GetInt(s, SAVE_TRANSFER_DAY, 0);
		transfer->event.sec = mxml_GetInt(s, SAVE_TRANSFER_SEC, 0);
		transfer->active = qtrue;

		/* Initializing some variables */
		transfer->hasItems = qfalse;
		transfer->hasEmployees = qfalse;
		transfer->hasAliens = qfalse;
		transfer->hasAircraft = qfalse;
		memset(transfer->itemAmount, 0, sizeof(transfer->itemAmount));
		memset(transfer->alienAmount, 0, sizeof(transfer->alienAmount));
		memset(transfer->employeeArray, 0, sizeof(transfer->employeeArray));
		memset(transfer->aircraftArray, TRANS_LIST_EMPTY_SLOT, sizeof(transfer->aircraftArray));

		/* load items */
		/* If there is at last one element, hasItems is true */
		ss = mxml_GetNode(s, SAVE_TRANSFER_ITEM);
		if (ss) {
			transfer->hasItems = qtrue;
			for (; ss; ss = mxml_GetNextNode(ss, s, SAVE_TRANSFER_ITEM)) {
				const char *itemId = mxml_GetString(ss, SAVE_TRANSFER_ITEMID);
				const objDef_t *od = INVSH_GetItemByID(itemId);

				if (od)
					transfer->itemAmount[od->idx] = mxml_GetInt(ss, SAVE_TRANSFER_AMOUNT, 1);
			}
		}
		/* load aliens */
		ss = mxml_GetNode(s, SAVE_TRANSFER_ALIEN);
		if (ss) {
			transfer->hasAliens = qtrue;
			for (; ss; ss = mxml_GetNextNode(ss, s, SAVE_TRANSFER_ALIEN)) {
				const int alive = mxml_GetInt(ss, SAVE_TRANSFER_ALIVEAMOUNT, 0);
				const int dead  = mxml_GetInt(ss, SAVE_TRANSFER_DEADAMOUNT, 0);
				const char *id = mxml_GetString(ss, SAVE_TRANSFER_ALIENID);
				int j;

				/* look for alien teamDef */
				for (j = 0; j < ccs.numAliensTD; j++) {
					if (ccs.alienTeams[j] && !strcmp(id, ccs.alienTeams[j]->id))
						break;
				}

				if (j < ccs.numAliensTD) {
					transfer->alienAmount[j][TRANS_ALIEN_ALIVE] = alive;
					transfer->alienAmount[j][TRANS_ALIEN_DEAD] = dead;
				} else {
					Com_Printf("CL_LoadXML: AlienId '%s' is invalid\n", id);
				}
			}
		}
		/* load employee */
		ss = mxml_GetNode(s, SAVE_TRANSFER_EMPLOYEE);
		if (ss) {
			transfer->hasEmployees = qtrue;
			for (; ss; ss = mxml_GetNextNode(ss, s, SAVE_TRANSFER_EMPLOYEE)) {
				const int ucn = mxml_GetInt(ss, SAVE_TRANSFER_UCN, -1);
				employee_t *empl = E_GetEmployeeFromChrUCN(ucn);

				if (!empl) {
					Com_Printf("Error: No employee found with UCN: %i\n", ucn);
					return qfalse;
				}

				transfer->employeeArray[empl->type][empl->idx] = empl;
				transfer->employeeArray[empl->type][empl->idx]->transfer = qtrue;
			}
		}
		/* load aircraft */
		ss = mxml_GetNode(s, SAVE_TRANSFER_AIRCRAFT);
		if (ss) {
			transfer->hasAircraft = qtrue;
			for (; ss; ss = mxml_GetNextNode(ss, s, SAVE_TRANSFER_AIRCRAFT)) {
				const int j = mxml_GetInt(ss, SAVE_TRANSFER_ID, -1);

				if (j >= 0 && j < ccs.numAircraft)
					transfer->aircraftArray[j] = j;
			}
		}
		ccs.numTransfers++;
	}

	return qtrue;
}

/**
 * @brief Callback for adjusting spinner controls of item transferlist on scrolling
 */
static void TR_TransferList_Scroll_f (void)
{
	int i;
	int cnt = 0;
	int transferType;
	int viewPos;
	base_t *srcBase = B_GetCurrentSelectedBase();

	if (!srcBase)
		return;

	if (Cmd_Argc() < 3) {
		Com_Printf("Usage: %s <transferType> <viewPos>\n", Cmd_Argv(0));
		return;
	}

	transferType = TR_GetTransferType(Cmd_Argv(1));
	viewPos = atoi(Cmd_Argv(2));

	/* spinners are only on items screen */
	if (transferType != TRANS_TYPE_ITEM)
		return;

	for (i = 0; i < csi.numODs; i++) {
		if ((srcBase->storage.numItems[i] || td.trItemsTmp[i]) && !csi.ods[i].isVirtual) {
			if (cnt >= (viewPos + MAX_TRANSLIST_MENU_ENTRIES))
				break;
			if (cnt >= viewPos)
				MN_ExecuteConfunc("trans_updatespinners %i %i %i %i", cnt - viewPos,
						td.trItemsTmp[i], 0, srcBase->storage.numItems[i] + td.trItemsTmp[i]);
			cnt++;
		}
	}
}

/**
 * @brief Defines commands and cvars for the Transfer menu(s).
 * @sa MN_InitStartup
 */
void TR_InitStartup (void)
{
	/* add commands */
	Cmd_AddCommand("trans_init", TR_Init_f, "Init function for Transfer menu");
	Cmd_AddCommand("trans_start", TR_TransferStart_f, "Starts the transfer");
	Cmd_AddCommand("trans_type", TR_TransferSelect_f, "Switch between transfer types (employees, techs, items)");
	Cmd_AddCommand("trans_emptyairstorage", TR_TransferListClear_f, "Unload everything from transfer cargo back to base");
	Cmd_AddCommand("trans_list_click", TR_TransferListSelect_f, "Callback for transfer list node click");
	Cmd_AddCommand("trans_cargolist_click", TR_CargoListSelect_f, "Callback for cargo list node click");
	Cmd_AddCommand("trans_close", TR_TransferClose_f, "Callback for closing Transfer Menu");
	Cmd_AddCommand("trans_selectbase", TR_SelectBase_f, "Callback for selecting a base");
	Cmd_AddCommand("trans_baselist_click", TR_TransferBaseListClick_f, "Callback for choosing base while recovering alien after mission");
	Cmd_AddCommand("trans_list_scroll", TR_TransferList_Scroll_f, "");
#ifdef DEBUG
	Cmd_AddCommand("debug_listtransfers", TR_ListTransfers_f, "Lists an/all active transfer(s)");
#endif

	memset(&td, 0, sizeof(td));
	memset(td.trAircraftsTmp, TRANS_LIST_EMPTY_SLOT, sizeof(td.trAircraftsTmp));
}

