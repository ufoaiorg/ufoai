/**
 * @file cp_campaign.h
 * @brief Header file for single player campaign control.
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

#ifndef CP_CAMPAIGN_H
#define CP_CAMPAIGN_H

extern memPool_t *cp_campaignPool;

struct aircraft_s;
struct installation_s;
struct employee_s;
struct ugv_s;
struct campaign_s;
struct uiNode_s; /**< @todo remove this once the uiNode_t usage is cleaned up */

#define MAX_CAMPAIGNS	16

#define MAX_ASSEMBLIES	16

/** @todo rename this after merging with savegame breakage branch and also change the value to -1 */
#define	BYTES_NONE	0xFF

#include "cp_rank.h"
#include "cp_save.h"
#include "cp_parse.h"
#include "cp_event.h"
#include "cp_ufopedia.h"
#include "cp_research.h"
#include "cp_radar.h"
#include "cp_aircraft.h"
#include "cp_base.h"
#include "cp_employee.h"
#include "cp_transfer.h"
#include "cp_nation.h"
#include "cp_installation.h"
#include "cp_produce.h"
#include "cp_uforecovery.h"
#include "cp_airfight.h"
#include "cp_messageoptions.h"
#include "cp_alienbase.h"
#include "cp_market.h"
#include "cp_statistics.h"

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
#define HAPPINESS_ALIEN_MISSION_LOSS		-0.02
#define HAPPINESS_UFO_SALE_GAIN				0.02
#define HAPPINESS_UFO_SALE_LOSS				0.005
#define HAPPINESS_MAX_MISSION_IMPACT		0.07

/* Maximum alien groups per alien team category */
#define MAX_ALIEN_GROUP_PER_CATEGORY	4
/* Maximum alien team category defined in scripts */
#define ALIENCATEGORY_MAX	8
#define BID_FACTOR 0.9
#define MAX_PROJECTILESONGEOSCAPE 32

/**
 * @brief The amount of time (in hours) it takes for the interest to increase by 1. Is later affected by difficulty.
 */
#define HOURS_PER_ONE_INTEREST				22

/**
 * @brief Determines the interest interval for a single campaign
 */
#define INITIAL_OVERALL_INTEREST 			20
#define FINAL_OVERALL_INTEREST 				1000

/**
 * @brief The length of a single mission spawn cycle
 */
#define DELAY_BETWEEN_MISSION_SPAWNING 		8

/**
 * @brief The minimum and maximum amount of missions per mission cycle.
 * @note some of the missions can be non-occurrence missions.
 */
#define MINIMUM_MISSIONS_PER_CYCLE 			5
#define MAXIMUM_MISSIONS_PER_CYCLE 			25

/**
 * @brief The probability that any new alien mission will be a non-occurrence mission.
 */
#define NON_OCCURRENCE_PROBABILITY 			0.65

/** possible map types */
typedef enum mapType_s {
	MAPTYPE_TERRAIN,
	MAPTYPE_CULTURE,
	MAPTYPE_POPULATION,
	MAPTYPE_NATIONS,

	MAPTYPE_MAX
} mapType_t;

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
	INTERESTCATEGORY_ALIENBASE,		/**< Alien base already built on earth
									 * @note This is not a mission alien can undertake, but the result of
									 * INTERESTCATEGORY_BUILDING */
	INTERESTCATEGORY_RESCUE,

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

/** @brief alien team group definition.
 * @note This is the definition of one groups of aliens (several races) that can
 * be used on the same map.
 * @sa alienTeamCategory_s
 */
typedef struct alienTeamGroup_s {
	int idx;			/**< idx of the group in the alien team category */
	int categoryIdx;	/**< idx of category it's used in */
	int minInterest;	/**< Minimum interest value this group should be used with. */
	int maxInterest;	/**< Maximum interest value this group should be used with. */

	const teamDef_t *alienTeams[MAX_TEAMS_PER_MISSION];	/**< different alien teams available
													 * that will be used in mission */
	int numAlienTeams;		/**< Number of alienTeams defined in this group. */
} alienTeamGroup_t;

/** @brief alien team category definition
 * @note This is the definition of all groups of aliens that can be used for
 * a mission category
 * @sa alienTeamGroup_s
 */
typedef struct alienTeamCategory_s {
	char id[MAX_VAR];			/**< id of the category */
	interestCategory_t missionCategories[INTERESTCATEGORY_MAX];		/**< Mission category that should use this
															 * alien team Category. */
	int numMissionCategories;					/**< Number of category using this alien team Category. */

	linkedList_t *equipment;		/**< Equipment definitions that may be used for this def. */

	alienTeamGroup_t alienTeamGroups[MAX_ALIEN_GROUP_PER_CATEGORY];		/**< Different alien group available
																 * for this category */
	int numAlienTeamGroups;		/**< Number of alien group defined for this category */
} alienTeamCategory_t;

/** @brief mission definition
 * @note A mission is different from a map: a mission is the whole set of actions aliens will carry.
 * For example, coming with a UFO on earth, land, explore earth, and leave with UFO
 */
typedef struct mission_s {
	int idx;						/**< unique id of this mission */
	char id[MAX_VAR];				/**< script id */
	mapDef_t* mapDef;				/**< mapDef used for this mission */
	qboolean active;				/**< aircraft at place? */
	union missionData_t {
		base_t *base;
		aircraft_t *aircraft;
		installation_t *installation;
		alienBase_t *alienBase;
	} data;							/**< may be related to mission type (like pointer to base attacked, or to alien base) */
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
	qboolean crashed;				/**< is UFO crashed ? (only used if mission is spawned from a UFO */

	char onwin[MAX_VAR];			/**< trigger command after you've won a battle, @sa CP_ExecuteMissionTrigger */
	char onlose[MAX_VAR];			/**< trigger command after you've lost a battle, @sa CP_ExecuteMissionTrigger */
	qboolean posAssigned;			/**< is the position of this mission already set? */
} mission_t;

/**
 * @brief iterates through missions
 */
#define MIS_Foreach(var) LIST_Foreach(ccs.missions, mission_t, var)

/** battlescape parameters that were used */
typedef struct battleParam_s {
	mission_t *mission;
	alienTeamGroup_t *alienTeamGroup;	/**< Races of aliens present in battle */
	char *param;					/**< in case of a random map assembly we can't use the param from mapDef - because
									 * this is global for the mapDef - but we need a local mission param */
	char alienEquipment[MAX_VAR];					/**< Equipment of alien team */
	char civTeam[MAX_VAR];							/**< Type of civilian (European, ...) */
	qboolean day;									/**< Mission is played during day */
	const char *zoneType;							/**< Terrain type (used for texture replacement in some missions (base, ufocrash)) */
	int aliens, civilians;			/**< number of aliens and civilians in that particular mission */
	struct nation_s *nation;		/**< nation where the mission takes place */
} battleParam_t;

/** @brief Structure with mission info needed to create results summary at menu won. */
typedef struct missionResults_s {
	const mission_t *mission;
	qboolean won;
	qboolean recovery;		/**< @c true if player secured a UFO (landed or crashed). */
	qboolean crashsite;		/**< @c true if secured UFO was crashed one. */
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

/** salary values for a campaign */
typedef struct salary_s {
	int base[MAX_EMPL];
	int rankBonus[MAX_EMPL];
	int admin[MAX_EMPL];
	int aircraftFactor;
	int aircraftDivisor;
	int baseUpkeep;
	int adminInitial;
	float debtInterest;
} salary_t;

/** campaign definition */
typedef struct campaign_s {
	int idx;					/**< own index in global campaign array */
	char id[MAX_VAR];			/**< id of the campaign */
	char name[MAX_VAR];			/**< name of the campaign */
	int team;					/**< what team can play this campaign */
	char researched[MAX_VAR];	/**< name of the researched tech list to use on campaign start */
	char soldierEquipment[MAX_VAR];	/**< name of the equipment list that is used to equip soldiers on crafts that are added to the first base */
	char equipment[MAX_VAR];	/**< name of the equipment list to use on campaign start */
	char market[MAX_VAR];		/**< name of the market list containing initial items on market */
	char asymptoticMarket[MAX_VAR];		/**< name of the market list containing items on market at the end of the game */
	const equipDef_t *marketDef;		/**< market definition for this campaign (how many items on the market) containing initial items */
	const equipDef_t *asymptoticMarketDef;	/**< market definition for this campaign (how many items on the market) containing finale items */
	char text[MAX_VAR];			/**< placeholder for gettext stuff */
	char map[MAX_VAR];			/**< geoscape map */
	int soldiers;				/**< start with x soldiers */
	int scientists;				/**< start with x scientists */
	int workers;				/**< start with x workers */
	int pilots;					/**< start with x pilots */
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
	const campaignEvents_t *events;
	salary_t salaries;
} campaign_t;

int CP_GetSalaryBaseEmployee(const salary_t *salary, employeeType_t type);
int CP_GetSalaryAdminEmployee(const salary_t *salary, employeeType_t type);
int CP_GetSalaryRankBonusEmployee(const salary_t *salary, employeeType_t type);
int CP_GetSalaryAdministrative(const salary_t *salary);
int CP_GetSalaryUpKeepBase(const salary_t *salary, const base_t *base);

/** possible geoscape actions */
typedef enum mapAction_s {
	MA_NONE,
	MA_NEWBASE,				/**< build a new base */
	MA_NEWINSTALLATION,		/**< build a new installation */
	MA_INTERCEPT,			/**< intercept */
	MA_BASEATTACK,			/**< base attacking */
	MA_UFORADAR				/**< ufos are in our radar */
} mapAction_t;

typedef void (*missionSpawnFunction_t) (void);
typedef void (*missionResultFunction_t) (const missionResults_t *results);

/**
 * @brief client campaign structure
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

	int credits;			/**< actual credits amount */
	int civiliansKilled;	/**< how many civilians were killed already */
	int aliensKilled;		/**< how many aliens were killed already */
	date_t date;			/**< current date */
	qboolean XVIShowMap;			/**< means that PHALANX has a map of XVI - @see CP_IsXVIResearched */
	qboolean breathingMailSent;		/**< status flag indicating that mail about died aliens due to missing breathing tech was sent */
	float timer;
	float frametime;

	vec3_t angles;			/**< 3d geoscape rotation */
	vec2_t center;			/**< latitude and longitude of the point we're looking at on earth */
	float zoom;				/**< zoom used when looking at earth */

	/* Smoothing variables */
	qboolean smoothRotation;	/**< @c true if the rotation of 3D geoscape must me smooth */
	vec3_t smoothFinalGlobeAngle;	/**< value of final ccs.angles for a smooth change of angle (see MAP_CenterOnPoint)*/
	vec2_t smoothFinal2DGeoscapeCenter;		/**< value of ccs.center for a smooth change of position (see MAP_CenterOnPoint) */
	float smoothDeltaLength;	/**< angle/position difference that we need to change when smoothing */
	float smoothFinalZoom;		/**< value of final ccs.zoom for a smooth change of angle (see MAP_CenterOnPoint)*/
	float smoothDeltaZoom;		/**< zoom difference that we need to change when smoothing */
	float curZoomSpeed;			/**< The current zooming speed. Used for smooth zooming. */
	float curRotationSpeed;		/**< The current rotation speed. Used for smooth rotating.*/

	struct {
		mission_t *selectedMission;			/**< Currently selected mission on geoscape */
		aircraft_t *selectedAircraft;		/**< Currently selected aircraft on geoscape */
		aircraft_t *selectedUFO;			/**< Currently selected UFO on geoscape */
		aircraft_t *interceptAircraft;		/**< selected aircraft for interceptions */
		aircraft_t *missionAircraft;		/**< aircraft pointer for mission handling */
	} geoscape;

	/* == misc == */
	/* MA_NEWBASE, MA_INTERCEPT, MA_BASEATTACK, ... */
	mapAction_t mapAction;

	/** @todo move into the base node extra data */
	/* BA_NEWBUILDING ... */
	baseAction_t baseAction;

	/* how fast the game is running */
	int gameTimeScale;
	int gameLapse;

	/* already paid in this month? */
	qboolean paid;

	/** Coordinates to place the new base at (long, lat) */
	vec2_t newBasePos;

	/* == employees == */
	/* A list of all phalanx employees (soldiers, scientists, workers, etc...) */
	linkedList_t *employees[MAX_EMPL];

	/* == technologies == */
	/* A list of all research-topics resp. the research-tree. */
	technology_t technologies[MAX_TECHNOLOGIES];
	/* Total number of technologies. */
	int numTechnologies;

	/* == bases == */
	/* A list of _all_ bases ... even unbuilt ones. */
	base_t bases[MAX_BASES];
	/* Total number of built bases (how many are enabled). */
	int numBases;

	/* a list of all templates for building bases */
	baseTemplate_t baseTemplates[MAX_BASETEMPLATES];
	int numBaseTemplates;

	/* == aircraft == */
	linkedList_t *aircraft;

	/* == Alien bases == */
	linkedList_t *alienBases;

	/* == Nations == */
	nation_t nations[MAX_NATIONS];
	int numNations;

	/* == Cities == */
	linkedList_t *cities;
	int numCities;

	/* Projectiles on geoscape (during fights) */
	aircraftProjectile_t projectiles[MAX_PROJECTILESONGEOSCAPE];
	int numProjectiles;

	/* == Transfers == */
	linkedList_t* transfers;

	/* UFO components. */
	int numComponents;
	components_t components[MAX_ASSEMBLIES];

	/* == stored UFOs == */
	linkedList_t *storedUFOs;

	/* Alien Team Definitions. */
	teamDef_t *alienTeams[MAX_TEAMDEFS];
	int numAliensTD;

	/* Alien Team Package used during battle */
	alienTeamCategory_t alienCategories[ALIENCATEGORY_MAX];	/**< different alien team available
														 * that will be used in mission */
	int numAlienCategories;		/** number of alien team categories defined */

	/* == ufopedia == */
	/* A list of all UFOpaedia chapters. */
	pediaChapter_t upChapters[MAX_PEDIACHAPTERS];
	/* Total number of UFOpaedia chapters */
	int numChapters;
	int numUnreadMails; /**< only for faster access (don't cycle all techs every frame) */

	eventMail_t eventMails[MAX_EVENTMAILS];	/**< holds all event mails (cl_event.c) */
	int numEventMails;	/**< how many eventmails (script-id: mail) parsed */

	campaignEvents_t campaignEvents[MAX_CAMPAIGNS];	/**< holds all campaign events (cl_event.c) */
	int numCampaignEventDefinitions;	/**< how many event definitions (script-id: events) parsed */

	/* == buildings in bases == */
	/* A list of all possible unique buildings. */
	building_t buildingTemplates[MAX_BUILDINGS];
	int numBuildingTemplates;
	/*  A list of the building-list per base. (new buildings in a base get copied from buildingTypes) */
	building_t buildings[MAX_BASES][MAX_BUILDINGS];
	/* Total number of buildings per base. */
	int numBuildings[MAX_BASES];

	/* == installations == */
	/* A template for each possible installation with configurable values */
	installationTemplate_t installationTemplates[MAX_INSTALLATION_TEMPLATES];
	int numInstallationTemplates;

	/* A list of _all_ installations */
	linkedList_t* installations;

	/* UFOs on geoscape */
	aircraft_t ufos[MAX_UFOONGEOSCAPE];
	int numUFOs;	/**< The current amount of UFOS on the geoscape. */

	/* message categories */
	msgCategory_t messageCategories[MAX_MESSAGECATEGORIES];
	int numMsgCategories;

	/* entries for message categories */
	msgCategoryEntry_t msgCategoryEntries[NT_NUM_NOTIFYTYPE + MAX_MESSAGECATEGORIES];
	int numMsgCategoryEntries;

	/* == Ranks == */
	/* Global list of all ranks defined in medals.ufo. */
	rank_t ranks[MAX_RANKS];
	/* The number of entries in the list above. */
	int numRanks;

	/* cache for techdef technologies */
	technology_t *teamDefTechs[MAX_TEAMDEFS];

	/* cache for item technologies */
	technology_t *objDefTechs[MAX_OBJDEFS];

	campaign_t *curCampaign;			/**< Current running campaign */
	stats_t campaignStats;
	missionResults_t missionResults;

	campaign_t campaigns[MAX_CAMPAIGNS];
	int numCampaigns;

	aircraft_t aircraftTemplates[MAX_AIRCRAFT];		/**< Available aircraft types/templates/samples. */
	int numAircraftTemplates;		/**< Number of aircraft templates. */

	missionSpawnFunction_t missionSpawnCallback;
	missionResultFunction_t missionResultCallback;
} ccs_t;

typedef struct {
	int x, y;
} screenPoint_t;

extern ccs_t ccs;
extern const int DETECTION_INTERVAL;
extern cvar_t *cp_campaign;
extern cvar_t *cp_missiontest;
extern cvar_t *cp_start_employees;

#define MAX_CREDITS 10000000

#include "../cgame.h"
extern const cgame_import_t *cgi;

/* Campaign functions */
void CP_InitStartup(const struct cgame_import_s *import);
campaign_t* CP_GetCampaign(const char *name);
void CP_CampaignInit(campaign_t *campaign, qboolean load);
void CP_ParseCampaignData(void);
void CP_ReadCampaignData(const campaign_t *campaign);
qboolean CP_IsRunning(void);

void CP_CampaignRun(campaign_t *campaign, float secondsSinceLastFrame);
void CP_CheckLostCondition(const campaign_t *campaign);
void CP_EndCampaign(qboolean won);

void CP_Shutdown(void);
void CP_ResetCampaignData(void);

/* Mission related functions */
int CP_CountMissionOnGeoscape(void);
void CP_UpdateMissionVisibleOnGeoscape(void);
int CP_TerrorMissionAvailableUFOs(const mission_t const *mission, ufoType_t *ufoTypes);
qboolean AIR_SendAircraftToMission(aircraft_t *aircraft, mission_t *mission);
void AIR_AircraftsNotifyMissionRemoved(const mission_t *mission);

void CP_UFOProceedMission(const campaign_t* campaign, aircraft_t *ufocraft);
mission_t *CP_CreateNewMission(interestCategory_t category, qboolean beginNow);
qboolean CP_ChooseMap(mission_t *mission, const vec2_t pos);
void CP_StartSelectedMission(void);

void CP_HandleNationData(float minHappiness, mission_t * mis, const nation_t *nation, const missionResults_t *results);
void CP_UpdateCharacterStats(const base_t *base, const aircraft_t *aircraft);

/* Credits management */
qboolean CP_CheckCredits (int costs);
void CP_UpdateCredits(int credits);

/* Other functions */
void CP_ParseCharacterData(struct dbuffer *msg);
qboolean CP_CheckNextStageDestination(const campaign_t* campaign, aircraft_t *ufo);

const city_t * CP_GetCity(const char *id);

aircraft_t* AIR_NewAircraft(base_t * base, const aircraft_t *aircraftTemplate);

void CP_GetRandomPosOnGeoscape(vec2_t pos, qboolean noWater);
qboolean CP_GetRandomPosOnGeoscapeWithParameters(vec2_t pos, const linkedList_t *terrainTypes, const linkedList_t *cultureTypes, const linkedList_t *populationTypes, const linkedList_t *nations);

void CP_GameAutoGo(mission_t *mission, aircraft_t *aircraft, const campaign_t *campaign, const battleParam_t *battleParameters, missionResults_t *results);

qboolean CP_OnGeoscape(void);

#endif /* CP_CAMPAIGN_H */
