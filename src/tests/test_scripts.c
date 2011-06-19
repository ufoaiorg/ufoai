/**
 * @file test_scripts.c
 * @brief Test cases for checking the scripts for logic errors
 */

/*
Copyright (C) 2002-2011 UFO: Alien Invasion.

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
#include "test_scripts.h"
#include "../client/client.h"
#include "../client/renderer/r_state.h" /* r_state */
#include "../client/ui/ui_main.h"

/**
 * The suite initialization function.
 * Returns zero on success, non-zero otherwise.
 */
static int UFO_InitSuiteScripts (void)
{
	TEST_Init();

	cl_genericPool = Mem_CreatePool("Client: Generic");
	vid_imagePool = Mem_CreatePool("Vid: Image system");

	r_state.active_texunit = &r_state.texunits[0];
	R_FontInit();
	UI_Init();

	OBJZERO(cls);
	Com_ParseScripts(qfalse);

	return 0;
}

/**
 * The suite cleanup function.
 * Returns zero on success, non-zero otherwise.
 */
static int UFO_CleanSuiteScripts (void)
{
	UI_Shutdown();
	TEST_Shutdown();
	return 0;
}

static void testCharacterScriptData (void)
{
}

int UFO_AddScriptsTests (void)
{
	/* add a suite to the registry */
	CU_pSuite ScriptsSuite = CU_add_suite("ScriptsTests", UFO_InitSuiteScripts, UFO_CleanSuiteScripts);

	if (ScriptsSuite == NULL)
		return CU_get_error();

	/* add the tests to the suite */
	if (CU_ADD_TEST(ScriptsSuite, testCharacterScriptData) == NULL)
		return CU_get_error();

	return CUE_SUCCESS;
}
