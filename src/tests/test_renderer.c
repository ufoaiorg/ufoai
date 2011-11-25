/**
 * @file test_renderer.c
 * @brief Test cases for code below client/renderer
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
#include "test_renderer.h"
#include "../client/cl_video.h"
#include "../client/renderer/r_image.h"
#include "../client/renderer/r_model.h"

/**
 * The suite initialization function.
 * Returns zero on success, non-zero otherwise.
 */
static int UFO_InitSuiteRenderer (void)
{
	TEST_Init();
	return 0;
}

/**
 * The suite cleanup function.
 * Returns zero on success, non-zero otherwise.
 */
static int UFO_CleanSuiteRenderer (void)
{
	TEST_Shutdown();
	return 0;
}

static void testLoadAllAnimationFiles (void)
{
	const char *pattern = "models/**.anm";
	const char *filename;
	mAliasModel_t mod;

	FS_BuildFileList(pattern);

	OBJZERO(mod);
	/* set a very high value to work around the error check in the loading function */
	mod.num_frames = 100000;

	while ((filename = FS_NextFileFromFileList(pattern)) != NULL) {
		byte *animbuf;

		vid_modelPool = Mem_CreatePool("Vid Model Pool");
		FS_LoadFile(filename, &animbuf);
		Com_Printf("load anim file: %s\n", filename);
		R_ModLoadAnims(&mod, (const char *)animbuf);
		FS_FreeFile(animbuf);
		Mem_DeletePool(vid_modelPool);
	}

	FS_NextFileFromFileList(NULL);
}

static void testCharacterAnimationFiles (void)
{
	const char *pattern = "models/**.anm";
	const char *filename;
	mAliasModel_t mod;
	const char* bloodspider[] = { "death1", "death2", "death3", "dead1",
			"dead2", "dead3", "stand0", "stand1", "stand2", "stand3", "walk0",
			"walk1", "walk2", "walk3", "cstand0", "cstand1", "cstand2",
			"cstand3", "cwalk0", "cwalk1", "cwalk2", "cwalk3", "stand_menu",
			"panic0", NULL };
	const char* soldiers[] = { "death1", "death2", "death3", "dead1", "dead2",
			"dead3", "stand0", "stand1", "stand2", "stand3", "walk0", "walk1",
			"walk2", "walk3", "cstand0", "cstand1", "cstand2", "cstand3",
			"cwalk0", "cwalk1", "cwalk2", "cwalk3", "stand_menu", "panic0",
			"move_rifle", "shoot_rifle", "cmove_rifle", "cshoot_rifle",
			"move_biggun", "shoot_biggun", "cmove_biggun", "cshoot_biggun",
			"move_melee", "shoot_melee", "cmove_melee", "cshoot_melee",
			"stand_still", "move_pistol", "shoot_pistol", "cmove_pistol",
			"cshoot_pistol", "move_pistol_d", "shoot_pistol_d",
			"cmove_pistol_d", "cshoot_pistol_d", "move_grenade",
			"shoot_grenade", "cmove_grenade", "cshoot_grenade", "move_item",
			"shoot_item", "cmove_item", "cshoot_item", "move_rpg", "shoot_rpg",
			"cmove_rpg", "cshoot_rpg", NULL };
	const char* civilians[] = { "death1", "dead1", "death2", "dead2", "death3",
			"dead3", "stand0", "walk0", "panic0", "stand1", "stand2",
			"stand_menu", "stand_still", NULL };

	FS_BuildFileList(pattern);

	vid_modelPool = Mem_CreatePool("Vid Model Pool");

	while ((filename = FS_NextFileFromFileList(pattern)) != NULL) {
		const char **animList;
		if (Q_strstart(filename, "models/soldiers/"))
			animList = soldiers;
		else if (Q_strstart(filename, "models/civilians/"))
			animList = civilians;
		else if (Q_strstart(filename, "models/aliens/bloodspider/"))
			animList = bloodspider;
		else if (Q_strstart(filename, "models/aliens/"))
			animList = soldiers;
		else
			animList = NULL;

		/** @todo remove this hack - but ugvs are just not ready yet */
		if (Q_strstart(filename, "models/soldiers/ugv_"))
			continue;
		/** @todo remove this hack - alientank is just not ready yet */
		if (Q_strstart(filename, "models/aliens/alientank/"))
			continue;

		if (animList != NULL) {
			byte *animbuf;

			OBJZERO(mod);
			/* set a very high value to work around the error check in the loading function */
			mod.num_frames = 100000;

			FS_LoadFile(filename, &animbuf);
			Com_Printf("load character anim file: %s\n", filename);
			R_ModLoadAnims(&mod, (const char *)animbuf);
			FS_FreeFile(animbuf);

			while (*animList != NULL) {
				int i;
				for (i = 0; i < mod.num_anims; i++) {
					const mAliasAnim_t *a = &mod.animdata[i];
					if (Q_streq(a->name, *animList))
						break;
				}
				if (i == mod.num_anims)
					UFO_CU_ASSERT_MSG(va("anm file %s does not contain the needed animation definition %s", filename, *animList));

				animList++;
			}
		}
	}

	Mem_DeletePool(vid_modelPool);

	FS_NextFileFromFileList(NULL);
}

int UFO_AddRendererTests (void)
{
	/* add a suite to the registry */
	CU_pSuite RendererSuite = CU_add_suite("RendererTests", UFO_InitSuiteRenderer, UFO_CleanSuiteRenderer);

	if (RendererSuite == NULL)
		return CU_get_error();

	/* add the tests to the suite */
	if (CU_ADD_TEST(RendererSuite, testLoadAllAnimationFiles) == NULL)
		return CU_get_error();

	if (CU_ADD_TEST(RendererSuite, testCharacterAnimationFiles) == NULL)
		return CU_get_error();

	return CUE_SUCCESS;
}
