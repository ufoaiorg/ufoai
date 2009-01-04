/**
 * @file cl_team.h
 */

/*
Copyright (C) 1997-2008 UFO:AI Team

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

#ifndef CLIENT_CL_TEAM_H
#define CLIENT_CL_TEAM_H

#define MAX_WHOLETEAM	32

#define MAX_TEAMDATASIZE	32768

#define NUM_TEAMSKINS	6
#define NUM_TEAMSKINS_SINGLEPLAYER 4

qboolean CL_SoldierAwayFromBase(const employee_t *soldier);
int CL_UpdateActorAircraftVar(aircraft_t *aircraft, employeeType_t employeeType);
void CL_ReloadAndRemoveCarried(aircraft_t *aircraft, equipDef_t * equip);
void CL_CleanTempInventory(base_t* base);

void CL_ResetCharacters(base_t* const base);
int CL_GenTeamList(const base_t *base);
void CL_GenerateCharacter(employee_t *employee, const char *team, employeeType_t employeeType, const ugv_t *ugvType);
ugv_t *CL_GetUgvByID(const char *ugvID);
const char* CL_GetTeamSkinName(int id);

qboolean CL_PilotInAircraft(const employee_t *employee, const aircraft_t* aircraft);
const aircraft_t *CL_SoldierInAircraft(const employee_t *employee, const aircraft_t* aircraft);
qboolean CL_RemoveSoldierFromAircraft(employee_t *employee, aircraft_t* aircraft);
void CL_RemoveSoldiersFromAircraft(aircraft_t* aircraft);
void CL_AssignSoldierFromMenuToAircraft(base_t *base, const int num, aircraft_t *aircraft);
qboolean CL_AssignSoldierToAircraft(employee_t *employee, aircraft_t *aircraft);

void CL_SaveInventory(sizebuf_t * buf, const inventory_t * i);
void CL_NetReceiveItem(struct dbuffer * buf, item_t * item, int * container, int * x, int * y);
void CL_LoadInventory(sizebuf_t * buf, inventory_t * i);
void TEAM_InitStartup(void);
void CL_ParseResults(struct dbuffer *msg);
void CL_SendCurTeamInfo(struct dbuffer * buf, chrList_t *team, base_t *base);
void CL_AddCarriedToEquipment(const struct aircraft_s *aircraft, equipDef_t *equip);
void CL_ParseCharacterData(struct dbuffer *msg);

#endif
