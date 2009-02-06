/**
 * @file cl_fightequip_callbacks.c
 * @brief Header file for menu callback functions used for base and aircraft equip menu
 */

/*
All original materal Copyright (C) 2002-2007 UFO: Alien Invasion team.

Original file from Quake 2 v3.21: quake2-2.31/client/cl_main.c
Copyright (C) 1997-2001 Id Software, Inc.

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
#include "../cl_global.h"
#include "cl_fightequip_callbacks.h"
#include "cl_mapfightequip.h"
#include "../menu/node/m_node_abstractnode.h"


static int airequipID = -1;				/**< value of aircraftItemType_t that defines what item we are installing. */

static qboolean noparams = qfalse;			/**< true if AIM_AircraftEquipMenuUpdate_f or BDEF_BaseDefenseMenuUpdate_f don't need paramters */
static int airequipSelectedZone = ZONE_NONE;		/**< Selected zone in equip menu */
static int airequipSelectedSlot = ZONE_NONE;			/**< Selected slot in equip menu */
static technology_t *airequipSelectedTechnology = NULL;		/**< Selected technolgy in equip menu */

/**
 * @brief Script command to init the base defence menu.
 * @note this function is only called when the menu launches
 * @sa BDEF_BaseDefenseMenuUpdate_f
 */
void BDEF_MenuInit_f (void)
{
	menuNode_t *node;

	/* initialize selected item */
	Cvar_Set("basedef_item_name", "main");
	airequipSelectedTechnology = NULL;

	/* initialize different variables */
	airequipID = -1;
	noparams = qfalse;
	airequipSelectedZone = ZONE_NONE;
	airequipSelectedSlot = ZONE_NONE;

	/* initialize selected slot */
	airequipSelectedSlot = 0;
	/* update position of the arrow in front of the selected base defence */
	node = MN_GetNodeFromCurrentMenu("basedef_selected_slot");
	if (node)
		Vector2Set(node->pos, 25, 30);
}

/**
 * @brief Click function for base defence menu list.
 * @sa AIM_AircraftEquipMenuUpdate_f
 */
void BDEF_ListClick_f (void)
{
	int num, height;
	menuNode_t *node;

	if ((!baseCurrent && !installationCurrent) || (baseCurrent && installationCurrent))
		return;

	if (Cmd_Argc() < 2)
		return;
	num = atoi(Cmd_Argv(1));

	if ((baseCurrent && num < baseCurrent->numBatteries)
	 || (installationCurrent && num < installationCurrent->installationTemplate->maxBatteries))
		airequipSelectedSlot = num;

	/* draw an arrow in front of the selected base defence */
	node = MN_GetNodeFromCurrentMenu("basedef_slot_list");
	if (!node)
		return;
	height = node->texh[0];
	node = MN_GetNodeFromCurrentMenu("basedef_selected_slot");
	if (!node)
		return;
	Vector2Set(node->pos, 25, 30 + height * airequipSelectedSlot);

	noparams = qtrue;
	BDEF_BaseDefenseMenuUpdate_f();
}

/**
 * @brief Check airequipID value and set the correct values for aircraft items
 * and base defence items
 */
static void AIM_CheckAirequipID (void)
{
	const menu_t *activeMenu = MN_GetActiveMenu();
	const qboolean aircraftMenu = !Q_strcmp(activeMenu->name, "aircraft_equip");

	if (aircraftMenu) {
		switch (airequipID) {
		case AC_ITEM_AMMO:
		case AC_ITEM_WEAPON:
		case AC_ITEM_SHIELD:
		case AC_ITEM_ELECTRONICS:
		default:
			airequipID = AC_ITEM_WEAPON;
		}
	} else {
		switch (airequipID) {
		case AC_ITEM_BASE_MISSILE:
		case AC_ITEM_BASE_LASER:
		case AC_ITEM_AMMO_MISSILE:
		case AC_ITEM_AMMO_LASER:
			return;
		default:
			airequipID = AC_ITEM_BASE_MISSILE;
		}
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
 * @param[in] base Pointer to the aircraft
 * @return Pointer to the slot corresponding to airequipID
 * @note There is always at least one slot, otherwise you can't enter base defence menu screen.
 * @sa BDEF_ListClick_f
 */
aircraftSlot_t *BDEF_SelectBaseSlot (base_t *base, const int airequipID)
{
	aircraftSlot_t *slot;

	switch (airequipID) {
	case AC_ITEM_AMMO_MISSILE:
	case AC_ITEM_BASE_MISSILE:
		assert(base->numBatteries > 0);
		if (airequipSelectedSlot >= base->numBatteries) {
			/* update position of the arrow in front of the selected base defence */
			menuNode_t *node = MN_GetNodeFromCurrentMenu("basedef_selected_slot");
			if (!node)
				Sys_Error("BDEF_SelectBaseSlot: Could not find node basedef_selected_slot");
			Vector2Set(node->pos, 25, 30);
			airequipSelectedSlot = 0;
		}
		slot = &base->batteries[airequipSelectedSlot].slot;
		break;
	case AC_ITEM_AMMO_LASER:
	case AC_ITEM_BASE_LASER:
		assert(base->numLasers > 0);
		if (airequipSelectedSlot >= base->numLasers) {
			/* update position of the arrow in front of the selected base defence */
			menuNode_t *node = MN_GetNodeFromCurrentMenu("basedef_selected_slot");
			if (!node)
				Sys_Error("BDEF_SelectBaseSlot: Could not find node basedef_selected_slot");
			Vector2Set(node->pos, 25, 30);
			airequipSelectedSlot = 0;
		}
		slot = &base->lasers[airequipSelectedSlot].slot;
		break;
	default:
		Com_Printf("BDEF_SelectBaseSlot: Unknown airequipID: %i\n", airequipID);
		return NULL;
	}

	return slot;
}

/**
 * @brief Returns a pointer to the selected slot.
 * @param[in] installation Installation pointer.
 * @return Pointer to the slot corresponding to airequipID
 * @sa BDEF_ListClick_f
 */
aircraftSlot_t *BDEF_SelectInstallationSlot (installation_t *installation, const int airequipID)
{
	aircraftSlot_t *slot;

	switch (airequipID) {
	case AC_ITEM_AMMO_MISSILE:
	case AC_ITEM_BASE_MISSILE:
	case AC_ITEM_AMMO_LASER:
	case AC_ITEM_BASE_LASER:
		assert(installation->installationTemplate->maxBatteries > 0);
		if (airequipSelectedSlot >= installation->installationTemplate->maxBatteries) {
			/* update position of the arrow in front of the selected base defence */
			menuNode_t *node = MN_GetNodeFromCurrentMenu("basedef_selected_slot");
			if (!node)
				Sys_Error("BDEF_SelectBaseSlot: Could not find node basedef_selected_slot");
			Vector2Set(node->pos, 25, 30);
			airequipSelectedSlot = 0;
		}
		slot = &installation->batteries[airequipSelectedSlot].slot;
		break;
	default:
		Com_Printf("BDEF_SelectInstallationSlot: Unknown airequipID: %i\n", airequipID);
		return NULL;
	}

	return slot;
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
 * @brief Returns a list of craftitem technologies for the given type.
 * @note This list is terminated by a NULL pointer.
 * @param[in] type Type of the craft-items to return.
 */
static technology_t **AII_GetCraftitemTechsByType (int type)
{
	static technology_t *techList[MAX_TECHNOLOGIES];
	objDef_t *aircraftitem = NULL;
	int i, j = 0;

	for (i = 0; i < csi.numODs; i++) {
		aircraftitem = &csi.ods[i];
		if (aircraftitem->craftitem.type == type) {
			assert(j < MAX_TECHNOLOGIES);
			techList[j] = aircraftitem->tech;
			j++;
		}
		/* j+1 because last item has to be NULL */
		if (j + 1 >= MAX_TECHNOLOGIES) {
			Com_Printf("AII_GetCraftitemTechsByType: MAX_TECHNOLOGIES limit hit.\n");
			break;
		}
	}
	/* terminate the list */
	techList[j] = NULL;
	return techList;
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
		case AC_ITEM_AMMO_MISSILE:
			airequipID = AC_ITEM_BASE_MISSILE;
			break;
		case AC_ITEM_AMMO_LASER:
			airequipID = AC_ITEM_BASE_LASER;
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
 * @param[in] base Pointer to the base where items are listed (NULL if not in base).
 * @param[in] installation Pointer to installation where items are listed (NULL if not in installation).
 * @param[in] aircraft Pointer to aircraft where items are listed (NULL if not in aircraft).
 * @param[in] tech Pointer to the technolgy to select if needed (NULL if no selected technology).
 */
static void AIM_UpdateAircraftItemList (base_t* base, installation_t* installation, aircraft_t* aircraft, const technology_t *tech)
{
	static char buffer[1024];
	technology_t **list;
	int i;
	int selectedLine = 0;

	/* Delete list */
	buffer[0] = '\0';

	assert(base || aircraft || installation);

	if (airequipID == -1) {
		Com_Printf("AIM_UpdateAircraftItemList: airequipID is -1\n");
		return;
	}

	/* Add all items corresponding to airequipID to list */
	list = AII_GetCraftitemTechsByType(airequipID);

	/* Copy only those which are researched to buffer */
	i = 0;
	while (*list) {
		if (AIM_SelectableAircraftItem(base, installation, aircraft, *list, airequipID)) {
			Q_strcat(buffer, va("%s\n", _((*list)->name)), sizeof(buffer));
			if ((*list) == tech)
				selectedLine = i;
			i++;
		}
		list++;
	}

	/* copy buffer to mn.menuText to display it on screen */
	MN_RegisterText(TEXT_LIST, buffer);

	/* if there is at least one element, select the first one */
	if (i)
		Cmd_ExecuteString(va("airequip_list_click %i", selectedLine));
	else {
		/* no element in list, no tech selected */
		airequipSelectedTechnology = NULL;
		UP_AircraftItemDescription(NULL);
	}
}

/**
 * @brief Highlight selected zone
 */
static void AIM_DrawSelectedZone (void)
{
	menuNode_t *node;

	node = MN_GetNodeFromCurrentMenu("airequip_zone_select1");
	if (airequipSelectedZone == ZONE_MAIN)
		MN_HideNode(node);
	else
		MN_UnHideNode(node);

	node = MN_GetNodeFromCurrentMenu("airequip_zone_select2");
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

	node = MN_GetNodeFromCurrentMenu("airequip_slot0");
	for (i = 0; node && i < AIR_POSITIONS_MAX; node = node->next) {
		if (!Q_strncmp(node->name, "airequip_slot", 13)) {
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
}

/**
 * @brief Write in red the text in zone ammo (zone 2)
 * @sa AIM_NoEmphazeAmmoSlotText
 * @note This is intended to show the player that there is no ammo in his aircraft
 */
static inline void AIM_EmphazeAmmoSlotText (void)
{
	menuNode_t *node = MN_GetNodeFromCurrentMenu("airequip_text_zone2");
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
	menuNode_t *node = MN_GetNodeFromCurrentMenu("airequip_text_zone2");
	if (!node)
		return;
	VectorSet(node->color, 1.0f, 1.0f, 1.0f);
}

/**
 * @brief Fills the battery list, descriptions, and weapons in slots
 * of the basedefence equip menu
 * @sa BDEF_MenuInit_f
 */
void BDEF_BaseDefenseMenuUpdate_f (void)
{
	static char defBuffer[1024];
	static char smallbuffer1[256];
	static char smallbuffer2[128];
	aircraftSlot_t *slot;
	int i;
	int type;
	menuNode_t *node;

	/* don't let old links appear on this menu */
	MN_MenuTextReset(TEXT_BASEDEFENCE_LIST);
	MN_MenuTextReset(TEXT_AIREQUIP_1);
	MN_MenuTextReset(TEXT_AIREQUIP_2);
	MN_MenuTextReset(TEXT_STANDARD);

	/* baseCurrent or installationCurrent should be non NULL because we are in the menu of this base or installation */
	if (!baseCurrent && !installationCurrent)
		return;

	/* baseCurrent and installationCurrent should not be both be set.  This function requires one or the other set. */
	if (baseCurrent && installationCurrent) {
		Com_Printf("BDEF_BaseDefenseMenuUpdate_f: both the basecurrent and installationcurrent are set.  This shouldn't happen: you shouldn't be in this function.\n");
		return;
	}

	/* Check that the base or installation has at least 1 battery */
	if (baseCurrent) {
		if (baseCurrent->numBatteries + baseCurrent->numLasers < 1) {
			Com_Printf("BDEF_BaseDefenseMenuUpdate_f: there is no defence battery in this base: you shouldn't be in this function.\n");
			return;
		}
	} else if (installationCurrent) {
		if (installationCurrent->installationStatus != INSTALLATION_WORKING) {
			Com_Printf("BDEF_BaseDefenseMenuUpdate_f: installation isn't working: you shouldn't be in this function.\n");
			return;
		} else if (installationCurrent->installationTemplate->maxBatteries < 1) {
			Com_Printf("BDEF_BaseDefenseMenuUpdate_f: there is no defence battery in this installation: you shouldn't be in this function.\n");
			return;
		}
	}

	if (Cmd_Argc() != 2 || noparams) {
		if (airequipID == -1) {
			Com_Printf("Usage: %s <num>\n", Cmd_Argv(0));
			return;
		}
		AIM_CheckAirequipID();
	} else {
		type = atoi(Cmd_Argv(1));
		switch (type) {
		case 0:
			/* missiles */
			airequipID = AC_ITEM_BASE_MISSILE;
			break;
		case 1:
			/* lasers */
			airequipID = AC_ITEM_BASE_LASER;
			break;
		case 2:
			/* ammo */
			if (airequipID == AC_ITEM_BASE_MISSILE)
				airequipID = AC_ITEM_AMMO_MISSILE;
			else if (airequipID == AC_ITEM_BASE_LASER)
				airequipID = AC_ITEM_AMMO_LASER;
			break;
		default:
			Com_Printf("BDEF_BaseDefenseMenuUpdate_f: Invalid type %i.\n", type);
			return;
		}
	}

	/* Check if we can change to laser or missile */
	if (baseCurrent && baseCurrent->numBatteries > 0 && baseCurrent->numLasers > 0) {
		node = MN_GetNodeFromCurrentMenu("basedef_button_missile");
		MN_UnHideNode(node);
		node = MN_GetNodeFromCurrentMenu("basedef_button_laser");
		MN_UnHideNode(node);
	}

	/* Select slot */
	if (baseCurrent)
		slot = BDEF_SelectBaseSlot(baseCurrent, airequipID);
	else
		slot = BDEF_SelectInstallationSlot(installationCurrent, airequipID);

	/* Check that the selected zone is OK */
	AIM_CheckAirequipSelectedZone(slot);

	/* Fill the list of item you can equip your aircraft with */
	AIM_UpdateAircraftItemList(baseCurrent, installationCurrent, NULL, AII_GetTechnologyToDisplay(slot));

	/* Delete list */
	defBuffer[0] = '\0';

	if (installationCurrent) {
		/* we are in the installation defence menu */
		if (installationCurrent->installationTemplate->maxBatteries == 0)
			Q_strcat(defBuffer, _("No defence of this type in this installation\n"), sizeof(defBuffer));
		else {
			for (i = 0; i < installationCurrent->installationTemplate->maxBatteries; i++) {
				if (!installationCurrent->batteries[i].slot.item)
					Q_strcat(defBuffer, va(_("Slot %i:\tempty\n"), i), sizeof(defBuffer));
				else {
					const objDef_t *item = installationCurrent->batteries[i].slot.item;
					Q_strcat(defBuffer, va(_("Slot %i:\t%s\n"), i, _(item->tech->name)), sizeof(defBuffer));
				}
			}
		}
	} else if (airequipID == AC_ITEM_BASE_MISSILE || airequipID == AC_ITEM_AMMO_MISSILE) {
		/* we are in the base defence menu for missile */
		if (baseCurrent->numBatteries == 0)
			Q_strcat(defBuffer, _("No defence of this type in this base\n"), sizeof(defBuffer));
		else {
			for (i = 0; i < baseCurrent->numBatteries; i++) {
				if (!baseCurrent->batteries[i].slot.item)
					Q_strcat(defBuffer, va(_("Slot %i:\tempty\n"), i), sizeof(defBuffer));
				else {
					const objDef_t *item = baseCurrent->batteries[i].slot.item;
					Q_strcat(defBuffer, va(_("Slot %i:\t%s\n"), i, _(item->tech->name)), sizeof(defBuffer));
				}
			}
		}
	} else if (airequipID == AC_ITEM_BASE_LASER || airequipID == AC_ITEM_AMMO_LASER) {
		/* we are in the base defence menu for laser */
		if (baseCurrent->numLasers == 0)
			Q_strcat(defBuffer, _("No defence of this type in this base\n"), sizeof(defBuffer));
		else {
			for (i = 0; i < baseCurrent->numLasers; i++) {
				if (!baseCurrent->lasers[i].slot.item)
					Q_strcat(defBuffer, va(_("Slot %i:\tempty\n"), i), sizeof(defBuffer));
				else {
					const objDef_t *item = baseCurrent->lasers[i].slot.item;
					Q_strcat(defBuffer, va(_("Slot %i:\t%s\n"), i, _(item->tech->name)), sizeof(defBuffer));
				}
			}
		}
	} else {
		Com_Printf("BDEF_BaseDefenseMenuUpdate_f: unknown airequipId.\n");
		return;
	}
	MN_RegisterText(TEXT_BASEDEFENCE_LIST, defBuffer);

	/* Fill the texts of each zone */
	/* First slot: item currently assigned */
	if (!slot->item) {
		Com_sprintf(smallbuffer1, sizeof(smallbuffer1), "%s", _("No defence system assigned.\n"));
		/* Weight are not used for base defence atm
		Q_strcat(smallbuffer1, va(_("This slot is for %s or smaller items."), AII_WeightToName(slot->size)), sizeof(smallbuffer1)); */
	} else {
		/* Print next item if we are removing item currently installed and a new item has been added. */
		assert(slot->item->tech);
		Com_sprintf(smallbuffer1, sizeof(smallbuffer1), "%s\n",
			slot->nextItem ? _(slot->nextItem->tech->name) : _(slot->item->tech->name));
		if (!slot->installationTime)
			Q_strcat(smallbuffer1, _("This defence system is functional.\n"), sizeof(smallbuffer1));
		else if (slot->installationTime > 0)
			Q_strcat(smallbuffer1, va(_("This defence system will be installed in %i hours.\n"),
				slot->installationTime), sizeof(smallbuffer1));
		else if (slot->nextItem) {
			Q_strcat(smallbuffer1, va(_("%s will be removed in %i hours.\n"), _(slot->item->tech->name),
				- slot->installationTime), sizeof(smallbuffer1));
			Q_strcat(smallbuffer1, va(_("%s will be installed in %i hours.\n"), _(slot->nextItem->tech->name),
				slot->nextItem->craftitem.installationTime - slot->installationTime), sizeof(smallbuffer1));
		} else
			Q_strcat(smallbuffer1, va(_("This defence system will be removed in %i hours.\n"),
				-slot->installationTime), sizeof(smallbuffer1));
	}
	MN_RegisterText(TEXT_AIREQUIP_1, smallbuffer1);

	/* Second slot: ammo slot (only used for weapons) */
	if ((airequipID < AC_ITEM_WEAPON || airequipID > AC_ITEM_AMMO) && slot->item) {
		char const* const ammo = slot->nextAmmo ?
			_(slot->nextAmmo->tech->name) :
			(slot->ammo ?
			_(slot->ammo->tech->name) :
			_("No ammo assigned to this defence system."));
		Q_strncpyz(smallbuffer2, ammo, sizeof(smallbuffer2));
		/* inform player that base missile are unlimited */
		if (slot->ammo->craftitem.unlimitedAmmo)
			Q_strcat(smallbuffer2, _(" (unlimited missiles)"), sizeof(smallbuffer2));
	} else {
		*smallbuffer2 = '\0';
	}
	MN_RegisterText(TEXT_AIREQUIP_2, smallbuffer2);

	/* Draw selected zone */
	AIM_DrawSelectedZone();

	noparams = qfalse;
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

	if (!baseCurrent)
		return;

	/* don't let old links appear on this menu */
	MN_MenuTextReset(TEXT_STANDARD);
	MN_MenuTextReset(TEXT_AIREQUIP_1);
	MN_MenuTextReset(TEXT_AIREQUIP_2);
	MN_MenuTextReset(TEXT_LIST);

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
			Cbuf_AddText("airequip_zone2_off;");
			break;
		case AC_ITEM_WEAPON:
			airequipID = type;
			Cbuf_AddText("airequip_zone2_on;");
			break;
		default:
			airequipID = AC_ITEM_WEAPON;
			break;
		}
	}

	/* Reset value of noparams */
	noparams = qfalse;

	node = MN_GetNodeFromCurrentMenu("aircraftequip");

	/* we are not in the aircraft menu */
	if (!node) {
		Com_DPrintf(DEBUG_CLIENT, "AIM_AircraftEquipMenuUpdate_f: Error - node aircraftequip not found\n");
		return;
	}

	aircraft = baseCurrent->aircraftCurrent;

	assert(aircraft);
	assert(node);

	/* Check that airequipSelectedSlot corresponds to an existing slot for this aircraft */
	AIM_CheckAirequipSelectedSlot(aircraft);

	/* Select slot */
	slot = AII_SelectAircraftSlot(aircraft, airequipID);

	/* Check that the selected zone is OK */
	AIM_CheckAirequipSelectedZone(slot);

	/* Fill the list of item you can equip your aircraft with */
	AIM_UpdateAircraftItemList(NULL, NULL, aircraft, AII_GetTechnologyToDisplay(slot));

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
	} else {
		*smallbuffer2 = '\0';
	}
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

	if (!baseCurrent)
		return;

	if (Cmd_Argc() < 2) {
		Com_Printf("Usage: %s <arg>\n", Cmd_Argv(0));
		return;
	}

	aircraft = baseCurrent->aircraftCurrent;
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
	const menu_t *activeMenu;
	qboolean aircraftMenu;

	if ((!baseCurrent && !installationCurrent) || (baseCurrent && installationCurrent))
		return;

	if (Cmd_Argc() < 2) {
		Com_Printf("Usage: %s <arg>\n", Cmd_Argv(0));
		return;
	}

	/* select menu */
	activeMenu = MN_GetActiveMenu();
	aircraftMenu = !Q_strcmp(activeMenu->name, "aircraft_equip");

	zone = atoi(Cmd_Argv(1));

	if (aircraftMenu) {
		aircraft = baseCurrent->aircraftCurrent;
		assert(aircraft);
		/* Select slot */
		slot = AII_SelectAircraftSlot(aircraft, airequipID);
	} else {
		/* Select slot */
		if (installationCurrent)
			slot = BDEF_SelectInstallationSlot(installationCurrent, airequipID);
		else
			slot = BDEF_SelectBaseSlot(baseCurrent, airequipID);
		aircraft = NULL;
	}

	/* ammos are only available for weapons */
	switch (airequipID) {
	/* a weapon was selected - select ammo type corresponding to this weapon */
	case AC_ITEM_WEAPON:
		if (zone == ZONE_AMMO) {
			if (slot->item)
				airequipID = AC_ITEM_AMMO;
		}
		break;
	case AC_ITEM_BASE_MISSILE:
		if (zone == ZONE_AMMO) {
			if (slot->item)
				airequipID = AC_ITEM_AMMO_MISSILE;
		}
		break;
	case AC_ITEM_BASE_LASER:
		if (zone == ZONE_AMMO) {
			if (slot->item)
				airequipID = AC_ITEM_AMMO_LASER;
		}
		break;
	/* an ammo was selected - select weapon type corresponding to this ammo */
	case AC_ITEM_AMMO:
		if (zone != ZONE_AMMO)
			airequipID = AC_ITEM_WEAPON;
		break;
	case AC_ITEM_AMMO_MISSILE:
		if (zone != ZONE_AMMO)
			airequipID = AC_ITEM_BASE_MISSILE;
		break;
	case AC_ITEM_AMMO_LASER:
		if (zone != ZONE_AMMO)
			airequipID = AC_ITEM_BASE_LASER;
		break;
	default :
		/* ZONE_AMMO is not available for electronics and shields */
		if (zone == ZONE_AMMO)
			return;
	}
	airequipSelectedZone = zone;

	/* Fill the list of item you can equip your aircraft with */
	AIM_UpdateAircraftItemList(aircraftMenu ? NULL : baseCurrent, aircraftMenu ? NULL : installationCurrent,
		aircraft, AII_GetTechnologyToDisplay(slot));

	/* Check that the selected zone is OK */
	AIM_CheckAirequipSelectedZone(slot);

	/* Draw selected zone */
	AIM_DrawSelectedZone();
}

/**
 * @brief Add selected item to current zone.
 * @note May be called from airequip menu or basedefence menu
 * @sa aircraftItemType_t
 */
void AIM_AircraftEquipAddItem_f (void)
{
	int zone;
	aircraftSlot_t *slot;
	aircraft_t *aircraft = NULL;
	const menu_t *activeMenu;
	qboolean aircraftMenu;
	base_t* base = NULL;
	installation_t* installation = NULL;

	if ((!baseCurrent && !installationCurrent) || (baseCurrent && installationCurrent)) {
		Com_Printf("Exiting early base and install both true or both false\n");
		return;
	}

	if (Cmd_Argc() < 2) {
		Com_Printf("Usage: %s <arg>\n", Cmd_Argv(0));
		return;
	}

	zone = atoi(Cmd_Argv(1));

	/* select menu */
	activeMenu = MN_GetActiveMenu();
	aircraftMenu = !Q_strncmp(activeMenu->name, "aircraft_equip", 14);

	/* proceed only if an item has been selected */
	if (!airequipSelectedTechnology)
		return;

	/* check in which menu we are */
	if (aircraftMenu) {
		aircraft = baseCurrent->aircraftCurrent;
		assert(aircraft);
		base = aircraft->homebase;	/* we need to know where items will be removed */
		slot = AII_SelectAircraftSlot(aircraft, airequipID);
	} else if (baseCurrent) {
		base = baseCurrent;
		slot = BDEF_SelectBaseSlot(base, airequipID);
	} else {
		installation = installationCurrent;
		assert(installation);
		slot = BDEF_SelectInstallationSlot(installation, airequipID);
	}

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
			/* we add the weapon, shield, item, or base defence if slot is free or the installation of
			 * current item just began */
			if (!slot->item || (slot->item && slot->installationTime == slot->item->craftitem.installationTime)) {
				AII_RemoveItemFromSlot(base, slot, qfalse);
				AII_AddItemToSlot(base, airequipSelectedTechnology, slot, qfalse); /* Aircraft stats are updated below */
				AIM_AutoAddAmmo(base, installation, aircraft, slot, airequipID);
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
		AIM_AutoAddAmmo(base, installation, aircraft, slot, airequipID);
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

	if (aircraftMenu) {
		/* Update the values of aircraft stats (just in case an item has an installationTime of 0) */
		AII_UpdateAircraftStats(aircraft);

		noparams = qtrue; /* used for AIM_AircraftEquipMenuUpdate_f */
		AIM_AircraftEquipMenuUpdate_f();
	} else {
		noparams = qtrue; /* used for BDEF_BaseDefenseMenuUpdate_f */
		BDEF_BaseDefenseMenuUpdate_f();
	}
}

/**
 * @brief Delete an object from a zone.
 */
void AIM_AircraftEquipDeleteItem_f (void)
{
	int zone;
	aircraftSlot_t *slot;
	aircraft_t *aircraft = NULL;
	const menu_t *activeMenu;
	qboolean aircraftMenu;
	base_t* base = NULL;
	installation_t* installation = NULL;

	if ((!baseCurrent && !installationCurrent) || (baseCurrent && installationCurrent)) {
		Com_Printf("Exiting early base and install both true or both false\n");
		return;
	}

	if (Cmd_Argc() < 2) {
		Com_Printf("Usage: %s <arg>\n", Cmd_Argv(0));
		return;
	}
	zone = atoi(Cmd_Argv(1));

	/* select menu */
	activeMenu = MN_GetActiveMenu();
	aircraftMenu = !Q_strncmp(activeMenu->name, "aircraft_equip", 14);

	/* check in which menu we are
	 * and select the slot we are changing */
	if (aircraftMenu) {
		aircraft = baseCurrent->aircraftCurrent;
		assert(aircraft);
		base = aircraft->homebase;	/* we need to know where items will be added / removed */
		slot = AII_SelectAircraftSlot(aircraft, airequipID);
	} else if (baseCurrent) {
		base = baseCurrent;
		slot = BDEF_SelectBaseSlot(base, airequipID);
	} else {
		installation = installationCurrent;
		assert(installation);
		slot = BDEF_SelectInstallationSlot(installation, airequipID);
	}

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

	if (aircraftMenu) {
		/* Update the values of aircraft stats */
		AII_UpdateAircraftStats(aircraft);

		noparams = qtrue; /* used for AIM_AircraftEquipMenuUpdate_f */
		AIM_AircraftEquipMenuUpdate_f();
	} else {
		noparams = qtrue; /* used for BDEF_MenuUpdate_f */
		BDEF_BaseDefenseMenuUpdate_f();
	}
}

/**
 * @brief Set airequipSelectedTechnology to the technology of current selected aircraft item.
 * @sa AIM_AircraftEquipMenuUpdate_f
 */
void AIM_AircraftEquipMenuClick_f (void)
{
	aircraft_t *aircraft;
	base_t *base;
	installation_t *installation;
	int num;
	technology_t **list;
	const menu_t *activeMenu;

	if ((!baseCurrent && !installationCurrent) || (baseCurrent && installationCurrent) || airequipID == -1)
		return;

	if (Cmd_Argc() < 2) {
		Com_Printf("Usage: %s <num>\n", Cmd_Argv(0));
		return;
	}

	/* select menu */
	activeMenu = MN_GetActiveMenu();
	/* check in which menu we are */
	if (!Q_strncmp(activeMenu->name, "aircraft_equip", 14)) {
		if (baseCurrent->aircraftCurrent == NULL)
			return;
		aircraft = baseCurrent->aircraftCurrent;
		base = NULL;
		installation = NULL;
	} else if (!Q_strncmp(activeMenu->name, "basedefence", 11)) {
		base = baseCurrent;
		installation = installationCurrent;
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
	while (*list) {
		if (AIM_SelectableAircraftItem(base, installation, aircraft, *list, airequipID)) {
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
	Cvar_Set("mn_displayweapon", "0"); /* use strings here - no int */
	Cvar_Set("mn_changeweapon", "0"); /* use strings here - no int */
	Cvar_Set("mn_researchedlinkname", "");
	Cvar_Set("mn_upresearchedlinknametooltip", "");

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

void AIM_InitCallbacks (void)
{
	Cmd_AddCommand("mn_next_equiptype", AIM_NextItemtype_f, "Shows the next aircraft equip category.");
	Cmd_AddCommand("mn_prev_equiptype", AIM_PreviousItemtype_f, "Shows the previous aircraft equip category.");
}

void AIM_ShutdownCallbacks (void)
{
	Cmd_RemoveCommand("mn_next_equiptype");
	Cmd_RemoveCommand("mn_prev_equiptype");
}
