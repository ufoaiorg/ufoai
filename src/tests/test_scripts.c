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

static qboolean TEST_CheckModel (const char *path)
{
	const char *extensions[] = {"md2", "md3", "dpm", "obj", NULL};
	int i = 0;

	while (extensions[i]) {
		if (FS_CheckFile("models/%s.%s", path, extensions[i]) != -1)
			return qtrue;
		i++;
	}

	return qfalse;
}

static void testCharacterScriptData (void)
{
	int i;
	linkedList_t *armourPaths = NULL;
	const char *armourPath;

	for (i = 0; i < csi.numTeamDefs; i++) {
		int j;
		const teamDef_t *teamDef = &csi.teamDef[i];
		if (!teamDef->armour)
			continue;

		for (j = 0; j < csi.numODs; j++) {
			const objDef_t *od = INVSH_GetItemByIDX(j);
			if (!INV_IsArmour(od))
				continue;

			/* not for this team */
			if (!CHRSH_IsArmourUseableForTeam(od, teamDef))
				continue;

			if (!LIST_ContainsString(armourPaths, od->armourPath))
				LIST_AddString(&armourPaths, od->armourPath);
		}

		UFO_CU_ASSERT_TRUE_MSG(!LIST_IsEmpty(armourPaths), va("no armour definitions found for team %s - but armour is set to true",
				teamDef->id));

		LIST_Foreach(armourPaths, char, armourPath) {
			nametypes_t l;

			for (l = NAME_NEUTRAL; l < NAME_LAST; l++) {
				linkedList_t *list = teamDef->models[l];
				int k;

				/* no models for this gender */
				if (!teamDef->numModels[l])
					continue;

				CU_ASSERT_PTR_NOT_NULL(list);
				for (k = 0; k < teamDef->numModels[l]; k++) {
					const char *path;

					CU_ASSERT_PTR_NOT_NULL_FATAL(list);
					path = (const char*)list->data;
					/* body */
					list = list->next;
					CU_ASSERT_PTR_NOT_NULL_FATAL(list);
					UFO_CU_ASSERT_TRUE_MSG(TEST_CheckModel(va("%s/%s", path, list->data)), va("%s does not exist in models/%s (teamDef: %s)",
							list->data, path, teamDef->id));
					UFO_CU_ASSERT_TRUE_MSG(TEST_CheckModel(va("%s%s/%s", path, armourPath, list->data)), va("%s does not exist in models/%s%s (teamDef: %s)",
							list->data, path, armourPath, teamDef->id));

					list = list->next;
					CU_ASSERT_PTR_NOT_NULL_FATAL(list);
					/* head */
					UFO_CU_ASSERT_TRUE_MSG(TEST_CheckModel(va("%s/%s", path, list->data)), va("%s does not exist in models/%s (teamDef: %s)",
							list->data, path, teamDef->id));
					UFO_CU_ASSERT_TRUE_MSG(TEST_CheckModel(va("%s%s/%s", path, armourPath, list->data)), va("%s does not exist in models/%s%s (teamDef: %s)",
							list->data, path, armourPath, teamDef->id));

					/* skip skin */
					list = list->next;
					CU_ASSERT_PTR_NOT_NULL_FATAL(list);

					/* new path */
					list = list->next;
				}
			}
		}

		LIST_Delete(&armourPaths);
	}
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
