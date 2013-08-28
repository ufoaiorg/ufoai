/**
 * @file
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


#include "test_character.h"
#include "test_shared.h"
#include "../client/cl_team.h"

/**
 * The suite initialization function.
 * Returns zero on success, non-zero otherwise.
 */
static int UFO_InitSuiteDBuffer (void)
{
	TEST_Init();

	Com_ParseScripts(true);
	return 0;
}

/**
 * The suite cleanup function.
 * Returns zero on success, non-zero otherwise.
 */
static int UFO_CleanSuiteDBuffer (void)
{
	TEST_Shutdown();
	return 0;
}

static character_t* GetCharacter (const char *teamDefID = "phalanx")
{
	static character_t chr;
	CL_GenerateCharacter(&chr, teamDefID);
	return &chr;
}

static void testImplants (void)
{
	character_t* chr = GetCharacter();
	const implantDef_t* implantDef = INVSH_GetImplantByID("muscular");
	CU_ASSERT_PTR_NOT_NULL_FATAL(implantDef);
	const objDef_t* od = implantDef->item;
	CU_ASSERT_PTR_NOT_NULL_FATAL(od);
	CU_ASSERT_PTR_NOT_NULL_FATAL(od->strengthenEffect);
	const implant_t* implant = CHRSH_ApplyImplant(*chr, *implantDef);
	CU_ASSERT_PTR_NOT_NULL_FATAL(implant);
	CU_ASSERT_EQUAL(implant->removedTime, 0);
	CU_ASSERT_NOT_EQUAL(implant->installedTime, 0);
	CU_ASSERT_PTR_NOT_NULL(implant->def);
	CU_ASSERT_EQUAL(implant->installedTime, implantDef->installationTime);
	for (int i = 0; i < implantDef->installationTime; i++) {
		CHRSH_UpdateImplants(*chr);
	}
	CU_ASSERT_EQUAL(implant->installedTime, 0);
}

int UFO_AddCharacterTests (void)
{
	/* add a suite to the registry */
	CU_pSuite DBufferSuite = CU_add_suite("CharacterTests", UFO_InitSuiteDBuffer, UFO_CleanSuiteDBuffer);
	if (DBufferSuite == NULL)
		return CU_get_error();

	/* add the tests to the suite */
	if (CU_ADD_TEST(DBufferSuite, testImplants) == NULL)
		return CU_get_error();

	return CUE_SUCCESS;
}
