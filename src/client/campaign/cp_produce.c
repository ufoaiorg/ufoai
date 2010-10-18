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

#include "../cl_shared.h"
#include "../ui/ui_popup.h"
#include "cp_campaign.h"
#include "cp_ufo.h"
#include "cp_produce.h"
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
	float distanceFactor = 0.0f;

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
		/* Production is 4 times longer when installation is on Antipodes */
		distanceFactor = GetDistanceOnGlobe(storedUFO->installation->pos, base->pos) / 45.0f;
		assert(distanceFactor >= 0.0f);
		/* Penalty starts when distance is greater than 45 degrees */
		distanceFactor = max(1.0f, distanceFactor);
		Com_DPrintf(DEBUG_CLIENT, "PR_CalculatePercentDone: distanceFactor is %f\n", distanceFactor);
	}
	if (maxWorkers == PRODUCE_WORKERS) {
		/* No need to calculate: timeDefault is for PRODUCE_WORKERS workers. */
		const float fraction = 1.0f / ((NULL != storedUFO) ? (distanceFactor * timeDefault) : timeDefault);
		Com_DPrintf(DEBUG_CLIENT, "PR_CalculatePercentDone: workers: %i, tech: %s, percent: %f\n",
			maxWorkers, tech->id, fraction);
		return fraction;
	} else {
		/* Calculate the fraction of item produced for our amount of workers. */
		const float fraction = ((float)maxWorkers / (PRODUCE_WORKERS * ((NULL != storedUFO) ? (distanceFactor * timeDefault) : timeDefault)));
		Com_DPrintf(DEBUG_CLIENT, "PR_CalculatePercentDone: workers: %i, tech: %s, percent: %f\n",
			maxWorkers, tech->id, fraction);
		/* Don't allow to return fraction greater than 1 (you still need at least 1 hour to produce an item). */
		return min(fraction, 1.0f);
	}
}

/**
 * @brief Remove or add the required items from/to the a base.
 * @param[in] base Pointer to base.
 * @param[in] amount How many items are planned to be added (positive number) or removed (negative number).
 * @param[in] reqs The production requirements of the item that is to be produced. These included numbers are multiplied with 'amount')
 */
void PR_UpdateRequiredItemsInBasestorage (base_t *base, int amount, const requirements_t const *reqs)
{
	int i;

	if (!base)
		return;

	if (amount == 0)
		return;

	for (i = 0; i < reqs->numLinks; i++) {
		const requirement_t const *req = &reqs->links[i];
		switch (req->type) {
		case RS_LINK_ITEM: {
			const objDef_t *item = req->link.od;
			assert(item);
			B_AddToStorage(base, item, req->amount * amount);
			break;
		}
		case RS_LINK_ANTIMATTER:
			if (req->amount > 0)
				B_ManageAntimatter(base, req->amount * amount, qtrue);
			else if (req->amount < 0)
				B_ManageAntimatter(base, req->amount * -amount, qfalse);
			break;
		case RS_LINK_TECH:
		case RS_LINK_TECH_NOT:
			break;
		default:
			Com_Error(ERR_DROP, "Invalid requirement for production!\n");
		}
	}
}

/**
 * @brief Checks if the production requirements are met for a defined amount.
 * @param[in] amount How many items are planned to be produced.
 * @param[in] reqs The production requirements of the item that is to be produced.
 * @param[in] base Pointer to base.
 * @return how much item/aircraft/etc can be produced
 */
int PR_RequirementsMet (int amount, const requirements_t const *reqs, base_t *base)
{
	int i;
	int producibleAmount = amount;

	for (i = 0; i < reqs->numLinks; i++) {
		const requirement_t const *req = &reqs->links[i];

		switch (req->type) {
		case RS_LINK_ITEM: {
				const int items = min(amount, B_ItemInBase(req->link.od, base) / ((req->amount) ? req->amount : 1));
				producibleAmount = min(producibleAmount, items);
				break;
			}
		case RS_LINK_ANTIMATTER: {
				const int am = min(amount, B_AntimatterInBase(base) / ((req->amount) ? req->amount : 1));
				producibleAmount = min(producibleAmount, am);
				break;
			}
		case RS_LINK_TECH:
			producibleAmount = (RS_IsResearched_ptr(req->link.tech)) ? producibleAmount : 0;
			break;
		case RS_LINK_TECH_NOT:
			producibleAmount = (RS_IsResearched_ptr(req->link.tech)) ? 0 : producibleAmount;
			break;
		default:
			break;
		}
	}

	return producibleAmount;
}

/**
 * @brief returns the number of free production slots of a queue
 * @param[in] queue Pointer to the queue to check
 */
int PR_QueueFreeSpace (const base_t* base)
{
	const production_queue_t const *queue = PR_GetProductionForBase(base);

	int numWorkshops;

	assert(queue);
	assert(base);

	numWorkshops = max(B_GetNumberOfBuildingsInBaseByBuildingType(base, B_WORKSHOP), 0);

	return min(MAX_PRODUCTIONS, numWorkshops * MAX_PRODUCTIONS_PER_WORKSHOP - queue->numItems);
}

/**
 * @brief Add a new item to the bottom of the production queue.
 * @param[in] base Pointer to base, where the queue is.
 * @param[in] queue Pointer to the queue.
 * @param[in] item Item to add.
 * @param[in] aircraftTemplate aircraft to add.
 * @param[in] ufo The UFO in case of a disassemly.
 * @param[in] amount Desired amount to produce.
 */
production_t *PR_QueueNew (base_t *base, const objDef_t *item, aircraft_t *aircraftTemplate, storedUFO_t *ufo, signed int amount)
{
	production_t *prod;
	const technology_t *tech;
	production_queue_t *queue = PR_GetProductionForBase(base);

	assert((item && !aircraftTemplate && !ufo) || (!item && aircraftTemplate && !ufo) || (!item && !aircraftTemplate && ufo));

	if (PR_QueueFreeSpace(base) <= 0)
		return NULL;
	if (E_CountHired(base, EMPL_WORKER) <= 0)
		return NULL;

	/* Initialize */
	prod = &queue->items[queue->numItems];
	memset(prod, 0, sizeof(*prod));

	/* self-reference. */
	prod->idx = queue->numItems;

	if (item) {
		tech = RS_GetTechForItem(item);
	} else if (aircraftTemplate) {
		tech = aircraftTemplate->tech;
	} else {
		tech = ufo->ufoTemplate->tech;
		amount = 1;
	}

	if (!tech)
		return NULL;

	amount = PR_RequirementsMet(amount, &tech->requireForProduction, base);
	if (amount == 0)
		return NULL;

	PR_UpdateRequiredItemsInBasestorage(base, -amount, &tech->requireForProduction);

	/* We cannot queue new aircraft if no free hangar space. */
	/** @todo move this check out into a new function */
	if (aircraftTemplate) {
		if (!B_GetBuildingStatus(base, B_COMMAND)) {
			/** @todo move popup into menucode */
			UI_Popup(_("Hangars not ready"), _("You cannot queue aircraft.\nNo command centre in this base.\n"));
			return NULL;
		} else if (!B_GetBuildingStatus(base, B_HANGAR) && !B_GetBuildingStatus(base, B_SMALL_HANGAR)) {
			/** @todo move popup into menucode */
			UI_Popup(_("Hangars not ready"), _("You cannot queue aircraft.\nNo hangars in this base.\n"));
			return NULL;
		}
		/** @todo we should also count aircraft that are already in the queue list */
		if (AIR_CalculateHangarStorage(aircraftTemplate, base, 0) <= 0) {
			UI_Popup(_("Hangars not ready"), _("You cannot queue aircraft.\nNo free space in hangars.\n"));
			return NULL;
		}
	}

	prod->item = item;
	prod->aircraft = aircraftTemplate;
	prod->ufo = ufo;
	prod->amount = amount;
	prod->percentDone = 0.0f;

	if (ufo) {
		/* Disassembling. */
		prod->production = qfalse;
		ufo->disassembly = prod;
	} else {
		/* Production. */
		prod->production = qtrue;

		/* Don't try to add to queue an item which is not producible. */
		if (tech->produceTime < 0)
			return NULL;
	}

	queue->numItems++;
	return prod;
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
	const technology_t *tech;

	assert(base);

	if (prod->item) {
		tech = RS_GetTechForItem(prod->item);
	} else if (prod->aircraft) {
		tech = prod->aircraft->tech;
	} else if (prod->ufo) {
		assert(prod->ufo->ufoTemplate);
		tech = prod->ufo->ufoTemplate->tech;
		prod->ufo->disassembly = NULL;
	} else {
		Com_Error(ERR_DROP, "No tech pointer for production");
	}

	assert(tech);

	PR_UpdateRequiredItemsInBasestorage(base, prod->amount, &tech->requireForProduction);

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
		production_t *prod;
		queue->items[i] = queue->items[i + 1];
		prod = &queue->items[i];
		prod->idx = i;
		if (prod->ufo)
			prod->ufo->disassembly = prod;
	}

	/* copy down */
	for (i = index; i > newIndex; i--) {
		production_t *prod;
		queue->items[i] = queue->items[i - 1];
		prod = &queue->items[i];
		prod->idx = i;
		if (prod->ufo)
			prod->ufo->disassembly = prod;
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
	production_queue_t *queue = PR_GetProductionForBase(base);

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

	queue = PR_GetProductionForBase(base);
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

	queue = PR_GetProductionForBase(base);
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
				technology_t *tech = RS_GetTechForItem(compOd);
				B_UpdateStorageAndCapacity(base, compOd, amount, qfalse, qfalse);
				RS_MarkCollected(tech);
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
				Com_sprintf(cp_messageBuffer, sizeof(cp_messageBuffer), _("Not enough credits to finish production in %s."), base->name);
				MSO_CheckAddNewMessage(NT_PRODUCTION_FAILED, _("Notice"), cp_messageBuffer, qfalse, MSG_STANDARD, NULL);
				prod->creditMessage = qtrue;
			}
			PR_ProductionRollBottom_f();
			return;
		}
		/* Not enough free space in base storage for this item. */
		if (base->capacities[CAP_ITEMS].max - base->capacities[CAP_ITEMS].cur < od->size) {
			if (!prod->spaceMessage) {
				Com_sprintf(cp_messageBuffer, sizeof(cp_messageBuffer), _("Not enough free storage space in %s. Production postponed."), base->name);
				MSO_CheckAddNewMessage(NT_PRODUCTION_FAILED, _("Notice"), cp_messageBuffer, qfalse, MSG_STANDARD, NULL);
				prod->spaceMessage = qtrue;
			}
			PR_ProductionRollBottom_f();
			return;
		}
		prod->percentDone += (PR_CalculateProductionPercentDone(base, RS_GetTechForItem(od), NULL) / MINUTES_PER_HOUR);
	} else if (aircraft) {
		/* Not enough money to produce more items in this base. */
		if (aircraft->price * PRODUCE_FACTOR / PRODUCE_DIVISOR > ccs.credits) {
			if (!prod->creditMessage) {
				Com_sprintf(cp_messageBuffer, sizeof(cp_messageBuffer), _("Not enough credits to finish production in %s."), base->name);
				MSO_CheckAddNewMessage(NT_PRODUCTION_FAILED, _("Notice"), cp_messageBuffer, qfalse, MSG_STANDARD, NULL);
				prod->creditMessage = qtrue;
			}
			PR_ProductionRollBottom_f();
			return;
		}
		/* Not enough free space in hangars for this aircraft. */
		if (AIR_CalculateHangarStorage(prod->aircraft, base, 0) <= 0) {
			if (!prod->spaceMessage) {
				Com_sprintf(cp_messageBuffer, sizeof(cp_messageBuffer), _("Not enough free hangar space in %s. Production postponed."), base->name);
				MSO_CheckAddNewMessage(NT_PRODUCTION_FAILED, _("Notice"), cp_messageBuffer, qfalse, MSG_STANDARD, NULL);
				prod->spaceMessage = qtrue;
			}
			PR_ProductionRollBottom_f();
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
			tech = RS_GetTechForItem(od);
		} else if (aircraft) {
			CL_UpdateCredits(ccs.credits - (aircraft->price * PRODUCE_FACTOR / PRODUCE_DIVISOR));
			/* Now add new aircraft. */
			AIR_NewAircraft(base, aircraft);
			name = _(aircraft->tpl->name);
			tech = aircraft->tech;
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
			Com_sprintf(cp_messageBuffer, sizeof(cp_messageBuffer), _("Not enough free storage space in %s. Disassembling postponed."), base->name);
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
 * @brief increases production amount if possible
 * @param[in,out] prod Pointer to the production
 * @param[in] amount Additional amount to add
 * @returns the amount added
 */
int PR_IncreaseProduction (production_t *prod, int amount)
{
	base_t *base;
	const technology_t *tech;

	assert(prod);

	if (prod->ufo)
		return 0;

	base = PR_ProductionBase(prod);
	assert(base);

	/* amount limit per one production */
	if (prod->amount + amount > MAX_PRODUCTION_AMOUNT) {
		amount = max(0, MAX_PRODUCTION_AMOUNT - prod->amount);
	}

	if (prod->item) {
		tech = RS_GetTechForItem(prod->item);
	} else if (prod->aircraft) {
		tech = prod->aircraft->tech;
	} else {
		tech = NULL;
	}
	assert(tech);

	amount = PR_RequirementsMet(amount, &tech->requireForProduction, base);
	if (amount == 0)
		return 0;

	prod->amount += amount;
	PR_UpdateRequiredItemsInBasestorage(base, -amount, &tech->requireForProduction);

	return amount;
}

/**
 * @brief decreases production amount
 * @param[in,out] prod Pointer to the production
 * @param[in] amount Additional amount to remove (positive number)
 * @returns the amount removed
 * @note if production amount falls below 1 it removes the whole production from the queue as well
 */
int PR_DecreaseProduction (production_t *prod, int amount)
{
	base_t *base;
	const technology_t *tech;

	assert(prod);
	base = PR_ProductionBase(prod);
	assert(base);

	if (prod->ufo)
		return 0;

	if (prod->amount <= amount) {
		production_queue_t *queue = PR_GetProductionForBase(base);
		amount = prod->amount;
		PR_QueueDelete(base, queue, prod->idx);
		return amount;
	}

	if (prod->item) {
		tech = RS_GetTechForItem(prod->item);
	} else if (prod->aircraft) {
		tech = prod->aircraft->tech;
	} else if (prod->ufo) {
		assert(prod->ufo->ufoTemplate);
		tech = prod->ufo->ufoTemplate->tech;
	} else {
		Com_Error(ERR_DROP, "No tech pointer for production");
	}
	assert(tech);

	prod->amount += -amount;
	PR_UpdateRequiredItemsInBasestorage(base, amount, &tech->requireForProduction);

	return amount;
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
	base_t *base;

	/* Loop through all founded bases. Then check productions
	 * in global data array. Then increase prod->percentDone and check
	 * whether an item is produced. Then add to base storage. */
	base = NULL;
	while ((base = B_GetNextFounded(base)) != NULL) {
		production_t *prod;
		production_queue_t *q = PR_GetProductionForBase(base);

		/* not actually any active productions */
		if (q->numItems <= 0)
			continue;

		/* Workshop is disabled because their dependences are disabled */
		if (!PR_ProductionAllowed(base))
			continue;

		prod = &q->items[0];

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
	return !B_IsUnderAttack(base) && B_GetBuildingStatus(base, B_WORKSHOP) && E_CountHired(base, EMPL_WORKER) > 0;
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
	capacities_t *workspaceCapacity = &base->capacities[CAP_WORKSPACE];
	assert(base);

	if (workspaceCapacity->max <= 0) {
		PR_EmptyQueue(base);
	}

	if (workspaceCapacity->max >= E_CountHired(base, EMPL_WORKER)) {
		workspaceCapacity->cur = E_CountHired(base, EMPL_WORKER);
	} else {
		workspaceCapacity->cur = workspaceCapacity->max;
	}
}

/**
 * @brief check if an item is producable.
 * @param[in] item Pointer to the item that should be checked.
 */
qboolean PR_ItemIsProduceable (const objDef_t const *item)
{
	const technology_t *tech = RS_GetTechForItem(item);
	return tech->produceTime != -1;
}

/**
 * @brief Returns the base pointer the production belongs to
 * @param[in] production pointer to the production entry
 * @return base_t pointer to the base
 */
base_t *PR_ProductionBase (const production_t *production)
{
	base_t *base = NULL;
	while ((base = B_GetNext(base)) != NULL) {
		const ptrdiff_t diff = ((ptrdiff_t)((production) - PR_GetProductionForBase(base)->items));

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
	base_t *base;
	mxml_node_t *node = mxml_AddNode(p, SAVE_PRODUCE_PRODUCTION);

	base = NULL;
	while ((base = B_GetNextFounded(base)) != NULL) {
		const production_queue_t *pq = PR_GetProductionForBase(base);
		int j;

		mxml_node_t *snode = mxml_AddNode(node, SAVE_PRODUCE_QUEUE);
		/** @todo this should not be the base index */
		mxml_AddInt(snode, SAVE_PRODUCE_QUEUEIDX, base->idx);

		for (j = 0; j < pq->numItems; j++) {
			const production_t *prod = &pq->items[j];
			const objDef_t *item = prod->item;
			const aircraft_t *aircraft = prod->aircraft;
			const storedUFO_t *ufo = prod->ufo;

			mxml_node_t * ssnode = mxml_AddNode(snode, SAVE_PRODUCE_ITEM);
			assert(item || aircraft || ufo);
			if (item)
				mxml_AddString(ssnode, SAVE_PRODUCE_ITEMID, item->id);
			else if (aircraft)
				mxml_AddString(ssnode, SAVE_PRODUCE_AIRCRAFTID, aircraft->id);
			else if (ufo)
				mxml_AddInt(ssnode, SAVE_PRODUCE_UFOIDX, ufo->idx);
			mxml_AddInt(ssnode, SAVE_PRODUCE_AMOUNT, prod->amount);
			mxml_AddFloatValue(ssnode, SAVE_PRODUCE_PERCENTDONE, prod->percentDone);
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
		const int baseIDX = mxml_GetInt(snode, SAVE_PRODUCE_QUEUEIDX, MAX_BASES);
		base_t *base = B_GetBaseByIDX(baseIDX);
		production_queue_t *pq;

		if (base == NULL) {
			Com_Printf("Invalid production queue index %i\n", baseIDX);
			continue;
		}

		pq = PR_GetProductionForBase(base);

		for (ssnode = mxml_GetNode(snode, SAVE_PRODUCE_ITEM); pq->numItems < MAX_PRODUCTIONS && ssnode;
				ssnode = mxml_GetNextNode(ssnode, snode, SAVE_PRODUCE_ITEM)) {
			const char *s1 = mxml_GetString(ssnode, SAVE_PRODUCE_ITEMID);
			const char *s2;
			int ufoIDX;
			production_t *prod = &pq->items[pq->numItems];

			prod->idx = pq->numItems;
			prod->amount = mxml_GetInt(ssnode, SAVE_PRODUCE_AMOUNT, 0);
			prod->percentDone = mxml_GetFloat(ssnode, SAVE_PRODUCE_PERCENTDONE, 0.0);

			/* amount */
			if (prod->amount <= 0) {
				Com_Printf("PR_Load: Production with amount <= 0 dropped (baseidx=%i, production idx=%i).\n",
						baseIDX, pq->numItems);
				continue;
			}
			/* item */
			if (s1[0] != '\0')
				prod->item = INVSH_GetItemByID(s1);
			/* UFO */
			ufoIDX = mxml_GetInt(ssnode, SAVE_PRODUCE_UFOIDX, -1);
			if (ufoIDX != -1) {
				storedUFO_t *ufo = US_GetStoredUFOByIDX(ufoIDX);

				if (!ufo) {
					Com_Printf("PR_Load: Could not find ufo idx: %i\n", ufoIDX);
					continue;
				}

				prod->ufo = ufo;
				prod->production = qfalse;
				prod->ufo->disassembly = prod;
			} else {
				prod->production = qtrue;
			}
			/* aircraft */
			s2 = mxml_GetString(ssnode, SAVE_PRODUCE_AIRCRAFTID);
			if (s2[0] != '\0')
				prod->aircraft = AIR_GetAircraft(s2);

			if (!prod->item && !prod->ufo && !prod->aircraft) {
				Com_Printf("PR_Load: Production is not an item an aircraft nor a disassembly\n");
				continue;
			}

			pq->numItems++;
		}
	}
	return qtrue;
}
