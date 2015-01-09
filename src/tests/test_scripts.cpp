/**
 * @file
 * @brief Test cases for checking the scripts for logic errors
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
#include "../client/client.h"
#include "../client/renderer/r_state.h" /* r_state */
#include "../client/ui/ui_main.h"
#include "../client/battlescape/cl_particle.h"
#include "../client/cgame/campaign/cp_campaign.h"

class ScriptTest: public ::testing::Test {
protected:
	static void SetUpTestCase() {
		TEST_Init();

		cl_genericPool = Mem_CreatePool("Client: Generic");
		vid_imagePool = Mem_CreatePool("Vid: Image system");

		r_state.active_texunit = &r_state.texunits[0];
		R_FontInit();
		UI_Init();

		OBJZERO(cls);
		Com_ParseScripts(false);
	}

	static void TearDownTestCase() {
		TEST_Shutdown();
	}
};

static bool TEST_CheckImage (const char* path)
{
	const char* extensions[] = {"png", "tga", "jpg", nullptr};
	int i = 0;

	if (Q_strnull(path))
		return true;

	while (extensions[i]) {
		if (FS_CheckFile("pics/%s.%s", path, extensions[i]) != -1)
			return true;
		i++;
	}

	return false;
}

static bool TEST_CheckModel (const char* path)
{
	const char* extensions[] = {"md2", "md3", "obj", nullptr};
	int i = 0;
	if (Q_strnull(path))
		return true;

	while (extensions[i]) {
		if (FS_CheckFile("models/%s.%s", path, extensions[i]) != -1)
			return true;
		i++;
	}

	return false;
}

static bool TEST_CheckSound (const char* path)
{
	const char* extensions[] = {"wav", "ogg", nullptr};
	int i = 0;

	if (Q_strnull(path))
		return true;

	while (extensions[i]) {
		if (FS_CheckFile("sound/%s.%s", path, extensions[i]) != -1)
			return true;
		i++;
	}

	return false;
}

static bool TEST_CheckParticle (const char* particleID)
{
	if (Q_strnull(particleID))
		return true;

	/* find the particle definition */
	return CL_ParticleGet(particleID) != nullptr;
}

TEST_F(ScriptTest, TeamDefs)
{
	for (int i = 0; i < csi.numTeamDefs; i++) {
		const teamDef_t* teamDef = &csi.teamDef[i];

		ASSERT_TRUE(teamDef->numTemplates > 0) << teamDef->id << " has no character templates assigned";

		for (int k = 0; k < SND_MAX; k++) {
			for (int l = 0; l < NAME_LAST; l++) {
				LIST_Foreach(teamDef->sounds[k][l], char, soundFile) {
					ASSERT_TRUE(TEST_CheckSound(soundFile)) << "sound " << soundFile << " does not exist (team " << teamDef->id << ")";
				}
			}
		}
	}
}

TEST_F(ScriptTest, TeamDefsModelScriptData)
{
	linkedList_t* armourPaths = nullptr;

	for (int i = 0; i < csi.numTeamDefs; i++) {
		const teamDef_t* teamDef = &csi.teamDef[i];
		if (!teamDef->armour)
			continue;

		for (int j = 0; j < csi.numODs; j++) {
			const objDef_t* od = INVSH_GetItemByIDX(j);
			if (!od->isArmour())
				continue;

			/* not for this team */
			if (!CHRSH_IsArmourUseableForTeam(od, teamDef))
				continue;

			if (!LIST_ContainsString(armourPaths, od->armourPath))
				LIST_AddString(&armourPaths, od->armourPath);
		}

		ASSERT_TRUE(!LIST_IsEmpty(armourPaths)) << "no armour definitions found for team " << teamDef->id << " - but armour is set to true";

		LIST_Foreach(armourPaths, char const, armourPath) {
			for (int l = NAME_NEUTRAL; l < NAME_LAST; l++) {
				/* no models for this gender */
				if (!teamDef->numModels[l])
					continue;

				ASSERT_TRUE(nullptr != teamDef->models[l]);

				for (linkedList_t const* list = teamDef->models[l]; list; list = list->next) {
					teamDef_t::model_t const& m = *static_cast<teamDef_t::model_t const*>(list->data);

					ASSERT_TRUE(TEST_CheckModel(va("%s/%s", m.path, m.body))) << m.body << " does not exist in models/" << m.path << " (teamDef: " << teamDef->id << ")";
					ASSERT_TRUE(TEST_CheckModel(va("%s%s/%s", m.path, armourPath, m.body))) << m.body << " does not exist in models/" << m.path << armourPath << " (teamDef: " << teamDef->id << ")";

					ASSERT_TRUE(TEST_CheckModel(va("%s/%s", m.path, m.head))) << m.head << " does not exist in models/" << m.path << " (teamDef: " << teamDef->id << ")";
					ASSERT_TRUE(TEST_CheckModel(va("%s%s/%s", m.path, armourPath, m.head))) << m.head << " does not exist in models/" << m.path << armourPath << " (teamDef: " << teamDef->id << ")";
				}
			}
		}

		LIST_Delete(&armourPaths);
	}
}

TEST_F(ScriptTest, Items)
{
	for (int j = 0; j < csi.numODs; j++) {
		const objDef_t* od = INVSH_GetItemByIDX(j);
		if (od->isVirtual || od->isDummy)
			continue;

		ASSERT_TRUE(TEST_CheckSound(od->reloadSound)) << "sound " << od->reloadSound << " does not exist (item " << od->id << ")";
		ASSERT_TRUE(TEST_CheckModel(od->model)) << "model " << od->model << " does not exist (item " << od->id << ")";
		ASSERT_TRUE(TEST_CheckImage(od->image)) << "image " << od->image << " does not exist (item " << od->id << ")";

		for (int i = 0; i < od->numWeapons; i++) {
			for (int k = 0; k < od->numFiredefs[i]; k++) {
				const fireDef_t* fd = &od->fd[i][k];
				ASSERT_TRUE(TEST_CheckSound(fd->bounceSound)) << "sound " << fd->bounceSound << " does not exist (firedef " << fd->name << " for item " << od->id << ")";
				ASSERT_TRUE(TEST_CheckSound(fd->fireSound)) << "sound " << fd->fireSound << " does not exist (firedef " << fd->name << " for item " << od->id << ")";
				ASSERT_TRUE(TEST_CheckSound(fd->impactSound)) << "sound " << fd->impactSound << " does not exist (firedef " << fd->name << " for item " << od->id << ")";
				ASSERT_TRUE(TEST_CheckSound(fd->hitBodySound)) << "sound " << fd->hitBodySound << " does not exist (firedef " << fd->name << " for item " << od->id << ")";
				ASSERT_TRUE(TEST_CheckParticle(fd->hitBody)) << "particle " << fd->hitBody << " does not exist (firedef " << fd->name << " for item " << od->id << ")";
				ASSERT_TRUE(TEST_CheckParticle(fd->impact)) << "particle " << fd->impact << " does not exist (firedef " << fd->name << " for item " << od->id << ")";
				ASSERT_TRUE(TEST_CheckParticle(fd->projectile)) << "particle " << fd->projectile << " does not exist (firedef " << fd->name << " for item " << od->id << ")";
			}
		}
	}
}

TEST_F(ScriptTest, Nations)
{
	for (int i = 0; i < ccs.numNations; i++) {
		const nation_t* nat = NAT_GetNationByIDX(i);
		ASSERT_TRUE(TEST_CheckImage(va("nations/%s", nat->id))) << "nation " << nat->id << " has no image";
		ASSERT_TRUE(nullptr != Com_GetTeamDefinitionByID(nat->id));
	}
}

TEST_F(ScriptTest, Aircraft)
{
	AIR_Foreach(aircraft) {
		ASSERT_TRUE(TEST_CheckModel(aircraft->model)) << aircraft->model << " does not exist (aircraft: " << aircraft->id << ")";
		ASSERT_TRUE(TEST_CheckImage(aircraft->image)) << aircraft->image << " does not exist (aircraft: " << aircraft->id << ")";
	}
}

TEST_F(ScriptTest, MapDef)
{
	const mapDef_t* md;

	int i = 0;
	MapDef_Foreach(md) {
		if (md->civTeam != nullptr)
			ASSERT_TRUE(nullptr != Com_GetTeamDefinitionByID(md->civTeam));

		ASSERT_FALSE(md->maxAliens <= 0);
		ASSERT_TRUE(nullptr != md->mapTheme);
		ASSERT_TRUE(nullptr != md->description);
		i++;
	}

	ASSERT_TRUE(nullptr == md);
	ASSERT_EQ(i, csi.numMDs) << "only looped over " << i << " mapdefs, but expected " << csi.numMDs;

	i = csi.numMDs;

	MapDef_ForeachCondition(md, !md->singleplayer || !md->campaign) {
		i--;
		ASSERT_TRUE(nullptr != md);
	}

	ASSERT_TRUE(nullptr == md);
	ASSERT_NE(i, csi.numMDs);

	MapDef_ForeachSingleplayerCampaign(md) {
		i--;
		ASSERT_TRUE(nullptr != md);
		ASSERT_STRNE(md->id, "training_a");
		ASSERT_STRNE(md->id, "training_b");
	}

	ASSERT_TRUE(nullptr == md);
	ASSERT_EQ(i, 0);
}
