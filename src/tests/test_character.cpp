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
static int InitSuite (void)
{
	TEST_Init();

	Com_ParseScripts(true);
	return 0;
}

/**
 * The suite cleanup function.
 * Returns zero on success, non-zero otherwise.
 */
static int CleanSuite (void)
{
	TEST_Shutdown();
	return 0;
}

static character_t* GetCharacter (const char* teamDefID = "phalanx")
{
	static character_t chr;
	CL_GenerateCharacter(&chr, teamDefID);
	return &chr;
}

static void RunStrengthenImplant (character_t& chr, const implant_t& implant, const objDef_t& od)
{
	Com_Printf("%s ", od.id);
	/** @todo perform tests */
}

static void RunEffectForImplant (const fireDef_t& fd, character_t& chr, const implant_t& implant, const objDef_t& od, const itemEffect_t& effect)
{
	Com_Printf("%s ", implant.def->id);
	/** @todo perform tests */
}

static void RunImplant (const implantDef_t& implantDef)
{
	character_t* chr = GetCharacter();
	const objDef_t* od = implantDef.item;
	CU_ASSERT_PTR_NOT_NULL_FATAL(od);
	const implant_t* implant = CHRSH_ApplyImplant(*chr, implantDef);
	CU_ASSERT_PTR_NOT_NULL_FATAL(implant);
	CU_ASSERT_PTR_NOT_NULL_FATAL(implant->def);
	Com_Printf("implant: '%s': ", implant->def->id);
	CU_ASSERT_EQUAL(implant->removedTime, 0);
	CU_ASSERT_NOT_EQUAL(implant->installedTime, 0);
	CU_ASSERT_PTR_NOT_NULL(implant->def);
	CU_ASSERT_EQUAL(implant->installedTime, implantDef.installationTime);
	for (int i = 0; i < implantDef.installationTime; i++) {
		CHRSH_UpdateImplants(*chr);
	}
	CU_ASSERT_EQUAL(implant->installedTime, 0);

	int effects = 0;
	if (od->strengthenEffect != nullptr) {
		RunStrengthenImplant(*chr, *implant, *od);
		effects++;
	}
	for (int w = 0; w < MAX_WEAPONS_PER_OBJDEF; w++) {
		for (int f = 0; f < od->numFiredefs[w]; f++) {
			const fireDef_t& fd = od->fd[w][f];
			const itemEffect_t* effect[] = { fd.activeEffect, fd.deactiveEffect, fd.overdoseEffect };
			for (int e = 0; e < lengthof(effect); e++) {
				if (effect[e] == nullptr)
					continue;
				RunEffectForImplant(fd, *chr, *implant, *od, *effect[e]);
				effects++;
			}
		}
	}
	CU_ASSERT_TRUE_FATAL(effects >= 1);
	Com_Printf("passed %i effects\n", effects);
}

static void testImplants (void)
{
	Com_Printf("\n");
	for (int i = 0; i < csi.numImplants; i++) {
		RunImplant(csi.implants[i]);
	}
}

int UFO_AddCharacterTests (void)
{
	/* add a suite to the registry */
	CU_pSuite DBufferSuite = CU_add_suite("CharacterTests", InitSuite, CleanSuite);
	if (DBufferSuite == nullptr)
		return CU_get_error();

	/* add the tests to the suite */
	if (CU_ADD_TEST(DBufferSuite, testImplants) == nullptr)
		return CU_get_error();

	return CUE_SUCCESS;
}
