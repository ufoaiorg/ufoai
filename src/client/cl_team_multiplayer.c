/**
 * @file cl_team_multiplayer.c
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

#include "client.h"
#include "cl_global.h"
#include "cl_team.h"
#include "cl_team_multiplayer.h"
#include "menu/m_popup.h"

/**
 * @brief Reads tha comments from team files
 */
static void CL_MultiplayerTeamSlotComments_f (void)
{
	char comment[MAX_VAR];
	FILE *f;
	int i;
	byte version;

	for (i = 0; i < 8; i++) {
		/* open file */
		f = fopen(va("%s/save/team%i.mpt", FS_Gamedir(), i), "rb");
		if (!f) {
			Cvar_Set(va("mn_slot%i", i), "");
			continue;
		}

		/* read data */
		if (fread(&version, 1, 1, f) != 1)
			Com_Printf("Warning: Teamfile may have corrupted version\n");

		if (fread(comment, 1, MAX_VAR, f) != MAX_VAR)
			Com_Printf("Warning: Teamfile may have corrupted comment\n");
		Cvar_Set(va("mn_slot%i", i), comment);
		fclose(f);
	}
}


/**
 * @brief Multiplayer-only call to reset team and team inventory (when setting up new team).
 * @sa E_ResetEmployees
 * @sa CL_CleanTempInventory
 * @note Available via script command team_reset.
 * @note Called when initializing the multiplayer menu (for init node and new team button).
 */
void CL_ResetMultiplayerTeamInAircraft (aircraft_t *aircraft)
{
	if (!GAME_IsMultiplayer())
		return;

	if (!aircraft) {
		Com_DPrintf(DEBUG_CLIENT, "CL_ResetMultiplayerTeamInAircraft: No aircraft given\n");
		return;
	}

	CL_CleanTempInventory(NULL);

	AIR_ResetAircraftTeam(aircraft);

	E_ResetEmployees();
	while (gd.numEmployees[EMPL_SOLDIER] < cl_numnames->integer) {
		employee_t *employee = E_CreateEmployee(EMPL_SOLDIER, NULL, NULL);
		employee->hired = qtrue;
		Com_DPrintf(DEBUG_CLIENT, "CL_ResetMultiplayerTeamInAircraft: Generate character for multiplayer - employee->chr.name: '%s'\n", employee->chr.name);
	}

	/* reset the multiplayer inventory; stored in ccs.eMission */
	{
		const equipDef_t *ed;
		const char *name = "multiplayer";

		/* search equipment definition */
		Com_DPrintf(DEBUG_CLIENT, "CL_ResetMultiplayerTeamInAircraft: no curCampaign - using equipment '%s'\n", name);
		ed = INV_GetEquipmentDefinitionByID(name);
		if (ed == NULL) {
			Com_Printf("Equipment '%s' not found!\n", name);
			return;
		}
		ccs.eMission = *ed; /* copied, including the arrays inside! */
	}
}

static void CL_ResetMultiplayerTeamInAircraft_f (void)
{
	CL_ResetMultiplayerTeamInAircraft(cls.missionaircraft);
}

/**
 * @brief Call all the needed functions to generate a new initial team for multiplayer
 */
static void CL_GenerateNewMultiplayerTeam_f (void)
{
	if (!GAME_IsMultiplayer()) {
		Com_Printf("%s should only be called for multiplayer games\n", Cmd_Argv(0));
		return;
	}
	CL_ResetMultiplayerTeamInAircraft(cls.missionaircraft);
	Cvar_Set("mn_teamname", _("NewTeam"));
	CL_GameExit();
	MN_PushMenu("team", NULL);
}

/**
 * @brief Stores the wholeTeam info to buffer (which might be a network buffer, too)
 * @note Called by CL_SaveTeamMultiplayer to store the team info
 * @sa CL_SendCurTeamInfo
 * @sa CL_LoadTeamMultiplayerMember
 */
static void CL_SaveTeamMultiplayerInfo (sizebuf_t *buf, const employeeType_t type)
{
	linkedList_t *hiredEmployees = NULL;
	linkedList_t *hiredEmployeesTemp;
	int i, j, num;

	/* clean temp inventory */
	CL_CleanTempInventory(NULL);

	num = gd.numEmployees[type];

	/* header */
	MSG_WriteByte(buf, num);
	for (i = 0; i < num; i++) {
		const employee_t *employee = &gd.employees[type][i];

		/* send the fieldSize ACTOR_SIZE_* */
		MSG_WriteByte(buf, employee->chr.fieldSize);

		/* unique character number */
		MSG_WriteShort(buf, employee->chr.ucn);

		/* name */
		MSG_WriteString(buf, employee->chr.name);

		/* model */
		MSG_WriteString(buf, employee->chr.path);
		MSG_WriteString(buf, employee->chr.body);
		MSG_WriteString(buf, employee->chr.head);
		MSG_WriteByte(buf, employee->chr.skin);

		MSG_WriteShort(buf, employee->chr.HP);
		MSG_WriteShort(buf, employee->chr.maxHP);
		MSG_WriteByte(buf, employee->chr.teamDef ? employee->chr.teamDef->idx : BYTES_NONE);
		MSG_WriteByte(buf, employee->chr.gender);
		MSG_WriteByte(buf, employee->chr.STUN);
		MSG_WriteByte(buf, employee->chr.morale);

		/** Scores @sa inv_shared.h:chrScoreGlobal_t */
		MSG_WriteByte(buf, SKILL_NUM_TYPES + 1);
		for (j = 0; j < SKILL_NUM_TYPES + 1; j++)
			MSG_WriteLong(buf, employee->chr.score.experience[j]);
		MSG_WriteByte(buf, SKILL_NUM_TYPES);
		for (j = 0; j < SKILL_NUM_TYPES; j++)	/* even new attributes */
			MSG_WriteByte(buf, employee->chr.score.skills[j]);
		MSG_WriteByte(buf, SKILL_NUM_TYPES + 1);
		for (j = 0; j < SKILL_NUM_TYPES + 1; j++)
			MSG_WriteByte(buf, employee->chr.score.initialSkills[j]);
		MSG_WriteByte(buf, KILLED_NUM_TYPES);
		for (j = 0; j < KILLED_NUM_TYPES; j++)
			MSG_WriteShort(buf, employee->chr.score.kills[j]);
		MSG_WriteByte(buf, KILLED_NUM_TYPES);
		for (j = 0; j < KILLED_NUM_TYPES; j++)
			MSG_WriteShort(buf, employee->chr.score.stuns[j]);
		MSG_WriteShort(buf, employee->chr.score.assignedMissions);
		MSG_WriteShort(buf, employee->chr.score.rank);

		/* Save user-defined (default) reaction-state. */
		MSG_WriteShort(buf, employee->chr.reservedTus.reserveReaction);

		/* inventory */
		CL_SaveInventory(buf, employee->chr.inv);

		hiredEmployeesTemp = hiredEmployeesTemp->next;
	}

	LIST_Delete(&hiredEmployees);
}

/**
 * @brief Saves a multiplayer team
 * @sa CL_SaveTeamMultiplayerInfo
 * @sa CL_LoadTeamMultiplayer
 * @todo Implement EMPL_ROBOT
 */
static qboolean CL_SaveTeamMultiplayer (const char *filename)
{
	sizebuf_t sb;
	byte buf[MAX_TEAMDATASIZE];
	const char *name;
	int i, res;
	aircraft_t *aircraft = cls.missionaircraft;

	/* create data */
	SZ_Init(&sb, buf, MAX_TEAMDATASIZE);
	MSG_WriteByte(&sb, MPTEAM_SAVE_FILE_VERSION);

	name = Cvar_VariableString("mn_teamname");
	if (!strlen(name))
		Cvar_Set("mn_teamname", _("NewTeam"));
	/* store teamname */
	MSG_WriteString(&sb, name);

	/* store team */
	CL_SaveTeamMultiplayerInfo(&sb, EMPL_SOLDIER);

	/* store assignment */
	MSG_WriteByte(&sb, aircraft->teamSize);

	/* store aircraft soldier content for multi-player */
	MSG_WriteByte(&sb, aircraft->maxTeamSize);
	for (i = 0; i < aircraft->maxTeamSize; i++) {
		/* We save them all, even unused ones (->size). */
		if (aircraft->acTeam[i]) {
			MSG_WriteShort(&sb, aircraft->acTeam[i]->idx);
			MSG_WriteShort(&sb, aircraft->acTeam[i]->type);
		} else {
			MSG_WriteShort(&sb, -1);
			/** @todo use the presave value here - same for reading */
			MSG_WriteShort(&sb, MAX_EMPL);
		}
	}

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
static void CL_SaveTeamMultiplayerSlot_f (void)
{
	if (!GAME_IsMultiplayer())
		return;

	if (!gd.numEmployees[EMPL_SOLDIER]) {
		MN_Popup(_("Note"), _("Error saving team. Nothing to save yet."));
		return;
	} else if (cls.missionaircraft) {
		char filename[MAX_OSPATH];
		/* save */
		Com_sprintf(filename, sizeof(filename), "%s/save/team%s.mpt", FS_Gamedir(), Cvar_VariableString("mn_slot"));
		if (!CL_SaveTeamMultiplayer(filename))
			MN_Popup(_("Note"), _("Error saving team. Check free disk space!"));
	} else {
		Com_Printf("Nothing to safe - no team assigned to the aircraft\n");
	}
}

/**
 * @brief Load a team member for multiplayer
 * @sa CL_LoadTeamMultiplayer
 * @sa CL_SaveTeamMultiplayerInfo
 */
static void CL_LoadTeamMultiplayerMember (sizebuf_t * sb, character_t * chr, int version)
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
	INVSH_DestroyInventory(chr->inv);
	CL_LoadInventory(sb, chr->inv);
}

/**
 * @brief Load a multiplayer team
 * @sa CL_LoadTeamMultiplayer
 * @sa CL_SaveTeamMultiplayer
 * @todo only EMPL_SOLDIERs are saved and loaded
 */
static void CL_LoadTeamMultiplayer (const char *filename)
{
	sizebuf_t sb;
	byte buf[MAX_TEAMDATASIZE];
	FILE *f;
	int version;
	aircraft_t *aircraft;
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
	Com_DPrintf(DEBUG_CLIENT, "load %i teammembers\n", num);
	E_ResetEmployees();
	for (i = 0; i < num; i++) {
		/* New employee */
		employee_t *employee = E_CreateEmployee(EMPL_SOLDIER, NULL, NULL);
		employee->hired = qtrue;
		CL_LoadTeamMultiplayerMember(&sb, &employee->chr, version);
	}

	aircraft = cls.missionaircraft;
	AIR_ResetAircraftTeam(aircraft);

	/* get assignment */
	aircraft->teamSize = MSG_ReadByte(&sb);

	/* get aircraft soldier content for multi-player */
	aircraft->maxTeamSize = MSG_ReadByte(&sb);
	for (i = 0; i < aircraft->maxTeamSize; i++) {
		const int emplIdx = MSG_ReadShort(&sb);
		const employeeType_t emplType = MSG_ReadShort(&sb);
		if (emplIdx >= 0 && emplIdx < MAX_EMPLOYEES) {
			if (emplType != EMPL_SOLDIER && emplType != EMPL_ROBOT) {
				Com_Printf("Only soldiers and ugvs are allowed to be in an "
					"aircraft team, but %i should be added\n", emplType);
				return;
			}
			aircraft->acTeam[i] = &gd.employees[emplType][emplIdx];
		}
	}
	CL_UpdateActorAircraftVar(aircraft, EMPL_SOLDIER);
	CL_UpdateActorAircraftVar(aircraft, EMPL_ROBOT);

	/* read equipment */
	num = MSG_ReadShort(&sb);
	for (i = 0; i < num; i++) {
		const objDef_t *od = INVSH_GetItemByID(MSG_ReadString(&sb));
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
static void CL_LoadTeamMultiplayerSlot_f (void)
{
	char filename[MAX_OSPATH];

	if (!GAME_IsMultiplayer()) {
		Com_Printf("Only for multiplayer\n");
		return;
	}

	/* load */
	Com_sprintf(filename, sizeof(filename), "%s/save/team%s.mpt", FS_Gamedir(), Cvar_VariableString("mn_slot"));
	CL_LoadTeamMultiplayer(filename);

	Com_Printf("Team 'team%s' loaded.\n", Cvar_VariableString("mn_slot"));
}

void TEAM_MP_InitStartup (void)
{
	Cmd_AddCommand("saveteamslot", CL_SaveTeamMultiplayerSlot_f, "Save a multiplayer team slot - see cvar mn_slot");
	Cmd_AddCommand("loadteamslot", CL_LoadTeamMultiplayerSlot_f, "Load a multiplayer team slot - see cvar mn_slot");
	Cmd_AddCommand("team_comments", CL_MultiplayerTeamSlotComments_f, "Fills the multiplayer team selection menu with the team names");
	Cmd_AddCommand("team_reset", CL_ResetMultiplayerTeamInAircraft_f, NULL);
	Cmd_AddCommand("team_new", CL_GenerateNewMultiplayerTeam_f, "Generates a new empty multiplayer team");
}
