/**
 * @file cp_capacity.c
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

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
*/

#include "../../cl_shared.h"
#include "cp_campaign.h"
#include "cp_capacity.h"
#include "cp_aircraft.h"
#include "cp_missions.h"
#include "cp_map.h"
#include "cp_popup.h"
#include "cp_ufo.h"

/**
 * @brief Actions to perform when destroying one hangar.
 * @param[in] base Pointer to the base where hangar is destroyed.
 * @param[in] capacity Type of hangar capacity: CAP_AIRCRAFT_SMALL or CAP_AIRCRAFT_BIG
 * @note called when player destroy its building or hangar is destroyed during base attack.
 * @note These actions will be performed after we actually remove the building.
 * @pre we checked before calling this function that all parameters are valid.
 * @pre building is not under construction.
 * @sa B_BuildingDestroy_f
 * @todo If player choose to destroy the building, a popup should ask him if he wants to sell aircraft in it.
 */
void CAP_RemoveAircraftExceedingCapacity (base_t* base, baseCapacities_t capacity)
{
	linkedList_t *awayAircraft = NULL;
	int numAwayAircraft;
	int randomNum;

	/* destroy aircraft only if there's not enough hangar (hangar is already destroyed) */
	if (CAP_GetFreeCapacity(base, capacity) >= 0)
		return;

	/* destroy one aircraft (must not be sold: may be destroyed by aliens) */
	AIR_ForeachFromBase(aircraft, base) {
		const int aircraftSize = aircraft->size;

		switch (aircraftSize) {
		case AIRCRAFT_SMALL:
			if (capacity != CAP_AIRCRAFT_SMALL)
				continue;
			break;
		case AIRCRAFT_LARGE:
			if (capacity != CAP_AIRCRAFT_BIG)
				continue;
			break;
		default:
			Com_Error(ERR_DROP, "B_RemoveAircraftExceedingCapacity: Unknown type of aircraft '%i'", aircraftSize);
		}

		/* Only aircraft in hangar will be destroyed by hangar destruction */
		if (!AIR_IsAircraftInBase(aircraft)) {
			if (AIR_IsAircraftOnGeoscape(aircraft))
				LIST_AddPointer(&awayAircraft, (void*)aircraft);
			continue;
		}

		/* Remove aircraft and aircraft items, but do not fire employees */
		AIR_DeleteAircraft(aircraft);
		LIST_Delete(&awayAircraft);
		return;
	}
	numAwayAircraft = LIST_Count(awayAircraft);

	if (!numAwayAircraft)
		return;
	/* All aircraft are away from base, pick up one and change it's homebase */
	randomNum = rand() % numAwayAircraft;
	if (!CL_DisplayHomebasePopup((aircraft_t*)LIST_GetByIdx(awayAircraft, randomNum), qfalse)) {
		aircraft_t *aircraft = (aircraft_t*)LIST_GetByIdx(awayAircraft, randomNum);
		/* No base can hold this aircraft */
		UFO_NotifyPhalanxAircraftRemoved(aircraft);
		if (!MapIsWater(MAP_GetColor(aircraft->pos, MAPTYPE_TERRAIN, NULL)))
			CP_SpawnRescueMission(aircraft, NULL);
		else {
			/* Destroy the aircraft and everything onboard - the aircraft pointer
			 * is no longer valid after this point */
			AIR_DestroyAircraft(aircraft);
		}
	}
	LIST_Delete(&awayAircraft);
}

/**
 * @brief Remove exceeding antimatter if an antimatter tank has been destroyed.
 * @param[in] base Pointer to the base.
 */
void CAP_RemoveAntimatterExceedingCapacity (base_t *base)
{
	const int amount = CAP_GetCurrent(base, CAP_ANTIMATTER) - CAP_GetMax(base, CAP_ANTIMATTER);
	if (amount <= 0)
		return;

	B_ManageAntimatter(base, amount, qfalse);
}

/**
 * @brief Remove items until everything fits in storage.
 * @note items will be randomly selected for removal.
 * @param[in] base Pointer to the base
 */
void CAP_RemoveItemsExceedingCapacity (base_t *base)
{
	int i;
	int objIdx[MAX_OBJDEFS];	/**< Will contain idx of items that can be removed */
	int num;

	if (CAP_GetFreeCapacity(base, CAP_ITEMS) >= 0)
		return;

	for (i = 0, num = 0; i < csi.numODs; i++) {
		const objDef_t *obj = INVSH_GetItemByIDX(i);

		if (!B_ItemIsStoredInBaseStorage(obj))
			continue;

		/* Don't count item that we don't have in base */
		if (B_ItemInBase(obj, base) <= 0)
			continue;

		objIdx[num++] = i;
	}

	while (num && CAP_GetFreeCapacity(base, CAP_ITEMS) < 0) {
		/* Select the item to remove */
		const int randNumber = rand() % num;

		/* items are destroyed. We guess that all items of a given type are stored in the same location
		 *	=> destroy all items of this type */
		const int idx = objIdx[randNumber];
		const objDef_t *od = INVSH_GetItemByIDX(idx);
		B_UpdateStorageAndCapacity(base, od, -B_ItemInBase(od, base), qfalse);

		REMOVE_ELEM(objIdx, randNumber, num);

		/* Make sure that we don't have an infinite loop */
		if (num <= 0)
			break;
	}
	Com_DPrintf(DEBUG_CLIENT, "B_RemoveItemsExceedingCapacity: Remains %i in storage for a maximum of %i\n",
		CAP_GetCurrent(base, CAP_ITEMS), CAP_GetMax(base, CAP_ITEMS));
}

/**
 * @brief Update Storage Capacity.
 * @param[in] base Pointer to the base
 * @sa B_ResetAllStatusAndCapacities_f
 */
void CAP_UpdateStorageCap (base_t *base)
{
	int i;

	CAP_SetCurrent(base, CAP_ITEMS, 0);

	for (i = 0; i < csi.numODs; i++) {
		const objDef_t *obj = INVSH_GetItemByIDX(i);

		if (!B_ItemIsStoredInBaseStorage(obj))
			continue;

		CAP_AddCurrent(base, CAP_ITEMS, B_ItemInBase(obj, base) * obj->size);
	}
}

/**
 * @brief Sets the maximal capacity on a base
 * @param[in,out] base The base to set capacity at
 * @param[in] capacity Capacity type
 * @param[in] value New maximal capacity value
 */
void CAP_SetMax (base_t* base, baseCapacities_t capacity, int value)
{
	base->capacities[capacity].max = max(0, value);
}

/**
 * @brief Changes the maximal capacity on a base
 * @param[in,out] base The base to set capacity at
 * @param[in] capacity Capacity type
 * @param[in] value Capacity to add to the maximal capacity value (negative to decrease)
 */
void CAP_AddMax (base_t* base, baseCapacities_t capacity, int value)
{
	base->capacities[capacity].max = max(0, base->capacities[capacity].max + value);
}

/**
 * @brief Sets the current (used) capacity on a base
 * @param[in,out] base The base to set capacity at
 * @param[in] capacity Capacity type
 * @param[in] value New current (used) capacity value
 */
void CAP_SetCurrent (base_t* base, baseCapacities_t capacity, int value)
{
	base->capacities[capacity].cur = max(0, value);
}

/**
 * @brief Changes the current (used) capacity on a base
 * @param[in,out] base The base to set capacity at
 * @param[in] capacity Capacity type
 * @param[in] value Capacity to add to the current (used) capacity value (negative to decrease)
 */
void CAP_AddCurrent (base_t* base, baseCapacities_t capacity, int value)
{
	base->capacities[capacity].cur = max(0, base->capacities[capacity].cur + value);
}

/**
 * @brief Returns the free capacity of a type
 * @param[in] base Pointer to the base to check
 * @param[in] cap Capacity type
 * @sa baseCapacities_t
 */
int CAP_GetFreeCapacity (const base_t *base, baseCapacities_t capacityType)
{
	const capacities_t *cap = CAP_Get(base, capacityType);
	return cap->max - cap->cur;
}

/**
 * @brief Checks capacity overflows on bases
 * @sa CP_CampaignRun
 */
void CAP_CheckOverflow (void)
{
	base_t *base = NULL;

	while ((base = B_GetNext(base)) != NULL) {
		baseCapacities_t capacityType;

		for (capacityType = CAP_ALIENS; capacityType < MAX_CAP; capacityType++) {
			capacities_t *cap = CAP_Get(base, capacityType);

			if (cap->cur <= cap->max)
				continue;

			switch (capacityType) {
			case CAP_WORKSPACE:
				PR_UpdateProductionCap(base);
				break;
			case CAP_ITEMS:
				CAP_RemoveItemsExceedingCapacity(base);
				break;
			case CAP_ALIENS:
				AL_RemoveAliensExceedingCapacity(base);
				break;
			case CAP_LABSPACE:
				RS_RemoveScientistsExceedingCapacity(base);
				break;
			case CAP_AIRCRAFT_SMALL:
			case CAP_AIRCRAFT_BIG:
				CAP_RemoveAircraftExceedingCapacity(base, capacityType);
				break;
			case CAP_EMPLOYEES:
				E_DeleteEmployeesExceedingCapacity(base);
				break;
			case CAP_ANTIMATTER:
				CAP_RemoveAntimatterExceedingCapacity(base);
				break;
			default:
				/* nothing to do */
				break;
			}
		}
	}
}
