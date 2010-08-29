/**
 * @file cp_uforecovery.c
 * @brief UFO recovery and storing
 * @note UFO recovery functions with UR_*
 * @note UFO storing functions with US_*
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
#include "cp_ufo.h"
#include "cp_map.h"
#include "cp_uforecovery.h"
#include "cp_uforecovery_callbacks.h"
#include "cp_aircraft.h"
#include "save/save_uforecovery.h"

/*==================================
Backend functions
==================================*/

/**
 * @brief Function to process active recoveries.
 * @sa CL_CampaignRun
 */
void UR_ProcessActive (void)
{
	storedUFO_t *ufo = NULL;

	while ((ufo = US_GetNext(ufo)) != NULL) {
		assert(ufo->ufoTemplate);
		assert(ufo->ufoTemplate->tech);

		if (ufo->status == SUFO_STORED)
			continue;
		if (ufo->arrive.day > ccs.date.day || (ufo->arrive.day == ccs.date.day && ufo->arrive.sec > ccs.date.sec))
			continue;

		ufo->status = SUFO_STORED;

		Com_sprintf(cp_messageBuffer, sizeof(cp_messageBuffer), _("%s was transfered to %s."), UFO_TypeToName(ufo->ufoTemplate->ufotype), ufo->installation->name);
		MSO_CheckAddNewMessage(NT_TRANSFER_UFORECOVERY_FINISHED, _("UFO Recovered"), cp_messageBuffer, qfalse, MSG_TRANSFERFINISHED, NULL);

		if (!ufo->ufoTemplate->tech->statusCollected)
			RS_MarkCollected(ufo->ufoTemplate->tech);
	}
}

/* ==== UFO Storing stuff ==== */

/**
 * @brief Iterates through the stored UFOs
 * @param[in] lastBase Pointer of the UFO to iterate from. call with NULL to get the first one.
 */
storedUFO_t* US_GetNext (storedUFO_t *lastUFO)
{
	return (storedUFO_t*)LIST_GetNext(ccs.storedUFOs, lastUFO);
}

/**
 * @brief Returns a stored ufo
 * @param[in] idx index of the stored UFO
 * @return storedUFO_t Pointer
 */
storedUFO_t* US_GetStoredUFOByIDX (const int idx)
{
	storedUFO_t *ufo = NULL;

	while ((ufo = US_GetNext(ufo)) != NULL) {
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
 * @param[in] idx UFO's index. Call with negative value for
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
	ufo.comp = CL_GetComponentsByID(ufo.id);
	assert(ufo.comp);

	ufo.installation = installation;
	installation->ufoCapacity.cur++;

	assert(ufoTemplate->tech);

	ufo.ufoTemplate = ufoTemplate;
	ufo.disassembly = NULL;

	ufo.arrive = date;
	if (date.day < ccs.date.day || (date.day == ccs.date.day && date.sec <= ccs.date.sec)) {
		ufo.status = SUFO_STORED;
		RS_MarkCollected(ufo.ufoTemplate->tech);
	} else {
		ufo.status = SUFO_RECOVERED;
	}
	ufo.condition = min(max(0, condition), 1);

	return (storedUFO_t *)(LIST_Add(&ccs.storedUFOs, (void*)&ufo, sizeof(ufo)))->data;
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
			PR_QueueDelete(prodBase, &ccs.productions[prodBase->idx], ufo->disassembly->idx);
	}

	/* Stop running research if this is the only UFO from this type
	 * also clear collected status */
	assert(ufo->ufoTemplate);
	ufoCount = US_UFOsInStorage(ufo->ufoTemplate, NULL);
	if (ufoCount <= 1 && ufo->ufoTemplate->tech->statusResearch == RS_RUNNING)
		RS_StopResearch(ufo->ufoTemplate->tech);

	/* remove ufo */
	ufo->installation->ufoCapacity.cur--;
	LIST_RemovePointer(&ccs.storedUFOs, (void*)ufo);
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
	storedUFO_t *ufo = NULL;
	int count = 0;

	while ((ufo = US_GetNext(ufo)) != NULL) {
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
	storedUFO_t *lastUfo = NULL;
	storedUFO_t *ufo = NULL;

	if (!installation)
		Com_Error(ERR_DROP, "US_RemoveUFOsExceedingCapacity: No installation given!\n");

	ufoCap = &installation->ufoCapacity;

	while ((ufo = US_GetNext(ufo)) != NULL) {
		if (ufoCap->cur <= ufoCap->max)
			break;
		if (ufo->installation != installation) {
			lastUfo = ufo;
			continue;
		}
		US_RemoveStoredUFO(ufo);
		/* this ufo is removed from the list, continue iterating from the last */
		ufo = lastUfo;
	}
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
	storedUFO_t *ufo = NULL;
	storedUFO_t *closestUFO = NULL;

	while ((ufo = US_GetNext(ufo)) != NULL) {
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
qboolean US_SaveXML (mxml_node_t *p)
{
	storedUFO_t *ufo = NULL;
	mxml_node_t *node = mxml_AddNode(p, SAVE_UFORECOVERY_STOREDUFOS);

	Com_RegisterConstList(saveStoredUFOConstants);
	while ((ufo = US_GetNext(ufo)) != NULL) {
		mxml_node_t * snode = mxml_AddNode(node, SAVE_UFORECOVERY_UFO);

		mxml_AddInt(snode, SAVE_UFORECOVERY_UFOIDX, ufo->idx);
		mxml_AddString(snode, SAVE_UFORECOVERY_UFOID, ufo->id);
		mxml_AddDate(snode, SAVE_UFORECOVERY_DATE, ufo->arrive.day, ufo->arrive.sec);
		mxml_AddString(snode, SAVE_UFORECOVERY_STATUS, Com_GetConstVariable(SAVE_STOREDUFOSTATUS_NAMESPACE, ufo->status));
		mxml_AddFloat(snode, SAVE_UFORECOVERY_CONDITION, ufo->condition);

		if (ufo->installation)
			mxml_AddInt(snode, SAVE_UFORECOVERY_INSTALLATIONIDX, ufo->installation->idx);
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
qboolean US_LoadXML (mxml_node_t *p)
{
	int i; /**< @todo this is for old saves now only */
	mxml_node_t *node, *snode;

	node = mxml_GetNode(p, SAVE_UFORECOVERY_STOREDUFOS);

	Com_RegisterConstList(saveStoredUFOConstants);
	for (i = 0, snode = mxml_GetNode(node, SAVE_UFORECOVERY_UFO); snode;
			snode = mxml_GetNextNode(snode, node, SAVE_UFORECOVERY_UFO), i++) {
		const char *id = mxml_GetString(snode, SAVE_UFORECOVERY_STATUS);
		storedUFO_t ufo;
		int statusIDX;

		/* ufo->idx */
		ufo.idx = mxml_GetInt(snode, SAVE_UFORECOVERY_UFOIDX, -1);
		/* fallback code for compatibility */
		if (ufo.idx == -1) {
			Com_Printf("No IDX defined for stored UFO %d. This must be an old save.\n", i);
			ufo.idx = i;
		}
		/* ufo->status */
		if (!Com_GetConstIntFromNamespace(SAVE_STOREDUFOSTATUS_NAMESPACE, id, &statusIDX)) {
			Com_Printf("Invalid storedUFOStatus '%s'\n", id);
			continue;
		}
		ufo.status = statusIDX;
		/* ufo->installation */
		ufo.installation = INS_GetFoundedInstallationByIDX(mxml_GetInt(snode, SAVE_UFORECOVERY_INSTALLATIONIDX, MAX_INSTALLATIONS));
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
		Q_strncpyz(ufo.id, mxml_GetString(snode, SAVE_UFORECOVERY_UFOID), sizeof(ufo.id));
		/* ufo->ufoTemplate */
		ufo.ufoTemplate = AIR_GetAircraft(ufo.id);
		if (!ufo.ufoTemplate) {
			Com_Printf("UFO has no/invalid aircraftTemplare assigned\n");
			continue;
		}
		ufo.comp = CL_GetComponentsByID(ufo.id);
		if (!ufo.comp) {
			Com_Printf("UFO has no/invalid components set\n");
			continue;
		}
		mxml_GetDate(snode, SAVE_UFORECOVERY_DATE, &ufo.arrive.day, &ufo.arrive.sec);
		ufo.condition = mxml_GetFloat(snode, SAVE_UFORECOVERY_CONDITION, 1.0f);
		/* disassembly is set by production savesystem later but only for UFOs that are being disassembled */
		ufo.disassembly = NULL;
		LIST_Add(&ccs.storedUFOs, (void*)&ufo, sizeof(ufo));
	}
	Com_UnregisterConstList(saveStoredUFOConstants);
	return qtrue;
}

