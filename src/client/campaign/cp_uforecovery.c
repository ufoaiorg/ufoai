/**
 * @file cp_uforecovery.c
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
#include "cp_campaign.h"
#include "cp_ufo.h"
#include "cp_uforecovery.h"
#include "cp_uforecovery_callbacks.h"
#include "cp_aircraft.h"

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

	if (ccs.missionResults.crashsite) {
		/* take the source mail and create a copy of it */
		mail = CL_NewEventMail("ufo_crashed_report", va("ufo_crashed_report%i", ccs.date.sec), NULL);
		if (!mail)
			Com_Error(ERR_DROP, "UR_SendMail: ufo_crashed_report wasn't found");
		/* we need the source mail body here - this may not be NULL */
		if (!mail->body)
			Com_Error(ERR_DROP, "UR_SendMail: ufo_crashed_report has no mail body");

		/* Find components definition. */
		comp = CL_GetComponentsByItem(INVSH_GetItemByID(ufocraft->id));
		assert(comp);

		/* List all components of crashed UFO. */
		for (i = 0; i < comp->numItemtypes; i++) {
			compOd = comp->items[i];
			assert(compOd);
			if (comp->itemAmount2[i] > 0)
				Q_strcat(body, va(_("  * %i x\t%s\n"), comp->itemAmount2[i], compOd->name), sizeof(body));
		}
	} else {
		/* take the source mail and create a copy of it */
		mail = CL_NewEventMail("ufo_recovery_report", va("ufo_recovery_report%i", ccs.date.sec), NULL);
		if (!mail)
			Com_Error(ERR_DROP, "UR_SendMail: ufo_recovery_report wasn't found");
		/* we need the source mail body here - this may not be NULL */
		if (!mail->body)
			Com_Error(ERR_DROP, "UR_SendMail: ufo_recovery_report has no mail body");

		/* Find components definition. */
		comp = CL_GetComponentsByItem(INVSH_GetItemByID(ufocraft->id));
		assert(comp);

		/* List all components of crashed UFO. */
		for (i = 0; i < comp->numItemtypes; i++) {
			compOd = comp->items[i];
			assert(compOd);
			if (comp->itemAmount[i] > 0)
				Q_strcat(body, va(_("  * %s\n"), compOd->name), sizeof(body));
		}
	}
	assert(mail);

	/* don't free the old mail body here - it's the string of the source mail */
	mail->body = Mem_PoolStrDup(va(_(mail->body), UFO_TypeToName(ccs.missionResults.ufotype), base->name, body), cp_campaignPool, 0);

	/* update subject */
	/* Insert name of the mission in the template */
	mail->subject = Mem_PoolStrDup(va(_(mail->subject), base->name), cp_campaignPool, 0);

	/* Add the mail to unread mail */
	Cmd_ExecuteString(va("addeventmail %s", mail->id));
}

/**
 * @brief Prepares UFO recovery in global recoveries array.
 * @param[in] base Pointer to the base, where the UFO recovery will be made.
 * @sa UR_ProcessActive
 * @sa UR_ConditionsForStoring
 */
void UR_Prepare (base_t *base, ufoType_t ufoType)
{
	int i;
	ufoRecoveries_t *recovery = NULL;
	aircraft_t *ufocraft = NULL;
	date_t event;

	assert(base);

	/* Find ufo sample of given ufotype. */
	for (i = 0; i < ccs.numAircraftTemplates; i++) {
		ufocraft = &ccs.aircraftTemplates[i];
		if (ufocraft->type != AIRCRAFT_UFO)
			continue;
		if (ufocraft->ufotype == ufoType)
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
			Com_Error(ERR_DROP, "UR_Prepare: No room in large UFO hangars to store %s", ufocraft->id);
	} else {
		/* Small UFOs can only fit in small UFO hangars */
		if (base->capacities[CAP_UFOHANGARS_SMALL].max > base->capacities[CAP_UFOHANGARS_SMALL].cur)
			base->capacities[CAP_UFOHANGARS_SMALL].cur++;
		else
			/* no more room -- this shouldn't happen as we've used UR_ConditionsForStoring */
			Com_Error(ERR_DROP, "UR_Prepare: No room in small UFO hangars to store %s", ufocraft->id);
	}

	Com_DPrintf(DEBUG_CLIENT, "UR_Prepare: the recovery entry in global array is done; base: %s, ufotype: %s, date: %i\n",
		base->name, recovery->ufoTemplate->id, recovery->event.day);

	/* Send an email */
	UR_SendMail(ufocraft, base);
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

	if (!ccs.interceptAircraft)
		return;

	if (Cmd_Argc() < 2) {
		Com_Printf("Usage: %s <UFOType>\n", Cmd_Argv(0));
		return;
	}

	if ((atoi(Cmd_Argv(1)) >= 0) && (atoi(Cmd_Argv(1)) < UFO_MAX)) {
		UFOtype = atoi(Cmd_Argv(1));
	} else {
		UFOtype = Com_UFOShortNameToID(Cmd_Argv(1));
		if (UFOtype == UFO_MAX) {
			Com_Printf("CP_UFOCrashed_f: UFOType: %i does not exist!\n", atoi(Cmd_Argv(1)));
			return;
		}
	}

	/* Find ufo sample of given ufotype. */
	for (i = 0; i < ccs.numAircraftTemplates; i++) {
		ufocraft = &ccs.aircraftTemplates[i];
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
		Com_DPrintf(DEBUG_CLIENT, "CP_UFOCrashed_f: Collected %i of %s\n", comp->itemAmount2[i], comp->items[i]->id);
		/* Add items to cargo, increase itemtypes. */
		cargo[aircraft->itemtypes].item = compOd;
		cargo[aircraft->itemtypes].amount = comp->itemAmount2[i];
		aircraft->itemtypes++;
	}
	/* Put relevant info into missionResults array. */
	ccs.missionResults.recovery = qtrue;
	ccs.missionResults.crashsite = qtrue;
	ccs.missionResults.ufotype = ufocraft->ufotype;

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
	Cmd_AddCommand("cp_ufocrashed", CP_UFOCrashed_f, "Function to process crashed UFO after a mission.");
	UR_InitCallbacks();
}
