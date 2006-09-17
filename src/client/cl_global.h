/**
 * @file cl_global.h
 * @brief Defines global data structure.
 */

/*
Copyright (C) 2002-2006 UFO: Alien Invasion team.

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

#ifndef CLIENT_CL_GLOBAL_H
#define CLIENT_CL_GLOBAL_H

#include "cl_research.h"
#include "cl_basemanagement.h"


/**
 * @brief This struct/variable holds the global data for a game.
 * @note This struct will be saved directly to the savegame-file.
 * Everything that should be saved needs to be in here.
 * No pointers are allowed inside this struct. Use an index of an array instead.
 * @note If you need pointers you have to change CL_UpdatePointersInGlobalData, too
 * @sa CL_UpdatePointersInGlobalData
 * @sa CL_ReadSinglePlayerData
 */
typedef struct globalData_s
{
	/* == technolgies == */
	/* A list of all research-topics resp. the research-tree. */
	technology_t technologies[MAX_TECHNOLOGIES];
	/* Total nubmer of technologies. */
	int numTechnologies;

	/* == pedia == */
	/* A list of all Ufopedia chapters. */
	pediaChapter_t upChapters[MAX_PEDIACHAPTERS];
	/* Total number uf upchapters */
	int numChapters;

	/* == employees == */
	/* A list of all phalanx employees (soldiers, scies, workers, etc...) */
	employee_t employees[MAX_EMPL][MAX_EMPLOYEES];
	/* Total number of employees. */
	int numEmployees[MAX_EMPL];

	/* == bases == */
	/* A list of _all_ bases ... even unbuilt ones. */
	base_t bases[MAX_BASES];
	/* used for unique aircraft ids */
	int numAircraft;
	/* Total number of parsed base-names. */
	int numBaseNames;
	/* Total number of built bases (how many are enabled). */
	int numBases;

	/* == buildings in bases == */
	/* A list of all possible unique buldings. */
	building_t buildingTypes[MAX_BUILDINGS];
	int numBuildingTypes;
	/*  A list of the building-list per base. (new buildings in a base get copied from buildingTypes) */
	building_t buildings[MAX_BASES][MAX_BUILDINGS];
	/* Total number of buildings per base. */
	int numBuildings[MAX_BASES];
	/* governs zero build time for first base if empty base option chosen */
	int instant_build;

	/* == misc == */
	/* MA_NEWBASE, MA_INTERCEPT, MA_BASEATTACK, ... */
	int mapAction;
	/* how fast the game is running */
	int gameTimeScale;
	/* selected aircraft for interceptions */
	int interceptAircraft;
	/* already paid in this month? */
	qboolean fund;
	/* missions last seen on geoscape */
	char oldMis1[MAX_VAR];
	char oldMis2[MAX_VAR];
	char oldMis3[MAX_VAR];

	/* == production == */
	/* we will allow only one active production at the same time for each base */
	/* NOTE The facility to produce equipment should have the once-flag set */
	production_t productions[MAX_BASES];

	/* == Ranks == */
	/* Global list of all ranks defined in medals.ufo. */
	rank_t ranks[MAX_RANKS];
	/* The number of entries in the list above. */
	int numRanks;

	/* == Nations == */
	nation_t nations[MAX_NATIONS];
	int numNations;

	/* == UGVs == */
	ugv_t ugvs[MAX_UGV];
	int numUGV;

	/* unified character id */
	int nextUCN;

	/* == Aircraft == */
	/* UFOs on geoscape: TODO update their inner pointers if needed */
	aircraft_t ufos[MAX_UFOONGEOSCAPE];
	int numUfos;
} globalData_t;


extern globalData_t gd;

globalData_t gd;


#endif /* CLIENT_CL_GLOBAL_H */
