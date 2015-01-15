/**
 * @file
 * @brief UFO recovery and storing callback functions
 * @note UFO recovery functions with UR_*
 * @note UFO storing functions with US_*
 */

/*
Copyright (C) 2002-2015 UFO: Alien Invasion.

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
#include "../../ui/ui_dataids.h"
#include "cp_campaign.h"
#include "cp_ufo.h"
#include "cp_uforecovery.h"
#include "cp_uforecovery_callbacks.h"
#include "cp_geoscape.h"
#include "cp_time.h"

/**
 * @brief Entries on Sell UFO dialog
 */
typedef struct ufoRecoveryNation_s {
	const nation_t* nation;
	int price;										/**< price proposed by nation. */
} ufoRecoveryNation_t;

/**
 * @brief Pointer to compare function
 * @note This function is used by sorting algorithm.
 */
typedef int (*COMP_FUNCTION)(ufoRecoveryNation_t* a, ufoRecoveryNation_t* b);

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
	const aircraft_t* ufoTemplate;					/**< the ufo type of the current ufo recovery */
	const nation_t* nation;							/**< selected nation to sell to for current ufo recovery */
	bool recoveryDone;							/**< recoveryDone? Then the buttons are disabled */
	float condition;								/**< How much the UFO is damaged - used for disassembies */
	ufoRecoveryNation_t ufoNations[MAX_NATIONS];	/**< Sorted array of nations. */
	ufoRecoveryNationOrder_t sortedColumn;			/**< Column sell sorted by */
	bool sortDescending;						/**< ascending (false) / descending (true) */
} ufoRecovery_t;

static ufoRecovery_t ufoRecovery;

/**
 * @brief Updates UFO recovery process.
 */
static void UR_DialogRecoveryDone (void)
{
	ufoRecovery.recoveryDone = true;
}

/**
 * @brief Function to trigger UFO Recovered event.
 * @note This function prepares related cvars for the recovery dialog.
 * @note Command to call this: cp_uforecovery_init.
 */
static void UR_DialogInit_f (void)
{
	char ufoID[MAX_VAR];
	const aircraft_t* ufoCraft;
	float cond = 1.0f;

	if (cgi->Cmd_Argc() < 2) {
		Com_Printf("Usage: %s <ufoID> [UFO-Condition]\n", cgi->Cmd_Argv(0));
		return;
	}

	Q_strncpyz(ufoID, cgi->Cmd_Argv(1), sizeof(ufoID));

	if (cgi->Cmd_Argc() >= 3)
		cond = atof(cgi->Cmd_Argv(2));

	ufoCraft = AIR_GetAircraft(ufoID);

	/* Fill ufoRecovery structure */
	OBJZERO(ufoRecovery);
	ufoRecovery.ufoTemplate = ufoCraft;
	ufoRecovery.condition = cond;
	ufoRecovery.sortedColumn = ORDER_NATION;

	if (ufoCraft) {
		if (cond < 1.0)
			cgi->Cvar_Set("mn_uforecovery_actualufo", _("\nSecured crashed %s (%.0f%%)\n"), UFO_GetName(ufoCraft), cond * 100);
		else
			cgi->Cvar_Set("mn_uforecovery_actualufo", _("\nSecured landed %s\n"), UFO_GetName(ufoCraft));

		cgi->UI_PushWindow("uforecovery");
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
	linkedList_t* recoveryYardName = nullptr;
	linkedList_t* recoveryYardCapacity = nullptr;
	static char cap[MAX_VAR];

	/* Do nothing if recovery process is finished. */
	if (ufoRecovery.recoveryDone)
		return;

	/* Check how many bases can store this UFO. */
	INS_Foreach(installation) {
		const capacities_t* capacity = &installation->ufoCapacity;
		if (capacity->max > 0 && capacity->max > capacity->cur) {
			Com_sprintf(cap, lengthof(cap), "%i/%i", (capacity->max - capacity->cur), capacity->max);
			cgi->LIST_AddString(&recoveryYardName, installation->name);
			cgi->LIST_AddString(&recoveryYardCapacity, cap);
			count++;
		}
	}

	if (count == 0) {
		/* No UFO base with proper conditions, show a hint and disable list. */
		cgi->LIST_AddString(&recoveryYardName, _("No free UFO yard available."));
		cgi->UI_ExecuteConfunc("uforecovery_tabselect sell");
		cgi->UI_ExecuteConfunc("btbasesel disable");
	} else {
		cgi->UI_ExecuteConfunc("cp_basesel_select 0");
		cgi->UI_ExecuteConfunc("btbasesel enable");
	}
	cgi->UI_RegisterLinkedListText(TEXT_UFORECOVERY_UFOYARDS, recoveryYardName);
	cgi->UI_RegisterLinkedListText(TEXT_UFORECOVERY_CAPACITIES, recoveryYardCapacity);
}

/**
 * @brief Function to start UFO recovery process.
 * @note Command to call this: cp_uforecovery_store_start.
 */
static void UR_DialogStartStore_f (void)
{
	installation_t* installation = nullptr;
	int idx;
	int count = 0;
	date_t date;

	if (cgi->Cmd_Argc() < 2) {
		Com_Printf("Usage: %s <installationIDX>\n", cgi->Cmd_Argv(0));
		return;
	}

	idx = atoi(cgi->Cmd_Argv(1));

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
			UFO_GetName(ufoRecovery.ufoTemplate), installation->name);
	MS_AddNewMessage(_("UFO Recovery"), cp_messageBuffer);
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
	linkedList_t* nationList = nullptr;

	for (int i = 0; i < ccs.numNations; i++) {
		const nation_t* nation = ufoRecovery.ufoNations[i].nation;
		if (!nation)
			continue;
		char row[512];
		Com_sprintf(row, lengthof(row), "%s\t\t\t%i\t\t%s", _(nation->name),
			ufoRecovery.ufoNations[i].price, NAT_GetHappinessString(nation));
		cgi->LIST_AddString(&nationList, row);
	}

	cgi->UI_RegisterLinkedListText(TEXT_UFORECOVERY_NATIONS, nationList);
}

/**
 * @brief Compare nations by nation name.
 * @param[in] a First item to compare
 * @param[in] b Second item to compare
 * @sa UR_SortNations
 */
static int UR_CompareByName (ufoRecoveryNation_t* a, ufoRecoveryNation_t* b)
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
static int UR_CompareByPrice (ufoRecoveryNation_t* a, ufoRecoveryNation_t* b)
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
static int UR_CompareByHappiness (ufoRecoveryNation_t* a, ufoRecoveryNation_t* b)
{
	const nationInfo_t* statsA = NAT_GetCurrentMonthInfo(a->nation);
	const nationInfo_t* statsB = NAT_GetCurrentMonthInfo(b->nation);

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
static void UR_SortNations (COMP_FUNCTION comp, bool order)
{
	for (int i = 0; i < ccs.numNations; i++) {
		bool swapped = false;
		for (int j = 0; j < ccs.numNations - 1; j++) {
			int value = (*comp)(&ufoRecovery.ufoNations[j], &ufoRecovery.ufoNations[j + 1]);
			ufoRecoveryNation_t tmp;

			if (order)
				value *= -1;
			if (value > 0) {
				/* swap nations */
				tmp = ufoRecovery.ufoNations[j];
				ufoRecovery.ufoNations[j] = ufoRecovery.ufoNations[j + 1];
				ufoRecovery.ufoNations[j + 1] = tmp;
				swapped = true;
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
		return nullptr;
	}
}

/**
 * @brief Function to initialize list to sell recovered UFO to desired nation.
 * @note Command to call this: cp_uforecovery_sell_init.
 */
static void UR_DialogInitSell_f (void)
{
	/* Do nothing if recovery process is finished. */
	if (ufoRecovery.recoveryDone)
		return;
	/* Do nothing without a ufoTemplate set */
	if (!ufoRecovery.ufoTemplate)
		return;

	for (int i = 0; i < ccs.numNations; i++) {
		const nation_t* nation = NAT_GetNationByIDX(i);
		const nationInfo_t* stats = NAT_GetCurrentMonthInfo(nation);
		int price;

		price = (int) (ufoRecovery.ufoTemplate->price * (.85f + frand() * .3f));
		/* Nation will pay less if corrupted */
		price = (int) (price * exp(-stats->xviInfection / 20.0f));

		ufoRecovery.ufoNations[i].nation = nation;
		ufoRecovery.ufoNations[i].price = price;
	}
	UR_SortNations(UR_GetSortFunctionByColumn(ufoRecovery.sortedColumn), ufoRecovery.sortDescending);
	UR_DialogFillNations();
	cgi->UI_ExecuteConfunc("btnatsel disable");
}

/**
 * @brief Returns the index of the selected nation in SellUFO list
 */
static int UR_DialogGetCurrentNationIndex (void)
{
	for (int i = 0; i < ccs.numNations; i++)
		if (ufoRecovery.ufoNations[i].nation == ufoRecovery.nation)
			return i;
	return -1;
}

/**
 * @brief Converts script id string to ordercolumn constant
 * @param[in] id Script id for order column
 * @sa ufoRecoveryNationOrder_t
 */
static ufoRecoveryNationOrder_t UR_GetOrderTypeByID (const char* id)
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
	if (cgi->Cmd_Argc() < 2) {
		Com_Printf("Usage: %s <nation|price|happiness>\n", cgi->Cmd_Argv(0));
		return;
	}

	const ufoRecoveryNationOrder_t column = UR_GetOrderTypeByID(cgi->Cmd_Argv(1));
	if (ufoRecovery.sortedColumn != column) {
		ufoRecovery.sortDescending = false;
		ufoRecovery.sortedColumn = column;
	} else {
		ufoRecovery.sortDescending = !ufoRecovery.sortDescending;
	}

	COMP_FUNCTION comp = UR_GetSortFunctionByColumn(column);
	if (!comp)
		return;

	UR_SortNations(comp, ufoRecovery.sortDescending);
	UR_DialogFillNations();

	/* changed line selection corresponding to current nation */
	const int index = UR_DialogGetCurrentNationIndex();
	if (index == -1)
		return;

	cgi->UI_ExecuteConfunc("cp_nationsel_select %d", index);
}

/**
 * @brief Finds the nation to which recovered UFO will be sold.
 * @note The nation selection is being done here.
 * @note Callback command: cp_uforecovery_nationlist_click.
 */
static void UR_DialogSelectSellNation_f (void)
{
	int num;
	const nation_t* nation;

	if (cgi->Cmd_Argc() < 2) {
		Com_Printf("Usage: %s <nationid>\n", cgi->Cmd_Argv(0));
		return;
	}

	num = atoi(cgi->Cmd_Argv(1));

	/* don't do anything if index is higher than visible nations */
	if (0 > num || num >= ccs.numNations)
		return;

	nation = ufoRecovery.ufoNations[num].nation;

	ufoRecovery.nation = nation;
	Com_DPrintf(DEBUG_CLIENT, "CP_UFORecoveryNationSelectPopup_f: picked nation: %s\n", nation->name);

	cgi->Cvar_Set("mission_recoverynation", "%s", _(nation->name));
	cgi->UI_ExecuteConfunc("btnatsel enable");
}

/**
 * @brief Function to start UFO selling process.
 * @note Command to call this: cp_uforecovery_sell_start.
 */
static void UR_DialogStartSell_f (void)
{
	if (!ufoRecovery.nation)
		return;

	const nation_t* nation = ufoRecovery.nation;
	int i = UR_DialogGetCurrentNationIndex();
	int price = ufoRecovery.ufoNations[i].price;

	assert(price >= 0);
#if 0
	if (ufoRecovery.selectedStorage) {
		Com_sprintf(cp_messageBuffer, sizeof(cp_messageBuffer), _("Sold previously recovered %s from %s to nation %s, gained %i credits."), UFO_TypeToName(
				ufoRecovery.selectedStorage->ufoTemplate->ufotype), ufoRecovery.selectedStorage->base->name, _(nation->name), price);
	} else
#endif
	{
		Com_sprintf(cp_messageBuffer, sizeof(cp_messageBuffer), _("Recovered %s from the battlefield. UFO sold to nation %s, gained %i credits."), UFO_GetName(ufoRecovery.ufoTemplate), _(nation->name), price);
	}
	MS_AddNewMessage(_("UFO Recovery"), cp_messageBuffer);
	CP_UpdateCredits(ccs.credits + price);

	/* update nation happiness */
	for (i = 0; i < ccs.numNations; i++) {
		nation_t* nat = NAT_GetNationByIDX(i);
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
const char* US_StoredUFOStatus (const storedUFO_t* ufo)
{
	assert(ufo);

	if (ufo->disassembly != nullptr)
		return "disassembling";

	switch (ufo->status) {
		case SUFO_STORED:
			return "stored";
		case SUFO_RECOVERED:
		case SUFO_TRANSFERED:
			return "transferring";
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
	const storedUFO_t* ufo;

	if (cgi->Cmd_Argc() < 2 || (ufo = US_GetStoredUFOByIDX(atoi(cgi->Cmd_Argv(1)))) == nullptr) {
		cgi->UI_ExecuteConfunc("show_storedufo -");
		return;
	}

	const char* ufoName = UFO_GetName(ufo->ufoTemplate);
	const char* status = US_StoredUFOStatus(ufo);
	const char* eta;

	if (Q_streq(status, "transferring")) {
		date_t time = Date_Substract(ufo->arrive, ccs.date);
		eta = CP_SecondConvert(Date_DateToSeconds(&time));
	} else {
		eta = "-";
	}

	cgi->UI_ExecuteConfunc("show_storedufo %d \"%s\" %3.0f \"%s\" \"%s\" \"%s\" \"%s\"", ufo->idx, ufoName, ufo->condition * 100, ufo->ufoTemplate->model, status, eta, ufo->installation->name);
}


/**
 * @brief Destroys a stored UFO
 * @note it's called by 'ui_destroystoredufo' command with a parameter of the stored UFO IDX and a confirmation value
 */
static void US_DestroySoredUFO_f (void)
{
	if (cgi->Cmd_Argc() < 2) {
		Com_DPrintf(DEBUG_CLIENT, "Usage: %s <idx> [0|1]\nWhere the second, optional parameter is the confirmation.\n", cgi->Cmd_Argv(0));
		return;
	}
	storedUFO_t* ufo = US_GetStoredUFOByIDX(atoi(cgi->Cmd_Argv(1)));
	if (!ufo) {
		Com_DPrintf(DEBUG_CLIENT, "Stored UFO with idx: %i does not exist\n", atoi(cgi->Cmd_Argv(1)));
		return;
	}

	/* Ask 'Are you sure?' by default */
	if (cgi->Cmd_Argc() < 3 || !atoi(cgi->Cmd_Argv(2))) {
		char command[128];

		Com_sprintf(command, sizeof(command), "ui_pop; ui_destroystoredufo %d 1; mn_installation_select %d;", ufo->idx, ufo->installation->idx);
		cgi->UI_PopupButton(_("Destroy stored UFO"), _("Do you really want to destroy this stored UFO?"),
			command, _("Destroy"), _("Destroy stored UFO"),
			"ui_pop;", _("Cancel"), _("Forget it"),
			nullptr, nullptr, nullptr);
		return;
	}
	US_RemoveStoredUFO(ufo);
	cgi->Cmd_ExecuteString("mn_installation_select %d", ufo->installation->idx);
}

/**
 * @brief Fills UFO Yard UI with transfer destinations
 */
static void US_FillUFOTransfer_f (void)
{
	if (cgi->Cmd_Argc() < 2) {
		Com_DPrintf(DEBUG_CLIENT, "Usage: %s <idx>\n", cgi->Cmd_Argv(0));
		return;
	}

	storedUFO_t* ufo = US_GetStoredUFOByIDX(atoi(cgi->Cmd_Argv(1)));
	if (!ufo) {
		Com_DPrintf(DEBUG_CLIENT, "Stored UFO with idx: %i does not exist\n", atoi(cgi->Cmd_Argv(1)));
		return;
	}

	cgi->UI_ExecuteConfunc("ufotransferlist_clear");
	INS_ForeachOfType(ins, INSTALLATION_UFOYARD) {
		if (ins == ufo->installation)
			continue;
		nation_t* nat = GEO_GetNation(ins->pos);
		const char* nationName = nat ? _(nat->name) : "";
		const int freeSpace = std::max(0, ins->ufoCapacity.max - ins->ufoCapacity.cur);
		cgi->UI_ExecuteConfunc("ufotransferlist_addyard %d \"%s\" \"%s\" %d %d", ins->idx, ins->name, nationName, freeSpace, ins->ufoCapacity.max);
	}
}

/**
 * @brief Send Stored UFOs of the destination UFO Yard
 */
static void US_FillUFOTransferUFOs_f (void)
{
	if (cgi->Cmd_Argc() < 2) {
		Com_DPrintf(DEBUG_CLIENT, "Usage: %s <idx>\n", cgi->Cmd_Argv(0));
		return;
	}

	installation_t* ins = INS_GetByIDX(atoi(cgi->Cmd_Argv(1)));
	if (!ins) {
		Com_DPrintf(DEBUG_CLIENT, "Installation with idx: %i does not exist\n", atoi(cgi->Cmd_Argv(1)));
		return;
	}

	cgi->UI_ExecuteConfunc("ufotransferlist_clearufos %d", ins->idx);
	US_Foreach(ufo) {
		if (ufo->installation != ins)
			continue;
		cgi->UI_ExecuteConfunc("ufotransferlist_addufos %d %d \"%s\"", ins->idx, ufo->idx, ufo->ufoTemplate->model);
	}
}

/**
 * @brief Callback to start the transfer of a stored UFO
 */
static void US_TransferUFO_f (void)
{
	storedUFO_t* ufo;
	installation_t* ins = nullptr;

	if (cgi->Cmd_Argc() < 3) {
		Com_Printf("Usage: %s <stored-ufo-idx>  <ufoyard-idx>\n", cgi->Cmd_Argv(0));
		return;
	}
	ufo = US_GetStoredUFOByIDX(atoi(cgi->Cmd_Argv(1)));
	if (ufo == nullptr) {
		Com_Printf("Stored ufo with idx %i not found.\n", atoi(cgi->Cmd_Argv(1)));
		return;
	}
	ins = INS_GetByIDX(atoi(cgi->Cmd_Argv(2)));
	if (!ins) {
		Com_Printf("Installation with idx: %i does not exist\n", atoi(cgi->Cmd_Argv(2)));
		return;
	}
	US_TransferUFO(ufo, ins);
}

static const cmdList_t ufoRecoveryCallbacks[] = {
	{"cp_uforecovery_init", UR_DialogInit_f, "Function to trigger UFO Recovered event"},
	{"cp_uforecovery_sell_init", UR_DialogInitSell_f, "Function to initialize sell recovered UFO to desired nation."},
	{"cp_uforecovery_store_init", UR_DialogInitStore_f, "Function to initialize store recovered UFO in desired base."},
	{"cp_uforecovery_nationlist_click", UR_DialogSelectSellNation_f, "Callback for recovery sell to nation list."},
	{"cp_uforecovery_store_start", UR_DialogStartStore_f, "Function to start UFO recovery processing."},
	{"cp_uforecovery_sell_start", UR_DialogStartSell_f, "Function to start UFO selling processing."},
	{"cp_uforecovery_sort", UR_DialogSortByColumn_f, "Sorts nations and update ui state."},
	{"ui_selectstoredufo", US_SelectStoredUfo_f, "Send Stored UFO data to the UI"},
	{"ui_destroystoredufo", US_DestroySoredUFO_f, "Destroy stored UFO"},
	{"ui_fill_ufotransfer", US_FillUFOTransfer_f, "Fills UFO Yard UI with transfer destinations"},
	{"ui_selecttransferyard", US_FillUFOTransferUFOs_f, "Send Stored UFOs of the destination UFO Yard"},
	{"ui_transferufo", US_TransferUFO_f, "Transfer stored UFO to another UFO Yard"},
	{nullptr, nullptr, nullptr}
};
void UR_InitCallbacks (void)
{
	cgi->Cmd_TableAddList(ufoRecoveryCallbacks);
}

void UR_ShutdownCallbacks (void)
{
	cgi->Cmd_TableRemoveList(ufoRecoveryCallbacks);
}
