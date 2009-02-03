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
#include "mp_team.h"

static inventory_t mp_inventory;
character_t multiplayerCharacters[MAX_MULTIPLAYER_CHARACTERS];

/**
 * @brief Reads tha comments from team files
 */
void MP_MultiplayerTeamSlotComments_f (void)
{
	int i;

	for (i = 0; i < 8; i++) {
		byte version;
		char comment[MAX_VAR];
		const size_t size = sizeof(comment);

		/* open file */
		FILE *f = fopen(va("%s/save/team%i.mpt", FS_Gamedir(), i), "rb");
		if (!f) {
			Cvar_Set(va("mn_slot%i", i), "");
			continue;
		}

		/* read data */
		if (fread(&version, 1, 1, f) != 1)
			Com_Printf("Warning: Teamfile may have corrupted version\n");

		if (fread(comment, 1, size, f) != size)
			Com_Printf("Warning: Teamfile may have corrupted comment\n");
		Cvar_Set(va("mn_slot%i", i), comment);
		fclose(f);
	}
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

		/** Scores @sa inv_shared.h:chrScoreGlobal_t */
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
 * @brief Saves a multiplayer team
 * @sa MP_SaveTeamMultiplayerInfo
 * @sa MP_LoadTeamMultiplayer
 * @todo Implement EMPL_ROBOT
 */
static qboolean MP_SaveTeamMultiplayer (const char *filename)
{
	sizebuf_t sb;
	byte buf[MAX_TEAMDATASIZE];
	const char *name;
	int i, res;

	/* create data */
	SZ_Init(&sb, buf, MAX_TEAMDATASIZE);
	MSG_WriteByte(&sb, MPTEAM_SAVE_FILE_VERSION);

	name = Cvar_VariableString("mn_teamname");
	if (!strlen(name))
		Cvar_Set("mn_teamname", _("NewTeam"));
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
void MP_SaveTeamMultiplayerSlot_f (void)
{
	if (!chrDisplayList.num) {
		MN_Popup(_("Note"), _("Error saving team. Nothing to save yet."));
		return;
	} else {
		char filename[MAX_OSPATH];
		/* save */
		Com_sprintf(filename, sizeof(filename), "%s/save/team%s.mpt", FS_Gamedir(), Cvar_VariableString("mn_slot"));
		if (!MP_SaveTeamMultiplayer(filename))
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
	int td; /**< Team-definition index. */

	/* unique character number */
	chr->fieldSize = MSG_ReadByte(sb);
	chr->ucn = MSG_ReadShort(sb);
	if (chr->ucn >= gd.nextUCN)
		gd.nextUCN = chr->ucn + 1;

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

	/** Load scores @sa inv_shared.h:chrScoreGlobal_t */
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
	INVSH_DestroyInventory(&chr->inv);
	CL_LoadInventory(sb, &chr->inv);
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

	/* load the teamname */
	Cvar_Set("mn_teamname", MSG_ReadString(&sb));

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
void MP_LoadTeamMultiplayerSlot_f (void)
{
	char filename[MAX_OSPATH];

	/* load */
	Com_sprintf(filename, sizeof(filename), "%s/save/team%s.mpt", FS_Gamedir(), Cvar_VariableString("mn_slot"));
	MP_LoadTeamMultiplayer(filename);

	Com_Printf("Team 'team%s' loaded.\n", Cvar_VariableString("mn_slot"));
}


/**
 * @brief Displays actor info and equipment and unused items in proper (filter) category.
 * @note This function is called everytime the multiplayer equipment screen for the team pops up.
 */
void MP_UpdateMenuParameters_f (void)
{
	equipDef_t unused;
	int i;

	/* reset description */
	Cvar_Set("mn_itemname", "");
	Cvar_Set("mn_item", "");
	MN_MenuTextReset(TEXT_STANDARD);

	/* manage inventory */
	unused = ccs.eMission; /* copied, including arrays inside! */
	for (i = 0; i < MAX_MULTIPLAYER_CHARACTERS; i++) {
		const char *name;
		if (i < chrDisplayList.num)
			name = chrDisplayList.chr[i]->name;
		else
			name = "";
		Cvar_ForceSet(va("mn_name%i", i), name);
	}

	for (i = 0; i < csi.numODs; i++) {
		/* Don't allow to show armour for other teams in the menu. */
		if (!Q_strcmp(csi.ods[i].type, "armour") && csi.ods[i].useable != cl_team->integer)
			continue;

		while (unused.num[i]) {
			const item_t item = {NONE_AMMO, NULL, &csi.ods[i], 0, 0};
			inventory_t *i = &mp_inventory;
			if (!Com_AddToInventory(i, CL_AddWeaponAmmo(&unused, item), &csi.ids[csi.idEquip], NONE, NONE, 1))
				break; /* no space left in menu */
		}
	}
}
