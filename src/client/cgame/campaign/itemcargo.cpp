/**
 * @file
 * @brief Item cargo class
 */

/*
Copyright (C) 2002-2017 UFO: Alien Invasion.

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

#include "itemcargo.h"

#define SAVE_ITEMCARGO_ITEM "item"
#define SAVE_ITEMCARGO_ITEMID "itemid"
#define SAVE_ITEMCARGO_AMOUNT "amount"
#define SAVE_ITEMCARGO_LOOSEAMOUNT "looseamount"

/**
 * @brief Add items to the cargo
 * @param[in] od Pointer to the Object Definition
 * @param[in] amount Number of items to add
 * @param[in] looseAmount Number of loose items to add (bullets)
 * @note use negative values to remove
 */
bool ItemCargo::add(const objDef_t* od, int amount, int looseAmount = 0)
{
	if (!od)
		return false;
	if (amount == 0 && looseAmount == 0)
		return true;

	LIST_Foreach(this->cargo, itemCargo_t, item) {
		if (item->objDef != od)
			continue;

		if (amount + item->amount < 0)
			return false;
		if (looseAmount + item->looseAmount < 0)
			return false;

		item->amount += amount;
		item->looseAmount += looseAmount;

		if (item->amount == 0 && item->looseAmount == 0)
			cgi->LIST_Remove(&this->cargo, (void*)item);

		return true;
	}

	if (amount < 0 || looseAmount < 0)
		return false;

	itemCargo_t cargoItem = { od, amount, looseAmount };

	if (cgi->LIST_Add(&this->cargo, (const void*)&cargoItem, sizeof(cargoItem))) {
		return true;
	}

	return false;
}

/**
 * @brief Add items to the cargo by objDef Id
 * @param[in] objDefId Scripted Id of an object definition
 * @param[in] amount Number of items to add
 * @param[in] looseAmount Number of loose items to add (bullets)
 */
bool ItemCargo::add(const char* objDefId, int amount, int looseAmount = 0)
{
	if (!objDefId)
		return false;
	const objDef_t* od = INVSH_GetItemByIDSilent(objDefId);
	if (!od)
		return false;
	return this->add(od, amount, looseAmount);
}

/**
 * @brief Empties the cargo
 */
void ItemCargo::empty (void)
{
	cgi->LIST_Delete(&(this->cargo));
}

/**
 * @brief Checks if the cargo is empty
 */
bool ItemCargo::isEmpty (void) const
{
	return (this->cargo == nullptr);
}

/**
 * @brief Returns a cargo item by its object definition
 * @param[in] od Object Definition pointer
 * @return pointer to the itemCargo_t entry or @c if no objDef cargo found
 */
itemCargo_t* ItemCargo::get(const objDef_t* od) const
{
	LIST_Foreach(this->cargo, itemCargo_t, item) {
		if (item->objDef == od)
			return item;
	}
	return nullptr;
}

/**
 * @brief Returns amount of an item in the cargo
 * @param[in] od Object Definition pointer
 * @return number of items in the cargo
 */
int ItemCargo::getAmount(const objDef_t* od) const
{
	const itemCargo_t* const item = this->get(od);
	if (item == nullptr)
		return 0;
	return item->amount;
}

/**
 * @brief Returns amount of loose item in the cargo
 * @param[in] od Object Definition pointer
 * @return number of loose items in the cargo
 */
int ItemCargo::getLooseAmount(const objDef_t* od) const
{
	const itemCargo_t* const item = this->get(od);
	if (item == nullptr)
		return 0;
	return item->looseAmount;
}

/**
 * @brief Returns a copy of the cargo list
 * @return linked list of itemCargo_t structures
 */
linkedList_t* ItemCargo::list(void) const
{
	linkedList_t* listing = nullptr;

	LIST_Foreach(this->cargo, itemCargo_t, item) {
		if (!cgi->LIST_Add(&listing, (void*)item, sizeof(*item))) {
			cgi->LIST_Delete(&listing);
			return nullptr;
		}
	}
	return listing;
}

/**
 * @brief Count all items in the cargo
 */
int ItemCargo::count(void) const
{
	int count = 0;
	LIST_Foreach(this->cargo, itemCargo_t, item) {
		count += item->amount;
	}
	return count;
}

/**
 * @brief Calculate size of all items in the cargo
 */
int ItemCargo::size(void) const
{
	int size = 0;
	LIST_Foreach(this->cargo, itemCargo_t, item) {
		size += item->amount * item->objDef->size;
	}
	return size;
}

/**
 * @brief Load item cargo from xml savegame
 * @param[in] root Root xml node to load data from
 */
bool ItemCargo::load(xmlNode_t* root)
{
	if (!root)
		return false;

	for (xmlNode_t* itemNode = cgi->XML_GetNode(root, SAVE_ITEMCARGO_ITEM); itemNode;
		itemNode = cgi->XML_GetNextNode(itemNode, root, SAVE_ITEMCARGO_ITEM))
	{
		const char* objDefId = cgi->XML_GetString(itemNode, SAVE_ITEMCARGO_ITEMID);
		const int amount = cgi->XML_GetInt(itemNode, SAVE_ITEMCARGO_AMOUNT, 0);
		const int looseAmount = cgi->XML_GetInt(itemNode, SAVE_ITEMCARGO_LOOSEAMOUNT, 0);
		if (!add(objDefId, amount, looseAmount))
			cgi->Com_Printf("ItemCargo::load: Could add items to cargo: %s, %d, %d\n", objDefId, amount, looseAmount);
	}
	return true;
}

/**
 * @brief Save item cargo to xml savegame
 * @param[out] root Root xml node to save data under
 */
bool ItemCargo::save(xmlNode_t* root) const
{
	if (!root)
		return false;
	LIST_Foreach(this->cargo, itemCargo_t, item) {
		xmlNode_t* itemNode = cgi->XML_AddNode(root, SAVE_ITEMCARGO_ITEM);
		if (!itemNode)
			return false;
		cgi->XML_AddString(itemNode, SAVE_ITEMCARGO_ITEMID, item->objDef->id);
		cgi->XML_AddIntValue(itemNode, SAVE_ITEMCARGO_AMOUNT, item->amount);
		cgi->XML_AddIntValue(itemNode, SAVE_ITEMCARGO_LOOSEAMOUNT, item->looseAmount);
	}
	return true;
}

/**
 * @brief Creates and initializes ItemCargo object
 */
ItemCargo::ItemCargo(void) : cargo (nullptr)
{
}

/**
 * @brief Creates and initializes ItemCargo object from another one
 * @param[in] itemCargo Other object to make copy of
 */
ItemCargo::ItemCargo(ItemCargo& itemCargo) : cargo (nullptr)
{
	linkedList_t* list = itemCargo.list();

	LIST_Foreach(list, itemCargo_t, cargoItem) {
		cgi->LIST_Add(&this->cargo, (void*)cargoItem, sizeof(*cargoItem));
	}
	cgi->LIST_Delete(&list);
}

/**
 * @brief Destroys ItemCargo with it's internal data
 */
ItemCargo::~ItemCargo(void)
{
	cgi->LIST_Delete(&this->cargo);
}
