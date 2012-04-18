/**
 * @file cp_market_callbacks.c
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
#include "../../cl_inventory.h"
#include "../../ui/ui_main.h"
#include "cp_campaign.h"
#include "cp_market.h"
#include "cp_market_callbacks.h"
#include "cp_popup.h"

#define MAX_BUYLIST		64

#define MAX_MARKET_MENU_ENTRIES 22

/**
 * @brief An entry in the buylist.
 * @note The pointers are used XOR - there can be only one (used).
 */
typedef struct buyListEntry_s {
	const objDef_t *item;			/**< Item pointer */
	const aircraft_t *aircraft;	/**< Used for aircraft production - aircraft template */
} buyListEntry_t;

typedef struct buyList_s {
	buyListEntry_t l[MAX_BUYLIST];	/** The actual list */
	int length;		/**< Amount of entries on the list. */
	int scroll;		/**< Scroll Position. Start of the buylist index - due to scrolling. */
} buyList_t;

static buyList_t buyList;	/**< Current buylist that is shown in the menu. */
static const objDef_t *currentSelectedMenuEntry;	/**< Current selected entry on the list. */
static int buyCat = FILTER_S_PRIMARY;	/**< Category of items in the menu.
										 * @sa itemFilterTypes_t */

/**
 * @brief Checks whether a given aircraft should appear on the market
 * @param aircraft The aircraft to check
 * @return @c true if the aircraft should appear on the market
 */
static qboolean BS_AircraftIsOnMarket (const aircraft_t *aircraft)
{
	return aircraft->type != AIRCRAFT_UFO && aircraft->price != -1;
}

/**
 * @brief Set the number of item to buy or sell.
 */
static inline int BS_GetBuySellFactor (void)
{
	return 1;
}

static const objDef_t *BS_GetObjectDefition (const buyListEntry_t *entry)
{
	assert(entry);
	if (entry->item)
		return entry->item;
	else if (entry->aircraft)
		return NULL;

	Com_Error(ERR_DROP, "You should not check an empty buy list entry");
}


/**
 * @brief Prints general information about aircraft for Buy/Sell menu.
 * @param[in] aircraftTemplate Aircraft type.
 * @sa UP_AircraftDescription
 * @sa UP_AircraftItemDescription
 */
static void BS_MarketAircraftDescription (const aircraft_t *aircraftTemplate)
{
	const technology_t *tech;

	/* Break if no aircraft was given or if  it's no sample-aircraft (i.e. template). */
	if (!aircraftTemplate || aircraftTemplate != aircraftTemplate->tpl)
		return;

	tech = aircraftTemplate->tech;
	assert(tech);
	UP_AircraftDescription(tech);
	Cvar_Set("mn_aircraftname", _(aircraftTemplate->name));
	Cvar_Set("mn_item", aircraftTemplate->id);
}


/**
 * Returns the storage amount of a
 * @param[in] base The base to count the aircraft type in
 * @param[in] aircraftID Aircraft script id
 * @return The storage amount for the given aircraft type in the given base
 */
static int BS_GetStorageAmountInBase (const base_t* base, const char *aircraftID)
{
	int storage = 0;

	assert(base);

	/* Get storage amount in the base. */
	AIR_ForeachFromBase(aircraft, base) {
		if (Q_streq(aircraft->id, aircraftID))
			storage++;
	}
	return storage;
}

static inline qboolean BS_GetMinMaxValueByItemID (const base_t *base, int itemNum, int *min, int *max, int *value)
{
	assert(base);

	if (itemNum < 0 || itemNum + buyList.scroll >= buyList.length)
		return qfalse;

	if (buyCat == FILTER_AIRCRAFT && buyList.l[itemNum + buyList.scroll].aircraft) {
		const aircraft_t *aircraft = buyList.l[itemNum + buyList.scroll].aircraft;
		if (!aircraft)
			return qfalse;
		*value = BS_GetStorageAmountInBase(base, aircraft->id);
		*max = BS_GetStorageAmountInBase(base, aircraft->id) + BS_GetAircraftOnMarket(aircraft);
		*min = 0;
	} else {
		const objDef_t *item = BS_GetObjectDefition(&buyList.l[itemNum + buyList.scroll]);
		if (!item)
			return qfalse;
		*value = B_ItemInBase(item, base);
		*max = B_ItemInBase(item, base) + ccs.eMarket.numItems[item->idx];
		*min = 0;
	}

	return qtrue;
}

/**
 * @brief Update the GUI by calling a console function
 * @sa BS_BuyType
 */
static void BS_UpdateItem (const base_t *base, int itemNum)
{
	int min, max, value;

	if (BS_GetMinMaxValueByItemID(base, itemNum, &min, &max, &value))
		UI_ExecuteConfunc("buy_updateitem %d %d %d %d", itemNum, value, min, max);
}

/**
 * @brief
 * @sa BS_MarketClick_f
 * @sa BS_AddToList
 */
static void BS_MarketScroll_f (void)
{
	int i;
	base_t *base = B_GetCurrentSelectedBase();

	if (!base || buyCat >= MAX_FILTERTYPES || buyCat < 0)
		return;

	if (Cmd_Argc() < 2) {
		Com_Printf("Usage: %s <scrollpos>\n", Cmd_Argv(0));
		return;
	}

	buyList.scroll = atoi(Cmd_Argv(1));
	assert(buyList.scroll >= 0);
	assert(!((buyList.length > MAX_MARKET_MENU_ENTRIES && buyList.scroll >= buyList.length - MAX_MARKET_MENU_ENTRIES)));

	/* now update the menu pics */
	for (i = 0; i < MAX_MARKET_MENU_ENTRIES; i++) {
		UI_ExecuteConfunc("buy_autoselli %i", i);
	}

	/* get item list */
	for (i = buyList.scroll; i < buyList.length - buyList.scroll; i++) {
		const objDef_t *od = BS_GetObjectDefition(&buyList.l[i]);

		if (i >= MAX_MARKET_MENU_ENTRIES)
			break;

		/* Check whether the item matches the proper filter, storage in current base and market. */
		if (od && (B_ItemInBase(od, base) > 0 || ccs.eMarket.numItems[od->idx]) && INV_ItemMatchesFilter(od, buyCat)) {
			const technology_t *tech = RS_GetTechForItem(od);

			UI_ExecuteConfunc("buy_show %i", i - buyList.scroll);
			BS_UpdateItem(base, i - buyList.scroll);

			/* autosell setting */
			if (!RS_IsResearched_ptr(tech))
				continue;
			if (ccs.eMarket.autosell[od->idx])
				UI_ExecuteConfunc("buy_autoselle %i", i - buyList.scroll);
			else
				UI_ExecuteConfunc("buy_autoselld %i", i - buyList.scroll);
		}
	}
}

/**
 * @brief Select one entry on the list.
 * @sa BS_MarketScroll_f
 * @sa BS_AddToList
 */
static void BS_MarketClick_f (void)
{
	int num;

	if (Cmd_Argc() < 2) {
		Com_Printf("Usage: %s <num>\n", Cmd_Argv(0));
		return;
	}

	num = atoi(Cmd_Argv(1));
	if (num >= buyList.length || num < 0)
		return;

	Cvar_Set("mn_item", "");

	switch (buyCat) {
	case FILTER_AIRCRAFT:
		assert(buyList.l[num].aircraft);
		BS_MarketAircraftDescription(buyList.l[num].aircraft->tpl);
		break;
	case FILTER_CRAFTITEM:
		UP_AircraftItemDescription(buyList.l[num].item);
		Cvar_Set("mn_aircraftname", "");
		break;
	case MAX_FILTERTYPES:
		break;
	default:
		if (buyList.l[num].item->craftitem.type != MAX_ACITEMS)
			UP_AircraftItemDescription(buyList.l[num].item);
		else
			INV_ItemDescription(buyList.l[num].item);
		currentSelectedMenuEntry = buyList.l[num].item;
		break;
	}

	/* update selected element */
	UI_ExecuteConfunc("buy_selectitem %i", num);
}

/**
 * @brief Opens the UFOpedia for the current selected item/aircraft.
 * @note called by market_openpedia
 */
static void BS_MarketInfoClick_f (void)
{
	const technology_t *tech = RS_GetTechByProvided(Cvar_GetString("mn_item"));

	if (tech)
		UP_OpenWith(tech->id);
}

/** @brief Market text nodes buffers */
static linkedList_t *bsMarketNames;
static linkedList_t *bsMarketStorage;
static linkedList_t *bsMarketMarket;
static linkedList_t *bsMarketPrices;

/**
 * @brief Appends a new entry to the market buffers
 * @sa BS_MarketScroll_f
 * @sa BS_MarketClick_f
 */
static void BS_AddToList (const char *name, int storage, int market, int price)
{
	LIST_AddString(&bsMarketNames, _(name));
	LIST_AddString(&bsMarketStorage, va("%i", storage));
	LIST_AddString(&bsMarketMarket, va("%i", market));
	LIST_AddString(&bsMarketPrices, va(_("%i c"), price));
}

/**
 * @brief Updates the Buy/Sell menu list.
 * @param[in] base Pointer to the base to buy/sell at
 * @sa BS_BuyType_f
 */
static void BS_BuyType (const base_t *base)
{
	const objDef_t *od;
	int i, j = 0;
	char tmpbuf[MAX_VAR];

	if (!base || buyCat >= MAX_FILTERTYPES || buyCat < 0)
		return;

	CP_UpdateCredits(ccs.credits);

	bsMarketNames = NULL;
	bsMarketStorage = NULL;
	bsMarketMarket = NULL;
	bsMarketPrices = NULL;
	UI_ResetData(TEXT_ITEMDESCRIPTION);

	/* hide autosell checkboxes by default */
	for (i = 0; i < MAX_MARKET_MENU_ENTRIES; i++) {
		UI_ExecuteConfunc("buy_autoselli %i", i);
	}

	switch (buyCat) {
	case FILTER_AIRCRAFT:	/* Aircraft */
		{
		const aircraft_t *aircraftTemplate;
		for (i = 0, j = 0, aircraftTemplate = ccs.aircraftTemplates; i < ccs.numAircraftTemplates; i++, aircraftTemplate++) {
			if (!BS_AircraftIsOnMarket(aircraftTemplate))
				continue;
			assert(aircraftTemplate->tech);
			if (BS_GetStorageAmountInBase(base, aircraftTemplate->id) + BS_GetAircraftOnMarket(aircraftTemplate) > 0) {
				if (j >= buyList.scroll && j < MAX_MARKET_MENU_ENTRIES) {
					UI_ExecuteConfunc("buy_show %i", j - buyList.scroll);
				}
				BS_AddToList(aircraftTemplate->name, BS_GetStorageAmountInBase(base, aircraftTemplate->id),
						BS_GetAircraftOnMarket(aircraftTemplate), BS_GetAircraftBuyingPrice(aircraftTemplate));
				if (j >= MAX_BUYLIST)
					Com_Error(ERR_DROP, "Increase the MAX_BUYLIST value to handle that much items\n");
				buyList.l[j].item = NULL;
				buyList.l[j].aircraft = aircraftTemplate;
				buyList.length = j + 1;
				BS_UpdateItem(base, j - buyList.scroll);
				j++;
			}
		}
		}
		break;
	case FILTER_CRAFTITEM:	/* Aircraft items */
		/* get item list */
		for (i = 0, j = 0, od = csi.ods; i < csi.numODs; i++, od++) {
			if (!BS_IsOnMarket(od))
				continue;
			/* Check whether the item matches the proper filter, storage in current base and market. */
			if ((B_ItemInBase(od, base) || ccs.eMarket.numItems[i])
			 && INV_ItemMatchesFilter(od, FILTER_CRAFTITEM)) {
				if (j >= buyList.scroll && j < MAX_MARKET_MENU_ENTRIES) {
					const technology_t *tech = RS_GetTechForItem(od);

					UI_ExecuteConfunc("buy_show %i", j - buyList.scroll);
					if (RS_IsResearched_ptr(tech)) {
						if (ccs.eMarket.autosell[i])
							UI_ExecuteConfunc("buy_autoselle %i", j - buyList.scroll);
						else
							UI_ExecuteConfunc("buy_autoselld %i", j - buyList.scroll);
					}
				}
				BS_AddToList(od->name, B_ItemInBase(od, base), ccs.eMarket.numItems[i], BS_GetItemBuyingPrice(od));
				if (j >= MAX_BUYLIST)
					Com_Error(ERR_DROP, "Increase the MAX_FILTERLIST value to handle that much items\n");
				buyList.l[j].item = od;
				buyList.l[j].aircraft = NULL;
				buyList.length = j + 1;
				BS_UpdateItem(base, j - buyList.scroll);
				j++;
			}
		}
		break;
	default:	/* Normal items */
		if (buyCat < MAX_SOLDIER_FILTERTYPES || buyCat == FILTER_DUMMY) {
			/* get item list */
			for (i = 0, j = 0, od = csi.ods; i < csi.numODs; i++, od++) {
				if (!BS_IsOnMarket(od))
					continue;
				/* Check whether the item matches the proper filter, storage in current base and market. */
				if ((B_ItemInBase(od, base) || ccs.eMarket.numItems[i]) && INV_ItemMatchesFilter(od, buyCat)) {
					BS_AddToList(od->name, B_ItemInBase(od, base), ccs.eMarket.numItems[i], BS_GetItemBuyingPrice(od));
					/* Set state of Autosell button. */
					if (j >= buyList.scroll && j < MAX_MARKET_MENU_ENTRIES) {
						const technology_t *tech = RS_GetTechForItem(od);

						UI_ExecuteConfunc("buy_show %i", j - buyList.scroll);
						if (RS_IsResearched_ptr(tech)) {
							if (ccs.eMarket.autosell[i])
								UI_ExecuteConfunc("buy_autoselle %i", j - buyList.scroll);
							else
								UI_ExecuteConfunc("buy_autoselld %i", j - buyList.scroll);
						}
					}

					if (j >= MAX_BUYLIST)
						Com_Error(ERR_DROP, "Increase the MAX_BUYLIST value to handle that much items\n");
					buyList.l[j].item = od;
					buyList.l[j].aircraft = NULL;
					buyList.length = j + 1;
					BS_UpdateItem(base, j - buyList.scroll);
					j++;
				}
			}
		}
		break;
	}

	for (; j < MAX_MARKET_MENU_ENTRIES; j++) {
		/* Hide the rest of the entries. */
		UI_ExecuteConfunc("buy_hide %i", j);
	}

	/* Update some menu cvars. */
	/* Set up base capacities. */
	Com_sprintf(tmpbuf, sizeof(tmpbuf), "%i/%i", CAP_GetCurrent(base, CAP_ITEMS),
		CAP_GetMax(base, CAP_ITEMS));
	Cvar_Set("mn_bs_storage", tmpbuf);

	/* select first item */
	if (buyList.length) {
		switch (buyCat) {	/** @sa BS_MarketClick_f */
		case FILTER_AIRCRAFT:
			BS_MarketAircraftDescription(buyList.l[0].aircraft);
			break;
		case FILTER_CRAFTITEM:
			Cvar_Set("mn_aircraftname", "");	/** @todo Use craftitem name here? See also BS_MarketClick_f */
			/* Select current item or first one. */
			if (currentSelectedMenuEntry)
				UP_AircraftItemDescription(currentSelectedMenuEntry);
			else
				UP_AircraftItemDescription(buyList.l[0].item);
			break;
		default:
			assert(buyCat != MAX_FILTERTYPES);
			/* Select current item or first one. */
			if (currentSelectedMenuEntry)
				INV_ItemDescription(currentSelectedMenuEntry);
			else
				INV_ItemDescription(buyList.l[0].item);
			break;
		}
	} else {
		/* reset description */
		INV_ItemDescription(NULL);
	}

	UI_RegisterLinkedListText(TEXT_MARKET_NAMES, bsMarketNames);
	UI_RegisterLinkedListText(TEXT_MARKET_STORAGE, bsMarketStorage);
	UI_RegisterLinkedListText(TEXT_MARKET_MARKET, bsMarketMarket);
	UI_RegisterLinkedListText(TEXT_MARKET_PRICES, bsMarketPrices);
}

/**
 * @brief Init function for Buy/Sell menu.
 */
static void BS_BuyType_f (void)
{
	base_t *base = B_GetCurrentSelectedBase();

	if (Cmd_Argc() == 2) {
		buyCat = INV_GetFilterTypeID(Cmd_Argv(1));

		if (buyCat == FILTER_DISASSEMBLY)
			buyCat--;
		if (buyCat < 0) {
			buyCat = MAX_FILTERTYPES - 1;
			if (buyCat == FILTER_DISASSEMBLY)
				buyCat--;
		} else if (buyCat >= MAX_FILTERTYPES) {
			buyCat = 0;
		}

		Cvar_Set("mn_itemtype", INV_GetFilterType(buyCat));
		currentSelectedMenuEntry = NULL;
	}

	BS_BuyType(base);
	buyList.scroll = 0;
	UI_ExecuteConfunc("sync_market_scroll 0 %d", buyList.scroll);
	UI_ExecuteConfunc("market_scroll %d", buyList.scroll);
	UI_ExecuteConfunc("market_click 0");
}

/**
 * @brief Buys aircraft or craftitem.
 * @sa BS_SellAircraft_f
 */
static void BS_BuyAircraft_f (void)
{
	int num;
	const aircraft_t *aircraftTemplate;
	base_t *base = B_GetCurrentSelectedBase();

	if (Cmd_Argc() < 2) {
		Com_Printf("Usage: %s <num>\n", Cmd_Argv(0));
		return;
	}

	if (!base)
		return;

	num = atoi(Cmd_Argv(1));
	if (num < 0 || num >= buyList.length)
		return;

	if (buyCat == FILTER_AIRCRAFT) {
		int freeSpace;
		if (!B_GetBuildingStatus(base, B_COMMAND)) {
			CP_Popup(_("Note"), _("No command centre in this base.\nHangars are not functional.\n"));
			return;
		}
		/* We cannot buy aircraft if there is no power in our base. */
		if (!B_GetBuildingStatus(base, B_POWER)) {
			CP_Popup(_("Note"), _("No power supplies in this base.\nHangars are not functional."));
			return;
		}
		/* We cannot buy aircraft without any hangar. */
		if (!AIR_AircraftAllowed(base)) {
			CP_Popup(_("Note"), _("Build a hangar first."));
			return;
		}
		aircraftTemplate = buyList.l[num].aircraft;
		freeSpace = AIR_CalculateHangarStorage(aircraftTemplate, base, 0);

		/* Check free space in hangars. */
		if (freeSpace < 0) {
			Com_Printf("BS_BuyAircraft_f: something bad happened, AIR_CalculateHangarStorage returned -1!\n");
			return;
		}

		if (freeSpace == 0) {
			CP_Popup(_("Notice"), _("You cannot buy this aircraft.\nNot enough space in hangars.\n"));
			return;
		} else {
			const int price = BS_GetAircraftBuyingPrice(aircraftTemplate);
			if (ccs.credits < price) {
				CP_Popup(_("Notice"), _("You cannot buy this aircraft.\nNot enough credits.\n"));
				return;
			} else {
				/* Hangar capacities are being updated in AIR_NewAircraft().*/
				BS_RemoveAircraftFromMarket(aircraftTemplate, 1);
				CP_UpdateCredits(ccs.credits - price);
				AIR_NewAircraft(base, aircraftTemplate);
				Cmd_ExecuteString(va("buy_type %s", INV_GetFilterType(FILTER_AIRCRAFT)));
			}
		}
	}
}

/**
 * @brief Sells aircraft or craftitem.
 * @sa BS_BuyAircraft_f
 */
static void BS_SellAircraft_f (void)
{
	int num;
	base_t *base = B_GetCurrentSelectedBase();

	if (Cmd_Argc() < 2) {
		Com_Printf("Usage: %s <num>\n", Cmd_Argv(0));
		return;
	}

	if (!base)
		return;

	num = atoi(Cmd_Argv(1));
	if (num < 0 || num >= buyList.length)
		return;

	if (buyCat == FILTER_AIRCRAFT) {
		const aircraft_t *aircraftTemplate = buyList.l[num].aircraft;
		qboolean aircraftOutNote = qfalse;
		qboolean teamNote = qfalse;
		aircraft_t *aircraft = NULL;

		if (!aircraftTemplate)
			return;

		AIR_ForeachFromBase(a, base) {
			if (Q_streq(a->id, aircraftTemplate->id)) {
				if (AIR_GetTeamSize(a) > 0) {
					teamNote = qtrue;
					continue;
				}
				if (!AIR_IsAircraftInBase(a)) {
					/* aircraft is not in base */
					aircraftOutNote = qtrue;
					continue;
				}
				aircraft = a;
				break;
			}
		}

		if (aircraft) {
			BS_SellAircraft(aircraft);

			/* reinit the menu */
			BS_BuyType(base);
		} else {
			if (teamNote)
				CP_Popup(_("Note"), _("You can't sell an aircraft if it still has a team assigned"));
			else if (aircraftOutNote)
				CP_Popup(_("Note"), _("You can't sell an aircraft that is not in base"));
			else
				Com_DPrintf(DEBUG_CLIENT, "BS_SellAircraft_f: There are no aircraft available (with no team assigned) for selling\n");
		}
	}
}

/**
 * @brief Buy one item of a given type.
 * @sa BS_SellItem_f
 * @sa BS_SellAircraft_f
 * @sa BS_BuyAircraft_f
 */
static void BS_BuyItem_f (void)
{
	int num;
	base_t *base = B_GetCurrentSelectedBase();

	if (Cmd_Argc() < 2) {
		Com_Printf("Usage: %s <num>\n", Cmd_Argv(0));
		return;
	}

	if (!base)
		return;

	if (buyCat == FILTER_AIRCRAFT) {
		Com_DPrintf(DEBUG_CLIENT, "BS_BuyItem_f: Redirects to BS_BuyAircraft_f\n");
		BS_BuyAircraft_f();
		return;
	}

	num = atoi(Cmd_Argv(1));
	if (num < 0 || num >= buyList.length)
		return;

	UI_ExecuteConfunc("buy_selectitem %i", num + buyList.scroll);

	{
		/* Normal item (or equipment for UGVs/Robots if buyCategory==BUY_HEAVY) */
		const objDef_t *item = BS_GetObjectDefition(&buyList.l[num + buyList.scroll]);
		assert(item);
		currentSelectedMenuEntry = item;
		INV_ItemDescription(item);
		Com_DPrintf(DEBUG_CLIENT, "BS_BuyItem_f: item %s\n", item->id);
		BS_CheckAndDoBuyItem(base, item, BS_GetBuySellFactor());
		/* reinit the menu */
		BS_BuyType(base);
		BS_UpdateItem(base, num);
	}
}

/**
 * @brief Sell one item of a given type.
 * @sa BS_BuyItem_f
 * @sa BS_SellAircraft_f
 * @sa BS_BuyAircraft_f
 */
static void BS_SellItem_f (void)
{
	int num;
	base_t *base = B_GetCurrentSelectedBase();

	if (Cmd_Argc() < 2) {
		Com_Printf("Usage: %s <num>\n", Cmd_Argv(0));
		return;
	}

	if (!base)
		return;

	if (buyCat == FILTER_AIRCRAFT) {
		Com_DPrintf(DEBUG_CLIENT, "BS_SellItem_f: Redirects to BS_SellAircraft_f\n");
		BS_SellAircraft_f();
		return;
	}

	num = atoi(Cmd_Argv(1));
	if (num < 0 || num >= buyList.length)
		return;

	UI_ExecuteConfunc("buy_selectitem %i", num + buyList.scroll);
	{
		const objDef_t *item = BS_GetObjectDefition(&buyList.l[num + buyList.scroll]);
		/* don't sell more items than we have */
		const int numItems = min(B_ItemInBase(item, base), BS_GetBuySellFactor());
		/* Normal item (or equipment for UGVs/Robots if buyCategory==BUY_HEAVY) */
		assert(item);
		currentSelectedMenuEntry = item;
		INV_ItemDescription(item);

		/* don't sell more items than we have */
		if (numItems) {
			/* reinit the menu */
			B_UpdateStorageAndCapacity(base, item, -numItems, qfalse);
			BS_AddItemToMarket(item, numItems);
			BS_BuyType(base);
			CP_UpdateCredits(ccs.credits + BS_GetItemSellingPrice(item) * numItems);
			BS_UpdateItem(base, num);
		}
	}
}

static void BS_BuySellItem_f (void)
{
	int value;

	if (Cmd_Argc() < 3) {
		/* num is used in the other callbacks to do the real buy or sell */
		Com_Printf("Usage: %s <num> <value>\n", Cmd_Argv(0));
		return;
	}

	value = atoi(Cmd_Argv(2));
	if (value == 0)
		return;

	if (value > 0) {
		BS_BuyItem_f();
	} else {
		BS_SellItem_f();
	}
}

/**
 * @brief Enable or disable autosell option for given itemtype.
 */
static void BS_Autosell_f (void)
{
	int num;
	const objDef_t *item;
	base_t *base = B_GetCurrentSelectedBase();

	/* Can be called from everywhere. */
	if (!base)
		return;

	if (Cmd_Argc() < 2) {
		Com_Printf("Usage: %s <num>\n", Cmd_Argv(0));
		return;
	}

	num = atoi(Cmd_Argv(1));
	Com_DPrintf(DEBUG_CLIENT, "BS_Autosell_f: listnumber %i\n", num);
	if (num < 0 || num >= buyList.length)
		return;

	item = BS_GetObjectDefition(&buyList.l[num + buyList.scroll]);
	assert(item);

	if (ccs.eMarket.autosell[item->idx]) {
		ccs.eMarket.autosell[item->idx] = qfalse;
		Com_DPrintf(DEBUG_CLIENT, "item name: %s, autosell false\n", item->name);
	} else {
		const technology_t *tech = RS_GetTechForItem(item);
		/* Don't allow to enable autosell for items not researched. */
		if (!RS_IsResearched_ptr(tech))
			return;
		ccs.eMarket.autosell[item->idx] = qtrue;
		Com_DPrintf(DEBUG_CLIENT, "item name: %s, autosell true\n", item->name);
	}

	/* Reinit the menu. */
	BS_BuyType(base);
}



void BS_InitCallbacks(void)
{
	Cmd_AddCommand("buy_type", BS_BuyType_f, NULL);
	Cmd_AddCommand("market_click", BS_MarketClick_f, "Click function for buy menu text node");
	Cmd_AddCommand("market_scroll", BS_MarketScroll_f, "Scroll function for buy menu");
	Cmd_AddCommand("mn_buysell", BS_BuySellItem_f, NULL);
	Cmd_AddCommand("mn_buy", BS_BuyItem_f, NULL);
	Cmd_AddCommand("mn_sell", BS_SellItem_f, NULL);
	Cmd_AddCommand("buy_autosell", BS_Autosell_f, "Enable or disable autosell option for given item.");
	Cmd_AddCommand("market_openpedia", BS_MarketInfoClick_f, "Open UFOPedia entry for selected item");

	OBJZERO(buyList);
	buyList.length = -1;
}

void BS_ShutdownCallbacks(void)
{
	Cmd_RemoveCommand("buy_type");
	Cmd_RemoveCommand("market_click");
	Cmd_RemoveCommand("market_scroll");
	Cmd_RemoveCommand("mn_buysell");
	Cmd_RemoveCommand("mn_buy");
	Cmd_RemoveCommand("mn_sell");
	Cmd_RemoveCommand("buy_autosell");
	Cmd_RemoveCommand("market_openpedia");
}
