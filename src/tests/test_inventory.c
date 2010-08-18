/**
 * @file test_inventory.c
 */

/*
Copyright (C) 2002-2010 UFO: Alien Invasion.

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


#include "CUnit/Basic.h"
#include "test_inventory.h"
#include "test_shared.h"
#include "../common/common.h"
#include "../game/inventory.h"

static inventoryInterface_t i;

static inline void ResetInventoryList (void)
{
	INV_DestroyInventory("test", &i);
	INV_InitInventory("test", &i, &csi, com_genericPool);
}

/**
 * The suite initialization function.
 * Returns zero on success, non-zero otherwise.
 */
static int UFO_InitSuiteInventory (void)
{
	TEST_Init();

	Com_ParseScripts(qfalse);

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
	inventory_t inv;
	objDef_t *od;
	item_t item;
	invDef_t *container;

	ResetInventoryList();

	memset(&inv, 0, sizeof(inv));

	od = INVSH_GetItemByIDSilent("assault");
	CU_ASSERT_PTR_NOT_NULL(od);

	container = INVSH_GetInventoryDefinitionByID("right");
	CU_ASSERT_PTR_NOT_NULL(container);

	item.t = od;
	item.m = NULL;
	item.a = 0;

	CU_ASSERT(INVSH_ExistsInInventory(&inv, container, item) == qfalse);

	CU_ASSERT_PTR_NOT_NULL(i.AddToInventory(&i, &inv, item, container, NONE, NONE, 1));

	CU_ASSERT(INVSH_ExistsInInventory(&inv, container, item) == qtrue);
}

static void testItemDel (void)
{
	inventory_t inv;
	objDef_t *od;
	item_t item;
	invDef_t *container;
	invList_t *addedItem;

	ResetInventoryList();

	memset(&inv, 0, sizeof(inv));

	od = INVSH_GetItemByIDSilent("assault");
	CU_ASSERT_PTR_NOT_NULL(od);

	container = INVSH_GetInventoryDefinitionByID("right");
	CU_ASSERT_PTR_NOT_NULL(container);

	item.t = od;
	item.m = NULL;
	item.a = 0;

	CU_ASSERT(INVSH_ExistsInInventory(&inv, container, item) == qfalse);

	addedItem = i.AddToInventory(&i, &inv, item, container, NONE, NONE, 1);

	CU_ASSERT(INVSH_ExistsInInventory(&inv, container, item) == qtrue);

	CU_ASSERT(i.RemoveFromInventory(&i, &inv, container, addedItem));

	CU_ASSERT(INVSH_ExistsInInventory(&inv, container, item) == qfalse);
}

static void testItemMove (void)
{
	inventory_t inv;
	objDef_t *od;
	item_t item;
	invDef_t *container, *containerTo;
	invList_t *addedItem;

	ResetInventoryList();

	memset(&inv, 0, sizeof(inv));

	od = INVSH_GetItemByIDSilent("assault");
	CU_ASSERT_PTR_NOT_NULL(od);

	container = INVSH_GetInventoryDefinitionByID("right");
	CU_ASSERT_PTR_NOT_NULL(container);

	item.t = od;
	item.m = NULL;
	item.a = 0;

	CU_ASSERT(INVSH_ExistsInInventory(&inv, container, item) == qfalse);

	addedItem = i.AddToInventory(&i, &inv, item, container, NONE, NONE, 1);
	CU_ASSERT_PTR_NOT_NULL(addedItem);

	CU_ASSERT(INVSH_ExistsInInventory(&inv, container, item) == qtrue);

	containerTo = INVSH_GetInventoryDefinitionByID("backpack");
	CU_ASSERT_PTR_NOT_NULL(containerTo);

	CU_ASSERT_EQUAL(IA_MOVE, i.MoveInInventory(&i, &inv, container, addedItem, containerTo, NONE, NONE, NULL, NULL));

	CU_ASSERT(INVSH_ExistsInInventory(&inv, container, item) == qfalse);
	CU_ASSERT(INVSH_ExistsInInventory(&inv, containerTo, item) == qtrue);
}

static qboolean testAddSingle (inventory_t *inv, objDef_t *od, invDef_t *container)
{
	item_t item;

	item.t = od;
	item.m = NULL;
	item.a = 0;

	return i.TryAddToInventory(&i, inv, item, container);
}

static void testItemMassActions (void)
{
	inventory_t inv;
	objDef_t *od;
	invDef_t *container;
	qboolean addedItem;
	int i;

	ResetInventoryList();

	memset(&inv, 0, sizeof(inv));

	od = INVSH_GetItemByIDSilent("assault");
	CU_ASSERT_PTR_NOT_NULL_FATAL(od);

	container = INVSH_GetInventoryDefinitionByID("right");
	CU_ASSERT_PTR_NOT_NULL_FATAL(container);

	addedItem = testAddSingle(&inv, od, container);
	CU_ASSERT(addedItem == qtrue);

	/* second try should fail as the right container is a single container */
	addedItem = testAddSingle(&inv, od, container);
	CU_ASSERT(addedItem == qfalse);

	container = INVSH_GetInventoryDefinitionByID("left");
	CU_ASSERT_PTR_NOT_NULL_FATAL(container);

	od = INVSH_GetItemByIDSilent("fraggrenade");
	CU_ASSERT_PTR_NOT_NULL_FATAL(od);

	addedItem = testAddSingle(&inv, od, container);
	CU_ASSERT(addedItem == qtrue);

	container = INVSH_GetInventoryDefinitionByID("equip");
	CU_ASSERT_PTR_NOT_NULL_FATAL(container);

	for (i = 0; i < csi.numODs; i++) {
		objDef_t *od = INVSH_GetItemByIDX(i);
		int j;
		/* every item should be placable on the ground container and there should really be enough space */
		addedItem = testAddSingle(&inv, od, container);
		CU_ASSERT(addedItem == qtrue);
		addedItem = testAddSingle(&inv, od, container);
		CU_ASSERT(addedItem == qtrue);
		addedItem = testAddSingle(&inv, od, container);
		CU_ASSERT(addedItem == qtrue);
		for (j = 0; j < od->numAmmos; j++) {
 			addedItem = testAddSingle(&inv, od->ammos[j], container);
			CU_ASSERT(addedItem == qtrue);
 			addedItem = testAddSingle(&inv, od->ammos[j], container);
			CU_ASSERT(addedItem == qtrue);
 			addedItem = testAddSingle(&inv, od->ammos[j], container);
			CU_ASSERT(addedItem == qtrue);
 			addedItem = testAddSingle(&inv, od->ammos[j], container);
			CU_ASSERT(addedItem == qtrue);
 			addedItem = testAddSingle(&inv, od->ammos[j], container);
			CU_ASSERT(addedItem == qtrue);
 			addedItem = testAddSingle(&inv, od->ammos[j], container);
			CU_ASSERT(addedItem == qtrue);
 		}
	}
}

int UFO_AddInventoryTests (void)
{
	/* add a suite to the registry */
	CU_pSuite InventorySuite = CU_add_suite("InventoryTests", UFO_InitSuiteInventory, UFO_CleanSuiteInventory);
	if (InventorySuite == NULL)
		return CU_get_error();

	/* add the tests to the suite */
	if (CU_ADD_TEST(InventorySuite, testItemAdd) == NULL)
		return CU_get_error();
	if (CU_ADD_TEST(InventorySuite, testItemDel) == NULL)
		return CU_get_error();
	if (CU_ADD_TEST(InventorySuite, testItemMove) == NULL)
		return CU_get_error();
	if (CU_ADD_TEST(InventorySuite, testItemMassActions) == NULL)
		return CU_get_error();

	return CUE_SUCCESS;
}
