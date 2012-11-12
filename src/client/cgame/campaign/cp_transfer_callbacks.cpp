/**
 * @file
 */

/*
Copyright (C) 2002-2012 UFO: Alien Invasion.

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

#include "cp_transfer_callbacks.h"
#include "../../cl_shared.h"
#include "cp_campaign.h"
#include "cp_capacity.h"
#include "cp_transfer.h"
#include "cp_popup.h"
#include "cp_time.h"
#include "../../ui/ui_dataids.h"

/**
 * @brief transfer typeID strings
 */
static char const* const transferTypeIDs[] = {
	"item",
	"employee",
	"alien",
	"aircraft"
};
CASSERT(lengthof(transferTypeIDs) == TRANS_TYPE_MAX);

/** @todo move this into ccs_t */
static transferData_t td;

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

	if (cgi->Cmd_Argc() < 2) {
		Com_Printf("Usage: %s <base index>\n", cgi->Cmd_Argv(0));
		return;
	}

	num = atoi(cgi->Cmd_Argv(1));
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

	if (cgi->Cmd_Argc() < 2) {
		Com_Printf("Usage: %s <aircraft index>\n", cgi->Cmd_Argv(0));
		return;
	}

	i = atoi(cgi->Cmd_Argv(1));
	aircraft = AIR_AircraftGetFromIDX(i);
	if (!aircraft) {
		Com_Printf("Usage: No aircraft with index %i\n", i);
		return;
	}

	stunnedAliens = TR_CountStunnedAliensInCargo(aircraft);

	/* store the aircraft to be able to remove alien bodies */
	td.transferStartAircraft = aircraft;

	base = NULL;
	while ((base = B_GetNext(base)) != NULL) {
		const char* string;
		uiNode_t *option;
		int freeSpace;

		if (!AC_ContainmentAllowed(base))
			continue;

		freeSpace = CAP_GetFreeCapacity(base, CAP_ALIENS);

		string = va(ngettext("(can host %i live alien)", "(can host %i live aliens)", freeSpace), freeSpace);
		string = va("%s %s", base->name, string);
		option = cgi->UI_AddOption(&baseList, va("base%i", base->idx), string, va("%i", base->idx));

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
		cgi->UI_RegisterOption(OPTION_BASELIST, baseList);
		cgi->UI_PushWindow("popup_transferbaselist");
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
	int i;

	if (td.currentTransferType == TRANS_TYPE_INVALID) {
		Com_Printf("TR_TransferStart_f: currentTransferType is wrong!\n");
		return;
	}

	if (TR_TransferStart(base, td) == NULL)
		return;

	/* Clear temporary cargo arrays. */
	OBJZERO(td.trItemsTmp);
	OBJZERO(td.trAliensTmp);

	for (i = EMPL_SOLDIER; i < MAX_EMPL; i++)
		LIST_Delete(&td.trEmployeesTmp[i]);

	LIST_Delete(&td.aircraft);

	Com_sprintf(message, sizeof(message), _("Transport mission started, cargo is being transported to %s"), td.transferBase->name);
	MSO_CheckAddNewMessage(NT_TRANSFER_STARTED, _("Transport mission"), message, MSG_TRANSFERFINISHED);
	cgi->UI_PopWindow(false);
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
		if (Q_streq(transferTypeIDs[i], id))
			return (transferType_t)i;
	}
	return TRANS_TYPE_INVALID;
}

/**
 * @brief Checks condition for employee transfer.
 * @param[in] employee Pointer to employee for transfer.
 * @param[in] destbase Pointer to destination base.
 * @return true if transfer of this type of employee is possible.
 */
static bool TR_CheckEmployee (const employee_t *employee, const base_t *destbase)
{
	assert(employee && destbase);

	switch (employee->type) {
	case EMPL_SOLDIER:
		/* Is this a soldier assigned to aircraft? */
		if (AIR_IsEmployeeInAircraft(employee, NULL)) {
			const rank_t *rank = CL_GetRankByIdx(employee->chr.score.rank);
			CP_Popup(_("Soldier in aircraft"), _("%s %s is assigned to aircraft and cannot be\ntransfered to another base.\n"),
					_(rank->shortname), employee->chr.name);
			return false;
		}
		break;
	case EMPL_PILOT:
		/* Is this a pilot assigned to aircraft? */
		if (AIR_IsEmployeeInAircraft(employee, NULL)) {
			CP_Popup(_("Pilot in aircraft"), _("%s is assigned to aircraft and cannot be\ntransfered to another base.\n"),
					employee->chr.name);
			return false;
		}
		break;
	default:
		break;
	}

	return true;
}

/**
 * @brief Display cargo list.
 */
static void TR_CargoList (void)
{
	int i = 0;
	int emplType;
	linkedList_t *cargoList = NULL;
	linkedList_t *cargoListAmount = NULL;
	char str[128];

	td.trCargoCountTmp = 0;
	OBJZERO(td.cargo);

	/* Show items. */
	for (i = 0; i < cgi->csi->numODs; i++) {
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
		switch (emplType) {
		case EMPL_SOLDIER: {
			LIST_Foreach(td.trEmployeesTmp[emplType], employee_t, employee) {
				const rank_t *rank = CL_GetRankByIdx(employee->chr.score.rank);

				assert(rank);
				Com_sprintf(str, lengthof(str), _("Soldier %s %s"), _(rank->shortname), employee->chr.name);

				LIST_AddString(&cargoList, str);
				LIST_AddString(&cargoListAmount, "1");
				TR_AddData(&td, CARGO_TYPE_EMPLOYEE, employee);
			}
			break;
		}
		case EMPL_PILOT: {
			LIST_Foreach(td.trEmployeesTmp[emplType], employee_t, employee) {
				Com_sprintf(str, lengthof(str), _("Pilot %s"), employee->chr.name);
				LIST_AddString(&cargoList, str);
				LIST_AddString(&cargoListAmount, va("%i", 1));

				TR_AddData(&td, CARGO_TYPE_EMPLOYEE, employee);
			}
			break;
		}
		case EMPL_ROBOT:
			/** @todo implement UGV transfers */
			break;
		case EMPL_SCIENTIST:
		case EMPL_WORKER: {
			int emplCount = LIST_Count(td.trEmployeesTmp[emplType]);

			if (!emplCount)
				break;

			LIST_AddString(&cargoList, E_GetEmployeeString((employeeType_t)emplType, emplCount));
			LIST_AddString(&cargoListAmount, va("%i", emplCount));
			td.cargo[td.trCargoCountTmp].type = CARGO_TYPE_EMPLOYEE;
			td.trCargoCountTmp++;
			break;
		}
		default:
			cgi->Com_Error(ERR_DROP, "TR_CargoList: Invalid employeetype in cargo");
		}
		if (td.trCargoCountTmp >= MAX_CARGO) {
			Com_DPrintf(DEBUG_CLIENT, "TR_CargoList: Cargo is full\n");
			break;
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
	LIST_Foreach(td.aircraft, aircraft_t, aircraft) {
		Com_sprintf(str, lengthof(str), _("Aircraft %s"), aircraft->name);
		LIST_AddString(&cargoList, str);
		LIST_AddString(&cargoListAmount, "1");
		TR_AddData(&td, CARGO_TYPE_AIRCRAFT, aircraft);
	}

	cgi->UI_RegisterLinkedListText(TEXT_CARGO_LIST, cargoList);
	cgi->UI_RegisterLinkedListText(TEXT_CARGO_LIST_AMOUNT, cargoListAmount);
}

/**
 * @brief Check if an aircraft should be displayed for transfer.
 * @param[in] aircraft Pointer to an aircraft
 * @return true if the aircraft should be displayed, false else.
 */
static bool TR_AircraftListSelect (const aircraft_t *aircraft)
{
	if (!AIR_IsAircraftInBase(aircraft))	/* Aircraft is not in base. */
		return false;
	if (LIST_GetPointer(td.aircraft, aircraft))	/* Already on transfer list. */
		return false;

	return true;
}

static void TR_AddListEntry (linkedList_t **names, const char *name, linkedList_t **amounts, int amount, linkedList_t **transfers, int transfered)
{
	if (transfered > 0)
		LIST_AddString(transfers, va("%i", transfered));
	else
		LIST_AddString(transfers, "");

	if (amount > 0)
		LIST_AddString(amounts, va("%i", amount));
	else
		LIST_AddString(amounts, "");

	LIST_AddString(names, name);
}

static int TR_FillItems (const base_t *srcbase, const base_t *destbase, linkedList_t **names, linkedList_t **amounts, linkedList_t **transfers)
{
	int cnt = 0;
	const objDef_t *od;
	int itemCargoAmount;

	od = INVSH_GetItemByID(ANTIMATTER_TECH_ID);
	if (od) {
		itemCargoAmount = td.trItemsTmp[od->idx];
		const int antiMatterInBase = B_AntimatterInBase(srcbase);
		if (itemCargoAmount || antiMatterInBase) {
			TR_AddListEntry(names, _(od->name), amounts, antiMatterInBase, transfers, itemCargoAmount);
			cnt++;
		}
	}
	for (int i = 0; i < cgi->csi->numODs; i++) {
		od = INVSH_GetItemByIDX(i);
		assert(od);
		itemCargoAmount = td.trItemsTmp[od->idx];
		if (!B_ItemIsStoredInBaseStorage(od))
			continue;
		const int itemInBase = B_ItemInBase(od, srcbase);
		if (itemCargoAmount || itemInBase > 0) {
			TR_AddListEntry(names, _(od->name), amounts, itemInBase, transfers, itemCargoAmount);
			cnt++;
		}
	}

	return cnt;
}

static int TR_FillEmployees (const base_t *srcbase, const base_t *destbase, linkedList_t **names, linkedList_t **amounts, linkedList_t **transfers)
{
	int cnt = 0;

	for (int i = EMPL_SOLDIER; i < MAX_EMPL; i++) {
		const employeeType_t emplType = (employeeType_t)i;
		switch (emplType) {
		case EMPL_SOLDIER:
		case EMPL_PILOT: {
			E_Foreach(emplType, employee) {
				char str[128];

				if (!E_IsInBase(employee, srcbase))
					continue;

				/* Skip if already on transfer list. */
				if (LIST_GetPointer(td.trEmployeesTmp[emplType], (void*) employee))
					continue;

				if (emplType == EMPL_SOLDIER) {
					const rank_t *rank = CL_GetRankByIdx(employee->chr.score.rank);
					Com_sprintf(str, sizeof(str), "%s %s %s", E_GetEmployeeString(emplType, 1), rank->shortname, employee->chr.name);
				} else {
					Com_sprintf(str, sizeof(str), "%s %s", E_GetEmployeeString(emplType, 1), employee->chr.name);
				}

				TR_AddListEntry(names, str, amounts, 1, transfers, -1);
				cnt++;
			}
			break;
		}
		case EMPL_ROBOT:
			/** @todo implement UGV transfers */
			break;
		case EMPL_SCIENTIST:
		case EMPL_WORKER: {
			const int hired = E_CountHired(srcbase, emplType);
			const int trCount = LIST_Count(td.trEmployeesTmp[emplType]);

			if (hired <= 0)
				break;

			TR_AddListEntry(names, E_GetEmployeeString(emplType, hired), amounts, hired, transfers, trCount);
			cnt++;
			break;
		}
		default:
			cgi->Com_Error(ERR_DROP, "TR_CargoList: Invalid employeetype in cargo");
		}
	}

	return cnt;
}

static int TR_FillAliens (const base_t *srcbase, const base_t *destbase, linkedList_t **names, linkedList_t **amounts, linkedList_t **transfers)
{
	int cnt = 0;
	int i;

	for (i = 0; i < ccs.numAliensTD; i++) {
		const aliensCont_t *alienCont = &srcbase->alienscont[i];
		const teamDef_t *teamDef = alienCont->teamDef;
		if (teamDef == NULL)
			continue;
		if (alienCont->amountDead > 0) {
			char str[128];
			const int transferDead = td.trAliensTmp[i][TRANS_ALIEN_DEAD];
			Com_sprintf(str, sizeof(str), _("Corpse of %s"), _(teamDef->name));
			TR_AddListEntry(names, str, amounts, alienCont->amountDead, transfers, transferDead);
			cnt++;
		}
		if (alienCont->amountAlive > 0) {
			char str[128];
			const int transferAlive = td.trAliensTmp[i][TRANS_ALIEN_ALIVE];
			Com_sprintf(str, sizeof(str), _("Alive %s"), _(teamDef->name));
			TR_AddListEntry(names, str, amounts, alienCont->amountAlive, transfers, transferAlive);
			cnt++;
		}
	}

	return cnt;
}

static int TR_FillAircraft (const base_t *srcbase, const base_t *destbase, linkedList_t **names, linkedList_t **amounts, linkedList_t **transfers)
{
	int cnt = 0;

	AIR_ForeachFromBase(aircraft, srcbase) {
		if (TR_AircraftListSelect(aircraft)) {
			char str[128];
			Com_sprintf(str, sizeof(str), _("Aircraft %s"), aircraft->name);
			TR_AddListEntry(names, str, amounts, 1, transfers, -1);
			cnt++;
		}
	}

	return cnt;
}

/**
 * @brief Fills the items-in-base list with stuff available for transfer.
 * @note Filling the transfer list with proper stuff (items/employees/aliens/aircraft) is being done here.
 * @param[in] srcbase Pointer to the base the transfer starts from
 * @param[in] destbase Pointer to the base to transfer
 * @param[in] transferType Transfer category
 * @sa transferType_t
 */
static void TR_TransferSelect (const base_t *srcbase, const base_t *destbase, transferType_t transferType)
{
	linkedList_t *names = NULL;
	linkedList_t *amounts = NULL;
	linkedList_t *transfers = NULL;

	if (srcbase == NULL || destbase == NULL)
		return;

	/* reset for every new call */
	cgi->UI_ResetData(TEXT_TRANSFER_LIST);
	cgi->UI_ResetData(TEXT_TRANSFER_LIST_AMOUNT);
	cgi->UI_ResetData(TEXT_TRANSFER_LIST_TRANSFERED);

	switch (transferType) {
	case TRANS_TYPE_ITEM: {
		const int cnt = TR_FillItems(srcbase, destbase, &names, &amounts, &transfers);
		cgi->UI_ExecuteConfunc("trans_display_spinners %i", cnt);
		break;
	}
	case TRANS_TYPE_EMPLOYEE:
		TR_FillEmployees(srcbase, destbase, &names, &amounts, &transfers);
		cgi->UI_ExecuteConfunc("trans_display_spinners 0");
		break;
	case TRANS_TYPE_ALIEN:
		TR_FillAliens(srcbase, destbase, &names, &amounts, &transfers);
		cgi->UI_ExecuteConfunc("trans_display_spinners 0");
		break;
	case TRANS_TYPE_AIRCRAFT:
		TR_FillAircraft(srcbase, destbase, &names, &amounts, &transfers);
		cgi->UI_ExecuteConfunc("trans_display_spinners 0");
		break;
	default:
		cgi->Com_Error(ERR_DROP, "invalid transfertype given: %i", transferType);
	}

	/* Update cargo list. */
	TR_CargoList();

	td.currentTransferType = transferType;
	cgi->UI_RegisterLinkedListText(TEXT_TRANSFER_LIST, names);
	cgi->UI_RegisterLinkedListText(TEXT_TRANSFER_LIST_AMOUNT, amounts);
	cgi->UI_RegisterLinkedListText(TEXT_TRANSFER_LIST_TRANSFERED, transfers);
}

/**
 * @brief Function displays the transferable item/employee/aircraft/alien list
 * @sa TR_TransferStart_f
 * @sa TR_TransferInit_f
 */
static void TR_TransferSelect_f (void)
{
	transferType_t type;
	const base_t *base = B_GetCurrentSelectedBase();

	if (!td.transferBase || !base)
		return;

	if (cgi->Cmd_Argc() < 2)
		type = td.currentTransferType;
	else
		type = TR_GetTransferType(cgi->Cmd_Argv(1));

	if (type == TRANS_TYPE_INVALID)
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

	for (i = 0; i < cgi->csi->numODs; i++) {	/* Return items. */
		const objDef_t *od = INVSH_GetItemByIDX(i);
		const int itemCargoAmount = td.trItemsTmp[od->idx];
		if (itemCargoAmount > 0) {
			if (Q_streq(od->id, ANTIMATTER_TECH_ID))
				B_ManageAntimatter(base, itemCargoAmount, true);
			else
				B_UpdateStorageAndCapacity(base, od, itemCargoAmount, false);
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
	OBJZERO(td.trItemsTmp);
	OBJZERO(td.trAliensTmp);
	for (i = EMPL_SOLDIER; i < MAX_EMPL; i++)
		LIST_Delete(&td.trEmployeesTmp[i]);
	LIST_Delete(&td.aircraft);
	/* Update cargo list and items list. */
	TR_CargoList();
	TR_TransferSelect(base, td.transferBase, td.currentTransferType);
	cgi->UI_ExecuteConfunc("trans_resetscroll");

	/* Update capacity list of destination base */
	if (td.transferBase)
		cgi->Cmd_ExecuteString(va("ui_trans_caplist %d", td.transferBase->idx));
}

/**
 * @brief Set the number of item to transfer.
 */
static int TR_GetTransferFactor (void)
{
	return 1;
}

static bool TR_GetTransferEmployee (employeeType_t emplType, int *cnt, const base_t *base, int num)
{
	E_Foreach(emplType, employee) {
		if (!E_IsInBase(employee, base))
			continue;

		if (LIST_GetPointer(td.trEmployeesTmp[employee->type], (void*) employee))
			continue;

		if (*cnt == num) {
			if (TR_CheckEmployee(employee, td.transferBase)) {
				LIST_AddPointer(&td.trEmployeesTmp[employee->type], (void*) employee);
				return true;
			}
			/**
			 * @todo we have to decide what to do if the check fails - there
			 * are hard failures, and soft ones - soft ones should continue to add the next employee type
			 */
			return false;
		}
		(*cnt)++;
	}
	(*cnt)--;
	return false;
}

static void TR_AddItemToTransferList (base_t *base, transferData_t *td, int num, int amount)
{
	int cnt = 0;
	const objDef_t *od;

	od = INVSH_GetItemByID(ANTIMATTER_TECH_ID);
	if (od) {
		const int itemCargoAmount = td->trItemsTmp[od->idx];
		if (itemCargoAmount || B_AntimatterInBase(base)) {
			if (cnt == num) {
				/* you can't transfer more item than you have */
				if (amount > 0) {
					amount = std::min(amount, B_AntimatterInBase(base));
					if (amount == 0)
						return;
				} else if (amount < 0) {
					amount = std::max(amount, -itemCargoAmount);
				}

				if (amount) {
					td->trItemsTmp[od->idx] += amount;
					B_ManageAntimatter(base, amount, false);
				}
				return;
			}
			cnt++;
		}
	}
	for (int i = 0; i < cgi->csi->numODs; i++) {
		const objDef_t *od = INVSH_GetItemByIDX(i);
		const int itemCargoAmount = td->trItemsTmp[i];
		assert(od);
		if (!B_ItemIsStoredInBaseStorage(od))
			continue;
		if (itemCargoAmount || B_ItemInBase(od, base) > 0) {
			if (cnt == num) {
				/* you can't transfer more item than you have */
				if (amount > 0) {
					amount = std::min(amount, B_ItemInBase(od, base));
					if (amount == 0)
						return;
				} else if (amount < 0) {
					amount = std::max(amount, -itemCargoAmount);
				}

				if (amount) {
					td->trItemsTmp[od->idx] += amount;
					B_UpdateStorageAndCapacity(base, od, -amount, false);
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
	int cnt = 0;
	int i;

	for (i = EMPL_SOLDIER; i < MAX_EMPL; i++) {
		const employeeType_t emplType = (employeeType_t)i;
		switch (emplType) {
		case EMPL_SOLDIER:
		case EMPL_PILOT:
			TR_GetTransferEmployee(emplType, &cnt, base, num);
			cnt++;
			break;
		case EMPL_ROBOT:
			/** @todo implement UGV transfers */
			break;
		case EMPL_SCIENTIST:
		case EMPL_WORKER:
			/* no employee in base or all employees already in the transfer list */
			if (E_CountHired(base, emplType) <= 0)
				break;

			if (cnt == num) {
				E_Foreach(emplType, employee) {
					if (!E_IsInBase(employee, base))
						continue;

					if (LIST_GetPointer(td.trEmployeesTmp[emplType], (void*) employee))	/* Already on transfer list. */
						continue;

					if (TR_CheckEmployee(employee, td.transferBase)) {
						LIST_AddPointer(&td.trEmployeesTmp[emplType], (void*) employee);
						break;
					}

					return;
				}
			}
			cnt++;
			break;
		default:
			cgi->Com_Error(ERR_DROP, "TR_CargoList: Invalid employeetype in cargo");
		}
	}
}

static void TR_AddAlienToTransferList (base_t *base, transferData_t *transferData, int num)
{
	int cnt = 0, i;

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
				transferData->trAliensTmp[i][TRANS_ALIEN_ALIVE]++;
				/* Remove an alien from Alien Containment. */
				AL_ChangeAliveAlienNumber(base, aliensCont, -1);
			}
			cnt++;
		}
	}
}

static void TR_AddAircraftToTransferList (base_t *base, transferData_t *transferData, int num)
{
	int cnt = 0;

	AIR_ForeachFromBase(aircraft, base) {
		if (TR_AircraftListSelect(aircraft)) {
			if (cnt == num) {
				LIST_AddPointer(&transferData->aircraft, (void*)aircraft);
				break;
			}
			cnt++;
		}
	}
}

static void TR_AddToTransferList (base_t *base, transferData_t *transfer, int num, int amount)
{
	switch (transfer->currentTransferType) {
	case TRANS_TYPE_ITEM:
		TR_AddItemToTransferList(base, transfer, num, amount);
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
	default:
		break;
	}

	/* Update capacity list of destination base */
	if (td.transferBase)
		cgi->Cmd_ExecuteString(va("ui_trans_caplist %d", td.transferBase->idx));
}

/**
 * @brief Adds a thing to transfercargo by left mouseclick.
 * @sa TR_TransferSelect_f
 * @sa TR_TransferInit_f
 */
static void TR_TransferListSelect_f (void)
{
	int num;
	int amount;
	base_t *base = B_GetCurrentSelectedBase();

	if (cgi->Cmd_Argc() < 2)
		return;

	if (!base)
		return;

	if (!td.transferBase) {
		CP_Popup(_("No target base selected"), _("Please select the target base from the list"));
		return;
	}

	/* the index in the list that was clicked */
	num = atoi(cgi->Cmd_Argv(1));
	if (num < 0 || num >= cgi->csi->numODs)
		return;

	if (cgi->Cmd_Argc() == 3)
		amount = atoi(cgi->Cmd_Argv(2));
	else
		amount = TR_GetTransferFactor();

	TR_AddToTransferList(base, &td, num, amount);

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
	if (!destbase || !srcbase)
		return;

	/* Set global pointer to current selected base. */
	td.transferBase = destbase;
	cgi->Cvar_Set("mn_trans_base_name", destbase->name);
	cgi->Cvar_SetValue("mn_trans_base_id", destbase->idx);

	/* Update stuff-in-base list. */
	TR_TransferSelect(srcbase, destbase, td.currentTransferType);

	/* Update capacity list of destination base */
	if (td.transferBase)
		cgi->Cmd_ExecuteString(va("ui_trans_caplist %d", td.transferBase->idx));
}

/**
 * @brief Fills the optionlist with available bases to transfer to
 */
static void TR_InitBaseList (void)
{
	const base_t *currentBase = B_GetCurrentSelectedBase();
	uiNode_t *baseList = NULL;
	base_t *base = NULL;

	while ((base = B_GetNext(base)) != NULL) {
		if (base == currentBase)
			continue;

		cgi->UI_AddOption(&baseList, va("base%i", base->idx), base->name, va("%i", base->idx));
	}

	cgi->UI_RegisterOption(OPTION_BASELIST, baseList);
}

/**
 * @brief Callback to select destination base
 */
static void TR_SelectBase_f (void)
{
	int baseIdx;
	base_t *base = B_GetCurrentSelectedBase();
	base_t *destbase;

	if (cgi->Cmd_Argc() < 2) {
		Com_Printf("Usage: %s <baseIdx>\n", cgi->Cmd_Argv(0));
		return;
	}

	baseIdx = atoi(cgi->Cmd_Argv(1));
	destbase = B_GetFoundedBaseByIDX(baseIdx);

	TR_TransferBaseSelect(base, destbase);
}

static void TR_RemoveItemFromCargoList (base_t *base, transferData_t *transferData, int num)
{
	int i, cnt = 0;

	for (i = 0; i < cgi->csi->numODs; i++) {
		const objDef_t *od = INVSH_GetItemByIDX(i);
		const int itemCargoAmount = transferData->trItemsTmp[od->idx];
		if (itemCargoAmount > 0) {
			if (cnt == num) {
				const int amount = std::min(TR_GetTransferFactor(), itemCargoAmount);
				/* you can't transfer more item than there are in current transfer */
				transferData->trItemsTmp[i] -= amount;
				if (Q_streq(od->id, ANTIMATTER_TECH_ID))
					B_ManageAntimatter(base, amount, false);
				else
					B_UpdateStorageAndCapacity(base, od, amount, false);
				break;
			}
			cnt++;
		}
	}
}

static void TR_RemoveEmployeeFromCargoList (base_t *base, transferData_t *transferData, int num)
{
	int cnt = 0, entries = 0, i;
	bool removed = false;

	for (i = 0; i < MAX_CARGO; i++) {
		/* Count previous types on the list. */
		switch (transferData->cargo[i].type) {
		case CARGO_TYPE_ITEM:
			entries++;
			break;
		default:
			break;
		}
	}
	/* Start increasing cnt from the amount of previous entries. */
	cnt = entries;
	Com_DPrintf(DEBUG_CLIENT, "TR_CargoListSelect_f: cnt: %i, num: %i\n", cnt, num);

	for (i = EMPL_SOLDIER; i < MAX_EMPL; i++) {
		const employeeType_t emplType = (employeeType_t)i;
		switch (emplType) {
		case EMPL_SOLDIER:
		case EMPL_PILOT: {
			employee_t *employee = (employee_t*)LIST_GetByIdx(td.trEmployeesTmp[emplType], num - cnt);

			if (employee) {
				removed = LIST_Remove(&td.trEmployeesTmp[emplType], (void*) employee);
				break;
			}
			cnt += LIST_Count(td.trEmployeesTmp[emplType]);

			break;
		}
		case EMPL_ROBOT:
			/** @todo implement UGV transfers */
			break;
		case EMPL_SCIENTIST:
		case EMPL_WORKER:
			if (!LIST_IsEmpty(td.trEmployeesTmp[emplType])) {
				if (cnt == num) {
					LIST_RemoveEntry(&td.trEmployeesTmp[emplType], td.trEmployeesTmp[emplType]);
					removed = true;
				}
				cnt++;
			}
			break;
		default:
			cgi->Com_Error(ERR_DROP, "TR_CargoList: Invalid employeetype in cargo");
		}
		if (removed)
			break;
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
	LIST_Foreach(transferData->aircraft, aircraft_t, aircraft) {
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
	case CARGO_TYPE_INVALID:
		break;
	}

	/* Update capacity list of destination base */
	if (td.transferBase)
		cgi->Cmd_ExecuteString(va("ui_trans_caplist %d", td.transferBase->idx));
}

/**
 * @brief Removes items from cargolist by click.
 */
static void TR_CargoListSelect_f (void)
{
	int num;
	base_t *base = B_GetCurrentSelectedBase();

	if (cgi->Cmd_Argc() < 2)
		return;

	if (!base)
		return;

	num = atoi(cgi->Cmd_Argv(1));
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
	int i;

	/* Clear employees temp lists. */
	for (i = EMPL_SOLDIER; i < MAX_EMPL; i++) {
		const employeeType_t emplType = (employeeType_t)i;
		LIST_Delete(&td.trEmployeesTmp[emplType]);
	}
	/* Clear aircraft temp list. */
	LIST_Delete(&td.aircraft);

	/* Update destination base list */
	TR_InitBaseList();

	/* Select first available base. */
	td.transferBase = B_GetNext(base);
	/* If this was the last base select the first */
	if (!td.transferBase)
		td.transferBase = B_GetNext(NULL);
	if (!td.transferBase)
		cgi->Com_Error(ERR_DROP, "No bases! Transfer needs at least two...");
	TR_TransferBaseSelect(base, td.transferBase);
	/* Set up cvar used to display transferBase. */
	if (td.transferBase) {
		cgi->Cvar_Set("mn_trans_base_name", td.transferBase->name);
		cgi->Cvar_SetValue("mn_trans_base_id", td.transferBase->idx);
	} else {
		cgi->Cvar_Set("mn_trans_base_id", "");
	}

	/* Set up cvar used with tabset */
	cgi->Cvar_Set("mn_itemtype", transferTypeIDs[0]);
	/* Select first available item */
	cgi->Cmd_ExecuteString(va("trans_type %s", transferTypeIDs[0]));

	/* Reset scrolling for item-in-base list */
	cgi->UI_ExecuteConfunc("trans_resetscroll");
}

/**
 * @brief Closes Transfer Menu and resets temp arrays.
 */
static void TR_TransferClose_f (void)
{
	base_t *base = B_GetCurrentSelectedBase();
	int i;

	if (!base)
		return;

	/* Unload what was loaded. */
	TR_TransferListClear_f();

	/* Clear temporary cargo arrays. */
	OBJZERO(td.trItemsTmp);
	OBJZERO(td.trAliensTmp);
	for (i = EMPL_SOLDIER; i < MAX_EMPL; i++) {
		const employeeType_t emplType = (employeeType_t)i;
		LIST_Delete(&td.trEmployeesTmp[emplType]);
	}
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

	if (cgi->Cmd_Argc() < 3) {
		Com_Printf("Usage: %s <transferType> <viewPos>\n", cgi->Cmd_Argv(0));
		return;
	}

	transferType = TR_GetTransferType(cgi->Cmd_Argv(1));
	viewPos = atoi(cgi->Cmd_Argv(2));

	/* spinners are only on items screen */
	if (transferType != TRANS_TYPE_ITEM)
		return;

	if (B_GetBuildingStatus(td.transferBase, B_ANTIMATTER) && (antimatterCargo || B_AntimatterInBase(srcBase))) {
		if (cnt >= viewPos && cnt < viewPos + MAX_TRANSLIST_MENU_ENTRIES)
			cgi->UI_ExecuteConfunc("trans_updatespinners %i %i %i %i", cnt - viewPos,
					antimatterCargo, 0, B_AntimatterInBase(srcBase) + antimatterCargo);
		cnt++;
	}
	for (i = 0; i < cgi->csi->numODs; i++) {
		const objDef_t *od = INVSH_GetItemByIDX(i);
		const int itemCargoAmount = td.trItemsTmp[od->idx];
		if (!B_ItemIsStoredInBaseStorage(od))
			continue;
		if (itemCargoAmount || B_ItemInBase(od, srcBase) > 0) {
			if (cnt >= viewPos + MAX_TRANSLIST_MENU_ENTRIES)
				break;
			if (cnt >= viewPos)
				cgi->UI_ExecuteConfunc("trans_updatespinners %i %i %i %i", cnt - viewPos,
						itemCargoAmount, 0, B_ItemInBase(od, srcBase) + itemCargoAmount);
			cnt++;
		}
	}
}

/**
 * @brief Assembles the list of transfers for the popup
 */
static void TR_List_f (void)
{
	int i = 0;

	cgi->UI_ExecuteConfunc("tr_listclear");
	TR_Foreach(transfer) {
		const char *source = transfer->srcBase ? transfer->srcBase->name : "mission";
		date_t time = Date_Substract(transfer->event, ccs.date);

		cgi->UI_ExecuteConfunc("tr_listaddtransfer %d \"%s\" \"%s\" \"%s\"", ++i, source, transfer->destBase->name, CP_SecondConvert(Date_DateToSeconds(&time)));

		/* Items */
		if (transfer->hasItems) {
			int j;

			cgi->UI_ExecuteConfunc("tr_listaddcargo %d \"%s\" \"%s\" \"%s\"", i, "tr_cargo", "items", _("Items"));
			for (j = 0; j < cgi->csi->numODs; j++) {
				const objDef_t *od = INVSH_GetItemByIDX(j);

				if (transfer->itemAmount[od->idx] <= 0)
					continue;

				cgi->UI_ExecuteConfunc("tr_listaddcargo %d \"%s\" \"%s\" \"%s\"", i, "tr_cargo.items" ,od->id, va("%i %s", transfer->itemAmount[od->idx], _(od->name)));
			}
		}
		/* Employee */
		if (transfer->hasEmployees) {
			int j;
			cgi->UI_ExecuteConfunc("tr_listaddcargo %d \"%s\" \"%s\" \"%s\"", i, "tr_cargo", "employee", _("Employee"));
			for (j = EMPL_SOLDIER; j < MAX_EMPL; j++) {
				const employeeType_t emplType = (employeeType_t)j;
				TR_ForeachEmployee(employee, transfer, emplType) {
					if (employee->ugv) {
						/** @todo: add ugv listing when they're implemented */
					} else {
						cgi->UI_ExecuteConfunc("tr_listaddcargo %d \"%s\" \"%s\" \"%s\"", i, "tr_cargo.employee", va("ucn%i", employee->chr.ucn), va("%s %s", E_GetEmployeeString(employee->type, 1), employee->chr.name));
					}
				}
			}
		}
		/* Aliens */
		if (transfer->hasAliens) {
			int j;

			cgi->UI_ExecuteConfunc("tr_listaddcargo %d \"%s\" \"%s\" \"%s\"", i, "tr_cargo", "aliens", _("Aliens"));
			for (j = 0; j < cgi->csi->numTeamDefs; j++) {
				const teamDef_t *team = &cgi->csi->teamDef[j];

				if (transfer->alienAmount[j][TRANS_ALIEN_ALIVE]) {
					cgi->UI_ExecuteConfunc("tr_listaddcargo %d \"%s\" \"%s\" \"%s\"", i, "tr_cargo.aliens", va("%s_alive", team->id), va("%i %s %s", transfer->alienAmount[j][TRANS_ALIEN_ALIVE], _("alive"), _(team->name)));
				}

				if (transfer->alienAmount[j][TRANS_ALIEN_DEAD]) {
					cgi->UI_ExecuteConfunc("tr_listaddcargo %d \"%s\" \"%s\" \"%s\"", i, "tr_cargo.aliens", va("%s_dead", team->id), va("%i %s %s", transfer->alienAmount[j][TRANS_ALIEN_DEAD], _("dead"), _(team->name)));
				}
			}
		}
		/* Aircraft */
		if (transfer->hasAircraft) {
			cgi->UI_ExecuteConfunc("tr_listaddcargo %d \"%s\" \"%s\" \"%s\"", i, "tr_cargo", "aircraft", _("Aircraft"));

			TR_ForeachAircraft(aircraft, transfer) {
				cgi->UI_ExecuteConfunc("tr_listaddcargo %d \"%s\" \"%s\" \"%s\"", i, "tr_cargo.aircraft", va("craft%i", aircraft->idx), aircraft->name);
			}
		}
	}
}

/**
 * @brief Count capacity need of items and antimatter in arrays
 * @param[in] itemAmountArray Array to count items in
 * @param[in,out] capacity Capacity need array to update
 */
static void TR_CountItemSizeInArray (int itemAmountArray[], int capacity[])
{
	for (int i = 0; i < cgi->csi->numODs; i++) {
		const objDef_t *object = INVSH_GetItemByIDX(i);
		const int itemCargoAmount = itemAmountArray[i];

		if (itemCargoAmount <= 0)
			continue;
		if (Q_streq(object->id, ANTIMATTER_TECH_ID))
			capacity[CAP_ANTIMATTER] += itemCargoAmount;
		else
			capacity[CAP_ITEMS] += object->size * itemCargoAmount;
	}
}

/**
 * @brief Count capacity need of employee in array of lists
 * @param[in] employeeListArray Array to count employee in
 * @param[in,out] capacity Capacity need array to update
 */
static void TR_CountEmployeeInListArray (linkedList_t *employeeListArray[], int capacity[])
{
	for (int i = EMPL_SOLDIER; i < EMPL_ROBOT; i++) {
		capacity[CAP_EMPLOYEES] += LIST_Count(employeeListArray[i]);
	}
}

/**
 * @brief Count capacity need of live aliens in arrays
 * @param[in] aliensArray Array to count aliens in
 * @param[in,out] capacity Capacity need array to update
 */
static void TR_CountLiveAliensInArray (int aliensArray[][TRANS_ALIEN_MAX], int capacity[])
{
	for (int i = 0; i < ccs.numAliensTD; i++) {
		if (aliensArray[i][TRANS_ALIEN_ALIVE] > 0)
			capacity[CAP_ALIENS] += aliensArray[i][TRANS_ALIEN_ALIVE];
	}
}

/**
 * @brief Count capacity need of aircraft in lists
 * @param[in] aircraftList List to count aircraft in
 * @param[in,out] capacity Capacity need array to update
 */
static void TR_CountAircraftInList (linkedList_t *aircraftList, int capacity[])
{
	LIST_Foreach(aircraftList, aircraft_t, aircraft) {
		capacity[AIR_GetCapacityByAircraftWeight(aircraft)]++;
	}
}

/**
 * @brief Callback for assemble destination base capacity list
 * @note called via ui_trans_caplist <destbasidx>
 */
static void TR_DestinationCapacityList_f (void)
{
	if (cgi->Cmd_Argc() < 2) {
		Com_Printf("Usage: %s <destinationBaseIdx>\n", cgi->Cmd_Argv(0));
		return;
	}

	base_t *base = B_GetFoundedBaseByIDX(atoi(cgi->Cmd_Argv(1)));
	if (!base) {
		Com_Printf("Invalid destinationBaseIdx: %s\n", cgi->Cmd_Argv(1));
		return;
	}

	int currentCap[MAX_CAP];
	OBJZERO(currentCap);

	/* Count capacity need of active transfers */
	TR_Foreach(transfer) {
		if (transfer->destBase != base)
			continue;
		/* - Items and Antimatter */
		TR_CountItemSizeInArray(transfer->itemAmount, currentCap);
		/* - Employee */
		TR_CountEmployeeInListArray(transfer->employees, currentCap);
		/* - Aliens */
		TR_CountLiveAliensInArray(transfer->alienAmount, currentCap);
		/* - Aircraft */
		TR_CountAircraftInList(transfer->aircraft, currentCap);
	}

	/* Count capacity need of the current transfer plan */
	/* - Items and Antimatter */
	TR_CountItemSizeInArray(td.trItemsTmp, currentCap);
	/* - Employee */
	TR_CountEmployeeInListArray(td.trEmployeesTmp, currentCap);
	/* - Aliens */
	TR_CountLiveAliensInArray(td.trAliensTmp, currentCap);
	/* - Aircraft */
	TR_CountAircraftInList(td.aircraft, currentCap);

	cgi->UI_ExecuteConfunc("ui_t_capacities_clear");
	for (int i = 0; i < ccs.numBuildingTemplates; i++) {
		const building_t* building = &ccs.buildingTemplates[i];
		const baseCapacities_t capType = B_GetCapacityFromBuildingType(building->buildingType);

		/* skip not transferable capacities */
		if (capType == MAX_CAP || capType == CAP_LABSPACE || capType == CAP_WORKSPACE)
			continue;
		/* show only researched buildings' */
		if (!RS_IsResearched_ptr(building->tech))
			continue;

		capacities_t cap = *CAP_Get(base, capType);
		currentCap[capType] += cap.cur;
		if (cap.max <= 0 && currentCap[capType] <= 0)
			continue;

		cgi->UI_ExecuteConfunc("ui_t_capacities_add \"%s\" \"%s\" %d %d", building->id, _(building->name), currentCap[capType], cap.max);
	}
}

void TR_InitCallbacks (void)
{
	OBJZERO(td);

	cgi->Cmd_AddCommand("trans_list", TR_List_f, "Assembles the transferlist");
	cgi->Cmd_AddCommand("trans_init", TR_Init_f, "Init function for Transfer menu");
	cgi->Cmd_AddCommand("trans_list_scroll", TR_TransferList_Scroll_f, "Scrolls the transferlist");
	cgi->Cmd_AddCommand("trans_close", TR_TransferClose_f, "Callback for closing Transfer Menu");
	cgi->Cmd_AddCommand("trans_start", TR_TransferStart_f, "Starts the transfer");
	cgi->Cmd_AddCommand("trans_type", TR_TransferSelect_f, "Switch between transfer types (employees, techs, items)");
	cgi->Cmd_AddCommand("trans_emptyairstorage", TR_TransferListClear_f, "Unload everything from transfer cargo back to base");
	cgi->Cmd_AddCommand("trans_list_click", TR_TransferListSelect_f, "Callback for transfer list node click");
	cgi->Cmd_AddCommand("trans_cargolist_click", TR_CargoListSelect_f, "Callback for cargo list node click");
	cgi->Cmd_AddCommand("trans_selectbase", TR_SelectBase_f, "Callback for selecting a base");
	cgi->Cmd_AddCommand("trans_baselist_click", TR_TransferBaseListClick_f, "Callback for choosing base while recovering alien after mission");
	cgi->Cmd_AddCommand("trans_aliens", TR_TransferAliensFromMission_f, "Transfer aliens collected at missions");

	cgi->Cmd_AddCommand("ui_trans_caplist", TR_DestinationCapacityList_f, "Update destination base capacity list");
}

void TR_ShutdownCallbacks (void)
{
	int i;

	LIST_Delete(&td.aircraft);

	for (i = EMPL_SOLDIER; i < MAX_EMPL; i++) {
		LIST_Delete(&td.trEmployeesTmp[i]);
	}

	cgi->Cmd_RemoveCommand("ui_trans_caplist");

	cgi->Cmd_RemoveCommand("trans_list");
	cgi->Cmd_RemoveCommand("trans_init");
	cgi->Cmd_RemoveCommand("trans_list_scroll");
	cgi->Cmd_RemoveCommand("trans_close");
	cgi->Cmd_RemoveCommand("trans_start");
	cgi->Cmd_RemoveCommand("trans_type");
	cgi->Cmd_RemoveCommand("trans_emptyairstorage");
	cgi->Cmd_RemoveCommand("trans_list_click");
	cgi->Cmd_RemoveCommand("trans_cargolist_click");
	cgi->Cmd_RemoveCommand("trans_selectbase");
	cgi->Cmd_RemoveCommand("trans_baselist_click");
	cgi->Cmd_RemoveCommand("trans_aliens");
}
