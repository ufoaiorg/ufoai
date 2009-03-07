/**
 * @file cl_uforecovery.c
 * @brief UFO recovery
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
#include "../cl_menu.h"
#include "../menu/node/m_node_text.h"
#include "cl_campaign.h"
#include "cl_ufo.h"
#include "cl_uforecovery.h"
#include "cp_aircraft.h"

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

/*==================================
Campaign onwin functions
==================================*/

/**
 * @brief Send an email to list all recovered item.
 * @sa CL_EventAddMail_f
 * @sa CL_NewEventMail
 */
static void UR_SendMail (const aircraft_t *ufocraft, const base_t *base)
{
	int i;
	components_t *comp;
	objDef_t *compOd;
	eventMail_t *mail;
	char body[512] = "";

	assert(ufocraft);
	assert(base);

	if (missionresults.crashsite) {
		/* take the source mail and create a copy of it */
		mail = CL_NewEventMail("ufo_crashed_report", va("ufo_crashed_report%i", ccs.date.sec), NULL);
		if (!mail)
			Sys_Error("UR_SendMail: ufo_crashed_report wasn't found");
		/* we need the source mail body here - this may not be NULL */
		if (!mail->body)
			Sys_Error("UR_SendMail: ufo_crashed_report has no mail body");

		/* Find components definition. */
		comp = CL_GetComponentsByItem(INVSH_GetItemByID(ufocraft->id));
		assert(comp);

		/* List all components of crashed UFO. */
		for (i = 0; i < comp->numItemtypes; i++) {
			compOd = comp->items[i];
			assert(compOd);
			if (comp->item_amount2[i] > 0)
				Q_strcat(body, va(_("  * %i x\t%s\n"), comp->item_amount2[i], compOd->name), sizeof(body));
		}
	} else {
		/* take the source mail and create a copy of it */
		mail = CL_NewEventMail("ufo_recovery_report", va("ufo_recovery_report%i", ccs.date.sec), NULL);
		if (!mail)
			Sys_Error("UR_SendMail: ufo_recovery_report wasn't found");
		/* we need the source mail body here - this may not be NULL */
		if (!mail->body)
			Sys_Error("UR_SendMail: ufo_recovery_report has no mail body");

		/* Find components definition. */
		comp = CL_GetComponentsByItem(INVSH_GetItemByID(ufocraft->id));
		assert(comp);

		/* List all components of crashed UFO. */
		for (i = 0; i < comp->numItemtypes; i++) {
			compOd = comp->items[i];
			assert(compOd);
			if (comp->item_amount[i] > 0)
				Q_strcat(body, va(_("  * %s\n"), compOd->name), sizeof(body));
		}
	}
	assert(mail);

	/* don't free the old mail body here - it's the string of the source mail */
	mail->body = Mem_PoolStrDup(va(_(mail->body), UFO_TypeToName(missionresults.ufotype), base->name, body), cl_campaignPool, 0);

	/* update subject */
	/* Insert name of the mission in the template */
	mail->subject = Mem_PoolStrDup(va(_(mail->subject), base->name), cl_campaignPool, 0);

	/* Add the mail to unread mail */
	Cmd_ExecuteString(va("addeventmail %s", mail->id));
}

/**
 * @brief Function to trigger UFO Recovered event.
 * @note This function prepares related cvars for the recovery popup.
 * @note Command to call this: cp_uforecovery.
 */
static void CP_UFORecovered_f (void)
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
 * @brief Updates UFO recovery process and disables buttons.
 */
static void CP_UFORecoveryDone (void)
{
	ufoRecovery.recoveryDone = qtrue;
}

/**
 * @brief Prepares UFO recovery in global recoveries array.
 * @param[in] base Pointer to the base, where the UFO recovery will be made.
 * @sa UR_ProcessActive
 * @sa UR_ConditionsForStoring
 */
static void UR_Prepare (base_t *base)
{
	int i;
	ufoRecoveries_t *recovery = NULL;
	aircraft_t *ufocraft = NULL;
	date_t event;

	assert(base);

	/* Find ufo sample of given ufotype. */
	for (i = 0; i < numAircraftTemplates; i++) {
		ufocraft = &aircraftTemplates[i];
		if (ufocraft->type != AIRCRAFT_UFO)
			continue;
		if (ufocraft->ufotype == ufoRecovery.ufoType)
			break;
	}
	assert(ufocraft);

	/* Find free uforecovery slot. */
	for (i = 0; i < MAX_RECOVERIES; i++) {
		if (!ccs.recoveries[i].active) {
			/* Make sure it is empty here. */
			memset(&ccs.recoveries[i], 0, sizeof(ccs.recoveries[i]));
			recovery = &ccs.recoveries[i];
			break;
		}
	}

	if (!recovery) {
		Com_Printf("UR_Prepare: free recovery slot not found.\n");
		return;
	}
	assert(recovery);

	/* Prepare date of the recovery event - always two days after mission. */
	event = ccs.date;
	/* if you change these 2 days to something other you
	 * have to review all translations, too */
	event.day += 2;
	/* Prepare recovery. */
	recovery->active = qtrue;
	recovery->base = base;
	recovery->ufoTemplate = ufocraft->tpl;
	recovery->event = event;

	/* Update base capacity. */
	if (ufocraft->size == AIRCRAFT_LARGE) {
		/* Large UFOs can only fit in large UFO hangars */
		if (base->capacities[CAP_UFOHANGARS_LARGE].max > base->capacities[CAP_UFOHANGARS_LARGE].cur)
			base->capacities[CAP_UFOHANGARS_LARGE].cur++;
		else
			/* no more room -- this shouldn't happen as we've used UR_ConditionsForStoring */
			Com_Printf("UR_Prepare: No room in large UFO hangars to store %s\n", ufocraft->name);
	} else {
		/* Small UFOs can only fit in small UFO hangars */
		if (base->capacities[CAP_UFOHANGARS_SMALL].max > base->capacities[CAP_UFOHANGARS_SMALL].cur)
			base->capacities[CAP_UFOHANGARS_SMALL].cur++;
		else
			/* no more room -- this shouldn't happen as we've used UR_ConditionsForStoring */
			Com_Printf("UR_Prepare: No room in small UFO hangars to store %s\n", ufocraft->name);
	}

	Com_DPrintf(DEBUG_CLIENT, "UR_Prepare: the recovery entry in global array is done; base: %s, ufotype: %s, date: %i\n",
		base->name, recovery->ufoTemplate->id, recovery->event.day);

	/* Send an email */
	UR_SendMail(ufocraft, base);
}

/**
 * @brief Function to start UFO recovery process.
 * @note Command to call this: cp_uforecoverystart.
 * @note This needs the ufoRecovery base value set
 */
static void CP_UFORecoveredStart_f (void)
{
	base_t *base = ufoRecovery.base;
	if (!base)
		return;

	Com_sprintf(cp_messageBuffer, sizeof(cp_messageBuffer),
		_("Recovered %s from the battlefield. UFO is being transported to base %s."),
		UFO_TypeToName(ufoRecovery.ufoType), base->name);
	MS_AddNewMessage(_("UFO Recovery"), cp_messageBuffer, qfalse, MSG_STANDARD, NULL);
	UR_Prepare(base);

	/* UFO recovery process is done, disable buttons. */
	CP_UFORecoveryDone();
}

/**
 * @brief Function to store recovered UFO in desired base.
 * @note Command to call this: cp_uforecoverystore.
 * @sa UR_ConditionsForStoring
 */
static void CP_UFORecoveredStore_f (void)
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
		/* No UFO base with proper conditions, do nothing. */
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
	MN_RegisterText(TEXT_UFORECOVERY_BASESTORAGE, recoveryBaseSelectPopup);
}

/**
 * @brief Finds the destination base for UFO recovery.
 * @note The base selection is being done here.
 * @note Callback command: cp_uforecovery_baselist_click.
 */
static void CP_UFORecoveryBaseSelectPopup_f (void)
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
 * @brief Finds the nation to which recovered UFO will be sold.
 * @note The nation selection is being done here.
 * @note Callback command: cp_uforecovery_nationlist_click.
 */
static void CP_UFORecoveryNationSelectPopup_f (void)
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
 * @note Command to call this: cp_ufosellstart.
 */
static void CP_UFOSellStart_f (void)
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
	CP_UFORecoveryDone();
}

/**
 * @brief Function to sell recovered UFO to desired nation.
 * @note Command to call this: cp_uforecoverysell.
 */
static void CP_UFORecoveredSell_f (void)
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

	MN_RegisterText(TEXT_LIST, recoveryNationSelectPopup);
	/*MN_PushMenu("popup_recoverynationlist", NULL);*/
	MN_RegisterText(TEXT_UFORECOVERY_NATIONS, recoveryNationSelectPopup);
}

/**
 * @brief Function to process crashed UFO.
 * @note Command to call this: cp_ufocrashed.
 */
static void CP_UFOCrashed_f (void)
{
	int i;
	ufoType_t UFOtype;
	aircraft_t *aircraft, *ufocraft;
	qboolean ufofound = qfalse;
	components_t *comp;
	itemsTmp_t *cargo;

	if (!baseCurrent || !ccs.interceptAircraft)
		return;

	if (Cmd_Argc() < 2) {
		Com_Printf("Usage: %s <UFOType>\n", Cmd_Argv(0));
		return;
	}

	if ((atoi(Cmd_Argv(1)) >= 0) && (atoi(Cmd_Argv(1)) < UFO_MAX)) {
		UFOtype = atoi(Cmd_Argv(1));
	} else {
		UFOtype = UFO_ShortNameToID(Cmd_Argv(1));
		if (UFOtype == UFO_MAX) {
			Com_Printf("CP_UFOCrashed_f: UFOType: %i does not exist!\n", atoi(Cmd_Argv(1)));
			return;
		}
	}

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
		Com_Printf("CP_UFOCrashed_f: UFOType: %i does not have valid craft definition!\n", atoi(Cmd_Argv(1)));
		return;
	}

	/* Find dropship. */
	aircraft = ccs.interceptAircraft;
	assert(aircraft);
	cargo = aircraft->itemcargo;

	/* Find components definition. */
	comp = CL_GetComponentsByItem(INVSH_GetItemByID(ufocraft->id));
	assert(comp);

	/* Add components of crashed UFO to dropship. */
	for (i = 0; i < comp->numItemtypes; i++) {
		objDef_t *compOd = comp->items[i];
		assert(compOd);
		Com_DPrintf(DEBUG_CLIENT, "CP_UFOCrashed_f: Collected %i of %s\n", comp->item_amount2[i], comp->items[i]->id);
		/* Add items to cargo, increase itemtypes. */
		cargo[aircraft->itemtypes].item = compOd;
		cargo[aircraft->itemtypes].amount = comp->item_amount2[i];
		aircraft->itemtypes++;
	}
	/* Put relevant info into missionresults array. */
	missionresults.recovery = qtrue;
	missionresults.crashsite = qtrue;
	missionresults.ufotype = ufocraft->ufotype;

	/* send mail */
	UR_SendMail(ufocraft, aircraft->homebase);
}

/*==================================
Backend functions
==================================*/

/**
 * @brief Updates current capacities for UFO hangars in given base.
 * @param[in] base The base where you want the update to be done
 */
void UR_UpdateUFOHangarCapForAll (base_t *base)
{
	int i;
	aircraft_t *ufocraft;

	if (!base) {
#ifdef DEBUG
		Com_Printf("UR_UpdateUFOHangarCapForAll: no base given!\n");
#endif
		return;
	}

	/* Reset current capacities for UFO hangar. */
	base->capacities[CAP_UFOHANGARS_LARGE].cur = 0;
	base->capacities[CAP_UFOHANGARS_SMALL].cur = 0;

	for (i = 0; i < csi.numODs; i++) {
		/* we are looking for UFO */
		if (csi.ods[i].tech->type != RS_CRAFT)
			continue;

		/* do we have UFO of this type ? */
		if (!base->storage.num[i])
			continue;

		/* look for corresponding aircraft in global array */
		ufocraft = AIR_GetAircraft(csi.ods[i].id);
		if (!ufocraft) {
			Com_Printf("UR_UpdateUFOHangarCapForAll: Did not find UFO %s\n", csi.ods[i].id);
			continue;
		}

		/* Update base capacity. */
		if (ufocraft->size == AIRCRAFT_LARGE)
			base->capacities[CAP_UFOHANGARS_LARGE].cur += base->storage.num[i];
		else
			base->capacities[CAP_UFOHANGARS_SMALL].cur += base->storage.num[i];
	}

	Com_DPrintf(DEBUG_CLIENT, "UR_UpdateUFOHangarCapForAll: base capacities.cur: small: %i big: %i\n", base->capacities[CAP_UFOHANGARS_SMALL].cur, base->capacities[CAP_UFOHANGARS_LARGE].cur);
}

/**
 * @brief Function to process active recoveries.
 * @sa CL_CampaignRun
 * @sa UR_Prepare
 */
void UR_ProcessActive (void)
{
	int i;

	for (i = 0; i < MAX_RECOVERIES; i++) {
		ufoRecoveries_t *recovery = &ccs.recoveries[i];
		if (recovery->active && recovery->event.day == ccs.date.day) {
			objDef_t *od;
			const aircraft_t *ufocraft;
			base_t *base = recovery->base;
			if (!base->founded) {
				/* Destination base was destroyed meanwhile. */
				/* UFO is lost, send proper message to the user.*/
				recovery->active = qfalse;
				/** @todo prepare MS_AddNewMessage() here */
				return;
			}
			/* Get ufocraft. */
			ufocraft = recovery->ufoTemplate;
			assert(ufocraft);
			/* Get item. */
			/* We can do this because aircraft id is the same as dummy item id. */
			od = INVSH_GetItemByID(ufocraft->id);
			assert(od);

			/* Process UFO recovery. */
			/* don't call B_UpdateStorageAndCapacity here - it's a dummy item */
			base->storage.num[od->idx]++;	/* Add dummy UFO item to enable research topic. */
			RS_MarkCollected(od->tech);	/* Enable research topic. */
			/* Reset this recovery. */
			memset(&ccs.recoveries[i], 0, sizeof(ccs.recoveries[i]));
		}
	}
}

/**
 * @brief Checks conditions for storing given ufo in given base.
 * @param[in] base Pointer to the base, where we are going to store UFO.
 * @param[in] ufocraft Pointer to ufocraft which we are going to store.
 * @return qtrue if given base can store given ufo.
 */
qboolean UR_ConditionsForStoring (const base_t *base, const aircraft_t *ufocraft)
{
	assert(base && ufocraft);

	if (ufocraft->size == AIRCRAFT_LARGE) {
		/* Large UFOs can only fit large UFO hangars */
		if (!B_GetBuildingStatus(base, B_UFO_HANGAR))
			return qfalse;

		/* Check there is still enough room for this UFO */
		if (base->capacities[CAP_UFOHANGARS_LARGE].max <= base->capacities[CAP_UFOHANGARS_LARGE].cur)
			return qfalse;
	} else {
		/* This is a small UFO, can only fit in small hangar */

		/* There's no small hangar functional */
		if (!B_GetBuildingStatus(base, B_UFO_SMALL_HANGAR))
			return qfalse;

		/* Check there is still enough room for this UFO */
		if (base->capacities[CAP_UFOHANGARS_SMALL].max <= base->capacities[CAP_UFOHANGARS_SMALL].cur)
			return qfalse;
	}

	return qtrue;
}

void UR_InitStartup (void)
{
	Cmd_AddCommand("cp_uforecovery", CP_UFORecovered_f, "Function to trigger UFO Recovered event");
	Cmd_AddCommand("cp_uforecovery_baselist_click", CP_UFORecoveryBaseSelectPopup_f, "Callback for recovery base list popup.");
	Cmd_AddCommand("cp_uforecoverystart", CP_UFORecoveredStart_f, "Function to start UFO recovery processing.");
	Cmd_AddCommand("cp_uforecoverystore", CP_UFORecoveredStore_f, "Function to store recovered UFO in desired base.");
	Cmd_AddCommand("cp_uforecovery_nationlist_click", CP_UFORecoveryNationSelectPopup_f, "Callback for recovery nation list popup.");
	Cmd_AddCommand("cp_ufosellstart", CP_UFOSellStart_f, "Function to start UFO selling processing.");
	Cmd_AddCommand("cp_uforecoverysell", CP_UFORecoveredSell_f, "Function to sell recovered UFO to desired nation.");
	Cmd_AddCommand("cp_ufocrashed", CP_UFOCrashed_f, "Function to process crashed UFO after a mission.");
}
