/**
 * @file
 * @brief Single player production stuff
 * @note Production stuff functions prefix: PR_
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
#include "cp_ufo.h"
#include "cp_produce.h"
#include "cp_produce_callbacks.h"
#include "save/save_produce.h"

/** @brief Default amount of workers, the produceTime for technologies is defined. */
/** @note producetime for technology entries is the time for PRODUCE_WORKERS amount of workers. */
static const int PRODUCE_WORKERS = 10;

/**
 * @brief Calculates the total frame count (minutes) needed for producing an item
 * @param[in] base Pointer to the base the production happen
 * @param[in] prodData Pointer to the productionData structure
 */
static int PR_CalculateTotalFrames (const base_t* base, const productionData_t* prodData)
{
	/* Check how many workers hired in this base. */
	const signed int allWorkers = E_CountHired(base, EMPL_WORKER);
	/* We will not use more workers than workspace capacity in this base. */
	const signed int maxWorkers = std::min(allWorkers, CAP_GetMax(base, CAP_WORKSPACE));

	double timeDefault;
	if (PR_IsProductionData(prodData)) {
		const technology_t* tech = PR_GetTech(prodData);
		/* This is the default production time for PRODUCE_WORKERS workers. */
		timeDefault = tech->produceTime;
	} else {
		const storedUFO_t* storedUFO = prodData->data.ufo;
		/* This is the default disassemble time for PRODUCE_WORKERS workers. */
		timeDefault = storedUFO->comp->time;
		/* Production is 4 times longer when installation is on Antipodes
		 * Penalty starts when distance is greater than 45 degrees */
		timeDefault *= std::max(1.0, GetDistanceOnGlobe(storedUFO->installation->pos, base->pos) / 45.0);
	}
	/* Calculate the time needed for production of the item for our amount of workers. */
	const float rate = PRODUCE_WORKERS / ccs.curCampaign->produceRate;
	double const timeScaled = timeDefault * (MINUTES_PER_HOUR * rate) / std::max(1, maxWorkers);

	/* Don't allow to return a time of less than 1 (you still need at least 1 minute to produce an item). */
	return std::max(1.0, timeScaled) + 1;
}

/**
 * @brief Calculates the remaining time for a technology in minutes
 * @param[in] prod Pointer to the production structure
 */
int PR_GetRemainingMinutes (const production_t* prod)
{
	assert(prod);
	return prod->totalFrames - prod->frame;
}

/**
 * @brief Calculates the remaining hours for a technology
 * @param[in] prod Pointer to the production structure
 */
int PR_GetRemainingHours (const production_t* prod)
{
	return round(PR_GetRemainingMinutes(prod) / (double)MINUTES_PER_HOUR);
}

/**
 * @brief Calculates the production time (in hours) for a technology
 * @param[in] base Pointer to the base to calculate production time at
 * @param[in] prodData Pointer to the production data structure
 */
int PR_GetProductionHours (const base_t* base, const productionData_t* prodData)
{
	return round(PR_CalculateTotalFrames(base, prodData) / (double)MINUTES_PER_HOUR);
}

/**
 * @brief Remove or add the required items from/to the a base.
 * @param[in] base Pointer to base.
 * @param[in] amount How many items are planned to be added (positive number) or removed (negative number).
 * @param[in] reqs The production requirements of the item that is to be produced. These included numbers are multiplied with 'amount')
 */
void PR_UpdateRequiredItemsInBasestorage (base_t* base, int amount, const requirements_t* reqs)
{
	if (!base)
		return;

	if (amount == 0)
		return;

	for (int i = 0; i < reqs->numLinks; i++) {
		const requirement_t* req = &reqs->links[i];
		switch (req->type) {
		case RS_LINK_ITEM: {
			const objDef_t* item = req->link.od;
			assert(item);
			B_AddToStorage(base, item, req->amount * amount);
			break;
		}
		case RS_LINK_ANTIMATTER:
			B_AddAntimatter(base, req->amount * amount);
			break;
		case RS_LINK_TECH:
		case RS_LINK_TECH_NOT:
			break;
		default:
			cgi->Com_Error(ERR_DROP, "Invalid requirement for production!\n");
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
int PR_RequirementsMet (int amount, const requirements_t* reqs, base_t* base)
{
	int producibleAmount = amount;

	for (int i = 0; i < reqs->numLinks; i++) {
		const requirement_t* req = &reqs->links[i];

		switch (req->type) {
		case RS_LINK_ITEM: {
				const int items = std::min(amount, B_ItemInBase(req->link.od, base) / ((req->amount) ? req->amount : 1));
				producibleAmount = std::min(producibleAmount, items);
				break;
			}
		case RS_LINK_ANTIMATTER: {
				const int am = std::min(amount, B_AntimatterInBase(base) / ((req->amount) ? req->amount : 1));
				producibleAmount = std::min(producibleAmount, am);
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
 * @return The translated name of the item in production
 */
const char* PR_GetName (const productionData_t* data)
{
	switch (data->type) {
	case PRODUCTION_TYPE_ITEM:
		return _(data->data.item->name);
	case PRODUCTION_TYPE_AIRCRAFT:
		return _(data->data.aircraft->tpl->name);
	case PRODUCTION_TYPE_DISASSEMBLY:
		return UFO_TypeToName(data->data.ufo->ufoTemplate->getUfoType());
	default:
		cgi->Com_Error(ERR_DROP, "Invalid production type given: %i", data->type);
	}
}

/**
 * @return The technology for the item that is assigned to the given production
 */
technology_t* PR_GetTech (const productionData_t* data)
{
	switch (data->type) {
	case PRODUCTION_TYPE_ITEM:
		return RS_GetTechForItem(data->data.item);
	case PRODUCTION_TYPE_AIRCRAFT:
		return data->data.aircraft->tech;
	case PRODUCTION_TYPE_DISASSEMBLY:
		return data->data.ufo->ufoTemplate->tech;
	default:
		return nullptr;
	}
}

static void PR_ResetUFODisassembly (production_t* prod)
{
	/** @todo remove this and make the ufo const */
	if (PR_IsDisassembly(prod))
		prod->data.data.ufo->disassembly = nullptr;
}

static void PR_SetUFODisassembly (production_t* prod)
{
	/** @todo remove this and make the ufo const */
	if (PR_IsDisassembly(prod))
		prod->data.data.ufo->disassembly = prod;
}

/**
 * @brief Add a new item to the bottom of the production queue.
 * @param[in] base Pointer to base, where the queue is.
 * @param[in] data The production data
 * @param[in] amount Desired amount to produce.
 * @return @c NULL in case the production wasn't enqueued, otherwise the production pointer
 */
production_t* PR_QueueNew (base_t* base, const productionData_t* data, signed int amount)
{
	production_queue_t* queue = PR_GetProductionForBase(base);

	if (queue->numItems >= MAX_PRODUCTIONS)
		return nullptr;

	/* Initialize */
	production_t* prod = &queue->items[queue->numItems];
	OBJZERO(*prod);
	/* self-reference. */
	prod->idx = queue->numItems;
	prod->data = *data;
	prod->amount = amount;

	const technology_t* tech = PR_GetTech(&prod->data);
	if (tech == nullptr)
		return nullptr;


	/* only one item for disassemblies */
	if (PR_IsDisassemblyData(data))
		amount = 1;
	else if (tech->produceTime < 0)
		/* Don't try to add an item to the queue which is not producible. */
		return nullptr;

	amount = PR_RequirementsMet(amount, &tech->requireForProduction, base);
	if (amount == 0)
		return nullptr;

	prod->totalFrames = PR_CalculateTotalFrames(base, data);

	PR_UpdateRequiredItemsInBasestorage(base, -amount, &tech->requireForProduction);

	PR_SetUFODisassembly(prod);

	queue->numItems++;
	return prod;
}

/**
 * @brief Delete the selected entry from the queue.
 * @param[in] base Pointer to base, where the queue is.
 * @param[in] queue Pointer to the queue.
 * @param[in] index Selected index in queue.
 */
void PR_QueueDelete (base_t* base, production_queue_t* queue, int index)
{
	production_t* prod = &queue->items[index];
	const technology_t* tech = PR_GetTech(&prod->data);
	if (tech == nullptr)
		cgi->Com_Error(ERR_DROP, "No tech pointer for production");

	PR_ResetUFODisassembly(prod);

	assert(base);
	PR_UpdateRequiredItemsInBasestorage(base, prod->amount, &tech->requireForProduction);

	REMOVE_ELEM_ADJUST_IDX(queue->items, index, queue->numItems);

	/* Adjust ufos' disassembly pointer */
	for (int i = index; i < queue->numItems; i++) {
		production_t* disassembly = &queue->items[i];
		PR_SetUFODisassembly(disassembly);
	}
}

/**
 * @brief Moves the given queue item in the given direction.
 * @param[in] queue Pointer to the queue.
 * @param[in] index The production item index in the queue
 * @param[in] offset The offset relative to the given index where the item should be moved, too
 */
void PR_QueueMove (production_queue_t* queue, int index, int offset)
{
	const int newIndex = std::max(0, std::min(index + offset, queue->numItems - 1));

	if (newIndex == index)
		return;

	production_t saved = queue->items[index];

	/* copy up */
	for (int i = index; i < newIndex; i++) {
		production_t* prod;
		queue->items[i] = queue->items[i + 1];
		prod = &queue->items[i];
		prod->idx = i;
		PR_SetUFODisassembly(prod);
	}

	/* copy down */
	for (int i = index; i > newIndex; i--) {
		production_t* prod;
		queue->items[i] = queue->items[i - 1];
		prod = &queue->items[i];
		prod->idx = i;
		PR_SetUFODisassembly(prod);
	}

	/* insert item */
	queue->items[newIndex] = saved;
	queue->items[newIndex].idx = newIndex;
	PR_SetUFODisassembly(&queue->items[newIndex]);
}

/**
 * @brief Queues the next production in the queue.
 * @param[in] base Pointer to the base.
 */
void PR_QueueNext (base_t* base)
{
	production_queue_t* queue = PR_GetProductionForBase(base);

	PR_QueueDelete(base, queue, 0);

	if (queue->numItems == 0) {
		Com_sprintf(cp_messageBuffer, sizeof(cp_messageBuffer), _("Production queue for %s is empty"), base->name);
		MSO_CheckAddNewMessage(NT_PRODUCTION_QUEUE_EMPTY, _("Production queue empty"), cp_messageBuffer, MSG_PRODUCTION);
	}
}

/**
 * @brief clears the production queue on a base
 */
static void PR_EmptyQueue (base_t* base)
{
	if (!base)
		return;

	production_queue_t* queue = PR_GetProductionForBase(base);
	while (queue->numItems)
		PR_QueueDelete(base, queue, 0);
}

/**
 * @brief moves the first production to the bottom of the list
 */
static void PR_ProductionRollBottom (base_t* base)
{
	production_queue_t* queue = PR_GetProductionForBase(base);
	if (queue->numItems < 2)
		return;

	PR_QueueMove(queue, 0, queue->numItems - 1);
}

/**
 * @brief Checks whether production can continue
 * @param base The base the production is running in
 * @param prod The production
 * @return @c false if production is stopped, @c true if production can be continued.
 */
static bool PR_CheckFrame (base_t* base, production_t* prod)
{
	int price = 0;

	if (PR_IsItem(prod)) {
		const objDef_t* od = prod->data.data.item;
		price = PR_GetPrice(od);
	} else if (PR_IsAircraft(prod)) {
		const aircraft_t* aircraft = prod->data.data.aircraft;
		price = PR_GetPrice(aircraft);
	}

	/* Not enough money */
	if (price > 0 && price > ccs.credits) {
		if (!prod->creditMessage) {
			Com_sprintf(cp_messageBuffer, sizeof(cp_messageBuffer), _("Not enough credits to finish production in %s."), base->name);
			MSO_CheckAddNewMessage(NT_PRODUCTION_FAILED, _("Notice"), cp_messageBuffer);
			prod->creditMessage = true;
		}

		PR_ProductionRollBottom(base);
		return false;
	}

	return true;
}

/**
 * @brief Run actions on finishing production of one item/aircraft/UGV..
 * @param base The base to produce in
 * @param prod The production that is running
 */
static void PR_FinishProduction (base_t* base, production_t* prod)
{
	const char* name = PR_GetName(&prod->data);
	technology_t* tech = PR_GetTech(&prod->data);

	prod->frame = 0;
	prod->amount--;

	if (PR_IsItem(prod)) {
		CP_UpdateCredits(ccs.credits - PR_GetPrice(prod->data.data.item));
		/* Now add it to equipment and update capacity. */
		B_AddToStorage(base, prod->data.data.item, 1);
	} else if (PR_IsAircraft(prod)) {
		CP_UpdateCredits(ccs.credits - PR_GetPrice(prod->data.data.aircraft));
		/* Now add new aircraft. */
		AIR_NewAircraft(base, prod->data.data.aircraft);
	}

	if (prod->amount > 0)
		return;

	Com_sprintf(cp_messageBuffer, sizeof(cp_messageBuffer), _("Work on %s at %s has finished."), name, base->name);
	MSO_CheckAddNewMessage(NT_PRODUCTION_FINISHED, _("Production finished"), cp_messageBuffer, MSG_PRODUCTION, tech);
	/* queue the next production */
	PR_QueueNext(base);
}

/**
 * @brief Run actions on finishing disassembling of a ufo
 * @param base The base to produce in
 * @param prod The production that is running
 */
static void PR_FinishDisassembly (base_t* base, production_t* prod)
{
	storedUFO_t* ufo = prod->data.data.ufo;

	assert(ufo);
	for (int i = 0; i < ufo->comp->numItemtypes; i++) {
		const objDef_t* compOd = ufo->comp->items[i];
		const int amount = (ufo->condition < 1 && ufo->comp->itemAmount2[i] != COMP_ITEMCOUNT_SCALED) ?
			ufo->comp->itemAmount2[i] : round(ufo->comp->itemAmount[i] * ufo->condition);

		assert(compOd);

		if (amount <= 0)
			continue;

		if (Q_streq(compOd->id, ANTIMATTER_ITEM_ID)) {
			B_AddAntimatter(base, amount);
		} else {
			technology_t* tech = RS_GetTechForItem(compOd);
			B_AddToStorage(base, compOd, amount);
			RS_MarkCollected(tech);
		}
	}

	Com_sprintf(cp_messageBuffer, sizeof(cp_messageBuffer), _("The disassembling of %s at %s has finished."),
			UFO_TypeToName(ufo->ufoTemplate->getUfoType()), base->name);
	MSO_CheckAddNewMessage(NT_PRODUCTION_FINISHED, _("Production finished"), cp_messageBuffer, MSG_PRODUCTION, ufo->ufoTemplate->tech);

	/* Removing UFO will remove the production too */
	US_RemoveStoredUFO(ufo);
}

/**
 * @brief increases production amount if possible
 * @param[in,out] prod Pointer to the production
 * @param[in] amount Additional amount to add
 * @returns the amount added
 */
int PR_IncreaseProduction (production_t* prod, int amount)
{
	if (PR_IsDisassembly(prod))
		return 0;

	base_t* base = PR_ProductionBase(prod);
	assert(base);

	/* amount limit per one production */
	if (prod->amount + amount > MAX_PRODUCTION_AMOUNT) {
		amount = std::max(0, MAX_PRODUCTION_AMOUNT - prod->amount);
	}

	const technology_t* tech = PR_GetTech(&prod->data);
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
int PR_DecreaseProduction (production_t* prod, int amount)
{
	assert(prod);
	base_t* base = PR_ProductionBase(prod);
	assert(base);

	if (PR_IsDisassembly(prod))
		return 0;

	if (prod->amount <= amount) {
		production_queue_t* queue = PR_GetProductionForBase(base);
		amount = prod->amount;
		PR_QueueDelete(base, queue, prod->idx);
		return amount;
	}

	const technology_t* tech = PR_GetTech(&prod->data);
	if (tech == nullptr)
		cgi->Com_Error(ERR_DROP, "No tech pointer for production");

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
	/* Loop through all founded bases. Then check productions
	 * in global data array. Then increase prod->percentDone and check
	 * whether an item is produced. Then add to base storage. */
	base_t* base = nullptr;
	while ((base = B_GetNext(base)) != nullptr) {
		production_queue_t* q = PR_GetProductionForBase(base);

		/* not actually any active productions */
		if (q->numItems <= 0)
			continue;

		/* Workshop is disabled because their dependences are disabled */
		if (!PR_ProductionAllowed(base))
			continue;

		production_t* prod = &q->items[0];
		prod->totalFrames = PR_CalculateTotalFrames(base, &prod->data);

		if (!PR_CheckFrame(base, prod))
			return;

		prod->frame++;

		/* If Production/Disassembly is not finished yet, we're done, check next base */
		if (!PR_IsReady(prod))
			continue;

		if (PR_IsProduction(prod))
			PR_FinishProduction(base, prod);
		else if (PR_IsDisassembly(prod))
			PR_FinishDisassembly(base, prod);
	}
}

/**
 * @brief Returns true if the current base is able to produce items
 * @param[in] base Pointer to the base.
 * @sa B_BaseInit_f
 */
bool PR_ProductionAllowed (const base_t* base)
{
	assert(base);
	return !B_IsUnderAttack(base) && B_GetBuildingStatus(base, B_WORKSHOP) && E_CountHired(base, EMPL_WORKER) > 0;
}

/**
 * @brief Update the current capacity of Workshop
 * @param[in] base Pointer to the base containing workshop.
 * @param[in] workerChange Number of workers going to be hired/fired
 */
void PR_UpdateProductionCap (base_t* base, int workerChange)
{
	assert(base);
	capacities_t* workspaceCapacity = CAP_Get(base, CAP_WORKSPACE);

	if (workspaceCapacity->max <= 0)
		PR_EmptyQueue(base);

	const int workers = E_CountHired(base, EMPL_WORKER) + workerChange;
	if (workspaceCapacity->max >= workers)
		workspaceCapacity->cur = workers;
	else
		workspaceCapacity->cur = workspaceCapacity->max;

	/* recalculate time to finish */
	production_queue_t* q = PR_GetProductionForBase(base);
	/* not actually any active productions */
	if (q->numItems <= 0)
		return;
	/* Workshop is disabled because their dependences are disabled */
	if (!PR_ProductionAllowed(base))
		return;

	for (int i = 0; i < q->numItems; i++) {
		production_t* prod = &q->items[i];
		prod->totalFrames = std::max(prod->frame, PR_CalculateTotalFrames(base, &prod->data));
	}
}

/**
 * @brief check if an item is producable.
 * @param[in] item Pointer to the item that should be checked.
 */
bool PR_ItemIsProduceable (const objDef_t* item)
{
	const technology_t* tech = RS_GetTechForItem(item);
	return tech->produceTime != -1;
}

/**
 * @brief Returns the base pointer the production belongs to
 * @param[in] production pointer to the production entry
 * @return base_t pointer to the base
 */
base_t* PR_ProductionBase (const production_t* production)
{
	base_t* base = nullptr;
	while ((base = B_GetNext(base)) != nullptr) {
		const ptrdiff_t diff = ((ptrdiff_t)((production) - PR_GetProductionForBase(base)->items));

		if (diff >= 0 && diff < MAX_PRODUCTIONS)
			return base;
	}
	return nullptr;
}

/**
 * @brief Save callback for savegames in XML Format
 * @param[out] p XML Node structure, where we write the information to
 * @sa PR_LoadXML
 * @sa SAV_GameSaveXML
 */
bool PR_SaveXML (xmlNode_t* p)
{
	xmlNode_t* node = cgi->XML_AddNode(p, SAVE_PRODUCE_PRODUCTION);

	base_t* base = nullptr;
	while ((base = B_GetNext(base)) != nullptr) {
		const production_queue_t* pq = PR_GetProductionForBase(base);

		xmlNode_t* snode = cgi->XML_AddNode(node, SAVE_PRODUCE_QUEUE);
		/** @todo this should not be the base index */
		cgi->XML_AddInt(snode, SAVE_PRODUCE_QUEUEIDX, base->idx);

		for (int j = 0; j < pq->numItems; j++) {
			const production_t* prod = &pq->items[j];
			xmlNode_t* ssnode = cgi->XML_AddNode(snode, SAVE_PRODUCE_ITEM);
			if (PR_IsItem(prod))
				cgi->XML_AddString(ssnode, SAVE_PRODUCE_ITEMID, prod->data.data.item->id);
			else if (PR_IsAircraft(prod))
				cgi->XML_AddString(ssnode, SAVE_PRODUCE_AIRCRAFTID, prod->data.data.aircraft->id);
			else if (PR_IsDisassembly(prod))
				cgi->XML_AddInt(ssnode, SAVE_PRODUCE_UFOIDX, prod->data.data.ufo->idx);
			cgi->XML_AddInt(ssnode, SAVE_PRODUCE_AMOUNT, prod->amount);
			cgi->XML_AddInt(ssnode, SAVE_PRODUCE_PROGRESS, prod->frame);
		}
	}
	return true;
}

/**
 * @brief Load callback for xml savegames
 * @param[in] p XML Node structure, where we get the information from
 * @sa PR_SaveXML
 * @sa SAV_GameLoadXML
 */
bool PR_LoadXML (xmlNode_t* p)
{
	xmlNode_t* node = cgi->XML_GetNode(p, SAVE_PRODUCE_PRODUCTION);

	for (xmlNode_t* snode = cgi->XML_GetNode(node, SAVE_PRODUCE_QUEUE); snode;
			snode = cgi->XML_GetNextNode(snode, node, SAVE_PRODUCE_QUEUE)) {
		xmlNode_t* ssnode;
		const int baseIDX = cgi->XML_GetInt(snode, SAVE_PRODUCE_QUEUEIDX, MAX_BASES);
		base_t* base = B_GetBaseByIDX(baseIDX);
		production_queue_t* pq;

		if (base == nullptr) {
			Com_Printf("Invalid production queue index %i\n", baseIDX);
			continue;
		}

		pq = PR_GetProductionForBase(base);

		for (ssnode = cgi->XML_GetNode(snode, SAVE_PRODUCE_ITEM); pq->numItems < MAX_PRODUCTIONS && ssnode;
				ssnode = cgi->XML_GetNextNode(ssnode, snode, SAVE_PRODUCE_ITEM)) {
			const char* s1 = cgi->XML_GetString(ssnode, SAVE_PRODUCE_ITEMID);
			production_t* prod = &pq->items[pq->numItems];

			prod->idx = pq->numItems;
			prod->amount = cgi->XML_GetInt(ssnode, SAVE_PRODUCE_AMOUNT, 0);
			prod->frame = cgi->XML_GetInt(ssnode, SAVE_PRODUCE_PROGRESS, 0);

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
			const int ufoIDX = cgi->XML_GetInt(ssnode, SAVE_PRODUCE_UFOIDX, -1);
			if (ufoIDX != -1) {
				storedUFO_t* ufo = US_GetStoredUFOByIDX(ufoIDX);

				if (!ufo) {
					Com_Printf("PR_LoadXML: Could not find ufo idx: %i\n", ufoIDX);
					continue;
				}

				PR_SetData(&prod->data, PRODUCTION_TYPE_DISASSEMBLY, ufo);
				PR_SetUFODisassembly(prod);
			}
			/* aircraft */
			const char* s2 = cgi->XML_GetString(ssnode, SAVE_PRODUCE_AIRCRAFTID);
			if (s2[0] != '\0')
				PR_SetData(&prod->data, PRODUCTION_TYPE_AIRCRAFT, AIR_GetAircraft(s2));

			if (!PR_IsDataValid(&prod->data)) {
				Com_Printf("PR_LoadXML: Production is not an item an aircraft nor a disassembly\n");
				continue;
			}

			pq->numItems++;
		}
	}
	return true;
}

/**
 * @brief Set percentDone values after loading the campaign
 * @note it need to be done after B_PostLoadInitCapacity
 * @sa PR_PostLoadInit
 */
static bool PR_PostLoadInitProgress (void)
{
	base_t* base = nullptr;
	while ((base = B_GetNext(base)) != nullptr) {
		production_queue_t* pq = PR_GetProductionForBase(base);
		for (int j = 0; j < pq->numItems; j++) {
			production_t* prod = &pq->items[j];
			prod->totalFrames = PR_CalculateTotalFrames(base, &prod->data);
		}
	}

	return true;
}

/**
 * @brief actions to do with productions after loading a savegame
 * @sa SAV_GameActionsAfterLoad
 */
bool PR_PostLoadInit (void)
{
	return PR_PostLoadInitProgress();
}
