/**
 * @file cl_team.c
 * @brief Team management, name generation and parsing.
 */

/*
Copyright (C) 2002-2010 UFO: Alien Invasion.

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
#include "cl_game.h"
#include "cl_team.h"
#include "battlescape/cl_localentity.h"
#include "battlescape/cl_actor.h"
#include "cl_inventory.h"
#include "cl_ugv.h"
#include "battlescape/events/e_parse.h"
#include "menu/m_main.h"
#include "menu/m_nodes.h"
#include "battlescape/events/e_main.h"
#include "campaign/save/save_character.h"
#include "campaign/save/save_inventory.h"

/** @brief List of currently displayed or equipeable characters. */
chrList_t chrDisplayList;

/**
 * @brief Translate the skin id to skin name
 * @param[in] id The id of the skin
 * @return Translated skin name
 */
static const char* CL_GetTeamSkinName (int id)
{
	switch(id) {
	case 0:
		return _("Urban");
	case 1:
		return _("Jungle");
	case 2:
		return _("Desert");
	case 3:
		return _("Arctic");
	case 4:
		return _("Yellow");
	case 5:
		return _("CCCP");
	}
	Com_Error(ERR_DROP, "CL_GetTeamSkinName: Unknown skin id %i - max is %i", id, NUM_TEAMSKINS - 1);
}

void CL_CharacterSkillAndScoreCvars (const character_t *chr)
{
	Cvar_ForceSet("mn_name", chr->name);
	Cvar_ForceSet("mn_body", CHRSH_CharGetBody(chr));
	Cvar_ForceSet("mn_head", CHRSH_CharGetHead(chr));
	Cvar_ForceSet("mn_skin", va("%i", chr->skin));
	Cvar_ForceSet("mn_skinname", CL_GetTeamSkinName(chr->skin));

	Cvar_Set("mn_vpwr", va("%i", chr->score.skills[ABILITY_POWER]));
	Cvar_Set("mn_vspd", va("%i", chr->score.skills[ABILITY_SPEED]));
	Cvar_Set("mn_vacc", va("%i", chr->score.skills[ABILITY_ACCURACY]));
	Cvar_Set("mn_vmnd", va("%i", chr->score.skills[ABILITY_MIND]));
	Cvar_Set("mn_vcls", va("%i", chr->score.skills[SKILL_CLOSE]));
	Cvar_Set("mn_vhvy", va("%i", chr->score.skills[SKILL_HEAVY]));
	Cvar_Set("mn_vass", va("%i", chr->score.skills[SKILL_ASSAULT]));
	Cvar_Set("mn_vsnp", va("%i", chr->score.skills[SKILL_SNIPER]));
	Cvar_Set("mn_vexp", va("%i", chr->score.skills[SKILL_EXPLOSIVE]));
	Cvar_Set("mn_vpwri", va("%i", chr->score.initialSkills[ABILITY_POWER]));
	Cvar_Set("mn_vspdi", va("%i", chr->score.initialSkills[ABILITY_SPEED]));
	Cvar_Set("mn_vacci", va("%i", chr->score.initialSkills[ABILITY_ACCURACY]));
	Cvar_Set("mn_vmndi", va("%i", chr->score.initialSkills[ABILITY_MIND]));
	Cvar_Set("mn_vclsi", va("%i", chr->score.initialSkills[SKILL_CLOSE]));
	Cvar_Set("mn_vhvyi", va("%i", chr->score.initialSkills[SKILL_HEAVY]));
	Cvar_Set("mn_vassi", va("%i", chr->score.initialSkills[SKILL_ASSAULT]));
	Cvar_Set("mn_vsnpi", va("%i", chr->score.initialSkills[SKILL_SNIPER]));
	Cvar_Set("mn_vexpi", va("%i", chr->score.initialSkills[SKILL_EXPLOSIVE]));
	Cvar_Set("mn_vhp", va("%i", chr->HP));
	Cvar_Set("mn_vhpmax", va("%i", chr->maxHP));

	Cvar_Set("mn_tpwr", va("%s (%i)", CL_ActorGetSkillString(chr->score.skills[ABILITY_POWER]), chr->score.skills[ABILITY_POWER]));
	Cvar_Set("mn_tspd", va("%s (%i)", CL_ActorGetSkillString(chr->score.skills[ABILITY_SPEED]), chr->score.skills[ABILITY_SPEED]));
	Cvar_Set("mn_tacc", va("%s (%i)", CL_ActorGetSkillString(chr->score.skills[ABILITY_ACCURACY]), chr->score.skills[ABILITY_ACCURACY]));
	Cvar_Set("mn_tmnd", va("%s (%i)", CL_ActorGetSkillString(chr->score.skills[ABILITY_MIND]), chr->score.skills[ABILITY_MIND]));
	Cvar_Set("mn_tcls", va("%s (%i)", CL_ActorGetSkillString(chr->score.skills[SKILL_CLOSE]), chr->score.skills[SKILL_CLOSE]));
	Cvar_Set("mn_thvy", va("%s (%i)", CL_ActorGetSkillString(chr->score.skills[SKILL_HEAVY]), chr->score.skills[SKILL_HEAVY]));
	Cvar_Set("mn_tass", va("%s (%i)", CL_ActorGetSkillString(chr->score.skills[SKILL_ASSAULT]), chr->score.skills[SKILL_ASSAULT]));
	Cvar_Set("mn_tsnp", va("%s (%i)", CL_ActorGetSkillString(chr->score.skills[SKILL_SNIPER]), chr->score.skills[SKILL_SNIPER]));
	Cvar_Set("mn_texp", va("%s (%i)", CL_ActorGetSkillString(chr->score.skills[SKILL_EXPLOSIVE]), chr->score.skills[SKILL_EXPLOSIVE]));
	Cvar_Set("mn_thp", va("%i (%i)", chr->HP, chr->maxHP));
}

/**
 * @brief saves a character to a given xml node
 * @param[in] p The node to which we should save the character
 * @param[in] chr The charcter we should save
 */
qboolean CL_SaveCharacterXML (mxml_node_t *p, const character_t* chr)
{
	mxml_node_t *sScore;
	mxml_node_t *sInventory;
	int k;
	const chrScoreGlobal_t *score;

	assert(chr);
	Com_RegisterConstList(saveCharacterConstants);
	/* Store the character data */
	mxml_AddString(p, SAVE_CHARACTER_NAME, chr->name);
	mxml_AddString(p, SAVE_CHARACTER_BODY, chr->body);
	mxml_AddString(p, SAVE_CHARACTER_PATH, chr->path);
	mxml_AddString(p, SAVE_CHARACTER_HEAD, chr->head);
	mxml_AddInt(p, SAVE_CHARACTER_SKIN, chr->skin);
	mxml_AddString(p, SAVE_CHARACTER_TEAMDEF, chr->teamDef->id);
	mxml_AddInt(p, SAVE_CHARACTER_GENDER, chr->gender);
	mxml_AddInt(p, SAVE_CHARACTER_UCN, chr->ucn);
	mxml_AddInt(p, SAVE_CHARACTER_MAXHP, chr->maxHP);
	mxml_AddInt(p, SAVE_CHARACTER_HP, chr->HP);
	mxml_AddInt(p, SAVE_CHARACTER_STUN, chr->STUN);
	mxml_AddInt(p, SAVE_CHARACTER_MORALE, chr->morale);
	mxml_AddInt(p, SAVE_CHARACTER_FIELDSIZE, chr->fieldSize);
	mxml_AddIntValue(p, SAVE_CHARACTER_STATE, chr->state);

	score = &chr->score;

	sScore = mxml_AddNode(p, SAVE_CHARACTER_SCORES);
	/* Store skills */
	for (k = 0; k <= SKILL_NUM_TYPES; k++) {
		if (score->experience[k] || score->initialSkills[k]
		 || (k < SKILL_NUM_TYPES && score->skills[k])) {
			mxml_node_t *sSkill = mxml_AddNode(sScore, SAVE_CHARACTER_SKILLS);
			const int initial = score->initialSkills[k];
			const int experience = score->experience[k];

			mxml_AddString(sSkill, SAVE_CHARACTER_SKILLTYPE, Com_GetConstVariable(SAVE_CHARACTER_SKILLTYPE_NAMESPACE, k));
			mxml_AddIntValue(sSkill, SAVE_CHARACTER_INITSKILL, initial);
			mxml_AddIntValue(sSkill, SAVE_CHARACTER_EXPERIENCE, experience);
			if (k < SKILL_NUM_TYPES) {
				const int skills = *(score->skills + k);
				const int improve = skills - initial;
				mxml_AddIntValue(sSkill, SAVE_CHARACTER_SKILLIMPROVE, improve);
			}
		}
	}
	/* Store kills */
	for (k = 0; k < KILLED_NUM_TYPES; k++) {
		mxml_node_t *sKill;
		if (score->kills[k] || score->stuns[k]) {
			sKill = mxml_AddNode(sScore, SAVE_CHARACTER_KILLS);
			mxml_AddString(sKill, SAVE_CHARACTER_KILLTYPE, Com_GetConstVariable(SAVE_CHARACTER_KILLTYPE_NAMESPACE, k));
			mxml_AddIntValue(sKill, SAVE_CHARACTER_KILLED, score->kills[k]);
			mxml_AddIntValue(sKill, SAVE_CHARACTER_STUNNED, score->stuns[k]);
		}
	}
	mxml_AddIntValue(sScore, SAVE_CHARACTER_SCORE_ASSIGNEDMISSIONS, score->assignedMissions);
	mxml_AddInt(sScore, SAVE_CHARACTER_SCORE_RANK, score->rank);

	/* Store inventories */
	sInventory = mxml_AddNode(p, SAVE_INVENTORY_INVENTORY);
	CL_SaveInventoryXML(sInventory, &chr->i);

	Com_UnregisterConstList(saveCharacterConstants);
	return qtrue;
}

/**
 * @brief Loads a character from a given xml node.
 * @param[in] p The node from which we should load the character.
 * @param[in] chr Pointer to the charcter we should load.
 */
qboolean CL_LoadCharacterXML (mxml_node_t *p, character_t *chr)
{
	const char *s;
	mxml_node_t *sScore;
	mxml_node_t *sSkill;
	mxml_node_t *sKill;
	mxml_node_t *sInventory;
	qboolean success = qtrue;

	/* Load the character data */
	Q_strncpyz(chr->name, mxml_GetString(p, SAVE_CHARACTER_NAME), sizeof(chr->name));
	Q_strncpyz(chr->body, mxml_GetString(p, SAVE_CHARACTER_BODY), sizeof(chr->body));
	Q_strncpyz(chr->path, mxml_GetString(p, SAVE_CHARACTER_PATH), sizeof(chr->path));
	Q_strncpyz(chr->head, mxml_GetString(p, SAVE_CHARACTER_HEAD), sizeof(chr->head));

	chr->skin = mxml_GetInt(p, SAVE_CHARACTER_SKIN, 0);
	chr->gender = mxml_GetInt(p, SAVE_CHARACTER_GENDER, 0);
	chr->ucn = mxml_GetInt(p, SAVE_CHARACTER_UCN, 0);
	chr->maxHP = mxml_GetInt(p, SAVE_CHARACTER_MAXHP, 0);
	chr->HP = mxml_GetInt(p, SAVE_CHARACTER_HP, 0);
	chr->STUN = mxml_GetInt(p, SAVE_CHARACTER_STUN, 0);
	chr->morale = mxml_GetInt(p, SAVE_CHARACTER_MORALE, 0);
	chr->fieldSize = mxml_GetInt(p, SAVE_CHARACTER_FIELDSIZE, 1);
	chr->state = mxml_GetInt(p, SAVE_CHARACTER_STATE, 0);

	/* Team-definition */
	s = mxml_GetString(p, SAVE_CHARACTER_TEAMDEF);

	/* @todo remove this after time. this is a fallback code to keep savegame compatibility (teamDefIDX ~> teamDefID). */
	if (s[0] == '\0') {
		const int td = mxml_GetInt(p, SAVE_CHARACTER_TEAMDEFIDX, -1);

		Com_Printf("No TeamDefID for %s (ucn: %i) found. Try teamDefIDX (oldsave method)\n", chr->name, chr->ucn);
		if (td != -1) {
			assert(csi.numTeamDefs);
			if (td >= csi.numTeamDefs) {
				Com_Printf("Invalid TeamDefIDX %i for %s (ucn: %i)\n", td, chr->name, chr->ucn);
				return qfalse;
			}
			chr->teamDef = &csi.teamDef[td];
		}
	} else {
		chr->teamDef = Com_GetTeamDefinitionByID(s);
	}
	if (!chr->teamDef) {
		Com_Printf("No valid TeamDef found for %s (ucn: %i)\n", chr->name, chr->ucn);
		return qfalse;
	}
	Com_RegisterConstList(saveCharacterConstants);

	sScore = mxml_GetNode(p, SAVE_CHARACTER_SCORES);
	/* Load Skills */
	for (sSkill = mxml_GetNode(sScore, SAVE_CHARACTER_SKILLS); sSkill; sSkill = mxml_GetNextNode(sSkill, sScore, SAVE_CHARACTER_SKILLS)) {
		int idx;
		const char *type = mxml_GetString(sSkill, SAVE_CHARACTER_SKILLTYPE);

		if (!Com_GetConstIntFromNamespace(SAVE_CHARACTER_SKILLTYPE_NAMESPACE, type, &idx)) {
			Com_Printf("Invalid skill type '%s' for %s (ucn: %i)\n", type, chr->name, chr->ucn);
			success = qfalse;
			break;
		}

		chr->score.initialSkills[idx] = mxml_GetInt(sSkill, SAVE_CHARACTER_INITSKILL, 0);
		chr->score.experience[idx] = mxml_GetInt(sSkill, SAVE_CHARACTER_EXPERIENCE, 0);
		if (idx < SKILL_NUM_TYPES) {
			chr->score.skills[idx] = chr->score.initialSkills[idx];
			chr->score.skills[idx] += mxml_GetInt(sSkill, SAVE_CHARACTER_SKILLIMPROVE, 0);
		}
	}

	if (!success) {
		Com_UnregisterConstList(saveCharacterConstants);
		return qfalse;
	}

	/* Load kills */
	for (sKill = mxml_GetNode(sScore, SAVE_CHARACTER_KILLS); sKill; sKill = mxml_GetNextNode(sKill, sScore, SAVE_CHARACTER_KILLS)) {
		int idx;
		const char *type = mxml_GetString(sKill, SAVE_CHARACTER_KILLTYPE);

		if (!Com_GetConstIntFromNamespace(SAVE_CHARACTER_KILLTYPE_NAMESPACE, type, &idx)) {
			Com_Printf("Invalid kill type '%s' for %s (ucn: %i)\n", type, chr->name, chr->ucn);
			success = qfalse;
			break;
		}
		chr->score.kills[idx] = mxml_GetInt(sKill, SAVE_CHARACTER_KILLED, 0);
		chr->score.stuns[idx] = mxml_GetInt(sKill, SAVE_CHARACTER_STUNNED, 0);
	}

	if (!success) {
		Com_UnregisterConstList(saveCharacterConstants);
		return qfalse;
	}

	chr->score.assignedMissions = mxml_GetInt(sScore, SAVE_CHARACTER_SCORE_ASSIGNEDMISSIONS, 0);
	chr->score.rank = mxml_GetInt(sScore, SAVE_CHARACTER_SCORE_RANK, -1);

	cls.i.DestroyInventory(&cls.i, &chr->i);
	sInventory = mxml_GetNode(p, SAVE_INVENTORY_INVENTORY);
	CL_LoadInventoryXML(sInventory, &chr->i);

	Com_UnregisterConstList(saveCharacterConstants);
	return qtrue;
}

/**
 * @brief Save one item
 * @param[out] p XML Node structure, where we write the information to
 * @param[in] item Pointer to the item to save
 * @param[in] container Index of the container the item is in
 * @param[in] x Horizontal coordinate of the item in the container
 * @param[in] y Vertical coordinate of the item in the container
 * @sa CL_LoadItemXML
 */
static void CL_SaveItemXML (mxml_node_t *p, item_t item, containerIndex_t container, int x, int y)
{
	assert(item.t);

	mxml_AddString(p, SAVE_INVENTORY_CONTAINER, csi.ids[container].name);
	mxml_AddInt(p, SAVE_INVENTORY_X, x);
	mxml_AddInt(p, SAVE_INVENTORY_Y, y);
	mxml_AddIntValue(p, SAVE_INVENTORY_ROTATED, item.rotated);
	mxml_AddString(p, SAVE_INVENTORY_WEAPONID, item.t->id);
	/** @todo: is there any case when amount != 1 for soldier inventory item? */
	mxml_AddInt(p, SAVE_INVENTORY_AMOUNT, item.amount);
	if (item.a > NONE_AMMO) {
		mxml_AddString(p, SAVE_INVENTORY_MUNITIONID, item.m->id);
		mxml_AddInt(p, SAVE_INVENTORY_AMMO, item.a);
	}
}

/**
 * @brief Save callback for savegames in XML Format
 * @param[out] p XML Node structure, where we write the information to
 * @param[in] i Pointerto the inventory to save
 * @sa CL_SaveItemXML
 * @sa CL_LoadInventoryXML
 */
void CL_SaveInventoryXML (mxml_node_t *p, const inventory_t *i)
{
	containerIndex_t container;

	for (container = 0; container < csi.numIDs; container++) {
		invList_t *ic = i->c[container];

		/* ignore items linked from any temp container */
		if (INVDEF(container)->temp)
			continue;

		for (; ic; ic = ic->next) {
			mxml_node_t *s = mxml_AddNode(p, SAVE_INVENTORY_ITEM);
			CL_SaveItemXML(s, ic->item, container, ic->x, ic->y);
		}
	}
}

/**
 * @brief Load one item
 * @param[in] p XML Node structure, where we write the information to
 * @param[out] item Pointer to the item
 * @param[out] container Index of the container the item is in
 * @param[out] x Horizontal coordinate of the item in the container
 * @param[out] y Vertical coordinate of the item in the container
 * @sa CL_SaveItemXML
 */
static void CL_LoadItemXML (mxml_node_t *n, item_t *item, containerIndex_t *container, int *x, int *y)
{
	const char *itemID = mxml_GetString(n, SAVE_INVENTORY_WEAPONID);
	const char *contID = mxml_GetString(n, SAVE_INVENTORY_CONTAINER);
	int i;

	/* reset */
	memset(item, 0, sizeof(*item));

	for (i = 0; i < csi.numIDs; i++) {
		if (!strcmp(csi.ids[i].name, contID))
			break;
	}
	if (i >= csi.numIDs) {
		Com_Printf("Invalid container id '%s'\n", contID);
	}
	*container = i;

	item->t = INVSH_GetItemByID(itemID);
	*x = mxml_GetInt(n, SAVE_INVENTORY_X, 0);
	*y = mxml_GetInt(n, SAVE_INVENTORY_Y, 0);
	item->rotated = mxml_GetInt(n, SAVE_INVENTORY_ROTATED, 0);
	item->amount = mxml_GetInt(n, SAVE_INVENTORY_AMOUNT, 1);
	item->a = mxml_GetInt(n, SAVE_INVENTORY_AMMO, NONE_AMMO);
	if (item->a > NONE_AMMO) {
		itemID = mxml_GetString(n, SAVE_INVENTORY_MUNITIONID);
		item->m = INVSH_GetItemByID(itemID);

		/* reset ammo count if ammunition (item) not found */
		if (!item->m)
			item->a = NONE_AMMO;
	}
}

/**
 * @brief Load callback for savegames in XML Format
 * @param[in] p XML Node structure, where we load the information from
 * @param[out] i Pointerto the inventory
 * @sa CL_SaveInventoryXML
 * @sa CL_LoadItemXML
 * @sa I_AddToInventory
  */
void CL_LoadInventoryXML (mxml_node_t *p, inventory_t *i)
{
	mxml_node_t *s;

	for (s = mxml_GetNode(p, SAVE_INVENTORY_ITEM); s; s = mxml_GetNextNode(s, p, SAVE_INVENTORY_ITEM)) {
		item_t item;
		containerIndex_t container;
		int x, y;

		CL_LoadItemXML(s, &item, &container, &x, &y);
		if (!cls.i.AddToInventory(&cls.i, i, item, INVDEF(container), x, y, 1))
			Com_Printf("Could not add item '%s' to inventory\n", item.t ? item.t->id : "NULL");
	}
}

/**
 * @brief Generates the skills and inventory for a character and for a 2x2 unit
 * @param[in] chr The employee to create character data for.
 * @param[in] teamDefName Which team to use for creation.
 * @todo fix the assignment of ucn??
 * @todo fix the WholeTeam stuff
 */
void CL_GenerateCharacter (character_t *chr, const char *teamDefName)
{
	memset(chr, 0, sizeof(*chr));

	/* link inventory */
	cls.i.DestroyInventory(&cls.i, &chr->i);

	/* get ucn */
	chr->ucn = cls.nextUniqueCharacterNumber++;

	CL_ActorSetShotSettings(chr, ACTOR_HAND_NOT_SET, -1, NULL);

	Com_GetCharacterValues(teamDefName, chr);
	/* Create attributes. */
	CHRSH_CharGenAbilitySkills(chr, GAME_IsMultiplayer());
}

/**
 * @brief Init skins into the GUI
 * @todo We should move it into a script, or into a C struct
 */
static void CL_InitSkin_f (void)
{
	/* create singleplayer skins */
	if (MN_GetOption(OPTION_SINGLEPLAYER_SKINS) == NULL) {
		menuOption_t *skins = NULL;
		assert(NUM_TEAMSKINS_SINGLEPLAYER >= 4);	/*< the current code create 4 skins */
		MN_AddOption(&skins, "urban", N_("Urban"), "0");
		MN_AddOption(&skins, "jungle", N_("Jungle"), "1");
		MN_AddOption(&skins, "desert", N_("Desert"), "2");
		MN_AddOption(&skins, "arctic", N_("Arctic"), "3");
		MN_RegisterOption(OPTION_SINGLEPLAYER_SKINS, skins);
	}

	/* create multiplayer skins */
	if (MN_GetOption(OPTION_MULTIPLAYER_SKINS) == NULL) {
		menuOption_t *skins = NULL;
		assert(NUM_TEAMSKINS >= 6);		/*< the current code create 6 skins */
		MN_AddOption(&skins, "urban", N_("Urban"), "0");
		MN_AddOption(&skins, "jungle", N_("Jungle"), "1");
		MN_AddOption(&skins, "desert", N_("Desert"), "2");
		MN_AddOption(&skins, "arctic", N_("Arctic"), "3");
		MN_AddOption(&skins, "multionly_yellow", N_("Yellow"), "4");
		MN_AddOption(&skins, "multionly_cccp", N_("CCCP"), "5");
		MN_RegisterOption(OPTION_MULTIPLAYER_SKINS, skins);
	}
}

/**
 * @brief Change the skin of the selected actor.
 */
static void CL_ChangeSkin_f (void)
{
	const int sel = cl_selected->integer;

	if (sel >= 0 && sel < chrDisplayList.num) {
		int newSkin = Cvar_GetInteger("mn_skin");
		if (newSkin >= NUM_TEAMSKINS || newSkin < 0)
			newSkin = 0;

		/* don't allow all skins in singleplayer */
		if (GAME_IsSingleplayer() && newSkin >= NUM_TEAMSKINS_SINGLEPLAYER)
			newSkin = 0;

		if (chrDisplayList.chr[sel]) {
			chrDisplayList.chr[sel]->skin = newSkin;

			Cvar_SetValue("mn_skin", newSkin);
			Cvar_Set("mn_skinname", CL_GetTeamSkinName(newSkin));
		}
	}
}

/**
 * @brief Use current skin for all team members onboard.
 */
static void CL_ChangeSkinForWholeTeam_f (void)
{
	int newSkin, i;

	/* Get selected skin and fall back to default skin if it is not valid. */
	newSkin = Cvar_GetInteger("mn_skin");
	if (newSkin >= NUM_TEAMSKINS || newSkin < 0)
		newSkin = 0;

	/* don't allow all skins in singleplayer */
	if (GAME_IsSingleplayer() && newSkin >= NUM_TEAMSKINS_SINGLEPLAYER)
		newSkin = 0;

	/* Apply new skin to all (shown/displayed) team-members. */
	/** @todo What happens if a model of a team member does not have the selected skin? */
	for (i = 0; i < chrDisplayList.num; i++) {
		assert(chrDisplayList.chr[i]);
		chrDisplayList.chr[i]->skin = newSkin;
	}
}

/**
 * @brief Update the GUI with the selected item
 * @todo Doesn't belong into cl_team.c
 * @todo function is used for multiplayer, too - should be splitted
 * @todo function does not belong into the team code
 */
static void CL_UpdateObject_f (void)
{
	int num;
	const objDef_t *obj;
	qboolean changeTab;

	/* check syntax */
	if (Cmd_Argc() < 2) {
		Com_Printf("Usage: %s <objectid> <mustwechangetab>\n", Cmd_Argv(0));
		return;
	}

	if (Cmd_Argc() == 3)
		changeTab = atoi(Cmd_Argv(2)) >= 1;
	else
		changeTab = qtrue;

	num = atoi(Cmd_Argv(1));
	if (num < 0 || num >= csi.numODs) {
		Com_Printf("Id %i out of range 0..%i\n", num, csi.numODs);
		return;
	}
	obj = &csi.ods[num];

	/* update item description */
	INV_ItemDescription(obj);

	/* update tab */
	if (changeTab) {
		const cvar_t *var = Cvar_FindVar("mn_equiptype");
		const int filter = INV_GetFilterFromItem(obj);
		if (var && var->integer != filter) {
			Cvar_SetValue("mn_equiptype", filter);
			MN_ExecuteConfunc("update_item_list");
		}
	}
}

void TEAM_InitStartup (void)
{
	Cmd_AddCommand("team_initskin", CL_InitSkin_f, "Init skin according to the game mode");
	Cmd_AddCommand("team_changeskin", CL_ChangeSkin_f, "Change the skin of the soldier");
	Cmd_AddCommand("team_changeskinteam", CL_ChangeSkinForWholeTeam_f, "Change the skin for the whole current team");
	Cmd_AddCommand("object_update", CL_UpdateObject_f, _("Update the GUI with the selected item"));
}
