/**
 * @file cl_campaign.h
 * @brief Header file for single player campaign control.
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

#ifndef CLIENT_CL_CAMPAIGN_H
#define CLIENT_CL_CAMPAIGN_H

#include "cl_installation.h"

#define BID_FACTOR 0.9

/* check for water */
/* blue value is 64 */
#define MapIsWater(color)        (color[0] ==   0 && color[1] ==   0 && color[2] ==  64)

/* terrain types */
#define MapIsArctic(color)       (color[0] == 128 && color[1] == 255 && color[2] == 255)
#define MapIsDesert(color)       (color[0] == 255 && color[1] == 128 && color[2] ==   0)
#define MapIsMountain(color)     (color[0] == 255 && color[1] ==   0 && color[2] ==   0)
#define MapIsTropical(color)     (color[0] == 128 && color[1] == 128 && color[2] == 255)
#define MapIsGrass(color)        (color[0] == 128 && color[1] == 255 && color[2] ==   0)
#define MapIsWasted(color)       (color[0] == 128 && color[1] ==   0 && color[2] == 128)
#define MapIsCold(color)         (color[0] ==   0 && color[1] ==   0 && color[2] == 255)

/* culture types */
#define MapIsWestern(color)      (color[0] == 128 && color[1] == 255 && color[2] == 255)
#define MapIsEastern(color)      (color[0] == 255 && color[1] == 128 && color[2] ==   0)
#define MapIsOriental(color)     (color[0] == 255 && color[1] ==   0 && color[2] ==   0)
#define MapIsAfrican(color)      (color[0] == 128 && color[1] == 128 && color[2] == 255)

/* population types */
#define MapIsUrban(color)        (color[0] == 128 && color[1] == 255 && color[2] == 255)
#define MapIsSuburban(color)     (color[0] == 255 && color[1] == 128 && color[2] ==   0)
#define MapIsVillage(color)      (color[0] == 255 && color[1] ==   0 && color[2] ==   0)
#define MapIsRural(color)        (color[0] == 128 && color[1] == 128 && color[2] == 255)
#define MapIsNopopulation(color) (color[0] == 128 && color[1] == 255 && color[2] ==   0)

/* RASTER enables a better performance for CP_GetRandomPosOnGeoscapeWithParameters set it to 1-6
 * the higher the value the better the performance, but the smaller the coverage */
#define RASTER 2

/* nation happiness constants */
#define HAPPINESS_SUBVERSION_LOSS			-0.15
#define HAPPINESS_ALIEN_MISSION_LOSS		-0.01
#define HAPPINESS_UFO_SALE_GAIN				0.07
#define HAPPINESS_UFO_SALE_LOSS				0.01
#define HAPPINESS_MAX_MISSION_IMPACT		0.15

/** possible map types */
typedef enum mapType_s {
	MAPTYPE_TERRAIN,
	MAPTYPE_CULTURE,
	MAPTYPE_POPULATION,
	MAPTYPE_NATIONS,

	MAPTYPE_MAX
} mapType_t;

/** @brief possible alien teams type */
typedef enum alienTeamType_s {
	ALIENTEAM_DEFAULT,
	ALIENTEAM_XVI,
	ALIENTEAM_HARVEST,

	ALIENTEAM_MAX
} alienTeamType_t;

/** @brief possible mission detection status */
typedef enum missionDetectionStatus_s {
	MISDET_CANT_BE_DETECTED,		/**< Mission can't be seen on geoscape */
	MISDET_ALWAYS_DETECTED,			/**< Mission is seen on geoscape, whatever it's position */
	MISDET_MAY_BE_DETECTED			/**< Mission may be seen on geoscape, if a probability test is done */
} missionDetectionStatus_t;

/** possible campaign interest categories: type of missions that aliens can undertake */
typedef enum interestCategory_s {
	INTERESTCATEGORY_NONE,			/**< No mission */
	INTERESTCATEGORY_RECON,			/**< Aerial recon mission or ground mission (UFO may or not land) */
	INTERESTCATEGORY_TERROR_ATTACK,	/**< Terror attack */
	INTERESTCATEGORY_BASE_ATTACK,	/**< Alien attack a phalanx base */
	INTERESTCATEGORY_BUILDING,		/**< Alien build a new base or subverse governments */
	INTERESTCATEGORY_SUPPLY,		/**< Alien supply one of their bases */
	INTERESTCATEGORY_XVI,			/**< Alien try to spread XVI */
	INTERESTCATEGORY_INTERCEPT,		/**< Alien try to intercept PHALANX aircraft */
	INTERESTCATEGORY_HARVEST,		/**< Alien try to harvest */

	INTERESTCATEGORY_MAX
} interestCategory_t;

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



/** @brief mission definition
 * @note A mission is different from a map: a mission is the whole set of actions aliens will carry.
 * For example, comming with a UFO on earth, land, explore earth, and leave with UFO
 */
typedef struct mission_s {
	char id[MAX_VAR];				/**< script id (must be first element of mission_t to use LIST_ContainsString) */
	mapDef_t* mapDef;				/**< mapDef used for this mission */
	qboolean active;				/**< aircraft at place? */
	void* data;						/**< may be related to mission type (like pointer to base attacked, or to alien base) */
	char location[MAX_VAR];			/**< The name of the ground mission that will appear on geoscape */
	interestCategory_t category;	/**< The category of the event */
	missionStage_t stage;			/**< in which stage is this event? */
	int initialOverallInterest;		/**< The overall interest value when this event has been created */
	int initialIndividualInterest;	/**< The individual interest value (of type type) when this event has been created */
	date_t startDate;				/**< Date when the event should start */
	date_t finalDate;				/**< Date when the event should finish (e.g. for aerial recon)
									 * if finaleDate.day == 0, then delay is not a limitating factor for next stage */
	vec2_t pos;						/**< Position of the mission */
	aircraft_t *ufo;				/**< UFO on geoscape fulfilling the mission (may be NULL) */
	qboolean onGeoscape;			/**< Should the mission be displayed on geoscape */

	char onwin[MAX_VAR];			/**< trigger command after you've won a battle, @sa CP_ExecuteMissionTrigger */
	char onlose[MAX_VAR];			/**< trigger command after you've lost a battle, @sa CP_ExecuteMissionTrigger */
} mission_t;

/** battlescape parameters that were used */
typedef struct battleParam_s {
	mission_t *mission;
	teamDef_t *alienTeams[MAX_TEAMS_PER_MISSION];	/**< Race of aliens present in battle */
	char *param;					/**< in case of a random map assembly we can't use the param from mapDef - because
									 * this is global for the mapDef - but we need a local mission param */
	int numAlienTeams;								/**< Number of different races */
	char alienEquipment[MAX_VAR];					/**< Equipment of alien team */
	char civTeam[MAX_VAR];							/**< Type of civilian (europeean, ...) */
	qboolean day;									/**< Mission is played during day */
	const char *zoneType;							/**< Terrain type (used for texture replacement in base missions) */
	int ugv;						/**< uncontrolled ground units (entity: info_2x2_start) */
	int aliens, civilians;			/**< number of aliens and civilians in that particular mission */
	struct nation_s *nation;		/**< nation where the mission takes place */
} battleParam_t;

/** @brief Structure with mission info needed to create results summary at Menu Won. */
typedef struct missionResults_s {
	int itemtypes;		/**< Types of items gathered from a mission. */
	int itemamount;		/**< Amount of items (all) gathered from a mission. */
	qboolean recovery;	/**< Qtrue if player secured a UFO (landed or crashed). */
	ufoType_t ufotype;	/**< Type of UFO secured during the mission. */
	qboolean crashsite;	/**< Qtrue if secured UFO was crashed one. */
} missionResults_t;

extern missionResults_t missionresults;	/**< Mission results pointer used for Menu Won. */

typedef struct stats_s {
	int missionsWon;
	int missionsLost;
	int basesBuild;
	int basesAttacked;
	int installationsBuild;
	int interceptions;
	int soldiersLost;
	int soldiersNew;			/**< new recruits */
	int killedAliens;
	int rescuedCivilians;
	int researchedTechnologies;
	int moneyInterceptions;
	int moneyBases;
	int moneyResearch;
	int moneyWeapons;
} stats_t;

extern stats_t campaignStats;

/** campaign definition */
typedef struct campaign_s {
	int idx;					/**< own index in global campaign array */
	char id[MAX_VAR];			/**< id of the campaign */
	char name[MAX_VAR];			/**< name of the campaign */
	char team[MAX_VAR];			/**< what team can play this campaign */
	char researched[MAX_VAR];	/**< name of the researched tech list to use on campaign start */
	char equipment[MAX_VAR];	/**< name of the equipment list to use on campaign start */
	char market[MAX_VAR];		/**< name of the market list containing initial items on market */
	char asymptoticMarket[MAX_VAR];		/**< name of the market list containing items on market at the end of the game */
	equipDef_t *marketDef;		/**< market definition for this campaign (how many items on the market) containing initial items */
	equipDef_t *asymptoticMarketDef;	/**< market definition for this campaign (how many items on the market) containing finale items */
	char text[MAX_VAR];			/**< placeholder for gettext stuff */
	char map[MAX_VAR];			/**< geoscape map */
	int soldiers;				/**< start with x soldiers */
	int scientists;				/**< start with x scientists */
	int workers;				/**< start with x workers */
	int ugvs;					/**< start with x ugvs (robots) */
	int credits;				/**< start with x credits */
	int num;
	signed int difficulty;		/**< difficulty level -4 - 4 */
	float minhappiness;			/**< minimum value of mean happiness before the game is lost */
	int negativeCreditsUntilLost;	/**< bankrupt - negative credits until you've lost the game */
	int maxAllowedXVIRateUntilLost;	/**< 0 - 100 - the average rate of XVI over all nations before you've lost the game */
	qboolean visible;			/**< visible in campaign menu? */
	date_t date;				/**< starting date for this campaign */
	int basecost;				/**< base building cost for empty base */
	char firstBaseTemplate[MAX_VAR];	/**< template to use for setting up the first base */
	qboolean finished;
} campaign_t;

/** salary values for a campaign */
typedef struct salary_s {
	int soldier_base;
	int soldier_rankbonus;
	int worker_base;
	int worker_rankbonus;
	int scientist_base;
	int scientist_rankbonus;
	int pilot_base;
	int pilot_rankbonus;
	int robot_base;
	int robot_rankbonus;
	int aircraft_factor;
	int aircraft_divisor;
	int base_upkeep;
	int admin_initial;
	int admin_soldier;
	int admin_worker;
	int admin_scientist;
	int admin_pilot;
	int admin_robot;
	float debt_interest;
} salary_t;

#define SALARY_SOLDIER_BASE salaries[curCampaign->idx].soldier_base
#define SALARY_SOLDIER_RANKBONUS salaries[curCampaign->idx].soldier_rankbonus
#define SALARY_WORKER_BASE salaries[curCampaign->idx].worker_base
#define SALARY_WORKER_RANKBONUS salaries[curCampaign->idx].worker_rankbonus
#define SALARY_SCIENTIST_BASE salaries[curCampaign->idx].scientist_base
#define SALARY_SCIENTIST_RANKBONUS salaries[curCampaign->idx].scientist_rankbonus
#define SALARY_PILOT_BASE salaries[curCampaign->idx].pilot_base
#define SALARY_PILOT_RANKBONUS salaries[curCampaign->idx].pilot_rankbonus
#define SALARY_ROBOT_BASE salaries[curCampaign->idx].robot_base
#define SALARY_ROBOT_RANKBONUS salaries[curCampaign->idx].robot_rankbonus
#define SALARY_AIRCRAFT_FACTOR salaries[curCampaign->idx].aircraft_factor
#define SALARY_AIRCRAFT_DIVISOR salaries[curCampaign->idx].aircraft_divisor
#define SALARY_BASE_UPKEEP salaries[curCampaign->idx].base_upkeep
#define SALARY_ADMIN_INITIAL salaries[curCampaign->idx].admin_initial
#define SALARY_ADMIN_SOLDIER salaries[curCampaign->idx].admin_soldier
#define SALARY_ADMIN_WORKER salaries[curCampaign->idx].admin_worker
#define SALARY_ADMIN_SCIENTIST salaries[curCampaign->idx].admin_scientist
#define SALARY_ADMIN_PILOT salaries[curCampaign->idx].admin_pilot
#define SALARY_ADMIN_ROBOT salaries[curCampaign->idx].admin_robot
#define SALARY_DEBT_INTEREST salaries[curCampaign->idx].debt_interest

/** market structure */
typedef struct market_s {
	int num[MAX_OBJDEFS];					/**< number of items on the market */
	int bid[MAX_OBJDEFS];					/**< price of item for selling */
	int ask[MAX_OBJDEFS];					/**< price of item for buying */
	double currentEvolution[MAX_OBJDEFS];	/**< evolution of the market */
} market_t;

/**
 * @brief client structure
 * @sa csi_t
 */
typedef struct ccs_s {
	equipDef_t eMission;
	market_t eMarket;						/**< Prices, evolution and number of items on market */

	linkedList_t *missions;					/**< Missions spawned (visible on geoscape or not) */

	battleParam_t battleParameters;			/**< Structure used to remember every parameter used during last battle */

	int lastInterestIncreaseDelay;				/**< How many hours since last increase of alien overall interest */
	int overallInterest;						/**< overall interest of aliens: how far is the player in the campaign */
	int interest[INTERESTCATEGORY_MAX];			/**< interest of aliens: determine which actions aliens will undertake */
	int lastMissionSpawnedDelay;				/**< How many days since last mission has been spawned */

	vec2_t mapPos;		/**< geoscape map position (from the menu node) */
	vec2_t mapSize;		/**< geoscape map size (from the menu node) */

	qboolean singleplayer;	/**< singleplayer or multiplayer */

	int credits;			/**< actual credits amount */
	int civiliansKilled;	/**< how many civilians were killed already */
	int aliensKilled;		/**< how many aliens were killed already */
	date_t date;			/**< current date */
	qboolean XVISpreadActivated;	/**< should the XVI spread over the globe already */
	qboolean XVIShowMap;			/**< spread was activated and the map is activated now */
	qboolean humansAttackActivated;	/**< humans start to attack player */
	float timer;

	vec3_t angles;			/**< 3d geoscape rotation */
	vec2_t center;			/**< latitude and longitude of the point we're looking at on earth */
	float zoom;				/**< zoom used when looking at earth */

	/** governs zero build time for first base if empty base option chosen */
	int instant_build;
} ccs_t;

/** possible geoscape actions */
typedef enum mapAction_s {
	MA_NONE,
	MA_NEWBASE,				/**< build a new base */
	MA_NEWINSTALLATION,		/**< build a new installation */
	MA_INTERCEPT,			/**< intercept */
	MA_BASEATTACK,			/**< base attacking */
	MA_UFORADAR				/**< ufos are in our radar */
} mapAction_t;

extern mission_t *selectedMission;
extern campaign_t *curCampaign;
extern ccs_t ccs;
extern const int DETECTION_INTERVAL;

void AIR_SaveAircraft(sizebuf_t * sb, base_t * base);
void AIR_LoadAircraft(sizebuf_t * sb, base_t * base, int version);

void CP_CheckNextStageDestination(aircraft_t *ufo);

void CP_InitStartup(void);
void CL_ResetSinglePlayerData(void);
void CL_DateConvert(const date_t * date, byte *day, byte *month, short *year);
void CL_DateConvertLong(const date_t * date, dateLong_t * dateLong);
int CL_DateCreateDay(const short years, const byte months, const byte days);
int CL_DateCreateSeconds(byte hours, byte minutes, byte seconds);
const char *CL_DateGetMonthName(int month);
void CL_CampaignRun(void);
void CL_UpdateTime(void);
void CL_EnsureValidGameLapseForCombatZoom(void);
void CL_EnsureValidGameLapseForGeoscape(void);
void CL_GameTimeStop(void);
void CL_GameTimeFast(void);
void CL_GameTimeSlow(void);
void CL_SetGameTime(int gameLapseValue);
qboolean CL_NewBase(base_t* base, vec2_t pos);
qboolean CL_NewInstallation(installation_t* installation, installationTemplate_t *installationTemplate, vec2_t pos);
mission_t* CL_AddMission(const char *name);
void CP_RemoveLastMission(void);
void CL_ParseCampaign(const char *name, const char **text);
void CL_UpdateCredits(int credits);
qboolean CL_OnBattlescape(void);
void CL_GameInit(qboolean load);
aircraft_t* AIR_NewAircraft(base_t * base, const char *name);
void CL_ParseAlienTeam(const char *name, const char **text);
void CL_ParseResearchedCampaignItems(const char *name, const char **text);
void CL_ParseResearchableCampaignStates(const char *name, const char **text, qboolean researchable);
void CP_ExecuteMissionTrigger(mission_t * m, qboolean won);
const char* CL_SecondConvert(int second);

void CP_GetRandomPosOnGeoscape(vec2_t pos, qboolean noWater);
qboolean CP_GetRandomPosOnGeoscapeWithParameters(vec2_t pos, const linkedList_t *terrainTypes, const linkedList_t *cultureTypes, const linkedList_t *populationTypes, const linkedList_t *nations);

campaign_t* CL_GetCampaign(const char *name);
void CL_GameExit(void);
void CL_GameAutoGo(mission_t *mission);

/* Mission related functions */
int MAP_GetIdxByMission(const mission_t *mis);
mission_t* MAP_GetMissionByIdx(int id);
int CP_CountMission(void);
int CP_CountMissionActive(void);
int CP_CountMissionOnGeoscape(void);
void CP_UpdateMissionVisibleOnGeoscape(void);
void CP_MissionNotifyBaseDestroyed(const base_t *base);
void CP_MissionNotifyInstallationDestroyed(const installation_t const *installation);
mission_t *CP_GetMissionById(const char *missionId);
const char *CP_MissionToTypeString(const mission_t *mission);
base_t* CP_PositionCloseToBase(const vec2_t pos);
int CP_TerrorMissionAvailableUFOs(const mission_t const *mission, int *ufoTypes);
qboolean AIR_SendAircraftToMission(aircraft_t *aircraft, mission_t *mission);
void AIR_AircraftsNotifyMissionRemoved(const mission_t *mission);
void CP_MissionIsOverByUFO(aircraft_t *ufocraft);
const char* MAP_GetMissionModel(const mission_t *mission);
void CP_UFOProceedMission(aircraft_t *ufocraft);

base_t *CP_GetMissionBase(void);
void CP_SpawnCrashSiteMission(aircraft_t *ufo);
struct alienBase_s;
void CP_SpawnAlienBaseMission(struct alienBase_s *alienBase);

technology_t *CP_IsXVIResearched(void);

#endif /* CLIENT_CL_CAMPAIGN_H */
