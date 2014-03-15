/**
 * @file
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

#include "cp_transfer_callbacks.h"
#include "../../cl_shared.h"
#include "cp_campaign.h"
#include "cp_capacity.h"
#include "cp_transfer.h"
#include "cp_popup.h"
#include "cp_time.h"
#include "../../ui/ui_dataids.h"
#include "aliencargo.h"
#include "aliencontainment.h"

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
	if (td.alienCargo != nullptr) {
		delete td.alienCargo;
		td.alienCargo = nullptr;
	}
	for (int i = EMPL_SOLDIER; i < MAX_EMPL; i++) {
		const employeeType_t emplType = (employeeType_t)i;
		cgi->LIST_Delete(&td.trEmployeesTmp[emplType]);
	}
	cgi->LIST_Delete(&td.aircraft);
}

/**
 * @brief Starts the transfer.
 */
static void TR_TransferStart_f (void)
{
	char message[1024];
	base_t* base = B_GetCurrentSelectedBase();

	if (td.currentTransferType == TRANS_TYPE_INVALID) {
		Com_Printf("TR_TransferStart_f: currentTransferType is wrong!\n");
		return;
	}

	if (TR_TransferStart(base, td) == nullptr)
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
static transferType_t TR_GetTransferType (const char* id)
{
	for (int i = 0; i < TRANS_TYPE_MAX; i++) {
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
static bool TR_CheckEmployee (const Employee* employee, const base_t* destbase)
{
	assert(employee && destbase);

	switch (employee->getType()) {
	case EMPL_SOLDIER:
		/* Is this a soldier assigned to aircraft? */
		if (AIR_IsEmployeeInAircraft(employee, nullptr)) {
			const rank_t* rank = CL_GetRankByIdx(employee->chr.score.rank);
			CP_Popup(_("Soldier in aircraft"), _("%s %s is assigned to aircraft and cannot be\ntransferred to another base.\n"),
					_(rank->shortname), employee->chr.name);
			return false;
		}
		break;
	case EMPL_PILOT:
		/* Is this a pilot assigned to aircraft? */
		if (AIR_IsEmployeeInAircraft(employee, nullptr)) {
			CP_Popup(_("Pilot in aircraft"), _("%s is assigned to aircraft and cannot be\ntransferred to another base.\n"),
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
		const objDef_t* od = INVSH_GetItemByIDX(i);
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
			LIST_Foreach(td.trEmployeesTmp[emplType], Employee, employee) {
				if (emplType == EMPL_SOLDIER) {
					const rank_t* rank = CL_GetRankByIdx(employee->chr.score.rank);
					cgi->UI_ExecuteConfunc("ui_cargolist_add \"ucn:%d\" \"%s %s %s\" %d", employee->chr.ucn,
						E_GetEmployeeString((employeeType_t)emplType, 1), _(rank->shortname), employee->chr.name, 1);
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
			int emplCount = cgi->LIST_Count(td.trEmployeesTmp[emplType]);
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
	if (td.alienCargo != nullptr) {
		linkedList_t* cargo = td.alienCargo->list();
		LIST_Foreach(cargo, alienCargo_t, item) {
			if (item->dead > 0)
				cgi->UI_ExecuteConfunc("ui_cargolist_add \"dead:%s\" \"%s\" %d", item->teamDef->id, va(_("Corpse of %s"), _(item->teamDef->name)), item->dead);
			if (item->alive > 0)
				cgi->UI_ExecuteConfunc("ui_cargolist_add \"alive:%s\" \"%s\" %d", item->teamDef->id, va(_("Alive %s"), _(item->teamDef->name)), item->alive);
		}
		cgi->LIST_Delete(&cargo);
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
static bool TR_AircraftListSelect (const aircraft_t* aircraft)
{
	if (!AIR_IsAircraftInBase(aircraft))	/* Aircraft is not in base. */
		return false;
	if (cgi->LIST_GetPointer(td.aircraft, aircraft))	/* Already on transfer list. */
		return false;

	return true;
}

/**
 * @brief Add items to the transfer storages list
 * @param[in] srcBase Source base of the transfer
 * @param[in] destBase Destination base of the transfer
 */
static void TR_FillItems (const base_t* srcBase, const base_t* destBase)
{
	const objDef_t* od;

	od = INVSH_GetItemByID(ANTIMATTER_ITEM_ID);
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
static void TR_FillEmployees (const base_t* srcBase, const base_t* destBase)
{
	for (int i = EMPL_SOLDIER; i < MAX_EMPL; i++) {
		const employeeType_t emplType = (employeeType_t)i;
		switch (emplType) {
		case EMPL_SOLDIER:
		case EMPL_PILOT: {
			E_Foreach(emplType, employee) {
				char str[128];

				if (!employee->isHiredInBase(srcBase))
					continue;

				/* Skip if already on transfer list. */
				if (cgi->LIST_GetPointer(td.trEmployeesTmp[emplType], (void*) employee))
					continue;

				if (emplType == EMPL_SOLDIER) {
					const rank_t* rank = CL_GetRankByIdx(employee->chr.score.rank);
					Com_sprintf(str, sizeof(str), "%s %s %s", E_GetEmployeeString(emplType, 1), _(rank->shortname), employee->chr.name);
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
			const int trCount = cgi->LIST_Count(td.trEmployeesTmp[emplType]);

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
static void TR_FillAliens (const base_t* srcBase, const base_t* destBase)
{
	if (!srcBase->alienContainment)
		return;

	linkedList_t* list = srcBase->alienContainment->list();
	LIST_Foreach(list, alienCargo_t, item) {
		const int srcDead = item->dead;
		const int srcAlive = item->alive;
		const int dstDead = (destBase->alienContainment) ? destBase->alienContainment->getAlive(item->teamDef) : 0;
		const int dstAlive = (destBase->alienContainment) ? destBase->alienContainment->getDead(item->teamDef) : 0;
		const int transferDead = (td.alienCargo) ? td.alienCargo->getDead(item->teamDef) : 0;
		const int transferAlive = (td.alienCargo) ? td.alienCargo->getAlive(item->teamDef) : 0;

		if (srcDead > 0 || transferDead > 0) {
			char str[128];
			Com_sprintf(str, sizeof(str), _("Corpse of %s"), _(item->teamDef->name));
			cgi->UI_ExecuteConfunc("ui_translist_add \"dead:%s\" \"%s\" %d %d %d %d %d",
				item->teamDef->id, str, srcDead, dstDead, 0, transferDead, srcDead + transferDead);
		}
		if (srcAlive > 0 || transferAlive > 0) {
			char str[128];
			Com_sprintf(str, sizeof(str), _("Alive %s"), _(item->teamDef->name));
			cgi->UI_ExecuteConfunc("ui_translist_add \"alive:%s\" \"%s\" %d %d %d %d %d",
				item->teamDef->id, str, srcAlive, dstAlive,	0, transferAlive, srcAlive + transferAlive);
		}
	}
	cgi->LIST_Delete(&list);
}

/**
 * @brief Add aircraft to the transfer storages list
 * @param[in] srcBase Source base of the transfer
 * @param[in] destBase Destination base of the transfer
 */
static void TR_FillAircraft (const base_t* srcBase, const base_t* destBase)
{
	AIR_ForeachFromBase(aircraft, srcBase) {
		/* Aircraft is not in base. */
		if (!AIR_IsAircraftInBase(aircraft))
			continue;
		/* Already on transfer list. */
		if (cgi->LIST_GetPointer(td.aircraft, aircraft))
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
static void TR_Fill (const base_t* srcBase, const base_t* destBase, transferType_t transferType)
{
	if (srcBase == nullptr || destBase == nullptr)
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
	const base_t* base = B_GetCurrentSelectedBase();

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
	base_t* base = B_GetCurrentSelectedBase();

	if (cgi->Cmd_Argc() < 3) {
		Com_Printf("Usage: %s <itemid> <amount>", cgi->Cmd_Argv(0));
		return;
	}

	char itemId[MAX_VAR * 2];
	int amount = atoi(cgi->Cmd_Argv(2));
	Q_strncpyz(itemId, cgi->Cmd_Argv(1), sizeof(itemId));

	if (Q_strstart(itemId, "aircraft:")) {
		aircraft_t* aircraft = AIR_AircraftGetFromIDX(atoi(itemId + 9));
		if (!aircraft)
			return;
		if (amount > 0) {
			if (!TR_AircraftListSelect(aircraft))
				return;
			cgi->LIST_AddPointer(&td.aircraft, (void*)aircraft);
		} else if (amount < 0) {
			cgi->LIST_Remove(&td.aircraft, (void*)aircraft);
		}
	} else if (Q_strstart(itemId, "ucn:")) {
		Employee* employee = E_GetEmployeeFromChrUCN(atoi(itemId + 4));
		if (!employee)
			return;
		if (amount > 0) {
			if (!employee->isHiredInBase(base))
				return;
			if (cgi->LIST_GetPointer(td.trEmployeesTmp[employee->getType()], (void*)employee))
				return;
			if (!TR_CheckEmployee(employee, td.transferBase))
				return;

			cgi->LIST_AddPointer(&td.trEmployeesTmp[employee->getType()], (void*)employee);
		} else if (amount < 0) {
			cgi->LIST_Remove(&td.trEmployeesTmp[employee->getType()], (void*)employee);
		}
	} else if (Q_streq(itemId, "scientist")) {
		if (amount > 0) {
			E_Foreach(EMPL_SCIENTIST, employee) {
				if (amount == 0)
					break;
				if (!employee->isHiredInBase(base))
					continue;
				/* Already on transfer list. */
				if (cgi->LIST_GetPointer(td.trEmployeesTmp[EMPL_SCIENTIST], (void*)employee))
					continue;
				if (!TR_CheckEmployee(employee, td.transferBase))
					continue;
				cgi->LIST_AddPointer(&td.trEmployeesTmp[EMPL_SCIENTIST], (void*) employee);
				amount--;
			}
		} else if (amount < 0) {
			while (!cgi->LIST_IsEmpty(td.trEmployeesTmp[EMPL_SCIENTIST]) && amount < 0) {
				if (cgi->LIST_RemoveEntry(&td.trEmployeesTmp[EMPL_SCIENTIST], td.trEmployeesTmp[EMPL_SCIENTIST]))
					amount++;
			}
		}
	} else if (Q_streq(itemId, "worker")) {
		if (amount > 0) {
			E_Foreach(EMPL_WORKER, employee) {
				if (amount == 0)
					break;
				if (!employee->isHiredInBase(base))
					continue;
				/* Already on transfer list. */
				if (cgi->LIST_GetPointer(td.trEmployeesTmp[EMPL_WORKER], (void*)employee))
					continue;
				if (!TR_CheckEmployee(employee, td.transferBase))
					continue;
				cgi->LIST_AddPointer(&td.trEmployeesTmp[EMPL_WORKER], (void*) employee);
				amount--;
			}
		} else if (amount < 0) {
			while (!cgi->LIST_IsEmpty(td.trEmployeesTmp[EMPL_WORKER]) && amount < 0) {
				if (cgi->LIST_RemoveEntry(&td.trEmployeesTmp[EMPL_WORKER], td.trEmployeesTmp[EMPL_WORKER]))
					amount++;
			}
		}
	} else if (Q_strstart(itemId, "alive:")) {
		if (td.alienCargo == nullptr)
			td.alienCargo = new AlienCargo();
		if (td.alienCargo == nullptr)
			cgi->Com_Error(ERR_DROP, "TR_Add_f: Cannot create AlienCargo object\n");

		const teamDef_t* teamDef = cgi->Com_GetTeamDefinitionByID(itemId + 6);
		if (teamDef && base->alienContainment) {
			const int cargo = td.alienCargo->getAlive(teamDef);
			const int store = base->alienContainment->getAlive(teamDef);

			if (amount >= 0)
				amount = std::min(amount, store);
			else
				amount = std::max(amount, -cargo);

			if (amount != 0) {
				td.alienCargo->add(teamDef, amount, 0);
				base->alienContainment->add(teamDef, -amount, 0);
			}
		}
	} else if (Q_strstart(itemId, "dead:")) {
		if (td.alienCargo == nullptr)
			td.alienCargo = new AlienCargo();
		if (td.alienCargo == nullptr)
			cgi->Com_Error(ERR_DROP, "TR_Add_f: Cannot create AlienCargo object\n");

		const teamDef_t* teamDef = cgi->Com_GetTeamDefinitionByID(itemId + 5);
		if (teamDef && base->alienContainment) {
			const int cargo = td.alienCargo->getDead(teamDef);
			const int store = base->alienContainment->getDead(teamDef);

			if (amount >= 0)
				amount = std::min(amount, store);
			else
				amount = std::max(amount, -cargo);

			if (amount != 0) {
				td.alienCargo->add(teamDef, 0, amount);
				base->alienContainment->add(teamDef, 0, -amount);
			}
		}
	} else if (Q_streq(itemId, ANTIMATTER_ITEM_ID)) {
		/* antimatter */
		const objDef_t* od = INVSH_GetItemByID(itemId);
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
		const objDef_t* od = INVSH_GetItemByID(itemId);
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
			B_AddToStorage(base, od, -amount);
		}
	}

	TR_Fill(base, td.transferBase, td.currentTransferType);
	/* Update capacity list of destination base */
	if (td.transferBase)
		cgi->Cmd_ExecuteString("ui_trans_caplist %d", td.transferBase->idx);
}

/**
 * @brief Unload everything from transfer cargo back to base.
 * @note This is being executed by pressing Unload button in menu.
 */
static void TR_TransferListClear_f (void)
{
	base_t* base = B_GetCurrentSelectedBase();

	if (!base)
		return;

	for (int i = 0; i < cgi->csi->numODs; i++) {	/* Return items. */
		const objDef_t* od = INVSH_GetItemByIDX(i);
		const int itemCargoAmount = td.trItemsTmp[od->idx];
		if (itemCargoAmount > 0) {
			if (Q_streq(od->id, ANTIMATTER_ITEM_ID))
				B_ManageAntimatter(base, itemCargoAmount, true);
			else
				B_AddToStorage(base, od, itemCargoAmount);
		}
	}
	if (td.alienCargo && base->alienContainment) {
		linkedList_t* list = td.alienCargo->list();
		LIST_Foreach(list, alienCargo_t, item) {
			base->alienContainment->add(item->teamDef, item->alive, item->dead);
		}
		cgi->LIST_Delete(&list);
	}

	TR_ClearTempCargo();

	/* Update cargo list and items list. */
	TR_CargoList();
	TR_Fill(base, td.transferBase, td.currentTransferType);

	/* Update capacity list of destination base */
	if (td.transferBase)
		cgi->Cmd_ExecuteString("ui_trans_caplist %d", td.transferBase->idx);
}

/**
 * @brief Callback for base list click.
 * @note transferBase is being set here.
 * @param[in] srcbase
 * @param[in] destbase Pointer to base which will be transferBase.
 */
static void TR_TransferBaseSelect (base_t* srcbase, base_t* destbase)
{
	if (!destbase || !srcbase)
		return;

	/* Set global pointer to current selected base. */
	td.transferBase = destbase;
	cgi->Cvar_Set("mn_trans_base_name", "%s", destbase->name);
	cgi->Cvar_SetValue("mn_trans_base_id", destbase->idx);

	/* Update stuff-in-base list. */
	TR_Fill(srcbase, destbase, td.currentTransferType);

	/* Update capacity list of destination base */
	if (td.transferBase)
		cgi->Cmd_ExecuteString("ui_trans_caplist %d", td.transferBase->idx);
}

/**
 * @brief Fills the optionlist with available bases to transfer to
 */
static void TR_InitBaseList (void)
{
	const base_t* currentBase = B_GetCurrentSelectedBase();
	uiNode_t* baseList = nullptr;
	base_t* base = nullptr;

	while ((base = B_GetNext(base)) != nullptr) {
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
	base_t* base = B_GetCurrentSelectedBase();
	base_t* destbase;

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
	base_t* base = B_GetCurrentSelectedBase();

	TR_ClearTempCargo();

	/* Update destination base list */
	TR_InitBaseList();
	/* Select first available base. */
	td.transferBase = B_GetNext(base);
	/* If this was the last base select the first */
	if (!td.transferBase)
		td.transferBase = B_GetNext(nullptr);
	if (!td.transferBase)
		cgi->Com_Error(ERR_DROP, "No bases! Transfer needs at least two...");
	TR_TransferBaseSelect(base, td.transferBase);
	/* Set up cvar used to display transferBase. */
	if (td.transferBase) {
		cgi->Cvar_Set("mn_trans_base_name", "%s", td.transferBase->name);
		cgi->Cvar_SetValue("mn_trans_base_id", td.transferBase->idx);
	} else {
		cgi->Cvar_Set("mn_trans_base_id", "");
	}

	/* Set up cvar used with tabset */
	cgi->Cvar_Set("mn_itemtype", "%s", transferTypeIDs[0]);
	/* Select first available item */
	cgi->Cmd_ExecuteString("ui_trans_fill %s", transferTypeIDs[0]);
}

/**
 * @brief Closes Transfer Menu and resets temp arrays.
 */
static void TR_TransferClose_f (void)
{
	/* Unload what was loaded. */
	TR_TransferListClear_f();
	/* Clear temporary cargo arrays. */
	TR_ClearTempCargo();
}

/**
 * @brief Assembles the list of transfers for the popup
 */
static void TR_List_f (void)
{
	int i = 0;

	cgi->UI_ExecuteConfunc("tr_listclear");
	TR_Foreach(transfer) {
		const char* source = transfer->srcBase ? transfer->srcBase->name : "mission";
		date_t time = Date_Substract(transfer->event, ccs.date);

		cgi->UI_ExecuteConfunc("tr_listaddtransfer %d \"%s\" \"%s\" \"%s\"", ++i, source, transfer->destBase->name, CP_SecondConvert(Date_DateToSeconds(&time)));

		/* Items */
		if (transfer->hasItems) {
			cgi->UI_ExecuteConfunc("tr_listaddcargo %d \"%s\" \"%s\" \"%s\"", i, "tr_cargo", "items", _("Items"));
			for (int j = 0; j < cgi->csi->numODs; j++) {
				const objDef_t* od = INVSH_GetItemByIDX(j);

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
					if (employee->getUGV()) {
						/** @todo: add ugv listing when they're implemented */
					} else {
						cgi->UI_ExecuteConfunc("tr_listaddcargo %d \"%s\" \"%s\" \"%s\"", i, "tr_cargo.employee", va("ucn%i", employee->chr.ucn), va("%s %s", E_GetEmployeeString(employee->getType(), 1), employee->chr.name));
					}
				}
			}
		}
		/* Aliens */
		if (transfer->alienCargo != nullptr) {
			cgi->UI_ExecuteConfunc("tr_listaddcargo %d \"%s\" \"%s\" \"%s\"", i, "tr_cargo", "aliens", _("Aliens"));
			linkedList_t* cargo = transfer->alienCargo->list();
			LIST_Foreach(cargo, alienCargo_t, item) {
				if (item->alive > 0)
					cgi->UI_ExecuteConfunc("tr_listaddcargo %d \"%s\" \"%s\" \"%s\"", i, "tr_cargo.aliens", va("%s_alive", item->teamDef->id), va("%i %s %s", item->alive, _("alive"), _(item->teamDef->name)));
				if (item->dead > 0)
					cgi->UI_ExecuteConfunc("tr_listaddcargo %d \"%s\" \"%s\" \"%s\"", i, "tr_cargo.aliens", va("%s_dead", item->teamDef->id), va("%i %s %s", item->dead, _("dead"), _(item->teamDef->name)));
			}
			cgi->LIST_Delete(&cargo);
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
		const objDef_t* object = INVSH_GetItemByIDX(i);
		const int itemCargoAmount = itemAmountArray[i];

		if (itemCargoAmount <= 0)
			continue;
		if (Q_streq(object->id, ANTIMATTER_ITEM_ID))
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
static void TR_CountEmployeeInListArray (linkedList_t* employeeListArray[], int capacity[])
{
	for (int i = EMPL_SOLDIER; i < EMPL_ROBOT; i++) {
		capacity[CAP_EMPLOYEES] += cgi->LIST_Count(employeeListArray[i]);
	}
}

/**
 * @brief Count capacity need of aircraft in lists
 * @param[in] aircraftList List to count aircraft in
 * @param[in,out] capacity Capacity need array to update
 */
static void TR_CountAircraftInList (linkedList_t* aircraftList, int capacity[])
{
	LIST_Foreach(aircraftList, aircraft_t, aircraft) {
		capacity[AIR_GetCapacityByAircraftWeight(aircraft)]++;
	}
}

/**
 * @brief Callback for assemble destination base capacity list
 * @note called via "ui_trans_caplist <destbasidx>"
 */
static void TR_DestinationCapacityList_f (void)
{
	if (cgi->Cmd_Argc() < 2) {
		Com_Printf("Usage: %s <destinationBaseIdx>\n", cgi->Cmd_Argv(0));
		return;
	}

	base_t* base = B_GetFoundedBaseByIDX(atoi(cgi->Cmd_Argv(1)));
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
		currentCap[CAP_ALIENS] += (transfer->alienCargo) ? transfer->alienCargo->getAlive() : 0;
		/* - Aircraft */
		TR_CountAircraftInList(transfer->aircraft, currentCap);
	}

	/* Count capacity need of the current transfer plan */
	/* - Items and Antimatter */
	TR_CountItemSizeInArray(td.trItemsTmp, currentCap);
	/* - Employee */
	TR_CountEmployeeInListArray(td.trEmployeesTmp, currentCap);
	/* - Aliens */
	currentCap[CAP_ALIENS] += (td.alienCargo) ? td.alienCargo->getAlive() : 0;
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

static const cmdList_t transferCallbacks[] = {
	{"trans_list", TR_List_f, "Assembles the transferlist"},
	{"trans_init", TR_Init_f, "Init function for Transfer menu"},
	{"trans_close", TR_TransferClose_f, "Callback for closing Transfer Menu"},
	{"trans_start", TR_TransferStart_f, "Starts the transfer"},
	{"trans_emptyairstorage", TR_TransferListClear_f, "Unload everything from transfer cargo back to base"},
	{"trans_selectbase", TR_SelectBase_f, "Callback for selecting a base"},
	{"ui_trans_caplist", TR_DestinationCapacityList_f, "Update destination base capacity list"},
	{"ui_trans_fill", TR_Fill_f, "Fill itemlists for transfer"},
	{"ui_trans_add", TR_Add_f, "Add/Remove items to transfercargo"},
	{nullptr, nullptr, nullptr}
};
void TR_InitCallbacks (void)
{
	Cmd_TableAddList(transferCallbacks);
}

void TR_ShutdownCallbacks (void)
{
	TR_ClearTempCargo();

	Cmd_TableRemoveList(transferCallbacks);
}
