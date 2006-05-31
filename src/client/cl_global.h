/*

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


#include "cl_research.h"
#include "cl_ufopedia.h"
#include "cl_basemanagement.h"


// ============================
// globalData_t gd
//
// This struct/variable holds the global data for a game.
//
// Content
// * This struct will be save directly to the savegame-file.
// * Everything that should be saved needs to be in here.
// * No pointers are allowed inside this struct. Use an index of an array instead.
// ============================
// TODO: Implement me
// ============================

typedef struct globalData_s
{
	// == technolgies ==
	// A list of all research-topics resp. the research-tree.
	technology_t	technologies[MAX_TECHNOLOGIES];
	// Total nubmer of technologies.
	int	numTechnologies;

	// == pedia ==
	// A list of all Ufopedia chapters.
	pediaChapter_t	upChapters[MAX_PEDIACHAPTERS];
	// Total number uf upchapters,
	int numChapters;

	// == employees ==
	// A list of all phalanx employees (soldiers, scies, workers, etc...).
	employee_t	employees[MAX_EMPLOYEES];
	// Total number of employees.
	int   numEmployees;

	// == bases ==
	// A list of _all_ bases ... even unbuilt ones.
	base_t	bases[MAX_BASES];
	
	// Total number of parsed base-names.
	int	numBaseNames;
	// Total number of built bases (how many are enabled).
	int	numBases;

	// == buildings in bases ==
	// TODO: A list of all possible unique buldings.
	building_t buildingTypes[MAX_BUILDINGS];
	int numBuildingTypes;

	// TODO: A list of the building-list per base. (new buildings in a base get copied from buildingTypes)
	building_t buildings[MAX_BASES][MAX_BUILDINGS];
	// TODO: Total number of buildings per base.
	int numBuildings[MAX_BASES];
	
	/* TODO
	building_t	buildings[MAX_BASES][MAX_BUILDINGS];
	int numBuildings;

	craftupgrade_t craftupgrades[MAX_CRAFTUPGRADES];
	int    numCraftUpgrades;

	aircraft_t	aircraft[MAX_BASES][MAX_AIRCRAFT];
	int	numAircraft;

	team_t	teams[MAX_BASES][MAX_TEAMS];
	int numTeams;

	inventory_t	inventory[MAX_BASES][MAX_TEAMS];
	int numInv;
	*/
} globalData_t;

extern globalData_t	gd;

globalData_t	gd;

// TODO: Implement the saving of the initial parsed data.
//globalData_t	gd_on_startup;
