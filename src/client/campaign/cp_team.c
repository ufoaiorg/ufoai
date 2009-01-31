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

