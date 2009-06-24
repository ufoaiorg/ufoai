/**
 * @file cp_uforecovery_callbacks.c
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
#include "cp_campaign.h"
#include "cp_ufo.h"
#include "cp_uforecovery.h"
#include "cp_uforecovery_callbacks.h"

#include "../cl_menu.h"
#include "../menu/node/m_node_text.h"

typedef struct ufoRecoveryStorage_s {
	base_t *base; /**< base where actual ufo is located */
	aircraft_t *ufoTemplate; /**< template of ufo for storage id and name */
	objDef_t *ufoItem; /**< object definition of aircraft, used for capacity check */
} ufoRecoveryStorage_t;

/** @sa ufoRecoveries_t */
typedef struct ufoRecovery_s {
	base_t *base;			/**< selected base for current selected ufo recovery */
	ufoType_t ufoType;		/**< the ufo type of the current ufo recovery */
	nation_t *nation;		/**< selected nation to sell to for current ufo recovery */
	qboolean recoveryDone;	/**< recoveryDone? Then the buttons are disabled */
	base_t *UFObases[MAX_BASES];	/**< Array of base indexes where we can store UFO. */
	int baseHasUFOHangarCount;		/**< number of entries in the UFObases array */
	int UFOprices[MAX_NATIONS];		/**< Array of prices proposed by nation. */
	ufoRecoveryStorage_t replaceUFOs[MAX_BASES * UFO_MAX]; /**< Array of replaceable ufos in all bases */
	ufoRecoveryStorage_t *selectedStorage; /**< selected storage for replace */
	int replaceCount; 		/**< number of replaceable ufos in all bases */
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
 * @brief Checks conditions for storing given ufo in given base.
 * @param[in] base Pointer to the base, where we are going to store UFO.
 * @param[in] ufocraft Pointer to ufocraft which we are going to store.
 * @return qtrue if given base can store given ufo.
 */
static int UR_GetBaseCapacityForUfotype (const base_t *base, const aircraft_t *ufocraft)
{
	assert(base && ufocraft);

	if (ufocraft->size == AIRCRAFT_LARGE) {
		/* Large UFOs can only fit large UFO hangars */
		if (!B_GetBuildingStatus(base, B_UFO_HANGAR))
			return -1;

		/* Check there is still enough room for this UFO */
		return base->capacities[CAP_UFOHANGARS_LARGE].max;
	} else {
		/* This is a small UFO, can only fit in small hangar */

		/* There's no small hangar functional */
		if (!B_GetBuildingStatus(base, B_UFO_SMALL_HANGAR))
			return -1;

		/* Check there is still enough room for this UFO */
		return base->capacities[CAP_UFOHANGARS_SMALL].max;
	}
}


/**
 * @brief Select appropriate the base or replace option from list
 * @param num selected index must be within valid range
 */
static void UR_DialogSelectStorageBase (const int num)
{
	qboolean useBase = qtrue;
	if (num < ufoRecovery.baseHasUFOHangarCount) {
		if (!ufoRecovery.UFObases[num])
			return;
	} else {
		useBase = qfalse;
		if (!ufoRecovery.replaceUFOs[num - ufoRecovery.baseHasUFOHangarCount].base)
			return;
	}

	if (useBase) {
		ufoRecovery.base = ufoRecovery.UFObases[num];
		Com_DPrintf(DEBUG_CLIENT, "CP_UFORecoveryBaseSelectPopup_f: picked base: %s\n", ufoRecovery.base->name);
		MN_ExecuteConfunc("btbasesel_enable");
	} else {
		ufoRecovery.selectedStorage = &ufoRecovery.replaceUFOs[num - ufoRecovery.baseHasUFOHangarCount];
		Com_DPrintf(DEBUG_CLIENT, "CP_UFORecoveryBaseSelectPopup_f: picked replace ufo %s from base %s\n",
				ufoRecovery.selectedStorage->ufoTemplate->id, ufoRecovery.selectedStorage->base->name);
		ufoRecovery.base = ufoRecovery.selectedStorage->base;
		MN_ExecuteConfunc("btbaserepl_enable");
	}
	Cvar_Set("mission_recoverybase", ufoRecovery.base->name);
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
		Com_Printf("CP_UFORecovered: UFOType: %i does not have valid craft definition!\n", atoi(Cmd_Argv(1)));
		return;
	}

	/* Put relevant info into missionresults array. */
	ccs.missionresults.recovery = qtrue;
	ccs.missionresults.crashsite = qfalse;
	ccs.missionresults.ufotype = ufocraft->ufotype;

	/* Prepare related cvars. */
	Cvar_SetValue("mission_uforecovered", 1);	/* This is used in menus to enable UFO Recovery nodes. */
	memset(&ufoRecovery, 0, sizeof(ufoRecovery));
	ufoRecovery.ufoType = UFOtype;

}

/**
 * @brief Function calculates prices for actual ufo to sell and initializes menu text.
 * @param ufocraft the ufo type to sell
 * @note called by UR_DialogInitSell_f and UR_DialogStartReplace
 */
static void UR_DialogInitSell (aircraft_t *ufocraft)
{
	int i, nations = 0;
	static char recoveryNationSelectPopup[MAX_SMALLMENUTEXTLEN];

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
		Com_sprintf(recoveryNationSelectPopup + strlen(recoveryNationSelectPopup), sizeof(recoveryNationSelectPopup),
				"%s\t\t\t%i\t\t%s\n", _(nation->name), ufoRecovery.UFOprices[i], NAT_GetHappinessString(nation));
	}

	/* Do nothing without at least one nation. */
	if (nations == 0)
		return;

	MN_RegisterText(TEXT_UFORECOVERY_NATIONS, recoveryNationSelectPopup);
}

/**
 * @brief Function to initialize list of storage locations for recovered UFO.
 * @note Command to call this: cp_uforecovery_store_init.
 * @sa UR_ConditionsForStoring
 */
static void UR_DialogInitStore_f (void)
{
	int i,j, replaceTemplates = 0;
	aircraft_t *ufocraft;
	static char recoveryBaseSelectPopup[MAX_SMALLMENUTEXTLEN];
	qboolean ufofound = qfalse;
	linkedList_t *validReplaceTemplateList = NULL, *replaceObjDefList = NULL;

	/* Do nothing without any base. */
	if (!ccs.numBases)
		return;

	/* Do nothing if recovery process is finished. */
	if (ufoRecovery.recoveryDone)
		return;

	/* Find ufo sample of given ufotype. */
	for (i = 0; i < ccs.numAircraftTemplates; i++) {
		ufocraft = &ccs.aircraftTemplates[i];
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

	/* find valid replace templates */
	for (i = 0; i < ccs.numAircraftTemplates; i++) {
		aircraft_t *tempcraft = &ccs.aircraftTemplates[i];
		if (tempcraft == ufocraft)
			continue;
		if (tempcraft->type != AIRCRAFT_UFO)
			continue;
		if (tempcraft->size == ufocraft->size) {
			LIST_AddPointer(&validReplaceTemplateList,tempcraft);
			LIST_AddPointer(&replaceObjDefList,INVSH_GetItemByID(tempcraft->id));
			replaceTemplates++;
		}
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
		} else {
			int baseCapacity = UR_GetBaseCapacityForUfotype(base, ufocraft);
			if (baseCapacity > 0)
			{
				linkedList_t *curTemplate = validReplaceTemplateList;
				linkedList_t *curObjID = replaceObjDefList;
				for (j = 0; j < replaceTemplates; j++) {
					objDef_t *obj = (objDef_t*)curObjID->data;
					aircraft_t *craft = (aircraft_t *)curTemplate->data;
					if (base->storage.num[obj->idx] > 0) {
						/* found a valid replace, add it */
						ufoRecoveryStorage_t *store = &ufoRecovery.replaceUFOs[ufoRecovery.replaceCount];
						store->base = base;
						store->ufoItem = obj;
						store->ufoTemplate = craft;
						ufoRecovery.replaceCount++;
					}
					curObjID = curObjID->next;
					curTemplate = curTemplate->next;
				}
			}
		}
	}

	for (i = 0; i < ufoRecovery.replaceCount; i++)
	{
		ufoRecoveryStorage_t store = ufoRecovery.replaceUFOs[i];
		Q_strcat(recoveryBaseSelectPopup, va(_("Replace %s at %s (%i stored available)\n"), _(store.ufoItem->name), store.base->name, store.base->storage.num[store.ufoItem->idx] ), sizeof(recoveryBaseSelectPopup));
	}

	/* If only one base with UFO hangars, the recovery will be done in this base. */
	switch (ufoRecovery.baseHasUFOHangarCount + ufoRecovery.replaceCount) {
	case 0:
		/* No UFO base with proper conditions, show a hint and disable list. */
		Q_strcat(recoveryBaseSelectPopup, _("No ufo hangar or ufo yard available."), sizeof(recoveryBaseSelectPopup));
		MN_ExecuteConfunc("cp_basesel_disable");
		break;
	case 1:
		/* there should only be one entry in UFObases - so use that one. */
		if (ufoRecovery.baseHasUFOHangarCount)
			ufoRecovery.base = ufoRecovery.UFObases[0];
		else
			ufoRecovery.selectedStorage = &ufoRecovery.replaceUFOs[0];
		UR_DialogSelectStorageBase(0);
		/** @todo some better way to do so without the need of a confunc? */
		MN_ExecuteConfunc("cp_basesel_select");
		break;
	default:
		/** @todo check whether this is needed */
		if (!ufoRecovery.base)
			ufoRecovery.base = ufoRecovery.UFObases[0];
		if (ufoRecovery.base)
			Cvar_Set("mission_recoverybase", ufoRecovery.base->name);
		break;
	}
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
	if (num < 0 || num >= ufoRecovery.replaceCount + ufoRecovery.baseHasUFOHangarCount)
		return;

	UR_DialogSelectStorageBase(num);
}

/**
 * @brief Function starts the UFO recovery process.
 * @note Called by UR_DialogStartStore_f and UR_DialogStartReplace_f
 * @note uforecovery base value must be set
 * @param finish flag indicating whether uforecovery should me marked as done
 */
static void UR_DialogStartStore (qboolean finish)
{
	base_t *base = ufoRecovery.base;
	Com_sprintf(cp_messageBuffer, sizeof(cp_messageBuffer), _("Recovered %s from the battlefield. UFO is being transported to base %s."), UFO_TypeToName(
			ufoRecovery.ufoType), base->name);
	MS_AddNewMessage(_("UFO Recovery"), cp_messageBuffer, qfalse, MSG_STANDARD, NULL);
	UR_Prepare(base, ufoRecovery.ufoType);

	if (finish)
		/* UFO recovery process is done, disable buttons. */
		UR_DialogRecoveryDone();
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

	UR_DialogStartStore(qtrue);
}

/**
 * @brief Function to start replace process
 * @note Command to call this: cp_uforecovery_replace
 * @note This needs the uforecovery selectedStorage value set
 */
static void UR_DialogStartReplace_f (void)
{
	static char replacetext[MAX_SMALLMENUTEXTLEN];
	ufoRecoveryStorage_t *storage = ufoRecovery.selectedStorage;
	if (!storage)
		return;
	/** @todo can these asserts be merged into one? */
	assert(storage->base);
	assert(storage->ufoItem);
	assert(storage->ufoTemplate);

	/* update hangar cap, remove old ufo*/
	storage->base->storage.num[storage->ufoItem->idx]--;
	if (storage->ufoTemplate->size == AIRCRAFT_LARGE) {
		if (storage->base->capacities[CAP_UFOHANGARS_LARGE].cur > 0)
			storage->base->capacities[CAP_UFOHANGARS_LARGE].cur--;
		else
			Com_Printf("UR_DialogStartReplace_f: Large UFO-hangar capacity is not greater than zero for replacing a UFO.");
	} else {
		if (storage->base->capacities[CAP_UFOHANGARS_SMALL].cur > 0)
			storage->base->capacities[CAP_UFOHANGARS_SMALL].cur--;
		else
			Com_Printf("UR_DialogStartReplace_f: Small UFO-hangar capacity is not greater than zero for replacing a UFO.");
	}
	/* reinit sell values for replaced ufo */
	memset(ufoRecovery.UFOprices,-1,sizeof(ufoRecovery.UFOprices));
	ufoRecovery.nation = NULL;
	UR_DialogInitSell(storage->ufoTemplate);
	/* update other menu texts */
	Cvar_Set("mn_uforecovery_actualufo",va(_("previously recovered %s from base %s"),UFO_TypeToName(storage->ufoTemplate->type), storage->base->name));
	Com_sprintf(replacetext, sizeof(replacetext), _("Storage process for captured %s ongoing."), UFO_TypeToName(ufoRecovery.ufoType));
	MN_RegisterText(TEXT_UFORECOVERY_BASESTORAGE,replacetext);
	/* start storage process for recovered ufo */
	UR_DialogStartStore(qfalse);
}

/**
 * @brief Function to initialize list to sell recovered UFO to desired nation.
 * @note Command to call this: cp_uforecovery_sell_init.
 */
static void UR_DialogInitSell_f (void)
{
	int i;
	aircraft_t *ufocraft;

	/* Do nothing if recovery process is finished. */
	if (ufoRecovery.recoveryDone)
		return;

	ufocraft = NULL;
	/* Find ufo sample of given ufotype. */
	for (i = 0; i < ccs.numAircraftTemplates; i++) {
		ufocraft = &ccs.aircraftTemplates[i];
		if (ufocraft->type != AIRCRAFT_UFO)
			continue;
		if (ufocraft->ufotype == ufoRecovery.ufoType)
			break;
	}
	if (!ufocraft)
		return;

	memset(ufoRecovery.UFOprices, -1, sizeof(ufoRecovery.UFOprices));

	UR_DialogInitSell(ufocraft);
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
	if (ufoRecovery.selectedStorage) {
		Com_sprintf(cp_messageBuffer, sizeof(cp_messageBuffer), _("Sold previously recovered %s from %s to nation %s, gained %i credits."), UFO_TypeToName(
				ufoRecovery.selectedStorage->ufoTemplate->ufotype), ufoRecovery.selectedStorage->base->name, _(nation->name), ufoRecovery.UFOprices[nation->idx]);
	} else {
		Com_sprintf(cp_messageBuffer, sizeof(cp_messageBuffer), _("Recovered %s from the battlefield. UFO sold to nation %s, gained %i credits."), UFO_TypeToName(
				ufoRecovery.ufoType), _(nation->name), ufoRecovery.UFOprices[nation->idx]);
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

void UR_InitCallbacks (void)
{
	Cmd_AddCommand("cp_uforecovery_init", UR_DialogInit_f, "Function to trigger UFO Recovered event");
	Cmd_AddCommand("cp_uforecovery_sell_init", UR_DialogInitSell_f, "Function to initialize sell recovered UFO to desired nation.");
	Cmd_AddCommand("cp_uforecovery_store_init", UR_DialogInitStore_f, "Function to initialize store recovered UFO in desired base.");
	Cmd_AddCommand("cp_uforecovery_nationlist_click", UR_DialogSelectSellNation_f, "Callback for recovery sell to nation list.");
	Cmd_AddCommand("cp_uforecovery_baselist_click", UR_DialogSelectStorageBase_f, "Callback for recovery store in base list.");
	Cmd_AddCommand("cp_uforecovery_store_start", UR_DialogStartStore_f, "Function to start UFO recovery processing.");
	Cmd_AddCommand("cp_uforecovery_sell_start", UR_DialogStartSell_f, "Function to start UFO selling processing.");
	Cmd_AddCommand("cp_uforecovery_replace", UR_DialogStartReplace_f, "Function to start replace UFO in base processing. Sell is needed for replaced UFO.");
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
	Cmd_RemoveCommand("cp_uforecovery_replace");
}
