/**
 * @file
 */

/*
Copyright (C) 2002-2015 UFO: Alien Invasion.

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
#include "cp_geoscape.h"
#include "cp_popup.h"
#include "cp_time.h"
#include "cp_ufo.h"

/**
 * @brief Remove exceeding antimatter if an antimatter tank has been destroyed.
 * @param[in] base Pointer to the base.
 */
void CAP_RemoveAntimatterExceedingCapacity (base_t* base)
{
	const int amount = CAP_GetMax(base, CAP_ANTIMATTER) - CAP_GetCurrent(base, CAP_ANTIMATTER);
	if (amount >= 0)
		return;

	B_AddAntimatter(base, amount);
}

/**
 * @brief Update Storage Capacity.
 * @param[in] base Pointer to the base
 * @sa B_ResetAllStatusAndCapacities_f
 */
void CAP_UpdateStorageCap (base_t* base)
{
	CAP_SetCurrent(base, CAP_ITEMS, 0);

	for (int i = 0; i < cgi->csi->numODs; i++) {
		const objDef_t* obj = INVSH_GetItemByIDX(i);

		if (!B_ItemIsStoredInBaseStorage(obj))
			continue;

		CAP_AddCurrent(base, CAP_ITEMS, B_ItemInBase(obj, base) * obj->size);
	}

	/* UGV takes room in storage capacity */
	CAP_AddCurrent(base, CAP_ITEMS, UGV_SIZE * E_CountHired(base, EMPL_ROBOT));
}

/**
 * @brief Sets the maximal capacity on a base
 * @param[in,out] base The base to set capacity at
 * @param[in] capacity Capacity type
 * @param[in] value New maximal capacity value
 */
void CAP_SetMax (base_t* base, baseCapacities_t capacity, int value)
{
	base->capacities[capacity].max = std::max(0, value);
}

/**
 * @brief Changes the maximal capacity on a base
 * @param[in,out] base The base to set capacity at
 * @param[in] capacity Capacity type
 * @param[in] value Capacity to add to the maximal capacity value (negative to decrease)
 */
void CAP_AddMax (base_t* base, baseCapacities_t capacity, int value)
{
	base->capacities[capacity].max = std::max(0, base->capacities[capacity].max + value);
}

/**
 * @brief Sets the current (used) capacity on a base
 * @param[in,out] base The base to set capacity at
 * @param[in] capacity Capacity type
 * @param[in] value New current (used) capacity value
 */
void CAP_SetCurrent (base_t* base, baseCapacities_t capacity, int value)
{
	base->capacities[capacity].cur = std::max(0, value);
}

/**
 * @brief Changes the current (used) capacity on a base
 * @param[in,out] base The base to set capacity at
 * @param[in] capacity Capacity type
 * @param[in] value Capacity to add to the current (used) capacity value (negative to decrease)
 */
void CAP_AddCurrent (base_t* base, baseCapacities_t capacity, int value)
{
	base->capacities[capacity].cur = std::max(0, base->capacities[capacity].cur + value);
}

/**
 * @brief Returns the free capacity of a type
 * @param[in] base Pointer to the base to check
 * @param[in] capacityType Capacity type
 * @sa baseCapacities_t
 */
int CAP_GetFreeCapacity (const base_t* base, baseCapacities_t capacityType)
{
	const capacities_t* cap = CAP_Get(base, capacityType);
	return cap->max - cap->cur;
}

/**
 * @brief Checks capacity overflows on bases
 * @sa CP_CampaignRun
 */
void CAP_CheckOverflow (void)
{
	base_t* base = nullptr;

	while ((base = B_GetNext(base)) != nullptr) {
		for (int i = CAP_ALIENS; i < MAX_CAP; i++) {
			baseCapacities_t capacityType = (baseCapacities_t)i;
			capacities_t* cap = CAP_Get(base, capacityType);

			if (cap->cur <= cap->max)
				continue;

			switch (capacityType) {
			case CAP_ANTIMATTER:
				CAP_RemoveAntimatterExceedingCapacity(base);
				break;
			case CAP_WORKSPACE:
				PR_UpdateProductionCap(base);
				break;
			case CAP_LABSPACE:
				RS_RemoveScientistsExceedingCapacity(base);
				break;
			case CAP_AIRCRAFT_SMALL:
			case CAP_AIRCRAFT_BIG:
			case CAP_ALIENS:
			case CAP_EMPLOYEES:
			case CAP_ITEMS:
				if (base->baseStatus != BASE_DESTROYED) {
					const buildingType_t bldgType = B_GetBuildingTypeByCapacity((baseCapacities_t)i);
					const building_t* bldg = B_GetBuildingTemplateByType(bldgType);
					CP_GameTimeStop();
					cgi->Cmd_ExecuteString("ui_push popup_cap_overload base %d \"%s\" \"%s\" %d %d",
						base->idx, base->name, _(bldg->name), cap->max - cap->cur, cap->max);
				}
				break;
			default:
				/* nothing to do */
				break;
			}
		}
	}
}
