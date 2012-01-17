/**
 * @file cp_basedefence_callbacks.c
 * @brief Header file for menu callback functions used for basedefence menu
 */

/*
All original material Copyright (C) 2002-2011 UFO: Alien Invasion.

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
#include "../../ui/ui_data.h"
#include "../../ui/ui_main.h" /* UI_ExecuteConfunc */
#include "cp_campaign.h"
#include "cp_basedefence_callbacks.h"
#include "cp_fightequip_callbacks.h"
#include "cp_mapfightequip.h"
#include "cp_ufo.h"

/**
 * @brief returns the itemtype index from a string identifier
 * @param[in] type Defence type identifier string
 */
static aircraftItemType_t BDEF_GetItemTypeFromID (const char *type)
{
	assert(type);
	if (Q_streq(type, "missile"))
		return AC_ITEM_BASE_MISSILE;
	else if (Q_streq(type, "laser"))
		return AC_ITEM_BASE_LASER;

	return MAX_ACITEMS;
}
/**
 * @brief returns the string identifier from an itemtype index
 * @param[in] type Defence type
 */
static const char *BDEF_GetIDFromItemType (aircraftItemType_t type)
{
	switch (type) {
	case AC_ITEM_BASE_MISSILE:
		return "missile";
	case AC_ITEM_BASE_LASER:
		return "laser";
	default:
		return "unknown";
	}
}

/**
 * @brief Update the list of item you can choose
 * @param[in] slot Pointer to aircraftSlot where items can be equiped
 */
static void BDEF_UpdateAircraftItemList (const aircraftSlot_t *slot)
{
	linkedList_t *itemList = NULL;
	technology_t **list;

	assert(slot);

	/* Add all items corresponding to airequipID to list */
	list = AII_GetCraftitemTechsByType(slot->type);

	/* Copy only those which are researched to buffer */
	while (*list) {
		if (AIM_SelectableCraftItem(slot, *list))
			LIST_AddString(&itemList, _((*list)->name));
		list++;
	}

	/* copy buffer to mn.menuText to display it on screen */
	UI_RegisterLinkedListText(TEXT_LIST, itemList);
}

/**
 * @brief Show item description in bdef menu
 * @note it handles items in both slots and storage
 */
static void BDEF_SelectItem_f (void)
{
	aircraftSlot_t *slot;
	installation_t* installation = INS_GetCurrentSelectedInstallation();
	base_t *base = B_GetCurrentSelectedBase();
	int bdefType;
	int slotIDX;
	int itemIDX;

	if (Cmd_Argc() < 4) {
		Com_Printf("Usage: %s <type> <slotIDX> <itemIDX>\n", Cmd_Argv(0));
		return;
	}

	bdefType = BDEF_GetItemTypeFromID(Cmd_Argv(1));
	slotIDX = atoi(Cmd_Argv(2));
	itemIDX = atoi(Cmd_Argv(3));

	if (bdefType == MAX_ACITEMS) {
		Com_Printf("BDEF_AddItem_f: Invalid defence type.\n");
		return;
	}

	if (slotIDX >= 0) {
		const objDef_t *item;
		slot = (installation) ? BDEF_GetInstallationSlotByIDX(installation, bdefType, slotIDX) : BDEF_GetBaseSlotByIDX(base, bdefType, slotIDX);
		item = (slot) ? ( (slot->nextItem) ? slot->nextItem : slot->item ) : NULL;
		UP_AircraftItemDescription(item);
	} else if (itemIDX >= 0) {
		technology_t **list;
		technology_t *itemTech = NULL;
		int i = 0;

		slot = (installation) ? BDEF_GetInstallationSlotByIDX(installation, bdefType, 0) : BDEF_GetBaseSlotByIDX(base, bdefType, 0);
		list = AII_GetCraftitemTechsByType(bdefType);
		while (*list && i <= itemIDX) {
			if (AIM_SelectableCraftItem(slot, *list)) {
				itemTech = *list;
				i++;
				break;
			}
			list++;
		}
		UP_AircraftItemDescription((itemTech) ? INVSH_GetItemByIDSilent(itemTech->provides) : NULL);
	} else {
		Com_Printf("BDEF_AddItem_f: Invalid item-space.\n");
	}
}

static void BDEF_AddSlotToSlotList (const aircraftSlot_t *slot, linkedList_t **slotList)
{
	char defBuffer[512];
	const int size = LIST_Count(*slotList) + 1;
	if (!slot->item) {
		Com_sprintf(defBuffer, lengthof(defBuffer), _("%i: empty"), size);
		LIST_AddString(slotList, defBuffer);
	} else {
		const technology_t *tech;
		const char *status;
		if (!slot->installationTime)
			status = _("Working");
		else if (slot->installationTime > 0)
			status = _("Installing");
		else if (slot->nextItem)
			status = _("Replacing");
		else
			status = _("Removing");

		if (slot->nextItem != NULL)
			tech = RS_GetTechForItem(slot->nextItem);
		else
			tech = RS_GetTechForItem(slot->item);

		Com_sprintf(defBuffer, lengthof(defBuffer), "%i: %s (%s)", size, _(tech->name), status);
		LIST_AddString(slotList, defBuffer);
	}
}

static void BDEF_FillSlotList (const baseWeapon_t *batteries, int maxBatteries, linkedList_t **slotList)
{
	int i;

	BDEF_UpdateAircraftItemList(&batteries->slot);

	for (i = 0; i < maxBatteries; i++, batteries++) {
		const aircraftSlot_t *slot = &batteries->slot;
		BDEF_AddSlotToSlotList(slot, slotList);
	}
}

/**
 * @brief Fills the battery list, descriptions, and weapons in slots
 * of the basedefence equip menu
 */
static void BDEF_BaseDefenceMenuUpdate_f (void)
{
	char type[MAX_VAR];
	base_t *base = B_GetCurrentSelectedBase();
	installation_t *installation = INS_GetCurrentSelectedInstallation();
	aircraftItemType_t bdefType;
	linkedList_t *slotList = NULL;
	const qboolean missileResearched = RS_IsResearched_ptr(RS_GetTechByID("rs_building_missile"));
	const qboolean laserResearched = RS_IsResearched_ptr(RS_GetTechByID("rs_building_laser"));

	if (Cmd_Argc() != 2)
		type[0] = '\0';
	else
		Q_strncpyz(type, Cmd_Argv(1), sizeof(type));

	/* don't let old links appear on this menu */
	UI_ResetData(TEXT_BASEDEFENCE_LIST);
	UI_ResetData(TEXT_LIST);
	UI_ResetData(TEXT_ITEMDESCRIPTION);

	/* base or installation should not be NULL because we are in the menu of this base or installation */
	if (!base && !installation)
		return;

	/* base and installation should not both be set. This function requires one or the other set. */
	if (base && installation) {
		Sys_Error("BDEF_BaseDefenceMenuUpdate_f: Both the base and installation are set");
		return;
	}

	Cvar_Set("mn_target", _("None"));
	Cmd_ExecuteString("setautofire disable");
	if (installation) {
		/* Every slot aims the same target */
		if (installation->numBatteries) {
			Cmd_ExecuteString(va("setautofire %i", installation->batteries[0].autofire));

			if (installation->batteries[0].target)
				Cvar_Set("mn_target", UFO_AircraftToIDOnGeoscape(installation->batteries[0].target));
		}
	} else if (base) {
		qboolean autofire = qfalse;
		/* Every slot aims the same target */
		if (base->numBatteries) {
			autofire |= base->batteries[0].autofire;
			if (base->batteries[0].target)
				Cvar_Set("mn_target", UFO_AircraftToIDOnGeoscape(base->batteries[0].target));
		}
		if (base->numLasers) {
			autofire |= base->lasers[0].autofire;
			if (base->lasers[0].target && !base->batteries[0].target)
				Cvar_Set("mn_target", UFO_AircraftToIDOnGeoscape(base->lasers[0].target));
		}
		if (base->numBatteries || base->numLasers)
			Cmd_ExecuteString(va("setautofire %i", autofire));
	}

	/* Check if we can change to laser or missile */
	if (base) {
		UI_ExecuteConfunc("set_defencetypes %s %s",
				(!missileResearched) ? "na" : (base && base->numBatteries > 0) ? "enable" : "disable",
				(!laserResearched) ? "na" : (base && base->numLasers > 0) ? "enable" : "disable");
	} else if (installation) {
		UI_ExecuteConfunc("set_defencetypes %s %s",
				(!missileResearched) ? "na" : (installation && installation->installationStatus == INSTALLATION_WORKING
						&& installation->numBatteries > 0) ? "enable" : "disable", "na");
	}

	if (Q_streq(type, "missile"))
		bdefType = AC_ITEM_BASE_MISSILE;
	else if (Q_streq(type, "laser"))
		bdefType = AC_ITEM_BASE_LASER;
	else	/* info page */
		return;

	/* Check that the base or installation has at least 1 battery */
	if (base) {
		if (base->numBatteries + base->numLasers < 1) {
			Com_Printf("BDEF_BaseDefenceMenuUpdate_f: there is no defence battery in this base: you shouldn't be in this function.\n");
			return;
		}
	} else if (installation) {
		if (installation->installationStatus != INSTALLATION_WORKING) {
			Com_Printf("BDEF_BaseDefenceMenuUpdate_f: installation isn't working: you shouldn't be in this function.\n");
			return;
		} else if (installation->installationTemplate->maxBatteries < 1) {
			Com_Printf("BDEF_BaseDefenceMenuUpdate_f: there is no defence battery in this installation: you shouldn't be in this function.\n");
			return;
		}
	}

	if (installation) {
		/* we are in the installation defence menu */
		if (installation->installationTemplate->maxBatteries == 0) {
			LIST_AddString(&slotList, _("No defence of this type in this installation"));
		} else {
			BDEF_FillSlotList(installation->batteries, installation->installationTemplate->maxBatteries, &slotList);
		}
	} else if (bdefType == AC_ITEM_BASE_MISSILE) {
		/* we are in the base defence menu for missile */
		if (base->numBatteries == 0) {
			LIST_AddString(&slotList, _("No defence of this type in this base"));
		} else {
			BDEF_FillSlotList(base->batteries, base->numActiveBatteries, &slotList);
		}
	} else if (bdefType == AC_ITEM_BASE_LASER) {
		/* we are in the base defence menu for laser */
		if (base->numLasers == 0) {
			LIST_AddString(&slotList, _("No defence of this type in this base"));
		} else {
			BDEF_FillSlotList(base->lasers, base->numActiveLasers, &slotList);
		}
	} else {
		Com_Printf("BDEF_BaseDefenceMenuUpdate_f: unknown bdefType.\n");
		return;
	}
	UI_RegisterLinkedListText(TEXT_BASEDEFENCE_LIST, slotList);
}

/**
 * @brief add item to a base defence slot (installation too)
 */
static void BDEF_AddItem_f (void)
{
	aircraftSlot_t *slot;
	installation_t* installation = INS_GetCurrentSelectedInstallation();
	base_t *base = B_GetCurrentSelectedBase();
	technology_t **list;
	technology_t *itemTech = NULL;
	int bdefType;
	int slotIDX;

	if ((!base && !installation) || (base && installation)) {
		Com_Printf("Exiting early base and installation both true or both false\n");
		return;
	}

	if (Cmd_Argc() < 3) {
		Com_Printf("Usage: %s <type> <slotIDX>\n", Cmd_Argv(0));
		return;
	}

	bdefType = BDEF_GetItemTypeFromID(Cmd_Argv(1));
	slotIDX = atoi(Cmd_Argv(2));

	if (bdefType == MAX_ACITEMS) {
		Com_Printf("BDEF_AddItem_f: Invalid defence type.\n");
		return;
	}

	if (slotIDX < 0) {
		return;
	} else {
		int maxWeapon;
		if (base)
			maxWeapon = (bdefType == AC_ITEM_BASE_MISSILE) ? base->numActiveBatteries : base->numActiveLasers;
		else
			maxWeapon = installation->numBatteries;
		if (slotIDX >= maxWeapon)
			return;
	}

	slot = (installation) ? BDEF_GetInstallationSlotByIDX(installation, bdefType, slotIDX) : BDEF_GetBaseSlotByIDX(base, bdefType,slotIDX);

	if (!slot) {
		Com_Printf("BDEF_AddItem_f: Invalid slot.\n");
		return;
	}

	list = AII_GetCraftitemTechsByType(bdefType);
	while (*list) {
		if (AIM_SelectableCraftItem(slot, *list)) {
			itemTech = *list;
			break;
		}
		list++;
	}

	if (!itemTech)
		return;

	if (!slot->nextItem) {
		/* we add the weapon, shield, item, or base defence if slot is free or the installation of
		 * current item just began */
		if (!slot->item || (slot->item && slot->installationTime == slot->item->craftitem.installationTime)) {
			AII_RemoveItemFromSlot(base, slot, qfalse);
			AII_AddItemToSlot(base, itemTech, slot, qfalse); /* Aircraft stats are updated below */
			AII_AutoAddAmmo(slot);
		} else if (slot->item == INVSH_GetItemByID(itemTech->provides)) {
			/* the added item is the same than the one in current slot */
			if (slot->installationTime == -slot->item->craftitem.installationTime) {
				/* player changed his mind: he just want to re-add the item he just removed */
				slot->installationTime = 0;
			} else if (!slot->installationTime) {
				/* player try to add a weapon he already have: just skip */
			}
		} else {
			/* We start removing current item in slot, and the selected item will be installed afterwards */
			slot->installationTime = -slot->item->craftitem.installationTime;
			AII_AddItemToSlot(base, itemTech, slot, qtrue);
			AII_AutoAddAmmo(slot);
		}
	} else {
		/* remove weapon and ammo of next item */
		AII_RemoveItemFromSlot(base, slot, qfalse);
		AII_AddItemToSlot(base, itemTech, slot, qtrue);
		AII_AutoAddAmmo(slot);
	}

	/* Reinit menu */
	Cmd_ExecuteString(va("basedef_updatemenu %s", BDEF_GetIDFromItemType(slot->type)));
}

/**
 * @brief add item to a base defence slot (installation too)
 */
static void BDEF_RemoveItem_f (void)
{
	aircraftSlot_t *slot;
	installation_t* installation = INS_GetCurrentSelectedInstallation();
	base_t *base = B_GetCurrentSelectedBase();
	int bdefType;
	int slotIDX;

	if ((!base && !installation) || (base && installation)) {
		Com_Printf("Exiting early base and install both true or both false\n");
		return;
	}

	if (Cmd_Argc() < 3) {
		Com_Printf("Usage: %s <type> <slotIDX>\n", Cmd_Argv(0));
		return;
	}

	bdefType = BDEF_GetItemTypeFromID(Cmd_Argv(1));
	slotIDX = atoi(Cmd_Argv(2));

	if (bdefType == MAX_ACITEMS) {
		Com_Printf("BDEF_AddItem_f: Invalid defence type.\n");
		return;
	}

	if (slotIDX < 0) {
		return;
	} else {
		int maxWeapon;
		if (base)
			maxWeapon = (bdefType == AC_ITEM_BASE_MISSILE) ? base->numActiveBatteries : base->numActiveLasers;
		else
			maxWeapon = installation->numBatteries;
		if (slotIDX >= maxWeapon)
			return;
	}

	slot = (installation) ? BDEF_GetInstallationSlotByIDX(installation, bdefType, slotIDX) : BDEF_GetBaseSlotByIDX(base, bdefType,slotIDX);

	if (!slot) {
		Com_Printf("BDEF_AddItem_f: Invalid slot.\n");
		return;
	}

	if (!slot->item)
		return;

	if (!slot->nextItem) {
		/* we change the weapon, shield, item, or base defence that is already in the slot */
		/* if the item has been installed since less than 1 hour, you don't need time to remove it */
		if (slot->installationTime < slot->item->craftitem.installationTime) {
			slot->installationTime = -slot->item->craftitem.installationTime;
			AII_RemoveItemFromSlot(base, slot, qtrue); /* we remove only ammo, not item */
		} else {
			AII_RemoveItemFromSlot(base, slot, qfalse); /* we remove weapon and ammo */
		}
	} else {
		/* we change the weapon, shield, item, or base defence that will be installed AFTER the removal
		 * of the one in the slot atm */
		AII_RemoveItemFromSlot(base, slot, qfalse); /* we remove weapon and ammo */
		/* if you canceled next item for less than 1 hour, previous item is still functional */
		if (slot->installationTime == -slot->item->craftitem.installationTime) {
			slot->installationTime = 0;
		}
	}
	Cmd_ExecuteString(va("basedef_updatemenu %s", BDEF_GetIDFromItemType(slot->type)));
}

/**
 * @brief Remove a defence system from base.
 * @note 1st argument is the basedefence system type to destroy (sa basedefenceType_t).
 * @note 2nd argument is the idx of the base in which you want the battery to be destroyed.
 * @note if the first argument is BASEDEF_RANDOM, the type of the battery to destroy is randomly selected
 * @note the building must already be removed from ccs.buildings[baseIdx][]
 */
static void BDEF_RemoveBattery_f (void)
{
	int basedefType, baseIdx;
	base_t *base;

	if (Cmd_Argc() < 3) {
		Com_Printf("Usage: %s <basedefType> <baseIdx>", Cmd_Argv(0));
		return;
	} else {
		char type[MAX_VAR];

		Q_strncpyz(type, Cmd_Argv(1), sizeof(type));
		if (Q_streq(type, "missile"))
			basedefType = BASEDEF_MISSILE;
		else if (Q_streq(type, "laser"))
			basedefType = BASEDEF_LASER;
		else if (Q_streq(type, "random"))
			basedefType = BASEDEF_RANDOM;
		else
			return;
		baseIdx = atoi(Cmd_Argv(2));
	}

	/* Check that the baseIdx exists */
	if (baseIdx < 0 || baseIdx >= B_GetCount()) {
		Com_Printf("BDEF_RemoveBattery_f: baseIdx %i doesn't exist: there is only %i bases in game.\n",
				baseIdx, B_GetCount());
		return;
	}

	base = B_GetFoundedBaseByIDX(baseIdx);
	if (!base) {
		Com_Printf("BDEF_RemoveBattery_f: baseIdx %i is not founded.\n", baseIdx);
		return;
	}

	if (basedefType == BASEDEF_RANDOM) {
		/* Type of base defence to destroy is randomly selected */
		if (base->numBatteries <= 0 && base->numLasers <= 0) {
			Com_Printf("No base defence to destroy\n");
			return;
		} else if (base->numBatteries <= 0) {
			/* only laser battery is possible */
			basedefType = BASEDEF_LASER;
		} else if (base->numLasers <= 0) {
			/* only missile battery is possible */
			basedefType = BASEDEF_MISSILE;
		} else {
			/* both type are possible, choose one randomly */
			basedefType = rand() % 2 + BASEDEF_MISSILE;
		}
	} else {
		/* Check if the removed building was under construction */
		int type, max;
		int workingNum;
		building_t *building;

		switch (basedefType) {
		case BASEDEF_MISSILE:
			type = B_DEFENCE_MISSILE;
			max = base->numBatteries;
			break;
		case BASEDEF_LASER:
			type = B_DEFENCE_LASER;
			max = base->numLasers;
			break;
		default:
			Com_Printf("BDEF_RemoveBattery_f: base defence type %i doesn't exist.\n", basedefType);
			return;
		}

		building = NULL;
		workingNum = 0;
		while ((building = B_GetNextBuildingByType(base, building, type)))
			if (building->buildingStatus == B_STATUS_WORKING)
				workingNum++;

		if (workingNum == max) {
			/* Removed building was under construction, do nothing */
			return;
		} else if (workingNum != max - 1) {
			/* Should never happen, we only remove building one by one */
			Com_Printf("BDEF_RemoveBattery_f: Error while checking number of batteries (%i instead of %i) in base '%s'.\n",
				workingNum, max, base->name);
			return;
		}

		/* If we reached this point, that means we are removing a working building: continue */
	}

	BDEF_RemoveBattery(base, basedefType, -1);
}

/**
 * @brief Adds a defence system to base.
 */
static void BDEF_AddBattery_f (void)
{
	basedefenceType_t basedefType;
	base_t *base;
	const char* type;

	if (Cmd_Argc() < 3) {
		Com_Printf("Usage: %s <basedefType> <baseIdx>", Cmd_Argv(0));
		return;
	}

	type = Cmd_Argv(1);
	if (Q_streq(type, "missile"))
		basedefType = BASEDEF_MISSILE;
	else if (Q_streq(type, "laser"))
		basedefType = BASEDEF_LASER;
	else if (Q_streq(type, "random"))
		basedefType = BASEDEF_RANDOM;
	else {
		Com_Printf("BDEF_AddBattery_f: base defence type %s doesn't exist.\n", type);
		return;
	}

	base = B_GetBaseByIDX(atoi(Cmd_Argv(2)));
	if (base == NULL) {
		Com_Printf("BDEF_AddBattery_f: Invalid base index given\n");
		return;
	}

	BDEF_AddBattery(basedefType, base);
}

/**
 * @brief Function to turn on/off autofire of a base weapon
 * @param[in,out] weapon Pointer to the weapon to turn off
 * @param[in] state New status for autofire
 * @note on turning off it also reset the target so the defence weapon stop shoting
 */
static void BDEF_SetAutoFire (baseWeapon_t *weapon, qboolean state)
{
	assert(weapon);
	weapon->autofire = state;
	if (!weapon->autofire) {
		weapon->target = NULL;
		Cvar_Set("mn_target", _("None"));
	}
}

/**
 * @brief Updates the active defences counter
 */
static void BDEF_UpdateActiveBattery_f (void)
{
	base_t *base;
	const char* type;
	int count;

	if (Cmd_Argc() < 3) {
		Com_Printf("Usage: %s <basedefType> <baseIdx>", Cmd_Argv(0));
		return;
	}

	base = B_GetBaseByIDX(atoi(Cmd_Argv(2)));
	if (base == NULL) {
		Com_Printf("BDEF_UpdateActiveBattery_f: Invalid base index given\n");
		return;
	}

	type = Cmd_Argv(1);
	if (Q_streq(type, "missile")) {
		B_CheckBuildingTypeStatus(base, B_DEFENCE_MISSILE, B_STATUS_WORKING, &count);
		base->numActiveBatteries = min(count, base->numBatteries);
	} else if (Q_streq(type, "laser")) {
		B_CheckBuildingTypeStatus(base, B_DEFENCE_LASER, B_STATUS_WORKING, &count);
		base->numActiveLasers = min(count, base->numLasers);
	} else {
		Com_Printf("BDEF_UpdateActiveBattery_f: base defence type %s doesn't exist.\n", type);
		return;
	}
}

/**
 * @brief Menu callback for changing autofire state
 * Command: basedef_autofire <0|1>
 */
static void BDEF_ChangeAutoFire (void)
{
	installation_t* installation = INS_GetCurrentSelectedInstallation();
	base_t *base = B_GetCurrentSelectedBase();
	int i;

	if (!base && !installation)
		return;
	if (base && installation)
		return;
	if (Cmd_Argc() < 2)
		return;

	if (base) {
		for (i = 0; i < base->numBatteries; i++)
			BDEF_SetAutoFire(&base->batteries[i], atoi(Cmd_Argv(1)));
		for (i = 0; i < base->numLasers; i++)
			BDEF_SetAutoFire(&base->lasers[i], atoi(Cmd_Argv(1)));
	} else if (installation) {
		for (i = 0; i < installation->numBatteries; i++)
			BDEF_SetAutoFire(&installation->batteries[i], atoi(Cmd_Argv(1)));
	}
}

void BDEF_InitCallbacks (void)
{
	Cmd_AddCommand("add_battery", BDEF_AddBattery_f, "Add a new battery to base");
	Cmd_AddCommand("remove_battery", BDEF_RemoveBattery_f, "Remove a battery from base");
	Cmd_AddCommand("basedef_updatemenu", BDEF_BaseDefenceMenuUpdate_f, "Inits base defence menu");
	Cmd_AddCommand("basedef_selectitem", BDEF_SelectItem_f, NULL);
	Cmd_AddCommand("basedef_additem", BDEF_AddItem_f, "Add item to slot");
	Cmd_AddCommand("basedef_removeitem", BDEF_RemoveItem_f, "Remove item from slot");
	Cmd_AddCommand("basedef_autofire", BDEF_ChangeAutoFire, "Change autofire option for selected defence system");
	Cmd_AddCommand("basedef_updatebatteries", BDEF_UpdateActiveBattery_f, "Updates the active defence systems counters");
}

void BDEF_ShutdownCallbacks (void)
{
	Cmd_RemoveCommand("add_battery");
	Cmd_RemoveCommand("remove_battery");
	Cmd_RemoveCommand("basedef_updatebatteries");
	Cmd_RemoveCommand("basedef_updatemenu");
	Cmd_RemoveCommand("basedef_selectitem");
	Cmd_RemoveCommand("basedef_additem");
	Cmd_RemoveCommand("basedef_removeitem");
	Cmd_RemoveCommand("basedef_autofire");
}
