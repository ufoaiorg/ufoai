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

#include "client.h"
#include "cl_global.h"
#include "menu/m_popup.h"

/** Maximum number of produced/disassembled items. */
#define MAX_PRODUCTION_AMOUNT 500

/**
 * @brief A Linked list with a pointer inside and a counter.
 * @note Each entry _must_ only contain one pointer.
 */
typedef struct countedLinkedList_s {
	linkedList_t *list;	/**< The linked list itself. (only one pointer per entry) */
	int num;			/**< Number of entries. */
#if 0
	linkedList_t *cur;	/**< Currently selected entry. */
#endif
} countedLinkedList_t;


static int produceCategory = BUY_WEAP_PRI;	/**< Holds the current active production category (which is buytype). */

static qboolean productionDisassembling;	/**< Are we in disassembling state? */
static qboolean selectedQueueItem = qfalse;	/**< Did we select something in the queue. */
static production_t *selectedProduction = NULL;	/**< Holds the current active selected queue entry. */

/** A list if all producable items. */
static countedLinkedList_t productionItemList;

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
 * @sa PR_ProductionInfo
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

	if (produceCategory != BUY_AIRCRAFT)
		tech = item->tech;
	else
		tech = aircraftTemplate->tech;

	/* We cannot queue new aircraft if no free hangar space. */
	if (produceCategory == BUY_AIRCRAFT) {
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

	productionDisassembling = qfalse;

	productionAmount[0] = productionList[0] = productionQueued[0] = '\0';
	queue = &gd.productions[base->idx];

	/* First add all the queue items ... */
	for (i = 0; i < queue->numItems; i++) {
		const production_t *prod = &queue->items[i];
		if (!prod->aircraft) {
			const objDef_t *od = prod->item;
			Q_strcat(productionList, va("%s\n", od->name), sizeof(productionList));
			Q_strcat(productionAmount, va("%i\n", base->storage.num[prod->item->idx]), sizeof(productionAmount));
			Q_strcat(productionQueued, va("%i\n", prod->amount), sizeof(productionQueued));
		} else {
			const aircraft_t *aircraftTemplate = prod->aircraft;
			Q_strcat(productionList, va("%s\n", _(aircraftTemplate->name)), sizeof(productionList));
			for (j = 0, counter = 0; j < gd.numAircraft; j++) {
				const aircraft_t *aircraftBase = AIR_AircraftGetFromIdx(j);
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

	LIST_Delete(&productionItemList.list);
	productionItemList.num = 0;

	/* Then go through all object definitions ... */
	if (produceCategory != BUY_AIRCRAFT) {	/* Everything except aircraft. */
		objDef_t *od;
		for (i = 0, od = csi.ods; i < csi.numODs; i++, od++) {
			assert(od->tech);
			/* We will not show items with producetime = -1 - these are not producible.
			 * We can produce what was researched before. */
			if (BUYTYPE_MATCH(od->buytype, produceCategory)
			 && RS_IsResearched_ptr(od->tech) && od->name[0] != '\0'
			 && od->tech->produceTime > 0) {
				LIST_AddPointer(&productionItemList.list, od);
				productionItemList.num++;

				Q_strcat(productionList, va("%s\n", od->name), sizeof(productionList));
				Q_strcat(productionAmount, va("%i\n", base->storage.num[i]), sizeof(productionAmount));
				Q_strcat(productionQueued, "\n", sizeof(productionQueued));
			}
		}
	} else {
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
				LIST_AddPointer(&productionItemList.list, aircraftTemplate);
				productionItemList.num++;

				Q_strcat(productionList, va("%s\n", _(aircraftTemplate->name)), sizeof(productionList));
				for (j = 0, counter = 0; j < gd.numAircraft; j++) {
					const aircraft_t *aircraftBase = AIR_AircraftGetFromIdx(j);
					assert(aircraftBase);
					if (aircraftBase->homebase == base
					 && aircraftBase->tpl == aircraftTemplate)
						counter++;
				}
				Q_strcat(productionAmount, va("%i\n", counter), sizeof(productionAmount));
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
		selectedQueueItem = qfalse;
		PR_ClearSelected();
		Com_sprintf(mn.messageBuffer, sizeof(mn.messageBuffer), _("Production queue for base %s is empty"), base->name);
		MN_AddNewMessage(_("Production queue empty"), mn.messageBuffer, qfalse, MSG_PRODUCTION, NULL);
		CL_GameTimeStop();
		return;
	} else if (selectedProduction && selectedProduction->idx >= queue->numItems) {
		PR_ClearSelected();
		selectedProduction = &queue->items[queue->numItems - 1];
		PR_UpdateProductionList(base);
	}
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
 * @brief Checks whether an item is finished.
 * @sa CL_CampaignRun
 */
void PR_ProductionRun (void)
{
	int i;
	const objDef_t *od;
	const aircraft_t *aircraft;
	production_t *prod;
	const aircraft_t *ufocraft;

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
				if (od->price * PRODUCE_FACTOR/PRODUCE_DIVISOR > ccs.credits) {
					if (!prod->creditmessage) {
						Com_sprintf(mn.messageBuffer, sizeof(mn.messageBuffer), _("Not enough credits to finish production in base %s.\n"), base->name);
						MN_AddNewMessage(_("Notice"), mn.messageBuffer, qfalse, MSG_STANDARD, NULL);
						prod->creditmessage = qtrue;
					}
					PR_ProductionRollBottom_f();
					continue;
				}
				/* Not enough free space in base storage for this item. */
				if (base->capacities[CAP_ITEMS].max - base->capacities[CAP_ITEMS].cur < od->size) {
					if (!prod->spacemessage) {
						Com_sprintf(mn.messageBuffer, sizeof(mn.messageBuffer), _("Not enough free storage space in base %s. Production postponed.\n"), base->name);
						MN_AddNewMessage(_("Notice"), mn.messageBuffer, qfalse, MSG_STANDARD, NULL);
						prod->spacemessage = qtrue;
					}
					PR_ProductionRollBottom_f();
					continue;
				}
			} else {
				/* Not enough money to produce more items in this base. */
				if (aircraft->price * PRODUCE_FACTOR/PRODUCE_DIVISOR > ccs.credits) {
					if (!prod->creditmessage) {
						Com_sprintf(mn.messageBuffer, sizeof(mn.messageBuffer), _("Not enough credits to finish production in base %s.\n"), base->name);
						MN_AddNewMessage(_("Notice"), mn.messageBuffer, qfalse, MSG_STANDARD, NULL);
						prod->creditmessage = qtrue;
					}
					PR_ProductionRollBottom_f();
					continue;
				}
				/* Not enough free space in hangars for this aircraft. */
				if (AIR_CalculateHangarStorage(prod->aircraft, base, 0) <= 0) {
					if (!prod->spacemessage) {
						Com_sprintf(mn.messageBuffer, sizeof(mn.messageBuffer), _("Not enough free hangar space in base %s. Production postponed.\n"), base->name);
						MN_AddNewMessage(_("Notice"), mn.messageBuffer, qfalse, MSG_STANDARD, NULL);
						prod->spacemessage = qtrue;
					}
					PR_ProductionRollBottom_f();
					continue;
				}
			}
		} else {		/* This is disassembling. */
			if (base->capacities[CAP_ITEMS].max - base->capacities[CAP_ITEMS].cur < INV_DisassemblyItem(NULL, INV_GetComponentsByItem(prod->item), qtrue)) {
				if (!prod->spacemessage) {
					Com_sprintf(mn.messageBuffer, sizeof(mn.messageBuffer), _("Not enough free storage space in base %s. Disassembling postponed.\n"), base->name);
					MN_AddNewMessage(_("Notice"), mn.messageBuffer, qfalse, MSG_STANDARD, NULL);
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
						Com_sprintf(mn.messageBuffer, sizeof(mn.messageBuffer), _("The production of %s has finished."), od->name);
						MN_AddNewMessage(_("Production finished"), mn.messageBuffer, qfalse, MSG_PRODUCTION, od->tech);
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
						MN_AddNewMessage(_("Production finished"), mn.messageBuffer, qfalse, MSG_PRODUCTION, NULL);
						PR_QueueNext(base);
					}
				}
			} else {	/* This is disassembling. */
				base->capacities[CAP_ITEMS].cur += INV_DisassemblyItem(base, INV_GetComponentsByItem(prod->item), qfalse);
				prod->percentDone = 0.0f;
				prod->amount--;
				/* If this is aircraft dummy item, update UFO hangars capacity. */
				if (od->tech->type == RS_CRAFT) {
					ufocraft = AIR_GetAircraft(od->id);
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
					Com_sprintf(mn.messageBuffer, sizeof(mn.messageBuffer), _("The disassembling of %s has finished."), od->name);
					MN_AddNewMessage(_("Production finished"), mn.messageBuffer, qfalse, MSG_PRODUCTION, od->tech);
					PR_QueueNext(base);
				}
			}
		}
	}
}

/**
 * @brief Prints information about the selected item (no aircraft) in production.
 * @param[in] base Pointer to the base where informations should be printed.
 * @param[in] disassembly True, if we are trying to display disassembly info.
 */
static void PR_ProductionInfo (const base_t *base, qboolean disassembly)
{
	static char productionInfo[512];
	const objDef_t *od;
	int time, i;
	float prodPerHour;

	assert(base);

	if (!disassembly) {
		if (selectedQueueItem) {
			assert(selectedProduction);
			od = selectedProduction->item;
		} else if (selectedItem) {
			od = selectedItem;
		} else {
			od = NULL;
		}

		if (od) {
			assert(od->tech);
			/* Don't try to display the item which is not producible. */
			if (od->tech->produceTime < 0) {
				Com_sprintf(productionInfo, sizeof(productionInfo), _("No item selected"));
				Cvar_Set("mn_item", "");
			} else {
				/* If item is first in queue, take percentDone into account. */
				prodPerHour = PR_CalculateProductionPercentDone(base, od->tech, NULL, qfalse);
				/* If you entered production menu, that means that prodPerHour > 0 (must not divide by 0) */
				assert(prodPerHour > 0);
				if (od == gd.productions[base->idx].items[0].item)
					time = ceil((1.0f - gd.productions[base->idx].items[0].percentDone) / prodPerHour);
				else
					time = ceil(1.0f / prodPerHour);
				Com_sprintf(productionInfo, sizeof(productionInfo), "%s\n", od->name);
				Q_strcat(productionInfo, va(_("Costs per item\t%i c\n"), (od->price * PRODUCE_FACTOR / PRODUCE_DIVISOR)),
					sizeof(productionInfo));
				Q_strcat(productionInfo, va(_("Production time\t%ih\n"), time), sizeof(productionInfo));
				Q_strcat(productionInfo, va(_("Item size\t%i\n"), od->size), sizeof(productionInfo));
				UP_ItemDescription(od);
			}
		} else {
			Com_sprintf(productionInfo, sizeof(productionInfo), _("No item selected"));
			Cvar_Set("mn_item", "");
		}
	} else {	/* Disassembling. */
		const components_t *comp;

		if (selectedQueueItem) {
			assert(selectedProduction);
			od = selectedProduction->item;
			assert(od);
			comp = INV_GetComponentsByItem(od);
			assert(comp);
		} else if (selectedDisassembly) {
			comp = selectedDisassembly;
			od = comp->asItem;
			assert (comp->asItem);
		} else {
			od = NULL;
		}

		if (od) {
			assert(od->tech);

			/* If item is first in queue, take percentDone into account. */
			prodPerHour = PR_CalculateProductionPercentDone(base, od->tech, comp, qtrue);
			/* If you entered production menu, that means that prodPerHour > 0 (must not divide by 0) */
			assert(prodPerHour > 0);
			if (od == gd.productions[base->idx].items[0].item)
				time = ceil((1.0f - gd.productions[base->idx].items[0].percentDone) / prodPerHour);
			else
				time = ceil(1.0f / prodPerHour);
			Com_sprintf(productionInfo, sizeof(productionInfo), _("%s - disassembly\n"), od->name);
			Q_strcat(productionInfo, _("Components: "), sizeof(productionInfo));
			/* Print components. */
			for (i = 0; i < comp->numItemtypes; i++) {
				const objDef_t *compOd = comp->items[i];
				assert(compOd);
				Q_strcat(productionInfo, va(_("%s (%i) "), compOd->name, comp->item_amount[i]),
					sizeof(productionInfo));
			}
			Q_strcat(productionInfo, "\n", sizeof(productionInfo));
			Q_strcat(productionInfo, va(_("Disassembly time\t%ih\n"), time),
				sizeof(productionInfo));
		}
		UP_ItemDescription(od);
	}
	mn.menuText[TEXT_PRODUCTION_INFO] = productionInfo;
}

/**
 * @brief Prints information about the selected aircraft in production.
 * @param[in] aircraftTemplate The aircraft to print the information for
 */
static void PR_AircraftInfo (const aircraft_t * aircraftTemplate)
{
	if (aircraftTemplate) {
		static char productionInfo[512];
		Com_sprintf(productionInfo, sizeof(productionInfo), "%s\n", _(aircraftTemplate->name));
		Q_strcat(productionInfo, va(_("Production costs\t%i c\n"), (aircraftTemplate->price * PRODUCE_FACTOR / PRODUCE_DIVISOR)),
			sizeof(productionInfo));
		assert(aircraftTemplate->tech);
		Q_strcat(productionInfo, va(_("Production time\t%ih\n"), aircraftTemplate->tech->produceTime), sizeof(productionInfo));
		mn.menuText[TEXT_PRODUCTION_INFO] = productionInfo;
	} else {
		mn.menuText[TEXT_PRODUCTION_INFO] = _("No aircraft selected.");
	}
}


/**
 * @brief Get an entry of a linked list by its index in the list.
 * @param[in] list (Counted) list of pointers.
 * @param[in] num The index from the menu screen (index productionList).
 * @return A void pointer of the content in the list-entry.
 */
static inline void* PR_GetLISTPointerByIndex (countedLinkedList_t *list, int num)
{
	int i;
	linkedList_t *tempList = list->list;

	if (!list || !list->list)
		return NULL;

	if (num >= list->num || num < 0)
		return NULL;

	i = 0;
	while (tempList) {
		void *ptr = (void*)tempList->data;
		if (i == num)
			return ptr;
		i++;
		tempList = tempList->next;
	}

	return NULL;
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
	if (!baseCurrent || !curCampaign)
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
		const objDef_t *od = queue->items[num].item;
		assert(od->tech);
		selectedQueueItem = qtrue;
		selectedProduction = &queue->items[num];
		UP_OpenWith(od->tech->id);
	} else if (num >= queue->numItems + QUEUE_SPACERS) {
		/* Clicked in the item list. */
		const int idx = num - queue->numItems - QUEUE_SPACERS;

		if (produceCategory != BUY_AIRCRAFT) {
			objDef_t *od = (objDef_t*)PR_GetLISTPointerByIndex(&productionItemList, idx);
#ifdef DEBUG
			if (!od) {
				Com_DPrintf(DEBUG_CLIENT, "PR_ProductionListRightClick_f: No item found at the list-position %i!\n", idx);
				return;
			}

			if (!od->tech)
				Sys_Error("PR_ProductionListRightClick_f: No tech pointer for object '%s'\n", od->id);
#endif

			/* Open up UFOpaedia for this entry. */
			if (BUYTYPE_MATCH(od->buytype, produceCategory) && RS_IsResearched_ptr(od->tech)) {
				selectedQueueItem = qfalse;
				PR_ClearSelected();
				selectedItem = od;
				UP_OpenWith(od->tech->id);
				return;
			}
		} else {
			const aircraft_t *aircraftTemplate = (aircraft_t*)PR_GetLISTPointerByIndex(&productionItemList, idx);
			/* ufo research definition must not have a tech assigned
			 * only RS_CRAFT types have
			 * @sa RS_InitTree */
			if (aircraftTemplate->tech)
				UP_OpenWith(aircraftTemplate->tech->id);
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
	if (!baseCurrent || !curCampaign)
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
		production_t *prod = &queue->items[num];
		selectedQueueItem = qtrue;
		selectedProduction = prod;
		if (prod->production) {
			if (prod->aircraft)
				PR_AircraftInfo(selectedAircraft);
			else
				PR_ProductionInfo(base, qfalse);
		} else
			PR_ProductionInfo(base, qtrue);
	} else if (num >= queue->numItems + QUEUE_SPACERS) {
		/* Clicked in the item list. */
		const int idx = num - queue->numItems - QUEUE_SPACERS;

		if (!productionDisassembling) {
			if (produceCategory != BUY_AIRCRAFT) {	/* Everything except aircraft. */
				objDef_t *od = (objDef_t*)PR_GetLISTPointerByIndex(&productionItemList, idx);

#ifdef DEBUG
				if (!od) {
					Com_DPrintf(DEBUG_CLIENT, "PR_ProductionListClick_f: No item found at the list-position %i!\n", idx);
					return;
				}

				if (!od->tech)
					Sys_Error("PR_ProductionListClick_f: No tech pointer for object '%s'\n", od->id);
#endif
				/* We can only produce items that fulfill the following conditions... */
				if (BUYTYPE_MATCH(od->buytype, produceCategory)	/* Item is in the current inventory-category */
				 && RS_IsResearched_ptr(od->tech)		/* Tech is researched */
				 && od->tech->produceTime >= 0) {		/* Item is producible */
				 	assert(*od->name);

					selectedQueueItem = qfalse;
					PR_ClearSelected();
					selectedItem = od;
					PR_ProductionInfo(base, qfalse);
					return;
				 }
			} else {	/* Aircraft. */
				aircraft_t *aircraftTemplate = (aircraft_t*)PR_GetLISTPointerByIndex(&productionItemList, idx);
				/* ufo research definition must not have a tech assigned
				 * only RS_CRAFT types have
				 * @sa RS_InitTree */
				if (aircraftTemplate->tech
				 && aircraftTemplate->tech->produceTime >= 0
				 && RS_IsResearched_ptr(aircraftTemplate->tech)) {
					selectedQueueItem = qfalse;

					PR_ClearSelected();
					selectedAircraft = aircraftTemplate;

					PR_AircraftInfo(selectedAircraft);
					return;
				}
			}
		} else {	/* Disassembling. */
			components_t *comp = (components_t*)PR_GetLISTPointerByIndex(&productionItemList, idx);

			selectedQueueItem = qfalse;
			PR_ClearSelected();
			selectedDisassembly = comp;

			PR_ProductionInfo(base, qtrue);
			return;
		}
	}
#ifdef DEBUG
	else
		Com_DPrintf(DEBUG_CLIENT, "PR_ProductionListClick_f: Click on spacer %i\n", num);
#endif
}

/**
 * @brief update the list of items ready for disassembling
 */
static void PR_UpdateDisassemblingList_f (void)
{
	int i;
	static char productionList[1024];
	static char productionQueued[256];
	static char productionAmount[256];
	const production_queue_t *queue;
	const base_t* base;

	if (!baseCurrent)
		return;

	base = baseCurrent;
	productionDisassembling = qtrue;

	productionAmount[0] = productionList[0] = productionQueued[0] = '\0';
	queue = &gd.productions[base->idx];

	/* first add all the queue items */
	for (i = 0; i < queue->numItems; i++) {
		const production_t *prod = &queue->items[i];
		const objDef_t *od = prod->item;

		Q_strcat(productionList, va("%s\n", od->name), sizeof(productionList));
		Q_strcat(productionAmount, va("%i\n", base->storage.num[od->idx]), sizeof(productionAmount));
		Q_strcat(productionQueued, va("%i\n", prod->amount), sizeof(productionQueued));
	}

	/* then spacers */
	for (i = 0; i < QUEUE_SPACERS; i++) {
		Q_strcat(productionList, "\n", sizeof(productionList));
		Q_strcat(productionAmount, "\n", sizeof(productionAmount));
		Q_strcat(productionQueued, "\n", sizeof(productionQueued));
	}

	LIST_Delete(&productionItemList.list);
	productionItemList.num = 0;

	for (i = 0; i < gd.numComponents; i++) {
		const objDef_t *asOd = gd.components[i].asItem;
		components_t *comp = &gd.components[i];
		if (!asOd)
			continue;
		if (PR_ConditionsDisassembly(base, comp)) {
			LIST_AddPointer(&productionItemList.list, comp);
			productionItemList.num++;

			Q_strcat(productionList, va("%s\n", asOd->name), sizeof(productionList));
			Q_strcat(productionAmount, va("%i\n", base->storage.num[asOd->idx]), sizeof(productionAmount));
			Q_strcat(productionQueued, "\n", sizeof(productionQueued));
		}
	}

	/* Enable disassembly cvar. */
	productionDisassembling = qtrue;
	/* bind the menu text to our static char array */
	mn.menuText[TEXT_PRODUCTION_LIST] = productionList;
	/* bind the amount of available items */
	mn.menuText[TEXT_PRODUCTION_AMOUNT] = productionAmount;
	/* bind the amount of queued items */
	mn.menuText[TEXT_PRODUCTION_QUEUED] = productionQueued;
}

/**
 * @brief Will select a new tab on the production list.
 */
static void PR_ProductionSelect_f (void)
{
	int cat;

	/* Can be called from everywhere without a started game. */
	if (!baseCurrent || !curCampaign)
		return;

	if (Cmd_Argc() < 2) {
		Com_Printf("Usage: %s <category>\n", Cmd_Argv(0));
		return;
	}

	cat = atoi(Cmd_Argv(1));

	/* Check if the given category index is valid. */
	if (cat < MAX_BUYTYPES && cat >= BUY_WEAP_PRI) {
		produceCategory = cat;
		Cvar_Set("mn_itemtype", va("%d", produceCategory));
		switch (produceCategory) {
		case BUY_WEAP_PRI:
			Cvar_Set("mn_itemtypename", _("Primary weapons"));
			break;
		case BUY_WEAP_SEC:
			Cvar_Set("mn_itemtypename", _("Secondary weapons"));
			break;
		case BUY_MISC:
			Cvar_Set("mn_itemtypename", _("Miscellaneous"));
			break;
		case BUY_ARMOUR:
			Cvar_Set("mn_itemtypename", _("Personal Armours"));
			break;
		case BUY_MULTI_AMMO:
			/** @todo do we deed this? */
			/* Cvar_Set("mn_itemtypename", _("Weapons and ammo")); */
			break;
		case BUY_AIRCRAFT:
			Cvar_Set("mn_itemtypename", _("Aircraft"));
			break;
		case BUY_DUMMY:
			Cvar_Set("mn_itemtypename", _("Other"));
			break;
		case BUY_CRAFTITEM:
			Cvar_Set("mn_itemtypename", _("Aircraft equipment"));
			break;
		case BUY_HEAVY:
			Cvar_Set("mn_itemtypename", _("Heavy Weapons"));
			break;
		default:
			Cvar_Set("mn_itemtypename", _("Unknown"));
			break;
		}
	} else if (cat == MAX_BUYTYPES) {
		Cvar_Set("mn_itemtypename", _("Disassembling"));
		PR_ProductionInfo(baseCurrent, qtrue);
		PR_UpdateDisassemblingList_f();
		return;
	} else {
		return;
	}

	/** Enable disassembly cvar. @todo Better docu please? enable = qfalse? */
	productionDisassembling = qfalse;

	/* Reset scroll values of the list. */
	node1->textScroll = node2->textScroll = prodlist->textScroll = 0;

	/* Disable selection of queue entries. */
	selectedQueueItem = qfalse;

	/* Update list if entries for current production tab. */
	PR_UpdateProductionList(baseCurrent);

	/* Reset selected entries. */
	PR_ClearSelected();

	/* Select first entry in the list (if any). */
	if (productionItemList.num > 0) {
		if (produceCategory != BUY_AIRCRAFT)
			selectedItem = (objDef_t*)PR_GetLISTPointerByIndex(&productionItemList, 0);
		else
			selectedAircraft = (aircraft_t*)PR_GetLISTPointerByIndex(&productionItemList, 0);
	}

	/* Update displayed info about selected entry (if any). */
	if (produceCategory != BUY_AIRCRAFT)
		PR_ProductionInfo(baseCurrent, qfalse);
	else
		PR_AircraftInfo(selectedAircraft);
}

/**
 * @brief Will fill the list of producible items.
 * @note Some of Production Menu related cvars are being set here.
 */
static void PR_ProductionList_f (void)
{
	char tmpbuf[64];
	int numWorkshops = 0;

	if (Cmd_Argc() < 2) {
		Com_Printf("Usage: %s <type>\n", Cmd_Argv(0));
		return;
	}

	/* can be called from everywhere without a started game */
	if (!baseCurrent || !curCampaign)
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
	Cmd_ExecuteString(va("prod_select %i", produceCategory));
}

/**
 * @brief Rolls produceCategory left in Produce menu.
 */
static void BS_Prev_ProduceType_f (void)
{
	produceCategory--;
	if (produceCategory == BUY_MULTI_AMMO)
		produceCategory--;
	if (produceCategory < 0) {
		produceCategory = MAX_BUYTYPES;
	} else if (produceCategory > MAX_BUYTYPES) {
		produceCategory = 0;
	}
	selectedItem = NULL;
	selectedDisassembly = NULL;
	selectedAircraft = NULL;
	Cbuf_AddText(va("prod_select %i\n", produceCategory));
}

/**
 * @brief Rolls produceCategory right in Produce menu.
 */
static void BS_Next_ProduceType_f (void)
{
	produceCategory++;
	if (produceCategory == BUY_MULTI_AMMO)
		produceCategory++;
	if (produceCategory < 0) {
		produceCategory = MAX_BUYTYPES;
	} else if (produceCategory > MAX_BUYTYPES) {
		produceCategory = 0;
	}
	selectedItem = NULL;
	selectedDisassembly = NULL;
	selectedAircraft = NULL;
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

	node1->textScroll = node2->textScroll = prodlist->textScroll;
}

/**
 * @brief Sets the production array to 0
 * @sa CL_GameNew_f
 */
void PR_ProductionInit (void)
{
	Com_DPrintf(DEBUG_CLIENT, "Reset all productions\n");
	mn_production_limit = Cvar_Get("mn_production_limit", "0", 0, NULL);
	mn_production_workers = Cvar_Get("mn_production_workers", "0", 0, NULL);
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

	if (base->capacities[CAP_WORKSPACE].max >= E_CountHired(base, EMPL_WORKER))
		base->capacities[CAP_WORKSPACE].cur = E_CountHired(base, EMPL_WORKER);
	else
		base->capacities[CAP_WORKSPACE].cur = base->capacities[CAP_WORKSPACE].max;
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

	base = baseCurrent;

	if (Cmd_Argc() == 2)
		amount = atoi(Cmd_Argv(1));

	queue = &gd.productions[base->idx];

	if (selectedQueueItem) {
		assert(selectedProduction);
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
		if (!productionDisassembling) {
			if (selectedAircraft && AIR_CalculateHangarStorage(selectedAircraft, base, 0) <= 0) {
				MN_Popup(_("Hangars not ready"), _("You cannot queue aircraft.\nNo free space in hangars.\n"));
				return;
			}
			prod = PR_QueueNew(base, queue, selectedItem, selectedAircraft, amount, qfalse);	/* Production. (only of of the "selected" pointer can be nonNULL) */
		} else {
			/* We can disassembly only as many items as we have in base storage. */
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

		if (produceCategory != BUY_AIRCRAFT) {
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

				if (!productionDisassembling) {
					Com_sprintf(mn.messageBuffer, sizeof(mn.messageBuffer), _("Production of %s started"), od->name);
					MN_AddNewMessage(_("Production started"), mn.messageBuffer, qfalse, MSG_PRODUCTION, od->tech);
				} else {
					Com_sprintf(mn.messageBuffer, sizeof(mn.messageBuffer), _("Disassembling of %s started"), od->name);
					MN_AddNewMessage(_("Production started"), mn.messageBuffer, qfalse, MSG_PRODUCTION, od->tech);
				}

				/* Now we select the item we just created. */
				selectedQueueItem = qtrue;
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
			MN_AddNewMessage(_("Production started"), mn.messageBuffer, qfalse, MSG_PRODUCTION, NULL);
			/* Now we select the item we just created. */
			selectedQueueItem = qtrue;
			PR_ClearSelected();
			selectedProduction = &queue->items[queue->numItems - 1];
		}
	}

	if (!productionDisassembling) {	/* Production. */
		if (produceCategory != BUY_AIRCRAFT)
			PR_ProductionInfo(base, qfalse);
		else
			PR_AircraftInfo(selectedAircraft);
		PR_UpdateProductionList(base);
	} else {							/* Disassembling. */
		PR_ProductionInfo(base, qtrue);
		PR_UpdateDisassemblingList_f();
	}
}

/**
 * @brief Stops the current running production
 */
static void PR_ProductionStop_f (void)
{
	production_queue_t *queue;
	qboolean disassembling = qfalse;
	base_t* base;

	if (!baseCurrent || !selectedQueueItem)
		return;

	base = baseCurrent;

	queue = &gd.productions[base->idx];
	if (!selectedProduction->production)
		disassembling = qtrue;

	PR_QueueDelete(base, queue, selectedProduction->idx);

	if (queue->numItems == 0) {
		selectedQueueItem = qfalse;
		selectedProduction = NULL;
		/* empty selected item description */
		PR_ProductionInfo(base, qfalse);
	} else if (selectedProduction->idx >= queue->numItems) {
		selectedProduction = &queue->items[queue->numItems - 1];
		if (!selectedProduction->production)
			PR_ProductionInfo(base, qtrue);
		else
			PR_ProductionInfo(base, qfalse);
	}

	if (productionDisassembling) {
		PR_ProductionInfo(base, qtrue);
		PR_UpdateDisassemblingList_f();
	} else {
		PR_ProductionInfo(base, qfalse);
		PR_UpdateProductionList(base);
	}
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

	if (!baseCurrent || !selectedQueueItem)
		return;

	base = baseCurrent;

	queue = &gd.productions[base->idx];
	assert(selectedProduction);
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
		if (prod->production) {
			PR_ProductionInfo(base, qfalse);
			PR_UpdateProductionList(base);
		} else {
			PR_ProductionInfo(base, qtrue);
			PR_UpdateDisassemblingList_f();
		}
 	}
}

/**
 * @brief shift the current production up the list
 */
static void PR_ProductionUp_f (void)
{
	production_queue_t *queue;

	if (!baseCurrent || !selectedQueueItem)
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

	if (!baseCurrent || !selectedQueueItem)
		return;

	queue = &gd.productions[baseCurrent->idx];

	if (selectedProduction->idx >= queue->numItems - 1)
		return;

	PR_QueueMove(queue, selectedProduction->idx, 1);

	selectedProduction = &queue->items[selectedProduction->idx + 1];
	PR_UpdateProductionList(baseCurrent);
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
	Cmd_AddCommand("prod_disassemble", PR_UpdateDisassemblingList_f, "List of items ready for disassembling");
	Cmd_AddCommand("prodlist_rclick", PR_ProductionListRightClick_f, NULL);
	Cmd_AddCommand("prodlist_click", PR_ProductionListClick_f, NULL);
	Cmd_AddCommand("prod_inc", PR_ProductionIncrease_f, NULL);
	Cmd_AddCommand("prod_dec", PR_ProductionDecrease_f, NULL);
	Cmd_AddCommand("prod_stop", PR_ProductionStop_f, NULL);
	Cmd_AddCommand("prod_up", PR_ProductionUp_f, NULL);
	Cmd_AddCommand("prod_down", PR_ProductionDown_f, NULL);
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
