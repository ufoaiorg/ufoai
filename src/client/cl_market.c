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

/** @brief Max amount of aircraft type calculated for the market. */
static const int MAX_AIRCRAFT_SUPPLY = 8;

/** @brief Max values for Buy/Sell factors (base->buyfactor, base->sellfactor). */
static const int MAX_BS_FACTORS = 10;

void UP_AircraftDescription(technology_t* t);

/**
 * @brief Calculates free space in hangars in given base.
 * @param[in] aircraftID aircraftID Index of aircraft type in aircraft_samples.
 * @param[in] base_idx Index of base in global array.
 * @return Amount of free space in hangars suitable for given aircraft type.
 * @note Returns -1 in case of error. Returns 0 if no error but no free space.
 * @todo add ufo hangars space check here
 */
static int BS_CalculateHangarStorage (int aircraftID, int base_idx)
{
	int aircraftSize = 0, freespace = 0;
	base_t *base = NULL;

	aircraftSize = aircraft_samples[aircraftID].weight;
	base = &gd.bases[base_idx];

	if (aircraftSize < 1) {
#ifdef DEBUG
		Com_Printf("BS_CalculateHangarStorage()... aircraft weight is wrong!\n");
#endif
		return -1;
	}
	if (!base) {
#ifdef DEBUG
		Com_Printf("BS_CalculateHangarStorage()... base does not exist!\n");
#endif
		return -1;
	}
	assert (base);

	/* If the aircraft size is less than 8, we will check space in small hangar. */
	if (aircraftSize < 8) {
		freespace = base->capacities[CAP_AIRCRAFTS_SMALL].max - base->capacities[CAP_AIRCRAFTS_SMALL].cur;
		Com_DPrintf("BS_CalculateHangarStorage()... freespace: %i aircraft weight: %i\n", freespace, aircraftSize);
		if (aircraftSize <= freespace) {
			return freespace;
		} else {
			/* Small aircrafts (size < 8) can be stored in big hangars as well. */
			freespace = base->capacities[CAP_AIRCRAFTS_BIG].max - base->capacities[CAP_AIRCRAFTS_BIG].cur;
			Com_DPrintf("BS_CalculateHangarStorage()... freespace: %i aircraft weight: %i\n", freespace, aircraftSize);
			if (aircraftSize <= freespace) {
				return freespace;
			} else {
				/* No free space for this aircraft. */
				return 0;
			}
		}
	}
	/* If the aircraft size is more or equal to 8, we will check space in big hangar. */
	if (aircraftSize >= 8) {
		freespace = base->capacities[CAP_AIRCRAFTS_BIG].max - base->capacities[CAP_AIRCRAFTS_BIG].cur;
		if (aircraftSize <= freespace) {
			return freespace;
		} else {
			/* No free space for this aircraft. */
			return 0;
		}
	}
	return -1;
	/* @todo: introduce capacities for UFO hangars and do space checks for them here. */
}

/**
 * @brief Prints general information about aircraft for Buy/Sell menu.
 * @param[in] aircraftID Index of aircraft type in aircraft_samples.
 */
static void BS_MarketAircraftDescription (int aircraftID)
{
	technology_t *tech;
	aircraft_t *aircraft;

	if (aircraftID >= numAircraft_samples)
		return;

	/* Remember previous settings and restore them in AIM_ResetAircraftCvars_f(). */
	Cvar_Set("mn_aircraftname_before", Cvar_VariableString("mn_aircraftname"));

	aircraft = &aircraft_samples[aircraftID];
	tech = RS_GetTechByProvided(aircraft->id);
	assert(tech);
	UP_AircraftDescription(tech);
	Cvar_Set("mn_aircraftname", aircraft->name);
}

/**
 * @brief
 */
static void BS_MarketScroll_f (void)
{
	menuNode_t* node;
	objDef_t *od;
	int i;
	technology_t *tech;

	if (!baseCurrent)
		return;

	node = MN_GetNodeFromCurrentMenu("market");
	if (node)
		buyListScrollPos = node->textScroll;
	else
		return;

	if (buyListLength > MAX_MARKET_MENU_ENTRIES && buyListScrollPos >= buyListLength - MAX_MARKET_MENU_ENTRIES) {
		buyListScrollPos = buyListLength - MAX_MARKET_MENU_ENTRIES;
		node->textScroll = buyListScrollPos;
	}

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

	/* now update the menu pics */
	for (i = 0; i < MAX_MARKET_MENU_ENTRIES; i++)
		Cbuf_AddText(va("buy_autoselld%i\n", i));
	/* get item list */
	for (i = buyListScrollPos; i < buyListLength - buyListScrollPos; i++) {
		if (i >= MAX_MARKET_MENU_ENTRIES)
			break;
		assert(i >= 0);
		od = &csi.ods[buyList[i]];
		tech = (technology_t *) od->tech;
		/* Check whether the proper buytype, storage in current base and market. */
		if (tech && BUYTYPE_MATCH(od->buytype, buyCategory) && (baseCurrent->storage.num[buyList[i]] || ccs.eMarket.num[buyList[i]])) {
			Cbuf_AddText(va("buy_show%i\n", i - buyListScrollPos));
			if (gd.autosell[buyList[i]])
				Cbuf_AddText(va("buy_autoselle%i\n", i - buyListScrollPos));
		}
	}
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
		BS_MarketAircraftDescription(buyList[num]);
	else if (buyCategory != -1) {
		CL_ItemDescription(buyList[num]);
		Cvar_SetValue("mn_bs_current", buyList[num]);
	}
}

/**
 * @brief Calculates amount of aircrafts in base and on the market.
 * @param[in] *airCharId Aircraft id (type).
 * @param[in] inbase True if function has to return storage, false - when supply (market).
 * @return Amount of aircrafts in base or amount of aircrafts on the market.
 */
static int AIR_GetStorageSupply (const char *airCharId, qboolean inbase)
{
	base_t *base = NULL;
	aircraft_t *aircraft = NULL;
	int i, j;
	int amount = 0, storage = 0, supply = 0;

	assert(baseCurrent);

	/* Get storage amount in baseCurrent. */
	for (j = 0, aircraft = baseCurrent->aircraft; j < baseCurrent->numAircraftInBase; j++, aircraft++) {
		if (!Q_strncmp(aircraft->id, airCharId, MAX_VAR))
			storage++;
	}
	/* Get supply amount (global). */
	for (j = 0, base = gd.bases; j < gd.numBases; j++, base++) {
		if (!base->founded)
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
	int i, j = 0;
	menuNode_t* node;
	char tmpbuf[64];

	if (Cmd_Argc() == 2) {
		buyCategory = atoi(Cmd_Argv(1));
		buyListScrollPos = 0;
		node = MN_GetNodeFromCurrentMenu("market");
		if (node)
			node->textScroll = 0;
		BS_MarketScroll_f();
	}

	if (!baseCurrent || buyCategory == -1)
		return;

	RS_CheckAllCollected(); /* @todo: needed? */

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
				if (j >= buyListScrollPos && j < MAX_MARKET_MENU_ENTRIES) {
					Cbuf_AddText(va("buy_show%i\n", j - buyListScrollPos));
					if (gd.autosell[i])
						Cbuf_AddText(va("buy_autoselle%i\n", j - buyListScrollPos));
					else
						Cbuf_AddText(va("buy_autoselld%i\n", j - buyListScrollPos));
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
		if (!baseCurrent->hasHangar && !baseCurrent->hasHangarSmall) {
			MN_PopMenu(qfalse);
			MN_Popup(_("Note"), _("Build a hangar first"));
			return;
		}
		for (i = 0, j = 0, air_samp = aircraft_samples; i < numAircraft_samples; i++, air_samp++) {
			tech = RS_GetTechByProvided(air_samp->id);
			assert(tech);
			if (RS_Collected_(tech) || RS_IsResearched_ptr(tech)) {
				if (j >= buyListScrollPos && j < MAX_MARKET_MENU_ENTRIES)
					Cbuf_AddText(va("buy_show%i\n", j - buyListScrollPos));
				BS_AddToList(air_samp->name, AIR_GetStorageSupply(air_samp->id, qtrue), AIR_GetStorageSupply(air_samp->id, qfalse), air_samp->price);

				buyList[j] = i;
				j++;
			}
		}
		buyListLength = j;
	}

	for (; j < MAX_MARKET_MENU_ENTRIES; j++) {
		/* hide the rest of the entries. */
		Cbuf_AddText(va("buy_hide%i\n", j));
	}

	/* Update some menu cvars. */
	if (buyCategory < BUY_AIRCRAFT) {
		/* Set up Buy/Sell factors. */
		Cvar_SetValue("mn_bfactor", baseCurrent->buyfactor);
		Cvar_SetValue("mn_sfactor", baseCurrent->sellfactor);
		/* Set up base capacities. */
		Com_sprintf(tmpbuf, sizeof(tmpbuf), "%i/%i", baseCurrent->capacities[CAP_ITEMS].max, 
		baseCurrent->capacities[CAP_ITEMS].cur);
		Cvar_Set("mn_bs_storage", tmpbuf);
	}

	/* select first item */
	if (buyListLength) {
		if (buyCategory == BUY_AIRCRAFT)
			BS_MarketAircraftDescription(buyList[0]);
		else if (buyCategory != -1) {
			/* Select current item or first one. */
			if (Cvar_VariableInteger("mn_bs_current") > 0)
				CL_ItemDescription(Cvar_VariableInteger("mn_bs_current"));
			else
				CL_ItemDescription(buyList[0]);
		}
	} else {
		/* reset description */
		Cvar_Set("mn_itemname", "");
		Cvar_Set("mn_item", "");
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
	int num, item, i;

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

	Cbuf_AddText(va("market_click %i\n", num + buyListScrollPos));

	item = buyList[num + buyListScrollPos];
	Cvar_SetValue("mn_bs_current", item);
	CL_ItemDescription(item);
	Com_DPrintf("BS_BuyItem_f: item %i\n", item);
	for (i = 0; i < baseCurrent->buyfactor; i++) {
		if (ccs.credits >= ccs.eMarket.ask[item] && ccs.eMarket.num[item]) {
			if (baseCurrent->capacities[CAP_ITEMS].max - baseCurrent->capacities[CAP_ITEMS].cur >= csi.ods[item].size) {
				/* reinit the menu */
				baseCurrent->storage.num[item]++;
				baseCurrent->capacities[CAP_ITEMS].cur += csi.ods[item].size;
				ccs.eMarket.num[item]--;
				Cmd_BufClear();
				BS_BuyType_f();
				CL_UpdateCredits(ccs.credits - ccs.eMarket.ask[item]);
			} else {
				MN_Popup(_("Not enough storage space"), _("You cannot buy this item.\nNot enough space in storage.\nBuild more storage facilities."));
				break;
			}
		} else {
			break;
		}
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
	int num, item, i, j;

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

	Cbuf_AddText(va("market_click %i\n", num + buyListScrollPos));

	item = buyList[num + buyListScrollPos];
	Cvar_SetValue("mn_bs_current", item);
	CL_ItemDescription(item);
	for (i = 0; i < baseCurrent->sellfactor; i++) {
		if (baseCurrent->storage.num[item]) {
			/* reinit the menu */
			baseCurrent->storage.num[item]--;
			for (j = csi.ods[item].size; j > 0; j--) {
				if (baseCurrent->capacities[CAP_ITEMS].cur > 0)
					baseCurrent->capacities[CAP_ITEMS].cur--;
				else
					break;
			}
			ccs.eMarket.num[item]++;
			Cmd_BufClear();
			BS_BuyType_f();
			CL_UpdateCredits(ccs.credits + ccs.eMarket.bid[item]);
		} else {
			break;
		}
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
	item = buyList[num + buyListScrollPos];

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
 * @brief Increase the Buy/Sell factor.
 * @note Command to call this: buy_factor_inc.
 * @note call with 0 for buy, 1 for sell.
 */
static void BS_IncreaseFactor_f (void)
{
	int num;

	/* Can be called from everywhere. */
	if (!baseCurrent || !curCampaign)
		return;

	if (Cmd_Argc() < 2) {
		Com_Printf("Usage: %s <num>\n", Cmd_Argv(0));
		return;
	}

	num = atoi(Cmd_Argv(1));

	if (num == 0) {
		if (baseCurrent->buyfactor >= MAX_BS_FACTORS)
			return;
		else
			baseCurrent->buyfactor++;
		Cvar_SetValue("mn_bfactor", baseCurrent->buyfactor);
	} else {
		if (baseCurrent->sellfactor >= MAX_BS_FACTORS)
			return;
		else
			baseCurrent->sellfactor++;
		Cvar_SetValue("mn_sfactor", baseCurrent->sellfactor);
	}
	/* Reinit the menu. */
	Cmd_BufClear();
	BS_BuyType_f();
}

/**
 * @brief Decrease the Buy/Sell factor.
 * @note Command to call this: buy_factor_dec.
 * @note call with 0 for buy, 1 for sell.
 */
static void BS_DecreaseFactor_f (void)
{
	int num;

	/* Can be called from everywhere. */
	if (!baseCurrent || !curCampaign)
		return;

	if (Cmd_Argc() < 2) {
		Com_Printf("Usage: %s <num>\n", Cmd_Argv(0));
		return;
	}

	num = atoi(Cmd_Argv(1));

	if (num == 0) {
		if (baseCurrent->buyfactor <= 1)
			return;
		else
			baseCurrent->buyfactor--;
		Cvar_SetValue("mn_bfactor", baseCurrent->buyfactor);
	} else {
		if (baseCurrent->sellfactor <= 1)
			return;
		else
			baseCurrent->sellfactor--;
		Cvar_SetValue("mn_sfactor", baseCurrent->sellfactor);
	}
	/* Reinit the menu. */
	Cmd_BufClear();
	BS_BuyType_f();
}

/**
 * @brief
 * @sa BS_SellAircraft_f
 */
static void BS_BuyAircraft_f (void)
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

	/* Check free space in hangars. */
	if (BS_CalculateHangarStorage(aircraftID, baseCurrent->idx) < 0) {
#ifdef DEBUG
		Com_Printf("BS_BuyAircraft_f()... something bad happened, BS_CalculateHangarStorage returned -1!\n");
#endif
		return;
	}

	if (aircraft_samples[aircraftID].weight > BS_CalculateHangarStorage(aircraftID, baseCurrent->idx)) {
		MN_Popup(_("Notice"), _("You cannot buy this aircraft.\nNot enough space in hangars.\n"));
		return;
	} else {
		if (ccs.credits < aircraft_samples[aircraftID].price) {
			MN_Popup(_("Notice"), _("You cannot buy this aircraft.\nNot enough credits.\n"));
			return;
		} else {
			/* Hangar capacities are being updated in AIR_NewAircraft().*/
			CL_UpdateCredits(ccs.credits-aircraft_samples[aircraftID].price);
			Cbuf_AddText(va("aircraft_new %s %i;buy_type 5;", aircraft_samples[aircraftID].id, baseCurrent->idx));
		}
	}
}


/**
 * @brief This functions handles the selling of an aircraft and its equipment/crew/storage.
 * @sa BS_BuyAircraft_f
 * @todo Fixme: Remove all soldiers and put equipment back to base
 * @todo Fixme: If we ever produce aircraft equipment or buy and sell the equipment,
 * make sure, that the equipment gets sold here, too.
 */
static void BS_SellAircraft_f (void)
{
	int num, aircraftID, j;
	aircraft_t *aircraft;
	qboolean found = qfalse;
	qboolean teamNote = qfalse;

	if (Cmd_Argc() < 2) {
		Com_Printf("Usage: mn_sell_aircraft <num>\n");
		return;
	}

	if (!baseCurrent)
		return;

	assert(baseCurrent);

	num = atoi(Cmd_Argv(1));
	if (num < 0 || num >= buyListLength)
		return;

	aircraftID = buyList[num];
	if (aircraftID > numAircraft_samples)
		return;

	for (j = 0, aircraft = baseCurrent->aircraft; j < baseCurrent->numAircraftInBase; j++, aircraft++) {
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
		Com_DPrintf("BS_SellAircraft_f: Selling aircraft with IDX %i\n", aircraft->idx);
		AIR_DeleteAircraft(aircraft);

		CL_UpdateCredits(ccs.credits + aircraft_samples[aircraftID].price);
		/* reinit the menu */
		Cmd_BufClear();
		BS_BuyType_f();
		/* Update hangar capacities after selling an aircraft. */
		AIR_UpdateHangarCapForAll(baseCurrent->idx);
		return;
	}
	if (!found) {
		if (teamNote)
			MN_Popup(_("Note"), _("You can't sell an aircraft if it still has a team assigned"));
		else
			Com_DPrintf("BS_SellAircraft_f: There are no aircraft available (with no team assigned) for selling\n");
	}
}

/**
 * @brief Activates commands for Market Menu.
 */
extern void BS_ResetMarket (void)
{
	Cmd_AddCommand("buy_type", BS_BuyType_f, NULL);
	Cmd_AddCommand("market_click", BS_MarketClick_f, "Click function for buy menu text node");
	Cmd_AddCommand("market_scroll", BS_MarketScroll_f, "Scroll function for buy menu");
	Cmd_AddCommand("mn_buy", BS_BuyItem_f, NULL);
	Cmd_AddCommand("mn_sell", BS_SellItem_f, NULL);
	Cmd_AddCommand("mn_buy_aircraft", BS_BuyAircraft_f, NULL);
	Cmd_AddCommand("mn_sell_aircraft", BS_SellAircraft_f, NULL);
	Cmd_AddCommand("buy_autosell", BS_Autosell_f, "Enable or disable autosell option for given item.");
	Cmd_AddCommand("buy_factor_inc", BS_IncreaseFactor_f, "Increase Buy/Sell factor for current base.");
	Cmd_AddCommand("buy_factor_dec", BS_DecreaseFactor_f, "Decrease Buy/Sell factor for current base.");
	buyListLength = -1;
	buyListScrollPos = 0;
}

/**
 * @brief Save callback for savegames
 * @sa BS_Load
 * @sa SAV_GameSave
 */
extern qboolean BS_Save (sizebuf_t* sb, void* data)
{
	int i;

	/* store market */
	for (i = 0; i < MAX_OBJDEFS; i++) {
		MSG_WriteLong(sb, ccs.eMarket.num[i]);
		MSG_WriteLong(sb, ccs.eMarket.bid[i]);
		MSG_WriteLong(sb, ccs.eMarket.ask[i]);
		MSG_WriteFloat(sb, ccs.eMarket.cumm_supp_diff[i]);
		MSG_WriteByte(sb, gd.autosell[i]);
	}

	return qtrue;
}

/**
 * @brief Load callback for savegames
 * @sa BS_Save
 * @sa SAV_GameLoad
 */
extern qboolean BS_Load (sizebuf_t* sb, void* data)
{
	int i;

	/* read market */
	for (i = 0; i < MAX_OBJDEFS; i++) {
		ccs.eMarket.num[i] = MSG_ReadLong(sb);
		ccs.eMarket.bid[i] = MSG_ReadLong(sb);
		ccs.eMarket.ask[i] = MSG_ReadLong(sb);
		ccs.eMarket.cumm_supp_diff[i] = MSG_ReadFloat(sb);
		gd.autosell[i] = MSG_ReadByte(sb);
	}

	return qtrue;
}
