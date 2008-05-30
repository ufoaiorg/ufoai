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

/* Time Constants */
#define DAYS_PER_YEAR 365
#define DAYS_PER_YEAR_AVG 365.25
#define MONTHS_PER_YEAR 12

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

/** possible map types */
typedef enum mapType_s {
	MAPTYPE_TERRAIN,
	MAPTYPE_CULTURE,
	MAPTYPE_POPULATION,
	MAPTYPE_NATIONS,

	MAPTYPE_MAX
} mapType_t;

/** possible alien teams type */
typedef enum alienTeamType_s {
	ALIENTEAM_DEFAULT,
	ALIENTEAM_XVI,
	ALIENTEAM_HARVEST,

	ALIENTEAM_MAX
} alienTeamType_t;

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
	STAGE_INTERCEPT,				/**< UFO attacks any encountered PHALANX aircraft */
	STAGE_BASE_DISCOVERED,			/**< PHALANX discovered the base */
	STAGE_HARVEST,					/**< Harvesting */

	STAGE_RETURN_TO_ORBIT,			/**< UFO is going back to base */

	STAGE_OVER						/**< Mission is over */
} missionStage_t;

/** mission definition */
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

	/** @todo I'm not sure this is really needed still -- Kracken */
	/** @note Don't end with ; - the trigger commands get the base index as
	 * parameter - see CP_ExecuteMissionTrigger - If you don't need the base index
	 * in your trigger command, you can seperate more commands with ; of course */
	char onwin[MAX_VAR];		/**< trigger command after you've won a battle */
	char onlose[MAX_VAR];		/**< trigger command after you've lost a battle */
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

/* UFO Recoveries stuff. */
#define MAX_RECOVERIES 32

/** @brief Structure of UFO recoveries (all of them). */
typedef struct ufoRecoveries_s {
	qboolean active;		/**< True if the recovery is under processing. */
	base_t *base;			/**< Base where the recovery will be processing. */
	aircraft_t *ufotype;		/**< Entry of UFO in aircraft_samples array. */
	date_t event;			/**< When the process will start (UFO got transported to base). */
} ufoRecoveries_t;

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
	char market[MAX_VAR];		/**< name of the market list to use with this campaign */
	equipDef_t *marketDef;		/**< market definition for this campaign (how many items on the market) */
	char text[MAX_VAR];			/**< placeholder for gettext stuff */
	char map[MAX_VAR];			/**< geoscape map */
	int soldiers;				/**< start with x soldiers */
	int scientists;				/**< start with x scientists */
	int workers;				/**< start with x workers */
	int medics;					/**< start with x medics */
	int ugvs;					/**< start with x ugvs (robots) */
	int credits;				/**< start with x credits */
	int num;
	signed int difficulty;		/**< difficulty level -4 - 4 */
	float minhappiness;			/**< minimum value of mean happiness before the game is lost */
	int negativeCreditsUntilLost;	/**< bankrupt - negative credits until you've lost the game */
	int maxAllowedXVIRateUntilLost;	/**< 0 - 100 - the average rate of XVI over all nations before you've lost the game */
	qboolean visible;			/**< visible in campaign menu? */
	date_t date;				/**< starting date for this campaign */
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
	int medic_base;
	int medic_rankbonus;
	int robot_base;
	int robot_rankbonus;
	int aircraft_factor;
	int aircraft_divisor;
	int base_upkeep;
	int admin_initial;
	int admin_soldier;
	int admin_worker;
	int admin_scientist;
	int admin_medic;
	int admin_robot;
	float debt_interest;
} salary_t;

#define SALARY_SOLDIER_BASE salaries[curCampaign->idx].soldier_base
#define SALARY_SOLDIER_RANKBONUS salaries[curCampaign->idx].soldier_rankbonus
#define SALARY_WORKER_BASE salaries[curCampaign->idx].worker_base
#define SALARY_WORKER_RANKBONUS salaries[curCampaign->idx].worker_rankbonus
#define SALARY_SCIENTIST_BASE salaries[curCampaign->idx].scientist_base
#define SALARY_SCIENTIST_RANKBONUS salaries[curCampaign->idx].scientist_rankbonus
#define SALARY_MEDIC_BASE salaries[curCampaign->idx].medic_base
#define SALARY_MEDIC_RANKBONUS salaries[curCampaign->idx].medic_rankbonus
#define SALARY_ROBOT_BASE salaries[curCampaign->idx].robot_base
#define SALARY_ROBOT_RANKBONUS salaries[curCampaign->idx].robot_rankbonus
#define SALARY_AIRCRAFT_FACTOR salaries[curCampaign->idx].aircraft_factor
#define SALARY_AIRCRAFT_DIVISOR salaries[curCampaign->idx].aircraft_divisor
#define SALARY_BASE_UPKEEP salaries[curCampaign->idx].base_upkeep
#define SALARY_ADMIN_INITIAL salaries[curCampaign->idx].admin_initial
#define SALARY_ADMIN_SOLDIER salaries[curCampaign->idx].admin_soldier
#define SALARY_ADMIN_WORKER salaries[curCampaign->idx].admin_worker
#define SALARY_ADMIN_SCIENTIST salaries[curCampaign->idx].admin_scientist
#define SALARY_ADMIN_MEDIC salaries[curCampaign->idx].admin_medic
#define SALARY_ADMIN_ROBOT salaries[curCampaign->idx].admin_robot
#define SALARY_DEBT_INTEREST salaries[curCampaign->idx].debt_interest

/** market structure */
typedef struct market_s {
	int num[MAX_OBJDEFS];
	int bid[MAX_OBJDEFS];
	int ask[MAX_OBJDEFS];
	double cumm_supp_diff[MAX_OBJDEFS];
} market_t;

/**
 * @brief Detailed information about the nation relationship (currently per month, but could be used elsewhere).
 * @todo Maybe we should also move the "funding" stuff (see nation_t) in here? It is static right now though so i see no reason to do that yet.
 */
typedef struct nationInfo_s {
	qboolean	inuse;	/**< Is this entry used? */

	/* Relationship */
	float happiness;	/**< percentage (0.00 - 1.00) of how the nation appreciates PHALANX. 1.00 is the maximum happiness */
	int xviInfection;	/**< Increase by one each time a XVI spread is done in this nation. */
	float alienFriendly;	/**< How friendly is this nation towards the aliens. (percentage 0.00 - 1.00)
				 * @todo Check if this is still needed after XVI factors are in.
				 * Pro: People can be alien-friendly without being affected after all.
				 * Con: ?
				 */
} nationInfo_t;

#define MAX_NATION_BORDERS 64
/**
 * @brief Nation definition
 */
typedef struct nation_s {
	char *id;		/**< Unique ID of this nation. */
	char *name;		/**< Full name of the nation. */

	vec4_t color;		/**< The color this nation uses in the color-coded earth-map */
	vec2_t pos;		/**< Nation name position on geoscape. */

	nationInfo_t stats[MONTHS_PER_YEAR];	/**< Detailed information about the history of this nations relationship toward PHALANX and the aliens.
									 * The first entry [0] is the current month - all following entries are stored older months.
									 * Combined with the funding info below we can generate an overview over time.
									 */

	/* Funding */
	int maxFunding;		/**< How many (monthly) credits. */
	int maxSoldiers;	/**< How many (monthly) soldiers. */
	int maxScientists;	/**< How many (monthly) scientists. */
} nation_t;

#define MAX_NATIONS 8

/**
 * @brief client structure
 * @sa csi_t
 */
typedef struct ccs_s {
	equipDef_t eMission;
	market_t eMarket;

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
	qboolean humansAttackActivated;	/**< humans start to attack player */
	float timer;

	vec3_t angles;			/**< 3d geoscape rotation */
	vec2_t center;
	float zoom;

	/** governs zero build time for first base if empty base option chosen */
	int instant_build;
} ccs_t;

/** possible geoscape actions */
typedef enum mapAction_s {
	MA_NONE,
	MA_NEWBASE,				/**< build a new base */
	MA_INTERCEPT,			/**< intercept */
	MA_BASEATTACK,			/**< base attacking */
	MA_UFORADAR				/**< ufos are in our radar */
} mapAction_t;

/** possible aircraft states */
typedef enum aircraftStatus_s {
	AIR_NONE = 0,
	AIR_REFUEL,				/**< refill fuel */
	AIR_HOME,				/**< in homebase */
	AIR_IDLE,				/**< just sit there on geoscape */
	AIR_TRANSIT,			/**< moving */
	AIR_MISSION,			/**< moving to a mission */
	AIR_UFO,				/**< pursuing a UFO - also used for ufos that are pursuing an aircraft */
	AIR_DROP,				/**< ready to drop down */
	AIR_INTERCEPT,			/**< ready to intercept */
	AIR_TRANSFER,			/**< being transfered */
	AIR_RETURNING			/**< returning to homebase */
} aircraftStatus_t;

extern mission_t *selectedMission;
extern campaign_t *curCampaign;
extern ccs_t ccs;

void AIR_SaveAircraft(sizebuf_t * sb, base_t * base);
void AIR_LoadAircraft(sizebuf_t * sb, base_t * base, int version);

void CP_CheckNextStageDestination(aircraft_t *ufo);

void CL_ResetCampaign(void);
void CL_ResetSinglePlayerData(void);
void CL_DateConvert(const date_t * date, int *day, int *month);
const char *CL_DateGetMonthName(int month);
void CL_CampaignRun(void);
void CL_GameTimeStop(void);
void CL_GameTimeFast(void);
void CL_GameTimeSlow(void);
qboolean CL_NewBase(base_t* base, vec2_t pos);
void CL_ParseMission(const char *name, const char **text);
mission_t* CL_AddMission(const char *name);
void CP_RemoveLastMission(void);
void CL_ParseCampaign(const char *name, const char **text);
void CL_ParseNations(const char *name, const char **text);
void CL_UpdateCredits(int credits);
qboolean CL_OnBattlescape(void);
void CL_GameInit(qboolean load);
aircraft_t* AIR_NewAircraft(base_t * base, const char *name);
void CL_ParseAlienTeam(const char *name, const char **text);
void CL_ParseResearchedCampaignItems(const char *name, const char **text);
void CL_ParseResearchableCampaignStates(const char *name, const char **text, qboolean researchable);
void CP_ExecuteMissionTrigger(mission_t * m, qboolean won);
const char* CL_SecondConvert(int second);

nation_t *CL_GetNationByID(const char *nationID);

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
void CP_MissionNotifyBaseDestroyed(base_t *base);
mission_t *CP_GetMissionById(const char *missionId);
const char *CP_MissionToTypeString(const mission_t *mission);
base_t* CP_PositionCloseToBase(const vec2_t pos);
qboolean AIR_SendAircraftToMission(aircraft_t *aircraft, mission_t *mission);
void AIR_AircraftsNotifyMissionRemoved(const mission_t *mission);
void CP_MissionIsOverByUfo(aircraft_t *ufocraft);
const char* MAP_GetMissionModel(const mission_t *mission);
void CP_UFOProceedMission(aircraft_t *ufocraft);

base_t *CP_GetMissionBase(void);
void CP_SpawnCrashSiteMission(aircraft_t *ufo);
struct alienBase_s;
void CP_SpawnAlienBaseMission(struct alienBase_s *alienBase);
void CP_UFOSendMail(const aircraft_t *ufocraft, const base_t *base);

technology_t *CP_IsXVIResearched(void);

#endif /* CLIENT_CL_CAMPAIGN_H */
