/**
 * @file cl_mapfightequip.h
 * @brief Header for slot management related stuff.
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

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

*/

#ifndef CLIENT_CL_MAPFIGHTEQUIP_H
#define CLIENT_CL_MAPFIGHTEQUIP_H

/** Base defense functions. */
extern void BDEF_AddBattery_f(void);
extern void BDEF_RemoveBattery(base_t *base, int type, int idx);
extern void BDEF_RemoveBattery_f(void);
extern void BDEF_InitialiseBaseSlots(base_t *base);
extern void BDEF_Init_f(void);
extern void BDEF_ListClick_f(void);

extern void AII_UpdateInstallationDelay(void);
extern void AIM_AircraftEquipmenuInit_f(void);
extern void AIM_AircraftEquipSlotSelect_f(void);
extern void AIM_AircraftEquipzoneSelect_f(void);
extern qboolean AII_AddItemToSlot(technology_t *tech, aircraftSlot_t *slot);
extern void AIM_AircraftEquipAddItem_f(void);
extern void AIM_AircraftEquipDeleteItem_f(void);
extern void AIM_AircraftEquipmenuClick_f(void);
extern void AII_InitialiseSlot(aircraftSlot_t *slot, int aircraftIdx);
extern void AII_UpdateAircraftStats(aircraft_t *aircraft);
extern int AII_GetSlotItems(aircraftItemType_t type, aircraft_t *aircraft);
extern int AII_AircraftCanShoot(aircraft_t *aircraft);
extern int AII_BaseCanShoot(base_t *base);

const char* AII_WeightToName(itemWeight_t weight);

#endif /* CLIENT_CL_MAPFIGHTEQUIP_H */
