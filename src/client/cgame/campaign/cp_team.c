/**
 * @file cp_team.c
 * @brief Team management for the campaign gametype
 */

/*
Copyright (C) 2002-2012 UFO: Alien Invasion.

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

#include "../../cl_shared.h"
#include "../../cl_team.h"
#include "cp_campaign.h"
#include "cp_team.h"

/**
 * @brief Updates status of weapon (sets pointers, reloads, etc).
 * @param[in] ed Pointer to equipment definition.
 * @param[in] item An item to update.
 * @return Updated item in any case, even if there was no update.
 * @sa CP_CleanupAircraftCrew
 * @todo remove return value and make item a pointer
 */
static item_t CP_AddWeaponAmmo (equipDef_t * ed, item_t item)
{
	int i;
	const objDef_t *type = item.t;

	assert(ed->numItems[type->idx] > 0);
	ed->numItems[type->idx]--;

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
			if (ed->numItems[item.m->idx] > 0) {
				ed->numItems[item.m->idx]--;
				return item;
			} else {
				/* Your clip has been sold; give it back. */
				item.a = NONE_AMMO;
				return item;
			}
		}
	}

	/* Check for complete clips of the same kind */
	if (item.m && ed->numItems[item.m->idx] > 0) {
		ed->numItems[item.m->idx]--;
		item.a = type->ammo;
		return item;
	}

	/* Search for any complete clips. */
	/** @todo We may want to change this to use the type->ammo[] info. */
	for (i = 0; i < csi.numODs; i++) {
		const objDef_t *od = INVSH_GetItemByIDX(i);
		if (INVSH_LoadableInWeapon(od, type)) {
			if (ed->numItems[i] > 0) {
				ed->numItems[i]--;
				item.a = type->ammo;
				item.m = od;
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
	if (item.m && ed->numItemsLoose[item.m->idx] > 0) {
		item.a = ed->numItemsLoose[item.m->idx];
		ed->numItemsLoose[item.m->idx] = 0;
		return item;
	}

	/* See if there's any loose ammo */
	/** @todo We may want to change this to use the type->ammo[] info. */
	item.a = NONE_AMMO;
	for (i = 0; i < csi.numODs; i++) {
		const objDef_t *od = INVSH_GetItemByIDX(i);
		if (INVSH_LoadableInWeapon(od, type) && ed->numItemsLoose[i] > item.a) {
			if (item.a > 0) {
				/* We previously found some ammo, but we've now found other
				 * loose ammo of a different (but appropriate) type with
				 * more bullets.  Put the previously found ammo back, so
				 * we'll take the new type. */
				assert(item.m);
				ed->numItemsLoose[item.m->idx] = item.a;
				/* We don't have to accumulate loose ammo into clips
				 * because we did it previously and we create no new ammo */
			}
			/* Found some loose ammo to load the weapon with */
			item.a = ed->numItemsLoose[i];
			ed->numItemsLoose[i] = 0;
			item.m = od;
		}
	}
	return item;
}

/**
 * @brief Reloads weapons, removes not assigned and resets defaults
 * @param[in] aircraft Pointer to an aircraft for given team.
 * @param[in] ed equipDef_t pointer to equipment
 * @sa CL_AddWeaponAmmo
 * @note Iterate through in container order (right hand, left hand, belt,
 * holster, backpack) at the top level, i.e. each squad member reloads
 * the right hand, then each reloads the left hand, etc. The effect
 * of this is that when things are tight, everyone has the opportunity
 * to get their preferred weapon(s) loaded before anyone is allowed
 * to keep her spares in the backpack or on the floor. We don't want
 * the first person in the squad filling their backpack with spare ammo
 * leaving others with unloaded guns in their hands...
 */
void CP_CleanupAircraftCrew (aircraft_t *aircraft, equipDef_t * ed)
{
	containerIndex_t container;

	assert(aircraft);

	for (container = 0; container < csi.numIDs; container++) {
		LIST_Foreach(aircraft->acTeam, employee_t, employee) {
			invList_t *ic, *next;
			character_t *chr = &employee->chr;
#if 0
			/* ignore items linked from any temp container */
			if (INVDEF(container)->temp)
				continue;
#endif
			for (ic = CONTAINER(chr, container); ic; ic = next) {
				next = ic->next;
				if (ed->numItems[ic->item.t->idx] > 0) {
					ic->item = CP_AddWeaponAmmo(ed, ic->item);
				} else {
					/* Drop ammo used for reloading and sold carried weapons. */
					if (!cgi->INV_RemoveFromInventory(&chr->i, INVDEF(container), ic))
						Com_Error(ERR_DROP, "Could not remove item from inventory");
				}
			}
		}
	}
}

/**
 * @brief Clears all containers that are temp containers (see script definition).
 * @sa GAME_SaveTeamInfo
 * @sa GAME_SendCurrentTeamSpawningInfo
 */
void CP_CleanTempInventory (base_t* base)
{
	E_Foreach(EMPL_SOLDIER, employee) {
		int k;
		for (k = 0; k < csi.numIDs; k++) {
			/* idFloor and idEquip are temp */
			if (INVDEF(k)->temp)
				employee->chr.i.c[k] = NULL;
		}
	}

	if (!base)
		return;

	cgi->INV_DestroyInventory(&base->bEquipment);
}

/**
 * @brief Updates data about teams in aircraft.
 * @param[in] aircraft Pointer to an aircraft for a desired team.
 * @param[in] employeeType Type of employee for which data is being updated.
 * @returns the number of employees that are in the aircraft and were added to
 * the character list
 */
int CP_UpdateActorAircraftVar (aircraft_t *aircraft, employeeType_t employeeType)
{
	int i;
	size_t size;
	int numOnAircraft;
	const employee_t *pilot = AIR_GetPilot(aircraft);

	assert(aircraft);

	numOnAircraft = AIR_GetTeamSize(aircraft);
	Cvar_Set("mn_hired", va(_("%i of %i"), numOnAircraft, aircraft->maxTeamSize));
	Cvar_Set("mn_hirable_count", va("%i", aircraft->maxTeamSize - numOnAircraft));
	Cvar_Set("mn_hired_count", va("%i", numOnAircraft));

	if (pilot) {
		Cvar_Set("mn_pilotassigned", "1");
		Cvar_Set("mn_pilot_name", pilot->chr.name);
		Cvar_Set("mn_pilot_body", CHRSH_CharGetBody(&pilot->chr));
		Cvar_Set("mn_pilot_head", CHRSH_CharGetHead(&pilot->chr));
		Cvar_Set("mn_pilot_skin", va("%i", pilot->chr.skin));
	} else {
		Cvar_Set("mn_pilotassigned", "0");
		Cvar_Set("mn_pilot_name", "");
		Cvar_Set("mn_pilot_body", "");
		Cvar_Set("mn_pilot_head", "");
		Cvar_Set("mn_pilot_skin", "");
	}

	size = lengthof(chrDisplayList.chr);

	/* update chrDisplayList list (this is the one that is currently displayed) */
	chrDisplayList.num = 0;
	LIST_Foreach(aircraft->acTeam, employee_t, empl) {
		if (empl->type != employeeType)
			continue;

		assert(chrDisplayList.num < size);
		chrDisplayList.chr[chrDisplayList.num] = &empl->chr;

		/* Sanity check(s) */
		if (!chrDisplayList.chr[chrDisplayList.num])
			Com_Error(ERR_DROP, "CP_UpdateActorAircraftVar: Could not get employee character with idx: %i", chrDisplayList.num);
		Com_DPrintf(DEBUG_CLIENT, "add %s to chrDisplayList (pos: %i)\n", chrDisplayList.chr[chrDisplayList.num]->name, chrDisplayList.num);
		Cvar_Set(va("mn_name%i", chrDisplayList.num), chrDisplayList.chr[chrDisplayList.num]->name);

		/* Update number of displayed team-members. */
		chrDisplayList.num++;
	}

	for (i = chrDisplayList.num; i < size; i++)
		chrDisplayList.chr[i] = NULL;	/* Just in case */

	return chrDisplayList.num;
}
