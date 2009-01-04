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
#include "cl_game.h"
#include "cl_team.h"
#include "cl_actor.h"
#include "campaign/cl_ufo.h"
#include "menu/node/m_node_container.h"
#include "menu/m_popup.h"
#include "campaign/cp_mission_triggers.h"

static qboolean displayHeavyEquipmentList = qfalse; /**< Used in team assignment screen to tell if we are assigning soldiers or heavy equipment (ugvs/tanks) */

/**
 * @brief The current selected category in equip menu.
 * @todo Check if we can change this to be of type "itemFilterTypes_t"
 */
int equipType;

linkedList_t *employeeList;	/* @sa E_GetEmployeeByMenuIndex */
int employeesInCurrentList;

static int CL_GetRankIdx(const char* rankID);
static void CL_MarkTeam_f(void);

/* List of currently displayed or equipeable characters. extern definition in client.h */
chrList_t chrDisplayList;

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
 * @brief Returns the aircraft for the team and soldier selection menus
 * @note Multiplayer and skirmish are using @c cls.missionaircraft, campaign mode is
 * using the current selected aircraft in base
 */
static aircraft_t *CL_GetTeamAircraft (base_t *base)
{
	if (GAME_IsCampaign()) {
		if (!base)
			Sys_Error("CL_GetTeamAircraft: Called without base");
		return base->aircraftCurrent;
	} else {
		return cls.missionaircraft;
	}
}

/**
 * @sa CL_LoadItem
 */
static void CL_SaveItem (sizebuf_t *buf, item_t item, int container, int x, int y)
{
	assert(item.t);
/*	Com_Printf("Add item %s to container %i (t=%i:a=%i:m=%i) (x=%i:y=%i)\n", csi.ods[item.t].id, container, item.t, item.a, item.m, x, y);*/
	MSG_WriteFormat(buf, "bbbbbl", item.a, container, x, y, item.rotated, item.amount);
	MSG_WriteString(buf, item.t->id);
	if (item.a > NONE_AMMO)
		MSG_WriteString(buf, item.m->id);
}

/**
 * @sa CL_SaveItem
 * @sa CL_LoadInventory
 */
void CL_SaveInventory (sizebuf_t *buf, const inventory_t *i)
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
	memset(item, 0, sizeof(*item));
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
		if (!Com_AddToInventory(i, item, &csi.ids[container], x, y, 1))
			Com_Printf("Could not add item '%s' to inventory\n", item.t ? item.t->id : "NULL");
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
	int ammoIdx = item.m ? item.m->idx : NONE;
	assert(item.t);
	Com_DPrintf(DEBUG_CLIENT, "CL_NetSendItem: Add item %s to container %i (t=%i:a=%i:m=%i) (x=%i:y=%i)\n",
		item.t->id, container, item.t->idx, item.a, ammoIdx, x, y);
	NET_WriteFormat(buf, ev_format[EV_INV_TRANSFER], item.t->idx, item.a, ammoIdx, container, x, y, item.rotated);
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
	int t, m;
	item->t = item->m = NULL;
	item->a = NONE_AMMO;
	NET_ReadFormat(buf, ev_format[EV_INV_TRANSFER], &t, &item->a, &m, container, x, y, &item->rotated);

	if (t != NONE)
		item->t = &csi.ods[t];

	if (m != NONE)
		item->m = &csi.ods[m];
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
		Com_Printf("%s", name);

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
ugv_t *CL_GetUgvByID (const char *ugvID)
{
	int i;

	for (i = 0; i < gd.numUGV; i++) {
		if (!Q_strcmp(gd.ugvs[i].id, ugvID)) {
			return &gd.ugvs[i];
		}
	}

	Com_Printf("CL_GetUgvByID: No ugv_t entry found for id '%s' in %i entries.\n", ugvID, gd.numUGV);
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
	int teamValue;

	if (!employee)
		return;

	chr = &employee->chr;
	memset(chr, 0, sizeof(*chr));

	/* link inventory */
	chr->inv = &employee->inv;
	INVSH_DestroyInventory(chr->inv);

	/* get ucn */
	chr->ucn = gd.nextUCN++;

	Com_DPrintf(DEBUG_CLIENT, "Generate character for team: '%s' (type: %i)\n", team, employeeType);

	/* Backlink from chr to employee struct. */
	chr->emplIdx = employee->idx;
	chr->emplType = employeeType;

	/* if not human - then we TEAM_ALIEN */
	if (strstr(team, "human"))
		teamValue = TEAM_PHALANX;
	else if (strstr(team, "alien"))
		teamValue = TEAM_ALIEN;
	else
		teamValue = TEAM_CIVILIAN;

	/** Set default reaction-mode for all character-types to "once".
	 * AI actor (includes aliens if one doesn't play AS them) are set in @sa g_ai.c:G_SpawnAIPlayer */
	chr->reservedTus.reserveReaction = STATE_REACTION_ONCE;

	CL_CharacterSetShotSettings(chr, -1, -1, -1);

	/* Generate character stats, models & names. */
	switch (employeeType) {
	case EMPL_SOLDIER:
		chr->score.rank = CL_GetRankIdx("rifleman");
		/* Create attributes. */
		CHRSH_CharGenAbilitySkills(chr, teamValue, employeeType, GAME_IsMultiplayer());
		Q_strncpyz(teamDefName, team, sizeof(teamDefName));
		break;
	case EMPL_SCIENTIST:
		chr->score.rank = CL_GetRankIdx("scientist");
		/* Create attributes. */
		CHRSH_CharGenAbilitySkills(chr, teamValue, employeeType, GAME_IsMultiplayer());
		Com_sprintf(teamDefName, sizeof(teamDefName), "%s_scientist", team);
		break;
	case EMPL_PILOT:
		chr->score.rank = CL_GetRankIdx("pilot");
		/* Create attributes. */
		CHRSH_CharGenAbilitySkills(chr, teamValue, employeeType, GAME_IsMultiplayer());
		Com_sprintf(teamDefName, sizeof(teamDefName), "%s_pilot", team);
		break;
	case EMPL_WORKER:
		chr->score.rank = CL_GetRankIdx("worker");
		/* Create attributes. */
		CHRSH_CharGenAbilitySkills(chr, teamValue, employeeType, GAME_IsMultiplayer());
		Com_sprintf(teamDefName, sizeof(teamDefName), "%s_worker", team);
		break;
	case EMPL_ROBOT:
		if (!ugvType)
			Sys_Error("CL_GenerateCharacter: no type given for generation of EMPL_ROBOT employee.\n");

		chr->score.rank = CL_GetRankIdx("ugv");

		/** Create attributes.
		 * @todo get the min/max values from ugv_t def? */
		CHRSH_CharGenAbilitySkills(chr, teamValue, employeeType, GAME_IsMultiplayer());

		Com_sprintf(teamDefName, sizeof(teamDefName), "%s%s", team, ugvType->actors);
		break;
	default:
		Sys_Error("Unknown employee type\n");
		break;
	}
	chr->skin = Com_GetCharacterValues(teamDefName, chr);
}


/**
 * @brief Remove all character_t information (and linked to that employees & team info) from the game.
 * @param[in] base The base to remove all this stuff from.
 * @sa CL_GenerateCharacter
 * @sa AIR_ResetAircraftTeam
 */
void CL_ResetCharacters (base_t* const base)
{
	int i;
	employee_t *employee;
	linkedList_t *hiredEmployees = NULL;
	linkedList_t *hiredEmployeesTemp;

	/* Reset inventory data of all hired employees that can be sent into combat (i.e. characters with inventories).
	 * i.e. these are soldiers and robots right now. */
	E_GetHiredEmployees(base, EMPL_SOLDIER, &hiredEmployees);
	hiredEmployeesTemp = hiredEmployees;
	while (hiredEmployeesTemp) {
		employee = (employee_t*)hiredEmployeesTemp->data;
		if (employee)
			INVSH_DestroyInventory(employee->chr.inv);
		hiredEmployeesTemp = hiredEmployeesTemp->next;
	}

	E_GetHiredEmployees(base, EMPL_ROBOT, &hiredEmployees);
	hiredEmployeesTemp = hiredEmployees;
	while (hiredEmployeesTemp) {
		employee = (employee_t*)hiredEmployeesTemp->data;
		if (employee)
			INVSH_DestroyInventory(employee->chr.inv);
		hiredEmployeesTemp = hiredEmployeesTemp->next;
	}

	LIST_Delete(&hiredEmployees);

	/* Reset hire info. */
	Cvar_ForceSet("cl_selected", "0");

	/* Fire 'em all (in multiplayer they are not hired) */
	for (i = 0; i < MAX_EMPL; i++) {
		E_UnhireAllEmployees(base, i);
	}

	/** @todo in multiplayer and skirmish we have a fakeaircraft - but no base */
	/* Purge all team-info from the aircraft */
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
	employee_t *employee = selectedEmployee;

	/* Maybe called without base initialized or active. */
	if (!baseCurrent)
		return;

	if (employee) {
		const menu_t *activeMenu = MN_GetActiveMenu();
		Q_strncpyz(employee->chr.name, Cvar_VariableString("mn_name"), MAX_VAR);

		/* Now refresh the list. */
		if (!Q_strncmp(activeMenu->name, "employees", 9)) {
			/* We are in the hire (aka "employee") screen. */
			Cbuf_AddText(va("employee_init %i %i;", employee->type, employee->idx));
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
		if (GAME_IsSingleplayer() && newSkin >= NUM_TEAMSKINS_SINGLEPLAYER)
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
	if (baseCurrent->aircraftCurrent) {
		/* aircraft = &baseCurrent->aircraft[baseCurrent->aircraftCurrent]; */
	} else {
#ifdef DEBUG
		/* Should never happen. */
		Com_Printf("CL_ChangeSkinOnBoard_f: No aircraft selected!\n");
#endif
		return;
	}

	/* Get selected skin and fall back to default skin if it is not valid. */
	newSkin = Cvar_VariableInteger("mn_skin");
	if (newSkin >= NUM_TEAMSKINS || newSkin < 0)
		newSkin = 0;

	/* don't allow all skins in singleplayer */
	if (GAME_IsSingleplayer() && newSkin >= NUM_TEAMSKINS_SINGLEPLAYER)
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
 * @brief Move all the equipment carried by the team on the aircraft into the given equipment
 * @param[in] aircraft The craft with the team (and thus equipment) onboard.
 * @param[out] ed The equipment definition which will receive all the stuff from the aircraft-team.
 */
void CL_AddCarriedToEquipment (const aircraft_t *aircraft, equipDef_t *ed)
{
	int p, container;

	if (!aircraft) {
		Com_Printf("CL_AddCarriedToEquipment: Warning: Called with no aicraft (and thus no carried equipment to add).\n");
		return;
	}
	if (!ed) {
		Com_Printf("CL_AddCarriedToEquipment: Warning: Called with no equipment definition at add stuff to.\n");
		return;
	}

	if (aircraft->teamSize <= 0) {
		Com_DPrintf(DEBUG_CLIENT, "CL_AddCarriedToEquipment: No team to remove equipment from.\n");
		return;
	}

	for (container = 0; container < csi.numIDs; container++) {
		for (p = 0; p < aircraft->maxTeamSize; p++) {
			if (aircraft->acTeam[p]) {
				character_t *chr = &aircraft->acTeam[p]->chr;
				invList_t *ic = chr->inv->c[container];
				while (ic) {
					const item_t item = ic->item;
					const objDef_t *type = item.t;
					invList_t *next = ic->next;

					ed->num[type->idx]++;
					if (item.a) {
						assert(type->reload);
						assert(item.m);
						ed->numLoose[item.m->idx] += item.a;
						/* Accumulate loose ammo into clips */
						if (ed->numLoose[item.m->idx] >= type->ammo) {
							ed->numLoose[item.m->idx] -= type->ammo;
							ed->num[item.m->idx]++;
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
	objDef_t *type = item.t;

	assert(ed->num[type->idx] > 0);
	ed->num[type->idx]--;

	if (type->weapons[0]) {
		/* The given item is ammo or self-contained weapon (i.e. It has firedefinitions. */
		if (type->oneshot) {
			/* "Recharge" the oneshot weapon. */
			item.a = type->ammo;
			item.m = item.t; /* Just in case this hasn't been done yet. */
			Com_DPrintf(DEBUG_CLIENT, "CL_AddWeaponAmmo: oneshot weapon '%s'.\n", type->id);
			return item;
		} else {
			/* No change, nothing needs to be done to this item. */
			return item;
		}
	} else if (!type->reload) {
		/* The given item is a weapon but no ammo is needed,
		 * so fire definitions are in t (the weapon). Setting equal. */
		item.m = item.t;
		return item;
	} else if (item.a) {
		assert(item.m);
		/* The item is a weapon and it was reloaded one time. */
		if (item.a == type->ammo) {
			/* Fully loaded, no need to reload, but mark the ammo as used. */
			assert(item.m);	/** @todo Isn't this redundant here? */
			if (ed->num[item.m->idx] > 0) {
				ed->num[item.m->idx]--;
				return item;
			} else {
				/* Your clip has been sold; give it back. */
				item.a = NONE_AMMO;
				return item;
			}
		}
	}

	/* Check for complete clips of the same kind */
	if (item.m && ed->num[item.m->idx] > 0) {
		ed->num[item.m->idx]--;
		item.a = type->ammo;
		return item;
	}

	/* Search for any complete clips. */
	/** @todo We may want to change this to use the type->ammo[] info. */
	for (i = 0; i < csi.numODs; i++) {
		if (INVSH_LoadableInWeapon(&csi.ods[i], type)) {
			if (ed->num[i] > 0) {
				ed->num[i]--;
				item.a = type->ammo;
				item.m = &csi.ods[i];
				return item;
			}
		}
	}

	/** @todo on return from a mission with no clips left
	 * and one weapon half-loaded wielded by soldier
	 * and one empty in equip, on the first opening of equip,
	 * the empty weapon will be in soldier hands, the half-full in equip;
	 * this should be the other way around. */

	/* Failed to find a complete clip - see if there's any loose ammo
	 * of the same kind; if so, gather it all in this weapon. */
	if (item.m && ed->numLoose[item.m->idx] > 0) {
		item.a = ed->numLoose[item.m->idx];
		ed->numLoose[item.m->idx] = 0;
		return item;
	}

	/* See if there's any loose ammo */
	/** @todo We may want to change this to use the type->ammo[] info. */
	item.a = NONE_AMMO;
	for (i = 0; i < csi.numODs; i++) {
		if (INVSH_LoadableInWeapon(&csi.ods[i], type)
		&& (ed->numLoose[i] > item.a)) {
			if (item.a > 0) {
				/* We previously found some ammo, but we've now found other
				 * loose ammo of a different (but appropriate) type with
				 * more bullets.  Put the previously found ammo back, so
				 * we'll take the new type. */
				assert(item.m);
				ed->numLoose[item.m->idx] = item.a;
				/* We don't have to accumulate loose ammo into clips
				 * because we did it previously and we create no new ammo */
			}
			/* Found some loose ammo to load the weapon with */
			item.a = ed->numLoose[i];
			ed->numLoose[i] = 0;
			item.m = &csi.ods[i];
		}
	}
	return item;
}

/**
 * @brief Reloads weapons and removes "not assigned" ones from containers.
 * @param[in] aircraft Pointer to an aircraft for given team.
 * @param[in] ed equipDef_t pointer to equipment of given character in a team.
 * @sa CL_AddWeaponAmmo
 */
void CL_ReloadAndRemoveCarried (aircraft_t *aircraft, equipDef_t * ed)
{
	base_t *base;
	character_t *chr;
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

	Com_DPrintf(DEBUG_CLIENT, "CL_ReloadAndRemoveCarried:aircraft idx: %i, team size: %i\n",
		aircraft->idx, aircraft->teamSize);

	/* Auto-assign weapons to UGVs/Robots if they have no weapon yet. */
	for (p = 0; p < aircraft->maxTeamSize; p++) {
		if (aircraft->acTeam[p] && aircraft->acTeam[p]->ugv) {
			/** This is an UGV */
			chr = &aircraft->acTeam[p]->chr;
			assert(chr);

			/* Check if there is a weapon and add it if there isn't. */
			if (!chr->inv->c[csi.idRight] || !chr->inv->c[csi.idRight]->item.t)
				INVSH_EquipActorRobot(chr->inv, chr, INVSH_GetItemByID(aircraft->acTeam[p]->ugv->weapon));
		}
	}

	for (container = 0; container < csi.numIDs; container++) {
		for (p = 0; p < aircraft->maxTeamSize; p++) {
			if (aircraft->acTeam[p]) {
				chr = &aircraft->acTeam[p]->chr;
				assert(chr);
				for (ic = chr->inv->c[container]; ic; ic = next) {
					next = ic->next;
					if (ed->num[ic->item.t->idx] > 0) {
						ic->item = CL_AddWeaponAmmo(ed, ic->item);
					} else {
						/* Drop ammo used for reloading and sold carried weapons. */
						Com_RemoveFromInventory(chr->inv, &csi.ids[container], ic);
					}
				}
			}
		}
	}
}

/**
 * @brief Clears all containers that are temp containers (see script definition).
 * @sa CL_GenerateEquipment_f
 * @sa CL_ResetMultiplayerTeamInAircraft
 * @sa CL_SaveTeamMultiplayerInfo
 * @sa CL_SendCurTeamInfo
 */
void CL_CleanTempInventory (base_t* base)
{
	int i, k;

	for (i = 0; i < MAX_EMPLOYEES; i++)
		for (k = 0; k < csi.numIDs; k++)
			if (csi.ids[k].temp) {
				/* idFloor and idEquip are temp */
				gd.employees[EMPL_SOLDIER][i].inv.c[k] = NULL;
				gd.employees[EMPL_ROBOT][i].inv.c[k] = NULL;
			}

	if (!base)
		return;

	INVSH_DestroyInventory(&base->bEquipment);
}

/**
 * @brief Displays actor equipment and unused items in proper (filter) category.
 * @note This function is called everytime the equipment screen for the team pops up.
 * @todo Do we allow EMPL_ROBOTs to be equipable? Or is simple buying of ammo enough (similar to original UFO/XCOM)?
 * In the first case the EMPL_SOLDIER stuff below needs to be changed.
 */
static void CL_GenerateEquipment_f (void)
{
	equipDef_t unused;
	int i, p;
	aircraft_t *aircraft;
	int team;

	if (!baseCurrent || !baseCurrent->aircraftCurrent)
		return;

	aircraft = baseCurrent->aircraftCurrent;

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

	/** @todo Skip EMPL_ROBOT (i.e. ugvs) for now . */
	p = CL_UpdateActorAircraftVar(aircraft, EMPL_SOLDIER);
	for (; p < MAX_ACTIVETEAM; p++) {
		Cvar_ForceSet(va("mn_name%i", p), "");
		Cbuf_AddText(va("equipdisable%i\n", p));
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
	assert(MAX_CONTAINERS >= FILTER_AIRCRAFT);

	for (i = 0; i < csi.numODs; i++) {
		/* Don't allow to show armour for other teams in the menu. */
		if ((Q_strncmp(csi.ods[i].type, "armour", MAX_VAR) == 0) && (csi.ods[i].useable != team))
			continue;

		/* Don't allow to show unresearched items. */
		if (!RS_IsResearched_ptr(csi.ods[i].tech))
			continue;

		while (unused.num[i]) {
			const item_t item = {NONE_AMMO, NULL, &csi.ods[i], 0, 0};

			if (!Com_AddToInventory(&baseCurrent->bEquipment, CL_AddWeaponAmmo(&unused, item), &csi.ids[csi.idEquip], NONE, NONE, 1))
				break; /* no space left in menu */
		}
	}
}

/**
 * @brief Sets (filter type) category for equip menu.
 */
static void CL_EquipType_f (void)
{
	itemFilterTypes_t num;	/**< filter type/equipment category. */

	if (Cmd_Argc() < 2) {
		Com_Printf("Usage: %s <category name>\n", Cmd_Argv(0));
		return;
	}

	if (!baseCurrent)
		return;

	/* Read filter type and check range. */
	num = INV_GetFilterTypeID(Cmd_Argv(1));
	if (num < 0 || num >= MAX_SOLDIER_FILTERTYPES)
		return;

	/* Reset scroll info for a new filter type/category. */
	if (menuInventory && (equipType != num || !menuInventory->c[csi.idEquip])) {
		menuInventory->scrollCur = 0;
		menuInventory->scrollNum = 0;
		menuInventory->scrollTotalNum = 0;
	}

	/* Display new items. */
	equipType = num;

	/* First-time linking of menuInventory. */
	if (menuInventory && !menuInventory->c[csi.idEquip]) {
		menuInventory->c[csi.idEquip] = baseCurrent->bEquipment.c[csi.idEquip];
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
	employee_t *employee;
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
		/* no soldiers in the current aircraft */
		if (chrDisplayList.num <= 0) {
			/* multiplayer - load a team first */
			if (GAME_IsMultiplayer()) {
				MN_PopMenu(qfalse);
				MN_PushMenu("mp_team", NULL);
			}
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
			const employeeType_t employeeType = displayHeavyEquipmentList
					? EMPL_ROBOT : EMPL_SOLDIER;
			base_t *base = GAME_IsCampaign() ? baseCurrent : NULL;

			if (GAME_IsCampaign() && !base)
				return;

			if (num >= E_CountHired(base, employeeType))
				return;

			employee = E_GetEmployeeByMenuIndex(num);
			if (!employee)
				Sys_Error("CL_Select_f: No employee at list-pos %i (base: %i)\n", num, base ? base->idx : -1);

			chr = &employee->chr;
			if (!chr)
				Sys_Error("CL_Select_f: No hired character at list-pos %i (base: %i)\n", num, base ? base->idx : -1);
		}
		break;
	default:
		Sys_Error("Unknown select mode %i\n", mode);
	}

	if (mode == SELECT_MODE_SOLDIER) {
		const menu_t *activeMenu = MN_GetActiveMenu();
		if (!Q_strncmp(activeMenu->name, "employees", 9)) {
			/* this is hire menu: we can select soldiers, worker, pilots, or researcher */
			if (num < employeesInCurrentList) {
				Cmd_ExecuteString(va("employee_list_click %i", num));
				Cmd_ExecuteString(va("employee_select +%i", num));
			} else
				/* there's no employee corresponding to this num, skip */
				return;
		} else {
			/* HACK */
			/* deselect current selected soldier and select the new one */
			MN_ExecuteConfunc("teamdeselect%i", cl_selected->integer);
			MN_ExecuteConfunc("teamselect%i", num);
			MN_ExecuteConfunc("equipdeselect%i", cl_selected->integer);
			MN_ExecuteConfunc("equipselect%i", num);
		}
	} else {
		/* deselect current selected soldier and select the new one */
		MN_ExecuteConfunc("%sdeselect%i", command, cl_selected->integer);
		MN_ExecuteConfunc("%sselect%i", command, num);
	}
	/* now set the cl_selected cvar to the new actor id */
	Cvar_ForceSet("cl_selected", va("%i", num));

	Com_DPrintf(DEBUG_CLIENT, "CL_Select_f: Command: '%s' - num: %i\n", command, num);

	assert(chr);
	/* set info cvars */
	if (chr->emplType == EMPL_ROBOT)
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
 * @brief implements the reselect command
 */
static void CL_ThisSoldier_f (void)
{
	if (CL_OnBattlescape()) {
		CL_ActorSelectList(cl_selected->integer);
	}
}

/**
 * @brief Updates data about teams in aircraft.
 * @param[in] aircraft Pointer to an aircraft for a desired team.
 * @returns the number of employees that are in the aircraft and were added to
 * the character list
 */
int CL_UpdateActorAircraftVar (aircraft_t *aircraft, employeeType_t employeeType)
{
	int i;

	assert(aircraft);

	Cvar_Set("mn_hired", va(_("%i of %i"), aircraft->teamSize, aircraft->maxTeamSize));

	/* update chrDisplayList list (this is the one that is currently displayed) */
	chrDisplayList.num = 0;
	for (i = 0; i < aircraft->maxTeamSize; i++) {
		assert(chrDisplayList.num < MAX_ACTIVETEAM);
		if (!aircraft->acTeam[i])
			continue; /* Skip unused team-slot. */

		if (aircraft->acTeam[i]->type != employeeType)
			continue;

		chrDisplayList.chr[chrDisplayList.num] = &aircraft->acTeam[i]->chr;

		/* Sanity check(s) */
		if (!chrDisplayList.chr[chrDisplayList.num])
			Sys_Error("CL_UpdateActorAircraftVar: Could not get employee character with idx: %i\n", chrDisplayList.num);
		Com_DPrintf(DEBUG_CLIENT, "add %s to chrDisplayList (pos: %i)\n", chrDisplayList.chr[chrDisplayList.num]->name, chrDisplayList.num);
		Cvar_ForceSet(va("mn_name%i", chrDisplayList.num), chrDisplayList.chr[chrDisplayList.num]->name);

		/* Update number of displayed team-members. */
		chrDisplayList.num++;
	}

	for (i = chrDisplayList.num; i < MAX_ACTIVETEAM; i++)
		chrDisplayList.chr[i] = NULL;	/* Just in case */

	return chrDisplayList.num;
}

/**
 * @brief Fill employeeList with a list of employees in the current base (i.e. they are hired and not transferred)
 * @note Depends on displayHeavyEquipmentList to be set correctly.
 * @sa E_GetEmployeeByMenuIndex - It is used to get a specific entry from the generated employeeList.
 * @note If base is NULL all employees of all bases are added to the list - especially useful for none-campaign mode games
 */
int CL_GenTeamList (const base_t *base)
{
	const employeeType_t employeeType =
		displayHeavyEquipmentList
			? EMPL_ROBOT
			: EMPL_SOLDIER;

	employeesInCurrentList = E_GetHiredEmployees(base, employeeType, &employeeList);
	return employeesInCurrentList;
}

/**
 * @brief Init the teamlist checkboxes
 * @sa CL_UpdateActorAircraftVar
 * @todo Make this function use a temporary list with all list-able employees
 * instead of using gd.employees[][] directly. See also CL_Select_f->SELECT_MODE_TEAM
 */
static void CL_MarkTeam_f (void)
{
	int j, k = 0;
	qboolean alreadyInOtherShip = qfalse;
	aircraft_t *aircraft;
	linkedList_t *emplList;

	const employeeType_t employeeType =
		displayHeavyEquipmentList
			? EMPL_ROBOT
			: EMPL_SOLDIER;

	if (GAME_IsCampaign()) {
		/* Check if we are allowed to be here.
		* We are only allowed to be here if we already set up a base. */
		if (!baseCurrent) {
			Com_Printf("No base set up\n");
			MN_PopMenu(qfalse);
			return;
		}
		if (!baseCurrent->aircraftCurrent) {
			Com_Printf("No aircraft selected\n");
			MN_PopMenu(qfalse);
			return;
		}

		aircraft = baseCurrent->aircraftCurrent;
	} else
		aircraft = cls.missionaircraft;

	CL_UpdateActorAircraftVar(aircraft, employeeType);

	/* Populate employeeList - base might be NULL for none-campaign mode games */
	CL_GenTeamList(aircraft->homebase);
	emplList = employeeList;
	while (emplList) {
		const employee_t *employee = (employee_t*)emplList->data;
		assert(employee->hired);
		assert(!employee->transfer);

		/* Search all aircraft except the current one. */
		alreadyInOtherShip = qfalse;
		for (j = 0; j < gd.numAircraft; j++) {
			if (j == aircraft->idx)
				continue;
			/* already on another aircraft */
			if (CL_SoldierInAircraft(employee, AIR_AircraftGetFromIdx(j)))
				alreadyInOtherShip = qtrue;
		}

		/* Set name of the employee. */
		Cvar_ForceSet(va("mn_ename%i", k), employee->chr.name);
		/* Change the buttons */
		MN_ExecuteConfunc("listdel %i", k);
		if (!alreadyInOtherShip && CL_SoldierInAircraft(employee, aircraft))
			MN_ExecuteConfunc("listadd %i", k);
		else if (alreadyInOtherShip)
			/* Disable the button - the soldier is already on another aircraft */
			MN_ExecuteConfunc("listdisable %i", k);

		/* Check if the employee has something equipped. */
		for (j = 0; j < csi.numIDs; j++) {
			/** @todo Wouldn't it be better here to check for temp containers */
			if (j != csi.idFloor && j != csi.idEquip && employee->inv.c[j])
				break;
		}
		if (j < csi.numIDs)
			MN_ExecuteConfunc("listholdsequip %i", k);
		else
			MN_ExecuteConfunc("listholdsnoequip %i", k);

		k++;
		if (k >= cl_numnames->integer)
			break;

		emplList = emplList->next;
	}

	for (;k < cl_numnames->integer; k++) {
		MN_ExecuteConfunc("listdisable %i", k);
		Cvar_ForceSet(va("mn_name%i", k), "");
		MN_ExecuteConfunc("listholdsnoequip %i", k);
	}
}

/**
 * @brief changes the displayed list from soldiers to heavy equipment (e.g. tanks)
 * @note console command: team_toggle_list
 */
static void CL_ToggleTeamList_f (void)
{
	if (displayHeavyEquipmentList) {
		Com_DPrintf(DEBUG_CLIENT, "Changing to soldier-list.\n");
		displayHeavyEquipmentList = qfalse;
		MN_ExecuteConfunc("toggle_show_heavybutton");
	} else {
		if (gd.numEmployees[EMPL_ROBOT] > 0) {
			Com_DPrintf(DEBUG_CLIENT, "Changing to heavy equipment (tank) list.\n");
			displayHeavyEquipmentList = qtrue;
			MN_ExecuteConfunc("toggle_show_soldiersbutton");
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
 * @param[in] employee The soldier to search for.
 * @param[in] aircraft The aircraft to search the soldier in. Use @c NULL to
 * check if the soldier is in @b any aircraft.
 * @return true if the soldier was found in the aircraft otherwise false.
 */
const aircraft_t *CL_SoldierInAircraft (const employee_t *employee, const aircraft_t* aircraft)
{
	int i;

	if (!employee)
		return NULL;

	if (employee->transfer)
		return NULL;

	/* If no aircraft is given we search if he is in _any_ aircraft and return true if that's the case. */
	if (!aircraft) {
		for (i = 0; i < gd.numAircraft; i++) {
			const aircraft_t *aircraftByIDX = AIR_AircraftGetFromIdx(i);
			if (aircraftByIDX && CL_SoldierInAircraft(employee, aircraftByIDX))
				return aircraftByIDX;
		}
		return NULL;
	}

	if (AIR_IsInAircraftTeam(aircraft, employee))
		return aircraft;
	else
		return NULL;
}

/**
 * @brief Tells you if a pilot is assigned to an aircraft.
 * @param[in] employee The pilot to search for.
 * @param[in] aircraft The aircraft to search the pilot in. Use @c NULL to
 * check if the pilot is in @b any aircraft.
 * @return true if the pilot was found in the aircraft otherwise false.
 */
qboolean CL_PilotInAircraft (const employee_t *employee, const aircraft_t* aircraft)
{
	int i;

	if (!employee)
		return qfalse;

	if (employee->transfer)
		return qfalse;

	/* If no aircraft is given we search if he is in _any_ aircraft and return true if that's the case. */
	if (!aircraft) {
		for (i = 0; i < gd.numAircraft; i++) {
			const aircraft_t *aircraftByIDX = AIR_AircraftGetFromIdx(i);
			if (aircraftByIDX && CL_PilotInAircraft(employee, aircraftByIDX))
				return qtrue;
		}
		return qfalse;
	}

	return (aircraft->pilot == employee);
}

/**
 * @brief Tells you if a employee is away from his home base (gone in mission).
 * @param[in] employee Pointer to the employee.
 * @return qboolean qtrue if the employee is away in mission, qfalse if he is not or he is unhired.
 */
qboolean CL_SoldierAwayFromBase (const employee_t *employee)
{
	int i;
	const base_t *base;

	assert(employee);

	/* Check that employee is hired */
	if (!employee->hired)
		return qfalse;

	/* Check if employee is currently transferred. */
	if (employee->transfer)
		return qtrue;

	/* for now only soldiers, ugvs and pilots can be assigned to an aircraft */
	if (employee->type != EMPL_SOLDIER && employee->type != EMPL_ROBOT
	 && employee->type != EMPL_PILOT)
		return qfalse;

	base = employee->baseHired;
	assert(base);

	for (i = 0; i < base->numAircraftInBase; i++) {
		const aircraft_t *aircraft = &base->aircraft[i];
		assert(aircraft);

		if (!AIR_IsAircraftInBase(aircraft) && AIR_IsInAircraftTeam(aircraft, employee))
			return qtrue;
	}

	return qfalse;
}

/**
 * @brief Removes a soldier from an aircraft.
 * @param[in,out] employee The soldier to be removed from the aircraft.
 * @param[in,out] aircraft The aircraft to remove the soldier from.
 * Use @c NULL to remove the soldier from any aircraft.
 * @sa CL_AssignSoldierToAircraft
 */
qboolean CL_RemoveSoldierFromAircraft (employee_t *employee, aircraft_t *aircraft)
{
	if (!employee)
		return qfalse;

	/* If no aircraft is given we search if he is in _any_ aircraft and set
	 * the aircraft pointer to it. */
	if (!aircraft) {
		int i;
		for (i = 0; i < gd.numAircraft; i++) {
			aircraft_t *acTemp = AIR_AircraftGetFromIdx(i);
			if (CL_SoldierInAircraft(employee, acTemp)) {
				aircraft = acTemp;
				break;
			}
		}
		if (!aircraft)
			return qfalse;
	}

	Com_DPrintf(DEBUG_CLIENT, "CL_RemoveSoldierFromAircraft: base: %i - aircraft->idx: %i\n",
		aircraft->homebase ? aircraft->homebase->idx : -1, aircraft->idx);

	INVSH_DestroyInventory(&employee->inv);
	return AIR_RemoveFromAircraftTeam(aircraft, employee);
}

/**
 * @brief Removes all soldiers from an aircraft.
 * @param[in,out] aircraft The aircraft to remove the soldiers from.
 * @sa CL_RemoveSoldierFromAircraft
 */
void CL_RemoveSoldiersFromAircraft (aircraft_t *aircraft)
{
	int i;

	if (!aircraft)
		return;

	/* Counting backwards because aircraft->acTeam[] is changed in CL_RemoveSoldierFromAircraft */
	for (i = aircraft->maxTeamSize; i >= 0; i--) {
		/* use global aircraft index here */
		if (CL_RemoveSoldierFromAircraft(aircraft->acTeam[i], aircraft)) {
			/* if the acTeam is not NULL the acTeam list and CL_SoldierInAircraft
			 * is out of sync */
			assert(aircraft->acTeam[i] == NULL);
		} else if (aircraft->acTeam[i]) {
			Com_Printf("CL_RemoveSoldiersFromAircraft: Error, could not remove soldier from aircraft team at pos: %i\n", i);
		}
	}

	if (aircraft->teamSize > 0)
		Sys_Error("CL_RemoveSoldiersFromAircraft: Error, there went something wrong with soldier-removing from aircraft.");
}

/**
 * @brief Assigns a soldier to an aircraft.
 * @param[in] employee The employee to be assigned to the aircraft.
 * @param[in] aircraft What aircraft to assign employee to.
 * @return returns true if a soldier could be assigned to the aircraft.
 * @sa CL_RemoveSoldierFromAircraft
 * @sa AIR_AddToAircraftTeam
 */
qboolean CL_AssignSoldierToAircraft (employee_t *employee, aircraft_t *aircraft)
{
	if (!employee || !aircraft)
		return qfalse;

	if (aircraft->teamSize < MAX_ACTIVETEAM) {
		Com_DPrintf(DEBUG_CLIENT, "CL_AssignSoldierToAircraft: attempting to find idx '%d'\n", employee->idx);

		/* Check whether the soldier is already on another aircraft */
		if (CL_SoldierInAircraft(employee, NULL)) {
			Com_DPrintf(DEBUG_CLIENT, "CL_AssignSoldierToAircraft: found idx '%d' \n", employee->idx);
			return qfalse;
		}

		/* Assign the soldier to the aircraft. */
		if (aircraft->teamSize < aircraft->maxTeamSize) {
			Com_DPrintf(DEBUG_CLIENT, "CL_AssignSoldierToAircraft: attempting to add idx '%d' \n", employee->idx);
			return AIR_AddToAircraftTeam(aircraft, employee);
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
		if (aircraft->acTeam[i]) {
			const character_t *chr = &aircraft->acTeam[i]->chr;
			Com_Printf("ucn %i - employee->idx: %i\n", chr->ucn, aircraft->acTeam[i]->idx);
		}
	}
}
#endif

/**
 * @brief Adds or removes a soldier to/from an aircraft.
 * @sa E_EmployeeHire_f
 */
void CL_AssignSoldierFromMenuToAircraft (base_t *base, const int num, aircraft_t *aircraft)
{
	employee_t *employee;

	if (GAME_IsCampaign() && !base->numAircraftInBase) {
		Com_Printf("CL_AssignSoldierFromMenuToAircraft: No aircraft in base\n");
		return;
	}

	Com_DPrintf(DEBUG_CLIENT, "CL_AssignSoldierFromMenuToAircraft: Trying to get employee with hired-idx %i.\n", num);

	/* If this fails it's very likely that employeeList is not filled. */
	employee = E_GetEmployeeByMenuIndex(num);
	if (!employee)
		Sys_Error("CL_AssignSoldierFromMenuToAircraft: Could not get employee %i\n", num);

	Com_DPrintf(DEBUG_CLIENT, "CL_AssignSoldierFromMenuToAircraft: employee with idx %i selected\n", employee->idx);

	assert(aircraft);

	if (CL_SoldierInAircraft(employee, aircraft)) {
		/* Remove soldier from aircraft/team. */
		MN_ExecuteConfunc("listdel %i\n", num);
		/* use the global aircraft index here */
		CL_RemoveSoldierFromAircraft(employee, aircraft);
		MN_ExecuteConfunc("listholdsnoequip %i\n", num);
	} else {
		/* Assign soldier to aircraft/team if aircraft is not full */
		if (CL_AssignSoldierToAircraft(employee, aircraft))
			MN_ExecuteConfunc("listadd %i\n", num);
		else
			MN_ExecuteConfunc("listdel %i\n", num);
	}
	/* Select the desired one anyways. */
	CL_UpdateActorAircraftVar(aircraft, employee->type);
	Cbuf_AddText(va("team_select %i\n", num));
}

/**
 * @brief Adds or removes a soldier to/from an aircraft.
 * @sa E_EmployeeHire_f
 */
static void CL_AssignSoldier_f (void)
{
	base_t *base = baseCurrent;
	aircraft_t *aircraft;
	int num;
	const employeeType_t employeeType =
		displayHeavyEquipmentList
			? EMPL_ROBOT
			: EMPL_SOLDIER;

	/* check syntax */
	if (Cmd_Argc() < 2) {
		Com_Printf("Usage: %s <num>\n", Cmd_Argv(0));
		return;
	}
	num = atoi(Cmd_Argv(1));

	/* In case we didn't populate the list with CL_GenTeamList before. */
	if (!employeeList)
		return;

	/* baseCurrent is checked here */
	if (num >= E_CountHired(base, employeeType) || num >= cl_numnames->integer) {
		/*Com_Printf("num: %i, max: %i\n", num, E_CountHired(baseCurrent, employeeType));*/
		return;
	}

	aircraft = CL_GetTeamAircraft(base);
	if (!aircraft)
		return;

	CL_AssignSoldierFromMenuToAircraft(base, num, aircraft);
}

void TEAM_InitStartup (void)
{
	Cmd_AddCommand("givename", CL_GiveName_f, "Give the team members names from the team_*.ufo files");
	Cmd_AddCommand("genequip", CL_GenerateEquipment_f, NULL);
	Cmd_AddCommand("equip_type", CL_EquipType_f, NULL);
	Cmd_AddCommand("team_mark", CL_MarkTeam_f, NULL);
	Cmd_AddCommand("team_hire", CL_AssignSoldier_f, "Add/remove already hired actor to the aircraft");
	Cmd_AddCommand("team_select", CL_Select_f, NULL);
	Cmd_AddCommand("team_changename", CL_ChangeName_f, "Change the name of an actor");
	Cmd_AddCommand("team_changeskin", CL_ChangeSkin_f, "Change the skin of the soldier");
	Cmd_AddCommand("team_changeskinteam", CL_ChangeSkinOnBoard_f, "Change the skin for the hole team in the current aircraft");
	Cmd_AddCommand("equip_select", CL_Select_f, NULL);
	Cmd_AddCommand("soldier_select", CL_Select_f, _("Select a soldier from list"));
	Cmd_AddCommand("soldier_reselect", CL_ThisSoldier_f, _("Reselect the current soldier"));
	Cmd_AddCommand("nextsoldier", CL_NextSoldier_f, _("Toggle to next soldier"));
	Cmd_AddCommand("team_toggle_list", CL_ToggleTeamList_f, "Changes between assignment-list for soldiers and heavy equipment (e.g. Tanks)");
#ifdef DEBUG
	Cmd_AddCommand("teamlist", CL_TeamListDebug_f, "Debug function to show all hired and assigned teammembers");
#endif
}

/**
 * @brief Stores a team-list (chr-list) info to buffer (which might be a network buffer, too).
 * @sa G_ClientTeamInfo
 * @sa CL_SaveTeamMultiplayerInfo
 * @note Called in CL_Precache_f to send the team info to server
 */
void CL_SendCurTeamInfo (struct dbuffer * buf, chrList_t *team, base_t *base)
{
	int i, j;

	/* clean temp inventory */
	CL_CleanTempInventory(base);

	/* header */
	NET_WriteByte(buf, clc_teaminfo);
	NET_WriteByte(buf, team->num);

	Com_DPrintf(DEBUG_CLIENT, "CL_SendCurTeamInfo: Upload information about %i soldiers to server\n", team->num);
	for (i = 0; i < team->num; i++) {
		character_t *chr = team->chr[i];
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
		NET_WriteByte(buf, chr->teamDef ? chr->teamDef->idx : NONE);
		NET_WriteByte(buf, chr->gender);
		NET_WriteByte(buf, chr->STUN);
		NET_WriteByte(buf, chr->morale);

		/** Scores @sa inv_shared.h:chrScoreGlobal_t */
		for (j = 0; j < SKILL_NUM_TYPES + 1; j++)
			NET_WriteLong(buf, chr->score.experience[j]);
		for (j = 0; j < SKILL_NUM_TYPES; j++)
			NET_WriteByte(buf, chr->score.skills[j]);
		for (j = 0; j < SKILL_NUM_TYPES + 1; j++)
			NET_WriteByte(buf, chr->score.initialSkills[j]);
		for (j = 0; j < KILLED_NUM_TYPES; j++)
			NET_WriteShort(buf, chr->score.kills[j]);
		for (j = 0; j < KILLED_NUM_TYPES; j++)
			NET_WriteShort(buf, chr->score.stuns[j]);
		NET_WriteShort(buf, chr->score.assignedMissions);
		NET_WriteByte(buf, chr->score.rank);

		/* Send user-defined (default) reaction-state. */
		NET_WriteShort(buf, chr->reservedTus.reserveReaction);

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

	if (!msg) {
		for (i = 0; i < num; i++) {
			employee_t *employee = E_GetEmployeeFromChrUCN(updateCharacterArray[i].ucn);
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
 * @sa EV_RESULTS
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

	if (!GAME_CP_IsRunning() || !selectedMission) {
		/* the mission was started via console
		 * buffer - needs to be cleared and then append to it */
		/** @todo or is multiplayer? */
		if (GAME_IsCampaign() && GAME_CP_IsRunning()) {
			Com_sprintf(resultText, sizeof(resultText), _("Aliens killed\t%i\n"), thier_killed);
			ccs.aliensKilled += thier_killed;
			Q_strcat(resultText, va(_("Aliens captured\t%i\n\n"), thier_stunned), sizeof(resultText));
			Q_strcat(resultText, va(_("Alien survivors\t%i\n\n"), thier_surviviurs), sizeof(resultText));
		} else {
			Com_sprintf(resultText, sizeof(resultText), _("Enemies killed\t%i\n"), thier_killed + civilian_killed);
			ccs.aliensKilled += thier_killed + civilian_killed;
			Q_strcat(resultText, va(_("Enemies captured\t%i\n\n"), thier_stunned), sizeof(resultText));
			Q_strcat(resultText, va(_("Enemy survivors\t%i\n\n"), thier_surviviurs), sizeof(resultText));
		}

		/* team stats */
		Q_strcat(resultText, va(_("Team losses\t%i\n"), our_killed), sizeof(resultText));
		Q_strcat(resultText, va(_("Team missing in action\t%i\n"), our_stunned), sizeof(resultText));
		Q_strcat(resultText, va(_("Friendly fire losses\t%i\n"), num_kills[we][we]), sizeof(resultText));
		Q_strcat(resultText, va(_("Team survivors\t%i\n\n"), our_surviviurs), sizeof(resultText));

		if (GAME_IsCampaign())
			Q_strcat(resultText, va(_("Civilians killed by the Aliens\t%i\n"), civilian_killed - num_kills[we][TEAM_CIVILIAN]), sizeof(resultText));
		else
			Q_strcat(resultText, va(_("Civilians killed by the Enemies\t%i\n"), civilian_killed - civilian_stunned - num_kills[we][TEAM_CIVILIAN]), sizeof(resultText));

		Q_strcat(resultText, va(_("Civilians killed by your Team\t%i\n"), num_kills[we][TEAM_CIVILIAN]), sizeof(resultText));
		Q_strcat(resultText, va(_("Civilians saved\t%i\n\n\n"), civilian_surviviurs), sizeof(resultText));

		MN_PopMenu(qtrue);
		Cvar_Set("mn_main", "main");
		Cvar_Set("mn_active", "");
		MN_PushMenu("main", NULL);
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
		MN_PushMenu("map", NULL);
	}
	/* show win screen */
	if (GAME_IsCampaign()) {
		/* Make sure that at this point we are able to 'Try Again' a mission. */
		Cvar_SetValue("mission_tryagain", 0);
		if (selectedMission && base)
			CP_ExecuteMissionTrigger(selectedMission, winner == we);
		else if (GAME_CP_IsRunning())
			Com_Printf("CL_ParseResults: Error - no mission triggers, because selectedMission or base are not valid\n");

		if (winner == we) {
			/* We need to update Menu Won with UFO recovery stuff. */
			if (missionresults.recovery) {
				if (missionresults.crashsite)
					Q_strcat(resultText, va(_("\nSecured crashed %s\n"), UFO_TypeToName(missionresults.ufotype)), sizeof(resultText));
				else
					Q_strcat(resultText, va(_("\nSecured landed %s\n"), UFO_TypeToName(missionresults.ufotype)), sizeof(resultText));
			}
			MN_PushMenu("won", NULL);
		} else
			MN_PushMenu("lost", NULL);

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
static int CL_GetRankIdx (const char* rankID)
{
	int i;

	/* only check in singleplayer */
	if (!GAME_IsCampaign())
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
	{"factor", V_FLOAT, offsetof(rank_t, factor), MEMBER_SIZEOF(rank_t, factor)},
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
		memset(rank, 0, sizeof(*rank));
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
						Com_EParseValue(rank, token, v->type, v->ofs, v->size);
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
	memset(ugv, 0, sizeof(*ugv));
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
				Com_EParseValue(ugv, token, v->type, v->ofs, v->size);
				break;
			}
			if (!v->string)
				Com_Printf("Com_ParseUGVs: unknown token \"%s\" ignored (ugv %s)\n", token, name);
	} while (*text);
}
