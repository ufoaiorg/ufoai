/**
 * @file inv_shared.c
 * @brief Common object-, inventory-, container- and firemode-related functions.
 * @note Functions prefix: INVSH_
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

#include "inv_shared.h"

/* this is used to access the skill and ability arrays for the current campaign */
static int globalCampaignID = -1;

static csi_t *CSI;
static invList_t *invUnused;
static item_t cacheItem = {NONE,NONE,NONE}; /* to crash as soon as possible */

/**
 * @brief
 * @param[in] import
 * @todo Move to INV_shared.c
 * @sa InitGame
 * @sa Com_ParseScripts
 */
void Com_InitCSI (csi_t * import)
{
	CSI = import;
}


/**
 * @brief
 * @param[in] invList
 * @todo Move to INV_shared.c
 * @sa InitGame
 * @sa CL_ResetSinglePlayerData
 * @sa CL_InitLocal
 */
void Com_InitInventory (invList_t * invList)
{
	invList_t *last;
	int i;

	assert(invList);
#ifdef DEBUG
	if (!invList)
		return;	/* never reached. need for code analyst. */
#endif

	invUnused = invList;
	invUnused->next = NULL;
	for (i = 0; i < MAX_INVLIST - 1; i++) {
		last = invUnused++;
		invUnused->next = last;
	}
}

static int cache_Com_CheckToInventory = 0;

/**
 * @brief
 * @param[in] i
 * @param[in] item
 * @param[in] container
 * @param[in] x The x value in the container (1 << x in the shape bitmask)
 * @param[in] y The x value in the container (SHAPE_BIG_MAX_HEIGHT is the max)
 * @todo Move to INV_shared.c
 * @sa
 */
qboolean Com_CheckToInventory (const inventory_t * const i, const int item, const int container, int x, int y)
{
	invList_t *ic;
	static uint32_t mask[SHAPE_BIG_MAX_HEIGHT];
	int j;

	assert(i);
#ifdef DEBUG
	if (!i)
		return qfalse;	/* never reached. need for code analyst. */
#endif

	assert((container >= 0) && (container < CSI->numIDs));
#ifdef DEBUG
	if ((container < 0) || (container >= CSI->numIDs))
		return qfalse;	/* never reached. need for code analyst. */
#endif

	/* armor vs item */
	if (!Q_strncmp(CSI->ods[item].type, "armor", MAX_VAR)) {
		if (!CSI->ids[container].armor && !CSI->ids[container].all) {
			return qfalse;
		}
	} else if (CSI->ods[item].extension) {
		if (!CSI->ids[container].extension && !CSI->ids[container].all) {
			return qfalse;
		}
	} else if (CSI->ods[item].headgear) {
		if (!CSI->ids[container].headgear && !CSI->ids[container].all) {
			return qfalse;
		}
	} else if (CSI->ids[container].armor) {
		return qfalse;
	} else if (CSI->ids[container].extension) {
		return qfalse;
	} else if (CSI->ids[container].headgear) {
		return qfalse;
	}

	/* twohanded item */
	if (CSI->ods[item].holdtwohanded) {
		if ((container == CSI->idRight && i->c[CSI->idLeft])
			 || container == CSI->idLeft)
			return qfalse;
	}

	/* left hand is busy if right wields twohanded */
	if (container == CSI->idLeft) {
		if (i->c[CSI->idRight] && CSI->ods[i->c[CSI->idRight]->item.t].holdtwohanded)
			return qfalse;

		/* can't put an item that is 'firetwohanded' into the left hand */
		if (CSI->ods[item].firetwohanded)
			return qfalse;
	}

	/* Single item containers, e.g. hands, extension or headgear. */
	if (CSI->ids[container].single) {
		if (i->c[container]) {
			/* There is already an item. */
			return qfalse;
		} else {
			/* Looks good - we are returning because we can ignore shape here. */
			/** @todo Check shape and rotate model to fit inside anyway. (e.g. monomolecular blade) */
			return qtrue;
		}
	}

	/* check bounds */
	if (x < 0 || y < 0 || x >= SHAPE_BIG_MAX_WIDTH || y >= SHAPE_BIG_MAX_HEIGHT)
		return qfalse;

	if (!cache_Com_CheckToInventory) {
		/* extract shape info */
		for (j = 0; j < SHAPE_BIG_MAX_HEIGHT; j++)
			mask[j] = ~CSI->ids[container].shape[j];

		/* add other items to mask */
		for (ic = i->c[container]; ic; ic = ic->next)
			for (j = 0; j < SHAPE_SMALL_MAX_HEIGHT && ic->y + j < SHAPE_BIG_MAX_HEIGHT; j++)
				mask[ic->y + j] |= ((CSI->ods[ic->item.t].shape >> (j * SHAPE_SMALL_MAX_WIDTH)) & 0xFF) << ic->x;
	}

	/* test for collisions with newly generated mask */
	for (j = 0; j < SHAPE_SMALL_MAX_HEIGHT; j++)
		if ((((CSI->ods[item].shape >> (j * SHAPE_SMALL_MAX_WIDTH)) & 0xFF) << x) & mask[y + j])
			return qfalse;

	/* everything ok */
	return qtrue;
}


/**
 * @brief Searches a suitable place in container of a given inventory
 * @param[in] i
 * @param[in] container
 * @param[in] x
 * @param[in] y
 * @todo Move to INV_shared.c
 */
invList_t *Com_SearchInInventory (const inventory_t* const i, int container, int x, int y)
{
	invList_t *ic;

	/* only a single item */
	if (CSI->ids[container].single)
		return i->c[container];

	/* more than one item - search for a suitable place in this container */
	for (ic = i->c[container]; ic; ic = ic->next)
		if (x >= ic->x && y >= ic->y
		&& x < ic->x + SHAPE_SMALL_MAX_WIDTH && y < ic->y + SHAPE_SMALL_MAX_HEIGHT
		&& ((CSI->ods[ic->item.t].shape >> (x - ic->x) >> (y - ic->y) * SHAPE_SMALL_MAX_WIDTH)) & 1)
			return ic;

	/* found nothing */
	return NULL;
}


/**
 * @brief Add an item to a specified container in a given inventory
 * @param[in] i
 * @param[in] item
 * @param[in] container
 * @param[in] x
 * @param[in] y
 * @todo Move to INV_shared.c
 */
invList_t *Com_AddToInventory (inventory_t * const i, item_t item, int container, int x, int y)
{
	invList_t *ic;

	if (item.t == NONE)
		return NULL;

	if (!invUnused) {
		Sys_Error("No free inventory space!\n");
		return NULL;	/* never reached. need for code analyst. */
	}

	assert(i);
#ifdef DEBUG
	if (!i)
		return NULL;	/* never reached. need for code analyst. */
#endif

	/* allocate space */
	ic = i->c[container];
	/* set container to next free invUnused slot */
	i->c[container] = invUnused;
	/* ensure, that invUnused will be the next empty slot */
	invUnused = invUnused->next;
	i->c[container]->next = ic;
	ic = i->c[container];
/*	Com_Printf("Add to container %i: %s\n", container, CSI->ods[item.t].id);*/

	/* set the data */
	ic->item = item;
	ic->x = x;
	ic->y = y;
	return ic;
}

/**
 * @brief
 * @param[in] i The inventory the container is in.
 * @param[in] container The container where the item should be removed.
 * @param[in] x The x position of the item (container?) to be removed. @todo: is this correct?
 * @param[in] y The y position of the item (container?) to be removed. @todo: is this correct?
 * @return qtrue If removal was successful.
 * @return qfalse If nothing was removed or an error occured.
 * @todo Move to INV_shared.c
 * @sa Com_RemoveFromInventoryIgnore
 */
qboolean Com_RemoveFromInventory (inventory_t* const i, int container, int x, int y)
{
	return Com_RemoveFromInventoryIgnore(i, container, x, y, qfalse);
}

/**
 * @brief
 * @param[in] i The inventory the container is in.
 * @param[in] container The container where the item should be removed.
 * @param[in] x The x position of the item (container?) to be removed. @todo: is this correct?
 * @param[in] y The y position of the item (container?) to be removed. @todo: is this correct?
 * @param[in] ignore_type Ignroes teh type of container (only used for a workaround in the base-equipemnt see CL_MoveMultiEquipment) HACKHACK
 * @return qtrue If removal was successful.
 * @return qfalse If nothing was removed or an error occured.
 * @todo Move to INV_shared.c
 * @sa Com_RemoveFromInventory
 */
qboolean Com_RemoveFromInventoryIgnore (inventory_t* const i, int container, int x, int y, qboolean ignore_type)
{
	invList_t *ic, *old;

	assert(i);
#ifdef DEBUG
	if (!i)
		return qfalse;	/* never reached. need for code analyst. */
#endif

	assert(container < MAX_CONTAINERS);

	ic = i->c[container];
	if (!ic) {
#ifdef PARANOID
		Com_DPrintf("Com_RemoveFromInventoryIgnore - empty container %s\n", CSI->ids[container].name);
#endif
		return qfalse;
	}

	if (!ignore_type && (CSI->ids[container].single || (ic->x == x && ic->y == y))) {
		old = invUnused;
		invUnused = ic;
		cacheItem = ic->item;
		i->c[container] = ic->next;

		if (CSI->ids[container].single && ic->next)
			Com_Printf("Com_RemoveFromInventoryIgnore: Error: single container %s has many items.\n", CSI->ids[container].name);

		invUnused->next = old;
		return qtrue;
	}

	for (; ic->next; ic = ic->next)
		if (ic->next->x == x && ic->next->y == y) {
			old = invUnused;
			invUnused = ic->next;
			cacheItem = ic->next->item;
			ic->next = ic->next->next;
			invUnused->next = old;
			return qtrue;
		}
	return qfalse;
}

/**
 * @brief Conditions for moving items between containers.
 * @param[in] i an item
 * @param[in] from source container
 * @param[in] fx x for source container
 * @param[in] fy y for source container
 * @param[in] to destination container
 * @param[in] tx x for destination container
 * @param[in] ty y for destination container
 * @param[in] TU amount of TU needed to move an item
 * @param[in] icp
 * @return IA_NOTIME when not enough TU
 * @return IA_NONE if no action possible
 * @return IA_NORELOAD if you cannot reload a weapon
 * @return IA_RELOAD_SWAP in case of exchange of ammo in a weapon
 * @return IA_RELOAD when reloading
 * @return IA_ARMOR when placing an armour on the actor
 * @return IA_MOVE when just moving an item
 * @todo Move to INV_shared.c
 * @sa
 */
int Com_MoveInInventory (inventory_t* const i, int from, int fx, int fy, int to, int tx, int ty, int *TU, invList_t ** icp)
{
	return Com_MoveInInventoryIgnore(i, from, fx, fy, to, tx, ty, TU, icp, qfalse);
}

/**
 * @brief Conditions for moving items between containers.
 * @param[in] i an item
 * @param[in] from source container
 * @param[in] fx x for source container
 * @param[in] fy y for source container
 * @param[in] to destination container
 * @param[in] tx x for destination container
 * @param[in] ty y for destination container
 * @param[in] TU amount of TU needed to move an item
 * @param[in] icp
 * @param[in] ignore_type Ignroes teh type of container (only used for a workaround in the base-equipemnt see CL_MoveMultiEquipment) HACKHACK
 * @return IA_NOTIME when not enough TU
 * @return IA_NONE if no action possible
 * @return IA_NORELOAD if you cannot reload a weapon
 * @return IA_RELOAD_SWAP in case of exchange of ammo in a weapon
 * @return IA_RELOAD when reloading
 * @return IA_ARMOR when placing an armour on the actor
 * @return IA_MOVE when just moving an item
 */
int Com_MoveInInventoryIgnore (inventory_t* const i, int from, int fx, int fy, int to, int tx, int ty, int *TU, invList_t ** icp, qboolean ignore_type)
{
	invList_t *ic;
	int time;

	assert(to >= 0 && to < CSI->numIDs);
	assert(from >= 0 && from < CSI->numIDs);
#ifdef DEBUG
	if (from < 0 || from >= CSI->numIDs)
		return 0;	/* never reached. need for code analyst. */
#endif

	if (icp)
		*icp = NULL;

	if (from == to && fx == tx && fy == ty)
		return IA_NONE;

	time = CSI->ids[from].out + CSI->ids[to].in;
	if (from == to)
		time /= 2;
	if (TU && *TU < time)
		return IA_NOTIME;

	assert(i);

	/* break if source item is not removeable */

	if (!Com_RemoveFromInventoryIgnore(i, from, fx, fy, ignore_type))
		return IA_NONE;

	if (cacheItem.t == NONE)
		return IA_NONE;

	assert(cacheItem.t < MAX_OBJDEFS);

	/* We are in base-equip screen (multi-ammo workaround) and can skip a lot of checks. */
	if (ignore_type) {
		Com_TryAddToBuyType(i, cacheItem, to); /* get target coordinates */
		/* return data */
		/*if (icp)
			*icp = ic;*/
		return IA_NONE;
	}

	/* if weapon is twohanded and is moved from hand to hand do nothing. */
	/* twohanded weapon are only in CSI->idRight */
	if (CSI->ods[cacheItem.t].firetwohanded && to == CSI->idLeft && from == CSI->idRight) {
		Com_AddToInventory(i, cacheItem, from, fx, fy);
		return IA_NONE;
	}

	/* if non-armor moved to an armor slot then */
	/* move item back to source location and break */
	/* same for non extension items when moved to an extension slot */
	if ((CSI->ids[to].armor && Q_strcmp(CSI->ods[cacheItem.t].type, "armor"))
	 || (CSI->ids[to].extension && !CSI->ods[cacheItem.t].extension)
	 || (CSI->ids[to].headgear && !CSI->ods[cacheItem.t].headgear)) {
		Com_AddToInventory(i, cacheItem, from, fx, fy);
		return IA_NONE;
	}

	/* check if the target is a blocked inv-armor and source!=dest */
	if (CSI->ids[to].armor && from != to && !Com_CheckToInventory(i, cacheItem.t, to, tx, ty)) {
		item_t cacheItem2;

		/* save/cache (source) item */
		cacheItem2 = cacheItem;
		/* move the destination item to the source */
		Com_MoveInInventory(i, to, tx, ty, from, fx, fy, TU, icp);
		/* reset the cached item (source) and move it to the container emptied by destination item */
		Com_AddToInventory(i, cacheItem2, from, fx, fy);
		cacheItem = cacheItem2;
	} else if (!Com_CheckToInventory(i, cacheItem.t, to, tx, ty)) {
		ic = Com_SearchInInventory(i, to, tx, ty);

		if (ic && INV_LoadableInWeapon(&CSI->ods[cacheItem.t], ic->item.t)) {
			/* @todo (or do this in two places in cl_menu.c):
			if (!RS_ItemIsResearched(CSI->ods[ic->item.t].id)
				 || !RS_ItemIsResearched(CSI->ods[cacheItem.t].id)) {
				return IA_NORELOAD;
			} */
			if (ic->item.a >= CSI->ods[ic->item.t].ammo
				&& ic->item.m == cacheItem.t) {
				/* weapon already fully loaded with the same ammunition
				   --- back to source location */
				Com_AddToInventory(i, cacheItem, from, fx, fy);
				return IA_NORELOAD;
			}
			time += CSI->ods[ic->item.t].reload;
			if (!TU || *TU >= time) {
				if (TU)
					*TU -= time;
				if (ic->item.a >= CSI->ods[ic->item.t].ammo) {
					/* exchange ammo */
					item_t item = {NONE_AMMO, NONE, ic->item.m};

					Com_AddToInventory(i, item, from, fx, fy);

					ic->item.m = cacheItem.t;
					if (icp)
						*icp = ic;
					return IA_RELOAD_SWAP;
				} else {
					ic->item.m = cacheItem.t;
					/* loose ammo of type ic->item.m saved on server side */
					ic->item.a = CSI->ods[ic->item.t].ammo;
					if (icp)
						*icp = ic;
					return IA_RELOAD;
				}
			}
			/* not enough time - back to source location */
			Com_AddToInventory(i, cacheItem, from, fx, fy);
			return IA_NOTIME;
		}

		/* impossible move - back to source location */
		Com_AddToInventory(i, cacheItem, from, fx, fy);
		return IA_NONE;
	}

	/* twohanded exception - only CSI->idRight is allowed for firetwohanded weapons */
	if (CSI->ods[cacheItem.t].firetwohanded && to == CSI->idLeft) {
#ifdef DEBUG
		Com_DPrintf("Com_MoveInInventory - don't move the item to CSI->idLeft it's firetwohanded\n");
#endif
		to = CSI->idRight;
	}
#ifdef PARANOID
	else if (CSI->ods[cacheItem.t].firetwohanded)
		Com_DPrintf("Com_MoveInInventory: move firetwohanded item to container: %s\n", CSI->ids[to].name);
#endif

	/* successful */
	if (TU)
		*TU -= time;

	Com_RemoveFromInventory(i, from, fx, fy);
	ic = Com_AddToInventory(i, cacheItem, to, tx, ty);

	/* return data */
	if (icp)
		*icp = ic;

	if (to == CSI->idArmor) {
		assert(!Q_strcmp(CSI->ods[cacheItem.t].type, "armor"));
		return IA_ARMOR;
	} else
		return IA_MOVE;
}

/**
 * @brief
 * @param[in] i
 * @param[in] container
 * @todo Move to INV_shared.c
 * @sa Com_DestroyInventory
 */
void Com_EmptyContainer (inventory_t* const i, const int container)
{
	invList_t *ic, *old;

#ifdef DEBUG
	int cnt = 0;
	if (CSI->ids[container].temp)
		Com_DPrintf("Com_EmptyContainer: Emptying temp container %s.\n", CSI->ids[container].name);
#endif

	assert(i);
#ifdef DEBUG
	if (!i)
		return;	/* never reached. need for code analyst. */
#endif

	ic = i->c[container];

	while (ic) {
		old = ic;
		ic = ic->next;
		old->next = invUnused;
		invUnused = old;
#ifdef DEBUG
		if (cnt >= MAX_INVLIST) {
			Com_Printf("Error: There are more than the allowed entries in container %s (cnt:%i, MAX_INVLIST:%i) (Com_EmptyContainer)\n", CSI->ids[container].name, cnt,
					   MAX_INVLIST);
			break;
		}
		cnt++;
#endif
	}
	i->c[container] = NULL;
}

/**
 * @brief
 * @param i The invetory which should be erased
 * @todo Move to INV_shared.c
 * @sa Com_EmptyContainer
 */
void Com_DestroyInventory (inventory_t* const i)
{
	int k;

	if (!i)
		return;

	for (k = 0; k < CSI->numIDs; k++)
		if (CSI->ids[k].temp)
			i->c[k] = NULL;
		else
			Com_EmptyContainer(i, k);
}

/**
 * @brief Finds space for item in inv at container
 * @param[in] inv
 * @param[in] time
 * @param[in] container
 * @param[in] px
 * @param[in] py
 * @todo Move to INV_shared.c
 * @sa Com_CheckToInventory
 */
void Com_FindSpace (const inventory_t* const inv, const int item, const int container, int* const px, int* const py)
{
	int x, y;

	assert(inv);
	assert(!cache_Com_CheckToInventory);

	for (y = 0; y < SHAPE_BIG_MAX_HEIGHT; y++)
		for (x = 0; x < SHAPE_BIG_MAX_WIDTH; x++)
			if (Com_CheckToInventory(inv, item, container, x, y)) {
				cache_Com_CheckToInventory = 0;
				*px = x;
				*py = y;
				return;
			} else {
				cache_Com_CheckToInventory = 1;
			}
	cache_Com_CheckToInventory = 0;

#ifdef PARANOID
	Com_DPrintf("Com_FindSpace: no space for %s: %s in %s\n", CSI->ods[item].type, CSI->ods[item].id, CSI->ids[container].name);
#endif
	*px = *py = NONE;
}

/**
 * @brief Tries to add item to inventory inv at container
 * @param[in] inv Inventory pointer to add the item
 * @param[in] item Item to add to inventory
 * @param[in] container Container id
 * @todo Move to INV_shared.c
 * @sa Com_FindSpace
 * @sa Com_AddToInventory
 */
int Com_TryAddToInventory (inventory_t* const inv, item_t item, int container)
{
	int x, y;

	Com_FindSpace(inv, item.t, container, &x, &y);
	if (x == NONE) {
		assert(y == NONE);
		return 0;
	} else {
		Com_AddToInventory(inv, item, container, x, y);
		return 1;
	}
}

/**
 * @brief Tries to add item to buytype inventory inv at container
 * @param[in] inv Inventory pointer to add the item
 * @param[in] item Item to add to inventory
 * @param[in] container Container id
 * @todo Move to INV_shared.c
 * @sa Com_FindSpace
 * @sa Com_AddToInventory
 */
int Com_TryAddToBuyType (inventory_t* const inv, item_t item, int container)
{
	int x, y;
	inventory_t hackInv;

	/* link the temp container */
	hackInv.c[CSI->idEquip] = inv->c[container];

	Com_FindSpace(&hackInv, item.t, CSI->idEquip, &x, &y);
	if (x == NONE) {
		assert(y == NONE);
		return 0;
	} else {
		Com_AddToInventory(&hackInv, item, CSI->idEquip, x, y);
		inv->c[container] = hackInv.c[CSI->idEquip];
		return 1;
	}
}

/**
 * @brief Debug function to print the inventory items for a given inventory_t pointer
 * @param[in] i The inventory you want to see on the game console
 * @todo Move to INV_shared.c
 */
void INV_PrintToConsole (inventory_t* const i)
{
	int container;
	invList_t *ic;

	assert(i);

	for (container = 0; container < CSI->numIDs; container++) {
		ic = i->c[container];
		Com_Printf("Container: %i\n", container);
		while (ic) {
			Com_Printf(".. item.t: %i, item.m: %i, item.a: %i, x: %i, y: %i\n", ic->item.t, ic->item.m, ic->item.a, ic->x, ic->y);
			if (ic->item.t != NONE)
				Com_Printf(".... weapon: %s\n", CSI->ods[ic->item.t].id);
			if (ic->item.m != NONE)
				Com_Printf(".... ammo:   %s (%i)\n", CSI->ods[ic->item.m].id, ic->item.a);
			ic = ic->next;
		}
	}
}

/*
==============================================================================
CHARACTER GENERATION AND HANDLING
==============================================================================
*/

#define AKIMBO_CHANCE		0.3
#define WEAPONLESS_BONUS	0.4
#define PROB_COMPENSATION   3.0

/**
 * @brief Pack a weapon, possibly with some ammo
 * @param[in] inv The inventory that will get the weapon
 * @param[in] weapon The weapon type index in gi.csi->ods
 * @param[in] equip The equipment that shows how many clips to pack
 * @param[in] name The name of the equipment for debug messages
 * @todo Move to INV_shared.c
 * @sa INV_LoadableInWeapon
 */
static int Com_PackAmmoAndWeapon (inventory_t* const inv, const int weapon, const int equip[MAX_OBJDEFS], int missed_primary, const char *name)
{
	int ammo = -1; /* this variable is never used before being set */
	item_t item = {NONE_AMMO, NONE, NONE};
	int i, max_price, prev_price;
	objDef_t obj;
	qboolean allowLeft;
	qboolean packed;
	int ammoMult = 1;

#ifdef PARANOID
	if (weapon < 0) {
		Com_Printf("Error in Com_PackAmmoAndWeapon - weapon is %i\n", weapon);
	}
#endif

	assert(Q_strcmp(CSI->ods[weapon].type, "armor"));
	item.t = weapon;

	/* are we going to allow trying the left hand */
	allowLeft = !(inv->c[CSI->idRight] && CSI->ods[inv->c[CSI->idRight]->item.t].firetwohanded);

	if (!CSI->ods[weapon].reload) {
		item.m = item.t; /* no ammo needed, so fire definitions are in t */
	} else {
		if (CSI->ods[weapon].oneshot) {
			/* The weapon provides its own ammo (i.e. it is charged or loaded in the base.) */
			item.a = CSI->ods[weapon].ammo;
			item.m = weapon;
			Com_DPrintf("Com_PackAmmoAndWeapon: oneshot weapon '%s' in equipment '%s'.\n", CSI->ods[weapon].id, name);
		} else {
			max_price = 0;
			/* find some suitable ammo for the weapon */
			for (i = CSI->numODs - 1; i >= 0; i--)
				if (equip[i]
				&& INV_LoadableInWeapon(&CSI->ods[i], weapon)
				&& (CSI->ods[i].price > max_price) ) {
					ammo = i;
					max_price = CSI->ods[i].price;
				}

			if (ammo < 0) {
				Com_DPrintf("Com_PackAmmoAndWeapon: no ammo for sidearm or primary weapon '%s' in equipment '%s'.\n", CSI->ods[weapon].id, name);
				return 0;
			}
			/* load ammo */
			item.a = CSI->ods[weapon].ammo;
			item.m = ammo;
		}
	}

	if (item.m == NONE) {
		Com_Printf("Com_PackAmmoAndWeapon: no ammo for sidearm or primary weapon '%s' in equipment '%s'.\n", CSI->ods[weapon].id, name);
		return 0;
	}

	/* now try to pack the weapon */
	packed = Com_TryAddToInventory(inv, item, CSI->idRight);
	if (packed)
		ammoMult = 3;
	if (!packed && allowLeft)
		packed = Com_TryAddToInventory(inv, item, CSI->idLeft);
	if (!packed)
		packed = Com_TryAddToInventory(inv, item, CSI->idBelt);
	if (!packed)
		packed = Com_TryAddToInventory(inv, item, CSI->idHolster);
	if (!packed)
		return 0;

	max_price = INT_MAX;
	do {
		/* search for the most expensive matching ammo in the equipment */
		prev_price = max_price;
		max_price = 0;
		for (i = 0; i < CSI->numODs; i++) {
			obj = CSI->ods[i];
			if (equip[i] && INV_LoadableInWeapon(&obj, weapon)) {
				if (obj.price > max_price && obj.price < prev_price) {
					max_price = obj.price;
					ammo = i;
				}
			}
		}
		/* see if there is any */
		if (max_price) {
			int num;
			int numpacked = 0;

			/* how many clips? */
			num = min(
				equip[ammo] / equip[weapon]
				+ (equip[ammo] % equip[weapon] > rand() % equip[weapon])
				+ (PROB_COMPENSATION > 40 * frand())
				+ (float) missed_primary * (1 + frand() * PROB_COMPENSATION) / 40.0, 20);

			assert(num >= 0);
			/* pack some more ammo */
			while (num--) {
				item_t mun = {NONE_AMMO, NONE, NONE};

				mun.t = ammo;
				/* ammo to backpack; belt is for knives and grenades */
				numpacked += Com_TryAddToInventory(inv, mun, CSI->idBackpack);
				/* no problem if no space left; one ammo already loaded */
				if (numpacked > ammoMult || numpacked*CSI->ods[weapon].ammo > 11)
					break;
			}
		}
	} while (max_price);

	return qtrue;
}


/**
 * @brief Fully equip one actor
 * @param[in] inv The inventory that will get the weapon
 * @param[in] equip The equipment that shows what is available
 * @param[in] name The name of the equipment for debug messages
 * @param[in] chr Pointer to character data - to get the weapon and armor bools
 * @note The code below is a complete implementation
 * of the scheme sketched at the beginning of equipment_missions.ufo.
 * Beware: if two weapons in the same category have the same price,
 * @todo Move to INV_shared.c
 * only one will be considered for inventory.
 */
void Com_EquipActor (inventory_t* const inv, const int equip[MAX_OBJDEFS], const char *name, character_t* chr)
{
	int weapon = -1; /* this variable is never used before being set */
	int i, max_price, prev_price;
	int has_weapon = 0, has_armor = 0, repeat = 0, missed_primary = 0;
	int primary = 2; /* 0 particle or normal, 1 other, 2 no primary weapon */
	objDef_t obj;

	if (chr->weapons) {
		/* primary weapons */
		max_price = INT_MAX;
		do {
			int lastPos = CSI->numODs - 1;
			/* search for the most expensive primary weapon in the equipment */
			prev_price = max_price;
			max_price = 0;
			for (i = lastPos; i >= 0; i--) {
				obj = CSI->ods[i];
				if (equip[i] && obj.weapon && BUY_PRI(obj.buytype) && obj.firetwohanded) {
					if (frand() < 0.15) { /* small chance to pick any weapon */
						weapon = i;
						max_price = obj.price;
						lastPos = i - 1;
						break;
					} else if (obj.price > max_price && obj.price < prev_price) {
						max_price = obj.price;
						weapon = i;
						lastPos = i - 1;
					}
				}
			}
			/* see if there is any */
			if (max_price) {
				/* see if the actor picks it */
				if (equip[weapon] >= (28 - PROB_COMPENSATION) * frand()) {
					/* not decrementing equip[weapon]
					* so that we get more possible squads */
					has_weapon += Com_PackAmmoAndWeapon(inv, weapon, equip, 0, name);
					if (has_weapon) {
						int ammo;

						/* find the first possible ammo to check damage type */
						for (ammo = 0; ammo < CSI->numODs; ammo++)
							if (equip[ammo]
							&& INV_LoadableInWeapon(&CSI->ods[ammo], weapon))
								break;
						if (ammo < CSI->numODs) {
							primary =
								/* to avoid two particle weapons */
								!(CSI->ods[ammo].fd[0][0].dmgtype
								== CSI->damParticle)
								/* to avoid SMG + Assault Rifle */
								&& !(CSI->ods[ammo].fd[0][0].dmgtype
									== CSI->damNormal); /* fd[0][0] Seems to be ok here since we just check the damage type and they are the same for all fds i've found. */
						}
						max_price = 0; /* one primary weapon is enough */
						missed_primary = 0;
					}
				} else {
					missed_primary += equip[weapon];
				}
			}
		} while (max_price);

		/* sidearms (secondary weapons with reload) */
		if (!has_weapon)
			repeat = WEAPONLESS_BONUS > frand();
		else
			repeat = 0;
		do {
			max_price = primary ? INT_MAX : 0;
			do {
				prev_price = max_price;
				/* if primary is a particle or normal damage weapon,
				we pick cheapest sidearms first */
				max_price = primary ? 0 : INT_MAX;
				for (i = 0; i < CSI->numODs; i++) {
					obj = CSI->ods[i];
					if (equip[i] && obj.weapon
						&& BUY_SEC(obj.buytype) && obj.reload) {
						if (primary
							? obj.price > max_price && obj.price < prev_price
							: obj.price < max_price && obj.price > prev_price) {
							max_price = obj.price;
							weapon = i;
						}
					}
				}
				if (!(max_price == (primary ? 0 : INT_MAX))) {
					if (equip[weapon] >= 40 * frand()) {
						has_weapon += Com_PackAmmoAndWeapon(inv, weapon, equip, missed_primary, name);
						if (has_weapon) {
							/* try to get the second akimbo pistol */
							if (primary == 2
								&& !CSI->ods[weapon].firetwohanded
								&& frand() < AKIMBO_CHANCE) {
								Com_PackAmmoAndWeapon(inv, weapon, equip, 0, name);
							}
							/* enough sidearms */
							max_price = primary ? 0 : INT_MAX;
						}
					}
				}
			} while (!(max_price == (primary ? 0 : INT_MAX)));
		} while (!has_weapon && repeat--);

		/* misc items and secondary weapons without reload */
		if (!has_weapon)
			repeat = WEAPONLESS_BONUS > frand();
		else
			repeat = 0;
		do {
			max_price = INT_MAX;
			do {
				prev_price = max_price;
				max_price = 0;
				for (i = 0; i < CSI->numODs; i++) {
					obj = CSI->ods[i];
					if (equip[i] && ((obj.weapon && BUY_SEC(obj.buytype) && !obj.reload)
							|| obj.buytype == BUY_MISC) ) {
						if (obj.price > max_price && obj.price < prev_price) {
							max_price = obj.price;
							weapon = i;
						}
					}
				}
				if (max_price) {
					int num;

					num = equip[weapon] / 40 + (equip[weapon] % 40 >= 40 * frand());
					while (num--)
						has_weapon += Com_PackAmmoAndWeapon(inv, weapon, equip, 0, name);
				}
			} while (max_price);
		} while (repeat--); /* gives more if no serious weapons */

		/* if no weapon at all, bad guys will always find a blade to wield */
		if (!has_weapon) {
			Com_DPrintf("Com_EquipActor: no weapon picked in equipment '%s', defaulting to the most expensive secondary weapon without reload.\n", name);
			max_price = 0;
			for (i = 0; i < CSI->numODs; i++) {
				obj = CSI->ods[i];
				if (equip[i] && obj.weapon && BUY_SEC(obj.buytype) && !obj.reload) {
					if (obj.price > max_price && obj.price < prev_price) {
						max_price = obj.price;
						weapon = i;
					}
				}
			}
			if (max_price)
				has_weapon += Com_PackAmmoAndWeapon(inv, weapon, equip, 0, name);
		}
		/* if still no weapon, something is broken, or no blades in equip */
		if (!has_weapon)
			Com_DPrintf("Com_EquipActor: cannot add any weapon; no secondary weapon without reload detected for equipment '%s'.\n", name);

		/* armor; especially for those without primary weapons */
		repeat = (float) missed_primary * (1 + frand() * PROB_COMPENSATION) / 40.0;
	} else {
		Com_DPrintf("Com_EquipActor: character '%s' may not carry weapons\n", chr->name);
		return;
	}

	if (!chr->armor) {
		Com_DPrintf("Com_EquipActor: character '%s' may not carry armor\n", chr->name);
		return;
	}

	do {
		max_price = INT_MAX;
		do {
			prev_price = max_price;
			max_price = 0;
			for (i = 0; i < CSI->numODs; i++) {
				obj = CSI->ods[i];
				if (equip[i] && obj.buytype == BUY_ARMOUR) {
					if (obj.price > max_price && obj.price < prev_price) {
						max_price = obj.price;
						weapon = i;
					}
				}
			}
			if (max_price) {
				if (equip[weapon] >= 40 * frand()) {
					item_t item = {NONE_AMMO, NONE, NONE};

					item.t = weapon;
					if (Com_TryAddToInventory(inv, item, CSI->idArmor)) {
						has_armor++;
						max_price = 0; /* one armor is enough */
					}
				}
			}
		} while (max_price);
	} while (!has_armor && repeat--);
}

/**
 * @brief Translate the team string to the team int value
 * @sa TEAM_ALIEN, TEAM_CIVILIAN, TEAM_PHALANX
 * @param[in] teamString
 */
int Com_StringToTeamNum (const char* teamString)
{
	if (!Q_strncmp(teamString, "TEAM_PHALANX", MAX_VAR))
		return TEAM_PHALANX;
	if (!Q_strncmp(teamString, "TEAM_CIVILIAN", MAX_VAR))
		return TEAM_CIVILIAN;
	if (!Q_strncmp(teamString, "TEAM_ALIEN", MAX_VAR))
		return TEAM_ALIEN;
	/* there may be other ortnok teams - only check first 6 characters */
	Com_Printf("Com_StringToTeamNum: Unknown teamString: '%s'\n", teamString);
	return -1;
}

/* min and max values for all teams can be defined via campaign script */
int skillValues[MAX_CAMPAIGNS][MAX_TEAMS][MAX_EMPL][2];
int abilityValues[MAX_CAMPAIGNS][MAX_TEAMS][MAX_EMPL][2];

/**
 * @brief Fills the min and max values for abilities for the given character
 * @param[in] chr For which character - needed to check empl_type
 * @param[in] team TEAM_ALIEN, TEAM_CIVILIAN, ...
 * @param[in] minAbility Pointer to minAbility int value to use for this character
 * @param[in] maxAbility Pointer to maxAbility int value to use for this character
 * @sa Com_CharGenAbilitySkills
 */
static void Com_GetAbility (character_t *chr, int team, int *minAbility, int *maxAbility, int campaignID)
{
	*minAbility = *maxAbility = 0;
	/* some default values */
	switch (chr->empl_type) {
	case EMPL_SOLDIER:
		*minAbility = 15;
		*maxAbility = 75;
		break;
	case EMPL_MEDIC:
		*minAbility = 15;
		*maxAbility = 75;
		break;
	case EMPL_SCIENTIST:
		*minAbility = 15;
		*maxAbility = 75;
		break;
	case EMPL_WORKER:
		*minAbility = 15;
		*maxAbility = 50;
		break;
	case EMPL_ROBOT:
		*minAbility = 80;
		*maxAbility = 80;
		break;
	default:
		Sys_Error("Com_GetAbility: Unknown employee type: %i\n", chr->empl_type);
	}
	if (team == TEAM_ALIEN) {
		*minAbility = 0;
		*maxAbility = 100;
	} else if (team == TEAM_CIVILIAN) {
		*minAbility = 0;
		*maxAbility = 20;
	}
	/* we always need both values - min and max - otherwise it was already a Sys_Error at parsing time */
	if (campaignID >= 0 && abilityValues[campaignID][team][chr->empl_type][0] >= 0) {
		*minAbility = abilityValues[campaignID][team][chr->empl_type][0];
		*maxAbility = abilityValues[campaignID][team][chr->empl_type][1];
	}
#ifdef PARANOID
	Com_DPrintf("Com_GetAbility: use minAbility %i and maxAbility %i for team %i and empl_type %i\n", *minAbility, *maxAbility, team, chr->empl_type);
#endif
}

/**
 * @brief Fills the min and max values for skill for the given character
 * @param[in] chr For which character - needed to check empl_type
 * @param[in] team TEAM_ALIEN, TEAM_CIVILIAN, ...
 * @param[in] minSkill Pointer to minSkill int value to use for this character
 * @param[in] maxSkill Pointer to maxSkill int value to use for this character
 * @sa Com_CharGenAbilitySkills
 */
static void Com_GetSkill (character_t *chr, int team, int *minSkill, int *maxSkill, int campaignID)
{
	*minSkill = *maxSkill = 0;
	/* some default values */
	switch (chr->empl_type) {
	case EMPL_SOLDIER:
		*minSkill = 15;
		*maxSkill = 75;
		break;
	case EMPL_MEDIC:
		*minSkill = 15;
		*maxSkill = 75;
		break;
	case EMPL_SCIENTIST:
		*minSkill = 15;
		*maxSkill = 75;
		break;
	case EMPL_WORKER:
		*minSkill = 15;
		*maxSkill = 50;
		break;
	case EMPL_ROBOT:
		*minSkill = 80;
		*maxSkill = 80;
		break;
	default:
		Sys_Error("Com_GetSkill: Unknown employee type: %i\n", chr->empl_type);
	}
	if (team == TEAM_ALIEN) {
		*minSkill = 0;
		*maxSkill = 100;
	} else if (team == TEAM_CIVILIAN) {
		*minSkill = 0;
		*maxSkill = 20;
	}
	/* we always need both values - min and max - otherwise it was already a Sys_Error at parsing time */
	if (campaignID >= 0 && skillValues[campaignID][team][chr->empl_type][0] >= 0) {
		*minSkill = skillValues[campaignID][team][chr->empl_type][0];
		*maxSkill = skillValues[campaignID][team][chr->empl_type][1];
	}
#ifdef PARANOID
	Com_DPrintf("Com_GetSkill: use minSkill %i and maxSkill %i for team %i and empl_type %i\n", *minSkill, *maxSkill, team, chr->empl_type);
#endif
}

/**
 * @brief Sets the current active campaign id (see curCampaign pointer)
 * @note used to access the right values from skillValues and abilityValues
 * @sa CL_GameInit
 */
void Com_SetGlobalCampaignID (int campaignID)
{
	globalCampaignID = campaignID;
}

/**
 * TRAINING_SCALAR defines how much or little a characters skill set influences
 * their abilities using a basic mapping of skills to abilities. Higher numbers
 * results in less influence of natural ability on skill stats. Zero means
 * natural abilities do not influence skill numbers.
 */
#define TRAINING_SCALAR	3

/**
 * @brief Generate a skill and ability set for any character.
 *
 * Character stats are intended to be based on definitions in campaign.ufo,
 * resulting in more difficult campaigns yielding stronger aliens (and more feeble
 * soldiers.) Skills (CLOSE, HEAVY, ASSAULT, SNIPER, EXPLOSIVE) are further
 * influenced by the characters natural abilities (POWER, SPEED, ACCURACY, MIND)
 * @param[in] chr
 * @param[in] team
 * @sa Com_GetAbility
 * @sa Com_GetSkill
 * @sa Com_SetGlobalCampaignID
 */
void Com_CharGenAbilitySkills (character_t * chr, int team)
{
	int i, skillWindow, abilityWindow, training, ability1, ability2;
	float windowScalar;
	int minAbility = 0, maxAbility = 0, minSkill = 0, maxSkill = 0;

	assert(chr);

	/**
	 * Build the acceptable ranges for skills / abilities for this character on this team
	 * as appropriate for this campaign
	 */
	Com_GetAbility(chr, team, &minAbility, &maxAbility, globalCampaignID);
	Com_GetSkill(chr, team, &minSkill, &maxSkill, globalCampaignID);

	/* Abilities -- random within the range */
	abilityWindow = maxAbility - minAbility;
	for (i = 0; i < ABILITY_NUM_TYPES; i++) {
		chr->skills[i] = (frand() * abilityWindow) + minAbility;
	}

	/* Skills -- random within the range then scaled (or not) based on natural ability */
	skillWindow = maxSkill - minSkill;
	for (i = ABILITY_NUM_TYPES; i < SKILL_NUM_TYPES; i++) {

		/* training is the base for where they fall within the range. */
		training = (frand() * skillWindow) + minSkill;

		if (TRAINING_SCALAR > 0) {
			switch (i) {
			case SKILL_CLOSE:
				ability1 = chr->skills[ABILITY_POWER];
				ability2 = chr->skills[ABILITY_SPEED];
				break;
			case SKILL_HEAVY:
				ability1 = chr->skills[ABILITY_POWER];
				ability2 = chr->skills[ABILITY_ACCURACY];
				break;
			case SKILL_ASSAULT:
				ability1 = chr->skills[ABILITY_ACCURACY];
				ability2 = chr->skills[ABILITY_SPEED];
				break;
			case SKILL_SNIPER:
				ability1 = chr->skills[ABILITY_MIND];
				ability2 = chr->skills[ABILITY_ACCURACY];
				break;
			case SKILL_EXPLOSIVE:
				ability1 = chr->skills[ABILITY_MIND];
				ability2 = chr->skills[ABILITY_SPEED];
				break;
			default:
				ability1 = training;
				ability2 = training;
			}

			/* scale the abilitiy window to the skill window. */
			windowScalar = skillWindow / abilityWindow;

			/* scale the ability numbers to their place in the skill range. */
			ability1 = ((ability1 - minAbility) * windowScalar) + minSkill;
			ability2 = ((ability2 - minAbility) * windowScalar) + minSkill;

			/* influence the skills, for better or worse, based on the character's natural ability. */
			chr->skills[i] = ((TRAINING_SCALAR * training) + ability1 + ability2) / (TRAINING_SCALAR + 2);
		} else {
			chr->skills[i] = training;
		}
	}

}

/** @brief Used in Com_CharGetHead and Com_CharGetBody to generate the model path */
static char returnModel[MAX_VAR];

/**
 * @brief Returns the body model for the soldiers for armored and non armored soldiers
 * @param chr Pointer to character struct
 * @sa Com_CharGetBody
 */
char *Com_CharGetBody (character_t * const chr)
{
	char id[MAX_VAR];
	char *underline;

	assert(chr);
#ifdef DEBUG
	if (!chr)
		return NULL;	/* never reached. need for code analyst. */
#endif

	assert(chr->inv);
#ifdef DEBUG
	if (!chr->inv)
		return NULL;	/* never reached. need for code analyst. */
#endif

	/* models of UGVs don't change - because they are already armored */
	if (chr->inv->c[CSI->idArmor] && chr->fieldSize == ACTOR_SIZE_NORMAL) {
		assert(!Q_strcmp(CSI->ods[chr->inv->c[CSI->idArmor]->item.t].type, "armor"));
/*		Com_Printf("Com_CharGetBody: Use '%s' as armor\n", CSI->ods[chr->inv->c[CSI->idArmor]->item.t].id);*/

		/* check for the underline */
		Q_strncpyz(id, CSI->ods[chr->inv->c[CSI->idArmor]->item.t].id, sizeof(id));
		underline = strchr(id, '_');
		if (underline)
			*underline = '\0';

		Com_sprintf(returnModel, sizeof(returnModel), "%s%s/%s", chr->path, id, chr->body);
	} else
		Com_sprintf(returnModel, sizeof(returnModel), "%s/%s", chr->path, chr->body);
	return returnModel;
}

/**
 * @brief Returns the head model for the soldiers for armored and non armored soldiers
 * @param chr Pointer to character struct
 * @sa Com_CharGetBody
 */
char *Com_CharGetHead (character_t * const chr)
{
	char id[MAX_VAR];
	char *underline;

	assert(chr);
#ifdef DEBUG
	if (!chr)
		return NULL;	/* never reached. need for code analyst. */
#endif

	assert(chr->inv);
#ifdef DEBUG
	if (!chr->inv)
		return NULL;	/* never reached. need for code analyst. */
#endif

	/* models of UGVs don't change - because they are already armored */
	if (chr->inv->c[CSI->idArmor] && chr->fieldSize == ACTOR_SIZE_NORMAL) {
		assert(!Q_strcmp(CSI->ods[chr->inv->c[CSI->idArmor]->item.t].type, "armor"));
/*		Com_Printf("Com_CharGetHead: Use '%s' as armor\n", CSI->ods[chr->inv->c[CSI->idArmor]->item.t].id);*/

		/* check for the underline */
		Q_strncpyz(id, CSI->ods[chr->inv->c[CSI->idArmor]->item.t].id, sizeof(id));
		underline = strchr(id, '_');
		if (underline)
			*underline = '\0';

		Com_sprintf(returnModel, sizeof(returnModel), "%s%s/%s", chr->path, id, chr->head);
	} else
		Com_sprintf(returnModel, sizeof(returnModel), "%s/%s", chr->path, chr->head);
	return returnModel;
}

/**
 * @brief Prints a description of an object
 * @param[in] index of object in CSI
 */
void Com_PrintItemDescription (int i)
{
	objDef_t *ods_temp;

	ods_temp = &CSI->ods[i];
	Com_Printf("Item: %i %s\n", i, ods_temp->id);
	Com_Printf("... name          -> %s\n", ods_temp->name);
	Com_Printf("... type          -> %s\n", ods_temp->type);
	Com_Printf("... category      -> %i\n", ods_temp->category);
	Com_Printf("... weapon        -> %i\n", ods_temp->weapon);
	Com_Printf("... holdtwohanded -> %i\n", ods_temp->holdtwohanded);
	Com_Printf("... firetwohanded -> %i\n", ods_temp->firetwohanded);
	Com_Printf("... twohanded     -> %i\n", ods_temp->holdtwohanded);
	Com_Printf("... thrown        -> %i\n", ods_temp->thrown);
	Com_Printf("... usable for weapon (if type is ammo):\n");
	for (i = 0; i < ods_temp->numWeapons; i++) {
		if (ods_temp->weap_idx[i] >= 0)
			Com_Printf("    ... %s\n", CSI->ods[ods_temp->weap_idx[i]].name);
	}
	Com_Printf("\n");
}

#ifdef DEBUG
/**
 * @brief Lists all object definitions
 * @todo Why is this here - and not in a client only function
 * @todo Move to INV_shared.c or cl_inventory.c
 */
void Com_InventoryList_f (void)
{
	int i;

	for (i = 0; i < CSI->numODs; i++)
		Com_PrintItemDescription(i);
}
#endif /* DEBUG */

/**
 * @brief Returns the index of this item in the inventory.
 * @todo This function should be located in a inventory-related file.
 * @todo Move to INV_shared.c or cl_inventory.c
 * @note id may not be null or empty
 * @note previously known as RS_GetItem
 * @param[in] id the item id in our object definition array (csi.ods)
 */
int INVSH_GetItemByID (const char *id)
{
	int i;
	objDef_t *item = NULL;

#ifdef DEBUG
	if (!id || !*id) {
		Com_Printf("INVSH_GetItemByID: Called with empty id\n");
		return -1;
	}
#endif

	for (i = 0; i < CSI->numODs; i++) {	/* i = item index */
		item = &CSI->ods[i];
		if (!Q_strncmp(id, item->id, MAX_VAR)) {
			return i;
		}
	}

	Com_Printf("INVSH_GetItemByID: Item \"%s\" not found.\n", id);
	return -1;
}

/**
 * @brief Checks if an item can be used to reload a weapon.
 * @param[in] od The object definition of the ammo.
 * @param[in] weapon_idx The index of the weapon (in the inventory) to check the item with.
 * @return qboolean Returns qtrue if the item can be used in the given weapon, otherwise qfalse.
 * @note Formerly named INV_AmmoUsableInWeapon.
 * @todo Move to INV_shared.c or cl_inventory.c
 * @sa Com_PackAmmoAndWeapon
 */
qboolean INV_LoadableInWeapon (objDef_t *od, int weapon_idx)
{
	int i;
	qboolean usable = qfalse;

	for (i = 0; i < od->numWeapons; i++) {
#ifdef DEBUG
		if (od->weap_idx[i] < 0) {
			Com_DPrintf("INV_LoadableInWeapon: negative weap_idx entry (%s) found in item '%s'.\n", od->weap_id[i], od->id );
			break;
		}
#endif
		if (weapon_idx == od->weap_idx[i]) {
			usable = qtrue;
			break;
		}
	}
#if 0
	Com_DPrintf("INV_LoadableInWeapon: item '%s' usable/unusable (%i) in weapon '%s'.\n", od->id, usable, CSI->ods[weapon_idx].id );
#endif
	return usable;
}

/**
 * @brief Returns the index of the array that has the firedefinitions for a given weapon (-index)
 * @param[in] od The object definition of the item.
 * @param[in] weapon_idx The index of the weapon (in the inventory) to check the item with.
 * @return int Returns the index in the fd array. -1 if the weapon-idx was not found. 0 (equals the default firemode) if an invalid or unknown weapon idx was given.
 * @note the return value of -1 is in most cases a fatal error (except the scripts are not parsed while e.g. maptesting)
 * @todo Move to INV_shared.c
 */
int INV_FiredefsIDXForWeapon (objDef_t *od, int weapon_idx)
{
	int i;

	if (!od) {
		Com_DPrintf("INV_FiredefsIDXForWeapon: object definition is NULL.\n");
		return -1;
	}

	if (weapon_idx == NONE) {
		Com_DPrintf("INV_FiredefsIDXForWeapon: bad weapon_idx (NONE) in item '%s' - using default weapon/firemodes.\n", od->id);
		/* FIXME: this won't work if there is no weapon_idx, don't it? - should be -1 here, too */
		return 0;
	}

	for (i = 0; i < od->numWeapons; i++) {
		if (weapon_idx == od->weap_idx[i])
			return i;
	}

	/* No firedef index found for this weapon/ammo. */
#ifdef DEBUG
	Com_DPrintf("INV_FiredefsIDXForWeapon: No firedef index found for weapon. od:%s weap_idx:%i).\n", od->id, weapon_idx);
#endif
	return -1;
}

/**
 * @brief Returns the default reaction firemode for a given ammo in a given weapon.
 * @param[in] ammo The ammo(or weapon-)object that contains the firedefs
 * @param[in] weapon_fds_idx The index in objDef[x]
 * @return Default reaction-firemode index in objDef->fd[][x]. -1 if an error occurs or no firedefs exist.
 * @todo Move to INV_shared.c
 */
int Com_GetDefaultReactionFire (objDef_t *ammo, int weapon_fds_idx)
{
	int fd_idx;
	if (weapon_fds_idx >= MAX_WEAPONS_PER_OBJDEF) {
		Com_Printf("Com_GetDefaultReactionFire: bad weapon_fds_idx (%i) Maximum is %i.\n", weapon_fds_idx, MAX_WEAPONS_PER_OBJDEF-1);
		return -1;
	}
	if (weapon_fds_idx < 0) {
		Com_Printf("Com_GetDefaultReactionFire: Negative weapon_fds_idx given.\n");
		return -1;
	}

	if (ammo->numFiredefs[weapon_fds_idx] == 0) {
		Com_Printf("Com_GetDefaultReactionFire: Probably not an ammo-object: %s\n", ammo->id);
		return -1;
	}

	for (fd_idx = 0; fd_idx < ammo->numFiredefs[weapon_fds_idx]; fd_idx++) {
		if (ammo->fd[weapon_fds_idx][fd_idx].reaction)
			return fd_idx;
	}

	return -1; /* -1 = undef firemode. Default for objects without a reaction-firemode */
}

/**
 * @brief Will merge the second shape (=itemshape) into the first one (=big container shape) on the position x/y.
 * @param[in] shape Pointer to 'uint32_t shape[SHAPE_BIG_MAX_HEIGHT]'
 * @param[in] itemshape
 * @param[in] x
 * @param[in] y
 */
void Com_MergeShapes (uint32_t *shape, uint32_t itemshape, int x, int y)
{
	int i;

	for (i = 0; (i < SHAPE_SMALL_MAX_HEIGHT) && (y + i < SHAPE_BIG_MAX_HEIGHT); i++)
		shape[y + i] |= ((itemshape >> i * SHAPE_SMALL_MAX_WIDTH) & 0xFF) << x;
}

/**
 * @brief Checks the shape if there is a 1-bit on the position x/y.
 * @param[in] shape Pointer to 'uint32_t shape[SHAPE_BIG_MAX_HEIGHT]'
 * @param[in] x
 * @param[in] y
 */
qboolean Com_CheckShape (const uint32_t *shape, int x, int y)
{
	const uint32_t row = shape[y];
	int position = pow(2, x);

	if (y > SHAPE_BIG_MAX_HEIGHT)
		return qfalse;

	if ((row & position) == 0)
		return qfalse;
	else
		return qtrue;
}

/**
 * @brief Counts the used bits in a shape (item shape).
 * @param[in] shape The shape to count the bits in.
 * @return Number of bits.
 * @note Used to calculate the real field usage in the inventory
 */
int Com_ShapeUsage (uint32_t shape)
{
 	int bit_counter = 0;
	int i;

	for (i = 0; i < SHAPE_SMALL_MAX_HEIGHT * SHAPE_SMALL_MAX_WIDTH; i++)
		if (shape & (1 << i))
			bit_counter++;

	return bit_counter;
}

#if 0
/** README
 * @todo Draft functions for future auto-rotate feature (shape + model)
 * @todo Check bit operations that are used below and test the INV_ShapeRotate function.
 */

/**
 * @brief Sets one bit in a shape to true/1
 * @note Only works for V_SHAPE_SMALL!
 * If the bit is already set the shape is not changed.
 * @param[in] shape The shape to modify. (8x8 bits?)
 * @param[in] x The x (width) position of the bit to set.
 * @param[in] y The y (height) position of the bit to set.
 * @return The new shape.
 */
uint32_t Com_ShapeSetBit (uint32_t shape, int x, int y)
{
	shape |= 0x01 << (x * SHAPE_SMALL_MAX_WIDTH + y);
	return shape;
}


/**
 * @brief Rotates a shape definition 90 degree to the left (CCW)
 * @note Only works for V_SHAPE_SMALL!
 * @param[in] shape The shape to rotate.
 * @return The new shape.
 */
uint32_t Com_ShapeRotate (uint32_t shape)
{
	int h, w;
	uint32_t shape_new = 0;
	int h_new = 0;	/**< Counts the new height-rows */
	int row;

	for (h = 0; h < SHAPE_SMALL_MAX_HEIGHT; h++) {
		row = (shape >> (h * SHAPE_SMALL_MAX_WIDTH)); /* Shift the mask to the right to remove leading rows */
		row &= 0xFF;		/* Mask out trailing rows */
		for (w = SHAPE_SMALL_MAX_WIDTH - 1; w >= 0; w--) {
			if (row && (0x01 << w)) { /* Bit number 'w' in this row set? */
				shape_new = Com_ShapeSetBit(shape_new, h, h_new); /* "h" is the new width here. */
				h_new++;	/* Count row */
			}
		}
	}

	return shape_new;
}

/**
 * @brief Prints the shape.
 * @note Only works for V_SHAPE_SMALL!
 */
void Com_ShapePrint (uint32_t shape)
{
	int h, w;
	int row;

	for (h = 0; h < SHAPE_SMALL_MAX_HEIGHT; h++) {
		row = (shape >> (h * SHAPE_SMALL_MAX_WIDTH)); /* Shift the mask to the right to remove leading rows */
		row &= 0xFF;		/* Mask out trailing rows */
		Com_Printf("|");
		if (row) {
			for (w = 0; w < SHAPE_SMALL_MAX_WIDTH; w++) {
				if (row && (0x01 << w)) { /* Bit number 'w' in this row set? */
					Com_Printf("#");
				} else {
					Com_Printf(".");
				}
			}
		} else {
			break;
		}
		Com_Printf("|\n");
	}
}
#endif
