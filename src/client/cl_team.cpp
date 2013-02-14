/**
 * @file
 * @brief Team management, name generation and parsing.
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

See the GNU General Public License for more details.m

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

*/

#include "client.h"
#include "cl_team.h"
#include "cl_inventory.h"
#include "cgame/cl_game.h"
#include "battlescape/cl_localentity.h"
#include "battlescape/cl_actor.h"
#include "battlescape/events/e_parse.h"
#include "battlescape/events/e_main.h"
#include "ui/ui_data.h"
#include "ui/ui_nodes.h"

/** @brief List of currently displayed or equipable characters. */
linkedList_t *chrDisplayList;

/**
 * @brief Allocate a skin from the cls structure
 * @return A actorskin structure
 */
actorSkin_t* CL_AllocateActorSkin (const char *name)
{
	int index;
	actorSkin_t *skin;

	if (cls.numActorSkins >= lengthof(cls.actorSkins))
		Sys_Error("CL_AllocateActorSkin: Max actorskin hit");

	index = R_ModAllocateActorSkin(name);
	skin = &cls.actorSkins[index];
	OBJZERO(*skin);
	skin->idx = index;

	skin->id = Mem_PoolStrDup(name, com_genericPool, 0);

	cls.numActorSkins++;
	return skin;
}

/**
 * @brief Get number of registered actorskins
 * @return Number of registered actorskins
 */
unsigned int CL_GetActorSkinCount (void)
{
	return cls.numActorSkins;
}

/**
 * @brief Get a actorskin from idx
 * @return A actorskin, else NULL
 */
static const actorSkin_t* CL_GetActorSkinByIDS (unsigned int idx)
{
	if (idx >= cls.numActorSkins)
		return NULL;
	return &cls.actorSkins[idx];
}

static void CL_CharacterSkillAndScoreCvars (const character_t *chr)
{
	const chrScoreGlobal_t *score = &chr->score;
	Cvar_ForceSet("mn_name", chr->name);
	Cvar_ForceSet("mn_body", CHRSH_CharGetBody(chr));
	Cvar_ForceSet("mn_head", CHRSH_CharGetHead(chr));
	Cvar_ForceSet("mn_body_skin", va("%i", chr->bodySkin));
	Cvar_ForceSet("mn_head_skin", va("%i", chr->headSkin));

	Cvar_Set("mn_vpwr", va("%i", score->skills[ABILITY_POWER]));
	Cvar_Set("mn_vspd", va("%i", score->skills[ABILITY_SPEED]));
	Cvar_Set("mn_vacc", va("%i", score->skills[ABILITY_ACCURACY]));
	Cvar_Set("mn_vmnd", va("%i", score->skills[ABILITY_MIND]));
	Cvar_Set("mn_vcls", va("%i", score->skills[SKILL_CLOSE]));
	Cvar_Set("mn_vhvy", va("%i", score->skills[SKILL_HEAVY]));
	Cvar_Set("mn_vass", va("%i", score->skills[SKILL_ASSAULT]));
	Cvar_Set("mn_vsnp", va("%i", score->skills[SKILL_SNIPER]));
	Cvar_Set("mn_vexp", va("%i", score->skills[SKILL_EXPLOSIVE]));
	Cvar_Set("mn_vpil", va("%i", chr->score.skills[SKILL_PILOTING]));
	Cvar_Set("mn_vtar", va("%i", chr->score.skills[SKILL_TARGETING]));
	Cvar_Set("mn_vevad", va("%i", chr->score.skills[SKILL_EVADING]));
	Cvar_Set("mn_vpwri", va("%i", score->initialSkills[ABILITY_POWER]));
	Cvar_Set("mn_vspdi", va("%i", score->initialSkills[ABILITY_SPEED]));
	Cvar_Set("mn_vacci", va("%i", score->initialSkills[ABILITY_ACCURACY]));
	Cvar_Set("mn_vmndi", va("%i", score->initialSkills[ABILITY_MIND]));
	Cvar_Set("mn_vclsi", va("%i", score->initialSkills[SKILL_CLOSE]));
	Cvar_Set("mn_vhvyi", va("%i", score->initialSkills[SKILL_HEAVY]));
	Cvar_Set("mn_vassi", va("%i", score->initialSkills[SKILL_ASSAULT]));
	Cvar_Set("mn_vsnpi", va("%i", score->initialSkills[SKILL_SNIPER]));
	Cvar_Set("mn_vexpi", va("%i", score->initialSkills[SKILL_EXPLOSIVE]));
	Cvar_Set("mn_vpili", va("%i", chr->score.initialSkills[SKILL_PILOTING]));
	Cvar_Set("mn_vtari", va("%i", chr->score.initialSkills[SKILL_TARGETING]));
	Cvar_Set("mn_vevadi", va("%i", chr->score.initialSkills[SKILL_EVADING]));
	Cvar_Set("mn_vhp", va("%i", chr->HP));
	Cvar_Set("mn_vhpmax", va("%i", chr->maxHP));

	Cvar_Set("mn_tpwr", va("%s (%i)", CL_ActorGetSkillString(score->skills[ABILITY_POWER]), score->skills[ABILITY_POWER]));
	Cvar_Set("mn_tspd", va("%s (%i)", CL_ActorGetSkillString(score->skills[ABILITY_SPEED]), score->skills[ABILITY_SPEED]));
	Cvar_Set("mn_tacc", va("%s (%i)", CL_ActorGetSkillString(score->skills[ABILITY_ACCURACY]), score->skills[ABILITY_ACCURACY]));
	Cvar_Set("mn_tmnd", va("%s (%i)", CL_ActorGetSkillString(score->skills[ABILITY_MIND]), score->skills[ABILITY_MIND]));
	Cvar_Set("mn_tcls", va("%s (%i)", CL_ActorGetSkillString(score->skills[SKILL_CLOSE]), score->skills[SKILL_CLOSE]));
	Cvar_Set("mn_thvy", va("%s (%i)", CL_ActorGetSkillString(score->skills[SKILL_HEAVY]), score->skills[SKILL_HEAVY]));
	Cvar_Set("mn_tass", va("%s (%i)", CL_ActorGetSkillString(score->skills[SKILL_ASSAULT]), score->skills[SKILL_ASSAULT]));
	Cvar_Set("mn_tsnp", va("%s (%i)", CL_ActorGetSkillString(score->skills[SKILL_SNIPER]), score->skills[SKILL_SNIPER]));
	Cvar_Set("mn_texp", va("%s (%i)", CL_ActorGetSkillString(score->skills[SKILL_EXPLOSIVE]), score->skills[SKILL_EXPLOSIVE]));
	Cvar_Set("mn_tpil", va("%s (%i)", CL_ActorGetSkillString(chr->score.skills[SKILL_PILOTING]), chr->score.skills[SKILL_PILOTING]));
	Cvar_Set("mn_ttar", va("%s (%i)", CL_ActorGetSkillString(chr->score.skills[SKILL_TARGETING]), chr->score.skills[SKILL_TARGETING]));
	Cvar_Set("mn_tevad", va("%s (%i)", CL_ActorGetSkillString(chr->score.skills[SKILL_EVADING]), chr->score.skills[SKILL_EVADING]));
	Cvar_Set("mn_thp", va("%i (%i)", chr->HP, chr->maxHP));
}

/**
 * @brief Updates the character cvars for the given character.
 *
 * The models and stats that are displayed in the menu are stored in cvars.
 * These cvars are updated here when you select another character.
 *
 * @param[in] chr Pointer to character_t (may not be null)
 * @sa CL_UGVCvars
 * @sa CL_ActorSelect
 */
static void CL_ActorCvars (const character_t *chr)
{
	invList_t *weapon;
	assert(chr);

	/* visible equipment */
	weapon = RIGHT(chr);
	if (weapon)
		Cvar_Set("mn_rweapon", weapon->item.def()->model);
	else
		Cvar_Set("mn_rweapon", "");
	weapon = LEFT(chr);
	if (weapon)
		Cvar_Set("mn_lweapon", weapon->item.def()->model);
	else
		Cvar_Set("mn_lweapon", "");
}

/**
 * @brief Updates the UGV cvars for the given "character".
 * The models and stats that are displayed in the menu are stored in cvars.
 * These cvars are updated here when you select another character.
 * @param[in] chr Pointer to character_t (may not be null)
 * @sa CL_ActorCvars
 * @sa CL_ActorSelect
 */
static void CL_UGVCvars (const character_t *chr)
{
	Cvar_Set("mn_lweapon", "");
	Cvar_Set("mn_rweapon", "");
	Cvar_Set("mn_vmnd", "0");
	Cvar_Set("mn_tmnd", va("%s (0)", CL_ActorGetSkillString(chr->score.skills[ABILITY_MIND])));
}

void CL_UpdateCharacterValues (const character_t *chr)
{
	CL_CharacterSkillAndScoreCvars(chr);

	if (chr->teamDef->robot)
		CL_UGVCvars(chr);
	else
		CL_ActorCvars(chr);

	GAME_CharacterCvars(chr);
}

/**
 * @brief Generates the skills and inventory for a character and for a 2x2 unit
 * @param[in] chr The employee to create character data for.
 * @param[in] teamDefName Which team to use for creation.
 */
void CL_GenerateCharacter (character_t *chr, const char *teamDefName)
{
	OBJZERO(*chr);

	/* link inventory */
	cls.i.destroyInventory(&chr->inv);

	/* get ucn */
	chr->ucn = cls.nextUniqueCharacterNumber++;

	chr->reservedTus.shotSettings.set(ACTOR_HAND_NOT_SET, -1, NULL);

	Com_GetCharacterValues(teamDefName, chr);
	/* Create attributes. */
	CHRSH_CharGenAbilitySkills(chr, GAME_IsMultiplayer());
}

/**
 * @brief Init skins into the GUI
 */
static void CL_InitSkin_f (void)
{
	/* create option for singleplayer skins */
	if (UI_GetOption(OPTION_SINGLEPLAYER_SKINS) == NULL) {
		uiNode_t *skins = NULL;
		int idx = 0;
		const actorSkin_t *skin;
		while ((skin = CL_GetActorSkinByIDS(idx++))) {
			if (!skin->singleplayer)
				continue;
			UI_AddOption(&skins, skin->id, skin->name, va("%d", skin->idx));
		}
		UI_RegisterOption(OPTION_SINGLEPLAYER_SKINS, skins);
	}

	/* create option for multiplayer skins */
	if (UI_GetOption(OPTION_MULTIPLAYER_SKINS) == NULL) {
		uiNode_t *skins = NULL;
		int idx = 0;
		const actorSkin_t *skin;
		while ((skin = CL_GetActorSkinByIDS(idx++))) {
			if (!skin->multiplayer)
				continue;
			UI_AddOption(&skins, skin->id, skin->name, va("%d", skin->idx));
		}
		UI_RegisterOption(OPTION_MULTIPLAYER_SKINS, skins);
	}
}

/**
 * @brief Fix actorskin idx according to game mode
 */
static int CL_FixActorSkinIDX (int idx)
{
	const actorSkin_t *skin = CL_GetActorSkinByIDS(idx);

	/** @todo we should check somewhere there is at least 1 skin */
	if (skin == NULL) {
		idx = 0;
	} else {
		if (GAME_IsSingleplayer() && !skin->singleplayer)
			idx = 0;
		else if (GAME_IsMultiplayer() && !skin->multiplayer)
			idx = 0;
	}
	return idx;
}

/**
 * @brief Change the skin of the selected actor.
 */
static void CL_ChangeSkin_f (void)
{
	const int sel = cl_selected->integer;

	int i = 0;
	LIST_Foreach(chrDisplayList, character_t, chr) {
		if (i++ != sel)
			continue;
		const int newSkin = CL_FixActorSkinIDX(Cvar_GetInteger("mn_body_skin"));
		/** @todo Get the skin id from the model by using the actorskin id */
		/** @todo Or remove skins from models and convert character_t->skin to string */
		chr->bodySkin = newSkin;
		Cvar_SetValue("mn_body_skin", newSkin);
	}
}

/**
 * @brief Use current skin for all team members onboard.
 */
static void CL_ChangeSkinForWholeTeam_f (void)
{
	/* Apply new skin to all (shown/displayed) team-members. */
	/** @todo What happens if a model of a team member does not have the selected skin? */
	LIST_Foreach(chrDisplayList, character_t, chr) {
		/* Get selected skin and fall back to default skin if it is not valid. */
		const int newSkin = CL_FixActorSkinIDX(Cvar_GetInteger("mn_body_skin"));
		/** @todo Get the skin id from the model by using the actorskin id */
		/** @todo Or remove skins from models and convert character_t->skin to string */
		chr->bodySkin = newSkin;
	}
}

void TEAM_InitStartup (void)
{
	Cmd_AddCommand("team_initskin", CL_InitSkin_f, "Init skin according to the game mode");
	Cmd_AddCommand("team_changeskin", CL_ChangeSkin_f, "Change the skin of the soldier");
	Cmd_AddCommand("team_changeskinteam", CL_ChangeSkinForWholeTeam_f, "Change the skin for the whole current team");
}
