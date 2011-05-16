/**
 * @file cp_mapfightequip.h
 * @brief Header for slot management related stuff.
 */

/*
Copyright (C) 2002-2011 UFO: Alien Invasion.

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

#ifndef CLIENT_CL_MAPFIGHTEQUIP_H
#define CLIENT_CL_MAPFIGHTEQUIP_H

/**
 * @brief Zone number in airequip menu or base defence menu.
 * @note A zone is the rectangular box in the upper right of the screen.
 * @note A zone is different from a slot or the type of an item.
 */
typedef enum {
	ZONE_NONE,	/**< This is not a zone (used when no zone is selected). */
	ZONE_MAIN,	/**< This is the upper (1st) zone containing the current item installed in the slot. */
	ZONE_AMMO,	/**< This is the lowest (3rd) zone containing the ammo fitting the weapon in ZONE_MAIN
				 ** Therefore, this zone should only be used when 1st zone contain an WEAPON. */

	ZONE_MAX
} zoneaircraftParams_t;

/**
 * @brief Different status for numAmmo.
 * @todo do we still need this?
 */
typedef enum {
	AMMO_STATUS_NOT_SET = -1,			/**< This weapon can't shoot yet. */

	AMMO_STATUS_NO_MORE_AMMO = 0		/**< This weapon has no ammo. */
	/* positive values are used for number of ammo left in the weapon */
} ammoStatus_t;

/**
 * @brief The different possible types of base defence systems.
 * @sa BDEF_RemoveBattery_f: BASEDEF_LASER must be just after BASEDEF_MISSILE
 */
typedef enum {
	BASEDEF_RANDOM,		/**< The base defence system should be randomly selected. */
	BASEDEF_MISSILE,	/**< The base defence system is a missile battery. */
	BASEDEF_LASER,		/**< The base defence system is a laser battery. */

	BASEDEF_MAX
} basedefenceType_t;

/* Base defence functions. */
void BDEF_AddBattery(basedefenceType_t basedefType, base_t* base);
void BDEF_RemoveBattery(base_t *base, basedefenceType_t basedefType, int idx);
void BDEF_InitialiseBaseSlots(base_t *base);
void BDEF_InitialiseInstallationSlots(installation_t *installation);
void BDEF_ReloadBattery(void);
void BDEF_AutoSelectTarget(void);

technology_t **AII_GetCraftitemTechsByType(aircraftItemType_t type);
void AII_UpdateInstallationDelay(void);
qboolean AII_AddItemToSlot(base_t* base, const technology_t *tech, aircraftSlot_t *slot, qboolean nextItem);
qboolean AII_AddAmmoToSlot(base_t* base, const technology_t *tech, aircraftSlot_t *slot);
void AII_RemoveItemFromSlot(base_t* base, aircraftSlot_t *slot, qboolean ammo);
void AII_RemoveNextItemFromSlot(base_t* base, aircraftSlot_t *slot, qboolean ammo);
qboolean AIM_PilotAssignedAircraft(const base_t* base, const employee_t* pilot);
void AIM_AutoEquipAircraft(aircraft_t *aircraft);
void AII_InitialiseSlot(aircraftSlot_t *slot, aircraft_t *aircraft, base_t *base, installation_t *installation, aircraftItemType_t type);
float AIR_GetMaxAircraftWeaponRange(const aircraftSlot_t *slot, int maxSlot);
void AII_RepairAircraft(void);
void AII_UpdateAircraftStats(aircraft_t *aircraft);
int AII_BaseCanShoot(const base_t *base);
qboolean AII_InstallationCanShoot(const installation_t *installation);

itemWeight_t AII_GetItemWeightBySize(const objDef_t *od);

const char* AII_WeightToName(itemWeight_t weight);
void AII_AutoAddAmmo(aircraftSlot_t *slot);
qboolean AIM_SelectableCraftItem(const aircraftSlot_t *slot, const technology_t *tech);

aircraftSlot_t *BDEF_GetBaseSlotByIDX(base_t *base, aircraftItemType_t type, int idx);
aircraftSlot_t *BDEF_GetInstallationSlotByIDX(installation_t *installation, aircraftItemType_t type, int idx);
aircraftSlot_t *AII_GetAircraftSlotByIDX(aircraft_t *aircraft, aircraftItemType_t type, int idx);

qboolean AII_ReloadWeapon(aircraftSlot_t *slot);
void AII_ReloadAircraftWeapons(aircraft_t *aircraft);
void BDEF_ReloadBaseWeapons(base_t *base, installation_t *installation);

void AII_SaveOneSlotXML(xmlNode_t *p, const aircraftSlot_t *slot, qboolean weapon);
void AII_LoadOneSlotXML(xmlNode_t *node, aircraftSlot_t* slot, qboolean weapon);

#endif /* CLIENT_CL_MAPFIGHTEQUIP_H */
