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

See the GNU General Public License for more details.m

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

*/

#include "client.h"
#include "cl_global.h"
#include "cl_team.h"
#include "cl_actor.h"
#include "cl_ufo.h"
#include "menu/m_inventory.h"
#include "menu/m_popup.h"

#define MAX_TEAMDATASIZE	32768

static qboolean display_heavy_equipment_list = qfalse; /**< Used in team assignment screen to tell if we are assigning soldeirs or heavy equipment (ugvs/tanks) */

static int CL_GetRank(const char* rankID);
static void CL_SaveTeamInfo(sizebuf_t * buf, int baseID, int num);
static void CL_MarkTeam_f(void);

/* List of currently displayed or equipeable characters. extern definition in client.h */
chrList_t chrDisplayList;

/**
 * @brief Prepares environment for multiplayer.
 * @note Ugly hack which sets proper values of global variables.
 * @note Should be removed as soon as mp won't use base and aircraft
 * @note and mp team handling functions won't use baseCurrent
 */
static void CL_MultiplayerEnvironment_f (void)
{
	base_t *base = B_GetBase(0);

	/* multiplayer is not ready yet */
	if (!gd.numBases)
		CL_StartSingleplayer(qfalse);

	assert(gd.numBases == 1);
	baseCurrent = base;
	cls.missionaircraft = AIR_AircraftGetFromIdx(0);
	if (!cls.missionaircraft) {
		Sys_Error("Make sure that you've set sv_maxclients to a value higher than 1");
	}
	baseCurrent->aircraftCurrent = cls.missionaircraft->idxInBase;
}

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
	case 4:
		return _("Yellow");
		break;
	case 5:
		return _("CCCP");
		break;
	default:
		Sys_Error("CL_GetTeamSkinName: Unknown skin id %i - max is %i\n", id, NUM_TEAMSKINS-1);
		break;
	}
	return NULL; /* never reached */
}

/**
 * @sa CL_LoadItem
 */
static void CL_SaveItem (sizebuf_t *buf, item_t item, int container, int x, int y)
{
	assert(item.t != NONE);
/*	Com_Printf("Add item %s to container %i (t=%i:a=%i:m=%i) (x=%i:y=%i)\n", csi.ods[item.t].id, container, item.t, item.a, item.m, x, y);*/
	MSG_WriteFormat(buf, "bbbbbl", item.a, container, x, y, item.rotated, item.amount);
	MSG_WriteString(buf, csi.ods[item.t].id);
	if (item.a > NONE_AMMO)
		MSG_WriteString(buf, csi.ods[item.m].id);
}

/**
 * @sa CL_SaveItem
 * @sa CL_LoadInventory
 */
void CL_SaveInventory (sizebuf_t *buf, inventory_t *i)
{
	int j, nr = 0;
	invList_t *ic;

	for (j = 0; j < csi.numIDs; j++)
		for (ic = i->c[j]; ic; ic = ic->next)
			nr++;

	Com_DPrintf(DEBUG_CLIENT, "CL_SaveInventory: Send %i items\n", nr);
	MSG_WriteShort(buf, nr);
	for (j = 0; j < csi.numIDs; j++)
		for (ic = i->c[j]; ic; ic = ic->next)
			CL_SaveItem(buf, ic->item, j, ic->x, ic->y);
}

/**
 * @sa CL_SaveItem
 */
static void CL_LoadItem (sizebuf_t *buf, item_t *item, int *container, int *x, int *y)
{
	const char *itemID;

	/* reset */
	item->t = item->m = NONE;
	item->a = NONE_AMMO;

	MSG_ReadFormat(buf, "bbbbbl", &item->a, container, x, y, &item->rotated, &item->amount);
	itemID = MSG_ReadString(buf);
	item->t = INVSH_GetItemByID(itemID);
	if (item->a > NONE_AMMO) {
		itemID = MSG_ReadString(buf);
		item->m = INVSH_GetItemByID(itemID);
	}
}

/**
 * @sa CL_SaveInventory
 * @sa CL_LoadItem
 * @sa Com_AddToInventory
 */
void CL_LoadInventory (sizebuf_t *buf, inventory_t *i)
{
	item_t item;
	int container, x, y;
	int nr = MSG_ReadShort(buf);

	Com_DPrintf(DEBUG_CLIENT, "CL_LoadInventory: Read %i items\n", nr);
	assert(nr < MAX_INVLIST);
	for (; nr-- > 0;) {
		CL_LoadItem(buf, &item, &container, &x, &y);
		Com_AddToInventory(i, item, container, x, y, 1);
	}
}

/**
 * @sa G_WriteItem
 * @sa G_ReadItem
 * @note The amount of the item_t struct should not be needed here - because
 * the amount is only valid for idFloor and idEquip
 */
static void CL_NetSendItem (struct dbuffer *buf, item_t item, int container, int x, int y)
{
	assert(item.t != NONE);
	Com_DPrintf(DEBUG_CLIENT, "CL_NetSendItem: Add item %s to container %i (t=%i:a=%i:m=%i) (x=%i:y=%i)\n",
		csi.ods[item.t].id, container, item.t, item.a, item.m, x, y);
	NET_WriteFormat(buf, ev_format[EV_INV_TRANSFER], item.t, item.a, item.m, container, x, y, item.rotated);
}

/**
 * @sa G_SendInventory
 */
static void CL_NetSendInventory (struct dbuffer *buf, inventory_t *i)
{
	int j, nr = 0;
	invList_t *ic;

	for (j = 0; j < csi.numIDs; j++)
		for (ic = i->c[j]; ic; ic = ic->next)
			nr++;

	NET_WriteShort(buf, nr * INV_INVENTORY_BYTES);
	for (j = 0; j < csi.numIDs; j++)
		for (ic = i->c[j]; ic; ic = ic->next)
			CL_NetSendItem(buf, ic->item, j, ic->x, ic->y);
}

/**
 * @sa G_WriteItem
 * @sa G_ReadItem
 * @note The amount of the item_t struct should not be needed here - because
 * the amount is only valid for idFloor and idEquip
 */
void CL_NetReceiveItem (struct dbuffer *buf, item_t *item, int *container, int *x, int *y)
{
	/* reset */
	item->t = item->m = NONE;
	item->a = NONE_AMMO;

	NET_ReadFormat(buf, ev_format[EV_INV_TRANSFER], &item->t, &item->a, &item->m, container, x, y, &item->rotated);
}

/**
 * @brief Test the names in team_*.ufo
 *
 * This is a console command to test the names that were defined in team_*.ufo
 * Usage: givename <gender> <teamid> [num]
 * valid genders are male, female, neutral
 */
static void CL_GiveName_f (void)
{
	const char *name;
	int i, j, num;

	if (Cmd_Argc() < 3) {
		Com_Printf("Usage: %s <gender> <teamid> [num]\n", Cmd_Argv(0));
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
			Com_Printf("No first name in team '%s'\n", Cmd_Argv(2));
			return;
		}
		Com_Printf(name);

		/* get last name */
		name = Com_GiveName(i + LASTNAME, Cmd_Argv(2));
		if (!name) {
			Com_Printf("\nNo last name in team '%s'\n", Cmd_Argv(2));
			return;
		}

		/* print out name */
		Com_Printf(" %s\n", name);
	}
}

/**
 * @brief Return a given ugv_t pointer
 * @param[in] ugvID Which base the employee should be hired in.
 * @return ugv_t pointer or NULL if not found.
 * @note If there ever is a problem because an id with the name "NULL" isn't found then this is because NULL pointers in E_Save/Employee_t are stored like that (duh) ;).
 */
ugv_t *CL_GetUgvById (const char *ugvID)
{
	int i;

	for (i = 0; i < gd.numUGV; i++) {
		if (!Q_strcmp(gd.ugvs[i].id, ugvID)) {
			return &gd.ugvs[i];
		}
	}
#if 0
	Com_Printf("CL_GetUgvById: DEBUG No ugv_t entry found for id '%s' in %i entries.\n", ugvID, gd.numUGV);
#endif
	return NULL;
}


/**
 * @brief Generates the skills and inventory for a character and for a 2x2 unit
 *
 * @sa CL_ResetCharacters
 * @param[in] employee The employee to create character data for.
 * @param[in] team Which team to use for creation.
 * @todo fix the assignment of the inventory (assume that you do not know the base yet)
 * @todo fix the assignment of ucn??
 * @todo fix the WholeTeam stuff
 */
void CL_GenerateCharacter (employee_t *employee, const char *team, employeeType_t employeeType, const ugv_t *ugvType)
{
	character_t *chr;
	char teamDefName[MAX_VAR];
	int teamValue = TEAM_CIVILIAN;
	qboolean multiplayer = (sv_maxclients->integer >= 2);

	if (!employee)
		return;

	chr = &employee->chr;
	memset(chr, 0, sizeof(character_t));

	/* link inventory */
	chr->inv = &employee->inv;
	INVSH_DestroyInventory(chr->inv);

	/* get ucn */
	chr->ucn = gd.nextUCN++;

	Com_DPrintf(DEBUG_CLIENT, "Generate character for team: '%s' (type: %i)\n", team, employeeType);

	/* Backlink from chr to employee struct. */
	chr->empl_type = employeeType;
	chr->empl_idx = employee->idx;

	/* if not human - then we TEAM_ALIEN */
	if (strstr(team, "human"))
		teamValue = TEAM_PHALANX;
	else if (strstr(team, "alien"))
		teamValue = TEAM_ALIEN;

	/* Set default reaction-mode for all character-types to "once". */
	/** @todo Set aliens to "multi"? */
	chr->reservedTus.reserveReaction = STATE_REACTION_ONCE;

	/* Generate character stats, models & names. */
	switch (employeeType) {
	case EMPL_SOLDIER:
		chr->score.rank = CL_GetRank("rifleman");
		/* Create attributes. */
		CHRSH_CharGenAbilitySkills(chr, teamValue, employeeType, multiplayer);
		Q_strncpyz(teamDefName, team, sizeof(teamDefName));
		break;
	case EMPL_SCIENTIST:
		chr->score.rank = CL_GetRank("scientist");
		/* Create attributes. */
		CHRSH_CharGenAbilitySkills(chr, teamValue, employeeType, multiplayer);
		Com_sprintf(teamDefName, sizeof(teamDefName), "%s_scientist", team);
		break;
	case EMPL_MEDIC:
		chr->score.rank = CL_GetRank("medic");
		/* Create attributes. */
		CHRSH_CharGenAbilitySkills(chr, teamValue, employeeType, multiplayer);
		Com_sprintf(teamDefName, sizeof(teamDefName), "%s_medic", team);
		break;
	case EMPL_WORKER:
		chr->score.rank = CL_GetRank("worker");
		/* Create attributes. */
		CHRSH_CharGenAbilitySkills(chr, teamValue, employeeType, multiplayer);
		Com_sprintf(teamDefName, sizeof(teamDefName), "%s_worker", team);
		break;
	case EMPL_ROBOT:
		if (!ugvType)
			Sys_Error("CL_GenerateCharacter: no type given for generation of EMPL_ROBOT employee.\n");

		chr->score.rank = CL_GetRank("ugv");

		/** Create attributes.
		 * @todo get the min/max values from ugv_t def? */
		CHRSH_CharGenAbilitySkills(chr, teamValue, employeeType, multiplayer);

		Com_sprintf(teamDefName, sizeof(teamDefName), "%s%s", team, ugvType->actors);
		break;
	default:
		Sys_Error("Unknown employee type\n");
		break;
	}
	chr->skin = Com_GetCharacterValues(teamDefName, chr);
	/* chr->HP = GET_HP(chr->skills[ABILITY_POWER]); */
	chr->maxHP = chr->HP;
	chr->morale = GET_MORALE(chr->score.skills[ABILITY_MIND]);
	if (chr->morale >= MAX_SKILL)
		chr->morale = MAX_SKILL;
}


/**
 * @brief Remove all character_t information (and linekd tot hat employees & team info) from the game.
 * @param[in] base The base to Remove all this stuff from.
 * @sa CL_GenerateCharacter
 * @sa AIR_ResetAircraftTeam
 */
void CL_ResetCharacters (base_t* const base)
{
	int i;
	character_t *chr;

	/* Reset inventory data of all hired emplyoees that can be sent into combat (i.e. characters with inventories). */
	for (i = 0; i < MAX_EMPLOYEES; i++) {
		chr = E_GetHiredCharacter(base, EMPL_SOLDIER, i);
		if (chr)
			INVSH_DestroyInventory(chr->inv);

		chr = E_GetHiredCharacter(base, EMPL_ROBOT, i);
		if (chr)
			INVSH_DestroyInventory(chr->inv);
	}

	/* Reset hire info. */
	Cvar_ForceSet("cl_selected", "0");

	/* Fire 'em all (Yes, even in multiplayer they are hired) */
	for (i = 0; i < MAX_EMPL; i++) {
		E_UnhireAllEmployees(base, i);
	}

	/* Purge all team-info from the aircraft (even in multiplayer - we use a dummy craft there) */
	for (i = 0; i < base->numAircraftInBase; i++) {
		AIR_ResetAircraftTeam(B_GetAircraftFromBaseByIndex(base, i));
	}
}

/**
 * @brief Change the name of the selected actor.
 * @sa CL_MessageMenu_f
 */
static void CL_ChangeName_f (void)
{
	const int sel = Cvar_VariableInteger("mn_employee_idx");
	const int type = Cvar_VariableInteger("mn_employee_type");
	const menu_t *activeMenu;

	/* Maybe called without base initialized or active. */
	if (!baseCurrent)
		return;

	if (sel >= 0 && sel < gd.numEmployees[type]) {
		Q_strncpyz(gd.employees[type][sel].chr.name, Cvar_VariableString("mn_name"), MAX_VAR);

		/* Now refresh the list. */
		activeMenu = MN_GetActiveMenu();
		if (!Q_strncmp(activeMenu->name, "employees", 9)) {
			/* We are in the hire (aka "employee") screen. */
			Cbuf_AddText(va("employee_init %i %i;", type, sel));
		} else {
			/* We are in the "assign to aircraft" screen. */
			CL_MarkTeam_f();
		}
	}
}


/**
 * @brief Change the skin of the selected actor.
 */
static void CL_ChangeSkin_f (void)
{
	const int sel = cl_selected->integer;

	if (sel >= 0 && sel < chrDisplayList.num) {
		int newSkin = Cvar_VariableInteger("mn_skin") + 1;
		if (newSkin >= NUM_TEAMSKINS || newSkin < 0)
			newSkin = 0;

		/* don't allow all skins in singleplayer */
		if (ccs.singleplayer && newSkin >= NUM_TEAMSKINS_SINGLEPLAYER)
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
static void CL_ChangeSkinOnBoard_f (void)
{
	int newSkin, i;

	if (!baseCurrent)
		return;

	/** @todo Do we really need to check aircraftcurrent here? */
	if ((baseCurrent->aircraftCurrent != AIRCRAFT_INBASE_INVALID) && (baseCurrent->aircraftCurrent < baseCurrent->numAircraftInBase)) {
		/* aircraft = &baseCurrent->aircraft[baseCurrent->aircraftCurrent]; */
	} else {
#ifdef DEBUG
		/* Should never happen. */
		Com_Printf("CL_ChangeSkinOnBoard_f()... No aircraft selected!\n");
#endif
		return;
	}

	/* Get selected skin and fall back to default skin if it is not valid. */
	newSkin = Cvar_VariableInteger("mn_skin");
	if (newSkin >= NUM_TEAMSKINS || newSkin < 0)
		newSkin = 0;

	/* don't allow all skins in singleplayer */
	if (ccs.singleplayer && newSkin >= NUM_TEAMSKINS_SINGLEPLAYER)
		newSkin = 0;

	/**
	 * Apply new skin to all (shown/dsiplayed) team-members.
	 * @todo What happens if a model of a team member does not have the selected skin?
	 */
	for (i = 0; i < chrDisplayList.num; i++) {
		assert(chrDisplayList.chr[i]);
		chrDisplayList.chr[i]->skin = newSkin;
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
 * @brief Move all the equipment carried by the team on the aircraft into the given equipment
 * @param[in] aircraft The craft with the tream (and thus equipment) onboard
 * @param[out] ed The equipment definition which will receive all teh stuff from the aircraft-team.
 */
void CL_AddCarriedToEq (aircraft_t *aircraft, equipDef_t * ed)
{
	character_t *chr;
	invList_t *ic, *next;
	int p, container;

	if (!aircraft) {
		Com_Printf("CL_AddCarriedToEq: Warning: Called with no aicraft (and thus no carried equipment to add).\n");
		return;
	}
	if (!ed) {
		Com_Printf("CL_AddCarriedToEq: Warning: Called with no equipment definition at add stuff to.\n");
		return;
	}

	if (aircraft->teamSize <= 0) {
		Com_DPrintf(DEBUG_CLIENT, "CL_AddCarriedToEq: No team to remove equipment from.\n");
		return;
	}

	for (container = 0; container < csi.numIDs; container++) {
		for (p= 0; p < aircraft->maxTeamSize; p++) {
			if (aircraft->teamIdxs[p] != -1) {
				chr = &gd.employees[aircraft->teamTypes[p]][aircraft->teamIdxs[p]].chr;
				ic = chr->inv->c[container];
				while (ic) {
					const item_t item = ic->item;
					const int type = item.t;

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
					ic = next;
				}
			}
		}
	}
}

/**
 * @sa CL_ReloadAndRemoveCarried
 */
static item_t CL_AddWeaponAmmo (equipDef_t * ed, item_t item)
{
	int i;
	const int type = item.t;	/* 'type' equals idx in "&csi.ods[idx]" */

	assert(ed->num[type] > 0);
	ed->num[type]--;

	if (csi.ods[type].weap_idx[0] != NONE) {
		/* The given item is ammo or self-contained weapon (i.e. It has firedefinitions. */
		if (csi.ods[type].oneshot) {
			/* "Recharge" the oneshot weapon. */
			item.a = csi.ods[type].ammo;
			item.m = item.t; /* Just in case this hasn't been done yet. */
			Com_DPrintf(DEBUG_CLIENT, "CL_AddWeaponAmmo: oneshot weapon '%s'.\n", csi.ods[type].id);
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
		if (INVSH_LoadableInWeapon(&csi.ods[i], type)) {
			if (ed->num[i] > 0) {
				ed->num[i]--;
				item.a = csi.ods[type].ammo;
				item.m = i;
				return item;
			}
		}
	}

	/* @todo: on return from a mission with no clips left
	 * and one weapon half-loaded wielded by soldier
	 * and one empty in equip, on the first opening of equip,
	 * the empty weapon will be in soldier hands, the half-full in equip;
	 * this should be the other way around. */

	/* Failed to find a complete clip - see if there's any loose ammo
	 * of the same kind; if so, gather it all in this weapon. */
	if (item.m != NONE && ed->num_loose[item.m] > 0) {
		item.a = ed->num_loose[item.m];
		ed->num_loose[item.m] = 0;
		return item;
	}

	/* See if there's any loose ammo */
	item.a = NONE_AMMO;
	for (i = 0; i < csi.numODs; i++) {
		if (INVSH_LoadableInWeapon(&csi.ods[i], type)
		&& (ed->num_loose[i] > item.a) ) {
			if (item.a > 0) {
				/* We previously found some ammo, but we've now found other
				 * loose ammo of a different (but appropriate) type with
				 * more bullets.  Put the previously found ammo back, so
				 * we'll take the new type. */
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
 * @param[in] aircraft	Pointer to an aircraft for given team.
 * @param[in] ed equipDef_t pointer to equipment of given character in a team.
 * @sa CL_AddWeaponAmmo
 */
void CL_ReloadAndRemoveCarried (aircraft_t *aircraft, equipDef_t * ed)
{
	base_t *base;
	character_t *cp;
	invList_t *ic, *next;
	int p, container;

	/** Iterate through in container order (right hand, left hand, belt,
	 * holster, backpack) at the top level, i.e. each squad member reloads
	 * her right hand, then each reloads his left hand, etc. The effect
	 * of this is that when things are tight, everyone has the opportunity
	 * to get their preferred weapon(s) loaded before anyone is allowed
	 * to keep her spares in the backpack or on the floor. We don't want
	 * the first person in the squad filling their backpack with spare ammo
	 * leaving others with unloaded guns in their hands... */

	assert(aircraft);
	base = aircraft->homebase;
	assert(base);

	Com_DPrintf(DEBUG_CLIENT, "CL_ReloadAndRemoveCarried()...aircraft idx: %i, team size: %i\n",
		aircraft->idx, aircraft->teamSize);

	for (container = 0; container < csi.numIDs; container++) {
		for (p = 0; p < aircraft->maxTeamSize; p++) {
			if (aircraft->teamIdxs[p] != -1) {
				cp = &gd.employees[aircraft->teamTypes[p]][aircraft->teamIdxs[p]].chr;
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
}

/**
 * @brief Clears all containers that are temp containers (see script definition).
 * @sa CL_GenerateEquipment_f
 * @sa CL_ResetMultiplayerTeamInBase
 * @sa CL_SaveTeamInfo
 * @sa CL_SendCurTeamInfo
 */
void CL_CleanTempInventory (base_t* base)
{
	int i, k;

	if (!base)
		return;

	INVSH_DestroyInventory(&base->equipByBuyType);
	for (i = 0; i < MAX_EMPLOYEES; i++)
		for (k = 0; k < csi.numIDs; k++)
			if (csi.ids[k].temp) {
				/* idFloor and idEquip are temp */
				gd.employees[EMPL_SOLDIER][i].inv.c[k] = NULL;
				gd.employees[EMPL_ROBOT][i].inv.c[k] = NULL;
			}
}

/**
 * @brief Displays actor equipment and unused items in proper buytype category.
 * @note This function is called everytime the equipment screen for the team pops up.
 * @sa CL_UpdatePointersInGlobalData
 * @todo Do we allow EMPL_ROBOTs to be equipable? Or is simple buying of ammo enough (similar to original UFO/XCOM)?
 * In the first case the EMPL_SOLDIER stuff below needs to be changed.
 */
static void CL_GenerateEquipment_f (void)
{
	equipDef_t unused;
	int i, p;
	aircraft_t *aircraft;
	int team;

	assert(baseCurrent);
	assert((baseCurrent->aircraftCurrent != AIRCRAFT_INBASE_INVALID) && (baseCurrent->aircraftCurrent < baseCurrent->numAircraftInBase));
	aircraft = &baseCurrent->aircraft[baseCurrent->aircraftCurrent];

	/* Popup if no soldiers are assigned to the current aircraft. */
	if (!aircraft->teamSize) {
		MN_PopMenu(qfalse);
		return;
	}

	/* Get team. */
	if (strstr(Cvar_VariableString("team"), "alien")) {
		team = 1;
		Com_DPrintf(DEBUG_CLIENT, "CL_GenerateEquipment_f().. team alien, id: %i\n", team);
	} else {
		team = 0;
		Com_DPrintf(DEBUG_CLIENT, "CL_GenerateEquipment_f().. team human, id: %i\n", team);
	}

	/* Store hired names. */
	Cvar_ForceSet("cl_selected", "0");
	chrDisplayList.num = 0;
	for (i = 0; i < aircraft->maxTeamSize; i++) {
		assert(chrDisplayList.num < MAX_ACTIVETEAM);

		if (aircraft->teamIdxs[i] <= -1)
			continue; /* Skip unused team-slot. */

		if (aircraft->teamTypes[i] != EMPL_SOLDIER)
			continue; /* Skip EMPL_ROBOT (i.e. ugvs) for now . */

		chrDisplayList.chr[chrDisplayList.num] = &gd.employees[aircraft->teamTypes[i]][aircraft->teamIdxs[i]].chr;

		/* Sanity check(s) */
		if (!chrDisplayList.chr[chrDisplayList.num])
			Sys_Error("CL_GenerateEquipment_f: Could not get employee character with idx: %i\n", chrDisplayList.num);
		Com_DPrintf(DEBUG_CLIENT, "add %s to chrDisplayList (pos: %i)\n", chrDisplayList.chr[chrDisplayList.num]->name, chrDisplayList.num);
		Cvar_ForceSet(va("mn_name%i", chrDisplayList.num), chrDisplayList.chr[chrDisplayList.num]->name);

		/* Update number of displayed team-members. */
		chrDisplayList.num++;
	}

	for (p = chrDisplayList.num; p < MAX_ACTIVETEAM; p++) {
		Cvar_ForceSet(va("mn_name%i", p), "");
		Cbuf_AddText(va("equipdisable%i\n", p));
		chrDisplayList.chr[p] = NULL;	/* just in case */
	}

	if (chrDisplayList.num > 0)
		menuInventory = chrDisplayList.chr[0]->inv;
	else
		menuInventory = NULL;
	selActor = NULL;

	/* reset description */
	Cvar_Set("mn_itemname", "");
	Cvar_Set("mn_item", "");
	MN_MenuTextReset(TEXT_STANDARD);

	/* manage inventory */
	unused = baseCurrent->storage; /* copied, including arrays inside! */

	CL_CleanTempInventory(baseCurrent);
	CL_ReloadAndRemoveCarried(aircraft, &unused);

	/* a 'tiny hack' to add the remaining equipment (not carried)
	 * correctly into buy categories, reloading at the same time;
	 * it is valid only due to the following property: */
	assert(MAX_CONTAINERS >= BUY_AIRCRAFT);

	for (i = 0; i < csi.numODs; i++) {
		/* Don't allow to show armours for other teams in the menu. */
		if ((Q_strncmp(csi.ods[i].type, "armour", MAX_VAR) == 0) && (csi.ods[i].useable != team))
			continue;

		/* Don't allow to show unresearched items. */
		if (!RS_IsResearched_ptr(csi.ods[i].tech))
			continue;

		while (unused.num[i]) {
			const item_t item = {NONE_AMMO, NONE, i, 0, 0};

			/* Check if there are any "multi_ammo" items and move them to the PRI container (along with PRI items of course).
			 * Otherwise just use the container-buytype of the item.
			 * HACKHACK */
			if (BUY_PRI(csi.ods[i].buytype)) {
				if (!Com_TryAddToBuyType(&baseCurrent->equipByBuyType, CL_AddWeaponAmmo(&unused, item), BUY_WEAP_PRI, 1))
					break; /* no space left in menu */
			} else {
				if (!Com_TryAddToBuyType(&baseCurrent->equipByBuyType, CL_AddWeaponAmmo(&unused, item), csi.ods[i].buytype, 1))
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
 * @note Some ic->next and ic will be modified by Com_MoveInInventoryIgnore.
 * @note HACKHACK
 */
static void CL_MoveMultiEquipment (inventory_t* const inv, int buytype_container)
{
	/* Set source container to the one that is not the destination container. */
	const int container = (buytype_container == BUY_WEAP_PRI)
			? BUY_WEAP_SEC : BUY_WEAP_PRI;
	invList_t *ic;
	invList_t *ic_temp;

	if (!inv)
		return;

	/* Do nothing if no pri/sec category is shown. */
	if ((buytype_container != BUY_WEAP_PRI) && (buytype_container != BUY_WEAP_SEC))
		return;

	/* This is a container that might hold some of the affected items.
	 * Move'em to the target (buytype_container) container (if there are any) */
	Com_DPrintf(DEBUG_CLIENT, "CL_MoveMultiEquipment: buytype_container:%i\n", buytype_container);
	Com_DPrintf(DEBUG_CLIENT, "CL_MoveMultiEquipment: container:%i\n", container);
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
		Com_Printf("Usage: %s <category>\n", Cmd_Argv(0));
		return;
	}

	/* read and range check */
	num = atoi(Cmd_Argv(1));
	if (num < 0 && num >= BUY_MULTI_AMMO)
		return;

	/* display new items */
	baseCurrent->equipType = num;
	if (menuInventory) {
		CL_MoveMultiEquipment(&baseCurrent->equipByBuyType, num); /**< Move all multi-ammo items to the current container. */
		menuInventory->c[csi.idEquip] = baseCurrent->equipByBuyType.c[num];
	}
}

typedef enum {
	SELECT_MODE_SOLDIER,
	SELECT_MODE_EQUIP,
	SELECT_MODE_TEAM
} selectSoldierModes_t;

/**
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
	else
		return;

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
		if (!baseCurrent || baseCurrent->aircraftCurrent == AIRCRAFT_INBASE_INVALID)
			return;
		/* no soldiers in the current aircraft */
		if (chrDisplayList.num <= 0) {
			/* multiplayer - load a team first */
			if (!ccs.singleplayer)
				Cbuf_AddText("mn_pop;mn_push mp_team;");
			return;
		/* not that many soldiers from the  aircraft shown. */
		} else if (num >= chrDisplayList.num)
			return;

		/* update menu inventory */
		if (menuInventory && menuInventory != chrDisplayList.chr[num]->inv) {
			chrDisplayList.chr[num]->inv->c[csi.idEquip] = menuInventory->c[csi.idEquip];
			/* set 'old' idEquip to NULL */
			menuInventory->c[csi.idEquip] = NULL;
		}
		menuInventory = chrDisplayList.chr[num]->inv;
		chr = chrDisplayList.chr[num];
		break;
	case SELECT_MODE_TEAM:
		{
			const employeeType_t employeeType = display_heavy_equipment_list
					? EMPL_ROBOT : EMPL_SOLDIER;
			if (!baseCurrent || num >= E_CountHired(baseCurrent, employeeType)) {
				/*Com_Printf("num: %i / max: %i\n", num, E_CountHired(baseCurrent, EMPL_SOLDIER));*/
				return;
			}
			chr = E_GetHiredCharacter(baseCurrent, employeeType, -(num + 1));
			if (!chr)
				Sys_Error("CL_Select_f: No hired character at pos %i (base: %i)\n", num, baseCurrent->idx);
		}
		break;
	default:
		Sys_Error("Unknown select mode %i\n", mode);
	}

	if (mode == SELECT_MODE_SOLDIER) {
		const menu_t *activeMenu = MN_GetActiveMenu();
		if (!Q_strncmp(activeMenu->name, "employees", 9)) {
			/* this is hire menu: we can select soldiers, worker, medics, or researcher */
			if (num < employeesInCurrentList) {
				Cmd_ExecuteString(va("employee_list_click %i", num));
				Cmd_ExecuteString(va("employee_select +%i", num));
			} else
				/* there's no employee corresponding to this num, skip */
				return;
		} else {
			/* HACK */
			/* deselect current selected soldier and select the new one - these are confuncs */
			Cmd_ExecuteString(va("teamdeselect%i", cl_selected->integer));
			Cmd_ExecuteString(va("teamselect%i", num));
			Cmd_ExecuteString(va("equipdeselect%i", cl_selected->integer));
			Cmd_ExecuteString(va("equipselect%i", num));
		}
	} else {
		/* deselect current selected soldier and select the new one - these are confuncs */
		Cmd_ExecuteString(va("%sdeselect%i", command, cl_selected->integer));
		Cmd_ExecuteString(va("%sselect%i", command, num));
	}
	/* now set the cl_selected cvar to the new actor id */
	Cvar_ForceSet("cl_selected", va("%i", num));

	Com_DPrintf(DEBUG_CLIENT, "CL_Select_f: Command: '%s' - num: %i\n", command, num);

	assert(chr);
	/* set info cvars */
	if (chr->empl_type == EMPL_ROBOT)
		CL_UGVCvars(chr);
	else
		CL_CharacterCvars(chr);
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
 * @brief Updates data about teams in aircraft.
 * @param[in] aircraft Pointer to an aircraft for a desired team.
 * @todo We already know what team-members are in the aircraft (teamIdxs+types) no need to loop through hired employees i think.
 */
void CL_UpdateHireVar (aircraft_t *aircraft, employeeType_t employeeType)
{
	int i, p;
	int hired_in_base;
	employee_t *employee;
	base_t *base;

	assert(aircraft);
	base = aircraft->homebase;
	assert(base);

	Cvar_Set("mn_hired", va(_("%i of %i"), aircraft->teamSize, aircraft->maxTeamSize));
	hired_in_base = E_CountHired(base, employeeType);

	/* Uncomment this Com_Printf here for better output in case of problems. */
	/* Com_Printf("CL_UpdateHireVar()... aircraft idx: %i, homebase: %s\n", aircraft->idx, base->name); */

	/* update chrDisplayList list (this is the one that is currently displayed) */
	chrDisplayList.num = 0;
	for (i = 0; i < hired_in_base; i++) {
		employee = E_GetHiredEmployee(base, employeeType, -(i+1));
		if (!employee)
			Sys_Error("CL_UpdateHireVar: Could not get employee %i\n", i);

		if (CL_SoldierInAircraft(employee->idx, employee->type, aircraft->idx)) {
			chrDisplayList.chr[chrDisplayList.num] = &employee->chr;
			chrDisplayList.num++;
		}
	}

	for (p = chrDisplayList.num; p < MAX_ACTIVETEAM; p++)
		chrDisplayList.chr[p] = NULL;	/* Just in case */
}

/**
 * @brief Multiplayer-only call to reset team and team inventory (when setting up new team).
 * @sa E_ResetEmployees
 * @sa CL_CleanTempInventory
 * @note We need baseCurrent to point to gd.bases[0] here.
 * @note Available via script command team_reset.
 * @note Called when initializing the multiplayer menu (for init node and new team button).
 */
void CL_ResetMultiplayerTeamInBase (void)
{
	employee_t* employee;

	if (ccs.singleplayer)
		return;

	Com_DPrintf(DEBUG_CLIENT, "Reset of baseCurrent team flags.\n");
	if (!baseCurrent) {
		Com_DPrintf(DEBUG_CLIENT, "CL_ResetMultiplayerTeamInBase: No baseCurrent\n");
		return;
	}

	CL_CleanTempInventory(baseCurrent);

	AIR_ResetAircraftTeam(B_GetAircraftFromBaseByIndex(baseCurrent,0));

	E_ResetEmployees();
	while (gd.numEmployees[EMPL_SOLDIER] < cl_numnames->integer) {
		employee = E_CreateEmployee(EMPL_SOLDIER, NULL, NULL);
		employee->hired = qtrue;
		employee->baseIDHired = baseCurrent->idx;
		Com_DPrintf(DEBUG_CLIENT, "CL_ResetMultiplayerTeamInBase: Generate character for multiplayer - employee->chr.name: '%s' (base: %i)\n", employee->chr.name, baseCurrent->idx);
	}

	/* reset the multiplayer inventory; stored in baseCurrent->storage */
	{
		const equipDef_t *ed;
		const char *name = "multiplayer";
		int i;

		/* search equipment definition */
		Com_DPrintf(DEBUG_CLIENT, "CL_ResetMultiplayerTeamInBase: no curCampaign - using equipment '%s'\n", name);
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
	const employeeType_t employeeType =
		display_heavy_equipment_list
			? EMPL_ROBOT
			: EMPL_SOLDIER;

	/* check if we are allowed to be here? */
	/* we are only allowed to be here if we already set up a base */
	if (!baseCurrent) {
		Com_Printf("No base set up\n");
		MN_PopMenu(qfalse);
		return;
	}
	if (baseCurrent->aircraftCurrent == AIRCRAFT_INBASE_INVALID) {
		Com_Printf("No aircraft selected\n");
		MN_PopMenu(qfalse);
		return;
	}

	/* create a team if there isn't one already */
	if (gd.numEmployees[employeeType] < 1) {
		Cmd_ExecuteString("new_team");
		MN_Popup(_("New team created"), _("A new team has been created for you."));
	}

	aircraft = &baseCurrent->aircraft[baseCurrent->aircraftCurrent];
	CL_UpdateHireVar(aircraft, employeeType);

	for (i = 0; i < gd.numEmployees[employeeType]; i++) {
		alreadyInOtherShip = qfalse;
		/* don't show other base's recruits or none hired ones */
		if (!gd.employees[employeeType][i].hired || gd.employees[employeeType][i].baseIDHired != baseCurrent->idx)
			continue;

		/* search all aircraft except the current one */
		for (j = 0; j < gd.numAircraft; j++) {
			if (j == aircraft->idx)
				continue;
			/* already on another aircraft */
			if (CL_SoldierInAircraft(i, employeeType, j))
				alreadyInOtherShip = qtrue;
		}

		Cvar_ForceSet(va("mn_name%i", k), gd.employees[employeeType][i].chr.name);
		/* change the buttons */
		Cbuf_AddText(va("listdel%i\n", k));
		if (!alreadyInOtherShip && CL_SoldierInAircraft(i, employeeType, aircraft->idx))
			Cbuf_AddText(va("listadd%i\n", k));
		/* disable the button - the soldier is already on another aircraft */
		else if (alreadyInOtherShip)
			Cbuf_AddText(va("listdisable%i\n", k));

		for (j = 0; j < csi.numIDs; j++) {
			/** @todo FIXME: Wouldn't it be better here to check for temp containers */
			if (j != csi.idFloor && j != csi.idEquip && gd.employees[employeeType][i].inv.c[j])
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
 * @brief changes the displayed list from soldiers to heavy equipment (e.g. tanks)
 * @note console command: team_toggle_list
 */
static void CL_ToggleTeamList_f (void)
{
	if (display_heavy_equipment_list) {
		Com_DPrintf(DEBUG_CLIENT, "Changing to soldier-list.\n");
		display_heavy_equipment_list = qfalse;
		Cbuf_AddText("toggle_show_heavybutton\n");
	} else {
		if (gd.numEmployees[EMPL_ROBOT] > 0) {
			Com_DPrintf(DEBUG_CLIENT, "Changing to heavy equipment (tank) list.\n");
			display_heavy_equipment_list = qtrue;
			Cbuf_AddText("toggle_show_soldiersbutton\n");
		} else {
			/* Nothing to display/assign - staying in soldier-list. */
			Com_DPrintf(DEBUG_CLIENT, "No heavy equipment available.\n");
		}
	}
	CL_MarkTeam_f();
	Cbuf_AddText("team_select 0\n");
}

/**
 * @brief Tells you if a soldier is assigned to an aircraft.
 * @param[in] employee_idx The index of the soldier in the global list.
 * @param[in] aircraft_idx The global index of the aircraft. use -1 to check if the soldier is in _any_ aircraft.
 * @return qboolean qtrue if the soldier was found in the aircraft(s) else: qfalse.
 * @todo params should be employee_t* and aircraft_t*
 */
qboolean CL_SoldierInAircraft (int employee_idx, employeeType_t employeeType, int aircraft_idx)
{
	int i;
	const aircraft_t* aircraft;

	if (employee_idx < 0 || employee_idx > gd.numEmployees[employeeType])
		return qfalse;

	if (aircraft_idx < 0) {
		/* Search if he is in _any_ aircraft and return true if it's the case. */
		for (i = 0; i < gd.numAircraft; i++) {
			if (CL_SoldierInAircraft(employee_idx, employeeType, i))
				return qtrue;
		}
		return qfalse;
	}

	aircraft = AIR_AircraftGetFromIdx(aircraft_idx);
	if (!aircraft)
		Com_Printf("CL_SoldierInAircraft: Error No aircraft found for index %i\n", aircraft_idx);
	return AIR_IsInAircraftTeam(aircraft, employee_idx, employeeType);
}

/**
 * @brief Tells you if a employee is away from his home base (gone in mission).
 * @param[in] employee Pointer to the employee.
 * @return qboolean qtrue if the employee is away in mission, qfalse if he is not or he is unhired.
 */
qboolean CL_SoldierAwayFromBase (employee_t *employee)
{
	int i;
	base_t *base;

	assert(employee);

	/* Check that employee is hired */
	if (!employee->hired)
		return qfalse;

	/* for now only soldiers amd ugvs can be assigned to aircraft */
	/* @todo add pilots */
	if (employee->type != EMPL_SOLDIER && employee->type != EMPL_ROBOT)
		return qfalse;

	base = B_GetBase(employee->baseIDHired);
	assert(base);

	for (i = 0; i < base->numAircraftInBase; i++) {
		const aircraft_t *aircraft = &base->aircraft[i];
		assert(aircraft);

		if (aircraft->status > AIR_HOME && AIR_IsInAircraftTeam(aircraft, employee->idx, employee->type))
			return qtrue;
	}

	return qfalse;
}

/**
 * @brief Removes a soldier from an aircraft.
 * @param[in] employee_idx The index of the soldier in the global list.
 * @param[in] aircraft_idx The global index of aircraft. Use -1 to remove the soldier from any aircraft.
 * @param[in] base Pointer to the base where employee is hired.
 */
void CL_RemoveSoldierFromAircraft (int employee_idx, employeeType_t employeeType, int aircraft_idx, base_t *base)
{
	aircraft_t *aircraft;
	int i;

	if (employee_idx < 0)
		return;

	assert(base);

	if (aircraft_idx < 0) {
		/* Search if he is in _any_ aircraft and set the aircraft_idx if found.. */
		for (i = 0; i < gd.numAircraft; i++) {
			if (CL_SoldierInAircraft(employee_idx, employeeType, i) ) {
				aircraft_idx = i;
				break;
			}
		}
		if (aircraft_idx < 0)
			return;
	}

	aircraft = AIR_AircraftGetFromIdx(aircraft_idx);

	assert(base == aircraft->homebase);

	Com_DPrintf(DEBUG_CLIENT, "CL_RemoveSoldierFromAircraft: base: %i - aircraftID: %i - aircraft_idx: %i\n", base->idx, aircraft->idx, aircraft_idx);

	INVSH_DestroyInventory(&gd.employees[employeeType][employee_idx].inv);
	AIR_RemoveFromAircraftTeam(aircraft, employee_idx, employeeType);
}

/**
 * @brief Removes all soldiers from an aircraft.
 * @param[in] aircraft_idx The global index of aircraft.
 * @param[in] base Pointer to the homebase of the aircraft.
 */
void CL_RemoveSoldiersFromAircraft (int aircraft_idx, base_t *base)
{
	int i = 0;
	aircraft_t* aircraft;

	if (!base)
		return;

	aircraft = AIR_AircraftGetFromIdx(aircraft_idx);

	if (!aircraft || aircraft->idxInBase < 0 || aircraft->idxInBase >= base->numAircraftInBase)
		return;

	/* Counting backwards because aircraft->teamIdxs and teamTypes are changed in CL_RemoveSoldierFromAircraft */
	for (i = aircraft->maxTeamSize; i >= 0; i--) {
		if (aircraft->teamIdxs[i] != -1) {
			/* use global aircraft index here */
			CL_RemoveSoldierFromAircraft(aircraft->teamIdxs[i], aircraft->teamTypes[i], aircraft_idx, base);
		}
	}

	if (aircraft->teamSize > 0) {
		Com_DPrintf(DEBUG_CLIENT, "CL_RemoveSoldiersFromAircraft: there went something wrong with soldier-removing (more exactly the counting) from aircraft.\n");
	}
}

/**
 * @brief Assigns a soldier to an aircraft.
 * @param[in] employee_idx The index of the soldier in the global list.
 * @param[in] aircraft_idx The index of aircraft in the base.
 * @return returns true if a soldier could be assigned to the aircraft.
 */
static qboolean CL_AssignSoldierToAircraft (int employee_idx, employeeType_t employeeType, aircraft_t *aircraft)
{
	if (employee_idx < 0 || !aircraft)
		return qfalse;

	if (aircraft->idxInBase < 0)
		return qfalse;

	if (aircraft->teamSize < MAX_ACTIVETEAM) {
		/* Check whether the soldier is already on another aircraft */
		Com_DPrintf(DEBUG_CLIENT, "CL_AssignSoldierToAircraft: attempting to find idx '%d'\n", employee_idx);

		if (CL_SoldierInAircraft(employee_idx,employeeType, -1)) {
			Com_DPrintf(DEBUG_CLIENT, "CL_AssignSoldierToAircraft: found idx '%d' \n",employee_idx);
			return qfalse;
		}

		/* Assign the soldier to the aircraft. */
		if (aircraft->teamSize < aircraft->maxTeamSize) {
			Com_DPrintf(DEBUG_CLIENT, "CL_AssignSoldierToAircraft: attempting to add idx '%d' \n",employee_idx);
			AIR_AddToAircraftTeam(aircraft, employee_idx, employeeType);
			return qtrue;
		}
#ifdef DEBUG
	} else {
		Com_DPrintf(DEBUG_CLIENT, "CL_AssignSoldierToAircraft: aircraft full - not added\n");
#endif
	}
	return qfalse;
}


#ifdef DEBUG
static void CL_TeamListDebug_f (void)
{
	int i;
	character_t *chr;
	employee_t *employee;
	base_t *base;
	aircraft_t *aircraft;

	base = CP_GetMissionBase();
	aircraft = cls.missionaircraft;

	if (!base) {
		Com_Printf("Build and select a base first\n");
		return;
	}

	if (!aircraft) {
		Com_Printf("Buy/build an aircraft first.\n");
		return;
	}

	Com_Printf("%i members in the current team", aircraft->teamSize);
	for (i = 0; i < aircraft->maxTeamSize; i++) {
		if (aircraft->teamIdxs[i] != -1) {
			employee = E_GetEmployeeFromChrUCN(chr->ucn);
			if (!employee)
				Com_Printf("Could not find a valid employee for ucn %i\n", chr->ucn);
			else
				Com_Printf("ucn %i - employee->idx: %i\n", chr->ucn, employee->idx);
		}
	}
}
#endif

/**
 * @brief Adds or removes a soldier to/from an aircraft.
 * @sa E_EmployeeHire_f
 */
static void CL_AssignSoldier_f (void)
{
	int num = -1;
	employee_t *employee;
	aircraft_t *aircraft;
	const employeeType_t employeeType =
		display_heavy_equipment_list
			? EMPL_ROBOT
			: EMPL_SOLDIER;

	/* check syntax */
	if (Cmd_Argc() < 2) {
		Com_Printf("Usage: %s <num>\n", Cmd_Argv(0));
		return;
	}
	num = atoi(Cmd_Argv(1));

	/* baseCurrent is checked here */
	if (num >= E_CountHired(baseCurrent, employeeType) || num >= cl_numnames->integer) {
		/*Com_Printf("num: %i, max: %i\n", num, E_CountHired(baseCurrent, employeeType));*/
		return;
	}

	employee = E_GetHiredEmployee(baseCurrent, employeeType, -(num + 1));
	if (!employee)
		Sys_Error("CL_AssignSoldier_f: Could not get employee %i\n", num);

	Com_DPrintf(DEBUG_CLIENT, "CL_AssignSoldier_f: employee with idx %i selected\n", employee->idx);
	aircraft = &baseCurrent->aircraft[baseCurrent->aircraftCurrent];
	Com_DPrintf(DEBUG_CLIENT, "aircraft->idx: %i - aircraft->idxInBase: %i\n", aircraft->idx, aircraft->idxInBase);
	assert(aircraft->idxInBase == baseCurrent->aircraftCurrent);

	if (CL_SoldierInAircraft(employee->idx, employee->type, aircraft->idx)) {
		Com_DPrintf(DEBUG_CLIENT, "CL_AssignSoldier_f: removing\n");
		/* Remove soldier from aircraft/team. */
		Cbuf_AddText(va("listdel%i\n", num));
		/* use the global aircraft index here */
		CL_RemoveSoldierFromAircraft(employee->idx, employee->type, aircraft->idx, baseCurrent);
		Cbuf_AddText(va("listholdsnoequip%i\n", num));
	} else {
		Com_DPrintf(DEBUG_CLIENT, "CL_AssignSoldier_f: assigning\n");
		/* Assign soldier to aircraft/team if aircraft is not full */
		if (CL_AssignSoldierToAircraft(employee->idx, employee->type, aircraft))
			Cbuf_AddText(va("listadd%i\n", num));
	}
	/* Select the desired one anyways. */
	CL_UpdateHireVar(aircraft, employee->type);
	Cbuf_AddText(va("team_select %i\n", num));
}

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
	MSG_WriteByte(&sb, aircraft->teamSize);

	/* store aircraft soldier content for multi-player */
	MSG_WriteByte(&sb, aircraft->maxTeamSize);
	for (i = 0; i < aircraft->maxTeamSize; i++) {
		/* We save them all, even unused ones (->size). */
		MSG_WriteShort(&sb, aircraft->teamIdxs[i]);
		MSG_WriteShort(&sb, aircraft->teamTypes[i]);
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

	if (!baseCurrent || !E_CountHired(baseCurrent, EMPL_SOLDIER)) {
		MN_Popup(_("Note"), _("Error saving team. Nothing to save yet."));
		return;
	}

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
	chr->teamDefIndex = MSG_ReadByte(sb);
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
	chr->score.rank = MSG_ReadByte(sb);

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
	employee_t *employee;
	aircraft_t *aircraft;
	int i, p, num;
	base_t *base;

	/* open file */
	f = fopen(filename, "rb");
	if (!f) {
		Com_Printf("Couldn't open file '%s'.\n", filename);
		return;
	}

	CL_MultiplayerEnvironment_f();

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

	base = B_GetBase(0);

	/* load the team */
	/* reset data */
	CL_ResetCharacters(base);

	/* read whole team list */
	num = MSG_ReadByte(&sb);
	Com_DPrintf(DEBUG_CLIENT, "load %i teammembers\n", num);
	E_ResetEmployees();
	for (i = 0; i < num; i++) {
		/* New employee */
		employee = E_CreateEmployee(EMPL_SOLDIER, NULL, NULL);
		employee->hired = qtrue;
		employee->baseIDHired = base->idx;
		CL_LoadTeamMultiplayerMember(&sb, &employee->chr, version);
	}

	aircraft = &base->aircraft[0];
	AIR_ResetAircraftTeam(aircraft);

	/* get assignment */
	aircraft->teamSize = MSG_ReadByte(&sb);

	/* get aircraft soldier content for multi-player */
	Com_DPrintf(DEBUG_CLIENT, "Multiplayer aircraft IDX = %i\n", aircraft->idx);
	aircraft->maxTeamSize = MSG_ReadByte(&sb);
	chrDisplayList.num = 0;
	for (i = 0; i < aircraft->maxTeamSize; i++) {
		aircraft->teamIdxs[i] = MSG_ReadShort(&sb);
		aircraft->teamTypes[i] = MSG_ReadShort(&sb);
		if (aircraft->teamIdxs[i] >= 0) { /** @todo remove this? */
			chrDisplayList.chr[i] = &gd.employees[aircraft->teamTypes[i]][aircraft->teamIdxs[i]].chr;	/** @todo remove this? */
			chrDisplayList.num++;	/** @todo remove this? */
		}
	}

	/* read equipment */
	p = MSG_ReadShort(&sb);
	for (i = 0; i < p; i++) {
		num = INVSH_GetItemByID(MSG_ReadString(&sb));
		if (num == -1) {
			MSG_ReadLong(&sb);
			MSG_ReadByte(&sb);
		} else {
			base->storage.num[num] = MSG_ReadLong(&sb);
			base->storage.num_loose[num] = MSG_ReadByte(&sb);
		}
	}
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
	CL_ResetMultiplayerTeamInBase();
	Cvar_Set("mn_teamname", _("NewTeam"));
	CL_GameExit();
	MN_PushMenu("team");
}

void CL_ResetTeams (void)
{
	Cmd_AddCommand("new_team", CL_GenerateNewTeam_f, "Generates a new empty team");
	Cmd_AddCommand("givename", CL_GiveName_f, "Give the team members names from the team_*.ufo files");
	Cmd_AddCommand("team_reset", CL_ResetMultiplayerTeamInBase, NULL);
	Cmd_AddCommand("genequip", CL_GenerateEquipment_f, NULL);
	Cmd_AddCommand("equip_type", CL_EquipType_f, NULL);
	Cmd_AddCommand("team_mark", CL_MarkTeam_f, NULL);
	Cmd_AddCommand("team_hire", CL_AssignSoldier_f, "Add/remove already hired actor to the aircraft");
	Cmd_AddCommand("team_select", CL_Select_f, NULL);
	Cmd_AddCommand("team_changename", CL_ChangeName_f, "Change the name of an actor");
	Cmd_AddCommand("team_changeskin", CL_ChangeSkin_f, "Change the skin of the soldier");
	Cmd_AddCommand("team_changeskinteam", CL_ChangeSkinOnBoard_f, "Change the skin for the hole team in the current aircraft");
	Cmd_AddCommand("team_comments", CL_MultiplayerTeamSlotComments_f, "Fills the multiplayer team selection menu with the team names");
	Cmd_AddCommand("equip_select", CL_Select_f, NULL);
	Cmd_AddCommand("soldier_select", CL_Select_f, _("Select a soldier from list"));
	Cmd_AddCommand("nextsoldier", CL_NextSoldier_f, _("Toggle to next soldier"));
	Cmd_AddCommand("saveteamslot", CL_SaveTeamMultiplayerSlot_f, "Save a multiplayer team slot - see cvar mn_slot");
	Cmd_AddCommand("loadteamslot", CL_LoadTeamMultiplayerSlot_f, "Load a multiplayer team slot - see cvar mn_slot");
	Cmd_AddCommand("team_toggle_list", CL_ToggleTeamList_f, "Changes between assignment-list for soldiers and heavy equipment (e.g. Tanks)");
	Cmd_AddCommand("mp_env", CL_MultiplayerEnvironment_f, "Prepares environment for multiplayer");
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
	CL_CleanTempInventory(B_GetBase(baseID));

	/* header */
	MSG_WriteByte(buf, num);
	for (i = 0; i < num; i++) {
		chr = E_GetHiredCharacter(B_GetBase(baseID), EMPL_SOLDIER, -(i + 1));
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
		MSG_WriteByte(buf, chr->teamDefIndex);
		MSG_WriteByte(buf, chr->gender);
		MSG_WriteByte(buf, chr->STUN);
		MSG_WriteByte(buf, chr->morale);

		/** Scores @sa inv_shared.h:chrScoreGlobal_t */
		MSG_WriteByte(buf, SKILL_NUM_TYPES+1);
		for (j = 0; j < SKILL_NUM_TYPES+1; j++)
			MSG_WriteLong(buf, chr->score.experience[j]);
		MSG_WriteByte(buf, SKILL_NUM_TYPES);
		for (j = 0; j < SKILL_NUM_TYPES; j++)	/* even new attributes */
			MSG_WriteByte(buf, chr->score.skills[j]);
		MSG_WriteByte(buf, SKILL_NUM_TYPES);
		for (j = 0; j < SKILL_NUM_TYPES; j++)
			MSG_WriteByte(buf, chr->score.initialSkills[j]);
		MSG_WriteByte(buf, KILLED_NUM_TYPES);
		for (j = 0; j < KILLED_NUM_TYPES; j++)
			MSG_WriteShort(buf, chr->score.kills[j]);
		MSG_WriteByte(buf, KILLED_NUM_TYPES);
		for (j = 0; j < KILLED_NUM_TYPES; j++)
			MSG_WriteShort(buf, chr->score.stuns[j]);
		MSG_WriteShort(buf, chr->score.assignedMissions);
		MSG_WriteByte(buf, chr->score.rank);

		/* inventory */
		CL_SaveInventory(buf, chr->inv);
	}
}

/**
 * @brief Stores a team-list (chr-list) info to buffer (which might be a network buffer, too).
 * @sa G_ClientTeamInfo
 * @sa CL_SaveTeamInfo
 * @note Called in cl_main.c CL_Precache_f to send the team info to server
 */
void CL_SendCurTeamInfo (struct dbuffer * buf, chrList_t *team)
{
	character_t *chr;
	int i, j;

	assert(baseCurrent);

	/* clean temp inventory */
	CL_CleanTempInventory(baseCurrent);

	/* header */
	NET_WriteByte(buf, clc_teaminfo);
	NET_WriteByte(buf, team->num);

	Com_DPrintf(DEBUG_CLIENT, "CL_SendCurTeamInfo: Upload information about %i soldiers to server\n", team->num);
	for (i = 0; i < team->num; i++) {
		chr = team->chr[i];
		assert(chr);
		assert(chr->fieldSize > 0);
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
		NET_WriteByte(buf, chr->teamDefIndex);
		NET_WriteByte(buf, chr->gender);
		NET_WriteByte(buf, chr->STUN);
		NET_WriteByte(buf, chr->morale);

		/** Scores @sa inv_shared.h:chrScoreGlobal_t */
		for (j = 0; j < SKILL_NUM_TYPES + 1; j++)
			NET_WriteLong(buf, chr->score.experience[j]);
		for (j = 0; j < SKILL_NUM_TYPES; j++)
			NET_WriteByte(buf, chr->score.skills[j]);
		for (j = 0; j < SKILL_NUM_TYPES; j++)
			NET_WriteByte(buf, chr->score.initialSkills[j]);
		for (j = 0; j < KILLED_NUM_TYPES; j++)
			NET_WriteShort(buf, chr->score.kills[j]);
		for (j = 0; j < KILLED_NUM_TYPES; j++)
			NET_WriteShort(buf, chr->score.stuns[j]);
		NET_WriteShort(buf, chr->score.assignedMissions);
		NET_WriteByte(buf, chr->score.rank);

		/* inventory */
		CL_NetSendInventory(buf, chr->inv);
	}
}

typedef struct {
	int ucn;
	int HP;
	int STUN;
	int morale;

	chrScoreGlobal_t chrscore;
} updateCharacter_t;

/**
 * @brief Parses the character data which was send by G_EndGame using G_SendCharacterData
 * @param[in] msg The network buffer message. If this is NULL the character is updated, if this
 * is not NULL the data is stored in a temp buffer because the player can choose to retry
 * the mission and we have to catch this situation to not update the character data in this case.
 * @sa G_SendCharacterData
 * @sa CL_SendCurTeamInfo
 * @sa G_EndGame
 * @sa E_Save
 */
void CL_ParseCharacterData (struct dbuffer *msg)
{
	static updateCharacter_t updateCharacterArray[MAX_WHOLETEAM];
	static int num = 0;
	int i, j;
	character_t* chr;
	employee_t* employee;

	if (!msg) {
		for (i = 0; i < num; i++) {
			employee = E_GetEmployeeFromChrUCN(updateCharacterArray[i].ucn);
			if (!employee) {
				Com_Printf("Warning: Could not get character with ucn: %i.\n", updateCharacterArray[i].ucn);
				continue;
			}
			chr = &employee->chr;
			chr->HP = updateCharacterArray[i].HP;
			chr->STUN = updateCharacterArray[i].STUN;
			chr->morale = updateCharacterArray[i].morale;

			/** Scores @sa inv_shared.h:chrScoreGlobal_t */
			memcpy(chr->score.experience, updateCharacterArray[i].chrscore.experience, sizeof(chr->score.experience));
			memcpy(chr->score.skills, updateCharacterArray[i].chrscore.skills, sizeof(chr->score.skills));
			memcpy(chr->score.kills, updateCharacterArray[i].chrscore.kills, sizeof(chr->score.kills));
			memcpy(chr->score.stuns, updateCharacterArray[i].chrscore.stuns, sizeof(chr->score.stuns));
			chr->score.assignedMissions = updateCharacterArray[i].chrscore.assignedMissions;
			chr->score.rank = updateCharacterArray[i].chrscore.rank;
		}
		num = 0;
	} else {
		/* invalidate ucn in the array first */
		for (i = 0; i < MAX_WHOLETEAM; i++) {
			updateCharacterArray[i].ucn = -1;
		}
		/* number of soldiers */
		num = NET_ReadByte(msg);
		if (num > MAX_WHOLETEAM)
			Sys_Error("CL_ParseCharacterData: num exceeded MAX_WHOLETEAM\n");
		else if (num < 0)
			Sys_Error("CL_ParseCharacterData: NET_ReadShort error (%i)\n", num);

		for (i = 0; i < num; i++) {
			/* updateCharacter_t */
			updateCharacterArray[i].ucn = NET_ReadShort(msg);
			updateCharacterArray[i].HP = NET_ReadShort(msg);
			updateCharacterArray[i].STUN = NET_ReadByte(msg);
			updateCharacterArray[i].morale = NET_ReadByte(msg);

			/** Scores @sa inv_shared.h:chrScoreGlobal_t */
			for (j = 0; j < SKILL_NUM_TYPES + 1; j++)
				updateCharacterArray[i].chrscore.experience[j] = NET_ReadLong(msg);
			for (j = 0; j < SKILL_NUM_TYPES; j++)
				updateCharacterArray[i].chrscore.skills[j] = NET_ReadByte(msg);
			for (j = 0; j < KILLED_NUM_TYPES; j++)
				updateCharacterArray[i].chrscore.kills[j] = NET_ReadShort(msg);
			for (j = 0; j < KILLED_NUM_TYPES; j++)
				updateCharacterArray[i].chrscore.stuns[j] = NET_ReadShort(msg);
			updateCharacterArray[i].chrscore.assignedMissions = NET_ReadShort(msg);
			updateCharacterArray[i].chrscore.rank = NET_ReadByte(msg);
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
	base_t *base;

	/* get number of teams */
	num = NET_ReadByte(msg);
	if (num > MAX_TEAMS)
		Sys_Error("Too many teams in result message\n");

	Com_DPrintf(DEBUG_CLIENT, "Receiving results with %i teams.\n", num);

	/* get winning team */
	winner = NET_ReadByte(msg);
	we = cls.team;

	if (we > num)
		Sys_Error("Team number %d too high (only %d teams)\n", we, num);

	/* get spawn and alive count */
	for (i = 0; i < num; i++) {
		num_spawned[i] = NET_ReadByte(msg);
		num_alive[i] = NET_ReadByte(msg);
	}

	/* get kills */
	for (i = 0; i < num; i++)
		for (j = 0; j < num; j++)
			num_kills[i][j] = NET_ReadByte(msg);

	/* get stuns */
	for (i = 0; i < num; i++)
		for (j = 0; j < num; j++)
			num_stuns[i][j] = NET_ReadByte(msg);

	base = CP_GetMissionBase();
	CL_ParseCharacterData(msg);

	/* init result text */
	mn.menuText[TEXT_STANDARD] = resultText;

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

	if (!curCampaign || !selectedMission) {
		/* the mission was started via console (@todo: or is multiplayer) */
		/* buffer needs to be cleared and then append to it */
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

		MN_PopMenu(qtrue);
		Cvar_Set("mn_main", "main");
		Cvar_Set("mn_active", "");
		MN_PushMenu("main");
	} else {
		/* the mission was in singleplayer */
		/* loot the battlefield */
		INV_CollectingItems(winner == we);				/**< Collect items from the battlefield. */
		if (winner == we)
			AL_CollectingAliens(cls.missionaircraft);	/**< Collect aliens from the battlefield. */

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

		MN_PopMenu(qtrue);
		Cvar_Set("mn_main", "singleplayerInGame");
		Cvar_Set("mn_active", "map");
		MN_PushMenu("map");
	}
	/* show win screen */
	if (ccs.singleplayer) {
		/* Make sure that at this point we are able to 'Try Again' a mission. */
		Cvar_SetValue("mission_tryagain", 0);
		if (selectedMission && base)
			CP_ExecuteMissionTrigger(selectedMission, winner == we);
		else if (curCampaign)
			Com_Printf("CL_ParseResults: Error - no mission triggers, because selectedMission or baseCurrent are not valid\n");

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

		/* on singleplayer we disconnect the game and shutdown the server
		 * we can safely wipe all mission data now */
		SV_Shutdown("Mission end", qfalse);
		CL_Disconnect();
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
			Q_strncpyz(popupText, _("You've lost the game"), sizeof(popupText));
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

static const value_t rankValues[] = {
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
	rank_t *rank;
	const char *errhead = "CL_ParseMedalsAndRanks: unexpected end of file (medal/rank ";
	const char *token;
	const value_t *v;
	int i;

	/* get name list body body */
	token = COM_Parse(text);

	if (!*text || *token != '{') {
		Com_Printf("CL_ParseMedalsAndRanks: rank/medal \"%s\" without body ignored\n", name);
		return;
	}

	if (parserank) {
		for (i = 0; i < gd.numRanks; i++) {
			if (!Q_strcmp(name, gd.ranks[i].name)) {
				Com_Printf("CL_ParseMedalsAndRanks: Rank with same name '%s' already loaded.\n", name);
				return;
			}
		}
		/* parse ranks */
		if (gd.numRanks >= MAX_RANKS) {
			Com_Printf("CL_ParseMedalsAndRanks: Too many rank descriptions, '%s' ignored.\n", name);
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
					case V_TRANSLATION_MANUAL_STRING:
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
				/* error check is performed in E_GetEmployeeType function */
				rank->type = E_GetEmployeeType(token);
			} else if (!v->string)
				Com_Printf("CL_ParseMedalsAndRanks: unknown token \"%s\" ignored (medal/rank %s)\n", token, name);
		} while (*text);
	} else {
		/* parse medals */
	}
}

static const value_t ugvValues[] = {
	{"tu", V_INT, offsetof(ugv_t, tu), MEMBER_SIZEOF(ugv_t, tu)},
	{"weapon", V_STRING, offsetof(ugv_t, weapon), 0},
	{"armour", V_STRING, offsetof(ugv_t, armour), 0},
	{"actors", V_STRING, offsetof(ugv_t, actors), 0},
	{"price", V_INT, offsetof(ugv_t, price), 0},

	{NULL, 0, 0, 0}
};

/**
 * @brief Parse 2x2 units (e.g. UGVs)
 * @sa CL_ParseClientData
 */
void CL_ParseUGVs (const char *name, const char **text)
{
	const char *errhead = "Com_ParseUGVs: unexpected end of file (ugv ";
	const char *token;
	const value_t *v;
	ugv_t *ugv;
	int i;

	/* get name list body body */
	token = COM_Parse(text);

	if (!*text || *token != '{') {
		Com_Printf("Com_ParseUGVs: ugv \"%s\" without body ignored\n", name);
		return;
	}

	for (i = 0; i < gd.numUGV; i++) {
		if (!Q_strcmp(name, gd.ugvs[i].id)) {
			Com_Printf("Com_ParseUGVs: ugv \"%s\" with same name already loaded\n", name);
			return;
		}
	}

	/* parse ugv */
	if (gd.numUGV >= MAX_UGV) {
		Com_Printf("Too many UGV descriptions, '%s' ignored.\n", name);
		return;
	}

	ugv = &gd.ugvs[gd.numUGV++];
	memset(ugv, 0, sizeof(ugv_t));
	ugv->id = Mem_PoolStrDup(name, cl_localPool, CL_TAG_REPARSE_ON_NEW_GAME);

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
