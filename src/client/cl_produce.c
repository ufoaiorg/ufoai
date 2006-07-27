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

/* holds the current active buytype category */
static int produceCategory;

/**
 * @brief Checks whether an item is finished
 * @sa CL_CampaignRun
 */
void PR_ProductionRun(void)
{
	int i;
	char messageBuffer[256];
	objDef_t *od;
	technology_t *t;

	for (i = 0; i < MAX_BASES; i++) {
		Com_Printf("PR_ProductionRun for base %i - timeLeft: %i\n", i, gd.productions[i].timeLeft);
		if (!gd.bases[i].founded)
			continue;
		/* no active production */
		if (gd.productions[i].objID < 0)
			continue;

		od = &csi.ods[gd.productions[i].objID];
		t = (technology_t*)(od->tech);
#ifdef DEBUG
		if (!t)
			Sys_Error("PR_ProductionRun: No tech pointer for object id %i ('%s')\n", gd.productions[i].objID, od->kurz);
#endif
		gd.productions[i].timeLeft--;
		if (gd.productions[i].timeLeft <= 0) {
			gd.productions[i].timeLeft = t->produceTime;
			gd.productions[i].amount--;
			/* switch to no running production */
			if (gd.productions[i].amount<=0) {
				gd.productions[i].objID = -1;
				Com_sprintf(messageBuffer, sizeof(messageBuffer), _("Production of %s finished"), od->name);
				MN_AddNewMessage(_("Production finished"), messageBuffer, qfalse, MSG_PRODUCTION, t);
			}
			/* now add it to equipment */
			/* FIXME: overflow possible */
			ccs.eCampaign.num[gd.productions[i].objID]++;
		}
	}
}

/**
 * @brief Prints information about the current item in production
 * @note -1 for objID means that there ic no production in this base
 */
static void PR_ProductionInfo (void)
{
	static char productionInfo[512];
	technology_t *t;
	objDef_t *od;

	assert(baseCurrent);

	if (gd.productions[baseCurrent->idx].objID >= 0) {
		od = &csi.ods[gd.productions[baseCurrent->idx].objID];
		t = (technology_t*)(od->tech);
		Com_sprintf(productionInfo, sizeof(productionInfo), "%s\t%i\n", od->name, gd.productions[baseCurrent->idx].amount);
		Q_strcat(productionInfo, va(_("Costs per item\t%i c\n"), t->produceCosts), sizeof(productionInfo) );
	} else {
		Com_sprintf(productionInfo, sizeof(productionInfo), _("No running productions"));
	}
	menuText[TEXT_STANDARD] = productionInfo;
}

/**
 * @brief Click function for production list
 */
static void PR_ProductionListClick_f (void)
{
	int i, j, num;
	objDef_t *od;
	technology_t *t;

	/* can be called from everywhere without a started game */
	if (!baseCurrent ||!curCampaign)
		return;

	/* there is already a running production - stop it first */
	if (gd.productions[baseCurrent->idx].amount > 0 ) {
		PR_ProductionInfo();
		return;
	}

	/* not enough parameters */
	if (Cmd_Argc() < 2)
		return;

	/* clicked which item? */
	num = atoi(Cmd_Argv(1));
	for (j=0, i = 0, od = csi.ods; i < csi.numODs; i++, od++) {
		t = (technology_t*)(od->tech);
#ifdef DEBUG
		if (!t)
			Sys_Error("PR_ProductionListClick_f: No tech pointer for object id %i ('%s')\n", gd.productions[i].objID, od->kurz);
#endif
		/* we can produce what was researched before */
		if (od->buytype == produceCategory && RS_IsResearched_ptr(t)) {
			if (j==num) {
				gd.productions[baseCurrent->idx].objID = i;
				gd.productions[baseCurrent->idx].amount = 0;
				gd.productions[baseCurrent->idx].timeLeft = t->produceTime;
				PR_ProductionInfo();
				return;
			}
			j++;
		}
	}
}

/**
 * @brief Will fill the list of produceable items
 */
static void PR_ProductionList (void)
{
	int i;
	static char productionList[1024];
	objDef_t *od;
	productionList[0] = '\0';

	/* can be called from everywhere without a started game */
	if (!baseCurrent ||!curCampaign)
		return;

	if (Cmd_Argc() < 2) {
		Com_Printf("Usage: prod_init <category>\n");
		return;
	}
	produceCategory = atoi(Cmd_Argv(1));

	/* go through all object definitions */
	for (i = 0, od = csi.ods; i < csi.numODs; i++, od++) {
		/* we can produce what was researched before */
		if (od->buytype == produceCategory && RS_IsResearched_ptr(od->tech)) {
			Q_strcat(productionList, va("%s\n", od->name), sizeof(productionList));
		}
	}
	/* bind the menu text to our static char array */
	menuText[TEXT_LIST] = productionList;
	/* now print the information about the current item in production */
	PR_ProductionInfo();
}

/**
 * @brief Sets the production array to -1
 */
void PR_ProductionInit(void)
{
	Com_DPrintf("Reset all productions\n");
	memset(gd.productions, -1, sizeof(production_t)*MAX_BASES);
}

/**
 * @brief Increase the production amount by given parameter
 */
void PR_ProductionIncrease(void)
{
	int amount = 1;
	char messageBuffer[256];

	if (Cmd_Argc() == 2)
		amount = atoi(Cmd_Argv(1));

	if (!baseCurrent)
		return;

	if (gd.productions[baseCurrent->idx].objID >= 0) {
		if (gd.productions[baseCurrent->idx].amount == 0) {
			Com_sprintf(messageBuffer, sizeof(messageBuffer), "Production of %s started", csi.ods[gd.productions[baseCurrent->idx].objID].name);
			MN_AddNewMessage(_("Production started"), messageBuffer, qfalse, MSG_PRODUCTION, csi.ods[gd.productions[baseCurrent->idx].objID].tech);
		}
		/* FIXME: overflow possible - check max item count */
		gd.productions[baseCurrent->idx].amount += amount;
	}
	PR_ProductionInfo();
}

/**
 * @brief Decrease the production amount by given parameter
 */
void PR_ProductionDecrease(void)
{
	int amount = 1;

	if (Cmd_Argc() == 2)
		amount = atoi(Cmd_Argv(1));

	if (!baseCurrent)
		return;

	if (gd.productions[baseCurrent->idx].objID >= 0) {
		gd.productions[baseCurrent->idx].amount -= amount;
		if (gd.productions[baseCurrent->idx].amount <= 0) {
			gd.productions[baseCurrent->idx].objID = -1;
		}
	}
	PR_ProductionInfo();
}

/**
  * @brief This is more or less the initial
  * Bind some of the functions in this file to console-commands that you can call ingame.
  * Called from MN_ResetMenus resp. CL_InitLocal
  */
void PR_ResetProduction(void)
{
	/* add commands */
	Cmd_AddCommand("prod_init", PR_ProductionList);
	Cmd_AddCommand("prodlist_click", PR_ProductionListClick_f);
	Cmd_AddCommand("prod_inc", PR_ProductionIncrease);
	Cmd_AddCommand("prod_dec", PR_ProductionDecrease);
}

