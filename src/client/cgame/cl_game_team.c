/**
 * @file cl_game_team.c
 * @brief cgame team management.
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

See the GNU General Public License for more details.m

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

*/

#include "../client.h"
#include "cl_game.h"
#include "cl_game_team.h"
#include "../cl_inventory.h"
#include "../cl_team.h"
#include "../ui/ui_main.h"
#include "../ui/ui_popup.h"
#include "../ui/node/ui_node_container.h"	/**< ui_inventory */
#include "save_team.h"
#include "save_character.h"
#include "save_inventory.h"

#define TEAM_SAVE_FILE_VERSION 4

static inventory_t game_inventory;
static qboolean characterActive[MAX_ACTIVETEAM];

typedef struct teamSaveFileHeader_s {
	uint32_t version; /**< which savegame version */
	uint32_t soldiercount; /** the number of soldiers stored in this savegame */
	char name[32]; /**< savefile name */
	uint32_t xmlSize; /** needed, if we store compressed */
} teamSaveFileHeader_t;

static void GAME_UpdateActiveTeamList (void)
{
	int i;

	OBJZERO(characterActive);
	for (i = 0; i < chrDisplayList.num; i++)
		characterActive[i] = qtrue;

	UI_ExecuteConfunc("team_checkboxes_update %i", chrDisplayList.num);
}

void GAME_AutoTeam (const char *equipmentDefinitionID, int teamMembers)
{
	const equipDef_t *ed = INV_GetEquipmentDefinitionByID(equipmentDefinitionID);
	const char *teamDefID = GAME_GetTeamDef();
	cls.teamSaveSlotIndex = NO_TEAM_SLOT_LOADED;

	GAME_GenerateTeam(teamDefID, ed, teamMembers);
}

void GAME_AutoTeam_f (void)
{
	GAME_AutoTeam("multiplayer_initial", GAME_GetCharacterArraySize());

	GAME_UpdateActiveTeamList();
}

/**
 * @brief This will activate/deactivate the actor for the team
 * @sa GAME_SaveTeamState_f
 */
void GAME_ToggleActorForTeam_f (void)
{
	int num;
	int value;

	if (Cmd_Argc() != 3) {
		Com_Printf("Usage: %s <num> <value>\n", Cmd_Argv(0));
		return;
	}

	num = atoi(Cmd_Argv(1));
	value = atoi(Cmd_Argv(2));
	if (num < 0 || num >= chrDisplayList.num)
		return;

	characterActive[num] = (value != 0);
}

/**
 * @brief Will remove those actors that should not be used in the team
 * @sa GAME_ToggleActorForTeam_f
 */
void GAME_SaveTeamState_f (void)
{
	int i, num;

	num = 0;
	for (i = 0; i < chrDisplayList.num; i++) {
		chrDisplayList.chr[num]->ucn -= (i - num);
		if (characterActive[i]) {
			assert(chrDisplayList.chr[i] != NULL);
			chrDisplayList.chr[num++] = chrDisplayList.chr[i];
		}
	}
	chrDisplayList.num = num;
}

/**
 * @brief Reads the comments from team files
 */
void GAME_TeamSlotComments_f (void)
{
	int i;
	const int maxTeamSlots = 12;

	for (i = 0; i < maxTeamSlots; i++) {
		/* open file */
		qFILE f;

		FS_OpenFile(va("save/team%i.mpt", i), &f, FILE_READ);
		if (f.f || f.z) {
			teamSaveFileHeader_t header;
			const int clen = sizeof(header);
			if (FS_Read(&header, clen, &f) != clen) {
				Com_Printf("Warning: Could not read %i bytes from savefile\n", clen);
				FS_CloseFile(&f);
				Cvar_Set(va("mn_slot%i", i), "");
				continue;
			}
			FS_CloseFile(&f);
			if (LittleLong(header.version) == TEAM_SAVE_FILE_VERSION) {
				UI_ExecuteConfunc("set_slotname %i %i \"%s\"", i, LittleLong(header.soldiercount), header.name);
			} else {
				Cvar_Set(va("mn_slot%i", i), "");
			}
		} else {
			Cvar_Set(va("mn_slot%i", i), "");
		}
	}
}

/**
 * @brief Stores the wholeTeam into a xml structure
 * @note Called by GAME_SaveTeam to store the team info
 * @sa GAME_SendCurrentTeamSpawningInfo
 */
static void GAME_SaveTeamInfo (xmlNode_t *p)
{
	int i;

	/* header */
	Com_DPrintf(DEBUG_CLIENT, "Saving %i teammembers\n", chrDisplayList.num);
	for (i = 0; i < chrDisplayList.num; i++) {
		const character_t *chr = chrDisplayList.chr[i];
		xmlNode_t *n = XML_AddNode(p, SAVE_TEAM_CHARACTER);
		GAME_SaveCharacter(n, chr);
	}
}

/**
 * @brief Loads the wholeTeam from the xml file
 * @note Called by GAME_LoadTeam to load the team info
 * @sa GAME_SendCurrentTeamSpawningInfo
 */
static void GAME_LoadTeamInfo (xmlNode_t *p)
{
	int i;
	xmlNode_t *n;
	const size_t size = GAME_GetCharacterArraySize();

	GAME_ResetCharacters();
	OBJZERO(characterActive);

	/* header */
	for (i = 0, n = XML_GetNode(p, SAVE_TEAM_CHARACTER); n && i < size; i++, n = XML_GetNextNode(n, p, SAVE_TEAM_CHARACTER)) {
		character_t *chr = GAME_GetCharacter(i);
		GAME_LoadCharacter(n, chr);
		assert(i < lengthof(chrDisplayList.chr));
		chrDisplayList.chr[i] = chr;
	}
	chrDisplayList.num = i;

	GAME_UpdateActiveTeamList();

	Com_DPrintf(DEBUG_CLIENT, "Loaded %i teammembers\n", chrDisplayList.num);
}

/**
 * @brief Saves a team in xml format
 */
static qboolean GAME_SaveTeam (const char *filename, const char *name)
{
	int requiredBufferLength;
	byte *buf, *fbuf;
	teamSaveFileHeader_t header;
	char dummy[2];
	int i;
	xmlNode_t *topNode, *node, *snode;
	equipDef_t *ed = GAME_GetEquipmentDefinition();

	topNode = mxmlNewXML("1.0");
	node = XML_AddNode(topNode, SAVE_TEAM_ROOTNODE);
	OBJZERO(header);
	header.version = LittleLong(TEAM_SAVE_FILE_VERSION);
	header.soldiercount = LittleLong(chrDisplayList.num);
	Q_strncpyz(header.name, name, sizeof(header.name));

	Cvar_Set("mn_teamname", header.name);

	snode = XML_AddNode(node, SAVE_TEAM_NODE);
	GAME_SaveTeamInfo(snode);

	snode = XML_AddNode(node, SAVE_TEAM_EQUIPMENT);
	for (i = 0; i < csi.numODs; i++) {
		const objDef_t *od = INVSH_GetItemByIDX(i);
		if (ed->numItems[od->idx] || ed->numItemsLoose[od->idx]) {
			xmlNode_t *ssnode = XML_AddNode(snode, SAVE_TEAM_ITEM);
			XML_AddString(ssnode, SAVE_TEAM_ID, od->id);
			XML_AddIntValue(ssnode, SAVE_TEAM_NUM, ed->numItems[od->idx]);
			XML_AddIntValue(ssnode, SAVE_TEAM_NUMLOOSE, ed->numItemsLoose[od->idx]);
		}
	}
	requiredBufferLength = mxmlSaveString(topNode, dummy, 2, MXML_NO_CALLBACK);
	/* required for storing compressed */
	header.xmlSize = LittleLong(requiredBufferLength);

	buf = (byte *) Mem_PoolAlloc(sizeof(byte) * requiredBufferLength + 1, cl_genericPool, 0);
	if (!buf) {
		mxmlDelete(topNode);
		Com_Printf("Error: Could not allocate enough memory to save this game\n");
		return qfalse;
	}
	mxmlSaveString(topNode, (char*)buf, requiredBufferLength + 1, MXML_NO_CALLBACK);
	mxmlDelete(topNode);

	fbuf = (byte *) Mem_PoolAlloc(requiredBufferLength + 1 + sizeof(header), cl_genericPool, 0);
	memcpy(fbuf, &header, sizeof(header));
	memcpy(fbuf + sizeof(header), buf, requiredBufferLength + 1);
	Mem_Free(buf);

	/* last step - write data */
	FS_WriteFile(fbuf, requiredBufferLength + 1 + sizeof(header), filename);
	Mem_Free(fbuf);

	return qtrue;
}

/**
 * @brief Stores a team in a specified teamslot
 */
void GAME_SaveTeam_f (void)
{
	if (!chrDisplayList.num) {
		UI_Popup(_("Note"), _("Error saving team. Nothing to save yet."));
		return;
	} else {
		char filename[MAX_OSPATH];
		int index;
		const char *name;

		if (Cmd_Argc() != 2) {
			Com_Printf("Usage: %s <slotindex>\n", Cmd_Argv(0));
			return;
		}

		index = atoi(Cmd_Argv(1));

		name = Cvar_GetString(va("mn_slot%i", index));
		if (name[0] == '\0')
			name = _("NewTeam");

		/* save */
		Com_sprintf(filename, sizeof(filename), "save/team%i.mpt", index);
		if (!GAME_SaveTeam(filename, name))
			UI_Popup(_("Note"), _("Error saving team. Check free disk space!"));
	}
}

/**
 * @brief Load a team from an xml file
 */
static qboolean GAME_LoadTeam (const char *filename)
{
	qFILE f;
	byte *cbuf;
	int clen;
	xmlNode_t *topNode, *node, *snode, *ssnode;
	teamSaveFileHeader_t header;
	equipDef_t *ed;

	/* open file */
	FS_OpenFile(filename, &f, FILE_READ);
	if (!f.f && !f.z) {
		Com_Printf("Couldn't open file '%s'\n", filename);
		return qfalse;
	}

	clen = FS_FileLength(&f);
	cbuf = (byte *) Mem_PoolAlloc(sizeof(byte) * clen, cl_genericPool, 0);
	if (FS_Read(cbuf, clen, &f) != clen) {
		Com_Printf("Warning: Could not read %i bytes from savefile\n", clen);
		FS_CloseFile(&f);
		Mem_Free(cbuf);
		return qfalse;
	}
	FS_CloseFile(&f);

	memcpy(&header, cbuf, sizeof(header));
	/* swap all int values if needed */
	header.version = LittleLong(header.version);
	header.xmlSize = LittleLong(header.xmlSize);

	if (header.version != TEAM_SAVE_FILE_VERSION) {
		Com_Printf("Invalid version number\n");
		return qfalse;
	}

	Com_Printf("Loading team (size %d / %i)\n", clen, header.xmlSize);

	topNode = XML_Parse((const char*)(cbuf + sizeof(header)));
	Mem_Free(cbuf);
	if (!topNode) {
		Com_Printf("Error: Failure in loading the xml data!");
		return qfalse;
	}

	node = XML_GetNode(topNode, SAVE_TEAM_ROOTNODE);
	if (!node) {
		mxmlDelete(topNode);
		Com_Printf("Error: Failure in loading the xml data! (node '"SAVE_TEAM_ROOTNODE"' not found)\n");
		return qfalse;
	}
	Cvar_Set("mn_teamname", header.name);

	snode = XML_GetNode(node, SAVE_TEAM_NODE);
	if (!snode) {
		mxmlDelete(topNode);
		Mem_Free(cbuf);
		Com_Printf("Error: Failure in loading the xml data! (node '"SAVE_TEAM_NODE"' not found)\n");
		return qfalse;
	}
	GAME_LoadTeamInfo(snode);

	snode = XML_GetNode(node, SAVE_TEAM_EQUIPMENT);
	if (!snode) {
		mxmlDelete(topNode);
		Mem_Free(cbuf);
		Com_Printf("Error: Failure in loading the xml data! (node '"SAVE_TEAM_EQUIPMENT"' not found)\n");
		return qfalse;
	}

	ed = GAME_GetEquipmentDefinition();
	for (ssnode = XML_GetNode(snode, SAVE_TEAM_ITEM); ssnode; ssnode = XML_GetNextNode(ssnode, snode, SAVE_TEAM_ITEM)) {
		const char *objID = XML_GetString(ssnode, SAVE_TEAM_ID);
		const objDef_t *od = INVSH_GetItemByID(objID);

		if (od) {
			ed->numItems[od->idx] = XML_GetInt(snode, SAVE_TEAM_NUM, 0);
			ed->numItemsLoose[od->idx] = XML_GetInt(snode, SAVE_TEAM_NUMLOOSE, 0);
		}
	}

	Com_Printf("File '%s' loaded.\n", filename);

	mxmlDelete(topNode);

	return qtrue;
}

qboolean GAME_LoadDefaultTeam (qboolean force)
{
	if (cls.teamSaveSlotIndex == NO_TEAM_SLOT_LOADED) {
		if (!force)
			return qfalse;
		cls.teamSaveSlotIndex = 0;
	}

	if (GAME_LoadTeam(va("save/team%i.mpt", cls.teamSaveSlotIndex)) && !GAME_IsTeamEmpty()) {
		return qtrue;
	}

	cls.teamSaveSlotIndex = NO_TEAM_SLOT_LOADED;
	return qfalse;
}

/**
 * @brief Loads the selected teamslot
 */
void GAME_LoadTeam_f (void)
{
	char filename[MAX_OSPATH];
	int index;

	if (Cmd_Argc() != 2) {
		Com_Printf("Usage: %s <slotindex>\n", Cmd_Argv(0));
		return;
	}

	index = atoi(Cmd_Argv(1));

	/* first try to load the xml file, if this does not succeed, try the old file */
	Com_sprintf(filename, sizeof(filename), "save/team%i.mpt", index);
	if (GAME_LoadTeam(filename) && !GAME_IsTeamEmpty()) {
		cls.teamSaveSlotIndex = index;
	} else {
		cls.teamSaveSlotIndex = NO_TEAM_SLOT_LOADED;
	}
}

/**
 * @brief Get the equipment definition (from script files) for the current selected team
 * and updates the equipment inventory containers
 */
static void GAME_GetEquipment (void)
{
	const equipDef_t *edFromScript;
	const char *teamID = "phalanx";
	char equipmentName[MAX_VAR];
	equipDef_t *ed;

	Com_sprintf(equipmentName, sizeof(equipmentName), "multiplayer_%s", teamID);

	/* search equipment definition */
	edFromScript = INV_GetEquipmentDefinitionByID(equipmentName);

	ed = GAME_GetEquipmentDefinition();
	*ed = *edFromScript;

	OBJZERO(game_inventory);
	GAME_UpdateInventory(&game_inventory, ed);
}

void GAME_UpdateInventory (inventory_t *inv, const equipDef_t *ed)
{
	if (chrDisplayList.num > 0)
		ui_inventory = &chrDisplayList.chr[0]->i;
	else
		ui_inventory = NULL;

	/* manage inventory */
	UI_ContainerNodeUpdateEquipment(inv, ed);
}

/**
 * @brief Displays actor info and equipment and unused items in proper (filter) category.
 * @note This function is called every time the team equipment screen for the team pops up.
 */
void GAME_UpdateTeamMenuParameters_f (void)
{
	int i;
	const size_t size = lengthof(chrDisplayList.chr);

	/* reset description */
	Cvar_Set("mn_itemname", "");
	Cvar_Set("mn_item", "");
	UI_ResetData(TEXT_STANDARD);

	for (i = 0; i < size; i++) {
		const char *name;
		if (i < chrDisplayList.num)
			name = chrDisplayList.chr[i]->name;
		else
			name = "";
		Cvar_Set(va("mn_name%i", i), name);
	}

	GAME_GetEquipment();
}

void GAME_ActorSelect_f (void)
{
	const int oldChrIndex = cl_selected->integer;
	int chrIndex;
	character_t *chr;

	/* check syntax */
	if (Cmd_Argc() < 2) {
		Com_Printf("Usage: %s <num>\n", Cmd_Argv(0));
		return;
	}

	chrIndex = atoi(Cmd_Argv(1));
	if (chrIndex < 0 || chrIndex >= chrDisplayList.num)
		return;

	chr = chrDisplayList.chr[chrIndex];
	/* update menu inventory */
	if (ui_inventory && ui_inventory != &chr->i) {
		CONTAINER(chr, csi.idEquip) = ui_inventory->c[csi.idEquip];
		/* set 'old' idEquip to NULL */
		ui_inventory->c[csi.idEquip] = NULL;
	}
	ui_inventory = &chr->i;

	/* deselect current selected soldier and select the new one */
	UI_ExecuteConfunc("team_soldier_unselect %i", oldChrIndex);
	UI_ExecuteConfunc("team_soldier_select %i", chrIndex);

	/* now set the cl_selected cvar to the new actor id */
	Cvar_ForceSet("cl_selected", va("%i", chrIndex));
	/** @todo this is campaign only - really needed here? */
	Cvar_SetValue("mn_ucn", chr->ucn);

	/* set info cvars */
	CL_UpdateCharacterValues(chr, "mn_");
}

/**
 * @brief Save one item
 * @param[out] p XML Node structure, where we write the information to
 * @param[in] item Pointer to the item to save
 * @param[in] container Index of the container the item is in
 * @param[in] x Horizontal coordinate of the item in the container
 * @param[in] y Vertical coordinate of the item in the container
 * @sa GAME_LoadItem
 */
static void GAME_SaveItem (xmlNode_t *p, const item_t *item, containerIndex_t container, int x, int y)
{
	assert(item->t != NULL);

	XML_AddString(p, SAVE_INVENTORY_CONTAINER, INVDEF(container)->name);
	XML_AddInt(p, SAVE_INVENTORY_X, x);
	XML_AddInt(p, SAVE_INVENTORY_Y, y);
	XML_AddIntValue(p, SAVE_INVENTORY_ROTATED, item->rotated);
	XML_AddString(p, SAVE_INVENTORY_WEAPONID, item->t->id);
	/** @todo: is there any case when amount != 1 for soldier inventory item? */
	XML_AddInt(p, SAVE_INVENTORY_AMOUNT, item->amount);
	if (item->a > NONE_AMMO) {
		XML_AddString(p, SAVE_INVENTORY_MUNITIONID, item->m->id);
		XML_AddInt(p, SAVE_INVENTORY_AMMO, item->a);
	}
}

/**
 * @brief Save callback for savegames in XML Format
 * @param[out] p XML Node structure, where we write the information to
 * @param[in] i Pointer to the inventory to save
 * @sa GAME_SaveItem
 * @sa GAME_LoadInventory
 */
static void GAME_SaveInventory (xmlNode_t *p, const inventory_t *i)
{
	containerIndex_t container;

	for (container = 0; container < csi.numIDs; container++) {
		invList_t *ic = i->c[container];

		/* ignore items linked from any temp container */
		if (INVDEF(container)->temp)
			continue;

		for (; ic; ic = ic->next) {
			xmlNode_t *s = XML_AddNode(p, SAVE_INVENTORY_ITEM);
			GAME_SaveItem(s, &ic->item, container, ic->x, ic->y);
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
 * @sa GAME_SaveItem
 */
static void GAME_LoadItem (xmlNode_t *n, item_t *item, containerIndex_t *container, int *x, int *y)
{
	const char *itemID = XML_GetString(n, SAVE_INVENTORY_WEAPONID);
	const char *contID = XML_GetString(n, SAVE_INVENTORY_CONTAINER);
	int i;

	/* reset */
	OBJZERO(*item);

	for (i = 0; i < csi.numIDs; i++) {
		if (Q_streq(csi.ids[i].name, contID))
			break;
	}
	if (i >= csi.numIDs) {
		Com_Printf("Invalid container id '%s'\n", contID);
	}
	*container = i;

	item->t = INVSH_GetItemByID(itemID);
	*x = XML_GetInt(n, SAVE_INVENTORY_X, 0);
	*y = XML_GetInt(n, SAVE_INVENTORY_Y, 0);
	item->rotated = XML_GetInt(n, SAVE_INVENTORY_ROTATED, 0);
	item->amount = XML_GetInt(n, SAVE_INVENTORY_AMOUNT, 1);
	item->a = XML_GetInt(n, SAVE_INVENTORY_AMMO, NONE_AMMO);
	if (item->a > NONE_AMMO) {
		itemID = XML_GetString(n, SAVE_INVENTORY_MUNITIONID);
		item->m = INVSH_GetItemByID(itemID);

		/* reset ammo count if ammunition (item) not found */
		if (!item->m)
			item->a = NONE_AMMO;
	}
}

/**
 * @brief Load callback for savegames in XML Format
 * @param[in] p XML Node structure, where we load the information from
 * @param[out] i Pointer to the inventory
 * @sa GAME_SaveInventory
 * @sa GAME_LoadItem
 * @sa I_AddToInventory
  */
static void GAME_LoadInventory (xmlNode_t *p, inventory_t *i)
{
	xmlNode_t *s;

	for (s = XML_GetNode(p, SAVE_INVENTORY_ITEM); s; s = XML_GetNextNode(s, p, SAVE_INVENTORY_ITEM)) {
		item_t item;
		containerIndex_t container;
		int x, y;

		GAME_LoadItem(s, &item, &container, &x, &y);
		if (INVDEF(container)->temp)
			Com_Error(ERR_DROP, "GAME_LoadInventory failed, tried to add '%s' to a temp container %i", item.t->id, container);

		if (!cls.i.AddToInventory(&cls.i, i, &item, INVDEF(container), x, y, 1))
			Com_Printf("Could not add item '%s' to inventory\n", item.t ? item.t->id : "NULL");
	}
}

/**
 * @brief saves a character to a given xml node
 * @param[in] p The node to which we should save the character
 * @param[in] chr The character we should save
 */
qboolean GAME_SaveCharacter (xmlNode_t *p, const character_t* chr)
{
	xmlNode_t *sScore;
	xmlNode_t *sInventory;
	int k;
	const chrScoreGlobal_t *score;

	assert(chr);
	Com_RegisterConstList(saveCharacterConstants);
	/* Store the character data */
	XML_AddString(p, SAVE_CHARACTER_NAME, chr->name);
	XML_AddString(p, SAVE_CHARACTER_BODY, chr->body);
	XML_AddString(p, SAVE_CHARACTER_PATH, chr->path);
	XML_AddString(p, SAVE_CHARACTER_HEAD, chr->head);
	XML_AddInt(p, SAVE_CHARACTER_SKIN, chr->skin);
	XML_AddString(p, SAVE_CHARACTER_TEAMDEF, chr->teamDef->id);
	XML_AddInt(p, SAVE_CHARACTER_GENDER, chr->gender);
	XML_AddInt(p, SAVE_CHARACTER_UCN, chr->ucn);
	XML_AddInt(p, SAVE_CHARACTER_MAXHP, chr->maxHP);
	XML_AddInt(p, SAVE_CHARACTER_HP, chr->HP);
	XML_AddInt(p, SAVE_CHARACTER_STUN, chr->STUN);
	XML_AddInt(p, SAVE_CHARACTER_MORALE, chr->morale);
	XML_AddInt(p, SAVE_CHARACTER_FIELDSIZE, chr->fieldSize);
	XML_AddIntValue(p, SAVE_CHARACTER_STATE, chr->state);

	score = &chr->score;

	sScore = XML_AddNode(p, SAVE_CHARACTER_SCORES);
	/* Store skills */
	for (k = 0; k <= SKILL_NUM_TYPES; k++) {
		if (score->experience[k] || score->initialSkills[k]
		 || (k < SKILL_NUM_TYPES && score->skills[k])) {
			xmlNode_t *sSkill = XML_AddNode(sScore, SAVE_CHARACTER_SKILLS);
			const int initial = score->initialSkills[k];
			const int experience = score->experience[k];

			XML_AddString(sSkill, SAVE_CHARACTER_SKILLTYPE, Com_GetConstVariable(SAVE_CHARACTER_SKILLTYPE_NAMESPACE, k));
			XML_AddIntValue(sSkill, SAVE_CHARACTER_INITSKILL, initial);
			XML_AddIntValue(sSkill, SAVE_CHARACTER_EXPERIENCE, experience);
			if (k < SKILL_NUM_TYPES) {
				const int skills = *(score->skills + k);
				const int improve = skills - initial;
				XML_AddIntValue(sSkill, SAVE_CHARACTER_SKILLIMPROVE, improve);
			}
		}
	}
	/* Store kills */
	for (k = 0; k < KILLED_NUM_TYPES; k++) {
		xmlNode_t *sKill;
		if (score->kills[k] || score->stuns[k]) {
			sKill = XML_AddNode(sScore, SAVE_CHARACTER_KILLS);
			XML_AddString(sKill, SAVE_CHARACTER_KILLTYPE, Com_GetConstVariable(SAVE_CHARACTER_KILLTYPE_NAMESPACE, k));
			XML_AddIntValue(sKill, SAVE_CHARACTER_KILLED, score->kills[k]);
			XML_AddIntValue(sKill, SAVE_CHARACTER_STUNNED, score->stuns[k]);
		}
	}
	XML_AddIntValue(sScore, SAVE_CHARACTER_SCORE_ASSIGNEDMISSIONS, score->assignedMissions);
	XML_AddInt(sScore, SAVE_CHARACTER_SCORE_RANK, score->rank);

	/* Store inventories */
	sInventory = XML_AddNode(p, SAVE_INVENTORY_INVENTORY);
	GAME_SaveInventory(sInventory, &chr->i);

	Com_UnregisterConstList(saveCharacterConstants);
	return qtrue;
}

/**
 * @brief Loads a character from a given xml node.
 * @param[in] p The node from which we should load the character.
 * @param[in] chr Pointer to the character we should load.
 */
qboolean GAME_LoadCharacter (xmlNode_t *p, character_t *chr)
{
	const char *s;
	xmlNode_t *sScore;
	xmlNode_t *sSkill;
	xmlNode_t *sKill;
	xmlNode_t *sInventory;
	qboolean success = qtrue;

	/* Load the character data */
	Q_strncpyz(chr->name, XML_GetString(p, SAVE_CHARACTER_NAME), sizeof(chr->name));
	Q_strncpyz(chr->body, XML_GetString(p, SAVE_CHARACTER_BODY), sizeof(chr->body));
	Q_strncpyz(chr->path, XML_GetString(p, SAVE_CHARACTER_PATH), sizeof(chr->path));
	Q_strncpyz(chr->head, XML_GetString(p, SAVE_CHARACTER_HEAD), sizeof(chr->head));

	chr->skin = XML_GetInt(p, SAVE_CHARACTER_SKIN, 0);
	chr->gender = XML_GetInt(p, SAVE_CHARACTER_GENDER, 0);
	chr->ucn = XML_GetInt(p, SAVE_CHARACTER_UCN, 0);
	chr->maxHP = XML_GetInt(p, SAVE_CHARACTER_MAXHP, 0);
	chr->HP = XML_GetInt(p, SAVE_CHARACTER_HP, 0);
	chr->STUN = XML_GetInt(p, SAVE_CHARACTER_STUN, 0);
	chr->morale = XML_GetInt(p, SAVE_CHARACTER_MORALE, 0);
	chr->fieldSize = XML_GetInt(p, SAVE_CHARACTER_FIELDSIZE, 1);
	chr->state = XML_GetInt(p, SAVE_CHARACTER_STATE, 0);

	/* Team-definition */
	s = XML_GetString(p, SAVE_CHARACTER_TEAMDEF);
	chr->teamDef = Com_GetTeamDefinitionByID(s);
	if (!chr->teamDef)
		return qfalse;

	Com_RegisterConstList(saveCharacterConstants);

	sScore = XML_GetNode(p, SAVE_CHARACTER_SCORES);
	/* Load Skills */
	for (sSkill = XML_GetNode(sScore, SAVE_CHARACTER_SKILLS); sSkill; sSkill = XML_GetNextNode(sSkill, sScore, SAVE_CHARACTER_SKILLS)) {
		int idx;
		const char *type = XML_GetString(sSkill, SAVE_CHARACTER_SKILLTYPE);

		if (!Com_GetConstIntFromNamespace(SAVE_CHARACTER_SKILLTYPE_NAMESPACE, type, &idx)) {
			Com_Printf("Invalid skill type '%s' for %s (ucn: %i)\n", type, chr->name, chr->ucn);
			success = qfalse;
			break;
		}

		chr->score.initialSkills[idx] = XML_GetInt(sSkill, SAVE_CHARACTER_INITSKILL, 0);
		chr->score.experience[idx] = XML_GetInt(sSkill, SAVE_CHARACTER_EXPERIENCE, 0);
		if (idx < SKILL_NUM_TYPES) {
			chr->score.skills[idx] = chr->score.initialSkills[idx];
			chr->score.skills[idx] += XML_GetInt(sSkill, SAVE_CHARACTER_SKILLIMPROVE, 0);
		}
	}

	if (!success) {
		Com_UnregisterConstList(saveCharacterConstants);
		return qfalse;
	}

	/* Load kills */
	for (sKill = XML_GetNode(sScore, SAVE_CHARACTER_KILLS); sKill; sKill = XML_GetNextNode(sKill, sScore, SAVE_CHARACTER_KILLS)) {
		int idx;
		const char *type = XML_GetString(sKill, SAVE_CHARACTER_KILLTYPE);

		if (!Com_GetConstIntFromNamespace(SAVE_CHARACTER_KILLTYPE_NAMESPACE, type, &idx)) {
			Com_Printf("Invalid kill type '%s' for %s (ucn: %i)\n", type, chr->name, chr->ucn);
			success = qfalse;
			break;
		}
		chr->score.kills[idx] = XML_GetInt(sKill, SAVE_CHARACTER_KILLED, 0);
		chr->score.stuns[idx] = XML_GetInt(sKill, SAVE_CHARACTER_STUNNED, 0);
	}

	if (!success) {
		Com_UnregisterConstList(saveCharacterConstants);
		return qfalse;
	}

	chr->score.assignedMissions = XML_GetInt(sScore, SAVE_CHARACTER_SCORE_ASSIGNEDMISSIONS, 0);
	chr->score.rank = XML_GetInt(sScore, SAVE_CHARACTER_SCORE_RANK, -1);

	cls.i.DestroyInventory(&cls.i, &chr->i);
	sInventory = XML_GetNode(p, SAVE_INVENTORY_INVENTORY);
	GAME_LoadInventory(sInventory, &chr->i);

	Com_UnregisterConstList(saveCharacterConstants);
	return qtrue;
}
