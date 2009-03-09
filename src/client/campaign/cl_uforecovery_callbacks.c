/**
 * @file cl_uforecovery_callbacks.c
 * @brief UFO recovery callback functions
 * @note Functions with UR_*
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
#include "cl_campaign.h"
#include "cl_ufo.h"
#include "cl_uforecovery.h"
#include "cl_uforecovery_callbacks.h"

#include "../cl_menu.h"
#include "../menu/node/m_node_text.h"


/** @sa ufoRecoveries_t */
typedef struct ufoRecovery_s {
	base_t *base;			/**< selected base for current selected ufo recovery */
	ufoType_t ufoType;		/**< the ufo type of the current ufo recovery */
	nation_t *nation;		/**< selected nation to sell to for current ufo recovery */
	qboolean recoveryDone;	/**< recoveryDone? Then the buttons are disabled */
	base_t *UFObases[MAX_BASES];	/**< Array of base indexes where we can store UFO. */
	int baseHasUFOHangarCount;		/**< number of entries in the UFObases array */
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
	int i;
	ufoType_t UFOtype;
	aircraft_t *ufocraft;
	qboolean ufofound = qfalse;

	if (Cmd_Argc() < 2) {
		Com_Printf("Usage: %s <UFOType>\n", Cmd_Argv(0));
		return;
	}

	if ((atoi(Cmd_Argv(1)) >= 0) && (atoi(Cmd_Argv(1)) < UFO_MAX)) {
		UFOtype = atoi(Cmd_Argv(1));
	} else {
		Com_Printf("CP_UFORecovered: UFOType: %i does not exist!\n", atoi(Cmd_Argv(1)));
		return;
	}

	ufocraft = NULL;
	/* Find ufo sample of given ufotype. */
	for (i = 0; i < numAircraftTemplates; i++) {
		ufocraft = &aircraftTemplates[i];
		if (ufocraft->type != AIRCRAFT_UFO)
			continue;
		if (ufocraft->ufotype == UFOtype) {
			ufofound = qtrue;
			break;
		}
	}
	/* Do nothing without UFO of this type. */
	if (!ufofound) {
		Com_Printf("CP_UFORecovered: UFOType: %i does not have valid craft definition!\n", atoi(Cmd_Argv(1)));
		return;
	}

	/* Put relevant info into missionresults array. */
	missionresults.recovery = qtrue;
	missionresults.crashsite = qfalse;
	missionresults.ufotype = ufocraft->ufotype;

	/* Prepare related cvars. */
	Cvar_SetValue("mission_uforecovered", 1);	/* This is used in menus to enable UFO Recovery nodes. */
	memset(&ufoRecovery, 0, sizeof(ufoRecovery));
	ufoRecovery.ufoType = UFOtype;

}

/**
 * @brief Function to initialize list of storage locations for recovered UFO.
 * @note Command to call this: cp_uforecovery_store_init.
 * @sa UR_ConditionsForStoring
 */
static void UR_DialogInitStore_f (void)
{
	int i;
	aircraft_t *ufocraft;
	static char recoveryBaseSelectPopup[512];
	qboolean ufofound = qfalse;

	/* Do nothing without any base. */
	if (!ccs.numBases)
		return;

	/* Do nothing if recovery process is finished. */
	if (ufoRecovery.recoveryDone)
		return;

	/* Find ufo sample of given ufotype. */
	for (i = 0; i < numAircraftTemplates; i++) {
		ufocraft = &aircraftTemplates[i];
		if (ufocraft->type != AIRCRAFT_UFO)
			continue;
		if (ufocraft->ufotype == ufoRecovery.ufoType) {
			ufofound = qtrue;
			break;
		}
	}

	/* Do nothing without UFO of this type. */
	if (!ufofound) {
		Com_Printf("CP_UFORecoveredStore_f: UFOType: %i does not have valid craft definition!\n", ufoRecovery.ufoType);
		return;
	}

	/* Clear UFObases. */
	memset(ufoRecovery.UFObases, 0, sizeof(ufoRecovery.UFObases));

	recoveryBaseSelectPopup[0] = '\0';
	/* Check how many bases can store this UFO. */
	for (i = 0; i < MAX_BASES; i++) {
		base_t *base = B_GetFoundedBaseByIDX(i);
		if (!base)
			continue;
		if (UR_ConditionsForStoring(base, ufocraft)) {
			Q_strcat(recoveryBaseSelectPopup, base->name, sizeof(recoveryBaseSelectPopup));
			Q_strcat(recoveryBaseSelectPopup, "\n", sizeof(recoveryBaseSelectPopup));
			ufoRecovery.UFObases[ufoRecovery.baseHasUFOHangarCount++] = base;
		}
	}

	/* If only one base with UFO hangars, the recovery will be done in this base. */
	switch (ufoRecovery.baseHasUFOHangarCount) {
	case 0:
		/* No UFO base with proper conditions, show a hint and disable list. */
		Q_strcat(recoveryBaseSelectPopup, _("No ufo hangar or ufo yard available."), sizeof(recoveryBaseSelectPopup));
		MN_ExecuteConfunc("cp_basesel_disable");
		break;
	case 1:
		/* there should only be one entry in UFObases - so use that one. */
		ufoRecovery.base = ufoRecovery.UFObases[0];
		/** @todo preselect base */
		/*CP_UFORecoveredStart_f(); */

		break;
	default:
		if (!ufoRecovery.base)
			ufoRecovery.base = ufoRecovery.UFObases[0];
		if (ufoRecovery.base)
			Cvar_Set("mission_recoverybase", ufoRecovery.base->name);
		break;
	}
	/** @todo set this description to new value after replace */
	Cvar_Set("mn_uforecovery_actualufo",UFO_MissionResultToString());
	MN_RegisterText(TEXT_UFORECOVERY_BASESTORAGE, recoveryBaseSelectPopup);
}

/**
 * @brief Finds the destination base for UFO recovery.
 * @note The base selection is being done here.
 * @note Callback command: cp_uforecovery_baselist_click.
 */
static void UR_DialogSelectStorageBase_f (void)
{
	int num;

	if (Cmd_Argc() < 2) {
		Com_Printf("Usage: %s <baseid>\n", Cmd_Argv(0));
		return;
	}

	num = atoi(Cmd_Argv(1));
	if (num < 0 || num >= MAX_BASES || !ufoRecovery.UFObases[num])
		return;

	ufoRecovery.base = ufoRecovery.UFObases[num];
	Com_DPrintf(DEBUG_CLIENT, "CP_UFORecoveryBaseSelectPopup_f: picked base: %s\n",
		ufoRecovery.base->name);

	Cvar_Set("mission_recoverybase", ufoRecovery.base->name);
	MN_ExecuteConfunc("btbasesel_enable");
}

/**
 * @brief Function to start UFO recovery process.
 * @note Command to call this: cp_uforecovery_store_start.
 * @note This needs the ufoRecovery base value set
 */
static void UR_DialogStartStore_f (void)
{
	base_t *base = ufoRecovery.base;
	if (!base)
		return;

	Com_sprintf(cp_messageBuffer, sizeof(cp_messageBuffer),
		_("Recovered %s from the battlefield. UFO is being transported to base %s."),
		UFO_TypeToName(ufoRecovery.ufoType), base->name);
	MS_AddNewMessage(_("UFO Recovery"), cp_messageBuffer, qfalse, MSG_STANDARD, NULL);
	UR_Prepare(base, ufoRecovery.ufoType);

	/* UFO recovery process is done, disable buttons. */
	UR_DialogRecoveryDone();
}

/**
 * @brief Function to initialize list to sell recovered UFO to desired nation.
 * @note Command to call this: cp_uforecovery_sell_init.
 */
static void UR_DialogInitSell_f (void)
{
	int i, nations = 0;
	aircraft_t *ufocraft;
	static char recoveryNationSelectPopup[MAX_SMALLMENUTEXTLEN];

	/* Do nothing if recovery process is finished. */
	if (ufoRecovery.recoveryDone)
		return;

	ufocraft = NULL;
	/* Find ufo sample of given ufotype. */
	for (i = 0; i < numAircraftTemplates; i++) {
		ufocraft = &aircraftTemplates[i];
		if (ufocraft->type != AIRCRAFT_UFO)
			continue;
		if (ufocraft->ufotype == ufoRecovery.ufoType)
			break;
	}
	if (!ufocraft)
		return;

	if (!ufoRecovery.nation)
		memset(ufoRecovery.UFOprices, -1, sizeof(ufoRecovery.UFOprices));

	recoveryNationSelectPopup[0] = '\0';

	for (i = 0; i < ccs.numNations; i++) {
		const nation_t *nation = &ccs.nations[i];
		nations++;
		/* Calculate price offered by nation only if this is first popup opening. */
		if (!ufoRecovery.nation) {
			ufoRecovery.UFOprices[i] = (int) (ufocraft->price * (.85f + frand() * .3f));
			/* Nation will pay less if corrupted */
			ufoRecovery.UFOprices[i] = (int) (ufoRecovery.UFOprices[i] * exp(-nation->stats[0].xviInfection / 20.0f));
		}
		Com_sprintf(recoveryNationSelectPopup + strlen(recoveryNationSelectPopup),
			sizeof(recoveryNationSelectPopup), "%s\t\t\t%i\t\t%s\n",
			_(nation->name), ufoRecovery.UFOprices[i], NAT_GetHappinessString(nation));
	}

	/* Do nothing without at least one nation. */
	if (nations == 0)
		return;

	MN_RegisterText(TEXT_UFORECOVERY_NATIONS, recoveryNationSelectPopup);
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
	MN_ExecuteConfunc("btnatsel_enable");
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
	Com_sprintf(cp_messageBuffer, sizeof(cp_messageBuffer), _("Recovered %s from the battlefield. UFO sold to nation %s, gained %i credits."),
		UFO_TypeToName(ufoRecovery.ufoType), _(nation->name), ufoRecovery.UFOprices[nation->idx]);
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

void UR_InitCallbacks (void)
{
	Cmd_AddCommand("cp_uforecovery_init", UR_DialogInit_f, "Function to trigger UFO Recovered event");
	Cmd_AddCommand("cp_uforecovery_sell_init", UR_DialogInitSell_f, "Function to initialize sell recovered UFO to desired nation.");
	Cmd_AddCommand("cp_uforecovery_store_init", UR_DialogInitStore_f, "Function to initialize store recovered UFO in desired base.");
	Cmd_AddCommand("cp_uforecovery_nationlist_click", UR_DialogSelectSellNation_f, "Callback for recovery sell to nation list.");
	Cmd_AddCommand("cp_uforecovery_baselist_click", UR_DialogSelectStorageBase_f, "Callback for recovery store in base list.");
	Cmd_AddCommand("cp_uforecovery_store_start", UR_DialogStartStore_f, "Function to start UFO recovery processing.");
	Cmd_AddCommand("cp_uforecovery_sell_start", UR_DialogStartSell_f, "Function to start UFO selling processing.");

}

void UR_ShutdownCallbacks (void)
{
	Cmd_RemoveCommand("cp_uforecovery_init");
	Cmd_RemoveCommand("cp_uforecovery_sell_init");
	Cmd_RemoveCommand("cp_uforecovery_store_init");
	Cmd_RemoveCommand("cp_uforecovery_nationlist_click");
	Cmd_RemoveCommand("cp_uforecovery_baselist_click");
	Cmd_RemoveCommand("cp_uforecovery_store_start");
	Cmd_RemoveCommand("cp_uforecovery_sell_start");

}
