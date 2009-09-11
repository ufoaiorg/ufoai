/**
 * @file test_rma.c
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
#include "test_rma.h"

#include "../server/sv_rma.c"
#include "../server/sv_init.c"

/**
 * The suite initialization function.
 * Returns zero on success, non-zero otherwise.
 */
static int UFO_InitSuiteRandomMapAssembly (void)
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

	sv_dumpmapassembly = Cvar_Get("sv_dumpassembly", "0", 0, NULL);

	return 0;
}

/**
 * The suite cleanup function.
 * Returns zero on success, non-zero otherwise.
 */
static int UFO_CleanSuiteRandomMapAssembly (void)
{
	FS_Shutdown();
	Cmd_Shutdown();
	Cvar_Shutdown();
	Mem_Shutdown();
	return 0;
}

char map[MAX_TOKEN_CHARS * MAX_TILESTRINGS];
char pos[MAX_TOKEN_CHARS * MAX_TILESTRINGS];

static void testAssembly (void)
{
	mapInfo_t *randomMap;

	srand(0);
	randomMap = SV_AssembleMap("forest", "large", map, pos);
	CU_ASSERT(randomMap != NULL);
	Mem_Free(randomMap);
}

/* parallel version */
static void testMassAssemblyParallel (void)
{
	int i;
	long time;
	mapInfo_t *randomMap;

	sv_threads->integer = 1;
	for (i = 0; i < 50; i++) {
		srand(i);
		time = Sys_Milliseconds();
		randomMap = SV_AssembleMap("forest", "large", map, pos);
		CU_ASSERT(randomMap != NULL);
		time = (Sys_Milliseconds() - time);
		CU_ASSERT(time < 30000);
		printf("%i: %i %li\n", i, randomMap->numPlaced, time); fflush(stdout);
		if (randomMap)
			Mem_Free(randomMap);
	}
}

/* sequential version */
static void testMassAssemblySequential (void)
{
	int i;
	long time;
	mapInfo_t *randomMap;

	sv_threads->integer = 0;
	for (i = 0; i < 50; i++) {
		srand(i);
		time = Sys_Milliseconds();
		randomMap = SV_AssembleMap("forest", "large", map, pos);
		CU_ASSERT(randomMap != NULL);
		time = (Sys_Milliseconds() - time);
		CU_ASSERT(time < 30000);
		printf("%i: %i %li\n", i, randomMap->numPlaced, time); fflush(stdout);
		if (randomMap)
			Mem_Free(randomMap);
	}
}

int UFO_AddRandomMapAssemblyTests (void)
{
	static cvar_t svt;

	if (!sv_threads) {
		sv_threads = &svt;
		sv_threads->integer = 0;
	}

		/* add a suite to the registry */
	CU_pSuite RandomMapAssemblySuite = CU_add_suite("RandomMapAssemblyTests", UFO_InitSuiteRandomMapAssembly, UFO_CleanSuiteRandomMapAssembly);
	if (RandomMapAssemblySuite == NULL)
		return CU_get_error();

	/* add the tests to the suite */
	if (CU_ADD_TEST(RandomMapAssemblySuite, testAssembly) == NULL)
		return CU_get_error();

	if (CU_ADD_TEST(RandomMapAssemblySuite, testMassAssemblySequential) == NULL)
		return CU_get_error();

	if (CU_ADD_TEST(RandomMapAssemblySuite, testMassAssemblyParallel) == NULL)
		return CU_get_error();

	return CUE_SUCCESS;
}
