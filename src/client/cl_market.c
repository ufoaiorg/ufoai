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
#include "menu/m_popup.h"

#define MAX_BUYLIST		64

#define MAX_MARKET_MENU_ENTRIES 28

/**
 * @brief An entry in the buylist.
 * @note The pointers are used XOR - there can be only one (used).
 */
typedef struct buyListEntry_s {
	objDef_t *item;			/**< Item pointer (see also csi.ods[] and base->storage.num[] etc...) */
	ugv_t *ugv;				/**< Used for mixed UGV (characters) and BUY_HEAVY (items) list.
							 * If not NULL it's a pointer to the correct UGV-struct (duh) otherwise a BUY_HEAVY-item is set in "item". */
	aircraft_t *aircraft;	/**< Used for aircraft production */
} buyListEntry_t;

typedef struct buyList_s {
	buyListEntry_t l[MAX_BUYLIST];	/** The actual list */
	int length;		/**< Amount of entries on the list. */
	int scroll;		/**< Scroll Position. Start of the buylist index - due to scrolling. */
} buyList_t;

static buyList_t buyList;	/**< Current buylist that is shown in the menu. */
static objDef_t *currentSelectedMenuEntry;	/**< Current selected entry on the list. */
static int buyCategory = -1;			/**< Category of items in the menu. */

/** @brief Max amount of aircraft type calculated for the market. */
static const int MAX_AIRCRAFT_SUPPLY = 8;

/** @brief Max values for Buy/Sell factors (base->buyfactor, base->sellfactor). */
static const int MAX_BS_FACTORS = 10;

/**
 * @brief Prints general information about aircraft for Buy/Sell menu.
 * @param[in] aircraftSample Aircraft type.
 * @sa UP_AircraftDescription
 * @sa UP_AircraftItemDescription
 */
static void BS_MarketAircraftDescription (aircraft_t *aircraftSample)
{
	technology_t *tech;

	/* Break if no airaft was given or if  it's no sample-aircraft (i.e. template). */
	if (!aircraftSample || aircraftSample != aircraftSample->tpl)
		return;

	tech = RS_GetTechByProvided(aircraftSample->id);
	assert(tech);
	UP_AircraftDescription(tech);
	Cvar_Set("mn_aircraftname", _(aircraftSample->name));
}

/**
 * @brief
 * @sa BS_MarketClick_f
 * @sa BS_AddToList
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
		buyList.scroll = node->textScroll;
	else
		return;

	if (buyList.length > MAX_MARKET_MENU_ENTRIES && buyList.scroll >= buyList.length - MAX_MARKET_MENU_ENTRIES) {
		buyList.scroll = buyList.length - MAX_MARKET_MENU_ENTRIES;
		node->textScroll = buyList.scroll;
	}

	/* the following nodes must exist */
	node = MN_GetNodeFromCurrentMenu("market_market");
	assert(node);
	node->textScroll = buyList.scroll;
	node = MN_GetNodeFromCurrentMenu("market_storage");
	assert(node);
	node->textScroll = buyList.scroll;
	node = MN_GetNodeFromCurrentMenu("market_prices");
	assert(node);
	node->textScroll = buyList.scroll;

	/* now update the menu pics */
	for (i = 0; i < MAX_MARKET_MENU_ENTRIES; i++)
		Cbuf_AddText(va("buy_autoselld%i\n", i));
	/* get item list */
	for (i = buyList.scroll; i < buyList.length - buyList.scroll; i++) {
		if (i >= MAX_MARKET_MENU_ENTRIES)
			break;
		assert(i >= 0);
		od = buyList.l[i].item;
		tech = od->tech;
		/* Check whether the proper buytype, storage in current base and market. */
		if (tech && BUYTYPE_MATCH(od->buytype, buyCategory) && (baseCurrent->storage.num[buyList.l[i].item->idx] || ccs.eMarket.num[buyList.l[i].item->idx])) {
			Cbuf_AddText(va("buy_show%i\n", i - buyList.scroll));
			if (gd.autosell[buyList.l[i].item->idx])
				Cbuf_AddText(va("buy_autoselle%i\n", i - buyList.scroll));
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
	if (num >= buyList.length)
		return;

	switch (buyCategory) {
	case BUY_AIRCRAFT:
		BS_MarketAircraftDescription(buyList.l[num].aircraft->tpl);
		break;
	case BUY_CRAFTITEM:
		UP_AircraftItemDescription(buyList.l[num].item);
		Cvar_Set("mn_aircraftname", "");	/** @todo Use craftitem name here? */
		break;
	case BUY_HEAVY:
		if (buyList.l[num].ugv) {
			UP_UGVDescription(buyList.l[num].ugv);
			currentSelectedMenuEntry = NULL;
		} else {
			UP_ItemDescription(buyList.l[num].item);
			currentSelectedMenuEntry = buyList.l[num].item;
		}
		break;
	case -1:
		break;
	default:
		UP_ItemDescription(buyList.l[num].item);
		currentSelectedMenuEntry = buyList.l[num].item;
		break;
	}
}

/**
 * @brief Calculates amount of aircraft in base and on the market.
 * @param[in] airCharId Aircraft id (type).
 * @param[in] inbase True if function has to return storage, false - when supply (market).
 * @return Amount of aircraft in base or amount of aircraft on the market.
 */
static int AIR_GetStorageSupply (const char *airCharId, qboolean inbase)
{
	base_t *base;
	aircraft_t *aircraft;
	int i, j;
	int amount = 0, storage = 0, supply;

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
	/* MAX_MARKET_MENU_ENTRIES items in the list (of length 1024) */
	char shortName[36];

	Com_sprintf(shortName, sizeof(shortName), "%s\n", _(name));
	Q_strcat(bsMarketNames, shortName, sizeof(bsMarketNames));

	Com_sprintf(shortName, sizeof(shortName), "%i\n", storage);
	Q_strcat(bsMarketStorage, shortName, sizeof(bsMarketStorage));

	Com_sprintf(shortName, sizeof(shortName), "%i\n", market);
	Q_strcat(bsMarketMarket, shortName, sizeof(bsMarketMarket));

	Com_sprintf(shortName, sizeof(shortName), _("%i c\n"), price);
	Q_strcat(bsMarketPrices, shortName, sizeof(bsMarketPrices));
}

/**
 * @brief Updates status of category buttons in aircraft buy/sell menu.
 */
static void BS_UpdateAircraftBSButtons (void)
{
	if (!baseCurrent)
		return;
	/* We cannot buy aircraft without any hangar. */
	if (!B_GetBuildingStatus(baseCurrent, B_HANGAR) && !B_GetBuildingStatus(baseCurrent, B_SMALL_HANGAR))
		Cbuf_AddText("abuy_disableaircrafts2\n");
	else
		Cbuf_AddText("abuy_enableaircrafts\n");
	/* We cannot buy aircraft if there is no power in our base. */
	if (!B_GetBuildingStatus(baseCurrent, B_POWER))
		Cbuf_AddText("abuy_disableaircrafts1\n");
	else
		Cbuf_AddText("abuy_enableaircrafts\n");
	/* We cannot buy any item without storage. */
	if (!B_GetBuildingStatus(baseCurrent, B_STORAGE))
		Cbuf_AddText("abuy_disableitems\n");
	else
		Cbuf_AddText("abuy_enableitems\n");
}

/**
 * @brief Updates the Buy/Sell menu list.
 * @sa BS_BuyType_f
 */
static void BS_BuyType (void)
{
	objDef_t *od;
	aircraft_t *air_samp;
	int i, j = 0;
	char tmpbuf[MAX_VAR];

	if (!baseCurrent || buyCategory == -1)
		return;

	RS_CheckAllCollected(); /** @todo: needed? */

	CL_UpdateCredits(ccs.credits);

	*bsMarketNames = *bsMarketStorage = *bsMarketMarket = *bsMarketPrices = '\0';
	MN_MenuTextReset(TEXT_STANDARD);
	mn.menuText[TEXT_MARKET_NAMES] = bsMarketNames;
	mn.menuText[TEXT_MARKET_STORAGE] = bsMarketStorage;
	mn.menuText[TEXT_MARKET_MARKET] = bsMarketMarket;
	mn.menuText[TEXT_MARKET_PRICES] = bsMarketPrices;

	/* 'normal' items */
	switch (buyCategory) {
	case BUY_AIRCRAFT:	/* Aircraft */
		{
		technology_t* tech;
		BS_UpdateAircraftBSButtons();
		for (i = 0, j = 0, air_samp = aircraftTemplates; i < numAircraftTemplates; i++, air_samp++) {
			if (air_samp->type == AIRCRAFT_UFO || air_samp->price == -1)
				continue;
			tech = RS_GetTechByProvided(air_samp->id);
			assert(tech);
			if (RS_Collected_(tech) || RS_IsResearched_ptr(tech)) {
				if (j >= buyList.scroll && j < MAX_MARKET_MENU_ENTRIES) {
					Cbuf_AddText(va("buy_show%i\n", j - buyList.scroll));
					Cbuf_AddText(va("buy_tooltip_aircraft%i\n", j - buyList.scroll));
				}
				BS_AddToList(air_samp->name, AIR_GetStorageSupply(air_samp->id, qtrue), AIR_GetStorageSupply(air_samp->id, qfalse), air_samp->price);
				buyList.l[j].item = NULL;
				buyList.l[j].ugv = NULL;
				buyList.l[j].aircraft = air_samp;
				j++;
			}
		}
		buyList.length = j;
		}
		break;
	case BUY_CRAFTITEM:	/* Aircraft items */
		BS_UpdateAircraftBSButtons();
		/* get item list */
		for (i = 0, j = 0, od = csi.ods; i < csi.numODs; i++, od++) {
			if (od->notOnMarket)
				continue;
			/* Check whether the proper buytype, storage in current base and market. */
			if (od->tech && (od->buytype == BUY_CRAFTITEM)
			 && (RS_Collected_(od->tech) || RS_IsResearched_ptr(od->tech))
			 && (baseCurrent->storage.num[i] || ccs.eMarket.num[i])) {
				if (j >= buyList.scroll && j < MAX_MARKET_MENU_ENTRIES) {
					Cbuf_AddText(va("buy_show%i\n", j - buyList.scroll));
					Cbuf_AddText(va("buy_tooltip_craftitem%i\n", j - buyList.scroll));
				}
				BS_AddToList(od->name, baseCurrent->storage.num[i], ccs.eMarket.num[i], ccs.eMarket.ask[i]);
				if (j >= MAX_BUYLIST)
					Sys_Error("Increase the MAX_BUYLIST value to handle that much items\n");
				buyList.l[j].item = od;
				buyList.l[j].ugv = NULL;
				buyList.l[j].aircraft = NULL;
				j++;
			}
		}
		buyList.length = j;
		break;
	case BUY_HEAVY:	/* Heavy equipment like UGVs and it's weapons/ammo. */
		{
		technology_t* tech;
		/* Get item list. */

		j = 0;
		for (i = 0; i < gd.numUGV; i++) {
			/**@todo Add this entry to the list */
			tech = RS_GetTechByProvided(gd.ugvs[i].id);
			assert(tech);
			if (RS_IsResearched_ptr(tech)) {
				if (j >= buyList.scroll && j < MAX_MARKET_MENU_ENTRIES) {
					Cbuf_AddText(va("buy_show%i\n", j - buyList.scroll));
				}

				BS_AddToList(tech->name,
					E_CountHiredRobotByType(baseCurrent, &gd.ugvs[i]),	/* numInStorage */
					E_CountUnhiredRobotsByType(&gd.ugvs[i]),			/* numOnMarket */
					gd.ugvs[i].price);

				if (j >= MAX_BUYLIST)
					Sys_Error("Increase the MAX_BUYLIST value to handle that much entries.\n");
				buyList.l[j].item = NULL;
				buyList.l[j].ugv = &gd.ugvs[i];
				buyList.l[j].aircraft = NULL;

				j++;
			}
		}

		for (i = 0, od = csi.ods; i < csi.numODs; i++, od++) {
			if (od->notOnMarket)
				continue;

			/* Check whether the proper buytype, storage in current base and market. */
			if (od->tech && (od->buytype == BUY_HEAVY) && (baseCurrent->storage.num[i] || ccs.eMarket.num[i])) {
				BS_AddToList(od->name, baseCurrent->storage.num[i], ccs.eMarket.num[i], ccs.eMarket.ask[i]);
				/* Set state of Autosell button. */
				if (j >= buyList.scroll && j < MAX_MARKET_MENU_ENTRIES) {
					Cbuf_AddText(va("buy_show%i\n", j - buyList.scroll));
					if (gd.autosell[i])
						Cbuf_AddText(va("buy_autoselle%i\n", j - buyList.scroll));
					else
						Cbuf_AddText(va("buy_autoselld%i\n", j - buyList.scroll));
				}

				if (j >= MAX_BUYLIST)
					Sys_Error("Increase the MAX_BUYLIST value to handle that much items\n");
				buyList.l[j].item = od;
				buyList.l[j].ugv = NULL;
				buyList.l[j].aircraft = NULL;
				j++;
			}
		}
		buyList.length = j;
		}
		break;
	default:	/* Normal items */
		if (buyCategory < BUY_AIRCRAFT || buyCategory == BUY_DUMMY) {
			/* Add autosell button for every entry. */
			for (j = 0; j < MAX_MARKET_MENU_ENTRIES; j++)
				Cbuf_AddText(va("buy_autoselld%i\n", j));
			/* get item list */
			for (i = 0, j = 0, od = csi.ods; i < csi.numODs; i++, od++) {
				if (od->notOnMarket)
					continue;
				/* Check whether the proper buytype, storage in current base and market. */
				if (od->tech && BUYTYPE_MATCH(od->buytype, buyCategory) && (baseCurrent->storage.num[i] || ccs.eMarket.num[i])) {
					BS_AddToList(od->name, baseCurrent->storage.num[i], ccs.eMarket.num[i], ccs.eMarket.ask[i]);
					/* Set state of Autosell button. */
					if (j >= buyList.scroll && j < MAX_MARKET_MENU_ENTRIES) {
						Cbuf_AddText(va("buy_show%i\n", j - buyList.scroll));
						if (gd.autosell[i])
							Cbuf_AddText(va("buy_autoselle%i\n", j - buyList.scroll));
						else
							Cbuf_AddText(va("buy_autoselld%i\n", j - buyList.scroll));
					}

					if (j >= MAX_BUYLIST)
						Sys_Error("Increase the MAX_BUYLIST value to handle that much items\n");
					buyList.l[j].item = od;
					buyList.l[j].ugv = NULL;
					buyList.l[j].aircraft = NULL;
					j++;
				}
			}
			buyList.length = j;
		}
		break;
	}

	for (; j < MAX_MARKET_MENU_ENTRIES; j++) {
		/* Hide the rest of the entries. */
		Cbuf_AddText(va("buy_hide%i\n", j));
	}

	/* Update some menu cvars. */
	if ((buyCategory < BUY_AIRCRAFT) || (buyCategory == BUY_DUMMY)) {
		/* Set up Buy/Sell factors. */
		Cvar_SetValue("mn_bfactor", baseCurrent->buyfactor);
		Cvar_SetValue("mn_sfactor", baseCurrent->sellfactor);
		/* Set up base capacities. */
		Com_sprintf(tmpbuf, sizeof(tmpbuf), "%i/%i", baseCurrent->capacities[CAP_ITEMS].cur,
			baseCurrent->capacities[CAP_ITEMS].max);
		Cvar_Set("mn_bs_storage", tmpbuf);
	} else if (buyCategory == BUY_HEAVY) {
		/**@todo better way to do this? */
		Cvar_SetValue("mn_bfactor", 1);
		Cvar_SetValue("mn_sfactor", 1);
		Com_sprintf(tmpbuf, sizeof(tmpbuf), "%i/%i", baseCurrent->capacities[CAP_ITEMS].cur,
			baseCurrent->capacities[CAP_ITEMS].max);
	}

	/* select first item */
	if (buyList.length) {
		switch (buyCategory) {	/** @sa BS_MarketClick_f */
		case BUY_AIRCRAFT:
			BS_MarketAircraftDescription(buyList.l[0].aircraft);
			break;
		case BUY_CRAFTITEM:
			Cvar_Set("mn_aircraftname", "");	/** @todo Use craftitem name here? See also BS_MarketClick_f */
			/* Select current item or first one. */
			if (currentSelectedMenuEntry) {
				UP_AircraftItemDescription(currentSelectedMenuEntry);
			} else {
				UP_AircraftItemDescription(buyList.l[0].item);
			}
			break;
		case BUY_HEAVY:
			/** @todo select first heavy item */
			if (currentSelectedMenuEntry) {
				UP_ItemDescription(currentSelectedMenuEntry);
			} else if (buyList.l[0].ugv) {
				UP_UGVDescription(buyList.l[0].ugv);
			}
			break;
		default:
			assert(buyCategory != -1);
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

/**
 * @brief Init function for Buy/Sell menu.
 */
static void BS_BuyType_f (void)
{
	if (Cmd_Argc() == 2) {
		menuNode_t* node = MN_GetNodeFromCurrentMenu("market");
		buyCategory = atoi(Cmd_Argv(1));
		buyList.scroll = 0;
		if (node)
			node->textScroll = 0;
		BS_MarketScroll_f();
	}

	BS_BuyType();
}


/**
 * @brief Buy one item of a given type.
 * @sa BS_SellItem_f
 * @sa BS_SellAircraft_f
 * @sa BS_BuyAircraft_f
 */
static void BS_BuyItem_f (void)
{
	int num, i;
	objDef_t *item;

	if (Cmd_Argc() < 2) {
		Com_Printf("Usage: %s <num>\n", Cmd_Argv(0));
		return;
	}

	if (!baseCurrent)
		return;

	assert(buyCategory != BUY_AIRCRAFT);

	num = atoi(Cmd_Argv(1));
	if (num < 0 || num >= buyList.length)
		return;

	Cbuf_AddText(va("market_click %i\n", num + buyList.scroll));

	item = buyList.l[num + buyList.scroll].item;
	if ((buyCategory == BUY_HEAVY)
	&&  (buyList.l[num + buyList.scroll].ugv)) {
		/* The list entry is an actual ugv/robot */
		ugv_t *ugv = buyList.l[num + buyList.scroll].ugv;
		qboolean ugvWeaponBuyable;
		objDef_t *ugvWeapon;

		UP_UGVDescription(ugv);

		/** @todo Storage/capacity checks for UGVs. */

		if ((ccs.credits >= ugv->price)
		&&  (E_CountUnhiredRobotsByType(ugv) > 0)) {
			/* Check if we have a weapon for this ugv in the market and there is enough storage-room for it. */
			ugvWeapon = INVSH_GetItemByID(ugv->weapon);
			if (!ugvWeapon)
				Sys_Error("BS_BuyItem_f: Could not get wepaon '%s' for ugv/tank '%s'.", ugv->weapon, ugv->id);
			ugvWeaponBuyable = qfalse;
			if (ccs.eMarket.num[ugvWeapon->idx]
			&& baseCurrent->capacities[CAP_ITEMS].max - baseCurrent->capacities[CAP_ITEMS].cur >= ugvWeapon->size)
					ugvWeaponBuyable = qtrue;

			if (ugvWeaponBuyable && E_HireRobot(baseCurrent, ugv)) {
				/* Move the item into the storage. */
				B_UpdateStorageAndCapacity(baseCurrent, ugvWeapon, 1, qfalse, qfalse);
				ccs.eMarket.num[ugvWeapon->idx]--;

				/* Update Display/List and credits. */
				BS_BuyType();
				CL_UpdateCredits(ccs.credits - ugv->price);	/** @todo make this depend on market as well? */
			} else {
				Com_Printf("Could not buy this item.\n");
			}
		}
	} else {
		/* Normal item (or equipment for UGVs/Robots if buyCategory==BUY_HEAVY) */
		currentSelectedMenuEntry = item;
		UP_ItemDescription(item);
		Com_DPrintf(DEBUG_CLIENT, "BS_BuyItem_f: item %s\n", item->id);
		for (i = 0; i < baseCurrent->buyfactor; i++) {
			if (ccs.credits >= ccs.eMarket.ask[item->idx] && ccs.eMarket.num[item->idx]) {
				if (baseCurrent->capacities[CAP_ITEMS].max - baseCurrent->capacities[CAP_ITEMS].cur >= item->size) {
					/* reinit the menu */
					B_UpdateStorageAndCapacity(baseCurrent, item, 1, qfalse, qfalse);
					ccs.eMarket.num[item->idx]--;
					BS_BuyType();
					CL_UpdateCredits(ccs.credits - ccs.eMarket.ask[item->idx]);
				} else {
					MN_Popup(_("Not enough storage space"), _("You cannot buy this item.\nNot enough space in storage.\nBuild more storage facilities."));
					break;
				}
			} else {
				break;
			}
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
	int num, i;

	if (Cmd_Argc() < 2) {
		Com_Printf("Usage: %s <num>\n", Cmd_Argv(0));
		return;
	}

	if (!baseCurrent)
		return;

	assert(buyCategory != BUY_AIRCRAFT);

	num = atoi(Cmd_Argv(1));
	if (num < 0 || num >= buyList.length)
		return;

	Cbuf_AddText(va("market_click %i\n", num + buyList.scroll));
	if (buyCategory == BUY_HEAVY && buyList.l[num + buyList.scroll].ugv) {
			employee_t *employee;
			/* The list entry is an actual ugv/robot */
			ugv_t *ugv = buyList.l[num + buyList.scroll].ugv;
			objDef_t *ugvWeapon;

			UP_UGVDescription(ugv);

			/** @todo Storage/capacity checks for UGVs. */

			/* Check if we have a weapon for this ugv in the market and there is enough storage-room for it. */
			ugvWeapon = INVSH_GetItemByID(ugv->weapon);
			if (!ugvWeapon)
				Sys_Error("BS_BuyItem_f: Could not get wepaon '%s' for ugv/tank '%s'.", ugv->weapon, ugv->id);

			employee = E_GetHiredRobot(baseCurrent, ugv);
			if (!E_UnhireEmployee(employee)) {
				/** @todo: message - Couldn't fire employee. */
				Com_DPrintf(DEBUG_CLIENT, "Couldn't sell/fire robot/ugv.\n");
			} else {
				if (baseCurrent->storage.num[ugvWeapon->idx]) {
					/* If we have a weapon we sell it as well. */
					B_UpdateStorageAndCapacity(baseCurrent, ugvWeapon, -1, qfalse, qfalse);
					ccs.eMarket.num[ugvWeapon->idx]++;
				}
				BS_BuyType();
				CL_UpdateCredits(ccs.credits + ugv->price);	/** @todo make this depend on market as well? */
			}
	} else {
		objDef_t *item;
		item = buyList.l[num + buyList.scroll].item;
		/* Normal item (or equipment for UGVs/Robots if buyCategory==BUY_HEAVY) */
		assert(item);
		currentSelectedMenuEntry = item;
		UP_ItemDescription(item);
		for (i = 0; i < baseCurrent->sellfactor; i++) {
			if (baseCurrent->storage.num[item->idx]) {
				/* reinit the menu */
				B_UpdateStorageAndCapacity(baseCurrent, item, -1, qfalse, qfalse);
				ccs.eMarket.num[item->idx]++;

				BS_BuyType();
				CL_UpdateCredits(ccs.credits + ccs.eMarket.bid[item->idx]);
			} else {
				break;
			}
		}
	}
}

/**
 * @brief Enable or disable autosell option for given itemtype.
 */
static void BS_Autosell_f (void)
{
	int num;
	objDef_t *item;

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

	item = buyList.l[num + buyList.scroll].item;
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
	BS_BuyType();
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
	BS_BuyType();
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
	BS_BuyType();
}

/**
 * @brief Buys aircraft or craftitem.
 * @sa BS_SellAircraft_f
 */
static void BS_BuyAircraft_f (void)
{
	int num, freeSpace;
	aircraft_t *aircraftSample;
	objDef_t *craftitem;

	if (Cmd_Argc() < 2) {
		Com_Printf("Usage: %s <num>\n", Cmd_Argv(0));
		return;
	}

	if (!baseCurrent)
		return;

	num = atoi(Cmd_Argv(1));
	if (num < 0 || num >= buyList.length)
		return;

	if (buyCategory == BUY_AIRCRAFT) {
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
		aircraftSample = buyList.l[num].aircraft;
		freeSpace = AIR_CalculateHangarStorage(aircraftSample, baseCurrent, 0);

		/* Check free space in hangars. */
		if (freeSpace < 0) {
			Com_Printf("BS_BuyAircraft_f()... something bad happened, AIR_CalculateHangarStorage returned -1!\n");
			return;
		}

		if (freeSpace == 0) {
			MN_Popup(_("Notice"), _("You cannot buy this aircraft.\nNot enough space in hangars.\n"));
			return;
		} else {
			if (ccs.credits < aircraftSample->price) {
				MN_Popup(_("Notice"), _("You cannot buy this aircraft.\nNot enough credits.\n"));
				return;
			} else {
				/* Hangar capacities are being updated in AIR_NewAircraft().*/
				CL_UpdateCredits(ccs.credits - aircraftSample->price);
				Cbuf_AddText(va("aircraft_new %s %i;buy_type 5;", aircraftSample->id, baseCurrent->idx));
			}
		}
	} else {
		if (!B_GetBuildingStatus(baseCurrent, B_STORAGE)) {
			MN_Popup(_("Note"), _("No storage in this base."));
			return;
		}
		craftitem = buyList.l[num].item;
		if (ccs.credits >= ccs.eMarket.ask[craftitem->idx] && ccs.eMarket.num[craftitem->idx]) {
			if (baseCurrent->capacities[CAP_ITEMS].max - baseCurrent->capacities[CAP_ITEMS].cur >= craftitem->size) {
				B_UpdateStorageAndCapacity(baseCurrent, craftitem, 1, qfalse, qfalse);
				ccs.eMarket.num[craftitem->idx]--;
				/* reinit the menu */
				BS_BuyType();
				CL_UpdateCredits(ccs.credits - ccs.eMarket.ask[craftitem->idx]);
			} else {
				MN_Popup(_("Not enough storage space"), _("You cannot buy this item.\nNot enough space in storage.\nBuild more storage facilities."));
			}
		}
	}
}

/**
 * @brief Update storage, the market, and the player's credits
 */
static void BS_ProcessCraftItemSale (const objDef_t *craftitem)
{
	if (craftitem) {
		ccs.eMarket.num[craftitem->idx]++;
		/* reinit the menu */
		BS_BuyType();
		CL_UpdateCredits(ccs.credits + ccs.eMarket.bid[craftitem->idx]);
	}
}

/**
 * @brief Sells aircraft or craftitem.
 * @sa BS_BuyAircraft_f
 */
static void BS_SellAircraft_f (void)
{
	int num, j;
	aircraft_t *aircraft;
	aircraft_t *aircraftSample;
	objDef_t *craftitem;
	qboolean found = qfalse;
	qboolean teamNote = qfalse;
	qboolean aircraftOutNote = qfalse;

	if (Cmd_Argc() < 2) {
		Com_Printf("Usage: %s <num>\n", Cmd_Argv(0));
		return;
	}

	if (!baseCurrent)
		return;

	num = atoi(Cmd_Argv(1));
	if (num < 0 || num >= buyList.length)
		return;

	if (buyCategory == BUY_AIRCRAFT) {
		aircraftSample = buyList.l[num].aircraft;
		if (!aircraftSample)
			return;

		for (j = 0, aircraft = baseCurrent->aircraft; j < baseCurrent->numAircraftInBase; j++, aircraft++) {
			if (!Q_strncmp(aircraft->id, aircraftSample->id, MAX_VAR)) {
				if (aircraft->teamSize) {
					teamNote = qtrue;
					continue;
				}
				if (aircraft->status >= AIR_IDLE) {
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
				BS_ProcessCraftItemSale(aircraft->weapons[j].item);
				BS_ProcessCraftItemSale(aircraft->weapons[j].ammo);
			}

			BS_ProcessCraftItemSale(aircraft->shield.item);
			/* there should be no ammo here, but checking can't hurt */
			BS_ProcessCraftItemSale(aircraft->shield.ammo);

			for (j = 0; j < aircraft->maxElectronics; j++) {
				BS_ProcessCraftItemSale(aircraft->electronics[j].item);
				/* there should be no ammo here, but checking can't hurt */
				BS_ProcessCraftItemSale(aircraft->electronics[j].ammo);
			}

			Com_DPrintf(DEBUG_CLIENT, "BS_SellAircraft_f: Selling aircraft with IDX %i\n", aircraft->idx);
			/* the capacities are also updated here */
			AIR_DeleteAircraft(aircraft);

			CL_UpdateCredits(ccs.credits + aircraftSample->price);
			/* reinit the menu */
			BS_BuyType();
			return;
		}
		if (!found) {
			if (teamNote)
				MN_Popup(_("Note"), _("You can't sell an aircraft if it still has a team assigned"));
			else if (aircraftOutNote)
				MN_Popup(_("Note"), _("You can't sell an aircraft that is not in base"));
			else
				Com_DPrintf(DEBUG_CLIENT, "BS_SellAircraft_f: There are no aircraft available (with no team assigned) for selling\n");
		}
	} else {
		craftitem = buyList.l[num].item;

		if (baseCurrent->storage.num[craftitem->idx]) {
			B_UpdateStorageAndCapacity(baseCurrent, craftitem, -1, qfalse, qfalse);
			BS_ProcessCraftItemSale(craftitem);
		}
	}
}

/**
 * @brief Activates commands for Market Menu.
 */
void BS_ResetMarket (void)
{
	Cmd_AddCommand("buy_type", BS_BuyType_f, NULL);
	Cmd_AddCommand("market_click", BS_MarketClick_f, "Click function for buy menu text node");
	Cmd_AddCommand("market_scroll", BS_MarketScroll_f, "Scroll function for buy menu");
	Cmd_AddCommand("mn_buy", BS_BuyItem_f, NULL);
	Cmd_AddCommand("mn_sell", BS_SellItem_f, NULL);
	Cmd_AddCommand("mn_buy_aircraft", BS_BuyAircraft_f, "Buy aircraft or craftitem");
	Cmd_AddCommand("mn_sell_aircraft", BS_SellAircraft_f, "Sell aircraft or craftitem");
	Cmd_AddCommand("buy_autosell", BS_Autosell_f, "Enable or disable autosell option for given item.");
	Cmd_AddCommand("buy_factor_inc", BS_IncreaseFactor_f, "Increase Buy/Sell factor for current base.");
	Cmd_AddCommand("buy_factor_dec", BS_DecreaseFactor_f, "Decrease Buy/Sell factor for current base.");
	buyList.length = -1;
	buyList.scroll = 0;
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
qboolean BS_Load (sizebuf_t* sb, void* data)
{
	int i;
	objDef_t *od;
	const char *s;

	/* read market */
	for (i = 0; i < presaveArray[PRE_NUMODS]; i++) {
		s = MSG_ReadString(sb);
		od = INVSH_GetItemByID(s);
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
			ccs.eMarket.cumm_supp_diff[od->idx] = MSG_ReadFloat(sb);
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
