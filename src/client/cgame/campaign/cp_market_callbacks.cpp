/**
 * @file
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
#include "../../cl_inventory.h"
#include "../../ui/ui_dataids.h"
#include "cp_campaign.h"
#include "cp_market.h"
#include "cp_market_callbacks.h"
#include "cp_popup.h"

/**
 * @brief Prints general information about aircraft for Buy/Sell menu.
 * @param[in] aircraftTemplate Aircraft type.
 * @sa UP_AircraftDescription
 * @sa UP_AircraftItemDescription
 */
static void BS_MarketAircraftDescription (const aircraft_t* aircraftTemplate)
{
	const technology_t* tech;

	/* Break if no aircraft was given or if  it's no sample-aircraft (i.e. template). */
	if (!aircraftTemplate || aircraftTemplate != aircraftTemplate->tpl)
		return;

	tech = aircraftTemplate->tech;
	assert(tech);
	UP_AircraftDescription(tech);
	cgi->Cvar_Set("mn_aircraftname", "%s", _(aircraftTemplate->name));
	cgi->Cvar_Set("mn_item", "%s", aircraftTemplate->id);
}

/**
 * @brief Opens the UFOpedia for the current selected item/aircraft/ugv.
 * @note called by ui_market_openpedia
 */
static void BS_MarketInfoClick_f (void)
{
	const char* item = cgi->Cvar_GetString("mn_item");
	const technology_t* tech = RS_GetTechByProvided(item);

	if (tech)
		UP_OpenWith(tech->id);
}

/**
 * @brief Sets/unsets or flips the autosell property of an item on the market
 */
static void BS_SetAutosell_f (void)
{
	const objDef_t* od;
	const technology_t* tech;

	if (cgi->Cmd_Argc() < 2) {
		Com_Printf("Usage: %s <item-id> [0|1]\nWhere second parameter is the state (off/on), if omitted the autosell property will be flipped.\n",
				cgi->Cmd_Argv(0));
		return;
	}
	/* aircraft check */
	if (AIR_GetAircraftSilent(cgi->Cmd_Argv(1)) != nullptr) {
		Com_Printf("Aircraft can't be autosold!\n");
		return;
	}
	/* items */
	od = INVSH_GetItemByID(cgi->Cmd_Argv(1));
	if (!od) {
		/* no printf, INVSH_GetItemByID gave warning already */
		return;
	}
	if (od->isVirtual) {
		Com_Printf("Item %s is virtual, can't be autosold!\n", od->id);
		return;
	}
	if (od->notOnMarket) {
		Com_Printf("Item %s is not on market, can't be autosold!\n", od->id);
		return;
	}
	tech = RS_GetTechForItem(od);
	/* Don't allow to enable autosell for items not researched. */
	if (!RS_IsResearched_ptr(tech)) {
		Com_Printf("Item %s is not researched, can't be autosold!\n", od->id);
		return;
	}
	if (cgi->Cmd_Argc() >= 3)
		ccs.eMarket.autosell[od->idx] = atoi(cgi->Cmd_Argv(2));
	else
		ccs.eMarket.autosell[od->idx] = ! ccs.eMarket.autosell[od->idx];
}

/**
 * @brief Buy/Sell item/aircraft/ugv on the market
 */
static void BS_Buy_f (void)
{
	const char* itemid;
	int count;
	base_t* base = B_GetCurrentSelectedBase();
	const aircraft_t* aircraft;
	const ugv_t* ugv;
	const objDef_t* od;

	if (cgi->Cmd_Argc() < 2) {
		Com_Printf("Usage: %s <item-id> <count> [base-idx] \nNegative count means selling. If base index is omitted buys on the currently selected base.\n",
				cgi->Cmd_Argv(0));
		return;
	}

	itemid = cgi->Cmd_Argv(1);
	count = atoi(cgi->Cmd_Argv(2));

	if (cgi->Cmd_Argc() >= 4)
		base = B_GetFoundedBaseByIDX(atoi(cgi->Cmd_Argv(3)));

	if (char const* const rest = Q_strstart(itemid, "aircraft-")) {
		/* aircraft sell - with aircraft golbal idx */
		int idx = atoi(rest);
		aircraft_t* aircraft = AIR_AircraftGetFromIDX(idx);

		if (!aircraft) {
			Com_Printf("Invalid aircraft index!\n");
			return;
		}
		AIR_RemoveEmployees(*aircraft);
		BS_SellAircraft(aircraft);
		return;
	}

	if (char const* const rest = Q_strstart(itemid, "ugv-")) {
		/* ugv sell - with unique character number index */
		int ucn = atoi(rest);
		Employee* robot = E_GetEmployeeByTypeFromChrUCN(EMPL_ROBOT, ucn);

		if (!robot) {
			Com_Printf("Invalid UCN for UGV!\n");
			return;
		}

		BS_SellUGV(robot);
		return;
	}

	if (!base) {
		Com_Printf("No/invalid base selected.\n");
		return;
	}

	aircraft = AIR_GetAircraftSilent(itemid);
	if (aircraft) {
		if (!B_GetBuildingStatus(base, B_COMMAND)) {
			CP_Popup(_("Note"), _("No Command Centre in this base.\nHangars are not functional.\n"));
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
		/* Check free space in hangars. */
		if (CAP_GetFreeCapacity(base, AIR_GetCapacityByAircraftWeight(aircraft)) <= 0) {
			CP_Popup(_("Notice"), _("You cannot buy this aircraft.\nNot enough space in hangars.\n"));
			return;
		}

		if (ccs.credits < BS_GetAircraftBuyingPrice(aircraft)) {
			CP_Popup(_("Notice"), _("You cannot buy this aircraft.\nNot enough credits.\n"));
			return;
		}

		BS_BuyAircraft(aircraft, base);
		return;
	}

	ugv = cgi->Com_GetUGVByIDSilent(itemid);
	if (ugv) {
		const objDef_t* ugvWeapon = INVSH_GetItemByID(ugv->weapon);
		if (!ugvWeapon)
			cgi->Com_Error(ERR_DROP, "BS_BuyItem_f: Could not get weapon '%s' for ugv/tank '%s'.", ugv->weapon, ugv->id);

		if (E_CountUnhiredRobotsByType(ugv) < 1)
			return;
		if (ccs.eMarket.numItems[ugvWeapon->idx] < 1)
			return;

		if (ccs.credits < ugv->price) {
			CP_Popup(_("Not enough money"), _("You cannot buy this item as you don't have enough credits."));
			return;
		}

		if (CAP_GetFreeCapacity(base, CAP_ITEMS) < UGV_SIZE + ugvWeapon->size) {
			CP_Popup(_("Not enough storage space"), _("You cannot buy this item.\nNot enough space in storage.\nBuild more storage facilities."));
			return;
		}

		BS_BuyUGV(ugv, base);
		return;
	}

	if (count == 0) {
		Com_Printf("Invalid number of items to buy/sell: %s\n", cgi->Cmd_Argv(2));
		return;
	}

	/* item */
	od = INVSH_GetItemByID(cgi->Cmd_Argv(1));
	if (od) {
		if (!BS_IsOnMarket(od))
			return;

		if (count > 0) {
			/* buy */
			const int price = BS_GetItemBuyingPrice(od);
			count = std::min(count, BS_GetItemOnMarket(od));

			/* no items available on market */
			if (count <= 0)
				return;

			if (price <= 0) {
				Com_Printf("Item on market with invalid buying price: %s (%d)\n", od->id, BS_GetItemBuyingPrice(od));
				return;
			}
			/** @todo warn if player can buy less item due to available credits? */
			count = std::min(count, ccs.credits / price);
			/* not enough money for a single item */
			if (count <= 0) {
				CP_Popup(_("Not enough money"), _("You cannot buy this item as you don't have enough credits."));
				return;
			}

			if (od->size <= 0) {
				Com_Printf("Item on market with invalid size: %s (%d)\n", od->id, od->size);
				return;
			}
			count = std::min(count, CAP_GetFreeCapacity(base, CAP_ITEMS) / od->size);
			if (count <= 0) {
				CP_Popup(_("Not enough storage space"), _("You cannot buy this item.\nNot enough space in storage.\nBuild more storage facilities."));
				return;
			}

			BS_BuyItem(od, base, count);
		} else {
			/* sell */
			count = std::min(-1 * count, B_ItemInBase(od, base));
			/* no items in storage */
			if (count <= 0)
				return;
			BS_SellItem(od, base, count);
		}
		return;
	}
	Com_Printf("Invalid item ID\n");
}

/**
 * @brief Show information about item/aircaft/ugv in the market
 */
static void BS_ShowInfo_f (void)
{
	const char* itemid;
	const aircraft_t* aircraft;
	const ugv_t* ugv;
	const objDef_t* od;

	if (cgi->Cmd_Argc() < 2) {
		Com_Printf("Usage: %s <item-id>\n", cgi->Cmd_Argv(0));
		return;
	}

	itemid = cgi->Cmd_Argv(1);

	if (char const* const rest = Q_strstart(itemid, "aircraft-")) {
		/* PHALANX aircraft - with aircraft golbal idx */
		int idx = atoi(rest);
		aircraft = AIR_AircraftGetFromIDX(idx);

		if (!aircraft) {
			Com_Printf("Invalid aircraft index!\n");
			return;
		}
		/** @todo show specialized info about PHALANX aircraft */
		BS_MarketAircraftDescription(aircraft->tpl);
		return;
	}

	if (char const* const rest = Q_strstart(itemid, "ugv-")) {
		/* PHALANX ugv - with unique character number index */
		int ucn = atoi(rest);
		Employee* robot = E_GetEmployeeByTypeFromChrUCN(EMPL_ROBOT, ucn);

		if (!robot) {
			Com_Printf("Invalid UCN for UGV!\n");
			return;
		}

		/** @todo show specialized info about PHLANX UGVs */
		UP_UGVDescription(robot->getUGV());
		return;
	}

	aircraft = AIR_GetAircraftSilent(itemid);
	if (aircraft) {
		BS_MarketAircraftDescription(aircraft->tpl);
		return;
	}

	ugv = cgi->Com_GetUGVByIDSilent(itemid);
	if (ugv) {
		UP_UGVDescription(ugv);
		return;
	}

	/* item */
	od = INVSH_GetItemByID(cgi->Cmd_Argv(1));
	if (od) {
		if (!BS_IsOnMarket(od))
			return;

		if (od->craftitem.type != MAX_ACITEMS)
			UP_AircraftItemDescription(od);
		else
			cgi->INV_ItemDescription(od);
		return;
	}
	Com_Printf("Invalid item ID\n");
}

/**
 * @brief Fill market item list
 */
static void BS_FillMarket_f (void)
{
	const base_t* base = B_GetCurrentSelectedBase();
	itemFilterTypes_t type;

	if (cgi->Cmd_Argc() < 2) {
		Com_Printf("Usage: %s <category>\n", cgi->Cmd_Argv(0));
		return;
	}
	if (cgi->Cmd_Argc() >= 3)
		base = B_GetFoundedBaseByIDX(atoi(cgi->Cmd_Argv(2)));
	if (!base) {
		Com_Printf("No/invalid base selected.\n");
		return;
	}

	type = cgi->INV_GetFilterTypeID(cgi->Cmd_Argv(1));
	cgi->UI_ExecuteConfunc("ui_market_clear");
	switch (type) {
	case FILTER_UGVITEM:
		/* show own UGV */
		E_Foreach(EMPL_ROBOT, robot) {
			const ugv_t* ugv = robot->getUGV();
			const technology_t* tech = RS_GetTechByProvided(ugv->id);

			if (!robot->isHiredInBase(base))
				continue;

			cgi->UI_ExecuteConfunc("ui_market_add \"ugv-%d\" \"%s\" 1 0 0 %d - \"%s\"", robot->chr.ucn, _(tech->name), ugv->price, robot->isAwayFromBase() ? _("UGV is away from home") : "-");
		}
		/* show buyable UGV */
		for (int i = 0; i < cgi->csi->numUGV; i++) {
			const ugv_t* ugv = &cgi->csi->ugvs[i];
			const technology_t* tech = RS_GetTechByProvided(ugv->id);
			const objDef_t* ugvWeapon = INVSH_GetItemByID(ugv->weapon);
			const int buyable = std::min(E_CountUnhiredRobotsByType(ugv), BS_GetItemOnMarket(ugvWeapon));

			assert(tech);
			if (!RS_IsResearched_ptr(tech))
				continue;
			if (buyable <= 0)
				continue;

			cgi->UI_ExecuteConfunc("ui_market_add %s \"%s\" 0 %d %d %d - -", ugv->id, _(tech->name), buyable, ugv->price, ugv->price);
		}
		/* show (UGV) items */
	case FILTER_S_PRIMARY:
	case FILTER_S_SECONDARY:
	case FILTER_S_HEAVY:
	case FILTER_S_IMPLANT:
	case FILTER_S_MISC:
	case FILTER_S_ARMOUR:
	case FILTER_DUMMY:
	case FILTER_CRAFTITEM:
	case MAX_FILTERTYPES: {
		for (int i = 0; i < cgi->csi->numODs; i++) {
			const objDef_t* od = &cgi->csi->ods[i];
			const technology_t* tech = RS_GetTechForItem(od);

			if (!BS_IsOnMarket(od))
				continue;
			if (B_ItemInBase(od, base) + BS_GetItemOnMarket(od) <= 0)
				continue;
			if (type != MAX_FILTERTYPES && !cgi->INV_ItemMatchesFilter(od, type))
				continue;
			cgi->UI_ExecuteConfunc("ui_market_add %s \"%s\" %d %d %d %d %s -", od->id, _(od->name), B_ItemInBase(od, base), BS_GetItemOnMarket(od), BS_GetItemBuyingPrice(od), BS_GetItemSellingPrice(od), RS_IsResearched_ptr(tech) ? va("%d", ccs.eMarket.autosell[i]) : "-");
		}
		break;
	}
	case FILTER_AIRCRAFT: {
		AIR_ForeachFromBase(aircraft, base) {
			cgi->UI_ExecuteConfunc("ui_market_add \"aircraft-%d\" \"%s\" 1 0 0 %d - \"%s\"", aircraft->idx, aircraft->name, BS_GetAircraftSellingPrice(aircraft), AIR_IsAircraftInBase(aircraft) ? "-" : _("Aircraft is away from home"));
		}
		for (int i = 0; i < ccs.numAircraftTemplates; i++) {
			const aircraft_t* aircraft = &ccs.aircraftTemplates[i];
			if (!BS_AircraftIsOnMarket(aircraft))
				continue;
			if (!RS_IsResearched_ptr(aircraft->tech))
				continue;
			cgi->UI_ExecuteConfunc("ui_market_add \"%s\" \"%s\" 0 %d %d %d - -", aircraft->id, _(aircraft->tech->name), BS_GetAircraftOnMarket(aircraft), BS_GetAircraftBuyingPrice(aircraft), BS_GetAircraftSellingPrice(aircraft));
		}
		break;
	}
	default:
		break;
	}
	/* update capacity counters */
	cgi->UI_ExecuteConfunc("ui_market_update_caps %d %d %d %d %d %d", CAP_GetFreeCapacity(base, CAP_ITEMS), CAP_GetMax(base, CAP_ITEMS),
		CAP_GetFreeCapacity(base, CAP_AIRCRAFT_SMALL), CAP_GetMax(base, CAP_AIRCRAFT_SMALL),
		CAP_GetFreeCapacity(base, CAP_AIRCRAFT_BIG), CAP_GetMax(base, CAP_AIRCRAFT_BIG));
}

#ifdef DEBUG
static void BS_AddMarket_f (void)
{
	if (cgi->Cmd_Argc() < 3) {
		Com_Printf("Usage: %s <itemid> <count>\n", cgi->Cmd_Argv(0));
		return;
	}

	const objDef_t* obj = INVSH_GetItemByID(cgi->Cmd_Argv(1));
	if (!obj)
		return;

	const int amount = atoi(cgi->Cmd_Argv(2));
	BS_AddItemToMarket(obj, amount);
}
#endif

static const cmdList_t marketCallbacks[] = {
	{"ui_market_openpedia", BS_MarketInfoClick_f, "Open UFOPedia entry for selected item"},
	{"ui_market_setautosell", BS_SetAutosell_f, "Sets/unsets or flips the autosell property of an item on the market"},
	{"ui_market_buy", BS_Buy_f, "Buy/Sell item/aircraft/ugv on the market"},
	{"ui_market_showinfo", BS_ShowInfo_f, "Show information about item/aircaft/ugv in the market"},
	{"ui_market_fill", BS_FillMarket_f, "Fill market item list"},
#ifdef DEBUG
	{"debug_marketadd", BS_AddMarket_f, "Add items to the market"},
#endif
	{nullptr, nullptr, nullptr}
};
/**
 * @brief Function registers the callbacks of the maket UI and do initializations
 */
void BS_InitCallbacks(void)
{
	cgi->Cmd_TableAddList(marketCallbacks);
}

/**
 * @brief Function unregisters the callbacks of the maket UI
 */
void BS_ShutdownCallbacks(void)
{
	cgi->Cmd_TableRemoveList(marketCallbacks);
}
