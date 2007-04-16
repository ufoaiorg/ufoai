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

#define MAX_AIRCRAFT	256
#define MAX_CRAFTUPGRADES	64
#define LINE_MAXSEG 64
#define LINE_MAXPTS (LINE_MAXSEG+2)
#define LINE_DPHI	(M_PI/LINE_MAXSEG)

typedef struct mapline_s {
	int n;
	float dist;
	vec2_t p[LINE_MAXPTS];
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
/*	AC_ITEM_AMMO, unused right now */
	AC_ITEM_ARMOUR,
	AC_ITEM_ELECTRONICS,
	MAX_ACITEMS
} aircraftItemType_t;

struct actMis_s;

#define MAX_AIRCRAFTITEMS 64

/** @brief parsed craftitem from script files */
typedef struct aircraftItem_s {
	char id[MAX_VAR];			/**< from script files */
	int idx;					/**< self link */
	aircraftItemType_t type;		/**< The type of the aircraft item. */
	char tech[MAX_VAR];		/**< tech id for this item.*/
	int tech_idx;				/**< tech index for this item.*/
	char weapon[MAX_VAR];		/**< if this is ammo there must be a weapon */
	int weight;
	float damage;
	float range;
	float weaponRange;
	float speed;
	float shield;
	float accuracy;
	float ecm;
	int price;
} aircraftItem_t;

/** @brief A cargo of items collected after mission. */
typedef struct itemsTmp_s {
	int idx;		/**< Item idx (csi.ods[idx]). */
	int amount;		/**< Amount of collected items of this idx. */
} itemsTmp_t;

/** @brief An aircraft with all it's data */
typedef struct aircraft_s {
	int idx;					/**< Global index of this aircraft. See also gd.numAircraft. TODO: is this updated when one aircraft is lost (it is checked agains gd.numAircraft sometimes)? We do not really have a global list of acs do we? */
	int idx_sample;				/**< self-link in aircraft_sample list */
	char id[MAX_VAR];			/**< internal id from script file */
	char name[MAX_VAR];			/**< translateable name */
	char shortname[MAX_VAR];		/**< translateable shortname */
	char image[MAX_VAR];		/**< image on geoscape */
	aircraftType_t type;
	int status;					/**< see aircraftStatus_t */
	float speed;
	vec3_t angles;				/**< menu values for rotating */
	vec3_t scale;				/**< menu values for scaling */
	vec3_t center;				/**< menu values for shifting */
	vec3_t anglesEquip;			/**< menu values for rotating - aircraft_equip menu */
	vec3_t scaleEquip;			/**< menu values for scaling - aircraft_equip menu */
	vec3_t centerEquip;			/**< menu values for shifting - aircraft_equip menu */
	int price;
	int fuel;				/**< actual fuel */
	int fuelSize;				/**< max fuel */
	int size;				/**< how many soldiers max */
	int weight;				/**< "Size" of the aircraft used in capacity calculations. */
	vec3_t pos;				/**< actual pos on geoscape */
	int point;
	int time;
	int idxInBase;				/**< Index in base. See also base_t->numAircraftInBase. */
	int idxBase;				/**< Index of base. Used to restore homebase from savegame. See also "homebase" below.*/
	/* pointer to base->numOnTeam[AIRCRAFT_ID] */
	int *teamSize;				/**< how many soldiers on board */
	int teamIdxs[MAX_ACTIVETEAM];	/**< array of team members on board employee idx*/

	char model[MAX_QPATH];
	char weapon_string[MAX_VAR];
	/* NOTE: these pointers needs reinit after loading a saved game */
	technology_t *weapon;
	char shield_string[MAX_VAR];
	technology_t *shield;
	char item_string[MAX_VAR];
	technology_t *item;
	mapline_t route;
	void *homebase;				/**< Pointer to homebase for faster access. See also idxBase. */
	void *transferBase;			/**< Pointer to the base we are transfering equipment to */
	aliensTmp_t aliencargo[MAX_CARGO];	/**< Cargo of aliens. */
	int alientypes;				/**< How many types of aliens we collected. */
	itemsTmp_t itemcargo[MAX_CARGO];	/**< Cargo of items. */
	int itemtypes;				/**< How many types of items we collected. */

	char building[MAX_VAR];		/**< id of the building needed as hangar */

	int numUpgrades;

	struct actMis_s* mission;	/**< The mission the aircraft is moving to */
	int ufo;				/**< Ufo's id the aircraft is purchasing (gd.ufos + id) */
	radar_t	radar;			/**< Radar to track ufos */

	qboolean visible;		/**< The ufo is visible ? */
} aircraft_t;

/*
TODO: for later, this is used quite a lot in the code.
#define AIRCRAFTCURRENT_IS_SANE(base) (((base)->aircraftCurrent >= 0) && ((base)->aircraftCurrent < (base)->numAircraftInBase))
*/


extern aircraft_t aircraft_samples[MAX_AIRCRAFT]; /**< available aircraft types */
extern int numAircraft_samples;

/* script functions */

#ifdef DEBUG
void AIR_ListAircraft_f(void);
#endif

void AIM_AircraftStart_f(void);
void AIR_NewAircraft_f(void);
void AIM_ResetAircraftCvars_f (void);
void AIM_NextAircraft_f(void);
void AIM_PrevAircraft_f(void);
void AIR_AircraftReturnToBase_f(void);
void AIM_AircraftEquipmenuInit_f(void);
void AIM_AircraftEquipmenuClick_f(void);
char *AIR_AircraftStatusToName(aircraft_t *aircraft);
qboolean AIR_IsAircraftInBase(aircraft_t *aircraft);
void AIR_AircraftInit(void);
void AIR_AircraftSelect(aircraft_t *aircraft);
void AIR_AircraftSelect_f(void);

void AIR_DeleteAircraft(aircraft_t *aircraft);

void AIR_ResetAircraftTeam(aircraft_t *aircraft);
void AIR_AddToAircraftTeam(aircraft_t *aircraft,int idx);
void AIR_RemoveFromAircraftTeam(aircraft_t *aircraft,int idx);
void AIR_DecreaseAircraftTeamIdxGreaterThan(aircraft_t *aircraft,int idx);
qboolean AIR_IsInAircraftTeam(aircraft_t *aircraft,int idx);

void CL_CampaignRunAircraft(int dt);
aircraft_t *AIR_GetAircraft(const char *name);
extern aircraft_t* AIR_AircraftGetFromIdx(int idx);
extern void CP_GetRandomPosForAircraft(float *pos);
extern qboolean AIR_AircraftMakeMove(int dt, aircraft_t* aircraft);
void AIR_ParseAircraft(const char *name, char **text);
void AII_ParseAircraftItem(const char *name, char **text);
extern void AIR_AircraftReturnToBase(aircraft_t *aircraft);
extern qboolean AIR_SendAircraftToMission(aircraft_t* aircraft, struct actMis_s* mission);
extern void AIR_SendAircraftPurchasingUfo(aircraft_t* aircraft, aircraft_t* ufo);
extern void AIR_AircraftsNotifyUfoRemoved(const aircraft_t *const ufo);
extern void AIR_AircraftsUfoDisappear(const aircraft_t *const ufo);
void AIR_UpdateHangarCapForAll (int base_idx);

#endif
