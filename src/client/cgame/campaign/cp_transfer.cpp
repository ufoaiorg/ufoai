/**
 * @file
 * @brief Deals with the Transfer stuff.
 * @note Transfer menu functions prefix: TR_
 * @todo Remove direct access to nodes
 */

/*
Copyright (C) 2002-2015 UFO: Alien Invasion.

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

#include "../../cl_shared.h"
#include "cp_campaign.h"
#include "cp_capacity.h"
#include "cp_time.h"
#include "save/save_transfer.h"
#include "cp_transfer_callbacks.h"
#include "aliencargo.h"
#include "aliencontainment.h"

/**
 * @brief Unloads transfer cargo when finishing the transfer or destroys it when no buildings/base.
 * @param[in,out] destination The destination base - might be nullptr in case the base
 * is already destroyed
 * @param[in] transfer Pointer to transfer in ccs.transfers.
 * @param[in] success True if the transfer reaches dest base, false if the base got destroyed.
 * @sa TR_TransferEnd
 */
static void TR_EmptyTransferCargo (base_t* destination, transfer_t* transfer, bool success)
{
	assert(transfer);

	if (transfer->hasItems && success) {	/* Items. */
		const objDef_t* od = INVSH_GetItemByID(ANTIMATTER_ITEM_ID);
		int i;

		/* antimatter */
		if (transfer->itemAmount[od->idx] > 0) {
			if (B_GetBuildingStatus(destination, B_ANTIMATTER)) {
				B_AddAntimatter(destination, transfer->itemAmount[od->idx]);
			} else {
				Com_sprintf(cp_messageBuffer, sizeof(cp_messageBuffer), _("%s does not have Antimatter Storage, antimatter are removed!"), destination->name);
				MSO_CheckAddNewMessage(NT_TRANSFER_LOST, _("Transport mission"), cp_messageBuffer, MSG_TRANSFERFINISHED);
			}
		}
		/* items */
		for (i = 0; i < cgi->csi->numODs; i++) {
			od = INVSH_GetItemByIDX(i);

			if (transfer->itemAmount[od->idx] <= 0)
				continue;
			if (!B_ItemIsStoredInBaseStorage(od))
				continue;
			B_AddToStorage(destination, od, transfer->itemAmount[od->idx]);
		}
	}

	/* Employee */
	if (transfer->hasEmployees && transfer->srcBase) {	/* Employees. (cannot come from a mission) */
		for (int i = EMPL_SOLDIER; i < MAX_EMPL; i++) {
			const employeeType_t type = (employeeType_t)i;
			TR_ForeachEmployee(employee, transfer, type) {
				employee->transfer = false;
				if (!success) {
					E_DeleteEmployee(employee);
					continue;
				}
				switch (type) {
				case EMPL_WORKER:
					PR_UpdateProductionCap(destination, 0);
					break;
				case EMPL_PILOT:
					AIR_AutoAddPilotToAircraft(destination, employee);
					break;
				default:
					break;
				}
			}
		}
	}

	/* Aliens */
	if (transfer->alienCargo != nullptr) {
		if (success) {
			if (!destination->alienContainment) {
				Com_sprintf(cp_messageBuffer, sizeof(cp_messageBuffer), _("%s does not have Alien Containment, Aliens are removed!"), destination->name);
				MSO_CheckAddNewMessage(NT_TRANSFER_LOST, _("Transport mission"), cp_messageBuffer, MSG_TRANSFERFINISHED);
			} else {
				linkedList_t* cargo = transfer->alienCargo->list();
				LIST_Foreach(cargo, alienCargo_t, item) {
					destination->alienContainment->add(item->teamDef, item->alive, item->dead);
				}
				cgi->LIST_Delete(&cargo);
			}
		}
		delete transfer->alienCargo;
		transfer->alienCargo = nullptr;
	}

	TR_ForeachAircraft(aircraft, transfer) {
		if (success) {
			VectorCopy(destination->pos, aircraft->pos);
			aircraft->status = AIR_HOME;
			if (!destination->aircraftCurrent)
				destination->aircraftCurrent = aircraft;
		} else {
			AIR_DeleteAircraft(aircraft);
		}
	}
	cgi->LIST_Delete(&transfer->aircraft);
}

/**
 * @brief Ends the transfer.
 * @param[in] transfer Pointer to transfer in ccs.transfers
 */
static void TR_TransferEnd (transfer_t* transfer)
{
	base_t* destination = transfer->destBase;
	assert(destination);

	if (!destination->founded) {
		TR_EmptyTransferCargo(nullptr, transfer, false);
		MSO_CheckAddNewMessage(NT_TRANSFER_LOST, _("Transport mission"), _("The destination base no longer exists! Transfer cargo was lost, personnel has been discharged."), MSG_TRANSFERFINISHED);
		/** @todo what if source base is lost? we won't be able to unhire transferred employees. */
	} else {
		char message[256];
		TR_EmptyTransferCargo(destination, transfer, true);
		Com_sprintf(message, sizeof(message), _("Transport mission ended, unloading cargo in %s"), destination->name);
		MSO_CheckAddNewMessage(NT_TRANSFER_COMPLETED_SUCCESS, _("Transport mission"), message, MSG_TRANSFERFINISHED);
	}
	cgi->LIST_Remove(&ccs.transfers, transfer);
}

/**
 * @brief Starts a transfer
 * @param[in] srcBase start transfer from this base
 * @param[in] transData Container holds transfer details
 */
transfer_t* TR_TransferStart (base_t* srcBase, transfer_t& transData)
{
	transfer_t transfer;
	float time;
	int i;

	if (!transData.destBase || !srcBase) {
		Com_Printf("TR_TransferStart: No base selected!\n");
		return nullptr;
	}

	/* Initialize transfer. */
	OBJZERO(transfer);
	if (srcBase != nullptr && transData.destBase != nullptr) {
		/* calculate time to go from 1 base to another : 1 day for one quarter of the globe*/
		time = GetDistanceOnGlobe(transData.destBase->pos, srcBase->pos) / 90.0f;
	} else {
		time = DEFAULT_TRANSFER_TIME;
	}
	transfer.event.day = ccs.date.day + floor(time);	/* add day */
	time = (time - floor(time)) * SECONDS_PER_DAY;	/* convert remaining time in second */
	transfer.event.sec = ccs.date.sec + round(time);
	/* check if event is not the following day */
	if (transfer.event.sec > SECONDS_PER_DAY) {
		transfer.event.sec -= SECONDS_PER_DAY;
		transfer.event.day++;
	}
	transfer.destBase = transData.destBase;	/* Destination base. */
	transfer.srcBase = srcBase;	/* Source base. */

	int count = 0;
	for (i = 0; i < cgi->csi->numODs; i++) {	/* Items. */
		if (transData.itemAmount[i] == 0)
			continue;
		if (srcBase != nullptr) {
			const objDef_t* od = INVSH_GetItemByIDX(i);
			if (Q_streq(od->id, ANTIMATTER_ITEM_ID))
				B_AddAntimatter(srcBase, -transData.itemAmount[i]);
			else
				B_AddToStorage(srcBase, od, -transData.itemAmount[i]);
		}
		transfer.hasItems = true;
		transfer.itemAmount[i] = transData.itemAmount[i];
		count++;
	}

	for (i = 0; i < MAX_EMPL; i++) {		/* Employees. */
		LIST_Foreach(transData.employees[i], Employee, employee) {
			if (employee->isAssigned())
				employee->unassign();

			const aircraft_t *aircraft = AIR_IsEmployeeInAircraft(employee, nullptr);
			if (aircraft && cgi->LIST_GetPointer(transData.aircraft, (const void*)aircraft) == nullptr) {
				/* get a non-constant pointer */
				aircraft_t* craft = AIR_AircraftGetFromIDX(aircraft->idx);
				AIR_RemoveEmployee(employee, craft);
			}

			E_MoveIntoNewBase(employee, transfer.destBase);
			employee->transfer = true;
			cgi->LIST_AddPointer(&transfer.employees[i], (void*) employee);
			transfer.hasEmployees = true;
			count++;
		}
	}

	/* Aliens. */
	if (transData.alienCargo != nullptr) {
		transfer.alienCargo = new AlienCargo(*transData.alienCargo);

		linkedList_t* list = transData.alienCargo->list();
		LIST_Foreach(list, alienCargo_t, item) {
			if (srcBase != nullptr && srcBase->alienContainment != nullptr)
				srcBase->alienContainment->add(item->teamDef, -item->alive, -item->dead);
			count += item->alive;
			count += item->dead;
		}
		cgi->LIST_Delete(&list);
	}

	/* Aircraft */
	LIST_Foreach(transData.aircraft, aircraft_t, aircraft) {
		const baseCapacities_t capacity = AIR_GetCapacityByAircraftWeight(aircraft);
		aircraft->status = AIR_TRANSFER;
		aircraft->homebase = transData.destBase;

		/* Remove crew if not transfered */
		if (aircraft->pilot != nullptr && cgi->LIST_GetPointer(transfer.employees[aircraft->pilot->getType()], (void*)aircraft->pilot) == nullptr)
			AIR_RemoveEmployee(aircraft->pilot, aircraft);

		LIST_Foreach(aircraft->acTeam, Employee, employee) {
			if (cgi->LIST_GetPointer(transfer.employees[employee->getType()], (void*)employee) == nullptr)
				AIR_RemoveEmployee(employee, aircraft);
		}

		cgi->LIST_AddPointer(&transfer.aircraft, (void*)aircraft);

		if (srcBase->aircraftCurrent == aircraft)
			srcBase->aircraftCurrent = AIR_GetFirstFromBase(srcBase);
		CAP_AddCurrent(srcBase, capacity, -1);

		/* This should happen in TR_EmptyTransferCargo but on loading capacities are
			calculated based on aircraft->homebase. aircraft->homebase cannot be null yet
			there are hidden tests on that. */
		CAP_AddCurrent(transData.destBase, capacity, 1);

		count++;
	}

	/* don't start empty transfer */
	if (count == 0)
		return nullptr;

	/* Recheck if production/research can be done on srcbase (if there are workers/scientists) */
	PR_ProductionAllowed(srcBase);
	RS_ResearchAllowed(srcBase);

	return &LIST_Add(&ccs.transfers, transfer);
}

/**
 * @brief Notify that an aircraft has been removed.
 * @param[in] aircraft Aircraft that was removed from the game
 * @sa AIR_DeleteAircraft
 */
void TR_NotifyAircraftRemoved (const aircraft_t* aircraft)
{
	if (!aircraft)
		return;

	TR_Foreach(transfer) {
		if (cgi->LIST_Remove(&transfer->aircraft, aircraft))
			return;
	}
}

/**
 * @brief Checks whether given transfer should be processed.
 * @sa CP_CampaignRun
 */
void TR_TransferRun (void)
{
	TR_Foreach(transfer) {
		if (Date_IsDue(&transfer->event)) {
			assert(transfer->destBase);
			TR_TransferEnd(transfer);
			return;
		}
	}
}

#ifdef DEBUG
/**
 * @brief Lists an/all active transfer(s)
 */
static void TR_ListTransfers_f (void)
{
	int transIdx = -1;
	int i = 0;

	if (cgi->Cmd_Argc() == 2) {
		transIdx = atoi(cgi->Cmd_Argv(1));
		if (transIdx < 0 || transIdx > cgi->LIST_Count(ccs.transfers)) {
			Com_Printf("Usage: %s [transferIDX]\nWithout parameter it lists all.\n", cgi->Cmd_Argv(0));
			return;
		}
	}

	TR_Foreach(transfer) {
		dateLong_t date;
		i++;

		if (transIdx >= 0 && i != transIdx)
			continue;

		/* @todo: we need a strftime feature to make this easier */
		CP_DateConvertLong(&transfer->event, &date);

		Com_Printf("Transfer #%d\n", i);
		Com_Printf("...From %d (%s) To %d (%s) Arrival: %04i-%02i-%02i %02i:%02i:%02i\n",
			(transfer->srcBase) ? transfer->srcBase->idx : -1,
			(transfer->srcBase) ? transfer->srcBase->name : "(null)",
			(transfer->destBase) ? transfer->destBase->idx : -1,
			(transfer->destBase) ? transfer->destBase->name : "(null)",
			date.year, date.month, date.day, date.hour, date.min, date.sec);

		/* ItemCargo */
		if (transfer->hasItems) {
			Com_Printf("...ItemCargo:\n");
			for (int j = 0; j < cgi->csi->numODs; j++) {
				const objDef_t* od = INVSH_GetItemByIDX(j);
				if (transfer->itemAmount[od->idx])
					Com_Printf("......%s: %i\n", od->id, transfer->itemAmount[od->idx]);
			}
		}
		/* Carried Employees */
		if (transfer->hasEmployees) {
			int i;

			Com_Printf("...Carried Employee:\n");
			for (i = EMPL_SOLDIER; i < MAX_EMPL; i++) {
				const employeeType_t emplType = (employeeType_t)i;
				TR_ForeachEmployee(employee, transfer, emplType) {
					if (employee->getUGV()) {
						/** @todo: improve ugv listing when they're implemented */
						Com_Printf("......ugv: %s [ucn: %i]\n", employee->getUGV()->id, employee->chr.ucn);
					} else {
						Com_Printf("......%s (%s) / %s [ucn: %i]\n", employee->chr.name,
							E_GetEmployeeString(employee->getType(), 1),
							(employee->getNation()) ? employee->getNation()->id : "(nonation)",
							employee->chr.ucn);
						if (!employee->isHired())
							Com_Printf("Warning: employee^ not hired!\n");
						if (!employee->transfer)
							Com_Printf("Warning: employee^ not marked as being transferred!\n");
					}
				}
			}
		}
		/* AlienCargo */
		if (transfer->alienCargo != nullptr) {
			Com_Printf("...AlienCargo:\n");
			linkedList_t* cargo = transfer->alienCargo->list();
			LIST_Foreach(cargo, alienCargo_t, item) {
				Com_Printf("......%s alive: %i dead: %i\n", item->teamDef->id, item->alive, item->dead);
			}
			cgi->LIST_Delete(&cargo);
		}
		/* Transfered Aircraft */
		if (!cgi->LIST_IsEmpty(transfer->aircraft)) {
			Com_Printf("...Transfered Aircraft:\n");
			TR_ForeachAircraft(aircraft, transfer) {
				Com_Printf("......%s [idx: %i]\n", aircraft->id, aircraft->idx);
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
bool TR_SaveXML (xmlNode_t* p)
{
	xmlNode_t* n = cgi->XML_AddNode(p, SAVE_TRANSFER_TRANSFERS);

	TR_Foreach(transfer) {
		int j;
		xmlNode_t* s;

		s = cgi->XML_AddNode(n, SAVE_TRANSFER_TRANSFER);
		cgi->XML_AddInt(s, SAVE_TRANSFER_DAY, transfer->event.day);
		cgi->XML_AddInt(s, SAVE_TRANSFER_SEC, transfer->event.sec);
		if (!transfer->destBase) {
			Com_Printf("Could not save transfer, no destBase is set\n");
			return false;
		}
		cgi->XML_AddInt(s, SAVE_TRANSFER_DESTBASE, transfer->destBase->idx);
		/* scrBase can be nullptr if this is alien (mission->base) transport
		 * @sa TR_TransferAlienAfterMissionStart */
		if (transfer->srcBase)
			cgi->XML_AddInt(s, SAVE_TRANSFER_SRCBASE, transfer->srcBase->idx);
		/* save items */
		if (transfer->hasItems) {
			for (j = 0; j < MAX_OBJDEFS; j++) {
				if (transfer->itemAmount[j] > 0) {
					const objDef_t* od = INVSH_GetItemByIDX(j);
					xmlNode_t* ss = cgi->XML_AddNode(s, SAVE_TRANSFER_ITEM);

					assert(od);
					cgi->XML_AddString(ss, SAVE_TRANSFER_ITEMID, od->id);
					cgi->XML_AddInt(ss, SAVE_TRANSFER_AMOUNT, transfer->itemAmount[j]);
				}
			}
		}
		/* save aliens */
		if (transfer->alienCargo != nullptr) {
			xmlNode_t* alienNode = cgi->XML_AddNode(s, SAVE_TRANSFER_ALIENCARGO);
			if (!alienNode)
				return false;
			transfer->alienCargo->save(alienNode);
		}
		/* save employee */
		if (transfer->hasEmployees) {
			for (j = 0; j < MAX_EMPL; j++) {
				TR_ForeachEmployee(employee, transfer, j) {
					xmlNode_t* ss = cgi->XML_AddNode(s, SAVE_TRANSFER_EMPLOYEE);
					cgi->XML_AddInt(ss, SAVE_TRANSFER_UCN, employee->chr.ucn);
				}
			}
		}
		/* save aircraft */
		TR_ForeachAircraft(aircraft, transfer) {
			xmlNode_t* ss = cgi->XML_AddNode(s, SAVE_TRANSFER_AIRCRAFT);
			cgi->XML_AddInt(ss, SAVE_TRANSFER_ID, aircraft->idx);
		}
	}
	return true;
}

/**
 * @brief Load callback for xml savegames
 * @param[in] p XML Node structure, where we get the information from
 * @sa TR_SaveXML
 * @sa SAV_GameLoadXML
 */
bool TR_LoadXML (xmlNode_t* p)
{
	xmlNode_t* n, *s;

	n = cgi->XML_GetNode(p, SAVE_TRANSFER_TRANSFERS);
	if (!n)
		return false;

	assert(B_AtLeastOneExists());

	for (s = cgi->XML_GetNode(n, SAVE_TRANSFER_TRANSFER); s; s = cgi->XML_GetNextNode(s, n, SAVE_TRANSFER_TRANSFER)) {
		xmlNode_t* ss;
		transfer_t transfer;

		OBJZERO(transfer);

		transfer.destBase = B_GetBaseByIDX(cgi->XML_GetInt(s, SAVE_TRANSFER_DESTBASE, BYTES_NONE));
		if (!transfer.destBase) {
			Com_Printf("Error: Transfer has no destBase set\n");
			return false;
		}
		transfer.srcBase = B_GetBaseByIDX(cgi->XML_GetInt(s, SAVE_TRANSFER_SRCBASE, BYTES_NONE));

		transfer.event.day = cgi->XML_GetInt(s, SAVE_TRANSFER_DAY, 0);
		transfer.event.sec = cgi->XML_GetInt(s, SAVE_TRANSFER_SEC, 0);

		/* Initializing some variables */
		transfer.hasItems = false;
		transfer.hasEmployees = false;

		/* load items */
		/* If there is at last one element, hasItems is true */
		ss = cgi->XML_GetNode(s, SAVE_TRANSFER_ITEM);
		if (ss) {
			transfer.hasItems = true;
			for (; ss; ss = cgi->XML_GetNextNode(ss, s, SAVE_TRANSFER_ITEM)) {
				const char* itemId = cgi->XML_GetString(ss, SAVE_TRANSFER_ITEMID);
				const objDef_t* od = INVSH_GetItemByID(itemId);

				if (od)
					transfer.itemAmount[od->idx] = cgi->XML_GetInt(ss, SAVE_TRANSFER_AMOUNT, 1);
			}
		}
		/* load aliens */
		ss = cgi->XML_GetNode(s, SAVE_TRANSFER_ALIENCARGO);
		if (ss) {
			transfer.alienCargo = new AlienCargo();
			if (transfer.alienCargo == nullptr)
				cgi->Com_Error(ERR_DROP, "TR_LoadXML: Cannot create AlienCargo object\n");
			transfer.alienCargo->load(ss);
		} else {
			/** @todo Remove: Fallback for compatibility */
			ss = cgi->XML_GetNode(s, SAVE_TRANSFER_ALIEN);
			if (ss) {
				transfer.alienCargo = new AlienCargo();
				if (transfer.alienCargo == nullptr)
					cgi->Com_Error(ERR_DROP, "TR_LoadXML: Cannot create AlienCargo object\n");
				for (; ss; ss = cgi->XML_GetNextNode(ss, s, SAVE_TRANSFER_ALIEN)) {
					const int alive = cgi->XML_GetInt(ss, SAVE_TRANSFER_ALIVEAMOUNT, 0);
					const int dead  = cgi->XML_GetInt(ss, SAVE_TRANSFER_DEADAMOUNT, 0);
					const char* id = cgi->XML_GetString(ss, SAVE_TRANSFER_ALIENID);
					if (!transfer.alienCargo->add(id, alive, dead))
						Com_Printf("TR_LoadXML: Cannot add aliens to cargo: %s, alive: %d, dead: %d\n", id, alive, dead);
				}
			}
		}
		/* load employee */
		ss = cgi->XML_GetNode(s, SAVE_TRANSFER_EMPLOYEE);
		if (ss) {
			transfer.hasEmployees = true;
			for (; ss; ss = cgi->XML_GetNextNode(ss, s, SAVE_TRANSFER_EMPLOYEE)) {
				const int ucn = cgi->XML_GetInt(ss, SAVE_TRANSFER_UCN, -1);
				Employee* empl = E_GetEmployeeFromChrUCN(ucn);

				if (!empl) {
					Com_Printf("Error: No employee found with UCN: %i\n", ucn);
					return false;
				}

				cgi->LIST_AddPointer(&transfer.employees[empl->getType()], (void*) empl);
				empl->transfer = true;
			}
		}
		/* load aircraft */
		for (ss = cgi->XML_GetNode(s, SAVE_TRANSFER_AIRCRAFT); ss; ss = cgi->XML_GetNextNode(ss, s, SAVE_TRANSFER_AIRCRAFT)) {
			const int j = cgi->XML_GetInt(ss, SAVE_TRANSFER_ID, -1);
			aircraft_t* aircraft = AIR_AircraftGetFromIDX(j);

			if (aircraft)
				cgi->LIST_AddPointer(&transfer.aircraft, (void*)aircraft);
		}
		LIST_Add(&ccs.transfers, transfer);
	}

	return true;
}

/**
 * @brief Defines commands and cvars for the Transfer menu(s).
 * @sa UI_InitStartup
 */
void TR_InitStartup (void)
{
	TR_InitCallbacks();
#ifdef DEBUG
	cgi->Cmd_AddCommand("debug_listtransfers", TR_ListTransfers_f, "Lists an/all active transfer(s)");
#endif
}

/**
 * @brief Closing actions for transfer-subsystem
 */
void TR_Shutdown (void)
{
	TR_Foreach(transfer) {
		if (transfer->alienCargo != nullptr) {
			delete transfer->alienCargo;
			transfer->alienCargo = nullptr;
		}
		cgi->LIST_Delete(&transfer->aircraft);
		for (int i = EMPL_SOLDIER; i < MAX_EMPL; i++) {
			cgi->LIST_Delete(&transfer->employees[i]);
		}
	}
	cgi->LIST_Delete(&ccs.transfers);

	TR_ShutdownCallbacks();
#ifdef DEBUG
	cgi->Cmd_RemoveCommand("debug_listtransfers");
#endif
}
