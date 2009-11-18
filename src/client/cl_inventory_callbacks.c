/**
 * @file cl_inventory_callbacks.c
 */

/*
Copyright (C) 2002-2009 UFO: Alien Invasion.

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
#include "cl_inventory.h"
#include "cl_inventory_callbacks.h"
#include "menu/m_main.h"
#include "menu/m_nodes.h"
#include "cl_game.h"

#define ITEM_DESCRIPTION_AMMO "1"
#define ITEM_DESCRIPTION_WEAPON "2"

static const objDef_t *currentDisplayedObject;
static int itemIndex;
static int fireModeIndex;

/**
 * @brief Translate a weaponSkill integer to a translated string
 */
static const char* CL_WeaponSkillToName (int weaponSkill)
{
	switch (weaponSkill) {
	case SKILL_CLOSE:
		return _("skill_close");
	case SKILL_HEAVY:
		return _("skill_heavy");
	case SKILL_ASSAULT:
		return _("skill_assault");
	case SKILL_SNIPER:
		return _("skill_sniper");
	case SKILL_EXPLOSIVE:
		return _("skill_explosive");
	default:
		return _("Unknown weapon skill");
	}
}

/**
 * @brief Prints the description for items (weapons, armour, ...)
 * @param[in] od The object definition of the item
 * @note Not only called from UFOpaedia but also from other places to display
 * weapon and ammo statistics
 * @todo Do we need to add checks for @c od->isDummy here somewhere?
 */
void INV_ItemDescription (const objDef_t *od)
{
	static char itemText[MAX_SMALLMENUTEXTLEN];
	int i, count;

	/* reset everything */
	MN_ExecuteConfunc("change_action 0");

	currentDisplayedObject = od;

	if (!od) {	/* If nothing selected return */
		Cvar_Set("mn_itemname", "");
		Cvar_Set("mn_item", "");
		Cvar_Set("mn_linkname", "");
		MN_ResetData(TEXT_STANDARD);
		itemIndex = fireModeIndex = 0;
		return;
	}

	/* select item */
	Cvar_Set("mn_itemname", _(od->name));
	Cvar_Set("mn_item", od->id);
	Cvar_Set("mn_linkname", "");

	count = 0;
	if (INV_IsAmmo(od)) {
		/* We display the pre/next buttons for changing weapon only if there are at least 2 researched weapons
		 * we are counting the number of weapons that are usable with this ammo */
		for (i = 0; i < od->numWeapons; i++)
			if (GAME_ItemIsUseable(od->weapons[i]))
				count++;

		if (itemIndex >= od->numWeapons || itemIndex < 0)
			itemIndex = count - 1;

		/* Only display weapons if at least one is usable */
		if (count > 0) {
			MN_ExecuteConfunc("change_action "ITEM_DESCRIPTION_AMMO);
			/* Display the name of the associated weapon */
			Cvar_Set("mn_linkname", _(od->weapons[itemIndex]->name));
		}
	} else if (od->weapon && od->reload) {
		/* We have a weapon that uses ammos */
		const objDef_t *odAmmo;

		/* We display the pre/next buttons for changing ammo only if there are at least 2 researched ammo
		 * we are counting the number of ammo that is usable with this weapon */
		for (i = 0; i < od->numAmmos; i++) {
			if (GAME_ItemIsUseable(od->ammos[i]))
				count++;
		}

		if (itemIndex >= od->numAmmos || itemIndex < 0)
			itemIndex = count - 1;

		/* Only display ammos if at least one has been researched */
		if (count > 0) {
			MN_ExecuteConfunc("change_action "ITEM_DESCRIPTION_WEAPON);
			odAmmo = od->ammos[itemIndex];
			if (!GAME_ItemIsUseable(odAmmo)) {
				for (i = 0; i < od->numWeapons; i++) {
					itemIndex++;
					odAmmo = od->ammos[itemIndex];
					if (GAME_ItemIsUseable(odAmmo)) {
						Cvar_Set("mn_linkname", _(odAmmo->name));
						break;
					}
				}
			} else {
				Cvar_Set("mn_linkname", _(odAmmo->name));
			}
		}
	}


	/* set description text if item as been researched or one of its ammo/weapon has been researched */
	if (count > 0 || GAME_ItemIsUseable(od)) {
		*itemText = '\0';
		if (INV_IsArmour(od)) {
			Q_strcat(itemText, _("Type:\tProtection:\n"), sizeof(itemText));
			for (i = 0; i < csi.numDTs; i++) {
				if (!csi.dts[i].showInMenu)
					continue;
				Q_strcat(itemText, va(_("%s\t%i\n"), _(csi.dts[i].id), od->ratings[i]), sizeof(itemText));
			}
		} else if (INV_IsAmmo(od)) {
			/** @todo is there ammo with no firedefs? */
			if (od->numFiredefs[itemIndex] > 0) {
				const fireDef_t *fd;
				const int numFiredefs = od->numFiredefs[itemIndex];

				/* This contains everything common for weapons and ammos */

				/* We check if the wanted firemode to display exists. */
				if (fireModeIndex > numFiredefs - 1)
					fireModeIndex = 0;
				if (fireModeIndex < 0)
					fireModeIndex = numFiredefs - 1;

				fd = &od->fd[itemIndex][fireModeIndex];

				/* We always display the name of the firemode for an ammo */
				Cvar_Set("mn_firemodename", _(fd->name));

				/* We display the characteristics of this firemode */
				Q_strcat(itemText, va(_("Skill:\t%s\n"), CL_WeaponSkillToName(fd->weaponSkill)), sizeof(itemText));
				Q_strcat(itemText, va(_("Damage:\t%i\n"), (int) (fd->damage[0] + fd->spldmg[0]) * fd->shots), sizeof(itemText));
				Q_strcat(itemText, va(_("Time units:\t%i\n"), fd->time), sizeof(itemText));
				Q_strcat(itemText, va(_("Range:\t%g\n"), fd->range / UNIT_SIZE), sizeof(itemText));
				Q_strcat(itemText, va(_("Spreads:\t%g\n"), (fd->spread[0] + fd->spread[1]) / 2), sizeof(itemText));
			}
		} else if (od->weapon && (od->reload || od->thrown)) {
			Com_sprintf(itemText, sizeof(itemText), _("%s weapon with\n"), (od->fireTwoHanded ? _("Two-handed") : _("One-handed")));
			Q_strcat(itemText, va(_("Max ammo:\t%i\n"), (int) (od->ammo)), sizeof(itemText));
		} else if (od->weapon) {
			Com_sprintf(itemText, sizeof(itemText), _("%s ammo-less weapon with\n"), (od->fireTwoHanded ? _("Two-handed") : _("One-handed")));
		} else if (od->craftitem.type <= AC_ITEM_BASE_LASER) {
			/** @todo move this into the campaign mode only code */
			/* This is a weapon for base, can be displayed in equip menu */
			Com_sprintf(itemText, sizeof(itemText), _("Weapon for base defence system\n"));
		} else if (od->craftitem.type != AIR_STATS_MAX) {
			/** @todo move this into the campaign mode only code */
			/* This is an item for aircraft or ammos for bases -- do nothing */
		} else {
			/* just an item - only primary definition */
			Com_sprintf(itemText, sizeof(itemText), _("%s auxiliary equipment with\n"), (od->fireTwoHanded ? _("Two-handed") : _("One-handed")));
			Q_strcat(itemText, va(_("Action:\t%s\n"), od->fd[0][0].name), sizeof(itemText));
			Q_strcat(itemText, va(_("Time units:\t%i\n"), od->fd[0][0].time), sizeof(itemText));
			Q_strcat(itemText, va(_("Range:\t%g\n"), od->fd[0][0].range / UNIT_SIZE), sizeof(itemText));
		}

		MN_RegisterText(TEXT_STANDARD, itemText);
	} else {
		Com_sprintf(itemText, sizeof(itemText), _("Unknown - not useable"));
		MN_RegisterText(TEXT_STANDARD, itemText);
	}
}


/**
 * @brief Increases the number of the firemode to display
 * @sa UP_ItemDescription
 */
static void INV_IncreaseFiremode_f (void)
{
	if (!currentDisplayedObject)
		return;

	fireModeIndex++;

	INV_ItemDescription(currentDisplayedObject);
}

/**
 * @brief Decreases the number of the firemode to display
 * @sa UP_ItemDescription
 */
static void INV_DecreaseFiremode_f (void)
{
	if (!currentDisplayedObject)
		return;

	fireModeIndex--;

	INV_ItemDescription(currentDisplayedObject);
}

/**
 * @brief Increases the number of the weapon to display (for ammo) or the ammo to display (for weapon)
 * @sa UP_ItemDescription
 * @todo use the current select objDef_t directly and move this out of the ufopedia code - it's usable and needed in the equipment code, too
 */
static void INV_IncreaseItem_f (void)
{
	const objDef_t *od = currentDisplayedObject;
	if (!od)
		return;

	if (itemIndex < od->numWeapons) {
		do {
			if (itemIndex >= od->numWeapons - 1)
				break;
			itemIndex++;
		} while (!GAME_ItemIsUseable(od->weapons[itemIndex]));
	} else if (itemIndex < od->numAmmos) {
		do {
			if (itemIndex >= od->numAmmos - 1)
				break;
			itemIndex++;
		} while (!GAME_ItemIsUseable(od->ammos[itemIndex]));
	}
	INV_ItemDescription(od);
}

/**
 * @brief Decreases the number of the firemode to display (for ammo) or the ammo to display (for weapon)
 * @sa UP_ItemDescription
 * @todo use the current select objDef_t directly and move this out of the ufopedia code - it's usable and needed in the equipment code, too
 */
static void INV_DecreaseItem_f (void)
{
	const objDef_t *od = currentDisplayedObject;
	if (!od)
		return;

	if (od->numWeapons && itemIndex > 0) {
		do {
			if (itemIndex == 0)
				break;
			itemIndex--;
		} while (!GAME_ItemIsUseable(od->weapons[itemIndex]));
	} else if (od->numAmmos && itemIndex > 0) {
		do {
			if (itemIndex == 0)
				break;
			itemIndex--;
		} while (!GAME_ItemIsUseable(od->ammos[itemIndex]));
	}
	INV_ItemDescription(od);
}

void INV_InitCallbacks (void)
{
	Cmd_AddCommand("mn_increasefiremode", INV_IncreaseFiremode_f, "Increases the number of the firemode to display");
	Cmd_AddCommand("mn_decreasefiremode", INV_DecreaseFiremode_f, "Decreases the number of the firemode to display");
	Cmd_AddCommand("mn_increaseitem", INV_IncreaseItem_f, "Increases the number of the weapon or the ammo to display");
	Cmd_AddCommand("mn_decreaseitem", INV_DecreaseItem_f, "Decreases the number of the weapon or the ammo to display");
}
