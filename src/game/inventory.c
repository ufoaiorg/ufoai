#include "inventory.h"

static inline void I_Free (inventoryInterface_t* self, void *data)
{
	self->import->Free(data);
}

static inline void *I_Alloc (inventoryInterface_t* self, size_t size)
{
	return self->import->Alloc(size);
}

static void I_RemoveInvList (inventoryInterface_t* self, invList_t *invList)
{
	Com_DPrintf(DEBUG_SHARED, "I_RemoveInvList: remove one slot (%s)\n", self->name);

	/* first entry */
	if (self->invList == invList) {
		invList_t *ic = self->invList;
		self->invList = ic->next;
		I_Free(self, ic);
	} else {
		invList_t *ic = self->invList;
		invList_t* prev = NULL;
		while (ic) {
			if (ic == invList) {
				prev->next = ic->next;
				I_Free(self, ic);
				break;
			}
			prev = ic;
			ic = ic->next;
		}
	}
}

static invList_t* I_AddInvList (inventoryInterface_t* self, invList_t **invList)
{
	invList_t *newEntry;
	invList_t *list;

	Com_DPrintf(DEBUG_SHARED, "I_AddInvList: add one slot (%s)\n", self->name);

	/* create the list */
	if (!*invList) {
		*invList = (invList_t*)I_Alloc(self, sizeof(**invList));
		(*invList)->next = NULL; /* not really needed - but for better readability */
		return *invList;
	} else
		list = *invList;

	while (list->next)
		list = list->next;

	newEntry = (invList_t*)I_Alloc(self, sizeof(*newEntry));
	list->next = newEntry;
	newEntry->next = NULL; /* not really needed - but for better readability */

	return newEntry;
}

/**
 * @brief Add an item to a specified container in a given inventory.
 * @note Set x and y to NONE if the item should get added to an automatically chosen free spot in the container.
 * @param[in] self The inventory interface pointer
 * @param[in,out] inv Pointer to inventory definition, to which we will add item.
 * @param[in] item Item to add to given container (needs to have "rotated" tag already set/checked, this is NOT checked here!)
 * @param[in] container Container in given inventory definition, where the new item will be stored.
 * @param[in] x The x location in the container.
 * @param[in] y The x location in the container.
 * @param[in] amount How many items of this type should be added. (this will overwrite the amount as defined in "item.amount")
 * @sa I_RemoveFromInventory
 * @return the @c invList_t pointer the item was added to, or @c NULL in case of an error (item wasn't added)
 */
static invList_t *I_AddToInventory (inventoryInterface_t* self, inventory_t * const inv, const item_t* const item, const invDef_t * container, int x, int y, int amount)
{
	invList_t *ic;
	int checkedTo;

	if (!item->t)
		return NULL;

	if (amount <= 0)
		return NULL;

	assert(inv);
	assert(container);

	if (container->single && inv->c[container->id] && inv->c[container->id]->next)
		return NULL;

	/* idEquip and idFloor */
	if (container->temp) {
		for (ic = inv->c[container->id]; ic; ic = ic->next)
			if (INVSH_CompareItem(&ic->item, item)) {
				ic->item.amount += amount;
				Com_DPrintf(DEBUG_SHARED, "I_AddToInventory: Amount of '%s': %i (%s)\n",
					ic->item.t->name, ic->item.amount, self->name);
				return ic;
			}
	}

	if (x < 0 || y < 0 || x >= SHAPE_BIG_MAX_WIDTH || y >= SHAPE_BIG_MAX_HEIGHT) {
		/* No (sane) position in container given as parameter - find free space on our own. */
		INVSH_FindSpace(inv, item, container, &x, &y, NULL);
		if (x == NONE)
			return NULL;
	}

	checkedTo = INVSH_CheckToInventory(inv, item->t, container, x, y, NULL);
	assert(checkedTo);

	/* not found - add a new one */
	ic = I_AddInvList(self, &inv->c[container->id]);

	/* Set the data in the new entry to the data we got via function-parameters.*/
	ic->item = *item;
	ic->item.amount = amount;

	/* don't reset an already applied rotation */
	if (checkedTo == INV_FITS_ONLY_ROTATED)
		ic->item.rotated = qtrue;
	ic->x = x;
	ic->y = y;

	return ic;
}

/**
 * @param[in] self The inventory interface pointer
 * @param[in] i The inventory the container is in.
 * @param[in] container The container where the item should be removed.
 * @param[in] fItem The item to be removed.
 * @return qtrue If removal was successful.
 * @return qfalse If nothing was removed or an error occurred.
 * @sa I_AddToInventory
 */
static qboolean I_RemoveFromInventory (inventoryInterface_t* self, inventory_t* const i, const invDef_t * container, invList_t *fItem)
{
	invList_t *ic, *previous;

	assert(i);
	assert(container);
	assert(fItem);

	ic = i->c[container->id];
	if (!ic)
		return qfalse;

	/** @todo the problem here is, that in case of a move inside the same container
	 * the item don't just get updated x and y values but it is tried to remove
	 * one of the items => crap - maybe we have to change the inventory move function
	 * to check for this case of move and only update the x and y coordinates instead
	 * of calling the add and remove functions */
	if (container->single || ic == fItem) {
		self->cacheItem = ic->item;
		/* temp container like idEquip and idFloor */
		if (container->temp && ic->item.amount > 1) {
			ic->item.amount--;
			Com_DPrintf(DEBUG_SHARED, "I_RemoveFromInventory: Amount of '%s': %i (%s)\n",
				ic->item.t->name, ic->item.amount, self->name);
			return qtrue;
		}

		if (container->single && ic->next)
			Com_Printf("I_RemoveFromInventory: Error: single container %s has many items. (%s)\n", container->name, self->name);

		/* An item in other containers than idFloor or idEquip should
		 * always have an amount value of 1.
		 * The other container types do not support stacking.*/
		assert(ic->item.amount == 1);

		i->c[container->id] = ic->next;

		/* updated invUnused to be able to reuse this space later again */
		I_RemoveInvList(self, ic);

		return qtrue;
	}

	for (previous = i->c[container->id]; ic; ic = ic->next) {
		if (ic == fItem) {
			self->cacheItem = ic->item;
			/* temp container like idEquip and idFloor */
			if (ic->item.amount > 1 && container->temp) {
				ic->item.amount--;
				Com_DPrintf(DEBUG_SHARED, "I_RemoveFromInventory: Amount of '%s': %i (%s)\n",
					ic->item.t->name, ic->item.amount, self->name);
				return qtrue;
			}

			if (ic == i->c[container->id])
				i->c[container->id] = i->c[container->id]->next;
			else
				previous->next = ic->next;

			I_RemoveInvList(self, ic);

			return qtrue;
		}
		previous = ic;
	}
	return qfalse;
}

/**
 * @brief Conditions for moving items between containers.
 * @param[in] self The inventory interface pointer
 * @param[in] inv Inventory to move in.
 * @param[in] from Source container.
 * @param[in] fItem The item to be moved.
 * @param[in] to Destination container.
 * @param[in] tx X coordinate in destination container.
 * @param[in] ty Y coordinate in destination container.
 * @param[in,out] TU pointer to entity available TU at this moment
 * or @c NULL if TU doesn't matter (outside battlescape)
 * @param[out] icp
 * @return IA_NOTIME when not enough TU.
 * @return IA_NONE if no action possible.
 * @return IA_NORELOAD if you cannot reload a weapon.
 * @return IA_RELOAD_SWAP in case of exchange of ammo in a weapon.
 * @return IA_RELOAD when reloading.
 * @return IA_ARMOUR when placing an armour on the actor.
 * @return IA_MOVE when just moving an item.
 */
static int I_MoveInInventory (inventoryInterface_t* self, inventory_t* const inv, const invDef_t * from, invList_t *fItem, const invDef_t * to, int tx, int ty, int *TU, invList_t ** icp)
{
	invList_t *ic;

	int time;
	int checkedTo = INV_DOES_NOT_FIT;
	qboolean alreadyRemovedSource = qfalse;

	assert(to);
	assert(from);

	if (icp)
		*icp = NULL;

	if (from == to && fItem->x == tx && fItem->y == ty)
		return IA_NONE;

	time = from->out + to->in;
	if (from == to) {
		if (INV_IsFloorDef(from))
			time = 0;
		else
			time /= 2;
	}

	if (TU && *TU < time)
		return IA_NOTIME;

	assert(inv);

	/* Special case for moving an item within the same container. */
	if (from == to) {
		/* Do nothing if we move inside a scroll container. */
		if (from->scroll)
			return IA_NONE;

		ic = inv->c[from->id];
		for (; ic; ic = ic->next) {
			if (ic == fItem) {
				if (ic->item.amount > 1) {
					checkedTo = INVSH_CheckToInventory(inv, ic->item.t, to, tx, ty, fItem);
					if (checkedTo & INV_FITS) {
						ic->x = tx;
						ic->y = ty;
						if (icp)
							*icp = ic;
						return IA_MOVE;
					}
					return IA_NONE;
				}
			}
		}
	}

	/* If weapon is twohanded and is moved from hand to hand do nothing. */
	/* Twohanded weapon are only in CSI->idRight. */
	if (fItem->item.t->fireTwoHanded && INV_IsLeftDef(to) && INV_IsRightDef(from)) {
		return IA_NONE;
	}

	/* If non-armour moved to an armour slot then abort.
	 * Same for non extension items when moved to an extension slot. */
	if ((to->armour && !INV_IsArmour(fItem->item.t))
	 || (to->extension && !fItem->item.t->extension)
	 || (to->headgear && !fItem->item.t->headgear)) {
		return IA_NONE;
	}

	/* Check if the target is a blocked inv-armour and source!=dest. */
	if (to->single)
		checkedTo = INVSH_CheckToInventory(inv, fItem->item.t, to, 0, 0, fItem);
	else {
		if (tx == NONE || ty == NONE)
			INVSH_FindSpace(inv, &fItem->item, to, &tx, &ty, fItem);
		/* still no valid location found */
		if (tx == NONE || ty == NONE)
			return IA_NONE;

		checkedTo = INVSH_CheckToInventory(inv, fItem->item.t, to, tx, ty, fItem);
	}

	if (to->armour && from != to && !checkedTo) {
		item_t cacheItem2;
		invList_t *icTo;
		/* Store x/y origin coordinates of removed (source) item.
		 * When we re-add it we can use this. */
		const int cacheFromX = fItem->x;
		const int cacheFromY = fItem->y;

		/* Check if destination/blocking item is the same as source/from item.
		 * In that case the move is not needed -> abort. */
		icTo = INVSH_SearchInInventory(inv, to, tx, ty);
		if (fItem->item.t == icTo->item.t)
			return IA_NONE;

		/* Actually remove the ammo from the 'from' container. */
		if (!self->RemoveFromInventory(self, inv, from, fItem))
			return IA_NONE;
		else
			/* Removal successful - store this info. */
			alreadyRemovedSource = qtrue;

		cacheItem2 = self->cacheItem; /* Save/cache (source) item. The cacheItem is modified in I_MoveInInventory. */

		/* Move the destination item to the source. */
		self->MoveInInventory(self, inv, to, icTo, from, cacheFromX, cacheFromY, TU, icp);

		/* Reset the cached item (source) (It'll be move to container emptied by destination item later.) */
		self->cacheItem = cacheItem2;
	} else if (!checkedTo) {
		/* Get the target-invlist (e.g. a weapon). We don't need to check for
		 * scroll because checkedTo is always true here. */
		ic = INVSH_SearchInInventory(inv, to, tx, ty);

		if (ic && !INV_IsEquipDef(to) && INVSH_LoadableInWeapon(fItem->item.t, ic->item.t)) {
			/* A target-item was found and the dragged item (implicitly ammo)
			 * can be loaded in it (implicitly weapon). */
			if (ic->item.a >= ic->item.t->ammo && ic->item.m == fItem->item.t) {
				/* Weapon already fully loaded with the same ammunition -> abort */
				return IA_NORELOAD;
			}
			time += ic->item.t->reload;
			if (!TU || *TU >= time) {
				if (TU)
					*TU -= time;
				if (ic->item.a >= ic->item.t->ammo) {
					/* exchange ammo */
					const item_t item = {NONE_AMMO, NULL, ic->item.m, 0, 0};
					/* Put current ammo in place of the new ammo unless floor - there can be more than 1 item */
					const int cacheFromX = INV_IsFloorDef(from) ? NONE : fItem->x;
					const int cacheFromY = INV_IsFloorDef(from) ? NONE : fItem->y;

					/* Actually remove the ammo from the 'from' container. */
					if (!self->RemoveFromInventory(self, inv, from, fItem))
						return IA_NONE;

					/* Add the currently used ammo in place of the new ammo in the "from" container. */
					if (self->AddToInventory(self, inv, &item, from, cacheFromX, cacheFromY, 1) == NULL)
						Sys_Error("Could not reload the weapon - add to inventory failed (%s)", self->name);

					ic->item.m = self->cacheItem.t;
					if (icp)
						*icp = ic;
					return IA_RELOAD_SWAP;
				} else {
					/* Actually remove the ammo from the 'from' container. */
					if (!self->RemoveFromInventory(self, inv, from, fItem))
						return IA_NONE;

					ic->item.m = self->cacheItem.t;
					/* loose ammo of type ic->item.m saved on server side */
					ic->item.a = ic->item.t->ammo;
					if (icp)
						*icp = ic;
					return IA_RELOAD;
				}
			}
			/* Not enough time -> abort. */
			return IA_NOTIME;
		}

		/* temp container like idEquip and idFloor */
		if (ic && to->temp) {
			/* We are moving to a blocked location container but it's the base-equipment floor or a battlescape floor.
			 * We add the item anyway but it'll not be displayed (yet)
			 * This is then used in I_AddToInventory below.*/
			/** @todo change the other code to browse trough these things. */
			INVSH_FindSpace(inv, &fItem->item, to, &tx, &ty, fItem);
			if (tx == NONE || ty == NONE) {
				Com_DPrintf(DEBUG_SHARED, "I_MoveInInventory - item will be added non-visible (%s)\n", self->name);
			}
		} else {
			/* Impossible move -> abort. */
			return IA_NONE;
		}
	}

	/* twohanded exception - only CSI->idRight is allowed for fireTwoHanded weapons */
	if (fItem->item.t->fireTwoHanded && INV_IsLeftDef(to))
		to = &self->csi->ids[self->csi->idRight];

	if (checkedTo == INV_FITS_ONLY_ROTATED) {
		/* Set rotated tag */
		fItem->item.rotated = qtrue;
	} else if (fItem->item.rotated) {
		/* Remove rotated tag */
		fItem->item.rotated = qfalse;
	}

	/* Actually remove the item from the 'from' container (if it wasn't already removed). */
	if (!alreadyRemovedSource)
		if (!self->RemoveFromInventory(self, inv, from, fItem))
			return IA_NONE;

	/* successful */
	if (TU)
		*TU -= time;

	assert(self->cacheItem.t);
	ic = self->AddToInventory(self, inv, &self->cacheItem, to, tx, ty, 1);

	/* return data */
	if (icp) {
		assert(ic);
		*icp = ic;
	}

	if (INV_IsArmourDef(to)) {
		assert(INV_IsArmour(self->cacheItem.t));
		return IA_ARMOUR;
	} else
		return IA_MOVE;
}

/**
 * @brief Tries to add an item to a container (in the inventory inv).
 * @param[in] self The inventory interface pointer
 * @param[in] inv Inventory pointer to add the item.
 * @param[in] item Item to add to inventory.
 * @param[in] container Container id.
 * @sa INVSH_FindSpace
 * @sa I_AddToInventory
 */
static qboolean I_TryAddToInventory (inventoryInterface_t* self, inventory_t* const inv, const item_t * const item, const invDef_t * container)
{
	int x, y;

	INVSH_FindSpace(inv, item, container, &x, &y, NULL);

	if (x == NONE) {
		assert(y == NONE);
		return qfalse;
	} else {
		const int checkedTo = INVSH_CheckToInventory(inv, item->t, container, x, y, NULL);
		if (!checkedTo)
			return qfalse;
		else {
			item_t itemRotation = *item;
			if (checkedTo == INV_FITS_ONLY_ROTATED)
				itemRotation.rotated = qtrue;
			else
				itemRotation.rotated = qfalse;

			return self->AddToInventory(self, inv, &itemRotation, container, x, y, 1) != NULL;
		}
	}
}

/**
 * @brief Clears the linked list of a container - removes all items from this container.
 * @param[in] self The inventory interface pointer
 * @param[in] i The inventory where the container is located.
 * @param[in] container Index of the container which will be cleared.
 * @sa I_DestroyInventory
 * @note This should only be called for temp containers if the container is really a temp container
 * e.g. the container of a dropped weapon in tactical mission (ET_ITEM)
 * in every other case just set the pointer to NULL for a temp container like idEquip or idFloor
 */
static void I_EmptyContainer (inventoryInterface_t* self, inventory_t* const i, const invDef_t * container)
{
	invList_t *ic;

	ic = i->c[container->id];

	while (ic) {
		invList_t *old = ic;
		ic = ic->next;
		I_RemoveInvList(self, old);
	}

	i->c[container->id] = NULL;
}

/**
 * @brief Destroys inventory.
 * @param[in] self The inventory interface pointer
 * @param[in] inv Pointer to the inventory which should be erased.
 * @note Loops through all containers in inventory. @c NULL for temp containers are skipped,
 * for real containers @c I_EmptyContainer is called.
 * @sa I_EmptyContainer
 */
static void I_DestroyInventory (inventoryInterface_t* self, inventory_t* const inv)
{
	containerIndex_t container;

	if (!inv)
		return;

	for (container = 0; container < self->csi->numIDs; container++) {
		const invDef_t *invDef = &self->csi->ids[container];
		if (!invDef->temp)
			self->EmptyContainer(self, inv, invDef);
	}

	OBJZERO(*inv);
}


#define WEAPONLESS_BONUS	0.4		/* if you got neither primary nor secondary weapon, this is the chance to retry to get one (before trying to get grenades or blades) */

/**
 * @brief Pack a weapon, possibly with some ammo
 * @param[in] self The inventory interface pointer
 * @param[in] inv The inventory that will get the weapon
 * @param[in] weapon The weapon type index in gi.csi->ods
 * @param[in] ed The equipment for debug messages
 * @param[in] missedPrimary if actor didn't get primary weapon, this is 0-100 number to increase ammo number.
 * @sa INVSH_LoadableInWeapon
 */
static int I_PackAmmoAndWeapon (inventoryInterface_t *self, inventory_t* const inv, const objDef_t* weapon, int missedPrimary, const equipDef_t *ed)
{
	const objDef_t *ammo = NULL;
	item_t item = {NONE_AMMO, NULL, NULL, 0, 0};
	qboolean allowLeft;
	qboolean packed;
	int ammoMult = 1;

	assert(!INV_IsArmour(weapon));
	item.t = weapon;

	/* are we going to allow trying the left hand */
	allowLeft = !(inv->c[self->csi->idRight] && inv->c[self->csi->idRight]->item.t->fireTwoHanded);

	if (weapon->oneshot) {
		/* The weapon provides its own ammo (i.e. it is charged or loaded in the base.) */
		item.a = weapon->ammo;
		item.m = weapon;
		Com_DPrintf(DEBUG_SHARED, "I_PackAmmoAndWeapon: oneshot weapon '%s' in equipment '%s' (%s).\n",
				weapon->id, ed->id, self->name);
	} else if (!weapon->reload) {
		item.m = item.t; /* no ammo needed, so fire definitions are in t */
	} else {
		/* find some suitable ammo for the weapon (we will have at least one if there are ammos for this
		 * weapon in equipment definition) */
		int totalAvailableAmmo = 0;
		int i;
		for (i = 0; i < self->csi->numODs; i++) {
			const objDef_t *obj = INVSH_GetItemByIDX(i);
			if (ed->numItems[i] && INVSH_LoadableInWeapon(obj, weapon)) {
				totalAvailableAmmo++;
			}
		}
		if (totalAvailableAmmo) {
			int randNumber = rand() % totalAvailableAmmo;
			for (i = 0; i < self->csi->numODs; i++) {
				const objDef_t *obj = INVSH_GetItemByIDX(i);
				if (ed->numItems[i] && INVSH_LoadableInWeapon(obj, weapon)) {
					randNumber--;
					if (randNumber < 0) {
						ammo = obj;
						break;
					}
				}
			}
		}

		if (!ammo) {
			Com_DPrintf(DEBUG_SHARED, "I_PackAmmoAndWeapon: no ammo for sidearm or primary weapon '%s' in equipment '%s' (%s).\n",
					weapon->id, ed->id, self->name);
			return 0;
		}
		/* load ammo */
		item.a = weapon->ammo;
		item.m = ammo;
	}

	if (!item.m) {
		Com_Printf("I_PackAmmoAndWeapon: no ammo for sidearm or primary weapon '%s' in equipment '%s' (%s).\n",
				weapon->id, ed->id, self->name);
		return 0;
	}

	/* now try to pack the weapon */
	packed = self->TryAddToInventory(self, inv, &item, &self->csi->ids[self->csi->idRight]);
	if (packed)
		ammoMult = 3;
	if (!packed && allowLeft)
		packed = self->TryAddToInventory(self, inv, &item, &self->csi->ids[self->csi->idLeft]);
	if (!packed)
		packed = self->TryAddToInventory(self, inv, &item, &self->csi->ids[self->csi->idBelt]);
	if (!packed)
		packed = self->TryAddToInventory(self, inv, &item, &self->csi->ids[self->csi->idHolster]);
	if (!packed)
		return 0;


	/* pack some more ammo in the backpack */
	if (ammo) {
		int num;
		int numpacked = 0;

		/* how many clips? */
		num = (1 + ed->numItems[ammo->idx])
			* (float) (1.0f + missedPrimary / 100.0);

		/* pack some ammo */
		while (num--) {
			item_t mun = {NONE_AMMO, NULL, NULL, 0, 0};

			mun.t = ammo;
			/* ammo to backpack; belt is for knives and grenades */
			numpacked += self->TryAddToInventory(self, inv, &mun, &self->csi->ids[self->csi->idBackpack]);
			/* no problem if no space left; one ammo already loaded */
			if (numpacked > ammoMult || numpacked * weapon->ammo > 11)
				break;
		}
	}

	return qtrue;
}


/**
 * @brief Equip melee actor with item defined per teamDefs.
 * @param[in] self The inventory interface pointer
 * @param[in] inv The inventory that will get the weapon.
 * @param[in] td Pointer to a team definition.
 * @note Weapons assigned here cannot be collected in any case. These are dummy "actor weapons".
 */
static void I_EquipActorMelee (inventoryInterface_t *self, inventory_t* const inv, const teamDef_t* td)
{
	const objDef_t *obj;
	item_t item;

	assert(td->onlyWeapon);

	/* Get weapon */
	obj = td->onlyWeapon;

	/* Prepare item. This kind of item has no ammo, fire definitions are in item.t. */
	item.t = obj;
	item.m = item.t;
	item.a = NONE_AMMO;
	/* Every melee actor weapon definition is firetwohanded, add to right hand. */
	if (!obj->fireTwoHanded)
		Sys_Error("INVSH_EquipActorMelee: melee weapon %s for team %s is not firetwohanded! (%s)\n",
				obj->id, td->id, self->name);
	self->TryAddToInventory(self, inv, &item, &self->csi->ids[self->csi->idRight]);
}

/**
 * @brief Equip robot actor with default weapon. (defined in ugv_t->weapon)
 * @param[in] self The inventory interface pointer
 * @param[in] inv The inventory that will get the weapon.
 * @param[in] weapon Pointer to the item which being added to robot's inventory.
 */
static void I_EquipActorRobot (inventoryInterface_t *self, inventory_t* const inv, const objDef_t* weapon)
{
	item_t item;

	assert(weapon);

	/* Prepare weapon in item. */
	item.t = weapon;
	item.a = NONE_AMMO;

	/* Get ammo for item/weapon. */
	assert(weapon->numAmmos > 0);	/* There _has_ to be at least one ammo-type. */
	assert(weapon->ammos[0]);
	item.m = weapon->ammos[0];

	self->TryAddToInventory(self, inv, &item, &self->csi->ids[self->csi->idRight]);
}

/**
 * @brief Types of weapon that can be selected
 */
typedef enum {
	WEAPON_PARTICLE_OR_NORMAL = 0,	/**< primary weapon is a particle or normal weapon */
	WEAPON_OTHER = 1,				/**< primary weapon is not a particle or normal weapon */
	WEAPON_NO_PRIMARY = 2			/**< no primary weapon */
} equipPrimaryWeaponType_t;

/**
 * @brief Fully equip one actor. The equipment that is added to the inventory of the given actor
 * is taken from the equipment script definition.
 * @param[in] self The inventory interface pointer
 * @param[in] inv The inventory that will get the weapon.
 * @param[in] ed The equipment that is added from to the actors inventory
 * @param[in] td Pointer to teamdef data - to get the weapon and armour bools.
 * @note The code below is a complete implementation
 * of the scheme sketched at the beginning of equipment_missions.ufo.
 * Beware: If two weapons in the same category have the same price,
 * only one will be considered for inventory.
 */
static void I_EquipActor (inventoryInterface_t* self, inventory_t* const inv, const equipDef_t *ed, const teamDef_t* td)
{
	int i;
	const int numEquip = lengthof(ed->numItems);
	int repeat = 0;
	const float AKIMBO_CHANCE = 0.3; 	/**< if you got a one-handed secondary weapon (and no primary weapon),
											 this is the chance to get another one (between 0 and 1) */

	if (td->weapons) {
		equipPrimaryWeaponType_t primary = WEAPON_NO_PRIMARY;
		int sum;
		int missedPrimary = 0; /**< If actor has a primary weapon, this is zero. Otherwise, this is the probability * 100
								* that the actor had to get a primary weapon (used to compensate the lack of primary weapon) */
		const objDef_t *primaryWeapon = NULL;
		int hasWeapon = 0;
		/* Primary weapons */
		const int maxWeaponIdx = min(self->csi->numODs - 1, numEquip - 1);
		int randNumber = rand() % 100;
		for (i = 0; i < maxWeaponIdx; i++) {
			const objDef_t *obj = INVSH_GetItemByIDX(i);
			if (ed->numItems[i] && obj->weapon && obj->fireTwoHanded && obj->isPrimary) {
				randNumber -= ed->numItems[i];
				missedPrimary += ed->numItems[i];
				if (!primaryWeapon && randNumber < 0)
					primaryWeapon = obj;
			}
		}
		/* See if a weapon has been selected. */
		if (primaryWeapon) {
			hasWeapon += I_PackAmmoAndWeapon(self, inv, primaryWeapon, 0, ed);
			if (hasWeapon) {
				int ammo;

				/* Find the first possible ammo to check damage type. */
				for (ammo = 0; ammo < self->csi->numODs; ammo++)
					if (ed->numItems[ammo] && INVSH_LoadableInWeapon(&self->csi->ods[ammo], primaryWeapon))
						break;
				if (ammo < self->csi->numODs) {
					if (/* To avoid two particle weapons. */
						!(self->csi->ods[ammo].dmgtype == self->csi->damParticle)
						/* To avoid SMG + Assault Rifle */
						&& !(self->csi->ods[ammo].dmgtype == self->csi->damNormal)) {
						primary = WEAPON_OTHER;
					} else {
						primary = WEAPON_PARTICLE_OR_NORMAL;
					}
				}
				/* reset missedPrimary: we got a primary weapon */
				missedPrimary = 0;
			} else {
				Com_DPrintf(DEBUG_SHARED, "INVSH_EquipActor: primary weapon '%s' couldn't be equipped in equipment '%s' (%s).\n",
						primaryWeapon->id, ed->id, self->name);
				repeat = WEAPONLESS_BONUS > frand();
			}
		}

		/* Sidearms (secondary weapons with reload). */
		do {
			int randNumber = rand() % 100;
			const objDef_t *secondaryWeapon = NULL;
			for (i = 0; i < self->csi->numODs; i++) {
				const objDef_t *obj = INVSH_GetItemByIDX(i);
				if (ed->numItems[i] && obj->weapon && obj->reload && !obj->deplete && obj->isSecondary) {
					randNumber -= ed->numItems[i] / (primary == WEAPON_PARTICLE_OR_NORMAL ? 2 : 1);
					if (randNumber < 0) {
						secondaryWeapon = obj;
						break;
					}
				}
			}

			if (secondaryWeapon) {
				hasWeapon += I_PackAmmoAndWeapon(self, inv, secondaryWeapon, missedPrimary, ed);
				if (hasWeapon) {
					/* Try to get the second akimbo pistol if no primary weapon. */
					if (primary == WEAPON_NO_PRIMARY && !secondaryWeapon->fireTwoHanded && frand() < AKIMBO_CHANCE) {
						I_PackAmmoAndWeapon(self, inv, secondaryWeapon, 0, ed);
					}
				}
			}
		} while (!hasWeapon && repeat--);

		/* Misc items and secondary weapons without reload. */
		if (!hasWeapon)
			repeat = WEAPONLESS_BONUS > frand();
		else
			repeat = 0;
		/* Misc object probability can be bigger than 100 -- you're sure to
		 * have one misc if it fits your backpack */
		sum = 0;
		for (i = 0; i < self->csi->numODs; i++) {
			const objDef_t *obj = INVSH_GetItemByIDX(i);
			if (ed->numItems[i] && ((obj->weapon && obj->isSecondary
			 && (!obj->reload || obj->deplete)) || obj->isMisc)) {
				/* if ed->num[i] is greater than 100, the first number is the number of items you'll get:
				 * don't take it into account for probability
				 * Make sure that the probability is at least one if an item can be selected */
				sum += ed->numItems[i] ? max(ed->numItems[i] % 100, 1) : 0;
			}
		}
		if (sum) {
			do {
				int randNumber = rand() % sum;
				const objDef_t *secondaryWeapon = NULL;
				for (i = 0; i < self->csi->numODs; i++) {
					const objDef_t *obj = INVSH_GetItemByIDX(i);
					if (ed->numItems[i] && ((obj->weapon && obj->isSecondary
					 && (!obj->reload || obj->deplete)) || obj->isMisc)) {
						randNumber -= ed->numItems[i] ? max(ed->numItems[i] % 100, 1) : 0;
						if (randNumber < 0) {
							secondaryWeapon = obj;
							break;
						}
					}
				}

				if (secondaryWeapon) {
					int num = ed->numItems[secondaryWeapon->idx] / 100 + (ed->numItems[secondaryWeapon->idx] % 100 >= 100 * frand());
					while (num--) {
						hasWeapon += I_PackAmmoAndWeapon(self, inv, secondaryWeapon, 0, ed);
					}
				}
			} while (repeat--); /* Gives more if no serious weapons. */
		}

		/* If no weapon at all, bad guys will always find a blade to wield. */
		if (!hasWeapon) {
			int maxPrice = 0;
			const objDef_t *blade = NULL;
			Com_DPrintf(DEBUG_SHARED, "INVSH_EquipActor: no weapon picked in equipment '%s', defaulting to the most expensive secondary weapon without reload. (%s)\n",
					ed->id, self->name);
			for (i = 0; i < self->csi->numODs; i++) {
				const objDef_t *obj = INVSH_GetItemByIDX(i);
				if (ed->numItems[i] && obj->weapon && obj->isSecondary && !obj->reload) {
					if (obj->price > maxPrice) {
						maxPrice = obj->price;
						blade = obj;
					}
				}
			}
			if (maxPrice)
				hasWeapon += I_PackAmmoAndWeapon(self, inv, blade, 0, ed);
		}
		/* If still no weapon, something is broken, or no blades in equipment. */
		if (!hasWeapon)
			Com_DPrintf(DEBUG_SHARED, "INVSH_EquipActor: cannot add any weapon; no secondary weapon without reload detected for equipment '%s' (%s).\n",
					ed->id, self->name);

		/* Armour; especially for those without primary weapons. */
		repeat = (float) missedPrimary > frand() * 100.0;
	} else {
		return;
	}

	if (td->armour) {
		do {
			int randNumber = rand() % 100;
			for (i = 0; i < self->csi->numODs; i++) {
				const objDef_t *armour = INVSH_GetItemByIDX(i);
				if (ed->numItems[i] && INV_IsArmour(armour)) {
					randNumber -= ed->numItems[i];
					if (randNumber < 0) {
						const item_t item = {NONE_AMMO, NULL, armour, 0, 0};
						if (self->TryAddToInventory(self, inv, &item, &self->csi->ids[self->csi->idArmour])) {
							repeat = 0;
							break;
						}
					}
				}
			}
		} while (repeat-- > 0);
	} else {
		Com_DPrintf(DEBUG_SHARED, "INVSH_EquipActor: teamdef '%s' may not carry armour (%s)\n",
				td->name, self->name);
	}

	{
		int randNumber = rand() % 10;
		for (i = 0; i < self->csi->numODs; i++) {
			if (ed->numItems[i]) {
				const objDef_t *miscItem = INVSH_GetItemByIDX(i);
				if (miscItem->isMisc && !miscItem->weapon) {
					randNumber -= ed->numItems[i];
					if (randNumber < 0) {
						const qboolean oneShot = miscItem->oneshot;
						const item_t item = {oneShot ? miscItem->ammo : NONE_AMMO, oneShot ? miscItem : NULL, miscItem, 0, 0};
						containerIndex_t container;
						if (miscItem->headgear)
							container = self->csi->idHeadgear;
						else if (miscItem->extension)
							container = self->csi->idExtension;
						else
							container = self->csi->idBackpack;
						self->TryAddToInventory(self, inv, &item, &self->csi->ids[container]);
					}
				}
			}
		}
	}
}

/**
 * @brief Calculate the number of used inventory slots
 * @param[in] self The inventory interface pointer
 * @return The number of free inventory slots
 */
static int I_GetUsedSlots (inventoryInterface_t* self)
{
	int i = 0;
	const invList_t* slot = self->invList;
	while (slot) {
		slot = slot->next;
		i++;
	}
	Com_DPrintf(DEBUG_SHARED, "Used inventory slots %i (%s)\n", i, self->name);
	return i;
}

/**
 * @brief Initializes the inventory definition by linking the ->next pointers properly.
 * @param[in] name The name that is shown in the output
 * @param[out] interface The inventory interface pointer which should be initialized in this function.
 * @param[in] csi The client-server-information structure
 * @param[in] import Pointers to the lifecycle functions
 * @sa G_Init
 * @sa CL_InitLocal
 */
void INV_InitInventory (const char *name, inventoryInterface_t *interface, const csi_t* csi, const inventoryImport_t *import)
{
	const item_t item = {NONE_AMMO, NULL, NULL, 0, 0};

	OBJZERO(*interface);

	interface->import = import;
	interface->name = name;
	interface->cacheItem = item;
	interface->csi = csi;
	interface->invList = NULL;

	interface->TryAddToInventory = I_TryAddToInventory;
	interface->AddToInventory = I_AddToInventory;
	interface->RemoveFromInventory = I_RemoveFromInventory;
	interface->MoveInInventory = I_MoveInInventory;
	interface->DestroyInventory = I_DestroyInventory;
	interface->EmptyContainer = I_EmptyContainer;
	interface->EquipActor = I_EquipActor;
	interface->EquipActorMelee = I_EquipActorMelee;
	interface->EquipActorRobot = I_EquipActorRobot;
	interface->GetUsedSlots = I_GetUsedSlots;
}

void INV_DestroyInventory (inventoryInterface_t *interface)
{
	if (interface->import == NULL)
		return;
	interface->import->FreeAll();
	OBJZERO(*interface);
}
