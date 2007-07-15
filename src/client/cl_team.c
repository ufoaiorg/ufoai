/**
 * @file cl_team.c
 * @brief Team management, name generation and parsing.
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

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

*/

#include "client.h"
#include "cl_global.h"

static int CL_GetRank(const char* rankID);
static void CL_SaveTeamInfo(sizebuf_t * buf, int baseID, int num);

/**
 * @brief Translate the skin id to skin name
 * @param[in] id The id of the skin
 * @return Translated skin name
 */
char* CL_GetTeamSkinName (int id)
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
 * @sa CL_ReceiveItem
 * @sa G_WriteItem
 * @sa G_ReadItem
 */
void CL_SendItem (sizebuf_t *buf, item_t item, int container, int x, int y, qboolean save)
{
	assert(item.t != NONE);
/*	Com_Printf("Add item %s to container %i (t=%i:a=%i:m=%i) (x=%i:y=%i)\n", csi.ods[item.t].id, container, item.t, item.a, item.m, x, y);*/
	if (save) {
		MSG_WriteFormat(buf, "bbbb", item.a, container, x, y);
		MSG_WriteString(buf, csi.ods[item.t].id);
		if (item.a > NONE_AMMO)
			MSG_WriteString(buf, csi.ods[item.m].id);
	} else
		MSG_WriteFormat(buf, "bbbbbb", item.t, item.a, item.m, container, x, y);
}

/**
 * @brief
 * @sa CL_SendItem
 * @sa CL_ReceiveInventory
 * @sa G_SendInventory
 */
void CL_SendInventory (sizebuf_t *buf, inventory_t *i, qboolean save)
{
	int j, nr = 0;
	invList_t *ic;

	for (j = 0; j < csi.numIDs; j++)
		for (ic = i->c[j]; ic; ic = ic->next)
			nr++;

	Com_DPrintf("CL_SendInventory: Send %i items\n", nr);
	MSG_WriteShort(buf, nr * 6);
	for (j = 0; j < csi.numIDs; j++)
		for (ic = i->c[j]; ic; ic = ic->next)
			CL_SendItem(buf, ic->item, j, ic->x, ic->y, save);
}

static void CL_NetSendItem (struct dbuffer *buf, item_t item, int container, int x, int y, qboolean save)
{
	assert(item.t != NONE);
/*	Com_Printf("Add item %s to container %i (t=%i:a=%i:m=%i) (x=%i:y=%i)\n", csi.ods[item.t].id, container, item.t, item.a, item.m, x, y);*/
	if (save) {
		NET_WriteFormat(buf, "bbbb", item.a, container, x, y);
		NET_WriteRawString(buf, csi.ods[item.t].id);
		if (item.a > NONE_AMMO)
			NET_WriteRawString(buf, csi.ods[item.m].id);
		NET_WriteByte(buf, 0);
	} else
		NET_WriteFormat(buf, "bbbbbb", item.t, item.a, item.m, container, x, y);
}

static void CL_NetSendInventory (struct dbuffer *buf, inventory_t *i, qboolean save)
{
	int j, nr = 0;
	invList_t *ic;

	for (j = 0; j < csi.numIDs; j++)
		for (ic = i->c[j]; ic; ic = ic->next)
			nr++;

	NET_WriteShort(buf, nr * 6);
	for (j = 0; j < csi.numIDs; j++)
		for (ic = i->c[j]; ic; ic = ic->next)
			CL_NetSendItem(buf, ic->item, j, ic->x, ic->y, save);
}

/**
 * @brief
 * @sa CL_SendItem
 * @sa G_WriteItem
 * @sa G_ReadItem
 */
void CL_ReceiveItem (sizebuf_t *buf, item_t *item, int *container, int *x, int *y, qboolean save)
{
	const char *itemID;

	/* reset */
	item->t = item->m = NONE;
	item->a = NONE_AMMO;

	if (save) {
		MSG_ReadFormat(buf, "bbbb", &item->a, container, x, y);
		itemID = MSG_ReadString(buf);
		item->t = INVSH_GetItemByID(itemID);
		if (item->a > NONE_AMMO) {
			itemID = MSG_ReadString(buf);
			item->m = INVSH_GetItemByID(itemID);
		}
	} else
		/* network */
		MSG_ReadFormat(buf, "bbbbbb", &item->t, &item->a, &item->m, container, x, y);
}

void CL_NetReceiveItem (struct dbuffer *buf, item_t *item, int *container, int *x, int *y, qboolean save)
{
	const char *itemID;

	/* reset */
	item->t = item->m = NONE;
	item->a = NONE_AMMO;

	if (save) {
		NET_ReadFormat(buf, "bbbb", &item->a, container, x, y);
		itemID = NET_ReadString(buf);
		item->t = INVSH_GetItemByID(itemID);
		if (item->a > NONE_AMMO) {
			itemID = NET_ReadString(buf);
			item->m = INVSH_GetItemByID(itemID);
		}
	} else
		/* network */
		NET_ReadFormat(buf, "bbbbbb", &item->t, &item->a, &item->m, container, x, y);
}

/**
 * @brief
 * @sa CL_SendInventory
 * @sa CL_ReceiveItem
 * @sa Com_AddToInventory
 * @sa G_SendInventory
 */
void CL_ReceiveInventory (sizebuf_t *buf, inventory_t *i, qboolean save)
{
	item_t item;
	int container, x, y;
	int nr = MSG_ReadShort(buf) / 6;

	Com_DPrintf("CL_ReceiveInventory: Read %i items\n", nr);
	for (; nr-- > 0;) {
		CL_ReceiveItem(buf, &item, &container, &x, &y, save);
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
static void CL_GiveName_f (void)
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
 * @todo: Generate UGV
 * @sa CL_ResetCharacters
 * @param[in] employee The employee to create character data for.
 * @param[in] team Which team to use for creation.
 * @todo fix the assignment of the inventory (assume that you do not know the base yet)
 * @todo fix the assignment of ucn??
 * @todo fix the WholeTeam stuff
 */
void CL_GenerateCharacter (employee_t *employee, const char *team, employeeType_t employeeType)
{
	character_t *chr = NULL;
	char teamDefName[MAX_VAR];
	int teamValue = TEAM_CIVILIAN;

	if (!employee)
		return;

	chr = &employee->chr;
	memset(chr, 0, sizeof(character_t));
	/* default values for human characters */
	chr->weapons = chr->armor = qtrue;

	/* link inventory */
	chr->inv = &employee->inv;
	Com_DestroyInventory(chr->inv);

	/* get ucn */
	chr->ucn = gd.nextUCN++;

	Com_DPrintf("Generate character for team: '%s' (type: %i)\n", team, employeeType);

	/* Backlink from chr to employee struct. */
	chr->empl_type = employeeType;
	chr->empl_idx = employee->idx;

	/* if not human - then we TEAM_ALIEN */
	if (strstr(team, "human"))
		teamValue = TEAM_PHALANX;
	else if (strstr(team, "alien"))
		teamValue = TEAM_ALIEN;

	/* Generate character stats, moels & names. */
	switch (employeeType) {
	case EMPL_SOLDIER:
		chr->rank = CL_GetRank("rifleman");
		/* Create attributes. */
		Com_CharGenAbilitySkills(chr, teamValue);
		/* Get model and name. */
		chr->fieldSize = ACTOR_SIZE_NORMAL;
		chr->skin = Com_GetModelAndName(team, chr);
		break;
	case EMPL_SCIENTIST:
		chr->rank = CL_GetRank("scientist");
		/* Create attributes. */
		Com_CharGenAbilitySkills(chr, teamValue);
		/* Get model and name. */
		Com_sprintf(teamDefName, sizeof(teamDefName), "%s_scientist", team);
		chr->fieldSize = ACTOR_SIZE_NORMAL;
		chr->skin = Com_GetModelAndName(teamDefName, chr);
		break;
	case EMPL_MEDIC:
		chr->rank = CL_GetRank("medic");
		/* Create attributes. */
		Com_CharGenAbilitySkills(chr, teamValue);
		/* Get model and name. */
		Com_sprintf(teamDefName, sizeof(teamDefName), "%s_medic", team);
		chr->fieldSize = ACTOR_SIZE_NORMAL;
		chr->skin = Com_GetModelAndName(teamDefName, chr);
		break;
	case EMPL_WORKER:
		chr->rank = CL_GetRank("worker");
		/* Create attributes. */
		Com_CharGenAbilitySkills(chr, teamValue);
		/* Get model and name. */
		Com_sprintf(teamDefName, sizeof(teamDefName), "%s_worker", team);
		chr->fieldSize = ACTOR_SIZE_NORMAL;
		chr->skin = Com_GetModelAndName(teamDefName, chr);
		break;
	case EMPL_ROBOT:
		chr->rank = CL_GetRank("ugv");
		/* Create attributes. */
		Com_CharGenAbilitySkills(chr, teamValue);
		/* Get model and name. */
		chr->fieldSize = ACTOR_SIZE_UGV;
		Com_sprintf(teamDefName, sizeof(teamDefName), "%s_ugv", team);
		chr->skin = Com_GetModelAndName(teamDefName, chr);
		break;
	default:
		Sys_Error("Unknown employee type\n");
		break;
	}

	chr->HP = GET_HP(chr->skills[ABILITY_POWER]);
	chr->maxHP = chr->HP;
	chr->morale = GET_MORALE(chr->skills[ABILITY_MIND]);
	if (chr->morale >= MAX_SKILL)
		chr->morale = MAX_SKILL;
}


/**
 * @brief
 * @sa CL_GenerateCharacter
 * @sa AIR_ResetAircraftTeam
 */
void CL_ResetCharacters (base_t* const base)
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
		AIR_ResetAircraftTeam(B_GetAircraftFromBaseByIndex(base, i));
	}
}


/**
 * @brief
 */
static void CL_GenerateNames_f (void)
{
	Cbuf_AddText("disconnect\ngame_exit\n");
}


/**
 * @brief Change the name of the selected actor.
 */
static void CL_ChangeName_f (void)
{
	int sel = Cvar_VariableInteger("mn_employee_idx");
	int type = Cvar_VariableInteger("mn_employee_type");

	/* Maybe called without base initialized or active. */
	if (!baseCurrent)
		return;

	if (sel >= 0 && sel < gd.numEmployees[type])
		Q_strncpyz(gd.employees[type][sel].chr.name, Cvar_VariableString("mn_name"), MAX_VAR);

	/* Update mn_employee_namechange so refreshing the list will select current employee. */
	Cvar_SetValue("mn_employee_namechange", 1);
	/* Now refresh the list. */
	Cbuf_AddText(va("employee_init %i;", type));
}


/**
 * @brief Change the skin of the selected actor.
 */
static void CL_ChangeSkin_f (void)
{
	int sel, newSkin;

	sel = cl_selected->integer;
	if (sel >= 0 && sel < gd.numEmployees[EMPL_SOLDIER]) {
		newSkin = Cvar_VariableInteger("mn_skin") + 1;
		if (newSkin >= NUM_TEAMSKINS || newSkin < 0)
			newSkin = 0;

		baseCurrent->curTeam[sel]->skin = newSkin;

		Cvar_SetValue("mn_skin", newSkin);
		Cvar_Set("mn_skinname", CL_GetTeamSkinName(newSkin));
	}
}

/**
 * @brief Use current skin with the team onboard.
 */
static void CL_ChangeSkinOnBoard_f (void)
{
	int i, sel, newSkin, p;
	aircraft_t *aircraft = NULL;

	if (!baseCurrent)
		return;

	if ((baseCurrent->aircraftCurrent >= 0) && (baseCurrent->aircraftCurrent < baseCurrent->numAircraftInBase)) {
		aircraft = &baseCurrent->aircraft[baseCurrent->aircraftCurrent];
	} else {
#ifdef DEBUG
		/* should never happen */
		Com_Printf("CL_ChangeSkinOnBoard_f()... No aircraft selected!\n");
#endif
		return;
	}

	sel = cl_selected->integer;
	if (sel >= 0 && sel < gd.numEmployees[EMPL_SOLDIER]) {
		newSkin = Cvar_VariableInteger("mn_skin");
		if (newSkin >= NUM_TEAMSKINS || newSkin < 0)
			newSkin = 0;
		for (i = 0, p = 0; i < gd.numEmployees[EMPL_SOLDIER]; i++) {
			if (AIR_IsInAircraftTeam(aircraft, i)) {
				assert(p < MAX_ACTIVETEAM);
				baseCurrent->curTeam[p]->skin = newSkin;
				p++;
			}
		}
	}
}

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
 * @brief
 */
void CL_AddCarriedToEq (equipDef_t * ed)
{
	invList_t *ic, *next;
	int p, container;

	assert(baseCurrent);
	assert((baseCurrent->aircraftCurrent >= 0) && (baseCurrent->aircraftCurrent < baseCurrent->numAircraftInBase));

	for (container = 0; container < csi.numIDs; container++) {
		for (p = 0; p < baseCurrent->teamNum[baseCurrent->aircraftCurrent]; p++) {
			for (ic = baseCurrent->curTeam[p]->inv->c[container];
				 ic; ic = next) {
				item_t item = ic->item;
				int type = item.t;

				next = ic->next;
				ed->num[type]++;
				if (item.a) {
					assert(csi.ods[type].reload);
					assert(item.m != NONE);
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
 * @sa CL_ReloadAndRemoveCarried
 */
static item_t CL_AddWeaponAmmo (equipDef_t * ed, item_t item)
{
	int i = -1, type = item.t;	/* 'type' equals idx in "&csi.ods[idx]" */

	assert(ed->num[type] > 0);
	ed->num[type]--;

	if (csi.ods[type].weap_idx[0] >= 0) {
		/* The given item is ammo or self-contained weapon (i.e. It has firedefinitions. */
		if (csi.ods[type].oneshot) {
			/* "Recharge" the oneshot weapon. */
			item.a = csi.ods[type].ammo;
			item.m = item.t; /* Just in case this hasn't been done yet. */
			Com_DPrintf("CL_AddWeaponAmmo: oneshot weapon '%s'.\n", csi.ods[type].id);
			return item;
		} else {
			/* No change, nothing needs to be done to this item. */
			return item;
		}
	} else if (!csi.ods[type].reload) {
		/* The given item is a weapon but no ammo is needed,
		 * so fire definitions are in t (the weapon). Setting equal. */
		item.m = item.t;
		return item;
	} else if (item.a) {
		assert(item.m != NONE);
		/* The item is a weapon and it was reloaded one time. */
		if (item.a == csi.ods[type].ammo) {
			/* Fully loaded, no need to reload, but mark the ammo as used. */
			assert(item.m != NONE);	/* @todo: Isn't this redundant here? */
			if (ed->num[item.m] > 0) {
				ed->num[item.m]--;
				return item;
			} else {
				/* Your clip has been sold; give it back. */
				item.a = NONE_AMMO;
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

	/* Search for any complete clips. */
	for (i = 0; i < csi.numODs; i++) {
		if (INV_LoadableInWeapon(&csi.ods[i], type)) {
			if (ed->num[i] > 0) {
				ed->num[i]--;
				item.a = csi.ods[type].ammo;
				item.m = i;
				return item;
			}
		}
	}

	/* @todo: on return from a mission with no clips left
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
	item.a = NONE_AMMO;
	for (i = 0; i < csi.numODs; i++) {
		if (INV_LoadableInWeapon(&csi.ods[i], type)
		&& (ed->num_loose[i] > item.a) ) {
			if (item.a > 0) {
				/* We previously found some ammo, but we've now found other */
				/* loose ammo of a different (but appropriate) type with */
				/* more bullets.  Put the previously found ammo back, so */
				/* we'll take the new type. */
				assert(item.m != NONE);
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
 * @brief Reloads weapons and removes "not assigned" ones from containers.
 * @param[in] *aircraft	Pointer to an aircraft for given team.
 * @param[in] *ed equipDef_t pointer to equipment of given character in a team.
 * @sa CL_AddWeaponAmmo
 */
void CL_ReloadAndRemoveCarried (aircraft_t *aircraft, equipDef_t * ed)
{
	base_t *base;
	character_t *cp;
	invList_t *ic, *next;
	int p, container;

	/* Iterate through in container order (right hand, left hand, belt, */
	/* holster, backpack) at the top level, i.e. each squad member reloads */
	/* her right hand, then each reloads his left hand, etc. The effect */
	/* of this is that when things are tight, everyone has the opportunity */
	/* to get their preferred weapon(s) loaded before anyone is allowed */
	/* to keep her spares in the backpack or on the floor. We don't want */
	/* the first person in the squad filling their backpack with spare ammo */
	/* leaving others with unloaded guns in their hands... */

	assert(aircraft);
	base = aircraft->homebase;
	assert(base);

	Com_Printf("CL_ReloadAndRemoveCarried()...aircraft idx: %i, teamNum: %i\n",
	aircraft->idx, base->teamNum[aircraft->idx]);

	for (container = 0; container < csi.numIDs; container++) {
		for (p = 0; p < base->teamNum[aircraft->idx]; p++) {
			cp = base->curTeam[p];
			assert(cp);
			for (ic = cp->inv->c[container]; ic; ic = next) {
				next = ic->next;
				if (ed->num[ic->item.t] > 0) {
					ic->item = CL_AddWeaponAmmo(ed, ic->item);
				} else {
					/* Drop ammo used for reloading and sold carried weapons. */
					Com_RemoveFromInventory(cp->inv, container, ic->x, ic->y);
				}
			}
		}
	}
}

/**
 * @brief Clears all containers that are temp containers (see script definition).
 * @sa CL_GenerateEquipment_f
 * @sa CL_ResetTeamInBase
 * @sa CL_SaveTeamInfo
 * @sa CL_SendCurTeamInfo
 */
void CL_CleanTempInventory (base_t* base)
{
	int i, k;

	if (!base)
		return;

	Com_DestroyInventory(&base->equipByBuyType);
	for (i = 0; i < MAX_EMPLOYEES; i++)
		for (k = 0; k < csi.numIDs; k++)
			if (csi.ids[k].temp)
				/* idFloor and idEquip are temp */
				gd.employees[EMPL_SOLDIER][i].inv.c[k] = NULL;
}

/**
 * @brief Displays actor equipment and unused items in proper buytype category.
 * @note This function is called everytime the equipment screen for the team pops up.
 * @sa CL_UpdatePointersInGlobalData
 */
static void CL_GenerateEquipment_f (void)
{
	equipDef_t unused;
	int i, p;
	aircraft_t *aircraft;
	/* t value will be set below - a and m are not changed here */
	item_t item = {NONE_AMMO, NONE, NONE};
	int team = 0;

	assert(baseCurrent);
	assert((baseCurrent->aircraftCurrent >= 0) && (baseCurrent->aircraftCurrent < baseCurrent->numAircraftInBase));

	/* Popup if no soldiers are assigned to the current aircraft. */
	/* if ( !baseCurrent->numHired) { */
	if (!baseCurrent->teamNum[baseCurrent->aircraftCurrent]) {
		MN_PopMenu(qfalse);
		return;
	}

	/* Get team. */
	if (strstr(Cvar_VariableString("team"), "human")) {
		team = 0;
		Com_DPrintf("CL_GenerateEquipment_f().. team human, id: %i\n", team);
	} else if (strstr(Cvar_VariableString("team"), "alien")) {
		team = 1;
		Com_DPrintf("CL_GenerateEquipment_f().. team alien, id: %i\n", team);
	}

	aircraft = &baseCurrent->aircraft[baseCurrent->aircraftCurrent];

	/* Store hired names. */
	Cvar_ForceSet("cl_selected", "0");
	for (i = 0, p = 0; i < gd.numEmployees[EMPL_SOLDIER]; i++)
		if (CL_SoldierInAircraft(i, aircraft->idx)) {
			assert(p < MAX_ACTIVETEAM);
			/* maybe we already have soldiers in this aircraft */
			baseCurrent->curTeam[p] = E_GetHiredCharacter(baseCurrent, EMPL_SOLDIER, i);
			if (!baseCurrent->curTeam[p])
				Sys_Error("CL_GenerateEquipment_f: Could not get employee character with idx: %i\n", p);
			Com_DPrintf("add %s to curTeam (pos: %i)\n", baseCurrent->curTeam[p]->name, p);
			Cvar_ForceSet(va("mn_name%i", p), baseCurrent->curTeam[p]->name);
			p++;
/*			if (p >= cl_numnames->integer)
				break;*/
		}

	if (p != baseCurrent->teamNum[baseCurrent->aircraftCurrent])
		Sys_Error("CL_GenerateEquipment_f: numEmployees: %i, teamNum[%i]: %i, p: %i\n",
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
	menuText[TEXT_STANDARD] = NULL;

	/* manage inventory */
	unused = baseCurrent->storage; /* copied, including arrays inside! */

	CL_CleanTempInventory(baseCurrent);
	CL_ReloadAndRemoveCarried(aircraft, &unused);

	/* a 'tiny hack' to add the remaining equipment (not carried)
	   correctly into buy categories, reloading at the same time;
	   it is valid only due to the following property: */
	assert(MAX_CONTAINERS >= BUY_AIRCRAFT);

	for (i = 0; i < csi.numODs; i++) {
		/* Don't allow to show armours for other teams in the menu. */
		if ((Q_strncmp(csi.ods[i].type, "armor", MAX_VAR) == 0) && (csi.ods[i].useable != team))
			continue;
		/* Don't allow to show unresearched items. */
		if (!RS_IsResearched_ptr(csi.ods[i].tech))
			continue;
		while (unused.num[i]) {
			item.t = i;

			assert(unused.num[i] > 0);

			/* Check if there are any "multi_ammo" items and move them to the PRI container (along with PRI items of course).
			 * Otherwise just use the container-buytype of the item.
			 * HACKHACK
			 */
			if (BUY_PRI(csi.ods[i].buytype)) {
				if (!Com_TryAddToBuyType(&baseCurrent->equipByBuyType, CL_AddWeaponAmmo(&unused, item), BUY_WEAP_PRI))
					break; /* no space left in menu */
			} else {
				if (!Com_TryAddToBuyType(&baseCurrent->equipByBuyType, CL_AddWeaponAmmo(&unused, item), csi.ods[i].buytype))
					break; /* no space left in menu */
			}
		}
	}
}

/**
 * @brief Moves all  items marked with "buytype multi_ammo" to the currently used equipment-container (pri or sec)
 * @note This is a WORKAROUND, it is by no means efficient or sane, but currently the only way to display these items in the (multiple) correct categories.
 * Should be executed on a change of the equipemnt-category to either PRI or SEC items .. and only there.
 * @param[in|out] inv This is always the used equipByBuyType in the base.
 * @param[in] buytype_container The container we just switched to (where all the items should be moved to).
 * @sa CL_GenerateEquipment_f
 * @sa Com_MoveInInventoryIgnore
 * @note Some ic->next and ic will be modivied by Com_MoveInInventoryIgnore.
 * @note HACKHACK
 */
static void CL_MoveMultiEquipment (inventory_t* const inv, int buytype_container)
{
	int container;
	invList_t *ic = NULL;
	invList_t *ic_temp = NULL;

	if (!inv)
		return;

	/* Do nothing if no pri/sec category is shown. */
	if ((buytype_container != BUY_WEAP_PRI) && (buytype_container != BUY_WEAP_SEC))
		return;

	/* Set source container to the one that is not the destination container. */
	container = (buytype_container == BUY_WEAP_PRI)
		? BUY_WEAP_SEC
		: BUY_WEAP_PRI;

	/* This is a container that might hold some of the affected items.
	 * Move'em to the target (buytype_container) container (if there are any)
	 */
	Com_DPrintf("CL_MoveMultiEquipment: buytype_container:%i\n", buytype_container);
	Com_DPrintf("CL_MoveMultiEquipment: container:%i\n", container);
	ic = inv->c[container];
	while (ic) {
		if (csi.ods[ic->item.t].buytype == BUY_MULTI_AMMO) {
			ic_temp = ic->next;
			Com_MoveInInventoryIgnore(inv, container, ic->x, ic->y, buytype_container, NONE, NONE, NULL, &ic, qtrue); /**< @todo Does the function work like this? */
			ic = ic_temp;
		} else {
			ic = ic->next;
		}
	}
}

/**
 * @brief Sets buytype category for equip menu.
 * @note num = buytype/equipment category.
 */
static void CL_EquipType_f (void)
{
	int num;

	if (Cmd_Argc() < 2) {
		Com_Printf("Usage: equip_type <category>\n");
		return;
	}

	/* read and range check */
	num = atoi(Cmd_Argv(1));
	if (num < 0 && num >= BUY_MULTI_AMMO)
		return;


	/* display new items */
	baseCurrent->equipType = num;
	if (menuInventory) {
		CL_MoveMultiEquipment (&baseCurrent->equipByBuyType, num); /**< Move all multi-ammo items to the current container. */
		menuInventory->c[csi.idEquip] = baseCurrent->equipByBuyType.c[num];
	}
}

typedef enum {
	SELECT_MODE_SOLDIER,
	SELECT_MODE_EQUIP,
	SELECT_MODE_TEAM
} selectSoldierModes_t;

/**
 * @brief
 * @note This function has various console commands:
 * team_select, soldier_select, equip_select
 */
static void CL_Select_f (void)
{
	const char *arg;
	char command[MAX_VAR];
	character_t *chr;
	int num;
	selectSoldierModes_t mode;

	/* check syntax */
	if (Cmd_Argc() < 2) {
		Com_Printf("Usage: %s <num>\n", Cmd_Argv(0));
		return;
	}

	num = atoi(Cmd_Argv(1));
	/* don't select the same actor twice */
/*	if (num == cl_selected->integer)
		return;*/

	/* change highlights */
	arg = Cmd_Argv(0);
	/* there must be a _ in the console command */
	*strchr(arg, '_') = 0;
	Q_strncpyz(command, arg, sizeof(command));

	if (!Q_strcmp(command, "soldier"))
		mode = SELECT_MODE_SOLDIER;
	else if (!Q_strcmp(command, "equip"))
		mode = SELECT_MODE_EQUIP;
	else if (!Q_strcmp(command, "team"))
		mode = SELECT_MODE_TEAM;

	switch (mode) {
	case SELECT_MODE_SOLDIER:
		/* check whether we are connected (tactical mission) */
		if (CL_OnBattlescape()) {
			CL_ActorSelectList(num);
			return;
		}
		/* we are still in the menu - so fall through */
	case SELECT_MODE_EQUIP:
		/* no base or no aircraft selected */
		if (!baseCurrent || baseCurrent->aircraftCurrent < 0)
			return;
		/* no soldiers in the current aircraft */
		if (baseCurrent->teamNum[baseCurrent->aircraftCurrent] <= 0) {
			/* multiplayer - load a team first */
			if (!ccs.singleplayer)
				Cbuf_AddText("mn_pop;mn_push mp_team;");
			return;
		/* not that many soldiers in the aircraft */
		} else if (num >= baseCurrent->teamNum[baseCurrent->aircraftCurrent])
			return;

		/* update menu inventory */
		if (menuInventory && menuInventory != baseCurrent->curTeam[num]->inv) {
			baseCurrent->curTeam[num]->inv->c[csi.idEquip] = menuInventory->c[csi.idEquip];
			/* set 'old' idEquip to NULL */
			menuInventory->c[csi.idEquip] = NULL;
		}
		menuInventory = baseCurrent->curTeam[num]->inv;
		chr = baseCurrent->curTeam[num];
		break;
	case SELECT_MODE_TEAM:
		if (!baseCurrent || num >= E_CountHired(baseCurrent, EMPL_SOLDIER)) {
			/*Com_Printf("num: %i / max: %i\n", num, E_CountHired(baseCurrent, EMPL_SOLDIER));*/
			return;
		}
		chr = E_GetHiredCharacter(baseCurrent, EMPL_SOLDIER, -(num + 1));
		if (!chr)
			Sys_Error("CL_Select_f: No hired character at pos %i (base: %i)\n", num, baseCurrent->idx);
		break;
	default:
		Sys_Error("Unknown select mode %i\n", mode);
	}

	if (mode == SELECT_MODE_SOLDIER) {
		/* HACK */
		/* deselect current selected soldier and select the new one - these are confuncs */
		Cbuf_ExecuteText(EXEC_NOW, va("teamdeselect%i", cl_selected->integer));
		Cbuf_ExecuteText(EXEC_NOW, va("teamselect%i", num));
		Cbuf_ExecuteText(EXEC_NOW, va("equipdeselect%i", cl_selected->integer));
		Cbuf_ExecuteText(EXEC_NOW, va("equipselect%i", num));
	} else {
		/* deselect current selected soldier and select the new one - these are confuncs */
		Cbuf_ExecuteText(EXEC_NOW, va("%sdeselect%i", command, cl_selected->integer));
		Cbuf_ExecuteText(EXEC_NOW, va("%sselect%i", command, num));
	}
	/* now set the cl_selected cvar to the new actor id */
	Cvar_ForceSet("cl_selected", va("%i", num));

	Com_DPrintf("CL_Select_f: Command: '%s' - num: %i\n", command, num);

	/* set info cvars */
	/* FIXME: This isn't true ACTOR_SIZE_NORMAL has nothing to do with ugvs anymore - old code */
	if (chr->fieldSize == ACTOR_SIZE_NORMAL)
		CL_CharacterCvars(chr);
	else
		CL_UGVCvars(chr);
}

/**
 * @brief implements the "nextsoldier" command
 */
static void CL_NextSoldier_f (void)
{
	if (CL_OnBattlescape()) {
		CL_ActorSelectNext();
	}
}


/**
 * @brief Updates data about teams in aircrafts.
 * @param[in] *aircraft Pointer to an aircraft for a desired team.
 */
void CL_UpdateHireVar (aircraft_t *aircraft)
{
	int i, p;
	int hired_in_base;
	employee_t *employee = NULL;
	base_t *base = NULL;

	assert(aircraft);
	base = aircraft->homebase;
	assert(base);

	Cvar_Set("mn_hired", va(_("%i of %i"), base->teamNum[aircraft->idxInBase], aircraft->size));
	hired_in_base = E_CountHired(base, EMPL_SOLDIER);

	/* Uncomment this Com_Printf here for better output in case of problems. */
	/* Com_Printf("CL_UpdateHireVar()... aircraft idx: %i, homebase: %s\n", aircraft->idx, base->name); */

	/* update curTeam list */
	for (i = 0, p = 0; i < hired_in_base; i++) {
		employee = E_GetHiredEmployee(base, EMPL_SOLDIER, -(i+1));
		if (!employee)
			Sys_Error("CL_UpdateHireVar: Could not get employee %i\n", i);

		if (CL_SoldierInAircraft(employee->idx, aircraft->idx)) {
			base->curTeam[p] = &employee->chr;
			p++;
		}
	}
	if (p != base->teamNum[aircraft->idxInBase])
		Sys_Error("CL_UpdateHireVar: SoldiersInBase: %i, teamNum[%i]: %i, p: %i\n",
				hired_in_base,
				aircraft->idxInBase,
				base->teamNum[aircraft->idxInBase],
				p);

	for (; p < MAX_ACTIVETEAM; p++)
		base->curTeam[p] = NULL;
}

/**
 * @brief Multiplayer-only call to reset team and team inventory (when setting up new team).
 * @sa E_ResetEmployees
 * @sa CL_CleanTempInventory
 * @note We need baseCurrent to point to gd.bases[0] here.
 * @note Available via script command team_reset.
 * @note Called when initializing the multiplayer menu (for init node and new team button).
 */
void CL_ResetTeamInBase (void)
{
	employee_t* employee;

	if (ccs.singleplayer)
		return;

	Com_DPrintf("Reset of baseCurrent team flags.\n");
	if (!baseCurrent) {
		Com_DPrintf("CL_ResetTeamInBase: No baseCurrent\n");
		return;
	}

	CL_CleanTempInventory(baseCurrent);

	baseCurrent->teamNum[0] = 0;
	AIR_ResetAircraftTeam(B_GetAircraftFromBaseByIndex(baseCurrent,0));

	E_ResetEmployees();
	while (gd.numEmployees[EMPL_SOLDIER] < cl_numnames->integer) {
		employee = E_CreateEmployee(EMPL_SOLDIER);
		employee->hired = qtrue;
		employee->baseIDHired = baseCurrent->idx;
		Com_DPrintf("CL_ResetTeamInBase: Generate character for multiplayer - employee->chr.name: '%s' (base: %i)\n", employee->chr.name, baseCurrent->idx);
	}

	/* reset the multiplayer inventory; stored in baseCurrent->storage */
	{
		equipDef_t *ed;
		const char *name;
		int i;

		/* search equipment definition */
		name = "multiplayer";
		Com_DPrintf("CL_ResetTeamInBase: no curCampaign - using equipment '%s'\n", name);
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
static void CL_MarkTeam_f (void)
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
	if (baseCurrent->aircraftCurrent < 0) {
		Com_Printf("No aircraft selected\n");
		MN_PopMenu(qfalse);
		return;
	}

	aircraft = &baseCurrent->aircraft[baseCurrent->aircraftCurrent];
	CL_UpdateHireVar(aircraft);

	for (i = 0; i < gd.numEmployees[EMPL_SOLDIER]; i++) {
		alreadyInOtherShip = qfalse;
		/* don't show other base's recruits or none hired ones */
		if (!gd.employees[EMPL_SOLDIER][i].hired || gd.employees[EMPL_SOLDIER][i].baseIDHired != baseCurrent->idx)
			continue;

		/* search all aircrafts except the current one */
		for (j = 0; j < gd.numAircraft; j++) {
			if (j == aircraft->idx)
				continue;
			/* already on another aircraft */
			if (CL_SoldierInAircraft(i, j))
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
			/* FIXME: Wouldn't it be better here to check for temp containers */
			if (j != csi.idFloor && j != csi.idEquip && gd.employees[EMPL_SOLDIER][i].inv.c[j])
				break;
		}

		if (j < csi.numIDs)
			Cbuf_AddText(va("listholdsequip%i\n", k));
		else
			Cbuf_AddText(va("listholdsnoequip%i\n", k));
		k++;
		if (k >= cl_numnames->integer)
			break;
	}

	for (;k < cl_numnames->integer; k++) {
		Cbuf_AddText(va("listdisable%i\n", k));
		Cvar_ForceSet(va("mn_name%i", k), "");
		Cbuf_AddText(va("listholdsnoequip%i\n", k));
	}
}

/**
 * @brief Tells you if a soldier is assigned to an aircraft.
 * @param[in] employee_idx The index of the soldier in the global list.
 * @param[in] aircraft_idx The global index of the aircraft. use -1 to check if the soldier is in _any_ aircraft.
 * @return qboolean qtrue if the soldier was found in the aircraft(s) else: qfalse.
 * @todo params should be employee_t* and aircraft_t*
 */
qboolean CL_SoldierInAircraft (int employee_idx, int aircraft_idx)
{
	int i;
	aircraft_t* aircraft;

	if (employee_idx < 0 || employee_idx > gd.numEmployees[EMPL_SOLDIER])
		return qfalse;

	if (aircraft_idx < 0) {
		/* Search if he is in _any_ aircraft and return true if it's the case. */
		for (i = 0; i < gd.numAircraft; i++) {
			if (CL_SoldierInAircraft(employee_idx, i))
				return qtrue;
		}
		return qfalse;
	}

	aircraft = AIR_AircraftGetFromIdx(aircraft_idx);
	return AIR_IsInAircraftTeam(aircraft, employee_idx);
}

/**
 * @brief Tells you if a employee is away from his home base (gone in mission).
 * @param[in] employee Pointer to the employee.
 * @return qboolean qtrue if the employee is away in mission, qfalse if he is not or he is unhired.
 */
qboolean CL_SoldierAwayFromBase (employee_t *employee)
{
	int i;
	aircraft_t* aircraft;
	base_t *base;

	assert(employee);

	/* Check that employee is hired */
	if (employee->baseIDHired < 0)
		return qfalse;

	/* for now only soldiers can be assigned to aircrafts */
	/* @todo add pilots and UGVs */
	if (employee->type != EMPL_SOLDIER)
		return qfalse;

	base = &gd.bases[employee->baseIDHired];
	assert(base);

	for (i = 0; i < base->numAircraftInBase; i++) {
		aircraft = &base->aircraft[i];
		assert(aircraft);

		if (aircraft->status > AIR_HOME && AIR_IsInAircraftTeam(aircraft, employee->idx))
			return qtrue;
	}

	return qfalse;
}

/**
 * @brief Removes a soldier from an aircraft.
 * @param[in] employee_idx The index of the soldier in the global list.
 * @param[in] aircraft_idx The global index of aircraft. Use -1 to remove the soldier from any aircraft.
 * @param[in] base_idx The index of base where employee is hired.
 */
void CL_RemoveSoldierFromAircraft (int employee_idx, int aircraft_idx, int base_idx)
{
	aircraft_t *aircraft;
	base_t *base;
	int i;

	if (employee_idx < 0)
		return;

	base = &gd.bases[base_idx];
	assert (base);

	if (aircraft_idx < 0) {
		/* Search if he is in _any_ aircraft and set the aircraft_idx if found.. */
		for (i = 0; i < gd.numAircraft; i++) {
			if (CL_SoldierInAircraft(employee_idx, i) ) {
				aircraft_idx = i;
				break;
			}
		}
		if (aircraft_idx < 0)
			return;
	}

	aircraft = AIR_AircraftGetFromIdx(aircraft_idx);

	assert(base == (base_t *) aircraft->homebase);

	Com_DPrintf("CL_RemoveSoldierFromAircraft: base: %i - aircraftID: %i - aircraft_idx: %i\n", base->idx, aircraft->idx, aircraft_idx);

	Com_DestroyInventory(&gd.employees[EMPL_SOLDIER][employee_idx].inv);
	AIR_RemoveFromAircraftTeam(aircraft, employee_idx);
	baseCurrent->teamNum[aircraft->idxInBase]--;
}

/**
 * @brief Removes all soldiers from an aircraft.
 * @param[in] aircraft_idx The global index of aircraft.
 * @param[in] base_idx The index of the base the aircraft is located in.
 */
void CL_RemoveSoldiersFromAircraft (int aircraft_idx, int base_idx)
{
	int i = 0;
	base_t *base = NULL;
	aircraft_t* aircraft;

	if (base_idx < 0)
		return;

	aircraft = AIR_AircraftGetFromIdx(aircraft_idx);
	base = &gd.bases[base_idx];
	assert (base);

	if (aircraft->idxInBase < 0 || aircraft->idxInBase >= base->numAircraftInBase)
		return;

	/* Counting backwards because teamNum[aircraft->idx] is changed in CL_RemoveSoldierFromAircraft */
	for (i = base->teamNum[aircraft->idxInBase]-1; i >= 0; i--) {
		/* use global aircraft index here */
		CL_RemoveSoldierFromAircraft(i, aircraft_idx, base_idx);
	}
}

/**
 * @brief Assigns a soldier to an aircraft.
 * @param[in] employee_idx The index of the soldier in the global list.
 * @param[in] aircraft_idx The index of aircraft in the base.
 * @return returns true if a soldier could be assigned to the aircraft.
 */
static qboolean CL_AssignSoldierToAircraft (int employee_idx, int aircraft_idx)
{
	aircraft_t *aircraft = NULL;
	if (employee_idx < 0 || aircraft_idx < 0)
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
			AIR_AddToAircraftTeam(aircraft, employee_idx);
			baseCurrent->teamNum[aircraft_idx]++;
			return qtrue;
		}
#ifdef DEBUG
	} else {
		Com_DPrintf("CL_AssignSoldierToAircraft: aircraft full - not added\n");
#endif
	}
	return qfalse;
}


#ifdef DEBUG
/**
 * @brief
 */
static void CL_TeamListDebug_f (void)
{
	int i, cnt;
	character_t *team;
	employee_t *employee;

	if (!baseCurrent) {
		Com_Printf("Build and select a base first\n");
		return;
	}

	if (!baseCurrent->aircraftCurrent < 0) {
		Com_Printf("Select an aircraft\n");
		return;
	}

	cnt = B_GetNumOnTeam();
	team = baseCurrent->curTeam[baseCurrent->aircraftCurrent];

	Com_Printf("%i members in the current team", cnt);
	for (i = 0; i < cnt; i++) {
		employee = E_GetEmployeeFromChrUCN(team->ucn);
		if (!employee)
			Com_Printf("Could not find a valid employee for ucn %i\n", team->ucn);
		else
			Com_Printf("ucn %i - employee->idx: %i\n", team->ucn, employee->idx);
		team++;
	}
}
#endif

/**
 * @brief Adds or removes a soldier to/from an aircraft.
 */
static void CL_AssignSoldier_f (void)
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

	/* baseCurrent is checked here */
	if (num >= E_CountHired(baseCurrent, EMPL_SOLDIER) || num >= cl_numnames->integer) {
		/*Com_Printf("num: %i, max: %i\n", num, E_CountHired(baseCurrent, EMPL_SOLDIER));*/
		return;
	}

	employee = E_GetHiredEmployee(baseCurrent, EMPL_SOLDIER, -(num+1));
	if (!employee)
		Sys_Error("CL_AssignSoldier_f: Could not get employee %i\n", num);

	Com_DPrintf("CL_AssignSoldier_f: employee with idx %i selected\n", employee->idx);
	aircraft = &baseCurrent->aircraft[baseCurrent->aircraftCurrent];
	Com_DPrintf("aircraft->idx: %i - aircraft->idxInBase: %i\n", aircraft->idx, aircraft->idxInBase);
	assert(aircraft->idxInBase == baseCurrent->aircraftCurrent);

	if (CL_SoldierInAircraft(employee->idx, aircraft->idx)) {
		Com_DPrintf("CL_AssignSoldier_f: removing\n");
		/* Remove soldier from aircraft/team. */
		Cbuf_AddText(va("listdel%i\n", num));
		/* use the global aircraft index here */
		CL_RemoveSoldierFromAircraft(employee->idx, aircraft->idx, baseCurrent->idx);
		Cbuf_AddText(va("listholdsnoequip%i\n", num));
	} else {
		Com_DPrintf("CL_AssignSoldier_f: assigning\n");
		/* Assign soldier to aircraft/team if aircraft is not full */
		if (CL_AssignSoldierToAircraft(employee->idx, aircraft->idxInBase))
			Cbuf_AddText(va("listadd%i\n", num));
	}
	/* Select the desired one anyways. */
	CL_UpdateHireVar(aircraft);
	Cbuf_AddText(va("team_select %i\n", num));
}

#define MPTEAM_SAVE_FILE_VERSION 1

/**
 * @brief Saves a team
 * @sa CL_SaveTeamInfo
 */
static qboolean CL_SaveTeamMultiplayer (const char *filename)
{
	sizebuf_t sb;
	byte buf[MAX_TEAMDATASIZE];
	const char *name;
	aircraft_t *aircraft;
	int i, res;

	assert(baseCurrent);
	aircraft = &baseCurrent->aircraft[baseCurrent->aircraftCurrent];

	/* create data */
	SZ_Init(&sb, buf, MAX_TEAMDATASIZE);
	MSG_WriteByte(&sb, MPTEAM_SAVE_FILE_VERSION);

	name = Cvar_VariableString("mn_teamname");
	if (!strlen(name))
		Cvar_Set("mn_teamname", _("NewTeam"));
	/* store teamname */
	MSG_WriteString(&sb, name);

	/* store team */
	CL_SaveTeamInfo(&sb, baseCurrent->idx, E_CountHired(baseCurrent, EMPL_SOLDIER));

	/* store assignment */
	MSG_WriteByte(&sb, baseCurrent->teamNum[0]);

	/* store aircraft soldier content for multi-player */
	MSG_WriteByte(&sb, aircraft->size);
	for (i = 0; i < aircraft->size; i++) {
		if (aircraft->teamIdxs[i] >= 0)
			MSG_WriteByte(&sb, aircraft->teamIdxs[i]);
	}

	/* store equipment in baseCurrent so soldiers can be properly equipped */
	MSG_WriteShort(&sb, csi.numODs);
	for (i = 0; i < csi.numODs; i++) {
		MSG_WriteString(&sb, csi.ods[i].id);
		MSG_WriteLong(&sb, baseCurrent->storage.num[i]);
		MSG_WriteByte(&sb, baseCurrent->storage.num_loose[i]);
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
	char filename[MAX_OSPATH];

	/* save */
	Com_sprintf(filename, sizeof(filename), "%s/save/team%s.mpt", FS_Gamedir(), Cvar_VariableString("mn_slot"));
	if (!CL_SaveTeamMultiplayer(filename))
		MN_Popup(_("Note"), _("Error saving team. Check free disk space!"));
}

/**
 * @brief Load a team member for multiplayer
 * @sa CL_LoadTeamMultiplayer
 * @sa CL_SaveTeamInfo
 */
static void CL_LoadTeamMultiplayerMember (sizebuf_t * sb, character_t * chr, int version)
{
	int i, num;

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
	chr->category = MSG_ReadByte(sb);
	chr->gender = MSG_ReadByte(sb);
	chr->STUN = MSG_ReadByte(sb);
	chr->AP = MSG_ReadByte(sb);
	chr->morale = MSG_ReadByte(sb);

	/* new attributes */
	num = MSG_ReadByte(sb);
	for (i = 0; i < num; i++)
		chr->skills[i] = MSG_ReadByte(sb);

	/* load scores */
	num = MSG_ReadByte(sb);
	for (i = 0; i < num; i++)
		chr->kills[i] = MSG_ReadShort(sb);
	chr->assigned_missions = MSG_ReadShort(sb);

	/* inventory */
	Com_DestroyInventory(chr->inv);
	CL_ReceiveInventory(sb, chr->inv, qtrue);
}

/**
 * @brief Load a multiplayer team
 * @sa CL_LoadTeamMultiplayer
 * @sa CL_SaveTeamMultiplayer
 */
static void CL_LoadTeamMultiplayer (const char *filename)
{
	sizebuf_t sb;
	byte buf[MAX_TEAMDATASIZE];
	FILE *f;
	int version;
	character_t *chr;
	employee_t *employee;
	aircraft_t *aircraft;
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

	version = MSG_ReadByte(&sb);

	/* load the teamname */
	Cvar_Set("mn_teamname", MSG_ReadString(&sb));

	/* load the team */
	/* reset data */
	CL_ResetCharacters(&gd.bases[0]);

	/* read whole team list */
	num = MSG_ReadByte(&sb);
	Com_DPrintf("load %i teammembers\n", num);
	E_ResetEmployees();
	for (i = 0; i < num; i++) {
		/* New employee */
		employee = E_CreateEmployee(EMPL_SOLDIER);
		employee->hired = qtrue;
		employee->baseIDHired = gd.bases[0].idx;
		chr = &employee->chr;
		CL_LoadTeamMultiplayerMember(&sb, chr, version);
	}

	/* get assignment */
	gd.bases[0].teamNum[0] = MSG_ReadByte(&sb);

	/* get aircraft soldier content for multi-player */
	aircraft = &gd.bases[0].aircraft[0];
	Com_DPrintf("Multiplayer aircraft IDX = %i\n", aircraft->idx);
	aircraft->size = MSG_ReadByte(&sb);
	for (i = 0; i < aircraft->size; i++) {
		if (i < gd.bases[0].teamNum[0])
			aircraft->teamIdxs[i] = MSG_ReadByte(&sb);
		else
			aircraft->teamIdxs[i] = -1;
	}

	/* read equipment */
	p = MSG_ReadShort(&sb);
	for (i = 0; i < p; i++) {
		num = INVSH_GetItemByID(MSG_ReadString(&sb));
		if (num == -1) {
			MSG_ReadLong(&sb);
			MSG_ReadByte(&sb);
		} else {
			gd.bases[0].storage.num[num] = MSG_ReadLong(&sb);
			gd.bases[0].storage.num_loose[num] = MSG_ReadByte(&sb);
		}
	}

	for (i = 0, p = 0; i < num; i++)
		if (CL_SoldierInAircraft(i, 0))
			gd.bases[0].curTeam[p++] = E_GetHiredCharacter(&gd.bases[0], EMPL_SOLDIER, i);

	/* gd.bases[0].numHired = p; */

	Com_DPrintf("Load team with %i members and %i slots\n", p, num);

	for (;p < MAX_ACTIVETEAM; p++)
		gd.bases[0].curTeam[p] = NULL;
}

/**
 * @brief Loads the selected teamslot
 */
static void CL_LoadTeamMultiplayerSlot_f (void)
{
	char filename[MAX_OSPATH];

	if (ccs.singleplayer) {
		Com_Printf("Only for multiplayer\n");
		return;
	}

	/* load */
	Com_sprintf(filename, sizeof(filename), "%s/save/team%s.mpt", FS_Gamedir(), Cvar_VariableString("mn_slot"));
	CL_LoadTeamMultiplayer(filename);

	Com_Printf("Team 'team%s' loaded.\n", Cvar_VariableString("mn_slot"));
}

/**
 * @brief Call all the needed functions to generate a new initial team (e.g. for multiplayer)
 */
static void CL_GenerateNewTeam_f (void)
{
	CL_ResetTeamInBase();
	Cvar_Set("mn_teamname", _("NewTeam"));
	CL_GenerateNames_f();
	MN_PushMenu("team");
}

/**
 * @brief
 */
void CL_ResetTeams (void)
{
	Cmd_AddCommand("new_team", CL_GenerateNewTeam_f, NULL);
	Cmd_AddCommand("givename", CL_GiveName_f, NULL);
	Cmd_AddCommand("gennames", CL_GenerateNames_f, NULL);
	Cmd_AddCommand("team_reset", CL_ResetTeamInBase, NULL);
	Cmd_AddCommand("genequip", CL_GenerateEquipment_f, NULL);
	Cmd_AddCommand("equip_type", CL_EquipType_f, NULL);
	Cmd_AddCommand("team_mark", CL_MarkTeam_f, NULL);
	Cmd_AddCommand("team_hire", CL_AssignSoldier_f, NULL);
	Cmd_AddCommand("team_select", CL_Select_f, NULL);
	Cmd_AddCommand("team_changename", CL_ChangeName_f, NULL);
	Cmd_AddCommand("team_changeskin", CL_ChangeSkin_f, NULL);
	Cmd_AddCommand("team_changeskinteam", CL_ChangeSkinOnBoard_f, NULL);
	Cmd_AddCommand("team_comments", CL_MultiplayerTeamSlotComments_f, "Fills the multiplayer team selection menu with the team names");
	Cmd_AddCommand("equip_select", CL_Select_f, NULL);
	Cmd_AddCommand("soldier_select", CL_Select_f, _("Select a soldier from list"));
	Cmd_AddCommand("nextsoldier", CL_NextSoldier_f, _("Toggle to next soldier"));
	Cmd_AddCommand("saveteamslot", CL_SaveTeamMultiplayerSlot_f, NULL);
	Cmd_AddCommand("loadteamslot", CL_LoadTeamMultiplayerSlot_f, NULL);
#ifdef DEBUG
	Cmd_AddCommand("teamlist", CL_TeamListDebug_f, "Debug function to show all hired and assigned teammembers");
#endif
}


/**
 * @brief Stores the wholeTeam info to buffer (which might be a network buffer, too)
 *
 * Called by CL_SaveTeamMultiplayer to store the team info
 * @sa CL_SendCurTeamInfo
 * @sa CL_LoadTeamMultiplayerMember
 */
static void CL_SaveTeamInfo (sizebuf_t * buf, int baseID, int num)
{
	character_t *chr;
	int i, j;

	assert(baseID < gd.numBases);

	/* clean temp inventory */
	CL_CleanTempInventory(&gd.bases[baseID]);

	/* header */
	MSG_WriteByte(buf, num);
	for (i = 0; i < num; i++) {
		chr = E_GetHiredCharacter(&gd.bases[baseID], EMPL_SOLDIER, -(i+1));
		assert(chr);
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
		MSG_WriteByte(buf, chr->category);
		MSG_WriteByte(buf, chr->gender);
		MSG_WriteByte(buf, chr->STUN);
		MSG_WriteByte(buf, chr->AP);
		MSG_WriteByte(buf, chr->morale);

		/* even new attributes */
		MSG_WriteByte(buf, SKILL_NUM_TYPES);
		for (j = 0; j < SKILL_NUM_TYPES; j++)
			MSG_WriteByte(buf, chr->skills[j]);

		/* scores */
		MSG_WriteByte(buf, KILLED_NUM_TYPES);
		for (j = 0; j < KILLED_NUM_TYPES; j++)
			MSG_WriteShort(buf, chr->kills[j]);
		MSG_WriteShort(buf, chr->assigned_missions);

		/* inventory */
		CL_SendInventory(buf, chr->inv, qtrue);
	}
}

/**
 * @brief Stores the curTeam info to buffer (which might be a network buffer, too)
 * @sa G_ClientTeamInfo
 * @sa CL_SaveTeamInfo
 * @note Called in cl_main.c CL_Precache_f to send the team info to server
 */
void CL_SendCurTeamInfo (struct dbuffer * buf, character_t ** team, int num)
{
	character_t *chr;
	int i, j;

	/* clean temp inventory */
	CL_CleanTempInventory(baseCurrent);

	/* header */
	NET_WriteByte(buf, clc_teaminfo);
	NET_WriteByte(buf, num);

	for (i = 0; i < num; i++) {
		chr = team[i];
		/* send the fieldSize ACTOR_SIZE_* */
		NET_WriteByte(buf, chr->fieldSize);

		/* unique character number */
		NET_WriteShort(buf, chr->ucn);

		/* name */
		NET_WriteString(buf, chr->name);

		/* model */
		NET_WriteString(buf, chr->path);
		NET_WriteString(buf, chr->body);
		NET_WriteString(buf, chr->head);
		NET_WriteByte(buf, chr->skin);

		NET_WriteShort(buf, chr->HP);
		NET_WriteShort(buf, chr->maxHP);
		NET_WriteByte(buf, chr->category);
		NET_WriteByte(buf, chr->gender);
		NET_WriteByte(buf, chr->STUN);
		NET_WriteByte(buf, chr->AP);
		NET_WriteByte(buf, chr->morale);

		/* even new attributes */
		for (j = 0; j < SKILL_NUM_TYPES; j++)
			NET_WriteByte(buf, chr->skills[j]);

		/* scores */
		for (j = 0; j < KILLED_NUM_TYPES; j++)
			NET_WriteShort(buf, chr->kills[j]);
		NET_WriteShort(buf, chr->assigned_missions);

		/* inventory */
		CL_NetSendInventory(buf, chr->inv, qfalse);
	}
}


/* for updating after click on continue */
typedef struct updateCharacter_s {
	int ucn;
	int kills[KILLED_NUM_TYPES];
	int HP, AP, STUN;
	int morale;

	/* Those are chrScore_t. */
	int alienskilled;
	int aliensstunned;
	int civilianskilled;
	int civiliansstunned;
	int teamkilled;
	int teamstunned;
	int closekills;
	int heavykills;
	int assaultkills;
	int sniperkills;
	int explosivekills;
	int accuracystat;
	int powerstat;
} updateCharacter_t;

/**
 * @brief Parses the character data which was send by G_EndGame using G_SendCharacterData
 * @sa G_SendCharacterData
 * @sa G_EndGame
 * @sa E_Save
 * @note you also have to update the pascal string size in G_EndGame if you change the buffer here
 */
void CL_ParseCharacterData (struct dbuffer *msg, qboolean updateCharacter)
{
	static updateCharacter_t updateCharacterArray[MAX_WHOLETEAM];
	static int num = 0;
	int i, j;
	character_t* chr = NULL;
	employee_t* employee = NULL;

	if (updateCharacter) {
		for (i = 0; i < num; i++) {
			chr = NULL;
			employee = E_GetEmployeeFromChrUCN(updateCharacterArray[i].ucn);
			if (employee) {
				chr = &employee->chr;
			} else {
				Com_Printf("Warning: Could not get character with ucn: %i.\n", updateCharacterArray[i].ucn);
				continue;
			}
			chr->HP = updateCharacterArray[i].HP;
			chr->STUN = updateCharacterArray[i].STUN;
			chr->AP = updateCharacterArray[i].AP;
			chr->morale = updateCharacterArray[i].morale;

			for (j = 0; j < KILLED_NUM_TYPES; j++)
				chr->kills[j] = updateCharacterArray[i].kills[j];

			chr->chrscore.alienskilled = updateCharacterArray[i].alienskilled;
			chr->chrscore.aliensstunned = updateCharacterArray[i].aliensstunned;
			chr->chrscore.civilianskilled = updateCharacterArray[i].civilianskilled;
			chr->chrscore.civiliansstunned = updateCharacterArray[i].civiliansstunned;
			chr->chrscore.teamkilled = updateCharacterArray[i].teamkilled;
			chr->chrscore.teamstunned = updateCharacterArray[i].teamstunned;
			chr->chrscore.closekills = updateCharacterArray[i].closekills;
			chr->chrscore.heavykills = updateCharacterArray[i].heavykills;
			chr->chrscore.assaultkills = updateCharacterArray[i].assaultkills;
			chr->chrscore.sniperkills = updateCharacterArray[i].sniperkills;
			chr->chrscore.explosivekills = updateCharacterArray[i].explosivekills;
			chr->chrscore.accuracystat = updateCharacterArray[i].accuracystat;
			chr->chrscore.powerstat = updateCharacterArray[i].powerstat;
		}
		num = 0;
	} else {
		/* invalidate ucn in the array first */
		for (i = 0; i < MAX_WHOLETEAM; i++) {
			updateCharacterArray[i].ucn = -1;
		}
		/**
		 * this is the size of updateCharacter_t - also change it in G_SendCharacterData
		 * if you change something in updateCharacter_t
		 * KILLED_NUM_TYPES => size of kills array
		 * +2 => HP and ucn
		 * *2 => for shorts
		 * +16 => STUN, AP and chrScore_t
		 */
		num = NET_ReadShort(msg) / ((KILLED_NUM_TYPES + 2) * 2 + 16);
		if (num > MAX_EMPLOYEES)
			Sys_Error("CL_ParseCharacterData: num exceeded MAX_WHOLETEAM\n");
		else if (num < 0)
			Sys_Error("CL_ParseCharacterData: NET_ReadShort error (%i)\n", num);
		for (i = 0; i < num; i++) {
			updateCharacterArray[i].ucn = NET_ReadShort(msg);
			updateCharacterArray[i].HP = NET_ReadShort(msg);
			updateCharacterArray[i].STUN = NET_ReadByte(msg);
			updateCharacterArray[i].AP = NET_ReadByte(msg);
			updateCharacterArray[i].morale = NET_ReadByte(msg);

			for (j = 0; j < KILLED_NUM_TYPES; j++)
				updateCharacterArray[i].kills[j] = NET_ReadShort(msg);

			updateCharacterArray[i].alienskilled = NET_ReadByte(msg);
			updateCharacterArray[i].aliensstunned = NET_ReadByte(msg);
			updateCharacterArray[i].civilianskilled = NET_ReadByte(msg);
			updateCharacterArray[i].civiliansstunned = NET_ReadByte(msg);
			updateCharacterArray[i].teamkilled = NET_ReadByte(msg);
			updateCharacterArray[i].teamstunned = NET_ReadByte(msg);
			updateCharacterArray[i].closekills = NET_ReadByte(msg);
			updateCharacterArray[i].heavykills = NET_ReadByte(msg);
			updateCharacterArray[i].assaultkills = NET_ReadByte(msg);
			updateCharacterArray[i].sniperkills = NET_ReadByte(msg);
			updateCharacterArray[i].explosivekills = NET_ReadByte(msg);
			updateCharacterArray[i].accuracystat = NET_ReadByte(msg);
			updateCharacterArray[i].powerstat = NET_ReadByte(msg);
		}
	}
}

/**
 * @brief Reads mission result data from server
 * See EV_RESULTS
 * @sa G_EndGame
 * @sa CL_GameResults_f
 */
void CL_ParseResults (struct dbuffer *msg)
{
	static char resultText[MAX_SMALLMENUTEXTLEN];
	int num_spawned[MAX_TEAMS];
	int num_alive[MAX_TEAMS];
	int num_kills[MAX_TEAMS][MAX_TEAMS];
	int num_stuns[MAX_TEAMS][MAX_TEAMS];
	int winner, we;
	int i, j, num;
	int our_surviviurs, our_killed, our_stunned;
	int thier_surviviurs, thier_killed, thier_stunned;
	int civilian_surviviurs, civilian_killed, civilian_stunned;

	/* get number of teams */
	num = NET_ReadByte(msg);
	if (num > MAX_TEAMS)
		Sys_Error("Too many teams in result message\n");

	Com_DPrintf("Receiving results with %i teams.\n", num);

	/* get winning team */
	winner = NET_ReadByte(msg);
	we = cls.team;

	if (we > num)
		Sys_Error("Team number %d too high (only %d teams)\n", we, num);

	NET_ReadShort(msg); /* size */
	/* get spawn and alive count */
	for (i = 0; i < num; i++) {
		num_spawned[i] = NET_ReadByte(msg);
		num_alive[i] = NET_ReadByte(msg);
	}

	NET_ReadShort(msg); /* size */
	/* get kills */
	for (i = 0; i < num; i++)
		for (j = 0; j < num; j++)
			num_kills[i][j] = NET_ReadByte(msg);

	NET_ReadShort(msg); /* size */
	/* get stuns */
	for (i = 0; i < num; i++)
		for (j = 0; j < num; j++)
			num_stuns[i][j] = NET_ReadByte(msg);

	CL_ParseCharacterData(msg, qfalse);

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
	/* if we won, our stunned are alive */
	if (winner == we) {
		our_surviviurs += our_stunned;
		our_stunned = 0;
	} else
		/* if we lost, they revive stunned */
		thier_stunned = 0;

	/* we won, and we're not the dirty aliens */
	if ((winner == we) && (curCampaign))
		civilian_surviviurs += civilian_stunned;
	else
		civilian_killed += civilian_stunned;

	if (!curCampaign || !selMis) {
		/* the mission was started via console (@todo: or is multiplayer) */
		/* needs to be cleared and then append to it */
		if (curCampaign) {
			Com_sprintf(resultText, sizeof(resultText), _("Aliens killed\t%i\n"), thier_killed);
			ccs.aliensKilled += thier_killed;
		} else {
			Com_sprintf(resultText, sizeof(resultText), _("Enemies killed\t%i\n"), thier_killed + civilian_killed);
			ccs.aliensKilled += thier_killed + civilian_killed;
		}

		if (curCampaign) {
			Q_strcat(resultText, va(_("Aliens captured\t%i\n\n"), thier_stunned), sizeof(resultText));
			Q_strcat(resultText, va(_("Alien survivors\t%i\n\n"), thier_surviviurs), sizeof(resultText));
		} else {
			Q_strcat(resultText, va(_("Enemies captured\t%i\n\n"), thier_stunned), sizeof(resultText));
			Q_strcat(resultText, va(_("Enemy survivors\t%i\n\n"), thier_surviviurs), sizeof(resultText));
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
		INV_CollectingItems(winner == we);	/**< Collect items from the battlefield. */
		if (winner == we)
			AL_CollectingAliens();		/**< Collect aliens from the battlefield. */

		/* clear unused LE inventories */
		LE_Cleanup();

		/* needs to be cleared and then append to it */
		Com_sprintf(resultText, sizeof(resultText), _("Aliens killed\t%i\n"), thier_killed);
		ccs.aliensKilled += thier_killed;

		Q_strcat(resultText, va(_("Aliens captured\t%i\n"), thier_stunned), sizeof(resultText));
		Q_strcat(resultText, va(_("Alien survivors\t%i\n\n"), thier_surviviurs), sizeof(resultText));

		/* team stats */
		Q_strcat(resultText, va(_("PHALANX soldiers killed by Aliens\t%i\n"), our_killed - num_kills[we][we] - num_kills[TEAM_CIVILIAN][we]), sizeof(resultText));
		Q_strcat(resultText, va(_("PHALANX soldiers missing in action\t%i\n"), our_stunned), sizeof(resultText));
		Q_strcat(resultText, va(_("PHALANX friendly fire losses\t%i\n"), num_kills[we][we] + num_kills[TEAM_CIVILIAN][we]), sizeof(resultText));
		Q_strcat(resultText, va(_("PHALANX survivors\t%i\n\n"), our_surviviurs), sizeof(resultText));

		Q_strcat(resultText, va(_("Civilians killed by Aliens\t%i\n"), civilian_killed), sizeof(resultText));
		Q_strcat(resultText, va(_("Civilians killed by friendly fire\t%i\n"), num_kills[we][TEAM_CIVILIAN] + num_kills[TEAM_CIVILIAN][TEAM_CIVILIAN]), sizeof(resultText));
		Q_strcat(resultText, va(_("Civilians saved\t%i\n\n"), civilian_surviviurs), sizeof(resultText));
		Q_strcat(resultText, va(_("Gathered items (types/all)\t%i/%i\n"), missionresults.itemtypes,  missionresults.itemamount), sizeof(resultText));

		ccs.civiliansKilled += civilian_killed;

		MN_PopMenu(qtrue);
		Cvar_Set("mn_main", "singleplayerInGame");
		Cvar_Set("mn_active", "map");
		MN_PushMenu("map");
	}
	/* show win screen */
	if (ccs.singleplayer) {
		/* Make sure that at this point we are able to 'Try Again' a mission. */
		Cvar_SetValue("mission_tryagain", 0);
		if (selMis && baseCurrent)
			CP_ExecuteMissionTrigger(selMis->def, winner == we, baseCurrent);
		else
			Com_Printf("CL_ParseResults: Error - no mission triggers, because selMis or baseCurrent are not valid\n");

		if (winner == we) {
			/* We need to update Menu Won with UFO recovery stuff. */
			if (missionresults.recovery) {
				if (missionresults.crashsite)
					Q_strcat(resultText, va(_("\nSecured crashed %s\n"), UFO_TypeToName(missionresults.ufotype)), sizeof(resultText));
				else
					Q_strcat(resultText, va(_("\nSecured landed %s\n"), UFO_TypeToName(missionresults.ufotype)), sizeof(resultText));
			}
			MN_PushMenu("won");
		} else
			MN_PushMenu("lost");

		/* on singleplayer we disconnect the game */
		/* we can safely wipe all mission data now */
		/* @todo: I don't understand how this works
		 * and why, when I move this to CL_GameResults_f,
		 * the "won" menu get's garbled at "killteam 7"
		 */
		Cbuf_AddText("disconnect\n");
		Cbuf_Execute();
	} else {
		Com_sprintf(resultText, sizeof(resultText), _("\n\nEnemies killed:  %i\nTeam survivors:  %i"), thier_killed + thier_stunned, our_surviviurs);
		if (winner == we) {
			Q_strncpyz(popupText, _("You won the game!"), sizeof(popupText));
			Q_strcat(popupText, resultText, sizeof(popupText));
			MN_Popup(_("Congratulations"), popupText);
		} else if (winner == 0) {
			Q_strncpyz(popupText, _("The game was a draw!\n\nNo survivors left on any side."), sizeof(popupText));
			MN_Popup(_("Game Drawn!"), popupText);
		} else {
			Q_strncpyz(popupText, _("You lost the game"), sizeof(popupText));
			Q_strcat(popupText, resultText, sizeof(popupText));
			MN_Popup(_("Better luck next time"), popupText);
		}
	}
}

/* ======= RANKS & MEDALS =========*/

/**
 * @brief Get the index number of the given rankID
 */
static int CL_GetRank (const char* rankID)
{
	int i;

	/* only check in singleplayer */
	if (!ccs.singleplayer)
		return -1;

	for (i = 0; i < gd.numRanks; i++) {
		if (!Q_strncmp(gd.ranks[i].id, rankID, MAX_VAR))
			return i;
	}
	Com_Printf("Could not find rank '%s'\n", rankID);
	return -1;
}

static const value_t rankValues[] =
{
	{"name", V_TRANSLATION_STRING, offsetof(rank_t, name), 0},
	{"shortname", V_TRANSLATION_STRING,	offsetof(rank_t, shortname), 0},
	{"image", V_CLIENT_HUNK_STRING, offsetof(rank_t, image), 0},
	{"mind", V_INT, offsetof(rank_t, mind), MEMBER_SIZEOF(rank_t, mind)},
	{"killed_enemies", V_INT, offsetof(rank_t, killed_enemies), MEMBER_SIZEOF(rank_t, killed_enemies)},
	{"killed_others", V_INT, offsetof(rank_t, killed_others), MEMBER_SIZEOF(rank_t, killed_others)},
	{NULL, 0, 0, 0}
};

/**
 * @brief Parse medals and ranks defined in the medals.ufo file.
 * @sa CL_ParseScriptFirst
 * @note write into cl_localPool - free on every game restart and reparse
 */
void CL_ParseMedalsAndRanks (const char *name, const char **text, byte parserank)
{
	rank_t *rank = NULL;
	const char *errhead = "Com_ParseMedalsAndRanks: unexpected end of file (medal/rank ";
	const char *token;
	const value_t	*v;

	/* get name list body body */
	token = COM_Parse(text);

	if (!*text || *token != '{') {
		Com_Printf("Com_ParseMedalsAndRanks: rank/medal \"%s\" without body ignored\n", name);
		return;
	}

	if (parserank) {
		/* parse ranks */
		if (gd.numRanks >= MAX_RANKS) {
			Com_Printf("Too many rank descriptions, '%s' ignored.\n", name);
			gd.numRanks = MAX_RANKS;
			return;
		}

		rank = &gd.ranks[gd.numRanks++];
		memset(rank, 0, sizeof(rank_t));
		rank->id = Mem_PoolStrDup(name, cl_localPool, CL_TAG_REPARSE_ON_NEW_GAME);

		do {
			/* get the name type */
			token = COM_EParse(text, errhead, name);
			if (!*text)
				break;
			if (*token == '}')
				break;
			for (v = rankValues; v->string; v++)
				if (!Q_strncmp(token, v->string, sizeof(v->string))) {
					/* found a definition */
					token = COM_EParse(text, errhead, name);
					if (!*text)
						return;
					switch (v->type) {
					case V_TRANSLATION2_STRING:
						token++;
					case V_CLIENT_HUNK_STRING:
						Mem_PoolStrDupTo(token, (char**) ((char*)rank + (int)v->ofs), cl_localPool, CL_TAG_REPARSE_ON_NEW_GAME);
						break;
					default:
						Com_ParseValue(rank, token, v->type, v->ofs, v->size);
						break;
					}
					break;
				}

			if (!Q_strncmp(token, "type", 4)) {
				/* employeeType_t */
				token = COM_EParse(text, errhead, name);
				if (!*text)
					return;
				rank->type = E_GetEmployeeType(token);
			} else if (!v->string)
				Com_Printf("Com_ParseMedalsAndRanks: unknown token \"%s\" ignored (medal/rank %s)\n", token, name);
		} while (*text);
	} else {
		/* parse medals */
	}
}

static const value_t ugvValues[] = {
	{"tu", V_INT, offsetof(ugv_t, tu), MEMBER_SIZEOF(ugv_t, tu)},
	{"weapon", V_STRING, offsetof(ugv_t, weapon), 0},
	{"armor", V_STRING, offsetof(ugv_t, armor), 0},
	{"size", V_INT, offsetof(ugv_t, size), MEMBER_SIZEOF(ugv_t, size)},

	{NULL, 0, 0, 0}
};

/**
 * @brief Parse UGVs
 * @sa CL_ParseClientData
 */
void CL_ParseUGVs (const char *name, const char **text)
{
	const char *errhead = "Com_ParseUGVs: unexpected end of file (ugv ";
	const char	*token;
	const value_t	*v;
	ugv_t*	ugv;

	/* get name list body body */
	token = COM_Parse(text);

	if (!*text || *token != '{') {
		Com_Printf("Com_ParseUGVs: ugv \"%s\" without body ignored\n", name);
		return;
	}

	/* parse ugv */
	if (gd.numUGV >= MAX_UGV) {
		Com_Printf("Too many UGV descriptions, '%s' ignored.\n", name);
		return;
	}

	ugv = &gd.ugvs[gd.numUGV++];
	memset(ugv, 0, sizeof(ugv_t));

	do {
		/* get the name type */
		token = COM_EParse(text, errhead, name);
		if (!*text)
			break;
		if (*token == '}')
			break;
		for (v = ugvValues; v->string; v++)
			if (!Q_strncmp(token, v->string, sizeof(v->string))) {
				/* found a definition */
				token = COM_EParse(text, errhead, name);
				if (!*text)
					return;
				Com_ParseValue(ugv, token, v->type, v->ofs, v->size);
				break;
			}
			if (!v->string)
				Com_Printf("Com_ParseUGVs: unknown token \"%s\" ignored (ugv %s)\n", token, name);
	} while (*text);
}

/**
 * @brief Ensures skills increase quickly at first and then slows down as they near 100.
 * @note (number of calls to this function/skill increase range)
 * 5/20-40 6/40-50 8/50-60 11/60-70 15/80-90 24/90-100
 * @param[in] skill The current value of the skill to be increased.
 * @return The amount to be added to the skill
 */
static int CL_SkillIncreaseBy (float skill)
{
	if (skill >= MAX_SKILL)
		return 0;
	return (int)(2.3f - 0.02f * skill + (2.0f - 0.01f * skill) * frand());
}

/**
 * @brief Updates character skills after a mission.
 * @param[in] *chr Pointer to a character_t.
 * @sa CL_UpdateCharacterStats
 * @sa G_UpdateCharacterScore
 */
void CL_UpdateCharacterSkills (character_t *chr)
{
	qboolean changed = qfalse;

	if (!chr)
		return;
	/* We are only updating skills to max value 100.
	   TODO: More than 100 is available only with implants. */

	/* Update SKILL_CLOSE. 2 closekills needed. */
	if (chr->skills[SKILL_CLOSE] < MAX_SKILL) {
		if (chr->chrscore.closekills >= 2) {
			chr->skills[SKILL_CLOSE] += CL_SkillIncreaseBy((float)chr->skills[SKILL_CLOSE]);
			chr->chrscore.closekills = chr->chrscore.closekills - 2;
			changed = qtrue;
		}
	}

	/* Update SKILL_HEAVY. 4 heavykills needed.*/
	if (chr->skills[SKILL_HEAVY] < MAX_SKILL) {
		if (chr->chrscore.heavykills >= 4) {
			chr->skills[SKILL_HEAVY] += CL_SkillIncreaseBy((float)chr->skills[SKILL_HEAVY]);
			chr->chrscore.heavykills = chr->chrscore.heavykills - 4;
			changed = qtrue;
		}
	}

	/* Update SKILL_ASSAULT. 3 assaultkills needed.*/
	if (chr->skills[SKILL_ASSAULT] < MAX_SKILL) {
		if (chr->chrscore.assaultkills >= 3) {
			chr->skills[SKILL_ASSAULT] += CL_SkillIncreaseBy((float)chr->skills[SKILL_ASSAULT]);
			chr->chrscore.assaultkills = chr->chrscore.assaultkills - 3;
			changed = qtrue;
		}
	}

	/* Update SKILL_SNIPER. 3 sniperkills needed. */
	if (chr->skills[SKILL_SNIPER] < MAX_SKILL) {
		if (chr->chrscore.sniperkills >= 3) {
			chr->skills[SKILL_SNIPER] += CL_SkillIncreaseBy((float)chr->skills[SKILL_SNIPER]);
			chr->chrscore.sniperkills = chr->chrscore.sniperkills - 3;
			changed = qtrue;
		}
	}

	/* Update SKILL_EXPLOSIVE. 5 explosivekills needed. */
	if (chr->skills[SKILL_EXPLOSIVE] < MAX_SKILL) {
		if (chr->chrscore.explosivekills >= 5) {
			chr->skills[SKILL_EXPLOSIVE] += CL_SkillIncreaseBy((float)chr->skills[SKILL_EXPLOSIVE]);
			chr->chrscore.explosivekills = chr->chrscore.explosivekills - 5;
			changed = qtrue;
		}
	}

	/* Update ABILITY_ACCURACY. 8 accuracystat (succesful kill or stun) needed. */
	if (chr->skills[ABILITY_ACCURACY] < MAX_SKILL) {
		if (chr->chrscore.accuracystat >= 8) {
			chr->skills[ABILITY_ACCURACY] += CL_SkillIncreaseBy((float)chr->skills[ABILITY_ACCURACY]);
			chr->chrscore.accuracystat = chr->chrscore.accuracystat - 8;
			changed = qtrue;
		}
	}

	/* Update ABILITY_POWER. 8 powerstat (succesful kill or stun with heavy) needed. */
	if (chr->skills[ABILITY_POWER] < MAX_SKILL) {
		if (chr->chrscore.powerstat >= 8) {
			chr->skills[ABILITY_POWER] += CL_SkillIncreaseBy((float)chr->skills[ABILITY_POWER]);
			chr->chrscore.powerstat = chr->chrscore.powerstat - 8;
			changed = qtrue;
		}
	}

	/* Update ABILITY_MIND just to make ranks work.
	   @todo: ABILITY_MIND should be improved other way. FIXME. */
	if (!changed)
		chr->skills[ABILITY_MIND]++;

	/* Repeat to make sure we update skills with huge amount of chrscore->*kills. */
	if (changed)
		CL_UpdateCharacterSkills(chr);
}

