/**
 * @file cp_team.c
 * @brief Team management for the campaign gametype
 */

/*
Copyright (C) 2002-2009 UFO: Alien Invasion.

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
#include "../menu/m_main.h"
#include "cp_campaign.h"
#include "cp_team.h"

/**
 * @brief Reloads weapons, removes not assigned and resets defaults
 * @param[in] aircraft Pointer to an aircraft for given team.
 * @param[in] ed equipDef_t pointer to equipment
 * @sa CL_AddWeaponAmmo
 */
void CL_CleanupAircraftCrew (aircraft_t *aircraft, equipDef_t * ed)
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

	Com_DPrintf(DEBUG_CLIENT, "CL_CleanupAircraftCrew:aircraft idx: %i, team size: %i\n",
		aircraft->idx, aircraft->teamSize);

	/* Auto-assign weapons to UGVs/Robots if they have no weapon yet. */
	for (p = 0; p < aircraft->maxTeamSize; p++) {
		if (aircraft->acTeam[p]) {
			character_t *chr = &aircraft->acTeam[p]->chr;
			assert(chr);

			if (aircraft->acTeam[p]->ugv) {
				/* This is an UGV */

				/* Check if there is a weapon and add it if there isn't. */
				if (!chr->inv.c[csi.idRight] || !chr->inv.c[csi.idRight]->item.t)
					INVSH_EquipActorRobot(&chr->inv, chr, INVSH_GetItemByID(aircraft->acTeam[p]->ugv->weapon));
			}

			/* reset reserveReaction state to default */
			chr->reservedTus.reserveReaction = STATE_REACTION_ONCE;
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
 */
void CL_CleanTempInventory (base_t* base)
{
	int i, k;

	for (i = 0; i < MAX_EMPLOYEES; i++)
		for (k = 0; k < csi.numIDs; k++)
			if (csi.ids[k].temp) {
				/* idFloor and idEquip are temp */
				ccs.employees[EMPL_SOLDIER][i].chr.inv.c[k] = NULL;
				ccs.employees[EMPL_ROBOT][i].chr.inv.c[k] = NULL;
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
	Cvar_Set("mn_hirable_count", va("%i", aircraft->maxTeamSize - aircraft->teamSize));
	Cvar_Set("mn_hired_count", va("%i", aircraft->teamSize));
	Cvar_Set("mn_pilotassigned", va("%i", aircraft->pilot != NULL));

	if (aircraft->pilot) {
		Cvar_ForceSet("mn_pilot_name", aircraft->pilot->chr.name);
		Cvar_ForceSet("mn_pilot_body", CHRSH_CharGetBody(&aircraft->pilot->chr));
		Cvar_ForceSet("mn_pilot_head", CHRSH_CharGetHead(&aircraft->pilot->chr));
		Cvar_ForceSet("mn_pilot_skin", va("%i", aircraft->pilot->chr.skin));
	} else {
		Cvar_ForceSet("mn_pilot_name", "");
		Cvar_ForceSet("mn_pilot_body", "");
		Cvar_ForceSet("mn_pilot_head", "");
		Cvar_ForceSet("mn_pilot_skin", "");
	}

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
			Com_Error(ERR_DROP, "CL_UpdateActorAircraftVar: Could not get employee character with idx: %i", chrDisplayList.num);
		Com_DPrintf(DEBUG_CLIENT, "add %s to chrDisplayList (pos: %i)\n", chrDisplayList.chr[chrDisplayList.num]->name, chrDisplayList.num);
		Cvar_ForceSet(va("mn_name%i", chrDisplayList.num), chrDisplayList.chr[chrDisplayList.num]->name);

		/* Update number of displayed team-members. */
		chrDisplayList.num++;
	}

	for (i = chrDisplayList.num; i < MAX_ACTIVETEAM; i++)
		chrDisplayList.chr[i] = NULL;	/* Just in case */

	return chrDisplayList.num;
}
