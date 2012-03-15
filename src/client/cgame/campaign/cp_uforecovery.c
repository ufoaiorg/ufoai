/**
 * @file cp_uforecovery.c
 * @brief UFO recovery and storing
 * @note UFO recovery functions with UR_*
 * @note UFO storing functions with US_*
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
#include "cp_ufo.h"
#include "cp_map.h"
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

		Com_sprintf(cp_messageBuffer, sizeof(cp_messageBuffer), _("%s was transfered to %s."), UFO_TypeToName(ufo->ufoTemplate->ufotype), ufo->installation->name);
		switch (ufo->status) {
		case SUFO_RECOVERED:
			MSO_CheckAddNewMessage(NT_TRANSFER_UFORECOVERY_FINISHED, _("UFO Recovered"), cp_messageBuffer, qfalse, MSG_TRANSFERFINISHED, NULL);
			break;
		case SUFO_TRANSFERED:
			MSO_CheckAddNewMessage(NT_TRANSFER_UFORECOVERY_FINISHED, _("UFO Transfered"), cp_messageBuffer, qfalse, MSG_TRANSFERFINISHED, NULL);
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
	return NULL;
}

/**
 * @brief Adds an UFO to the storage
 * @param[in] ufoTemplate Pointer to the aircraft(ufo)Template to add
 * @param[in,out] installation Pointer to the installation it should be added to
 * @param[in] date Date when UFO is arrives to the storage (recovery || transfer)
 * @param[in] condition Condition of the UFO to store (How much the UFO is damaged)
 * @return storedUFO_t pointer to the newly stored UFO (or NULL if failed)
 */
storedUFO_t *US_StoreUFO (const aircraft_t *ufoTemplate, installation_t *installation, date_t date, float condition)
{
	storedUFO_t ufo;

	if (!ufoTemplate) {
		Com_DPrintf(DEBUG_CLIENT, "US_StoreUFO: Invalid aircraft (UFO) Template.\n");
		return NULL;
	}

	if (!installation) {
		Com_DPrintf(DEBUG_CLIENT, "US_StoreUFO: Invalid Installation\n");
		return NULL;
	}

	if (installation->ufoCapacity.cur >= installation->ufoCapacity.max) {
		Com_DPrintf(DEBUG_CLIENT, "US_StoreUFO: Installation is full with UFOs.\n");
		return NULL;
	}

	/* we can store it there */
	ufo.idx = ccs.campaignStats.ufosStored++;
	Q_strncpyz(ufo.id, ufoTemplate->id, sizeof(ufo.id));
	ufo.comp = CP_GetComponentsByID(ufo.id);
	assert(ufo.comp);

	ufo.installation = installation;
	installation->ufoCapacity.cur++;

	assert(ufoTemplate->tech);

	ufo.ufoTemplate = ufoTemplate;
	ufo.disassembly = NULL;

	ufo.arrive = date;
	if (Date_LaterThan(&ccs.date, &ufo.arrive)) {
		ufo.status = SUFO_STORED;
		RS_MarkCollected(ufo.ufoTemplate->tech);
	} else {
		ufo.status = SUFO_RECOVERED;
	}
	ufo.condition = min(max(0, condition), 1);

	return (storedUFO_t *)(LIST_Add(&ccs.storedUFOs, (const byte *)&ufo, sizeof(ufo)))->data;
}

/**
 * @brief Removes an UFO from the storage
 * @param[in,out] ufo stored UFO to remove
 */
void US_RemoveStoredUFO (storedUFO_t *ufo)
{
	int ufoCount;

	assert(ufo);

	/* Stop disassembling */
	if (ufo->disassembly) {
		base_t *prodBase = PR_ProductionBase(ufo->disassembly);

		assert(prodBase);

		if (ufo->disassembly->idx == 0)
			PR_QueueNext(prodBase);
		else
			PR_QueueDelete(prodBase, PR_GetProductionForBase(prodBase), ufo->disassembly->idx);
	}

	/* Stop running research if this is the only UFO from this type
	 * also clear collected status */
	assert(ufo->ufoTemplate);
	ufoCount = US_UFOsInStorage(ufo->ufoTemplate, NULL);
	if (ufoCount <= 1 && ufo->ufoTemplate->tech->statusResearch == RS_RUNNING)
		RS_StopResearch(ufo->ufoTemplate->tech);

	/* remove ufo */
	ufo->installation->ufoCapacity.cur--;
	LIST_Remove(&ccs.storedUFOs, (void*)ufo);
}


/**
 * @brief Returns the number of UFOs stored (on an installation or anywhere)
 * @param[in] ufoTemplate aircraftTemplate of the ufo
 * @param[in] installation Pointer to the installation to count at
 * @returns the number of ufos stored of the given ufotype at given installation or overall
 * @note installation == NULL means count on every ufoyards
 */
int US_UFOsInStorage (const aircraft_t *ufoTemplate, const installation_t *installation)
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
void US_RemoveUFOsExceedingCapacity (installation_t *installation)
{
	const capacities_t *ufoCap;

	if (!installation)
		Com_Error(ERR_DROP, "US_RemoveUFOsExceedingCapacity: No installation given!\n");

	ufoCap = &installation->ufoCapacity;

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
 * @brief Start transfering of a stored UFO
 * @param[in,out] ufo Stored UFO to transfer
 * @param[in,out] ufoyard Destination of the UFO transfer
 * @return success or failure indicator
 */
qboolean US_TransferUFO (storedUFO_t *ufo, installation_t *ufoyard)
{
	date_t date;

	if (!ufo)
		Com_Error(ERR_DROP, "No UFO cannot be transfered!");
	if (!ufoyard)
		Com_Error(ERR_DROP, "UFO cannot be transfered to void!");
	/* only stored ufo can be transfered */
	if (ufo->status != SUFO_STORED)
		return qfalse;
	/* UFO being disassembled cannot be transfered*/
	if (ufo->disassembly != NULL)
		return qfalse;
	/* UFO is in the same yard - no need of transfer */
	if (ufo->installation == ufoyard)
		return qfalse;
	if (ufoyard->ufoCapacity.cur >= ufoyard->ufoCapacity.max)
		return qfalse;

	date = ccs.date;
	date.day += (int) RECOVERY_DELAY;

	ufo->installation->ufoCapacity.cur--;
	ufo->status = SUFO_TRANSFERED;
	ufo->arrive = date;
	ufo->installation = ufoyard;
	ufoyard->ufoCapacity.cur++;

	Com_sprintf(cp_messageBuffer, sizeof(cp_messageBuffer), _("UFO transport started, cargo is being transported to %s"), ufoyard->name);
	MSO_CheckAddNewMessage(NT_TRANSFER_STARTED, _("UFO transport"), cp_messageBuffer, qfalse, MSG_TRANSFERFINISHED, NULL);

	return qtrue;
}

/**
 * @brief get the closest stored ufo (of a type) from a base
 * @param[in] ufoTemplate Pointer to the aircraft (ufo) template to look for (@c NULL for any type)
 * @param[in] base Pointer to the base. If it's @c NULL the function simply return the first stored UFO of type
 * @return Pointer to the first stored UFO matches the conditions
 */
storedUFO_t *US_GetClosestStoredUFO (const aircraft_t *ufoTemplate, const base_t *base)
{
	float minDistance = -1;
	storedUFO_t *closestUFO = NULL;

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
	return LIST_Count(ccs.storedUFOs);
}

/**
 * @brief Save callback for savegames in XML Format
 * @param[out] p XML Node structure, where we write the information to
 * @sa US_LoadXML
 * @sa SAV_GameSaveXML
 */
qboolean US_SaveXML (xmlNode_t *p)
{
	xmlNode_t *node = XML_AddNode(p, SAVE_UFORECOVERY_STOREDUFOS);

	Com_RegisterConstList(saveStoredUFOConstants);
	US_Foreach(ufo) {
		xmlNode_t * snode = XML_AddNode(node, SAVE_UFORECOVERY_UFO);

		XML_AddInt(snode, SAVE_UFORECOVERY_UFOIDX, ufo->idx);
		XML_AddString(snode, SAVE_UFORECOVERY_UFOID, ufo->id);
		XML_AddDate(snode, SAVE_UFORECOVERY_DATE, ufo->arrive.day, ufo->arrive.sec);
		XML_AddString(snode, SAVE_UFORECOVERY_STATUS, Com_GetConstVariable(SAVE_STOREDUFOSTATUS_NAMESPACE, ufo->status));
		XML_AddFloat(snode, SAVE_UFORECOVERY_CONDITION, ufo->condition);

		if (ufo->installation)
			XML_AddInt(snode, SAVE_UFORECOVERY_INSTALLATIONIDX, ufo->installation->idx);
	}
	Com_UnregisterConstList(saveStoredUFOConstants);
	return qtrue;
}

/**
 * @brief Load callback for xml savegames
 * @param[in] p XML Node structure, where we get the information from
 * @sa US_SaveXML
 * @sa SAV_GameLoadXML
 */
qboolean US_LoadXML (xmlNode_t *p)
{
	xmlNode_t *node, *snode;

	node = XML_GetNode(p, SAVE_UFORECOVERY_STOREDUFOS);

	Com_RegisterConstList(saveStoredUFOConstants);
	for (snode = XML_GetNode(node, SAVE_UFORECOVERY_UFO); snode;
			snode = XML_GetNextNode(snode, node, SAVE_UFORECOVERY_UFO)) {
		const char *id = XML_GetString(snode, SAVE_UFORECOVERY_STATUS);
		storedUFO_t ufo;
		int statusIDX;

		/* ufo->idx */
		ufo.idx = XML_GetInt(snode, SAVE_UFORECOVERY_UFOIDX, -1);
		if (ufo.idx < 0) {
			Com_Printf("Invalid or no IDX defined for stored UFO.\n");
			continue;
		}
		/* ufo->status */
		if (!Com_GetConstIntFromNamespace(SAVE_STOREDUFOSTATUS_NAMESPACE, id, &statusIDX)) {
			Com_Printf("Invalid storedUFOStatus '%s'\n", id);
			continue;
		}
		ufo.status = statusIDX;
		/* ufo->installation */
		ufo.installation = INS_GetByIDX(XML_GetInt(snode, SAVE_UFORECOVERY_INSTALLATIONIDX, -1));
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
		Q_strncpyz(ufo.id, XML_GetString(snode, SAVE_UFORECOVERY_UFOID), sizeof(ufo.id));
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
		XML_GetDate(snode, SAVE_UFORECOVERY_DATE, &ufo.arrive.day, &ufo.arrive.sec);
		ufo.condition = XML_GetFloat(snode, SAVE_UFORECOVERY_CONDITION, 1.0f);
		/* disassembly is set by production savesystem later but only for UFOs that are being disassembled */
		ufo.disassembly = NULL;
		LIST_Add(&ccs.storedUFOs, (const byte *)&ufo, sizeof(ufo));
	}
	Com_UnregisterConstList(saveStoredUFOConstants);
	return qtrue;
}

#ifdef DEBUG
/**
 * @brief Debug callback to list ufostores
 */
static void US_ListStoredUFOs_f (void)
{
	US_Foreach(ufo) {
		const base_t *prodBase = PR_ProductionBase(ufo->disassembly);
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
	int installationIDX;
	installation_t *installation;
	aircraft_t *ufoType = NULL;
	int i;

	if (Cmd_Argc() <= 2) {
		Com_Printf("Usage: %s <ufoType> <installationIdx>\n", Cmd_Argv(0));
		return;
	}

	Q_strncpyz(ufoId, Cmd_Argv(1), sizeof(ufoId));
	installationIDX = atoi(Cmd_Argv(2));

	/* Get The UFO Yard */
	if (installationIDX < 0) {
		Com_Printf("US_StoreUFO_f: Invalid Installation index.\n");
		return;
	}
	installation = INS_GetByIDX(installationIDX);
	if (!installation) {
		Com_Printf("US_StoreUFO_f: There is no Installation: idx=%i.\n", installationIDX);
		return;
	}

	/* Get UFO Type */
	for (i = 0; i < ccs.numAircraftTemplates; i++) {
		if (strstr(ccs.aircraftTemplates[i].id, ufoId)) {
			ufoType = &ccs.aircraftTemplates[i];
			break;
		}
	}
	if (ufoType == NULL) {
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
	if (Cmd_Argc() < 2) {
		Com_Printf("Usage: %s <idx>\n", Cmd_Argv(0));
		return;
	} else {
		const int idx = atoi(Cmd_Argv(1));
		storedUFO_t *storedUFO = US_GetStoredUFOByIDX(idx);
		if (!storedUFO) {
			Com_Printf("US_RemoveStoredUFO_f: No such ufo index.\n");
			return;
		}
		US_RemoveStoredUFO(storedUFO);
	}
}
#endif

/**
 * @brief Init actions for ufostoring-subsystem
 * @sa UI_InitStartup
 */
void UR_InitStartup (void)
{
	UR_InitCallbacks();
#ifdef DEBUG
	Cmd_AddCommand("debug_liststoredufos", US_ListStoredUFOs_f, "Debug function to list UFOs in Hangars.");
	Cmd_AddCommand("debug_storeufo", US_StoreUFO_f, "Debug function to Add UFO to Hangars.");
	Cmd_AddCommand("debug_removestoredufo", US_RemoveStoredUFO_f, "Debug function to Remove UFO from Hangars.");
#endif
}

/**
 * @brief Closing actions for ufostoring-subsystem
 */
void UR_Shutdown (void)
{
	LIST_Delete(&ccs.storedUFOs);

	UR_ShutdownCallbacks();
#ifdef DEBUG
	Cmd_RemoveCommand("debug_liststoredufos");
	Cmd_RemoveCommand("debug_storeufo");
	Cmd_RemoveCommand("debug_removestoredufo");
#endif
}
