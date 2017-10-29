/**
 * @file
 * @brief Campaign missions headers
 */

/*
Copyright (C) 2002-2017 UFO: Alien Invasion.

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

#pragma once

#include "cp_campaign.h"

extern const int MAX_POS_LOOP;

/** possible stage for campaign missions (i.e. possible actions for UFO) */
typedef enum missionStage_s {
	STAGE_NOT_ACTIVE,				/**< mission did not begin yet */
	STAGE_COME_FROM_ORBIT,			/**< UFO is arriving */

	STAGE_RECON_AIR,				/**< Aerial Recon */
	STAGE_MISSION_GOTO,				/**< Going to a new position */
	STAGE_RECON_GROUND,				/**< Ground Recon */
	STAGE_TERROR_MISSION,			/**< Terror mission */
	STAGE_BUILD_BASE,				/**< Building a base */
	STAGE_BASE_ATTACK,				/**< Base attack */
	STAGE_SUBVERT_GOV,				/**< Subvert government */
	STAGE_SUPPLY,					/**< Supply already existing base */
	STAGE_SPREAD_XVI,				/**< Spreading XVI Virus */
	STAGE_INTERCEPT,				/**< UFO attacks any encountered PHALANX aircraft or attack an installation */
	STAGE_BASE_DISCOVERED,			/**< PHALANX discovered the base */
	STAGE_HARVEST,					/**< Harvesting */

	STAGE_RETURN_TO_ORBIT,			/**< UFO is going back to base */

	STAGE_OVER						/**< Mission is over */
} missionStage_t;

typedef enum {
	WON, DRAW, LOST
} missionState_t;

/** @brief Structure with mission info needed to create results summary at menu won. */
typedef struct missionResults_s {
	const struct mission_s* mission;
	missionState_t state;
	bool recovery;		/**< @c true if player secured a UFO (landed or crashed). */
	bool crashsite;		/**< @c true if secured UFO was crashed one. */
	ufoType_t ufotype;		/**< Type of UFO secured during the mission. */
	float ufoCondition;		/**< How much the UFO is damaged */
	int itemTypes;			/**< Types of items gathered from a mission. */
	int itemAmount;			/**< Amount of items (all) gathered from a mission. */
	int aliensKilled;
	int aliensStunned;
	int aliensSurvived;
	int ownKilled;
	int ownStunned;
	int ownKilledFriendlyFire;
	int ownSurvived;
	int civiliansKilled;
	int civiliansKilledFriendlyFire;
	int civiliansSurvived;
} missionResults_t;

/** @brief mission definition
 * @note A mission is different from a map: a mission is the whole set of actions aliens will carry.
 * For example, coming with a UFO on earth, land, explore earth, and leave with UFO
 */
typedef struct mission_s {
	int idx;				/**< unique id of this mission */
	char id[MAX_VAR];			/**< script id */
	mapDef_t* mapDef;			/**< mapDef used for this mission */
	bool active;				/**< aircraft at place? */
	union missionData_t {
		base_t* base;
		aircraft_t* aircraft;
		installation_t* installation;
		alienBase_t* alienBase;
		city_t* city;
	} data;					/**< may be related to mission type (like pointer to base attacked, or to alien base) */
	interestCategory_t category;		/**< The category of the event */
	missionStage_t stage;			/**< in which stage is this event? */
	int initialOverallInterest;		/**< The overall interest value when this event has been created */
	int initialIndividualInterest;		/**< The individual interest value (of type type) when this event has been created */
	date_t startDate;			/**< Date when the event should start */
	date_t finalDate;			/**< Date when the event should finish (e.g. for aerial recon)
						 * if finaleDate.day == 0, then delay is not a limitating factor for next stage */
	vec2_t pos;				/**< Position of the mission */
	aircraft_t* ufo;			/**< UFO on geoscape fulfilling the mission (may be nullptr) */
	bool onGeoscape;			/**< Should the mission be displayed on geoscape */
	bool crashed;				/**< is UFO crashed ? (only used if mission is spawned from a UFO */

	char onwin[MAX_VAR];			/**< trigger command after you've won a battle, @sa CP_ExecuteMissionTrigger */
	char onlose[MAX_VAR];			/**< trigger command after you've lost a battle, @sa CP_ExecuteMissionTrigger */
	bool posAssigned;			/**< is the position of this mission already set? */
	missionResults_t missionResults;
} mission_t;

/**
 * @brief iterates through missions
 */
#define MIS_Foreach(var) LIST_Foreach(ccs.missions, mission_t, var)

const char* MIS_GetName(const mission_t* mission);

void BATTLE_SetVars(const battleParam_t* battleParameters);
void CP_CreateBattleParameters(mission_t* mission, battleParam_t* param, const aircraft_t* aircraft);
void BATTLE_Start(mission_t* mission, const battleParam_t* battleParameters);
mission_t* CP_GetMissionByIDSilent(const char* missionId);
mission_t* CP_GetMissionByID(const char* missionId);
int MIS_GetIdx(const mission_t* mis);
mission_t* MIS_GetByIdx(int id);
mission_t* CP_CreateNewMission(interestCategory_t category, bool beginNow);
void CP_UFOProceedMission(const struct campaign_s* campaign, struct aircraft_s* ufocraft);
void CP_MissionRemove(mission_t* mission);
bool CP_MissionBegin(mission_t* mission);
bool CP_CheckNewMissionDetectedOnGeoscape(void);
bool CP_CheckMissionLimitedInTime(const mission_t* mission);
void CP_MissionDisableTimeLimit(mission_t* mission);
void CP_MissionNotifyBaseDestroyed(const base_t* base);
void CP_MissionNotifyInstallationDestroyed(const installation_t* installation);
const char* MIS_GetModel(const mission_t* mission);
void CP_MissionRemoveFromGeoscape(mission_t* mission);
void CP_MissionAddToGeoscape(mission_t* mission, bool force);
void CP_UFORemoveFromGeoscape(mission_t* mission, bool destroyed);
void CP_SpawnCrashSiteMission(aircraft_t* ufo);
void CP_SpawnRescueMission(aircraft_t* aircraft, aircraft_t* ufo);
ufoType_t CP_MissionChooseUFO(const mission_t* mission);
void CP_MissionStageEnd(const campaign_t* campaign, mission_t* mission);
void CP_InitializeSpawningDelay(void);
void CP_SpawnNewMissions(void);
void CP_MissionIsOver(mission_t* mission);
void CP_MissionIsOverByUFO(aircraft_t* ufocraft);
void CP_MissionEnd(const campaign_t* campaign, mission_t* mission, const battleParam_t* battleParameters, bool won);
void CP_MissionEndActions(mission_t* mission, aircraft_t* aircraft, bool won);
bool CP_ChooseMap(mission_t* mission, const vec2_t pos);
bool CP_CheckNextStageDestination(const struct campaign_s* campaign, struct aircraft_s* ufo);
int CP_CountMissionOnGeoscape(void);
void CP_UpdateMissionVisibleOnGeoscape(void);

void MIS_InitStartup(void);
void MIS_Shutdown(void);
