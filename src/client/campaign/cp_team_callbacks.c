/**
 * @file cp_team_callbacks.c
 * @brief Menu related callback functions for the team menu
 */

/*
All original materal Copyright (C) 2002-2007 UFO: Alien Invasion team.

Original file from Quake 2 v3.21: quake2-2.31/client/cl_main.c
Copyright (C) 1997-2001 Id Software, Inc.

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

#include "../client.h"
#include "../cl_global.h"
#include "../cl_actor.h"
#include "../cl_team.h"
#include "../cl_ugv.h"
#include "../menu/m_nodes.h"	/**< menuInventory */

#include "cp_team.h"
#include "cp_team_callbacks.h"

linkedList_t *employeeList;	/* @sa E_GetEmployeeByMenuIndex */
int employeesInCurrentList;

/**
 * @brief Returns the aircraft for the team and soldier selection menus
 * @note Multiplayer and skirmish are using @c cls.missionaircraft, campaign mode is
 * using the current selected aircraft in base
 */
static inline aircraft_t *CL_GetTeamAircraft (base_t *base)
{
	if (!base)
		Sys_Error("CL_GetTeamAircraft: Called without base");
	return base->aircraftCurrent;
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

/***********************************************************
 * Bindings
 ***********************************************************/

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
		cls.displayHeavyEquipmentList
			? EMPL_ROBOT
			: EMPL_SOLDIER;

	/* Check if we are allowed to be here.
	 * We are only allowed to be here if we already set up a base. */
	if (!baseCurrent) {
		Com_Printf("No base set up\n");
		MN_PopMenu(qfalse);
		return;
	}

	aircraft = CL_GetTeamAircraft(baseCurrent);
	if (!aircraft) {
		MN_PopMenu(qfalse);
		return;
	}

	CL_UpdateActorAircraftVar(aircraft, employeeType);

	/* Populate employeeList - base might be NULL for none-campaign mode games */
	E_GenerateHiredEmployeesList(aircraft->homebase);
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
			if (AIR_IsEmployeeInAircraft(employee, AIR_AircraftGetFromIDX(j)))
				alreadyInOtherShip = qtrue;
		}

		/* Set name of the employee. */
		Cvar_ForceSet(va("mn_ename%i", k), employee->chr.name);
		/* Change the buttons */
		MN_ExecuteConfunc("listdel %i", k);
		if (!alreadyInOtherShip && AIR_IsEmployeeInAircraft(employee, aircraft))
			MN_ExecuteConfunc("listadd %i", k);
		else if (alreadyInOtherShip)
			/* Disable the button - the soldier is already on another aircraft */
			MN_ExecuteConfunc("listdisable %i", k);

		/* Check if the employee has something equipped. */
		for (j = 0; j < csi.numIDs; j++) {
			/** @todo Wouldn't it be better here to check for temp containers */
			if (j != csi.idFloor && j != csi.idEquip && employee->chr.inv.c[j])
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
 * @brief Displays actor equipment and unused items in proper (filter) category.
 * @note This function is called everytime the equipment screen for the team pops up.
 * @todo Do we allow EMPL_ROBOTs to be equipable? Or is simple buying of ammo enough (similar to original UFO/XCOM)?
 * In the first case the EMPL_SOLDIER stuff below needs to be changed.
 */
static void CL_UpdateEquipmentMenuParameters_f (void)
{
	equipDef_t unused;
	int i, p;
	aircraft_t *aircraft;
	int team;

	if (!baseCurrent)
		return;

	aircraft = CL_GetTeamAircraft(baseCurrent);
	if (!aircraft)
		return;

	/* no soldiers are assigned to the current aircraft. */
	if (!aircraft->teamSize) {
		MN_PopMenu(qfalse);
		return;
	}

	/* Get team. */
	team = cl_team->integer;

	Cvar_ForceSet("cl_selected", "0");

	/** @todo Skip EMPL_ROBOT (i.e. ugvs) for now . */
	p = CL_UpdateActorAircraftVar(aircraft, EMPL_SOLDIER);
	if (p > 0)
		menuInventory = &chrDisplayList.chr[0]->inv;
	else
		menuInventory = NULL;

	for (; p < MAX_ACTIVETEAM; p++) {
		Cvar_ForceSet(va("mn_name%i", p), "");
		MN_ExecuteConfunc("equipdisable %i", p);
	}

	selActor = NULL;

	/* reset description */
	Cvar_Set("mn_itemname", "");
	Cvar_Set("mn_item", "");
	MN_MenuTextReset(TEXT_STANDARD);

	/* manage inventory */
	unused = aircraft->homebase->storage; /* copied, including arrays inside! */

	CL_CleanTempInventory(aircraft->homebase);
	CL_ReloadAndRemoveCarried(aircraft, &unused);

	/* a 'tiny hack' to add the remaining equipment (not carried)
	 * correctly into buy categories, reloading at the same time;
	 * it is valid only due to the following property: */
	assert(MAX_CONTAINERS >= FILTER_AIRCRAFT);

	for (i = 0; i < csi.numODs; i++) {
		/* Don't allow to show armour for other teams in the menu. */
		if (!Q_strcmp(csi.ods[i].type, "armour") && csi.ods[i].useable != team)
			continue;

		/* Don't allow to show unresearched items. */
		if (!RS_IsResearched_ptr(csi.ods[i].tech))
			continue;

		while (unused.num[i]) {
			const item_t item = {NONE_AMMO, NULL, &csi.ods[i], 0, 0};
			inventory_t *i = &aircraft->homebase->bEquipment;
			if (!Com_AddToInventory(i, CL_AddWeaponAmmo(&unused, item), &csi.ids[csi.idEquip], NONE, NONE, 1))
				break; /* no space left in menu */
		}
	}

	/* First-time linking of menuInventory. */
	if (!menuInventory->c[csi.idEquip]) {
		menuInventory->c[csi.idEquip] = baseCurrent->bEquipment.c[csi.idEquip];
	}
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
		cls.displayHeavyEquipmentList
			? EMPL_ROBOT
			: EMPL_SOLDIER;

	/* check syntax */
	if (Cmd_Argc() < 2) {
		Com_Printf("Usage: %s <num>\n", Cmd_Argv(0));
		return;
	}
	num = atoi(Cmd_Argv(1));

	/* In case we didn't populate the list with E_GenerateHiredEmployeesList before. */
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

	AIM_AddEmployeeFromMenu(aircraft, num);
}

static void CL_ActorTeamSelect_f (void)
{
	employee_t *employee;
	character_t *chr;
	int num;
	const employeeType_t employeeType = cls.displayHeavyEquipmentList
			? EMPL_ROBOT : EMPL_SOLDIER;

	if (!baseCurrent)
		return;

	/* check syntax */
	if (Cmd_Argc() < 2) {
		Com_Printf("Usage: %s <num>\n", Cmd_Argv(0));
		return;
	}

	num = atoi(Cmd_Argv(1));
	if (num >= E_CountHired(baseCurrent, employeeType))
		return;

	employee = E_GetEmployeeByMenuIndex(num);
	if (!employee)
		Sys_Error("CL_ActorTeamSelect_f: No employee at list-pos %i (base: %i)\n", num, baseCurrent->idx);

	chr = &employee->chr;
	if (!chr)
		Sys_Error("CL_ActorTeamSelect_f: No hired character at list-pos %i (base: %i)\n", num, baseCurrent->idx);

	/* deselect current selected soldier and select the new one */
	MN_ExecuteConfunc("teamdeselect%i", cl_selected->integer);
	MN_ExecuteConfunc("teamselect%i", num);

	/* now set the cl_selected cvar to the new actor id */
	Cvar_ForceSet("cl_selected", va("%i", num));

	/* set info cvars */
	if (chr->emplType == EMPL_ROBOT)
		CL_UGVCvars(chr);
	else
		CL_CharacterCvars(chr);
}

void CP_TEAM_InitCallbacks (void)
{
	Cmd_AddCommand("genequip", CL_UpdateEquipmentMenuParameters_f, NULL);
	Cmd_AddCommand("team_mark", CL_MarkTeam_f, NULL);
	Cmd_AddCommand("team_hire", CL_AssignSoldier_f, "Add/remove already hired actor to the aircraft");
	Cmd_AddCommand("team_select", CL_ActorTeamSelect_f, "Select a soldier in the team creation menu");
}

void CP_TEAM_ShutdownCallbacks (void)
{
	Cmd_RemoveCommand("genequip");
	Cmd_RemoveCommand("team_mark");
	Cmd_RemoveCommand("team_hire");
	Cmd_RemoveCommand("team_select");
}
