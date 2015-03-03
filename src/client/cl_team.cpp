/**
 * @file
 * @brief Team management, name generation and parsing.
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
linkedList_t* chrDisplayList;

/**
 * @brief Allocate a skin from the cls structure
 * @return A actorskin structure
 */
actorSkin_t* CL_AllocateActorSkin (const char* name)
{
	if (cls.numActorSkins >= lengthof(cls.actorSkins))
		Sys_Error("CL_AllocateActorSkin: Max actorskin hit");

	const int index = R_ModAllocateActorSkin(name);
	actorSkin_t* skin = &cls.actorSkins[index];
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
 * @return A actorskin, else nullptr
 */
static const actorSkin_t* CL_GetActorSkinByIDS (unsigned int idx)
{
	if (idx >= cls.numActorSkins)
		return nullptr;
	return &cls.actorSkins[idx];
}

/**
 * @todo: replace this cvar hell with confuncs
 */
static void CL_CharacterSkillAndScoreCvars (const character_t* chr)
{
	const chrScoreGlobal_t* score = &chr->score;
	Cvar_ForceSet("mn_name", chr->name);
	Cvar_ForceSet("mn_body", CHRSH_CharGetBody(chr));
	Cvar_ForceSet("mn_head", CHRSH_CharGetHead(chr));
	Cvar_ForceSet("mn_body_skin", va("%i", chr->bodySkin));
	Cvar_ForceSet("mn_head_skin", va("%i", chr->headSkin));

	Cvar_SetValue("mn_vpwr", score->skills[ABILITY_POWER]);
	Cvar_SetValue("mn_vspd", score->skills[ABILITY_SPEED]);
	Cvar_SetValue("mn_vacc", score->skills[ABILITY_ACCURACY]);
	Cvar_SetValue("mn_vmnd", score->skills[ABILITY_MIND]);
	Cvar_SetValue("mn_vcls", score->skills[SKILL_CLOSE]);
	Cvar_SetValue("mn_vhvy", score->skills[SKILL_HEAVY]);
	Cvar_SetValue("mn_vass", score->skills[SKILL_ASSAULT]);
	Cvar_SetValue("mn_vsnp", score->skills[SKILL_SNIPER]);
	Cvar_SetValue("mn_vexp", score->skills[SKILL_EXPLOSIVE]);
	Cvar_SetValue("mn_vpil", score->skills[SKILL_PILOTING]);
	Cvar_SetValue("mn_vtar", score->skills[SKILL_TARGETING]);
	Cvar_SetValue("mn_vevad", score->skills[SKILL_EVADING]);
	Cvar_SetValue("mn_vpwri", score->initialSkills[ABILITY_POWER]);
	Cvar_SetValue("mn_vspdi", score->initialSkills[ABILITY_SPEED]);
	Cvar_SetValue("mn_vacci", score->initialSkills[ABILITY_ACCURACY]);
	Cvar_SetValue("mn_vmndi", score->initialSkills[ABILITY_MIND]);
	Cvar_SetValue("mn_vclsi", score->initialSkills[SKILL_CLOSE]);
	Cvar_SetValue("mn_vhvyi", score->initialSkills[SKILL_HEAVY]);
	Cvar_SetValue("mn_vassi", score->initialSkills[SKILL_ASSAULT]);
	Cvar_SetValue("mn_vsnpi", score->initialSkills[SKILL_SNIPER]);
	Cvar_SetValue("mn_vexpi", score->initialSkills[SKILL_EXPLOSIVE]);
	Cvar_SetValue("mn_vpili", score->initialSkills[SKILL_PILOTING]);
	Cvar_SetValue("mn_vtari", score->initialSkills[SKILL_TARGETING]);
	Cvar_SetValue("mn_vevadi", score->initialSkills[SKILL_EVADING]);
	Cvar_SetValue("mn_vhp", chr->HP);
	Cvar_SetValue("mn_vhpmax", chr->maxHP);

	Cvar_Set("mn_tpwr", "%s (%i)", CL_ActorGetSkillString(score->skills[ABILITY_POWER]), score->skills[ABILITY_POWER]);
	Cvar_Set("mn_tspd", "%s (%i)", CL_ActorGetSkillString(score->skills[ABILITY_SPEED]), score->skills[ABILITY_SPEED]);
	Cvar_Set("mn_tacc", "%s (%i)", CL_ActorGetSkillString(score->skills[ABILITY_ACCURACY]), score->skills[ABILITY_ACCURACY]);
	Cvar_Set("mn_tmnd", "%s (%i)", CL_ActorGetSkillString(score->skills[ABILITY_MIND]), score->skills[ABILITY_MIND]);
	Cvar_Set("mn_tcls", "%s (%i)", CL_ActorGetSkillString(score->skills[SKILL_CLOSE]), score->skills[SKILL_CLOSE]);
	Cvar_Set("mn_thvy", "%s (%i)", CL_ActorGetSkillString(score->skills[SKILL_HEAVY]), score->skills[SKILL_HEAVY]);
	Cvar_Set("mn_tass", "%s (%i)", CL_ActorGetSkillString(score->skills[SKILL_ASSAULT]), score->skills[SKILL_ASSAULT]);
	Cvar_Set("mn_tsnp", "%s (%i)", CL_ActorGetSkillString(score->skills[SKILL_SNIPER]), score->skills[SKILL_SNIPER]);
	Cvar_Set("mn_texp", "%s (%i)", CL_ActorGetSkillString(score->skills[SKILL_EXPLOSIVE]), score->skills[SKILL_EXPLOSIVE]);
	Cvar_Set("mn_tpil", "%s (%i)", CL_ActorGetSkillString(score->skills[SKILL_PILOTING]), score->skills[SKILL_PILOTING]);
	Cvar_Set("mn_ttar", "%s (%i)", CL_ActorGetSkillString(score->skills[SKILL_TARGETING]), score->skills[SKILL_TARGETING]);
	Cvar_Set("mn_tevad", "%s (%i)", CL_ActorGetSkillString(score->skills[SKILL_EVADING]), score->skills[SKILL_EVADING]);
	Cvar_Set("mn_thp", "%i (%i)", chr->HP, chr->maxHP);
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
static void CL_ActorCvars (const character_t* chr)
{
	Item* weapon;
	assert(chr);

	/* visible equipment */
	weapon = chr->inv.getRightHandContainer();
	if (weapon)
		Cvar_Set("mn_rweapon", "%s", weapon->def()->model);
	else
		Cvar_Set("mn_rweapon", "");
	weapon = chr->inv.getLeftHandContainer();
	if (weapon)
		Cvar_Set("mn_lweapon", "%s", weapon->def()->model);
	else
		Cvar_Set("mn_lweapon", "");
}

/**
 * @brief Return the skill string for the given skill level
 * @return skill string
 * @param[in] skill a skill value between 0 and MAX_SKILL
 */
const char* CL_ActorGetSkillString (const int skill)
{
	const int skillLevel = skill * 10 / MAX_SKILL;
#ifdef DEBUG
	if (skill > MAX_SKILL) {
		Com_Printf("CL_GetSkillString: Skill is bigger than max allowed skill value (%i/%i).\n", skill, MAX_SKILL);
	}
#endif
	switch (skillLevel) {
	case 0:
		return _("Poor");
	case 1:
		return _("Mediocre");
	case 2:
		return _("Average");
	case 3:
		return _("Competent");
	case 4:
		return _("Proficient");
	case 5:
		return _("Very Good");
	case 6:
		return _("Highly Proficient");
	case 7:
		return _("Excellent");
	case 8:
		return _("Outstanding");
	case 9:
		return _("Impressive");
	case 10:
		return _("Superhuman");
	default:
		Com_Printf("CL_ActorGetSkillString: Unknown skill: %i (index: %i).\n", skill, skillLevel);
		return "";
	}
}

/**
 * @brief Updates the UGV cvars for the given "character".
 * The models and stats that are displayed in the menu are stored in cvars.
 * These cvars are updated here when you select another character.
 * @param[in] chr Pointer to character_t (may not be null)
 * @sa CL_ActorCvars
 * @sa CL_ActorSelect
 */
static void CL_UGVCvars (const character_t* chr)
{
	Cvar_Set("mn_lweapon", "");
	Cvar_Set("mn_rweapon", "");
	Cvar_Set("mn_vmnd", "0");
	Cvar_Set("mn_tmnd", "%s (0)", CL_ActorGetSkillString(chr->score.skills[ABILITY_MIND]));
}

void CL_UpdateCharacterValues (const character_t* chr)
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
void CL_GenerateCharacter (character_t* chr, const char* teamDefName)
{
	chr->init();

	/* link inventory */
	cls.i.destroyInventory(&chr->inv);

	/* get ucn */
	chr->ucn = cls.nextUniqueCharacterNumber++;

	chr->reservedTus.shotSettings.set(ACTOR_HAND_NOT_SET, -1, nullptr);

	Com_GetCharacterValues(teamDefName, chr);
	/* Create attributes. */
	CHRSH_CharGenAbilitySkills(chr, GAME_IsMultiplayer());

	chr->RFmode.set(ACTOR_HAND_NOT_SET, -1, nullptr);
	chr->state |= STATE_REACTION;
}

/**
 * @brief Init skins into the GUI
 */
static void CL_InitSkin_f (void)
{
	/* create option for singleplayer skins */
	if (UI_GetOption(OPTION_SINGLEPLAYER_SKINS) == nullptr) {
		uiNode_t* skins = nullptr;
		int idx = 0;
		const actorSkin_t* skin;
		while ((skin = CL_GetActorSkinByIDS(idx++))) {
			if (!skin->singleplayer)
				continue;
			UI_AddOption(&skins, skin->id, skin->name, va("%d", skin->idx));
		}
		UI_RegisterOption(OPTION_SINGLEPLAYER_SKINS, skins);
	}

	/* create option for multiplayer skins */
	if (UI_GetOption(OPTION_MULTIPLAYER_SKINS) == nullptr) {
		uiNode_t* skins = nullptr;
		int idx = 0;
		const actorSkin_t* skin;
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
	const actorSkin_t* skin = CL_GetActorSkinByIDS(idx);

	/** @todo we should check somewhere there is at least 1 skin */
	if (skin == nullptr) {
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
	character_t* chr = (character_t*)LIST_GetByIdx(chrDisplayList, sel);
	if (chr == nullptr) {
		return;
	}
	const int newSkin = CL_FixActorSkinIDX(Cvar_GetInteger("mn_body_skin"));
	Cvar_SetValue("mn_body_skin", newSkin);
	/** @todo Get the skin id from the model by using the actorskin id */
	/** @todo Or remove skins from models and convert character_t->skin to string */
	chr->bodySkin = newSkin;
}

/**
 * @brief Use current skin for all team members onboard.
 */
static void CL_ChangeSkinForWholeTeam_f (void)
{
	/* Get selected skin and fall back to default skin if it is not valid. */
	const int newSkin = CL_FixActorSkinIDX(Cvar_GetInteger("mn_body_skin"));
	/* Apply new skin to all (shown/displayed) team-members. */
	/** @todo What happens if a model of a team member does not have the selected skin? */
	LIST_Foreach(chrDisplayList, character_t, chr) {
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
