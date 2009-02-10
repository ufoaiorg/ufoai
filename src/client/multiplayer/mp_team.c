/**
 * @file mp_team.c
 * @brief Multiplayer team management.
 */

/*
Copyright (C) 2002-2007 UFO: Alien Invasion team.

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
#include "../cl_global.h"
#include "../cl_game.h"
#include "../cl_team.h"
#include "../menu/m_popup.h"
#include "../menu/m_nodes.h"	/**< menuInventory */
#include "mp_team.h"

static inventory_t mp_inventory;
character_t multiplayerCharacters[MAX_MULTIPLAYER_CHARACTERS];

typedef struct mpSaveFileHeader_s {
	int version; /**< which savegame version */
	int soldiercount; /** the number of soldiers stored in this savegame */
	int compressed; /**< is this file compressed via zlib, currently not implemented */
	char name[32]; /**< savefile name */
	int dummy[14]; /**< maybe we have to extend this later */
	long xml_size; /** needed, if we store compressed */
} mpSaveFileHeader_t;




/**
 * @brief Reads tha comments from team files
 */
void MP_MultiplayerTeamSlotComments_f (void)
{
	byte buf[MAX_TEAMDATASIZE];
	int i;

	for (i = 0; i < 8; i++) {
		int version, soldierCount;
		const char *comment;
		mpSaveFileHeader_t header;
		/* open file */
		qFILE qf;
		qboolean error = qfalse;

		qf.f = fopen(va("%s/save/team%i.mptx", FS_Gamedir(), i), "rb");
		if (!qf.f) {
			error = qtrue;
		} else {
			const int clen = sizeof(header);
			byte *cbuf = (byte *) Mem_PoolAlloc(sizeof(byte) * clen, cl_genericPool, CL_TAG_NONE);
			if (fread(cbuf, 1, clen, qf.f) != clen) {
				error = qtrue;
				Com_Printf("Warning: Could not read %i bytes from savefile\n", clen);
			}
			fclose(qf.f);
			memcpy(&header, cbuf, sizeof(header));
			version = header.version;
			comment = header.name;
		}

		if (error) {
			sizebuf_t sb;
			FILE *f = fopen(va("%s/save/team%i.mpt", FS_Gamedir(), i), "rb");
			if (!f) {
				Cvar_Set(va("mn_slot%i", i), "");
				continue;
			}

			/* read data */
			SZ_Init(&sb, buf, MAX_TEAMDATASIZE);
			sb.cursize = fread(buf, 1, MAX_TEAMDATASIZE, f);
			fclose(f);

			version = MSG_ReadByte(&sb);
			comment = MSG_ReadString(&sb);
			soldierCount = MSG_ReadByte(&sb);
		}

		MN_ExecuteConfunc("set_slotname %i %i \"%s\"", i, soldierCount, comment);
	}
}

/**
 * @brief Stores the wholeTeam into a xml structure
 * @note Called by MP_SaveTeamMultiplayer to store the team info
 * @sa GAME_SendCurrentTeamSpawningInfo
 * @sa MP_LoadTeamMultiplayerMember
 */
static void MP_SaveTeamMultiplayerInfoXML (mxml_node_t *p)
{
	int i;

	/* header */
	Com_DPrintf(DEBUG_CLIENT, "Saving %i teammembers\n", chrDisplayList.num);
	for (i = 0; i < chrDisplayList.num; i++) {
		const character_t *chr = chrDisplayList.chr[i];
		mxml_node_t *n = mxml_AddNode(p, "character");
		CL_SaveCharacterXML(n, *chr);
	}
}

/**
 * @brief Loads the wholeTeam from the xml file
 * @note Called by MP_SaveTeamMultiplayer to store the team info
 * @sa GAME_SendCurrentTeamSpawningInfo
 * @sa MP_LoadTeamMultiplayerMember
 */
static void MP_LoadTeamMultiplayerInfoXML (mxml_node_t *p)
{
	int i;
	mxml_node_t *n;

	/* header */
	for (i = 0, n = mxml_GetNode(p, "character"); n && i < MAX_MULTIPLAYER_CHARACTERS; i++, n = mxml_GetNextNode(n, p, "character")) {
		CL_LoadCharacterXML(n, &multiplayerCharacters[i]);
		chrDisplayList.chr[i] = &multiplayerCharacters[i];
	}
	chrDisplayList.num = i;
	Com_DPrintf(DEBUG_CLIENT, "Loaded %i teammembers\n", chrDisplayList.num);
}

/**
 * @brief Stores the wholeTeam info to buffer (which might be a network buffer, too)
 * @note Called by MP_SaveTeamMultiplayer to store the team info
 * @sa GAME_SendCurrentTeamSpawningInfo
 * @sa MP_LoadTeamMultiplayerMember
 */
static void MP_SaveTeamMultiplayerInfo (sizebuf_t *buf)
{
	int i, j;

	/* header */
	Com_DPrintf(DEBUG_CLIENT, "Saving %i teammembers\n", chrDisplayList.num);
	MSG_WriteByte(buf, chrDisplayList.num);
	for (i = 0; i < chrDisplayList.num; i++) {
		const character_t *chr = chrDisplayList.chr[i];
		/* send the fieldSize ACTOR_SIZE_* */
		MSG_WriteByte(buf, chr->fieldSize);

		/* unique character number */
		MSG_WriteShort(buf, chr->ucn);

		/* name */
		MSG_WriteString(buf, chr->name);

		/* model */
		MSG_WriteString(buf, chr->path);
		MSG_WriteString(buf, chr->body);
		MSG_WriteString(buf, chr->head);
		MSG_WriteByte(buf, chr->skin);

		MSG_WriteShort(buf, chr->HP);
		MSG_WriteShort(buf, chr->maxHP);
		MSG_WriteByte(buf, chr->teamDef ? chr->teamDef->idx : BYTES_NONE);
		MSG_WriteByte(buf, chr->gender);
		MSG_WriteByte(buf, chr->STUN);
		MSG_WriteByte(buf, chr->morale);

		/* Scores */
		MSG_WriteByte(buf, SKILL_NUM_TYPES + 1);
		for (j = 0; j < SKILL_NUM_TYPES + 1; j++)
			MSG_WriteLong(buf, chr->score.experience[j]);
		MSG_WriteByte(buf, SKILL_NUM_TYPES);
		for (j = 0; j < SKILL_NUM_TYPES; j++)	/* even new attributes */
			MSG_WriteByte(buf, chr->score.skills[j]);
		MSG_WriteByte(buf, SKILL_NUM_TYPES + 1);
		for (j = 0; j < SKILL_NUM_TYPES + 1; j++)
			MSG_WriteByte(buf, chr->score.initialSkills[j]);
		MSG_WriteByte(buf, KILLED_NUM_TYPES);
		for (j = 0; j < KILLED_NUM_TYPES; j++)
			MSG_WriteShort(buf, chr->score.kills[j]);
		MSG_WriteByte(buf, KILLED_NUM_TYPES);
		for (j = 0; j < KILLED_NUM_TYPES; j++)
			MSG_WriteShort(buf, chr->score.stuns[j]);
		MSG_WriteShort(buf, chr->score.assignedMissions);
		MSG_WriteShort(buf, chr->score.rank);

		/* Save user-defined (default) reaction-state. */
		MSG_WriteShort(buf, chr->reservedTus.reserveReaction);

		/* inventory */
		CL_SaveInventory(buf, &chr->inv);
	}
}

/**
 * @brief Saves a multiplayer team in xml format
 * @sa MP_SaveTeamMultiplayerInfoXML
 * @sa MP_LoadTeamMultiplayerXML
 * @todo Implement EMPL_ROBOT
 */
static qboolean MP_SaveTeamMultiplayerXML (const char *filename, const char *name)
{
	int res;
	int requiredbuflen;
	byte *buf, *fbuf;
	uLongf bufLen;
	mpSaveFileHeader_t header;
	char dummy[2];
	int i;
	mxml_node_t *top_node, *node, *snode;

	top_node = mxmlNewXML("1.0");
	node = mxml_AddNode(top_node, "multiplayer");
	memset(&header, 0, sizeof(header));
	header.compressed = 0;
	header.version = LittleLong(MPTEAM_SAVE_FILE_VERSION);
	header.soldiercount = LittleLong(chrDisplayList.num);
	Q_strncpyz(header.name, name, sizeof(header.name));

	snode = mxml_AddNode(node, "team");
	MP_SaveTeamMultiplayerInfoXML(snode);

	snode = mxml_AddNode(node, "equipment");
	for (i = 0; i < csi.numODs; i++) {
		mxml_node_t *ssnode = mxml_AddNode(snode, "emission");
		mxml_AddString(ssnode, "id", csi.ods[i].id);
		mxml_AddInt(ssnode, "num", ccs.eMission.num[i]);
		mxml_AddInt(ssnode, "numloose", ccs.eMission.numLoose[i]);
	}
	requiredbuflen = mxmlSaveString(top_node, dummy, 2, MXML_NO_CALLBACK);
	/** @todo little/big endian */
	/* required for storing compressed */
	header.xml_size = requiredbuflen;

	buf = (byte *) Mem_PoolAlloc(sizeof(byte) * requiredbuflen + 1, cl_genericPool, CL_TAG_NONE);
	if (!buf) {
		Com_Printf("Error: Could not allocate enough memory to save this game\n");
		return qfalse;
	}
	res = mxmlSaveString(top_node, (char*)buf, requiredbuflen + 1, MXML_NO_CALLBACK);
	Com_Printf("XML Written to buffer (%d Bytes)\n", res);

	bufLen = (uLongf) (24 + 1.02 * requiredbuflen);
	fbuf = (byte *) Mem_PoolAlloc(sizeof(byte) * bufLen + sizeof(header), cl_genericPool, CL_TAG_NONE);
	memcpy(fbuf, &header, sizeof(header));

	if (header.compressed) {
		res = compress(fbuf + sizeof(header), &bufLen, buf, requiredbuflen + 1);
		Mem_Free(buf);
		if (res != Z_OK) {
			Mem_Free(fbuf);
			Com_Printf("Memory error compressing save-game data (Error: %i)!\n", res);
			return qfalse;
		}
	} else {
		memcpy(fbuf + sizeof(header), buf, requiredbuflen + 1);
		Mem_Free(buf);
	}
	/* last step - write data */
	res = FS_WriteFile(fbuf, bufLen + sizeof(header), filename);
	Mem_Free(fbuf);
	return qtrue;
}

/**
 * @brief Saves a multiplayer team
 * @sa MP_SaveTeamMultiplayerInfo
 * @sa MP_LoadTeamMultiplayer
 * @todo Implement EMPL_ROBOT
 */
static qboolean MP_SaveTeamMultiplayer (const char *filename, const char *name)
{
	sizebuf_t sb;
	byte buf[MAX_TEAMDATASIZE];
	int i, res;

	/* create data */
	SZ_Init(&sb, buf, MAX_TEAMDATASIZE);
	MSG_WriteByte(&sb, MPTEAM_SAVE_FILE_VERSION);

	/* store teamname */
	MSG_WriteString(&sb, name);

	/* store team */
	MP_SaveTeamMultiplayerInfo(&sb);

	/* store equipment in ccs.eMission so soldiers can be properly equipped */
	MSG_WriteShort(&sb, csi.numODs);
	for (i = 0; i < csi.numODs; i++) {
		MSG_WriteString(&sb, csi.ods[i].id);
		MSG_WriteLong(&sb, ccs.eMission.num[i]);
		MSG_WriteByte(&sb, ccs.eMission.numLoose[i]);
	}

	/* write data */
	res = FS_WriteFile(buf, sb.cursize, filename);
	if (res == sb.cursize && res > 0) {
		Com_Printf("Team '%s' saved. Size written: %i\n", filename, res);
		return qtrue;
	} else {
		Com_Printf("Team '%s' not saved.\n", filename);
		return qfalse;
	}
}

/**
 * @brief Stores a team in a specified teamslot (multiplayer)
 */
void MP_SaveTeamMultiplayer_f (void)
{
	if (!chrDisplayList.num) {
		MN_Popup(_("Note"), _("Error saving team. Nothing to save yet."));
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

		name = Cvar_VariableString(va("mn_slot%i", index));
		if (!strlen(name))
			name = _("NewTeam");

		/* save */
		Com_sprintf(filename, sizeof(filename), "%s/save/team%i.mpt", FS_Gamedir(), index);
		if (!MP_SaveTeamMultiplayer(filename, name))
			MN_Popup(_("Note"), _("Error saving team. Check free disk space!"));
		/*also saving the xml version */
		Com_sprintf(filename, sizeof(filename), "%s/save/team%i.mptx", FS_Gamedir(), index);
		if (!MP_SaveTeamMultiplayerXML(filename, name))
			MN_Popup(_("Note"), _("Error saving team. Check free disk space!"));
	}
}

/**
 * @brief Load a team member for multiplayer
 * @sa MP_LoadTeamMultiplayer
 * @sa MP_SaveTeamMultiplayerInfo
 */
static void MP_LoadTeamMultiplayerMember (sizebuf_t * sb, character_t * chr, int version)
{
	int i, num;
	/* Team-definition index. */
	int td;

	/* unique character number */
	chr->fieldSize = MSG_ReadByte(sb);
	chr->ucn = MSG_ReadShort(sb);
	if (chr->ucn >= cl.nextUniqueCharacterNumber)
		cl.nextUniqueCharacterNumber = chr->ucn + 1;

	/* name and model */
	Q_strncpyz(chr->name, MSG_ReadStringRaw(sb), sizeof(chr->name));
	Q_strncpyz(chr->path, MSG_ReadString(sb), sizeof(chr->path));
	Q_strncpyz(chr->body, MSG_ReadString(sb), sizeof(chr->body));
	Q_strncpyz(chr->head, MSG_ReadString(sb), sizeof(chr->head));
	chr->skin = MSG_ReadByte(sb);

	chr->HP = MSG_ReadShort(sb);
	chr->maxHP = MSG_ReadShort(sb);
	chr->teamDef = NULL;
	td = MSG_ReadByte(sb);
	if (td != BYTES_NONE)
		chr->teamDef = &csi.teamDef[td];
	chr->gender = MSG_ReadByte(sb);
	chr->STUN = MSG_ReadByte(sb);
	chr->morale = MSG_ReadByte(sb);

	/* Load scores @sa inv_shared.h:chrScoreGlobal_t */
	num = MSG_ReadByte(sb);
	for (i = 0; i < num; i++)
		chr->score.experience[i] = MSG_ReadLong(sb);
	num = MSG_ReadByte(sb);
	for (i = 0; i < num; i++)
		chr->score.skills[i] = MSG_ReadByte(sb);
	num = MSG_ReadByte(sb);
	for (i = 0; i < num; i++)
		chr->score.initialSkills[i] = MSG_ReadByte(sb);
	num = MSG_ReadByte(sb);
	for (i = 0; i < num; i++)
		chr->score.kills[i] = MSG_ReadShort(sb);
	num = MSG_ReadByte(sb);
	for (i = 0; i < num; i++)
		chr->score.stuns[i] = MSG_ReadShort(sb);
	chr->score.assignedMissions = MSG_ReadShort(sb);
	chr->score.rank = MSG_ReadShort(sb);

	/* Load user-defined (default) reaction-state. */
	chr->reservedTus.reserveReaction = MSG_ReadShort(sb);

	/* Inventory */
	/** @todo really needed? */
	INVSH_DestroyInventory(&chr->inv);
	CL_LoadInventory(sb, &chr->inv);
}

/**
 * @brief Load a multiplayer team from an xml file
 * @sa MP_LoadTeamMultiplayer
 * @sa MP_SaveTeamMultiplayer
 * @todo only EMPL_SOLDIERs are saved and loaded
 */
static qboolean MP_LoadTeamMultiplayerXML (const char *filename)
{
	uLongf len;
	qFILE f;
	byte *cbuf, *buf;
	int i, clen;
	mxml_node_t *top_node, *node, *snode, *ssnode;
	mpSaveFileHeader_t header;

	/* open file */
	f.f = fopen(filename, "rb");
	if (!f.f) {
		Com_Printf("Couldn't open file '%s'\n", filename);
		return qfalse;
	}

	clen = FS_FileLength(&f);
	cbuf = (byte *) Mem_PoolAlloc(sizeof(byte) * clen, cl_genericPool, CL_TAG_NONE);
	if (fread(cbuf, 1, clen, f.f) != clen)
		Com_Printf("Warning: Could not read %i bytes from savefile\n", clen);
	fclose(f.f);
	Com_Printf("Loading multiplayer save from xml (size %d)\n", clen);

	memcpy(&header, cbuf, sizeof(header));
		/* swap all int values if needed */
	header.compressed = LittleLong(header.compressed);
	header.version = LittleLong(header.version);
	len = header.xml_size+50;
	buf = (byte *) Mem_PoolAlloc(sizeof(byte)*len, cl_genericPool, CL_TAG_NONE);

	if (header.compressed) {
		/* uncompress data, skipping comment header */
		const int res = uncompress(buf, &len, cbuf + sizeof(header), clen - sizeof(header));
		Mem_Free(cbuf);

		if (res != Z_OK) {
			Mem_Free(buf);
			Com_Printf("Error decompressing data in '%s'.\n", filename);
			return qfalse;
		}
		top_node = mxmlLoadString(NULL, (char*)buf , mxml_ufo_type_cb);
		if (!top_node) {
			Mem_Free(buf);
			Com_Printf("Error: Failure in Loading the xml Data!");
			return qfalse;
		}
	} else {
		/*memcpy(buf, cbuf + sizeof(header), clen - sizeof(header));*/
		top_node = mxmlLoadString(NULL, (char*)(cbuf + sizeof(header)) , mxml_ufo_type_cb);
		Mem_Free(cbuf);
		if (!top_node) {
			Mem_Free(buf);
			Com_Printf("Error: Failure in Loading the xml Data!");
			return qfalse;
		}
	}

	node = mxml_GetNode(top_node, "multiplayer");
	if (!node){
		Com_Printf("Error: Failure in Loading the xml Data! (savegame node not found)");
		Mem_Free(buf);
		return qfalse;
	}
	Cvar_Set("mn_teamname", header.name);

	snode = mxml_GetNode(node, "team");
	if (!snode)
		Com_Printf("Team Node not found\n");
	MP_LoadTeamMultiplayerInfoXML(snode);

	snode = mxml_GetNode(node, "equipment");
	if (!snode)
		Com_Printf("Equipment Node not found\n");
	for (i = 0, ssnode = mxml_GetNode(snode, "emission"); ssnode && i < csi.numODs; i++, ssnode = mxml_GetNextNode(ssnode, snode, "emission")) {
		const char *objID = mxml_GetString(ssnode, "id");
		const objDef_t *od = INVSH_GetItemByID(objID);
		if (od) {
			ccs.eMission.num[od->idx] = mxml_GetInt(snode, "num", 0);
			ccs.eMission.numLoose[od->idx] = mxml_GetInt(snode, "numloose", 0);
		}
	}

	Com_Printf("File '%s' loaded.\n", filename);
	Mem_Free(buf);
	return qtrue;
}

/**
 * @brief Load a multiplayer team
 * @sa MP_LoadTeamMultiplayer
 * @sa MP_SaveTeamMultiplayer
 * @todo only EMPL_SOLDIERs are saved and loaded
 */
static void MP_LoadTeamMultiplayer (const char *filename)
{
	sizebuf_t sb;
	byte buf[MAX_TEAMDATASIZE];
	FILE *f;
	int version;
	int i, num;

	/* open file */
	f = fopen(filename, "rb");
	if (!f) {
		Com_Printf("Couldn't open file '%s'.\n", filename);
		return;
	}

	/* read data */
	SZ_Init(&sb, buf, MAX_TEAMDATASIZE);
	sb.cursize = fread(buf, 1, MAX_TEAMDATASIZE, f);
	fclose(f);

	version = MSG_ReadByte(&sb);
	if (version != MPTEAM_SAVE_FILE_VERSION) {
		Com_Printf("Could not load multiplayer team '%s' - version differs.\n", filename);
		return;
	}

	MSG_ReadString(&sb);

	/* read whole team list */
	num = MSG_ReadByte(&sb);
	Com_DPrintf(DEBUG_CLIENT, "Loading %i teammembers\n", num);
	for (i = 0; i < num; i++) {
		MP_LoadTeamMultiplayerMember(&sb, &multiplayerCharacters[i], version);
		chrDisplayList.chr[i] = &multiplayerCharacters[i];
	}

	chrDisplayList.num = i;

	/* read equipment */
	num = MSG_ReadShort(&sb);
	for (i = 0; i < num; i++) {
		const char *objID = MSG_ReadString(&sb);
		const objDef_t *od = INVSH_GetItemByID(objID);
		if (!od) {
			MSG_ReadLong(&sb);
			MSG_ReadByte(&sb);
		} else {
			ccs.eMission.num[od->idx] = MSG_ReadLong(&sb);
			ccs.eMission.numLoose[od->idx] = MSG_ReadByte(&sb);
		}
	}
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
	Com_sprintf(filename, sizeof(filename), "%s/save/team%i.mptx", FS_Gamedir(), index);
	if (MP_LoadTeamMultiplayerXML(filename)){
		Com_Printf("Team 'team%i' loaded from xml.\n", index);
		return;
	}
	/* load */
	Com_sprintf(filename, sizeof(filename), "%s/save/team%i.mpt", FS_Gamedir(), index);
	MP_LoadTeamMultiplayer(filename);

	Com_Printf("Team 'team%i' loaded.\n", index);
}

/**
 * @brief Get the equipment definition (from script files) for the current selected multiplayer team
 * and updates the equipment inventory containers
 * @todo Multiplayer is currently using the ccs.eMission - this should be
 * changed - ccs is campaign mode only
 */
static void MP_GetEquipment (void)
{
	const equipDef_t *ed;
	const char *teamID = Com_ValueToStr(&cl_team->integer, V_TEAM, 0);
	char equipmentName[MAX_VAR];
	equipDef_t unused;

	Com_sprintf(equipmentName, sizeof(equipmentName), "multiplayer_%s", teamID);

	/* search equipment definition */
	ed = INV_GetEquipmentDefinitionByID(equipmentName);
	if (ed == NULL) {
		Com_Printf("Equipment '%s' not found!\n", equipmentName);
		return;
	}

	if (chrDisplayList.num > 0)
		menuInventory = &chrDisplayList.chr[0]->inv;
	else
		menuInventory = NULL;

	ccs.eMission = *ed;

	/* manage inventory */
	unused = ccs.eMission;
	MN_ContainerNodeUpdateEquipment(&mp_inventory, &unused);
}

/**
 * @brief Displays actor info and equipment and unused items in proper (filter) category.
 * @note This function is called everytime the multiplayer equipment screen for the team pops up.
 */
void MP_UpdateMenuParameters_f (void)
{
	int i;

	/* reset description */
	Cvar_Set("mn_itemname", "");
	Cvar_Set("mn_item", "");
	MN_MenuTextReset(TEXT_STANDARD);

	for (i = 0; i < MAX_MULTIPLAYER_CHARACTERS; i++) {
		const char *name;
		if (i < chrDisplayList.num)
			name = chrDisplayList.chr[i]->name;
		else
			name = "";
		Cvar_ForceSet(va("mn_name%i", i), name);
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

	MN_ExecuteConfunc("mp_soldier_select %i", num);
	MN_ExecuteConfunc("mp_soldier_unselect %i", old);
	Cmd_ExecuteString(va("equip_select %i", num));
}
