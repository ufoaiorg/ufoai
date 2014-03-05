/**
 * @file
 */

/*
Copyright (C) 2002-2014 UFO: Alien Invasion.

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


#include "test_inventory.h"
#include "test_shared.h"
#include "../common/common.h"
#include "../game/inventory.h"

static InventoryInterface i;
static const int TAG_INVENTORY = 5546;

static void FreeInventory (void* data)
{
	Mem_Free(data);
}

static void* AllocInventoryMemory (size_t size)
{
	return Mem_PoolAlloc(size, com_genericPool, TAG_INVENTORY);
}

static void FreeAllInventory (void)
{
	Mem_FreeTag(com_genericPool, TAG_INVENTORY);
}

static const inventoryImport_t inventoryImport = { FreeInventory, FreeAllInventory, AllocInventoryMemory };

static inline void ResetInventoryList (void)
{
	i.destroyInventoryInterface();
	i.initInventory("test", &csi, &inventoryImport);
}

/**
 * The suite initialization function.
 * Returns zero on success, non-zero otherwise.
 */
static int UFO_InitSuiteInventory (void)
{
	TEST_Init();
	Com_ParseScripts(true);

	return 0;
}

/**
 * The suite cleanup function.
 * Returns zero on success, non-zero otherwise.
 */
static int UFO_CleanSuiteInventory (void)
{
	TEST_Shutdown();
	return 0;
}

static void testItemAdd (void)
{
	Inventory inv;
	const objDef_t* od;
	const invDef_t* container;

	ResetInventoryList();

	od = INVSH_GetItemByIDSilent("assault");
	CU_ASSERT_PTR_NOT_NULL(od);

	container = INVSH_GetInventoryDefinitionByID("right");
	CU_ASSERT_PTR_NOT_NULL(container);

	Item item(od);

	CU_ASSERT(inv.containsItem(container->id, &item) == false);

	CU_ASSERT_PTR_NOT_NULL(i.addToInventory(&inv, &item, container, NONE, NONE, 1));

	CU_ASSERT(inv.containsItem(container->id, &item) == true);
}

static void testItemDel (void)
{
	Inventory inv;
	const objDef_t* od;
	const invDef_t* container;
	Item* addedItem;

	ResetInventoryList();

	od = INVSH_GetItemByIDSilent("assault");
	CU_ASSERT_PTR_NOT_NULL(od);

	container = INVSH_GetInventoryDefinitionByID("right");
	CU_ASSERT_PTR_NOT_NULL(container);

	Item item(od);

	CU_ASSERT(inv.containsItem(container->id, &item) == false);

	addedItem = i.addToInventory(&inv, &item, container, NONE, NONE, 1);

	CU_ASSERT(inv.containsItem(container->id, &item) == true);

	CU_ASSERT(i.removeFromInventory(&inv, container, addedItem));

	CU_ASSERT(inv.containsItem(container->id, &item) == false);
}

static void testItemMove (void)
{
	Inventory inv;
	const objDef_t* od;
	const invDef_t* container, *containerTo;
	Item* addedItem;

	ResetInventoryList();

	od = INVSH_GetItemByIDSilent("assault");
	CU_ASSERT_PTR_NOT_NULL(od);

	container = INVSH_GetInventoryDefinitionByID("right");
	CU_ASSERT_PTR_NOT_NULL(container);

	Item item(od);

	CU_ASSERT(inv.containsItem(container->id, &item) == false);

	addedItem = i.addToInventory(&inv, &item, container, NONE, NONE, 1);
	CU_ASSERT_PTR_NOT_NULL(addedItem);

	CU_ASSERT(inv.containsItem(container->id, &item) == true);

	containerTo = INVSH_GetInventoryDefinitionByID("backpack");
	CU_ASSERT_PTR_NOT_NULL(containerTo);

	CU_ASSERT_EQUAL(IA_MOVE, i.moveInInventory(&inv, container, addedItem, containerTo, NONE, NONE, nullptr, nullptr));

	CU_ASSERT(inv.containsItem(container->id, &item) == false);
	CU_ASSERT(inv.containsItem(containerTo->id, &item) == true);
}

static void testItemReload (void)
{
	Inventory inv;
	const objDef_t* od, *ad;
	const invDef_t* container, *containerFrom;
	Item* addedItem;

	ResetInventoryList();

	od = INVSH_GetItemByIDSilent("rpg");
	CU_ASSERT_PTR_NOT_NULL(od);

	container = INVSH_GetInventoryDefinitionByID("right");
	CU_ASSERT_PTR_NOT_NULL(container);

	Item item(od);

	CU_ASSERT(inv.containsItem(container->id, &item) == false);

	addedItem = i.addToInventory(&inv, &item, container, NONE, NONE, 1);
	CU_ASSERT_PTR_NOT_NULL(addedItem);

	CU_ASSERT(inv.containsItem(container->id, &item) == true);

	ad = INVSH_GetItemByIDSilent("rpg_ammo");
	CU_ASSERT_PTR_NOT_NULL(ad);

	Item ammo(ad);

	containerFrom = INVSH_GetInventoryDefinitionByID("backpack");
	CU_ASSERT_PTR_NOT_NULL(containerFrom);

	CU_ASSERT(inv.containsItem(containerFrom->id, &ammo) == false);

	addedItem = i.addToInventory(&inv, &ammo, containerFrom, NONE, NONE, 1);
	CU_ASSERT_PTR_NOT_NULL(addedItem);

	CU_ASSERT(inv.containsItem(containerFrom->id, &ammo) == true);

	CU_ASSERT_EQUAL(IA_RELOAD, i.moveInInventory(&inv, containerFrom, addedItem, container, NONE, NONE, nullptr, nullptr));

	CU_ASSERT(inv.containsItem(containerFrom->id, &ammo) == false);

	item.setAmmoDef(ad);
	item.setAmmoLeft(1);

	CU_ASSERT(inv.containsItem(container->id, &item) == true);

	ad = INVSH_GetItemByIDSilent("rpg_incendiary_ammo");
	CU_ASSERT_PTR_NOT_NULL(ad);

	Item ammoFrom(ad);

	CU_ASSERT(inv.containsItem(containerFrom->id, &ammoFrom) == false);

	addedItem = i.addToInventory(&inv, &ammoFrom, containerFrom, NONE, NONE, 1);
	CU_ASSERT_PTR_NOT_NULL(addedItem);

	CU_ASSERT(inv.containsItem(containerFrom->id, &ammoFrom) == true);

	CU_ASSERT_EQUAL(IA_RELOAD_SWAP, i.moveInInventory(&inv, containerFrom, addedItem, container, NONE, NONE, nullptr, nullptr));

	CU_ASSERT(inv.containsItem(containerFrom->id, &ammoFrom) == false);
	CU_ASSERT(inv.containsItem(containerFrom->id, &ammo) == true);

	item.setAmmoDef(ad);

	CU_ASSERT(inv.containsItem(container->id, &item) == true);
}

static bool testAddSingle (Inventory* inv, const objDef_t* od, const invDef_t* container)
{
	Item item(od);

	return i.tryAddToInventory(inv, &item, container);
}

static void testItemMassActions (void)
{
	ResetInventoryList();

	const objDef_t* od = INVSH_GetItemByIDSilent("assault");
	CU_ASSERT_PTR_NOT_NULL_FATAL(od);

	const invDef_t* container = INVSH_GetInventoryDefinitionByID("right");
	CU_ASSERT_PTR_NOT_NULL_FATAL(container);

	Inventory inv;
	bool addedItem = testAddSingle(&inv, od, container);
	CU_ASSERT(addedItem == true);

	/* second try should fail as the right container is a single container */
	addedItem = testAddSingle(&inv, od, container);
	CU_ASSERT(addedItem == false);

	container = INVSH_GetInventoryDefinitionByID("left");
	CU_ASSERT_PTR_NOT_NULL_FATAL(container);

	od = INVSH_GetItemByIDSilent("fraggrenade");
	CU_ASSERT_PTR_NOT_NULL_FATAL(od);

	addedItem = testAddSingle(&inv, od, container);
	CU_ASSERT(addedItem == true);

	container = INVSH_GetInventoryDefinitionByID("equip");
	CU_ASSERT_PTR_NOT_NULL_FATAL(container);

	for (int i = 0; i < csi.numODs; i++) {
		od = INVSH_GetItemByIDX(i);
		/* every item should be placable on the ground container and there should really be enough space */
		addedItem = testAddSingle(&inv, od, container);
		CU_ASSERT(addedItem == true);
		addedItem = testAddSingle(&inv, od, container);
		CU_ASSERT(addedItem == true);
		addedItem = testAddSingle(&inv, od, container);
		CU_ASSERT(addedItem == true);
		for (int j = 0; j < od->numAmmos; j++) {
			addedItem = testAddSingle(&inv, od->ammos[j], container);
			CU_ASSERT(addedItem == true);
			addedItem = testAddSingle(&inv, od->ammos[j], container);
			CU_ASSERT(addedItem == true);
			addedItem = testAddSingle(&inv, od->ammos[j], container);
			CU_ASSERT(addedItem == true);
			addedItem = testAddSingle(&inv, od->ammos[j], container);
			CU_ASSERT(addedItem == true);
			addedItem = testAddSingle(&inv, od->ammos[j], container);
			CU_ASSERT(addedItem == true);
			addedItem = testAddSingle(&inv, od->ammos[j], container);
			CU_ASSERT(addedItem == true);
		}
	}
}

static void testItemToHeadgear (void)
{
	Inventory inv;
	const objDef_t* od;
	const invDef_t* container;

	ResetInventoryList();

	od = INVSH_GetItemByIDSilent("irgoggles");
	CU_ASSERT_PTR_NOT_NULL_FATAL(od);

	container = INVSH_GetInventoryDefinitionByID("headgear");
	CU_ASSERT_PTR_NOT_NULL_FATAL(container);

	Item item(od);

	CU_ASSERT_FALSE(inv.containsItem(container->id, &item));

	CU_ASSERT_PTR_NOT_NULL(i.addToInventory(&inv, &item, container, NONE, NONE, 1));

	CU_ASSERT_TRUE(inv.containsItem(container->id, &item));

	CU_ASSERT_PTR_NULL(i.addToInventory(&inv, &item, container, NONE, NONE, 1));
}

int UFO_AddInventoryTests (void)
{
	/* add a suite to the registry */
	CU_pSuite InventorySuite = CU_add_suite("InventoryTests", UFO_InitSuiteInventory, UFO_CleanSuiteInventory);
	if (InventorySuite == nullptr)
		return CU_get_error();

	/* add the tests to the suite */
	if (CU_ADD_TEST(InventorySuite, testItemAdd) == nullptr)
		return CU_get_error();
	if (CU_ADD_TEST(InventorySuite, testItemDel) == nullptr)
		return CU_get_error();
	if (CU_ADD_TEST(InventorySuite, testItemMove) == nullptr)
		return CU_get_error();
	if (CU_ADD_TEST(InventorySuite, testItemMassActions) == nullptr)
		return CU_get_error();
	if (CU_ADD_TEST(InventorySuite, testItemReload) == nullptr)
		return CU_get_error();
	if (CU_ADD_TEST(InventorySuite, testItemToHeadgear) == nullptr)
		return CU_get_error();

	return CUE_SUCCESS;
}
