/**
 * @file cp_building.h
 * @brief Header for base building related stuff.
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

#ifndef CP_BUILDING_H
#define CP_BUILDING_H

#include "../../../shared/shared.h"
#include "../../../shared/mathlib.h"
#include "../../../common/common.h"

struct technology_s;

/** @brief All possible building status. */
typedef enum {
	B_STATUS_NOT_SET,				/**< not build yet */
	B_STATUS_UNDER_CONSTRUCTION,	/**< right now under construction */
	B_STATUS_CONSTRUCTION_FINISHED,	/**< construction finished - no workers assigned */
	/* and building needs workers */
	B_STATUS_WORKING,				/**< working normal (or workers assigned when needed) */
	B_STATUS_DOWN					/**< totally damaged */
} buildingStatus_t;

/** @brief Building events */
typedef enum {
	B_ONCONSTRUCT,					/**< building was placed on the base map */
	B_ONENABLE,						/**< building get operational (built up and all dependencies met) */
	B_ONDISABLE,					/**< building got disabled (ex: a dependency was destroyed) */
	B_ONDESTROY,					/**< building was desroyed */
	B_MAXEVENT
} buildingEvent_t;

/** @brief All different building types.
 * @note if you change the order, you'll load values of hasBuilding in wrong indice */
typedef enum {
	B_MISC,							/**< this building is nothing with a special function (used when a building appears twice in .ufo file) */
	B_LAB,							/**< this building is a lab */
	B_QUARTERS,						/**< this building is a quarter */
	B_STORAGE,						/**< this building is a storage */
	B_WORKSHOP,						/**< this building is a workshop */
	B_HOSPITAL,						/**< this building is a hospital */
	B_HANGAR,						/**< this building is a hangar */
	B_ALIEN_CONTAINMENT,			/**< this building is an alien containment */
	B_SMALL_HANGAR,					/**< this building is a small hangar */
	B_POWER,						/**< this building is power plant */
	B_COMMAND,						/**< this building is command centre */
	B_ANTIMATTER,					/**< this building is antimatter storage */
	B_ENTRANCE,						/**< this building is an entrance */
	B_DEFENCE_MISSILE,				/**< this building is a missile rack */
	B_DEFENCE_LASER,				/**< this building is a laser battery */
	B_RADAR,						/**< this building is a radar */

	MAX_BUILDING_TYPE
} buildingType_t;

/** @brief A building with all it's data. */
typedef struct building_s {
	int idx;						/**< Index in in the base buildings list. */
	struct building_s *tpl;			/**< Self link in "buildingTemplates" list. */
	struct base_s *base;			/**< The base this building is located in. */

	const char *id;
	char *name;						/**< translatable name of the building */
	const char *image, *mapPart, *pedia;

	vec2_t	size;
	int fixCosts, varCosts;

	/**
	 * level of the building.
	 * @note This value depends on the implementation of the affected building
	 * might e.g. be a factor */
	float level;

	date_t timeStart;
	int buildTime;

	buildingStatus_t buildingStatus;

	/** Event handler functions */
	char *onConstruct;
	char *onDestroy;
	char *onEnable;
	char *onDisable;

	int maxCount;					/**< How many building of the same type allowed? */

	vec2_t pos;						/**< location in the base. */
	qboolean mandatory;

	/** How many employees to hire on construction in the first base */
	int maxEmployees;

	buildingType_t buildingType;	/**< This way we can rename the buildings without loosing the control. @note Not to be confused with "tpl".*/
	struct technology_s *tech;		/**< Link to the building-technology. */
	const struct building_s *dependsBuilding;	/**< If the building needs another one to work (= to be buildable). @sa "buildingTemplates" list*/

	int capacity;					/**< Capacity of this building (used in calculate base capacities). */
} building_t;

/**
 * @brief Macro sets a building used
 * @param[in] usedArray must be a qboolean array of the size MAX_BUILDINGS
 * @param[in] buildingIDX Index of building to set used
 */
#define B_BuildingSetUsed(usedArray, buildingIDX) { (usedArray)[buildingIDX] = qtrue; }
/**
 * @brief Macro returns if a building is used
 * @param[in] usedArray must be a qboolean array of the size MAX_BUILDINGS
 * @param[in] buildingIDX Index of building to check
 */
#define B_BuildingGetUsed(usedArray, buildingIDX) ((usedArray)[buildingIDX])

void B_ParseBuildings(const char *name, const char **text, qboolean link);
qboolean B_BuildingScriptSanityCheck(void);
building_t *B_GetBuildingTemplate(const char *buildingName);
buildingType_t B_GetBuildingTypeByBuildingID(const char *buildingID);
qboolean B_CheckBuildingDependencesStatus(const building_t* building);
qboolean B_IsBuildingBuiltUp(const building_t *building);
float B_GetConstructionTimeRemain(const building_t const* building);

qboolean B_FireEvent(const building_t const* buildingTemplate, const struct base_s const* base, buildingEvent_t eventType);

#endif
