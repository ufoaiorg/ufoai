/**
 * @file cp_missions.h
 * @brief Campaign missions headers
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

#ifndef CP_MISSIONS_H
#define CP_MISSIONS_H

#include "missions/cp_mission_baseattack.h"
#include "missions/cp_mission_buildbase.h"
#include "missions/cp_mission_harvest.h"
#include "missions/cp_mission_intercept.h"
#include "missions/cp_mission_recon.h"
#include "missions/cp_mission_rescue.h"
#include "missions/cp_mission_supply.h"
#include "missions/cp_mission_terror.h"
#include "missions/cp_mission_xvi.h"

extern const int MAX_POS_LOOP;

void CP_SetMissionVars(const mission_t *mission, const battleParam_t *battleParameters);
void CP_CreateBattleParameters(mission_t *mission, battleParam_t *param, const aircraft_t *aircraft);
void CP_StartMissionMap(mission_t* mission, const battleParam_t *battleParameters);
mission_t* CP_GetMissionByIDSilent(const char *missionId);
mission_t *CP_GetMissionByID(const char *missionId);
const char *CP_MissionToTypeString(const mission_t *mission);
int MAP_GetIDXByMission(const mission_t *mis);
mission_t* MAP_GetMissionByIDX(int id);
void CP_MissionRemove(mission_t *mission);
qboolean CP_MissionBegin(mission_t *mission);
qboolean CP_CheckNewMissionDetectedOnGeoscape(void);
qboolean CP_CheckMissionLimitedInTime(const mission_t *mission);
void CP_MissionDisableTimeLimit(mission_t *mission);
void CP_MissionNotifyBaseDestroyed(const base_t *base);
void CP_MissionNotifyInstallationDestroyed(const installation_t const *installation);
const char* MAP_GetMissionModel(const mission_t *mission);
void CP_MissionRemoveFromGeoscape(mission_t *mission);
void CP_MissionAddToGeoscape(mission_t *mission, qboolean force);
void CP_UFORemoveFromGeoscape(mission_t *mission, qboolean destroyed);
void CP_SpawnCrashSiteMission(aircraft_t *ufo);
void CP_SpawnRescueMission(aircraft_t *aircraft, aircraft_t *ufo);
ufoType_t CP_MissionChooseUFO(const mission_t *mission);
void CP_MissionStageEnd(const campaign_t* campaign, mission_t *mission);
void CP_InitializeSpawningDelay(void);
void CP_SpawnNewMissions(void);
void CP_MissionIsOver(mission_t *mission);
void CP_MissionIsOverByUFO(aircraft_t *ufocraft);
void CP_MissionEnd(const campaign_t *campaign, mission_t* mission, const battleParam_t* battleParameters, qboolean won);
void CP_MissionEndActions(mission_t *mission, aircraft_t *aircraft, qboolean won);

void MIS_InitStartup(void);
void MIS_Shutdown(void);

#endif
