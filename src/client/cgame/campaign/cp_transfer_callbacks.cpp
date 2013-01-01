/**
 * @file
 */

/*
Copyright (C) 2002-2013 UFO: Alien Invasion.

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

/**
 * @brief Clear temporary cargo arrays
 */
static void TR_ClearTempCargo (void)
{
	OBJZERO(td.trItemsTmp);
	OBJZERO(td.trAliensTmp);
	for (int i = EMPL_SOLDIER; i < MAX_EMPL; i++) {
		const employeeType_t emplType = (employeeType_t)i;
		LIST_Delete(&td.trEmployeesTmp[emplType]);
	}
	LIST_Delete(&td.aircraft);
}

/* === Transfer Aliens From Mission (to be removed) === */

/**
 * @brief Counts the stunned aliens in the cargo of the aircraft.
 * @param[in] transferAircraft with the stunned aliens.
 */
static int TR_CountStunnedAliensInCargo (const aircraft_t *transferAircraft)
{
	int stunnedAliens = 0;
	const alienCargo_t *cargo = AL_GetAircraftAlienCargo(transferAircraft);

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

/* === Normal base-to-base transfers === */

/**
 * @brief Starts the transfer.
 */
static void TR_TransferStart_f (void)
{
	char message[1024];
	base_t *base = B_GetCurrentSelectedBase();

	if (td.currentTransferType == TRANS_TYPE_INVALID) {
		Com_Printf("TR_TransferStart_f: currentTransferType is wrong!\n");
		return;
	}

	if (TR_TransferStart(base, td) == NULL)
		return;
	TR_ClearTempCargo();

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
	/* reset for every new call */
	cgi->UI_ExecuteConfunc("ui_cargolist_clear");

	/* Show items. */
	for (int i = 0; i < cgi->csi->numODs; i++) {
		const objDef_t *od = INVSH_GetItemByIDX(i);
		const int amount = td.trItemsTmp[i];
		if (amount > 0)
			cgi->UI_ExecuteConfunc("ui_cargolist_add \"%s\" \"%s\" %d", od->id, _(od->name), amount);
	}

	/* Show employees. */
	for (int i = 0; i < MAX_EMPL; i++) {
		const employeeType_t emplType = (employeeType_t)i;
		switch (emplType) {
		case EMPL_SOLDIER:
		case EMPL_PILOT:
			LIST_Foreach(td.trEmployeesTmp[emplType], employee_t, employee) {
				if (emplType == EMPL_SOLDIER) {
					const rank_t *rank = CL_GetRankByIdx(employee->chr.score.rank);
					cgi->UI_ExecuteConfunc("ui_cargolist_add \"ucn:%d\" \"%s %s %s\" %d", employee->chr.ucn,
						E_GetEmployeeString((employeeType_t)emplType, 1), rank->shortname, employee->chr.name, 1);
				} else {
					cgi->UI_ExecuteConfunc("ui_cargolist_add \"ucn:%d\" \"%s %s\" %d", employee->chr.ucn,
						E_GetEmployeeString((employeeType_t)emplType, 1), employee->chr.name, 1);
				}
			}
			break;
		case EMPL_ROBOT:
			/** @todo implement UGV transfers */
			break;
		case EMPL_SCIENTIST:
		case EMPL_WORKER: {
			int emplCount = LIST_Count(td.trEmployeesTmp[emplType]);
			if (emplCount <= 0)
				break;
			cgi->UI_ExecuteConfunc("ui_cargolist_add \"%s\" \"%s\" %d", (emplType == EMPL_SCIENTIST) ? "scientist" : "worker",
				E_GetEmployeeString((employeeType_t)emplType, emplCount), emplCount);
			break;
		}
		default:
			cgi->Com_Error(ERR_DROP, "TR_CargoList: Invalid employeetype in cargo");
		}
	}

	/* Show aliens. */
	for (int i = 0; i < ccs.numAliensTD; i++) {
		const teamDef_t* teamDef = AL_GetAlienTeamDef(i);
		const int transferDead = td.trAliensTmp[i][TRANS_ALIEN_DEAD];
		const int transferAlive = td.trAliensTmp[i][TRANS_ALIEN_ALIVE];

		if (teamDef == NULL)
			continue;
		if (transferDead > 0)
			cgi->UI_ExecuteConfunc("ui_cargolist_add \"dead:%s\" \"%s\" %d", teamDef->id, va(_("Corpse of %s"), _(teamDef->name)), transferDead);
		if (transferAlive > 0)
			cgi->UI_ExecuteConfunc("ui_translist_add \"alive:%s\" \"%s\" %d", teamDef->id, va(_("Alive %s"), _(teamDef->name)), transferAlive);
	}

	/* Show all aircraft. */
	LIST_Foreach(td.aircraft, aircraft_t, aircraft) {
		cgi->UI_ExecuteConfunc("ui_cargolist_add \"aircraft:%d\" \"%s\" %d", aircraft->idx, va(_("Aircraft %s"), aircraft->name), 1);
	}
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

/**
 * @brief Add items to the transfer storages list
 * @param[in] srcBase Source base of the transfer
 * @param[in] destBase Destination base of the transfer
 */
static void TR_FillItems (const base_t *srcBase, const base_t *destBase)
{
	const objDef_t *od;

	od = INVSH_GetItemByID(ANTIMATTER_TECH_ID);
	if (od) {
		const int itemCargoAmount = td.trItemsTmp[od->idx];
		const int antiMatterInSrcBase = B_AntimatterInBase(srcBase);
		const int antiMatterInDstBase = B_AntimatterInBase(destBase);

		if (itemCargoAmount || antiMatterInSrcBase) {
			cgi->UI_ExecuteConfunc("ui_translist_add \"%s\" \"%s\" %d %d %d %d %d", od->id, _(od->name),
				antiMatterInSrcBase, antiMatterInDstBase, 0, itemCargoAmount, antiMatterInSrcBase + itemCargoAmount);
		}
	}
	for (int i = 0; i < cgi->csi->numODs; i++) {
		od = INVSH_GetItemByIDX(i);
		assert(od);
		if (!B_ItemIsStoredInBaseStorage(od))
			continue;
		const int itemCargoAmount = td.trItemsTmp[od->idx];
		const int itemInSrcBase = B_ItemInBase(od, srcBase);
		const int itemInDstBase = B_ItemInBase(od, destBase);
		if (itemCargoAmount || itemInSrcBase > 0) {
			cgi->UI_ExecuteConfunc("ui_translist_add \"%s\" \"%s\" %d %d %d %d %d", od->id, _(od->name),
				itemInSrcBase, itemInDstBase, 0, itemCargoAmount, itemInSrcBase + itemCargoAmount);
		}
	}

}

/**
 * @brief Add employees to the transfer storages list
 * @param[in] srcBase Source base of the transfer
 * @param[in] destBase Destination base of the transfer
 */
static void TR_FillEmployees (const base_t *srcBase, const base_t *destBase)
{
	for (int i = EMPL_SOLDIER; i < MAX_EMPL; i++) {
		const employeeType_t emplType = (employeeType_t)i;
		switch (emplType) {
		case EMPL_SOLDIER:
		case EMPL_PILOT: {
			E_Foreach(emplType, employee) {
				char str[128];

				if (!E_IsInBase(employee, srcBase))
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

				cgi->UI_ExecuteConfunc("ui_translist_add \"ucn:%d\" \"%s\" %d %d %d %d %d", employee->chr.ucn,
					str, -1, -1, -1, -1, -1);
			}
			break;
		}
		case EMPL_ROBOT:
			/** @todo implement UGV transfers */
			break;
		case EMPL_SCIENTIST:
		case EMPL_WORKER: {
			const int hiredSrc = E_CountHired(srcBase, emplType);
			const int hiredDst = E_CountHired(destBase, emplType);
			const int trCount = LIST_Count(td.trEmployeesTmp[emplType]);

			if (hiredSrc <= 0)
				break;

			cgi->UI_ExecuteConfunc("ui_translist_add \"%s\" \"%s\" %d %d %d %d %d",
				(emplType == EMPL_SCIENTIST) ? "scientist" : "worker", E_GetEmployeeString(emplType, hiredSrc),
				hiredSrc - trCount, hiredDst, 0, trCount, hiredSrc);
			break;
		}
		default:
			cgi->Com_Error(ERR_DROP, "TR_CargoList: Invalid employeetype in cargo");
		}
	}
}

/**
 * @brief Add aliens to the transfer storages list
 * @param[in] srcBase Source base of the transfer
 * @param[in] destBase Destination base of the transfer
 */
static void TR_FillAliens (const base_t *srcBase, const base_t *destBase)
{
	for (int i = 0; i < ccs.numAliensTD; i++) {
		const aliensCont_t *alienContSrc = &srcBase->alienscont[i];
		const aliensCont_t *alienContDst = &destBase->alienscont[i];
		const teamDef_t *teamDef = alienContSrc->teamDef;
		if (teamDef == NULL)
			continue;
		if (alienContSrc->amountDead > 0 || td.trAliensTmp[i][TRANS_ALIEN_DEAD] > 0) {
			char str[128];
			const int transferDead = td.trAliensTmp[i][TRANS_ALIEN_DEAD];
			Com_sprintf(str, sizeof(str), _("Corpse of %s"), _(teamDef->name));

			cgi->UI_ExecuteConfunc("ui_translist_add \"dead:%s\" \"%s\" %d %d %d %d %d",
				teamDef->id, str, alienContSrc->amountDead, alienContDst->amountDead,
				0, transferDead, alienContSrc->amountDead + transferDead);
		}
		if (alienContSrc->amountAlive > 0 || td.trAliensTmp[i][TRANS_ALIEN_ALIVE] > 0) {
			char str[128];
			const int transferAlive = td.trAliensTmp[i][TRANS_ALIEN_ALIVE];
			Com_sprintf(str, sizeof(str), _("Alive %s"), _(teamDef->name));

			cgi->UI_ExecuteConfunc("ui_translist_add \"alive:%s\" \"%s\" %d %d %d %d %d",
				teamDef->id, str, alienContSrc->amountAlive, alienContDst->amountAlive,
				0, transferAlive, alienContSrc->amountAlive + transferAlive);
		}
	}
}

/**
 * @brief Add aircraft to the transfer storages list
 * @param[in] srcBase Source base of the transfer
 * @param[in] destBase Destination base of the transfer
 */
static void TR_FillAircraft (const base_t *srcBase, const base_t *destBase)
{
	AIR_ForeachFromBase(aircraft, srcBase) {
		/* Aircraft is not in base. */
		if (!AIR_IsAircraftInBase(aircraft))
			continue;
		/* Already on transfer list. */
		if (LIST_GetPointer(td.aircraft, aircraft))
			continue;

		cgi->UI_ExecuteConfunc("ui_translist_add \"aircraft:%d\" \"%s\" %d %d %d %d %d",
			aircraft->idx, aircraft->name, -1, -1, -1, -1, -1);
	}
}

/**
 * @brief Fills the items-in-base list with stuff available for transfer.
 * @note Filling the transfer list with proper stuff (items/employees/aliens/aircraft) is being done here.
 * @param[in] srcBase Pointer to the base the transfer starts from
 * @param[in] destBase Pointer to the base to transfer
 * @param[in] transferType Transfer category
 * @sa transferType_t
 */
static void TR_Fill (const base_t *srcBase, const base_t *destBase, transferType_t transferType)
{
	if (srcBase == NULL || destBase == NULL)
		return;

	td.currentTransferType = transferType;
	/* reset for every new call */
	cgi->UI_ExecuteConfunc("ui_translist_clear");
	switch (transferType) {
	case TRANS_TYPE_ITEM:
		TR_FillItems(srcBase, destBase);
		break;
	case TRANS_TYPE_EMPLOYEE:
		TR_FillEmployees(srcBase, destBase);
		break;
	case TRANS_TYPE_ALIEN:
		TR_FillAliens(srcBase, destBase);
		break;
	case TRANS_TYPE_AIRCRAFT:
		TR_FillAircraft(srcBase, destBase);
		break;
	default:
		cgi->Com_Error(ERR_DROP, "invalid transfertype given: %i", transferType);
	}
	/* Update cargo list. */
	TR_CargoList();
}

/**
 * @brief Callback for filling list with stuff available for transfer.
 */
static void TR_Fill_f (void)
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
	TR_Fill(base, td.transferBase, type);
}

/**
 * @brief Callback handles adding/removing items to transfercargo
 */
static void TR_Add_f (void)
{
	base_t *base = B_GetCurrentSelectedBase();

	if (cgi->Cmd_Argc() < 3) {
		Com_Printf("Usage: %s <itemid> <amount>", cgi->Cmd_Argv(0));
		return;
	}

	char itemId[MAX_VAR * 2];
	int amount = atoi(cgi->Cmd_Argv(2));
	Q_strncpyz(itemId, cgi->Cmd_Argv(1), sizeof(itemId));

	if (Q_strstart(itemId, "aircraft:")) {
		aircraft_t *aircraft = AIR_AircraftGetFromIDX(atoi(itemId + 9));
		if (!aircraft)
			return;
		if (amount > 0) {
			if (!TR_AircraftListSelect(aircraft))
				return;
			LIST_AddPointer(&td.aircraft, (void*)aircraft);
		} else if (amount < 0) {
			LIST_Remove(&td.aircraft, (void*)aircraft);
		}
	} else if (Q_strstart(itemId, "ucn:")) {
		employee_t *employee = E_GetEmployeeFromChrUCN(atoi(itemId + 4));
		if (!employee)
			return;
		if (amount > 0) {
			if (!E_IsInBase(employee, base))
				return;
			if (LIST_GetPointer(td.trEmployeesTmp[employee->type], (void*)employee))
				return;
			if (!TR_CheckEmployee(employee, td.transferBase))
				return;

			LIST_AddPointer(&td.trEmployeesTmp[employee->type], (void*)employee);
		} else if (amount < 0) {
			LIST_Remove(&td.trEmployeesTmp[employee->type], (void*)employee);
		}
	} else if (Q_streq(itemId, "scientist")) {
		if (amount > 0) {
			E_Foreach(EMPL_SCIENTIST, employee) {
				if (amount == 0)
					break;
				if (!E_IsInBase(employee, base))
					continue;
				/* Already on transfer list. */
				if (LIST_GetPointer(td.trEmployeesTmp[EMPL_SCIENTIST], (void*)employee))
					continue;
				if (!TR_CheckEmployee(employee, td.transferBase))
					continue;
				LIST_AddPointer(&td.trEmployeesTmp[EMPL_SCIENTIST], (void*) employee);
				amount--;
			}
		} else if (amount < 0) {
			while (!LIST_IsEmpty(td.trEmployeesTmp[EMPL_SCIENTIST]) && amount < 0) {
				if (LIST_RemoveEntry(&td.trEmployeesTmp[EMPL_SCIENTIST], td.trEmployeesTmp[EMPL_SCIENTIST]))
					amount++;
			}
		}
	} else if (Q_streq(itemId, "worker")) {
		if (amount > 0) {
			E_Foreach(EMPL_WORKER, employee) {
				if (amount == 0)
					break;
				if (!E_IsInBase(employee, base))
					continue;
				/* Already on transfer list. */
				if (LIST_GetPointer(td.trEmployeesTmp[EMPL_WORKER], (void*)employee))
					continue;
				if (!TR_CheckEmployee(employee, td.transferBase))
					continue;
				LIST_AddPointer(&td.trEmployeesTmp[EMPL_WORKER], (void*) employee);
				amount--;
			}
		} else if (amount < 0) {
			while (!LIST_IsEmpty(td.trEmployeesTmp[EMPL_WORKER]) && amount < 0) {
				if (LIST_RemoveEntry(&td.trEmployeesTmp[EMPL_WORKER], td.trEmployeesTmp[EMPL_WORKER]))
					amount++;
			}
		}
	} else if (Q_strstart(itemId, "alive:")) {
		for (int i = 0; i < ccs.numAliensTD; i++) {
			aliensCont_t *aliensCont = &base->alienscont[i];
			if (!aliensCont->teamDef)
				continue;
			if (!Q_streq(aliensCont->teamDef->id, itemId + 6))
				continue;

			const int cargo = td.trAliensTmp[i][TRANS_ALIEN_ALIVE];
			const int store = aliensCont->amountAlive;

			if (amount >= 0)
				amount = std::min(amount, store);
			else
				amount = std::max(amount, -cargo);

			if (amount != 0) {
				td.trAliensTmp[i][TRANS_ALIEN_ALIVE] += amount;
				AL_ChangeAliveAlienNumber(base, aliensCont, -amount);
			}
			break;
		}
	} else if (Q_strstart(itemId, "dead:")) {
		for (int i = 0; i < ccs.numAliensTD; i++) {
			aliensCont_t *aliensCont = &base->alienscont[i];
			if (!aliensCont->teamDef)
				continue;
			if (!Q_streq(aliensCont->teamDef->id, itemId + 5))
				continue;

			const int cargo = td.trAliensTmp[i][TRANS_ALIEN_DEAD];
			const int store = aliensCont->amountDead;

			if (amount >= 0)
				amount = std::min(amount, store);
			else
				amount = std::max(amount, -cargo);

			if (amount != 0) {
				td.trAliensTmp[i][TRANS_ALIEN_DEAD] += amount;
				aliensCont->amountDead -= amount;
			}
			break;
		}
	} else if (Q_streq(itemId, ANTIMATTER_TECH_ID)) {
		/* antimatter */
		const objDef_t *od = INVSH_GetItemByID(itemId);
		if (!od)
			return;
		const int cargo = td.trItemsTmp[od->idx];
		const int store = B_AntimatterInBase(base);

		if (amount >= 0)
			amount = std::min(amount, store);
		else
			amount = std::max(amount, -cargo);

		if (amount != 0) {
			td.trItemsTmp[od->idx] += amount;
			B_ManageAntimatter(base, amount, false);
		}
	} else {
		/* items */
		const objDef_t *od = INVSH_GetItemByID(itemId);
		if (!od)
			return;
		if (!B_ItemIsStoredInBaseStorage(od))
			return;

		const int cargo = td.trItemsTmp[od->idx];
		const int store = B_ItemInBase(od, base);
		if (amount >= 0)
			amount = std::min(amount, store);
		else
			amount = std::max(amount, -cargo);

		if (amount != 0) {
			td.trItemsTmp[od->idx] += amount;
			B_UpdateStorageAndCapacity(base, od, -amount, false);
		}
	}

	TR_Fill(base, td.transferBase, td.currentTransferType);
	/* Update capacity list of destination base */
	if (td.transferBase)
		cgi->Cmd_ExecuteString(va("ui_trans_caplist %d", td.transferBase->idx));
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

	TR_ClearTempCargo();

	/* Update cargo list and items list. */
	TR_CargoList();
	TR_Fill(base, td.transferBase, td.currentTransferType);

	/* Update capacity list of destination base */
	if (td.transferBase)
		cgi->Cmd_ExecuteString(va("ui_trans_caplist %d", td.transferBase->idx));
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
	TR_Fill(srcbase, destbase, td.currentTransferType);

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

/**
 * @brief Transfer menu init function.
 * @note Command to call this: trans_init
 * @note Should be called whenever the Transfer menu gets active.
 */
static void TR_Init_f (void)
{
	base_t *base = B_GetCurrentSelectedBase();

	TR_ClearTempCargo();

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
	cgi->Cmd_ExecuteString(va("ui_trans_fill %s", transferTypeIDs[0]));
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
	cgi->Cmd_AddCommand("trans_list", TR_List_f, "Assembles the transferlist");
	cgi->Cmd_AddCommand("trans_init", TR_Init_f, "Init function for Transfer menu");
	cgi->Cmd_AddCommand("trans_close", TR_TransferClose_f, "Callback for closing Transfer Menu");
	cgi->Cmd_AddCommand("trans_start", TR_TransferStart_f, "Starts the transfer");
	cgi->Cmd_AddCommand("trans_emptyairstorage", TR_TransferListClear_f, "Unload everything from transfer cargo back to base");
	cgi->Cmd_AddCommand("trans_selectbase", TR_SelectBase_f, "Callback for selecting a base");
	cgi->Cmd_AddCommand("trans_baselist_click", TR_TransferBaseListClick_f, "Callback for choosing base while recovering alien after mission");
	cgi->Cmd_AddCommand("trans_aliens", TR_TransferAliensFromMission_f, "Transfer aliens collected at missions");

	cgi->Cmd_AddCommand("ui_trans_caplist", TR_DestinationCapacityList_f, "Update destination base capacity list");
	cgi->Cmd_AddCommand("ui_trans_fill", TR_Fill_f, "Fill itemlists for transfer");
	cgi->Cmd_AddCommand("ui_trans_add", TR_Add_f, "Add/Remove items to transfercargo");
}

void TR_ShutdownCallbacks (void)
{
	TR_ClearTempCargo();

	cgi->Cmd_RemoveCommand("ui_trans_caplist");
	cgi->Cmd_RemoveCommand("ui_trans_fill");
	cgi->Cmd_RemoveCommand("ui_trans_add");

	cgi->Cmd_RemoveCommand("trans_list");
	cgi->Cmd_RemoveCommand("trans_init");
	cgi->Cmd_RemoveCommand("trans_close");
	cgi->Cmd_RemoveCommand("trans_start");
	cgi->Cmd_RemoveCommand("trans_emptyairstorage");
	cgi->Cmd_RemoveCommand("trans_selectbase");
	cgi->Cmd_RemoveCommand("trans_baselist_click");
	cgi->Cmd_RemoveCommand("trans_aliens");
}
