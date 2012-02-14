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
#include "../client/battlescape/cl_particle.h"
#include "../client/cgame/campaign/cp_campaign.h"

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

static qboolean TEST_CheckImage (const char *path)
{
	const char *extensions[] = {"png", "tga", "jpg", NULL};
	int i = 0;

	if (Q_strnull(path))
		return qtrue;

	while (extensions[i]) {
		if (FS_CheckFile("pics/%s.%s", path, extensions[i]) != -1)
			return qtrue;
		i++;
	}

	return qfalse;
}

static qboolean TEST_CheckModel (const char *path)
{
	const char *extensions[] = {"md2", "md3", "dpm", "obj", NULL};
	int i = 0;
	if (Q_strnull(path))
		return qtrue;

	while (extensions[i]) {
		if (FS_CheckFile("models/%s.%s", path, extensions[i]) != -1)
			return qtrue;
		i++;
	}

	return qfalse;
}

static qboolean TEST_CheckSound (const char *path)
{
	const char *extensions[] = {"wav", "ogg", NULL};
	int i = 0;

	if (Q_strnull(path))
		return qtrue;

	while (extensions[i]) {
		if (FS_CheckFile("sound/%s.%s", path, extensions[i]) != -1)
			return qtrue;
		i++;
	}

	return qfalse;
}

static qboolean TEST_CheckParticle (const char *particleID)
{
	if (Q_strnull(particleID))
		return qtrue;

	/* find the particle definition */
	return CL_ParticleGet(particleID) != NULL;
}

static void testTeamDefs (void)
{
	int i;

	for (i = 0; i < csi.numTeamDefs; i++) {
		const teamDef_t *teamDef = &csi.teamDef[i];
		int k;

		UFO_CU_ASSERT_TRUE_MSG(teamDef->numTemplates > 0, va("%s has no character templates assigned", teamDef->id));

		for (k = 0; k < SND_MAX; k++) {
			int l;
			for (l = 0; l < NAME_LAST; l++) {
				LIST_Foreach(teamDef->sounds[k][l], char, soundFile) {
					UFO_CU_ASSERT_TRUE_MSG(TEST_CheckSound(soundFile), va("sound %s does not exist (team %s)", soundFile, teamDef->id));
				}
			}
		}
	}
}

static void testTeamDefsModelScriptData (void)
{
	int i;
	linkedList_t *armourPaths = NULL;

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

		LIST_Foreach(armourPaths, char const, armourPath) {
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
					/** @todo check that the skin is valid for the given model */
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

static void testItems (void)
{
	int j;

	for (j = 0; j < csi.numODs; j++) {
		const objDef_t *od = INVSH_GetItemByIDX(j);
		int i;

		if (od->isVirtual || od->isDummy)
			continue;

		UFO_CU_ASSERT_TRUE_MSG(TEST_CheckSound(od->reloadSound), va("sound %s does not exist (item %s)", od->reloadSound, od->id));
		UFO_CU_ASSERT_TRUE_MSG(TEST_CheckModel(od->model), va("model %s does not exist (item %s)", od->model, od->id));
		UFO_CU_ASSERT_TRUE_MSG(TEST_CheckImage(od->image), va("image %s does not exist (item %s)", od->image, od->id));

		for (i = 0; i < od->numWeapons; i++) {
			int k;

			for (k = 0; k < od->numFiredefs[i]; k++) {
				const fireDef_t *fd = &od->fd[i][k];
				UFO_CU_ASSERT_TRUE_MSG(TEST_CheckSound(fd->bounceSound), va("sound %s does not exist (firedef %s for item %s)", fd->bounceSound, fd->name, od->id));
				UFO_CU_ASSERT_TRUE_MSG(TEST_CheckSound(fd->fireSound), va("sound %s does not exist (firedef %s for item %s)", fd->fireSound, fd->name, od->id));
				UFO_CU_ASSERT_TRUE_MSG(TEST_CheckSound(fd->impactSound), va("sound %s does not exist (firedef %s for item %s)", fd->impactSound, fd->name, od->id));
				UFO_CU_ASSERT_TRUE_MSG(TEST_CheckSound(fd->hitBodySound), va("sound %s does not exist (firedef %s for item %s)", fd->hitBodySound, fd->name, od->id));
				UFO_CU_ASSERT_TRUE_MSG(TEST_CheckParticle(fd->hitBody), va("particle %s does not exist (firedef %s for item %s)", fd->hitBody, fd->name, od->id));
				UFO_CU_ASSERT_TRUE_MSG(TEST_CheckParticle(fd->impact), va("particle %s does not exist (firedef %s for item %s)", fd->impact, fd->name, od->id));
				UFO_CU_ASSERT_TRUE_MSG(TEST_CheckParticle(fd->projectile), va("particle %s does not exist (firedef %s for item %s)", fd->projectile, fd->name, od->id));
			}
		}
	}
}

static void testNations (void)
{
	int i;

	for (i = 0; i < ccs.numNations; i++) {
		const nation_t *nat = NAT_GetNationByIDX(i);
		UFO_CU_ASSERT_TRUE_MSG(TEST_CheckImage(va("nations/%s", nat->id)), va("nation %s has no image", nat->id));
		CU_ASSERT_PTR_NOT_NULL(Com_GetTeamDefinitionByID(nat->id));
	}
}

static void testAircraft (void)
{
	AIR_Foreach(aircraft) {
		UFO_CU_ASSERT_TRUE_MSG(TEST_CheckModel(aircraft->model), va("%s does not exist (aircraft: %s)", aircraft->model, aircraft->id));
		UFO_CU_ASSERT_TRUE_MSG(TEST_CheckImage(aircraft->image), va("%s does not exist (aircraft: %s)", aircraft->image, aircraft->id));
	}
}

static void testMapDef (void)
{
	int i;
	const mapDef_t *md;

	i = 0;
	MapDef_Foreach(md) {
		if (md->civTeam != NULL)
			CU_ASSERT_PTR_NOT_NULL(Com_GetTeamDefinitionByID(md->civTeam));

		CU_ASSERT_FALSE(md->maxAliens <= 0);
		CU_ASSERT_PTR_NOT_NULL(md->map);
		CU_ASSERT_PTR_NOT_NULL(md->description);
		i++;
	}

	CU_ASSERT_PTR_NULL(md);
	UFO_CU_ASSERT_EQUAL_INT_MSG_FATAL(i, csi.numMDs, va("only looped over %i mapdefs, but expected %i", i, csi.numMDs));

	i = csi.numMDs;

	MapDef_ForeachCondition(md, !md->singleplayer || !md->campaign) {
		i--;
		CU_ASSERT_PTR_NOT_NULL(md);
	}

	CU_ASSERT_PTR_NULL(md);
	CU_ASSERT_NOT_EQUAL(i, csi.numMDs);

	MapDef_ForeachSingleplayerCampaign(md) {
		i--;
		CU_ASSERT_PTR_NOT_NULL(md);
		CU_ASSERT_STRING_NOT_EQUAL(md->id, "training_a");
		CU_ASSERT_STRING_NOT_EQUAL(md->id, "training_b");
	}

	CU_ASSERT_PTR_NULL(md);
	UFO_CU_ASSERT_EQUAL_INT_MSG_FATAL(i, 0, va("i: %i", i));
}

int UFO_AddScriptsTests (void)
{
	/* add a suite to the registry */
	CU_pSuite ScriptsSuite = CU_add_suite("ScriptsTests", UFO_InitSuiteScripts, UFO_CleanSuiteScripts);

	if (ScriptsSuite == NULL)
		return CU_get_error();

	/* add the tests to the suite */
	if (CU_ADD_TEST(ScriptsSuite, testTeamDefs) == NULL)
		return CU_get_error();

	if (CU_ADD_TEST(ScriptsSuite, testTeamDefsModelScriptData) == NULL)
		return CU_get_error();

	if (CU_ADD_TEST(ScriptsSuite, testItems) == NULL)
		return CU_get_error();

	if (CU_ADD_TEST(ScriptsSuite, testNations) == NULL)
		return CU_get_error();

	if (CU_ADD_TEST(ScriptsSuite, testAircraft) == NULL)
		return CU_get_error();

	if (CU_ADD_TEST(ScriptsSuite, testMapDef) == NULL)
		return CU_get_error();

	return CUE_SUCCESS;
}
