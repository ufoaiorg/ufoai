/**
 * @file cl_market.c
 * @brief Single player market stuff.
 * @note Buy/Sell menu functions prefix: BS_
 */

/*
Copyright (C) 2002-2006 UFO: Alien Invasion team.

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

#define MAX_BUYLIST		32

static byte buyList[MAX_BUYLIST];	/**< Current entry on the list. */
static int buyListLength;		/**< Amount of entries on the list. */
static int buyCategory;			/**< Category of items in the menu. */

/* 20060921 LordHavoc: added market buy/sell factors */
static const int MARKET_BUY_FACTOR = 1;
static const int MARKET_BUY_DIVISOR = 1;
static const int MARKET_SELL_FACTOR = 1;
static const int MARKET_SELL_DIVISOR = 1;

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

	aircraft = &aircraft_samples[aircraftID];
	tech = RS_GetTechByProvided(aircraft->id);
	assert(tech);
	UP_AircraftDescription(tech);
	Cvar_Set("mn_aircraftname", aircraft->name);
	Cvar_Set("mn_aircraft_model", aircraft->model);
}

/**
 * @brief Select one entry on the list.
 */
static void BS_BuySelect_f (void)
{
	int num;

	if (Cmd_Argc() < 2) {
		Com_Printf("Usage: buy_select <num>\n");
		return;
	}

	num = atoi(Cmd_Argv(1));
	if (num >= buyListLength)
		return;

	Cbuf_AddText(va("buyselect%i\n", num));
	if (buyCategory == NUM_BUYTYPES)
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

/**
 * @brief Init function for Buy/Sell menu. Updates the Buy/Sell menu list.
 */
static void BS_BuyType_f (void)
{
	objDef_t *od;
	aircraft_t *air_samp;
	technology_t *tech;
	int i, j = 0, num, storage = 0, supply;
	char str[MAX_VAR];

	if (Cmd_Argc() < 2) {
		Com_Printf("Usage: buy_type <category>\n");
		return;
	}
	num = atoi(Cmd_Argv(1));
	buyCategory = num;

	if (!baseCurrent)
		return;

	RS_CheckAllCollected(); /* TODO: needed? */

	CL_UpdateCredits(ccs.credits);

	/* 'normal' items */
	if (buyCategory < NUM_BUYTYPES) {
		/* Add autosell button for every entry. */
		for (j = 0; j < 28; j++)
			Cbuf_AddText(va("buy_autoselld%i\n", j));
		/* get item list */
		for (i = 0, j = 0, od = csi.ods; i < csi.numODs; i++, od++) {
			tech = (technology_t *) od->tech;
			/* Check whether the proper buytype, storage in current base and market. */
			if (tech && od->buytype == num && (baseCurrent->storage.num[i] || ccs.eMarket.num[i])) {
				Q_strncpyz(str, va("mn_item%i", j), MAX_VAR);
				Cvar_Set(str, _(od->name));

				Q_strncpyz(str, va("mn_storage%i", j), MAX_VAR);
				Cvar_SetValue(str, baseCurrent->storage.num[i]);

				Q_strncpyz(str, va("mn_supply%i", j), MAX_VAR);
				Cvar_SetValue(str, ccs.eMarket.num[i]);

				Q_strncpyz(str, va("mn_price%i", j), MAX_VAR);
				Cvar_Set(str, va("%i c", od->price));

				/* Set state of Autosell button. */
				if (gd.autosell[i])
					Cbuf_AddText(va("buy_autoselle%i\n", j));
				else
					Cbuf_AddText(va("buy_autoselld%i\n", j));

				buyList[j] = i;
				j++;
			}
		}
		buyListLength = j;
		for (; j < 28; j++) {
			Cvar_Set(va("mn_item%i", j), "");
			Cvar_Set(va("mn_storage%i", j), "");
			Cvar_Set(va("mn_supply%i", j), "");
			Cvar_Set(va("mn_price%i", j), "");
		}
	}
	/* aircraft */
	else if (buyCategory == NUM_BUYTYPES) {
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
				Q_strncpyz(str, va("mn_item%i", j), MAX_VAR);
				Cvar_Set(str, _(air_samp->name));

				Q_strncpyz(str, va("mn_storage%i", j), MAX_VAR);
				Cvar_SetValue(str, storage);

				Q_strncpyz(str, va("mn_supply%i", j), MAX_VAR);
				Cvar_SetValue(str, supply);

				Q_strncpyz(str, va("mn_price%i", j), MAX_VAR);
				Cvar_Set(str, va("%i c", air_samp->price));

				buyList[j] = i;
				j++;
			}
		}
		buyListLength = j;
		for (; j < 10; j++) {
			Cvar_Set(va("mn_item%i", j), "-");
			Cvar_Set(va("mn_storage%i", j), "-");
			Cvar_Set(va("mn_supply%i", j), "-");
			Cvar_Set(va("mn_price%i", j), "-");
		}
	}

	/* select first item */
	if (buyListLength) {
		Cbuf_AddText("buyselect0\n");
		if (buyCategory == NUM_BUYTYPES)
			CL_MarketAircraftDescription(buyList[0]);
		else
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

	num = atoi(Cmd_Argv(1));
	if (num < 0 || num >= buyListLength)
		return;

	item = buyList[num];
	Cbuf_AddText(va("buyselect%i\n", num));
	if (buyCategory == NUM_BUYTYPES) {
		CL_MarketAircraftDescription(item);
		Com_DPrintf("BS_BuyItem_f: aircraft %i\n", item);
		/* TODO: Buy aircraft */
	} else {
		CL_ItemDescription(item);
		Com_DPrintf("BS_BuyItem_f: item %i\n", item);
		if (ccs.credits >= csi.ods[item].price * MARKET_BUY_FACTOR / MARKET_BUY_DIVISOR && ccs.eMarket.num[item]) {
			Cvar_SetValue(va("mn_storage%i", num), ++baseCurrent->storage.num[item]);
			Cvar_SetValue(va("mn_supply%i", num), --ccs.eMarket.num[item]);
			CL_UpdateCredits(ccs.credits - csi.ods[item].price * MARKET_BUY_FACTOR / MARKET_BUY_DIVISOR);
		}
	}
}

/**
 * @brief Sell one item of a given type.
 * @sa BS_BuyItem_f
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

	num = atoi(Cmd_Argv(1));
	if (num < 0 || num >= buyListLength)
		return;

	item = buyList[num];
	Cbuf_AddText(va("buyselect%i\n", num));
	if (buyCategory == NUM_BUYTYPES) {
		CL_MarketAircraftDescription(item);
		/* TODO: Sell aircraft */
	} else {
		CL_ItemDescription(item);
		if (baseCurrent->storage.num[item]) {
			Cvar_SetValue(va("mn_storage%i", num), --baseCurrent->storage.num[item]);
			Cvar_SetValue(va("mn_supply%i", num), ++ccs.eMarket.num[item]);
			CL_UpdateCredits(ccs.credits + csi.ods[item].price * MARKET_SELL_FACTOR / MARKET_SELL_DIVISOR);
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
	Cbuf_AddText(va("buy_type %i\n", buyCategory));
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
	Cbuf_AddText(va("buyselect%i\n;", num));
	if (ccs.credits >= aircraft_samples[aircraftID].price) {
		CL_UpdateCredits(ccs.credits-aircraft_samples[aircraftID].price);
		Cbuf_AddText(va("aircraft_new %s %i;buy_type 4;", aircraft_samples[aircraftID].id, baseCurrent->idx));
	}
}


/**
 * @brief This functions handles the selling of an aircraft and its equipment/crew/storage.
 * @sa CL_BuyAircraft_f
 * @todo Fixme: Remove all soldiers and put equipment back to base
 * @todo Fixme: If we ever produce aircraft equipment or buy and sell the equipment,
 * make sure, that the equipment gets sold here, too.
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
			Cbuf_AddText("buy_type 4;");
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
	Cmd_AddCommand("buy_select", BS_BuySelect_f, NULL);
	Cmd_AddCommand("mn_buy", BS_BuyItem_f, NULL);
	Cmd_AddCommand("mn_sell", BS_SellItem_f, NULL);
	Cmd_AddCommand("mn_buy_aircraft", CL_BuyAircraft_f, NULL);
	Cmd_AddCommand("mn_sell_aircraft", CL_SellAircraft_f, NULL);
	Cmd_AddCommand("buy_autosell", BS_Autosell_f, "Enable or disable autosell option for given item.");
	buyListLength = -1;
}
