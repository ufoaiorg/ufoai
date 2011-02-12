/**
 * @file mp_team.c
 * @brief Multiplayer team management.
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

#include "../../cl_shared.h"
#include "../cl_game.h"
#include "../../cl_inventory.h"
#include "../../cl_team.h"
#include "../../../common/qfiles.h"
#include "../../ui/ui_main.h"
#include "../../ui/ui_popup.h"
#include "../../ui/node/ui_node_container.h"	/**< ui_inventory */
#include "mp_team.h"
#include "save_multiplayer.h"

#define MPTEAM_SAVE_FILE_VERSION 4

static inventory_t mp_inventory;

static qboolean characterActive[MAX_ACTIVETEAM];

typedef struct teamSaveFileHeader_s {
	uint32_t version; /**< which savegame version */
	uint32_t soldiercount; /** the number of soldiers stored in this savegame */
	char name[32]; /**< savefile name */
	uint32_t xmlSize; /** needed, if we store compressed */
} teamSaveFileHeader_t;

static void MP_UpdateActiveTeamList (void)
{
	int i;

	OBJZERO(characterActive);
	for (i = 0; i < chrDisplayList.num; i++)
		characterActive[i] = qtrue;

	UI_ExecuteConfunc("mp_checkboxes_update %i", chrDisplayList.num);
}

void MP_AutoTeam_f (void)
{
	GAME_MP_AutoTeam();

	MP_UpdateActiveTeamList();
}

/**
 * @brief This will activate/deactivate the actor for the team
 * @sa GAME_MP_SaveTeamState_f
 */
void MP_ToggleActorForTeam_f (void)
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
		value = 0;

	characterActive[num] = (value != 0);
}

/**
 * @brief Will remove those actors that should not be used in the team
 * @sa GAME_MP_ToggleActorForTeam_f
 */
void MP_SaveTeamState_f (void)
{
	int i, num;

	num = 0;
	for (i = 0; i < chrDisplayList.num; i++) {
		chrDisplayList.chr[num]->ucn -= (i -num);
		if (characterActive[i]) {
			assert(chrDisplayList.chr[i] != NULL);
			chrDisplayList.chr[num++] = chrDisplayList.chr[i];
		}
	}
	chrDisplayList.num = num;
}

/**
 * @brief Reads tha comments from team files
 */
void MP_MultiplayerTeamSlotComments_f (void)
{
	int i;

	for (i = 0; i < 8; i++) {
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
			if (LittleLong(header.version) == MPTEAM_SAVE_FILE_VERSION) {
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
 * @note Called by MP_SaveTeamMultiplayer to store the team info
 * @sa GAME_SendCurrentTeamSpawningInfo
 */
static void MP_SaveTeamMultiplayerInfo (mxml_node_t *p)
{
	int i;

	/* header */
	Com_DPrintf(DEBUG_CLIENT, "Saving %i teammembers\n", chrDisplayList.num);
	for (i = 0; i < chrDisplayList.num; i++) {
		const character_t *chr = chrDisplayList.chr[i];
		mxml_node_t *n = mxml_AddNode(p, SAVE_MULTIPLAYER_CHARACTER);
		CL_SaveCharacterXML(n, chr);
	}
}

/**
 * @brief Loads the wholeTeam from the xml file
 * @note Called by MP_SaveTeamMultiplayer to store the team info
 * @sa GAME_SendCurrentTeamSpawningInfo
 */
static void MP_LoadTeamMultiplayerInfo (mxml_node_t *p)
{
	int i;
	mxml_node_t *n;
	const size_t size = GAME_GetCharacterArraySize();

	GAME_ResetCharacters();
	OBJZERO(characterActive);

	/* header */
	for (i = 0, n = mxml_GetNode(p, SAVE_MULTIPLAYER_CHARACTER); n && i < size; i++, n = mxml_GetNextNode(n, p, SAVE_MULTIPLAYER_CHARACTER)) {
		character_t *chr = GAME_GetCharacter(i);
		CL_LoadCharacterXML(n, chr);
		assert(i < lengthof(chrDisplayList.chr));
		chrDisplayList.chr[i] = chr;
	}
	chrDisplayList.num = i;

	MP_UpdateActiveTeamList();

	Com_DPrintf(DEBUG_CLIENT, "Loaded %i teammembers\n", chrDisplayList.num);
}

/**
 * @brief Saves a multiplayer team in xml format
 * @sa MP_SaveTeamMultiplayerInfoXML
 * @sa MP_LoadTeamMultiplayerXML
 */
static qboolean MP_SaveTeamMultiplayer (const char *filename, const char *name)
{
	int requiredBufferLength;
	byte *buf, *fbuf;
	teamSaveFileHeader_t header;
	char dummy[2];
	int i;
	mxml_node_t *topNode, *node, *snode;
	equipDef_t *ed = GAME_GetEquipmentDefinition();

	topNode = mxmlNewXML("1.0");
	node = mxml_AddNode(topNode, SAVE_MULTIPLAYER_ROOTNODE);
	OBJZERO(header);
	header.version = LittleLong(MPTEAM_SAVE_FILE_VERSION);
	header.soldiercount = LittleLong(chrDisplayList.num);
	Q_strncpyz(header.name, name, sizeof(header.name));

	snode = mxml_AddNode(node, SAVE_MULTIPLAYER_TEAM);
	MP_SaveTeamMultiplayerInfo(snode);

	snode = mxml_AddNode(node, SAVE_MULTIPLAYER_EQUIPMENT);
	for (i = 0; i < csi.numODs; i++) {
		const objDef_t *od = INVSH_GetItemByIDX(i);
		if (ed->numItems[od->idx] || ed->numItemsLoose[od->idx]) {
			mxml_node_t *ssnode = mxml_AddNode(snode, SAVE_MULTIPLAYER_ITEM);
			mxml_AddString(ssnode, SAVE_MULTIPLAYER_ID, od->id);
			mxml_AddIntValue(ssnode, SAVE_MULTIPLAYER_NUM, ed->numItems[od->idx]);
			mxml_AddIntValue(ssnode, SAVE_MULTIPLAYER_NUMLOOSE, ed->numItemsLoose[od->idx]);
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
 * @brief Stores a team in a specified teamslot (multiplayer)
 */
void MP_SaveTeamMultiplayer_f (void)
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
		if (!MP_SaveTeamMultiplayer(filename, name))
			UI_Popup(_("Note"), _("Error saving team. Check free disk space!"));
	}
}

/**
 * @brief Load a multiplayer team from an xml file
 * @sa MP_LoadTeamMultiplayer
 * @sa MP_SaveTeamMultiplayer
 */
static qboolean MP_LoadTeamMultiplayer (const char *filename)
{
	uLongf len;
	qFILE f;
	byte *cbuf;
	int clen;
	mxml_node_t *topNode, *node, *snode, *ssnode;
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
	len = header.xmlSize + 1 + sizeof(header);

	if (header.version != MPTEAM_SAVE_FILE_VERSION) {
		Com_Printf("Invalid version number\n");
		return qfalse;
	}

	Com_Printf("Loading multiplayer team (size %d / %i)\n", clen, header.xmlSize);

	topNode = mxmlLoadString(NULL, (char*)(cbuf + sizeof(header)) , mxml_ufo_type_cb);
	Mem_Free(cbuf);
	if (!topNode) {
		Com_Printf("Error: Failure in loading the xml data!");
		return qfalse;
	}

	node = mxml_GetNode(topNode, SAVE_MULTIPLAYER_ROOTNODE);
	if (!node) {
		mxmlDelete(topNode);
		Com_Printf("Error: Failure in loading the xml data! (node 'multiplayer' not found)\n");
		return qfalse;
	}
	Cvar_Set("mn_teamname", header.name);

	snode = mxml_GetNode(node, SAVE_MULTIPLAYER_TEAM);
	if (!snode) {
		mxmlDelete(topNode);
		Mem_Free(cbuf);
		Com_Printf("Error: Failure in loading the xml data! (node 'team' not found)\n");
		return qfalse;
	}
	MP_LoadTeamMultiplayerInfo(snode);

	snode = mxml_GetNode(node, SAVE_MULTIPLAYER_EQUIPMENT);
	if (!snode) {
		mxmlDelete(topNode);
		Mem_Free(cbuf);
		Com_Printf("Error: Failure in loading the xml data! (node 'equipment' not found)\n");
		return qfalse;
	}

	ed = GAME_GetEquipmentDefinition();
	for (ssnode = mxml_GetNode(snode, SAVE_MULTIPLAYER_ITEM); ssnode; ssnode = mxml_GetNextNode(ssnode, snode, SAVE_MULTIPLAYER_ITEM)) {
		const char *objID = mxml_GetString(ssnode, SAVE_MULTIPLAYER_ID);
		const objDef_t *od = INVSH_GetItemByID(objID);

		if (od) {
			ed->numItems[od->idx] = mxml_GetInt(snode, SAVE_MULTIPLAYER_NUM, 0);
			ed->numItemsLoose[od->idx] = mxml_GetInt(snode, SAVE_MULTIPLAYER_NUMLOOSE, 0);
		}
	}

	Com_Printf("File '%s' loaded.\n", filename);

	mxmlDelete(topNode);

	return qtrue;
}

qboolean MP_LoadDefaultTeamMultiplayer (void)
{
	return MP_LoadTeamMultiplayer("save/team0.mpt");
}

/**
 * @brief Loads the selected teamslot
 */
void MP_LoadTeamMultiplayer_f (void)
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
	MP_LoadTeamMultiplayer(filename);
}

/**
 * @brief Get the equipment definition (from script files) for the current selected multiplayer team
 * and updates the equipment inventory containers
 */
static void MP_GetEquipment (void)
{
	const equipDef_t *edFromScript;
	const char *teamID = Com_ValueToStr(&cl_teamnum->integer, V_TEAM, 0);
	char equipmentName[MAX_VAR];
	equipDef_t unused;
	equipDef_t *ed;

	Com_sprintf(equipmentName, sizeof(equipmentName), "multiplayer_%s", teamID);

	/* search equipment definition */
	edFromScript = INV_GetEquipmentDefinitionByID(equipmentName);

	if (chrDisplayList.num > 0)
		ui_inventory = &chrDisplayList.chr[0]->i;
	else
		ui_inventory = NULL;

	ed = GAME_GetEquipmentDefinition();
	*ed = *edFromScript;

	/* we don't want to lose anything from ed - so we copy it and screw the copied stuff afterwards */
	unused = *edFromScript;
	/* manage inventory */
	UI_ContainerNodeUpdateEquipment(&mp_inventory, &unused);
}

/**
 * @brief Displays actor info and equipment and unused items in proper (filter) category.
 * @note This function is called everytime the multiplayer equipment screen for the team pops up.
 */
void MP_UpdateMenuParameters_f (void)
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

	MP_GetEquipment();
}

void MP_TeamSelect_f (void)
{
	const int old = cl_selected->integer;
	int num;

	/* check syntax */
	if (Cmd_Argc() < 2) {
		Com_Printf("Usage: %s <num>\n", Cmd_Argv(0));
		return;
	}

	num = atoi(Cmd_Argv(1));
	if (num < 0 || num >= chrDisplayList.num)
		return;

	UI_ExecuteConfunc("mp_soldier_select %i", num);
	UI_ExecuteConfunc("mp_soldier_unselect %i", old);
	Cmd_ExecuteString(va("actor_select_equip %i", num));
}
