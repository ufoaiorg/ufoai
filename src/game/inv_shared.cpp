/**
 * @file
 * @brief Common object-, inventory-, container- and firemode-related functions.
 * @note Shared inventory management functions prefix: INVSH_
 * @note Shared firemode management functions prefix: FIRESH_
 */

/*
Copyright (C) 2002-2013 UFO: Alien Invasion.

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

#include "q_shared.h"

static const csi_t *CSI;

/**
 * @brief Initializes client server shared data pointer. This works because the client and the server are both
 * using exactly the same pointer.
 * @param[in] import The client server interface pointer
 * @sa G_Init
 * @sa Com_ParseScripts
 */
void INVSH_InitCSI (const csi_t *import)
{
	CSI = import;
}

/**
 * @brief Checks whether the inventory definition is the floor
 * @return @c true if the given inventory definition is of type floor
 */
bool invDef_t::isFloorDef () const
{
	return id == CSI->idFloor;
}

/**
 * @brief Checks whether the inventory definition is the right Hand
 * @return @c true if the given inventory definition is of type right
 */
bool invDef_t::isRightDef () const
{
	return id == CSI->idRight;
}

/**
 * @brief Checks whether a given inventory definition is of special type
 * @return @c true if the given inventory definition is of type left
 */
bool invDef_t::isLeftDef () const
{
	return id == CSI->idLeft;
}

/**
 * @brief Checks whether a given inventory definition is of special type
 * @return @c true if the given inventory definition is of type equip
 */
bool invDef_t::isEquipDef () const
{
	return id == CSI->idEquip;
}

/**
 * @brief Checks whether a given inventory definition is of special type
 * @return @c true if the given inventory definition is of type armour
 */
bool invDef_t::isArmourDef () const
{
	return id == CSI->idArmour;
}

static int cacheCheckToInventory = INV_DOES_NOT_FIT;

/**
 * @brief Will check if the item-shape is colliding with something else in the container-shape at position x/y.
 * @note The function expects an already rotated shape for itemShape. Use getShapeRotated() if needed.
 * @param[in] shape The shape of the container [SHAPE_BIG_MAX_HEIGHT]
 * @param[in] itemShape The shape of the item [SHAPE_SMALL_MAX_HEIGHT]
 * @param[in] x The x value in the container (1 << x in the shape bitmask)
 * @param[in] y The y value in the container (SHAPE_BIG_MAX_HEIGHT is the max)
 */
static bool INVSH_CheckShapeCollision (const uint32_t *shape, const uint32_t itemShape, const int x, const int y)
{
	int i;

	/* Negative positions not allowed (all items are supposed to have at least one bit set in the first row and column) */
	if (x < 0 || y < 0) {
		Com_DPrintf(DEBUG_SHARED, "INVSH_CheckShapeCollision: x or y value negative: x=%i y=%i!\n", x, y);
		return true;
	}

	for (i = 0; i < SHAPE_SMALL_MAX_HEIGHT; i++) {
		/* 0xFF is the length of one row in a "small shape" i.e. SHAPE_SMALL_MAX_WIDTH */
		const int itemRow = (itemShape >> (i * SHAPE_SMALL_MAX_WIDTH)) & 0xFF;
		/* Result has to be limited to 32bit (SHAPE_BIG_MAX_WIDTH) */
		const uint32_t itemRowShifted = itemRow << x;

		/* Check x maximum. */
		if (itemRowShifted >> x != itemRow)
			/* Out of bounds (32bit; a few bits of this row in itemShape were truncated) */
			return true;

		/* Check y maximum. */
		if (y + i >= SHAPE_BIG_MAX_HEIGHT && itemRow)
			/* This row (row "i" in itemShape) is outside of the max. bound and has bits in it. */
			return true;

		/* Check for collisions of the item with the big mask. */
		if (itemRowShifted & shape[y + i])
			return true;
	}

	return false;
}

/**
 * @brief Checks if an item-shape can be put into a container at a certain position... ignores any 'special' types of containers.
 * @param[in] inv The inventory
 * @param[in] container The container (index) to look into.
 * @param[in] itemShape The shape info of an item to fit into the container.
 * @param[in] x The x value in the container (1 << x in the shape bitmask)
 * @param[in] y The y value in the container (SHAPE_BIG_MAX_HEIGHT is the max)
 * @param[in] ignoredItem You can ignore one item in the container (most often the currently dragged one). Use NULL if you want to check against all items in the container.
 * @sa canHoldItem
 * @return false if the item does not fit, true if it fits.
 */
static bool INVSH_CheckToInventory_shape (const inventory_t *const inv, const invDef_t *container, const uint32_t itemShape, const int x, const int y, const invList_t *ignoredItem)
{
	invList_t *ic;
	static uint32_t mask[SHAPE_BIG_MAX_HEIGHT];

	assert(container);

	if (container->scroll)
		Sys_Error("INVSH_CheckToInventory_shape: No scrollable container will ever use this. This type does not support grid-packing!");

	/* check bounds */
	if (x < 0 || y < 0 || x >= SHAPE_BIG_MAX_WIDTH || y >= SHAPE_BIG_MAX_HEIGHT)
		return false;

	if (!cacheCheckToInventory) {
		int j;
		/* extract shape info */
		for (j = 0; j < SHAPE_BIG_MAX_HEIGHT; j++)
			mask[j] = ~container->shape[j];

		/* Add other items to mask. (i.e. merge their shapes at their location into the generated mask) */
		for (ic = inv->getContainer(container->id); ic; ic = ic->getNext()) {
			if (ignoredItem == ic)
				continue;

			if (ic->item.rotated)
				INVSH_MergeShapes(mask, ic->item.def()->getShapeRotated(), ic->getX(), ic->getY());
			else
				INVSH_MergeShapes(mask, ic->item.def()->shape, ic->getX(), ic->getY());
		}
	}

	/* Test for collisions with newly generated mask. */
	if (INVSH_CheckShapeCollision(mask, itemShape, x, y))
		return false;

	/* Everything ok. */
	return true;
}

/**
 * @brief Checks the shape if there is a 1-bit on the position x/y.
 * @param[in] shape The shape to check in. (8x4)
 * @param[in] x The x value in the shape (1 << x in the shape bitmask)
 * @param[in] y The y value in the shape (SHAPE_SMALL_MAX_HEIGHT is the max)
 */
static bool INVSH_CheckShapeSmall (const uint32_t shape, const int x, const int y)
{
	if (y >= SHAPE_SMALL_MAX_HEIGHT || x >= SHAPE_SMALL_MAX_WIDTH|| x < 0 || y < 0) {
		return false;
	}

	return shape & (0x01 << (y * SHAPE_SMALL_MAX_WIDTH + x));
}

/**

 * @brief Check if a position in a container is used by an item (i.e. collides with the shape).
 * @param[in] ic A pointer to an invList_t struct. The position is checked against its contained item.
 * @param[in] x The x location in the container.
 * @param[in] y The y location in the container.
 */
static bool INVSH_ShapeCheckPosition (const item_t *item, const int x, const int y)
{
	uint32_t shape;

	assert(item);

	/* Check if the position is inside the shape (depending on rotation value) of the item. */
	if (item->rotated) {
		shape = item->def()->getShapeRotated();
	} else {
		shape = item->def()->shape;
	}

	return INVSH_CheckShapeSmall(shape, x - item->getX(), y - item->getY());
}

/**
 * @brief Checks whether a given item is an aircraftitem item
 * @note This is done by checking whether it's a craftitem and not
 * marked as a dummy item - the combination of both means, that it's a
 * basedefence item.
 * @return true if the given item is an aircraftitem item
 * @sa isBaseDefenceItem
 */
bool objDef_t::isCraftItem () const
{
	return craftitem.type != MAX_ACITEMS && !isDummy;
}

/**
 * @brief Checks whether the item is a basedefence item
 * @note This is done by checking whether it's a craftitem and whether it's
 * marked as a dummy item - the combination of both means, that it's a
 * basedefence item.
 * @return true if the given item is a basedefence item
 * @sa isCraftItem
 */
bool objDef_t::isBaseDefenceItem () const
{
	return craftitem.type != MAX_ACITEMS && isDummy;
}

/**
 * @brief Returns the index of this item in the inventory.
 * @note check that id is a none empty string
 * @note previously known as RS_GetItem
 * @param[in] id the item id in our object definition array (csi.ods)
 * @sa INVSH_GetItemByID
 */
const objDef_t *INVSH_GetItemByIDSilent (const char *id)
{
	int i;

	if (!id)
		return NULL;
	for (i = 0; i < CSI->numODs; i++) {	/* i = item index */
		const objDef_t *item = &CSI->ods[i];
		if (Q_streq(id, item->id)) {
			return item;
		}
	}
	return NULL;
}

/**
 * @brief Returns the index of this item in the inventory.
 */
const objDef_t *INVSH_GetItemByIDX (int index)
{
	if (index == NONE)
		return NULL;

	if (index < 0 || index >= CSI->numODs)
		Sys_Error("Invalid object index given: %i", index);

	return &CSI->ods[index];
}

/**
 * @brief Returns the item that belongs to the given id or @c NULL if it wasn't found.
 * @param[in] id the item id in our object definition array (csi.ods)
 * @sa INVSH_GetItemByIDSilent
 */
const objDef_t *INVSH_GetItemByID (const char *id)
{
	const objDef_t *od = INVSH_GetItemByIDSilent(id);
	if (!od)
		Com_Printf("INVSH_GetItemByID: Item \"%s\" not found.\n", id);

	return od;
}

/**
 * Searched an inventory container by a given container id
 * @param[in] id ID or name of the inventory container to search for
 * @return @c NULL if not found
 */
const invDef_t *INVSH_GetInventoryDefinitionByID (const char *id)
{
	containerIndex_t i;
	const invDef_t *container;

	for (i = 0, container = CSI->ids; i < CSI->numIDs; container++, i++)
		if (Q_streq(id, container->name))
			return container;

	return NULL;
}

/**
 * @brief Checks if an item can be used to reload a weapon.
 * @param[in] weapon The weapon (in the inventory) to check the item with. Might not be @c NULL.
 * @return @c true if the item can be used in the given weapon, otherwise @c false.
 */
bool objDef_t::isLoadableInWeapon (const objDef_t *weapon) const
{
	int i;

	assert(this);
	assert(weapon);

	/* check whether the weapon is only linked to itself. */
	if (this->numWeapons == 1 && this->weapons[0] == this)
		return false;

	for (i = 0; i < this->numWeapons; i++)
		if (weapon == this->weapons[i])
			return true;

	return false;
}

/*
===============================
FIREMODE MANAGEMENT FUNCTIONS
===============================
*/

/**
 * @brief Get the fire definitions for a given object
 * @param[in] obj The object to get the firedef for
 * @param[in] weapFdsIdx the weapon index in the fire definition array
 * @param[in] fdIdx the fire definition index for the weapon (given by @c weapFdsIdx)
 * @return Will never return NULL
 * @sa FIRESH_FiredefForWeapon
 */
const fireDef_t *FIRESH_GetFiredef (const objDef_t *obj, const weaponFireDefIndex_t weapFdsIdx, const fireDefIndex_t fdIdx)
{
	if (weapFdsIdx < 0 || weapFdsIdx >= MAX_WEAPONS_PER_OBJDEF)
		Sys_Error("FIRESH_GetFiredef: weapFdsIdx out of bounds [%i] for item '%s'", weapFdsIdx, obj->id);
	if (fdIdx < 0 || fdIdx >= MAX_FIREDEFS_PER_WEAPON)
		Sys_Error("FIRESH_GetFiredef: fdIdx out of bounds [%i] for item '%s'", fdIdx, obj->id);
	return &obj->fd[weapFdsIdx & (MAX_WEAPONS_PER_OBJDEF - 1)][fdIdx & (MAX_FIREDEFS_PER_WEAPON - 1)];
}

/**
 * @brief Returns the firedefinitions for a given weapon/ammo
 * @return The array (one-dimensional) of the firedefs of the ammo for a given weapon, or @c NULL if the ammo
 * doesn't support the given weapon
 * @sa FIRESH_GetFiredef
 */
const fireDef_t *FIRESH_FiredefForWeapon (const item_t *item)
{
	int i;
	const objDef_t *ammo = item->ammo;
	const objDef_t *weapon = item->def();

	/* this weapon does not use ammo, check for
	 * existing firedefs in the weapon. */
	if (weapon->numWeapons > 0)
		ammo = item->def();

	if (!ammo)
		return NULL;

	for (i = 0; i < ammo->numWeapons; i++) {
		if (weapon == ammo->weapons[i])
			return &ammo->fd[i][0];
	}

	return NULL;
}

/**
 * @brief Get the firedef that uses the most TU for the given item.
 * @param[in] item The item for which we want to find the slowest fireDef.
 * @return The firedef that uses the most TU for this item or @c NULL.
 */
const fireDef_t *FIRESH_SlowestFireDef (const item_t &item)
{
	const fireDef_t *fdArray = FIRESH_FiredefForWeapon(&item);
	int slowest = 0;

	if (fdArray == NULL)
		return NULL;

	for (int i = 0; i < MAX_FIREDEFS_PER_WEAPON; i++)
		if (fdArray[i].time > fdArray[slowest].time)
			slowest = i;

	return &fdArray[slowest];
}

/**
 * @brief Checks whether this list is a reaction fire enabled weapon.
 * @note The invList is supposed to be the right or left hand
 * @return @c NULL if no reaction fire enabled weapon was found in the given list, the
 * reaction fire enabled object otherwise.
 */
const objDef_t *item_t::getReactionFireWeaponType () const
{
	if (!this)
		return NULL;

	if (def()) {
		const fireDef_t *fd = FIRESH_FiredefForWeapon(this);
		if (fd && fd->reaction)
			return def();
	}

	return NULL;
}

/**
 * @brief Will merge the second shape (=itemShape) into the first one (=big container shape) on the position x/y.
 * @note The function expects an already rotated shape for itemShape. Use getShapeRotated() if needed.
 * @param[in] shape The shape of the container [SHAPE_BIG_MAX_HEIGHT]'
 * @param[in] itemShape The shape of the item [SHAPE_SMALL_MAX_HEIGHT]
 * @param[in] x The x value in the container (1 << x in the shape bitmask)
 * @param[in] y The y value in the container (SHAPE_BIG_MAX_HEIGHT is the max)
 */
void INVSH_MergeShapes (uint32_t *shape, const uint32_t itemShape, const int x, const int y)
{
	int i;

	for (i = 0; (i < SHAPE_SMALL_MAX_HEIGHT) && (y + i < SHAPE_BIG_MAX_HEIGHT); i++)
		shape[y + i] |= ((itemShape >> i * SHAPE_SMALL_MAX_WIDTH) & 0xFF) << x;
}

/**
 * @brief Checks the shape if there is a 1-bit on the position x/y.
 * @param[in] shape Pointer to 'uint32_t shape[SHAPE_BIG_MAX_HEIGHT]'
 * @param[in] x The x value in the container (1 << x in the shape bitmask)
 * @param[in] y The y value in the container (SHAPE_BIG_MAX_HEIGHT is the max)
 */
bool INVSH_CheckShape (const uint32_t *shape, const int x, const int y)
{
	const uint32_t row = shape[y];
	int position = pow(2, x);

	if (y >= SHAPE_BIG_MAX_HEIGHT || x >= SHAPE_BIG_MAX_WIDTH || x < 0 || y < 0) {
		Com_Printf("INVSH_CheckShape: Bad x or y value: (x=%i, y=%i)\n", x, y);
		return false;
	}

	if ((row & position) == 0)
		return false;
	else
		return true;
}

/**
 * @brief Counts the used bits in a shape (item shape).
 * @param[in] shape The shape to count the bits in.
 * @return Number of bits.
 * @note Used to calculate the real field usage in the inventory
 */
int INVSH_ShapeSize (const uint32_t shape)
{
	int bitCounter = 0;
	int i;

	for (i = 0; i < SHAPE_SMALL_MAX_HEIGHT * SHAPE_SMALL_MAX_WIDTH; i++)
		if (shape & (1 << i))
			bitCounter++;

	return bitCounter;
}

/**
 * @brief Sets one bit in a shape to true/1
 * @note Only works for V_SHAPE_SMALL!
 * If the bit is already set the shape is not changed.
 * @param[in] shape The shape to modify. (8x4)
 * @param[in] x The x (width) position of the bit to set.
 * @param[in] y The y (height) position of the bit to set.
 * @return The new shape.
 */
static uint32_t INVSH_ShapeSetBit (uint32_t shape, const int x, const int y)
{
	if (x >= SHAPE_SMALL_MAX_WIDTH || y >= SHAPE_SMALL_MAX_HEIGHT || x < 0 || y < 0) {
		Com_Printf("INVSH_ShapeSetBit: Bad x or y value: (x=%i, y=%i)\n", x,y);
		return shape;
	}

	shape |= 0x01 << (y * SHAPE_SMALL_MAX_WIDTH + x);
	return shape;
}


/**
 * @brief Rotates a shape definition 90 degree to the left (CCW)
 * @note Only works for V_SHAPE_SMALL!
 * @return The new shape.
 */
uint32_t objDef_t::getShapeRotated () const
{
	int h, w;
	uint32_t shapeNew = 0;
	int maxWidth = -1;

	for (w = SHAPE_SMALL_MAX_WIDTH - 1; w >= 0; w--) {
		for (h = 0; h < SHAPE_SMALL_MAX_HEIGHT; h++) {
			if (INVSH_CheckShapeSmall(shape, w, h)) {
				if (w >= SHAPE_SMALL_MAX_HEIGHT) {
					/* Object can't be rotated (code-wise), it is longer than SHAPE_SMALL_MAX_HEIGHT allows. */
					return shape;
				}

				if (maxWidth < 0)
					maxWidth = w;

				shapeNew = INVSH_ShapeSetBit(shapeNew, h, maxWidth - w);
			}
		}
	}

	return shapeNew;
}

/**
 * @brief Return the weight of an item.
 * @return The weight of the given item including any ammo loaded.
 */
float item_t::getWeight () const
{
	float weight = def()->weight;
	if (ammo && ammo != def() && ammoLeft > 0) {
		weight += ammo->weight;
	}
	return weight;
}

/**
 * @brief Check if the (physical) information of 2 items is exactly the same.
 * @param[in] other Second item to compare.
 * @return true if they are identical or false otherwise.
 */
bool item_t::isSameAs (const item_t *const other) const
{
	if (this == other)
		return true;

	if (this == NULL || other == NULL)
		return false;

	if (this->def() == other->def() && this->ammo == other->ammo && this->ammoLeft == other->ammoLeft)
		return true;

	return false;
}

/**
 * @brief Calculates the first "true" bit in the shape and returns its position in the item.
 * @note Use this to get the first "grab-able" grid-location of an item (not in the container !).
 * @param[out] x The x location inside the item.
 * @param[out] y The x location inside the item.
 * @sa canHoldItem
 */
void item_t::getFirstShapePosition (int* const x, int* const y) const
{
	int tempX, tempY;

	assert(this);

	for (tempX = 0; tempX < SHAPE_SMALL_MAX_HEIGHT; tempX++)
		for (tempY = 0; tempY < SHAPE_SMALL_MAX_HEIGHT; tempY++)
			if (INVSH_ShapeCheckPosition(this, this->getX() + tempX, this->getY() + tempY)) {
				*x = tempX;
				*y = tempY;
				return;
			}

	*x = *y = NONE;
}

inventory_s::inventory_s ()
{
// This (prototype-)constructor does not work as intended. Seems like the first inventory is created before CSI is set.
//	containers[CSI->idRight]._invList = NULL;
}

/**
 * @param[in] container The index of the container in the inventory to check the item in.
 * @param[in] od The type of item to check in the inventory.
 * @param[in] x The x value in the container (1 << x in the shape bitmask)
 * @param[in] y The y value in the container (SHAPE_BIG_MAX_HEIGHT is the max)
 * @param[in] ignoredItem You can ignore one item in the container (most often the currently dragged one). Use NULL if you want to check against all items in the container.
 * @return INV_DOES_NOT_FIT if the item does not fit
 * @return INV_FITS if it fits and
 * @return INV_FITS_ONLY_ROTATED if it fits only when rotated 90 degree (to the left).
 * @return INV_FITS_BOTH if it fits either normally or when rotated 90 degree (to the left).
 */
int inventory_t::canHoldItem (const invDef_t *container, const objDef_t *od, const int x, const int y, const invList_t *ignoredItem) const
{
	int fits;
	assert(this);
	assert(container);
	assert(od);

	/* armour vs item */
	if (od->isArmour()) {
		if (!container->armour && !container->all) {
			return INV_DOES_NOT_FIT;
		}
	} else if (!od->extension && container->extension) {
		return INV_DOES_NOT_FIT;
	} else if (!od->headgear && container->headgear) {
		return INV_DOES_NOT_FIT;
	} else if (container->armour) {
		return INV_DOES_NOT_FIT;
	}

	/* twohanded item */
	if (od->holdTwoHanded) {
		if ((container->isRightDef() && getContainer(CSI->idLeft)) || container->isLeftDef())
			return INV_DOES_NOT_FIT;
	}

	/* left hand is busy if right wields twohanded */
	if (container->isLeftDef()) {
		if (getContainer(CSI->idRight) && getContainer(CSI->idRight)->item.isHeldTwoHanded())
			return INV_DOES_NOT_FIT;

		/* can't put an item that is 'fireTwoHanded' into the left hand */
		if (od->fireTwoHanded)
			return INV_DOES_NOT_FIT;
	}

	/* Single item containers, e.g. hands, extension or headgear. */
	if (container->single) {
		if (getContainer(container->id)) {
			/* There is already an item. */
			return INV_DOES_NOT_FIT;
		} else {
			fits = INV_DOES_NOT_FIT; /* equals 0 */

			if (INVSH_CheckToInventory_shape(this, container, od->shape, x, y, ignoredItem))
				fits |= INV_FITS;
			if (INVSH_CheckToInventory_shape(this, container, od->getShapeRotated(), x, y, ignoredItem))
				fits |= INV_FITS_ONLY_ROTATED;

			if (fits != INV_DOES_NOT_FIT)
				return fits;	/**< Return INV_FITS_BOTH if both if statements where true above. */

			Com_DPrintf(DEBUG_SHARED, "canHoldItem: INFO: Moving to 'single' container but item would not fit normally.\n");
			return INV_FITS; /**< We are returning with status true (1) if the item does not fit at all - unlikely but not impossible. */
		}
	}

	/* Scrolling container have endless room, the item always fits. */
	if (container->scroll)
		return INV_FITS;

	/* Check 'grid' containers. */
	fits = INV_DOES_NOT_FIT; /* equals 0 */
	if (INVSH_CheckToInventory_shape(this, container, od->shape, x, y, ignoredItem))
		fits |= INV_FITS;
	/** @todo aren't both (equip and floor) temp container? */
	if (!container->isEquipDef() && !container->isFloorDef()
	&& INVSH_CheckToInventory_shape(this, container, od->getShapeRotated(), x, y, ignoredItem))
		fits |= INV_FITS_ONLY_ROTATED;

	return fits;	/**< Return INV_FITS_BOTH if both if statements where true above. */
}

/**
 * @brief Searches if there is an item at location (x,y) in a container.
 * @param[in] container Container in the inventory.
 * @param[in] x/y Position in the container that you want to check.
 * @return invList_t Pointer to the invList_t/item that is located at x/y.
 */
invList_t *inventory_t::getItemAtPos (const invDef_t *container, const int x, const int y) const
{
	assert(container);

	/* Only a single item. */
	if (container->single)
		return getContainer(container->id);

	if (container->scroll)
		Sys_Error("getItemAtPos: Scrollable containers (%i:%s) are not supported by this function.", container->id, container->name);

	/* More than one item - search for the item that is located at location x/y in this container. */
	invList_t *ic;
	for (ic = getContainer(container->id); ic; ic = ic->getNext())
		if (INVSH_ShapeCheckPosition(&ic->item, x, y))
			return ic;

	/* Found nothing. */
	return NULL;
}

/**
 * @brief Finds space for item in inv at container
 * @param[in] item The item to check the space for
 * @param[in] container The container to search in
 * @param[out] px The x position in the container
 * @param[out] py The y position in the container
 * @param[in] ignoredItem You can ignore one item in the container (most often the currently dragged one). Use NULL if you want to check against all items in the container.
 * @sa canHoldItem
 * @note x and y are NONE if no free space is available
 */
void inventory_t::findSpace (const invDef_t *container, const item_t *item, int* const px, int* const py, const invList_t *ignoredItem) const
{
	int x, y;

	assert(container);
	assert(!cacheCheckToInventory);

	/* Scrollable container always have room. We return a dummy location. */
	if (container->scroll) {
		*px = *py = 0;
		return;
	}

	/** @todo optimize for single containers */

	for (y = 0; y < SHAPE_BIG_MAX_HEIGHT; y++) {
		for (x = 0; x < SHAPE_BIG_MAX_WIDTH; x++) {
			const int checkedTo = canHoldItem(container, item->def(), x, y, ignoredItem);
			if (checkedTo) {
				cacheCheckToInventory = INV_DOES_NOT_FIT;
				*px = x;
				*py = y;
				return;
			} else {
				cacheCheckToInventory = INV_FITS;
			}
		}
	}
	cacheCheckToInventory = INV_DOES_NOT_FIT;

#ifdef PARANOID
	Com_DPrintf(DEBUG_SHARED, "findSpace: no space for %s: %s in %s\n",
		item->def()->type, item->def()->id, container->name);
#endif
	*px = *py = NONE;
}

/**
 * @brief Check that adding an item to the inventory won't exceed the max permitted weight.
 * @param[in] from Index of the container the item comes from.
 * @param[in] to Index of the container the item is being placed.
 * @param[in] item The item that is being added.
 * @param[in] maxWeight The max permitted weight.
 * @return @c true if it is Ok to add the item @c false otherwise.
 */
bool inventory_t::canHoldItemWeight (containerIndex_t from, containerIndex_t to, const item_t &item, int maxWeight) const
{
	if (CSI->ids[to].temp || !CSI->ids[from].temp)
		return true;

	const bool swapArmour = item.isArmour() && getArmourContainer();
	const float invWeight = getWeight() - (swapArmour ? getArmourContainer()->item.getWeight() : 0);
	float itemWeight = item.getWeight();

	return (maxWeight < 0 || maxWeight >= invWeight + itemWeight);
}

/**
 * @brief Get the weight of the items in the given inventory (excluding those in temp containers).
 * @return The total weight of the inventory items (excluding those in temp containers)
 */
float inventory_t::getWeight () const
{
	float weight = 0;

	for (int containerID = 0; containerID < CSI->numIDs; containerID++) {
		if (CSI->ids[containerID].temp)
			continue;
		for (invList_t *ic = getContainer(containerID); ic; ic = ic->getNext()) {
			weight += ic->item.getWeight();
		}
	}
	return weight;
}

void inventory_t::setFloorContainer(invList_t *cont)
{
	setContainer(CSI->idFloor, cont);
}

invList_t *inventory_t::getRightHandContainer () const
{
	return getContainer(CSI->idRight);
}

invList_t *inventory_t::getLeftHandContainer () const
{
	return getContainer(CSI->idLeft);
}

invList_t *inventory_t::getHeadgearContainer () const
{
	return getContainer(CSI->idHeadgear);
}

invList_t *inventory_t::getHolsterContainer () const
{
	return getContainer(CSI->idHolster);
}

invList_t *inventory_t::getFloorContainer () const
{
	return getContainer(CSI->idFloor);
}

invList_t *inventory_t::getEquipContainer () const
{
	return getContainer(CSI->idEquip);
}

invList_t *inventory_t::getArmourContainer (void) const
{
	return getContainer(CSI->idArmour);
}

/**
 * @brief Searches a specific item in the inventory&container.
 * @param[in] container Container in the inventory.
 * @param[in] item The item to search for.
 * @return Pointer to the first item of this type found, otherwise @c NULL.
 */
invList_t *inventory_t::findInContainer (const invDef_t *container, const item_t *const item) const
{
	invList_t *ic;

	for (ic = getContainer(container->id); ic; ic = ic->getNext())
		if (ic->item.isSameAs(item)) {
			return ic;
		}

	return NULL;
}

/**
 * @brief Checks if there is a weapon in the hands that can be used for reaction fire.
 */
bool inventory_t::holdsReactionFireWeapon () const
{
	if (getRightHandContainer()->item.getReactionFireWeaponType())
		return true;
	if (getLeftHandContainer()->item.getReactionFireWeaponType())
		return true;
	return false;
}
