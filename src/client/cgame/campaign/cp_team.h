/**
 * @file
 * @brief Team management for the campaign gametype headers
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

See the GNU General Public License for more details.m

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

*/

#pragma once

void CP_CleanTempInventory(base_t* base);
void CP_UpdateActorAircraftVar(aircraft_t* aircraft, employeeType_t employeeType);
void CP_CleanupAircraftTeam(aircraft_t* aircraft, equipDef_t* ed);
void CP_CleanupTeam(base_t* base, equipDef_t* ed);
void CP_SetEquipContainer(character_t* chr);
void CP_AddWeaponAmmo(equipDef_t* ed, Item* item);
