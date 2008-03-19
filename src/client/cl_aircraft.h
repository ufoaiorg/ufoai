/**
 * @file cl_aircraft.h
 * @brief Header file for aircraft stuff
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

#ifndef CLIENT_CL_AIRCRAFT_H
#define CLIENT_CL_AIRCRAFT_H

#define MAX_AIRCRAFT	64
#define LINE_MAXSEG 64
#define LINE_MAXPTS (LINE_MAXSEG+2)
#define LINE_DPHI	(M_PI/LINE_MAXSEG)

#define AIRCRAFT_INVALID -1		/**< Invalid aircraft index (global index). */
#define AIRCRAFT_INBASE_INVALID -1	/**< Invalid aircraft index in base-list of aircraft. */

/** @brief A path on the map described by 2D points */
typedef struct mapline_s {
	int numPoints; /**< number of points that make up this path */
	float distance; /**< the distance between two points of the path - total distance is then (distance * (numPoints - 1)) */
	vec2_t point[LINE_MAXPTS]; /**< array of 2D points that make up this path */
} mapline_t;

/** @brief All different types of aircraft. */
typedef enum {
	AIRCRAFT_TRANSPORTER,
	AIRCRAFT_INTERCEPTOR,
	AIRCRAFT_UFO
} aircraftType_t;

#define MAX_HUMAN_AIRCRAFT_TYPE AIRCRAFT_INTERCEPTOR

/** @brief All different size of aircraft. */
typedef enum {
	AIRCRAFT_SMALL = 1,
	AIRCRAFT_LARGE = 2,
} aircraftSize_t;

/** @brief All different Hangar size.
 * @note for Phalanx aircraft and UFO.
 */
typedef enum {
	AIRCRAFT_HANGAR_NONE = 0,
	AIRCRAFT_HANGAR_SMALL = 1,
	AIRCRAFT_HANGAR_BIG = 2,

	AIRCRAFT_HANGAR_ERROR
} aircraftHangarType_t;

/** @brief different weight for aircraft items
 * @note values must go from the lightest to the heaviest item */
typedef enum {
	ITEM_LIGHT,
	ITEM_MEDIUM,
	ITEM_HEAVY
} itemWeight_t;

#define MAX_AIRCRAFTITEMS 64

/** @brief different positions for aircraft items */
typedef enum {
	AIR_NOSE_LEFT,
	AIR_NOSE_CENTER,
	AIR_NOSE_RIGHT,
	AIR_WING_LEFT,
	AIR_WING_RIGHT,
	AIR_REAR_LEFT,
	AIR_REAR_CENTER,
	AIR_REAR_RIGHT,

	AIR_POSITIONS_MAX
} itemPos_t;

#define MAX_AIRCRAFTSLOT 4

/** @brief slot of aircraft */
typedef struct aircraftSlot_s {
	int idx;					/**< self link */
	struct base_s *base;		/**< A link to the base. (if defined by aircraftItemType_t) */
	struct aircraft_s *aircraft;		/**< A link to the aircraft (if defined by aircraftItemType_t). */
	aircraftItemType_t type;	/**< The type of item that can fit in this slot. */

	objDef_t *item;				/**< Item that is currently in the slot. NULL if empty. */
	objDef_t *ammo;				/**< Ammo that is currently in the slot. NULL if empty. */
	itemWeight_t size;			/**< The maximum size (weight) of item that can fit in this slot. */
	int ammoLeft;				/**< The number of ammo left in this slot */
	int delayNextShot;			/**< The delay before the next projectile can be shot */
	int installationTime;		/**< The time (in hours) left before the item is finished to be installed or removed in/from slot
								  *	This is > 0 if the item is being installed, < 0 if the item is being removed, 0 if the item is in place */
	objDef_t *nextItem;			/**< Next item to install when the current item in slot will be removed
								  *	(Should be used only if installationTime is different of 0 */
	itemPos_t pos;				/**< Position of the slot on the aircraft */
} aircraftSlot_t;


/** @brief A cargo of items collected after mission. */
typedef struct itemsTmp_s {
	int idx;		/**< Item idx (csi.ods[idx]). */
	int amount;		/**< Amount of collected items of this idx. */
} itemsTmp_t;

/**
 * @brief All different types of UFOs.
 * @note If you change the order, you have to change the ids in the scriptfiles, too
 * @sa cp_uforecovery <id>
 */
typedef enum {
	UFO_SCOUT,
	UFO_FIGHTER,
	UFO_HARVESTER,
	UFO_CORRUPTER,
	UFO_BOMBER,
	UFO_CARRIER,
	UFO_SUPPLY,

	UFO_MAX
} ufoType_t;

/** @brief An aircraft with all it's data */
typedef struct aircraft_s {
	int idx;			/**< Global index of this aircraft. See also gd.numAircraft and AIRCRAFT_INVALID
						 * this index is also updated when AIR_DeleteAircraft was called
						 * for all the other aircraft.  */
	int idx_sample;			/**< Self-link in aircraft_sample list. */
	char *id;			/**< Internal id from script file. */
	char *name;			/**< Translateable name. */
	char *shortname;		/**< Translateable shortname (being used in small popups). */
	char *image;			/**< Image on geoscape. */
	char *model;
	aircraftType_t type;		/**< Type of aircraft, see aircraftType_t. */
	ufoType_t ufotype;		/**< Type of UFO, see ufoType_t (UFO_MAX if craft is not a UFO). */
	int status;			/**< Status of this aircraft, see aircraftStatus_t. */

	int price;			/**< Price of this aircraft type. */
	int fuel;			/**< Current fuel amount. */
	int maxTeamSize;	/**< Max amount of soldiers onboard. */
	int weight;			/**< "Size" of the aircraft used in capacity calculations. */	/* @todo: rename me to size. */
	vec3_t pos;			/**< Current position on the geoscape. */
	vec3_t direction;		/**< Direction in which the aircraft is going on 3D geoscape (used for smoothed rotation). */
	int point;
	int time;
	int hangar;			/**< This is the baseCapacities_t enum value which says in which hangar this aircraft
						is being parked in (CAP_AIRCRAFTS_SMALL/CAP_AIRCRAFTS_BIG). */

	/* pointer to base->numOnTeam[AIRCRAFT_ID] */
	int teamSize;				/**< How many soldiers/units are on board (i.e. in the craft-team).
						 * @note ATTENTION do not use this in "for" loops or similar.
						 * Entries in teamIdxs and teamTypes are not stored in order and may contain "empties" (see teamIdxs).
						 */
	int teamIdxs[MAX_ACTIVETEAM];	/**< Array of team members on board (global employee idx).
					 * This value is -1 if the entry is unused/empty. (See teamSize) */
	employeeType_t teamTypes[MAX_ACTIVETEAM];	/**< Array of team member types on board (=employee type). */

	aircraftSlot_t weapons[MAX_AIRCRAFTSLOT];	/**< Weapons assigned to aircraft */
	int maxWeapons;					/**< Total number of weapon slots aboard this aircraft (empty or not) */
	aircraftSlot_t shield;			/**< Armour assigned to aircraft (1 maximum) */
	aircraftSlot_t electronics[MAX_AIRCRAFTSLOT];		/**< Electronics assigned to aircraft */
	int maxElectronics;				/**< Total number of electronics slots aboard this aircraft  (empty or not) */

	mapline_t route;
	struct base_s *homebase;		/**< Pointer to homebase for faster access. */
	aliensTmp_t aliencargo[MAX_CARGO];	/**< Cargo of aliens. */
	int alientypes;				/**< How many types of aliens we collected. */
	itemsTmp_t itemcargo[MAX_CARGO];	/**< Cargo of items. */
	int itemtypes;				/**< How many types of items we collected. */

	char *building;		/**< id of the building needed as hangar */

	int numUpgrades;

	struct mission_s* mission;	/**< The mission the aircraft is moving to if this is a PHALANX aircraft
									The mission the UFO is involved if this is a UFO */
	char *missionID;			/**< if this is a crashsite, we need the string here
								 * this is needed because we won't find the ufocrash mission
								 * in the parsed missions in csi.missions until we loaded the campaign */
	struct base_s *baseTarget;		/**< Target of the aircraft. NULL if the target is an aircraft */
	struct aircraft_s *aircraftTarget;		/**< Target of the aircraft (ufo or phalanx) */
	radar_t	radar;			/**< Radar to track ufos */
	int stats[AIR_STATS_MAX];	/**< aircraft parameters for speed, damage and so on */

	technology_t* tech;		/**< link to the aircraft tech */

	qboolean visible;		/**< The ufo is visible ? */
	qboolean notOnGeoscape;	/**< don't let this aircraft ever appear on geoscape (e.g. ufo_carrier) */
} aircraft_t;

/*
@todo: for later, this is used quite a lot in the code.
#define AIRCRAFTCURRENT_IS_SANE(base) (((base)->aircraftCurrent >= 0) && ((base)->aircraftCurrent < (base)->numAircraftInBase))
*/
extern aircraft_t aircraft_samples[MAX_AIRCRAFT]; /**< available aircraft types */
extern int numAircraft_samples;

/* script functions */

#ifdef DEBUG
void AIR_ListAircraft_f(void);
void AIR_ListAircraftSamples_f(void);
#endif

void AIM_AircraftStart_f(void);
void AIR_NewAircraft_f(void);
void AIM_ResetAircraftCvars_f (void);
void AIM_NextAircraft_f(void);
void AIM_PrevAircraft_f(void);
void AIR_AircraftReturnToBase_f(void);

int AIR_GetAircraftIdxInBase(const aircraft_t* aircraft);
const char *AIR_AircraftStatusToName(const aircraft_t *aircraft);
qboolean AIR_IsAircraftInBase(const aircraft_t *aircraft);
void AIR_AircraftSelect(aircraft_t *aircraft);
void AIR_AircraftSelect_f(void);

void AIR_DeleteAircraft(aircraft_t *aircraft);
void AIR_DestroyAircraft(aircraft_t *aircraft);

void AIR_ResetAircraftTeam(aircraft_t *aircraft);
void AIR_AddToAircraftTeam(aircraft_t *aircraft, int employee_idx, employeeType_t employeeType);
void AIR_RemoveFromAircraftTeam(aircraft_t *aircraft, int employee_idx, employeeType_t employeeType);
void AIR_DecreaseAircraftTeamIdxGreaterThan(aircraft_t *aircraft, int employee_idx, employeeType_t employeeType);
qboolean AIR_IsInAircraftTeam(const aircraft_t *aircraft, int employee_idx, employeeType_t employeeType);

void CL_CampaignRunAircraft(int dt);
aircraft_t *AIR_GetAircraft(const char *name);
aircraft_t* AIR_AircraftGetFromIdx(int idx);
objDef_t *AII_GetAircraftItemByID(const char *id);
qboolean AIR_AircraftMakeMove(int dt, aircraft_t* aircraft);
void AIR_ParseAircraft(const char *name, const char **text, qboolean assignAircraftItems);
void AII_ReloadWeapon(aircraft_t *aircraft);
qboolean AIR_AircraftHasEnoughFuel(const aircraft_t *aircraft, const vec2_t destination);
void AIR_AircraftReturnToBase(aircraft_t *aircraft);
qboolean AIR_SendAircraftToMission(aircraft_t* aircraft, struct mission_s* mission);
qboolean AIR_SendAircraftPursuingUFO(aircraft_t* aircraft, aircraft_t* ufo);
void AIR_AircraftsNotifyUFORemoved(const aircraft_t *const ufo);
void AIR_AircraftsUFODisappear(const aircraft_t *const ufo);
void AIR_UpdateHangarCapForAll(struct base_s *base);
qboolean AIR_ScriptSanityCheck(void);
int AIR_CalculateHangarStorage(int aircraftID, struct base_s *base, int used);
int CL_AircraftMenuStatsValues(const int value, const int stat);
int AIR_CountTypeInBase(const struct base_s *base, aircraftType_t aircraftType);
const char *AIR_GetAircraftString(aircraftType_t aircraftType);

#endif
