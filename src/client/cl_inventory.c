/**
 * @file cl_inventory.c
 * @brief General actor related inventory function for are used in every game mode
 * @note Inventory functions prefix: INV_
 * @todo Remove those functions that doesn't really belong here
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
#include "cl_game.h"
#include "../shared/parse.h"

/**
 * Collecting items functions.
 */

cvar_t *cl_equip;

/**
 * @brief Process items carried by soldiers.
 * @param[in] soldier Pointer to le_t being a soldier from our team.
 * @sa AII_CollectingItems
 * @todo this is campaign mode only - doesn't belong here
 */
void INV_CarriedItems (const le_t *soldier)
{
	int container;
	invList_t *item;
	technology_t *tech;

	for (container = 0; container < csi.numIDs; container++) {
		if (csi.ids[container].temp) /* Items collected as ET_ITEM */
			continue;
		for (item = soldier->i.c[container]; item; item = item->next) {
			/* Fake item. */
			assert(item->item.t);
			/* Twohanded weapons and container is left hand container. */
			/** @todo */
			/* assert(container == csi.idLeft && csi.ods[item->item.t].holdTwoHanded); */

			ccs.eMission.num[item->item.t->idx]++;
			/** @todo move techs away from here */
			tech = item->item.t->tech;
			if (!tech)
				Sys_Error("INV_CarriedItems: No tech for %s / %s\n", item->item.t->id, item->item.t->name);
			RS_MarkCollected(tech);

			if (!item->item.t->reload || item->item.a == 0)
				continue;
			ccs.eMission.numLoose[item->item.m->idx] += item->item.a;
			if (ccs.eMission.numLoose[item->item.m->idx] >= item->item.t->ammo) {
				ccs.eMission.numLoose[item->item.m->idx] -= item->item.t->ammo;
				ccs.eMission.num[item->item.m->idx]++;
			}
			/* The guys keep their weapons (half-)loaded. Auto-reload
			 * will happen at equip screen or at the start of next mission,
			 * but fully loaded weapons will not be reloaded even then. */
		}
	}
}

equipDef_t *INV_GetEquipmentDefinitionByID (const char *name)
{
	int i;

	for (i = 0, csi.eds; i < csi.numEDs; i++)
		if (!Q_strcmp(name, csi.eds[i].name))
			return &csi.eds[i];

	Com_DPrintf(DEBUG_CLIENT, "INV_GetEquipmentDefinitionByID: equipment %s not found.\n", name);
	return NULL;
}

/**
 * @brief Parses one "components" entry in a .ufo file and writes it into the next free entry in xxxxxxxx (components_t).
 * @param[in] name The unique id of a components_t array entry.
 * @param[in] text the whole following text after the "components" definition.
 * @sa CL_ParseScriptFirst
 */
void INV_ParseComponents (const char *name, const char **text)
{
	components_t *comp;
	const char *errhead = "INV_ParseComponents: unexpected end of file.";
	const char *token;

	/* get body */
	token = COM_Parse(text);
	if (!*text || *token != '{') {
		Com_Printf("INV_ParseComponents: \"%s\" components def without body ignored.\n", name);
		return;
	}
	if (gd.numComponents >= MAX_ASSEMBLIES) {
		Com_Printf("INV_ParseComponents: too many technology entries. limit is %i.\n", MAX_ASSEMBLIES);
		return;
	}

	/* New components-entry (next free entry in global comp-list) */
	comp = &gd.components[gd.numComponents];
	gd.numComponents++;

	memset(comp, 0, sizeof(*comp));

	/* set standard values */
	Q_strncpyz(comp->asId, name, sizeof(comp->asId));
	comp->asItem = INVSH_GetItemByID(comp->asId);
	Com_DPrintf(DEBUG_CLIENT, "INV_ParseComponents: linked item: %s with components: %s\n", name, comp->asId);

	do {
		/* get the name type */
		token = COM_EParse(text, errhead, name);
		if (!*text)
			break;
		if (*token == '}')
			break;

		/* get values */
		if (!Q_strncmp(token, "item", MAX_VAR)) {
			/* Defines what items need to be collected for this item to be researchable. */
			if (comp->numItemtypes < MAX_COMP) {
				/* Parse item name */
				token = COM_Parse(text);

				comp->items[comp->numItemtypes] = INVSH_GetItemByID(token);	/* item id -> item pointer */

				/* Parse number of items. */
				token = COM_Parse(text);
				comp->item_amount[comp->numItemtypes] = atoi(token);
				token = COM_Parse(text);
				comp->item_amount2[comp->numItemtypes] = atoi(token);

				/** @todo Set item links to NONE if needed */
				/* comp->item_idx[comp->numItemtypes] = xxx */

				comp->numItemtypes++;
			} else {
				Com_Printf("INV_ParseComponents: \"%s\" Too many 'items' defined. Limit is %i - ignored.\n", name, MAX_COMP);
			}
		} else if (!Q_strncmp(token, "time", MAX_VAR)) {
			/* Defines how long disassembly lasts. */
			token = COM_Parse(text);
			comp->time = atoi(token);
		} else {
			Com_Printf("INV_ParseComponents: Error in \"%s\" - unknown token: \"%s\".\n", name, token);
		}
	} while (*text);
}

/**
 * @brief Returns components definition for an item.
 * @param[in] item Item to search the components for.
 * @return comp Pointer to components_t definition.
 */
components_t *INV_GetComponentsByItem (const objDef_t *item)
{
	int i;

	for (i = 0; i < gd.numComponents; i++) {
		components_t *comp = &gd.components[i];
		if (comp->asItem == item) {
			Com_DPrintf(DEBUG_CLIENT, "INV_GetComponentsByItem: found components id: %s\n", comp->asId);
			return comp;
		}
	}
	Sys_Error("INV_GetComponentsByItem: could not find components id for: %s", item->id);
	return NULL;
}

/**
 * @brief Check if an item is stored in storage.
 * @param[in] obj Pointer to the item to check.
 * @return True if item is stored in storage.
 */
qboolean INV_ItemsIsStoredInStorage (const objDef_t *obj)
{
	/* antimatter is stored in antimatter storage */
	if (!Q_strncmp(obj->id, "antimatter", 10))
		return qfalse;

	/* aircraft are stored in hangars */
	assert(obj->tech);
	if (obj->tech->type == RS_CRAFT)
		return qfalse;

	return qtrue;
}

/**
 * @param[in] inv list where the item is currently in
 * @param[in] toContainer target container to place the item in
 * @param[in] px target x position in the toContainer container
 * @param[in] py target y position in the toContainer container
 * @param[in] fromContainer Container the item is in
 * @param[in] fromX X position of the item to move (in container fromContainer)
 * @param[in] fromY y position of the item to move (in container fromContainer)
 * @note If you set px or py to -1/NONE the item is automatically placed on a free
 * spot in the targetContainer
 * @return qtrue if the move was successful.
 */
qboolean INV_MoveItem (inventory_t* inv, const invDef_t * toContainer, int px, int py,
	const invDef_t * fromContainer, invList_t *fItem)
{
	int moved;

	if (px >= SHAPE_BIG_MAX_WIDTH || py >= SHAPE_BIG_MAX_HEIGHT)
		return qfalse;

	if (!fItem)
		return qfalse;

	/** @todo this case should be removed as soon as right clicking in equip container
	 * will try to put the item in a reasonable container automatically */
	if ((px == -1 || py == -1) && toContainer == fromContainer)
		return qtrue;

	/* move the item */
	moved = Com_MoveInInventory(inv, fromContainer, fItem, toContainer, px, py, NULL, NULL);

	switch (moved) {
	case IA_MOVE:
	case IA_ARMOUR:
		return qtrue;
	default:
		return qfalse;
	}
}

#ifdef DEBUG
/**
 * @brief Lists all object definitions.
 * @note called with debug_listinventory
 */
static void INV_InventoryList_f (void)
{
	int i;

	for (i = 0; i < csi.numODs; i++)
		INVSH_PrintItemDescription(&csi.ods[i]);
}
#endif

/**
 * @brief Make sure values of items after parsing are proper.
 */
qboolean INV_ItemsSanityCheck (void)
{
	int i;
	qboolean result = qtrue;

	for (i = 0; i < csi.numODs; i++) {
		const objDef_t *item = &csi.ods[i];

		/* Warn if item has no size set. */
		if (item->size <= 0 && !(item->notOnMarket && INV_ItemMatchesFilter(item, FILTER_DUMMY))) {
			result = qfalse;
			Com_Printf("INV_ItemsSanityCheck: Item %s has zero size set.\n", item->id);
		}

		/* Warn if no price is set. */
		if (item->price <= 0 && !item->notOnMarket) {
			result = qfalse;
			Com_Printf("INV_ItemsSanityCheck: Item %s has zero price set.\n", item->id);
		}

		if (item->price > 0 && item->notOnMarket && !PR_ItemIsProduceable(item)) {
			result = qfalse;
			Com_Printf("INV_ItemsSanityCheck: Item %s has a price set though it is neither available on the market and production.\n", item->id);
		}

		/* extension and headgear are mutual exclusive */
		if (item->extension && item->headgear) {
			result = qfalse;
			Com_Printf("INV_ItemsSanityCheck: Item %s has both extension and headgear set.\n",  item->id);
		}
	}

	return result;
}

/**
 * @brief Make sure equipment definitions used to generate teams are proper.
 * @note Check that the sum of all proabilities is smaller or equal to 100 for a weapon type.
 * @sa INVSH_EquipActor
 */
qboolean INV_EquipmentDefSanityCheck (void)
{
	int i, j;
	int sum;
	qboolean result = qtrue;

	for (i = 0; i < csi.numEDs; i++) {
		const equipDef_t const *ed = &csi.eds[i];
		/* only check definitions used for generating teams */
		if (Q_strncmp(ed->name, "alien", 5) && Q_strncmp(ed->name, "human", 5))
			continue;

		/* Check primary */
		sum = 0;
		for (j = 0; j < csi.numODs; j++) {
			const objDef_t const *obj = &csi.ods[j];
			if (obj->weapon && obj->fireTwoHanded
			 && (INV_ItemMatchesFilter(obj, FILTER_S_PRIMARY) || INV_ItemMatchesFilter(obj, FILTER_S_HEAVY)))
				sum += ed->num[j];
		}
		if (sum > 100) {
			Com_Printf("INV_EquipmentDefSanityCheck: Equipment Def '%s' has a total probability for primary weapons greater than 100\n", ed->name);
			result = qfalse;
		}

		/* Check secondary */
		sum = 0;
		for (j = 0; j < csi.numODs; j++) {
			const objDef_t const *obj = &csi.ods[j];
			if (obj->weapon && obj->reload && !obj->deplete && INV_ItemMatchesFilter(obj, FILTER_S_SECONDARY))
				sum += ed->num[j];
		}
		if (sum > 100) {
			Com_Printf("INV_EquipmentDefSanityCheck: Equipment Def '%s' has a total probability for secondary weapons greater than 100\n", ed->name);
			result = qfalse;
		}

		/* Check armour */
		sum = 0;
		for (j = 0; j < csi.numODs; j++) {
			const objDef_t const *obj = &csi.ods[j];
			if (INV_ItemMatchesFilter(obj, FILTER_S_ARMOUR))
				sum += ed->num[j];
		}
		if (sum > 100) {
			Com_Printf("INV_EquipmentDefSanityCheck: Equipment Def '%s' has a total probability for armours greater than 100\n", ed->name);
			result = qfalse;
		}

		/* Don't check misc: the total probability can be greater than 100 */
	}

	return result;
}

void INV_InitStartup (void)
{
	cl_equip = Cvar_Get("cl_equip", "multiplayer_initial", CVAR_USERINFO | CVAR_ARCHIVE, "Equipment that is used for none-campaign mode games");
#ifdef DEBUG
	Cmd_AddCommand("debug_listinventory", INV_InventoryList_f, "Print the current inventory to the game console");
#endif
}
