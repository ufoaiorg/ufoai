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

/**
 * @brief Transfer items carried by a soldier from one base to another.
 * @param[in] employee Pointer to employee.
 * @param[in] sourceBase Base where employee comes from.
 * @param[in] destBase Base where employee is going.
 * @todo this is campaign mode only - doesn't belong here
 */
void INV_TransferItemCarriedByChr (character_t *chr, base_t *sourceBase, base_t* destBase)
{
	invList_t *ic;
	int container;

	for (container = 0; container < csi.numIDs; container++) {
		for (ic = chr->inv.c[container]; ic; ic = ic->next) {
			objDef_t *obj = ic->item.t;
			B_UpdateStorageAndCapacity(sourceBase, obj, -1, qfalse, qfalse);
			B_UpdateStorageAndCapacity(destBase, obj, 1, qfalse, qfalse);

			obj = ic->item.m;
			if (obj) {
				B_UpdateStorageAndCapacity(sourceBase, obj, -1, qfalse, qfalse);
				B_UpdateStorageAndCapacity(destBase, obj, 1, qfalse, qfalse);
			}
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
 * @brief Disassembles item, adds components to base storage and calculates all components size.
 * @param[in] base Pointer to base where the disassembling is being made.
 * @param[in] comp Pointer to components definition.
 * @param[in] calculate True if this is only calculation of item size, false if this is real disassembling.
 * @return Size of all components in this disassembling.
 */
int INV_DisassemblyItem (base_t *base, components_t *comp, qboolean calculate)
{
	int i, size = 0;

	assert(comp);
	if (!calculate && !base)	/* We need base only if this is real disassembling. */
		Sys_Error("INV_DisassemblyItem: No base given");

	for (i = 0; i < comp->numItemtypes; i++) {
		const objDef_t *compOd = comp->items[i];
		assert(compOd);
		size += compOd->size * comp->item_amount[i];
		/* Add to base storage only if this is real disassembling, not calculation of size. */
		if (!calculate) {
			if (!Q_strncmp(compOd->id, "antimatter", 10))
				INV_ManageAntimatter(base, comp->item_amount[i], qtrue);
			else
				B_UpdateStorageAndCapacity(base, compOd, comp->item_amount[i], qfalse, qfalse);
			Com_DPrintf(DEBUG_CLIENT, "INV_DisassemblyItem: added %i amounts of %s\n", comp->item_amount[i], compOd->id);
		}
	}
	return size;
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
 * @brief Remove items until everything fits in storage.
 * @note items will be randomly selected for removal.
 * @param[in] base Pointer to the base
 */
void INV_RemoveItemsExceedingCapacity (base_t *base)
{
	int i;
	int objIdx[MAX_OBJDEFS];	/**< Will contain idx of items that can be removed */
	int numObj;

	if (base->capacities[CAP_ITEMS].cur <= base->capacities[CAP_ITEMS].max)
		return;

	for (i = 0, numObj = 0; i < csi.numODs; i++) {
		const objDef_t *obj = &csi.ods[i];

		if (!INV_ItemsIsStoredInStorage(obj))
			continue;

		/* Don't count item that we don't have in base */
		if (!base->storage.num[i])
			continue;

		objIdx[numObj++] = i;
	}

	/* UGV takes room in storage capacity: we store them with a value MAX_OBJDEFS that can't be used by objIdx */
	for (i = 0; i < E_CountHired(base, EMPL_ROBOT); i++) {
		objIdx[numObj++] = MAX_OBJDEFS;
	}

	while (numObj && base->capacities[CAP_ITEMS].cur > base->capacities[CAP_ITEMS].max) {
		/* Select the item to remove */
		const int randNumber = rand() % numObj;
		if (objIdx[randNumber] >= MAX_OBJDEFS) {
			/* A UGV is destroyed: get first one */
			employee_t* employee = E_GetHiredRobot(base, 0);
			/* There should be at least a UGV */
			assert(employee);
			E_DeleteEmployee(employee, EMPL_ROBOT);
		} else {
			/* items are destroyed. We guess that all items of a given type are stored in the same location
			 *	=> destroy all items of this type */
			const int idx = objIdx[randNumber];
			assert(idx >= 0);
			assert(idx < MAX_OBJDEFS);
			B_UpdateStorageAndCapacity(base, &csi.ods[idx], -base->storage.num[idx], qfalse, qfalse);
		}
		REMOVE_ELEM(objIdx, randNumber, numObj);

		/* Make sure that we don't have an infinite loop */
		if (numObj <= 0)
			break;
	}
	Com_DPrintf(DEBUG_CLIENT, "INV_RemoveItemsExceedingCapacity: Remains %i in storage for a maximum of %i\n",
		base->capacities[CAP_ITEMS].cur, base->capacities[CAP_ITEMS].max);
}

/**
 * @brief Remove ufos until everything fits in ufo hangars.
 * @param[in] base Pointer to the base
 * @param[in] ufohangar type
 */
void INV_RemoveUFOsExceedingCapacity (base_t *base, const buildingType_t buildingType)
{
	const baseCapacities_t capacity_type = B_GetCapacityFromBuildingType (buildingType);
	int i;
	int objIdx[MAX_OBJDEFS];	/**< Will contain idx of items that can be removed */
	int numObj;

	if (capacity_type != CAP_UFOHANGARS_SMALL && capacity_type != CAP_UFOHANGARS_LARGE)
		return;

	if (base->capacities[capacity_type].cur <= base->capacities[capacity_type].max)
		return;

	for (i = 0, numObj = 0; i < csi.numODs; i++) {
		const objDef_t *obj = &csi.ods[i];
		aircraft_t *ufocraft;

		/* Don't count what isn't an aircraft */
		assert(obj->tech);
		if (obj->tech->type != RS_CRAFT) {
			continue;
		}

		/* look for corresponding aircraft in global array */
		ufocraft = AIR_GetAircraft (obj->id);
		if (!ufocraft) {
			Com_DPrintf(DEBUG_CLIENT, "INV_RemoveUFOsExceedingCapacity: Did not find UFO %s\n", obj->id);
			continue;
		}

		if (ufocraft->size == AIRCRAFT_LARGE && capacity_type != CAP_UFOHANGARS_LARGE)
			continue;
		if (ufocraft->size == AIRCRAFT_SMALL && capacity_type != CAP_UFOHANGARS_SMALL)
			continue;

		/* Don't count item that we don't have in base */
		if (!base->storage.num[i])
			continue;

		objIdx[numObj++] = i;
	}

	while (numObj && base->capacities[capacity_type].cur > base->capacities[capacity_type].max) {
		/* Select the item to remove */
		const int randNumber = rand() % numObj;
		/* items are destroyed. We guess that all items of a given type are stored in the same location
		 *	=> destroy all items of this type */
		const int idx = objIdx[randNumber];

		assert(idx >= 0);
		assert(idx < MAX_OBJDEFS);
		B_UpdateStorageAndCapacity(base, &csi.ods[idx], -base->storage.num[idx], qfalse, qfalse);

		REMOVE_ELEM(objIdx, randNumber, numObj);
		UR_UpdateUFOHangarCapForAll(base);

		/* Make sure that we don't have an infinite loop */
		if (numObj <= 0)
			break;
	}
	Com_DPrintf(DEBUG_CLIENT, "INV_RemoveUFOsExceedingCapacity: Remains %i in storage for a maxium of %i\n",
		base->capacities[capacity_type].cur, base->capacities[capacity_type].max);
}

/**
 * @brief Update Storage Capacity.
 * @param[in] base Pointer to the base
 * @sa B_ResetAllStatusAndCapacities_f
 */
void INV_UpdateStorageCap (base_t *base)
{
	int i;

	base->capacities[CAP_ITEMS].cur = 0;

	for (i = 0; i < csi.numODs; i++) {
		const objDef_t *obj = &csi.ods[i];

		if (!INV_ItemsIsStoredInStorage(obj))
			continue;

		base->capacities[CAP_ITEMS].cur += base->storage.num[i] * obj->size;
	}

	/* UGV takes room in storage capacity */
	base->capacities[CAP_ITEMS].cur += UGV_SIZE * E_CountHired(base, EMPL_ROBOT);
}

/**
 * @brief Update Antimatter Capacity.
 * @param[in] base Pointer to the base
 * @sa B_ResetAllStatusAndCapacities_f
 */
void INV_UpdateAntimatterCap (base_t *base)
{
	int i;

	for (i = 0; i < csi.numODs; i++) {
		if (!Q_strncmp(csi.ods[i].id, "antimatter", 10)) {
			base->capacities[CAP_ANTIMATTER].cur = (base->storage.num[i] * ANTIMATTER_SIZE);
			return;
		}
	}
}

/**
 * @brief Manages Antimatter (adding, removing) through Antimatter Storage Facility.
 * @param[in] base Pointer to the base.
 * @param[in] amount quantity of antimatter to add/remove (> 0 even if antimatter is removed)
 * @param[in] add True if we are adding antimatter, false when removing.
 * @note This function should be called whenever we add or remove antimatter from Antimatter Storage Facility.
 * @note Call with amount = 0 if you want to remove ALL antimatter from given base.
 */
void INV_ManageAntimatter (base_t *base, int amount, qboolean add)
{
	int i, j;
	objDef_t *od;

	assert(base);

	if (!B_GetBuildingStatus(base, B_ANTIMATTER) && add) {
		Com_sprintf(mn.messageBuffer, sizeof(mn.messageBuffer),
			_("Base %s does not have Antimatter Storage Facility. %i units of Antimatter got removed."),
			base->name, amount);
		MN_AddNewMessage(_("Notice"), mn.messageBuffer, qfalse, MSG_STANDARD, NULL);
		return;
	}

	for (i = 0, od = csi.ods; i < csi.numODs; i++, od++) {
		if (!Q_strncmp(od->id, "antimatter", 10))
			break;
	}

	if (i == csi.numODs)
		Sys_Error("Could not find antimatter object definition");

	if (add) {	/* Adding. */
		if (base->capacities[CAP_ANTIMATTER].cur + (amount * ANTIMATTER_SIZE) <= base->capacities[CAP_ANTIMATTER].max) {
			base->storage.num[i] += amount;
			base->capacities[CAP_ANTIMATTER].cur += (amount * ANTIMATTER_SIZE);
		} else {
			for (j = 0; j < amount; j++) {
				if (base->capacities[CAP_ANTIMATTER].cur + ANTIMATTER_SIZE <= base->capacities[CAP_ANTIMATTER].max) {
					base->storage.num[i]++;
					base->capacities[CAP_ANTIMATTER].cur += ANTIMATTER_SIZE;
				} else
					break;
			}
		}
	} else {	/* Removing. */
		if (amount == 0) {
			base->capacities[CAP_ANTIMATTER].cur = 0;
			base->storage.num[i] = 0;
		} else {
			base->capacities[CAP_ANTIMATTER].cur -= amount * ANTIMATTER_SIZE;
			base->storage.num[i] -= amount;
		}
	}
}

/**
 * @brief Remove exceeding antimatter if an antimatter tank has been destroyed.
 * @param[in] base Pointer to the base.
 */
void INV_RemoveAntimatterExceedingCapacity (base_t *base)
{
	const int amount = ceil(((float) (base->capacities[CAP_ANTIMATTER].cur - base->capacities[CAP_ANTIMATTER].max)) / ANTIMATTER_SIZE);
	if (amount < 0)
		return;

	INV_ManageAntimatter(base, amount, qfalse);
}

#ifdef DEBUG
/**
 * @brief Lists all object definitions.
 * @note called with debug_listinventory
 */
void INV_InventoryList_f (void)
{
	int i;

	for (i = 0; i < csi.numODs; i++)
		INVSH_PrintItemDescription(&csi.ods[i]);
}
#endif

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
#endif

void INV_InitStartup (void)
{
	cl_equip = Cvar_Get("cl_equip", "multiplayer_initial", CVAR_USERINFO | CVAR_ARCHIVE, "Equipment that is used for none-campaign mode games");
}
