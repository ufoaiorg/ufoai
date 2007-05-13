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

/* holds the current active production category */
static int produceCategory = 0;

/* holds the current active selected queue index/objID */
static qboolean selectedQueueItem 	= qfalse;
static int selectedIndex 			= -1;

/* 20060921 LordHavoc: added PRODUCE_DIVISOR to allow reducing prices below 1x */
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
 * @brief Calculates production time.
 * @param[in] *base Pointer to the base with given production.
 * @param[in] *tech Pointer to the technology for given production.
 * @sa PR_UpdateProductionTime
 * @sa PR_QueueNew
 */
static int PR_CalculateProductionTime (base_t *base, technology_t *tech)
{
	signed int allworkers = 0, maxworkers = 0;
	signed int timeDefault = 0, time = 0;
	float timeTemp = 0;
	assert (base);
	assert (tech);

	/* Check how many workers hired in this base. */
	allworkers = E_CountHired(base, EMPL_WORKER);
	/* We will not use more workers than base capacity. */
	if (allworkers > base->capacities[CAP_WORKSPACE].max) {
		maxworkers = base->capacities[CAP_WORKSPACE].max;
	} else {
		maxworkers = allworkers;
	}

	timeDefault = tech->produceTime; /* This is the default production time for 10 workers. */
	if (maxworkers == 10) {
		/* Return default time because we can use only 10 workers. */
		Com_DPrintf("PR_CalculateProductionTime()... workers: %i, tech: %s, time: %i\n",
		maxworkers, tech->id, timeDefault);
		return timeDefault;
	} else {
		/* Calculate the real time used for our amount of workers. */
		/* NOTE: If somebody does not understand such algorithm here
		   (which is 100% realistic :>), ask me. Zenerka. */
		timeTemp = (maxworkers - PRODUCE_WORKERS);
		timeTemp = timeTemp / PRODUCE_WORKERS;
		timeTemp = timeTemp * timeDefault;
		time = timeDefault - (int)timeTemp;
		Com_DPrintf("PR_CalculateProductionTime()... workers: %i, tech: %s, time: %i\n",
		maxworkers, tech->id, time);
		/* Don't allow to return less time than 1 hour. */
		if (time < 1)
			return 1;
		else
			return time;
	}
}

/**
 * @brief Updates production time for all items in current queue.
 * @param[in] base_idx Index of base in global array.
 * @note This should be called whenever workers amount is going to
 * @note change (or base capacity going to update).
 * @sa B_BuildingDestroy_f
 * @sa B_UpdateBaseBuildingStatus
 */
void PR_UpdateProductionTime (int base_idx)
{
	technology_t *tech = NULL;
	base_t *base = NULL;
	int i, time = 0, timeTemp = 0;

	base = &gd.bases[base_idx];

	if (!base) {
#ifdef DEBUG
		Com_Printf("PR_UpdateProductionTime()... baseCurrent does not exist!\n");
#endif
		return;
	}
	assert (base);

	/* Loop through all productions in queue and adjust production time. */
	if (gd.productions[base_idx].numItems > 0) {
		/* Don't change anything for first (current) item in queue. (that's why i = 1)*/
		for (i = 1; i < gd.productions[base_idx].numItems; i++) {
			timeTemp = gd.productions[base_idx].items[i].timeLeft;
			tech = RS_GetTechByProvided(csi.ods[gd.productions[base_idx].items[i].objID].id);
			time = PR_CalculateProductionTime(base, tech);
			gd.productions[base_idx].items[i].timeLeft = time;
			Com_DPrintf("PR_UpdateProductionTime()... updating production time for %s. Original time: %i, new time: %i\n",
			csi.ods[gd.productions[base_idx].items[i].objID].id, timeTemp, gd.productions[base_idx].items[i].timeLeft);
		}
	}
}

/**
 * @brief Checks if the production requirements are met for a defined amount.
 * @param[in] amount How many items are planned to be produced.
 * @param[in] req The production requirements of the item that is to be produced.
 * @return 0: If nothing can be produced. 1+: If anything can be produced. 'amount': Maximum.
 */
static int PR_RequirementsMet (int amount, requirements_t *req)
{
	int a, i;
	int produceable_amount = 0;
	qboolean produceable = qfalse;

	for (a = 0; a < amount; a++) {
		produceable = qtrue;
		for (i = 0; i < req->numLinks; i++) {
			if (req->type[i] == RS_LINK_ITEM) {
				/* The same code is used in "RS_RequirementsMet" */
				Com_DPrintf("PR_RequirementsMet: %s / %i\n", req->id[i], req->idx[i]);
				if (B_ItemInBase(req->idx[i], baseCurrent) < req->amount[i]) {
					produceable = qfalse;
				}
			}
		}
		if (produceable)
			produceable_amount++;
		else
			break;
	}
	return produceable_amount;
}

/**
 * @brief Remove or add the required items from/to the current base.
 * @param[in] amount How many items are planned to be added (positive number) or removed (negative number).
 * @param[in] req The production requirements of the item that is to be produced. Thes included numbers are multiplied with 'amount')
 * @todo This doesn't check yet if there are more items removed than are in the base-storage (might be fixed if we used a storage-fuction with checks, otherwise we can make it a 'contition' in order to run this function.
 */
static void PR_UpdateRequiredItemsInBasestorage (int amount, requirements_t *req)
{
	int i;
	equipDef_t *ed = NULL;

	if (!baseCurrent)
		return;

	ed = &baseCurrent->storage;
	if (!ed)
		return;

	if (amount == 0)
		return;

	for (i = 0; i < req->numLinks; i++) {
		if (req->type[i] == RS_LINK_ITEM) {
			if (amount > 0) {
				/* Add items to the base-storage. */
				ed->num[req->idx[i]] += (req->amount[i] * amount);
			} else { /* amount < 0 */
				/* Remove items from the base-storage. */
				ed->num[req->idx[i]] -= (req->amount[i] * -amount);
			}
		}
	}
}

/**
 * @brief Add a new item to the bottom of the production queue.
 * @param[in] *queue
 * @param[in] objID Index of object to produce (in csi.ods[]).
 * @param[in] amount Desired amount to produce.
 * @return
 */
static production_t *PR_QueueNew (production_queue_t *queue, signed int objID, signed int amount)
{
	int numWorkshops = 0;
	technology_t *t = NULL;
	objDef_t *od = NULL;
	production_t *prod = NULL;

	assert(baseCurrent);

	if (E_CountHired(baseCurrent, EMPL_WORKER) <= 0) {
		MN_Popup(_("Not enough workers"), _("You cannot queue productions without workers hired\nin this base.\n\nHire workers."));
		return NULL;
	}

	numWorkshops = B_GetNumberOfBuildingsInBaseByType(baseCurrent->idx, B_WORKSHOP);

	if (queue->numItems >= numWorkshops * MAX_PRODUCTIONS_PER_WORKSHOP) {
		MN_Popup(_("Not enough workshops"), _("You cannot queue more items.\nBuild more workshops.\n"));
		return NULL;
	}

	/* initialize */
	prod = &queue->items[queue->numItems];
	od = &csi.ods[objID];
	t = (technology_t*)(od->tech);

	prod->objID = objID;
	prod->amount = amount;
	/* Don't try to add to queue an item which is not produceable. */
	if (t->produceTime < 0)
		return NULL;
	else
		prod->timeLeft = PR_CalculateProductionTime(baseCurrent, t);

	queue->numItems++;
	return prod;
}


/**
 * @brief Delete the selected entry from the queue.
 */
static void PR_QueueDelete (production_queue_t *queue, int index)
{
	int i;
	objDef_t *od = NULL;
	technology_t *tech = NULL;
	production_t *prod = NULL;

	prod = &queue->items[index];

	if (prod->items_cached) {
		/* Get technology of the item in the selected queue-entry. */
		od = &csi.ods[prod->objID];
		tech = (technology_t*)(od->tech);
		if (tech) {
			/* Add all items listed in the prod.-requirements /multiplied by amount) to the storage again. */
			PR_UpdateRequiredItemsInBasestorage(prod->amount, &tech->require_for_production);
		} else {
			Com_DPrintf("PR_QueueDelete: Problem getting technology entry for %i\n", index);
		}
		prod->items_cached = qfalse;
	}

	queue->numItems--;
	if (queue->numItems < 0)
		queue->numItems = 0;

	/* Copy up other items. */
	for (i = index; i < queue->numItems; i++) {
		queue->items[i] = queue->items[i+1];
	}
}

/**
 * @brief Moves the given queue item in the given direction.
 * @param[in] *queue
 * @param[in] index
 * @param[in] dir
 */
static void PR_QueueMove (production_queue_t *queue, int index, int dir)
{
	int newIndex = index + dir;
	int i;
	production_t saved;

	if (newIndex == index)
		return;

	saved = queue->items[index];

	/* copy up */
	for (i = index; i < newIndex; i++) {
		queue->items[i] = queue->items[i+1];
	}

	/* copy down */
	for (i = index; i > newIndex; i--) {
		queue->items[i] = queue->items[i-1];
	}

	/* insert item */
	queue->items[newIndex] = saved;
}

/**
 * @brief Queues the next production in the queue.
 * @param[in] base The index of the base
 */
static void PR_QueueNext (int base)
{
	char messageBuffer[256];
	production_queue_t *queue = &gd.productions[base];

	PR_QueueDelete(queue, 0);
	if (queue->numItems == 0) {
		selectedQueueItem = qfalse;
		selectedIndex = -1;
		Com_sprintf(messageBuffer, sizeof(messageBuffer), _("Production queue for base %s is empty"), gd.bases[base].name);
		MN_AddNewMessage(_("Production queue empty"), messageBuffer, qfalse, MSG_PRODUCTION, NULL);
		CL_GameTimeStop();
		return;
	} else if (selectedIndex >= queue->numItems) {
		selectedIndex = queue->numItems - 1;
	}
}

/**
 * @brief Checks whether an item is finished.
 * @sa CL_CampaignRun
 */
void PR_ProductionRun (void)
{
	int i;
	objDef_t *od;
	technology_t *t;
	production_t *prod;

	/* Loop through all founded bases. Then check productions
	   in global data array. Then decrease timeLeft and check
	   wheter an item is produced. Then add to base storage. */

	for (i = 0; i < MAX_BASES; i++) {
		if (!gd.bases[i].founded)
			continue;

		/* not actually any active productions */
		if (gd.productions[i].numItems <= 0)
			continue;

		prod = &gd.productions[i].items[0];
		assert(prod->objID >= 0);
		od = &csi.ods[prod->objID];

		/* not enough money to produce more items in this base */
		if (od->price*PRODUCE_FACTOR/PRODUCE_DIVISOR > ccs.credits)
			continue;

		/* Not enough free space in base storage for this item. */
		if (gd.bases[i].capacities[CAP_ITEMS].max - gd.bases[i].capacities[CAP_ITEMS].cur < od->size)
			continue;

		t = (technology_t*)(od->tech);
#ifdef DEBUG
		if (!t)
			Sys_Error("PR_ProductionRun: No tech pointer for object id %i ('%s')\n", prod->objID, od->id);
#endif
		prod->timeLeft--;
		if (prod->timeLeft <= 0) {
			CL_UpdateCredits(ccs.credits - (od->price*PRODUCE_FACTOR/PRODUCE_DIVISOR));
			prod->timeLeft = t->produceTime;
			prod->amount--;
			/* now add it to equipment */
			gd.bases[i].storage.num[prod->objID]++;
			/* and update storage capacity.cur */
			gd.bases[i].capacities[CAP_ITEMS].cur += od->size;

			/* queue the next production */
			if (prod->amount <= 0) {
				Com_sprintf(messageBuffer, sizeof(messageBuffer), _("The production of %s has finished."),od->name);
				MN_AddNewMessage(_("Production finished"), messageBuffer, qfalse, MSG_PRODUCTION, od->tech);
				PR_QueueNext(i);
			}
		}
	}
}

/**
 * @brief Prints information about the selected item in production.
 * @param[in] disassembly True, if we are trying to display disassembly info.
 * @note -1 for objID means that there is no production in this base.
 */
static void PR_ProductionInfo (qboolean disassembly)
{
	static char productionInfo[512];
	technology_t *t;
	objDef_t *od, *compod;
	int objID;
	int time, i, j;
	components_t *comp = NULL;

	assert(baseCurrent);

	if (selectedQueueItem) {
		objID = gd.productions[baseCurrent->idx].items[selectedIndex].objID;
	} else {
		objID = selectedIndex;
	}

	if (!disassembly) {
		if (objID >= 0) {
			od = &csi.ods[objID];
			t = (technology_t*)(od->tech);
			/* Don't try to display the item which is not produceable. */
			if (t->produceTime < 0) {
				Com_sprintf(productionInfo, sizeof(productionInfo), _("No item selected"));
				Cvar_Set("mn_item", "");
			} else {
				/* If item is first in queue, use timeLeft as time, otherwise calculate. */
				if (objID == gd.productions[baseCurrent->idx].items[0].objID)
					time = gd.productions[baseCurrent->idx].items[0].timeLeft;
				else
					time = PR_CalculateProductionTime(baseCurrent, t);
				Com_sprintf(productionInfo, sizeof(productionInfo), "%s\n", od->name);
				Q_strcat(productionInfo, va(_("Costs per item\t%i c\n"), (od->price*PRODUCE_FACTOR/PRODUCE_DIVISOR)),
					sizeof(productionInfo) );
				Q_strcat(productionInfo, va(_("Productiontime\t%ih\n"), time),
					sizeof(productionInfo) );
				Q_strcat(productionInfo, va(_("Item size\t%i\n"), od->size),
					sizeof(productionInfo) );
				CL_ItemDescription(objID);
			}
		} else {
			Com_sprintf(productionInfo, sizeof(productionInfo), _("No item selected"));
			Cvar_Set("mn_item", "");
		}
	} else {	/* Disassembling. */
		/* Find related components array. */
		for (i = 0; i < gd.numComponents; i++) {
			comp = &gd.components[i];
			Com_Printf("components definition: %s item: %s\n", comp->assembly_id, csi.ods[comp->assembly_idx].id);
			if (comp->assembly_idx == objID)
				break;
		}
		assert (comp);
		if (objID >= 0) {
			od = &csi.ods[objID];
			t = (technology_t*)(od->tech);
			Com_Printf("od: %s\n", od->name);
			/* Don't try to display the item which is not produceable. */
			if (t->produceTime < 0) {
				Com_sprintf(productionInfo, sizeof(productionInfo), _("No disassembly selected"));
				Cvar_Set("mn_item", "");
			} else {
				/* If item is first in queue, use timeLeft as time, otherwise calculate. */
				if (objID == gd.productions[baseCurrent->idx].items[0].objID)
					time = gd.productions[baseCurrent->idx].items[0].timeLeft;
				else
					time = PR_CalculateProductionTime(baseCurrent, t);
				Com_sprintf(productionInfo, sizeof(productionInfo), _("%s - disassembly\n"), od->name);
				Q_strcat(productionInfo, va(_("Components: ")),
					sizeof(productionInfo) );
				/* Print components. */
				for (i = 0; i < comp->numItemtypes; i++) {
					for (j = 0, compod = csi.ods; j < csi.numODs; j++, compod++) {
						if ((Q_strncmp(compod->id, comp->item_id[i], MAX_VAR)) == 0)
							break;
					}
					Q_strcat(productionInfo, va(_("%s (%i) "), compod->name, comp->item_amount[i]), 
						sizeof(productionInfo) );
				}
				Q_strcat(productionInfo, "\n", sizeof(productionInfo));
				Q_strcat(productionInfo, va(_("Disassemblingtime\t%ih\n"), time),
					sizeof(productionInfo) );
				CL_ItemDescription(objID);
			}
	}
	menuText[TEXT_PRODUCTION_INFO] = productionInfo;
}

/**
 * @brief Click function for production list
 * @note Opens the ufopedia - by right clicking an item
 */
static void PR_ProductionListRightClick_f (void)
{
	int i, j, num, idx;
	technology_t *t = NULL;
	objDef_t *od = NULL;
	production_queue_t *queue = NULL;

	/* can be called from everywhere without a started game */
	if (!baseCurrent ||!curCampaign)
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
	if (num < queue->numItems) {
		od = &csi.ods[queue->items[num].objID];
		t = (technology_t*)(od->tech);
		UP_OpenWith(t->id);
	} else if (num >= queue->numItems + QUEUE_SPACERS) {
		/* Clicked in the item list. */
		idx = num - queue->numItems - QUEUE_SPACERS;
		for (j = 0, i = 0, od = csi.ods; i < csi.numODs; i++, od++) {
			t = (technology_t*)(od->tech);
#ifdef DEBUG
			if (!t)
				Sys_Error("PR_ProductionListRightClick_f: No tech pointer for object id %i ('%s')\n", i, od->id);
#endif
			/* Open up ufopedia for this entry. */
			if (BUYTYPE_MATCH(od->buytype,produceCategory) && RS_IsResearched_ptr(t)) {
				if (j == idx) {
					UP_OpenWith(t->id);
					return;
				}
				j++;
			}
		}
	}
#ifdef DEBUG
	else
		Com_DPrintf("PR_ProductionListRightClick_f: Click on spacer %i\n", num);
#endif
}

/**
 * @brief Click function for production list.
 */
static void PR_ProductionListClick_f (void)
{
	int i, j, num, idx;
	objDef_t *od = NULL;
	technology_t *t = NULL;
	production_queue_t *queue = NULL;
	production_t *prod = NULL;

	/* can be called from everywhere without a started game */
	if (!baseCurrent || !curCampaign)
		return;
	queue = &gd.productions[baseCurrent->idx];

#if 0 /* FIXME: not a concern any more ... */
	/* there is already a running production - stop it first */
	if (gd.productions[baseCurrent->idx].amount > 0 ) {
		PR_ProductionInfo(qfalse);
		return;
	}
#endif

	/* not enough parameters */
	if (Cmd_Argc() < 2) {
		Com_Printf("Usage: %s <arg>\n", Cmd_Argv(0));
		return;
	}

	/* clicked which item? */
	num = atoi(Cmd_Argv(1));

	/* Clicked the production queue or the item list? */
	if (num < queue->numItems) {
		prod = &queue->items[num];
		selectedQueueItem = qtrue;
		selectedIndex = num;
		PR_ProductionInfo(qfalse);
	} else if (num >= queue->numItems + QUEUE_SPACERS) {
		/* Clicked in the item list. */
		idx = num - queue->numItems - QUEUE_SPACERS;
		for (j = 0, i = 0, od = csi.ods; i < csi.numODs; i++, od++) {
			t = (technology_t*)(od->tech);
#ifdef DEBUG
			if (!t)
				Sys_Error("PR_ProductionListClick_f: No tech pointer for object id %i ('%s')\n", i, od->id);
#endif
			/* We can only produce items that fulfill the following conditions... */
			if (BUYTYPE_MATCH(od->buytype, produceCategory)	/* Item is in the current inventory-category */
				&& RS_IsResearched_ptr(t)		/* Tech is researched */
				&& t->produceTime >= 0		/* Item is produceable */
			) {
				assert(*od->name);
				if (j == idx) {
#if 0
					/* FIXME: don't start production yet .. */
					gd.productions[baseCurrent->idx].objID = i;
					gd.productions[baseCurrent->idx].amount = 0;
					gd.productions[baseCurrent->idx].timeLeft = t->produceTime;
#endif
					selectedQueueItem = qfalse;
					selectedIndex = i;
					PR_ProductionInfo(qfalse);
					return;
				}
				j++;
			}
		}
	}
#ifdef DEBUG
	else
		Com_DPrintf("PR_ProductionListClick_f: Click on spacer %i\n", num);
#endif
}

/** @brief update the list of queued and available items */
static void PR_UpdateProductionList (void)
{
	int i;
	static char productionList[1024];
	static char productionQueued[256];
	static char productionAmount[256];
	objDef_t *od;
	production_queue_t *queue;
	production_t *prod;
	technology_t *tech = NULL;

	Cvar_SetValue("mn_prod_disassembling", 0);

	productionAmount[0] = productionList[0] = productionQueued[0] = '\0';
	queue = &gd.productions[baseCurrent->idx];

	/* first add all the queue items */
	for (i = 0; i < queue->numItems; i++) {
		prod = &queue->items[i];
		od = &csi.ods[prod->objID];

		Q_strcat(productionList, va("%s\n", od->name), sizeof(productionList));
		Q_strcat(productionAmount, va("%i\n", baseCurrent->storage.num[prod->objID]), sizeof(productionAmount));
		Q_strcat(productionQueued, va("%i\n", prod->amount), sizeof(productionQueued));
	}

	/* then spacers */
	for (i = 0; i < QUEUE_SPACERS; i++) {
		Q_strcat(productionList, "\n", sizeof(productionList));
		Q_strcat(productionAmount, "\n", sizeof(productionAmount));
		Q_strcat(productionQueued, "\n", sizeof(productionQueued));
	}

	/* then go through all object definitions */
	for (i = 0, od = csi.ods; i < csi.numODs; i++, od++) {
		/* we will not show items with producetime = -1 - these are not produceable */
		if (*od->id)
			tech = RS_GetTechByProvided(od->id);

		/* we can produce what was researched before */
		if (BUYTYPE_MATCH(od->buytype, produceCategory) && RS_IsResearched_ptr(od->tech)
		&& *od->name && tech && (tech->produceTime > 0)) {
			Q_strcat(productionList, va("%s\n", od->name), sizeof(productionList));
			Q_strcat(productionAmount, va("%i\n", baseCurrent->storage.num[i]), sizeof(productionAmount));
			Q_strcat(productionQueued, "\n", sizeof(productionQueued));
		}
	}
	/* bind the menu text to our static char array */
	menuText[TEXT_PRODUCTION_LIST] = productionList;
	/* bind the amount of available items */
	menuText[TEXT_PRODUCTION_AMOUNT] = productionAmount;
	/* bind the amount of queued items */
	menuText[TEXT_PRODUCTION_QUEUED] = productionQueued;

#if 0 /* FIXME: needed now? */
	/* now print the information about the current item in production */
	PR_ProductionInfo(qfalse);
#endif
}

/** @brief update the list of items ready for disassembling */
static void PR_UpdateDisassemblingList_f (void)
{
	int i;
	static char productionList[1024];
	static char productionQueued[256];
	static char productionAmount[256];
	objDef_t *od;
	production_queue_t *queue;
	production_t *prod;
	
	Cvar_SetValue("mn_prod_disassembling", 1);

	productionAmount[0] = productionList[0] = productionQueued[0] = '\0';
	queue = &gd.productions[baseCurrent->idx];
	
	/* first add all the queue items */
	for (i = 0; i < queue->numItems; i++) {
		prod = &queue->items[i];
		od = &csi.ods[prod->objID];

		Q_strcat(productionList, va("%s\n", od->name), sizeof(productionList));
		Q_strcat(productionAmount, va("%i\n", baseCurrent->storage.num[prod->objID]), sizeof(productionAmount));
		Q_strcat(productionQueued, va("%i\n", prod->amount), sizeof(productionQueued));
	}

	/* then spacers */
	for (i = 0; i < QUEUE_SPACERS; i++) {
		Q_strcat(productionList, "\n", sizeof(productionList));
		Q_strcat(productionAmount, "\n", sizeof(productionAmount));
		Q_strcat(productionQueued, "\n", sizeof(productionQueued));
	}

	for (i = 0; i < gd.numComponents; i++) {
		Com_Printf("num: %i loc: %i idx: %i\n", gd.numComponents, i, gd.components[i].assembly_idx);
		if (gd.components[i].assembly_idx <= 0)
			continue;
		od = &csi.ods[gd.components[i].assembly_idx];
/*		if (RS_IsResearched_ptr(od->tech) && (baseCurrent->storage.num[i] > 0)) { */
			Q_strcat(productionList, va("%s\n", od->name), sizeof(productionList));
			Q_strcat(productionAmount, va("%i\n", baseCurrent->storage.num[gd.components[i].assembly_idx]), sizeof(productionAmount));
			Q_strcat(productionQueued, "\n", sizeof(productionQueued));
/*		}*/
	}
	/* bind the menu text to our static char array */
	menuText[TEXT_PRODUCTION_LIST] = productionList;
	/* bind the amount of available items */
	menuText[TEXT_PRODUCTION_AMOUNT] = productionAmount;
	/* bind the amount of queued items */
	menuText[TEXT_PRODUCTION_QUEUED] = productionQueued;

#if 0 /* FIXME: needed now? */
	/* now print the information about the current item in production */
	PR_ProductionInfo(qfalse);
#endif
}

/**
 * @brief will select a new tab on the produciton list
 */
static void PR_ProductionSelect_f (void)
{
	/* can be called from everywhere without a started game */
	if (!baseCurrent ||!curCampaign)
		return;

	if (Cmd_Argc() < 2) {
		Com_Printf("Usage: prod_select <category>\n");
		return;
	}
	produceCategory = atoi(Cmd_Argv(1));

	/* reset scroll values */
	node1->textScroll = node2->textScroll = prodlist->textScroll = 0;

	PR_UpdateProductionList();
}

/**
 * @brief Will fill the list of produceable items.
 * @note Some of Production Menu related cvars are being set here.
 */
static void PR_ProductionList_f (void)
{
	char tmpbuf[64];

	/* can be called from everywhere without a started game */
	if (!baseCurrent ||!curCampaign)
		return;

	PR_ProductionSelect_f();
	PR_ProductionInfo(qfalse);
	Cvar_SetValue("mn_production_limit", MAX_PRODUCTIONS_PER_WORKSHOP * B_GetNumberOfBuildingsInBaseByType(baseCurrent->idx, B_WORKSHOP));
	Cvar_SetValue("mn_production_basecap", baseCurrent->capacities[CAP_WORKSPACE].max);
	/* Set amount of workers - all/ready to work (determined by base capacity. */
	if (baseCurrent->capacities[CAP_WORKSPACE].max >= E_CountHired(baseCurrent, EMPL_WORKER))
		baseCurrent->capacities[CAP_WORKSPACE].cur = E_CountHired(baseCurrent, EMPL_WORKER);
	else
		baseCurrent->capacities[CAP_WORKSPACE].cur = baseCurrent->capacities[CAP_WORKSPACE].max;
	Com_sprintf(tmpbuf, sizeof(tmpbuf), "%i/%i", E_CountHired(baseCurrent, EMPL_WORKER),
	baseCurrent->capacities[CAP_WORKSPACE].cur);
	Cvar_Set("mn_production_workers", tmpbuf);
	Com_sprintf(tmpbuf, sizeof(tmpbuf), "%i/%i", baseCurrent->capacities[CAP_ITEMS].max,
	baseCurrent->capacities[CAP_ITEMS].cur);
	Cvar_Set("mn_production_storage", tmpbuf);
}

/**
 * @brief Returns true if the current base is able to produce items
 */
qboolean PR_ProductionAllowed (void)
{
	if (baseCurrent->baseStatus != BASE_UNDER_ATTACK &&
		B_GetNumberOfBuildingsInBaseByType(baseCurrent->idx, B_WORKSHOP) > 0 &&
		E_CountHired(baseCurrent, EMPL_WORKER) > 0 ) {
		Cbuf_AddText("set_prod_enabled;");
		return qtrue;
	} else {
		Cbuf_AddText("set_prod_disabled;");
		return qfalse;
	}
}

/**
 * @brief Function binding for prod_scroll that scrolls other text nodes
 * simultan with the
 */
static void PR_ProductionListScroll_f (void)
{
	node1->textScroll = node2->textScroll = prodlist->textScroll;
}

/**
 * @brief Sets the production array to 0
 */
void PR_ProductionInit (void)
{
	Com_DPrintf("Reset all productions\n");
	memset(gd.productions, 0, sizeof(production_queue_t)*MAX_BASES);
	mn_production_limit = Cvar_Get("mn_production_limit", "0", 0, NULL);
	mn_production_workers = Cvar_Get("mn_production_workers", "0", 0, NULL);
}

/**
 * @brief
 * @sa CL_InitAfter
 */
extern void PR_Init (void)
{
	menu_t* menu = MN_GetMenu("production");
	if (!menu)
		Sys_Error("Could not find the production menu\n");

	prodlist = MN_GetNode(menu, "prodlist");
	node1 = MN_GetNode(menu, "prodlist_amount");
	node2 = MN_GetNode(menu, "prodlist_queued");

	if (!prodlist || !node1 || !node2)
		Sys_Error("Could not find the needed menu nodes in production menu\n");
}

/**
 * @brief Increases the production amount by given parameter.
 */
static void PR_ProductionIncrease_f (void)
{
	int amount = 1;
	int produceable_amount;
	char messageBuffer[256];
	production_queue_t *queue = NULL;
	objDef_t *od = NULL;
	technology_t *tech = NULL;
	production_t *prod = NULL;

	if (Cmd_Argc() == 2)
		amount = atoi(Cmd_Argv(1));

	if (!baseCurrent)
		return;
	queue = &gd.productions[baseCurrent->idx];

	if (selectedQueueItem) {
		assert(selectedIndex >= 0 && selectedIndex < queue->numItems);
		prod = &queue->items[selectedIndex];
		prod->amount += amount;
	} else {
		if (selectedIndex < 0)
			return;
		prod = PR_QueueNew(queue, selectedIndex, amount);
		/* prod is NULL when queue limit is reached */
		if (!prod)
			return;

		/* Get technology of the item in the selected queue-entry. */
		od = &csi.ods[prod->objID];
		tech = (technology_t*)(od->tech);

		if (tech)
			produceable_amount = PR_RequirementsMet(amount, &tech->require_for_production);
		else
			produceable_amount = amount;

		if (produceable_amount > 0) {	/* Check if production requirements have been (even partially) met. */
			if (tech) {
				/* Remove the additionally required items (multiplied by 'produceable_amount') from base-storage.*/
				PR_UpdateRequiredItemsInBasestorage(-amount, &tech->require_for_production);
				prod->items_cached = qtrue;
			}

			if (produceable_amount < amount) {
				 /* @todo: make the numbers work here. */
				MN_Popup("Not enough material!", "You don't have enough material to produce all (xx) items. Production will continue with a reduced (xx) number.");
			}

			if (prod) {
				Com_sprintf(messageBuffer, sizeof(messageBuffer), _("Production of %s started"), csi.ods[selectedIndex].name);
				MN_AddNewMessage(_("Production started"), messageBuffer, qfalse, MSG_PRODUCTION, csi.ods[selectedIndex].tech);

				/* Now we select the item we just created. */
				selectedQueueItem = qtrue;
				selectedIndex = queue->numItems-1;
			} else {
				/* Oops! Too many items! */
				MN_Popup(_("Queue full!"), _("You cannot queue any more items!"));
			}
		} else { /*produceable_amount <= 0 */
			MN_Popup("Not enough material!", "To produce this item you need at least the following materials ..."); /* @todo: better messages needed - therefore i skip the gettext code for now */
			/* @todo:
			 * If the requirements are not met (produceable_amount<=0) we
			 *  -) need to popup something like: "You need the following items in order to produce more of ITEM:   x of ITEM, x of ITEM, etc..."
			 *     This info should also be displayed in the item-info.
			 *  -) can can (if possible) thange the 'amount' to a vlalue that _can_ be produced (i.e. the maximum amount possible).*/
		}
	}

	PR_ProductionInfo(qfalse);
	PR_UpdateProductionList();
}

/**
 * @brief Stops the current running production
 */
static void PR_ProductionStop_f (void)
{
	production_queue_t *queue;

	if (!baseCurrent || !selectedQueueItem)
		return;

	queue = &gd.productions[baseCurrent->idx];
	PR_QueueDelete(queue, selectedIndex);

	if (queue->numItems == 0) {
		selectedQueueItem = qfalse;
		selectedIndex = -1;
	} else if (selectedIndex >= queue->numItems) {
		selectedIndex = queue->numItems - 1;
	}

	PR_ProductionInfo(qfalse);
	PR_UpdateProductionList();
}

/**
 * @brief Decrease the production amount by given parameter
 */
static void PR_ProductionDecrease_f (void)
{
	int amount = 1;
	production_queue_t *queue;
	production_t *prod;

	if (Cmd_Argc() == 2)
		amount = atoi(Cmd_Argv(1));

	if (!baseCurrent || !selectedQueueItem)
		return;

	queue = &gd.productions[baseCurrent->idx];
	assert(selectedIndex >= 0 && selectedIndex < queue->numItems);
	prod = &queue->items[selectedIndex];
	prod->amount -= amount;

	if (prod->amount <= 0) {
		PR_ProductionStop_f();
	} else {
		PR_ProductionInfo(qfalse);
		PR_UpdateProductionList();
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

	if (selectedIndex == 0)
		return;

	queue = &gd.productions[baseCurrent->idx];
	PR_QueueMove(queue, selectedIndex, -1);

	selectedIndex--;
	PR_UpdateProductionList();
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

	if (selectedIndex >= queue->numItems-1)
		return;

	PR_QueueMove(queue, selectedIndex, 1);

	selectedIndex++;
	PR_UpdateProductionList();
}

/**
 * @brief This is more or less the initial
 * Bind some of the functions in this file to console-commands that you can call ingame.
 * Called from MN_ResetMenus resp. CL_InitLocal
 */
extern void PR_ResetProduction (void)
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
}

/**
 * @brief Save callback for savegames
 * @sa PR_Load
 * @sa SAV_GameSave
 */
extern qboolean PR_Save (sizebuf_t* sb, void* data)
{
	int i, j;
	production_queue_t *pq;

	MSG_WriteByte(sb, MAX_BASES);
	for (i = 0; i < MAX_BASES; i++) {
		pq = &gd.productions[i];
		MSG_WriteByte(sb, pq->numItems);
		for (j = 0; j < pq->numItems; j++) {
			MSG_WriteLong(sb, pq->items[j].objID);
			MSG_WriteLong(sb, pq->items[j].amount);
			MSG_WriteLong(sb, pq->items[j].timeLeft);
			MSG_WriteLong(sb, pq->items[j].workers);
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
extern qboolean PR_Load (sizebuf_t* sb, void* data)
{
	int i, j, cnt;
	production_queue_t *pq;

	cnt = MSG_ReadByte(sb);
	for (i = 0; i < cnt; i++) {
		pq = &gd.productions[i];
		pq->numItems = MSG_ReadByte(sb);
		for (j = 0; j < pq->numItems; j++) {
			pq->items[j].objID = MSG_ReadLong(sb);
			pq->items[j].amount = MSG_ReadLong(sb);
			pq->items[j].timeLeft = MSG_ReadLong(sb);
			if (*(int*)data >= 2)
				pq->items[j].workers = MSG_ReadLong(sb);
			pq->items[j].items_cached = MSG_ReadByte(sb);
		}
	}
	return qtrue;
}
