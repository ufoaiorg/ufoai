/**
 * @file cp_produce.c
 * @brief Single player production stuff
 * @note Production stuff functions prefix: PR_
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
#include "../cl_game.h"
#include "../menu/m_main.h"
#include "../menu/m_popup.h"
#include "../menu/m_nodes.h"
#include "../mxml/mxml_ufoai.h"
#include "cp_campaign.h"
#include "cp_ufo.h"
#include "cp_produce_callbacks.h"
#include "save/save_produce.h"

/** @brief Used in production costs (to allow reducing prices below 1x). */
const int PRODUCE_FACTOR = 1;
const int PRODUCE_DIVISOR = 1;

/** @brief Default amount of workers, the produceTime for technologies is defined. */
/** @note producetime for technology entries is the time for PRODUCE_WORKERS amount of workers. */
static const int PRODUCE_WORKERS = 10;

static cvar_t* mn_production_limit;		/**< Maximum items in queue. */
static cvar_t* mn_production_workers;		/**< Amount of hired workers in base. */
static cvar_t* mn_production_amount;	/**< Amount of the current production; if no production, an invalid value */

/**
 * @brief Calculates the fraction (percentage) of production of an item in 1 hour.
 * @param[in] base Pointer to the base with given production.
 * @param[in] tech Pointer to the technology for given production.
 * @param[in] storedUFO Pointer to disassembled UFO.
 * @sa PR_ProductionRun
 * @sa PR_ItemProductionInfo
 * @sa PR_DisassemblyInfo
 * @return 0 if the production does not make any progress, 1 if the whole item is built in 1 hour
 */
float PR_CalculateProductionPercentDone (const base_t *base, const technology_t *tech, const storedUFO_t *const storedUFO)
{
	signed int allWorkers = 0;
	signed int maxWorkers = 0;
	signed int timeDefault = 0;

	assert(base);
	assert(tech);

	/* Check how many workers hired in this base. */
	allWorkers = E_CountHired(base, EMPL_WORKER);
	/* We will not use more workers than base capacity. */
	maxWorkers = min(allWorkers, base->capacities[CAP_WORKSPACE].max);

	if (!storedUFO) {
		/* This is the default production time for 10 workers. */
		timeDefault = tech->produceTime;
	} else {
		assert(storedUFO->comp);
		/* This is the default disassembly time for 10 workers. */
		timeDefault = storedUFO->comp->time;
	}
	if (maxWorkers == PRODUCE_WORKERS) {
		/* No need to calculate: timeDefault is for PRODUCE_WORKERS workers. */
		const float fraction =  1.0f / timeDefault;
		Com_DPrintf(DEBUG_CLIENT, "PR_CalculatePercentDone: workers: %i, tech: %s, percent: %f\n",
			maxWorkers, tech->id, fraction);
		return fraction;
	} else {
		/* Calculate the fraction of item produced for our amount of workers. */
		const float fraction = ((float)maxWorkers / (PRODUCE_WORKERS * timeDefault));
		Com_DPrintf(DEBUG_CLIENT, "PR_CalculatePercentDone: workers: %i, tech: %s, percent: %f\n",
			maxWorkers, tech->id, fraction);
		/* Don't allow to return fraction greater than 1 (you still need at least 1 hour to produce an item). */
		return min(fraction, 1.0f);
	}
}

/**
 * @brief Delete the selected entry from the queue.
 * @param[in] base Pointer to base, where the queue is.
 * @param[in] queue Pointer to the queue.
 * @param[in] index Selected index in queue.
 */
void PR_QueueDelete (base_t *base, production_queue_t *queue, int index)
{
	int i;
	production_t *prod = &queue->items[index];

	if (prod->ufo) {
		prod->ufo->disassembly = NULL;
	}

	REMOVE_ELEM_ADJUST_IDX(queue->items, index, queue->numItems);

	/* Adjust ufos' disassembly pointer */
	for (i = index; i < queue->numItems; i++) {
		production_t *disassembly = &queue->items[i];

		if (disassembly->ufo)
			disassembly->ufo->disassembly = disassembly;
	}
}

/**
 * @brief Moves the given queue item in the given direction.
 * @param[in] queue Pointer to the queue.
 * @param[in] index
 * @param[in] dir
 */
void PR_QueueMove (production_queue_t *queue, int index, int dir)
{
	const int newIndex = max(0, min(index + dir, queue->numItems - 1));
	int i;
	production_t saved;

	if (newIndex == index)
		return;

	saved = queue->items[index];

	/* copy up */
	for (i = index; i < newIndex; i++) {
		queue->items[i] = queue->items[i + 1];
		queue->items[i].idx = i;
		if (queue->items[i].ufo)
			queue->items[i].ufo->disassembly = &(queue->items[i]);
	}

	/* copy down */
	for (i = index; i > newIndex; i--) {
		queue->items[i] = queue->items[i - 1];
		queue->items[i].idx = i;
		if (queue->items[i].ufo)
			queue->items[i].ufo->disassembly = &(queue->items[i]);
	}

	/* insert item */
	queue->items[newIndex] = saved;
	queue->items[newIndex].idx = newIndex;
	if (queue->items[newIndex].ufo)
		queue->items[newIndex].ufo->disassembly = &(queue->items[newIndex]);
}

/**
 * @brief Queues the next production in the queue.
 * @param[in] base Pointer to the base.
 */
void PR_QueueNext (base_t *base)
{
	production_queue_t *queue = &ccs.productions[base->idx];

	PR_QueueDelete(base, queue, 0);

	if (queue->numItems == 0) {
		Com_sprintf(cp_messageBuffer, sizeof(cp_messageBuffer), _("Production queue for %s is empty"), base->name);
		MSO_CheckAddNewMessage(NT_PRODUCTION_QUEUE_EMPTY, _("Production queue empty"), cp_messageBuffer, qfalse, MSG_PRODUCTION, NULL);
	}
}

/**
 * @brief clears the production queue on a base
 */
static void PR_EmptyQueue (base_t *base)
{
	production_queue_t *queue;

	if (!base)
		return;

	queue = &ccs.productions[base->idx];
	if (!queue)
		return;

	while (queue->numItems)
		PR_QueueDelete(base, queue, 0);
}

/**
 * @brief moves the first production to the bottom of the list
 */
static void PR_ProductionRollBottom_f (void)
{
	production_queue_t *queue;
	base_t *base = B_GetCurrentSelectedBase();

	if (!base)
		return;

	queue = &ccs.productions[base->idx];

	if (queue->numItems < 2)
		return;

	PR_QueueMove(queue, 0, queue->numItems - 1);
}

/**
 * @brief Disassembles item, adds components to base storage and calculates all components size.
 * @param[in] base Pointer to base where the disassembling is being made.
 * @param[in] comp Pointer to components definition.
 * @param[in] condition condition of the item/UFO being disassembled, objects gathered from disassembly decreased to that factor
 * @param[in] calculate True if this is only calculation of item size, false if this is real disassembling.
 * @return Size of all components in this disassembling.
 */
static int PR_DisassembleItem (base_t *base, components_t *comp, float condition, qboolean calculate)
{
	int i;
	int size = 0;

	if (!calculate && !base)	/* We need base only if this is real disassembling. */
		Com_Error(ERR_DROP, "PR_DisassembleItem: No base given");

	assert(comp);
	for (i = 0; i < comp->numItemtypes; i++) {
		const objDef_t *compOd = comp->items[i];
		const int amount = (condition < 1 && comp->itemAmount2[i] != COMP_ITEMCOUNT_SCALED) ? comp->itemAmount2[i] : round(comp->itemAmount[i] * condition);

		assert(compOd);
		size += compOd->size * amount;
		/* Add to base storage only if this is real disassembling, not calculation of size. */
		if (!calculate) {
			if (!strcmp(compOd->id, ANTIMATTER_TECH_ID)) {
				B_ManageAntimatter(base, amount, qtrue);
			} else {
				B_UpdateStorageAndCapacity(base, compOd, amount, qfalse, qfalse);
				RS_MarkCollected(compOd->tech);
			}
			Com_DPrintf(DEBUG_CLIENT, "PR_DisassembleItem: added %i amounts of %s\n", amount, compOd->id);
		}
	}
	return size;
}

/**
 * @brief Runs the production of an item or an aircraft
 * @sa PR_DisassemblingFrame
 * @sa PR_ProductionRun
 * @param base The base to produce in
 * @param prod The production that is running
 */
static void PR_ProductionFrame (base_t* base, production_t *prod)
{
	const objDef_t *od;
	const aircraft_t *aircraft;

	if (!prod->production)
		return;

	if (!prod->item && !prod->aircraft)
		return;

	if (prod->item) {
		od = prod->item;
		aircraft = NULL;
	} else if (prod->aircraft) {
		od = NULL;
		aircraft = prod->aircraft;
	}

	if (od) {
		/* Not enough money to produce more items in this base. */
		if (od->price * PRODUCE_FACTOR / PRODUCE_DIVISOR > ccs.credits) {
			if (!prod->creditMessage) {
				Com_sprintf(cp_messageBuffer, sizeof(cp_messageBuffer), _("Not enough credits to finish production in %s.\n"), base->name);
				MSO_CheckAddNewMessage(NT_PRODUCTION_FAILED, _("Notice"), cp_messageBuffer, qfalse, MSG_STANDARD, NULL);
				prod->creditMessage = qtrue;
			}
			PR_ProductionRollBottom_f();
			return;
		}
		/* Not enough free space in base storage for this item. */
		if (base->capacities[CAP_ITEMS].max - base->capacities[CAP_ITEMS].cur < od->size) {
			if (!prod->spaceMessage) {
				Com_sprintf(cp_messageBuffer, sizeof(cp_messageBuffer), _("Not enough free storage space in %s. Production postponed.\n"), base->name);
				MSO_CheckAddNewMessage(NT_PRODUCTION_FAILED, _("Notice"), cp_messageBuffer, qfalse, MSG_STANDARD, NULL);
				prod->spaceMessage = qtrue;
			}
			PR_ProductionRollBottom_f();
			return;
		}
		prod->percentDone += (PR_CalculateProductionPercentDone(base, od->tech, NULL) / MINUTES_PER_HOUR);
	} else if (aircraft) {
		/* Not enough money to produce more items in this base. */
		if (aircraft->price * PRODUCE_FACTOR / PRODUCE_DIVISOR > ccs.credits) {
			if (!prod->creditMessage) {
				Com_sprintf(cp_messageBuffer, sizeof(cp_messageBuffer), _("Not enough credits to finish production in %s.\n"), base->name);
				MSO_CheckAddNewMessage(NT_PRODUCTION_FAILED, _("Notice"), cp_messageBuffer, qfalse, MSG_STANDARD, NULL);
				prod->creditMessage = qtrue;
			}
			PR_ProductionRollBottom_f();
			return;
		}
		/* Not enough free space in hangars for this aircraft. */
		if (AIR_CalculateHangarStorage(prod->aircraft, base, 0) <= 0) {
			if (!prod->spaceMessage) {
				Com_sprintf(cp_messageBuffer, sizeof(cp_messageBuffer), _("Not enough free hangar space in %s. Production postponed.\n"), base->name);
				MSO_CheckAddNewMessage(NT_PRODUCTION_FAILED, _("Notice"), cp_messageBuffer, qfalse, MSG_STANDARD, NULL);
				prod->spaceMessage = qtrue;
			}
			PR_ProductionRollBottom_f();
			return;
		}
		if (ccs.numAircraft >= MAX_AIRCRAFT) {
			production_queue_t *queue = &ccs.productions[base->idx];

			Com_sprintf(cp_messageBuffer, sizeof(cp_messageBuffer), _("Cannot produce more aircraft (at %s). Max aircraft limit reached.\n"), base->name);
			MSO_CheckAddNewMessage(NT_PRODUCTION_FAILED, _("Notice"), cp_messageBuffer, qfalse, MSG_STANDARD, NULL);
			PR_QueueDelete(base, queue, prod->idx);
			return;
		}
		prod->percentDone += (PR_CalculateProductionPercentDone(base, aircraft->tech, NULL) / MINUTES_PER_HOUR);
	}

	if (prod->percentDone >= 1.0f) {
		const char *name = NULL;
		technology_t *tech = NULL;

		prod->percentDone = 0.0f;
		prod->amount--;

		if (od) {
			CL_UpdateCredits(ccs.credits - (od->price * PRODUCE_FACTOR / PRODUCE_DIVISOR));
			/* Now add it to equipment and update capacity. */
			B_UpdateStorageAndCapacity(base, prod->item, 1, qfalse, qfalse);
			name = _(od->name);
			tech = od->tech;
		} else if (aircraft) {
			CL_UpdateCredits(ccs.credits - (aircraft->price * PRODUCE_FACTOR / PRODUCE_DIVISOR));
			/* Now add new aircraft. */
			AIR_NewAircraft(base, aircraft->id);
			name = _(aircraft->tpl->name);
			tech = NULL;
		}

		/* queue the next production */
		if (prod->amount <= 0) {
			Com_sprintf(cp_messageBuffer, sizeof(cp_messageBuffer), _("The production of %s at %s has finished."), name, base->name);
			MSO_CheckAddNewMessage(NT_PRODUCTION_FINISHED, _("Production finished"), cp_messageBuffer, qfalse, MSG_PRODUCTION, tech);
			PR_QueueNext(base);
		}
	}
}

/**
 * @brief Runs the disassembling of a ufo
 * @sa PR_ProductionFrame
 * @sa PR_ProductionRun
 * @param base The base to produce in
 * @param prod The production that is running
 */
static void PR_DisassemblingFrame (base_t* base, production_t* prod)
{
	storedUFO_t *ufo;

	if (prod->production)
		return;

	if (!prod->ufo)
		return;

	ufo = prod->ufo;

	if (base->capacities[CAP_ITEMS].max - base->capacities[CAP_ITEMS].cur < PR_DisassembleItem(NULL, ufo->comp, ufo->condition, qtrue)) {
		if (!prod->spaceMessage) {
			Com_sprintf(cp_messageBuffer, sizeof(cp_messageBuffer), _("Not enough free storage space in %s. Disassembling postponed.\n"), base->name);
			MSO_CheckAddNewMessage(NT_PRODUCTION_FAILED, _("Notice"), cp_messageBuffer, qfalse, MSG_STANDARD, NULL);
			prod->spaceMessage = qtrue;
		}
		PR_ProductionRollBottom_f();
		return;
	}
	prod->percentDone += (PR_CalculateProductionPercentDone(base, ufo->ufoTemplate->tech, ufo) / MINUTES_PER_HOUR);

	if (prod->percentDone >= 1.0f) {
		base->capacities[CAP_ITEMS].cur += PR_DisassembleItem(base, ufo->comp, ufo->condition, qfalse);

		Com_sprintf(cp_messageBuffer, sizeof(cp_messageBuffer), _("The disassembling of %s at %s has finished."), UFO_TypeToName(ufo->ufoTemplate->ufotype), base->name);
		MSO_CheckAddNewMessage(NT_PRODUCTION_FINISHED, _("Production finished"), cp_messageBuffer, qfalse, MSG_PRODUCTION, ufo->ufoTemplate->tech);

		/* Removing UFO will remove the production too */
		US_RemoveStoredUFO(ufo);
	}
}

/**
 * @brief Checks whether an item is finished.
 * @sa CL_CampaignRun
 * @sa PR_DisassemblingFrame
 * @sa PR_ProductionFrame
 */
void PR_ProductionRun (void)
{
	int i;
	production_t *prod;

	/* Loop through all founded bases. Then check productions
	 * in global data array. Then increase prod->percentDone and check
	 * whether an item is produced. Then add to base storage. */
	for (i = 0; i < MAX_BASES; i++) {
		base_t *base = B_GetFoundedBaseByIDX(i);
		if (!base)
			continue;

		/* not actually any active productions */
		if (ccs.productions[i].numItems <= 0)
			continue;

		/* Workshop is disabled because their dependences are disabled */
		if (!PR_ProductionAllowed(base))
			continue;

		prod = &ccs.productions[i].items[0];

		PR_ProductionFrame(base, prod);
		PR_DisassemblingFrame(base, prod);
	}
}

/**
 * @brief Returns true if the current base is able to produce items
 * @param[in] base Pointer to the base.
 * @sa B_BaseInit_f
 */
qboolean PR_ProductionAllowed (const base_t* base)
{
	assert(base);
	if (base->baseStatus != BASE_UNDER_ATTACK
	 && B_GetBuildingStatus(base, B_WORKSHOP)
	 && E_CountHired(base, EMPL_WORKER) > 0) {
		return qtrue;
	} else {
		return qfalse;
	}
}

void PR_ProductionInit (void)
{
	mn_production_limit = Cvar_Get("mn_production_limit", "0", 0, NULL);
	mn_production_workers = Cvar_Get("mn_production_workers", "0", 0, NULL);
	mn_production_amount = Cvar_Get("mn_production_amount", "0", 0, NULL);
}

/**
 * @brief Update the current capacity of Workshop
 * @param[in] base Pointer to the base containing workshop.
 */
void PR_UpdateProductionCap (base_t *base)
{
	assert(base);

	if (base->capacities[CAP_WORKSPACE].max <= 0) {
		PR_EmptyQueue(base);
	}

	if (base->capacities[CAP_WORKSPACE].max >= E_CountHired(base, EMPL_WORKER)) {
		base->capacities[CAP_WORKSPACE].cur = E_CountHired(base, EMPL_WORKER);
	} else {
		base->capacities[CAP_WORKSPACE].cur = base->capacities[CAP_WORKSPACE].max;
	}
}

/**
 * @brief check if an item is producable.
 * @param[in] item Pointer to the item that should be checked.
 */
qboolean PR_ItemIsProduceable (const objDef_t const *item)
{
	assert(item);

	return !(item->tech && item->tech->produceTime == -1);
}

/**
 * @brief Returns the base pointer the production belongs to
 * @param[in] production pointer to the production entry
 * @returns base_t pointer to the base
 */
base_t *PR_ProductionBase (production_t *production) {
	int i;
	for (i = 0; i < ccs.numBases; i++) {
		base_t *base = B_GetBaseByIDX(i);
		const ptrdiff_t diff = ((ptrdiff_t)((production) - ccs.productions[i].items));

		if (diff >= 0 && diff < MAX_PRODUCTIONS)
			return base;
	}
	return NULL;
}

/**
 * @brief Save callback for savegames in XML Format
 * @param[out] p XML Node structure, where we write the information to
 * @sa PR_LoadXML
 * @sa SAV_GameSaveXML
 */
qboolean PR_SaveXML (mxml_node_t *p)
{
	int i;
	mxml_node_t *node = mxml_AddNode(p, SAVE_PRODUCE_PRODUCTION);

	for (i = 0; i < ccs.numBases; i++) {
		const production_queue_t *pq = &ccs.productions[i];
		int j;

		mxml_node_t *snode = mxml_AddNode(node, SAVE_PRODUCE_QUEUE);
		mxml_AddInt(snode, SAVE_PRODUCE_QUEUEIDX, i);

		for (j = 0; j < pq->numItems; j++) {
			const objDef_t *item = pq->items[j].item;
			const aircraft_t *aircraft = pq->items[j].aircraft;
			const storedUFO_t *ufo = pq->items[j].ufo;

			mxml_node_t * ssnode = mxml_AddNode(snode, SAVE_PRODUCE_ITEM);
			assert(item || aircraft || ufo);
			if (item)
				mxml_AddString(ssnode, SAVE_PRODUCE_ITEMID, item->id);
			else if (aircraft)
				mxml_AddString(ssnode, SAVE_PRODUCE_AIRCRAFTID, aircraft->id);
			else if (ufo)
				mxml_AddInt(ssnode, SAVE_PRODUCE_UFOIDX, ufo->idx);
			mxml_AddInt(ssnode, SAVE_PRODUCE_AMOUNT, pq->items[j].amount);
			mxml_AddFloatValue(ssnode, SAVE_PRODUCE_PERCENTDONE, pq->items[j].percentDone);
		}
	}
	return qtrue;
}

/**
 * @brief Load callback for xml savegames
 * @param[in] p XML Node structure, where we get the information from
 * @sa PR_SaveXML
 * @sa SAV_GameLoadXML
 */
qboolean PR_LoadXML (mxml_node_t *p)
{
	mxml_node_t *node, *snode;

	node = mxml_GetNode(p, SAVE_PRODUCE_PRODUCTION);

	for (snode = mxml_GetNode(node, SAVE_PRODUCE_QUEUE); snode;
			snode = mxml_GetNextNode(snode, node, SAVE_PRODUCE_QUEUE)) {
		mxml_node_t *ssnode;
		int baseIDX = mxml_GetInt(snode, SAVE_PRODUCE_QUEUEIDX, MAX_BASES);
		production_queue_t *pq;

		if (baseIDX < 0 || baseIDX >= ccs.numBases) {
			Com_Printf("Invalid production queue index %i\n", baseIDX);
			continue;
		}

		pq = &ccs.productions[baseIDX];

		for (ssnode = mxml_GetNode(snode, SAVE_PRODUCE_ITEM); pq->numItems < MAX_PRODUCTIONS && ssnode;
				ssnode = mxml_GetNextNode(ssnode, snode, SAVE_PRODUCE_ITEM)) {
			const char *s1 = mxml_GetString(ssnode, SAVE_PRODUCE_ITEMID);
			const char *s2;
			int ufoIDX;

			pq->items[pq->numItems].idx = pq->numItems;
			pq->items[pq->numItems].amount = mxml_GetInt(ssnode, SAVE_PRODUCE_AMOUNT, 0);
			pq->items[pq->numItems].percentDone = mxml_GetFloat(ssnode, SAVE_PRODUCE_PERCENTDONE, 0.0);

			/* amount */
			if (pq->items[pq->numItems].amount <= 0) {
				Com_Printf("PR_Load: Production with amount <= 0 dropped (baseidx=%i, production idx=%i).\n", baseIDX, pq->numItems);
				continue;
			}
			/* item */
			if (s1[0] != '\0')
				pq->items[pq->numItems].item = INVSH_GetItemByID(s1);
			/* UFO */
			ufoIDX = mxml_GetInt(ssnode, SAVE_PRODUCE_UFOIDX, MAX_STOREDUFOS);
			if (ufoIDX != MAX_STOREDUFOS) {
				storedUFO_t *ufo = US_GetStoredUFOByIDX(ufoIDX);

				if (!ufo) {
					Com_Printf("PR_Load: Could not find ufo idx: %i\n", ufoIDX);
					continue;
				}

				pq->items[pq->numItems].ufo = ufo;
				pq->items[pq->numItems].production = qfalse;
				pq->items[pq->numItems].ufo->disassembly = &(pq->items[pq->numItems]);
			} else {
				pq->items[pq->numItems].production = qtrue;
			}
			/* aircraft */
			s2 = mxml_GetString(ssnode, SAVE_PRODUCE_AIRCRAFTID);
			if (s2[0] != '\0')
				pq->items[pq->numItems].aircraft = AIR_GetAircraft(s2);

			if (!pq->items[pq->numItems].item && !pq->items[pq->numItems].ufo && !pq->items[pq->numItems].aircraft) {
				Com_Printf("PR_Load: Production is not an item an aircraft nor a disassembly\n");
				continue;
			}

			pq->numItems++;
		}
	}
	return qtrue;
}

