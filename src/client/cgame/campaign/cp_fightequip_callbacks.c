/**
 * @file cp_fightequip_callbacks.c
 * @brief Header file for menu callback functions used for base and aircraft equip menu
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
#include "../../ui/ui_main.h" /* UI_ExecuteConfunc - otherwise only ui_data.h would be needed */
#include "cp_campaign.h"
#include "cp_fightequip_callbacks.h"
#include "cp_mapfightequip.h"
#include "cp_ufo.h"

static int airequipID = -1;				/**< value of aircraftItemType_t that defines what item we are installing. */

static int airequipSelectedZone = ZONE_NONE;		/**< Selected zone in equip menu */
static int airequipSelectedSlot = ZONE_NONE;			/**< Selected slot in equip menu */
static technology_t *aimSelectedTechnology = NULL;		/**< Selected technology in equip menu */

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
 * @param[in] type Type of slot we want to select
 * @note also used by BDEF_ functions
 * @return Pointer to the slot corresponding to airequipID
 */
aircraftSlot_t *AII_SelectAircraftSlot (aircraft_t *aircraft, aircraftItemType_t type)
{
	aircraftSlot_t *slot;

	AIM_CheckAirequipSelectedSlot(aircraft);
	switch (type) {
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
		Com_Printf("AII_SelectAircraftSlot: Unknown airequipID: %i\n", type);
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

	if (airequipSelectedZone == ZONE_AMMO && airequipID < AC_ITEM_AMMO && airequipID > AC_ITEM_WEAPON) {
		/* you can select ammo only for weapons and ammo */
		airequipSelectedZone = ZONE_MAIN;
	}

	/* You can choose an ammo only if a weapon has already been selected */
	if (airequipID >= AC_ITEM_AMMO && !slot->item) {
		airequipSelectedZone = ZONE_MAIN;
	}
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
 * @return @c true if the technology is available and matches the filter
 */
static qboolean AIM_CrafttypeFilter (const base_t *base, aircraftItemType_t filterType, const technology_t *tech)
{
	const objDef_t *item;
	if (!base)
		return qfalse;

	if (!RS_IsResearched_ptr(tech))
		return qfalse;

	item = INVSH_GetItemByID(tech->provides);
	if (!item)
		return qfalse;
	if (item->isVirtual)
		return qfalse;
	if (!B_BaseHasItem(base, item))
		return qfalse;

	/* filter by type: special case for ammo because more than 1 type is an ammo type */
	if (filterType != AC_ITEM_AMMO) {
		if (item->craftitem.type != filterType)
			return qfalse;
	} else {
		if (item->craftitem.type < AC_ITEM_AMMO)
			return qfalse;
	}

	/* you can't install an item that does not have an installation time (alien item)
	 * except for ammo which does not have installation time */
	if (item->craftitem.installationTime == -1 && filterType >= AC_ITEM_AMMO)
		return qfalse;

	return qtrue;
}

/**
 * @brief Update the list of item you can choose
 * @param[in] slot Pointer to aircraftSlot where items can be equiped
 */
static void AIM_UpdateAircraftItemList (const aircraftSlot_t *slot)
{
	linkedList_t *amountList = NULL;
	technology_t **techList;
	technology_t **currentTech;
	const base_t *base = slot->aircraft->homebase;
	int count = 0;
	uiNode_t *AIM_items = NULL;

	/* Add all items corresponding to airequipID to list */
	techList = AII_GetCraftitemTechsByType(airequipID);

	/* Count only those which are researched to buffer */
	currentTech = techList;
	while (*currentTech) {
		if (AIM_CrafttypeFilter(base, airequipID, *currentTech))
			count++;
		currentTech++;
	}

	/* List only those which are researched to buffer */
	currentTech = techList;
	while (*currentTech) {
		if (AIM_CrafttypeFilter(base, airequipID, *currentTech)) {
			uiNode_t *option;
			const objDef_t *item = INVSH_GetItemByID((*currentTech)->provides);
			const int amount = B_ItemInBase(item, base);

			LIST_AddString(&amountList, va("%d", amount));
			option = UI_AddOption(&AIM_items, (*currentTech)->name, _((*currentTech)->name), va("%d", (*currentTech)->idx));
			if (!AIM_SelectableCraftItem(slot, *currentTech))
				option->disabled = qtrue;
		}
		currentTech++;
	}

	UI_RegisterOption(TEXT_LIST, AIM_items);
	UI_RegisterLinkedListText(TEXT_LIST2, amountList);
}

/**
 * @brief Draw only slots existing for this aircraft, and emphases selected one
 * @return[out] aircraft Pointer to the aircraft
 */
static void AIM_DrawAircraftSlots (const aircraft_t *aircraft)
{
	itemPos_t i;

	/* initialise model cvars */
	for (i = 0; i < AIR_POSITIONS_MAX; i++)
		Cvar_Set(va("mn_aircraft_item_model_slot%i", i), "");

	for (i = 0; i < AIR_POSITIONS_MAX; i++) {
		const aircraftSlot_t *slot;
		int max, j;

		/* Default value */
		UI_ExecuteConfunc("airequip_display_slot %i 0", i);

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
				/* draw in white if this is the selected slot */
				if (j == airequipSelectedSlot) {
					UI_ExecuteConfunc("airequip_display_slot %i 2", i);
				} else {
					UI_ExecuteConfunc("airequip_display_slot %i 1", i);
				}
				if (slot->item) {
					Cvar_Set(va("mn_aircraft_item_model_slot%i", i), RS_GetTechForItem(slot->item)->mdl);
				} else
					Cvar_Set(va("mn_aircraft_item_model_slot%i", i), "");
			}
		}
	}
}

/**
 * @brief Write in red the text in zone ammo (zone 2)
 * @sa AIM_NoEmphazeAmmoSlotText
 * @note This is intended to show the player that there is no ammo in his aircraft
 */
static inline void AIM_EmphazeAmmoSlotText (void)
{
	UI_ExecuteConfunc("airequip_zone2_color \"1 0 0 1\"");
}

/**
 * @brief Write in white the text in zone ammo (zone 2)
 * @sa AIM_EmphazeAmmoSlotText
 * @note This is intended to revert effects of AIM_EmphazeAmmoSlotText
 */
static inline void AIM_NoEmphazeAmmoSlotText (void)
{
	UI_ExecuteConfunc("airequip_zone2_color \"1 1 1 1\"");
}

static void AIM_AircraftEquipMenuUpdate (void)
{
	static char smallbuffer1[256];
	static char smallbuffer2[128];
	const char *typeName;
	aircraft_t *aircraft;
	aircraftSlot_t *slot;
	base_t *base = B_GetCurrentSelectedBase();

	if (!base)
		return;

	/* don't let old links appear on this menu */
	UI_ResetData(TEXT_AIREQUIP_1);
	UI_ResetData(TEXT_AIREQUIP_2);
	UI_ResetData(TEXT_ITEMDESCRIPTION);
	UI_ResetData(TEXT_LIST);

	aircraft = base->aircraftCurrent;

	assert(aircraft);

	/* Check that airequipSelectedSlot corresponds to an existing slot for this aircraft */
	AIM_CheckAirequipSelectedSlot(aircraft);

	/* Select slot */
	slot = AII_SelectAircraftSlot(aircraft, airequipID);

	/* Check that the selected zone is OK */
	AIM_CheckAirequipSelectedZone(slot);

	/* Fill the list of item you can equip your aircraft with */
	AIM_UpdateAircraftItemList(slot);

	Cvar_Set("mn_equip_itemtype_name", AIM_AircraftItemtypeName(airequipID));
	switch (airequipID) {
	case AC_ITEM_ELECTRONICS:
		typeName = "item";
		break;
	case AC_ITEM_SHIELD:
		typeName = "armour";
		break;
	case AC_ITEM_AMMO:
		typeName = "ammo";
		break;
	case AC_ITEM_WEAPON:
		typeName = "weapon";
		break;
	default:
		typeName = "unknown";
		break;
	}
	Cvar_Set("mn_equip_itemtype", typeName);

	/* First slot: item currently assigned */
	if (!slot->item) {
		Com_sprintf(smallbuffer1, sizeof(smallbuffer1), "%s", _("No item assigned.\n"));
		Q_strcat(smallbuffer1, va(_("This slot is for %s or smaller items."),
			AII_WeightToName(slot->size)), sizeof(smallbuffer1));
	} else {
		technology_t *itemTech = RS_GetTechForItem(slot->item);
		technology_t *nextItemTech = slot->nextItem ? RS_GetTechForItem(slot->nextItem) : NULL;
		/* Print next item if we are removing item currently installed and a new item has been added. */
		Com_sprintf(smallbuffer1, sizeof(smallbuffer1), "%s\n", slot->nextItem ? _(nextItemTech->name) : _(itemTech->name));
		if (!slot->installationTime) {
			Q_strcat(smallbuffer1, _("This item is functional.\n"), sizeof(smallbuffer1));
		} else if (slot->installationTime > 0) {
			Q_strcat(smallbuffer1, va(_("This item will be installed in %i hours.\n"),
				slot->installationTime), sizeof(smallbuffer1));
		} else if (slot->nextItem) {
			Q_strcat(smallbuffer1, va(_("%s will be removed in %i hours.\n"), _(itemTech->name),
				- slot->installationTime), sizeof(smallbuffer1));
			Q_strcat(smallbuffer1, va(_("%s will be installed in %i hours.\n"), _(nextItemTech->name),
				slot->nextItem->craftitem.installationTime - slot->installationTime), sizeof(smallbuffer1));
		} else {
			Q_strcat(smallbuffer1, va(_("This item will be removed in %i hours.\n"),
				-slot->installationTime), sizeof(smallbuffer1));
		}
	}
	UI_RegisterText(TEXT_AIREQUIP_1, smallbuffer1);

	/* Second slot: ammo slot (only used for weapons) */
	if ((airequipID == AC_ITEM_WEAPON || airequipID == AC_ITEM_AMMO) && slot->item) {
		if (!slot->ammo) {
			AIM_EmphazeAmmoSlotText();
			Com_sprintf(smallbuffer2, sizeof(smallbuffer2), "%s", _("No ammo assigned to this weapon."));
		} else {
			const objDef_t *ammo = slot->nextAmmo ? slot->nextAmmo : slot->ammo;
			const technology_t *tech = RS_GetTechForItem(ammo);
			AIM_NoEmphazeAmmoSlotText();
			if (!ammo->isVirtual)
				Q_strncpyz(smallbuffer2, _(tech->name), sizeof(smallbuffer2));
			else
				Q_strncpyz(smallbuffer2, _("No ammo needed"), sizeof(smallbuffer2));
		}
	} else
		*smallbuffer2 = '\0';

	UI_RegisterText(TEXT_AIREQUIP_2, smallbuffer2);

	/* Draw existing slots for this aircraft */
	AIM_DrawAircraftSlots(aircraft);
}

#define AIM_LOADING_OK							0
#define AIM_LOADING_NOSLOTSELECTED				1
#define AIM_LOADING_NOTECHNOLOGYSELECTED		2
#define AIM_LOADING_ALIENTECH					3
#define AIM_LOADING_TECHNOLOGYNOTRESEARCHED		4
#define AIM_LOADING_TOOHEAVY 					5
#define AIM_LOADING_UNKNOWNPROBLEM				6
#define AIM_LOADING_NOWEAPON					7
#define AIM_LOADING_NOTUSABLEWITHWEAPON			8

/**
 * @todo It is a generic function, we can move it into cp_mapfightequip.c
 * @param[in] slot Pointer to an aircraft slot (can be base/installation too)
 * @param[in] tech Pointer to the technology to test
 * @return The status of the technology versus the slot
 */
static int AIM_CheckTechnologyIntoSlot (const aircraftSlot_t *slot, const technology_t *tech)
{
	const objDef_t *item;

	if (!tech)
		return AIM_LOADING_NOTECHNOLOGYSELECTED;

	if (!slot)
		return AIM_LOADING_NOSLOTSELECTED;

	if (!RS_IsResearched_ptr(tech))
		return AIM_LOADING_TECHNOLOGYNOTRESEARCHED;

	item = INVSH_GetItemByID(tech->provides);
	if (!item)
		return AIM_LOADING_NOTECHNOLOGYSELECTED;

	if (item->craftitem.type >= AC_ITEM_AMMO) {
		const objDef_t *weapon = slot->item;
		int k;
		if (slot->nextItem != NULL)
			weapon = slot->nextItem;

		if (weapon == NULL)
			return AIM_LOADING_NOWEAPON;

		/* Is the ammo is usable with the slot */
		for (k = 0; k < weapon->numAmmos; k++) {
			const objDef_t *usable = weapon->ammos[k];
			if (usable && item->idx == usable->idx)
				break;
		}
		if (k >= weapon->numAmmos)
			return AIM_LOADING_NOTUSABLEWITHWEAPON;

#if 0
		/** @todo This only works for ammo that is useable in exactly one weapon
		 * check the weap_idx array and not only the first value */
		if (!slot->nextItem && item->weapons[0] != slot->item)
			return AIM_LOADING_UNKNOWNPROBLEM;

		/* are we trying to change ammos for nextItem? */
		if (slot->nextItem && item->weapons[0] != slot->nextItem)
			return AIM_LOADING_UNKNOWNPROBLEM;
#endif
	}

	/* you can install an item only if its weight is small enough for the slot */
	if (AII_GetItemWeightBySize(item) > slot->size)
		return AIM_LOADING_TOOHEAVY;

	/* you can't install an item that you don't possess
	 * virtual ammo don't need to be possessed
	 * installations always have weapon and ammo */
	if (slot->aircraft) {
		if (!B_BaseHasItem(slot->aircraft->homebase, item))
			return AIM_LOADING_UNKNOWNPROBLEM;
	} else if (slot->base) {
		if (!B_BaseHasItem(slot->base, item))
			return AIM_LOADING_UNKNOWNPROBLEM;
	}

	/* you can't install an item that does not have an installation time (alien item)
	 * except for ammo which does not have installation time */
	if (item->craftitem.installationTime == -1 && slot->type < AC_ITEM_AMMO)
		return AIM_LOADING_ALIENTECH;

	return AIM_LOADING_OK;
}

/**
 * @brief Update the item description according to the tech and the slot selected
 */
static void AIM_UpdateItemDescription (qboolean fromList, qboolean fromSlot)
{
	int status;
	aircraft_t *aircraft;
	aircraftSlot_t *slot;
	base_t *base = B_GetCurrentSelectedBase();
	assert(base);

	aircraft = base->aircraftCurrent;
	assert(aircraft);
	slot = AII_SelectAircraftSlot(aircraft, airequipID);

	/* update mini ufopedia */
	/** @todo we should clone the text, and not using the ufopedia text */
	if (fromList)
		UP_AircraftItemDescription(INVSH_GetItemByIDSilent(aimSelectedTechnology ? aimSelectedTechnology->provides : NULL));
	else if (fromSlot) {
		if (airequipID == AC_ITEM_AMMO)
			UP_AircraftItemDescription(slot->ammo);
		else
			UP_AircraftItemDescription(slot->item);
	}

	/* update status */
	status = AIM_CheckTechnologyIntoSlot(slot, aimSelectedTechnology);
	switch (status) {
	case AIM_LOADING_NOSLOTSELECTED:
		Cvar_Set("mn_aircraft_item_warning", _("No slot selected."));
		break;
	case AIM_LOADING_NOTECHNOLOGYSELECTED:
		Cvar_Set("mn_aircraft_item_warning", _("No item selected."));
		break;
	case AIM_LOADING_ALIENTECH:
		Cvar_Set("mn_aircraft_item_warning", _("You can't equip an alien technology."));
		break;
	case AIM_LOADING_TECHNOLOGYNOTRESEARCHED:
		Cvar_Set("mn_aircraft_item_warning", _("Technology requested is not yet completed."));
		break;
	case AIM_LOADING_TOOHEAVY:
		Cvar_Set("mn_aircraft_item_warning", _("This item is too heavy for the selected slot."));
		break;
	case AIM_LOADING_NOWEAPON:
		Cvar_Set("mn_aircraft_item_warning", _("Equip a weapon first."));
		break;
	case AIM_LOADING_NOTUSABLEWITHWEAPON:
		Cvar_Set("mn_aircraft_item_warning", _("Ammo not usable with current weapon."));
		break;
	case AIM_LOADING_UNKNOWNPROBLEM:
		Cvar_Set("mn_aircraft_item_warning", _("Unknown problem."));
		break;
	case AIM_LOADING_OK:
		Cvar_Set("mn_aircraft_item_warning", _("Ok"));
		break;
	}

	if (*Cvar_GetString("mn_item") == '\0') {
		UI_ExecuteConfunc("airequip_no_item");
	} else {
		if (fromSlot) {
			UI_ExecuteConfunc("airequip_installed_item");
		} else {
			if (status == AIM_LOADING_OK)
				UI_ExecuteConfunc("airequip_installable_item");
			else
				UI_ExecuteConfunc("airequip_noinstallable_item");
		}
	}
}

/**
 * @brief Fills the weapon and shield list of the aircraft equip menu
 * @sa AIM_AircraftEquipMenuClick_f
 */
static void AIM_AircraftEquipMenuUpdate_f (void)
{
	if (Cmd_Argc() != 2) {
		if (airequipID == -1) {
			Com_Printf("Usage: %s <num>\n", Cmd_Argv(0));
			return;
		}
		AIM_CheckAirequipID();
	} else {
		const int type = atoi(Cmd_Argv(1));
		switch (type) {
		case AC_ITEM_ELECTRONICS:
		case AC_ITEM_SHIELD:
			airequipID = type;
			UI_ExecuteConfunc("airequip_zone2_off");
			break;
		case AC_ITEM_AMMO:
		case AC_ITEM_WEAPON:
			airequipID = type;
			UI_ExecuteConfunc("airequip_zone2_on");
			break;
		default:
			airequipID = AC_ITEM_WEAPON;
			break;
		}
	}

	AIM_AircraftEquipMenuUpdate();
}

/**
 * @brief Select the current slot you want to assign the item to.
 * @note This function is only for aircraft and not far bases.
 */
static void AIM_AircraftEquipSlotSelect_f (void)
{
	int i;
	itemPos_t pos;
	aircraft_t *aircraft;
	base_t *base = B_GetCurrentSelectedBase();
	int updateZone = 0;

	if (!base)
		return;

	if (Cmd_Argc() < 2) {
		Com_Printf("Usage: %s <arg> <zone1|zone2|item>\n", Cmd_Argv(0));
		return;
	}

	aircraft = base->aircraftCurrent;
	assert(aircraft);

	pos = atoi(Cmd_Argv(1));

	if (Cmd_Argc() == 3) {
		if (Q_streq(Cmd_Argv(2), "zone1")) {
			updateZone = 1;
		} else if (Q_streq(Cmd_Argv(2), "zone2")) {
			updateZone = 2;
		}
	}

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
	AIM_AircraftEquipMenuUpdate();

	/* update description with the selected slot */
	if (updateZone > 0)
		AIM_UpdateItemDescription(qfalse, qtrue);
	else
		AIM_UpdateItemDescription(qtrue, qfalse);
}

/**
 * @brief Select the current zone you want to assign the item to.
 */
static void AIM_AircraftEquipZoneSelect_f (void)
{
	int zone;
	aircraft_t *aircraft;
	aircraftSlot_t *slot;
	base_t *base = B_GetCurrentSelectedBase();

	if (!base)
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

	/* update menu */
	AIM_AircraftEquipMenuUpdate();
	/* Check that the selected zone is OK */
	AIM_CheckAirequipSelectedZone(slot);

	AIM_UpdateItemDescription(qfalse, qtrue);
}

/**
 * @brief Add selected item to current zone.
 * @note Called from airequip menu
 * @sa aircraftItemType_t
 */
static void AIM_AircraftEquipAddItem_f (void)
{
	int zone;
	aircraftSlot_t *slot;
	aircraft_t *aircraft = NULL;
	base_t *base = B_GetCurrentSelectedBase();

	zone = (airequipID == AC_ITEM_AMMO)?2:1;

	/* proceed only if an item has been selected */
	if (!aimSelectedTechnology)
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
				AII_AddItemToSlot(base, aimSelectedTechnology, slot, qfalse); /* Aircraft stats are updated below */
				AII_AutoAddAmmo(slot);
				break;
			} else if (slot->item == INVSH_GetItemByID(aimSelectedTechnology->provides)) {
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
			AII_RemoveNextItemFromSlot(base, slot, qfalse);
			/* more below */
		}

		/* we change the weapon, shield, item, or base defence that will be installed AFTER the removal
		 * of the one in the slot atm */
		AII_AddItemToSlot(base, aimSelectedTechnology, slot, qtrue);
		AII_AutoAddAmmo(slot);
		break;
	case ZONE_AMMO:
		/* we can change ammo only if the selected item is an ammo (for weapon or base defence system) */
		if (airequipID >= AC_ITEM_AMMO) {
			AII_AddAmmoToSlot(base, aimSelectedTechnology, slot);
		}
		break;
	default:
		/* Zone higher than ZONE_AMMO shouldn't exist */
		return;
	}

	/* Update the values of aircraft stats (just in case an item has an installationTime of 0) */
	AII_UpdateAircraftStats(aircraft);

	AIM_AircraftEquipMenuUpdate();
}

/**
 * @brief Delete an object from a zone.
 */
static void AIM_AircraftEquipRemoveItem_f (void)
{
	int zone;
	aircraftSlot_t *slot;
	aircraft_t *aircraft = NULL;
	base_t *base = B_GetCurrentSelectedBase();

	zone = (airequipID == AC_ITEM_AMMO)?2:1;

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
			AII_RemoveNextItemFromSlot(base, slot, qfalse); /* we remove weapon and ammo */
			/* if you canceled next item for less than 1 hour, previous item is still functional */
			if (slot->installationTime == -slot->item->craftitem.installationTime) {
				slot->installationTime = 0;
			}
		}
		break;
	case ZONE_AMMO:
		/* we can change ammo only if the selected item is an ammo (for weapon or base defence system) */
		if (airequipID >= AC_ITEM_AMMO) {
			if (slot->nextAmmo)
				AII_RemoveNextItemFromSlot(base, slot, qtrue);
			else
				AII_RemoveItemFromSlot(base, slot, qtrue);
		}
		break;
	default:
		/* Zone higher than ZONE_AMMO shouldn't exist */
		return;
	}

	/* Update the values of aircraft stats */
	AII_UpdateAircraftStats(aircraft);

	AIM_AircraftEquipMenuUpdate();
}

/**
 * @brief Set AIM_selectedTechnology to the technology of current selected aircraft item.
 * @sa AIM_AircraftEquipMenuUpdate_f
 */
static void AIM_AircraftEquipMenuClick_f (void)
{
	int techIdx;

	if (Cmd_Argc() < 2) {
		Com_Printf("Usage: %s <num>\n", Cmd_Argv(0));
		return;
	}

	/* Which tech? */
	techIdx = atoi(Cmd_Argv(1));
	aimSelectedTechnology = RS_GetTechByIDX(techIdx);
	AIM_UpdateItemDescription(qtrue, qfalse);
}

/**
 * @brief Update the GUI with a named itemtype
 */
static void AIM_AircraftItemtypeByName_f (void)
{
	int i;
	const char *name;

	if (Cmd_Argc() != 2) {
		Com_Printf("Usage: %s <weapon|ammo|armour|item>\n", Cmd_Argv(0));
		return;
	}

	name = Cmd_Argv(1);

	if (Q_streq(name, "weapon"))
		i = AC_ITEM_WEAPON;
	else if (Q_streq(name, "ammo"))
		i = AC_ITEM_AMMO;
	else if (Q_streq(name, "armour"))
		i = AC_ITEM_SHIELD;
	else if (Q_streq(name, "item"))
		i = AC_ITEM_ELECTRONICS;
	else {
		Com_Printf("AIM_AircraftItemtypeByName_f: Invalid itemtype!\n");
		return;
	}

	airequipID = i;
	Cmd_ExecuteString(va("airequip_updatemenu %d", airequipID));
}

void AIM_InitCallbacks (void)
{
	Cmd_AddCommand("airequip_updatemenu", AIM_AircraftEquipMenuUpdate_f, "Init function for the aircraft equip menu");
	Cmd_AddCommand("airequip_selectcategory", AIM_AircraftItemtypeByName_f, "Select an item category and update the GUI");
	Cmd_AddCommand("airequip_list_click", AIM_AircraftEquipMenuClick_f, NULL);
	Cmd_AddCommand("airequip_slot_select", AIM_AircraftEquipSlotSelect_f, NULL);
	Cmd_AddCommand("airequip_add_item", AIM_AircraftEquipAddItem_f, "Add item to slot");
	Cmd_AddCommand("airequip_remove_item", AIM_AircraftEquipRemoveItem_f, "Remove item from slot");
	Cmd_AddCommand("airequip_zone_select", AIM_AircraftEquipZoneSelect_f, NULL);
}

void AIM_ShutdownCallbacks (void)
{
	Cmd_RemoveCommand("airequip_updatemenu");
	Cmd_RemoveCommand("airequip_selectcategory");
	Cmd_RemoveCommand("airequip_list_click");
	Cmd_RemoveCommand("airequip_slot_select");
	Cmd_RemoveCommand("airequip_add_item");
	Cmd_RemoveCommand("airequip_remove_item");
	Cmd_RemoveCommand("airequip_zone_select");
}
