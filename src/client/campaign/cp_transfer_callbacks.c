/**
 * @file cp_transfer_callbacks.c
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

#include "../cl_shared.h"
#include "cp_campaign.h"
#include "cp_transfer_callbacks.h"
#include "cp_transfer.h"
#include "../ui/ui_main.h"
#include "../ui/ui_popup.h"
#include "../ui/ui_data.h"

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

/** @todo move this into ccs_t */
static transferData_t td;

/** @brief Max values for transfer factors. */
static const int MAX_TR_FACTORS = 500;
/** @brief number of entries on menu */
static const int MAX_TRANSLIST_MENU_ENTRIES = 21;

/**
 * @brief Counts the stunned aliens in the cargo of the aircraft.
 * @param[in] transferAircraft with the stunned aliens.
 */
static int TR_CountStunnedAliensInCargo (const aircraft_t *transferAircraft)
{
	int stunnedAliens = 0;
	const aliensTmp_t *cargo = AL_GetAircraftAlienCargo(transferAircraft);

	if (cargo) {
		int i;
		for (i = 0; i < MAX_CARGO; i++)
			stunnedAliens += cargo[i].amountAlive;
	}
	return stunnedAliens;
}

/**
 * @brief Action to realize when clicking on Transfer Menu.
 * @note This menu is used when a dropship ending a mission collected alien bodies, but there's no alien cont. in home base
 * @sa TR_TransferAliensFromMission_f
 */
static void TR_TransferBaseListClick_f (void)
{
	int num;
	base_t *base;

	assert(td.transferStartAircraft);

	if (Cmd_Argc() < 2) {
		Com_Printf("Usage: %s <base index>\n", Cmd_Argv(0));
		return;
	}

	num = atoi(Cmd_Argv(1));
	base = B_GetFoundedBaseByIDX(num);
	if (!base) {
		Com_Printf("TR_TransferBaseListClick_f: baseIdx %i doesn't exist.\n", num);
		return;
	}

	TR_TransferAlienAfterMissionStart(base, td.transferStartAircraft);
}

/**
 * @brief Shows available bases to collect aliens after a mission is finished.
 * @sa TR_TransferBaseListClick_f
 */
static void TR_TransferAliensFromMission_f (void)
{
	const vec4_t green = {0.0f, 1.0f, 0.0f, 0.8f};
	const vec4_t yellow = {1.0f, 0.874f, 0.294f, 1.0f};
	const vec4_t red = {1.0f, 0.0f, 0.0f, 0.8f};
	int i;
	aircraft_t *aircraft;
	int stunnedAliens;
	uiNode_t *baseList = NULL;
	base_t *base;

	if (Cmd_Argc() < 2) {
		Com_Printf("Usage: %s <aircraft index>\n", Cmd_Argv(0));
		return;
	}

	i = atoi(Cmd_Argv(1));
	aircraft = AIR_AircraftGetFromIDX(i);
	if (!aircraft) {
		Com_Printf("Usage: No aircraft with index %i\n", i);
		return;
	}

	stunnedAliens = TR_CountStunnedAliensInCargo(aircraft);

	/* store the aircraft to be able to remove alien bodies */
	td.transferStartAircraft = aircraft;

	base = NULL;
	while ((base = B_GetNextFounded(base)) != NULL) {
		const char* string;
		uiNode_t *option;
		int freeSpace;

		if (!AC_ContainmentAllowed(base))
			continue;

		freeSpace = B_FreeCapacity(base, CAP_ALIENS);

		string = va(ngettext("(can host %i live alien)", "(can host %i live aliens)", freeSpace), freeSpace);
		string = va("%s %s", base->name, string);
		option = UI_AddOption(&baseList, va("base%i", base->idx), string, va("%i", base->idx));

		/** @todo tooltip assigining wrong here, also ui doesn't yet support tooltip on options */
		if (freeSpace >= stunnedAliens) {
			Vector4Copy(green, option->color);
			/* option->tooltip = _("Has free space for all captured aliens."); */
		} else if (freeSpace > 0) {
			Vector4Copy(yellow, option->color);
			/* option->tooltip = _("Has free space for some captured aliens, others will die."); */
		} else {
			Vector4Copy(red, option->color);
			/* option->tooltip = _("Doesn't have free space captured aliens, only dead ones can be stored."); */
		}
	}

	if (baseList != NULL){
		UI_RegisterOption(OPTION_BASELIST, baseList);
		UI_PushWindow("popup_transferbaselist", NULL);
	} else {
		/** @todo send a message (?) */
		Com_Printf("TR_TransferAliensFromMission_f: No base with alien containment available.\n");
	}
}

/**
 * @brief Starts the transfer.
 * @sa TR_TransferSelect_f
 * @sa TR_TransferInit_f
 */
static void TR_TransferStart_f (void)
{
	char message[1024];
	base_t *base = B_GetCurrentSelectedBase();

	if (td.currentTransferType == TRANS_TYPE_INVALID) {
		Com_Printf("TR_TransferStart_f: currentTransferType is wrong!\n");
		return;
	}

	if (TR_TransferStart(base, &td) == NULL)
		return;

	/* Clear temporary cargo arrays. */
	memset(td.trItemsTmp, 0, sizeof(td.trItemsTmp));
	memset(td.trAliensTmp, 0, sizeof(td.trAliensTmp));
	memset(td.trEmployeesTmp, 0, sizeof(td.trEmployeesTmp));
	LIST_Delete(&td.aircraft);

	Com_sprintf(message, sizeof(message), _("Transport mission started, cargo is being transported to %s"), td.transferBase->name);
	MSO_CheckAddNewMessage(NT_TRANSFER_STARTED, _("Transport mission"), message, qfalse, MSG_TRANSFERFINISHED, NULL);
	UI_PopWindow(qfalse);
}


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
		const objDef_t *object = INVSH_GetItemByIDX(i);
		const int itemCargoAmount = td.trItemsTmp[i];
		if (itemCargoAmount > 0) {
			if (!strcmp(object->id, ANTIMATTER_TECH_ID))
				amtransfer = itemCargoAmount;
			else
				intransfer += object->size * itemCargoAmount;
		}
	}

	/* Is this antimatter and destination base has enough space in Antimatter Storage? */
	if (!strcmp(od->id, ANTIMATTER_TECH_ID)) {
		/* Give some meaningful feedback to the player if the player clicks on an a.m. item but base doesn't have am storage. */
		if (!B_GetBuildingStatus(destbase, B_ANTIMATTER)) {
			UI_Popup(_("Missing storage"), _("Destination base does not have an Antimatter Storage.\n"));
			return 0;
		}
		amount = min(amount, B_FreeCapacity(destbase, CAP_ANTIMATTER) - amtransfer);
		if (amount <= 0) {
			UI_Popup(_("Not enough space"), _("Destination base does not have enough\nAntimatter Storage space to store more antimatter.\n"));
			return 0;
		}
	} else {	/*This is not antimatter */
		if (!B_GetBuildingStatus(destbase, B_STORAGE))	/* Return if the target base doesn't have storage or power. */
			return 0;

		/* Does the destination base has enough space in storage? */
		amount = min(amount, destbase->capacities[CAP_ITEMS].max - destbase->capacities[CAP_ITEMS].cur - intransfer / od->size);
		if (amount <= 0) {
			UI_Popup(_("Not enough space"), _("Destination base does not have enough\nStorage space to store this item.\n"));
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
		UI_Popup(_("Not enough space"), _("Destination base does not have enough space\nin Living Quarters.\n"));
		return qfalse;
	}

	switch (employee->type) {
	case EMPL_SOLDIER:
		/* Is this a soldier assigned to aircraft? */
		if (AIR_IsEmployeeInAircraft(employee, NULL)) {
			const rank_t *rank = CL_GetRankByIdx(employee->chr.score.rank);
			Com_sprintf(popupText, sizeof(popupText), _("%s %s is assigned to aircraft and cannot be\ntransfered to another base.\n"),
				_(rank->shortname), employee->chr.name);
			UI_Popup(_("Soldier in aircraft"), popupText);
			return qfalse;
		}
		break;
	case EMPL_PILOT:
		/* Is this a pilot assigned to aircraft? */
		if (AIR_IsEmployeeInAircraft(employee, NULL)) {
			Com_sprintf(popupText, sizeof(popupText), _("%s is assigned to aircraft and cannot be\ntransfered to another base.\n"),
				employee->chr.name);
			UI_Popup(_("Pilot in aircraft"), popupText);
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
static qboolean TR_CheckAlien (const base_t *destbase)
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
		UI_Popup(_("Not enough space"), _("Destination base does not have enough space\nin Alien Containment.\n"));
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
	int hangarStorage;
	int numAircraftTransfer = 0;
	aircraft_t *aircraftTemp = NULL;

	assert(aircraft);
	assert(destbase);

	/* Hangars in destbase functional? */
	if (!B_GetBuildingStatus(destbase, B_POWER)) {
		UI_Popup(_("Hangars not ready"), _("Destination base does not have hangars ready.\nProvide power supplies.\n"));
		return qfalse;
	} else if (!B_GetBuildingStatus(destbase, B_COMMAND)) {
		UI_Popup(_("Hangars not ready"), _("Destination base does not have command centre.\nHangars not functional.\n"));
		return qfalse;
	} else if (!AIR_AircraftAllowed(destbase)) {
		UI_Popup(_("Hangars not ready"), _("Destination base does not have any hangar."));
		return qfalse;
	}

	/* Count weight and number of all aircraft already on the transfer list that goes
	 * into the same hangar type than aircraft. */
	while ((aircraftTemp = (aircraft_t*)LIST_GetNext(td.aircraft, (void*)aircraftTemp))) {
		if (aircraftTemp->size == aircraft->size)
			numAircraftTransfer++;
	}
	/* Is there a place for this aircraft in destination base? */
	hangarStorage = AIR_CalculateHangarStorage(aircraft->tpl, destbase, numAircraftTransfer);
	if (hangarStorage <= 0) {
		UI_Popup(_("Not enough space"), _("Destination base does not have enough space in hangars.\n"));
		return qfalse;
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
	aircraft_t *aircraft = NULL;

	td.trCargoCountTmp = 0;
	memset(td.cargo, 0, sizeof(td.cargo));
	memset(trempl, 0, sizeof(trempl));

	/* Show items. */
	for (i = 0; i < csi.numODs; i++) {
		const objDef_t *od = INVSH_GetItemByIDX(i);
		const int itemCargoAmount = td.trItemsTmp[i];
		if (itemCargoAmount > 0) {
			LIST_AddString(&cargoList, _(od->name));
			LIST_AddString(&cargoListAmount, va("%i", itemCargoAmount));
			TR_AddData(&td, CARGO_TYPE_ITEM, od);
		}
	}

	/* Show employees. */
	for (emplType = 0; emplType < MAX_EMPL; emplType++) {
		for (i = 0; i < ccs.numEmployees[emplType]; i++) {
			if (td.trEmployeesTmp[emplType][i]) {
				if (emplType == EMPL_SOLDIER || emplType == EMPL_PILOT) {
					const employee_t *employee = td.trEmployeesTmp[emplType][i];
					if (emplType == EMPL_SOLDIER) {
						const rank_t *rank = CL_GetRankByIdx(employee->chr.score.rank);
						Com_sprintf(str, lengthof(str), _("Soldier %s %s"), _(rank->shortname), employee->chr.name);
					} else
						Com_sprintf(str, lengthof(str), _("Pilot %s"), employee->chr.name);
					LIST_AddString(&cargoList, str);
					LIST_AddString(&cargoListAmount, "1");
					TR_AddData(&td, CARGO_TYPE_EMPLOYEE, employee);
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
			TR_AddData(&td, CARGO_TYPE_EMPLOYEE, NULL);
		}
	}

	/* Show aliens. */
	for (i = 0; i < ccs.numAliensTD; i++) {
		if (td.trAliensTmp[i][TRANS_ALIEN_DEAD] > 0) {
			const teamDef_t* teamDef = AL_GetAlienTeamDef(i);
			Com_sprintf(str, sizeof(str), _("Corpse of %s"), _(teamDef->name));
			LIST_AddString(&cargoList, str);
			LIST_AddString(&cargoListAmount, va("%i", td.trAliensTmp[i][TRANS_ALIEN_DEAD]));
			TR_AddData(&td, CARGO_TYPE_ALIEN_DEAD, teamDef);
		}
	}
	for (i = 0; i < ccs.numAliensTD; i++) {
		if (td.trAliensTmp[i][TRANS_ALIEN_ALIVE] > 0) {
			const teamDef_t* teamDef = AL_GetAlienTeamDef(i);
			LIST_AddString(&cargoList, _(teamDef->name));
			LIST_AddString(&cargoListAmount, va("%i", td.trAliensTmp[i][TRANS_ALIEN_ALIVE]));
			TR_AddData(&td, CARGO_TYPE_ALIEN_ALIVE, teamDef);
		}
	}

	/* Show all aircraft. */
	while ((aircraft = (aircraft_t*)LIST_GetNext(td.aircraft, (void*)aircraft))) {
		Com_sprintf(str, lengthof(str), _("Aircraft %s"), aircraft->name);
		LIST_AddString(&cargoList, str);
		LIST_AddString(&cargoListAmount, "1");
		TR_AddData(&td, CARGO_TYPE_AIRCRAFT, aircraft);
	}

	UI_RegisterLinkedListText(TEXT_CARGO_LIST, cargoList);
	UI_RegisterLinkedListText(TEXT_CARGO_LIST_AMOUNT, cargoListAmount);
}

/**
 * @brief Check if an aircraft should be displayed for transfer.
 * @param[in] i global idx of the aircraft
 * @return qtrue if the aircraft should be displayed, qfalse else.
 */
static qboolean TR_AircraftListSelect (const aircraft_t *aircraft)
{
	if (!AIR_IsAircraftInBase(aircraft))	/* Aircraft is not in base. */
		return qfalse;
	if (LIST_GetPointer(td.aircraft, aircraft))	/* Already on transfer list. */
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
	UI_ResetData(TEXT_TRANSFER_LIST);
	UI_ResetData(TEXT_TRANSFER_LIST_AMOUNT);
	UI_ResetData(TEXT_TRANSFER_LIST_TRANSFERED);

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
		if (B_GetBuildingStatus(destbase, B_ANTIMATTER)) {
			const objDef_t *od = INVSH_GetItemByID(ANTIMATTER_TECH_ID);
			const int itemCargoAmount = td.trItemsTmp[od->idx];
			const int antiMatterInBase = B_AntimatterInBase(srcbase);
			if (itemCargoAmount || antiMatterInBase) {
				if (itemCargoAmount > 0)
					LIST_AddString(&transferListTransfered, va("%i", itemCargoAmount));
				else
					LIST_AddString(&transferListTransfered, "");
				Com_sprintf(str, sizeof(str), "%s", _(od->name));
				LIST_AddString(&transferList, str);
				LIST_AddString(&transferListAmount, va("%i", antiMatterInBase));
				cnt++;
			}
		}
		if (B_GetBuildingStatus(destbase, B_STORAGE)) {
			for (i = 0; i < csi.numODs; i++) {
				const objDef_t *od = INVSH_GetItemByIDX(i);
				const int itemCargoAmount = td.trItemsTmp[od->idx];
				if (!B_ItemIsStoredInBaseStorage(od))
					continue;
				if (itemCargoAmount || B_ItemInBase(od, srcbase) > 0) {
					if (itemCargoAmount > 0)
						LIST_AddString(&transferListTransfered, va("%i", itemCargoAmount));
					else
						LIST_AddString(&transferListTransfered, "");
					Com_sprintf(str, sizeof(str), "%s", _(od->name));
					LIST_AddString(&transferList, str);
					LIST_AddString(&transferListAmount, va("%i", B_ItemInBase(od, srcbase)));
					cnt++;
				}
			}
			if (!cnt) {
				LIST_AddString(&transferList, _("Storage is empty."));
				LIST_AddString(&transferListAmount, "");
				LIST_AddString(&transferListTransfered, "");
			}
		} else if (B_GetBuildingStatus(destbase, B_POWER) && cnt == 0) {
			LIST_AddString(&transferList, _("Transfer is not possible - the base doesn't have a Storage."));
			LIST_AddString(&transferListAmount, "");
			LIST_AddString(&transferListTransfered, "");
		} else if (cnt == 0) {
			LIST_AddString(&transferList, _("Transfer is not possible - the base does not have power supplies."));
			LIST_AddString(&transferListAmount, "");
			LIST_AddString(&transferListTransfered, "");
		}
		UI_ExecuteConfunc("trans_display_spinners %i", cnt);
		break;
	case TRANS_TYPE_EMPLOYEE:
		if (B_GetBuildingStatus(destbase, B_QUARTERS)) {
			employeeType_t emplType;
			for (emplType = 0; emplType < MAX_EMPL; emplType++) {
				employee_t *employee = NULL;
				while ((employee = E_GetNextFromBase(emplType, employee, srcbase))) {
					if (td.trEmployeesTmp[emplType][employee->idx])	/* Already on transfer list. */
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
		UI_ExecuteConfunc("trans_display_spinners 0");
		break;
	case TRANS_TYPE_ALIEN:
		if (B_GetBuildingStatus(destbase, B_ALIEN_CONTAINMENT)) {
			for (i = 0; i < ccs.numAliensTD; i++) {
				const aliensCont_t *alienCont = &srcbase->alienscont[i];
				if (alienCont->teamDef && alienCont->amountDead > 0) {
					Com_sprintf(str, sizeof(str), _("Corpse of %s"), _(alienCont->teamDef->name));
					LIST_AddString(&transferList, str);
					LIST_AddString(&transferListAmount, va("%i", alienCont->amountDead));
					if (td.trAliensTmp[i][TRANS_ALIEN_DEAD] > 0)
						LIST_AddString(&transferListTransfered, va("%i", td.trAliensTmp[i][TRANS_ALIEN_DEAD]));
					else
						LIST_AddString(&transferListTransfered, "");
					cnt++;
				}
				if (alienCont->teamDef && alienCont->amountAlive > 0) {
					Com_sprintf(str, sizeof(str), _("Alive %s"), _(alienCont->teamDef->name));
					LIST_AddString(&transferList, str);
					LIST_AddString(&transferListAmount, va("%i", alienCont->amountAlive));
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
		UI_ExecuteConfunc("trans_display_spinners 0");
		break;
	case TRANS_TYPE_AIRCRAFT:
		if (AIR_AircraftAllowed(destbase)) {
			aircraft_t *aircraft = NULL;

			while ((aircraft = AIR_GetNextFromBase(srcbase, aircraft))) {
				if (TR_AircraftListSelect(aircraft)) {
					Com_sprintf(str, sizeof(str), _("Aircraft %s"), aircraft->name);
					LIST_AddString(&transferList, str);
					LIST_AddString(&transferListAmount, "1");
					LIST_AddString(&transferListTransfered, "");
					cnt++;
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
		UI_ExecuteConfunc("trans_display_spinners 0");
		break;
	default:
		Com_Printf("TR_TransferSelect: Unknown transferType id %i\n", transferType);
		UI_ExecuteConfunc("trans_display_spinners 0");
		return;
	}

	/* Update cargo list. */
	TR_CargoList();

	td.currentTransferType = transferType;
	UI_RegisterLinkedListText(TEXT_TRANSFER_LIST, transferList);
	UI_RegisterLinkedListText(TEXT_TRANSFER_LIST_AMOUNT, transferListAmount);
	UI_RegisterLinkedListText(TEXT_TRANSFER_LIST_TRANSFERED, transferListTransfered);
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
		const objDef_t *od = INVSH_GetItemByIDX(i);
		const int itemCargoAmount = td.trItemsTmp[od->idx];
		if (itemCargoAmount > 0) {
			if (!strcmp(od->id, ANTIMATTER_TECH_ID))
				B_ManageAntimatter(base, itemCargoAmount, qtrue);
			else
				B_UpdateStorageAndCapacity(base, od, itemCargoAmount, qfalse, qfalse);
		}
	}
	for (i = 0; i < ccs.numAliensTD; i++) {	/* Return aliens. */
		const int alienCargoAmountAlive = td.trAliensTmp[i][TRANS_ALIEN_ALIVE];
		const int alienCargoAmountDead = td.trAliensTmp[i][TRANS_ALIEN_DEAD];
		if (alienCargoAmountAlive > 0)
			AL_ChangeAliveAlienNumber(base, &(base->alienscont[i]), alienCargoAmountAlive);
		if (alienCargoAmountDead > 0)
			base->alienscont[i].amountDead += alienCargoAmountDead;
	}

	/* Clear temporary cargo arrays. */
	memset(td.trItemsTmp, 0, sizeof(td.trItemsTmp));
	memset(td.trAliensTmp, 0, sizeof(td.trAliensTmp));
	memset(td.trEmployeesTmp, 0, sizeof(td.trEmployeesTmp));
	LIST_Delete(&td.aircraft);
	/* Update cargo list and items list. */
	TR_CargoList();
	TR_TransferSelect(base, td.transferBase, td.currentTransferType);
	UI_ExecuteConfunc("trans_resetscroll");
}

/**
 * @brief Set the number of item to transfer.
 */
static int TR_GetTransferFactor (void)
{
	return 1;
}

static qboolean TR_GetTransferEmployee (employeeType_t emplType, int *cnt, const base_t *base, int num)
{
	employee_t *employee = NULL;
	while ((employee = E_GetNextFromBase(emplType, employee, base))) {
		if (td.trEmployeesTmp[employee->type][employee->idx])
			continue;
		if (*cnt == num) {
			if (TR_CheckEmployee(employee, td.transferBase)) {
				td.trEmployeesTmp[employee->type][employee->idx] = employee;
				return qtrue;
			}
			/**
			 * @todo we have to decide what to do if the check fails - there
			 * are hard failures, and soft ones - soft ones should continue to add the next employee type
			 */
			return qfalse;
		}
		(*cnt)++;
	}
	return qfalse;
}

static void TR_AddItemToTransferList (base_t *base, transferData_t *td, int num)
{
	int cnt = 0, i;

	if (B_GetBuildingStatus(td->transferBase, B_ANTIMATTER)) {
		const objDef_t *od = INVSH_GetItemByID(ANTIMATTER_TECH_ID);
		const int itemCargoAmount = td->trItemsTmp[od->idx];
		if (itemCargoAmount || B_AntimatterInBase(base)) {
			if (cnt == num) {
				int amount;

				if (Cmd_Argc() == 3)
					amount = atoi(Cmd_Argv(2));
				else
					amount = TR_GetTransferFactor();

				/* you can't transfer more item than you have */
				if (amount > 0) {
					amount = min(amount, B_AntimatterInBase(base));
					if (amount == 0)
						return;
					/* you can only transfer items that destination base can accept */
					amount = TR_CheckItem(od, td->transferBase, amount);
				} else if (amount < 0) {
					amount = max(amount, -itemCargoAmount);
				}

				if (amount) {
					td->trItemsTmp[od->idx] += amount;
					B_ManageAntimatter(base, amount, qfalse);
				}
				return;
			}
			cnt++;
		}
	}
	for (i = 0; i < csi.numODs; i++) {
		const objDef_t *od = INVSH_GetItemByIDX(i);
		const int itemCargoAmount = td->trItemsTmp[i];
		if (!B_ItemIsStoredInBaseStorage(od))
			continue;
		if (od->isVirtual)
			continue;
		if (itemCargoAmount || B_ItemInBase(od, base) > 0) {
			if (cnt == num) {
				int amount;

				if (Cmd_Argc() == 3)
					amount = atoi(Cmd_Argv(2));
				else
					amount = TR_GetTransferFactor();

				/* you can't transfer more item than you have */
				if (amount > 0) {
					amount = min(amount, B_ItemInBase(od, base));
					if (amount == 0)
						return;
					/* you can only transfer items that destination base can accept */
					amount = TR_CheckItem(od, td->transferBase, amount);
				} else if (amount < 0) {
					amount = max(amount, -itemCargoAmount);
				}

				if (amount) {
					td->trItemsTmp[od->idx] += amount;
					B_UpdateStorageAndCapacity(base, od, -amount, qfalse, qfalse);
					break;
				} else
					return;
			}
			cnt++;
		}
	}
}

static void TR_AddEmployeeToTransferList (base_t *base, transferData_t *transferData, int num)
{
	int cnt = 0, i;
	employeeType_t emplType;
	int numEmployees[MAX_EMPL];

	if (TR_GetTransferEmployee(EMPL_SOLDIER, &cnt, base, num))
		return;

	if (TR_GetTransferEmployee(EMPL_PILOT, &cnt, base, num))
		return;

	/* Reset and fill temp employees arrays. */
	for (emplType = 0; emplType < MAX_EMPL; emplType++) {
		numEmployees[emplType] = E_CountHired(base, emplType);
		for (i = 0; i < MAX_EMPLOYEES; i++) {
			if (transferData->trEmployeesTmp[emplType][i])
				numEmployees[emplType]--;
		}
	}

	for (emplType = 0; emplType < MAX_EMPL; emplType++) {
		if (emplType == EMPL_SOLDIER || emplType == EMPL_PILOT)
			continue;
		/* no employee in base or all employees already in the transfer list */
		if (numEmployees[emplType] < 1)
			continue;
		if (cnt == num) {
			int amount = min(E_CountHired(base, emplType), TR_GetTransferFactor());
			employee_t *employee = NULL;
			while ((employee = E_GetNextFromBase(emplType, employee, base))) {
				if (transferData->trEmployeesTmp[emplType][employee->idx])	/* Already on transfer list. */
					continue;
				if (TR_CheckEmployee(employee, transferData->transferBase)) {
					transferData->trEmployeesTmp[emplType][employee->idx] = employee;
					amount--;
					if (amount == 0)
						break;
				} else
					return;
			}
		}
		cnt++;
	}
}

static void TR_AddAlienToTransferList (base_t *base, transferData_t *transferData, int num)
{
	const base_t *transferBase = transferData->transferBase;
	int cnt = 0, i;

	if (!B_GetBuildingStatus(transferBase, B_ALIEN_CONTAINMENT))
		return;
	for (i = 0; i < ccs.numAliensTD; i++) {
		aliensCont_t *aliensCont = &base->alienscont[i];
		if (!aliensCont->teamDef)
			continue;
		if (aliensCont->amountDead > 0) {
			if (cnt == num) {
				transferData->trAliensTmp[i][TRANS_ALIEN_DEAD]++;
				/* Remove the corpse from Alien Containment. */
				aliensCont->amountDead--;
				break;
			}
			cnt++;
		}
		if (aliensCont->amountAlive > 0) {
			if (cnt == num) {
				if (TR_CheckAlien(transferBase)) {
					transferData->trAliensTmp[i][TRANS_ALIEN_ALIVE]++;
					/* Remove an alien from Alien Containment. */
					AL_ChangeAliveAlienNumber(base, aliensCont, -1);
					break;
				} else
					return;
			}
			cnt++;
		}
	}
}

static void TR_AddAircraftToTransferList (base_t *base, transferData_t *transferData, int num)
{
	const base_t *transferBase = transferData->transferBase;
	int cnt = 0;

	if (AIR_AircraftAllowed(transferBase)) {
		aircraft_t *aircraft = NULL;

		while ((aircraft = AIR_GetNextFromBase(base, aircraft))) {
			if (TR_AircraftListSelect(aircraft)) {
				if (cnt == num) {
					if (TR_CheckAircraft(aircraft, transferBase)) {
						LIST_AddPointer(&transferData->aircraft, (void*)aircraft);
						break;
					} else
						return;
				}
				cnt++;
			}
		}
	}
}

static void TR_AddToTransferList (base_t *base, transferData_t *transfer, int num)
{
	switch (transfer->currentTransferType) {
	case TRANS_TYPE_ITEM:
		TR_AddItemToTransferList(base, transfer, num);
		break;
	case TRANS_TYPE_EMPLOYEE:
		TR_AddEmployeeToTransferList(base, transfer, num);
		break;
	case TRANS_TYPE_ALIEN:
		TR_AddAlienToTransferList(base, transfer, num);
		break;
	case TRANS_TYPE_AIRCRAFT:
		TR_AddAircraftToTransferList(base, transfer, num);
		break;
	}
}

/**
 * @brief Adds a thing to transfercargo by left mouseclick.
 * @sa TR_TransferSelect_f
 * @sa TR_TransferInit_f
 */
static void TR_TransferListSelect_f (void)
{
	int num;
	base_t *base = B_GetCurrentSelectedBase();

	if (Cmd_Argc() < 2)
		return;

	if (!base)
		return;

	if (!td.transferBase) {
		UI_Popup(_("No target base selected"), _("Please select the target base from the list"));
		return;
	}

	/* the index in the list that was clicked */
	num = atoi(Cmd_Argv(1));
	if (num < 0 || num >= csi.numODs)
		return;

	TR_AddToTransferList(base, &td, num);

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

	if (AIR_AircraftAllowed(destbase)) {
		Q_strcat(baseInfo, _("You can transfer aircraft - this base has a Hangar.\n"), sizeof(baseInfo));
	} else if (!B_GetBuildingStatus(destbase, B_COMMAND)) {
		Q_strcat(baseInfo, _("Aircraft transfer not possible - this base does not have a Command Centre.\n"), sizeof(baseInfo));
	} else if (powercomm) {
		Q_strcat(baseInfo, _("No Hangar in this base.\n"), sizeof(baseInfo));
	}

	if (!powercomm)
		Q_strcat(baseInfo, _("No power supplies in this base.\n"), sizeof(baseInfo));

	UI_RegisterText(TEXT_BASE_INFO, baseInfo);

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
	const base_t const *currentBase = B_GetCurrentSelectedBase();
	uiNode_t *baseList = NULL;
	base_t *base = NULL;

	while ((base = B_GetNextFounded(base)) != NULL) {
		if (base == currentBase)
			continue;

		UI_AddOption(&baseList, va("base%i", base->idx), base->name, va("%i", base->idx));
	}

	UI_RegisterOption(OPTION_BASELIST, baseList);
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

static void TR_RemoveItemFromCargoList (base_t *base, transferData_t *transferData, int num)
{
	int i, cnt;

	for (i = 0; i < csi.numODs; i++) {
		const objDef_t *od = INVSH_GetItemByIDX(i);
		const int itemCargoAmount = transferData->trItemsTmp[od->idx];
		if (itemCargoAmount > 0) {
			if (cnt == num) {
				const int amount = min(TR_GetTransferFactor(), itemCargoAmount);
				/* you can't transfer more item than there are in current transfer */
				transferData->trItemsTmp[i] -= amount;
				if (!strcmp(od->id, ANTIMATTER_TECH_ID))
					B_ManageAntimatter(base, amount, qfalse);
				else
					B_UpdateStorageAndCapacity(base, od, amount, qfalse, qfalse);
				break;
			}
			cnt++;
		}
	}
}

static void TR_RemoveEmployeeFromCargoList (base_t *base, transferData_t *transferData, int num)
{
	int cnt = 0, entries = 0, i, j;
	qboolean removed = qfalse;
	int numempl[MAX_EMPL];
	employeeType_t emplType;

	for (i = 0; i < MAX_CARGO; i++) {
		/* Count previous types on the list. */
		switch (transferData->cargo[i].type) {
		case CARGO_TYPE_ITEM:
			entries++;
		default:
			break;
		}
	}
	/* Start increasing cnt from the amount of previous entries. */
	cnt = entries;
	for (i = 0; i < ccs.numEmployees[EMPL_SOLDIER]; i++) {
		if (transferData->trEmployeesTmp[EMPL_SOLDIER][i]) {
			if (cnt == num) {
				transferData->trEmployeesTmp[EMPL_SOLDIER][i] = NULL;
				removed = qtrue;
				break;
			}
			cnt++;
		}
	}
	if (removed)	/* We already removed soldier, break here. */
		return;
	for (i = 0; i < ccs.numEmployees[EMPL_PILOT]; i++) {
		if (transferData->trEmployeesTmp[EMPL_PILOT][i]) {
			if (cnt == num) {
				transferData->trEmployeesTmp[EMPL_PILOT][i] = NULL;
				removed = qtrue;
				break;
			}
			cnt++;
		}
	}
	if (removed)	/* We already removed pilot, break here. */
		return;

	Com_DPrintf(DEBUG_CLIENT, "TR_CargoListSelect_f: cnt: %i, num: %i\n", cnt, num);

	/* Reset and fill temp employees arrays. */
	for (emplType = 0; emplType < MAX_EMPL; emplType++) {
		numempl[emplType] = 0;
		/** @todo not ccs.numEmployees? isn't this (the td.trEmployeesTmp array)
		 * updated when an employee gets deleted? */
		for (i = 0; i < MAX_EMPLOYEES; i++) {
			if (transferData->trEmployeesTmp[emplType][i])
				numempl[emplType]++;
		}
	}

	for (emplType = 0; emplType < MAX_EMPL; emplType++) {
		if (numempl[emplType] < 1 || emplType == EMPL_SOLDIER || emplType == EMPL_PILOT)
			continue;
		if (cnt == num) {
			int amount = min(TR_GetTransferFactor(), E_CountHired(base, emplType));
			for (j = 0; j < ccs.numEmployees[emplType]; j++) {
				if (transferData->trEmployeesTmp[emplType][j]) {
					transferData->trEmployeesTmp[emplType][j] = NULL;
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
}

static void TR_RemoveDeadAliensFromCargoList (base_t *base, transferData_t *transferData, int num)
{
	int i, cnt;
	int entries = 0;

	for (i = 0; i < MAX_CARGO; i++) {
		/* Count previous types on the list. */
		switch (transferData->cargo[i].type) {
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
		if (transferData->trAliensTmp[i][TRANS_ALIEN_DEAD] > 0) {
			if (cnt == num) {
				transferData->trAliensTmp[i][TRANS_ALIEN_DEAD]--;
				base->alienscont[i].amountDead++;
				break;
			}
			cnt++;
		}
	}
}

static void TR_RemoveAliveAliensFromCargoList (base_t *base, transferData_t *transferData, int num)
{
	int i, cnt;
	int entries = 0;

	for (i = 0; i < MAX_CARGO; i++) {
		/* Count previous types on the list. */
		switch (transferData->cargo[i].type) {
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
		if (transferData->trAliensTmp[i][TRANS_ALIEN_ALIVE] > 0) {
			if (cnt == num) {
				transferData->trAliensTmp[i][TRANS_ALIEN_ALIVE]--;
				AL_ChangeAliveAlienNumber(base, &(base->alienscont[i]), 1);
				break;
			}
			cnt++;
		}
	}
}

static void TR_RemoveAircraftFromCargoList (base_t *base, transferData_t *transferData, int num)
{
	int i, cnt;
	int entries = 0;
	aircraft_t *aircraft = NULL;

	for (i = 0; i < MAX_CARGO; i++) {
		/* Count previous types on the list. */
		switch (transferData->cargo[i].type) {
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
	while ((aircraft = (aircraft_t*)LIST_GetNext(transferData->aircraft, (void*)aircraft))) {
		if (cnt == num) {
			LIST_Remove(&transferData->aircraft, aircraft);
			break;
		}
		cnt++;
	}
}

static void TR_RemoveFromCargoList (base_t *base, transferData_t *transferData, int num)
{
	const transferCargo_t *cargo = &transferData->cargo[num];

	switch (cargo->type) {
	case CARGO_TYPE_ITEM:
		TR_RemoveItemFromCargoList(base, transferData, num);
		break;
	case CARGO_TYPE_EMPLOYEE:
		TR_RemoveEmployeeFromCargoList(base, transferData, num);
		break;
	case CARGO_TYPE_ALIEN_DEAD:
		TR_RemoveDeadAliensFromCargoList(base, transferData, num);
		break;
	case CARGO_TYPE_ALIEN_ALIVE:
		TR_RemoveAliveAliensFromCargoList(base, transferData, num);
		break;
	case CARGO_TYPE_AIRCRAFT:
		TR_RemoveAircraftFromCargoList(base, transferData, num);
		break;
	}
}

/**
 * @brief Removes items from cargolist by click.
 */
static void TR_CargoListSelect_f (void)
{
	int num;
	base_t *base = B_GetCurrentSelectedBase();

	if (Cmd_Argc() < 2)
		return;

	if (!base)
		return;

	num = atoi(Cmd_Argv(1));
	if (num < 0 || num >= MAX_CARGO)
		return;

	TR_RemoveFromCargoList(base, &td, num);

	TR_TransferSelect(base, td.transferBase, td.currentTransferType);
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
	LIST_Delete(&td.aircraft);

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
	UI_ExecuteConfunc("trans_resetscroll");
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
	LIST_Delete(&td.aircraft);
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
	const objDef_t *antimatterOD = INVSH_GetItemByID(ANTIMATTER_TECH_ID);
	const int antimatterCargo = td.trItemsTmp[antimatterOD->idx];

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

	if (B_GetBuildingStatus(td.transferBase, B_ANTIMATTER) && (antimatterCargo || B_AntimatterInBase(srcBase))) {
		if (cnt >= viewPos && cnt < viewPos + MAX_TRANSLIST_MENU_ENTRIES)
			UI_ExecuteConfunc("trans_updatespinners %i %i %i %i", cnt - viewPos,
					antimatterCargo, 0, B_AntimatterInBase(srcBase) + antimatterCargo);
		cnt++;
	}
	for (i = 0; i < csi.numODs; i++) {
		const objDef_t *od = INVSH_GetItemByIDX(i);
		const int itemCargoAmount = td.trItemsTmp[od->idx];
		if (!B_ItemIsStoredInBaseStorage(od))
			continue;
		if (itemCargoAmount || B_ItemInBase(od, srcBase) > 0) {
			if (cnt >= viewPos + MAX_TRANSLIST_MENU_ENTRIES)
				break;
			if (cnt >= viewPos)
				UI_ExecuteConfunc("trans_updatespinners %i %i %i %i", cnt - viewPos,
						itemCargoAmount, 0, B_ItemInBase(od, srcBase) + itemCargoAmount);
			cnt++;
		}
	}
}

void TR_InitCallbacks (void)
{
	memset(&td, 0, sizeof(td));

	Cmd_AddCommand("trans_init", TR_Init_f, "Init function for Transfer menu");
	Cmd_AddCommand("trans_list_scroll", TR_TransferList_Scroll_f, "Scrolls the transferlist");
	Cmd_AddCommand("trans_close", TR_TransferClose_f, "Callback for closing Transfer Menu");
	Cmd_AddCommand("trans_start", TR_TransferStart_f, "Starts the transfer");
	Cmd_AddCommand("trans_type", TR_TransferSelect_f, "Switch between transfer types (employees, techs, items)");
	Cmd_AddCommand("trans_emptyairstorage", TR_TransferListClear_f, "Unload everything from transfer cargo back to base");
	Cmd_AddCommand("trans_list_click", TR_TransferListSelect_f, "Callback for transfer list node click");
	Cmd_AddCommand("trans_cargolist_click", TR_CargoListSelect_f, "Callback for cargo list node click");
	Cmd_AddCommand("trans_selectbase", TR_SelectBase_f, "Callback for selecting a base");
	Cmd_AddCommand("trans_baselist_click", TR_TransferBaseListClick_f, "Callback for choosing base while recovering alien after mission");
	Cmd_AddCommand("trans_aliens", TR_TransferAliensFromMission_f, "Transfer aliens collected at missions");
}

void TR_ShutdownCallbacks (void)
{
	Cmd_RemoveCommand("trans_init");
	Cmd_RemoveCommand("trans_list_scroll");
	Cmd_RemoveCommand("trans_close");
	Cmd_RemoveCommand("trans_start");
	Cmd_RemoveCommand("trans_type");
	Cmd_RemoveCommand("trans_emptyairstorage");
	Cmd_RemoveCommand("trans_list_click");
	Cmd_RemoveCommand("trans_cargolist_click");
	Cmd_RemoveCommand("trans_selectbase");
	Cmd_RemoveCommand("trans_baselist_click");
	Cmd_RemoveCommand("trans_aliens");
}
