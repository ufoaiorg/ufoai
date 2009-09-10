/**
 * @file cp_basedefence_callbacks.c
 * @brief Header file for menu callback functions used for basedefence menu
 */

/*
All original material Copyright (C) 2002-2009 UFO: Alien Invasion.

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
#include "../menu/m_main.h"
#include "../menu/node/m_node_abstractnode.h"
#include "cp_campaign.h"
#include "cp_basedefence_callbacks.h"
#include "cp_fightequip_callbacks.h"
#include "cp_mapfightequip.h"
#include "cp_ufo.h"

static int airequipID = -1;				/**< value of aircraftItemType_t that defines what item we are installing. */

static technology_t *airequipSelectedTechnology = NULL;		/**< Selected technolgy in equip menu */

/**
 * @brief Update the list of item you can choose
 * @param[in] slot Pointer to aircraftSlot where items can be equiped
 * @param[in] tech Pointer to the technolgy to select if needed (NULL if no selected technology).
 */
static void BDEF_UpdateAircraftItemList (const aircraftSlot_t *slot, const technology_t *tech)
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
	MN_RegisterLinkedListText(TEXT_LIST, itemList);
}

/**
 * @brief Set airequipSelectedTechnology to the technology of current selected aircraft item.
 * @sa AIM_AircraftEquipMenuUpdate_f
 */
static void BDEF_AircraftEquipMenuClick_f (void)
{
	aircraft_t *aircraft;
	int num;
	technology_t **list;
	installation_t* installation = INS_GetCurrentSelectedInstallation();
	base_t *base = B_GetCurrentSelectedBase();
	aircraftSlot_t *slot = NULL;

	if ((!base && !installation) || (base && installation) || airequipID == -1)
		return;

	if (Cmd_Argc() < 2) {
		Com_Printf("Usage: %s <num>\n", Cmd_Argv(0));
		return;
	}

	/* check in which menu we are */
	if (!strcmp(MN_GetActiveMenuName(), "aircraft_equip")) {
		if (base->aircraftCurrent == NULL)
			return;
		aircraft = base->aircraftCurrent;
		base = NULL;
		installation = NULL;
	} else if (!strcmp(MN_GetActiveMenuName(), "basedefence")) {
		aircraft = NULL;
	} else
		return;

	/* Which entry in the list? */
	num = atoi(Cmd_Argv(1));

	/* make sure that airequipSelectedTechnology is NULL if no tech is found */
	airequipSelectedTechnology = NULL;

	/* build the list of all aircraft items of type airequipID - null terminated */
	list = AII_GetCraftitemTechsByType(airequipID);
	/* to prevent overflows we go through the list instead of address it directly */
	if (aircraft)
		slot = AII_SelectAircraftSlot(aircraft, airequipID);
	if (slot)
		while (*list) {
			if (AIM_SelectableCraftItem(slot, *list)) {
				/* found it */
				if (num <= 0) {
					airequipSelectedTechnology = *list;
					UP_AircraftItemDescription(AII_GetAircraftItemByID(airequipSelectedTechnology->provides));
					break;
				}
				num--;
			}
			/* next item in the tech pointer list */
			list++;
		}
}

/**
 * @brief returns the itemtype index from a string identifier
 */
static aircraftItemType_t BDEF_GetItemTypeFromID (const char *type)
{
	assert(type);
	if (!strcmp(type, "missile"))
		return AC_ITEM_BASE_MISSILE;
	else if (!strcmp(type, "laser"))
		return AC_ITEM_BASE_LASER;
	else {
		return MAX_ACITEMS;
	}
}
/**
 * @brief returns the string identifier from an itemtype index
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
 * @brief Fills the battery list, descriptions, and weapons in slots
 * of the basedefence equip menu
 */
static void BDEF_BaseDefenseMenuUpdate_f (void)
{
	int i;
	char type[MAX_VAR];
	base_t *base = B_GetCurrentSelectedBase();
	installation_t *installation = INS_GetCurrentSelectedInstallation();
	aircraftItemType_t bdefType;
	char defBuffer[MAX_SMALLMENUTEXTLEN];
	linkedList_t *slotList = NULL;
	const qboolean missileResearched = RS_IsResearched_ptr(RS_GetTechByID("rs_building_missile"));
	const qboolean laserResearched = RS_IsResearched_ptr(RS_GetTechByID("rs_building_laser"));

	if (Cmd_Argc() != 2)
		type[0] = '\0';
	else
		Q_strncpyz(type, Cmd_Argv(1), sizeof(type));

	/* don't let old links appear on this menu */
	MN_ResetData(TEXT_BASEDEFENCE_LIST);
	MN_ResetData(TEXT_LIST);
	MN_ResetData(TEXT_UFOPEDIA_METADATA);
	MN_ResetData(TEXT_STANDARD);

	/* base or installation should not be NULL because we are in the menu of this base or installation */
	if (!base && !installation)
		return;

	/* base and installation should not both be set. This function requires one or the other set. */
	if (base && installation) {
		Com_Printf("BDEF_BaseDefenseMenuUpdate_f: both the base and installation are set.  This shouldn't happen: you shouldn't be in this function.\n");
		return;
	}

	Cvar_ForceSet("mn_target", _("None"));
	Cmd_ExecuteString("setautofire disable");
	if (installation) {
		/** Every slot aims the same target */
		if (installation->numBatteries) {
			Cmd_ExecuteString(va("setautofire %i", installation->batteries[0].autofire));

			if (installation->batteries[0].target)
				Cvar_ForceSet("mn_target", UFO_AircraftToIDOnGeoscape(installation->batteries[0].target));
		}		
	} else if (base) {
		qboolean autofire = qfalse;
		/** Every slot aims the same target */
		if (base->numBatteries) {
			autofire |= base->batteries[0].autofire;
			if (base->batteries[0].target)
				Cvar_ForceSet("mn_target", UFO_AircraftToIDOnGeoscape(base->batteries[0].target));
		}
		if (base->numLasers) {
			autofire |= base->lasers[0].autofire;
			if (base->lasers[0].target && !base->batteries[0].target)
				Cvar_ForceSet("mn_target", UFO_AircraftToIDOnGeoscape(base->lasers[0].target));
		}
		if (base->numBatteries || base->numLasers)
			Cmd_ExecuteString(va("setautofire %i", autofire));
	}

	/* Check if we can change to laser or missile */
	if (base) {
		Com_sprintf(defBuffer, lengthof(defBuffer), "set_defencetypes %s %s",
			(!missileResearched) ? "na" : (base && base->numBatteries > 0) ? "enable" : "disable",
			(!laserResearched) ? "na" : (base && base->numLasers > 0) ? "enable" : "disable");
		MN_ExecuteConfunc("%s", defBuffer);
	} else if (installation) {
		Com_sprintf(defBuffer, lengthof(defBuffer), "set_defencetypes %s %s",
			(!missileResearched) ? "na" : (installation && installation->installationStatus == INSTALLATION_WORKING && installation->numBatteries > 0) ? "enable" : "disable", "na");
		MN_ExecuteConfunc("%s", defBuffer);
	}

	if (!strcmp(type, "missile"))
		bdefType = AC_ITEM_BASE_MISSILE;
	else if (!strcmp(type, "laser"))
		bdefType = AC_ITEM_BASE_LASER;
	else	/** info page */
		return;

	/* Check that the base or installation has at least 1 battery */
	if (base) {
		if (base->numBatteries + base->numLasers < 1) {
			Com_Printf("BDEF_BaseDefenseMenuUpdate_f: there is no defence battery in this base: you shouldn't be in this function.\n");
			return;
		}
	} else if (installation) {
		if (installation->installationStatus != INSTALLATION_WORKING) {
			Com_Printf("BDEF_BaseDefenseMenuUpdate_f: installation isn't working: you shouldn't be in this function.\n");
			return;
		} else if (installation->installationTemplate->maxBatteries < 1) {
			Com_Printf("BDEF_BaseDefenseMenuUpdate_f: there is no defence battery in this installation: you shouldn't be in this function.\n");
			return;
		}
	}

	if (installation) {
		/* we are in the installation defence menu */
		if (installation->installationTemplate->maxBatteries == 0) {
			Q_strncpyz(defBuffer, _("No defence of this type in this installation"), lengthof(defBuffer));
			LIST_AddString(&slotList, defBuffer);
		} else {
			BDEF_UpdateAircraftItemList(&installation->batteries[0].slot, NULL);
			for (i = 0; i < installation->installationTemplate->maxBatteries; i++) {
				if (!installation->batteries[i].slot.item) {
					Com_sprintf(defBuffer, lengthof(defBuffer), _("Slot %i:\tempty"), i);
					LIST_AddString(&slotList, defBuffer);
				} else {
					const aircraftSlot_t *slot = &installation->batteries[i].slot ;
					char status[MAX_VAR];
					if (!slot->installationTime)
						Q_strncpyz(status, _("Working"), sizeof(status));
					else if (slot->installationTime > 0)
						Q_strncpyz(status, _("Installing"), sizeof(status));
					else if (slot->nextItem)
						Q_strncpyz(status, _("Replacing"), sizeof(status));
					else
						Q_strncpyz(status, _("Removing"), sizeof(status));

					Com_sprintf(defBuffer, lengthof(defBuffer), _("Slot %i:\t%s (%s)"), i, (slot->nextItem) ? _(slot->nextItem->tech->name) : _(slot->item->tech->name), status);
					LIST_AddString(&slotList, defBuffer);
				}
			}
		}
	} else if (bdefType == AC_ITEM_BASE_MISSILE) {
		/* we are in the base defence menu for missile */
		if (base->numBatteries == 0) {
			Q_strncpyz(defBuffer, _("No defence of this type in this base"), lengthof(defBuffer));
			LIST_AddString(&slotList, defBuffer);
		} else {
			BDEF_UpdateAircraftItemList(&base->batteries[0].slot, NULL);
			for (i = 0; i < base->numBatteries; i++) {
				if (!base->batteries[i].slot.item) {
					Com_sprintf(defBuffer, lengthof(defBuffer), _("Slot %i:\tempty"), i);
					LIST_AddString(&slotList, defBuffer);
				} else {
					const aircraftSlot_t *slot = &base->batteries[i].slot ;
					char status[MAX_VAR];
					if (!slot->installationTime)
						Q_strncpyz(status, _("Working"), sizeof(status));
					else if (slot->installationTime > 0)
						Q_strncpyz(status, _("Installing"), sizeof(status));
					else if (slot->nextItem)
						Q_strncpyz(status, _("Replacing"), sizeof(status));
					else
						Q_strncpyz(status, _("Removing"), sizeof(status));

					Com_sprintf(defBuffer, lengthof(defBuffer), _("Slot %i:\t%s (%s)"), i, (slot->nextItem) ? _(slot->nextItem->tech->name) : _(slot->item->tech->name), status);
					LIST_AddString(&slotList, defBuffer);
				}
			}
		}
	} else if (bdefType == AC_ITEM_BASE_LASER) {
		/* we are in the base defence menu for laser */
		if (base->numLasers == 0) {
			Q_strncpyz(defBuffer, _("No defence of this type in this base"), lengthof(defBuffer));
			LIST_AddString(&slotList, defBuffer);
		} else {
			BDEF_UpdateAircraftItemList(&base->lasers[0].slot, NULL);
			for (i = 0; i < base->numLasers; i++) {
				if (!base->lasers[i].slot.item) {
					Com_sprintf(defBuffer, lengthof(defBuffer), _("Slot %i:\tempty"), i);
					LIST_AddString(&slotList, defBuffer);
				} else {
					const aircraftSlot_t *slot = &base->lasers[i].slot ;
					char status[MAX_VAR];
					if (!slot->installationTime)
						Q_strncpyz(status, _("Working"), sizeof(status));
					else if (slot->installationTime > 0)
						Q_strncpyz(status, _("Installing"), sizeof(status));
					else if (slot->nextItem)
						Q_strncpyz(status, _("Replacing"), sizeof(status));
					else
						Q_strncpyz(status, _("Removing"), sizeof(status));

					Com_sprintf(defBuffer, lengthof(defBuffer), _("Slot %i:\t%s (%s)"), i, (slot->nextItem) ? _(slot->nextItem->tech->name) : _(slot->item->tech->name), status);
					LIST_AddString(&slotList, defBuffer);
				}
			}
		}
	} else {
		Com_Printf("BDEF_BaseDefenseMenuUpdate_f: unknown bdefType.\n");
		return;
	}
	MN_RegisterLinkedListText(TEXT_BASEDEFENCE_LIST, slotList);
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
	int itemIDX;

	if ((!base && !installation) || (base && installation)) {
		Com_Printf("Exiting early base and install both true or both false\n");
		return;
	}

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
			AIM_AutoAddAmmo(slot);
		} else if (slot->item == AII_GetAircraftItemByID(itemTech->provides)) {
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
			AIM_AutoAddAmmo(slot);
		}
	} else {
		/* remove weapon and ammo of next item */
		AII_RemoveItemFromSlot(base, slot, qfalse);
		AII_AddItemToSlot(base, itemTech, slot, qtrue);
		AIM_AutoAddAmmo(slot);
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
		if (!strcmp(type, "missile"))
			basedefType = BASEDEF_MISSILE;
		else if (!strcmp(type, "laser"))
			basedefType = BASEDEF_LASER;
		else if (!strcmp(type, "random"))
			basedefType = BASEDEF_RANDOM;
		else
			return;
		baseIdx = atoi(Cmd_Argv(2));
	}

	/* Check that the baseIdx exists */
	if (baseIdx < 0 || baseIdx >= ccs.numBases) {
		Com_Printf("BDEF_RemoveBattery_f: baseIdx %i doesn't exists: there is only %i bases in game.\n", baseIdx, ccs.numBases);
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
		int i, type, max;
		int workingNum = 0;

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
			Com_Printf("BDEF_RemoveBattery_f: base defence type %i doesn't exists.\n", basedefType);
			return;
		}

		for (i = 0; i < ccs.numBuildings[baseIdx]; i++) {
			if (ccs.buildings[baseIdx][i].buildingType == type
			 && ccs.buildings[baseIdx][i].buildingStatus == B_STATUS_WORKING)
				workingNum++;
		}

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
	int basedefType, baseIdx;

	if (Cmd_Argc() < 3) {
		Com_Printf("Usage: %s <basedefType> <baseIdx>", Cmd_Argv(0));
		return;
	} else {
		char type[MAX_VAR];

		Q_strncpyz(type, Cmd_Argv(1), sizeof(type));
		if (!strcmp(type, "missile"))
			basedefType = BASEDEF_MISSILE;
		else if (!strcmp(type, "laser"))
			basedefType = BASEDEF_LASER;
		else if (!strcmp(type, "random"))
			basedefType = BASEDEF_RANDOM;
		else
			return;
		baseIdx = atoi(Cmd_Argv(2));
	}

	/* Check that the baseIdx exists */
	if (baseIdx < 0 || baseIdx >= ccs.numBases) {
		Com_Printf("BDEF_AddBattery_f: baseIdx %i doesn't exists: there is only %i bases in game.\n", baseIdx, ccs.numBases);
		return;
	}

	/* Check that the basedefType exists */
	if (basedefType != BASEDEF_MISSILE && basedefType != BASEDEF_LASER) {
		Com_Printf("BDEF_AddBattery_f: base defence type %i doesn't exists.\n", basedefType);
		return;
	}

	BDEF_AddBattery(basedefType, B_GetBaseByIDX(baseIdx));
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
			base->batteries[i].autofire = atoi(Cmd_Argv(1));
		for (i = 0; i < base->numLasers; i++)
			base->lasers[i].autofire = atoi(Cmd_Argv(1));
	} else if (installation)
		for (i = 0; i < installation->numBatteries; i++)
			installation->batteries[i].autofire = atoi(Cmd_Argv(1));
}

void BDEF_InitCallbacks (void)
{
	Cmd_AddCommand("add_battery", BDEF_AddBattery_f, "Add a new battery to base");
	Cmd_AddCommand("remove_battery", BDEF_RemoveBattery_f, "Remove a battery from base");
	Cmd_AddCommand("basedef_updatemenu", BDEF_BaseDefenseMenuUpdate_f, "Inits base defence menu");
	Cmd_AddCommand("basedef_list_click", BDEF_AircraftEquipMenuClick_f, NULL);
	Cmd_AddCommand("basedef_additem", BDEF_AddItem_f, "Add item to slot");
	Cmd_AddCommand("basedef_removeitem", BDEF_RemoveItem_f, "Remove item from slot");
	Cmd_AddCommand("basedef_autofire", BDEF_ChangeAutoFire, "Change autofire option for selected defence system");
}

void BDEF_ShutdownCallbacks (void)
{
	Cmd_RemoveCommand("add_battery");
	Cmd_RemoveCommand("remove_battery");
	Cmd_RemoveCommand("basedef_updatemenu");
	Cmd_RemoveCommand("basedef_list_click");
	Cmd_RemoveCommand("basedef_additem");
	Cmd_RemoveCommand("basedef_removeitem");
	Cmd_RemoveCommand("basedef_autofire");
}
