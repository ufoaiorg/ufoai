/**
 * @file cl_produce.c
 * @brief Single player production stuff
 */

/*
Copyright (C) 2002-2006 UFO: Alien Invasion team.

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

/**
 * @TODO: Now that we have the Workshop in basemanagement.ufo
 * we need to check whether the player has already set this building up
 * otherwise he won't be allowed to produce more equipment stuff
 */

/* 20060921 LordHavoc: added PRODUCE_DIVISOR to allow reducing prices below 1x */
#ifdef LORDHAVOC_ECONOMY
static int PRODUCE_FACTOR = 1;
static int PRODUCE_DIVISOR = 2;
#else
static int PRODUCE_FACTOR = 10;
static int PRODUCE_DIVISOR = 1;
#endif

/** @brief number of blank lines between queued items and tech list */
static int QUEUE_SPACERS = 2;

/**
 * @brief add a new item to the bottom of the production queue
 */
static production_t *PR_QueueNew(production_queue_t *queue, signed int objID, signed int amount)
{
	technology_t *t;
	objDef_t *od;
	production_t *prod;
	int i;

	/* initialize */
	prod = &queue->items[queue->numItems];
	od = &csi.ods[objID];
	t = (technology_t*)(od->tech);

	prod->objID = objID;
	prod->amount = amount;
	prod->timeLeft = t->produceTime;

	queue->numItems++;
	return prod;
}

/**
 * @brief delete the selected index from the queue
 */
static void PR_QueueDelete(production_queue_t *queue, int index)
{
	int i;

	queue->numItems--;
	/* copy up other items */
	for (i = index; i < queue->numItems; i++) {
		queue->items[i] = queue->items[i+1];
	}
}

/**
 * @brief move the given queue item in the given direction
 */
static void PR_QueueMove(production_queue_t *queue, int index, int dir)
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
 * @brief queue the next production in the queue
 * @param base the index of the base
 */
static void PR_QueueNext(int base)
{
	int i;
	char messageBuffer[256];
	production_queue_t *queue = &gd.productions[base];

	PR_QueueDelete(queue, 0);
	if (queue->numItems == 0) {
		Com_sprintf(messageBuffer, sizeof(messageBuffer), _("Production queue for base %s is empty"), gd.bases[base].name);
		MN_AddNewMessage(_("Production queue empty"), messageBuffer, qfalse, MSG_PRODUCTION, NULL);
		CL_GameTimeStop();
		return;
	}
}


/**
 * @brief Checks whether an item is finished
 * @sa CL_CampaignRun
 */
void PR_ProductionRun(void)
{
	int b, i;
	objDef_t *od;
	technology_t *t;
	production_t *prod;

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

		t = (technology_t*)(od->tech);
#ifdef DEBUG
		if (!t)
			Sys_Error("PR_ProductionRun: No tech pointer for object id %i ('%s')\n", prod->objID, od->kurz);
#endif
		prod->timeLeft--;
		if (prod->timeLeft <= 0) {
			CL_UpdateCredits(ccs.credits - (od->price*PRODUCE_FACTOR/PRODUCE_DIVISOR));
			prod->timeLeft = t->produceTime;
			prod->amount--;
			/* now add it to equipment */
			/* FIXME: overflow possible */
			gd.bases[i].storage.num[prod->objID]++;

			/* queue the next production */
			if (prod->amount<=0) {
				PR_QueueNext(i);
			}
		}
	}
}

/**
 * @brief Prints information about the selected item in production
 * @note -1 for objID means that there ic no production in this base
 */
static void PR_ProductionInfo ()
{
	static char productionInfo[512];
	technology_t *t;
	objDef_t *od;
	int objID;

	assert(baseCurrent);

	if (selectedQueueItem) {
		objID = gd.productions[baseCurrent->idx].items[selectedIndex].objID;
	} else {
		objID = selectedIndex;
	}

	if (objID >= 0) {
		od = &csi.ods[objID];
		t = (technology_t*)(od->tech);
		Com_sprintf(productionInfo, sizeof(productionInfo), "%s\n", od->name);
		Q_strcat(productionInfo, va(_("Costs per item\t%i c\n"), (od->price*PRODUCE_FACTOR/PRODUCE_DIVISOR)),
				 sizeof(productionInfo) );
		CL_ItemDescription(objID);
	} else {
		Com_sprintf(productionInfo, sizeof(productionInfo), _("No item selected"));
		Cvar_Set("mn_item", "");
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
	technology_t *t;
	objDef_t *od;
	production_queue_t *queue;

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

	/* is that part of the queue */
	if (num < queue->numItems) {
		od = &csi.ods[queue->items[num].objID];
		t = (technology_t*)(od->tech);
		UP_OpenWith(t->id);
	}else if (num >= queue->numItems + QUEUE_SPACERS) {
		/* clicked in the tech list */
		idx = num - queue->numItems - QUEUE_SPACERS;
		for (j=0, i = 0, od = csi.ods; i < csi.numODs; i++, od++) {
			t = (technology_t*)(od->tech);
#ifdef DEBUG
			if (!t)
				Sys_Error("PR_ProductionListClick_f: No tech pointer for object id %i ('%s')\n", i, od->kurz);
#endif
			/* we can produce what was researched before */
			if (od->buytype == produceCategory && RS_IsResearched_ptr(t)) {
				if (j==idx) {
					UP_OpenWith(t->id);
					return;
				}
				j++;
			}
		}
	}
}

/**
 * @brief Click function for production list
 */
static void PR_ProductionListClick_f (void)
{
	int i, j, num, idx;
	objDef_t *od;
	technology_t *t;
	production_queue_t *queue;
	production_t *prod;

	/* can be called from everywhere without a started game */
	if (!baseCurrent ||!curCampaign)
		return;
	queue = &gd.productions[baseCurrent->idx];

#if 0 /* FIXME: not a concern any more ... */
	/* there is already a running production - stop it first */
	if (gd.productions[baseCurrent->idx].amount > 0 ) {
		PR_ProductionInfo();
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

	/* clicked the production queue or the tech list? */
	if (num < queue->numItems) {
		prod = &queue->items[num];
		selectedQueueItem = qtrue;
		selectedIndex = num;
		PR_ProductionInfo();
	}else if (num >= queue->numItems + QUEUE_SPACERS) {
		/* clicked the tech list */
		idx = num - queue->numItems - QUEUE_SPACERS;
		for (j=0, i = 0, od = csi.ods; i < csi.numODs; i++, od++) {
			t = (technology_t*)(od->tech);
#ifdef DEBUG
			if (!t)
				Sys_Error("PR_ProductionListClick_f: No tech pointer for object id %i ('%s')\n", i, od->kurz);
#endif
			/* we can produce what was researched before */
			if (od->buytype == produceCategory && RS_IsResearched_ptr(t)) {
				if (j==idx) {
#if 0	/* FIXME: don't start production yet .. */
					gd.productions[baseCurrent->idx].objID = i;
					gd.productions[baseCurrent->idx].amount = 0;
					gd.productions[baseCurrent->idx].timeLeft = t->produceTime;
#endif
					selectedQueueItem = qfalse;
					selectedIndex = i;
					PR_ProductionInfo();
					return;
				}
				j++;
			}
		}
	}
}

/** @brief update the list of queued and available items */
static void PR_UpdateProductionList()
{
	int i;
	static char productionList[1024];
	static char productionQueued[256];
	static char productionAmount[256];
	objDef_t *od;
	production_queue_t *queue;
	production_t *prod;

	productionAmount[0] = productionList[0] = productionQueued[0] = '\0';
	queue = &gd.productions[baseCurrent->idx];

	/* first add all the queue items */
	for (i = 0; i < queue->numItems; i++) {
		prod = &queue->items[i];
		od = &csi.ods[prod->objID];

		Q_strcat(productionList, va("%s\n", od->name), sizeof(productionList));
		Q_strcat(productionAmount, va("\n", baseCurrent->storage.num[prod->objID]), sizeof(productionAmount));
		Q_strcat(productionQueued, va("%i\n", prod->amount), sizeof(productionQueued));
	}

	/* then spacers */
	for (i = 0; i < QUEUE_SPACERS; i++) {
		Q_strcat(productionList, va("\n"), sizeof(productionList));
		Q_strcat(productionAmount, va("\n"), sizeof(productionAmount));
		Q_strcat(productionQueued, va("\n"), sizeof(productionQueued));
	}

	/* then go through all object definitions */
	for (i = 0, od = csi.ods; i < csi.numODs; i++, od++) {
		/* we can produce what was researched before */
		if (od->buytype == produceCategory && RS_IsResearched_ptr(od->tech)) {
			Q_strcat(productionList, va("%s\n", od->name), sizeof(productionList));
			Q_strcat(productionAmount, va("%i\n", baseCurrent->storage.num[i]), sizeof(productionAmount));
			Q_strcat(productionQueued, va("\n"), sizeof(productionQueued));
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
	PR_ProductionInfo();
#endif
}

/**
 * @brief will select a new tab on the produciton list
 */
static void PR_ProductionSelect()
{
	/* can be called from everywhere without a started game */
	if (!baseCurrent ||!curCampaign)
		return;

	if (Cmd_Argc() < 2) {
		Com_Printf("Usage: prod_init <category>\n");
		return;
	}
	produceCategory = atoi(Cmd_Argv(1));
	PR_UpdateProductionList();
}

/**
 * @brief Will fill the list of produceable items
 */
static void PR_ProductionList (void)
{
	/* can be called from everywhere without a started game */
	if (!baseCurrent ||!curCampaign)
		return;

	PR_ProductionSelect();
	PR_ProductionInfo();
}


/**
 * @brief Sets the production array to 0
 */
void PR_ProductionInit(void)
{
	Com_DPrintf("Reset all productions\n");
	memset(gd.productions, 0, sizeof(production_queue_t)*MAX_BASES);
}

/**
 * @brief Increase the production amount by given parameter
 */
void PR_ProductionIncrease(void)
{
	int amount = 1;
	char messageBuffer[256];
	production_queue_t *queue;
	production_t *prod;

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
		Com_sprintf(messageBuffer, sizeof(messageBuffer), "Production of %s started", csi.ods[selectedIndex].name);
		MN_AddNewMessage(_("Production started"), messageBuffer, qfalse, MSG_PRODUCTION, csi.ods[selectedIndex].tech);

		/* now we select the item we just created */
		selectedQueueItem = qtrue;
		selectedIndex = queue->numItems-1;
	}

	PR_ProductionInfo();
	PR_UpdateProductionList();
}

/**
 * @brief Stops the current running production
 */
void PR_ProductionStop(void)
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

	PR_ProductionInfo();
	PR_UpdateProductionList();
}

/**
 * @brief Decrease the production amount by given parameter
 */
void PR_ProductionDecrease(void)
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
		PR_ProductionStop();
	} else {
		PR_ProductionInfo();
		PR_UpdateProductionList();
 	}
}

/**
 * @brief shift the current production up the list
 */
static void PR_ProductionUp()
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
static void PR_ProductionDown()
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
void PR_ResetProduction(void)
{
	/* add commands */
	Cmd_AddCommand("prod_init", PR_ProductionList, NULL);
	Cmd_AddCommand("prod_select", PR_ProductionSelect, NULL);
	Cmd_AddCommand("prodlist_rclick", PR_ProductionListRightClick_f, NULL);
	Cmd_AddCommand("prodlist_click", PR_ProductionListClick_f, NULL);
	Cmd_AddCommand("prod_inc", PR_ProductionIncrease, NULL);
	Cmd_AddCommand("prod_dec", PR_ProductionDecrease, NULL);
	Cmd_AddCommand("prod_stop", PR_ProductionStop, NULL);
	Cmd_AddCommand("prod_up", PR_ProductionUp, NULL);
	Cmd_AddCommand("prod_down", PR_ProductionDown, NULL);
}

