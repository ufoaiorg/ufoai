/**
 * @file cp_uforecovery_callbacks.c
 * @brief UFO recovery and storing callback functions
 * @note UFO recovery functions with UR_*
 * @note UFO storing functions with US_*
 */

/*
Copyright (C) 2002-2012 UFO: Alien Invasion.

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

#include "../../cl_shared.h"
#include "cp_campaign.h"
#include "cp_ufo.h"
#include "cp_uforecovery.h"
#include "cp_uforecovery_callbacks.h"
#include "cp_map.h"
#include "cp_time.h"
#include "../../ui/ui_popup.h" /* UI_PopupButton */
#include "../../ui/ui_main.h"

/**
 * @brief Entries on Sell UFO dialog
 */
typedef struct ufoRecoveryNation_s {
	const nation_t *nation;
	int price;										/**< price proposed by nation. */
} ufoRecoveryNation_t;

/**
 * @brief Pointer to compare function
 * @note This function is used by sorting algorithm.
 */
typedef int (*COMP_FUNCTION)(ufoRecoveryNation_t *a, ufoRecoveryNation_t *b);

/**
 * @brief Constants for Sell UFO menu order
 */
typedef enum {
	ORDER_NATION,
	ORDER_PRICE,
	ORDER_HAPPINESS,

	MAX_ORDER
} ufoRecoveryNationOrder_t;

/** @sa ufoRecoveries_t */
typedef struct ufoRecovery_s {
	const aircraft_t *ufoTemplate;					/**< the ufo type of the current ufo recovery */
	const nation_t *nation;							/**< selected nation to sell to for current ufo recovery */
	qboolean recoveryDone;							/**< recoveryDone? Then the buttons are disabled */
	float condition;								/**< How much the UFO is damaged - used for disassembies */
	ufoRecoveryNation_t UFONations[MAX_NATIONS];	/**< Sorted array of nations. */
	ufoRecoveryNationOrder_t sortedColumn;			/**< Column sell sorted by */
	qboolean sortDescending;						/**< ascending (qfalse) / descending (qtrue) */
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
	float cond = 1.0f;

	if (Cmd_Argc() < 2) {
		Com_Printf("Usage: %s <ufoID> [UFO-Condition]\n", Cmd_Argv(0));
		return;
	}

	Q_strncpyz(ufoID, Cmd_Argv(1), sizeof(ufoID));

	if (Cmd_Argc() >= 3)
		cond = atof(Cmd_Argv(2));

	ufoCraft = AIR_GetAircraft(ufoID);

	/* Fill ufoRecovery structure */
	OBJZERO(ufoRecovery);
	ufoRecovery.ufoTemplate = ufoCraft;
	ufoRecovery.condition = cond;
	ufoRecovery.sortedColumn = ORDER_NATION;

	if (ufoCraft) {
		if (cond < 1.0)
			Cvar_Set("mn_uforecovery_actualufo", va(_("\nSecured crashed %s (%.0f%%)\n"), UFO_AircraftToIDOnGeoscape(ufoCraft), cond * 100));
		else
			Cvar_Set("mn_uforecovery_actualufo", va(_("\nSecured landed %s\n"), UFO_AircraftToIDOnGeoscape(ufoCraft)));

		UI_PushWindow("uforecovery", NULL, NULL);
	}
}

/**
 * @brief Function to initialize list of storage locations for recovered UFO.
 * @note Command to call this: cp_uforecovery_store_init.
 * @sa UR_ConditionsForStoring
 */
static void UR_DialogInitStore_f (void)
{
	int count = 0;
	linkedList_t *recoveryYardName = NULL;
	linkedList_t *recoveryYardCapacity = NULL;
	static char cap[MAX_VAR];

	/* Do nothing if recovery process is finished. */
	if (ufoRecovery.recoveryDone)
		return;

	/* Check how many bases can store this UFO. */
	INS_Foreach(installation) {
		const capacities_t *capacity;

		capacity = &installation->ufoCapacity;
		if (capacity->max > 0 && capacity->max > capacity->cur) {
			Com_sprintf(cap, lengthof(cap), "%i/%i", (capacity->max - capacity->cur), capacity->max);
			LIST_AddString(&recoveryYardName, installation->name);
			LIST_AddString(&recoveryYardCapacity, cap);
			count++;
		}
	}

	if (count == 0) {
		/* No UFO base with proper conditions, show a hint and disable list. */
		LIST_AddString(&recoveryYardName, _("No free UFO yard available."));
		UI_ExecuteConfunc("uforecovery_tabselect sell");
		UI_ExecuteConfunc("btbasesel disable");
	} else {
		UI_ExecuteConfunc("cp_basesel_select 0");
		UI_ExecuteConfunc("btbasesel enable");
	}
	UI_RegisterLinkedListText(TEXT_UFORECOVERY_UFOYARDS, recoveryYardName);
	UI_RegisterLinkedListText(TEXT_UFORECOVERY_CAPACITIES, recoveryYardCapacity);
}

/**
 * @brief Function to start UFO recovery process.
 * @note Command to call this: cp_uforecovery_store_start.
 */
static void UR_DialogStartStore_f (void)
{
	installation_t *installation = NULL;
	int idx;
	int count = 0;
	date_t date;

	if (Cmd_Argc() < 2) {
		Com_Printf("Usage: %s <installationIDX>\n", Cmd_Argv(0));
		return;
	}

	idx = atoi(Cmd_Argv(1));

	INS_Foreach(i) {
		if (i->ufoCapacity.max <= 0 || i->ufoCapacity.max <= i->ufoCapacity.cur)
			continue;

		if (count == idx) {
			installation = i;
			break;
		}
		count++;
	}

	if (!installation)
		return;

	Com_sprintf(cp_messageBuffer, lengthof(cp_messageBuffer), _("Recovered %s from the battlefield. UFO is being transported to %s."),
			UFO_AircraftToIDOnGeoscape(ufoRecovery.ufoTemplate), installation->name);
	MS_AddNewMessage(_("UFO Recovery"), cp_messageBuffer, qfalse, MSG_STANDARD, NULL);
	date = ccs.date;
	date.day += (int) RECOVERY_DELAY;

	US_StoreUFO(ufoRecovery.ufoTemplate, installation, date, ufoRecovery.condition);
	UR_DialogRecoveryDone();
}

/**
 * @brief Build the Sell UFO dialog's nationlist
 */
static void UR_DialogFillNations (void)
{
	int i;
	linkedList_t *nationList = NULL;

	for (i = 0; i < ccs.numNations; i++) {
		const nation_t *nation = ufoRecovery.UFONations[i].nation;
		if (nation) {
			char row[512];
			Com_sprintf(row, lengthof(row), "%s\t\t\t%i\t\t%s", _(nation->name),
				ufoRecovery.UFONations[i].price, NAT_GetHappinessString(nation));
			LIST_AddString(&nationList, row);
		}
	}

	UI_RegisterLinkedListText(TEXT_UFORECOVERY_NATIONS, nationList);
}

/**
 * @brief Compare nations by nation name.
 * @param[in] a First item to compare
 * @param[in] b Second item to compare
 * @sa UR_SortNations
 */
static int UR_CompareByName (ufoRecoveryNation_t *a, ufoRecoveryNation_t *b)
{
	return strcmp(_(a->nation->name), _(b->nation->name));
}

/**
 * @brief Compare nations by price.
 * @param[in] a First item to compare
 * @param[in] b Second item to compare
 * @return 1 if a > b
 * @return -1 if b > a
 * @return 0 if a == b
 * @sa UR_SortNations
 */
static int UR_CompareByPrice (ufoRecoveryNation_t *a, ufoRecoveryNation_t *b)
{
	if (a->price > b->price)
		return 1;
	if (a->price < b->price)
		return -1;
	return 0;
}

/**
 * @brief Compare nations by happiness.
 * @param[in] a First item to compare
 * @param[in] b Second item to compare
 * @return 1 if a > b
 * @return -1 if b > a
 * @return 0 if a == b
 * @sa UR_SortNations
 */
static int UR_CompareByHappiness (ufoRecoveryNation_t *a, ufoRecoveryNation_t *b)
{
	const nationInfo_t *statsA = NAT_GetCurrentMonthInfo(a->nation);
	const nationInfo_t *statsB = NAT_GetCurrentMonthInfo(b->nation);

	if (statsA->happiness > statsB->happiness)
		return 1;
	if (statsA->happiness < statsB->happiness)
		return -1;
	return 0;
}

/**
 * @brief Sort nations
 * @note uses Bubble sort algorithm
 * @param[in] comp Compare function
 * @param[in] order Ascending/Descending order
 * @sa UR_CompareByName
 * @sa UR_CompareByPrice
 * @sa UR_CompareByHappiness
 */
static void UR_SortNations (COMP_FUNCTION comp, qboolean order)
{
	int i;

	for (i = 0; i < ccs.numNations; i++) {
		qboolean swapped = qfalse;
		int j;

		for (j = 0; j < ccs.numNations - 1; j++) {
			int value = (*comp)(&ufoRecovery.UFONations[j], &ufoRecovery.UFONations[j + 1]);
			ufoRecoveryNation_t tmp;

			if (order)
				value *= -1;
			if (value > 0) {
				/* swap nations */
				tmp = ufoRecovery.UFONations[j];
				ufoRecovery.UFONations[j] = ufoRecovery.UFONations[j + 1];
				ufoRecovery.UFONations[j + 1] = tmp;
				swapped = qtrue;
			}
		}
		if (!swapped)
			break;
	}
}

/**
 * @brief Returns the sort function for a column
 * @param[in] column Column ordertype.
 */
static COMP_FUNCTION UR_GetSortFunctionByColumn (ufoRecoveryNationOrder_t column)
{
	switch (column) {
	case ORDER_NATION:
		return UR_CompareByName;
	case ORDER_PRICE:
		return UR_CompareByPrice;
	case ORDER_HAPPINESS:
		return UR_CompareByHappiness;
	default:
		Com_Printf("UR_DialogSortByColumn_f: Invalid sort option!\n");
		return NULL;
	}
}

/**
 * @brief Function to initialize list to sell recovered UFO to desired nation.
 * @note Command to call this: cp_uforecovery_sell_init.
 */
static void UR_DialogInitSell_f (void)
{
	int i;

	/* Do nothing if recovery process is finished. */
	if (ufoRecovery.recoveryDone)
		return;
	/* Do nothing without a ufoTemplate set */
	if (!ufoRecovery.ufoTemplate)
		return;

	for (i = 0; i < ccs.numNations; i++) {
		const nation_t *nation = NAT_GetNationByIDX(i);
		const nationInfo_t *stats = NAT_GetCurrentMonthInfo(nation);
		int price;

		price = (int) (ufoRecovery.ufoTemplate->price * (.85f + frand() * .3f));
		/* Nation will pay less if corrupted */
		price = (int) (price * exp(-stats->xviInfection / 20.0f));

		ufoRecovery.UFONations[i].nation = nation;
		ufoRecovery.UFONations[i].price = price;
	}
	UR_SortNations(UR_GetSortFunctionByColumn(ufoRecovery.sortedColumn), ufoRecovery.sortDescending);
	UR_DialogFillNations();
	UI_ExecuteConfunc("btnatsel disable");
}

/**
 * @brief Returns the index of the selected nation in SellUFO list
 */
static int UR_DialogGetCurrentNationIndex (void)
{
	int i;

	for (i = 0; i < ccs.numNations; i++)
		if (ufoRecovery.UFONations[i].nation == ufoRecovery.nation)
			return i;
	return -1;
}

/**
 * @brief Converts script id string to ordercolumn constant
 * @param[in] id Script id for order column
 * @sa ufoRecoveryNationOrder_t
 */
static ufoRecoveryNationOrder_t UR_GetOrderTypeByID (const char *id)
{
	if (!id)
		return MAX_ORDER;
	if (Q_streq(id, "nation"))
		return ORDER_NATION;
	if (Q_streq(id, "price"))
		return ORDER_PRICE;
	if (Q_streq(id, "happiness"))
		return ORDER_HAPPINESS;
	return MAX_ORDER;
}

/**
 * @brief Sort Sell UFO dialog
 */
static void UR_DialogSortByColumn_f (void)
{
	COMP_FUNCTION comp = 0;
	ufoRecoveryNationOrder_t column;

	if (Cmd_Argc() < 2) {
		Com_Printf("Usage: %s <nation|price|happiness>\n", Cmd_Argv(0));
		return;
	}

	column = UR_GetOrderTypeByID(Cmd_Argv(1));
	if (ufoRecovery.sortedColumn != column) {
		ufoRecovery.sortDescending = qfalse;
		ufoRecovery.sortedColumn = column;
	} else {
		ufoRecovery.sortDescending = !ufoRecovery.sortDescending;
	}

	comp = UR_GetSortFunctionByColumn(column);
	if (comp) {
		int index;

		UR_SortNations(comp, ufoRecovery.sortDescending);
		UR_DialogFillNations();

		/* changed line selection corresponding to current nation */
		index = UR_DialogGetCurrentNationIndex();
		if (index != -1)
			UI_ExecuteConfunc("cp_nationsel_select %d", index);
	}
}

/**
 * @brief Finds the nation to which recovered UFO will be sold.
 * @note The nation selection is being done here.
 * @note Callback command: cp_uforecovery_nationlist_click.
 */
static void UR_DialogSelectSellNation_f (void)
{
	int num;
	const nation_t *nation;

	if (Cmd_Argc() < 2) {
		Com_Printf("Usage: %s <nationid>\n", Cmd_Argv(0));
		return;
	}

	num = atoi(Cmd_Argv(1));

	/* don't do anything if index is higher than visible nations */
	if (0 > num || num >= ccs.numNations)
		return;

	nation = ufoRecovery.UFONations[num].nation;

	ufoRecovery.nation = nation;
	Com_DPrintf(DEBUG_CLIENT, "CP_UFORecoveryNationSelectPopup_f: picked nation: %s\n", nation->name);

	Cvar_Set("mission_recoverynation", _(nation->name));
	UI_ExecuteConfunc("btnatsel enable");
}

/**
 * @brief Function to start UFO selling process.
 * @note Command to call this: cp_uforecovery_sell_start.
 */
static void UR_DialogStartSell_f (void)
{
	int price = -1;
	const nation_t *nation;
	int i;

	if (!ufoRecovery.nation)
		return;

	nation = ufoRecovery.nation;

	i = UR_DialogGetCurrentNationIndex();
	price = ufoRecovery.UFONations[i].price;

	assert(price >= 0);
#if 0
	if (ufoRecovery.selectedStorage) {
		Com_sprintf(cp_messageBuffer, sizeof(cp_messageBuffer), _("Sold previously recovered %s from %s to nation %s, gained %i credits."), UFO_TypeToName(
				ufoRecovery.selectedStorage->ufoTemplate->ufotype), ufoRecovery.selectedStorage->base->name, _(nation->name), price);
	} else
#endif
	{
		Com_sprintf(cp_messageBuffer, sizeof(cp_messageBuffer), _("Recovered %s from the battlefield. UFO sold to nation %s, gained %i credits."), UFO_AircraftToIDOnGeoscape(ufoRecovery.ufoTemplate), _(nation->name), price);
	}
	MS_AddNewMessage(_("UFO Recovery"), cp_messageBuffer, qfalse, MSG_STANDARD, NULL);
	CP_UpdateCredits(ccs.credits + price);

	/* update nation happiness */
	for (i = 0; i < ccs.numNations; i++) {
		nation_t *nat = NAT_GetNationByIDX(i);
		float ufoHappiness;

		assert(nat);
		if (nat == nation)
			/* nation is happy because it got the UFO */
			ufoHappiness = HAPPINESS_UFO_SALE_GAIN;
		else
			/* nation is unhappy because it wanted the UFO */
			ufoHappiness = HAPPINESS_UFO_SALE_LOSS;

		NAT_SetHappiness(ccs.curCampaign->minhappiness, nat, nat->stats[0].happiness + ufoHappiness);
	}

	/* UFO recovery process is done, disable buttons. */
	UR_DialogRecoveryDone();
}

/**
 * @brief Returns string representation of the stored UFO's status
 * @note uses stored ufo status and disassembly
 */
const char *US_StoredUFOStatus (const storedUFO_t *ufo)
{
	assert(ufo);

	if (ufo->disassembly != NULL)
		return "disassembling";

	switch (ufo->status) {
		case SUFO_STORED:
			return "stored";
		case SUFO_RECOVERED:
		case SUFO_TRANSFERED:
			return "transfering";
		default:
			return "unknown";
	}
}

/**
 * @brief Send Stored UFO data to the UI
 * @note it's called by 'ui_selectstoredufo' command with a parameter of the stored UFO IDX
 */
static void US_SelectStoredUfo_f (void)
{
	const storedUFO_t *ufo;

	if (Cmd_Argc() < 2 || (ufo = US_GetStoredUFOByIDX(atoi(Cmd_Argv(1)))) == NULL) {
		UI_ExecuteConfunc("show_storedufo -");
	} else {
		const char *ufoName = UFO_AircraftToIDOnGeoscape(ufo->ufoTemplate);
		const char *status = US_StoredUFOStatus(ufo);
		const char *eta;

		if (Q_streq(status, "transfering")) {
			date_t time = Date_Substract(ufo->arrive, ccs.date);
			eta = CP_SecondConvert(Date_DateToSeconds(&time));
		} else {
			eta = "-";
		}

		UI_ExecuteConfunc("show_storedufo %d \"%s\" %3.0f \"%s\" \"%s\" \"%s\" \"%s\"", ufo->idx, ufoName, ufo->condition * 100, ufo->ufoTemplate->model, status, eta, ufo->installation->name);
	}
}


/**
 * @brief Destroys a stored UFO
 * @note it's called by 'ui_destroystoredufo' command with a parameter of the stored UFO IDX and a confirmation value
 */
static void US_DestroySoredUFO_f (void)
{
	storedUFO_t *ufo = NULL;

	if (Cmd_Argc() < 2) {
			Com_DPrintf(DEBUG_CLIENT, "Usage: %s <idx> [0|1]\nWhere the second, optional parameter is the confirmation.\n", Cmd_Argv(0));
			return;
	} else {
		ufo = US_GetStoredUFOByIDX(atoi(Cmd_Argv(1)));
		if (!ufo) {
			Com_DPrintf(DEBUG_CLIENT, "Stored UFO with idx: %i does not exist\n", atoi(Cmd_Argv(1)));
			return;
		}
	}

	/* Ask 'Are you sure?' by default */
	if (Cmd_Argc() < 3 || !atoi(Cmd_Argv(2))) {
		char command[128];

		Com_sprintf(command, sizeof(command), "ui_destroystoredufo %d 1;ui_pop; mn_installation_select %d;", ufo->idx, ufo->installation->idx);
		UI_PopupButton(_("Destroy stored UFO"), _("Do you really want to destroy this stored UFO?"),
			command, _("Destroy"), _("Destroy stored UFO"),
			"ui_pop;", _("Cancel"), _("Forget it"),
			NULL, NULL, NULL);
		return;
	}
	US_RemoveStoredUFO(ufo);
	Cmd_ExecuteString(va("mn_installation_select %d", ufo->installation->idx));
}

/**
 * @brief Fills UFO Yard UI with transfer destinations
 */
static void US_FillUFOTransfer_f (void)
{
	storedUFO_t *ufo = NULL;

	if (Cmd_Argc() < 2) {
			Com_DPrintf(DEBUG_CLIENT, "Usage: %s <idx>\n", Cmd_Argv(0));
			return;
	} else {
		ufo = US_GetStoredUFOByIDX(atoi(Cmd_Argv(1)));
		if (!ufo) {
			Com_DPrintf(DEBUG_CLIENT, "Stored UFO with idx: %i does not exist\n", atoi(Cmd_Argv(1)));
			return;
		}
	}

	UI_ExecuteConfunc("ufotransferlist_clear");
	INS_Foreach(ins) {
		nation_t *nat = MAP_GetNation(ins->pos);
		const char *nationName = nat ? _(nat->name) : "";
		const int freeSpace = max(0, ins->ufoCapacity.max - ins->ufoCapacity.cur);

		if (ins == ufo->installation)
			continue;
		if (INS_GetType(ins) != INSTALLATION_UFOYARD)
			continue;
		UI_ExecuteConfunc("ufotransferlist_addyard %d \"%s\" \"%s\" %d %d", ins->idx, ins->name, nationName, freeSpace, ins->ufoCapacity.max);
	}
}

/**
 * @brief Send Stored UFOs of the destination UFO Yard
 */
static void US_FillUFOTransferUFOs_f (void)
{
	installation_t *ins = NULL;

	if (Cmd_Argc() < 2) {
		Com_DPrintf(DEBUG_CLIENT, "Usage: %s <idx>\n", Cmd_Argv(0));
		return;
	} else {
		ins = INS_GetByIDX(atoi(Cmd_Argv(1)));
		if (!ins) {
			Com_DPrintf(DEBUG_CLIENT, "Installation with idx: %i does not exist\n", atoi(Cmd_Argv(1)));
			return;
		}
	}

	UI_ExecuteConfunc("ufotransferlist_clearufos %d", ins->idx);
	if (ins) {
		US_Foreach(ufo) {
			if (ufo->installation != ins)
				continue;
			UI_ExecuteConfunc("ufotransferlist_addufos %d %d \"%s\"", ins->idx, ufo->idx, ufo->ufoTemplate->model);
		}
	}
}

/**
 * @brief Callback to start the transfer of a stored UFO
 */
static void US_TransferUFO_f (void)
{
	storedUFO_t *ufo;
	installation_t *ins = NULL;

	if (Cmd_Argc() < 3) {
		Com_Printf("Usage: %s <stored-ufo-idx>  <ufoyard-idx>\n", Cmd_Argv(0));
		return;
	}
	ufo = US_GetStoredUFOByIDX(atoi(Cmd_Argv(1)));
	if (ufo == NULL) {
		Com_Printf("Stored ufo with idx %i not found.\n", atoi(Cmd_Argv(1)));
		return;
	}
	ins = INS_GetByIDX(atoi(Cmd_Argv(2)));
	if (!ins) {
		Com_Printf("Installation with idx: %i does not exist\n", atoi(Cmd_Argv(2)));
		return;
	}
	US_TransferUFO(ufo, ins);
}

void UR_InitCallbacks (void)
{
	Cmd_AddCommand("cp_uforecovery_init", UR_DialogInit_f, "Function to trigger UFO Recovered event");
	Cmd_AddCommand("cp_uforecovery_sell_init", UR_DialogInitSell_f, "Function to initialize sell recovered UFO to desired nation.");
	Cmd_AddCommand("cp_uforecovery_store_init", UR_DialogInitStore_f, "Function to initialize store recovered UFO in desired base.");
	Cmd_AddCommand("cp_uforecovery_nationlist_click", UR_DialogSelectSellNation_f, "Callback for recovery sell to nation list.");
	Cmd_AddCommand("cp_uforecovery_store_start", UR_DialogStartStore_f, "Function to start UFO recovery processing.");
	Cmd_AddCommand("cp_uforecovery_sell_start", UR_DialogStartSell_f, "Function to start UFO selling processing.");
	Cmd_AddCommand("cp_uforecovery_sort", UR_DialogSortByColumn_f, "Sorts nations and update ui state.");

	Cmd_AddCommand("ui_selectstoredufo", US_SelectStoredUfo_f, "Send Stored UFO data to the UI");
	Cmd_AddCommand("ui_destroystoredufo", US_DestroySoredUFO_f, "Destroy stored UFO");
	Cmd_AddCommand("ui_fill_ufotransfer", US_FillUFOTransfer_f, "Fills UFO Yard UI with transfer destinations");
	Cmd_AddCommand("ui_selecttransferyard", US_FillUFOTransferUFOs_f, "Send Stored UFOs of the destination UFO Yard");
	Cmd_AddCommand("ui_transferufo", US_TransferUFO_f, "Transfer stored UFO to another UFO Yard");
}

void UR_ShutdownCallbacks (void)
{
	Cmd_RemoveCommand("ui_transferufo");
	Cmd_RemoveCommand("ui_selecttransferyard");
	Cmd_RemoveCommand("ui_fill_ufotransfer");
	Cmd_RemoveCommand("ui_destroystoredufo");
	Cmd_RemoveCommand("ui_selectstoredufo");

	Cmd_RemoveCommand("cp_uforecovery_init");
	Cmd_RemoveCommand("cp_uforecovery_sell_init");
	Cmd_RemoveCommand("cp_uforecovery_store_init");
	Cmd_RemoveCommand("cp_uforecovery_nationlist_click");
	Cmd_RemoveCommand("cp_uforecovery_store_start");
	Cmd_RemoveCommand("cp_uforecovery_sell_start");
	Cmd_RemoveCommand("cp_uforecovery_sort");
}
