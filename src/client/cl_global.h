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
	/* TODO
	base_t	bases[MAX_BASES];
	int	numBases;
	
	building_t	buildings[MAX_BASES][MAX_BUILDINGS];
	int numBuildings;
	
	employee_t	employees[MAX_EMPLOYEES];
	int   numEmployees;

	craftupgrade_t craftupgrades[MAX_CRAFTUPGRADES];
	int    numCraftUpgrades;
	
		
	aircraft_t	aircraft[MAX_BASES][MAX_AIRCRAFT];
	int	numAircraft;
	
	team_t	teams[MAX_BASES][MAX_TEAMS];
	int numTeams;
	
	inventory_t	inventory[MAX_BASES][MAX_TEAMS];
	int numInv;
	*/
	
	// ===================
	
	/*  even further down the road -> TODO
	building_t buildingTypes[MAX_BUILDING_TYPES];		// will hold the parsed data (.ufo)
	int numBuildingTypes;
	building_t buildings[MAX_BASES][MAX_BUILDINGS];	// new buildings in the MAX_BUILDINGS listz will get _copied_ from buildingTypes
	int numBuildings[MAX_BASES];					// holds the building-numbers of each base.
	*/
} globalData_t;

globalData_t gd;
