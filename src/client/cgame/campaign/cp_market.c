/**
 * @file cp_market.c
 * @brief Single player market stuff.
 * @note Buy/Sell menu functions prefix: BS_
 */

/*
Copyright (C) 2002-2011 UFO: Alien Invasion.

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
#include "cp_market.h"
#include "cp_popup.h"
#include "save/save_market.h"

#define BS_GetMarket() (&ccs.eMarket)

void BS_AddItemToMarket (const objDef_t *od, int amount)
{
	market_t *market = BS_GetMarket();
	assert(amount >= 0);
	market->numItems[od->idx] += amount;
}

void BS_RemoveItemFromMarket (const objDef_t *od, int amount)
{
	market_t *market = BS_GetMarket();
	assert(amount >= 0);
	market->numItems[od->idx] -= amount;
	if (market->numItems[od->idx] < 0)
		market->numItems[od->idx] = 0;
}

void BS_AddAircraftToMarket (const aircraft_t *aircraft, int amount)
{
	const humanAircraftType_t type = Com_DropShipShortNameToID(aircraft->id);
	market_t *market = BS_GetMarket();
	assert(amount >= 0);
	assert(aircraft->type != AIRCRAFT_UFO);
	market->numAircraft[type] += amount;
}

void BS_RemoveAircraftFromMarket (const aircraft_t *aircraft, int amount)
{
	const humanAircraftType_t type = Com_DropShipShortNameToID(aircraft->id);
	market_t *market = BS_GetMarket();
	assert(amount >= 0);
	assert(aircraft->type != AIRCRAFT_UFO);
	market->numAircraft[type] -= amount;
	if (market->numAircraft[type] < 0)
		market->numAircraft[type] = 0;
}

/**
 * @brief Get the number of aircraft of the given type on the market
 * @param[in] aircraft The aircraft to search the market for
 * @return The amount of aircraft for the given type
 */
int BS_GetAircraftOnMarket (const aircraft_t *aircraft)
{
	const humanAircraftType_t type = Com_DropShipShortNameToID(aircraft->id);
	const market_t *market = BS_GetMarket();
	assert(aircraft->type != AIRCRAFT_UFO);
	return market->numAircraft[type];
}

/**
 * @brief Get the price for an aircraft that you want to sell on the market
 * @param[in] aircraft The aircraft to sell
 * @return The price of the aircraft
 */
int BS_GetAircraftSellingPrice (const aircraft_t *aircraft)
{
	const humanAircraftType_t type = Com_DropShipShortNameToID(aircraft->id);
	const market_t *market = BS_GetMarket();
	assert(aircraft->type != AIRCRAFT_UFO);
	return market->bidAircraft[type];
}

/**
 * @brief Get the price for an aircraft that you want to buy on the market
 * @param[in] aircraft The aircraft to buy
 * @return The price of the aircraft
 */
int BS_GetAircraftBuyingPrice (const aircraft_t *aircraft)
{
	const humanAircraftType_t type = Com_DropShipShortNameToID(aircraft->id);
	const market_t *market = BS_GetMarket();
	assert(aircraft->type != AIRCRAFT_UFO);
	return market->askAircraft[type];
}

/**
 * @brief Get the price for an item that you want to sell on the market
 * @param[in] od The item to sell
 * @return The price of the item
 */
int BS_GetItemSellingPrice (const objDef_t *od)
{
	const market_t *market = BS_GetMarket();
	return market->bidItems[od->idx];
}

/**
 * @brief Get the price for an item that you want to buy on the market
 * @param[in] od The item to buy
 * @return The price of the item
 */
int BS_GetItemBuyingPrice (const objDef_t *od)
{
	const market_t *market = BS_GetMarket();
	return market->askItems[od->idx];
}

/**
 * @brief Buy items.
 * @param[in] base Pointer to the base where items are bought.
 * @param[in] item Pointer to the item to buy.
 * @param[in] number Number of items to buy.
 */
qboolean BS_CheckAndDoBuyItem (base_t* base, const objDef_t *item, int number)
{
	int numItems;
	const int price = BS_GetItemBuyingPrice(item);
	const market_t *market = BS_GetMarket();

	assert(base);

	/* you can't buy more items than there are on market */
	numItems = min(number, market->numItems[item->idx]);

	/* you can't buy more items than you have credits for */
	/** @todo Handle items with price 0 better */
	if (price)
		numItems = min(numItems, ccs.credits / price);
	if (numItems <= 0)
		return qfalse;

	/* you can't buy more items than you have room for */
	/** @todo Handle items with size 0 better */
	if (item->size)
		numItems = min(numItems, CAP_GetFreeCapacity(base, CAP_ITEMS) / item->size);
	/* make sure that numItems is > 0 (can be negative because capacities.cur may be greater than
	 * capacities.max if storage is disabled or if alien items have been collected on mission */
	if (numItems <= 0) {
		CP_Popup(_("Not enough storage space"), _("You cannot buy this item.\nNot enough space in storage.\nBuild more storage facilities."));
		return qfalse;
	}

	B_UpdateStorageAndCapacity(base, item, numItems, qfalse);
	BS_RemoveItemFromMarket(item, numItems);
	CP_UpdateCredits(ccs.credits - price * numItems);
	return qtrue;
}


/**
 * @brief Update storage, the market, and the player's credits
 * @note Don't update capacity here because we can sell items directly from aircraft (already removed from storage).
 */
static void BS_ProcessCraftItemSale (const objDef_t *craftitem, const int numItems)
{
	if (craftitem) {
		BS_AddItemToMarket(craftitem, numItems);
		CP_UpdateCredits(ccs.credits + BS_GetItemSellingPrice(craftitem) * numItems);
	}
}

/**
 * @brief Sells the given aircraft with all the equipment.
 * @param aircraft The aircraft to sell
 * @return @c true if the aircraft could get sold, @c false otherwise
 */
qboolean BS_SellAircraft (aircraft_t *aircraft)
{
	int j;

	if (AIR_GetTeamSize(aircraft) > 0)
		return qfalse;

	if (!AIR_IsAircraftInBase(aircraft))
		return qfalse;

	/* sell off any items which are mounted on it */
	for (j = 0; j < aircraft->maxWeapons; j++) {
		const aircraftSlot_t *slot = &aircraft->weapons[j];
		BS_ProcessCraftItemSale(slot->item, 1);
		BS_ProcessCraftItemSale(slot->ammo, 1);
	}

	BS_ProcessCraftItemSale(aircraft->shield.item, 1);
	/* there should be no ammo here, but checking can't hurt */
	BS_ProcessCraftItemSale(aircraft->shield.ammo, 1);

	for (j = 0; j < aircraft->maxElectronics; j++) {
		const aircraftSlot_t *slot = &aircraft->electronics[j];
		BS_ProcessCraftItemSale(slot->item, 1);
		/* there should be no ammo here, but checking can't hurt */
		BS_ProcessCraftItemSale(slot->ammo, 1);
	}

	/* the capacities are also updated here */
	BS_AddAircraftToMarket(aircraft, 1);
	CP_UpdateCredits(ccs.credits + BS_GetAircraftSellingPrice(aircraft));

	AIR_DeleteAircraft(aircraft);

	return qtrue;
}

/**
 * @brief Save callback for savegames
 * @param[out] parent XML Node structure, where we write the information to
 * @sa BS_LoadXML
 * @sa SAV_GameSaveXML
 */
qboolean BS_SaveXML (xmlNode_t *parent)
{
	int i;
	xmlNode_t *node;
	const market_t *market = BS_GetMarket();

	/* store market */
	node = XML_AddNode(parent, SAVE_MARKET_MARKET);
	for (i = 0; i < csi.numODs; i++) {
		const objDef_t *od = INVSH_GetItemByIDX(i);
		if (BS_IsOnMarket(od)) {
			xmlNode_t * snode = XML_AddNode(node, SAVE_MARKET_ITEM);
			XML_AddString(snode, SAVE_MARKET_ID, od->id);
			XML_AddIntValue(snode, SAVE_MARKET_NUM, market->numItems[i]);
			XML_AddIntValue(snode, SAVE_MARKET_BID, market->bidItems[i]);
			XML_AddIntValue(snode, SAVE_MARKET_ASK, market->askItems[i]);
			XML_AddDoubleValue(snode, SAVE_MARKET_EVO, market->currentEvolutionItems[i]);
			XML_AddBoolValue(snode, SAVE_MARKET_AUTOSELL, market->autosell[i]);
		}
	}
	for (i = 0; i < AIRCRAFTTYPE_MAX; i++) {
		if (market->bidAircraft[i] > 0 || market->askAircraft[i] > 0) {
			xmlNode_t * snode = XML_AddNode(node, SAVE_MARKET_AIRCRAFT);
			XML_AddString(snode, SAVE_MARKET_ID, Com_DropShipTypeToShortName(i));
			XML_AddIntValue(snode, SAVE_MARKET_NUM, market->numAircraft[i]);
			XML_AddIntValue(snode, SAVE_MARKET_BID, market->bidAircraft[i]);
			XML_AddIntValue(snode, SAVE_MARKET_ASK, market->askAircraft[i]);
			XML_AddDoubleValue(snode, SAVE_MARKET_EVO, market->currentEvolutionAircraft[i]);
		}
	}
	return qtrue;
}

/**
 * @brief Load callback for savegames
 * @param[in] parent XML Node structure, where we get the information from
 * @sa BS_Save
 * @sa SAV_GameLoad
 */
qboolean BS_LoadXML (xmlNode_t *parent)
{
	xmlNode_t *node, *snode;
	market_t *market = BS_GetMarket();

	node = XML_GetNode(parent, SAVE_MARKET_MARKET);
	if (!node)
		return qfalse;

	for (snode = XML_GetNode(node, SAVE_MARKET_ITEM); snode; snode = XML_GetNextNode(snode, node, SAVE_MARKET_ITEM)) {
		const char *s = XML_GetString(snode, SAVE_MARKET_ID);
		const objDef_t *od = INVSH_GetItemByID(s);

		if (!od) {
			Com_Printf("BS_LoadXML: Could not find item '%s'\n", s);
			continue;
		}

		market->numItems[od->idx] = XML_GetInt(snode, SAVE_MARKET_NUM, 0);
		market->bidItems[od->idx] = XML_GetInt(snode, SAVE_MARKET_BID, 0);
		market->askItems[od->idx] = XML_GetInt(snode, SAVE_MARKET_ASK, 0);
		market->currentEvolutionItems[od->idx] = XML_GetDouble(snode, SAVE_MARKET_EVO, 0.0);
		market->autosell[od->idx] = XML_GetBool(snode, SAVE_MARKET_AUTOSELL, qfalse);
	}
	for (snode = XML_GetNode(node, SAVE_MARKET_AIRCRAFT); snode; snode = XML_GetNextNode(snode, node, SAVE_MARKET_AIRCRAFT)) {
		const char *s = XML_GetString(snode, SAVE_MARKET_ID);
		humanAircraftType_t type = Com_DropShipShortNameToID(s);

		market->numAircraft[type] = XML_GetInt(snode, SAVE_MARKET_NUM, 0);
		market->bidAircraft[type] = XML_GetInt(snode, SAVE_MARKET_BID, 0);
		market->askAircraft[type] = XML_GetInt(snode, SAVE_MARKET_ASK, 0);
		market->currentEvolutionAircraft[type] = XML_GetDouble(snode, SAVE_MARKET_EVO, 0.0);
	}

	return qtrue;
}

/**
 * @brief sets market prices at start of the game
 * @sa CP_CampaignInit
 * @sa B_SetUpFirstBase
 * @sa BS_Load (Market load function)
 */
void BS_InitMarket (const campaign_t *campaign)
{
	int i;
	market_t *market = BS_GetMarket();

	for (i = 0; i < csi.numODs; i++) {
		const objDef_t *od = INVSH_GetItemByIDX(i);
		if (market->askItems[i] == 0) {
			market->askItems[i] = od->price;
			market->bidItems[i] = floor(market->askItems[i] * BID_FACTOR);
		}

		if (campaign->marketDef->numItems[i] <= 0)
			continue;

		if (RS_IsResearched_ptr(RS_GetTechForItem(od))) {
			/* the other relevant values were already set above */
			market->numItems[i] = campaign->marketDef->numItems[i];
		} else {
			Com_Printf("BS_InitMarket: Could not add item %s to the market - not marked as researched in campaign %s\n",
					od->id, campaign->id);
		}
	}

	for (i = 0; i < AIRCRAFTTYPE_MAX; i++) {
		const char* name = Com_DropShipTypeToShortName(i);
		const aircraft_t *aircraft = AIR_GetAircraft(name);
		if (market->askAircraft[i] == 0) {
			market->askAircraft[i] = aircraft->price;
			market->bidAircraft[i] = floor(market->askAircraft[i] * BID_FACTOR);
		}

		if (campaign->marketDef->numAircraft[i] <= 0)
			continue;

		if (RS_IsResearched_ptr(aircraft->tech)) {
			/* the other relevant values were already set above */
			market->numAircraft[i] = campaign->marketDef->numAircraft[i];
		} else {
			Com_Printf("BS_InitMarket: Could not add aircraft %s to the market - not marked as researched in campaign %s\n",
					aircraft->id, campaign->id);
		}
	}
}

/**
 * @brief make number of items change every day.
 * @sa CP_CampaignRun
 * @sa daily called
 * @note This function makes items number on market slowly reach the asymptotic number of items defined in equipment.ufo
 * If an item has just been researched, it's not available on market until RESEARCH_LIMIT_DELAY days is reached.
 */
void CP_CampaignRunMarket (campaign_t *campaign)
{
	int i;
	const float TYPICAL_TIME = 10.f;			/**< Number of days to reach the asymptotic number of items */
	const int RESEARCH_LIMIT_DELAY = 30;		/**< Numbers of days after end of research to wait in order to have
												 * items added on market */
	market_t *market = BS_GetMarket();

	assert(campaign->marketDef);
	assert(campaign->asymptoticMarketDef);

	for (i = 0; i < csi.numODs; i++) {
		const objDef_t *od = INVSH_GetItemByIDX(i);
		const technology_t *tech = RS_GetTechForItem(od);
		int asymptoticNumber;

		if (RS_IsResearched_ptr(tech) && (campaign->marketDef->numItems[i] != 0 || ccs.date.day > tech->researchedDate.day + RESEARCH_LIMIT_DELAY)) {
			/* if items are researched for more than RESEARCH_LIMIT_DELAY or was on the initial market,
			 * there number tend to the value defined in equipment.ufo.
			 * This value is the asymptotic value if it is not 0, or initial value else */
			asymptoticNumber = campaign->asymptoticMarketDef->numItems[i] ? campaign->asymptoticMarketDef->numItems[i] : campaign->marketDef->numItems[i];
		} else {
			/* items that have just been researched don't appear on market, but they can disappear */
			asymptoticNumber = 0;
		}

		/* Store the evolution of the market in currentEvolution */
		market->currentEvolutionItems[i] += (asymptoticNumber - market->numItems[i]) / TYPICAL_TIME;

		/* Check if new items appeared or disappeared on market */
		if (fabs(market->currentEvolutionItems[i]) >= 1.0f) {
			const int num = (int)(market->currentEvolutionItems[i]);
			if (num >= 0)
				BS_AddItemToMarket(od, num);
			else
				BS_RemoveItemFromMarket(od, -num);
			market->currentEvolutionItems[i] -= num;
		}
	}

	for (i = 0; i < AIRCRAFTTYPE_MAX; i++) {
		const humanAircraftType_t type = i;
		const char *aircraftID = Com_DropShipTypeToShortName(type);
		const aircraft_t* aircraft = AIR_GetAircraft(aircraftID);
		const technology_t *tech = aircraft->tech;
		int asymptoticNumber;

		if (RS_IsResearched_ptr(tech) && (campaign->marketDef->numAircraft[i] != 0 || ccs.date.day > tech->researchedDate.day + RESEARCH_LIMIT_DELAY)) {
			/* if aircraft is researched for more than RESEARCH_LIMIT_DELAY or was on the initial market,
			 * there number tend to the value defined in equipment.ufo.
			 * This value is the asymptotic value if it is not 0, or initial value else */
			asymptoticNumber = campaign->asymptoticMarketDef->numAircraft[i] ? campaign->asymptoticMarketDef->numAircraft[i] : campaign->marketDef->numAircraft[i];
		} else {
			/* items that have just been researched don't appear on market, but they can disappear */
			asymptoticNumber = 0;
		}
		/* Store the evolution of the market in currentEvolution */
		market->currentEvolutionAircraft[i] += (asymptoticNumber - market->numAircraft[i]) / TYPICAL_TIME;

		/* Check if new items appeared or disappeared on market */
		if (fabs(market->currentEvolutionAircraft[i]) >= 1.0f) {
			const int num = (int)(market->currentEvolutionAircraft[i]);
			if (num >= 0)
				BS_AddAircraftToMarket(aircraft, num);
			else
				BS_RemoveAircraftFromMarket(aircraft, -num);
			market->currentEvolutionAircraft[i] -= num;
		}
	}
}

/**
 * @brief Check if an item is on market
 * @param[in] item Pointer to the item to check
 * @note this function doesn't check if the item is available on market (buyable > 0)
 */
qboolean BS_IsOnMarket (const objDef_t const* item)
{
	assert(item);
	return !(item->isVirtual || item->notOnMarket);
}

/**
 * @brief Returns true if you can buy or sell equipment
 * @param[in] base Pointer to base to check on
 * @sa B_BaseInit_f
 */
qboolean BS_BuySellAllowed (const base_t* base)
{
	return !B_IsUnderAttack(base) && B_GetBuildingStatus(base, B_STORAGE);
}
