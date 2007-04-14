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

#include "client.h"
#include "cl_global.h"

#define MAX_BUYLIST		64
#define MAX_MARKET_MENU_ENTRIES 28

static byte buyList[MAX_BUYLIST];	/**< Current entry on the list. */
static int buyListLength;		/**< Amount of entries on the list. */
static int buyCategory = -1;			/**< Category of items in the menu. */
static int buyListScrollPos;	/**< start of the buylist index - due to scrolling */

void UP_AircraftDescription(technology_t* t);

/**
 * @brief Prints general information about aircraft for buy/sell menu
 */
static void CL_MarketAircraftDescription (int aircraftID)
{
	technology_t *tech;
	aircraft_t *aircraft;

	if (aircraftID >= numAircraft_samples)
		return;

	/* Remember previous settings and restore them in CL_ResetAircraftCvars_f(). */
	Cvar_Set("mn_aircraftname_before", Cvar_VariableString("mn_aircraftname"));
	Cvar_Set("mn_aircraftmodel_before", Cvar_VariableString("mn_aircraft_model"));

	aircraft = &aircraft_samples[aircraftID];
	tech = RS_GetTechByProvided(aircraft->id);
	assert(tech);
	UP_AircraftDescription(tech);
	Cvar_Set("mn_aircraftname", aircraft->name);
	Cvar_Set("mn_aircraft_model", aircraft->model);
}

/**
 * @brief
 */
static void BS_MarketScroll_f (void)
{
	menuNode_t* node;

	node = MN_GetNodeFromCurrentMenu("market");
	if (node)
		buyListScrollPos = node->textScroll;
	else
		return;

	/* the following nodes must exist */
	node = MN_GetNodeFromCurrentMenu("market_market");
	assert(node);
	node->textScroll = buyListScrollPos;
	node = MN_GetNodeFromCurrentMenu("market_storage");
	assert(node);
	node->textScroll = buyListScrollPos;
	node = MN_GetNodeFromCurrentMenu("market_prices");
	assert(node);
	node->textScroll = buyListScrollPos;
}

/**
 * @brief Select one entry on the list.
 */
static void BS_MarketClick_f (void)
{
	int num;

	if (Cmd_Argc() < 2) {
		Com_Printf("Usage: market_click <num>\n");
		return;
	}

	num = atoi(Cmd_Argv(1));
	if (num >= buyListLength)
		return;

	if (buyCategory == BUY_AIRCRAFT)
		CL_MarketAircraftDescription(buyList[num]);
	else
		CL_ItemDescription(buyList[num]);
}

#define MAX_AIRCRAFT_STORAGE 8
/**
 * @brief set storage and supply to the values of aircraft
 * in use - and the value of aircraft available for buying
 *
 * @param aircraft Aircraft ID from scriptfile
 * @param storage Pointer to int value which will hold the amount of aircraft
 *        given by aircraft parameter in all of your bases
 * @param supply Pointer to int which will hold the amount of buyable aircraft
 * @code
 * for (i = 0, j = 0, air_samp = aircraft_samples; i < numAircraft_samples; i++, air_samp++)
 *   AIR_GetStorageSupplyCount(air_samp->id, &storage, &supply);
 * @endcode
 * @sa BS_BuyType_f
 */
static void AIR_GetStorageSupplyCount (char *airCharId, int *const storage, int *const supply)
{
	base_t *base;
	aircraft_t *aircraft;
	int i, j;

	*supply = MAX_AIRCRAFT_STORAGE;

	for (i = 0, base = gd.bases; i < gd.numBases; i++, base++) {
		if (!base->founded)
			continue;
		for (j = 0, aircraft = base->aircraft; j < base->numAircraftInBase; j++, aircraft++)
			if (!Q_strncmp(aircraft->id, airCharId, MAX_VAR))
				(*storage)++;
	}
	if (*storage < MAX_AIRCRAFT_STORAGE)
		*supply -= *storage;
	else
		*supply = 0;
}

static char bsMarketNames[1024];
static char bsMarketStorage[256];
static char bsMarketMarket[256];
static char bsMarketPrices[256];

/**
 * @brief
 */
static void BS_AddToList (const char *name, int storage, int market, int price)
{
	/* MAX_MARKET_MENU_ENTRIES items in the list (of length 1024) */
	char shortName[36];

	Com_sprintf(shortName, sizeof(shortName), "%s\n", _(name));
	Q_strcat(bsMarketNames, shortName, sizeof(bsMarketNames));

	Com_sprintf(shortName, sizeof(shortName), "%i\n", storage);
	Q_strcat(bsMarketStorage, shortName, sizeof(bsMarketStorage));

	Com_sprintf(shortName, sizeof(shortName), "%i\n", market);
	Q_strcat(bsMarketMarket, shortName, sizeof(bsMarketMarket));

	Com_sprintf(shortName, sizeof(shortName), "%i c\n", price);
	Q_strcat(bsMarketPrices, shortName, sizeof(bsMarketPrices));
}

/**
 * @brief Init function for Buy/Sell menu. Updates the Buy/Sell menu list.
 */
static void BS_BuyType_f (void)
{
	objDef_t *od;
	aircraft_t *air_samp;
	technology_t *tech;
	int i, j = 0, storage = 0, supply;
	menuNode_t* node;

	if (Cmd_Argc() == 2) {
		buyListScrollPos = 0;
		node = MN_GetNodeFromCurrentMenu("market");
		if (node)
			node->textScroll = 0;
		BS_MarketScroll_f();
		buyCategory = atoi(Cmd_Argv(1));
	}

	if (!baseCurrent || buyCategory == -1)
		return;

	RS_CheckAllCollected(); /* TODO: needed? */

	CL_UpdateCredits(ccs.credits);
	*bsMarketNames = *bsMarketStorage = *bsMarketMarket = *bsMarketPrices = '\0';

	menuText[TEXT_MARKET_NAMES] = bsMarketNames;
	menuText[TEXT_MARKET_STORAGE] = bsMarketStorage;
	menuText[TEXT_MARKET_MARKET] = bsMarketMarket;
	menuText[TEXT_MARKET_PRICES] = bsMarketPrices;

	/* 'normal' items */
	if (buyCategory < BUY_AIRCRAFT) {
		/* Add autosell button for every entry. */
		for (j = 0; j < MAX_MARKET_MENU_ENTRIES; j++)
			Cbuf_AddText(va("buy_autoselld%i\n", j));
		/* get item list */
		for (i = 0, j = 0, od = csi.ods; i < csi.numODs; i++, od++) {
			tech = (technology_t *) od->tech;
			/* Check whether the proper buytype, storage in current base and market. */
			if (tech && BUYTYPE_MATCH(od->buytype, buyCategory) && (baseCurrent->storage.num[i] || ccs.eMarket.num[i])) {
				BS_AddToList(od->name, baseCurrent->storage.num[i], ccs.eMarket.num[i], ccs.eMarket.ask[i]);

				/* Set state of Autosell button. */
				if (j < MAX_MARKET_MENU_ENTRIES) {
					if (gd.autosell[i])
						Cbuf_AddText(va("buy_autoselle%i\n", j));
					else
						Cbuf_AddText(va("buy_autoselld%i\n", j));
				}

				if (j >= MAX_BUYLIST)
					Sys_Error("Increase the MAX_BUYLIST value to handle that much items\n");
				buyList[j] = i;
				j++;
			}
		}
		buyListLength = j;
	}
	/* aircraft */
	else if (buyCategory == BUY_AIRCRAFT) {
		/* we can't buy aircraft without a hangar */
		if (!baseCurrent->hasHangar) {
			MN_PopMenu(qfalse);
			MN_Popup(_("Note"), _("Build a hangar first"));
			return;
		}
		for (i = 0, j = 0, air_samp = aircraft_samples; i < numAircraft_samples; i++, air_samp++) {
			tech = RS_GetTechByProvided(air_samp->id);
			assert(tech);
			if (RS_Collected_(tech) || RS_IsResearched_ptr(tech)) {
				storage = supply = 0;
				AIR_GetStorageSupplyCount(air_samp->id, &storage, &supply);
				BS_AddToList(air_samp->name, storage, supply, air_samp->price);

				buyList[j] = i;
				j++;
			}
		}
		buyListLength = j;
	}

	/* select first item */
	if (buyListLength) {
		if (buyCategory == BUY_AIRCRAFT)
			CL_MarketAircraftDescription(buyList[0]);
		else if (buyCategory != -1)
			CL_ItemDescription(buyList[0]);
	} else {
		/* reset description */
		Cvar_Set("mn_itemname", "");
		Cvar_Set("mn_item", "");
		Cvar_Set("mn_weapon", "");
		Cvar_Set("mn_ammo", "");
		menuText[TEXT_STANDARD] = NULL;
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
	int num, item;

	if (Cmd_Argc() < 2) {
		Com_Printf("Usage: mn_buy <num>\n");
		return;
	}

	if (!baseCurrent)
		return;

	assert(buyCategory != BUY_AIRCRAFT);

	num = atoi(Cmd_Argv(1));
	if (num < 0 || num >= buyListLength)
		return;

	item = buyList[num];
	CL_ItemDescription(item);
	Com_DPrintf("BS_BuyItem_f: item %i\n", item);
	if (ccs.credits >= ccs.eMarket.ask[item] && ccs.eMarket.num[item]) {
		/* reinit the menu */
		baseCurrent->storage.num[item]++;
		ccs.eMarket.num[item]--;
		Cmd_BufClear();
		BS_BuyType_f();
		CL_UpdateCredits(ccs.credits - ccs.eMarket.ask[item]);
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
	int num, item;

	if (Cmd_Argc() < 2) {
		Com_Printf("Usage: mn_sell <num>\n");
		return;
	}

	if (!baseCurrent)
		return;

	assert(buyCategory != BUY_AIRCRAFT);

	num = atoi(Cmd_Argv(1));
	if (num < 0 || num >= buyListLength)
		return;

	item = buyList[num];
	CL_ItemDescription(item);
	if (baseCurrent->storage.num[item]) {
		/* reinit the menu */
		baseCurrent->storage.num[item]--;
		ccs.eMarket.num[item]++;
		Cmd_BufClear();
		BS_BuyType_f();
		CL_UpdateCredits(ccs.credits + ccs.eMarket.bid[item]);
	}
}

/**
 * @brief Enable or disable autosell option for given itemtype.
 */
static void BS_Autosell_f (void)
{
	int num, item;
	technology_t *tech = NULL;

	/* Can be called from everywhere. */
	if (!baseCurrent || !curCampaign)
		return;

	if (Cmd_Argc() < 2) {
		Com_Printf("Usage: %s <num>\n", Cmd_Argv(0));
		return;
	}

	num = atoi(Cmd_Argv(1));
	Com_DPrintf("BS_Autosell_f: listnumber %i\n", num);
	if (num < 0 || num >= buyListLength)
		return;
	item = buyList[num];

	if (gd.autosell[item]) {
		gd.autosell[item] = qfalse;
		Com_DPrintf("item name: %s, autosell false\n", csi.ods[item].name);
	} else {
		/* Don't allow to enable autosell for items not researched. */
		tech = csi.ods[item].tech;
		if (!RS_IsResearched_ptr(tech))
			return;
		gd.autosell[item] = qtrue;
		Com_DPrintf("item name: %s, autosell true\n", csi.ods[item].name);
	}

	/* Reinit the menu. */
	Cmd_BufClear();
	BS_BuyType_f();
}

/**
 * @brief
 * @sa CL_SellAircraft_f
 */
static void CL_BuyAircraft_f (void)
{
	int num, aircraftID;

	if (Cmd_Argc() < 2) {
		Com_Printf("Usage: mn_buy_aircraft <num>\n");
		return;
	}

	if (!baseCurrent)
		return;

	num = atoi(Cmd_Argv(1));
	if (num < 0 || num >= buyListLength)
		return;

	aircraftID = buyList[num];
	if (ccs.credits >= aircraft_samples[aircraftID].price) {
		CL_UpdateCredits(ccs.credits-aircraft_samples[aircraftID].price);
		Cbuf_AddText(va("aircraft_new %s %i;buy_type 5;", aircraft_samples[aircraftID].id, baseCurrent->idx));
	}
}


/**
 * @brief
 *
 * FIXME: This needs work in reassigning the base aircraft array
 * or the other functions need to check whether the aircraft
 * at current arraypos is valid
 * FIXME: Remove all soldiers and put equipment back to base
 * @sa CL_BuyAircraft_f
 * TODO: if we ever produce aircraft equipment or buy and sell the equipment,
 * make sure, that the equipment gets sold here, too
 */
static void CL_SellAircraft_f (void)
{
	int num, aircraftID, i, j;
	base_t *base;
	aircraft_t *aircraft;
	qboolean found = qfalse;
	qboolean teamNote = qfalse;

	if (Cmd_Argc() < 2) {
		Com_Printf("Usage: mn_sell_aircraft <num>\n");
		return;
	}

	num = atoi(Cmd_Argv(1));
	if (num < 0 || num >= buyListLength)
		return;

	aircraftID = buyList[num];
	if (aircraftID > numAircraft_samples)
		return;

	for (i = 0, base = gd.bases; i < gd.numBases; i++, base++) {
		if (!base->founded)
			continue;
		for (j = 0, aircraft = base->aircraft; j < base->numAircraftInBase; j++, aircraft++) {
			if (!Q_strncmp(aircraft->id, aircraft_samples[aircraftID].id, MAX_VAR)) {
				if (*aircraft->teamSize) {
					teamNote = qtrue;
					continue;
				}
				found = qtrue;
				break;
			}
		}
		/* ok, we've found an empty aircraft (no team) in a base
		   so now we can sell it */
		if (found) {
			Com_DPrintf("CL_SellAircraft_f: Selling aircraft with IDX %i\n", aircraft->idx);
			CL_DeleteAircraft(aircraft);

			Cbuf_AddText(va("buyselect%i\n;", num));
			CL_UpdateCredits(ccs.credits + aircraft_samples[aircraftID].price);
			/* reinit the menu */
			Cmd_BufClear();
			BS_BuyType_f();
			return;
		}
	}
	if (!found) {
		if (teamNote)
			MN_Popup(_("Note"), _("You can't sell an aircraft if it still has a team assigned"));
		else
			Com_DPrintf("CL_SellAircraft_f: There are no aircraft available (with no team assigned) for selling\n");
	}
}

/**
 * @brief
 */
extern void CL_ResetMarket (void)
{
	Cmd_AddCommand("buy_type", BS_BuyType_f, NULL);
	Cmd_AddCommand("market_click", BS_MarketClick_f, "Click function for buy menu text node");
	Cmd_AddCommand("market_scroll", BS_MarketScroll_f, "Scroll function for buy menu");
	Cmd_AddCommand("mn_buy", BS_BuyItem_f, NULL);
	Cmd_AddCommand("mn_sell", BS_SellItem_f, NULL);
	Cmd_AddCommand("mn_buy_aircraft", CL_BuyAircraft_f, NULL);
	Cmd_AddCommand("mn_sell_aircraft", CL_SellAircraft_f, NULL);
	Cmd_AddCommand("buy_autosell", BS_Autosell_f, "Enable or disable autosell option for given item.");
	buyListLength = -1;
	buyListScrollPos = 0;
}
