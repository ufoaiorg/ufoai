/**
 * @file cl_team.c
 * @brief Team management, name generation and parsing.
 */

/*
Copyright (C) 2002-2006 UFO: Alien Invasion team.

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

*/

#include "client.h"
#include "cl_global.h"

char *teamSkinNames[NUM_TEAMSKINS] = {
	"Urban",
	"Jungle",
	"Desert",
	"Arctic"
};

/**
  * @brief Test the names in team_*.ufo
  *
  * This is a console command to test the names that were defined in team_*.ufo
  * Usage: givename <gender> <category> [num]
  * valid genders are male, female, neutral
  */
static void CL_GiveNameCmd(void)
{
	char *name;
	int i, j, num;

	if (Cmd_Argc() < 3) {
		Com_Printf("Usage: givename <gender> <category> [num]\n");
		return;
	}

	/* get gender */
	for (i = 0; i < NAME_LAST; i++)
		if (!Q_strcmp(Cmd_Argv(1), name_strings[i]))
			break;

	if (i == NAME_LAST) {
		Com_Printf("'%s' isn't a gender! (male and female are)\n", Cmd_Argv(1));
		return;
	}

	if (Cmd_Argc() > 3) {
		num = atoi(Cmd_Argv(3));
		if (num < 1)
			num = 1;
		if (num > 20)
			num = 20;
	} else
		num = 1;

	for (j = 0; j < num; j++) {
		/* get first name */
		name = Com_GiveName(i, Cmd_Argv(2));
		if (!name) {
			Com_Printf("No first name in category '%s'\n", Cmd_Argv(2));
			return;
		}
		Com_Printf(name);

		/* get last name */
		name = Com_GiveName(i + LASTNAME, Cmd_Argv(2));
		if (!name) {
			Com_Printf("\nNo last name in category '%s'\n", Cmd_Argv(2));
			return;
		}

		/* print out name */
		Com_Printf(" %s\n", name);
	}
}

/**
  * @brief Generates the skills and inventory for a character and for a UGV
  *
  * TODO: Generate UGV
  * @sa CL_ResetCharacters
  */
void CL_GenerateCharacter(char *team, base_t *base, uint8_t type)
{
	character_t *chr;

	/* check for too many characters */
	if (base->numWholeTeam >= (int) cl_numnames->value)
		return;
#ifdef DEBUG
	if (base->numWholeTeam >= MAX_WHOLETEAM) {
		Sys_Error("numWholeTeam (%i) is bigger than the allowed maximum (%i) - this is a prospectiv overflow\n", base->numWholeTeam, MAX_WHOLETEAM);
		return; /* for code analysts - never reached */
	}
#endif

	/* reset character */
	chr = &base->wholeTeam[base->numWholeTeam];
	memset(chr, 0, sizeof(character_t));

	/* link inventory */
	chr->inv = &base->teamInv[base->numWholeTeam];
	Com_DestroyInventory(chr->inv);

	/* get ucn */
	chr->ucn = base->nextUCN++;

	/* set the actor size */
	switch ( type ) {
	case ET_ACTOR:
		chr->fieldSize = ACTOR_SIZE_NORMAL;
		chr->rank = 0;
		break;
	case ET_UGV:
		chr->fieldSize = ACTOR_SIZE_UGV;
		/* UGV does not have a rank */
		chr->rank = -1;
		break;
	default:
		Sys_Error("CL_GenerateCharacter: Unknown character type (%i)\n", type);
	}

	/* new attributes */
	Com_CharGenAbilitySkills(chr, 15, 75, 15, 75);

	/* get model and name */
	chr->skin = Com_GetModelAndName(team, chr->path, chr->body, chr->head, chr->name);
	Cvar_ForceSet(va("mn_name%i", base->numWholeTeam), chr->name);

	base->numWholeTeam++;
}


/**
  * @brief
  * @sa CL_GenerateCharacter
  */
void CL_ResetCharacters(base_t * base)
{
	int i;

	/* reset inventory data */
	for (i = 0; i < MAX_WHOLETEAM; i++) {
		(base->teamInv[i]).c[csi.idFloor] = NULL;
		Com_DestroyInventory(&base->teamInv[i]);
/*		base->teamInv[i].c[csi.idFloor] = base->equipment; */
		base->wholeTeam[i].inv = &base->teamInv[i];
	}

	/* reset hire info */
	Cvar_ForceSet("cl_selected", "0");
	base->numWholeTeam = 0;
	base->numHired = 0;
	for (i = 0; i < base->numAircraftInBase; i++)
		base->teamMask[i] = base->numOnTeam[i] = 0;
}


/**
  * @brief
  */
static void CL_GenerateNamesCmd(void)
{
	Cbuf_AddText("disconnect\ngame_exit\n");
}


/**
  * @brief Change the name of the selected actor
  */
static void CL_ChangeNameCmd(void)
{
	int sel;

	sel = cl_selected->value;

	/* maybe called without base initialized or active */
	if (!baseCurrent)
		return;

	if (sel >= 0 && sel < baseCurrent->numWholeTeam)
		Q_strncpyz(baseCurrent->wholeTeam[sel].name, Cvar_VariableString("mn_name"), MAX_VAR);
}


/**
  * @brief Change the skin of the selected actor
  */
static void CL_ChangeSkinCmd(void)
{
	int sel, newSkin;

	sel = cl_selected->value;
	if (sel >= 0 && sel < baseCurrent->numWholeTeam) {
		newSkin = (int) Cvar_VariableValue("mn_skin") + 1;
		if (newSkin >= NUM_TEAMSKINS || newSkin < 0)
			newSkin = 0;

		baseCurrent->curTeam[sel]->skin = newSkin;

		Cvar_SetValue("mn_skin", newSkin);
		Cvar_Set("mn_skinname", _(teamSkinNames[newSkin]));
	}
}

/**
  * @brief Reads tha comments from team files
  */
static void CL_TeamCommentsCmd(void)
{
	char comment[MAX_VAR];
	FILE *f;
	int i;

	for (i = 0; i < 8; i++) {
		/* open file */
		f = fopen(va("%s/save/team%i.mpt", FS_Gamedir(), i), "rb");
		if (!f) {
			Cvar_Set(va("mn_slot%i", i), "");
			continue;
		}

		/* read data */
		fread(comment, 1, MAX_VAR, f);
		Cvar_Set(va("mn_slot%i", i), comment);
		fclose(f);
	}
}

/**
  * @brief
  */
item_t CL_AddWeaponAmmo(equipDef_t * ed, int type)
{
	item_t item;
	int i;

	item.a = 0;
	item.m = NONE;
	item.t = NONE;
	if (ed->num[type] <= 0)
		return item;

	ed->num[type]--;
	item.t = type;

	if (!csi.ods[type].reload) {
		item.a = csi.ods[type].ammo;
		item.m = type;
		return item;
	}

	item.a = 0;
	item.m = NONE;
	/* Search for any complete clips */
	for (i = 0; i < csi.numODs; i++) {
		if (csi.ods[i].link == type) {
			item.m = i;
			if (ed->num[i] > 0) {
				ed->num[i]--;
				item.a = csi.ods[type].ammo;
				return item;
			}
		}
	}
	/* Failed to find a complete clip - see if there's any loose ammo */
	for (i = 0; i < csi.numODs; i++) {
		if (csi.ods[i].link == type && ed->num_loose[i] > 0) {
			if (item.m != NONE && ed->num_loose[i] > item.a) {
				/* We previously found some ammo, but we've now found other */
				/* loose ammo of a different (but appropriate) type with */
				/* more bullets.  Put the previously found ammo back, so */
				/* we'll take the new type. */
				ed->num_loose[item.m] = item.a;
				item.m = NONE;
			}
			if (item.m == NONE) {
				/* Found some loose ammo to load the weapon with */
				item.a = ed->num_loose[i];
				ed->num_loose[i] = 0;
				item.m = i;
			}
		}
	}
	return item;
}


/**
  * @brief
  */
void CL_CheckInventory(equipDef_t * equip)
{
	character_t *cp;
	invList_t *ic, *next;
	int p, container;

	assert(baseCurrent);

	/* Iterate through in container order (right hand, left hand, belt, */
	/* armor, backpack) at the top level, i.e. each squad member fills */
	/* their right hand, then each fills their left hand, etc.  The effect */
	/* of this is that when things are tight, everyone has the opportunity */
	/* to get their preferred weapon(s) loaded and in their hands before */
	/* anyone fills their backpack with spares.  We don't want the first */
	/* person in the squad filling their backpack with spare ammo leaving */
	/* others with unloaded guns in their hands. */
	Com_DPrintf("NumOnTeam in aircraft %i: %i\n", baseCurrent->aircraftCurrent, baseCurrent->numOnTeam[baseCurrent->aircraftCurrent] );
	for (container = 0; container < csi.numIDs; container++) {
		for (p = 0, cp = baseCurrent->curTeam[0]; p < baseCurrent->numOnTeam[baseCurrent->aircraftCurrent]; p++, cp++) {
			for (ic = cp->inv->c[container]; ic; ic = next) {
				next = ic->next;
				if (equip->num[ic->item.t] > 0)
					ic->item = CL_AddWeaponAmmo(equip, ic->item.t);
				else
					Com_RemoveFromInventory(cp->inv, container, ic->x, ic->y);
			}
		}
	}
}

/**
  * @brief
  */
void CL_CleanTempInventory(void)
{
	int i, k;

	if (!baseCurrent)
		return;

	Com_DestroyInventory(&baseCurrent->equipment);
	for (i = 0; i < MAX_WHOLETEAM; i++)
		for (k = 0; k < csi.numIDs; k++)
			if (k == csi.idEquip)
				baseCurrent->teamInv[i].c[k] = NULL;
			/* idFloor and idEquip are temp */
			else if (csi.ids[k].temp)
				Com_EmptyContainer(&baseCurrent->teamInv[i], k);
}

/**
  * @brief
  *
  * This function is called everytime the equipment screen for the team pops up
  * TODO: Make this faster
  */
static void CL_GenerateEquipmentCmd(void)
{
	equipDef_t *ed;
	equipDef_t unused;
	char *name;
	int i, p;
	int x, y;

	assert(baseCurrent);

	if (!baseCurrent->numHired) {
		MN_PopMenu(false);
		return;
	}

	/* clean equipment */
	CL_CleanTempInventory();

	/* store hired names */
	Cvar_ForceSet("cl_selected", "0");
	for (i = 0, p = 0; i < baseCurrent->numWholeTeam; i++)
		if (baseCurrent->teamMask[baseCurrent->aircraftCurrent] & (1 << i)) {
			/* maybe we already have soldiers in this base */
			baseCurrent->curTeam[p] = &baseCurrent->wholeTeam[i];
			Com_DPrintf("add %s to curTeam (pos: %i)\n", baseCurrent->curTeam[p]->name, p);
			Cvar_ForceSet(va("mn_name%i", p), baseCurrent->curTeam[p]->name);
			p++;
		}

	if ( p != baseCurrent->numOnTeam[baseCurrent->aircraftCurrent])
		Sys_Error("CL_GenerateEquipmentCmd: numWholeTeam: %i, numOnTeam[%i]: %i, p: %i, mask %i\n",baseCurrent->numWholeTeam, baseCurrent->aircraftCurrent, baseCurrent->numOnTeam[baseCurrent->aircraftCurrent], p, baseCurrent->teamMask[baseCurrent->aircraftCurrent]);

	for (; p < MAX_ACTIVETEAM; p++) {
		Cvar_ForceSet(va("mn_name%i", p), "");
		Cbuf_AddText(va("equipdisable%i\n", p));
		baseCurrent->curTeam[p] = NULL;
	}

	menuInventory = &baseCurrent->teamInv[0];
	selActor = NULL;

	/* reset description */
	Cvar_Set("mn_itemname", "");
	Cvar_Set("mn_item", "");
	Cvar_Set("mn_weapon", "");
	Cvar_Set("mn_ammo", "");
	menuText[TEXT_STANDARD] = NULL;

	if (!curCampaign) {
		/* search equipment definition */
		name = Cvar_VariableString("equip");
		Com_DPrintf("CL_GenerateEquipmentCmd: no curCampaign - using cvar equip '%s'\n", name);
		for (i = 0, ed = csi.eds; i < csi.numEDs; i++, ed++) {
			if (!Q_strncmp(name, ed->name, MAX_VAR))
				break;
		}
		if (i == csi.numEDs) {
			Com_Printf("Equipment '%s' not found!\n", name);
			return;
		}
		unused = *ed;
	} else
		unused = ccs.eCampaign;

	/* manage inventory */
	CL_CheckInventory(&unused);

	for (i = 0; i < csi.numODs; i++)
		while (unused.num[i]) {
			/* 'tiny hack' to add the equipment correctly into buy categories */
			baseCurrent->equipment.c[csi.idEquip] = baseCurrent->equipment.c[csi.ods[i].buytype];

			Com_FindSpace(&baseCurrent->equipment, i, csi.idEquip, &x, &y);
			if (x >= 32 && y >= 16)
				break;
			Com_AddToInventory(&baseCurrent->equipment, CL_AddWeaponAmmo(&unused, i), csi.idEquip, x, y);

			baseCurrent->equipment.c[csi.ods[i].buytype] = baseCurrent->equipment.c[csi.idEquip];
		}

	baseCurrent->equipment.c[csi.idEquip] = NULL;
}


/**
  * @brief
  */
static void CL_EquipTypeCmd(void)
{
	int num;

	if (Cmd_Argc() < 2) {
		Com_Printf("Usage: equip_type <category>\n");
		return;
	}

	/* read and range check */
	num = atoi(Cmd_Argv(1));
	if (num < 0 && num >= NUM_BUYTYPES)
		return;

	/* display new items */
	baseCurrent->equipType = num;
	if (menuInventory)
		menuInventory->c[csi.idEquip] = baseCurrent->equipment.c[num];
}

/**
  * @brief
  */
static void CL_SelectCmd(void)
{
	char *command;
	int num;

	/* check syntax */
	if (Cmd_Argc() < 2) {
		Com_Printf("Usage: team_select <num>\n");
		return;
	}
	num = atoi(Cmd_Argv(1));

	/* change highlights */
	command = Cmd_Argv(0);
	*strchr(command, '_') = 0;

	if (!Q_strncmp(command, "equip", 5)) {
		if (baseCurrent->numOnTeam[baseCurrent->aircraftCurrent] <= 0)
			return;
		if (!baseCurrent || baseCurrent->aircraftCurrent < 0 || num >= baseCurrent->numOnTeam[baseCurrent->aircraftCurrent])
			return;
		if (menuInventory && menuInventory != baseCurrent->curTeam[num]->inv) {
			baseCurrent->curTeam[num]->inv->c[csi.idEquip] = menuInventory->c[csi.idEquip];
			/* set 'old' idEquip to NULL */
			menuInventory->c[csi.idEquip] = NULL;
		}
		menuInventory = baseCurrent->curTeam[num]->inv;
	} else if (!Q_strncmp(command, "team", 4)) {
		if (!baseCurrent || num >= baseCurrent->numWholeTeam)
			return;
	} else if (!Q_strncmp(command, "hud", 3)) {
		CL_ActorSelectList(num);
		return;
	}

	/* console commands */
	Cbuf_AddText(va("%sdeselect%i\n", command, (int) cl_selected->value));
	Cbuf_AddText(va("%sselect%i\n", command, num));
	Cvar_ForceSet("cl_selected", va("%i", num));

	/* set info cvars */
	if (!Q_strncmp(command, "team", 4)) {
		if ( baseCurrent->wholeTeam[num].fieldSize == ACTOR_SIZE_NORMAL )
			CL_CharacterCvars(&baseCurrent->wholeTeam[num]);
		else
			CL_UGVCvars(&baseCurrent->wholeTeam[num]);
	} else {
		if ( baseCurrent->curTeam[num]->fieldSize == ACTOR_SIZE_NORMAL )
			CL_CharacterCvars(baseCurrent->curTeam[num]);
		else
			CL_UGVCvars(baseCurrent->curTeam[num]);
	}
}

/**
  * @brief
  */
void CL_UpdateHireVar(void)
{
	int i, p;
	aircraft_t *air = NULL;

	assert(baseCurrent);

	air = &baseCurrent->aircraft[baseCurrent->aircraftCurrent];
	Cvar_Set("mn_hired", va(_("%i of %i"), baseCurrent->numOnTeam[baseCurrent->aircraftCurrent], air->size));
	/* now update the mask for display the hired soldiers */
	baseCurrent->hiredMask = baseCurrent->teamMask[baseCurrent->aircraftCurrent];

	/* update curTeam list */
	for (i = 0, p = 0; i < baseCurrent->numWholeTeam; i++)
		if (baseCurrent->teamMask[baseCurrent->aircraftCurrent] & (1 << i)) {
			baseCurrent->curTeam[p] = &baseCurrent->wholeTeam[i];
			p++;
		}

	if ( p != baseCurrent->numOnTeam[baseCurrent->aircraftCurrent])
		Sys_Error("CL_UpdateHireVar: numWholeTeam: %i, numOnTeam[%i]: %i, p: %i, mask %i\n",baseCurrent->numWholeTeam, baseCurrent->aircraftCurrent, baseCurrent->numOnTeam[baseCurrent->aircraftCurrent], p, baseCurrent->teamMask[baseCurrent->aircraftCurrent]);

	for (; p<MAX_ACTIVETEAM; p++)
		baseCurrent->curTeam[p] = NULL;
}

/**
  * @brief
  *
  * only for multiplayer when setting up a new team
  */
void CL_ResetTeamInBase(void)
{
	if (ccs.singleplayer)
		return;

	Com_DPrintf("Reset of baseCurrent team flags like hiredMask\n");
	if (!baseCurrent) {
		Com_DPrintf("CL_ResetTeamInBase: No baseCurrent\n");
		return;
	}

	CL_CleanTempInventory();
	baseCurrent->teamMask[0] = baseCurrent->hiredMask = baseCurrent->numOnTeam[0] = 0;
}

/**
  * @brief Init the teamlist checkboxes
  */
static void CL_MarkTeamCmd(void)
{
	int i;

	/* check if we are allowed to be here? */
	/* we are only allowed to be here if we already set up a base */
	if (!baseCurrent) {
		Com_Printf("No base set up\n");
		MN_PopMenu(false);
		return;
	}

	CL_UpdateHireVar();

	for (i = 0; i < baseCurrent->numWholeTeam; i++) {
		Cvar_ForceSet(va("mn_name%i", i), baseCurrent->wholeTeam[i].name);
		if (baseCurrent->hiredMask & (1 << i))
			Cbuf_AddText(va("listadd%i\n", i));
	}

	for (i = baseCurrent->numWholeTeam; i < (int) cl_numnames->value; i++) {
		Cbuf_AddText(va("listdisable%i\n", i));
		Cvar_ForceSet(va("mn_name%i", i), "");
	}
}


/**
  * @brief Hires an actor or drop an actor
  */
static void CL_HireActorCmd(void)
{
	int num;
	aircraft_t *air = NULL;

	/* check syntax */
	if (Cmd_Argc() < 2) {
		Com_Printf("Usage: hire <num>\n");
		return;
	}
	num = atoi(Cmd_Argv(1));

	if (num >= baseCurrent->numWholeTeam)
		return;

	if (baseCurrent->teamMask[baseCurrent->aircraftCurrent] & (1 << num)) {
		/* unhire */
		Cbuf_AddText(va("listdel%i\n", num));
		baseCurrent->hiredMask &= ~(1 << num);
		baseCurrent->teamMask[baseCurrent->aircraftCurrent] &= ~(1 << num);
		baseCurrent->numHired--;
		baseCurrent->numOnTeam[baseCurrent->aircraftCurrent]--;
	} else if (baseCurrent->numOnTeam[baseCurrent->aircraftCurrent] < MAX_ACTIVETEAM && !(baseCurrent->hiredMask & (1 << num))) {
		if (baseCurrent && baseCurrent->aircraftCurrent >= 0)
			air = &baseCurrent->aircraft[baseCurrent->aircraftCurrent];
		/* hire */
		if (!air || air->size > baseCurrent->numOnTeam[baseCurrent->aircraftCurrent]) {
			Cbuf_AddText(va("listadd%i\n", num));
			baseCurrent->hiredMask |= (1 << num);
			baseCurrent->numHired++;
			baseCurrent->numOnTeam[baseCurrent->aircraftCurrent]++;
			baseCurrent->teamMask[baseCurrent->aircraftCurrent] |= (1 << num);
		}
	}
	/* select the desired one anyways */
	CL_UpdateHireVar();
	Cbuf_AddText(va("team_select %i\n", num));
}


/**
  * @brief Calls script function on cvar change
  *
  * This is for inline editing of cvar values
  * The cvarname_changed function are called,
  * the editing is activated and ended here
  *
  * Done by the script command msgmenu [?|!|:][cvarname]
  */
static char nameBackup[MAX_VAR];
static char cvarName[MAX_VAR];

static void CL_MessageMenuCmd(void)
{
	char *msg;

	if (Cmd_Argc() < 2) {
		Com_Printf("Usage: msgmenu <msg>\n");
		return;
	}

	msg = Cmd_Argv(1);
	if (msg[0] == '?') {
		/* start */
		Cbuf_AddText("messagemenu\n");
		Q_strncpyz(cvarName, msg + 1, MAX_VAR);
		Q_strncpyz(nameBackup, Cvar_VariableString(cvarName), MAX_VAR);
		Q_strncpyz(msg_buffer, nameBackup, sizeof(msg_buffer));
		msg_bufferlen = strlen(nameBackup);
	} else if (msg[0] == '!') {
		/* cancel */
		Cvar_ForceSet(cvarName, nameBackup);
		Cvar_ForceSet(va("%s%i", cvarName, (int) cl_selected->value), nameBackup);
		Cbuf_AddText(va("%s_changed\n", cvarName));
	} else if (msg[0] == ':') {
		/* end */
		Cvar_ForceSet(cvarName, msg + 1);
		Cvar_ForceSet(va("%s%i", cvarName, (int) cl_selected->value), msg + 1);
		Cbuf_AddText(va("%s_changed\n", cvarName));
	} else {
		/* continue */
		Cvar_ForceSet(cvarName, msg);
		Cvar_ForceSet(va("%s%i", cvarName, (int) cl_selected->value), msg);
	}
}

/**
  * @brief Saves a team
  */
void CL_SaveTeam(char *filename)
{
	sizebuf_t sb;
	uint8_t buf[MAX_TEAMDATASIZE];
	char *name;
	int res, i;

	assert(baseCurrent);

	/* create the save dir - if needed */
	FS_CreatePath(filename);

	/* create data */
	SZ_Init(&sb, buf, MAX_TEAMDATASIZE);

	name = Cvar_VariableString("mn_teamname");
	if (!strlen(name))
		Cvar_Set("mn_teamname", _("NewTeam"));
	/* store teamname */
	MSG_WriteString(&sb, name);

	/* store team */
	CL_SendTeamInfo(&sb, baseCurrent->wholeTeam, baseCurrent->numWholeTeam);

	/* store assignement */
	/* get assignement */
	MSG_WriteFormat(&sb, "bl", baseCurrent->numHired, baseCurrent->hiredMask);
	for (i = 0; i < baseCurrent->numAircraftInBase; i++)
		MSG_WriteFormat(&sb, "bl", baseCurrent->numOnTeam[i], baseCurrent->teamMask[i]);

	/* write data */
	res = FS_WriteFile(buf, sb.cursize, filename);

	if (res == sb.cursize)
		Com_Printf("Team '%s' saved.\n", filename);
	else if (!res)
		Com_Printf("Team '%s' not saved.\n", filename);
}


/**
  * @brief Saves a team from commandline
  *
  * Call CL_SaveTeam to store the team to a given filename
  */
static void CL_SaveTeamCmd(void)
{
	char filename[MAX_QPATH];

	if (Cmd_Argc() < 2) {
		Com_Printf("Usage: saveteam <filename>\n");
		return;
	}

	/* save */
	Com_sprintf(filename, MAX_QPATH, "%s/save/%s.mpt", FS_Gamedir(), Cmd_Argv(1));
	CL_SaveTeam(filename);
}


/**
  * @brief Stores a team in a specified teamslot
  */
static void CL_SaveTeamSlotCmd(void)
{
	char filename[MAX_QPATH];

	/* save */
	Com_sprintf(filename, MAX_QPATH, "%s/save/team%s.mpt", FS_Gamedir(), Cvar_VariableString("mn_slot"));
	CL_SaveTeam(filename);
}

/**
  * @brief Load team members
  */
void CL_LoadTeamMember(sizebuf_t * sb, character_t * chr)
{
	item_t item;
	uint8_t container, x, y;
	int i;

	/* unique character number */
	chr->fieldSize = MSG_ReadByte(sb, NULL);
	chr->ucn = MSG_ReadShort(sb, NULL);
	if (chr->ucn >= baseCurrent->nextUCN)
		baseCurrent->nextUCN = chr->ucn + 1;

	/* name and model */
	Q_strncpyz(chr->name, MSG_ReadString(sb, NULL), MAX_VAR);
	Q_strncpyz(chr->path, MSG_ReadString(sb, NULL), MAX_VAR);
	Q_strncpyz(chr->body, MSG_ReadString(sb, NULL), MAX_VAR);
	Q_strncpyz(chr->head, MSG_ReadString(sb, NULL), MAX_VAR);
	chr->skin = MSG_ReadByte(sb, NULL);

	/* new attributes */
	for (i = 0; i < SKILL_NUM_TYPES; i++)
		chr->skills[i] = MSG_ReadByte(sb, NULL);

	/* load scores */
	for (i = 0; i < KILLED_NUM_TYPES; i++)
		chr->kills[i] = MSG_ReadShort(sb, NULL);
	chr->assigned_missions = MSG_ReadShort(sb, NULL);

	/* inventory */
	Com_DestroyInventory(chr->inv);
	item.t = MSG_ReadByte(sb, NULL);
	while (item.t != NONE) {
		/* read info */
		item.a = MSG_ReadByte(sb, NULL);
		item.m = MSG_ReadByte(sb, NULL);
		container = MSG_ReadByte(sb, NULL);
		x = MSG_ReadByte(sb, NULL);
		y = MSG_ReadByte(sb, NULL);

		/* check info and add item if ok */
		Com_AddToInventory(chr->inv, item, container, x, y);

		/* get next item */
		item.t = MSG_ReadByte(sb, NULL);
	}
}


/**
  * @brief Loads a team
  *
  * for singleplayer loading the function is called directly
  * for multiplayer this function is called by CL_LoadTeamMultiplayer
  */
void CL_LoadTeam(sizebuf_t * sb, base_t * base, int version)
{
	character_t *chr;
	int i, p;

	/* reset data */
	CL_ResetCharacters(base);

	/* read whole team list */
	MSG_ReadByte(sb, NULL);
	base->numWholeTeam = MSG_ReadByte(sb, NULL);
	for (i = 0, chr = base->wholeTeam; i < base->numWholeTeam; chr++, i++)
		CL_LoadTeamMember(sb, chr);

	/* get assignement */
	MSG_ReadFormat(sb, NULL, "bl", &base->numHired, &base->hiredMask);
	for (i = 0; i < base->numAircraftInBase; i++)
		MSG_ReadFormat(sb, NULL, "bl", &base->numOnTeam[i], &base->teamMask[i]);

	Com_DPrintf("Load team with %i members and %i slots\n", base->numHired, base->numWholeTeam);

	for (i = 0, p = 0; i < base->numWholeTeam; i++)
		if (base->teamMask[base->aircraftCurrent] & (1 << i))
			base->curTeam[p++] = &base->wholeTeam[i];

	for (;p<MAX_ACTIVETEAM;p++)
		base->curTeam[p] = NULL;
}


/**
  * @brief Load a multiplayer team
  * @sa CL_LoadTeam
  * @sa CL_SaveTeam
  */
void CL_LoadTeamMultiplayer(char *filename)
{
	sizebuf_t sb;
	uint8_t buf[MAX_TEAMDATASIZE];
	FILE *f;
	char title[MAX_VAR];

	CL_ResetTeamInBase();

	/* return the base title */
	Q_strncpyz(title, gd.bases[0].name, MAX_VAR);
	B_ClearBase(&gd.bases[0]);
	Q_strncpyz(gd.bases[0].name, title, MAX_VAR);

	/* set base for multiplayer */
	baseCurrent = &gd.bases[0];
	gd.numBases = 1;
	baseCurrent->hiredMask = 0;
	CL_NewAircraft(baseCurrent, "craft_dropship");

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

	/* load the teamname */
	Cvar_Set("mn_teamname", MSG_ReadString(&sb, NULL));

	/* load the team */
	CL_LoadTeam(&sb, &gd.bases[0], SAVE_FILE_VERSION);
}


/**
  * @brief Loads a team from commandline
  */
static void CL_LoadTeamCmd(void)
{
	char filename[MAX_QPATH];

	if (Cmd_Argc() < 2) {
		Com_Printf("Usage: loadteam <filename>\n");
		return;
	}

	/* load */
	Com_sprintf(filename, MAX_QPATH, "%s/save/%s.mpt", FS_Gamedir(), Cmd_Argv(1));
	CL_LoadTeamMultiplayer(filename);

	Com_Printf("Team '%s' loaded.\n", Cmd_Argv(1));
}


/**
  * @brief Loads the selected teamslot
  */
static void CL_LoadTeamSlotCmd(void)
{
	char filename[MAX_QPATH];

	/* load */
	Com_sprintf(filename, MAX_QPATH, "%s/save/team%s.mpt", FS_Gamedir(), Cvar_VariableString("mn_slot"));
	CL_LoadTeamMultiplayer(filename);

	Com_Printf("Team 'team%s' loaded.\n", Cvar_VariableString("mn_slot"));
}

/**
  * @brief
  */
void CL_ResetTeams(void)
{
	Cmd_AddCommand("givename", CL_GiveNameCmd);
	Cmd_AddCommand("gennames", CL_GenerateNamesCmd);
	Cmd_AddCommand("team_reset", CL_ResetTeamInBase);
	Cmd_AddCommand("genequip", CL_GenerateEquipmentCmd);
	Cmd_AddCommand("equip_type", CL_EquipTypeCmd);
	Cmd_AddCommand("team_mark", CL_MarkTeamCmd);
	Cmd_AddCommand("team_hire", CL_HireActorCmd);
	Cmd_AddCommand("team_select", CL_SelectCmd);
	Cmd_AddCommand("team_changename", CL_ChangeNameCmd);
	Cmd_AddCommand("team_changeskin", CL_ChangeSkinCmd);
	Cmd_AddCommand("team_comments", CL_TeamCommentsCmd);
	Cmd_AddCommand("equip_select", CL_SelectCmd);
	Cmd_AddCommand("hud_select", CL_SelectCmd);
	Cmd_AddCommand("saveteam", CL_SaveTeamCmd);
	Cmd_AddCommand("saveteamslot", CL_SaveTeamSlotCmd);
	Cmd_AddCommand("loadteam", CL_LoadTeamCmd);
	Cmd_AddCommand("loadteamslot", CL_LoadTeamSlotCmd);
	Cmd_AddCommand("msgmenu", CL_MessageMenuCmd);
}

/**
  * @brief
  */
void CL_SendItem(sizebuf_t * buf, item_t item, int container, int x, int y)
{
	if (!csi.ods[item.t].reload) {
		/* transfer with full free ammo */
		item.m = item.t;
		item.a = csi.ods[item.t].ammo;
	}
	MSG_WriteFormat(buf, "bbbbbb", item.t, item.a, item.m, container, x, y);
}

/**
  * @brief Stores the team info to buffer (which might be a network buffer, too)
  *
  * Called in cl_main.c CL_Precache_f to send the team info to server
  * Called by CL_SaveTeam to store the team info
  */
void CL_SendTeamInfo(sizebuf_t * buf, character_t * team, int num)
{
	character_t *chr;
	invList_t *ic;
	int i, j;

	/* clean temp inventory */
	CL_CleanTempInventory();

	/* header */
	MSG_WriteByte(buf, clc_teaminfo);
	MSG_WriteByte(buf, num);

	for (i = 0, chr = team; i < num; chr++, i++) {
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

		/* even new attributes */
		for (j = 0; j < SKILL_NUM_TYPES; j++)
			MSG_WriteShort(buf, chr->skills[j]);

		/* scores */
		for (j = 0; j < KILLED_NUM_TYPES; j++)
			MSG_WriteShort(buf, chr->kills[j]);
		MSG_WriteShort(buf, chr->assigned_missions);

		/* equipment */
		for (j = 0; j < csi.numIDs; j++)
			for (ic = chr->inv->c[j]; ic; ic = ic->next)
				CL_SendItem(buf, ic->item, j, ic->x, ic->y);

		/* terminate list */
		MSG_WriteByte(buf, NONE);
	}
}

/**
  * @brief Parses the character data which was send by G_SendCharacterData
  */
void CL_ParseCharacterData(sizebuf_t *buf, bool_t updateCharacter)
{
	int ucn, i, j;
	int num = MSG_ReadShort(buf, NULL);
	character_t* chr;
	for (i=0; i<num; i++) {
		chr = NULL;
		ucn = MSG_ReadShort(buf, NULL);
		for (j=0; j<baseCurrent->numWholeTeam; j++)
			if (baseCurrent->wholeTeam[j].ucn == ucn) {
				chr = &baseCurrent->wholeTeam[j];
				break;
			}
		if (!chr)
			Sys_Error("Could not get character with ucn: %i\n", ucn);
		for (j=0; j<KILLED_NUM_TYPES; j++)
			if ( updateCharacter )
				chr->kills[j] = MSG_ReadShort(buf, NULL);
			else
				MSG_ReadShort(buf, NULL);
	}
}


/**
  * @brief
  * See EV_RESULTS
  * @sa G_EndGame
  * @sa CL_GameResultsCmd
  */
void CL_ParseResults(sizebuf_t * buf)
{
	static char resultText[MAX_MENUTEXTLEN];
	uint8_t num_spawned[MAX_TEAMS];
	uint8_t num_alive[MAX_TEAMS];
	uint8_t num_kills[MAX_TEAMS][MAX_TEAMS];
	uint8_t winner, we;
	int i, j, res, kills;
	uint8_t num;

	/* get number of teams */
	num = MSG_ReadByte(buf, NULL);
	if (num > MAX_TEAMS)
		Sys_Error("Too many teams in result message\n");

	/* get winning team */
	winner = MSG_ReadByte(buf, NULL);
	we = cls.team;

	/* get spawn and alive count */
	for (i = 0; i < num; i++) {
		num_spawned[i] = MSG_ReadByte(buf, NULL);
		num_alive[i] = MSG_ReadByte(buf, NULL);
	}

	/* get kills */
	for (i = 0; i < num; i++)
		for (j = 0; j < num; j++)
			num_kills[i][j] = MSG_ReadByte(buf, NULL);

	/* read terminator */
	if (MSG_ReadByte(buf, NULL) != NONE)
		Com_Printf("WARNING: bad result message\n");

	/* init result text */
	menuText[TEXT_STANDARD] = resultText;

	/* alien stats */
	for (i = 1, kills = 0; i < num; i++)
		kills += (i == we) ? 0 : num_kills[we][i];

	/* needs to be cleared and then append to it */
	if (curCampaign)
		Com_sprintf(resultText, MAX_MENUTEXTLEN, _("Aliens killed\t%i\n"), kills);
	else
		Com_sprintf(resultText, MAX_MENUTEXTLEN, _("Enemies killed\t%i\n"), kills);

	for (i = 1, res = 0; i < num; i++)
		res += (i == we) ? 0 : num_alive[i];

	if (curCampaign)
		Q_strcat(resultText, va(_("Alien survivors\t%i\n\n"), res), sizeof(resultText));
	else
		Q_strcat(resultText, va(_("Enemy survivors\t%i\n\n"), res), sizeof(resultText));

	/* team stats */
	Q_strcat(resultText, va(_("Team losses\t%i\n"), num_spawned[we] - num_alive[we]), sizeof(resultText));
	Q_strcat(resultText, va(_("Friendly fire losses\t%i\n"), num_kills[we][we]), sizeof(resultText));
	Q_strcat(resultText, va(_("Team survivors\t%i\n\n"), num_alive[we]), sizeof(resultText));

	/* kill civilians on campaign, if not won */
	if (curCampaign && num_alive[TEAM_CIVILIAN] && winner != we) {
		num_kills[TEAM_ALIEN][TEAM_CIVILIAN] += num_alive[TEAM_CIVILIAN];
		num_alive[TEAM_CIVILIAN] = 0;
	}

	/* civilian stats */
	for (i = 1, res = 0; i < num; i++)
		res += (i == we) ? 0 : num_kills[i][TEAM_CIVILIAN];

	if (curCampaign)
		Q_strcat(resultText, va(_("Civilians killed by the Aliens\t%i\n"), res), sizeof(resultText));
	else
		Q_strcat(resultText, va(_("Civilians killed by the Enemies\t%i\n"), res), sizeof(resultText));
	Q_strcat(resultText, va(_("Civilians killed by your Team\t%i\n"), num_kills[we][TEAM_CIVILIAN]), sizeof(resultText));
	Q_strcat(resultText, va(_("Civilians saved\t%i\n\n\n"), num_alive[TEAM_CIVILIAN]), sizeof(resultText));

	MN_PopMenu(true);
	if (!curCampaign) {
		/* get correct menus */
		Cvar_Set("mn_main", "main");
		Cvar_Set("mn_active", "");
		MN_PushMenu("main");
	} else {
		mission_t *ms;

		/* get correct menus */
		Cvar_Set("mn_main", "singleplayer");
		Cvar_Set("mn_active", "map");
		MN_PushMenu("map");

		/* show money stats */
		ms = selMis->def;
		ccs.reward = 0;
		if (winner == we) {
			Q_strcat(resultText, va(_("Collected alien technology\t%i c\n"), ms->cr_win), sizeof(resultText));
			ccs.reward += ms->cr_win;
		}
		if (kills) {
			Q_strcat(resultText, va(_("Aliens killed\t%i c\n"), kills * ms->cr_alien), sizeof(resultText));
			ccs.reward += kills * ms->cr_alien;
		}
		if (winner == we && num_alive[TEAM_CIVILIAN]) {
			Q_strcat(resultText, va(_("Civilians saved\t%i c\n"), num_alive[TEAM_CIVILIAN] * ms->cr_civilian), sizeof(resultText));
			ccs.reward += num_alive[TEAM_CIVILIAN] * ms->cr_civilian;
		}
		Q_strcat(resultText, va(_("Total reward\t%i c\n\n\n"), ccs.reward), sizeof(resultText));

		/* recruits */
		if (winner == we && ms->recruits)
			Q_strcat(resultText, va(_("New Recruits\t%i\n"), ms->recruits), sizeof(resultText));

		/* loot the battlefield */
		CL_CollectItems(winner == we);

		/* check for stunned aliens */
		if (winner == we)
			CL_CollectAliens(ms);

		/* update stats */
		CL_UpdateCharacterStats(winner == we);

		ccs.eCampaign = ccs.eMission;
	}

	/* disconnect and show win screen */
	if (winner == we)
		MN_PushMenu("won");
	else
		MN_PushMenu("lost");
	Cbuf_AddText("disconnect\n");
	Cbuf_Execute();
}

/* ======= RANKS & MEDALS =========*/

value_t rankValues[] =
{
	{ "name",	V_TRANSLATION_STRING,	offsetof( rank_t, name ) },
	{ "image",	V_STRING,				offsetof( rank_t, image ) },
	{ "mind",		V_INT,			offsetof( rank_t, mind ) },
	{ "killed_enemies",	V_INT,			offsetof( rank_t, killed_enemies ) },
	{ "killed_others",	V_INT,			offsetof( rank_t, killed_others ) },
	{ NULL,	0, 0 }
};

/**
  * @brief Parse medals and ranks defined in the medals.ufo file.
  */
void CL_ParseMedalsAndRanks( char *title, char **text, uint8_t parserank )
{
	rank_t		*rank = NULL;
	char	*errhead = "Com_ParseMedalsAndRanks: unexptected end of file (medal/rank ";
	char	*token;
	value_t	*v;

	/* get name list body body */
	token = COM_Parse( text );

	if ( !*text || *token != '{' ) {
		Com_Printf( "Com_ParseMedalsAndRanks: rank/medal \"%s\" without body ignored\n", title );
		return;
	}

	if ( parserank) {
		/* parse ranks */
		if ( gd.numRanks >= MAX_RANKS ) {
			Com_Printf( "Too many rank descriptions, '%s' ignored.\n", title );
			gd.numRanks = MAX_RANKS;
			return;
		}

		rank = &gd.ranks[gd.numRanks++];
		memset( rank, 0, sizeof( rank_t ) );

		do {
			/* get the name type */
			token = COM_EParse( text, errhead, title );
			if ( !*text )
				break;
			if ( *token == '}' )
				break;
			for ( v = rankValues; v->string; v++ )
				if ( !Q_strncmp( token, v->string, sizeof(v->string) ) ) {
					/* found a definition */
					token = COM_EParse( text, errhead, title );
					if ( !*text )
						return;
					Com_ParseValue( rank, token, v->type, v->ofs );
					break;
				}

			if ( !v->string )
				Com_Printf( "Com_ParseMedalsAndRanks: unknown token \"%s\" ignored (medal/rank %s)\n", token, title );
		} while ( *text );
	} else {
		/* parse medals */
	}
}

value_t ugvValues[] =
{
	{ "tu",	V_INT,			offsetof( ugv_t, tu ) },
	{ "weapon",	V_STRING,	offsetof( ugv_t, weapon ) },
	{ "armor",	V_STRING,	offsetof( ugv_t, armor ) },
	{ "size",	V_INT,		offsetof( ugv_t, size ) },

	{ NULL,	0, 0 }
};

/**
  * @brief Parse UGVs
  */
void CL_ParseUGVs(char *title, char **text)
{
	char	*errhead = "Com_ParseUGVs: unexptected end of file (ugv ";
	char	*token;
	value_t	*v;
	ugv_t*	ugv;

	/* get name list body body */
	token = COM_Parse( text );

	if ( !*text || *token != '{' ) {
		Com_Printf( "Com_ParseUGVs: ugv \"%s\" without body ignored\n", title );
		return;
	}

	/* parse ugv */
	if ( gd.numUGV >= MAX_UGV ) {
		Com_Printf( "Too many UGV descriptions, '%s' ignored.\n", title );
		gd.numUGV = MAX_UGV;
		return;
	}

	ugv = &gd.ugvs[gd.numUGV++];
	memset( ugv, 0, sizeof( ugv_t ) );

	do {
		/* get the name type */
		token = COM_EParse( text, errhead, title );
		if ( !*text )
			break;
		if ( *token == '}' )
			break;
		for ( v = ugvValues; v->string; v++ )
			if ( !Q_strncmp( token, v->string, sizeof(v->string) ) ) {
				/* found a definition */
				token = COM_EParse( text, errhead, title );
				if ( !*text )
					return;
				Com_ParseValue( ugv, token, v->type, v->ofs );
				break;
			}
			if ( !v->string )
				Com_Printf( "Com_ParseUGVs: unknown token \"%s\" ignored (ugv %s)\n", token, title );
	} while ( *text );
}
