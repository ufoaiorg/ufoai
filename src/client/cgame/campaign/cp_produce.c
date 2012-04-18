/**
 * @file cp_produce.c
 * @brief Single player production stuff
 * @note Production stuff functions prefix: PR_
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
#include "cp_ufo.h"
#include "cp_popup.h"
#include "cp_produce.h"
#include "cp_produce_callbacks.h"
#include "save/save_produce.h"

/** @brief Default amount of workers, the produceTime for technologies is defined. */
/** @note producetime for technology entries is the time for PRODUCE_WORKERS amount of workers. */
static const int PRODUCE_WORKERS = 10;

static cvar_t* mn_production_limit;		/**< Maximum items in queue. */
static cvar_t* mn_production_workers;		/**< Amount of hired workers in base. */
static cvar_t* mn_production_amount;	/**< Amount of the current production; if no production, an invalid value */

/**
 * @brief Calculates the fraction (percentage) of production of an item in 1 minute.
 * @param[in] base Pointer to the base with given production.
 * @param[in] prodData Pointer to the productionData structure.
 * @sa PR_ProductionRun
 * @sa PR_ItemProductionInfo
 * @sa PR_DisassemblyInfo
 * @return 0 if the production does not make any progress, 1 if the whole item is built in 1 minute
 */
static double PR_CalculateProductionPercentPerMinute (const base_t *base, const productionData_t *prodData)
{
	/* Check how many workers hired in this base. */
	const signed int allWorkers = E_CountHired(base, EMPL_WORKER);
	/* We will not use more workers than workspace capacity in this base. */
	const signed int maxWorkers = min(allWorkers, CAP_GetMax(base, CAP_WORKSPACE));
	signed int timeDefault;
	double distanceFactor;

	if (PR_IsProductionData(prodData)) {
		const technology_t *tech = PR_GetTech(prodData);
		/* This is the default production time for PRODUCE_WORKERS workers. */
		timeDefault = tech->produceTime * MINUTES_PER_HOUR;
		distanceFactor = 1.0;
	} else {
		const storedUFO_t *storedUFO = prodData->data.ufo;
		/* This is the default disassemble time for PRODUCE_WORKERS workers. */
		timeDefault = storedUFO->comp->time * MINUTES_PER_HOUR;
		/* Production is 4 times longer when installation is on Antipodes
		 * Penalty starts when distance is greater than 45 degrees */
		distanceFactor = max(1.0, GetDistanceOnGlobe(storedUFO->installation->pos, base->pos) / 45.0);
	}
	if (maxWorkers == PRODUCE_WORKERS) {
		/* No need to calculate: timeDefault is for PRODUCE_WORKERS workers. */
		const double divisor = distanceFactor * timeDefault;
		const double fraction = 1.0 / divisor;
		return min(fraction, 1.0);
	} else {
		const double divisor = PRODUCE_WORKERS * distanceFactor * timeDefault;
		/* Calculate the fraction of item produced for our amount of workers. */
		const double fraction = maxWorkers / divisor;
		/* Don't allow to return fraction greater than 1 (you still need at least 1 hour to produce an item). */
		return min(fraction, 1.0);
	}
}

/**
 * @brief Calculates the total frame count needed for producing an item
 * @param[in] base Pointer to the base the production happen
 * @param[in] prodData Pointer to the productionData structure
 */
static int PR_CalculateTotalFrames (const base_t const *base, const productionData_t const *prodData)
{
	return (1.0 / PR_CalculateProductionPercentPerMinute(base, prodData)) + 1;
}

/**
 * @brief Calculates the remaining time for a technology in minutes
 * @param[in] prod Pointer to the production structure
 */
int PR_GetRemainingMinutes (const production_t const *prod)
{
	assert(prod);
	return prod->totalFrames - prod->frame;
}

/**
 * @brief Calculates the remaining hours for a technology
 * @param[in] prod Pointer to the production structure
 */
int PR_GetRemainingHours (const production_t const *prod)
{
	return round(PR_GetRemainingMinutes(prod) / (double)MINUTES_PER_HOUR);
}

/**
 * @brief Calculates the production time (in hours) for a technology
 * @param[in] base Pointer to the base to calculate production time at
 * @param[in] prodData Pointer to the production data structure
 */
int PR_GetProductionHours (const base_t const *base, const productionData_t const *prodData)
{
	return round(PR_CalculateTotalFrames(base, prodData) / (double)MINUTES_PER_HOUR);
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
 * @param[in] base Pointer to the base to check
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
 * @return The translated name of the item in production
 */
const char* PR_GetName (const productionData_t *data)
{
	switch (data->type) {
	case PRODUCTION_TYPE_ITEM:
		return _(data->data.item->name);
	case PRODUCTION_TYPE_AIRCRAFT:
		return _(data->data.aircraft->tpl->name);
	case PRODUCTION_TYPE_DISASSEMBLY:
		return UFO_TypeToName(data->data.ufo->ufoTemplate->ufotype);
	default:
		Com_Error(ERR_DROP, "Invalid production type given: %i", data->type);
	}
}

/**
 * @return The technology for the item that is assigned to the given production
 */
technology_t* PR_GetTech (const productionData_t *data)
{
	switch (data->type) {
	case PRODUCTION_TYPE_ITEM:
		return RS_GetTechForItem(data->data.item);
	case PRODUCTION_TYPE_AIRCRAFT:
		return data->data.aircraft->tech;
	case PRODUCTION_TYPE_DISASSEMBLY:
		return data->data.ufo->ufoTemplate->tech;
	default:
		return NULL;
	}
}

/**
 * @brief Add a new item to the bottom of the production queue.
 * @param[in] base Pointer to base, where the queue is.
 * @param[in] data The production data
 * @param[in] amount Desired amount to produce.
 */
production_t *PR_QueueNew (base_t *base, const productionData_t const *data, signed int amount)
{
	production_t *prod;
	const technology_t *tech;
	production_queue_t *queue = PR_GetProductionForBase(base);

	if (PR_QueueFreeSpace(base) <= 0)
		return NULL;
	if (E_CountHired(base, EMPL_WORKER) <= 0)
		return NULL;

	/* Initialize */
	prod = &queue->items[queue->numItems];
	OBJZERO(*prod);
	/* self-reference. */
	prod->idx = queue->numItems;
	prod->data = *data;
	prod->amount = amount;

	tech = PR_GetTech(&prod->data);
	if (!tech)
		return NULL;

	prod->totalFrames = PR_CalculateTotalFrames(base, data);

	if (PR_IsDisassemblyData(data)) {
		/* only one item for disassemblies */
		amount = 1;
	} else if (tech->produceTime < 0) {
		/* Don't try to add an item to the queue which is not producible. */
		return NULL;
	}

	amount = PR_RequirementsMet(amount, &tech->requireForProduction, base);
	if (amount == 0)
		return NULL;

	PR_UpdateRequiredItemsInBasestorage(base, -amount, &tech->requireForProduction);

	/* We cannot queue new aircraft if no free hangar space. */
	/** @todo move this check out into a new function */
	if (data->type == PRODUCTION_TYPE_AIRCRAFT) {
		if (!B_GetBuildingStatus(base, B_COMMAND)) {
			/** @todo move popup into menucode */
			CP_Popup(_("Hangars not ready"), _("You cannot queue aircraft.\nNo command centre in this base.\n"));
			return NULL;
		} else if (!B_GetBuildingStatus(base, B_HANGAR) && !B_GetBuildingStatus(base, B_SMALL_HANGAR)) {
			/** @todo move popup into menucode */
			CP_Popup(_("Hangars not ready"), _("You cannot queue aircraft.\nNo hangars in this base.\n"));
			return NULL;
		}
		/** @todo we should also count aircraft that are already in the queue list */
		if (AIR_CalculateHangarStorage(data->data.aircraft, base, 0) <= 0) {
			CP_Popup(_("Hangars not ready"), _("You cannot queue aircraft.\nNo free space in hangars.\n"));
			return NULL;
		}
	}

	/** @todo remove this and make the ufo const */
	if (PR_IsDisassembly(prod))
		prod->data.data.ufo->disassembly = prod;

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

	tech = PR_GetTech(&prod->data);
	if (tech == NULL)
		Com_Error(ERR_DROP, "No tech pointer for production");

	if (PR_IsDisassembly(prod))
		prod->data.data.ufo->disassembly = NULL;

	PR_UpdateRequiredItemsInBasestorage(base, prod->amount, &tech->requireForProduction);

	REMOVE_ELEM_ADJUST_IDX(queue->items, index, queue->numItems);

	/* Adjust ufos' disassembly pointer */
	for (i = index; i < queue->numItems; i++) {
		production_t *disassembly = &queue->items[i];

		if (PR_IsDisassembly(disassembly))
			disassembly->data.data.ufo->disassembly = disassembly;
	}
}

/**
 * @brief Moves the given queue item in the given direction.
 * @param[in] queue Pointer to the queue.
 * @param[in] index The production item index in the queue
 * @param[in] offset The offset relative to the given index where the item should be moved, too
 */
void PR_QueueMove (production_queue_t *queue, int index, int offset)
{
	const int newIndex = max(0, min(index + offset, queue->numItems - 1));
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
		if (PR_IsDisassembly(prod))
			prod->data.data.ufo->disassembly = prod;
	}

	/* copy down */
	for (i = index; i > newIndex; i--) {
		production_t *prod;
		queue->items[i] = queue->items[i - 1];
		prod = &queue->items[i];
		prod->idx = i;
		if (PR_IsDisassembly(prod))
			prod->data.data.ufo->disassembly = prod;
	}

	/* insert item */
	queue->items[newIndex] = saved;
	queue->items[newIndex].idx = newIndex;
	if (PR_IsDisassembly(&queue->items[newIndex]))
		queue->items[newIndex].data.data.ufo->disassembly = &(queue->items[newIndex]);
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
static void PR_ProductionRollBottom (base_t *base)
{
	production_queue_t *queue = PR_GetProductionForBase(base);
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
static int PR_DisassembleItem (base_t *base, const components_t *comp, float condition, qboolean calculate)
{
	int i;
	int size = 0;

	for (i = 0; i < comp->numItemtypes; i++) {
		const objDef_t *compOd = comp->items[i];
		const int amount = (condition < 1 && comp->itemAmount2[i] != COMP_ITEMCOUNT_SCALED) ? comp->itemAmount2[i] : round(comp->itemAmount[i] * condition);

		if (amount <= 0)
			continue;

		assert(compOd);
		size += compOd->size * amount;
		/* Add to base storage only if this is real disassembling, not calculation of size. */
		if (!calculate) {
			if (Q_streq(compOd->id, ANTIMATTER_TECH_ID)) {
				B_ManageAntimatter(base, amount, qtrue);
			} else {
				technology_t *tech = RS_GetTechForItem(compOd);
				B_UpdateStorageAndCapacity(base, compOd, amount, qfalse);
				RS_MarkCollected(tech);
			}
			Com_DPrintf(DEBUG_CLIENT, "PR_DisassembleItem: added %i amounts of %s\n", amount, compOd->id);
		}
	}
	return size;
}

/**
 * @brief Checks whether production can continue
 * @param base The base the production is running in
 * @param prod The production
 * @return @c qfalse if production is stopped, @c qtrue if production can be continued.
 */
static qboolean PR_CheckFrame (base_t *base, production_t *prod)
{
	qboolean state = qtrue;

	if (PR_IsItem(prod)) {
		const objDef_t *od = prod->data.data.item;
		/* Not enough money to produce more items in this base. */
		if (PR_HasInsufficientCredits(od)) {
			if (!prod->creditMessage) {
				Com_sprintf(cp_messageBuffer, sizeof(cp_messageBuffer), _("Not enough credits to finish production in %s."), base->name);
				MSO_CheckAddNewMessage(NT_PRODUCTION_FAILED, _("Notice"), cp_messageBuffer, qfalse, MSG_STANDARD, NULL);
				prod->creditMessage = qtrue;
			}
			state = qfalse;
		}
		/* Not enough free space in base storage for this item. */
		else if (CAP_GetFreeCapacity(base, CAP_ITEMS) < od->size) {
			if (!prod->spaceMessage) {
				Com_sprintf(cp_messageBuffer, sizeof(cp_messageBuffer), _("Not enough free storage space in %s. Production postponed."), base->name);
				MSO_CheckAddNewMessage(NT_PRODUCTION_FAILED, _("Notice"), cp_messageBuffer, qfalse, MSG_STANDARD, NULL);
				prod->spaceMessage = qtrue;
			}
			state = qfalse;
		}
	} else if (PR_IsAircraft(prod)) {
		const aircraft_t *aircraft = prod->data.data.aircraft;
		/* Not enough money to produce more items in this base. */
		if (PR_HasInsufficientCredits(aircraft)) {
			if (!prod->creditMessage) {
				Com_sprintf(cp_messageBuffer, sizeof(cp_messageBuffer), _("Not enough credits to finish production in %s."), base->name);
				MSO_CheckAddNewMessage(NT_PRODUCTION_FAILED, _("Notice"), cp_messageBuffer, qfalse, MSG_STANDARD, NULL);
				prod->creditMessage = qtrue;
			}
			state = qfalse;
		}
		/* Not enough free space in hangars for this aircraft. */
		else if (AIR_CalculateHangarStorage(aircraft, base, 0) <= 0) {
			if (!prod->spaceMessage) {
				Com_sprintf(cp_messageBuffer, sizeof(cp_messageBuffer), _("Not enough free hangar space in %s. Production postponed."), base->name);
				MSO_CheckAddNewMessage(NT_PRODUCTION_FAILED, _("Notice"), cp_messageBuffer, qfalse, MSG_STANDARD, NULL);
				prod->spaceMessage = qtrue;
			}
			state = qfalse;
		}
	} else if (PR_IsDisassembly(prod)) {
		const storedUFO_t *ufo = prod->data.data.ufo;

		if (CAP_GetFreeCapacity(base, CAP_ITEMS) < PR_DisassembleItem(base, ufo->comp, ufo->condition, qtrue)) {
			if (!prod->spaceMessage) {
				Com_sprintf(cp_messageBuffer, sizeof(cp_messageBuffer), _("Not enough free storage space in %s. Disassembling postponed."), base->name);
				MSO_CheckAddNewMessage(NT_PRODUCTION_FAILED, _("Notice"), cp_messageBuffer, qfalse, MSG_STANDARD, NULL);
				prod->spaceMessage = qtrue;
			}
			state = qfalse;
		}
	}

	if (!state)
		PR_ProductionRollBottom(base);

	return state;
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
	if (!PR_IsProduction(prod))
		return;

	if (PR_IsReady(prod)) {
		const char *name = PR_GetName(&prod->data);
		technology_t *tech = PR_GetTech(&prod->data);

		prod->frame = 0;
		prod->amount--;

		if (PR_IsItem(prod)) {
			CP_UpdateCredits(ccs.credits - PR_GetPrice(prod->data.data.item));
			/* Now add it to equipment and update capacity. */
			B_UpdateStorageAndCapacity(base, prod->data.data.item, 1, qfalse);
		} else if (PR_IsAircraft(prod)) {
			CP_UpdateCredits(ccs.credits - PR_GetPrice(prod->data.data.aircraft));
			/* Now add new aircraft. */
			AIR_NewAircraft(base, prod->data.data.aircraft);
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
	if (!PR_IsDisassembly(prod))
		return;

	if (PR_IsReady(prod)) {
		storedUFO_t *ufo = prod->data.data.ufo;
		CAP_AddCurrent(base, CAP_ITEMS, PR_DisassembleItem(base, ufo->comp, ufo->condition, qfalse));

		Com_sprintf(cp_messageBuffer, sizeof(cp_messageBuffer), _("The disassembling of %s at %s has finished."),
				UFO_TypeToName(ufo->ufoTemplate->ufotype), base->name);
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

	if (PR_IsDisassembly(prod))
		return 0;

	base = PR_ProductionBase(prod);
	assert(base);

	/* amount limit per one production */
	if (prod->amount + amount > MAX_PRODUCTION_AMOUNT) {
		amount = max(0, MAX_PRODUCTION_AMOUNT - prod->amount);
	}

	tech = PR_GetTech(&prod->data);
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

	if (PR_IsDisassembly(prod))
		return 0;

	if (prod->amount <= amount) {
		production_queue_t *queue = PR_GetProductionForBase(base);
		amount = prod->amount;
		PR_QueueDelete(base, queue, prod->idx);
		return amount;
	}

	tech = PR_GetTech(&prod->data);
	if (tech == NULL)
		Com_Error(ERR_DROP, "No tech pointer for production");

	prod->amount += -amount;
	PR_UpdateRequiredItemsInBasestorage(base, amount, &tech->requireForProduction);

	return amount;
}

/**
 * @brief Checks whether an item is finished.
 * @note One call each game time minute
 * @sa CP_CampaignRun
 * @sa PR_DisassemblingFrame
 * @sa PR_ProductionFrame
 */
void PR_ProductionRun (void)
{
	base_t *base;

	/* Loop through all founded bases. Then check productions
	 * in global data array. Then increase prod->percentDone and check
	 * whether an item is produced. Then add to base storage. */
	base = NULL;
	while ((base = B_GetNext(base)) != NULL) {
		production_t *prod;
		production_queue_t *q = PR_GetProductionForBase(base);

		/* not actually any active productions */
		if (q->numItems <= 0)
			continue;

		/* Workshop is disabled because their dependences are disabled */
		if (!PR_ProductionAllowed(base))
			continue;

		prod = &q->items[0];
		prod->totalFrames = PR_CalculateTotalFrames(base, &prod->data);

		if (!PR_CheckFrame(base, prod))
			return;

		prod->frame++;

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
	capacities_t *workspaceCapacity = CAP_Get(base, CAP_WORKSPACE);
	int workers;
	assert(base);

	if (workspaceCapacity->max <= 0)
		PR_EmptyQueue(base);

	workers = E_CountHired(base, EMPL_WORKER);
	if (workspaceCapacity->max >= workers)
		workspaceCapacity->cur = workers;
	else
		workspaceCapacity->cur = workspaceCapacity->max;

	/* recalculate time to finish */
	production_queue_t *q = PR_GetProductionForBase(base);
	/* not actually any active productions */
	if (q->numItems <= 0)
		return;
	/* Workshop is disabled because their dependences are disabled */
	if (!PR_ProductionAllowed(base))
		return;

	for (int i = 0; i < q->numItems; i++) {
		production_t *prod = &q->items[i];
		prod->totalFrames = max(prod->frame, PR_CalculateTotalFrames(base, &prod->data));
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
qboolean PR_SaveXML (xmlNode_t *p)
{
	base_t *base;
	xmlNode_t *node = XML_AddNode(p, SAVE_PRODUCE_PRODUCTION);

	base = NULL;
	while ((base = B_GetNext(base)) != NULL) {
		const production_queue_t *pq = PR_GetProductionForBase(base);
		int j;

		xmlNode_t *snode = XML_AddNode(node, SAVE_PRODUCE_QUEUE);
		/** @todo this should not be the base index */
		XML_AddInt(snode, SAVE_PRODUCE_QUEUEIDX, base->idx);

		for (j = 0; j < pq->numItems; j++) {
			const production_t *prod = &pq->items[j];
			xmlNode_t * ssnode = XML_AddNode(snode, SAVE_PRODUCE_ITEM);
			if (PR_IsItem(prod))
				XML_AddString(ssnode, SAVE_PRODUCE_ITEMID, prod->data.data.item->id);
			else if (PR_IsAircraft(prod))
				XML_AddString(ssnode, SAVE_PRODUCE_AIRCRAFTID, prod->data.data.aircraft->id);
			else if (PR_IsDisassembly(prod))
				XML_AddInt(ssnode, SAVE_PRODUCE_UFOIDX, prod->data.data.ufo->idx);
			XML_AddInt(ssnode, SAVE_PRODUCE_AMOUNT, prod->amount);
			XML_AddInt(ssnode, SAVE_PRODUCE_PROGRESS, prod->frame);
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
qboolean PR_LoadXML (xmlNode_t *p)
{
	xmlNode_t *node, *snode;

	node = XML_GetNode(p, SAVE_PRODUCE_PRODUCTION);

	for (snode = XML_GetNode(node, SAVE_PRODUCE_QUEUE); snode;
			snode = XML_GetNextNode(snode, node, SAVE_PRODUCE_QUEUE)) {
		xmlNode_t *ssnode;
		const int baseIDX = XML_GetInt(snode, SAVE_PRODUCE_QUEUEIDX, MAX_BASES);
		base_t *base = B_GetBaseByIDX(baseIDX);
		production_queue_t *pq;

		if (base == NULL) {
			Com_Printf("Invalid production queue index %i\n", baseIDX);
			continue;
		}

		pq = PR_GetProductionForBase(base);

		for (ssnode = XML_GetNode(snode, SAVE_PRODUCE_ITEM); pq->numItems < MAX_PRODUCTIONS && ssnode;
				ssnode = XML_GetNextNode(ssnode, snode, SAVE_PRODUCE_ITEM)) {
			const char *s1 = XML_GetString(ssnode, SAVE_PRODUCE_ITEMID);
			const char *s2;
			int ufoIDX;
			production_t *prod = &pq->items[pq->numItems];

			prod->idx = pq->numItems;
			prod->amount = XML_GetInt(ssnode, SAVE_PRODUCE_AMOUNT, 0);
			prod->frame = XML_GetInt(ssnode, SAVE_PRODUCE_PROGRESS, 0);

			/* amount */
			if (prod->amount <= 0) {
				Com_Printf("PR_LoadXML: Production with amount <= 0 dropped (baseidx=%i, production idx=%i).\n",
						baseIDX, pq->numItems);
				continue;
			}
			/* item */
			if (s1[0] != '\0')
				PR_SetData(&prod->data, PRODUCTION_TYPE_ITEM, INVSH_GetItemByID(s1));
			/* UFO */
			ufoIDX = XML_GetInt(ssnode, SAVE_PRODUCE_UFOIDX, -1);
			if (ufoIDX != -1) {
				storedUFO_t *ufo = US_GetStoredUFOByIDX(ufoIDX);

				if (!ufo) {
					Com_Printf("PR_LoadXML: Could not find ufo idx: %i\n", ufoIDX);
					continue;
				}

				PR_SetData(&prod->data, PRODUCTION_TYPE_DISASSEMBLY, ufo);
				prod->data.data.ufo->disassembly = prod;
			}
			/* aircraft */
			s2 = XML_GetString(ssnode, SAVE_PRODUCE_AIRCRAFTID);
			if (s2[0] != '\0')
				PR_SetData(&prod->data, PRODUCTION_TYPE_AIRCRAFT, AIR_GetAircraft(s2));

			if (!PR_IsDataValid(&prod->data)) {
				Com_Printf("PR_LoadXML: Production is not an item an aircraft nor a disassembly\n");
				continue;
			}

			pq->numItems++;
		}
	}
	return qtrue;
}

/**
 * @brief Set percentDone values after loading the campaign
 * @note it need to be done after B_PostLoadInitCapacity
 * @sa PR_PostLoadInit
 */
static qboolean PR_PostLoadInitProgress (void)
{
	base_t *base = NULL;
	while ((base = B_GetNext(base)) != NULL) {
		production_queue_t *pq = PR_GetProductionForBase(base);
		int j;

		for (j = 0; j < pq->numItems; j++) {
			production_t *prod = &pq->items[j];
			prod->totalFrames = PR_CalculateTotalFrames(base, &prod->data);
		}
	}

	return qtrue;
}

/**
 * @brief actions to do with productions after loading a savegame
 * @sa SAV_GameActionsAfterLoad
 */
qboolean PR_PostLoadInit (void)
{
	return PR_PostLoadInitProgress();
}
