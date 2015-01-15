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

class InventoryTest: public ::testing::Test {
protected:
	static void SetUpTestCase() {
		TEST_Init();
		Com_ParseScripts(true);
	}

	static void TearDownTestCase() {
		TEST_Shutdown();
		NET_Shutdown();
	}

	void SetUp() {
		ResetInventoryList();
	}
};

TEST_F(InventoryTest, ItemAdd)
{
	Inventory inv;
	const objDef_t* od;
	const invDef_t* container;

	od = INVSH_GetItemByIDSilent("assault");
	ASSERT_TRUE(nullptr != od);

	container = INVSH_GetInventoryDefinitionByID("right");
	ASSERT_TRUE(nullptr != container);

	Item item(od);

	ASSERT_TRUE(inv.containsItem(container->id, &item) == false);

	ASSERT_TRUE(nullptr != i.addToInventory(&inv, &item, container, NONE, NONE, 1));

	ASSERT_TRUE(inv.containsItem(container->id, &item) == true);
}

TEST_F(InventoryTest, ItemDel)
{
	Inventory inv;
	const objDef_t* od;
	const invDef_t* container;
	Item* addedItem;

	od = INVSH_GetItemByIDSilent("assault");
	ASSERT_TRUE(nullptr != od);

	container = INVSH_GetInventoryDefinitionByID("right");
	ASSERT_TRUE(nullptr != container);

	Item item(od);

	ASSERT_TRUE(inv.containsItem(container->id, &item) == false);

	addedItem = i.addToInventory(&inv, &item, container, NONE, NONE, 1);

	ASSERT_TRUE(inv.containsItem(container->id, &item) == true);

	ASSERT_TRUE(i.removeFromInventory(&inv, container, addedItem));

	ASSERT_TRUE(inv.containsItem(container->id, &item) == false);
}

TEST_F(InventoryTest, ItemMove)
{
	Inventory inv;
	const objDef_t* od;
	const invDef_t* container, *containerTo;
	Item* addedItem;

	od = INVSH_GetItemByIDSilent("assault");
	ASSERT_TRUE(nullptr != od);

	container = INVSH_GetInventoryDefinitionByID("right");
	ASSERT_TRUE(nullptr != container);

	Item item(od);

	ASSERT_TRUE(inv.containsItem(container->id, &item) == false);

	addedItem = i.addToInventory(&inv, &item, container, NONE, NONE, 1);
	ASSERT_TRUE(nullptr != addedItem);

	ASSERT_TRUE(inv.containsItem(container->id, &item) == true);

	containerTo = INVSH_GetInventoryDefinitionByID("backpack");
	ASSERT_TRUE(nullptr != containerTo);

	ASSERT_EQ(IA_MOVE, i.moveInInventory(&inv, container, addedItem, containerTo, NONE, NONE, nullptr, nullptr));

	ASSERT_TRUE(inv.containsItem(container->id, &item) == false);
	ASSERT_TRUE(inv.containsItem(containerTo->id, &item) == true);
}

TEST_F(InventoryTest, ItemReload)
{
	Inventory inv;
	const objDef_t* od, *ad;
	const invDef_t* container, *containerFrom;
	Item* addedItem;

	od = INVSH_GetItemByIDSilent("rpg");
	ASSERT_TRUE(nullptr != od);

	container = INVSH_GetInventoryDefinitionByID("right");
	ASSERT_TRUE(nullptr != container);

	Item item(od);

	ASSERT_TRUE(inv.containsItem(container->id, &item) == false);

	addedItem = i.addToInventory(&inv, &item, container, NONE, NONE, 1);
	ASSERT_TRUE(nullptr != addedItem);

	ASSERT_TRUE(inv.containsItem(container->id, &item) == true);

	ad = INVSH_GetItemByIDSilent("rpg_ammo");
	ASSERT_TRUE(nullptr != ad);

	Item ammo(ad);

	containerFrom = INVSH_GetInventoryDefinitionByID("backpack");
	ASSERT_TRUE(nullptr != containerFrom);

	ASSERT_TRUE(inv.containsItem(containerFrom->id, &ammo) == false);

	addedItem = i.addToInventory(&inv, &ammo, containerFrom, NONE, NONE, 1);
	ASSERT_TRUE(nullptr != addedItem);

	ASSERT_TRUE(inv.containsItem(containerFrom->id, &ammo) == true);

	ASSERT_EQ(IA_RELOAD, i.moveInInventory(&inv, containerFrom, addedItem, container, NONE, NONE, nullptr, nullptr));

	ASSERT_TRUE(inv.containsItem(containerFrom->id, &ammo) == false);

	item.setAmmoDef(ad);
	item.setAmmoLeft(1);

	ASSERT_TRUE(inv.containsItem(container->id, &item) == true);

	ad = INVSH_GetItemByIDSilent("rpg_incendiary_ammo");
	ASSERT_TRUE(nullptr != ad);

	Item ammoFrom(ad);

	ASSERT_TRUE(inv.containsItem(containerFrom->id, &ammoFrom) == false);

	addedItem = i.addToInventory(&inv, &ammoFrom, containerFrom, NONE, NONE, 1);
	ASSERT_TRUE(nullptr != addedItem);

	ASSERT_TRUE(inv.containsItem(containerFrom->id, &ammoFrom) == true);

	ASSERT_EQ(IA_RELOAD_SWAP, i.moveInInventory(&inv, containerFrom, addedItem, container, NONE, NONE, nullptr, nullptr));

	ASSERT_TRUE(inv.containsItem(containerFrom->id, &ammoFrom) == false);
	ASSERT_TRUE(inv.containsItem(containerFrom->id, &ammo) == true);

	item.setAmmoDef(ad);

	ASSERT_TRUE(inv.containsItem(container->id, &item) == true);
}

static bool testAddSingle (Inventory* inv, const objDef_t* od, const invDef_t* container)
{
	Item item(od);

	return i.tryAddToInventory(inv, &item, container);
}

TEST_F(InventoryTest, ItemMassActions)
{
	const objDef_t* od = INVSH_GetItemByIDSilent("assault");
	ASSERT_TRUE(nullptr != od);

	const invDef_t* container = INVSH_GetInventoryDefinitionByID("right");
	ASSERT_TRUE(nullptr != container);

	Inventory inv;
	bool addedItem = testAddSingle(&inv, od, container);
	ASSERT_TRUE(addedItem == true);

	/* second try should fail as the right container is a single container */
	addedItem = testAddSingle(&inv, od, container);
	ASSERT_TRUE(addedItem == false);

	container = INVSH_GetInventoryDefinitionByID("left");
	ASSERT_TRUE(nullptr != container);

	od = INVSH_GetItemByIDSilent("fraggrenade");
	ASSERT_TRUE(nullptr != od);

	addedItem = testAddSingle(&inv, od, container);
	ASSERT_TRUE(addedItem == true);

	container = INVSH_GetInventoryDefinitionByID("equip");
	ASSERT_TRUE(nullptr != container);

	for (int i = 0; i < csi.numODs; i++) {
		od = INVSH_GetItemByIDX(i);
		/* every item should be placable on the ground container and there should really be enough space */
		addedItem = testAddSingle(&inv, od, container);
		ASSERT_TRUE(addedItem == true);
		addedItem = testAddSingle(&inv, od, container);
		ASSERT_TRUE(addedItem == true);
		addedItem = testAddSingle(&inv, od, container);
		ASSERT_TRUE(addedItem == true);
		for (int j = 0; j < od->numAmmos; j++) {
			addedItem = testAddSingle(&inv, od->ammos[j], container);
			ASSERT_TRUE(addedItem == true);
			addedItem = testAddSingle(&inv, od->ammos[j], container);
			ASSERT_TRUE(addedItem == true);
			addedItem = testAddSingle(&inv, od->ammos[j], container);
			ASSERT_TRUE(addedItem == true);
			addedItem = testAddSingle(&inv, od->ammos[j], container);
			ASSERT_TRUE(addedItem == true);
			addedItem = testAddSingle(&inv, od->ammos[j], container);
			ASSERT_TRUE(addedItem == true);
			addedItem = testAddSingle(&inv, od->ammos[j], container);
			ASSERT_TRUE(addedItem == true);
		}
	}
}

TEST_F(InventoryTest, ItemToHeadgear)
{
	Inventory inv;
	const objDef_t* od;
	const invDef_t* container;

	od = INVSH_GetItemByIDSilent("irgoggles");
	ASSERT_TRUE(nullptr != od);

	container = INVSH_GetInventoryDefinitionByID("headgear");
	ASSERT_TRUE(nullptr != container);

	Item item(od);

	ASSERT_FALSE(inv.containsItem(container->id, &item));

	ASSERT_TRUE(nullptr != i.addToInventory(&inv, &item, container, NONE, NONE, 1));

	ASSERT_TRUE(inv.containsItem(container->id, &item));

	ASSERT_TRUE(nullptr == i.addToInventory(&inv, &item, container, NONE, NONE, 1));
}
