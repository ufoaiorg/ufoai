/**
 * @file cp_uforecovery_callbacks.c
 * @brief UFO recovery and storing callback functions
 * @note UFO recovery functions with UR_*
 * @note UFO storing functions with US_*
 */

/*
Copyright (C) 2002-2009 UFO: Alien Invasion.

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
#include "cp_campaign.h"
#include "cp_ufo.h"
#include "cp_uforecovery.h"
#include "cp_uforecovery_callbacks.h"

#include "../menu/m_main.h"
#include "../menu/node/m_node_text.h"

#ifdef DEBUG
#include "cp_time.h"
#endif

/** @sa ufoRecoveries_t */
typedef struct ufoRecovery_s {
	installation_t *installation;			/**< selected ufoyard for current selected ufo recovery */
	const aircraft_t *ufoTemplate;		/**< the ufo type of the current ufo recovery */
	nation_t *nation;		/**< selected nation to sell to for current ufo recovery */
	qboolean recoveryDone;	/**< recoveryDone? Then the buttons are disabled */

	int UFOprices[MAX_NATIONS];		/**< Array of prices proposed by nation. */
} ufoRecovery_t;

static ufoRecovery_t ufoRecovery;

/**
 * @brief Updates UFO recovery process.
 */
static void UR_DialogRecoveryDone (void)
{
	ufoRecovery.recoveryDone = qtrue;
}

/**
 * @brief Function to trigger UFO Recovered event.
 * @note This function prepares related cvars for the recovery dialog.
 * @note Command to call this: cp_uforecovery_init.
 */
static void UR_DialogInit_f (void)
{
	char ufoID[MAX_VAR];
	const aircraft_t *ufoCraft;

	if (Cmd_Argc() < 2) {
		Com_Printf("Usage: %s <ufoID>\n", Cmd_Argv(0));
		return;
	}

	Q_strncpyz(ufoID, Cmd_Argv(1), sizeof(ufoID));
	ufoCraft = AIR_GetAircraft(ufoID);
	/* Do nothing without UFO of this type. */
	if (!ufoCraft) {
		Com_Printf("CP_UFORecovered: UFO Type: %s is not a valid craft definition!\n", ufoID);
		return;
	}

	/* Put relevant info into missionResults array. */
	ccs.missionResults.recovery = qtrue;
	ccs.missionResults.crashsite = qfalse;
	ccs.missionResults.ufotype = ufoCraft->ufotype;

	/* Prepare related cvars. */
	Cvar_SetValue("mission_uforecovered", 1);	/* This is used in menus to enable UFO Recovery nodes. */
	memset(&ufoRecovery, 0, sizeof(ufoRecovery));
	ufoRecovery.ufoTemplate = ufoCraft;

	Cvar_Set("mn_uforecovery_actualufo", UFO_MissionResultToString());
}

/**
 * @brief Function to initialize list of storage locations for recovered UFO.
 * @note Command to call this: cp_uforecovery_store_init.
 * @sa UR_ConditionsForStoring
 */
static void UR_DialogInitStore_f (void)
{
	int i;
	int count = 0;
	linkedList_t *recoveryYardName = NULL;
	linkedList_t *recoveryYardCapacity = NULL;
	static char cap[MAX_VAR];

	/* Do nothing if recovery process is finished. */
	if (ufoRecovery.recoveryDone)
		return;

	/* Check how many bases can store this UFO. */
	for (i = 0; i < ccs.numInstallations; i++) {
		const installation_t *installation = INS_GetFoundedInstallationByIDX(i);

		if (!installation)
			continue;

		if (installation->ufoCapacity.max > 0
		 && installation->ufoCapacity.max > installation->ufoCapacity.cur) {

			Com_sprintf(cap, lengthof(cap), "%i/%i", (installation->ufoCapacity.max - installation->ufoCapacity.cur), installation->ufoCapacity.max);
			LIST_AddString(&recoveryYardName, installation->name);
			LIST_AddString(&recoveryYardCapacity, cap);
			count++;
		}
	}

	Cvar_Set("mn_uforecovery_actualufo", UFO_MissionResultToString());
	if (count == 0) {
		/* No UFO base with proper conditions, show a hint and disable list. */
		LIST_AddString(&recoveryYardName, _("No free UFO yard available."));
		MN_ExecuteConfunc("uforecovery_tabselect sell");
		MN_ExecuteConfunc("btbasesel disable");
	} else {
		MN_ExecuteConfunc("cp_basesel_select 0");
		MN_ExecuteConfunc("btbasesel enable");
	}
	MN_RegisterLinkedListText(TEXT_UFORECOVERY_UFOYARDS, recoveryYardName);
	MN_RegisterLinkedListText(TEXT_UFORECOVERY_CAPACITIES, recoveryYardCapacity);
}

/**
 * @brief Function to start UFO recovery process.
 * @note Command to call this: cp_uforecovery_store_start.
 */
static void UR_DialogStartStore_f (void)
{
	installation_t *UFOYard = NULL;
	int idx;
	int i;
	int count = 0;
	date_t date;

	if (Cmd_Argc() < 2) {
		Com_Printf("Usage: %s <installationIDX>\n", Cmd_Argv(0));
		return;
	}

	idx = atoi(Cmd_Argv(1));

	for (i = 0; i < ccs.numInstallations; i++) {
		installation_t *installation = INS_GetFoundedInstallationByIDX(i);

		if (!installation)
			continue;

		if (installation->ufoCapacity.max <= 0
		 || installation->ufoCapacity.max <= installation->ufoCapacity.cur)
		 	continue;
		
		if (count == idx) {
			UFOYard = installation;
			break;
		}
		count++;
	}

	if (!UFOYard)
		return;

	Com_sprintf(cp_messageBuffer, lengthof(cp_messageBuffer), _("Recovered %s from the battlefield. UFO is being transported to %s."),
			UFO_TypeToName(ufoRecovery.ufoTemplate->ufotype), UFOYard->name);
	MS_AddNewMessage(_("UFO Recovery"), cp_messageBuffer, qfalse, MSG_STANDARD, NULL);
/*	UR_Prepare(UFOYard, ufoRecovery.ufoTemplate->ufotype); */
	date = ccs.date;
	date.day += (int) RECOVERY_DELAY;
/*	date.sec += (RECOVERY_DELAY - (int) RECOVERY_DELAY) % SECONDS_PER_DAY;*/

	US_StoreUFO(ufoRecovery.ufoTemplate, UFOYard, date);

	UR_DialogRecoveryDone();
}

/**
 * @brief Function to initialize list to sell recovered UFO to desired nation.
 * @note Command to call this: cp_uforecovery_sell_init.
 */
static void UR_DialogInitSell_f (void)
{
	int i;
	int nations = 0;
	static char recoveryNationSelectPopup[MAX_SMALLMENUTEXTLEN];

	/* Do nothing if recovery process is finished. */
	if (ufoRecovery.recoveryDone)
		return;

	if (!ufoRecovery.ufoTemplate)
		return;

	memset(ufoRecovery.UFOprices, -1, sizeof(ufoRecovery.UFOprices));

	recoveryNationSelectPopup[0] = '\0';

	for (i = 0; i < ccs.numNations; i++) {
		const nation_t *nation = &ccs.nations[i];
		nations++;
		/* Calculate price offered by nation only if this is first popup opening. */
		if (!ufoRecovery.nation) {
			ufoRecovery.UFOprices[i] = (int) (ufoRecovery.ufoTemplate->price * (.85f + frand() * .3f));
			/* Nation will pay less if corrupted */
			ufoRecovery.UFOprices[i] = (int) (ufoRecovery.UFOprices[i] * exp(-nation->stats[0].xviInfection / 20.0f));
		}
		Com_sprintf(recoveryNationSelectPopup + strlen(recoveryNationSelectPopup), lengthof(recoveryNationSelectPopup),
				"%s\t\t\t%i\t\t%s\n", _(nation->name), ufoRecovery.UFOprices[i], NAT_GetHappinessString(nation));
	}

	/* Do nothing without at least one nation. */
	if (nations == 0)
		return;

	MN_RegisterText(TEXT_UFORECOVERY_NATIONS, recoveryNationSelectPopup);
	MN_ExecuteConfunc("btnatsel disable");
}

/**
 * @brief Finds the nation to which recovered UFO will be sold.
 * @note The nation selection is being done here.
 * @note Callback command: cp_uforecovery_nationlist_click.
 */
static void UR_DialogSelectSellNation_f (void)
{
	int num;
	nation_t *nation;

	if (Cmd_Argc() < 2) {
		Com_Printf("Usage: %s <nationid>\n", Cmd_Argv(0));
		return;
	}

	num = atoi(Cmd_Argv(1));

	/* don't do anything if index is higher than visible nations */
	if (0 > num || num >= ccs.numNations)
		return;

	nation = &ccs.nations[num];
	ufoRecovery.nation = nation;
	Com_DPrintf(DEBUG_CLIENT, "CP_UFORecoveryNationSelectPopup_f: picked nation: %s\n", nation->name);

	Cvar_Set("mission_recoverynation", _(nation->name));
	MN_ExecuteConfunc("btnatsel enable");
}

/**
 * @brief Function to start UFO selling process.
 * @note Command to call this: cp_uforecovery_sell_start.
 */
static void UR_DialogStartSell_f (void)
{
	nation_t *nation;
	int i;

	if (!ufoRecovery.nation)
		return;

	nation = ufoRecovery.nation;
	assert(nation->name);
	if (ufoRecovery.UFOprices[nation->idx] == -1) {
		Com_Printf("CP_UFOSellStart_f: Error: ufo price of -1 - nation: '%s'\n", nation->id);
		return;
	}
#if 0
	if (ufoRecovery.selectedStorage) {
		Com_sprintf(cp_messageBuffer, sizeof(cp_messageBuffer), _("Sold previously recovered %s from %s to nation %s, gained %i credits."), UFO_TypeToName(
				ufoRecovery.selectedStorage->ufoTemplate->ufotype), ufoRecovery.selectedStorage->base->name, _(nation->name), ufoRecovery.UFOprices[nation->idx]);
	} else
#endif
	{
		Com_sprintf(cp_messageBuffer, sizeof(cp_messageBuffer), _("Recovered %s from the battlefield. UFO sold to nation %s, gained %i credits."), UFO_TypeToName(
				ufoRecovery.ufoTemplate->ufotype), _(nation->name), ufoRecovery.UFOprices[nation->idx]);
	}
	MS_AddNewMessage(_("UFO Recovery"), cp_messageBuffer, qfalse, MSG_STANDARD, NULL);
	CL_UpdateCredits(ccs.credits + ufoRecovery.UFOprices[nation->idx]);

	/* update nation happiness */
	for (i = 0; i < ccs.numNations; i++) {
		if (ccs.nations + i == nation)
			/* nation is happy because it got the UFO */
			NAT_SetHappiness(nation, nation->stats[0].happiness + HAPPINESS_UFO_SALE_GAIN);
		else
			/* nation is unhappy because it wanted the UFO */
			NAT_SetHappiness(&ccs.nations[i], ccs.nations[i].stats[0].happiness + HAPPINESS_UFO_SALE_LOSS);
	}

	/* UFO recovery process is done, disable buttons. */
	UR_DialogRecoveryDone();
}


/* === UFO Storing === */

#ifdef DEBUG
/**
 * @brief Debug callback to list ufostores
 */
static void US_ListStoredUFOs_f (void)
{
	int i;

	Com_Printf("Number of stored UFOs: %i\n", ccs.numStoredUFOs);
	for (i = 0; i < ccs.numStoredUFOs; i++) {
		const storedUFO_t *ufo = US_GetStoredUFOByIDX(i);
		const base_t *prodBase = PR_ProductionBase(ufo->disassembly);
		dateLong_t date;

		Com_Printf("IDX: %i\n", ufo->idx);
		Com_Printf("id: %s\n", ufo->id);
		Com_Printf("stored at %s\n", (ufo->installation) ? ufo->installation->name : "NOWHERE");

		CL_DateConvertLong(&(ufo->arrive), &date);
		Com_Printf("arrived at: %i %s %02i, %02i:%02i\n", date.year,
		Date_GetMonthName(date.month - 1), date.day, date.hour, date.min);

		if (ufo->ufoTemplate->tech->base)
			Com_Printf("tech being researched at %s\n", ufo->ufoTemplate->tech->base->name);
		if (prodBase)
			Com_Printf("being disassembled at %s\n", prodBase->name);
	}
}

/**
 * @brief Adds an UFO to the stores
 */
static void US_StoreUFO_f (void)
{
	char ufoId[MAX_VAR];
	int installationIDX;
	installation_t *installation;
	aircraft_t *ufoType = NULL;
	int i;

	if (Cmd_Argc() <= 2) {
		Com_Printf("Usage: %s <ufoType> <installationIdx>\n", Cmd_Argv(0));
		return;
	}

	Q_strncpyz(ufoId, Cmd_Argv(1), sizeof(ufoId));
	installationIDX = atoi(Cmd_Argv(2));

	/* Get The UFO Yard */
	if (installationIDX < 0 || installationIDX >= MAX_INSTALLATIONS) {
		Com_Printf("US_StoreUFO_f: Invalid Installation index.\n");
		return;
	}		
	installation = INS_GetFoundedInstallationByIDX(installationIDX);
	if (!installation) {
		Com_Printf("US_StoreUFO_f: There is no Installation: idx=%i.\n", installationIDX);
		return;
	}

	/* Get UFO Type */
	for (i = 0; i < ccs.numAircraftTemplates; i++) {
		if (strstr(ccs.aircraftTemplates[i].id, ufoId)) {
			ufoType = &ccs.aircraftTemplates[i];
			break;
		}
	}
	if (ufoType == NULL) {
		Com_Printf("US_StoreUFO_f: In valid UFO Id.\n");
		return;
	}

	US_StoreUFO(ufoType, installation, ccs.date);
}

/**
 * @brief Removes an UFO from the stores
 */
static void US_RemoveStoredUFO_f (void)
{
	if (Cmd_Argc() < 2) {
		Com_Printf("Usage: %s <idx>\n", Cmd_Argv(0));
		return;
	} else {
		const int idx = atoi(Cmd_Argv(1));
		storedUFO_t *storedUFO = US_GetStoredUFOByIDX(idx);
		if (!storedUFO) {
			Com_Printf("US_RemoveStoredUFO_f: No such ufo index.\n");
			return;
		}
		US_RemoveStoredUFO(storedUFO);
	}
}

#endif

void UR_InitCallbacks (void)
{
	Cmd_AddCommand("cp_uforecovery_init", UR_DialogInit_f, "Function to trigger UFO Recovered event");
	Cmd_AddCommand("cp_uforecovery_sell_init", UR_DialogInitSell_f, "Function to initialize sell recovered UFO to desired nation.");
	Cmd_AddCommand("cp_uforecovery_store_init", UR_DialogInitStore_f, "Function to initialize store recovered UFO in desired base.");
	Cmd_AddCommand("cp_uforecovery_nationlist_click", UR_DialogSelectSellNation_f, "Callback for recovery sell to nation list.");
	Cmd_AddCommand("cp_uforecovery_store_start", UR_DialogStartStore_f, "Function to start UFO recovery processing.");
	Cmd_AddCommand("cp_uforecovery_sell_start", UR_DialogStartSell_f, "Function to start UFO selling processing.");

#ifdef DEBUG
	Cmd_AddCommand("debug_liststoredufos", US_ListStoredUFOs_f, "Debug function to list UFOs in Hangars.");
	Cmd_AddCommand("debug_storeufo", US_StoreUFO_f, "Debug function to Add UFO to Hangars.");
	Cmd_AddCommand("debug_removestoredufo", US_RemoveStoredUFO_f, "Debug function to Remove UFO from Hangars.");
#endif
}

void UR_ShutdownCallbacks (void)
{
	Cmd_RemoveCommand("cp_uforecovery_init");
	Cmd_RemoveCommand("cp_uforecovery_sell_init");
	Cmd_RemoveCommand("cp_uforecovery_store_init");
	Cmd_RemoveCommand("cp_uforecovery_nationlist_click");
	Cmd_RemoveCommand("cp_uforecovery_store_start");
	Cmd_RemoveCommand("cp_uforecovery_sell_start");
#ifdef DEBUG
	Cmd_RemoveCommand("debug_liststoredufos");
	Cmd_RemoveCommand("debug_storeufo");
	Cmd_RemoveCommand("debug_removestoredufo");
#endif
}
