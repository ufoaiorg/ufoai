/**
 * @file cp_building.h
 */

/*
Copyright (C) 2002-2010 UFO: Alien Invasion.

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

#include "../../shared/shared.h"
#include "../../shared/mathlib.h"

struct technology_s;

/** @brief All possible building status. */
typedef enum {
	B_STATUS_NOT_SET,			/**< not build yet */
	B_STATUS_UNDER_CONSTRUCTION,	/**< right now under construction */
	B_STATUS_CONSTRUCTION_FINISHED,	/**< construction finished - no workers assigned */
	/* and building needs workers */
	B_STATUS_WORKING,			/**< working normal (or workers assigned when needed) */
	B_STATUS_DOWN				/**< totally damaged */
} buildingStatus_t;

/** @brief All different building types.
 * @note if you change the order, you'll load values of hasBuilding in wrong indice */
typedef enum {
	B_MISC,			/**< this building is nothing with a special function (used when a building appears twice in .ufo file) */
	B_LAB,			/**< this building is a lab */
	B_QUARTERS,		/**< this building is a quarter */
	B_STORAGE,		/**< this building is a storage */
	B_WORKSHOP,		/**< this building is a workshop */
	B_HOSPITAL,		/**< this building is a hospital */
	B_HANGAR,		/**< this building is a hangar */
	B_ALIEN_CONTAINMENT,	/**< this building is an alien containment */
	B_SMALL_HANGAR,		/**< this building is a small hangar */
	B_POWER,		/**< this building is power plant */
	B_COMMAND,		/**< this building is command centre */
	B_ANTIMATTER,		/**< this building is antimatter storage */
	B_ENTRANCE,		/**< this building is an entrance */
	B_DEFENCE_MISSILE,		/**< this building is a missile rack */
	B_DEFENCE_LASER,		/**< this building is a laser battery */
	B_RADAR,			/**< this building is a radar */

	MAX_BUILDING_TYPE
} buildingType_t;

/** @brief A building with all it's data. */
typedef struct building_s {
	int idx;				/**< Index in in the base buildings list. */
	struct building_s *tpl;	/**< Self link in "buildingTemplates" list. */
	struct base_s *base;	/**< The base this building is located in. */

	char *id;
	char *name;			/**< translatable name of the building */
	char *image, *mapPart, *pedia;

	char *needs;		/**< "needs" determines the second building part. */
	int fixCosts, varCosts;

	/**
	 * level of the building.
	 * @note This value depends on the implementation of the affected building
	 * might e.g. be a factor */
	float level;

	int timeStart, buildTime;

	buildingStatus_t buildingStatus;	/**< [BASE_SIZE*BASE_SIZE]; */

	qboolean visible;	/**< Is this building visible in the building list. */

	/** Event handler functions */
	char onConstruct[MAX_VAR];
	char onAttack[MAX_VAR];
	char onDestroy[MAX_VAR];

	int maxCount;		/**< How many building of the same type allowed? */

	vec2_t pos;			/**< location in the base. */
	qboolean mandatory;

	/** How many employees to hire on construction in the first base */
	int maxEmployees;

	buildingType_t buildingType;	/**< This way we can rename the buildings without loosing the control. @note Not to be confused with "tpl".*/
	struct technology_s *tech;				/**< Link to the building-technology. */
	const struct building_s *dependsBuilding;	/**< If the building needs another one to work (= to be buildable). @sa "buildingTemplates" list*/

	int capacity;		/**< Capacity of this building (used in calculate base capacities). */
} building_t;

void B_ParseBuildings(const char *name, const char **text, qboolean link);
buildingType_t B_GetBuildingTypeByBuildingID(const char *buildingID);
qboolean B_CheckBuildingDependencesStatus(const building_t* building);

#endif
