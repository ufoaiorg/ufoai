/**
 * @file cp_transfer.c
 * @brief Deals with the Transfer stuff.
 * @note Transfer menu functions prefix: TR_
 * @todo Remove direct access to nodes
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

#include "../../cl_shared.h"
#include "cp_campaign.h"
#include "cp_capacity.h"
#include "cp_time.h"
#include "save/save_transfer.h"
#include "cp_transfer_callbacks.h"
#include "../../ui/ui_main.h"

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
	assert(transfer);

	if (transfer->hasItems && success) {	/* Items. */
		const objDef_t *od = INVSH_GetItemByID(ANTIMATTER_TECH_ID);
		int i;

		/* antimatter */
		if (transfer->itemAmount[od->idx] > 0) {
			if (B_GetBuildingStatus(destination, B_ANTIMATTER)) {
				B_ManageAntimatter(destination, transfer->itemAmount[od->idx], qtrue);
			} else {
				Com_sprintf(cp_messageBuffer, sizeof(cp_messageBuffer), _("%s does not have Antimatter Storage, antimatter are removed!"), destination->name);
				MSO_CheckAddNewMessage(NT_TRANSFER_LOST, _("Transport mission"), cp_messageBuffer, qfalse, MSG_TRANSFERFINISHED, NULL);
			}
		}
		/* items */
		for (i = 0; i < csi.numODs; i++) {
			od = INVSH_GetItemByIDX(i);

			if (transfer->itemAmount[od->idx] <= 0)
				continue;
			if (!B_ItemIsStoredInBaseStorage(od))
				continue;
			if (!B_GetBuildingStatus(destination, B_STORAGE)) {
				Com_sprintf(cp_messageBuffer, sizeof(cp_messageBuffer), _("%s does not have Storage, items are removed!"), destination->name);
				MSO_CheckAddNewMessage(NT_TRANSFER_LOST, _("Transport mission"), cp_messageBuffer, qfalse, MSG_TRANSFERFINISHED, NULL);
				break;
			}
			B_UpdateStorageAndCapacity(destination, od, transfer->itemAmount[od->idx], qtrue);
		}
	}

	if (transfer->hasEmployees && transfer->srcBase) {	/* Employees. (cannot come from a mission) */
		if (!success || !B_GetBuildingStatus(destination, B_QUARTERS)) {	/* Employees will be unhired. */
			employeeType_t i;
			if (success) {
				Com_sprintf(cp_messageBuffer, sizeof(cp_messageBuffer), _("%s does not have Living Quarters, employees got unhired!"), destination->name);
				MSO_CheckAddNewMessage(NT_TRANSFER_LOST, _("Transport mission"), cp_messageBuffer, qfalse, MSG_TRANSFERFINISHED, NULL);
			}
			for (i = EMPL_SOLDIER; i < MAX_EMPL; i++) {
				TR_ForeachEmployee(employee, transfer, i) {
					employee->baseHired = transfer->srcBase;	/* Restore back the original baseid. */
					employee->transfer = qfalse;
					E_UnhireEmployee(employee);
				}
			}
		} else {
			employeeType_t i;
			for (i = EMPL_SOLDIER; i < MAX_EMPL; i++) {
				TR_ForeachEmployee(employee, transfer, i) {
					employee->baseHired = transfer->srcBase;	/* Restore back the original baseid. */
					employee->transfer = qfalse;
					E_UnhireEmployee(employee);
					E_HireEmployee(destination, employee);
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
			int i;
			qboolean capacityWarning = qfalse;

			for (i = 0; i < ccs.numAliensTD; i++) {
				/* dead aliens */
				if (transfer->alienAmount[i][TRANS_ALIEN_DEAD] > 0) {
					destination->alienscont[i].amountDead += transfer->alienAmount[i][TRANS_ALIEN_DEAD];
				}
				/* alive aliens */
				if (transfer->alienAmount[i][TRANS_ALIEN_ALIVE] > 0) {
					int amount = min(transfer->alienAmount[i][TRANS_ALIEN_ALIVE], CAP_GetFreeCapacity(destination, CAP_ALIENS));

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
		TR_ForeachAircraft(aircraft, transfer) {
			if ((AIR_CalculateHangarStorage(aircraft->tpl, destination, 0) <= 0) || !AIR_MoveAircraftIntoNewHomebase(aircraft, destination)) {
				/* No space, aircraft will be lost. */
				Com_sprintf(cp_messageBuffer, sizeof(cp_messageBuffer), _("%s does not have enough free space. Aircraft is lost!"), destination->name);
				MSO_CheckAddNewMessage(NT_TRANSFER_LOST, _("Transport mission"), cp_messageBuffer, qfalse, MSG_TRANSFERFINISHED, NULL);
				AIR_DeleteAircraft(aircraft);
			}
		}
		LIST_Delete(&transfer->aircraft);
	}
}

/**
 * @brief Starts alien bodies transfer between mission and base.
 * @param[in] base Pointer to the base to send the alien bodies.
 * @param[in, out] transferAircraft Pointer to the dropship held bodies (temporary)
 * @sa TR_TransferBaseListClick_f
 * @sa TR_TransferStart_f
 */
void TR_TransferAlienAfterMissionStart (const base_t *base, aircraft_t *transferAircraft)
{
	int i, j;
	transfer_t transfer;
	float time;
	char message[256];
	int alienCargoTypes;
	aliensTmp_t *cargo;
	const technology_t *breathingTech;
	qboolean alienBreathing = qfalse;

	if (!base) {
		Com_Printf("TR_TransferAlienAfterMissionStart_f: No base selected!\n");
		return;
	}

	breathingTech = RS_GetTechByID(BREATHINGAPPARATUS_TECH);
	if (!breathingTech)
		Com_Error(ERR_DROP, "AL_AddAliens: Could not get breathing apparatus tech definition");
	alienBreathing = RS_IsResearched_ptr(breathingTech);

	OBJZERO(transfer);

	/* Initialize transfer.
	 * calculate time to go from 1 base to another : 1 day for one quarter of the globe*/
	time = GetDistanceOnGlobe(base->pos, transferAircraft->pos) / 90.0f;
	transfer.event.day = ccs.date.day + floor(time);	/* add day */
	time = (time - floor(time)) * SECONDS_PER_DAY;	/* convert remaining time in second */
	transfer.event.sec = ccs.date.sec + round(time);
	/* check if event is not the following day */
	if (transfer.event.sec > SECONDS_PER_DAY) {
		transfer.event.sec -= SECONDS_PER_DAY;
		transfer.event.day++;
	}
	transfer.destBase = B_GetFoundedBaseByIDX(base->idx);	/* Destination base. */
	transfer.srcBase = NULL;	/* Source base. */

	alienCargoTypes = AL_GetAircraftAlienCargoTypes(transferAircraft);
	cargo = AL_GetAircraftAlienCargo(transferAircraft);
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
					transfer.hasAliens = qtrue;
					transfer.alienAmount[j][TRANS_ALIEN_ALIVE] = cargo->amountAlive;
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
					transfer.hasAliens = qtrue;
					transfer.alienAmount[j][TRANS_ALIEN_DEAD] = cargo->amountDead;
					cargo->amountDead = 0;
					break;
				}
			}
		}
	}
	AL_SetAircraftAlienCargoTypes(transferAircraft, 0);

	Com_sprintf(message, sizeof(message), _("Transport mission started, cargo is being transported to %s"), transfer.destBase->name);
	MSO_CheckAddNewMessage(NT_TRANSFER_ALIENBODIES_DEFERED, _("Transport mission"), message, qfalse, MSG_TRANSFERFINISHED, NULL);
	UI_PopWindow(qfalse);

	LIST_Add(&ccs.transfers, (const byte*) &transfer, sizeof(transfer));
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
	LIST_Remove(&ccs.transfers, transfer);
}

qboolean TR_AddData (transferData_t *transferData, transferCargoType_t type, const void* data)
{
	if (transferData->trCargoCountTmp >= MAX_CARGO)
		return qfalse;
	TR_SetData(&transferData->cargo[transferData->trCargoCountTmp], type, data);
	transferData->trCargoCountTmp++;

	return qtrue;
}

/**
 * @brief Starts a transfer
 * @param[in] srcBase start transfer from this base
 * @param[in] transData Container holds transfer details
 */
transfer_t* TR_TransferStart (base_t *srcBase, transferData_t *transData)
{
	transfer_t transfer;
	float time;
	int i;

	if (!transData->transferBase || !srcBase) {
		Com_Printf("TR_TransferStart: No base selected!\n");
		return NULL;
	}

	/* don't start any empty transport */
	if (!transData->trCargoCountTmp) {
		return NULL;
	}

	OBJZERO(transfer);

	/* Initialize transfer. */
	/* calculate time to go from 1 base to another : 1 day for one quarter of the globe*/
	time = GetDistanceOnGlobe(transData->transferBase->pos, srcBase->pos) / 90.0f;
	transfer.event.day = ccs.date.day + floor(time);	/* add day */
	time = (time - floor(time)) * SECONDS_PER_DAY;	/* convert remaining time in second */
	transfer.event.sec = ccs.date.sec + round(time);
	/* check if event is not the following day */
	if (transfer.event.sec > SECONDS_PER_DAY) {
		transfer.event.sec -= SECONDS_PER_DAY;
		transfer.event.day++;
	}
	transfer.destBase = transData->transferBase;	/* Destination base. */
	assert(transfer.destBase);
	transfer.srcBase = srcBase;	/* Source base. */

	for (i = 0; i < csi.numODs; i++) {	/* Items. */
		if (transData->trItemsTmp[i] > 0) {
			transfer.hasItems = qtrue;
			transfer.itemAmount[i] = transData->trItemsTmp[i];
		}
	}
	/* Note that the employee remains hired in source base during the transfer, that is
	 * it takes Living Quarters capacity, etc, but it cannot be used anywhere. */
	for (i = 0; i < MAX_EMPL; i++) {		/* Employees. */
		LIST_Foreach(transData->trEmployeesTmp[i], employee_t, employee) {
			assert(E_IsInBase(employee, srcBase));

			transfer.hasEmployees = qtrue;
			E_ResetEmployee(employee);
			LIST_AddPointer(&transfer.employees[i], (void*) employee);
			employee->baseHired = NULL;
			employee->transfer = qtrue;
		}
	}
	for (i = 0; i < ccs.numAliensTD; i++) {		/* Aliens. */
		if (transData->trAliensTmp[i][TRANS_ALIEN_ALIVE] > 0) {
			transfer.hasAliens = qtrue;
			transfer.alienAmount[i][TRANS_ALIEN_ALIVE] = transData->trAliensTmp[i][TRANS_ALIEN_ALIVE];
		}
		if (transData->trAliensTmp[i][TRANS_ALIEN_DEAD] > 0) {
			transfer.hasAliens = qtrue;
			transfer.alienAmount[i][TRANS_ALIEN_DEAD] = transData->trAliensTmp[i][TRANS_ALIEN_DEAD];
		}
	}

	LIST_Foreach(transData->aircraft, aircraft_t, aircraft) {
		aircraft->status = AIR_TRANSFER;
		AIR_RemoveEmployees(aircraft);
		transfer.hasAircraft = qtrue;
		LIST_AddPointer(&transfer.aircraft, (void*)aircraft);
	}

	/* Recheck if production/research can be done on srcbase (if there are workers/scientists) */
	PR_ProductionAllowed(srcBase);
	RS_ResearchAllowed(srcBase);

	return (transfer_t*) LIST_Add(&ccs.transfers, (const byte*) &transfer, sizeof(transfer))->data;
}

/**
 * @brief Notify that an aircraft has been removed.
 * @param[in] aircraft Aircraft that was removed from the game
 * @sa AIR_DeleteAircraft
 */
void TR_NotifyAircraftRemoved (const aircraft_t *aircraft)
{
	if (!aircraft)
		return;

	TR_Foreach(transfer) {
		if (!transfer->hasAircraft)
			continue;
		if (LIST_Remove(&transfer->aircraft, aircraft))
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

	if (Cmd_Argc() == 2) {
		transIdx = atoi(Cmd_Argv(1));
		if (transIdx < 0 || transIdx > LIST_Count(ccs.transfers)) {
			Com_Printf("Usage: %s [transferIDX]\nWithout parameter it lists all.\n", Cmd_Argv(0));
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
			int j;
			Com_Printf("...ItemCargo:\n");
			for (j = 0; j < csi.numODs; j++) {
				const objDef_t *od = INVSH_GetItemByIDX(j);
				if (transfer->itemAmount[od->idx])
					Com_Printf("......%s: %i\n", od->id, transfer->itemAmount[od->idx]);
			}
		}
		/* Carried Employees */
		if (transfer->hasEmployees) {
			employeeType_t emplType;

			Com_Printf("...Carried Employee:\n");
			for (emplType = EMPL_SOLDIER; emplType < MAX_EMPL; emplType++) {
				TR_ForeachEmployee(employee, transfer, emplType) {
					Com_Printf("......%s (%s) / %s [ucn: %i]\n", employee->chr.name,
						E_GetEmployeeString(employee->type, 1),
						(employee->nation) ? employee->nation->id : "(nonation)",
						employee->chr.ucn);
					if (!E_IsHired(employee))
						Com_Printf("Warning: employee^ not hired!\n");
					if (!employee->transfer)
						Com_Printf("Warning: employee^ not marked as being transfered!\n");
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
qboolean TR_SaveXML (xmlNode_t *p)
{
	xmlNode_t *n = XML_AddNode(p, SAVE_TRANSFER_TRANSFERS);

	TR_Foreach(transfer) {
		int j;
		xmlNode_t *s;

		s = XML_AddNode(n, SAVE_TRANSFER_TRANSFER);
		XML_AddInt(s, SAVE_TRANSFER_DAY, transfer->event.day);
		XML_AddInt(s, SAVE_TRANSFER_SEC, transfer->event.sec);
		if (!transfer->destBase) {
			Com_Printf("Could not save transfer, no destBase is set\n");
			return qfalse;
		}
		XML_AddInt(s, SAVE_TRANSFER_DESTBASE, transfer->destBase->idx);
		/* scrBase can be NULL if this is alien (mission->base) transport
		 * @sa TR_TransferAlienAfterMissionStart */
		if (transfer->srcBase)
			XML_AddInt(s, SAVE_TRANSFER_SRCBASE, transfer->srcBase->idx);
		/* save items */
		if (transfer->hasItems) {
			for (j = 0; j < MAX_OBJDEFS; j++) {
				if (transfer->itemAmount[j] > 0) {
					const objDef_t *od = INVSH_GetItemByIDX(j);
					xmlNode_t *ss = XML_AddNode(s, SAVE_TRANSFER_ITEM);

					assert(od);
					XML_AddString(ss, SAVE_TRANSFER_ITEMID, od->id);
					XML_AddInt(ss, SAVE_TRANSFER_AMOUNT, transfer->itemAmount[j]);
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
					xmlNode_t *ss = XML_AddNode(s, SAVE_TRANSFER_ALIEN);

					assert(team);
					XML_AddString(ss, SAVE_TRANSFER_ALIENID, team->id);
					XML_AddIntValue(ss, SAVE_TRANSFER_ALIVEAMOUNT, transfer->alienAmount[j][TRANS_ALIEN_ALIVE]);
					XML_AddIntValue(ss, SAVE_TRANSFER_DEADAMOUNT, transfer->alienAmount[j][TRANS_ALIEN_DEAD]);
				}
			}
		}
		/* save employee */
		if (transfer->hasEmployees) {
			for (j = 0; j < MAX_EMPL; j++) {
				TR_ForeachEmployee(employee, transfer, j) {
					xmlNode_t *ss = XML_AddNode(s, SAVE_TRANSFER_EMPLOYEE);
					XML_AddInt(ss, SAVE_TRANSFER_UCN, employee->chr.ucn);
				}
			}
		}
		/* save aircraft */
		if (transfer->hasAircraft) {
			TR_ForeachAircraft(aircraft, transfer) {
				xmlNode_t *ss = XML_AddNode(s, SAVE_TRANSFER_AIRCRAFT);
				XML_AddInt(ss, SAVE_TRANSFER_ID, aircraft->idx);
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
qboolean TR_LoadXML (xmlNode_t *p)
{
	xmlNode_t *n, *s;

	n = XML_GetNode(p, SAVE_TRANSFER_TRANSFERS);
	if (!n)
		return qfalse;

	assert(B_AtLeastOneExists());

	for (s = XML_GetNode(n, SAVE_TRANSFER_TRANSFER); s; s = XML_GetNextNode(s, n, SAVE_TRANSFER_TRANSFER)) {
		xmlNode_t *ss;
		transfer_t transfer;

		OBJZERO(transfer);

		transfer.destBase = B_GetBaseByIDX(XML_GetInt(s, SAVE_TRANSFER_DESTBASE, BYTES_NONE));
		if (!transfer.destBase) {
			Com_Printf("Error: Transfer has no destBase set\n");
			return qfalse;
		}
		transfer.srcBase = B_GetBaseByIDX(XML_GetInt(s, SAVE_TRANSFER_SRCBASE, BYTES_NONE));

		transfer.event.day = XML_GetInt(s, SAVE_TRANSFER_DAY, 0);
		transfer.event.sec = XML_GetInt(s, SAVE_TRANSFER_SEC, 0);

		/* Initializing some variables */
		transfer.hasItems = qfalse;
		transfer.hasEmployees = qfalse;
		transfer.hasAliens = qfalse;
		transfer.hasAircraft = qfalse;

		/* load items */
		/* If there is at last one element, hasItems is true */
		ss = XML_GetNode(s, SAVE_TRANSFER_ITEM);
		if (ss) {
			transfer.hasItems = qtrue;
			for (; ss; ss = XML_GetNextNode(ss, s, SAVE_TRANSFER_ITEM)) {
				const char *itemId = XML_GetString(ss, SAVE_TRANSFER_ITEMID);
				const objDef_t *od = INVSH_GetItemByID(itemId);

				if (od)
					transfer.itemAmount[od->idx] = XML_GetInt(ss, SAVE_TRANSFER_AMOUNT, 1);
			}
		}
		/* load aliens */
		ss = XML_GetNode(s, SAVE_TRANSFER_ALIEN);
		if (ss) {
			transfer.hasAliens = qtrue;
			for (; ss; ss = XML_GetNextNode(ss, s, SAVE_TRANSFER_ALIEN)) {
				const int alive = XML_GetInt(ss, SAVE_TRANSFER_ALIVEAMOUNT, 0);
				const int dead  = XML_GetInt(ss, SAVE_TRANSFER_DEADAMOUNT, 0);
				const char *id = XML_GetString(ss, SAVE_TRANSFER_ALIENID);
				int j;

				/* look for alien teamDef */
				for (j = 0; j < ccs.numAliensTD; j++) {
					if (ccs.alienTeams[j] && Q_streq(id, ccs.alienTeams[j]->id))
						break;
				}

				if (j < ccs.numAliensTD) {
					transfer.alienAmount[j][TRANS_ALIEN_ALIVE] = alive;
					transfer.alienAmount[j][TRANS_ALIEN_DEAD] = dead;
				} else {
					Com_Printf("TR_LoadXML: AlienId '%s' is invalid\n", id);
				}
			}
		}
		/* load employee */
		ss = XML_GetNode(s, SAVE_TRANSFER_EMPLOYEE);
		if (ss) {
			transfer.hasEmployees = qtrue;
			for (; ss; ss = XML_GetNextNode(ss, s, SAVE_TRANSFER_EMPLOYEE)) {
				const int ucn = XML_GetInt(ss, SAVE_TRANSFER_UCN, -1);
				employee_t *empl = E_GetEmployeeFromChrUCN(ucn);

				if (!empl) {
					Com_Printf("Error: No employee found with UCN: %i\n", ucn);
					return qfalse;
				}

				LIST_AddPointer(&transfer.employees[empl->type], (void*) empl);
				empl->transfer = qtrue;
			}
		}
		/* load aircraft */
		ss = XML_GetNode(s, SAVE_TRANSFER_AIRCRAFT);
		if (ss) {
			transfer.hasAircraft = qtrue;
			for (; ss; ss = XML_GetNextNode(ss, s, SAVE_TRANSFER_AIRCRAFT)) {
				const int j = XML_GetInt(ss, SAVE_TRANSFER_ID, -1);
				aircraft_t *aircraft = AIR_AircraftGetFromIDX(j);

				if (aircraft)
					LIST_AddPointer(&transfer.aircraft, (void*)aircraft);
			}
		}
		LIST_Add(&ccs.transfers, (const byte*) &transfer, sizeof(transfer));
	}

	return qtrue;
}

/**
 * @brief Defines commands and cvars for the Transfer menu(s).
 * @sa UI_InitStartup
 */
void TR_InitStartup (void)
{
	TR_InitCallbacks();
#ifdef DEBUG
	Cmd_AddCommand("debug_listtransfers", TR_ListTransfers_f, "Lists an/all active transfer(s)");
#endif
}

/**
 * @brief Closing actions for transfer-subsystem
 */
void TR_Shutdown (void)
{
	TR_Foreach(transfer) {
		employeeType_t emplType;

		LIST_Delete(&transfer->aircraft);

		for (emplType = EMPL_SOLDIER; emplType < MAX_EMPL; emplType++)
			LIST_Delete(&transfer->employees[emplType]);
	}

	TR_ShutdownCallbacks();
#ifdef DEBUG
	Cmd_RemoveCommand("debug_listtransfers");
#endif
}
