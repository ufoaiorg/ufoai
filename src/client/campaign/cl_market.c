/**
 * @file cl_market.c
 * @brief Single player market stuff.
 * @note Buy/Sell menu functions prefix: BS_
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
#include "../cl_global.h"
#include "../menu/m_popup.h"

#define MAX_BUYLIST		64

#define MAX_MARKET_MENU_ENTRIES 28

/**
 * @brief An entry in the buylist.
 * @note The pointers are used XOR - there can be only one (used).
 */
typedef struct buyListEntry_s {
	const objDef_t *item;			/**< Item pointer (see also csi.ods[] and base->storage.num[] etc...) */
	const ugv_t *ugv;				/**< Used for mixed UGV (characters) and FILTER_UGVITEM (items) list.
									 * If not NULL it's a pointer to the correct UGV-struct (duh)
									 * otherwise a FILTER_UGVITEM-item is set in "item". */
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

/** @brief Max amount of aircraft type calculated for the market. */
static const int MAX_AIRCRAFT_SUPPLY = 8;

/** @brief Max values for Buy/Sell factors. */
static const int MAX_BS_FACTORS = 500;

static const objDef_t *BS_GetObjectDefition (const buyListEntry_t *entry)
{
	assert(entry);
	if (entry->item)
		return entry->item;
	else if (entry->ugv)
		return NULL;
	else if (entry->aircraft)
		return NULL;

	Sys_Error("You should not check an empty buy list entry");
	return NULL;
}

/**
 * @brief Set the number of item to buy or sell.
 */
static int BS_GetBuySellFactor (void)
{
	const float NUM_CLICK_PARAMETER = 13.0f;		/**< The higher this value, the slowest buy/sell factor will change */
	const int numItems = exp(mn.mouseRepeat.numClick / NUM_CLICK_PARAMETER);
	return min(MAX_BS_FACTORS, numItems);
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

	/* Break if no airaft was given or if  it's no sample-aircraft (i.e. template). */
	if (!aircraftTemplate || aircraftTemplate != aircraftTemplate->tpl)
		return;

	tech = aircraftTemplate->tech;
	assert(tech);
	UP_AircraftDescription(tech);
	Cvar_Set("mn_aircraftname", _(aircraftTemplate->name));
	Cvar_Set("mn_item", aircraftTemplate->id);
}

/**
 * @brief
 * @param[in] base
 * @param[in] itemNum
 * @param[out] min
 * @param[out] max
 * @param[out] value
 */
static inline qboolean BS_GetMinMaxValueByItemID (const base_t *base, int itemNum, int *min, int *max, int *value)
{
	assert(base);

	if (itemNum < 0 || itemNum + buyList.scroll >= buyList.length)
		return qfalse;

	if (buyCat == FILTER_UGVITEM && buyList.l[itemNum + buyList.scroll].ugv) {
		/** @todo compute something better */
		*min = 0;
		*value = 10000;
		*max = 20000;
	} else {
		const objDef_t *item = BS_GetObjectDefition(&buyList.l[itemNum + buyList.scroll]);
		if (!item)
			return qfalse;
		*value = base->storage.num[item->idx];
		*max = base->storage.num[item->idx] + ccs.eMarket.num[item->idx];
		*min = 0;
	}

	return qtrue;
}

/**
 * @brief Update the GUI by calling a console function
 */
static void BS_UpdateItem (const base_t *base, int itemNum)
{
	int min, max, value;

	if (BS_GetMinMaxValueByItemID(base, itemNum, &min, &max, &value))
		MN_ExecuteConfunc("buy_updateitem %d %d %d %d", itemNum, value, min, max);
}

/**
 * @brief
 * @sa BS_MarketClick_f
 * @sa BS_AddToList
 */
static void BS_MarketScroll_f (void)
{
	menuNode_t* node;
	int i;

	if (!baseCurrent || buyCat == MAX_FILTERTYPES || buyCat < 0)
		return;

	node = MN_GetNodeFromCurrentMenu("market");
	if (!node)
		return;

	buyList.scroll = node->u.text.textScroll;

	if (buyList.length > MAX_MARKET_MENU_ENTRIES && buyList.scroll >= buyList.length - MAX_MARKET_MENU_ENTRIES) {
		buyList.scroll = buyList.length - MAX_MARKET_MENU_ENTRIES;
		node->u.text.textScroll = buyList.scroll;
	}

	/* the following nodes must exist */
	node = MN_GetNodeFromCurrentMenu("market_market");
	assert(node);
	node->u.text.textScroll = buyList.scroll;
	node = MN_GetNodeFromCurrentMenu("market_storage");
	assert(node);
	node->u.text.textScroll = buyList.scroll;
	node = MN_GetNodeFromCurrentMenu("market_prices");
	assert(node);
	node->u.text.textScroll = buyList.scroll;

	/* now update the menu pics */
	for (i = 0; i < MAX_MARKET_MENU_ENTRIES; i++) {
		MN_ExecuteConfunc("buy_autoselld%i", i);
	}

	assert(buyList.scroll >= 0);

	/* get item list */
	for (i = buyList.scroll; i < buyList.length - buyList.scroll; i++) {
		if (i >= MAX_MARKET_MENU_ENTRIES)
			break;
		else {
			const objDef_t *od = BS_GetObjectDefition(&buyList.l[i]);
			/* Check whether the item matches the proper filter, storage in current base and market. */
			if (od && INV_ItemMatchesFilter(od, buyCat) && (baseCurrent->storage.num[od->idx] || ccs.eMarket.num[od->idx])) {
				MN_ExecuteConfunc("buy_show%i", i - buyList.scroll);
				BS_UpdateItem(baseCurrent, i - buyList.scroll);
				if (gd.autosell[od->idx])
					MN_ExecuteConfunc("buy_autoselle%i", i - buyList.scroll);
			}
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
		Cvar_Set("mn_aircraftname", "");	/** @todo Use craftitem name here? */
		break;
	case FILTER_UGVITEM:
		if (buyList.l[num].ugv) {
			UP_UGVDescription(buyList.l[num].ugv);
			currentSelectedMenuEntry = NULL;
		} else {
			UP_ItemDescription(buyList.l[num].item);
			currentSelectedMenuEntry = buyList.l[num].item;
		}
		break;
	case MAX_FILTERTYPES:
		break;
	default:
		UP_ItemDescription(buyList.l[num].item);
		currentSelectedMenuEntry = buyList.l[num].item;
		break;
	}

	/* update selected element */
	MN_ExecuteConfunc("buy_selectitem %i", num);
}

/**
 * @brief Calculates amount of aircraft in base and on the market.
 * @param[in] base The base to get the storage amount from
 * @param[in] airCharId Aircraft id (type).
 * @param[in] inbase True if function has to return storage, false - when supply (market).
 * @return Amount of aircraft in base or amount of aircraft on the market.
 */
static int AIR_GetStorageSupply (const base_t *base, const char *airCharId, qboolean inbase)
{
	const aircraft_t *aircraft;
	int i, j;
	int amount = 0, storage = 0, supply;

	/* Get storage amount in baseCurrent. */
	for (j = 0, aircraft = base->aircraft; j < base->numAircraftInBase; j++, aircraft++) {
		if (!Q_strncmp(aircraft->id, airCharId, MAX_VAR))
			storage++;
	}
	/* Get supply amount (global). */
	for (j = 0; j < MAX_BASES; j++) {
		base = B_GetFoundedBaseByIDX(j);
		if (!base)
			continue;
		for (i = 0, aircraft = base->aircraft; i < base->numAircraftInBase; i++, aircraft++) {
			if (!Q_strncmp(aircraft->id, airCharId, MAX_VAR))
				amount++;
		}
	}
	if (amount < MAX_AIRCRAFT_SUPPLY)
		supply = MAX_AIRCRAFT_SUPPLY - amount;
	else
		supply = 0;

	if (inbase)
		return storage;
	else
		return supply;
}

/** @brief Market text nodes buffers */
static char bsMarketNames[1024];
static char bsMarketStorage[256];
static char bsMarketMarket[256];
static char bsMarketPrices[256];

/**
 * @brief Appends a new entry to the market buffers
 * @sa BS_MarketScroll_f
 * @sa BS_MarketClick_f
 */
static void BS_AddToList (const char *name, int storage, int market, int price)
{
	char shortString[36];

	/* MAX_MARKET_MENU_ENTRIES items in the list (of length 1024) */
	Q_strcat(bsMarketNames, _(name), sizeof(bsMarketNames));
	Q_strcat(bsMarketNames, "\n", sizeof(bsMarketNames));

	Com_sprintf(shortString, sizeof(shortString), "%i\n", storage);
	Q_strcat(bsMarketStorage, shortString, sizeof(bsMarketStorage));

	Com_sprintf(shortString, sizeof(shortString), "%i\n", market);
	Q_strcat(bsMarketMarket, shortString, sizeof(bsMarketMarket));

	Com_sprintf(shortString, sizeof(shortString), _("%i c\n"), price);
	Q_strcat(bsMarketPrices, shortString, sizeof(bsMarketPrices));
}

/**
 * @brief Updates the Buy/Sell menu list.
 * @sa BS_BuyType_f
 */
static void BS_BuyType (const base_t *base)
{
	const objDef_t *od;
	const aircraft_t *air_samp;
	int i, j = 0;
	char tmpbuf[MAX_VAR];

	if (!base || buyCat == MAX_FILTERTYPES || buyCat < 0)
		return;

	CL_UpdateCredits(ccs.credits);

	*bsMarketNames = *bsMarketStorage = *bsMarketMarket = *bsMarketPrices = '\0';
	MN_MenuTextReset(TEXT_STANDARD);
	mn.menuText[TEXT_MARKET_NAMES] = bsMarketNames;
	mn.menuText[TEXT_MARKET_STORAGE] = bsMarketStorage;
	mn.menuText[TEXT_MARKET_MARKET] = bsMarketMarket;
	mn.menuText[TEXT_MARKET_PRICES] = bsMarketPrices;

	/* 'normal' items */
	switch (buyCat) {
	case FILTER_AIRCRAFT:	/* Aircraft */
		{
		const technology_t* tech;
		for (i = 0, j = 0, air_samp = aircraftTemplates; i < numAircraftTemplates; i++, air_samp++) {
			if (air_samp->type == AIRCRAFT_UFO || air_samp->price == -1)
				continue;
			tech = air_samp->tech;
			assert(tech);
			if (RS_Collected_(tech) || RS_IsResearched_ptr(tech)) {
				if (j >= buyList.scroll && j < MAX_MARKET_MENU_ENTRIES) {
					MN_ExecuteConfunc("buy_show%i", j - buyList.scroll);
				}
				BS_AddToList(air_samp->name, AIR_GetStorageSupply(base, air_samp->id, qtrue),
						AIR_GetStorageSupply(base, air_samp->id, qfalse), air_samp->price);
				if (j >= MAX_BUYLIST)
					Sys_Error("Increase the MAX_BUYLIST value to handle that much items\n");
				buyList.l[j].item = NULL;
				buyList.l[j].ugv = NULL;
				buyList.l[j].aircraft = air_samp;
				BS_UpdateItem(base, j - buyList.scroll);
				j++;
			}
		}
		buyList.length = j;
		}
		break;
	case FILTER_CRAFTITEM:	/* Aircraft items */
		/* get item list */
		for (i = 0, j = 0, od = csi.ods; i < csi.numODs; i++, od++) {
			if (od->notOnMarket)
				continue;
			/* Check whether the item matches the proper filter, storage in current base and market. */
			if (od->tech && INV_ItemMatchesFilter(od, FILTER_CRAFTITEM)
			 && (RS_Collected_(od->tech) || RS_IsResearched_ptr(od->tech))
			 && (base->storage.num[i] || ccs.eMarket.num[i])) {
				if (j >= buyList.scroll && j < MAX_MARKET_MENU_ENTRIES) {
					MN_ExecuteConfunc("buy_show%i", j - buyList.scroll);
				}
				BS_AddToList(od->name, base->storage.num[i], ccs.eMarket.num[i], ccs.eMarket.ask[i]);
				if (j >= MAX_BUYLIST)
					Sys_Error("Increase the MAX_FILTERLIST value to handle that much items\n");
				buyList.l[j].item = od;
				buyList.l[j].ugv = NULL;
				buyList.l[j].aircraft = NULL;
				BS_UpdateItem(base, j - buyList.scroll);
				j++;
			}
		}
		buyList.length = j;
		break;
	case FILTER_UGVITEM:	/* Heavy equipment like UGVs and it's weapons/ammo. */
		{
		/* Get item list. */
		j = 0;
		for (i = 0; i < gd.numUGV; i++) {
			/**@todo Add this entry to the list */
			const technology_t* tech = RS_GetTechByProvided(gd.ugvs[i].id);
			assert(tech);
			if (RS_IsResearched_ptr(tech)) {
				const int hiredRobot = E_CountHiredRobotByType(base, &gd.ugvs[i]);
				const int unhiredRobot = E_CountUnhiredRobotsByType(&gd.ugvs[i]);

				if (hiredRobot + unhiredRobot <= 0)
					continue;

				if (j >= buyList.scroll && j < MAX_MARKET_MENU_ENTRIES) {
					MN_ExecuteConfunc("buy_show%i", j - buyList.scroll);
				}

				BS_AddToList(tech->name,
					hiredRobot,			/* numInStorage */
					unhiredRobot,			/* numOnMarket */
					gd.ugvs[i].price);

				if (j >= MAX_BUYLIST)
					Sys_Error("Increase the MAX_BUYLIST value to handle that much entries.\n");
				buyList.l[j].item = NULL;
				buyList.l[j].ugv = &gd.ugvs[i];
				buyList.l[j].aircraft = NULL;
				BS_UpdateItem(base, j - buyList.scroll);
				j++;
			}
		}

		for (i = 0, od = csi.ods; i < csi.numODs; i++, od++) {
			if (od->notOnMarket)
				continue;

			/* Check whether the item matches the proper filter, storage in current base and market. */
			if (od->tech && INV_ItemMatchesFilter(od, FILTER_UGVITEM) && (base->storage.num[i] || ccs.eMarket.num[i])) {
				BS_AddToList(od->name, base->storage.num[i], ccs.eMarket.num[i], ccs.eMarket.ask[i]);
				/* Set state of Autosell button. */
				if (j >= buyList.scroll && j < MAX_MARKET_MENU_ENTRIES) {
					MN_ExecuteConfunc("buy_show%i", j - buyList.scroll);
					if (gd.autosell[i])
						MN_ExecuteConfunc("buy_autoselle%i", j - buyList.scroll);
					else
						MN_ExecuteConfunc("buy_autoselld%i", j - buyList.scroll);
				}

				if (j >= MAX_BUYLIST)
					Sys_Error("Increase the MAX_BUYLIST value to handle that much items\n");
				buyList.l[j].item = od;
				buyList.l[j].ugv = NULL;
				buyList.l[j].aircraft = NULL;
				BS_UpdateItem(base, j - buyList.scroll);
				j++;
			}
		}
		buyList.length = j;
		}
		break;
	default:	/* Normal items */
		if (buyCat < MAX_SOLDIER_FILTERTYPES || buyCat == FILTER_DUMMY) {
			/* Add autosell button for every entry. */
			for (j = 0; j < MAX_MARKET_MENU_ENTRIES; j++)
				MN_ExecuteConfunc("buy_autoselld%i", j);
			/* get item list */
			for (i = 0, j = 0, od = csi.ods; i < csi.numODs; i++, od++) {
				if (od->notOnMarket)
					continue;
				/* Check whether the item matches the proper filter, storage in current base and market. */
				if (od->tech && INV_ItemMatchesFilter(od, buyCat) && (base->storage.num[i] || ccs.eMarket.num[i])) {
					BS_AddToList(od->name, base->storage.num[i], ccs.eMarket.num[i], ccs.eMarket.ask[i]);
					/* Set state of Autosell button. */
					if (j >= buyList.scroll && j < MAX_MARKET_MENU_ENTRIES) {
						MN_ExecuteConfunc("buy_show%i", j - buyList.scroll);
						if (gd.autosell[i])
							MN_ExecuteConfunc("buy_autoselle%i", j - buyList.scroll);
						else
							MN_ExecuteConfunc("buy_autoselld%i", j - buyList.scroll);
					}

					if (j >= MAX_BUYLIST)
						Sys_Error("Increase the MAX_BUYLIST value to handle that much items\n");
					buyList.l[j].item = od;
					buyList.l[j].ugv = NULL;
					buyList.l[j].aircraft = NULL;
					BS_UpdateItem(base, j - buyList.scroll);
					j++;
				}
			}
			buyList.length = j;
		}
		break;
	}

	for (; j < MAX_MARKET_MENU_ENTRIES; j++) {
		/* Hide the rest of the entries. */
		MN_ExecuteConfunc("buy_hide%i", j);
	}

	/* Update some menu cvars. */
	/* Set up base capacities. */
	Com_sprintf(tmpbuf, sizeof(tmpbuf), "%i/%i", base->capacities[CAP_ITEMS].cur,
		base->capacities[CAP_ITEMS].max);
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
			if (currentSelectedMenuEntry) {
				UP_AircraftItemDescription(currentSelectedMenuEntry);
			} else {
				UP_AircraftItemDescription(buyList.l[0].item);
			}
			break;
		case FILTER_UGVITEM:
			/** @todo select first heavy item */
			if (currentSelectedMenuEntry) {
				UP_ItemDescription(currentSelectedMenuEntry);
			} else if (buyList.l[0].ugv) {
				UP_UGVDescription(buyList.l[0].ugv);
			}
			break;
		default:
			assert(buyCat != MAX_FILTERTYPES);
			/* Select current item or first one. */
			if (currentSelectedMenuEntry)
				UP_ItemDescription(currentSelectedMenuEntry);
			else
				UP_ItemDescription(buyList.l[0].item);
			break;
		}
	} else {
		/* reset description */
		Cvar_Set("mn_itemname", "");
		Cvar_Set("mn_item", "");
		MN_MenuTextReset(TEXT_STANDARD);
	}
}

/*
 * @brief Function gives the user friendly name of a buyCategory
 */
char *BS_BuyTypeName(const int buyCat)
{
	switch (buyCat) {
	case FILTER_S_PRIMARY:
		return _("Primary weapons");
		break;
	case FILTER_S_SECONDARY:
		return _("Secondary weapons");
		break;
	case FILTER_S_HEAVY:
		return _("Heavy weapons");
		break;
	case FILTER_S_MISC:
		return _("Miscellaneous");
		break;
	case FILTER_S_ARMOUR:
		return _("Personal Armour");
		break;
	case FILTER_AIRCRAFT:
		return _("Aircraft");
		break;
	case FILTER_DUMMY:
		return _("Other");
		break;
	case FILTER_CRAFTITEM:
		return _("Aircraft equipment");
		break;
	case FILTER_UGVITEM:
		return _("Tanks");
		break;
	case FILTER_DISASSEMBLY:
		return _("Disassembling");
		break;
	default:
		return "Unknown";
		break;
	}
	return "Unknown";
}

/**
 * @brief Init function for Buy/Sell menu.
 */
static void BS_BuyType_f (void)
{
	if (Cmd_Argc() == 2) {
		menuNode_t* node = MN_GetNodeFromCurrentMenu("market");

		buyCat = atoi(Cmd_Argv(1));
		/** @todo
		buyCat = INV_GetFilterTypeID(Cmd_Argv(1));
		*/

		if (buyCat == FILTER_DISASSEMBLY)
			buyCat--;
		if (buyCat < 0) {
			buyCat = MAX_FILTERTYPES - 1;
			if (buyCat == FILTER_DISASSEMBLY)
				buyCat--;
		} else if (buyCat >= MAX_FILTERTYPES) {
			buyCat = 0;
		}

		Cvar_Set("mn_itemtype", va("%d", buyCat));	/**< @todo use a better identifier (i.e. filterTypeNames[]) for mn_itemtype @sa menu_buy.ufo */
		Cvar_Set("mn_itemtypename", _(BS_BuyTypeName(buyCat)));
		buyList.scroll = 0;
		if (node) {
			node->u.text.textScroll = 0;
			/* reset selection */
			MN_TextNodeSelectLine(node, 0);
		}
		currentSelectedMenuEntry = NULL;
	}

	BS_BuyType(baseCurrent);
	BS_MarketScroll_f();
}

/**
 * @brief Rolls buyCategory left in Buy/Sell menu.
 */
static void BS_Prev_BuyType_f (void)
{
	buyCat--;

	if (buyCat == MAX_SOLDIER_FILTERTYPES)
		buyCat--;

	if (buyCat < 0) {
		buyCat = MAX_FILTERTYPES - 1;
		if (buyCat == FILTER_DISASSEMBLY)
			buyCat--;
	} else if (buyCat >= MAX_FILTERTYPES) {
		buyCat = 0;
	}
	currentSelectedMenuEntry = NULL;
	Cbuf_AddText(va("buy_type %i\n", buyCat));
}

/**
 * @brief Rolls buyCategory right in Buy/Sell menu.
 */
static void BS_Next_BuyType_f (void)
{
	buyCat++;

	if (buyCat == MAX_SOLDIER_FILTERTYPES)
		buyCat++;
	if (buyCat == FILTER_DISASSEMBLY)
		buyCat++;

	if (buyCat < 0) {
		buyCat = MAX_FILTERTYPES - 1;
		if (buyCat == FILTER_DISASSEMBLY)
			buyCat--;
	} else if (buyCat >= MAX_FILTERTYPES) {
		buyCat = 0;
	}
	currentSelectedMenuEntry = NULL;
	Cbuf_AddText(va("buy_type %i\n", buyCat));
}

/**
 * @brief Buy items.
 * @param[in] base Pointer to the base where items are bought.
 * @param[in] item Pointer to the item to buy.
 * @param[in] number Number of items to buy.
 */
static qboolean BS_CheckAndDoBuyItem (base_t* base, const objDef_t *item, int number)
{
	int numItems;

	assert(base);
	assert(item);

	/* you can't buy more items than there are on market */
	numItems = min(number, ccs.eMarket.num[item->idx]);

	/* you can't buy more items than you have credits for */
	/** @todo Handle items with price 0 better */
	if (ccs.eMarket.ask[item->idx])
		numItems = min(numItems, ccs.credits / ccs.eMarket.ask[item->idx]);
	if (numItems <= 0)
		return qfalse;

	/* you can't buy more items than you have room for */
	/** @todo Handle items with size 0 better */
	if (item->size)
		numItems = min(numItems, (base->capacities[CAP_ITEMS].max - base->capacities[CAP_ITEMS].cur) / item->size);
	/* make sure that numItems is > 0 (can be negative because capacities.cur may be greater than
		capacities.max if storage is disabled or if alien items have been collected on mission */
	if (numItems <= 0) {
		MN_Popup(_("Not enough storage space"), _("You cannot buy this item.\nNot enough space in storage.\nBuild more storage facilities."));
		return qfalse;
	}

	B_UpdateStorageAndCapacity(base, item, numItems, qfalse, qfalse);
	ccs.eMarket.num[item->idx] -= numItems;
	/* reinit the menu */
	BS_BuyType(base);
	CL_UpdateCredits(ccs.credits - ccs.eMarket.ask[item->idx] * numItems);
	return qtrue;
}

/**
 * @brief Buys aircraft or craftitem.
 * @sa BS_SellAircraft_f
 */
static void BS_BuyAircraft_f (void)
{
	int num, freeSpace;
	const aircraft_t *aircraftTemplate;

	if (Cmd_Argc() < 2) {
		Com_Printf("Usage: %s <num>\n", Cmd_Argv(0));
		return;
	}

	if (!baseCurrent)
		return;

	num = atoi(Cmd_Argv(1));
	if (num < 0 || num >= buyList.length)
		return;

	if (buyCat == FILTER_AIRCRAFT) {
		/* We cannot buy aircraft if there is no power in our base. */
		if (!B_GetBuildingStatus(baseCurrent, B_POWER)) {
			MN_Popup(_("Note"), _("No power supplies in this base.\nHangars are not functional."));
			return;
		}
		/* We cannot buy aircraft without any hangar. */
		if (!B_GetBuildingStatus(baseCurrent, B_HANGAR) && !B_GetBuildingStatus(baseCurrent, B_SMALL_HANGAR)) {
			MN_Popup(_("Note"), _("Build a hangar first."));
			return;
		}
		aircraftTemplate = buyList.l[num].aircraft;
		freeSpace = AIR_CalculateHangarStorage(aircraftTemplate, baseCurrent, 0);

		/* Check free space in hangars. */
		if (freeSpace < 0) {
			Com_Printf("BS_BuyAircraft_f: something bad happened, AIR_CalculateHangarStorage returned -1!\n");
			return;
		}

		if (freeSpace == 0) {
			MN_Popup(_("Notice"), _("You cannot buy this aircraft.\nNot enough space in hangars.\n"));
			return;
		} else {
			assert(aircraftTemplate);
			if (ccs.credits < aircraftTemplate->price) {
				MN_Popup(_("Notice"), _("You cannot buy this aircraft.\nNot enough credits.\n"));
				return;
			} else {
				/* Hangar capacities are being updated in AIR_NewAircraft().*/
				CL_UpdateCredits(ccs.credits - aircraftTemplate->price);
				Cbuf_AddText(va("aircraft_new %s %i;buy_type %i;", aircraftTemplate->id, baseCurrent->idx, FILTER_AIRCRAFT));
			}
		}
	}
}

/**
 * @brief Update storage, the market, and the player's credits
 * @note Don't update capacity here because we can sell items directly from aircraft (already removed from storage).
 */
static void BS_ProcessCraftItemSale (const base_t *base, const objDef_t *craftitem, const int numItems)
{
	if (craftitem) {
		ccs.eMarket.num[craftitem->idx] += numItems;
		/* reinit the menu */
		BS_BuyType(base);
		CL_UpdateCredits(ccs.credits + (ccs.eMarket.bid[craftitem->idx] * numItems));
	}
}

/**
 * @brief Sells aircraft or craftitem.
 * @sa BS_BuyAircraft_f
 */
static void BS_SellAircraft_f (void)
{
	int num, j;
	qboolean found = qfalse;
	qboolean teamNote = qfalse;
	qboolean aircraftOutNote = qfalse;
	base_t *base;

	if (Cmd_Argc() < 2) {
		Com_Printf("Usage: %s <num>\n", Cmd_Argv(0));
		return;
	}

	if (!baseCurrent)
		return;

	num = atoi(Cmd_Argv(1));
	if (num < 0 || num >= buyList.length)
		return;

	base = baseCurrent;

	if (buyCat == FILTER_AIRCRAFT) {
		aircraft_t *aircraft;
		const aircraft_t *aircraftTemplate = buyList.l[num].aircraft;
		if (!aircraftTemplate)
			return;

		for (j = 0, aircraft = base->aircraft; j < base->numAircraftInBase; j++, aircraft++) {
			if (!Q_strncmp(aircraft->id, aircraftTemplate->id, MAX_VAR)) {
				if (aircraft->teamSize) {
					teamNote = qtrue;
					continue;
				}
				if (!AIR_IsAircraftInBase(aircraft)) {
					/* aircraft is not in base */
					aircraftOutNote = qtrue;
					continue;
				}
				found = qtrue;
				break;
			}
		}
		/* ok, we've found an empty aircraft (no team) in a base
		 * so now we can sell it */
		if (found) {
			/* sell off any items which are mounted on it */
			for (j = 0; j < aircraft->maxWeapons; j++) {
				BS_ProcessCraftItemSale(base, aircraft->weapons[j].item, 1);
				BS_ProcessCraftItemSale(base, aircraft->weapons[j].ammo, 1);
			}

			BS_ProcessCraftItemSale(base, aircraft->shield.item, 1);
			/* there should be no ammo here, but checking can't hurt */
			BS_ProcessCraftItemSale(base, aircraft->shield.ammo, 1);

			for (j = 0; j < aircraft->maxElectronics; j++) {
				BS_ProcessCraftItemSale(base, aircraft->electronics[j].item, 1);
				/* there should be no ammo here, but checking can't hurt */
				BS_ProcessCraftItemSale(base, aircraft->electronics[j].ammo, 1);
			}

			Com_DPrintf(DEBUG_CLIENT, "BS_SellAircraft_f: Selling aircraft with IDX %i\n", aircraft->idx);
			/* the capacities are also updated here */
			AIR_DeleteAircraft(base, aircraft);

			CL_UpdateCredits(ccs.credits + aircraftTemplate->price);
			/* reinit the menu */
			BS_BuyType(base);
		} else {
			if (teamNote)
				MN_Popup(_("Note"), _("You can't sell an aircraft if it still has a team assigned"));
			else if (aircraftOutNote)
				MN_Popup(_("Note"), _("You can't sell an aircraft that is not in base"));
			else
				Com_DPrintf(DEBUG_CLIENT, "BS_SellAircraft_f: There are no aircraft available (with no team assigned) for selling\n");
		}
	}
}

static void BS_BuySellItem_f (void)
{
	int num;
	float value;

	if (Cmd_Argc() < 3) {
		Com_Printf("Usage: %s <num> <value>\n", Cmd_Argv(0));
		return;
	}

	if (!baseCurrent)
		return;

	num = atoi(Cmd_Argv(1));
	value = atof(Cmd_Argv(2));
	if (value == 0)
		return;

	if (value > 0) {
		Cbuf_AddText(va("mn_buy %d\n", num));
	} else {
		Cbuf_AddText(va("mn_sell %d\n", num));
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

	if (Cmd_Argc() < 2) {
		Com_Printf("Usage: %s <num>\n", Cmd_Argv(0));
		return;
	}

	if (!baseCurrent)
		return;

	if (buyCat == FILTER_AIRCRAFT) {
		Com_DPrintf(DEBUG_CLIENT, "BS_BuyItem_f: Redirects to BS_BuyAircraft_f\n");
		BS_BuyAircraft_f();
		return;
	}

	num = atoi(Cmd_Argv(1));
	if (num < 0 || num >= buyList.length)
		return;

	Cbuf_AddText(va("market_click %i\n", num + buyList.scroll));

	if ((buyCat == FILTER_UGVITEM) && (buyList.l[num + buyList.scroll].ugv)) {
		/* The list entry is an actual ugv/robot */
		const ugv_t *ugv = buyList.l[num + buyList.scroll].ugv;
		qboolean ugvWeaponBuyable;

		UP_UGVDescription(ugv);

		if ((ccs.credits >= ugv->price) && (E_CountUnhiredRobotsByType(ugv) > 0)) {
			/* Check if we have a weapon for this ugv in the market and there is enough storage-room for it. */
			const objDef_t *ugvWeapon = INVSH_GetItemByID(ugv->weapon);
			if (!ugvWeapon)
				Sys_Error("BS_BuyItem_f: Could not get weapon '%s' for ugv/tank '%s'.", ugv->weapon, ugv->id);

			ugvWeaponBuyable = qtrue;
			if (!ccs.eMarket.num[ugvWeapon->idx])
				ugvWeaponBuyable = qfalse;

			if (baseCurrent->capacities[CAP_ITEMS].max - baseCurrent->capacities[CAP_ITEMS].cur <
				UGV_SIZE + ugvWeapon->size) {
				MN_Popup(_("Not enough storage space"), _("You cannot buy this item.\nNot enough space in storage.\nBuild more storage facilities."));
				ugvWeaponBuyable = qfalse;
			}

			if (ugvWeaponBuyable && E_HireRobot(baseCurrent, ugv)) {
				/* Move the item into the storage. */
				B_UpdateStorageAndCapacity(baseCurrent, ugvWeapon, 1, qfalse, qfalse);
				ccs.eMarket.num[ugvWeapon->idx]--;

				/* Update Display/List and credits. */
				BS_BuyType(baseCurrent);
				CL_UpdateCredits(ccs.credits - ugv->price);	/** @todo make this depend on market as well? */
			} else {
				Com_Printf("Could not buy this item.\n");
			}
		}
	} else {
		/* Normal item (or equipment for UGVs/Robots if buyCategory==BUY_HEAVY) */
		const objDef_t *item = BS_GetObjectDefition(&buyList.l[num + buyList.scroll]);
		assert(item);
		currentSelectedMenuEntry = item;
		UP_ItemDescription(item);
		Com_DPrintf(DEBUG_CLIENT, "BS_BuyItem_f: item %s\n", item->id);
		BS_CheckAndDoBuyItem(baseCurrent, item, BS_GetBuySellFactor());
		BS_UpdateItem(baseCurrent, num);
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

	if (Cmd_Argc() < 2) {
		Com_Printf("Usage: %s <num>\n", Cmd_Argv(0));
		return;
	}

	if (!baseCurrent)
		return;

	if (buyCat == FILTER_AIRCRAFT) {
		Com_DPrintf(DEBUG_CLIENT, "BS_SellItem_f: Redirects to BS_SellAircraft_f\n");
		BS_SellAircraft_f();
		return;
	}

	num = atoi(Cmd_Argv(1));
	if (num < 0 || num >= buyList.length)
		return;

	Cbuf_AddText(va("market_click %i\n", num + buyList.scroll));
	if (buyCat == FILTER_UGVITEM && buyList.l[num + buyList.scroll].ugv) {
		employee_t *employee;
		/* The list entry is an actual ugv/robot */
		const ugv_t *ugv = buyList.l[num + buyList.scroll].ugv;
		const objDef_t *ugvWeapon;

		UP_UGVDescription(ugv);

		/* Check if we have a weapon for this ugv in the market to sell it. */
		ugvWeapon = INVSH_GetItemByID(ugv->weapon);
		if (!ugvWeapon)
			Sys_Error("BS_BuyItem_f: Could not get wepaon '%s' for ugv/tank '%s'.", ugv->weapon, ugv->id);

		employee = E_GetHiredRobot(baseCurrent, ugv);
		if (!E_UnhireEmployee(employee)) {
			/** @todo message - Couldn't fire employee. */
			Com_DPrintf(DEBUG_CLIENT, "Couldn't sell/fire robot/ugv.\n");
		} else {
			if (baseCurrent->storage.num[ugvWeapon->idx]) {
				/* If we have a weapon we sell it as well. */
				B_UpdateStorageAndCapacity(baseCurrent, ugvWeapon, -1, qfalse, qfalse);
				ccs.eMarket.num[ugvWeapon->idx]++;
			}
			BS_BuyType(baseCurrent);
			CL_UpdateCredits(ccs.credits + ugv->price);	/** @todo make this depend on market as well? */
		}
	} else {
		const objDef_t *item = BS_GetObjectDefition(&buyList.l[num + buyList.scroll]);
		/* don't sell more items than we have */
		const int numItems = min(baseCurrent->storage.num[item->idx], BS_GetBuySellFactor());
		/* Normal item (or equipment for UGVs/Robots if buyCategory==BUY_HEAVY) */
		assert(item);
		currentSelectedMenuEntry = item;
		UP_ItemDescription(item);

		/* don't sell more items than we have */
		if (numItems) {
			/* reinit the menu */
			B_UpdateStorageAndCapacity(baseCurrent, item, -numItems, qfalse, qfalse);
			ccs.eMarket.num[item->idx] += numItems;

			BS_BuyType(baseCurrent);
			CL_UpdateCredits(ccs.credits + ccs.eMarket.bid[item->idx] * numItems);
			BS_UpdateItem(baseCurrent, num);
		}
	}
}

/**
 * @brief Enable or disable autosell option for given itemtype.
 */
static void BS_Autosell_f (void)
{
	int num;
	const objDef_t *item;

	/* Can be called from everywhere. */
	if (!baseCurrent || !curCampaign)
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

	if (gd.autosell[item->idx]) {
		gd.autosell[item->idx] = qfalse;
		Com_DPrintf(DEBUG_CLIENT, "item name: %s, autosell false\n", item->name);
	} else {
		/* Don't allow to enable autosell for items not researched. */
		if (!RS_IsResearched_ptr(item->tech))
			return;
		gd.autosell[item->idx] = qtrue;
		Com_DPrintf(DEBUG_CLIENT, "item name: %s, autosell true\n", item->name);
	}

	/* Reinit the menu. */
	BS_BuyType(baseCurrent);
}

/**
 * @brief Activates commands for Market Menu.
 */
void BS_InitStartup (void)
{
	Cmd_AddCommand("buy_type", BS_BuyType_f, NULL);
	Cmd_AddCommand("prev_buy_type", BS_Prev_BuyType_f, NULL);
	Cmd_AddCommand("next_buy_type", BS_Next_BuyType_f, NULL);
	Cmd_AddCommand("market_click", BS_MarketClick_f, "Click function for buy menu text node");
	Cmd_AddCommand("market_scroll", BS_MarketScroll_f, "Scroll function for buy menu");
	Cmd_AddCommand("mn_buysell", BS_BuySellItem_f, NULL);
	Cmd_AddCommand("mn_buy", BS_BuyItem_f, NULL);
	Cmd_AddCommand("mn_sell", BS_SellItem_f, NULL);
	Cmd_AddCommand("buy_autosell", BS_Autosell_f, "Enable or disable autosell option for given item.");

	memset(&buyList, 0, sizeof(buyList));
	buyList.length = -1;
}

/**
 * @brief Save callback for savegames
 * @sa BS_Load
 * @sa SAV_GameSave
 */
qboolean BS_Save (sizebuf_t* sb, void* data)
{
	int i;

	/* store market */
	for (i = 0; i < presaveArray[PRE_NUMODS]; i++) {
		MSG_WriteString(sb, csi.ods[i].id);
		MSG_WriteLong(sb, ccs.eMarket.num[i]);
		MSG_WriteLong(sb, ccs.eMarket.bid[i]);
		MSG_WriteLong(sb, ccs.eMarket.ask[i]);
		MSG_WriteFloat(sb, ccs.eMarket.currentEvolution[i]);
		MSG_WriteByte(sb, gd.autosell[i]);
	}

	return qtrue;
}

/**
 * @brief Load callback for savegames
 * @sa BS_Save
 * @sa SAV_GameLoad
 */
qboolean BS_Load (sizebuf_t* sb, void* data)
{
	int i;

	/* read market */
	for (i = 0; i < presaveArray[PRE_NUMODS]; i++) {
		const char *s = MSG_ReadString(sb);
		objDef_t *od = INVSH_GetItemByID(s);
		if (!od) {
			Com_Printf("BS_Load: Could not find item '%s'\n", s);
			MSG_ReadLong(sb);
			MSG_ReadLong(sb);
			MSG_ReadLong(sb);
			MSG_ReadFloat(sb);
			MSG_ReadByte(sb);
		} else {
			ccs.eMarket.num[od->idx] = MSG_ReadLong(sb);
			ccs.eMarket.bid[od->idx] = MSG_ReadLong(sb);
			ccs.eMarket.ask[od->idx] = MSG_ReadLong(sb);
			ccs.eMarket.currentEvolution[od->idx] = MSG_ReadFloat(sb);
			gd.autosell[od->idx] = MSG_ReadByte(sb);
		}
	}

	return qtrue;
}

/**
 * @brief Returns true if you can buy or sell equipment
 * @sa B_BaseInit_f
 */
qboolean BS_BuySellAllowed (const base_t* base)
{
	if (base->baseStatus != BASE_UNDER_ATTACK
	 && B_GetBuildingStatus(base, B_STORAGE)) {
		return qtrue;
	} else {
		return qfalse;
	}
}
