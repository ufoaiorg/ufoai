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
#include "campaign/save/save_character.h"
#include "campaign/save/save_inventory.h"

/** @brief List of currently displayed or equipeable characters. */
chrList_t chrDisplayList;

/**
 * @brief Translate the skin id to skin name
 * @param[in] id The id of the skin
 * @return Translated skin name
 */
const char* CL_GetTeamSkinName (int id)
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

	assert(chr);
	Com_RegisterConstList(saveCharacterConstants);
	/* Store the character data */
	mxml_AddString(p, SAVE_CHARACTER_NAME, chr->name);
	mxml_AddString(p, SAVE_CHARACTER_BODY, chr->body);
	mxml_AddString(p, SAVE_CHARACTER_PATH, chr->path);
	mxml_AddString(p, SAVE_CHARACTER_HEAD, chr->head);
	mxml_AddInt(p, SAVE_CHARACTER_SKIN, chr->skin);
	mxml_AddInt(p, SAVE_CHARACTER_TEAMDEFIDX, chr->teamDef->idx);
	mxml_AddInt(p, SAVE_CHARACTER_GENDER, chr->gender);
	mxml_AddInt(p, SAVE_CHARACTER_UCN, chr->ucn);
	mxml_AddInt(p, SAVE_CHARACTER_MAXHP, chr->maxHP);
	mxml_AddInt(p, SAVE_CHARACTER_HP, chr->HP);
	mxml_AddInt(p, SAVE_CHARACTER_STUN, chr->STUN);
	mxml_AddInt(p, SAVE_CHARACTER_MORALE, chr->morale);
	mxml_AddInt(p, SAVE_CHARACTER_FIELDSIZE, chr->fieldSize);
	mxml_AddIntValue(p, SAVE_CHARACTER_STATE, chr->state);

	sScore = mxml_AddNode(p, SAVE_CHARACTER_SCORES);
	/* Store skills */
	for (k = 0; k < SKILL_NUM_TYPES + 1; k++) {
		mxml_node_t *sSkill;
		
		if ((k < SKILL_NUM_TYPES && chr->score.skills[k])
		 || chr->score.experience[k] || chr->score.initialSkills[k]) {
			sSkill = mxml_AddNode(sScore, SAVE_CHARACTER_SKILLS);;

			mxml_AddString(sSkill, SAVE_CHARACTER_SKILLTYPE, Com_GetConstVariable(SAVE_CHARACTER_SKILLTYPE_NAMESPACE, k));
			mxml_AddIntValue(sSkill, SAVE_CHARACTER_INITSKILL, chr->score.initialSkills[k]);
			mxml_AddIntValue(sSkill, SAVE_CHARACTER_EXPERIENCE, chr->score.experience[k]);
			if (k < SKILL_NUM_TYPES)
				mxml_AddIntValue(sSkill, SAVE_CHARACTER_SKILLIMPROVE, chr->score.skills[k] - chr->score.initialSkills[k]);
		}
	}
	/* Store kills */
	for (k = 0; k < KILLED_NUM_TYPES; k++) {
		mxml_node_t *sKill;
		if (chr->score.kills[k] || chr->score.stuns[k]) {
			sKill = mxml_AddNode(sScore, SAVE_CHARACTER_KILLS);
			mxml_AddString(sKill, SAVE_CHARACTER_KILLTYPE, Com_GetConstVariable(SAVE_CHARACTER_KILLTYPE_NAMESPACE, k));
			mxml_AddIntValue(sKill, SAVE_CHARACTER_KILLED, chr->score.kills[k]);
			mxml_AddIntValue(sKill, SAVE_CHARACTER_STUNNED, chr->score.stuns[k]);
		}
	}
	mxml_AddIntValue(sScore, SAVE_CHARACTER_SCORE_ASSIGNEDMISSIONS, chr->score.assignedMissions);
	mxml_AddInt(sScore, SAVE_CHARACTER_SCORE_RANK, chr->score.rank);

	/* Store inventories */
	sInventory = mxml_AddNode(p, SAVE_INVENTORY_INVENTORY);
	CL_SaveInventoryXML(sInventory, &chr->inv);

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
	int td;
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

	chr->teamDef = NULL;
	/* Team-definition index */
	td = mxml_GetInt(p, SAVE_CHARACTER_TEAMDEFIDX, -1);

	if (td != -1) {
		assert(csi.numTeamDefs);
		if (td >= csi.numTeamDefs) {
			Com_Printf("Invalid TeamDefIDX %i for %s (ucn: %i)\n", td, chr->name, chr->ucn);
			return qfalse;
		}
		chr->teamDef = &csi.teamDef[td];
	}

	Com_RegisterConstList(saveCharacterConstants);

	sScore = mxml_GetNode(p, SAVE_CHARACTER_SCORES);
	/* Load Skills */
	for (sSkill = mxml_GetNode(sScore, SAVE_CHARACTER_SKILLS); sSkill; sSkill = mxml_GetNextNode(sSkill, sScore, SAVE_CHARACTER_SKILLS)) {
		int idx;
		const char *type = mxml_GetString(sSkill, SAVE_CHARACTER_SKILLTYPE);

		if (!Com_GetConstInt(type, &idx)) {
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

		if (!Com_GetConstInt(type, &idx)) {
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

	cls.i.DestroyInventory(&cls.i, &chr->inv);
	sInventory = mxml_GetNode(p, SAVE_INVENTORY_INVENTORY);
	CL_LoadInventoryXML(sInventory, &chr->inv);

	Com_UnregisterConstList(saveCharacterConstants);
	return qtrue;
}

/**
 * @sa CL_LoadItemXML
 */
static void CL_SaveItemXML (mxml_node_t *p, item_t item, int container, int x, int y)
{
	assert(item.t);

	mxml_AddInt(p, SAVE_INVENTORY_CONTAINER, container);
	mxml_AddInt(p, SAVE_INVENTORY_X, x);
	mxml_AddInt(p, SAVE_INVENTORY_Y, y);
	mxml_AddInt(p, SAVE_INVENTORY_ROTATED, item.rotated);
	mxml_AddInt(p, SAVE_INVENTORY_AMOUNT, item.amount);
	mxml_AddString(p, SAVE_INVENTORY_WEAPONID, item.t->id);
	if (item.a > NONE_AMMO) {
		mxml_AddString(p, SAVE_INVENTORY_MUNITIONID, item.m->id);
		mxml_AddInt(p, SAVE_INVENTORY_AMMO, item.a);
	}
}

/**
 * @sa CL_SaveItemXML
 * @sa CL_LoadInventoryXML
 */
void CL_SaveInventoryXML (mxml_node_t *p, const inventory_t *i)
{
	int container;

	for (container = 0; container < csi.numIDs; container++) {
		invList_t *ic = i->c[container];

#if 0
		/* ignore items linked from any temp container */
		if (csi.ids[container].temp)
			continue;
#endif

		for (; ic; ic = ic->next) {
			mxml_node_t *s = mxml_AddNode(p, SAVE_INVENTORY_ITEM);
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
	item->a = mxml_GetInt(n, SAVE_INVENTORY_AMMO, NONE_AMMO);
	item->rotated = mxml_GetInt(n, SAVE_INVENTORY_ROTATED, 0);
	item->amount = mxml_GetInt(n, SAVE_INVENTORY_AMOUNT, 1);
	*x = mxml_GetInt(n, SAVE_INVENTORY_X, 0);
	*y = mxml_GetInt(n, SAVE_INVENTORY_Y, 0);
	*container = mxml_GetInt(n, SAVE_INVENTORY_CONTAINER, 0);
	itemID = mxml_GetString(n, SAVE_INVENTORY_WEAPONID);
	item->t = INVSH_GetItemByID(itemID);

	if (item->a > NONE_AMMO) {
		itemID = mxml_GetString(n, SAVE_INVENTORY_MUNITIONID);
		item->m = INVSH_GetItemByID(itemID);

		/* reset ammo count if ammunition (item) not found */
		if (!item->m)
			item->a = NONE_AMMO;
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

	for (s = mxml_GetNode(p, SAVE_INVENTORY_ITEM); s; s = mxml_GetNextNode(s, p, SAVE_INVENTORY_ITEM)) {
		item_t item;
		int container, x, y;
		CL_LoadItemXML(s, &item, &container, &x, &y);
		if (!cls.i.AddToInventory(&cls.i, i, item, &csi.ids[container], x, y, 1))
			Com_Printf("Could not add item '%s' to inventory\n", item.t ? item.t->id : "NULL");
	}
}

/**
 * @brief Generates the skills and inventory for a character and for a 2x2 unit
 * @param[in] chr The employee to create character data for.
 * @param[in] teamDefName Which team to use for creation.
 * @param[in] ugvType Currently unused.
 * @todo fix the assignment of ucn??
 * @todo fix the WholeTeam stuff
 */
void CL_GenerateCharacter (character_t *chr, const char *teamDefName, const ugv_t *ugvType)
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
