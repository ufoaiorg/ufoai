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
#include "battlescape/events/e_parse.h"
#include "ui/ui_data.h"
#include "ui/ui_nodes.h"
#include "battlescape/events/e_main.h"
#include "campaign/save/save_character.h"
#include "campaign/save/save_inventory.h"

/** @brief List of currently displayed or equipeable characters. */
chrList_t chrDisplayList;

/**
 * @brief Allocate a skin from the cls structure
 * @return A actorskin structure, only idx is initialized
 */
actorSkin_t* Com_AllocateActorSkin (const char *name)
{
	int index;
	actorSkin_t *skin;

	if (cls.numActorSkins >= lengthof(cls.actorSkins))
		Sys_Error("Com_AllocateActorSkin: Max actorskin hit");

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
unsigned int Com_GetActorSkinCount (void)
{
	return cls.numActorSkins;
}

/**
 * @brief Get a actorskin from idx
 * @return A actorskin, else NULL
 */
static const actorSkin_t* Com_GetActorSkinByIDS (unsigned int idx)
{
	if (idx >= cls.numActorSkins)
		return NULL;
	return &cls.actorSkins[idx];
}

/**
 * @brief Translate the skin id to skin name
 * @param[in] id The id of the skin
 * @return Translated skin name
 */
static const char* CL_GetTeamSkinName (unsigned int id)
{
	const actorSkin_t *skin = Com_GetActorSkinByIDS(id);
	if (skin == NULL)
		Com_Error(ERR_DROP, "CL_GetTeamSkinName: Unknown skin id %i", id);
	return skin->name;
}

static void CL_CharacterSkillAndScoreCvars (const character_t *chr, const char* cvarPrefix)
{
	Cvar_ForceSet(va("%s%s", cvarPrefix, "name"), chr->name);
	Cvar_ForceSet(va("%s%s", cvarPrefix, "body"), CHRSH_CharGetBody(chr));
	Cvar_ForceSet(va("%s%s", cvarPrefix, "head"), CHRSH_CharGetHead(chr));
	Cvar_ForceSet(va("%s%s", cvarPrefix, "skin"), va("%i", chr->skin));
	Cvar_ForceSet(va("%s%s", cvarPrefix, "skinname"), CL_GetTeamSkinName(chr->skin));

	Cvar_Set(va("%s%s", cvarPrefix, "vpwr"), va("%i", chr->score.skills[ABILITY_POWER]));
	Cvar_Set(va("%s%s", cvarPrefix, "vspd"), va("%i", chr->score.skills[ABILITY_SPEED]));
	Cvar_Set(va("%s%s", cvarPrefix, "vacc"), va("%i", chr->score.skills[ABILITY_ACCURACY]));
	Cvar_Set(va("%s%s", cvarPrefix, "vmnd"), va("%i", chr->score.skills[ABILITY_MIND]));
	Cvar_Set(va("%s%s", cvarPrefix, "vcls"), va("%i", chr->score.skills[SKILL_CLOSE]));
	Cvar_Set(va("%s%s", cvarPrefix, "vhvy"), va("%i", chr->score.skills[SKILL_HEAVY]));
	Cvar_Set(va("%s%s", cvarPrefix, "vass"), va("%i", chr->score.skills[SKILL_ASSAULT]));
	Cvar_Set(va("%s%s", cvarPrefix, "vsnp"), va("%i", chr->score.skills[SKILL_SNIPER]));
	Cvar_Set(va("%s%s", cvarPrefix, "vexp"), va("%i", chr->score.skills[SKILL_EXPLOSIVE]));
	Cvar_Set(va("%s%s", cvarPrefix, "vpwri"), va("%i", chr->score.initialSkills[ABILITY_POWER]));
	Cvar_Set(va("%s%s", cvarPrefix, "vspdi"), va("%i", chr->score.initialSkills[ABILITY_SPEED]));
	Cvar_Set(va("%s%s", cvarPrefix, "vacci"), va("%i", chr->score.initialSkills[ABILITY_ACCURACY]));
	Cvar_Set(va("%s%s", cvarPrefix, "vmndi"), va("%i", chr->score.initialSkills[ABILITY_MIND]));
	Cvar_Set(va("%s%s", cvarPrefix, "vclsi"), va("%i", chr->score.initialSkills[SKILL_CLOSE]));
	Cvar_Set(va("%s%s", cvarPrefix, "vhvyi"), va("%i", chr->score.initialSkills[SKILL_HEAVY]));
	Cvar_Set(va("%s%s", cvarPrefix, "vassi"), va("%i", chr->score.initialSkills[SKILL_ASSAULT]));
	Cvar_Set(va("%s%s", cvarPrefix, "vsnpi"), va("%i", chr->score.initialSkills[SKILL_SNIPER]));
	Cvar_Set(va("%s%s", cvarPrefix, "vexpi"), va("%i", chr->score.initialSkills[SKILL_EXPLOSIVE]));
	Cvar_Set(va("%s%s", cvarPrefix, "vhp"), va("%i", chr->HP));
	Cvar_Set(va("%s%s", cvarPrefix, "vhpmax"), va("%i", chr->maxHP));

	Cvar_Set(va("%s%s", cvarPrefix, "tpwr"), va("%s (%i)", CL_ActorGetSkillString(chr->score.skills[ABILITY_POWER]), chr->score.skills[ABILITY_POWER]));
	Cvar_Set(va("%s%s", cvarPrefix, "tspd"), va("%s (%i)", CL_ActorGetSkillString(chr->score.skills[ABILITY_SPEED]), chr->score.skills[ABILITY_SPEED]));
	Cvar_Set(va("%s%s", cvarPrefix, "tacc"), va("%s (%i)", CL_ActorGetSkillString(chr->score.skills[ABILITY_ACCURACY]), chr->score.skills[ABILITY_ACCURACY]));
	Cvar_Set(va("%s%s", cvarPrefix, "tmnd"), va("%s (%i)", CL_ActorGetSkillString(chr->score.skills[ABILITY_MIND]), chr->score.skills[ABILITY_MIND]));
	Cvar_Set(va("%s%s", cvarPrefix, "tcls"), va("%s (%i)", CL_ActorGetSkillString(chr->score.skills[SKILL_CLOSE]), chr->score.skills[SKILL_CLOSE]));
	Cvar_Set(va("%s%s", cvarPrefix, "thvy"), va("%s (%i)", CL_ActorGetSkillString(chr->score.skills[SKILL_HEAVY]), chr->score.skills[SKILL_HEAVY]));
	Cvar_Set(va("%s%s", cvarPrefix, "tass"), va("%s (%i)", CL_ActorGetSkillString(chr->score.skills[SKILL_ASSAULT]), chr->score.skills[SKILL_ASSAULT]));
	Cvar_Set(va("%s%s", cvarPrefix, "tsnp"), va("%s (%i)", CL_ActorGetSkillString(chr->score.skills[SKILL_SNIPER]), chr->score.skills[SKILL_SNIPER]));
	Cvar_Set(va("%s%s", cvarPrefix, "texp"), va("%s (%i)", CL_ActorGetSkillString(chr->score.skills[SKILL_EXPLOSIVE]), chr->score.skills[SKILL_EXPLOSIVE]));
	Cvar_Set(va("%s%s", cvarPrefix, "thp"), va("%i (%i)", chr->HP, chr->maxHP));
}

/**
 * @brief Updates the character cvars for the given character.
 *
 * The models and stats that are displayed in the menu are stored in cvars.
 * These cvars are updated here when you select another character.
 *
 * @param[in] chr Pointer to character_t (may not be null)
 * @param[in] cvarPrefix
 * @sa CL_UGVCvars
 * @sa CL_ActorSelect
 */
static void CL_ActorCvars (const character_t * chr, const char* cvarPrefix)
{
	invList_t *weapon;
	assert(chr);

	/* visible equipment */
	weapon = RIGHT(chr);
	if (weapon)
		Cvar_Set(va("%s%s", cvarPrefix, "rweapon"), weapon->item.t->model);
	else
		Cvar_Set(va("%s%s", cvarPrefix, "rweapon"), "");
	weapon = LEFT(chr);
	if (weapon)
		Cvar_Set(va("%s%s", cvarPrefix, "lweapon"), weapon->item.t->model);
	else
		Cvar_Set(va("%s%s", cvarPrefix, "lweapon"), "");
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

void CL_UpdateCharacterValues (const character_t *chr, const char *cvarPrefix)
{
	CL_CharacterSkillAndScoreCvars(chr, cvarPrefix);

	if (chr->teamDef->race == RACE_ROBOT)
		CL_UGVCvars(chr);
	else
		CL_ActorCvars(chr, cvarPrefix);

	GAME_CharacterCvars(chr);
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
	chr->teamDef = Com_GetTeamDefinitionByID(s);
	if (!chr->teamDef)
		return qfalse;

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
 * @param[in] n XML Node structure, where we write the information to
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
	OBJZERO(*item);

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
 */
void CL_GenerateCharacter (character_t *chr, const char *teamDefName)
{
	OBJZERO(*chr);

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
 */
static void CL_InitSkin_f (void)
{
	/* create option for singleplayer skins */
	if (UI_GetOption(OPTION_SINGLEPLAYER_SKINS) == NULL) {
		uiNode_t *skins = NULL;
		int idx = 0;
		const actorSkin_t *skin;
		while ((skin = Com_GetActorSkinByIDS(idx++))) {
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
		while ((skin = Com_GetActorSkinByIDS(idx++))) {
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
	const actorSkin_t *skin = Com_GetActorSkinByIDS(idx);

	/** @todo we should check somewhere there is at least 1 skin */
	if (skin == NULL) {
		idx = 0;
	} else {
		if (GAME_IsSingleplayer() && !skin->singleplayer)
			idx = 0;
		if (GAME_IsMultiplayer() && !skin->multiplayer)
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

	if (sel >= 0 && sel < chrDisplayList.num) {
		int newSkin = Cvar_GetInteger("mn_skin");
		newSkin = CL_FixActorSkinIDX(newSkin);

		if (chrDisplayList.chr[sel]) {
			/** @todo Get the skin id from the model by using the actorskin id */
			/** @todo Or remove skins from models and convert character_t->skin to string */
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
	newSkin = CL_FixActorSkinIDX(newSkin);

	/* Apply new skin to all (shown/displayed) team-members. */
	/** @todo What happens if a model of a team member does not have the selected skin? */
	for (i = 0; i < chrDisplayList.num; i++) {
		assert(chrDisplayList.chr[i]);
		/** @todo Get the skin id from the model by using the actorskin id */
		/** @todo Or remove skins from models and convert character_t->skin to string */
		chrDisplayList.chr[i]->skin = newSkin;
	}
}

void TEAM_InitStartup (void)
{
	Cmd_AddCommand("team_initskin", CL_InitSkin_f, "Init skin according to the game mode");
	Cmd_AddCommand("team_changeskin", CL_ChangeSkin_f, "Change the skin of the soldier");
	Cmd_AddCommand("team_changeskinteam", CL_ChangeSkinForWholeTeam_f, "Change the skin for the whole current team");
}
