/**
 * @file cl_team.c
 * @brief Team management, name generation and parsing.
 */

/*
Copyright (C) 2002-2009 UFO: Alien Invasion.

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
	/* Store the character data */
	mxml_node_t *s;
	int k;
	const int count = max(SKILL_NUM_TYPES + 1, KILLED_NUM_TYPES);

	mxml_AddString(p, "name", chr->name);
	mxml_AddString(p, "body", chr->body);
	mxml_AddString(p, "path", chr->path);
	mxml_AddString(p, "head", chr->head);
	mxml_AddInt(p, "skin", chr->skin);
	mxml_AddInt(p, "teamdefidx", chr->teamDef->idx);
	mxml_AddInt(p, "gender", chr->gender);
	mxml_AddInt(p, "state", chr->state);
	mxml_AddInt(p, "ucn", chr->ucn);
	mxml_AddInt(p, "maxhp", chr->maxHP);
	mxml_AddInt(p, "hp", chr->HP);
	mxml_AddInt(p, "stun", chr->STUN);
	mxml_AddInt(p, "morale", chr->morale);
	mxml_AddInt(p, "fieldsize", chr->fieldSize);

	/* Store character stats/score */
	for (k = 0; k < count; k++) {
		s = mxml_AddNode(p, "score");
		if (k < SKILL_NUM_TYPES)
			mxml_AddInt(s, "skill", chr->score.skills[k]);
		if (k < SKILL_NUM_TYPES + 1) {
			mxml_AddInt(s, "experience", chr->score.experience[k]);
			/** @todo There is a Bug in storing the initial skills. Fix it in loading */
			mxml_AddInt(s, "initskill", chr->score.initialSkills[k]);
		}
		if (k < KILLED_NUM_TYPES) {
			mxml_AddInt(s, "kills", chr->score.kills[k]);
			mxml_AddInt(s, "stuns", chr->score.stuns[k]);
		}
	}

	mxml_AddInt(p, "score.assignedmissions", chr->score.assignedMissions);
	mxml_AddInt(p, "score.rank", chr->score.rank);

	/* Store inventories */
	s = mxml_AddNode(p, "inventory");
	CL_SaveInventoryXML(s, &chr->inv);

	return qtrue;
}

/**
 * @brief Loads a character from a given xml node.
 * @param[in] p The node from which we should load the character.
 * @param[in] chr Pointer to the charcter we should load.
 */
qboolean CL_LoadCharacterXML (mxml_node_t *p, character_t *chr)
{
	mxml_node_t *s;
	int td, k, count;

	Q_strncpyz(chr->name, mxml_GetString(p, "name"), sizeof(chr->name));
	Q_strncpyz(chr->body, mxml_GetString(p, "body"), sizeof(chr->body));
	Q_strncpyz(chr->path, mxml_GetString(p, "path"), sizeof(chr->path));
	Q_strncpyz(chr->head, mxml_GetString(p, "head"), sizeof(chr->head));

	chr->skin = mxml_GetInt(p, "skin", 0);

	chr->teamDef = NULL;
	/* Team-definition index */
	td = mxml_GetInt(p, "teamdefidx", -1);
	if (td != -1) {
		assert(csi.numTeamDefs);
		if (td >= csi.numTeamDefs)
			return qfalse;
		chr->teamDef = &csi.teamDef[td];
	}

	/* Load the character data */
	chr->gender = mxml_GetInt(p, "gender", 0);
	chr->state = mxml_GetInt(p, "state", 0);
	chr->ucn = mxml_GetInt(p, "ucn", 0);
	chr->maxHP = mxml_GetInt(p, "maxhp", 0);
	chr->HP = mxml_GetInt(p, "hp", 0);
	chr->STUN = mxml_GetInt(p, "stun", 0);
	chr->morale = mxml_GetInt(p, "morale", 0);
	chr->fieldSize = mxml_GetInt(p, "fieldsize", 1);

	/** Load character stats/score
	 * @sa chrScoreGlobal_t */
	count = max(SKILL_NUM_TYPES + 1, KILLED_NUM_TYPES);
	for (k = 0, s = mxml_GetNode(p, "score"); s && k < count; k++, s = mxml_GetNextNode(s, p, "score")) {
		if (k < SKILL_NUM_TYPES)
			chr->score.skills[k] = mxml_GetInt(s, "skill", 0);
		if (k < SKILL_NUM_TYPES + 1) {
			chr->score.experience[k] = mxml_GetInt(s, "experience", 0);
			/** @todo There was a Bug in storing the initial skills. Zero Values will be set to max(hp,max_hp)*/
			chr->score.initialSkills[k] = mxml_GetInt(s, "initskill", 0);
			if (k == SKILL_NUM_TYPES && chr->score.initialSkills[k] == 0) {
				Com_DPrintf(DEBUG_CLIENT, "Skill was zero!");
				chr->score.initialSkills[k] = max(chr->HP, chr->maxHP);
			}
		}
		if (k < KILLED_NUM_TYPES) {
			chr->score.kills[k] = mxml_GetInt(s, "kills", 0);
			chr->score.stuns[k] = mxml_GetInt(s, "stuns", 0);
		}
	}
	chr->score.assignedMissions = mxml_GetInt(p, "score.assignedmissions", 0);
	chr->score.rank = mxml_GetInt(p, "score.rank", -1);

	/*memset(&chr->inv, 0, sizeof(inventory_t));*/
	cls.i.DestroyInventory(&cls.i, &chr->inv);
	s = mxml_GetNode(p, "inventory");
	CL_LoadInventoryXML(s, &chr->inv);

	return qtrue;
}

/**
 * @sa CL_LoadItemXML
 */
static void CL_SaveItemXML (mxml_node_t *p, item_t item, containerIndex_t container, int x, int y)
{
	assert(item.t);
	mxml_AddInt(p, "ammo", item.a);
	mxml_AddInt(p, "container", container);
	mxml_AddInt(p, "x", x);
	mxml_AddInt(p, "y", y);
	mxml_AddInt(p, "rotated", item.rotated);
	mxml_AddInt(p, "amount", item.amount);
	mxml_AddString(p, "weaponid", item.t->id);
	if (item.a > NONE_AMMO)
		mxml_AddString(p, "munitionid", item.m->id);
}

/**
 * @sa CL_SaveItemXML
 * @sa CL_LoadInventoryXML
 */
void CL_SaveInventoryXML (mxml_node_t *p, const inventory_t *i)
{
	containerIndex_t container;

	for (container = 0; container < csi.numIDs; container++) {
		invList_t *ic = i->c[container];

#if 0
		/* ignore items linked from any temp container */
		if (csi.ids[container].temp)
			continue;
#endif

		for (; ic; ic = ic->next) {
			mxml_node_t *s = mxml_AddNode(p, "item");
			CL_SaveItemXML(s, ic->item, container, ic->x, ic->y);
		}
	}
}

/**
 * @sa CL_SaveItemXML
 */
static void CL_LoadItemXML (mxml_node_t *n, item_t *item, int *container, int *x, int *y)
{
	const char *itemID;

	/* reset */
	memset(item, 0, sizeof(*item));
	item->a = mxml_GetInt(n, "ammo", NONE_AMMO);
	item->rotated = mxml_GetInt(n, "rotated", 0);
	item->amount = mxml_GetInt(n, "amount", 1);
	*x = mxml_GetInt(n, "x", 0);
	*y = mxml_GetInt(n, "y", 0);
	*container = mxml_GetInt(n, "container", 0);
	itemID = mxml_GetString(n, "weaponid");
	item->t = INVSH_GetItemByID(itemID);
	if (item->a > NONE_AMMO) {
		itemID = mxml_GetString(n, "munitionid");
		item->m = INVSH_GetItemByID(itemID);
	}
}

/**
 * @sa CL_SaveInventoryXML
 * @sa CL_LoadItemXML
 * @sa I_AddToInventory
  */
void CL_LoadInventoryXML (mxml_node_t *p, inventory_t *i)
{
	mxml_node_t *s;

	for (s = mxml_GetNode(p, "item"); s; s = mxml_GetNextNode(s, p, "item")) {
		item_t item;
		containerIndex_t container;
		int x, y;
		CL_LoadItemXML(s, &item, &container, &x, &y);
		if (!cls.i.AddToInventory(&cls.i, i, item, &csi.ids[container], x, y, 1))
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
	cls.i.DestroyInventory(&cls.i, &chr->inv);

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
