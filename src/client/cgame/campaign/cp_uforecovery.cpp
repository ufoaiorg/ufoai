/**
 * @file
 * @brief UFO recovery and storing
 * @note UFO recovery functions with UR_*
 * @note UFO storing functions with US_*
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
#include "cp_ufo.h"
#include "cp_geoscape.h"
#include "cp_time.h"
#include "cp_uforecovery.h"
#include "cp_uforecovery_callbacks.h"
#include "cp_aircraft.h"
#include "save/save_uforecovery.h"

/*==================================
Backend functions
==================================*/

/**
 * @brief Function to process active recoveries.
 * @sa CP_CampaignRun
 */
void UR_ProcessActive (void)
{
	US_Foreach(ufo) {
		assert(ufo->ufoTemplate);
		assert(ufo->ufoTemplate->tech);

		if (ufo->status == SUFO_STORED)
			continue;
		if (Date_LaterThan(&ufo->arrive, &ccs.date))
			continue;

		Com_sprintf(cp_messageBuffer, sizeof(cp_messageBuffer), _("%s was transferred to %s."), UFO_TypeToName(ufo->ufoTemplate->getUfoType()), ufo->installation->name);
		switch (ufo->status) {
		case SUFO_RECOVERED:
			MSO_CheckAddNewMessage(NT_TRANSFER_UFORECOVERY_FINISHED, _("UFO Recovered"), cp_messageBuffer, MSG_TRANSFERFINISHED);
			break;
		case SUFO_TRANSFERED:
			MSO_CheckAddNewMessage(NT_TRANSFER_UFORECOVERY_FINISHED, _("UFO Transfered"), cp_messageBuffer, MSG_TRANSFERFINISHED);
			break;
		default:
			break;
		}
		ufo->status = SUFO_STORED;

		if (!ufo->ufoTemplate->tech->statusCollected)
			RS_MarkCollected(ufo->ufoTemplate->tech);
	}
}

/* ==== UFO Storing stuff ==== */

/**
 * @brief Returns a stored ufo
 * @param[in] idx index of the stored UFO
 * @return storedUFO_t Pointer
 */
storedUFO_t* US_GetStoredUFOByIDX (const int idx)
{
	US_Foreach(ufo) {
		if (ufo->idx == idx)
			return ufo;
	}
	return nullptr;
}

/**
 * @brief Adds an UFO to the storage
 * @param[in] ufoTemplate Pointer to the aircraft(ufo)Template to add
 * @param[in,out] installation Pointer to the installation it should be added to
 * @param[in] date Date when UFO is arrives to the storage (recovery || transfer)
 * @param[in] condition Condition of the UFO to store (How much the UFO is damaged)
 * @return storedUFO_t pointer to the newly stored UFO (or nullptr if failed)
 */
storedUFO_t* US_StoreUFO (const aircraft_t* ufoTemplate, installation_t* installation, date_t date, float condition)
{
	if (!ufoTemplate) {
		Com_DPrintf(DEBUG_CLIENT, "US_StoreUFO: Invalid aircraft (UFO) Template.\n");
		return nullptr;
	}

	if (!installation) {
		Com_DPrintf(DEBUG_CLIENT, "US_StoreUFO: Invalid Installation\n");
		return nullptr;
	}

	if (installation->ufoCapacity.cur >= installation->ufoCapacity.max) {
		Com_DPrintf(DEBUG_CLIENT, "US_StoreUFO: Installation is full with UFOs.\n");
		return nullptr;
	}

	/* we can store it there */
	storedUFO_t ufo;
	ufo.idx = ccs.campaignStats.ufosStored++;
	Q_strncpyz(ufo.id, ufoTemplate->id, sizeof(ufo.id));
	ufo.comp = CP_GetComponentsByID(ufo.id);
	assert(ufo.comp);

	ufo.installation = installation;
	installation->ufoCapacity.cur++;

	assert(ufoTemplate->tech);

	ufo.ufoTemplate = ufoTemplate;
	ufo.disassembly = nullptr;

	ufo.arrive = date;
	if (Date_LaterThan(&ccs.date, &ufo.arrive)) {
		ufo.status = SUFO_STORED;
		RS_MarkCollected(ufo.ufoTemplate->tech);
	} else {
		ufo.status = SUFO_RECOVERED;
	}
	ufo.condition = std::min(std::max(0.0f, condition), 1.0f);

	return &LIST_Add(&ccs.storedUFOs, ufo);
}

/**
 * @brief Removes an UFO from the storage
 * @param[in,out] ufo stored UFO to remove
 */
void US_RemoveStoredUFO (storedUFO_t* ufo)
{
	assert(ufo);

	/* Stop disassembling */
	if (ufo->disassembly) {
		base_t* prodBase = PR_ProductionBase(ufo->disassembly);

		assert(prodBase);

		if (ufo->disassembly->idx == 0)
			PR_QueueNext(prodBase);
		else
			PR_QueueDelete(prodBase, PR_GetProductionForBase(prodBase), ufo->disassembly->idx);
	}

	/* Check all researches their requirements may broke */
	RS_CheckRequirements();

	/* remove ufo */
	ufo->installation->ufoCapacity.cur--;
	cgi->LIST_Remove(&ccs.storedUFOs, (void*)ufo);
}


/**
 * @brief Returns the number of UFOs stored (on an installation or anywhere)
 * @param[in] ufoTemplate aircraftTemplate of the ufo
 * @param[in] installation Pointer to the installation to count at
 * @returns the number of ufos stored of the given ufotype at given installation or overall
 * @note installation == nullptr means count on every ufoyards
 */
int US_UFOsInStorage (const aircraft_t* ufoTemplate, const installation_t* installation)
{
	int count = 0;

	US_Foreach(ufo) {
		if (ufo->ufoTemplate != ufoTemplate)
			continue;
		if (installation && ufo->installation != installation)
			continue;
		if (ufo->status != SUFO_STORED)
			continue;

		count++;
	}

	return count;
}

/**
 * @brief Removes ufos which are over the storing capacity
 * @param[in] installation pointer to the ufoyard the ufos are stored in
 */
void US_RemoveUFOsExceedingCapacity (installation_t* installation)
{
	if (!installation)
		cgi->Com_Error(ERR_DROP, "US_RemoveUFOsExceedingCapacity: No installation given!\n");

	const capacities_t* ufoCap = &installation->ufoCapacity;

	US_Foreach(ufo) {
		if (ufoCap->cur <= ufoCap->max)
			break;
		if (ufo->installation != installation) {
			continue;
		}
		US_RemoveStoredUFO(ufo);
	}
}

/**
 * @brief Start transferring of a stored UFO
 * @param[in,out] ufo Stored UFO to transfer
 * @param[in,out] ufoyard Destination of the UFO transfer
 * @return success or failure indicator
 */
bool US_TransferUFO (storedUFO_t* ufo, installation_t* ufoyard)
{
	date_t date;

	if (!ufo)
		cgi->Com_Error(ERR_DROP, "No UFO cannot be transferred!");
	if (!ufoyard)
		cgi->Com_Error(ERR_DROP, "UFO cannot be transferred to void!");
	/* only stored ufo can be transferred */
	if (ufo->status != SUFO_STORED)
		return false;
	/* UFO being disassembled cannot be transferred*/
	if (ufo->disassembly != nullptr)
		return false;
	/* UFO is in the same yard - no need of transfer */
	if (ufo->installation == ufoyard)
		return false;
	if (ufoyard->ufoCapacity.cur >= ufoyard->ufoCapacity.max)
		return false;

	date = ccs.date;
	date.day += (int) RECOVERY_DELAY;

	ufo->installation->ufoCapacity.cur--;
	ufo->status = SUFO_TRANSFERED;
	ufo->arrive = date;
	ufo->installation = ufoyard;
	ufoyard->ufoCapacity.cur++;

	Com_sprintf(cp_messageBuffer, sizeof(cp_messageBuffer), _("UFO transport started, cargo is being transported to %s"), ufoyard->name);
	MSO_CheckAddNewMessage(NT_TRANSFER_STARTED, _("UFO transport"), cp_messageBuffer, MSG_TRANSFERFINISHED);

	return true;
}

/**
 * @brief get the closest stored ufo (of a type) from a base
 * @param[in] ufoTemplate Pointer to the aircraft (ufo) template to look for (@c nullptr for any type)
 * @param[in] base Pointer to the base. If it's @c nullptr the function simply return the first stored UFO of type
 * @return Pointer to the first stored UFO matches the conditions
 */
storedUFO_t* US_GetClosestStoredUFO (const aircraft_t* ufoTemplate, const base_t* base)
{
	float minDistance = -1;
	storedUFO_t* closestUFO = nullptr;

	US_Foreach(ufo) {
		float distance = 0;

		if (ufoTemplate && ufo->ufoTemplate != ufoTemplate)
			continue;
		if (ufo->status != SUFO_STORED)
			continue;
		assert(ufo->installation);
		if (base)
			distance = GetDistanceOnGlobe(ufo->installation->pos, base->pos);

		if (minDistance < 0 || minDistance > distance) {
			minDistance = distance;
			closestUFO = ufo;
		}
	}
	return closestUFO;
}

/**
 * @brief Returns the number of storedUFOs
 */
int US_StoredUFOCount (void)
{
	return cgi->LIST_Count(ccs.storedUFOs);
}

/**
 * @brief Save callback for savegames in XML Format
 * @param[out] p XML Node structure, where we write the information to
 * @sa US_LoadXML
 * @sa SAV_GameSaveXML
 */
bool US_SaveXML (xmlNode_t* p)
{
	xmlNode_t* node = cgi->XML_AddNode(p, SAVE_UFORECOVERY_STOREDUFOS);

	cgi->Com_RegisterConstList(saveStoredUFOConstants);
	US_Foreach(ufo) {
		xmlNode_t* snode = cgi->XML_AddNode(node, SAVE_UFORECOVERY_UFO);

		cgi->XML_AddInt(snode, SAVE_UFORECOVERY_UFOIDX, ufo->idx);
		cgi->XML_AddString(snode, SAVE_UFORECOVERY_UFOID, ufo->id);
		cgi->XML_AddDate(snode, SAVE_UFORECOVERY_DATE, ufo->arrive.day, ufo->arrive.sec);
		cgi->XML_AddString(snode, SAVE_UFORECOVERY_STATUS, cgi->Com_GetConstVariable(SAVE_STOREDUFOSTATUS_NAMESPACE, ufo->status));
		cgi->XML_AddFloat(snode, SAVE_UFORECOVERY_CONDITION, ufo->condition);

		if (ufo->installation)
			cgi->XML_AddInt(snode, SAVE_UFORECOVERY_INSTALLATIONIDX, ufo->installation->idx);
	}
	cgi->Com_UnregisterConstList(saveStoredUFOConstants);
	return true;
}

/**
 * @brief Load callback for xml savegames
 * @param[in] p XML Node structure, where we get the information from
 * @sa US_SaveXML
 * @sa SAV_GameLoadXML
 */
bool US_LoadXML (xmlNode_t* p)
{
	xmlNode_t* node, *snode;

	node = cgi->XML_GetNode(p, SAVE_UFORECOVERY_STOREDUFOS);

	cgi->Com_RegisterConstList(saveStoredUFOConstants);
	for (snode = cgi->XML_GetNode(node, SAVE_UFORECOVERY_UFO); snode;
			snode = cgi->XML_GetNextNode(snode, node, SAVE_UFORECOVERY_UFO)) {
		const char* id = cgi->XML_GetString(snode, SAVE_UFORECOVERY_STATUS);
		storedUFO_t ufo;
		int statusIDX;

		/* ufo->idx */
		ufo.idx = cgi->XML_GetInt(snode, SAVE_UFORECOVERY_UFOIDX, -1);
		if (ufo.idx < 0) {
			Com_Printf("Invalid or no IDX defined for stored UFO.\n");
			continue;
		}
		/* ufo->status */
		if (!cgi->Com_GetConstIntFromNamespace(SAVE_STOREDUFOSTATUS_NAMESPACE, id, &statusIDX)) {
			Com_Printf("Invalid storedUFOStatus '%s'\n", id);
			continue;
		}
		ufo.status = (storedUFOStatus_t)statusIDX;
		/* ufo->installation */
		ufo.installation = INS_GetByIDX(cgi->XML_GetInt(snode, SAVE_UFORECOVERY_INSTALLATIONIDX, -1));
		if (!ufo.installation) {
			Com_Printf("UFO has no/invalid installation assigned\n");
			continue;
		}
		if (ufo.installation->ufoCapacity.cur >= ufo.installation->ufoCapacity.max) {
			Com_Printf("UFO Yard %i if full!\n", ufo.installation->idx);
			continue;
		}
		ufo.installation->ufoCapacity.cur++;
		/* ufo->id */
		Q_strncpyz(ufo.id, cgi->XML_GetString(snode, SAVE_UFORECOVERY_UFOID), sizeof(ufo.id));
		/* ufo->ufoTemplate */
		ufo.ufoTemplate = AIR_GetAircraft(ufo.id);
		if (!ufo.ufoTemplate) {
			Com_Printf("UFO has no/invalid aircraftTemplare assigned\n");
			continue;
		}
		ufo.comp = CP_GetComponentsByID(ufo.id);
		if (!ufo.comp) {
			Com_Printf("UFO has no/invalid components set\n");
			continue;
		}
		cgi->XML_GetDate(snode, SAVE_UFORECOVERY_DATE, &ufo.arrive.day, &ufo.arrive.sec);
		ufo.condition = cgi->XML_GetFloat(snode, SAVE_UFORECOVERY_CONDITION, 1.0f);
		/* disassembly is set by production savesystem later but only for UFOs that are being disassembled */
		ufo.disassembly = nullptr;
		LIST_Add(&ccs.storedUFOs, ufo);
	}
	cgi->Com_UnregisterConstList(saveStoredUFOConstants);
	return true;
}

#ifdef DEBUG
/**
 * @brief Debug callback to list ufostores
 */
static void US_ListStoredUFOs_f (void)
{
	US_Foreach(ufo) {
		const base_t* prodBase = PR_ProductionBase(ufo->disassembly);
		dateLong_t date;

		Com_Printf("IDX: %i\n", ufo->idx);
		Com_Printf("id: %s\n", ufo->id);
		Com_Printf("stored at %s\n", (ufo->installation) ? ufo->installation->name : "NOWHERE");

		CP_DateConvertLong(&(ufo->arrive), &date);
		Com_Printf("arrived at: %i %s %02i, %02i:%02i\n", date.year,
		Date_GetMonthName(date.month - 1), date.day, date.hour, date.min);

		if (ufo->ufoTemplate->tech->base)
			Com_Printf("tech being researched at %s\n", ufo->ufoTemplate->tech->base->name);
		if (prodBase)
			Com_Printf("being disassembled at %s\n", prodBase->name);
	}
}

/**
 * @brief Adds an UFO to the stores
 */
static void US_StoreUFO_f (void)
{
	char ufoId[MAX_VAR];

	if (cgi->Cmd_Argc() <= 2) {
		Com_Printf("Usage: %s <ufoType> <installationIdx>\n", cgi->Cmd_Argv(0));
		return;
	}

	Q_strncpyz(ufoId, cgi->Cmd_Argv(1), sizeof(ufoId));
	int installationIDX = atoi(cgi->Cmd_Argv(2));

	/* Get The UFO Yard */
	if (installationIDX < 0) {
		Com_Printf("US_StoreUFO_f: Invalid Installation index.\n");
		return;
	}
	installation_t* installation = INS_GetByIDX(installationIDX);
	if (!installation) {
		Com_Printf("US_StoreUFO_f: There is no Installation: idx=%i.\n", installationIDX);
		return;
	}

	/* Get UFO Type */
	aircraft_t* ufoType = nullptr;
	for (int i = 0; i < ccs.numAircraftTemplates; i++) {
		if (strstr(ccs.aircraftTemplates[i].id, ufoId)) {
			ufoType = &ccs.aircraftTemplates[i];
			break;
		}
	}
	if (ufoType == nullptr) {
		Com_Printf("US_StoreUFO_f: In valid UFO Id.\n");
		return;
	}

	US_StoreUFO(ufoType, installation, ccs.date, 1.0f);
}

/**
 * @brief Removes an UFO from the stores
 */
static void US_RemoveStoredUFO_f (void)
{
	if (cgi->Cmd_Argc() < 2) {
		Com_Printf("Usage: %s <idx>\n", cgi->Cmd_Argv(0));
		return;
	} else {
		const int idx = atoi(cgi->Cmd_Argv(1));
		storedUFO_t* storedUFO = US_GetStoredUFOByIDX(idx);
		if (!storedUFO) {
			Com_Printf("US_RemoveStoredUFO_f: No such ufo index.\n");
			return;
		}
		US_RemoveStoredUFO(storedUFO);
	}
}
#endif

static const cmdList_t debugStoredUfosCmds[] = {
#ifdef DEBUG
	{"debug_liststoredufos", US_ListStoredUFOs_f, "Debug function to list UFOs in Hangars."},
	{"debug_storeufo", US_StoreUFO_f, "Debug function to Add UFO to Hangars."},
	{"debug_removestoredufo", US_RemoveStoredUFO_f, "Debug function to Remove UFO from Hangars."},
#endif
	{nullptr, nullptr, nullptr}
};
/**
 * @brief Init actions for ufostoring-subsystem
 * @sa UI_InitStartup
 */
void UR_InitStartup (void)
{
	UR_InitCallbacks();
	cgi->Cmd_TableAddList(debugStoredUfosCmds);
}

/**
 * @brief Closing actions for ufostoring-subsystem
 */
void UR_Shutdown (void)
{
	cgi->LIST_Delete(&ccs.storedUFOs);

	UR_ShutdownCallbacks();
	cgi->Cmd_TableRemoveList(debugStoredUfosCmds);
}
