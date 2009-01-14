/**
 * @file cl_fightequip_callbacks.h
 * @brief Header file for menu callback functions used for base and aircraft equip menu
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
#ifndef CLIENT_CAMPAIGN_CL_FIGHTEQUIP_CALLBACKS_H
#define CLIENT_CAMPAIGN_CL_FIGHTEQUIP_CALLBACKS_H

void BDEF_MenuInit_f(void);
void BDEF_ListClick_f(void);

void AIM_AircraftEquipMenuUpdate_f(void);
void AIM_AircraftEquipSlotSelect_f(void);
void AIM_AircraftEquipZoneSelect_f(void);
void AIM_AircraftEquipAddItem_f(void);
void AIM_AircraftEquipDeleteItem_f(void);
void AIM_AircraftEquipMenuClick_f(void);
void AIM_ResetEquipAircraftMenu(void);

void BDEF_BaseDefenseMenuUpdate_f(void);

aircraftSlot_t *AII_SelectAircraftSlot (aircraft_t *aircraft, const int airequipID);
aircraftSlot_t *BDEF_SelectBaseSlot (base_t *base, const int airequipID);
aircraftSlot_t *BDEF_SelectInstallationSlot (installation_t *installation, const int airequipID);

void AIM_InitCallbacks(void);
void AIM_ShutdownCallbacks(void);

#endif /* CLIENT_CAMPAIGN_CL_FIGHTEQUIP_CALLBACKS_H */
