/**
 * @file test_generic.c
 * @brief Test cases for code below common/ and shared/
 */

/*
Copyright (C) 2002-2009 UFO: Alien Invasion.

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
#include "../common/common.h"
#include "test_generic.h"

/**
 * The suite initialization function.
 * Returns zero on success, non-zero otherwise.
 */
static int UFO_InitSuiteGeneric (void)
{
	com_aliasSysPool = Mem_CreatePool("Common: Alias system");
	com_cmdSysPool = Mem_CreatePool("Common: Command system");
	com_cmodelSysPool = Mem_CreatePool("Common: Collision model");
	com_cvarSysPool = Mem_CreatePool("Common: Cvar system");
	com_fileSysPool = Mem_CreatePool("Common: File system");
	com_genericPool = Mem_CreatePool("Generic");

	Mem_Init();
	Cmd_Init();
	Cvar_Init();
	FS_InitFilesystem(qtrue);
	Swap_Init();

	return 0;
}

/**
 * The suite cleanup function.
 * Returns zero on success, non-zero otherwise.
 */
static int UFO_CleanSuiteGeneric (void)
{
	FS_Shutdown();
	Cmd_Shutdown();
	Cvar_Shutdown();
	Mem_Shutdown();
	return 0;
}

static void testConstInt (void)
{
	int out;

	Com_RegisterConstInt("namespace::variable", 1);
	CU_ASSERT(Com_UnregisterConstVariable("namespace::variable"));

	Com_RegisterConstInt("namespace::variable", 1);
	CU_ASSERT(Com_UnregisterConstVariable("namespace::variable"));

	Com_RegisterConstInt("namespace::variable2", 2);
	Com_RegisterConstInt("namespace::variable3", 3);
	Com_RegisterConstInt("namespace::variable4", 4);
	Com_RegisterConstInt("namespace::variable5", 5);
	Com_RegisterConstInt("namespace::variable6", 6);

	out = 0;
	CU_ASSERT_TRUE(Com_GetConstInt("namespace::variable2", &out));
	CU_ASSERT_EQUAL(out, 2);
	out = 0;
	CU_ASSERT_TRUE(Com_GetConstInt("variable2", &out));
	CU_ASSERT_EQUAL(out, 2);

	CU_ASSERT_STRING_EQUAL(Com_GetConstVariable("namespace", 2), "variable2");

	CU_ASSERT(Com_UnregisterConstVariable("namespace::variable2"));
	CU_ASSERT(Com_UnregisterConstVariable("namespace::variable3"));
	CU_ASSERT(Com_UnregisterConstVariable("namespace::variable4"));
	CU_ASSERT(Com_UnregisterConstVariable("namespace::variable5"));
	CU_ASSERT(Com_UnregisterConstVariable("namespace::variable6"));

	CU_ASSERT(!Com_UnregisterConstVariable("namespace::variable"));
	CU_ASSERT(!Com_UnregisterConstVariable("namespace::variable2"));
	CU_ASSERT(!Com_UnregisterConstVariable("namespace::variable3"));
	CU_ASSERT(!Com_UnregisterConstVariable("namespace::variable4"));
	CU_ASSERT(!Com_UnregisterConstVariable("namespace::variable5"));
	CU_ASSERT(!Com_UnregisterConstVariable("namespace::variable6"));
}


static void testLinkedList (void)
{
	linkedList_t *list = NULL;
	const char* data = "SomeDataForTheLinkedList";
	const size_t length = strlen(data);
	linkedList_t *entry;
	const linkedList_t *entry2;
	const char *returnedData;

	entry = LIST_Add(&list, (const byte*)data, length);
	CU_ASSERT_EQUAL(LIST_Count(list), 1);
	CU_ASSERT_TRUE(entry != NULL);
	returnedData = LIST_GetByIdx(list, 0);
	CU_ASSERT_TRUE(returnedData != NULL);
	entry2 = LIST_ContainsString(list, returnedData);
	CU_ASSERT_TRUE(entry2 != NULL);
	CU_ASSERT_EQUAL((const void*)entry2->data, (const void*)returnedData);
	CU_ASSERT_STRING_EQUAL(entry2->data, returnedData);
	LIST_Remove(&list, entry);
	CU_ASSERT_EQUAL(LIST_Count(list), 0);
}

int UFO_AddGenericTests (void)
{
	/* add a suite to the registry */
	CU_pSuite GenericSuite = CU_add_suite("GenericTests", UFO_InitSuiteGeneric, UFO_CleanSuiteGeneric);

	if (GenericSuite == NULL)
		return CU_get_error();

	/* add the tests to the suite */
	if (CU_ADD_TEST(GenericSuite, testConstInt) == NULL)
		return CU_get_error();

	if (CU_ADD_TEST(GenericSuite, testLinkedList) == NULL)
		return CU_get_error();

	return CUE_SUCCESS;
}
