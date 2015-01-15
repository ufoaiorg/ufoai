/**
 * @file
 * @brief Test cases for code below client/renderer
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
#include "../client/cl_video.h"
#include "../client/renderer/r_image.h"
#include "../client/renderer/r_model.h"

class RendererTest: public ::testing::Test {
protected:
	static void SetUpTestCase() {
		TEST_Init();
	}

	static void TearDownTestCase() {
		TEST_Shutdown();
	}
};

TEST_F(RendererTest, LoadAllAnimationFiles)
{
	const char* pattern = "models/**.anm";
	const char* filename;
	mAliasModel_t mod;

	OBJZERO(mod);
	/* set a very high value to work around the error check in the loading function */
	mod.num_frames = 100000;

	while ((filename = FS_NextFileFromFileList(pattern)) != nullptr) {
		vid_modelPool = Mem_CreatePool("Vid Model Pool");
		Com_Printf("load anim file: %s\n", filename);
		R_ModLoadAnims(&mod, filename);
		Mem_DeletePool(vid_modelPool);
	}

	FS_NextFileFromFileList(nullptr);
}

TEST_F(RendererTest, CharacterAnimationFiles)
{
	const char* pattern = "models/**.anm";
	const char* filename;
	mAliasModel_t mod;
	const char* bloodspider[] = { "death1", "death2", "death3", "dead1",
			"dead2", "dead3", "stand0", "stand1", "stand2", "stand3", "walk0",
			"walk1", "walk2", "walk3", "cstand0", "cstand1", "cstand2",
			"cstand3", "cwalk0", "cwalk1", "cwalk2", "cwalk3", "stand_menu",
			"panic0", nullptr };
	const char* hovernet[] = { "death1", "dead1", "death2","dead2", "death3",
			"dead3", "stand0", "walk0", "cstand0", "cwalk0", "stand1", "walk1",
			"cstand1", "cwalk1", "stand2", "walk2", "cstand2", "cwalk2",
			"stand3", "walk3", "cstand3", "cwalk3", "move_rifle", "shoot_rifle",
			"cmove_rifle", "cshoot_rifle", "stand_menu", "panic0", nullptr };
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
			"cmove_rpg", "cshoot_rpg", nullptr };
	const char* civilians[] = { "death1", "dead1", "death2", "dead2", "death3",
			"dead3", "stand0", "walk0", "panic0", "stand1", "stand2",
			"stand_menu", "stand_still", nullptr };

	FS_BuildFileList(pattern);

	vid_modelPool = Mem_CreatePool("Vid Model Pool");

	while ((filename = FS_NextFileFromFileList(pattern)) != nullptr) {
		const char** animList;
		if (Q_strstart(filename, "models/soldiers/"))
			animList = soldiers;
		else if (Q_strstart(filename, "models/civilians/"))
			animList = civilians;
		else if (Q_strstart(filename, "models/aliens/bloodspider"))
			animList = bloodspider;
		else if (Q_strstart(filename, "models/aliens/hovernet"))
			animList = hovernet;
		else if (Q_strstart(filename, "models/aliens/"))
			animList = soldiers;
		else
			animList = nullptr;

		/** @todo remove this hack - but ugvs are just not ready yet */
		if (Q_strstart(filename, "models/soldiers/ugv_"))
			continue;
		/** @todo remove this hack - alientank is just not ready yet */
		if (Q_strstart(filename, "models/aliens/alientank/"))
			continue;

		if (animList != nullptr) {
			OBJZERO(mod);
			/* set a very high value to work around the error check in the loading function */
			mod.num_frames = 100000;

			Com_Printf("load character anim file: %s\n", filename);
			R_ModLoadAnims(&mod, filename);

			while (*animList != nullptr) {
				int i;
				for (i = 0; i < mod.num_anims; i++) {
					const mAliasAnim_t* a = &mod.animdata[i];
					if (Q_streq(a->name, *animList))
						break;
				}
				ASSERT_FALSE(i == mod.num_anims) << "anm file " << filename << " does not contain the needed animation definition " << *animList;

				animList++;
			}
		}
	}

	Mem_DeletePool(vid_modelPool);

	FS_NextFileFromFileList(nullptr);
}
