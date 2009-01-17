/**
 * @file cl_produce.c
 * @brief Single player production stuff
 * @note Production stuff functions prefix: PR_
 */

/*
Copyright (C) 2002-2007 UFO: Alien Invasion team.

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
#include "../cl_global.h"
#include "../cl_game.h"
#include "../menu/m_popup.h"

/** Maximum number of produced/disassembled items. */
#define MAX_PRODUCTION_AMOUNT 500

static int produceCategory = FILTER_S_PRIMARY;	/**< Holds the current active production category/filter type.
												 * @sa itemFilterTypes_t */

static production_t *selectedProduction = NULL;	/**< Holds the current active selected queue entry. */

/** A list if all producable items. */
static linkedList_t *productionItemList;

/** Currently selected entry in the productionItemList (depends on content) */
static objDef_t *selectedItem = NULL;
static components_t *selectedDisassembly = NULL;
static aircraft_t *selectedAircraft = NULL;


/** @brief Used in production costs (to allow reducing prices below 1x). */
static const int PRODUCE_FACTOR = 1;
static const int PRODUCE_DIVISOR = 2;

/** @brief Default amount of workers, the produceTime for technologies is defined. */
/** @note producetime for technology entries is the time for PRODUCE_WORKERS amount of workers. */
static const int PRODUCE_WORKERS = 10;

/** @brief Number of blank lines between queued items and tech list. */
static const int QUEUE_SPACERS = 2;

static cvar_t* mn_production_limit;		/**< Maximum items in queue. */
static cvar_t* mn_production_workers;		/**< Amount of hired workers in base. */
static cvar_t* mn_production_amount;	/**< Amount of the current production; if no production, an invalide value */

static menuNode_t *node1, *node2, *prodlist;

/**
 * @brief Resets all "selected" pointers to NULL.
 */
static void PR_ClearSelected (void)
{
	selectedProduction = NULL;
	selectedAircraft = NULL;
	selectedItem = NULL;
	selectedDisassembly = NULL;
}

/**
 * @brief Conditions for disassembling.
 * @param[in] base Pointer to base where the component is stored.
 * @param[in] comp Pointer to components definition.
 * @return qtrue if disassembling is ready, qfalse otherwise.
 */
static qboolean PR_ConditionsDisassembly (const base_t *base, const components_t *comp)
{
	const objDef_t *od;

	assert(base);
	assert(comp);

	od = comp->asItem;
	assert(od);

	if (RS_IsResearched_ptr(od->tech) && base->storage.num[od->idx] > 0)
		return qtrue;
	else
		return qfalse;
}

/**
 * @brief Calculates the fraction (percentage) of production of an item in 1 hour.
 * @param[in] base Pointer to the base with given production.
 * @param[in] tech Pointer to the technology for given production.
 * @param[in] comp Pointer to components definition.
 * @param[in] disassembly True if calculations for disassembling, false otherwise.
 * @sa PR_ProductionRun
 * @sa PR_ItemProductionInfo
 * @sa PR_DisassemblyInfo
 * @return 0 if the production does not make any progress, 1 if the whole item is built in 1 hour
 */
static float PR_CalculateProductionPercentDone (const base_t *base, const technology_t *tech, const components_t *comp, qboolean disassembly)
{
	signed int allworkers = 0, maxworkers = 0;
	signed int timeDefault = 0;
	assert(base);
	assert(tech);

	/* Check how many workers hired in this base. */
	allworkers = E_CountHired(base, EMPL_WORKER);
	/* We will not use more workers than base capacity. */
	if (allworkers > base->capacities[CAP_WORKSPACE].max) {
		maxworkers = base->capacities[CAP_WORKSPACE].max;
	} else {
		maxworkers = allworkers;
	}

	if (!disassembly) {
		assert(tech);
		timeDefault = tech->produceTime;	/* This is the default production time for 10 workers. */
	} else {
		assert(comp);
		timeDefault = comp->time;		/* This is the default disassembly time for 10 workers. */
	}

	if (maxworkers == PRODUCE_WORKERS) {
		/* No need to calculate: timeDefault is for PRODUCE_WORKERS workers. */
		const float fraction = 1.0f / timeDefault;
		Com_DPrintf(DEBUG_CLIENT, "PR_CalculatePercentDone: workers: %i, tech: %s, percent: %f\n",
			maxworkers, tech->id, fraction);
		return fraction;
	} else {
		/* Calculate the fraction of item produced for our amount of workers. */
		/* NOTE: I changed algorithm for a more realistic one, variing like maxworkers^2 -- Kracken 2007/11/18
		 * now, production time is divided by 4 each time you double the number of worker */
		const float fraction = ((float)maxworkers / (PRODUCE_WORKERS * timeDefault))
			* ((float)maxworkers / PRODUCE_WORKERS);
		Com_DPrintf(DEBUG_CLIENT, "PR_CalculatePercentDone: workers: %i, tech: %s, percent: %f\n",
			maxworkers, tech->id, fraction);
		/* Don't allow to return fraction greater than 1 (you still need at least 1 hour to produce an item). */
		if (fraction > 1.0f)
			return 1.0f;
		else
			return fraction;
	}
}

/**
 * @brief Checks if the production requirements are met for a defined amount.
 * @param[in] amount How many items are planned to be produced.
 * @param[in] reqs The production requirements of the item that is to be produced.
 * @param[in] base Pointer to base.
 * @return 0: If nothing can be produced. 1+: If anything can be produced. 'amount': Maximum.
 */
static int PR_RequirementsMet (int amount, requirements_t *reqs, base_t *base)
{
	int a, i;
	int producibleAmount = 0;

	for (a = 0; a < amount; a++) {
		qboolean producible = qtrue;
		for (i = 0; i < reqs->numLinks; i++) {
			requirement_t *req = &reqs->links[i];
			if (req->type == RS_LINK_ITEM) {
				/* The same code is used in "RS_RequirementsMet" */
				Com_DPrintf(DEBUG_CLIENT, "PR_RequirementsMet: %s\n", req->id);
				if (B_ItemInBase(req->link, base) < req->amount) {
					producible = qfalse;
				}
			}
		}
		if (producible)
			producibleAmount++;
		else
			break;
	}
	return producibleAmount;
}

/**
 * @brief Remove or add the required items from/to the a base.
 * @param[in] base Pointer to base.
 * @param[in] amount How many items are planned to be added (positive number) or removed (negative number).
 * @param[in] reqs The production requirements of the item that is to be produced. Thes included numbers are multiplied with 'amount')
 * @todo This doesn't check yet if there are more items removed than are in the base-storage (might be fixed if we used a storage-fuction with checks, otherwise we can make it a 'contition' in order to run this function.
 */
static void PR_UpdateRequiredItemsInBasestorage (base_t *base, int amount, requirements_t *reqs)
{
	int i;
	equipDef_t *ed;

	if (!base)
		return;

	ed = &base->storage;
	if (!ed)
		return;

	if (amount == 0)
		return;

	for (i = 0; i < reqs->numLinks; i++) {
		requirement_t *req = &reqs->links[i];
		if (req->type == RS_LINK_ITEM) {
			const objDef_t *item = req->link;
			assert(req->link);
			if (amount > 0) {
				/* Add items to the base-storage. */
				ed->num[item->idx] += (req->amount * amount);
			} else { /* amount < 0 */
				/* Remove items from the base-storage. */
				ed->num[item->idx] -= (req->amount * -amount);
			}
		}
	}
}

/**
 * @brief Add a new item to the bottom of the production queue.
 * @param[in] base Pointer to base, where the queue is.
 * @param[in] queue Pointer to the queue.
 * @param[in] item Item to add.
 * @param[in] aircraftTemplate aircraft to add.
 * @param[in] amount Desired amount to produce.
 * @param[in] disassembling True if this is disassembling, false if production.
 */
static production_t *PR_QueueNew (base_t *base, production_queue_t *queue, objDef_t *item, aircraft_t *aircraftTemplate, signed int amount, qboolean disassembling)
{
	int numWorkshops = 0;
	production_t *prod;
	const technology_t *tech;

	assert((item && !aircraftTemplate) || (!item && aircraftTemplate));
	assert(base);

	if (queue->numItems >= MAX_PRODUCTIONS)
		return NULL;

	if (E_CountHired(base, EMPL_WORKER) <= 0) {
		MN_Popup(_("Not enough workers"), _("You cannot queue productions without workers hired in this base.\n\nHire workers."));
		return NULL;
	}

	numWorkshops = max(B_GetNumberOfBuildingsInBaseByBuildingType(base, B_WORKSHOP), 0);

	if (queue->numItems >= numWorkshops * MAX_PRODUCTIONS_PER_WORKSHOP) {
		MN_Popup(_("Not enough workshops"), _("You cannot queue more items.\nBuild more workshops.\n"));
		return NULL;
	}

	/* Initialize */
	prod = &queue->items[queue->numItems];
	memset(prod, 0, sizeof(*prod));

	prod->idx = queue->numItems;	/**< Initialize self-reference. */

	if (produceCategory != FILTER_AIRCRAFT)
		tech = item->tech;
	else
		tech = aircraftTemplate->tech;

	/* We cannot queue new aircraft if no free hangar space. */
	if (produceCategory == FILTER_AIRCRAFT) {
		if (!B_GetBuildingStatus(base, B_COMMAND)) {
			MN_Popup(_("Hangars not ready"), _("You cannot queue aircraft.\nNo command centre in this base.\n"));
			return NULL;
		} else if (!B_GetBuildingStatus(base, B_HANGAR) && !B_GetBuildingStatus(base, B_SMALL_HANGAR)) {
			MN_Popup(_("Hangars not ready"), _("You cannot queue aircraft.\nNo hangars in this base.\n"));
			return NULL;
		}
		/** @todo we should also count aircraft that are already in the queue list */
		if (AIR_CalculateHangarStorage(aircraftTemplate, base, 0) <= 0) {
			MN_Popup(_("Hangars not ready"), _("You cannot queue aircraft.\nNo free space in hangars.\n"));
			return NULL;
		}
	}

	prod->item = item;
	prod->aircraft = aircraftTemplate;
	prod->amount = amount;
	if (disassembling) {	/* Disassembling. */
		prod->production = qfalse;

		/* We have to remove amount of items being disassembled from base storage. */
		base->storage.num[item->idx] -= amount;
		/* Now find related components definition. */
		prod->percentDone = 0.0f;
	} else {	/* Production. */
		prod->production = qtrue;

		/* Don't try to add to queue an item which is not producible. */
		if (tech->produceTime < 0)
			return NULL;
		else
			prod->percentDone = 0.0f;
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
static void PR_QueueDelete (base_t *base, production_queue_t *queue, int index)
{
	int i;
	const objDef_t *od;
	production_t *prod;

	prod = &queue->items[index];

	assert(base);

	if (prod->items_cached && !prod->aircraft) {
		/* Get technology of the item in the selected queue-entry. */
		od = prod->item;
		if (od->tech) {
			/* Add all items listed in the prod.-requirements /multiplied by amount) to the storage again. */
			PR_UpdateRequiredItemsInBasestorage(base, prod->amount, &od->tech->require_for_production);
		} else {
			Com_DPrintf(DEBUG_CLIENT, "PR_QueueDelete: Problem getting technology entry for %i\n", index);
		}
		prod->items_cached = qfalse;
	}

	/* Read disassembly to base storage. */
	if (!prod->production)
		base->storage.num[prod->item->idx] += prod->amount;

	queue->numItems--;
	if (queue->numItems < 0)
		queue->numItems = 0;

	/* Copy up other items. */
	for (i = index; i < queue->numItems; i++) {
		queue->items[i] = queue->items[i + 1];
		/* Fix index in list */
		queue->items[i].idx = i;
	}
}

/**
 * @brief Moves the given queue item in the given direction.
 * @param[in] queue Pointer to the queue.
 * @param[in] index
 * @param[in] dir
 */
static void PR_QueueMove (production_queue_t *queue, int index, int dir)
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
	}

	/* copy down */
	for (i = index; i > newIndex; i--) {
		queue->items[i] = queue->items[i - 1];
		queue->items[i].idx = i;
	}

	/* insert item */
	queue->items[newIndex] = saved;
	queue->items[newIndex].idx = newIndex;
}

/**
 * @brief update the list of queued and available items
 * @param[in] base Pointer to the base.
 */
static void PR_UpdateProductionList (const base_t* base)
{
	int i, j, counter;
	static char productionList[1024];
	static char productionQueued[256];
	static char productionAmount[256];
	const production_queue_t *queue;

	assert(base);

	productionAmount[0] = productionList[0] = productionQueued[0] = '\0';
	queue = &gd.productions[base->idx];

	/* First add all the queue items ... */
	for (i = 0; i < queue->numItems; i++) {
		const production_t *prod = &queue->items[i];
		if (!prod->aircraft) {
			const objDef_t *od = prod->item;
			Q_strcat(productionList, va("%s\n", _(od->name)), sizeof(productionList));
			Q_strcat(productionAmount, va("%i\n", base->storage.num[prod->item->idx]), sizeof(productionAmount));
			Q_strcat(productionQueued, va("%i\n", prod->amount), sizeof(productionQueued));
		} else {
			const aircraft_t *aircraftTemplate = prod->aircraft;
			Q_strcat(productionList, va("%s\n", _(aircraftTemplate->name)), sizeof(productionList));
			for (j = 0, counter = 0; j < gd.numAircraft; j++) {
				const aircraft_t *aircraftBase = AIR_AircraftGetFromIDX(j);
				assert(aircraftBase);
				if (aircraftBase->homebase == base && aircraftBase->tpl == aircraftTemplate)
					counter++;
			}
			Q_strcat(productionAmount, va("%i\n", counter), sizeof(productionAmount));
			Q_strcat(productionQueued, va("%i\n", prod->amount), sizeof(productionQueued));
		}
	}

	/* Then spacers ... */
	for (i = 0; i < QUEUE_SPACERS; i++) {
		Q_strcat(productionList, "\n", sizeof(productionList));
		Q_strcat(productionAmount, "\n", sizeof(productionAmount));
		Q_strcat(productionQueued, "\n", sizeof(productionQueued));
	}

	LIST_Delete(&productionItemList);

	/* Then go through all object definitions ... */
	if (produceCategory == FILTER_DISASSEMBLY) {
		for (i = 0; i < gd.numComponents; i++) {
			const objDef_t *asOd = gd.components[i].asItem;
			components_t *comp = &gd.components[i];
			if (!asOd)
				continue;
			if (PR_ConditionsDisassembly(base, comp)) {
				LIST_AddPointer(&productionItemList, comp);

				Q_strcat(productionList, va("%s\n", _(asOd->name)), sizeof(productionList));
				Q_strcat(productionAmount, va("%i\n", base->storage.num[asOd->idx]), sizeof(productionAmount));
				Q_strcat(productionQueued, "\n", sizeof(productionQueued));
			}
		}
	} else if (produceCategory == FILTER_AIRCRAFT) {
		for (i = 0; i < numAircraftTemplates; i++) {
			aircraft_t *aircraftTemplate = &aircraftTemplates[i];
			/* don't allow producing ufos */
			if (aircraftTemplate->ufotype != UFO_MAX)
				continue;
			if (!aircraftTemplate->tech) {
				Com_Printf("PR_UpdateProductionList: no technology for craft %s!\n", aircraftTemplate->id);
				continue;
			}
			Com_DPrintf(DEBUG_CLIENT, "air: %s ufotype: %i tech: %s time: %i\n", aircraftTemplate->id, aircraftTemplate->ufotype, aircraftTemplate->tech->id, aircraftTemplate->tech->produceTime);
			if (aircraftTemplate->tech->produceTime > 0 && RS_IsResearched_ptr(aircraftTemplate->tech)) {
				LIST_AddPointer(&productionItemList, aircraftTemplate);

				Q_strcat(productionList, va("%s\n", _(aircraftTemplate->name)), sizeof(productionList));
				for (j = 0, counter = 0; j < gd.numAircraft; j++) {
					const aircraft_t *aircraftBase = AIR_AircraftGetFromIDX(j);
					assert(aircraftBase);
					if (aircraftBase->homebase == base
					 && aircraftBase->tpl == aircraftTemplate)
						counter++;
				}
				Q_strcat(productionAmount, va("%i\n", counter), sizeof(productionAmount));
				Q_strcat(productionQueued, "\n", sizeof(productionQueued));
			}
		}
	} else {
		objDef_t *od;
		for (i = 0, od = csi.ods; i < csi.numODs; i++, od++) {
			assert(od->tech);
			/* We will not show items with producetime = -1 - these are not producible.
			 * We can produce what was researched before. */
			if (INV_ItemMatchesFilter(od, produceCategory)
			 && RS_IsResearched_ptr(od->tech) && od->name[0] != '\0'
			 && od->tech->produceTime > 0) {
				LIST_AddPointer(&productionItemList, od);

				Q_strcat(productionList, va("%s\n", _(od->name)), sizeof(productionList));
				Q_strcat(productionAmount, va("%i\n", base->storage.num[i]), sizeof(productionAmount));
				Q_strcat(productionQueued, "\n", sizeof(productionQueued));
			}
		}
	}

	/* bind the menu text to our static char array */
	mn.menuText[TEXT_PRODUCTION_LIST] = productionList;
	/* bind the amount of available items */
	mn.menuText[TEXT_PRODUCTION_AMOUNT] = productionAmount;
	/* bind the amount of queued items */
	mn.menuText[TEXT_PRODUCTION_QUEUED] = productionQueued;
}

/**
 * @brief Queues the next production in the queue.
 * @param[in] base Pointer to the base.
 */
static void PR_QueueNext (base_t *base)
{
	production_queue_t *queue = &gd.productions[base->idx];

	PR_QueueDelete(base, queue, 0);
	if (queue->numItems == 0) {
		PR_ClearSelected();
		Com_sprintf(mn.messageBuffer, sizeof(mn.messageBuffer), _("Production queue for base %s is empty"), base->name);
		MSO_CheckAddNewMessage(NT_PRODUCTION_QUEUE_EMPTY,_("Production queue empty"), mn.messageBuffer, qfalse, MSG_PRODUCTION, NULL);
		return;
	} else if (selectedProduction && selectedProduction->idx >= queue->numItems) {
		PR_ClearSelected();
		selectedProduction = &queue->items[queue->numItems - 1];
		PR_UpdateProductionList(base);
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

	queue = &gd.productions[base->idx];

	/* #ifdef PARANOID ? */
	if (!queue)
		return;
	/* #endif */

	while (queue->numItems)
		PR_QueueDelete(base, queue, 0);
	PR_ClearSelected();
}

/**
 * @brief moves the first production to the bottom of the list
 */
static void PR_ProductionRollBottom_f (void)
{
	production_queue_t *queue;

	if (!baseCurrent)
		return;

	queue = &gd.productions[baseCurrent->idx];

	if (queue->numItems < 2)
		return;

	PR_QueueMove(queue, 0, queue->numItems - 1);
	PR_UpdateProductionList(baseCurrent);
}

/**
 * @brief Disassembles item, adds components to base storage and calculates all components size.
 * @param[in] base Pointer to base where the disassembling is being made.
 * @param[in] comp Pointer to components definition.
 * @param[in] calculate True if this is only calculation of item size, false if this is real disassembling.
 * @return Size of all components in this disassembling.
 */
static int PR_DisassembleItem (base_t *base, components_t *comp, qboolean calculate)
{
	int i, size = 0;

	assert(comp);
	if (!calculate && !base)	/* We need base only if this is real disassembling. */
		Sys_Error("PR_DisassembleItem: No base given");

	for (i = 0; i < comp->numItemtypes; i++) {
		const objDef_t *compOd = comp->items[i];
		assert(compOd);
		size += compOd->size * comp->item_amount[i];
		/* Add to base storage only if this is real disassembling, not calculation of size. */
		if (!calculate) {
			if (!Q_strncmp(compOd->id, "antimatter", 10))
				B_ManageAntimatter(base, comp->item_amount[i], qtrue);
			else
				B_UpdateStorageAndCapacity(base, compOd, comp->item_amount[i], qfalse, qfalse);
			Com_DPrintf(DEBUG_CLIENT, "PR_DisassembleItem: added %i amounts of %s\n", comp->item_amount[i], compOd->id);
		}
	}
	return size;
}

/**
 * @brief Checks whether an item is finished.
 * @sa CL_CampaignRun
 */
void PR_ProductionRun (void)
{
	int i;
	const objDef_t *od;
	const aircraft_t *aircraft;
	production_t *prod;

	/* Loop through all founded bases. Then check productions
	 * in global data array. Then increase prod->percentDone and check
	 * wheter an item is produced. Then add to base storage. */
	for (i = 0; i < MAX_BASES; i++) {
		base_t *base = B_GetFoundedBaseByIDX(i);
		if (!base)
			continue;

		/* not actually any active productions */
		if (gd.productions[i].numItems <= 0)
			continue;

		/* Workshop is disabled because their dependences are disabled */
		if (!PR_ProductionAllowed(base))
			continue;

		prod = &gd.productions[i].items[0];

		if (prod->item) {
			od = prod->item;
			aircraft = NULL;
		} else {
			assert(prod->aircraft);
			od = NULL;
			aircraft = prod->aircraft;
		}

		if (prod->production) {	/* This is production, not disassembling. */
			if (!prod->aircraft) {
				/* Not enough money to produce more items in this base. */
				if (od->price * PRODUCE_FACTOR / PRODUCE_DIVISOR > ccs.credits) {
					if (!prod->creditmessage) {
						Com_sprintf(mn.messageBuffer, sizeof(mn.messageBuffer), _("Not enough credits to finish production in base %s.\n"), base->name);
						MSO_CheckAddNewMessage(NT_PRODUCTION_FAILED, _("Notice"), mn.messageBuffer, qfalse, MSG_STANDARD, NULL);
						prod->creditmessage = qtrue;
					}
					PR_ProductionRollBottom_f();
					continue;
				}
				/* Not enough free space in base storage for this item. */
				if (base->capacities[CAP_ITEMS].max - base->capacities[CAP_ITEMS].cur < od->size) {
					if (!prod->spacemessage) {
						Com_sprintf(mn.messageBuffer, sizeof(mn.messageBuffer), _("Not enough free storage space in base %s. Production postponed.\n"), base->name);
						MSO_CheckAddNewMessage(NT_PRODUCTION_FAILED, _("Notice"), mn.messageBuffer, qfalse, MSG_STANDARD, NULL);
						prod->spacemessage = qtrue;
					}
					PR_ProductionRollBottom_f();
					continue;
				}
			} else {
				/* Not enough money to produce more items in this base. */
				if (aircraft->price * PRODUCE_FACTOR / PRODUCE_DIVISOR > ccs.credits) {
					if (!prod->creditmessage) {
						Com_sprintf(mn.messageBuffer, sizeof(mn.messageBuffer), _("Not enough credits to finish production in base %s.\n"), base->name);
						MSO_CheckAddNewMessage(NT_PRODUCTION_FAILED, _("Notice"), mn.messageBuffer, qfalse, MSG_STANDARD, NULL);
						prod->creditmessage = qtrue;
					}
					PR_ProductionRollBottom_f();
					continue;
				}
				/* Not enough free space in hangars for this aircraft. */
				if (AIR_CalculateHangarStorage(prod->aircraft, base, 0) <= 0) {
					if (!prod->spacemessage) {
						Com_sprintf(mn.messageBuffer, sizeof(mn.messageBuffer), _("Not enough free hangar space in base %s. Production postponed.\n"), base->name);
						MSO_CheckAddNewMessage(NT_PRODUCTION_FAILED, _("Notice"), mn.messageBuffer, qfalse, MSG_STANDARD, NULL);
						prod->spacemessage = qtrue;
					}
					PR_ProductionRollBottom_f();
					continue;
				}
			}
		} else {		/* This is disassembling. */
			if (base->capacities[CAP_ITEMS].max - base->capacities[CAP_ITEMS].cur < PR_DisassembleItem(NULL, INV_GetComponentsByItem(prod->item), qtrue)) {
				if (!prod->spacemessage) {
					Com_sprintf(mn.messageBuffer, sizeof(mn.messageBuffer), _("Not enough free storage space in base %s. Disassembling postponed.\n"), base->name);
					MSO_CheckAddNewMessage(NT_PRODUCTION_FAILED, _("Notice"), mn.messageBuffer, qfalse, MSG_STANDARD, NULL);
					prod->spacemessage = qtrue;
				}
				PR_ProductionRollBottom_f();
				continue;
			}
		}

#ifdef DEBUG
		if (!prod->aircraft && !od->tech)
			Sys_Error("PR_ProductionRun: No tech pointer for object %s ('%s')\n", prod->item->id, od->id);
#endif
		if (prod->production) {	/* This is production, not disassembling. */
			if (!prod->aircraft)
				prod->percentDone += PR_CalculateProductionPercentDone(base, od->tech, NULL, qfalse);
			else
				prod->percentDone += PR_CalculateProductionPercentDone(base, aircraft->tech, NULL, qfalse);
		} else /* This is disassembling. */
			prod->percentDone += PR_CalculateProductionPercentDone(base, od->tech, INV_GetComponentsByItem(prod->item), qtrue);

		if (prod->percentDone >= 1.0f) {
			if (prod->production) {	/* This is production, not disassembling. */
				if (!prod->aircraft) {
					CL_UpdateCredits(ccs.credits - (od->price * PRODUCE_FACTOR / PRODUCE_DIVISOR));
					prod->percentDone = 0.0f;
					prod->amount--;
					/* Now add it to equipment and update capacity. */
					B_UpdateStorageAndCapacity(base, prod->item, 1, qfalse, qfalse);

					/* queue the next production */
					if (prod->amount <= 0) {
						Com_sprintf(mn.messageBuffer, sizeof(mn.messageBuffer), _("The production of %s has finished."), _(od->name));
						MSO_CheckAddNewMessage(NT_PRODUCTION_FINISHED, _("Production finished"), mn.messageBuffer, qfalse, MSG_PRODUCTION, od->tech);
						PR_QueueNext(base);
					}
				} else {
					CL_UpdateCredits(ccs.credits - (aircraft->price * PRODUCE_FACTOR / PRODUCE_DIVISOR));
					prod->percentDone = 0.0f;
					prod->amount--;
					/* Now add new aircraft. */
					AIR_NewAircraft(base, aircraft->id);
					/* queue the next production */
					if (prod->amount <= 0) {
						Com_sprintf(mn.messageBuffer, sizeof(mn.messageBuffer), _("The production of %s has finished."), _(aircraft->name));
						MSO_CheckAddNewMessage(NT_PRODUCTION_FINISHED, _("Production finished"), mn.messageBuffer, qfalse, MSG_PRODUCTION, NULL);
						PR_QueueNext(base);
					}
				}
			} else {	/* This is disassembling. */
				base->capacities[CAP_ITEMS].cur += PR_DisassembleItem(base, INV_GetComponentsByItem(prod->item), qfalse);
				prod->percentDone = 0.0f;
				prod->amount--;
				/* If this is aircraft dummy item, update UFO hangars capacity. */
				if (od->tech->type == RS_CRAFT) {
					const aircraft_t *ufocraft = AIR_GetAircraft(od->id);
					assert(ufocraft);
					if (ufocraft->size == AIRCRAFT_LARGE) {
						/* Large UFOs can only be stored in Large UFO Hangar */
						if (base->capacities[CAP_UFOHANGARS_LARGE].cur > 0)
							base->capacities[CAP_UFOHANGARS_LARGE].cur--;
						else
							/* Should never be reached */
							Com_Printf("PR_ProductionRun: Can not find %s in large UFO Hangar\n", ufocraft->name);
					} else {
						/* Small UFOs can only be stored in Small UFO Hangar */
						if (base->capacities[CAP_UFOHANGARS_SMALL].cur > 0)
							base->capacities[CAP_UFOHANGARS_SMALL].cur--;
						else
							/* Should never be reached */
							Com_Printf("PR_ProductionRun: Can not find %s in small UFO Hangar\n", ufocraft->name);
						}
				}
				if (prod->amount <= 0) {
					Com_sprintf(mn.messageBuffer, sizeof(mn.messageBuffer), _("The disassembling of %s has finished."), _(od->name));
					MSO_CheckAddNewMessage(NT_PRODUCTION_FINISHED, _("Production finished"), mn.messageBuffer, qfalse, MSG_PRODUCTION, od->tech);
					PR_QueueNext(base);
				}
			}
		}
	}
}

/**
 * @brief Prints information about the selected item (no aircraft) in production.
 * @param[in] base Pointer to the base where informations should be printed.
 * @sa PR_ProductionInfo
 */
static void PR_ItemProductionInfo (const base_t *base, const objDef_t *od, float percentDone)
{
	static char productionInfo[512];
	int time;
	float prodPerHour;

	assert(base);
	assert(od);
	assert(od->tech);

	/* Don't try to display an item which is not producible. */
	if (od->tech->produceTime < 0) {
		Com_sprintf(productionInfo, sizeof(productionInfo), _("No item selected"));
		Cvar_Set("mn_item", "");
	} else {
		prodPerHour = PR_CalculateProductionPercentDone(base, od->tech, NULL, qfalse);
		/* If you entered production menu, that means that prodPerHour > 0 (must not divide by 0) */
		assert(prodPerHour > 0);
		time = ceil((1.0f - percentDone) / prodPerHour);

		Com_sprintf(productionInfo, sizeof(productionInfo), "%s\n", _(od->name));
		Q_strcat(productionInfo, va(_("Costs per item\t%i c\n"), (od->price * PRODUCE_FACTOR / PRODUCE_DIVISOR)),
			sizeof(productionInfo));
		Q_strcat(productionInfo, va(_("Production time\t%ih\n"), time), sizeof(productionInfo));
		Q_strcat(productionInfo, va(_("Item size\t%i\n"), od->size), sizeof(productionInfo));
		Cvar_Set("mn_item", od->id);
	}
	mn.menuText[TEXT_PRODUCTION_INFO] = productionInfo;
}

/**
 * @brief Prints information about the selected disassembly task
 * @param[in] base Pointer to the base where informations should be printed.
 * @sa PR_ProductionInfo
 */
static void PR_DisassemblyInfo (const base_t *base, const objDef_t *od, const components_t *comp, float percentDone)
{
	static char productionInfo[512];
	int time, i;
	float prodPerHour;

	assert(base);
	assert(od);
	assert(comp);
	assert(od->tech);

	prodPerHour = PR_CalculateProductionPercentDone(base, od->tech, comp, qtrue);
	/* If you entered production menu, that means that prodPerHour > 0 (must not divide by 0) */
	assert(prodPerHour > 0);
	time = ceil((1.0f - percentDone) / prodPerHour);

	Com_sprintf(productionInfo, sizeof(productionInfo), _("%s - disassembly\n"), _(od->name));
	Q_strcat(productionInfo, _("Components: "), sizeof(productionInfo));
	/* Print components. */
	for (i = 0; i < comp->numItemtypes; i++) {
		const objDef_t *compOd = comp->items[i];
		assert(compOd);
		Q_strcat(productionInfo, va(_("%s (%i) "), _(compOd->name), comp->item_amount[i]),
			sizeof(productionInfo));
	}
	Q_strcat(productionInfo, "\n", sizeof(productionInfo));
	Q_strcat(productionInfo, va(_("Disassembly time\t%ih\n"), time), sizeof(productionInfo));
	Cvar_Set("mn_item", od->id);
	mn.menuText[TEXT_PRODUCTION_INFO] = productionInfo;
}

/**
 * @brief Prints information about the selected aircraft in production.
 * @param[in] aircraftTemplate The aircraft to print the information for
 * @sa PR_ProductionInfo
 */
static void PR_AircraftInfo (const aircraft_t *aircraftTemplate)
{
	static char productionInfo[512];
	assert(aircraftTemplate);

	Com_sprintf(productionInfo, sizeof(productionInfo), "%s\n", _(aircraftTemplate->name));
	Q_strcat(productionInfo, va(_("Production costs\t%i c\n"), (aircraftTemplate->price * PRODUCE_FACTOR / PRODUCE_DIVISOR)),
		sizeof(productionInfo));
	assert(aircraftTemplate->tech);
	Q_strcat(productionInfo, va(_("Production time\t%ih\n"), aircraftTemplate->tech->produceTime), sizeof(productionInfo));
	mn.menuText[TEXT_PRODUCTION_INFO] = productionInfo;
	Cvar_Set("mn_item", aircraftTemplate->id);
}

/**
 * @brief Prints information about the selected item in production.
 * @param[in] base Pointer to the base where informations should be printed.
 * @sa PR_AircraftInfo
 * @sa PR_ItemProductionInfo
 * @sa PR_DisassemblyInfo
 */
static void PR_ProductionInfo (const base_t *base)
{
	if (selectedProduction) {
		production_t *prod = selectedProduction;
		MN_ExecuteConfunc("prod_taskselected");
		if (prod->aircraft) {
			PR_AircraftInfo(prod->aircraft);
		} else if (prod->production) {
			PR_ItemProductionInfo(base, prod->item, prod->percentDone);
		} else {
			PR_DisassemblyInfo(base, prod->item, INV_GetComponentsByItem(prod->item), prod->percentDone);
		}
		Cvar_SetValue("mn_production_amount", selectedProduction->amount);

	} else {
		if (selectedAircraft) {
			MN_ExecuteConfunc("prod_availableselected");
			PR_AircraftInfo(selectedAircraft);
		} else if (selectedItem) {
			MN_ExecuteConfunc("prod_availableselected");
			PR_ItemProductionInfo(base, selectedItem, 0.0);
		} else if (selectedDisassembly) {
			MN_ExecuteConfunc("prod_availableselected");
			PR_DisassemblyInfo(base, selectedDisassembly->asItem, selectedDisassembly, 0.0);
		} else {
			MN_ExecuteConfunc("prod_nothingselected");
			if (produceCategory == FILTER_AIRCRAFT)
				mn.menuText[TEXT_PRODUCTION_INFO] = _("No aircraft selected.");
			else
				mn.menuText[TEXT_PRODUCTION_INFO] = _("No item selected");
			Cvar_Set("mn_item", "");
		}
	}
}

/**
 * @brief Click function for production list
 * @note Opens the UFOpaedia - by right clicking an item
 */
static void PR_ProductionListRightClick_f (void)
{
	int num;
	production_queue_t *queue;

	/* can be called from everywhere without a started game */
	if (!baseCurrent || !GAME_CP_IsRunning())
		return;
	queue = &gd.productions[baseCurrent->idx];

	/* not enough parameters */
	if (Cmd_Argc() < 2) {
		Com_Printf("Usage: %s <arg>\n", Cmd_Argv(0));
		return;
	}

	/* clicked which item? */
	num = atoi(Cmd_Argv(1));

	/* Clicked the production queue or the item list? */
	if (num < queue->numItems && num >= 0) {
		selectedProduction = &queue->items[num];
		if (selectedProduction->aircraft) {
			assert(selectedProduction->aircraft->tech);
			UP_OpenWith(selectedProduction->aircraft->tech->id);
		} else {
			const objDef_t *od = selectedProduction->item;
			assert(od->tech);
			UP_OpenWith(od->tech->id);
		}
	} else if (num >= queue->numItems + QUEUE_SPACERS) {
		/* Clicked in the item list. */
		const int idx = num - queue->numItems - QUEUE_SPACERS;

		if (produceCategory == FILTER_AIRCRAFT) {
			const aircraft_t *aircraftTemplate = (aircraft_t*)LIST_GetByIdx(productionItemList, idx);
			/* aircraftTemplate may be empty if rclicked below real entry
			 * ufo research definition must not have a tech assigned
			 * only RS_CRAFT types have
			 * @sa RS_InitTree */
			if (aircraftTemplate && aircraftTemplate->tech)
				UP_OpenWith(aircraftTemplate->tech->id);
		} else if (produceCategory == FILTER_DISASSEMBLY) {
			components_t *comp = (components_t*)LIST_GetByIdx(productionItemList, idx);
			if (comp && comp->asItem && comp->asItem->tech) {
				UP_OpenWith(comp->asItem->tech->id);
			}
		} else {
			objDef_t *od = (objDef_t*)LIST_GetByIdx(productionItemList, idx);
#ifdef DEBUG
			if (!od) {
				Com_DPrintf(DEBUG_CLIENT, "PR_ProductionListRightClick_f: No item found at the list-position %i!\n", idx);
				return;
			}

			if (!od->tech)
				Sys_Error("PR_ProductionListRightClick_f: No tech pointer for object '%s'\n", od->id);
#endif

			/* Open up UFOpaedia for this entry. */
			if (INV_ItemMatchesFilter(od, produceCategory) && RS_IsResearched_ptr(od->tech)) {
				PR_ClearSelected();
				selectedItem = od;
				UP_OpenWith(od->tech->id);
				return;
			}
		}
	}
#ifdef DEBUG
	else
		Com_DPrintf(DEBUG_CLIENT, "PR_ProductionListRightClick_f: Click on spacer %i\n", num);
#endif
}

/**
 * @brief Click function for production list.
 * @note "num" is the entry in the visible production list (includes queued entries and spaces).
 */
static void PR_ProductionListClick_f (void)
{
	int num;
	production_queue_t *queue;
	base_t* base;

	/* can be called from everywhere without a started game */
	if (!baseCurrent || !GAME_CP_IsRunning())
		return;

	base = baseCurrent;
	queue = &gd.productions[base->idx];

	/* Break if there are not enough parameters. */
	if (Cmd_Argc() < 2) {
		Com_Printf("Usage: %s <arg>\n", Cmd_Argv(0));
		return;
	}

	/* Clicked which item? */
	num = atoi(Cmd_Argv(1));

	/* Clicked the production queue or the item list? */
	if (num < queue->numItems && num >= 0) {
		selectedProduction = &queue->items[num];
		PR_ProductionInfo(base);
	} else if (num >= queue->numItems + QUEUE_SPACERS) {
		/* Clicked in the item list. */
		const int idx = num - queue->numItems - QUEUE_SPACERS;

		if (produceCategory == FILTER_DISASSEMBLY) {
			components_t *comp = (components_t*)LIST_GetByIdx(productionItemList, idx);

			PR_ClearSelected();
			selectedDisassembly = comp;

			PR_ProductionInfo(base);
		} else if (produceCategory == FILTER_AIRCRAFT) {
			aircraft_t *aircraftTemplate = (aircraft_t*)LIST_GetByIdx(productionItemList, idx);
			if (!aircraftTemplate) {
				Com_DPrintf(DEBUG_CLIENT, "PR_ProductionListClick_f: No item found at the list-position %i!\n", idx);
				return;
			}
			/* ufo research definition must not have a tech assigned
			 * only RS_CRAFT types have
			 * @sa RS_InitTree */
			if (aircraftTemplate->tech
			 && aircraftTemplate->tech->produceTime >= 0
			 && RS_IsResearched_ptr(aircraftTemplate->tech)) {
				PR_ClearSelected();
				selectedAircraft = aircraftTemplate;
				PR_ProductionInfo(base);
			}
		} else {
			objDef_t *od = (objDef_t*)LIST_GetByIdx(productionItemList, idx);

			if (!od) {
				Com_DPrintf(DEBUG_CLIENT, "PR_ProductionListClick_f: No item found at the list-position %i!\n", idx);
				return;
			}

			if (!od->tech)
				Sys_Error("PR_ProductionListClick_f: No tech pointer for object '%s'\n", od->id);
			/* We can only produce items that fulfill the following conditions... */
			if (INV_ItemMatchesFilter(od, produceCategory)	/* Item is in the current inventory-category */
			 && RS_IsResearched_ptr(od->tech)		/* Tech is researched */
			 && od->tech->produceTime >= 0) {		/* Item is producible */
				assert(*od->name);

				PR_ClearSelected();
				selectedItem = od;
				PR_ProductionInfo(base);
			}
		}
	}
#ifdef DEBUG
	else
		Com_DPrintf(DEBUG_CLIENT, "PR_ProductionListClick_f: Click on spacer %i\n", num);
#endif
}

/**
 * @brief Will select a new tab on the production list.
 */
static void PR_ProductionSelect_f (void)
{
	int cat;

	/* Can be called from everywhere without a started game. */
	if (!baseCurrent || !GAME_CP_IsRunning())
		return;

	if (Cmd_Argc() < 2) {
		Com_Printf("Usage: %s <category>\n", Cmd_Argv(0));
		return;
	}

	cat = atoi(Cmd_Argv(1));

	/* Check if the given category index is valid. */
	if (cat < MAX_FILTERTYPES && cat >= FILTER_S_PRIMARY) {	/**< Check for valid bounds */
		produceCategory = cat;
		Cvar_Set("mn_itemtype", va("%d", produceCategory));
		Cvar_Set("mn_itemtypename", _(BS_BuyTypeName(produceCategory)));
	} else {
		return;
	}

	/* Reset scroll values of the list. */
	node1->u.text.textScroll = node2->u.text.textScroll = prodlist->u.text.textScroll = 0;

	/* Update list of entries for current production tab. */
	PR_UpdateProductionList(baseCurrent);

	/* Reset selected entry, if it was not from the queue */
	selectedItem = NULL;
	selectedDisassembly = NULL;
	selectedAircraft = NULL;

	/* Select first entry in the list (if any). */
	if (LIST_Count(productionItemList) > 0) {
		if (produceCategory == FILTER_AIRCRAFT)
			selectedAircraft = (aircraft_t*)LIST_GetByIdx(productionItemList, 0);
		else if (produceCategory == FILTER_DISASSEMBLY)
			selectedDisassembly = (components_t*)LIST_GetByIdx(productionItemList, 0);
		else
			selectedItem = (objDef_t*)LIST_GetByIdx(productionItemList, 0);
	}
	/* update selection index if first entry of actual list was chosen */
	if (!selectedProduction) {
		MN_TextNodeSelectLine(prodlist, gd.productions[baseCurrent->idx].numItems + QUEUE_SPACERS);
	}

	/* Update displayed info about selected entry (if any). */
	PR_ProductionInfo(baseCurrent);
}

/**
 * @brief Will fill the list of producible items.
 * @note Some of Production Menu related cvars are being set here.
 */
static void PR_ProductionList_f (void)
{
	char tmpbuf[MAX_VAR];
	int numWorkshops = 0;

	if (Cmd_Argc() < 2) {
		Com_Printf("Usage: %s <type>\n", Cmd_Argv(0));
		return;
	}

	/* can be called from everywhere without a started game */
	if (!baseCurrent || !GAME_CP_IsRunning())
		return;

	numWorkshops = max(0, B_GetNumberOfBuildingsInBaseByBuildingType(baseCurrent, B_WORKSHOP));

	Cvar_SetValue("mn_production_limit", MAX_PRODUCTIONS_PER_WORKSHOP * numWorkshops);
	Cvar_SetValue("mn_production_basecap", baseCurrent->capacities[CAP_WORKSPACE].max);

	/* Set amount of workers - all/ready to work (determined by base capacity. */
	PR_UpdateProductionCap(baseCurrent);

	Com_sprintf(tmpbuf, sizeof(tmpbuf), "%i/%i",
		baseCurrent->capacities[CAP_WORKSPACE].cur,
		E_CountHired(baseCurrent, EMPL_WORKER));
	Cvar_Set("mn_production_workers", tmpbuf);

	Com_sprintf(tmpbuf, sizeof(tmpbuf), "%i/%i",
		baseCurrent->capacities[CAP_ITEMS].cur,
		baseCurrent->capacities[CAP_ITEMS].max);
	Cvar_Set("mn_production_storage", tmpbuf);

	produceCategory = atoi(Cmd_Argv(1));
	PR_ClearSelected();
	Cmd_ExecuteString(va("prod_select %i", produceCategory));
}

/**
 * @brief Rolls produceCategory left in Produce menu.
 */
static void BS_Prev_ProduceType_f (void)
{
	produceCategory--;

	/* Skip internal filter types */
	if (produceCategory == MAX_SOLDIER_FILTERTYPES)
		produceCategory--;

	if (produceCategory < 0) {
		produceCategory = MAX_FILTERTYPES - 1;
	} else if (produceCategory >= MAX_FILTERTYPES) {
		produceCategory = FILTER_S_PRIMARY;	/* First entry of itemFilterTypes_t */
	}

	Cbuf_AddText(va("prod_select %i\n", produceCategory));
}

/**
 * @brief Rolls produceCategory right in Produce menu.
 */
static void BS_Next_ProduceType_f (void)
{
	produceCategory++;

	/* Skip internal filter types */
	if (produceCategory == MAX_SOLDIER_FILTERTYPES)
		produceCategory++;

	if (produceCategory < 0) {
		produceCategory = MAX_FILTERTYPES - 1;
	} else if (produceCategory >= MAX_FILTERTYPES) {
		produceCategory = FILTER_S_PRIMARY;	/* First entry of itemFilterTypes_t */
	}

	Cbuf_AddText(va("prod_select %i\n", produceCategory));
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
		MN_ExecuteConfunc("set_prod_enabled");
		return qtrue;
	} else {
		MN_ExecuteConfunc("set_prod_disabled");
		return qfalse;
	}
}

/**
 * @brief Function binding for prod_scroll that scrolls other text nodes, too
 */
static void PR_ProductionListScroll_f (void)
{
	assert(node1);
	assert(node2);
	assert(prodlist);

	node1->u.text.textScroll = node2->u.text.textScroll = prodlist->u.text.textScroll;
}

void PR_ProductionInit (void)
{
	Com_DPrintf(DEBUG_CLIENT, "Reset all productions\n");
	mn_production_limit = Cvar_Get("mn_production_limit", "0", 0, NULL);
	mn_production_workers = Cvar_Get("mn_production_workers", "0", 0, NULL);
	mn_production_amount = Cvar_Get("mn_production_amount", "0", 0, NULL);
}

/**
 * @sa CL_InitAfter
 */
void PR_Init (void)
{
	const menu_t* menu = MN_GetMenu("production");
	if (!menu)
		Sys_Error("Could not find the production menu\n");

	prodlist = MN_GetNode(menu, "prodlist");
	node1 = MN_GetNode(menu, "prodlist_amount");
	node2 = MN_GetNode(menu, "prodlist_queued");

	if (!prodlist || !node1 || !node2)
		Sys_Error("Could not find the needed menu nodes in production menu\n");
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
 * @brief Increases the production amount by given parameter.
 */
static void PR_ProductionIncrease_f (void)
{
	int amount = 1, amountTemp = 0;
	production_queue_t *queue;
	production_t *prod;
	base_t* base;

	if (!baseCurrent)
		return;

	if (!(selectedProduction || selectedAircraft || selectedItem
			|| selectedDisassembly))
		return;

	base = baseCurrent;

	if (Cmd_Argc() == 2)
		amount = atoi(Cmd_Argv(1));

	queue = &gd.productions[base->idx];

	if (selectedProduction) {
		prod = selectedProduction;
		if (prod->production) {		/* Production. */
			if (prod->aircraft) {
				/* Don't allow to queue more aircraft if there is no free space. */
				if (AIR_CalculateHangarStorage(prod->aircraft, base, 0) <= 0) {
					MN_Popup(_("Hangars not ready"), _("You cannot queue aircraft.\nNo free space in hangars.\n"));
					return;
				}
			}

			/* Check if we can add more items. */
			if (prod->amount + amount > MAX_PRODUCTION_AMOUNT) {
				if (MAX_PRODUCTION_AMOUNT - prod->amount >= 0) {
					/* Add as many items as allowed. */
					prod->amount = MAX_PRODUCTION_AMOUNT;
				} else {
					return;
				}
			} else {
				prod->amount += amount;
			}
		} else {	/* Disassembling. */
			/* We can disassembly only as many items as we have in base storage. */
			if (base->storage.num[prod->item->idx] > amount)
				amountTemp = amount;
			else
				amountTemp = base->storage.num[prod->item->idx];
			Com_DPrintf(DEBUG_CLIENT, "PR_ProductionIncrease_f: amounts: storage: %i, param: %i, temp: %i\n", base->storage.num[prod->item->idx], amount, amountTemp);

			/* Now check if we can add more items and
			 * remove the amount we just added to queue from base storage. */
			if (prod->amount + amountTemp > MAX_PRODUCTION_AMOUNT) {
				if (MAX_PRODUCTION_AMOUNT - prod->amount >= 0) {
					/* Calculate the maximum allowed amount to be added. */
					amountTemp = MAX_PRODUCTION_AMOUNT - prod->amount;
				} else {
					return;
				}
			}

			base->storage.num[prod->item->idx] -= amountTemp;
			prod->amount += amountTemp;
		}
	} else {
		if (!selectedDisassembly) {
			if (selectedAircraft && AIR_CalculateHangarStorage(selectedAircraft, base, 0) <= 0) {
				MN_Popup(_("Hangars not ready"), _("You cannot queue aircraft.\nNo free space in hangars.\n"));
				return;
			}
			prod = PR_QueueNew(base, queue, selectedItem, selectedAircraft, amount, qfalse);	/* Production. (only of of the "selected" pointer can be nonNULL) */
		} else {
			/* We can disassemble only as many items as we have in base storage. */
			assert(selectedDisassembly && selectedDisassembly->asItem);

			if (base->storage.num[selectedDisassembly->asItem->idx] > amount)
				amountTemp = amount;
			else
				amountTemp = base->storage.num[selectedDisassembly->asItem->idx];

			/* Check if we can add more items. */
			if (amountTemp > MAX_PRODUCTION_AMOUNT) {
				/* Add as many items as possible. */
				amountTemp = MAX_PRODUCTION_AMOUNT;
			}

			prod = PR_QueueNew(base, queue, selectedDisassembly->asItem, NULL, amountTemp, qtrue);	/* Disassembling. */
		}

		/** prod is NULL when queue limit is reached
		 * @todo this popup hides any previous popup, like popup created in PR_QueueNew */
		if (!prod) {
			/* Oops! Too many items! */
			MN_Popup(_("Queue full!"), _("You cannot queue any more items!"));
			return;
		}

		if (produceCategory != FILTER_AIRCRAFT) {
			/* Get technology of the item in the selected queue-entry. */
			const objDef_t *od = prod->item;
			int producibleAmount = amount;
			if (od->tech)
				producibleAmount = PR_RequirementsMet(amount, &od->tech->require_for_production, base);

			if (producibleAmount > 0) {	/* Check if production requirements have been (even partially) met. */
				if (od->tech) {
					/* Remove the additionally required items (multiplied by 'producibleAmount') from base-storage.*/
					PR_UpdateRequiredItemsInBasestorage(base, -amount, &od->tech->require_for_production);
					prod->items_cached = qtrue;
				}

				if (producibleAmount < amount) {
					/** @todo make the numbers work here. */
					MN_Popup(_("Not enough material!"), va(_("You don't have enough material to produce all (%i) items. Production will continue with a reduced (%i) number."), amount, producibleAmount));
				}

				if (!selectedDisassembly) {
					Com_sprintf(mn.messageBuffer, sizeof(mn.messageBuffer), _("Production of %s started"), _(od->name));
					MSO_CheckAddNewMessage(NT_PRODUCTION_STARTED, _("Production started"), mn.messageBuffer, qfalse, MSG_PRODUCTION, od->tech);
				} else {
					Com_sprintf(mn.messageBuffer, sizeof(mn.messageBuffer), _("Disassembling of %s started"), _(od->name));
					MSO_CheckAddNewMessage(NT_PRODUCTION_STARTED, _("Production started"), mn.messageBuffer, qfalse, MSG_PRODUCTION, od->tech);
				}

				/* Now we select the item we just created. */
				PR_ClearSelected();
				selectedProduction = &queue->items[queue->numItems - 1];
			} else { /* requirements are not met => producibleAmount <= 0 */
				/** @todo better messages needed */
				MN_Popup(_("Not enough material!"), _("You don't have enough of the needed material to produce this item."));
				/** @todo
				 *  -) need to popup something like: "You need the following items in order to produce more of ITEM:   x of ITEM, x of ITEM, etc..."
				 *     This info should also be displayed in the item-info.
				 *  -) can can (if possible) change the 'amount' to a vlalue that _can_ be produced (i.e. the maximum amount possible).*/
			}
		} else {
			const aircraft_t *aircraftTemplate = prod->aircraft;
			assert(aircraftTemplate);
			assert(aircraftTemplate == aircraftTemplate->tpl);

			Com_DPrintf(DEBUG_CLIENT, "Increasing %s\n", aircraftTemplate->name);
			Com_sprintf(mn.messageBuffer, sizeof(mn.messageBuffer), _("Production of %s started"), _(aircraftTemplate->name));
			MSO_CheckAddNewMessage(NT_PRODUCTION_STARTED, _("Production started"), mn.messageBuffer, qfalse, MSG_PRODUCTION, NULL);
			/* Now we select the item we just created. */
			PR_ClearSelected();
			selectedProduction = &queue->items[queue->numItems - 1];
		}
	}

	PR_ProductionInfo(base);
	PR_UpdateProductionList(base);
}

/**
 * @brief Stops the current running production
 */
static void PR_ProductionStop_f (void)
{
	production_queue_t *queue;
	base_t* base;

	if (!baseCurrent || !selectedProduction)
		return;

	base = baseCurrent;

	queue = &gd.productions[base->idx];

	PR_QueueDelete(base, queue, selectedProduction->idx);

	if (queue->numItems == 0) {
		selectedProduction = NULL;
	} else if (selectedProduction->idx >= queue->numItems) {
		selectedProduction = &queue->items[queue->numItems - 1];
	}

	PR_ProductionInfo(base);
	PR_UpdateProductionList(base);
}

/**
 * @brief Decrease the production amount by given parameter
 */
static void PR_ProductionDecrease_f (void)
{
	int amount = 1, amountTemp = 0;
	production_queue_t *queue;
	production_t *prod;
	base_t* base;

	if (Cmd_Argc() == 2)
		amount = atoi(Cmd_Argv(1));

	if (!baseCurrent || !selectedProduction)
		return;

	base = baseCurrent;

	queue = &gd.productions[base->idx];
	prod = selectedProduction;
	if (prod->amount >= amount)
		amountTemp = amount;
	else
		amountTemp = prod->amount;

	prod->amount -= amountTemp;
	/* We need to readd items being disassembled to base storage. */
	if (!prod->production)
		base->storage.num[prod->item->idx] += amountTemp;

	if (prod->amount <= 0) {
		PR_ProductionStop_f();
	} else {
		PR_ProductionInfo(base);
		PR_UpdateProductionList(base);
	}
}

/**
 * @brief Change the production amount by given diff.
 */
static void PR_ProductionChange_f (void)
{
	int amount;

	if (!baseCurrent)
		return;

	if (!(selectedProduction || selectedAircraft || selectedItem
			|| selectedDisassembly))
		return;

	if (Cmd_Argc() != 2) {
		Com_Printf("Usage: %s <diff> : change the production amount\n", Cmd_Argv(0));
		return;
	}

	amount = atoi(Cmd_Argv(1));
	if (amount > 0) {
		Cbuf_AddText(va("prod_inc %i\n", amount));
	} else {
		Cbuf_AddText(va("prod_dec %i\n", -amount));
	}
}

/**
 * @brief shift the current production up the list
 */
static void PR_ProductionUp_f (void)
{
	production_queue_t *queue;

	if (!baseCurrent || !selectedProduction)
		return;

	/* first position already */
	if (selectedProduction->idx == 0)
		return;

	queue = &gd.productions[baseCurrent->idx];
	PR_QueueMove(queue, selectedProduction->idx, -1);

	selectedProduction = &queue->items[selectedProduction->idx - 1];
	PR_UpdateProductionList(baseCurrent);
}

/**
 * @brief shift the current production down the list
 */
static void PR_ProductionDown_f (void)
{
	production_queue_t *queue;

	if (!baseCurrent || !selectedProduction)
		return;

	queue = &gd.productions[baseCurrent->idx];

	if (selectedProduction->idx >= queue->numItems - 1)
		return;

	PR_QueueMove(queue, selectedProduction->idx, 1);

	selectedProduction = &queue->items[selectedProduction->idx + 1];
	PR_UpdateProductionList(baseCurrent);
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
 * @brief This is more or less the initial
 * Bind some of the functions in this file to console-commands that you can call ingame.
 * Called from MN_InitStartup resp. CL_InitLocal
 */
void PR_InitStartup (void)
{
	/* add commands */
	Cmd_AddCommand("prod_init", PR_ProductionList_f, NULL);
	Cmd_AddCommand("prod_scroll", PR_ProductionListScroll_f, "Scrolls the production lists");
	Cmd_AddCommand("prod_select", PR_ProductionSelect_f, NULL);
	Cmd_AddCommand("prodlist_rclick", PR_ProductionListRightClick_f, NULL);
	Cmd_AddCommand("prodlist_click", PR_ProductionListClick_f, NULL);
	Cmd_AddCommand("prod_inc", PR_ProductionIncrease_f, "Increase production amount");
	Cmd_AddCommand("prod_dec", PR_ProductionDecrease_f, "Decrease production amount");
	Cmd_AddCommand("prod_change", PR_ProductionChange_f, "Change production amount");
	Cmd_AddCommand("prod_stop", PR_ProductionStop_f, "Stop production");
	Cmd_AddCommand("prod_up", PR_ProductionUp_f, "Move production item up in the queue");
	Cmd_AddCommand("prod_down", PR_ProductionDown_f, "Move production item down in the queue");
	Cmd_AddCommand("prev_prod_type", BS_Prev_ProduceType_f, NULL);
	Cmd_AddCommand("next_prod_type", BS_Next_ProduceType_f, NULL);
}

/**
 * @brief Save callback for savegames
 * @sa PR_Load
 * @sa SAV_GameSave
 */
qboolean PR_Save (sizebuf_t *sb, void *data)
{
	int i, j;

	for (i = 0; i < presaveArray[PRE_MAXBAS]; i++) {
		const production_queue_t *pq = &gd.productions[i];
		MSG_WriteByte(sb, pq->numItems);
		for (j = 0; j < pq->numItems; j++) {
			/** @todo This will crash */
			const objDef_t *item = pq->items[j].item;
			const aircraft_t *aircraft = pq->items[j].aircraft;
			assert(item || aircraft);
			MSG_WriteString(sb, (item ? item->id : ""));
			MSG_WriteLong(sb, pq->items[j].amount);
			MSG_WriteFloat(sb, pq->items[j].percentDone);
			MSG_WriteByte(sb, pq->items[j].production);
			MSG_WriteString(sb, (aircraft ? aircraft->id : ""));
			MSG_WriteByte(sb, pq->items[j].items_cached);
		}
	}
	return qtrue;
}

/**
 * @brief Load callback for savegames
 * @sa PR_Save
 * @sa SAV_GameLoad
 */
qboolean PR_Load (sizebuf_t *sb, void *data)
{
	int i, j;

	for (i = 0; i < presaveArray[PRE_MAXBAS]; i++) {
		production_queue_t *pq = &gd.productions[i];
		pq->numItems = MSG_ReadByte(sb);

		for (j = 0; j < pq->numItems; j++) {
			const char *s1 = MSG_ReadString(sb);
			const char *s2;
			if (s1[0] != '\0')
				pq->items[j].item = INVSH_GetItemByID(s1);
			pq->items[j].amount = MSG_ReadLong(sb);
			pq->items[j].percentDone = MSG_ReadFloat(sb);
			pq->items[j].production = MSG_ReadByte(sb);
			s2 = MSG_ReadString(sb);
			if (s2[0] != '\0')
				pq->items[j].aircraft = AIR_GetAircraft(s2);
			pq->items[j].items_cached = MSG_ReadByte(sb);

			if (!pq->items[j].item && *s1)
				Com_Printf("PR_Load: Could not find item '%s'\n", s1);
			if (!pq->items[j].aircraft && *s2)
				Com_Printf("PR_Load: Could not find aircraft sample '%s'\n", s2);
		}
	}
	return qtrue;
}
