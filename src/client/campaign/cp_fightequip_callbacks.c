/**
 * @file cp_fightequip_callbacks.c
 * @brief Header file for menu callback functions used for base and aircraft equip menu
 * @todo we should split basedefence and aircraftequipment in 2 files
 */

/*
All original material Copyright (C) 2002-2009 UFO: Alien Invasion team.

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
#include "../cl_menu.h"
#include "../menu/node/m_node_abstractnode.h"
#include "cp_campaign.h"
#include "cp_fightequip_callbacks.h"
#include "cp_mapfightequip.h"
#include "cp_ufo.h"

static int airequipID = -1;				/**< value of aircraftItemType_t that defines what item we are installing. */

static qboolean noparams = qfalse;			/**< true if AIM_AircraftEquipMenuUpdate_f or BDEF_BaseDefenseMenuUpdate_f don't need paramters */
static int airequipSelectedZone = ZONE_NONE;		/**< Selected zone in equip menu */
static int airequipSelectedSlot = ZONE_NONE;			/**< Selected slot in equip menu */
static technology_t *airequipSelectedTechnology = NULL;		/**< Selected technolgy in equip menu */


/**
 * @brief Check airequipID value and set the correct values for aircraft items
 */
static void AIM_CheckAirequipID (void)
{
	switch (airequipID) {
	case AC_ITEM_AMMO:
	case AC_ITEM_WEAPON:
	case AC_ITEM_SHIELD:
	case AC_ITEM_ELECTRONICS:
		return;
	default:
		airequipID = AC_ITEM_WEAPON;
	}
}

/**
 * @brief Check that airequipSelectedSlot is the indice of an existing slot in the aircraft
 * @note airequipSelectedSlot concerns only weapons and electronics
 * @sa aircraft Pointer to the aircraft
 */
static void AIM_CheckAirequipSelectedSlot (const aircraft_t *aircraft)
{
	switch (airequipID) {
	case AC_ITEM_AMMO:
	case AC_ITEM_WEAPON:
		if (airequipSelectedSlot >= aircraft->maxWeapons) {
			airequipSelectedSlot = 0;
			return;
		}
		break;
	case AC_ITEM_ELECTRONICS:
		if (airequipSelectedSlot >= aircraft->maxElectronics) {
			airequipSelectedSlot = 0;
			return;
		}
		break;
	}
}

/**
 * @brief Returns a pointer to the selected slot.
 * @param[in] aircraft Pointer to the aircraft
 * @return Pointer to the slot corresponding to airequipID
 */
aircraftSlot_t *AII_SelectAircraftSlot (aircraft_t *aircraft, const int airequipID)
{
	aircraftSlot_t *slot;

	AIM_CheckAirequipSelectedSlot(aircraft);
	switch (airequipID) {
	case AC_ITEM_SHIELD:
		slot = &aircraft->shield;
		break;
	case AC_ITEM_ELECTRONICS:
		slot = aircraft->electronics + airequipSelectedSlot;
		break;
	case AC_ITEM_AMMO:
	case AC_ITEM_WEAPON:
		slot = aircraft->weapons + airequipSelectedSlot;
		break;
	default:
		Com_Printf("AII_SelectAircraftSlot: Unknown airequipID: %i\n", airequipID);
		return NULL;
	}

	return slot;
}

/**
 * @brief Check that airequipSelectedZone is available
 * @sa slot Pointer to the slot
 */
static void AIM_CheckAirequipSelectedZone (aircraftSlot_t *slot)
{
	assert(slot);

	if (airequipSelectedZone == ZONE_AMMO && (airequipID < AC_ITEM_AMMO) && (airequipID > AC_ITEM_WEAPON)) {
		/* you can select ammo only for weapons and ammo */
		airequipSelectedZone = ZONE_MAIN;
	}

	/* You can choose an ammo only if a weapon has already been selected */
	if (airequipID >= AC_ITEM_AMMO && !slot->item) {
		airequipSelectedZone = ZONE_MAIN;
		switch (airequipID) {
		case AC_ITEM_AMMO:
			airequipID = AC_ITEM_WEAPON;
			break;
		default:
			Com_Printf("AIM_CheckAirequipSelectedZone: aircraftItemType_t must end with ammos !!!\n");
			return;
		}
	}
}

/**
 * @brief Get the technology of the item that is in current zone.
 * @note NULL if zone is empty.
 * @param[in] slot Pointer to the slot.
 */
static inline technology_t *AII_GetTechnologyToDisplay (const aircraftSlot_t const *slot)
{
	assert(slot);

	if (airequipSelectedZone == ZONE_MAIN) {
		if (!slot->item)
			return NULL;
		else if (slot->nextItem)
			return slot->nextItem->tech;
		else
			return slot->item->tech;
	} else if (airequipSelectedZone == ZONE_AMMO) {
		if (slot->ammo)
			return slot->ammo->tech;
		else
			return NULL;
	} else
		return NULL;
}

/**
 * @brief Returns the userfriendly name for craftitem types (shown in aircraft equip menu)
 */
static inline const char *AIM_AircraftItemtypeName (const int equiptype)
{
	switch (equiptype) {
	case AC_ITEM_WEAPON:
		return _("Weapons");
	case AC_ITEM_SHIELD:
		return _("Armour");
	case AC_ITEM_ELECTRONICS:
		return _("Items");
	case AC_ITEM_AMMO:
		/* ammo - only available from weapons */
		return _("Ammo");
	default:
		return _("Unknown");
	}
}

/**
 * @brief Update the list of item you can choose
 * @param[in] slot Pointer to aircraftSlot where items can be equiped
 * @param[in] tech Pointer to the technolgy to select if needed (NULL if no selected technology).
 */
static void AIM_UpdateAircraftItemList (const aircraftSlot_t *slot, const technology_t *tech)
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
 * @brief Highlight selected zone
 */
static void AIM_DrawSelectedZone (void)
{
	menuNode_t *node;

	node = MN_GetNodeByPath("aircraft_equip.airequip_zone_select1");
	if (airequipSelectedZone == ZONE_MAIN)
		MN_HideNode(node);
	else
		MN_UnHideNode(node);

	node = MN_GetNodeByPath("aircraft_equip.airequip_zone_select2");
	if (airequipSelectedZone == ZONE_AMMO)
		MN_HideNode(node);
	else
		MN_UnHideNode(node);
}
/**
 * @brief Draw only slots existing for this aircraft, and emphases selected one
 * @return[out] aircraft Pointer to the aircraft
 */
static void AIM_DrawAircraftSlots (const aircraft_t *aircraft)
{
	menuNode_t *node;
	int i, j;
	const aircraftSlot_t *slot;
	int max;

	/* initialise models Cvar */
	for (i = 0; i < 8; i++)
		Cvar_Set(va("mn_aircraft_item_model_slot%i", i), "");

	node = MN_GetNodeByPath("aircraft_equip.airequip_slot0");
	for (i = 0; node && i < AIR_POSITIONS_MAX; node = node->next) {
		if (strncmp(node->name, "airequip_slot", 13) != 0)
			continue;

		/* Default value */
		MN_HideNode(node);
		/* Draw available slots */
		switch (airequipID) {
		case AC_ITEM_AMMO:
		case AC_ITEM_WEAPON:
			max = aircraft->maxWeapons;
			slot = aircraft->weapons;
			break;
		case AC_ITEM_ELECTRONICS:
			max = aircraft->maxElectronics;
			slot = aircraft->electronics;
			break;
		/* do nothing for shield: there is only one slot */
		default:
			continue;
		}
		for (j = 0; j < max; j++, slot++) {
			/* check if one of the aircraft slots is at this position */
			if (slot->pos == i) {
				MN_UnHideNode(node);
				/* draw in white if this is the selected slot */
				if (j == airequipSelectedSlot) {
					Vector2Set(node->texl, 64, 0);
					Vector2Set(node->texh, 128, 64);
				} else {
					Vector2Set(node->texl, 0, 0);
					Vector2Set(node->texh, 64, 64);
				}
				if (slot->item) {
					assert(slot->item->tech);
					Cvar_Set(va("mn_aircraft_item_model_slot%i", i), slot->item->tech->mdl);
				} else
					Cvar_Set(va("mn_aircraft_item_model_slot%i", i), "");
			}
		}
		i++;
	}
}

/**
 * @brief Write in red the text in zone ammo (zone 2)
 * @sa AIM_NoEmphazeAmmoSlotText
 * @note This is intended to show the player that there is no ammo in his aircraft
 */
static inline void AIM_EmphazeAmmoSlotText (void)
{
	menuNode_t *node = MN_GetNodeByPath("aircraft_equip.airequip_text_zone2");
	if (!node)
		return;
	VectorSet(node->color, 1.0f, .0f, .0f);
}

/**
 * @brief Write in white the text in zone ammo (zone 2)
 * @sa AIM_EmphazeAmmoSlotText
 * @note This is intended to revert effects of AIM_EmphazeAmmoSlotText
 */
static inline void AIM_NoEmphazeAmmoSlotText (void)
{
	menuNode_t *node = MN_GetNodeByPath("aircraft_equip.airequip_text_zone2");
	if (!node)
		return;
	VectorSet(node->color, 1.0f, 1.0f, 1.0f);
}


/**
 * @brief Fills the weapon and shield list of the aircraft equip menu
 * @sa AIM_AircraftEquipMenuClick_f
 */
void AIM_AircraftEquipMenuUpdate_f (void)
{
	static char smallbuffer1[256];
	static char smallbuffer2[128];
	int type;
	menuNode_t *node;
	aircraft_t *aircraft;
	aircraftSlot_t *slot;
	base_t *base = B_GetCurrentSelectedBase();

	if (!base)
		return;

	/* don't let old links appear on this menu */
	MN_ResetData(TEXT_STANDARD);
	MN_ResetData(TEXT_AIREQUIP_1);
	MN_ResetData(TEXT_AIREQUIP_2);
	MN_ResetData(TEXT_LIST);

	if (Cmd_Argc() != 2 || noparams) {
		if (airequipID == -1) {
			Com_Printf("Usage: %s <num>\n", Cmd_Argv(0));
			return;
		}
		AIM_CheckAirequipID();
	} else {
		type = atoi(Cmd_Argv(1));
		switch (type) {
		case AC_ITEM_AMMO:
			if (airequipID == AC_ITEM_WEAPON)
				airequipID = AC_ITEM_AMMO;
			break;
		case AC_ITEM_ELECTRONICS:
		case AC_ITEM_SHIELD:
			airequipID = type;
			MN_ExecuteConfunc("airequip_zone2_off");
			break;
		case AC_ITEM_WEAPON:
			airequipID = type;
			MN_ExecuteConfunc("airequip_zone2_on");
			break;
		default:
			airequipID = AC_ITEM_WEAPON;
			break;
		}
	}

	/* Reset value of noparams */
	noparams = qfalse;

	node = MN_GetNodeByPath("aircraft_equip.aircraftequip");

	/* we are not in the aircraft menu */
	if (!node) {
		Com_DPrintf(DEBUG_CLIENT, "AIM_AircraftEquipMenuUpdate_f: Error - node aircraftequip not found\n");
		return;
	}

	aircraft = base->aircraftCurrent;

	assert(aircraft);
	assert(node);

	/* Check that airequipSelectedSlot corresponds to an existing slot for this aircraft */
	AIM_CheckAirequipSelectedSlot(aircraft);

	/* Select slot */
	slot = AII_SelectAircraftSlot(aircraft, airequipID);

	/* Check that the selected zone is OK */
	AIM_CheckAirequipSelectedZone(slot);

	/* Fill the list of item you can equip your aircraft with */
	AIM_UpdateAircraftItemList(slot, AII_GetTechnologyToDisplay(slot));

	Cvar_Set("mn_equip_itemtype_name", AIM_AircraftItemtypeName(airequipID));

	/* First slot: item currently assigned */
	if (!slot->item) {
		Com_sprintf(smallbuffer1, sizeof(smallbuffer1), "%s", _("No item assigned.\n"));
		Q_strcat(smallbuffer1, va(_("This slot is for %s or smaller items."),
			AII_WeightToName(slot->size)), sizeof(smallbuffer1));
	} else {
		/* Print next item if we are removing item currently installed and a new item has been added. */
		assert(slot->item->tech);
		Com_sprintf(smallbuffer1, sizeof(smallbuffer1), "%s\n",
			slot->nextItem ? _(slot->nextItem->tech->name) : _(slot->item->tech->name));
		if (!slot->installationTime)
			Q_strcat(smallbuffer1, _("This item is functional.\n"), sizeof(smallbuffer1));
		else if (slot->installationTime > 0)
			Q_strcat(smallbuffer1, va(_("This item will be installed in %i hours.\n"),
				slot->installationTime), sizeof(smallbuffer1));
		else if (slot->nextItem) {
			Q_strcat(smallbuffer1, va(_("%s will be removed in %i hours.\n"), _(slot->item->tech->name),
				- slot->installationTime), sizeof(smallbuffer1));
			Q_strcat(smallbuffer1, va(_("%s will be installed in %i hours.\n"), _(slot->nextItem->tech->name),
				slot->nextItem->craftitem.installationTime - slot->installationTime), sizeof(smallbuffer1));
		} else
			Q_strcat(smallbuffer1, va(_("This item will be removed in %i hours.\n"),
				-slot->installationTime), sizeof(smallbuffer1));
	}
	MN_RegisterText(TEXT_AIREQUIP_1, smallbuffer1);

	/* Second slot: ammo slot (only used for weapons) */
	if ((airequipID == AC_ITEM_WEAPON || airequipID == AC_ITEM_AMMO) && slot->item) {
		if (!slot->ammo) {
			AIM_EmphazeAmmoSlotText();
			Com_sprintf(smallbuffer2, sizeof(smallbuffer2), "%s", _("No ammo assigned to this weapon."));
		} else {
			AIM_NoEmphazeAmmoSlotText();
			assert(slot->ammo->tech);
			if (slot->nextAmmo) {
				Q_strncpyz(smallbuffer2, _(slot->nextAmmo->tech->name), sizeof(smallbuffer2));
				/* inform player that base missile are unlimited */
				if (slot->nextAmmo->craftitem.unlimitedAmmo)
					Q_strcat(smallbuffer2, _(" (unlimited ammos)"), sizeof(smallbuffer2));
			} else {
				Q_strncpyz(smallbuffer2, _(slot->ammo->tech->name), sizeof(smallbuffer2));
				/* inform player that base missile are unlimited */
				if (slot->ammo->craftitem.unlimitedAmmo)
					Q_strcat(smallbuffer2, _(" (unlimited ammos)"), sizeof(smallbuffer2));
			}
		}
	} else
		*smallbuffer2 = '\0';

	MN_RegisterText(TEXT_AIREQUIP_2, smallbuffer2);

	/* Draw existing slots for this aircraft */
	AIM_DrawAircraftSlots(aircraft);

	/* Draw selected zone */
	AIM_DrawSelectedZone();
}

/**
 * @brief Select the current slot you want to assign the item to.
 * @note This function is only for aircraft and not far bases.
 */
void AIM_AircraftEquipSlotSelect_f (void)
{
	int i, pos;
	aircraft_t *aircraft;
	base_t *base = B_GetCurrentSelectedBase();

	if (!base)
		return;

	if (Cmd_Argc() < 2) {
		Com_Printf("Usage: %s <arg>\n", Cmd_Argv(0));
		return;
	}

	aircraft = base->aircraftCurrent;
	assert(aircraft);

	pos = atoi(Cmd_Argv(1));

	airequipSelectedSlot = ZONE_NONE;

	/* select the slot corresponding to pos, and set airequipSelectedSlot to this slot */
	switch (airequipID) {
	case AC_ITEM_ELECTRONICS:
		/* electronics selected */
		for (i = 0; i < aircraft->maxElectronics; i++) {
			if (aircraft->electronics[i].pos == pos) {
				airequipSelectedSlot = i;
				break;
			}
		}
		if (i == aircraft->maxElectronics)
			Com_Printf("this slot hasn't been found in aircraft electronics slots\n");
		break;
	case AC_ITEM_AMMO:
	case AC_ITEM_WEAPON:
		/* weapon selected */
		for (i = 0; i < aircraft->maxWeapons; i++) {
			if (aircraft->weapons[i].pos == pos) {
				airequipSelectedSlot = i;
				break;
			}
		}
		if (i == aircraft->maxWeapons)
			Com_Printf("this slot hasn't been found in aircraft weapon slots\n");
		break;
	default:
		Com_Printf("AIM_AircraftEquipSlotSelect_f : only weapons and electronics have several slots\n");
	}

	/* Update menu after changing slot */
	noparams = qtrue; /* used for AIM_AircraftEquipMenuUpdate_f */
	AIM_AircraftEquipMenuUpdate_f();
}

/**
 * @brief Select the current zone you want to assign the item to.
 */
void AIM_AircraftEquipZoneSelect_f (void)
{
	int zone;
	aircraft_t *aircraft;
	aircraftSlot_t *slot;
	base_t *base = B_GetCurrentSelectedBase();

	if (!base)
		return;

	if (strcmp(MN_GetActiveMenuName(), "aircraft_equip"))
		return;

	if (Cmd_Argc() < 2) {
		Com_Printf("Usage: %s <arg>\n", Cmd_Argv(0));
		return;
	}

	zone = atoi(Cmd_Argv(1));

	aircraft = base->aircraftCurrent;
	assert(aircraft);
	/* Select slot */
	slot = AII_SelectAircraftSlot(aircraft, airequipID);

	/* ammos are only available for weapons */
	switch (airequipID) {
	/* a weapon was selected - select ammo type corresponding to this weapon */
	case AC_ITEM_WEAPON:
		if (zone == ZONE_AMMO) {
			if (slot->item)
				airequipID = AC_ITEM_AMMO;
		}
		break;
	/* an ammo was selected - select weapon type corresponding to this ammo */
	case AC_ITEM_AMMO:
		if (zone != ZONE_AMMO)
			airequipID = AC_ITEM_WEAPON;
		break;
	default :
		/* ZONE_AMMO is not available for electronics and shields */
		if (zone == ZONE_AMMO)
			return;
	}
	airequipSelectedZone = zone;

	/* Fill the list of item you can equip your aircraft with */
	AIM_UpdateAircraftItemList(slot, AII_GetTechnologyToDisplay(slot));

	/* Check that the selected zone is OK */
	AIM_CheckAirequipSelectedZone(slot);

	/* Draw selected zone */
	AIM_DrawSelectedZone();
}

/**
 * @brief Add selected item to current zone.
 * @note Called from airequip menu
 * @sa aircraftItemType_t
 */
void AIM_AircraftEquipAddItem_f (void)
{
	int zone;
	aircraftSlot_t *slot;
	aircraft_t *aircraft = NULL;
	base_t *base = B_GetCurrentSelectedBase();

	if (Cmd_Argc() < 2) {
		Com_Printf("Usage: %s <arg>\n", Cmd_Argv(0));
		return;
	}

	zone = atoi(Cmd_Argv(1));

	/* proceed only if an item has been selected */
	if (!airequipSelectedTechnology)
		return;

	assert(base);
	aircraft = base->aircraftCurrent;
	assert(aircraft);
	base = aircraft->homebase;	/* we need to know where items will be removed */
	slot = AII_SelectAircraftSlot(aircraft, airequipID);

	/* the clicked button doesn't correspond to the selected zone */
	if (zone != airequipSelectedZone)
		return;

	/* check if the zone exists */
	if (zone >= ZONE_MAX)
		return;

	/* update the new item to slot */

	switch (zone) {
	case ZONE_MAIN:
		if (!slot->nextItem) {
			/* we add the weapon, shield, item if slot is free or the installation of current item just began */
			if (!slot->item || (slot->item && slot->installationTime == slot->item->craftitem.installationTime)) {
				AII_RemoveItemFromSlot(base, slot, qfalse);
				AII_AddItemToSlot(base, airequipSelectedTechnology, slot, qfalse); /* Aircraft stats are updated below */
				AIM_AutoAddAmmo(slot);
				break;
			} else if (slot->item == AII_GetAircraftItemByID(airequipSelectedTechnology->provides)) {
				/* the added item is the same than the one in current slot */
				if (slot->installationTime == -slot->item->craftitem.installationTime) {
					/* player changed his mind: he just want to re-add the item he just removed */
					slot->installationTime = 0;
					break;
				} else if (!slot->installationTime) {
					/* player try to add a weapon he already have: just skip */
					return;
				}
			} else {
				/* We start removing current item in slot, and the selected item will be installed afterwards */
				slot->installationTime = -slot->item->craftitem.installationTime;
				/* more below */
			}
		} else {
			/* remove weapon and ammo of next item */
			AII_RemoveItemFromSlot(base, slot, qfalse);
			/* more below */
		}

		/* we change the weapon, shield, item, or base defence that will be installed AFTER the removal
		 * of the one in the slot atm */
		AII_AddItemToSlot(base, airequipSelectedTechnology, slot, qtrue);
		AIM_AutoAddAmmo(slot);
		break;
	case ZONE_AMMO:
		/* we can change ammo only if the selected item is an ammo (for weapon or base defence system) */
		if (airequipID >= AC_ITEM_AMMO) {
			AII_AddAmmoToSlot(base, airequipSelectedTechnology, slot);
		}
		break;
	default:
		/* Zone higher than ZONE_AMMO shouldn't exist */
		return;
	}

	/* Update the values of aircraft stats (just in case an item has an installationTime of 0) */
	AII_UpdateAircraftStats(aircraft);

	noparams = qtrue; /* used for AIM_AircraftEquipMenuUpdate_f */
	AIM_AircraftEquipMenuUpdate_f();
}

/**
 * @brief Delete an object from a zone.
 */
void AIM_AircraftEquipDeleteItem_f (void)
{
	int zone;
	aircraftSlot_t *slot;
	aircraft_t *aircraft = NULL;
	base_t *base = B_GetCurrentSelectedBase();

	if (Cmd_Argc() < 2) {
		Com_Printf("Usage: %s <arg>\n", Cmd_Argv(0));
		return;
	}
	zone = atoi(Cmd_Argv(1));

	assert(base);
	aircraft = base->aircraftCurrent;
	assert(aircraft);
	slot = AII_SelectAircraftSlot(aircraft, airequipID);

	/* no item in slot: nothing to remove */
	if (!slot->item)
		return;

	/* update the new item to slot */

	switch (zone) {
	case ZONE_MAIN:
		if (!slot->nextItem) {
			/* we change the weapon, shield, item, or base defence that is already in the slot */
			/* if the item has been installed since less than 1 hour, you don't need time to remove it */
			if (slot->installationTime < slot->item->craftitem.installationTime) {
				slot->installationTime = -slot->item->craftitem.installationTime;
				AII_RemoveItemFromSlot(base, slot, qtrue); /* we remove only ammo, not item */
			} else {
				AII_RemoveItemFromSlot(base, slot, qfalse); /* we remove weapon and ammo */
			}
			/* aircraft stats are updated below */
		} else {
			/* we change the weapon, shield, item, or base defence that will be installed AFTER the removal
			 * of the one in the slot atm */
			AII_RemoveItemFromSlot(base, slot, qfalse); /* we remove weapon and ammo */
			/* if you canceled next item for less than 1 hour, previous item is still functional */
			if (slot->installationTime == -slot->item->craftitem.installationTime) {
				slot->installationTime = 0;
			}
		}
		break;
	case ZONE_AMMO:
		/* we can change ammo only if the selected item is an ammo (for weapon or base defence system) */
		if (airequipID >= AC_ITEM_AMMO) {
			AII_RemoveItemFromSlot(base, slot, qtrue);
		}
		break;
	default:
		/* Zone higher than ZONE_AMMO shouldn't exist */
		return;
	}

	/* Update the values of aircraft stats */
	AII_UpdateAircraftStats(aircraft);

	noparams = qtrue; /* used for AIM_AircraftEquipMenuUpdate_f */
	AIM_AircraftEquipMenuUpdate_f();
}

/**
 * @brief Set airequipSelectedTechnology to the technology of current selected aircraft item.
 * @sa AIM_AircraftEquipMenuUpdate_f
 */
void AIM_AircraftEquipMenuClick_f (void)
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

void AIM_ResetEquipAircraftMenu (void)
{
	/* Reset all used menu variables/nodes. */
	Cvar_Set("mn_itemname", "");
	Cvar_Set("mn_item", "");
	Cvar_Set("mn_upmodel_top", "");

	MN_RegisterText(TEXT_STANDARD, "");
}

/**
 * @brief Selects the next aircraft item category
 */
static void AIM_NextItemtype_f (void)
{
	airequipID++;

	if (airequipID > AC_ITEM_ELECTRONICS)
		airequipID = AC_ITEM_WEAPON;
	else if (airequipID < AC_ITEM_WEAPON)
		airequipID = AC_ITEM_ELECTRONICS;

	/* you should never be able to reach ammo by using this button */
	airequipSelectedZone = ZONE_MAIN;

	Cmd_ExecuteString(va("airequip_updatemenu %d;", airequipID));
}

/**
 * @brief Selects the previous aircraft item category
 */
static void AIM_PreviousItemtype_f (void)
{
	airequipID--;

	if (airequipID > AC_ITEM_ELECTRONICS)
		airequipID = AC_ITEM_WEAPON;
	else if (airequipID < AC_ITEM_WEAPON)
		airequipID = AC_ITEM_ELECTRONICS;

	/* you should never be able to reach ammo by using this button */
	airequipSelectedZone = ZONE_MAIN;

	Cmd_ExecuteString(va("airequip_updatemenu %d;", airequipID));
}


/***************************
 *	Base Defences
 ***************************/


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
	MN_ResetData(TEXT_STANDARD);

	/* base or installation should not be NULL because we are in the menu of this base or installation */
	if (!base && !installation)
		return;

	/* base and installation should not both be set. This function requires one or the other set. */
	if (base && installation) {
		Com_Printf("BDEF_BaseDefenseMenuUpdate_f: both the base and installation are set.  This shouldn't happen: you shouldn't be in this function.\n");
		return;
	}

	if (installation) {
		/** Every slot aims the same target */
		if (installation->numBatteries && installation->batteries[0].target) {
			Cvar_ForceSet("mn_target", UFO_AircraftToIDOnGeoscape(installation->batteries[0].target));
		} else
			Cvar_ForceSet("mn_target", _("None"));
		Cmd_ExecuteString(va("setautofire %i", installation->batteries[0].autofire));
	} else if (base) {
		/** Every slot aims the same target */
		if (base->numBatteries && base->batteries[0].target) {
			Cvar_ForceSet("mn_target", UFO_AircraftToIDOnGeoscape(base->batteries[0].target));
		} else
			Cvar_ForceSet("mn_target", _("None"));
		Cmd_ExecuteString(va("setautofire %i", base->batteries[0].autofire));
	} else
		Cvar_ForceSet("mn_target", _("None"));

	/* Check if we can change to laser or missile */
	/** @todo make the laser depend on laser defence tech - once we have it */
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
			AIM_UpdateAircraftItemList(&installation->batteries[0].slot, NULL);
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
			AIM_UpdateAircraftItemList(&base->batteries[0].slot, NULL);
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
			AIM_UpdateAircraftItemList(&base->lasers[0].slot, NULL);
			for (i = 0; i < base->numLasers; i++) {
				if (!base->lasers[i].slot.item) {
					Com_sprintf(defBuffer, lengthof(defBuffer), _("Slot %i:\tempty"), i);
					LIST_AddString(&slotList, defBuffer);
				} else {
					const aircraftSlot_t *slot = &base->lasers[i].slot ;
					char status[MAX_VAR];
					if (!slot->installationTime)
						Q_strcat(status, _("Working"), sizeof(status));
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

void AIM_InitCallbacks (void)
{
	Cmd_AddCommand("mn_next_equiptype", AIM_NextItemtype_f, "Shows the next aircraft equip category.");
	Cmd_AddCommand("mn_prev_equiptype", AIM_PreviousItemtype_f, "Shows the previous aircraft equip category.");
	Cmd_AddCommand("airequip_zone_select", AIM_AircraftEquipZoneSelect_f, NULL);

	Cmd_AddCommand("airequip_updatemenu", AIM_AircraftEquipMenuUpdate_f, "Init function for the aircraft equip menu");
	Cmd_AddCommand("airequip_list_click", AIM_AircraftEquipMenuClick_f, NULL);
	Cmd_AddCommand("airequip_slot_select", AIM_AircraftEquipSlotSelect_f, NULL);
	Cmd_AddCommand("airequip_add_item", AIM_AircraftEquipAddItem_f, "Add item to slot");
	Cmd_AddCommand("airequip_del_item", AIM_AircraftEquipDeleteItem_f, "Remove item from slot");

	Cmd_AddCommand("add_battery", BDEF_AddBattery_f, "Add a new battery to base");
	Cmd_AddCommand("remove_battery", BDEF_RemoveBattery_f, "Remove a battery from base");
	Cmd_AddCommand("basedef_updatemenu", BDEF_BaseDefenseMenuUpdate_f, "Inits base defence menu");
	Cmd_AddCommand("basedef_list_click", AIM_AircraftEquipMenuClick_f, NULL);
	Cmd_AddCommand("basedef_additem", BDEF_AddItem_f, "Add item to slot");
	Cmd_AddCommand("basedef_removeitem", BDEF_RemoveItem_f, "Remove item from slot");
	Cmd_AddCommand("basedef_autofire", BDEF_ChangeAutoFire, "Change autofire option for selected defence system");
}

void AIM_ShutdownCallbacks (void)
{
	Cmd_RemoveCommand("airequip_updatemenu");
	Cmd_RemoveCommand("airequip_list_click");
	Cmd_RemoveCommand("airequip_slot_select");
	Cmd_RemoveCommand("airequip_add_item");
	Cmd_RemoveCommand("airequip_del_item");

	Cmd_RemoveCommand("add_battery");
	Cmd_RemoveCommand("remove_battery");
	Cmd_RemoveCommand("basedef_updatemenu");
	Cmd_RemoveCommand("basedef_list_click");
	Cmd_RemoveCommand("basedef_additem");
	Cmd_RemoveCommand("basedef_removeitem");
	Cmd_RemoveCommand("basedef_autofire");

	Cmd_RemoveCommand("mn_next_equiptype");
	Cmd_RemoveCommand("mn_prev_equiptype");
	Cmd_RemoveCommand("airequip_zone_select");
}
