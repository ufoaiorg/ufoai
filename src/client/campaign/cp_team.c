/**
 * @file cp_team.c
 * @brief Team management for the campaign gametype
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
#include "../cl_team.h"
#include "../cl_global.h"
#include "cp_team.h"

/**
 * @brief Reloads weapons and removes "not assigned" ones from containers.
 * @param[in] aircraft Pointer to an aircraft for given team.
 * @param[in] ed equipDef_t pointer to equipment of given character in a team.
 * @sa CL_AddWeaponAmmo
 * @todo campaign mode only function
 */
void CL_ReloadAndRemoveCarried (aircraft_t *aircraft, equipDef_t * ed)
{
	invList_t *ic, *next;
	int p, container;

	/* Iterate through in container order (right hand, left hand, belt,
	 * holster, backpack) at the top level, i.e. each squad member reloads
	 * her right hand, then each reloads his left hand, etc. The effect
	 * of this is that when things are tight, everyone has the opportunity
	 * to get their preferred weapon(s) loaded before anyone is allowed
	 * to keep her spares in the backpack or on the floor. We don't want
	 * the first person in the squad filling their backpack with spare ammo
	 * leaving others with unloaded guns in their hands... */

	assert(aircraft);

	Com_DPrintf(DEBUG_CLIENT, "CL_ReloadAndRemoveCarried:aircraft idx: %i, team size: %i\n",
		aircraft->idx, aircraft->teamSize);

	/* Auto-assign weapons to UGVs/Robots if they have no weapon yet. */
	for (p = 0; p < aircraft->maxTeamSize; p++) {
		if (aircraft->acTeam[p] && aircraft->acTeam[p]->ugv) {
			/* This is an UGV */
			character_t *chr = &aircraft->acTeam[p]->chr;
			assert(chr);

			/* Check if there is a weapon and add it if there isn't. */
			if (!chr->inv.c[csi.idRight] || !chr->inv.c[csi.idRight]->item.t)
				INVSH_EquipActorRobot(&chr->inv, chr, INVSH_GetItemByID(aircraft->acTeam[p]->ugv->weapon));
		}
	}

	for (container = 0; container < csi.numIDs; container++) {
		for (p = 0; p < aircraft->maxTeamSize; p++) {
			if (aircraft->acTeam[p]) {
				character_t *chr = &aircraft->acTeam[p]->chr;
				assert(chr);
				for (ic = chr->inv.c[container]; ic; ic = next) {
					next = ic->next;
					if (ed->num[ic->item.t->idx] > 0) {
						ic->item = CL_AddWeaponAmmo(ed, ic->item);
					} else {
						/* Drop ammo used for reloading and sold carried weapons. */
						Com_RemoveFromInventory(&chr->inv, &csi.ids[container], ic);
					}
				}
			}
		}
	}
}

/**
 * @brief Clears all containers that are temp containers (see script definition).
 * @sa CL_UpdateEquipmentMenuParameters_f
 * @sa MP_SaveTeamMultiplayerInfo
 * @sa GAME_SendCurrentTeamSpawningInfo
 * @todo campaign mode only function
 */
void CL_CleanTempInventory (base_t* base)
{
	int i, k;

	for (i = 0; i < MAX_EMPLOYEES; i++)
		for (k = 0; k < csi.numIDs; k++)
			if (csi.ids[k].temp) {
				/* idFloor and idEquip are temp */
				gd.employees[EMPL_SOLDIER][i].chr.inv.c[k] = NULL;
				gd.employees[EMPL_ROBOT][i].chr.inv.c[k] = NULL;
			}

	if (!base)
		return;

	INVSH_DestroyInventory(&base->bEquipment);
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
