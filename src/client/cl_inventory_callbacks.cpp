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

#include "cl_inventory_callbacks.h"
#include "cl_shared.h"
#include "cl_inventory.h"
#include "ui/ui_main.h"
#include "ui/ui_nodes.h"
#include "cgame/cl_game.h"
#include "ui/ui_popup.h"

static const objDef_t* currentDisplayedObject;
static int itemIndex;
static int fireModeIndex;

/**
 * @brief Translate a weaponSkill integer to a translated string
 */
static const char* CL_WeaponSkillToName (int weaponSkill)
{
	switch (weaponSkill) {
	case SKILL_CLOSE:
		return _("Close quarters");
#if 0
	case SKILL_HEAVY:
		return _("Heavy");
#endif
	case SKILL_ASSAULT:
		return _("Assault");
	case SKILL_SNIPER:
		return _("Sniper");
	case SKILL_EXPLOSIVE:
		return _("Explosives");
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
void INV_ItemDescription (const objDef_t* od)
{
	static char itemText[UI_MAX_SMALLTEXTLEN];
	int i;
	int count;

	currentDisplayedObject = od;
	Cvar_Set("mn_firemodename", "");
	Cvar_Set("mn_linkname", "");

	if (!od) {	/* If nothing selected return */
		Cvar_Set("mn_itemname", "");
		Cvar_Set("mn_item", "");
		UI_ResetData(TEXT_ITEMDESCRIPTION);
		itemIndex = fireModeIndex = 0;
		UI_ExecuteConfunc("itemdesc_view 0 0;");
		return;
	}

	/* select item */
	Cvar_Set("mn_itemname", "%s", _(od->name));
	Cvar_Set("mn_item", "%s", od->id);

	count = 0;
	if (GAME_ItemIsUseable(od)) {
		if (od->isAmmo()) {
			/* We display the pre/next buttons for changing weapon only if there are at least 2 researched weapons
			 * we are counting the number of weapons that are usable with this ammo */
			for (i = 0; i < od->numWeapons; i++)
				if (GAME_ItemIsUseable(od->weapons[i]))
					count++;
			if (itemIndex >= od->numWeapons || itemIndex < 0)
				itemIndex = 0;
			if (count > 0) {
				while (!GAME_ItemIsUseable(od->weapons[itemIndex])) {
					itemIndex++;
					if (itemIndex >= od->numWeapons)
						itemIndex = 0;
				}
				Cvar_ForceSet("mn_linkname", _(od->weapons[itemIndex]->name));
			}
		} else if (od->weapon) {
			/* We display the pre/next buttons for changing ammo only if there are at least 2 researched ammo
			 * we are counting the number of ammo that is usable with this weapon */
			for (i = 0; i < od->numAmmos; i++)
				if (GAME_ItemIsUseable(od->ammos[i]))
					count++;

			if (itemIndex >= od->numAmmos || itemIndex < 0)
				itemIndex = 0;

			/* Only display ammos if at least one has been researched */
			if (count > 0) {
				/* We have a weapon that uses ammos */
				while (!GAME_ItemIsUseable(od->ammos[itemIndex])) {
					itemIndex++;
					if (itemIndex >= od->numAmmos)
						itemIndex = 0;
				}
				Cvar_ForceSet("mn_linkname", _(od->ammos[itemIndex]->name));
			}
		} else {
			Cvar_ForceSet("mn_linkname", "");
		}
	}

	/* set description text if item has been researched or one of its ammo/weapon has been researched */
	if (count > 0 || GAME_ItemIsUseable(od)) {
		int numFiredefs = 0;

		*itemText = '\0';
		if (od->isArmour()) {
			Com_sprintf(itemText, sizeof(itemText), _("Size:\t%i\n"), od->size);
			Q_strcat(itemText, sizeof(itemText), _("Weight:\t%g Kg\n"), od->weight / WEIGHT_FACTOR);
			Q_strcat(itemText, sizeof(itemText), "\n");
			Q_strcat(itemText, sizeof(itemText), _("^BDamage type:\tProtection:\n"));
			for (i = 0; i < csi.numDTs; i++) {
				const damageType_t* dt = &csi.dts[i];
				if (!dt->showInMenu)
					continue;
				Q_strcat(itemText, sizeof(itemText), _("%s\t%i\n"), _(dt->id), od->ratings[i]);
			}
		} else if ((od->weapon && od->numAmmos) || od->isAmmo()) {
			const objDef_t* odAmmo;

			if (count > 0) {
				int weaponIndex;
				if (od->weapon) {
					Com_sprintf(itemText, sizeof(itemText), _("%s weapon\n"), (od->fireTwoHanded ? _("Two-handed") : _("One-handed")));
					if (od->ammo > 0)
						Q_strcat(itemText, sizeof(itemText), _("Max ammo:\t%i\n"), od->ammo);
					odAmmo = (od->numAmmos) ? od->ammos[itemIndex] : od;
					assert(odAmmo);
					for (weaponIndex = 0; (weaponIndex < odAmmo->numWeapons) && (odAmmo->weapons[weaponIndex] != od); weaponIndex++) {}
				} else {
					odAmmo = od;
					weaponIndex = itemIndex;
				}

				Q_strcat(itemText, sizeof(itemText), _("Weight:\t%g Kg\n"), od->weight / WEIGHT_FACTOR);
				/** @todo is there ammo with no firedefs? */
				if (GAME_ItemIsUseable(odAmmo) && odAmmo->numFiredefs[weaponIndex] > 0) {
					const fireDef_t* fd;
					numFiredefs = odAmmo->numFiredefs[weaponIndex];

					/* This contains everything common for weapons and ammos */
					/* We check if the wanted firemode to display exists. */
					if (fireModeIndex > numFiredefs - 1)
						fireModeIndex = 0;
					if (fireModeIndex < 0)
						fireModeIndex = numFiredefs - 1;

					fd = &odAmmo->fd[weaponIndex][fireModeIndex];

					/* We always display the name of the firemode for an ammo */
					Cvar_Set("mn_firemodename", "%s", _(fd->name));

					/* We display the characteristics of this firemode */
					Q_strcat(itemText, sizeof(itemText), _("Skill:\t%s\n"), CL_WeaponSkillToName(fd->weaponSkill));
					Q_strcat(itemText, sizeof(itemText), _("Damage:\t%i\n"), (int) (fd->damage[0] + fd->spldmg[0]) * fd->shots);
					Q_strcat(itemText, sizeof(itemText), _("Time units:\t%i\n"), fd->time);
					Q_strcat(itemText, sizeof(itemText), _("Range:\t%g\n"), fd->range / UNIT_SIZE);
					Q_strcat(itemText, sizeof(itemText), _("Spreads:\t%g\n"), (fd->spread[0] + fd->spread[1]) / 2);
				}
			} else {
				Com_sprintf(itemText, sizeof(itemText), _("%s. No detailed info available.\n"), od->isAmmo() ? _("Ammunition") : _("Weapon"));
				Q_strcat(itemText, sizeof(itemText), _("Weight:\t%g Kg\n"), od->weight / WEIGHT_FACTOR);
			}
		} else if (od->implant) {
			//const implantDef_t* implant = INVSH_GetImplantForObjDef(od);
			/* the strengthen effect that is bound to the implant no matter whether the user triggers some action */
			itemEffect_t* effect = od->strengthenEffect;
			if (effect != nullptr) {
				Q_strcat(itemText, sizeof(itemText), _("Permanent:\t%s\n"), (effect->isPermanent ? _("Yes") : _("No")));
				Q_strcat(itemText, sizeof(itemText), _("Duration:\t%i\n"), effect->duration);
				Q_strcat(itemText, sizeof(itemText), _("Period:\t%i\n"), effect->period);
			}
			Q_strcat(itemText, sizeof(itemText), _("Price:\t%i c\n"), od->price);
		} else if (od->weapon) {
			Com_sprintf(itemText, sizeof(itemText), _("%s ammo-less weapon\n"), (od->fireTwoHanded ? _("Two-handed") : _("One-handed")));
			Q_strcat(itemText, sizeof(itemText), _("Weight:\t%g Kg\n"), od->weight / WEIGHT_FACTOR);
		} else {
			/* just an item - only primary definition */
			Com_sprintf(itemText, sizeof(itemText), _("%s auxiliary equipment\n"), (od->fireTwoHanded ? _("Two-handed") : _("One-handed")));
			Q_strcat(itemText, sizeof(itemText), _("Weight:\t%g Kg\n"), od->weight / WEIGHT_FACTOR);
			if (od->numWeapons > 0 && od->numFiredefs[0] > 0) {
				const fireDef_t* fd = &od->fd[0][0];
				Q_strcat(itemText, sizeof(itemText), _("Action:\t%s\n"), _(fd->name));
				Q_strcat(itemText, sizeof(itemText), _("Time units:\t%i\n"), fd->time);
				Q_strcat(itemText, sizeof(itemText), _("Range:\t%g\n"), fd->range / UNIT_SIZE);
			}
		}

		UI_RegisterText(TEXT_ITEMDESCRIPTION, itemText);
		UI_ExecuteConfunc("itemdesc_view %i %i;", count, numFiredefs);
	} else {
		Com_sprintf(itemText, sizeof(itemText), _("Unknown - not useable"));
		UI_RegisterText(TEXT_ITEMDESCRIPTION, itemText);
		UI_ExecuteConfunc("itemdesc_view 0 0;");
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
 */
static void INV_IncreaseItem_f (void)
{
	const objDef_t* od = currentDisplayedObject;

	if (!od)
		return;

	if (od->numWeapons) {
		const int current = itemIndex;
		do {
			itemIndex++;
			if (itemIndex > od->numWeapons - 1) {
				itemIndex = 0;
			}
		} while (itemIndex != current && !GAME_ItemIsUseable(od->weapons[itemIndex]));
	} else if (od->numAmmos) {
		const int current = itemIndex;
		do {
			itemIndex++;
			if (itemIndex > od->numAmmos - 1) {
				itemIndex = 0;
			}
		} while (itemIndex != current && !GAME_ItemIsUseable(od->ammos[itemIndex]));
	}
	INV_ItemDescription(od);
}

/**
 * @brief Decreases the number of the firemode to display (for ammo) or the ammo to display (for weapon)
 * @sa UP_ItemDescription
 */
static void INV_DecreaseItem_f (void)
{
	const objDef_t* od = currentDisplayedObject;

	if (!od)
		return;

	if (od->numWeapons) {
		const int current = itemIndex;
		do {
			itemIndex--;
			if (itemIndex < 0) {
				itemIndex = od->numWeapons - 1;
			}
		} while (itemIndex != current && !GAME_ItemIsUseable(od->weapons[itemIndex]));
	} else if (od->numAmmos) {
		const int current = itemIndex;
		do {
			itemIndex--;
			if (itemIndex < 0) {
				itemIndex = od->numAmmos - 1;
			}
		} while (itemIndex != current && !GAME_ItemIsUseable(od->ammos[itemIndex]));
	}
	INV_ItemDescription(od);
}

/**
 * @brief Update the GUI with the selected item
 */
static void INV_UpdateObject_f (void)
{
	/* check syntax */
	if (Cmd_Argc() < 3) {
		Com_Printf("Usage: %s <objectid> <confunc> [mustwechangetab]\n", Cmd_Argv(0));
		return;
	}

	bool changeTab = true;
	if (Cmd_Argc() == 4)
		changeTab = atoi(Cmd_Argv(3)) >= 1;

	const int num = atoi(Cmd_Argv(1));
	if (num < 0 || num >= csi.numODs) {
		Com_Printf("Id %i out of range 0..%i\n", num, csi.numODs);
		return;
	}
	const objDef_t* obj = INVSH_GetItemByIDX(num);

	/* update tab */
	if (changeTab) {
		const cvar_t* var = Cvar_FindVar("mn_equiptype");
		const int filter = INV_GetFilterFromItem(obj);
		if (var && var->integer != filter) {
			Cvar_SetValue("mn_equiptype", filter);
			UI_ExecuteConfunc("%s", Cmd_Argv(2));
		}
	}

	/* update item description */
	INV_ItemDescription(obj);
}

/**
 * @brief Update the equipment weight for the selected actor.
 */
static void INV_UpdateActorLoad_f (void)
{
	if (Cmd_Argc() < 2) {
		Com_Printf("Usage: %s <callback>\n", Cmd_Argv(0));
		return;
	}

	const character_t* chr = GAME_GetSelectedChr();
	if (chr == nullptr)
		return;

	const float invWeight = chr->inv.getWeight();
	const int maxWeight = GAME_GetChrMaxLoad(chr);
	const float penalty = GET_ENCUMBRANCE_PENALTY(invWeight, maxWeight);
	const int normalTU = GET_TU(chr->score.skills[ABILITY_SPEED], 1.0f - WEIGHT_NORMAL_PENALTY);
	const int tus = GET_TU(chr->score.skills[ABILITY_SPEED], penalty);
	const int tuPenalty = tus - normalTU;
	int count = 0;

	const Container* cont = nullptr;
	while ((cont = chr->inv.getNextCont(cont))) {
		for (Item* invList = cont->_invList, *next; invList; invList = next) {
			next = invList->getNext();
			const fireDef_t* fireDef = invList->getFiredefs();
			if (fireDef == nullptr)
				continue;
			for (int i = 0; i < MAX_FIREDEFS_PER_WEAPON; i++) {
				if (fireDef[i].time <= 0)
					continue;
				if (fireDef[i].time <= tus)
					continue;
				if (count <= 0)
					Com_sprintf(popupText, sizeof(popupText), _("This soldier no longer has enough TUs to use the following items:\n\n"));
				Q_strcat(popupText, sizeof(popupText), "%s: %s (%i)\n", _(invList->def()->name), _(fireDef[i].name), fireDef[i].time);
				++count;
			}
		}
	}

	if ((Cmd_Argc() < 3 || atoi(Cmd_Argv(2)) == 0) && count > 0)
		UI_Popup(_("Warning"), popupText);

	char label[MAX_VAR];
	char tooltip[MAX_VAR];
	Com_sprintf(label, sizeof(label), "%g/%i %s %s", invWeight / WEIGHT_FACTOR, maxWeight, _("Kg"),
			(count > 0 ? _("Warning!") : ""));
	Com_sprintf(tooltip, sizeof(tooltip), "%s %i (%+i)", _("TU:"), tus, tuPenalty);
	UI_ExecuteConfunc("%s \"%s\" \"%s\" %f %i", Cmd_Argv(1), label, tooltip, WEIGHT_NORMAL_PENALTY - (1.0f - penalty), count);
}

void INV_InitCallbacks (void)
{
	Cmd_AddCommand("mn_increasefiremode", INV_IncreaseFiremode_f, "Increases the number of the firemode to display");
	Cmd_AddCommand("mn_decreasefiremode", INV_DecreaseFiremode_f, "Decreases the number of the firemode to display");
	Cmd_AddCommand("mn_increaseitem", INV_IncreaseItem_f, "Increases the number of the weapon or the ammo to display");
	Cmd_AddCommand("mn_decreaseitem", INV_DecreaseItem_f, "Decreases the number of the weapon or the ammo to display");
	Cmd_AddCommand("object_update", INV_UpdateObject_f, "Update the GUI with the selected item");
	Cmd_AddCommand("mn_updateactorload", INV_UpdateActorLoad_f, "Update the GUI with the selected actor inventory load");
}
