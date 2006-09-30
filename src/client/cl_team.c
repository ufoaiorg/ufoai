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

/**
 * @brief Translate the skin id to skin name
 * @param[in] id The id of the skin
 * @return Translated skin name
 */
char* CL_GetTeamSkinName(int id)
{
	switch(id) {
	case 0:
		return _("Urban");
		break;
	case 1:
		return _("Jungle");
		break;
	case 2:
		return _("Desert");
		break;
	case 3:
		return _("Arctic");
		break;
	default:
		Sys_Error("CL_GetTeamSkinName: Unknown skin id %i - max is %i\n", id, NUM_TEAMSKINS-1);
		break;
	}
	return NULL; /* never reached */
}


/**
 * @brief
 */
void CL_SendItem(sizebuf_t * buf, item_t item, int container, int x, int y)
{
	assert (item.t != NONE);
/*	Com_Printf("Add item %s to container %i (t=%i:a=%i:m=%i) (x=%i:y=%i)\n", csi.ods[item.t].kurz, container, item.t, item.a, item.m, x, y);*/
	MSG_WriteFormat(buf, "bbbbbb",
					item.t, item.a, item.m, container, x, y);
}

/**
 * @brief
 */
void CL_SendInventory(sizebuf_t * buf, inventory_t * i)
{
	int j, nr = 0;
	invList_t *ic;

	for (j = 0; j < csi.numIDs; j++)
		for (ic = i->c[j]; ic; ic = ic->next)
			nr++;

	MSG_WriteShort(buf, nr * 6);
	for (j = 0; j < csi.numIDs; j++)
		for (ic = i->c[j]; ic; ic = ic->next)
			CL_SendItem(buf, ic->item, j, ic->x, ic->y);
}

/**
 * @brief
 */
void CL_ReceiveItem(sizebuf_t * buf, item_t * item, int * container, int * x, int * y)
{
	MSG_ReadFormat(buf, "bbbbbb",
				   &item->t, &item->a, &item->m, container, x, y);
}

/**
 * @brief
 */
void CL_ReceiveInventory(sizebuf_t * buf, inventory_t * i)
{
	item_t item;
	int container, x, y;
	int nr = MSG_ReadShort(buf) / 6;

	for (; nr-- > 0;) {

		CL_ReceiveItem(buf, &item, &container, &x, &y);

		Com_AddToInventory(i, item, container, x, y);
	}
}


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
 * @param[in] employee The employee to create character data for.
 * @param[in] team Which team to use for creation.
 * @todo fix the assignment of the inventory (assume that you do not know the base yet)
 * @todo fix the assignment of ucn??
 * @todo fix the WholeTeam stuff
 */
void CL_GenerateCharacter(employee_t *employee, char *team, employeeType_t employeeType)
{
	character_t *chr = NULL;
	char teamDefName[MAX_VAR];

	if (!employee)
		return;

	chr = &employee->chr;
	memset(chr, 0, sizeof(character_t));

	/* link inventory */
	chr->inv = &employee->inv;
	Com_DestroyInventory(chr->inv);

	/* get ucn */
	chr->ucn = gd.nextUCN++;

	Com_DPrintf("Generate character for team: '%s' (type: %i)\n", team, employeeType);

	/* Backlink from chr to employee struct. */
	chr->empl_type = employeeType;
	chr->empl_idx = employee->idx;

	/* Generate character stats, moels & names. */
	switch (employeeType) {
	case EMPL_SOLDIER:
		chr->rank = CL_GetRank("rookie");
		/* Create attributes. */
		Com_CharGenAbilitySkills(chr, 15, 75, 15, 75);
		/* Get model and name. */
		chr->fieldSize = ACTOR_SIZE_NORMAL;
		chr->skin = Com_GetModelAndName(team, chr);
		break;
	case EMPL_SCIENTIST:
		chr->rank = CL_GetRank("scientist");
		/* Create attributes. */
		Com_CharGenAbilitySkills(chr, 15, 75, 15, 75);
		/* Get model and name. */
		Com_sprintf(teamDefName, MAX_VAR, "%s_scientist", team);
		chr->fieldSize = ACTOR_SIZE_NORMAL;
		chr->skin = Com_GetModelAndName(teamDefName, chr);
		break;
	case EMPL_MEDIC:
		chr->rank = CL_GetRank("medic");
		/* Create attributes. */
		Com_CharGenAbilitySkills(chr, 15, 75, 15, 75);
		/* Get model and name. */
		Com_sprintf(teamDefName, MAX_VAR, "%s_medic", team);
		chr->fieldSize = ACTOR_SIZE_NORMAL;
		chr->skin = Com_GetModelAndName(teamDefName, chr);
		break;
	case EMPL_WORKER:
		chr->rank = CL_GetRank("worker");
		/* Create attributes. */
		Com_CharGenAbilitySkills(chr, 15, 50, 15, 50);
		/* Get model and name. */
		Com_sprintf(teamDefName, MAX_VAR, "%s_worker", team);
		chr->fieldSize = ACTOR_SIZE_NORMAL;
		chr->skin = Com_GetModelAndName(teamDefName, chr);
		break;
	case EMPL_ROBOT:
		chr->rank = CL_GetRank("ugv");
		/* Create attributes. */
		Com_CharGenAbilitySkills(chr, 80, 80, 80, 80);
		/* Get model and name. */
		chr->fieldSize = ACTOR_SIZE_UGV;
		Com_sprintf(teamDefName, MAX_VAR, "%s_ugv", team);
		chr->skin = Com_GetModelAndName(teamDefName, chr);
		break;
	default:
		Sys_Error("Unknown employee type\n");
		break;
	}

	chr->HP = GET_HP(chr->skills[ABILITY_POWER]);
	chr->morale = GET_MORALE(chr->skills[ABILITY_MIND]);
	if (chr->morale >= MAX_SKILL)
		chr->morale = MAX_SKILL;
}


/**
 * @brief
 * @sa CL_GenerateCharacter
 */
void CL_ResetCharacters(base_t* const base)
{
	int i;
	character_t *chr;

	/* reset inventory data */
	for (i = 0; i < MAX_EMPLOYEES; i++) {
		chr = E_GetHiredCharacter(base, EMPL_SOLDIER, i);
		if (!chr)
			continue;
		Com_DestroyInventory(chr->inv);
	}

	/* reset hire info */
	Cvar_ForceSet("cl_selected", "0");

	for (i = 0; i < MAX_EMPL; i++)
		E_UnhireAllEmployees(base, i);

	for (i = 0; i < base->numAircraftInBase; i++) {
		base->teamNum[i] = 0;
		CL_ResetAircraftTeam(B_GetAircraftFromBaseByIndex(base,i));
	}
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

	if (sel >= 0 && sel < gd.numEmployees[EMPL_SOLDIER])
		Q_strncpyz(gd.employees[EMPL_SOLDIER][sel].chr.name, Cvar_VariableString("mn_name"), MAX_VAR);
}


/**
 * @brief Change the skin of the selected actor
 */
static void CL_ChangeSkinCmd(void)
{
	int sel, newSkin;

	sel = cl_selected->value;
	if (sel >= 0 && sel < gd.numEmployees[EMPL_SOLDIER]) {
		newSkin = (int) Cvar_VariableValue("mn_skin") + 1;
		if (newSkin >= NUM_TEAMSKINS || newSkin < 0)
			newSkin = 0;

		baseCurrent->curTeam[sel]->skin = newSkin;

		Cvar_SetValue("mn_skin", newSkin);
		Cvar_Set("mn_skinname", CL_GetTeamSkinName(newSkin));
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
void CL_AddCarriedToEq(equipDef_t * ed)
{
	invList_t *ic, *next;
	int p, container;

	assert(baseCurrent);

	for (container = 0; container < csi.numIDs; container++) {
		for (p = 0; p < baseCurrent->teamNum[baseCurrent->aircraftCurrent]; p++) {
			for (ic = baseCurrent->curTeam[p]->inv->c[container];
				 ic; ic = next) {
				item_t item = ic->item;
				int type = item.t;

				next = ic->next;
				ed->num[type]++;
				if (item.a) {
					assert (csi.ods[type].reload);
					assert (item.m != NONE);
					ed->num_loose[item.m] += item.a;
					/* Accumulate loose ammo into clips */
					if (ed->num_loose[item.m] >= csi.ods[type].ammo) {
						ed->num_loose[item.m] -= csi.ods[type].ammo;
						ed->num[item.m]++;
					}
				}
			}
		}
	}
}

/**
 * @brief
 */
item_t CL_AddWeaponAmmo(equipDef_t * ed, item_t item)
{
	int i = -1, type = item.t;

	assert (ed->num[type] > 0);
	ed->num[type]--;

	if (csi.ods[type].link != NONE) { /* ammo */
		return item;
	} else if (!csi.ods[type].reload) {
		item.m = item.t; /* no ammo needed, so fire definitions are in t */
		return item;
	} else if (item.a) {
		assert (item.m != NONE); /* was reloaded one time */
		if (item.a == csi.ods[type].ammo) {
			/* fully loaded, no need to reload, but mark the ammo as used. */
			assert (item.m != NONE);
			if (ed->num[item.m] > 0) {
				ed->num[item.m]--;
				return item;
			} else {
				/* your clip has been sold; give it back */
				item.a = 0;
				return item;
			}
		}
	}

	/* Check for complete clips of the same kind */
	if (item.m != NONE && ed->num[item.m] > 0) {
		ed->num[item.m]--;
		item.a = csi.ods[type].ammo;
		return item;
	}

	/* Search for any complete clips */
	for (i = 0; i < csi.numODs; i++) {
		if (csi.ods[i].link == type) {
			if (ed->num[i] > 0) {
				ed->num[i]--;
				item.a = csi.ods[type].ammo;
				item.m = i;
				return item;
			}
		}
	}

	/* TODO: on return from a mission with no clips left
	   and one weapon half-loaded wielded by soldier
	   and one empty in equip, on the first opening of equip,
	   the empty weapon will be in soldier hands, the half-full in equip;
	   this should be the other way around. */

	/* Failed to find a complete clip - see if there's any loose ammo
	   of the same kind; if so, gather it all in this weapon. */
	if (item.m != NONE && ed->num_loose[item.m] > 0) {
		item.a = ed->num_loose[item.m];
		ed->num_loose[item.m] = 0;
		return item;
	}

	/* See if there's any loose ammo */
	item.a = 0;
	for (i = 0; i < csi.numODs; i++) {
		if (csi.ods[i].link == type && ed->num_loose[i] > item.a) {
			if (item.a > 0) {
				/* We previously found some ammo, but we've now found other */
				/* loose ammo of a different (but appropriate) type with */
				/* more bullets.  Put the previously found ammo back, so */
				/* we'll take the new type. */
				assert (item.m != NONE);
				ed->num_loose[item.m] = item.a;
				/* We don't have to accumulate loose ammo into clips
				   because we did it previously and we create no new ammo */
			}
			/* Found some loose ammo to load the weapon with */
			item.a = ed->num_loose[i];
			ed->num_loose[i] = 0;
			item.m = i;
		}
	}
	return item;
}

/**
 * @brief
 */
void CL_ReloadAndRemoveCarried(equipDef_t * ed)
{
	character_t *cp;
	invList_t *ic, *next;
	int p, container;

	assert(baseCurrent);

	/* Iterate through in container order (right hand, left hand, belt, */
	/* holster, backpack) at the top level, i.e. each squad member reloads */
	/* her right hand, then each reloads his left hand, etc. The effect */
	/* of this is that when things are tight, everyone has the opportunity */
	/* to get their preferred weapon(s) loaded before anyone is allowed */
	/* to keep her spares in the backpack or on the floor. We don't want */
	/* the first person in the squad filling their backpack with spare ammo */
	/* leaving others with unloaded guns in their hands... */
	Com_DPrintf("teamNum in aircraft %i: %i\n", baseCurrent->aircraftCurrent, baseCurrent->teamNum[baseCurrent->aircraftCurrent] );
	for (container = 0; container < csi.numIDs; container++) {
		for (p = 0; p < baseCurrent->teamNum[baseCurrent->aircraftCurrent]; p++) {
			cp = baseCurrent->curTeam[p];
			for (ic = cp->inv->c[container]; ic; ic = next) {
				next = ic->next;
				if (ed->num[ic->item.t] > 0) {
					ic->item = CL_AddWeaponAmmo(ed, ic->item);
				} else {
					/* drop ammo used for reloading and sold carried weapons */
					Com_RemoveFromInventory(cp->inv, container, ic->x, ic->y);
				}
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

	Com_DestroyInventory(&baseCurrent->equipByBuyType);
	for (i = 0; i < MAX_EMPLOYEES; i++)
		for (k = 0; k < csi.numIDs; k++)
			if (csi.ids[k].temp)
				/* idFloor and idEquip are temp */
				gd.employees[EMPL_SOLDIER][i].inv.c[k] = NULL;
}

/**
 * @brief
 * @note This function is called everytime the equipment screen for the team pops up
 * @sa CL_UpdatePointersInGlobalData
 */
static void CL_GenerateEquipmentCmd(void)
{
	equipDef_t unused;
	int i, p;
	aircraft_t *aircraft;
	/* t value will be set below - a and m are not changed here */
	item_t item = {0,NONE,NONE};

	assert(baseCurrent);

	/* Popup if no soldiers are assigned to the current aircraft. */
	/* if ( !baseCurrent->numHired) { */
	if ( !baseCurrent->teamNum[baseCurrent->aircraftCurrent] ) {
		MN_PopMenu(qfalse);
		return;
	}

	aircraft = &baseCurrent->aircraft[baseCurrent->aircraftCurrent];

	/* Store hired names. */
	Cvar_ForceSet("cl_selected", "0");
	for (i = 0, p = 0; i < (int)cl_numnames->value; i++)
		if ( CL_SoldierInAircraft(i, aircraft->idx) ) {
			/* maybe we already have soldiers in this aircraft */
			baseCurrent->curTeam[p] = E_GetCharacter(baseCurrent, EMPL_SOLDIER, i);
			if (!baseCurrent->curTeam[p])
				Sys_Error("CL_GenerateEquipmentCmd: Could not get employee character with idx: %i\n", p);
			Com_DPrintf("add %s to curTeam (pos: %i)\n", baseCurrent->curTeam[p]->name, p);
			Cvar_ForceSet(va("mn_name%i", p), baseCurrent->curTeam[p]->name);
			p++;
		}

	if ( p != baseCurrent->teamNum[baseCurrent->aircraftCurrent])
		Sys_Error("CL_GenerateEquipmentCmd: numEmployees: %i, teamNum[%i]: %i, p: %i\n",
			gd.numEmployees[EMPL_SOLDIER],
			baseCurrent->aircraftCurrent,
			baseCurrent->teamNum[baseCurrent->aircraftCurrent],
			p);

	for (; p < MAX_ACTIVETEAM; p++) {
		Cvar_ForceSet(va("mn_name%i", p), "");
		Cbuf_AddText(va("equipdisable%i\n", p));
		baseCurrent->curTeam[p] = NULL;
	}

	menuInventory = &(gd.employees[EMPL_SOLDIER][0].inv);
	selActor = NULL;

	/* reset description */
	Cvar_Set("mn_itemname", "");
	Cvar_Set("mn_item", "");
	Cvar_Set("mn_weapon", "");
	Cvar_Set("mn_ammo", "");
	menuText[TEXT_STANDARD] = NULL;

	/* manage inventory */
	unused = baseCurrent->storage; /* copied, including arrays inside! */

	CL_CleanTempInventory();
	CL_ReloadAndRemoveCarried(&unused);

	/* a 'tiny hack' to add the remaining equipment (not carried)
	   correctly into buy categories, reloading at the same time;
	   it is valid only due to the following property: */
	assert (MAX_CONTAINERS >= NUM_BUYTYPES);

	for (i = 0; i < csi.numODs; i++)
		while (unused.num[i]) {
			item.t = i;

			assert (unused.num[i] > 0);
			if (!Com_TryAddToBuyType(&baseCurrent->equipByBuyType, CL_AddWeaponAmmo(&unused, item), csi.ods[i].buytype))
				break; /* no space left in menu */
		}
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
		menuInventory->c[csi.idEquip] = baseCurrent->equipByBuyType.c[num];
}

/**
 * @brief
 * @note This function has various console commands:
 * team_select, soldier_select, equip_select
 */
static void CL_SelectCmd(void)
{
	char *arg;
	char command[MAX_VAR];
	character_t *chr;
	int num;

	/* check syntax */
	if (Cmd_Argc() < 2) {
		Com_Printf("Usage: team_select <num>\n");
		return;
	}
	num = atoi(Cmd_Argv(1));

	/* change highlights */
	arg = Cmd_Argv(0);
	*strchr(arg, '_') = 0;
	Q_strncpyz(command, arg, MAX_VAR);

	if (!Q_strncmp(command, "soldier", 7)) {
		/* check whether we are connected (tactical mission) */
		if (CL_OnBattlescape()) {
			CL_ActorSelectList(num);
			return;
		/* we are still in the menu */
		} else
			Q_strncpyz(command, "equip", MAX_VAR);
	}

	if (!Q_strncmp(command, "equip", 5)) {
		if (baseCurrent->teamNum[baseCurrent->aircraftCurrent] <= 0) {
			if (!ccs.singleplayer)
				Cbuf_AddText("mn_pop;mn_push mp_team;");
			return;
		}
		if (!baseCurrent || baseCurrent->aircraftCurrent < 0 || num >= baseCurrent->teamNum[baseCurrent->aircraftCurrent])
			return;
		if (menuInventory && menuInventory != baseCurrent->curTeam[num]->inv) {
			baseCurrent->curTeam[num]->inv->c[csi.idEquip] = menuInventory->c[csi.idEquip];
			/* set 'old' idEquip to NULL */
			menuInventory->c[csi.idEquip] = NULL;
		}
		menuInventory = baseCurrent->curTeam[num]->inv;
	} else if (!Q_strncmp(command, "team", 4)) {
		if (!baseCurrent || num >= E_CountHired(baseCurrent, EMPL_SOLDIER))
			return;
	}

	/* console commands */
	Cbuf_AddText(va("%sdeselect%i\n", command, (int) cl_selected->value));
	Cbuf_AddText(va("%sselect%i\n", command, num));
	Cvar_ForceSet("cl_selected", va("%i", num));

	Com_DPrintf("CL_SelectCmd: Command: '%s' - num: %i\n", command, num);

	if (!Q_strncmp(command, "team", 4)) {
		/* set info cvars */
		num++;
		chr = E_GetHiredCharacter(baseCurrent, EMPL_SOLDIER, -num);
		if (!chr)
			Sys_Error("CL_SelectCmd: No hired character at pos %i (base: %i)\n", num, baseCurrent->idx);
		if ( chr->fieldSize == ACTOR_SIZE_NORMAL )
			CL_CharacterCvars(chr);
		else
			CL_UGVCvars(chr);
	} else {
		/* set info cvars */
		chr = baseCurrent->curTeam[num];
		if ( chr->fieldSize == ACTOR_SIZE_NORMAL )
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
	aircraft_t *aircraft = NULL;

	assert(baseCurrent);

	aircraft = &baseCurrent->aircraft[baseCurrent->aircraftCurrent];
	Cvar_Set("mn_hired", va(_("%i of %i"), baseCurrent->teamNum[baseCurrent->aircraftCurrent], aircraft->size));

	/* update curTeam list */
	for (i = 0, p = 0; i < (int)cl_numnames->value; i++) {
		if ( CL_SoldierInAircraft(i, aircraft->idx) ) {
			baseCurrent->curTeam[p] = E_GetHiredCharacter(baseCurrent, EMPL_SOLDIER, i);
			p++;
		}
	}
	if ( p != baseCurrent->teamNum[baseCurrent->aircraftCurrent])
		Sys_Error("CL_UpdateHireVar: SoldiersInBase: %i, teamNum[%i]: %i, p: %i\n",
				  E_CountHired(baseCurrent, EMPL_SOLDIER),
				  baseCurrent->aircraftCurrent,
				  baseCurrent->teamNum[baseCurrent->aircraftCurrent],
				  p);

	for (; p<MAX_ACTIVETEAM; p++)
		baseCurrent->curTeam[p] = NULL;
}

/**
 * @brief only for multiplayer when setting up a new team
 * @sa E_ResetEmployees
 * @sa CL_CleanTempInventory
 * @note We need baseCurrent to point to gd.bases[0] here
 * @note available via script command team_reset
 * @note called when initializing the multiplayer menu (for init node and new team button)
 */
void CL_ResetTeamInBase(void)
{
	employee_t* employee;

	if (ccs.singleplayer)
		return;

	Com_DPrintf("Reset of baseCurrent team flags.\n");
	if (!baseCurrent) {
		Com_DPrintf("CL_ResetTeamInBase: No baseCurrent\n");
		return;
	}

	CL_CleanTempInventory();

	baseCurrent->teamNum[0] = 0;
	CL_ResetAircraftTeam(B_GetAircraftFromBaseByIndex(baseCurrent,0));

	E_ResetEmployees();
	while (gd.numEmployees[EMPL_SOLDIER] < cl_numnames->value) {
		employee = E_CreateEmployee(EMPL_SOLDIER);
		employee->hired = qtrue;
		employee->baseIDHired = baseCurrent->idx;
		Com_DPrintf("B_ClearBase: Generate character for multiplayer - employee->chr.name: '%s' (base: %i)\n", employee->chr.name, baseCurrent->idx);
	}

	/* reset the multiplayer inventory; stored in baseCurrent->storage */
	{
		equipDef_t *ed;
		char *name;
		int i;

		/* search equipment definition */
		name = "multiplayer";
		Com_DPrintf("CL_GenerateEquipmentCmd: no curCampaign - using equipment '%s'\n", name);
		for (i = 0, ed = csi.eds; i < csi.numEDs; i++, ed++) {
			if (!Q_strncmp(name, ed->name, MAX_VAR))
				break;
		}
		if (i == csi.numEDs) {
			Com_Printf("Equipment '%s' not found!\n", name);
			return;
		}
		baseCurrent->storage = *ed; /* copied, including the arrays inside! */
	}
}

/**
 * @brief Init the teamlist checkboxes
 * @sa CL_UpdateHireVar
 */
static void CL_MarkTeamCmd(void)
{
	int i, j, k = 0;
	qboolean alreadyInOtherShip = qfalse;
	aircraft_t *aircraft;

	/* check if we are allowed to be here? */
	/* we are only allowed to be here if we already set up a base */
	if (!baseCurrent) {
		Com_Printf("No base set up\n");
		MN_PopMenu(qfalse);
		return;
	}

	CL_UpdateHireVar();
	aircraft = &baseCurrent->aircraft[baseCurrent->aircraftCurrent];

	for (i = 0; i < (int)cl_numnames->value; i++) {
		alreadyInOtherShip = qfalse;
		/* don't show other base's recruits */
		if (!gd.employees[EMPL_SOLDIER][i].hired || gd.employees[EMPL_SOLDIER][i].baseIDHired != baseCurrent->idx)
			continue;
		for (j = 0; j < gd.numAircraft; j++) {
			if (j==aircraft->idx)
				continue;
			/* already on another aircraft */
			if (CL_SoldierInAircraft(i, j) )
				alreadyInOtherShip = qtrue;
		}
		Cvar_ForceSet(va("mn_name%i", k), gd.employees[EMPL_SOLDIER][i].chr.name);
		/* change the buttons */
		if (!alreadyInOtherShip && CL_SoldierInAircraft(i, aircraft->idx))
			Cbuf_AddText(va("listadd%i\n", k));
		/* disable the button - the soldier is already on another aircraft */
		else if (alreadyInOtherShip)
			Cbuf_AddText(va("listdisable%i\n", k));

		for (j = 0; j < csi.numIDs; j++) {
			if ( j != csi.idFloor
				 && j != csi.idEquip
				 && gd.employees[EMPL_SOLDIER][i].inv.c[j] )
				break;
		}

		if (j < csi.numIDs)
			Cbuf_AddText(va("listholdsequip%i\n", k));
		else
			Cbuf_AddText(va("listholdsnoequip%i\n", k));
		k++;
	}

	for (;k < (int) cl_numnames->value; k++) {
		Cbuf_AddText(va("listdisable%i\n", k));
		Cvar_ForceSet(va("mn_name%i", k), "");
		Cbuf_AddText(va("listholdsnoequip%i\n", k));
	}
}

/**
 * @brief Tells you if a soldier is assigned to an aircraft.
 * @param[in] employee_idx The index of the soldier in the global list.
 * @param[in] aircraft_idx The index of aircraft in the base. use -1 to check if the soldier is in _any_ aircraft.
 * @return qboolean qtrue if the soldier was found in the aircraft(s) else: qfalse.
 * @pre Needs baseCurrent set to the base the aircraft is located in.
 */
qboolean CL_SoldierInAircraft(int employee_idx, int aircraft_idx)
{
	int i;

	assert(baseCurrent);

	if ( employee_idx < 0 )
		return qfalse;

	if ( aircraft_idx < 0) {
		/* Search if he is in _any_ aircraft and return true if it's the case. */
		for ( i = 0; i < gd.numAircraft; i++ ) {
			if ( CL_SoldierInAircraft(employee_idx, i) )
				return qtrue;
		}
	}

	if ( aircraft_idx < 0 )
		return qfalse;

	return CL_IsInAircraftTeam(CL_AircraftGetFromIdx(aircraft_idx),employee_idx);
}

/**
 * @brief Removes a soldier from an aircraft.
 * @param[in] employee_idx The index of the soldier in the global list.
 * @param[in] aircraft_idx The index of aircraft in the base. Use -1 to remove the soldier from any aircraft.
 * @pre Needs baseCurrent set to the base the aircraft is located in.
 */
void CL_RemoveSoldierFromAircraft(int employee_idx, int aircraft_idx)
{
	aircraft_t *aircraft;
	int i;

	if ( employee_idx < 0 )
		return;

	if ( aircraft_idx < 0 ) {
		/* Search if he is in _any_ aircraft and set the aircraft_idx if found.. */
		for ( i = 0; i < gd.numAircraft; i++ ) {
			if ( CL_SoldierInAircraft(employee_idx, i) ) {
				aircraft_idx = i;
				break;
			}
		}
	}

	if ( aircraft_idx < 0 )
		return;

	aircraft = &baseCurrent->aircraft[aircraft_idx];

	Com_DestroyInventory(&gd.employees[EMPL_SOLDIER][employee_idx].inv);
	CL_RemoveFromAircraftTeam(aircraft, employee_idx);
	baseCurrent->teamNum[aircraft_idx]--;
}

/**
 * @brief Removes all soldiers from an aircraft.
 * @param[in] aircraft_idx The index of aircraft in the base.
 * @param[in] base_idx The index of the base the aircraft is located in.
 */
void CL_RemoveSoldiersFromAircraft(int aircraft_idx, int base_idx)
{
	int i = 0;
	base_t *base = NULL;
	aircraft_t *aircraft = NULL;

	if ( aircraft_idx < 0 || base_idx < 0)
		return;

	base = &gd.bases[base_idx];
	aircraft = &base->aircraft[aircraft_idx];

	/* Counting backwards because teamNum[aircraft->idx] is changed in CL_RemoveSoldierFromAircraft */
	for ( i = base->teamNum[aircraft->idx]-1; i >= 0; i-- ) {
		CL_RemoveSoldierFromAircraft(i, aircraft->idx);
	}
}

/**
 * @brief Assigns a soldier to an aircraft.
 * @param[in] employee_idx The index of the soldier in the global list.
 * @param[in] aircraft_idx The index of aircraft in the base.
 * @return returns true if a soldier could be assigned to the aircraft.
 */
static qboolean CL_AssignSoldierToAircraft(int employee_idx, int aircraft_idx)
{
	aircraft_t *aircraft = NULL;
	if ( employee_idx < 0 || aircraft_idx < 0 )
		return qfalse;

	if (baseCurrent->teamNum[aircraft_idx] < MAX_ACTIVETEAM) {
		aircraft = &baseCurrent->aircraft[aircraft_idx];
		/* Check whether the soldier is already on another aircraft */
		Com_DPrintf("CL_AssignSoldierToAircraft: attempting to find idx '%d'\n", employee_idx);

		if (CL_SoldierInAircraft(employee_idx, -1)) {
			Com_DPrintf("CL_AssignSoldierToAircraft: found idx '%d' \n",employee_idx);
			return qfalse;
		}

		/* Assign the soldier to the aircraft. */
		if (aircraft->size > baseCurrent->teamNum[aircraft_idx]) {
			Com_DPrintf("CL_AssignSoldierToAircraft: attemting to add idx '%d' \n",employee_idx);
			CL_AddToAircraftTeam(aircraft,employee_idx);
			baseCurrent->teamNum[aircraft_idx]++;
			return qtrue;
		}
	}
	return qfalse;
}

/**
 * @brief Adds or removes a soldier to/from an aircraft.
 */
static void CL_AssignSoldierCmd(void)
{
	int num = -1;
	employee_t *employee = NULL;
	aircraft_t *aircraft;

	/* check syntax */
	if (Cmd_Argc() < 2) {
		Com_Printf("Usage: team_hire <num>\n");
		return;
	}
	num = atoi(Cmd_Argv(1));

	if (num >= E_CountHired(baseCurrent, EMPL_SOLDIER) || num >= (int)cl_numnames->value)
		return;

	employee = E_GetHiredEmployee(baseCurrent, EMPL_SOLDIER, -(num+1));
	if (!employee)
		Sys_Error("CL_AssignSoldierCmd: Could not get employee %i\n", num);

	Com_DPrintf("CL_AssignSoldierCmd: employee with idx %i selected\n", employee->idx);
	aircraft = &baseCurrent->aircraft[baseCurrent->aircraftCurrent];

	if ( CL_SoldierInAircraft(employee->idx, aircraft->idx) ) {
		Com_DPrintf("CL_AssignSoldierCmd: removing\n");
		/* Remove soldier from aircraft/team. */
		Cbuf_AddText(va("listdel%i\n", num));
		CL_RemoveSoldierFromAircraft(employee->idx, baseCurrent->aircraftCurrent);
		Cbuf_AddText(va("listholdsnoequip%i\n", num));
	} else {
		Com_DPrintf("CL_AssignSoldierCmd: assigning\n");
		/* Assign soldier to aircraft/team. */
		if (CL_AssignSoldierToAircraft(employee->idx, baseCurrent->aircraftCurrent))
			Cbuf_AddText(va("listadd%i\n", num));
	}
	/* Select the desired one anyways. */
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
static void CL_MessageMenuCmd(void)
{
	static char nameBackup[MAX_VAR];
	static char cvarName[MAX_VAR];
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
	byte buf[MAX_TEAMDATASIZE];
	char *name;
	int res;

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
	CL_SendTeamInfo(&sb, baseCurrent->idx, E_CountHired(baseCurrent, EMPL_SOLDIER));

	/* store assignement */
	MSG_WriteByte(&sb, baseCurrent->teamNum[0]);

	/* write data */
	res = FS_WriteFile(buf, sb.cursize, filename);

	if (res == sb.cursize)
		Com_Printf("Team '%s' saved.\n", filename);
	else if (!res)
		Com_Printf("Team '%s' not saved.\n", filename);
}

/**
 * @brief Stores a team in a specified teamslot (multiplayer)
 */
static void CL_SaveTeamSlotCmd(void)
{
	char filename[MAX_QPATH];

	/* save */
	Com_sprintf(filename, MAX_QPATH, "%s/save/team%s.mpt", FS_Gamedir(), Cvar_VariableString("mn_slot"));
	CL_SaveTeam(filename);
}

/**
 * @brief Load a team member for multiplayer
 * @sa CL_LoadTeam
 */
void CL_LoadTeamMember(sizebuf_t * sb, character_t * chr)
{
	int i;

	/* unique character number */
	chr->fieldSize = MSG_ReadByte(sb);
	chr->ucn = MSG_ReadShort(sb);
	if (chr->ucn >= gd.nextUCN)
		gd.nextUCN = chr->ucn + 1;

	/* name and model */
	Q_strncpyz(chr->name, MSG_ReadString(sb), MAX_VAR);
	Q_strncpyz(chr->path, MSG_ReadString(sb), MAX_VAR);
	Q_strncpyz(chr->body, MSG_ReadString(sb), MAX_VAR);
	Q_strncpyz(chr->head, MSG_ReadString(sb), MAX_VAR);
	chr->skin = MSG_ReadByte(sb);

	chr->HP = MSG_ReadShort(sb);
	chr->morale = MSG_ReadByte(sb);

	/* new attributes */
	for (i = 0; i < SKILL_NUM_TYPES; i++)
		chr->skills[i] = MSG_ReadByte(sb);

	/* load scores */
	for (i = 0; i < KILLED_NUM_TYPES; i++)
		chr->kills[i] = MSG_ReadShort(sb);
	chr->assigned_missions = MSG_ReadShort(sb);

	/* inventory */
	Com_DestroyInventory(chr->inv);
	CL_ReceiveInventory(sb, chr->inv);
}

/**
 * @brief Load a multiplayer team
 * @sa CL_LoadTeam
 * @sa CL_SaveTeam
 */
void CL_LoadTeamMultiplayer(char *filename)
{
	sizebuf_t sb;
	byte buf[MAX_TEAMDATASIZE];
	FILE *f;
	character_t *chr;
	employee_t *employee;
	int i, p, num;

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
	Cvar_Set("mn_teamname", MSG_ReadString(&sb));

	/* load the team */
	/* reset data */
	CL_ResetCharacters(&gd.bases[0]);

	/* read whole team list */
	MSG_ReadByte(&sb);
	num = MSG_ReadByte(&sb);
	Com_DPrintf("load %i teammembers\n", num);
	E_ResetEmployees();
	for (i=0; i<num; i++) {
		/* New employee */
		employee = E_CreateEmployee(EMPL_SOLDIER);
		employee->hired = qtrue;
		employee->baseIDHired = gd.bases[0].idx;
		chr = &employee->chr;
		CL_LoadTeamMember(&sb, chr);
	}

	/* get assignement */
	gd.bases[0].teamNum[0] = MSG_ReadByte(&sb);

	for (i = 0, p = 0; i < num; i++)
		if ( CL_SoldierInAircraft(i, 0) )
			gd.bases[0].curTeam[p++] = E_GetHiredCharacter(&gd.bases[0], EMPL_SOLDIER, i);

	/* gd.bases[0].numHired = p; */

	Com_DPrintf("Load team with %i members and %i slots\n", p, num);

	for (;p<MAX_ACTIVETEAM;p++)
		gd.bases[0].curTeam[p] = NULL;
}

/**
 * @brief Loads the selected teamslot
 */
static void CL_LoadTeamSlotCmd(void)
{
	char filename[MAX_QPATH];

	if (ccs.singleplayer) {
		Com_Printf("Only for multiplayer\n");
		return;
	}

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
	Cmd_AddCommand("givename", CL_GiveNameCmd, NULL);
	Cmd_AddCommand("gennames", CL_GenerateNamesCmd, NULL);
	Cmd_AddCommand("team_reset", CL_ResetTeamInBase, NULL);
	Cmd_AddCommand("genequip", CL_GenerateEquipmentCmd, NULL);
	Cmd_AddCommand("equip_type", CL_EquipTypeCmd, NULL);
	Cmd_AddCommand("team_mark", CL_MarkTeamCmd, NULL);
	Cmd_AddCommand("team_hire", CL_AssignSoldierCmd, NULL);
	Cmd_AddCommand("team_select", CL_SelectCmd, NULL);
	Cmd_AddCommand("team_changename", CL_ChangeNameCmd, NULL);
	Cmd_AddCommand("team_changeskin", CL_ChangeSkinCmd, NULL);
	Cmd_AddCommand("team_comments", CL_TeamCommentsCmd, NULL);
	Cmd_AddCommand("equip_select", CL_SelectCmd, NULL);
	Cmd_AddCommand("soldier_select", CL_SelectCmd, _("Select a soldier from list"));
	Cmd_AddCommand("saveteamslot", CL_SaveTeamSlotCmd, NULL);
	Cmd_AddCommand("loadteamslot", CL_LoadTeamSlotCmd, NULL);
	Cmd_AddCommand("msgmenu", CL_MessageMenuCmd, NULL);
}


/**
 * @brief Stores the wholeTeam info to buffer (which might be a network buffer, too)
 *
 * Called by CL_SaveTeam to store the team info
 */
void CL_SendTeamInfo(sizebuf_t * buf, int baseID, int num)
{
	character_t *chr;
	int i, j;

	/* clean temp inventory */
	CL_CleanTempInventory();

	/* header */
	MSG_WriteByte(buf, clc_teaminfo);
	MSG_WriteByte(buf, num);

	for (i = 0; i < num; i++) {
		/* FIXME: This will not lead to the i.th character if we have more than one base
		maybe not even if we only have one base
		*/
		chr = E_GetHiredCharacter(&gd.bases[baseID], EMPL_SOLDIER, -(i+1));
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
		MSG_WriteByte(buf, chr->morale);

		/* even new attributes */
		for (j = 0; j < SKILL_NUM_TYPES; j++)
			MSG_WriteByte(buf, chr->skills[j]);

		/* scores */
		for (j = 0; j < KILLED_NUM_TYPES; j++)
			MSG_WriteShort(buf, chr->kills[j]);
		MSG_WriteShort(buf, chr->assigned_missions);

		/* inventory */
		CL_SendInventory(buf, chr->inv);
	}
}

/**
 * @brief Stores the curTteam info to buffer (which might be a network buffer, too)
 * see G_ClientTeamInfo
 *
 * Called in cl_main.c CL_Precache_f to send the team info to server
 */
void CL_SendCurTeamInfo(sizebuf_t * buf, character_t ** team, int num)
{
	character_t *chr;
	int i, j;

	/* clean temp inventory */
	CL_CleanTempInventory();

	/* header */
	MSG_WriteByte(buf, clc_teaminfo);
	MSG_WriteByte(buf, num);

	for (i = 0; i < num; i++) {
		chr = team[i];
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
		MSG_WriteByte(buf, chr->morale);

		/* even new attributes */
		for (j = 0; j < SKILL_NUM_TYPES; j++)
			MSG_WriteByte(buf, chr->skills[j]);

		/* scores */
		for (j = 0; j < KILLED_NUM_TYPES; j++)
			MSG_WriteShort(buf, chr->kills[j]);
		MSG_WriteShort(buf, chr->assigned_missions);

		/* inventory */
		CL_SendInventory(buf, chr->inv);
	}
}


/* for updating after click on continue */
typedef struct updateCharacter_s {
	int ucn;
	int kills[KILLED_NUM_TYPES];
	int HP;
	int morale;
} updateCharacter_t;

/**
 * @brief Parses the character data which was send by G_EndGame using G_SendCharacterData
 * @sa G_SendCharacterData
 */
void CL_ParseCharacterData(sizebuf_t *buf, qboolean updateCharacter)
{
	static updateCharacter_t updateCharacterArray[MAX_WHOLETEAM];
	static int num = 0;
	int i, j;
	character_t* chr = NULL;

	if (updateCharacter) {
		for (i=0; i<num; i++) {
			chr = NULL;
			/* MAX_EMPLOYEES and not numWholeTeam - maybe some other soldier died */
			for (j=0; j<MAX_EMPLOYEES; j++) {
				chr = E_GetHiredCharacter(baseCurrent, EMPL_SOLDIER, j);
				if (chr && chr->ucn == updateCharacterArray[i].ucn)
					break;
				chr = NULL;
			}
			if (!chr) {
				Com_Printf("Warning: Could not get character with ucn: %i.\n", updateCharacterArray[i].ucn);
				continue;
			}
			chr->HP = updateCharacterArray[i].HP;
			chr->morale = updateCharacterArray[i].morale;

			for (j=0; j<KILLED_NUM_TYPES; j++)
				chr->kills[j] = updateCharacterArray[i].kills[j];
		}
		num = 0;
	} else {
		/* invalidate ucn in the array first */
		for (i=0; i<MAX_WHOLETEAM; i++) {
			updateCharacterArray[i].ucn = -1;
		}
		num = MSG_ReadShort(buf) / ((KILLED_NUM_TYPES + 2) * 2 + 1);
		if (num > MAX_EMPLOYEES)
			Sys_Error("CL_ParseCharacterData: num exceeded MAX_WHOLETEAM\n");
		else if (num < 0)
			Sys_Error("CL_ParseCharacterData: MSG_ReadShort error (%i)\n", num);
		for (i=0; i<num; i++) {
			updateCharacterArray[i].ucn = MSG_ReadShort(buf);
			updateCharacterArray[i].HP = MSG_ReadShort(buf);
			updateCharacterArray[i].morale = MSG_ReadByte(buf);

			for (j=0; j<KILLED_NUM_TYPES; j++)
				updateCharacterArray[i].kills[j] = MSG_ReadShort(buf);
		}
	}
}


/**
 * @brief Reads mission result data from server
 * See EV_RESULTS
 * @sa G_EndGame
 * @sa CL_GameResultsCmd
 */
void CL_ParseResults(sizebuf_t * buf)
{
	static char resultText[MAX_MENUTEXTLEN];
	byte num_spawned[MAX_TEAMS];
	byte num_alive[MAX_TEAMS];
	byte num_kills[MAX_TEAMS][MAX_TEAMS];
	byte num_stuns[MAX_TEAMS][MAX_TEAMS];
	byte winner, we;

	int i, j, num, number_items, credits_gained;
	int our_surviviurs,our_killed,our_stunned;
	int thier_surviviurs,thier_killed,thier_stunned;
	int civilian_surviviurs,civilian_killed,civilian_stunned;

	/* get number of teams */
	num = MSG_ReadByte(buf);
	if (num > MAX_TEAMS)
		Sys_Error("Too many teams in result message\n");

	Com_DPrintf("Receiving results with %i teams.\n", num);

	/* get winning team */
	winner = MSG_ReadByte(buf);
	we = cls.team;

	MSG_ReadShort(buf); /* size */
	/* get spawn and alive count */
	for (i = 0; i < num; i++) {
		num_spawned[i] = MSG_ReadByte(buf);
		num_alive[i] = MSG_ReadByte(buf);
	}

	MSG_ReadShort(buf); /* size */
	/* get kills */
	for (i = 0; i < num; i++)
		for (j = 0; j < num; j++)
			num_kills[i][j] = MSG_ReadByte(buf);

	MSG_ReadShort(buf); /* size */
	/* get stuns */
	for (i = 0; i < num; i++)
		for (j = 0; j < num; j++)
			num_stuns[i][j] = MSG_ReadByte(buf);

	CL_ParseCharacterData(buf, qfalse);

	/* init result text */
	menuText[TEXT_STANDARD] = resultText;

	our_surviviurs = 0;
	our_killed = 0;
	our_stunned = 0;
	thier_surviviurs = 0;
	thier_killed = 0;
	thier_stunned = 0;
	civilian_surviviurs = 0;
	civilian_killed = 0;
	civilian_stunned = 0;

	for (i = 0; i < num; i++) {
		if (i == we)
			our_surviviurs = num_alive[i];
		else if (i == TEAM_CIVILIAN)
			civilian_surviviurs = num_alive[i];
		else
			thier_surviviurs += num_alive[i];
		for (j = 0; j < num; j++)
			if (j == we) {
				our_killed += num_kills[i][j];
				our_stunned += num_stuns[i][j]++;
			} else if (j == TEAM_CIVILIAN) {
				civilian_killed += num_kills[i][j];
				civilian_stunned += num_stuns[i][j]++;
			} else {
				thier_killed += num_kills[i][j];
				thier_stunned += num_stuns[i][j]++;
			}
	}
	/* if we won, our stunned are alive*/
	if (winner==we) {
		our_surviviurs += our_stunned;
		our_stunned = 0;
	} else
		/* if we lost, they revive stunned */
		thier_stunned = 0;

	/* we won,and we'r not the dirty aliens*/
	if ((winner==we)&&(curCampaign))
		civilian_surviviurs+=civilian_stunned;
	else
		civilian_killed+=civilian_stunned;

	if (!curCampaign || !selMis) {
		/* the mission was started via console (TODO: or is multiplayer) */
		/* needs to be cleared and then append to it */
		if (curCampaign) {
			Com_sprintf(resultText, MAX_MENUTEXTLEN, _("Aliens killed\t%i\n"), thier_killed);
			ccs.aliensKilled += thier_killed;
		} else {
			Com_sprintf(resultText, MAX_MENUTEXTLEN, _("Enemies killed\t%i\n"), thier_killed + civilian_killed);
			ccs.aliensKilled += thier_killed + civilian_killed;
		}

		if (curCampaign) {
			Q_strcat(resultText, va(_("Aliens captured\t%i\n\n"), thier_stunned), sizeof(resultText));
			Q_strcat(resultText, va(_("Alien surviviurs\t%i\n\n"), thier_surviviurs), sizeof(resultText));
		} else {
			Q_strcat(resultText, va(_("Enemies captured\t%i\n\n"), thier_stunned), sizeof(resultText));
			Q_strcat(resultText, va(_("Enemy surviviurs\t%i\n\n"), thier_surviviurs), sizeof(resultText));
		}

		/* team stats */
		Q_strcat(resultText, va(_("Team losses\t%i\n"), our_killed), sizeof(resultText));
		Q_strcat(resultText, va(_("Team missing in action\t%i\n"), our_stunned), sizeof(resultText));
		Q_strcat(resultText, va(_("Friendly fire losses\t%i\n"), num_kills[we][we]), sizeof(resultText));
		Q_strcat(resultText, va(_("Team survivors\t%i\n\n"), our_surviviurs), sizeof(resultText));

		if (curCampaign)
			Q_strcat(resultText, va(_("Civilians killed by the Aliens\t%i\n"), civilian_killed - num_kills[we][TEAM_CIVILIAN]), sizeof(resultText));
		else
			Q_strcat(resultText, va(_("Civilians killed by the Enemies\t%i\n"), civilian_killed - civilian_stunned - num_kills[we][TEAM_CIVILIAN]), sizeof(resultText));

		Q_strcat(resultText, va(_("Civilians killed by your Team\t%i\n"), num_kills[we][TEAM_CIVILIAN]), sizeof(resultText));
		Q_strcat(resultText, va(_("Civilians saved\t%i\n\n\n"), civilian_surviviurs), sizeof(resultText));
		ccs.civiliansKilled += civilian_killed;

		MN_PopMenu(qtrue);
		Cvar_Set("mn_main", "main");
		Cvar_Set("mn_active", "");
		MN_PushMenu("main");
	} else {
		/* the mission was in singleplayer */
		/* loot the battlefield */
		CL_CollectItems(winner == we, &number_items, &credits_gained);

		/* check for stunned aliens;
		 TODO: make this reversible, like CL_CollectItems above */
		if (winner == we)
			CL_CollectAliens();

		/* clear unused LE inventories */
		LE_Cleanup();

		/* needs to be cleared and then append to it */
		Com_sprintf(resultText, MAX_MENUTEXTLEN, _("Aliens killed\t%i\n"), thier_killed);
		ccs.aliensKilled += thier_killed;

		Q_strcat(resultText, va(_("Aliens captured\t%i\n"), thier_stunned), sizeof(resultText));
		Q_strcat(resultText, va(_("Alien survivors\t%i\n\n"), thier_surviviurs), sizeof(resultText));

		/* team stats */
		Q_strcat(resultText, va(_("Phalanx soldiers killed by Aliens\t%i\n"), our_killed - num_kills[we][we] - num_kills[TEAM_CIVILIAN][we]), sizeof(resultText));
		Q_strcat(resultText, va(_("Phalanx soldiers missing in action\t%i\n"), our_stunned), sizeof(resultText));
		Q_strcat(resultText, va(_("Phalanx friendly fire losses\t%i\n"), num_kills[we][we] + num_kills[TEAM_CIVILIAN][we]), sizeof(resultText));
		Q_strcat(resultText, va(_("Phalanx survivors\t%i\n\n"), our_surviviurs), sizeof(resultText));

		Q_strcat(resultText, va(_("Civilians killed by Aliens\t%i\n"), civilian_killed), sizeof(resultText));
		Q_strcat(resultText, va(_("Civilians killed by friendly fire\t%i\n"), num_kills[we][TEAM_CIVILIAN] + num_kills[TEAM_CIVILIAN][TEAM_CIVILIAN]), sizeof(resultText));
		Q_strcat(resultText, va(_("Civilians saved\t%i\n\n"), civilian_surviviurs), sizeof(resultText));

		ccs.civiliansKilled += civilian_killed;

		Q_strcat(resultText, va(_("Items salvaged and sold\t%i\n"), number_items),sizeof(resultText));
		Q_strcat(resultText, va(_("Total item sale value\t%i\n\n"), credits_gained),sizeof(resultText));

		MN_PopMenu(qtrue);
		Cvar_Set("mn_main", "singleplayer");
		Cvar_Set("mn_active", "map");
		MN_PushMenu("map");
	}
	/* show win screen */
	if (winner == we)
		MN_PushMenu("won");
	else
		MN_PushMenu("lost");

	/* we can safely wipe all mission data now */
	/* TODO: I don't understand how this works
	   and why, when I move this to CL_GameResultsCmd,
	   the "won" menu get's garbled at "killteam 7" */
	Cbuf_AddText("disconnect\n");
	Cbuf_Execute();
}

/* ======= RANKS & MEDALS =========*/

/**
 * @brief Get the index number of the given rankID
 */
int CL_GetRank(char* rankID)
{
	int i;
	for (i = 0; i < gd.numRanks; i++) {
		if (!Q_strncmp(gd.ranks[i].id, rankID, MAX_VAR))
			return i;
	}
	Com_Printf("Could not find rank '%s'\n", rankID);
	return -1;
}

static value_t rankValues[] =
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
void CL_ParseMedalsAndRanks( char *title, char **text, byte parserank )
{
	rank_t		*rank = NULL;
	char	*errhead = "Com_ParseMedalsAndRanks: unexptected end of file (medal/rank ";
	char	*token;
	value_t	*v;

	/* get name list body body */
	token = COM_Parse( text );

	if (!*text || *token != '{') {
		Com_Printf( "Com_ParseMedalsAndRanks: rank/medal \"%s\" without body ignored\n", title );
		return;
	}

	if (parserank) {
		/* parse ranks */
		if (gd.numRanks >= MAX_RANKS) {
			Com_Printf( "Too many rank descriptions, '%s' ignored.\n", title );
			gd.numRanks = MAX_RANKS;
			return;
		}

		rank = &gd.ranks[gd.numRanks++];
		memset(rank, 0, sizeof(rank_t));
		Q_strncpyz(rank->id, title, MAX_VAR);

		do {
			/* get the name type */
			token = COM_EParse(text, errhead, title);
			if (!*text)
				break;
			if (*token == '}')
				break;
			for (v = rankValues; v->string; v++)
				if (!Q_strncmp(token, v->string, sizeof(v->string))) {
					/* found a definition */
					token = COM_EParse(text, errhead, title);
					if (!*text)
						return;
					Com_ParseValue(rank, token, v->type, v->ofs);
					break;
				}

			if (!Q_strncmp(token, "type", 4)) {
				/* employeeType_t */
				token = COM_EParse(text, errhead, title);
				if (!*text)
					return;
				rank->type = E_GetEmployeeType(token);
			} else if (!v->string)
				Com_Printf( "Com_ParseMedalsAndRanks: unknown token \"%s\" ignored (medal/rank %s)\n", token, title );
		} while (*text);
	} else {
		/* parse medals */
	}
}

static value_t ugvValues[] =
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
