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

/** @brief All different types of craft items. */
typedef enum {
	AC_ITEM_WEAPON,
	AC_ITEM_AMMO,
	AC_ITEM_SHIELD,
	AC_ITEM_ELECTRONICS,

	MAX_ACITEMS
} aircraftItemType_t;

/**
 * @brief Aircraft parameters.
 * @note This is a list of all aircraft parameters that depends on aircraft items.
 *  those values doesn't change with shield or weapon assigned to aircraft
 * @note AIR_STATS_WRANGE must be the last parameter (see AII_UpdateAircraftStats)
 */
typedef enum {
	AIR_STATS_SPEED,	/**< Aircraft speed. */
	AIR_STATS_SHIELD,	/**< Aircraft shield. */
	AIR_STATS_ECM,		/**< Aircraft electronic warfare level. */
	AIR_STATS_DAMAGE,	/**< Aircraft damage points (= hit points of the aircraft). */
	AIR_STATS_ACCURACY,	/**< Aircraft accuracy - base accuracy (without weapon). */
	AIR_STATS_FUELSIZE,	/**< Aircraft fuel capacity. */
	AIR_STATS_WRANGE,	/**< Aircraft weapon range - the maximum distance aircraft can open fire. */

	AIR_STATS_MAX
} aircraftParams_t;

/** @brief different weight for aircraft items
 * @note values must go from the lightest to the heaviest item */
typedef enum {
	ITEM_LIGHT,
	ITEM_MEDIUM,
	ITEM_HEAVY
} itemWeight_t;

struct actMis_s;

#define MAX_AIRCRAFTITEMS 64

/** @brief parsed craftitem from script files */
typedef struct aircraftItem_s {
	char *id;			/**< from script files */
	int idx;					/**< self link */
	aircraftItemType_t type;		/**< The type of the aircraft item. */
	char *tech;		/**< tech id for this item.*/
	int tech_idx;				/**< tech index for this item.*/
	char *weapon;		/**< if this is ammo there must be a weapon */
	float stats[AIR_STATS_MAX];	/**< All coefficient that can affect aircraft->stats */
	itemWeight_t itemWeight;	/**< The weight of the item (which must be smaller that slot size) */
	float weaponDamage;			/**< The base damage inflicted by an ammo */
	float weaponSpeed;			/**< The speed of the projectile on geoscape */
	float weaponDelay;			/**< The minimum delay between 2 shots */
	int ammo;					/**< The total number of ammo that can be fired */
	int installationTime;		/**< The time needed to install/remove the item on an aircraft */
	int price;
} aircraftItem_t;

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
	int aircraftIdx;			/**< Global index of this aircraft. See also gd.numAircraft. */
	aircraftItemType_t type;	/**< The type of item that can fit in this slot. */
	int itemIdx;				/**< The index in aircraftItems[] of item that is currently in the slot. -1 if empty */
	int ammoIdx;				/**< The index in aircraftItems[] of the ammo that is currently in the slot. -1 if empty */
	itemWeight_t size;			/**< The maximum size (weight) of item that can fit in this slot. */
	int ammoLeft;				/**< The number of ammo left in this slot */
	int delayNextShot;			/**< The delay before the next projectile can be shot */
	int installationTime;		/**< The time (in hours) left before the item is finished to be installed or removed in/from slot
								  *	This is > 0 if the item is being installed, < 0 if the item is being removed, 0 if the item is in place */
	int nextItemIdx;			/**< Indice in aircraftItems[] of the next item to install when the current item in slot will be removed
								  *	(Should be used only if installationTime is different of 0 */
	itemPos_t pos;				/**< Position of the slot on the aircraft */
} aircraftSlot_t;


/** @brief A cargo of items collected after mission. */
typedef struct itemsTmp_s {
	int idx;		/**< Item idx (csi.ods[idx]). */
	int amount;		/**< Amount of collected items of this idx. */
} itemsTmp_t;

/** @brief All different types of UFOs. */
typedef enum {
	UFO_SCOUT,
	UFO_FIGHTER,
	UFO_HARVESTER,
	UFO_CONDOR,
	UFO_CARRIER,
	UFO_SUPPLY,

	UFO_MAX
} ufoType_t;

/** @brief An aircraft with all it's data */
typedef struct aircraft_s {
	int idx;					/**< Global index of this aircraft. See also gd.numAircraft. @todo: is this updated when one aircraft is lost (it is checked agains gd.numAircraft sometimes)? We do not really have a global list of acs do we? */
	int idx_sample;				/**< self-link in aircraft_sample list */
	char *id;				/**< internal id from script file */
	char *name;				/**< translateable name */
	char *shortname;		/**< translateable shortname */
	char *image;			/**< image on geoscape */
	char *model;
	aircraftType_t type;
	ufoType_t ufotype;			/**< type of UFO (if craft is not UFO - not used) */
	int status;				/**< see aircraftStatus_t */
	vec3_t angles;				/**< menu values for rotating */
	vec3_t scale;				/**< menu values for scaling */
	vec3_t center;				/**< menu values for shifting */
	vec3_t anglesEquip;			/**< menu values for rotating - aircraft_equip menu */
	vec3_t scaleEquip;			/**< menu values for scaling - aircraft_equip menu */
	vec3_t centerEquip;			/**< menu values for shifting - aircraft_equip menu */
	int price;
	int fuel;				/**< actual fuel */
	int size;				/**< how many soldiers max */
	int weight;				/**< "Size" of the aircraft used in capacity calculations. */
	vec3_t pos;				/**< actual pos on geoscape */
	vec3_t direction;		/**< direction in which the aircraft is going on 3D geoscape (used for smoothed rotation)*/
	int point;
	int time;
	int idxInBase;				/**< Index in base. See also base_t->numAircraftInBase. */
	/* pointer to base->numOnTeam[AIRCRAFT_ID] */
	int *teamSize;				/**< how many soldiers on board */
	int teamIdxs[MAX_ACTIVETEAM];	/**< array of team members on board employee idx*/

	aircraftSlot_t weapons[MAX_AIRCRAFTSLOT];	/**< Weapons assigned to aircraft */
	int maxWeapons;					/**< Total number of weapon slots aboard this aircraft */
	aircraftSlot_t shield;			/**< Armour assigned to aircraft (1 maximum) */
	aircraftSlot_t electronics[MAX_AIRCRAFTSLOT];		/**< Electronics assigned to aircraft */
	int maxElectronics;				/**< Total number of electronics slots aboard this aircraft */

	mapline_t route;
	struct base_s *homebase;		/**< Pointer to homebase for faster access. */
	aliensTmp_t aliencargo[MAX_CARGO];	/**< Cargo of aliens. */
	int alientypes;				/**< How many types of aliens we collected. */
	itemsTmp_t itemcargo[MAX_CARGO];	/**< Cargo of items. */
	int itemtypes;				/**< How many types of items we collected. */

	char *building;		/**< id of the building needed as hangar */

	int numUpgrades;

	struct actMis_s* mission;	/**< The mission the aircraft is moving to */
	int baseTargetIdx;		/**< Target of the aircraft. -1 if the target is an aircraft */
	struct aircraft_s *target;		/**< Target of the aircraft (ufo or phalanx) */
	radar_t	radar;			/**< Radar to track ufos */
	int stats[AIR_STATS_MAX];	/**< aircraft parameters for speed, damage and so on */

	qboolean visible;		/**< The ufo is visible ? */
} aircraft_t;

/*
@todo: for later, this is used quite a lot in the code.
#define AIRCRAFTCURRENT_IS_SANE(base) (((base)->aircraftCurrent >= 0) && ((base)->aircraftCurrent < (base)->numAircraftInBase))
*/


extern aircraft_t aircraft_samples[MAX_AIRCRAFT]; /**< available aircraft types */
extern int numAircraft_samples;
extern int numAircraftItems;			/**< number of available aircrafts items in game. */
extern aircraftItem_t aircraftItems[MAX_AIRCRAFTITEMS];	/**< Available aicraft items. */
extern int airequipSelectedZone;		/**< Selected zone in equip menu */
extern int airequipSelectedSlot;			/**< Selected slot in equip menu */

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
void AIM_AircraftEquipmenuInit_f(void);
extern void AIM_AircraftEquipSlotSelect_f(void);
extern void AIM_AircraftEquipzoneSelect_f(void);
extern void AIM_AircraftEquipAddItem_f(void);
extern void AIM_AircraftEquipDeleteItem_f(void);
void AIM_AircraftEquipmenuClick_f(void);
const char *AIR_AircraftStatusToName(aircraft_t *aircraft);
qboolean AIR_IsAircraftInBase(aircraft_t *aircraft);
void AIR_AircraftInit(void);
void AIR_AircraftSelect(aircraft_t *aircraft);
void AIR_AircraftSelect_f(void);

void AIR_DeleteAircraft(aircraft_t *aircraft);
void AIR_DestroyAircraft(aircraft_t *aircraft);

void AIR_ResetAircraftTeam(aircraft_t *aircraft);
void AIR_AddToAircraftTeam(aircraft_t *aircraft,int idx);
void AIR_RemoveFromAircraftTeam(aircraft_t *aircraft,int idx);
void AIR_DecreaseAircraftTeamIdxGreaterThan(aircraft_t *aircraft,int idx);
qboolean AIR_IsInAircraftTeam(aircraft_t *aircraft,int idx);

void CL_CampaignRunAircraft(int dt);
aircraft_t *AIR_GetAircraft(const char *name);
aircraft_t* AIR_AircraftGetFromIdx(int idx);
void AII_InitialiseSlot(aircraftSlot_t *slot, int aircraftIdx);
int AII_GetAircraftItemByID(const char *id);
void CP_GetRandomPosForAircraft(float *pos);
qboolean AIR_AircraftMakeMove(int dt, aircraft_t* aircraft);
void AIR_ParseAircraft(const char *name, const char **text, qboolean assignAircraftItems);
void AII_ParseAircraftItem(const char *name, const char **text);
void AII_UpdateInstallationDelay(void);
void AII_ReloadWeapon(aircraft_t *aircraft);
qboolean AII_AddItemToSlot(technology_t *tech, aircraftSlot_t *slot);
void AII_UpdateAircraftStats(aircraft_t *aircraft);
int AII_GetSlotItems(aircraftItemType_t type, aircraft_t *aircraft);
qboolean AIR_AircraftHasEnoughFuel(aircraft_t *aircraft, const vec2_t destination);
void AIR_AircraftReturnToBase(aircraft_t *aircraft);
qboolean AIR_SendAircraftToMission(aircraft_t* aircraft, struct actMis_s* mission);
void AIR_SendAircraftPurchasingUfo(aircraft_t* aircraft, aircraft_t* ufo);
void AIR_SendUfoPurchasingAircraft(aircraft_t* ufo, aircraft_t* aircraft);
void AIR_SendUfoPurchasingBase(aircraft_t* ufo, struct base_s *base);
void AIR_AircraftsNotifyUfoRemoved(const aircraft_t *const ufo);
void AIR_AircraftsUfoDisappear(const aircraft_t *const ufo);
void AIR_UpdateHangarCapForAll(int base_idx);
qboolean AIR_AircraftAllowed(void);
qboolean AIR_ScriptSanityCheck(void);

const char* AII_WeightToName(itemWeight_t weight);

#endif
