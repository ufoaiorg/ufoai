/**
 * @file
 * @brief Handles everything that is located in or accessed trough a base.
 * @note Basemanagement functions prefix: B_
 * @note See "base/ufos/basemanagement.ufo" for the underlying content.
 */

/*
Copyright (C) 2002-2015 UFO: Alien Invasion.

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

#include "../../cl_shared.h"
#include "../../cl_inventory.h" /* INV_GetEquipmentDefinitionByID */
#include "../../ui/ui_dataids.h"
#include "../../../shared/parse.h"
#include "cp_campaign.h"
#include "cp_mapfightequip.h"
#include "cp_aircraft.h"
#include "cp_missions.h"
#include "cp_geoscape.h"
#include "cp_popup.h"
#include "cp_radar.h"
#include "cp_time.h"
#include "cp_base_callbacks.h"
#include "cp_ufo.h"
#include "save/save_base.h"
#include "aliencontainment.h"

#define B_GetBuildingByIDX(baseIdx, buildingIdx) (&ccs.buildings[(baseIdx)][(buildingIdx)])
#define B_GetBuildingIDX(base, building) ((ptrdiff_t)((building) - ccs.buildings[base->idx]))
#define B_GetBaseIDX(base) ((ptrdiff_t)((base) - ccs.bases))

static void B_InitialEquipment(aircraft_t* aircraft, const equipDef_t* ed);

/**
 * @brief Returns the neighbourhood of a building
 * @param[in] building The building we ask neighbours
 * @returns a linkedList with neighbouring buildings
 * @note for unfinished building it returns an empty list
 */
static linkedList_t* B_GetNeighbours (const building_t* building)
{
	if (!building || !B_IsBuildingBuiltUp(building))
		return nullptr;

	const int x = building->pos[0];
	const int y = building->pos[1];
	base_t* base = building->base;
	linkedList_t* neighbours = nullptr;

	assert(base);
	for (int i = x; i < x + building->size[0]; i++) {
		/* North */
		if (y > 0 && B_GetBuildingAt(base, i, y - 1) != nullptr)
			cgi->LIST_AddPointer(&neighbours, (void*)B_GetBuildingAt(base, i, y - 1));
		/* South */
		if (y < BASE_SIZE - building->size[1] && B_GetBuildingAt(base, i, y + building->size[1]) != nullptr)
			cgi->LIST_AddPointer(&neighbours, (void*)B_GetBuildingAt(base, i, y + building->size[1]));
	}
	for (int i = y; i < y + building->size[1]; i++) {
		/* West */
		if (x > 0 && B_GetBuildingAt(base, x - 1, i) != nullptr)
			cgi->LIST_AddPointer(&neighbours, (void*)B_GetBuildingAt(base, x - 1, i));
		/* East */
		if (x < BASE_SIZE - building->size[0] && B_GetBuildingAt(base, x + building->size[0], i) != nullptr)
			cgi->LIST_AddPointer(&neighbours, (void*)B_GetBuildingAt(base, x + building->size[0], i));
	}
	return neighbours;
}

#ifdef DEBUG
/**
 * @brief Check if base coherent (every building connected to eachothers)
 * @param[in] base Pointer to the base to check
 */
static bool B_IsCoherent (const base_t* base)
{
	int found[MAX_BUILDINGS];
	linkedList_t* queue = nullptr;
	building_t* bldg = nullptr;

	OBJZERO(found);
	while ((bldg = B_GetNextBuilding(base, bldg)) != nullptr) {
		cgi->LIST_AddPointer(&queue, (void*)bldg);
		break;
	}
	if (!bldg)
		return true;

	while (!cgi->LIST_IsEmpty(queue)) {
		bldg = (building_t*)queue->data;
		found[bldg->idx] = 1;
		cgi->LIST_RemoveEntry(&queue, queue);

		linkedList_t* neighbours = B_GetNeighbours(bldg);
		LIST_Foreach(neighbours, building_t, bldg) {
			if (found[bldg->idx] == 0) {
				found[bldg->idx] = 1;
				cgi->LIST_AddPointer(&queue, (void*)bldg);
			}
		}
		cgi->LIST_Delete(&neighbours);
	}
	cgi->LIST_Delete(&queue);

	for (int i = 0; i < ccs.numBuildings[base->idx]; i++) {
		if (found[i] != 1)
			return false;
	}
	return true;
}
#endif

/**
 * @brief Check and add blocked tile to the base
 * @param[in,out] base The base to add blocked tile to
 * @param[in] row Row position of the tile
 * @param[in] column Column position of the tile
 * @return bool value of success
 */
static bool B_AddBlockedTile (base_t* base, int row, int column)
{
	assert(base);

	if (B_GetBuildingAt(base, column, row) != nullptr)
		return false;

	int found[BASE_SIZE][BASE_SIZE];
	OBJZERO(found);
	found[row][column] = -1;

	linkedList_t* queue = nullptr;
	/* Get first non blocked tile */
	for (int y = 0; y < BASE_SIZE && cgi->LIST_IsEmpty(queue); y++) {
		for (int x = 0; x < BASE_SIZE && cgi->LIST_IsEmpty(queue); x++) {
			if (x == column && y == row)
				continue;
			if (!B_IsTileBlocked(base, x, y))
				cgi->LIST_AddPointer(&queue, &base->map[y][x]);
		}
	}

	if (cgi->LIST_IsEmpty(queue))
		return false;

	/* BS Traversal */
	while (!cgi->LIST_IsEmpty(queue)) {
		baseBuildingTile_t* tile = (baseBuildingTile_t*)queue->data;
		cgi->LIST_RemoveEntry(&queue, queue);
		const int x = tile->posX;
		const int y = tile->posY;

		found[y][x] = 1;
		/* West */
		if (x > 0 && !B_IsTileBlocked(base, x - 1, y) && found[y][x - 1] == 0)
			cgi->LIST_AddPointer(&queue, (void*)&base->map[y][x - 1]);
		/* East */
		if (x < BASE_SIZE - 1 && !B_IsTileBlocked(base, x + 1, y) && found[y][x + 1] == 0)
			cgi->LIST_AddPointer(&queue, (void*)&base->map[y][x + 1]);
		/* North */
		if (y > 0 && !B_IsTileBlocked(base, x, y - 1) && found[y - 1][x] == 0)
			cgi->LIST_AddPointer(&queue, (void*)&base->map[y - 1][x]);
		/* South */
		if (y < BASE_SIZE - 1 && !B_IsTileBlocked(base, x, y + 1) && found[y + 1][x] == 0)
			cgi->LIST_AddPointer(&queue, (void*)&base->map[y + 1][x]);
	}
	cgi->LIST_Delete(&queue);

	/* Check for unreached areas */
	for (int y = 0; y < BASE_SIZE; y++) {
		for (int x = 0; x < BASE_SIZE; x++) {
			if (!B_IsTileBlocked(base, x, y) && found[y][x] == 0)
				return false;
		}
	}
	base->map[row][column].blocked = true;
	return true;
}

/**
 * @brief Fuction to put blocked tiles on basemap
 * @param[out] base The base to fill
 * @param[in] count Number of blocked tiles to add
 */
static void B_AddBlockedTiles (base_t* base, int count)
{
	assert(base);

	for (int placed = 0; placed < count; placed++) {
		const int x = rand() % BASE_SIZE;
		const int y = rand() % BASE_SIZE;

		if (B_IsTileBlocked(base, x, y))
			continue;

		if (B_GetBuildingAt(base, x, y) != nullptr)
			continue;

		B_AddBlockedTile(base, y, x);
	}
}

/**
 * @brief Returns if a base building is destroyable
 * @param[in] building Pointer to the building to check
 */
bool B_IsBuildingDestroyable (const building_t* building)
{
	assert(building);
	base_t* base = building->base;
	assert(base);

	if (base->baseStatus == BASE_DESTROYED)
		return true;

	linkedList_t* queue = nullptr;
	building_t* bldg = nullptr;

	while ((bldg = B_GetNextBuilding(base, bldg)) != nullptr) {
		if (bldg != building) {
			cgi->LIST_AddPointer(&queue, (void*)bldg);
			break;
		}
	}
	if (!bldg)
		return true;

	int found[MAX_BUILDINGS];
	OBJZERO(found);
	/* prevents adding building to be removed to the queue */
	found[building->idx] = 1;

	while (!cgi->LIST_IsEmpty(queue)) {
		bldg = (building_t*)queue->data;
		found[bldg->idx] = 1;
		cgi->LIST_RemoveEntry(&queue, queue);

		linkedList_t* neighbours = B_GetNeighbours(bldg);
		LIST_Foreach(neighbours, building_t, bldg) {
			if (found[bldg->idx] == 0) {
				found[bldg->idx] = 1;
				cgi->LIST_AddPointer(&queue, (void*)bldg);
			}
		}
		cgi->LIST_Delete(&neighbours);
	}
	cgi->LIST_Delete(&queue);

	for (int i = 0; i < ccs.numBuildings[base->idx]; i++) {
		if (found[i] != 1)
			return false;
	}

	return true;
}

/**
 * @brief Returns the count of founded bases
 */
int B_GetCount (void)
{
	return ccs.numBases;
}

/**
 * @brief Iterates through founded bases
 * @param[in] lastBase Pointer of the base to iterate from. call with nullptr to get the first one.
 */
base_t* B_GetNext (base_t* lastBase)
{
	base_t* endOfBases = &ccs.bases[B_GetCount()];
	base_t* base;

	if (!B_GetCount())
		return nullptr;

	if (!lastBase)
		return ccs.bases;
	assert(lastBase >= ccs.bases);
	assert(lastBase < endOfBases);

	base = lastBase;

	base++;
	if (base >= endOfBases)
		return nullptr;
	return base;
}

/**
 * @brief Array bound check for the base index. Will also return unfounded bases as
 * long as the index is in the valid ranges,
 * @param[in] baseIdx Index to check
 * @return Pointer to the base corresponding to baseIdx.
 */
base_t* B_GetBaseByIDX (int baseIdx)
{
	if (baseIdx >= MAX_BASES || baseIdx < 0)
		return nullptr;

	return &ccs.bases[baseIdx];
}

/**
 * @brief Array bound check for the base index.
 * @param[in] baseIdx Index to check
 * @return Pointer to the base corresponding to baseIdx if base is founded, nullptr else.
 */
base_t* B_GetFoundedBaseByIDX (int baseIdx)
{
	if (baseIdx >= B_GetCount())
		return nullptr;

	return B_GetBaseByIDX(baseIdx);
}

/**
 * @brief Iterates through buildings in a base
 * @param[in] base Pointer to the base which buildings asked
 * @param[in] lastBuilding Pointer to the building iterate from. Call with nullptr to get the first one.
 */
building_t* B_GetNextBuilding (const base_t* base, building_t* lastBuilding)
{
	building_t* endOfBuildings = B_GetBuildingByIDX(base->idx, ccs.numBuildings[base->idx]);
	building_t* building;

	if (!ccs.numBuildings[base->idx])
		return nullptr;

	if (!lastBuilding)
		return ccs.buildings[base->idx];
	assert(lastBuilding >= ccs.buildings[base->idx]);
	assert(lastBuilding < endOfBuildings);

	building = lastBuilding;

	building++;
	if (building >= endOfBuildings)
		return nullptr;
	return building;
}

/**
 * @brief Iterates throught buildings of a type in a base
 * @param[in] base Pointer to the base which buildings asked
 * @param[in] lastBuilding Pointer to the building iterate from. Call with nullptr to get the first one.
 * @param[in] buildingType Type of the buildings to search
 * @sa buildingType_t
 */
building_t* B_GetNextBuildingByType (const base_t* base, building_t* lastBuilding, buildingType_t buildingType)
{
	building_t* building = lastBuilding;

	while ((building = B_GetNextBuilding(base, building))) {
		if (building->buildingType == buildingType)
			break;
	}
	return building;
}

/**
 * @brief Searches the base for a given building type with the given status
 * @param[in] base Base to search
 * @param[in] type Building type to search
 * @param[in] status The status the building should have
 * @param[out] cnt This is a pointer to an int value which will hold the building
 * count of that type with the status you are searching - might also be nullptr
 * if you are not interested in this value
 * @note If you are searching for a quarter (e.g.) you should perform a
 * <code>if (hasBuilding[B_QUARTERS])</code> check - this should speed things up a lot
 * @return true if building with status was found
 */
bool B_CheckBuildingTypeStatus (const base_t* const base, buildingType_t type, buildingStatus_t status, int* cnt)
{
	int cntlocal = 0;
	building_t* building = nullptr;

	while ((building = B_GetNextBuildingByType(base, building, type))) {
		if (building->buildingStatus != status)
			continue;
		cntlocal++;
		/* don't count any further - the caller doesn't want to know the value */
		if (!cnt)
			return true;
	}

	/* set the cnt pointer if the caller wants to know this value */
	if (cnt)
		*cnt = cntlocal;

	return cntlocal ? true : false;
}

/**
 * @brief Get the capacity associated to a building type
 * @param[in] type The type of the building
 * @return capacity (baseCapacities_t), or MAX_CAP if building has no capacity
 */
baseCapacities_t B_GetCapacityFromBuildingType (buildingType_t type)
{
	switch (type) {
	case B_LAB:
		return CAP_LABSPACE;
	case B_QUARTERS:
		return CAP_EMPLOYEES;
	case B_STORAGE:
		return CAP_ITEMS;
	case B_WORKSHOP:
		return CAP_WORKSPACE;
	case B_HANGAR:
		return CAP_AIRCRAFT_BIG;
	case B_ALIEN_CONTAINMENT:
		return CAP_ALIENS;
	case B_SMALL_HANGAR:
		return CAP_AIRCRAFT_SMALL;
	case B_ANTIMATTER:
		return CAP_ANTIMATTER;
	default:
		return MAX_CAP;
	}
}

/**
 * @brief Get building type by base capacity.
 * @param[in] cap Enum type of baseCapacities_t.
 * @return Enum type of buildingType_t.
 * @sa B_UpdateBaseCapacities
 * @sa B_PrintCapacities_f
 */
buildingType_t B_GetBuildingTypeByCapacity (baseCapacities_t cap)
{
	switch (cap) {
	case CAP_ALIENS:
		return B_ALIEN_CONTAINMENT;
	case CAP_AIRCRAFT_SMALL:
		return B_SMALL_HANGAR;
	case CAP_AIRCRAFT_BIG:
		return B_HANGAR;
	case CAP_EMPLOYEES:
		return B_QUARTERS;
	case CAP_ITEMS:
		return B_STORAGE;
	case CAP_LABSPACE:
		return B_LAB;
	case CAP_WORKSPACE:
		return B_WORKSHOP;
	case CAP_ANTIMATTER:
		return B_ANTIMATTER;
	default:
		return MAX_BUILDING_TYPE;
	}
}

/**
 * @brief Get the status associated to a building
 * @param[in] base The base to search for the given building type
 * @param[in] buildingType value of building->buildingType
 * @return true if the building is functional
 * @sa B_SetBuildingStatus
 */
bool B_GetBuildingStatus (const base_t* const base, const buildingType_t buildingType)
{
	assert(base);

	if (buildingType == B_MISC)
		return true;

	if (buildingType < MAX_BUILDING_TYPE)
		return base->hasBuilding[buildingType];

	Com_Printf("B_GetBuildingStatus: Building-type %i does not exist.\n", buildingType);
	return false;
}

/**
 * @brief Set status associated to a building
 * @param[in] base Base to check
 * @param[in] buildingType value of building->buildingType
 * @param[in] newStatus New value of the status
 * @sa B_GetBuildingStatus
 */
void B_SetBuildingStatus (base_t* const base, const buildingType_t buildingType, bool newStatus)
{
	if (buildingType == B_MISC) {
		Com_Printf("B_SetBuildingStatus: No status is associated to B_MISC type of building.\n");
	} else if (buildingType < MAX_BUILDING_TYPE) {
		assert(base);
		base->hasBuilding[buildingType] = newStatus;
		Com_DPrintf(DEBUG_CLIENT, "B_SetBuildingStatus: set status for %i to %i\n", buildingType, newStatus);
	} else {
		Com_Printf("B_SetBuildingStatus: Type of building %i does not exist\n", buildingType);
	}
}

/**
 * @brief Resets the buildingCurrent variable and baseAction
 * @param[in,out] base Pointer to the base needs buildingCurrent to be reseted
 * @note Make sure you are not doing anything with the buildingCurrent pointer
 * in this function, the pointer might already be invalid
 */
void B_ResetBuildingCurrent (base_t* base)
{
	if (base)
		base->buildingCurrent = nullptr;
	ccs.baseAction = BA_NONE;
}

/**
 * @brief Get the maximum level of a building type in a base.
 * @param[in] base Pointer to base.
 * @param[in] type Building type to get the maximum level for.
 * @note This function checks base status for particular buildings.
 * @return 0.0f if there is no (operational) building of the requested type in the base, otherwise the maximum level.
 */
float B_GetMaxBuildingLevel (const base_t* base, const buildingType_t type)
{
	float max = 0.0f;

	if (B_GetBuildingStatus(base, type)) {
		building_t* building = nullptr;
		while ((building = B_GetNextBuildingByType(base, building, type)))
			if (building->buildingStatus == B_STATUS_WORKING)
				max = std::max(building->level, max);
	}

	return max;
}

/**
 * @brief Adds a map to the given position to the map string
 * @param[out] maps The map output string
 * @param[in] mapsLength The length of the maps string
 * @param[out] coords The coords output string
 * @param[in] coordsLength The length of the coords string
 * @param[in] map The map tile to add
 * @param[in] col The col to spawn the map at
 * @param[in] row The row to spawn the map at
 * @sa SV_Map_f
 * @sa SV_Map
 */
static inline void B_AddMap (char* maps, size_t mapsLength, char* coords, size_t coordsLength, const char* map, int col, int row)
{
	Q_strcat(coords, coordsLength, "%i %i %i ", col * BASE_TILE_UNITS, (BASE_SIZE - row - 1) * BASE_TILE_UNITS, 0);
	Q_strcat(maps, mapsLength, "%s", map);
}

/**
 * @brief Perform the base assembling in case of an alien attack
 * @param[out] maps The string containing the list of tilenames
 * @param[in] mapsLength The maximum length of maps string
 * @param[out] coords The string containing the positions of the tiles
 * @param[in] coordsLength The maximum length of coords string
 * @param[in,out] base The base to assemble
 * @return @c true if the assembly was successful, @c false if it failed
 * @todo Search a empty field and add a alien craft there
 * @todo If a building is still under construction, it will be assembled as a finished part.
 * Otherwise we need mapparts for all the maps under construction.
 */
bool B_AssembleMap (char* maps, size_t mapsLength, char* coords, size_t coordsLength, const base_t* base)
{
	if (!base) {
		Com_Printf("B_AssembleMap: No base to assemble\n");
		return false;
	}

	bool used[MAX_BUILDINGS];

	maps[0] = '\0';
	coords[0] = '\0';

	/* reset the used flag */
	OBJZERO(used);

	for (int row = 0; row < BASE_SIZE; ++row) {
		for (int col = 0; col < BASE_SIZE; ++col) {
			const building_t* entry = base->map[row][col].building;
			if (!entry) {
				B_AddMap(maps, mapsLength, coords, coordsLength, "b/empty ", col, row);
				continue;
			}
			if (!B_IsBuildingBuiltUp(entry)) {
				B_AddMap(maps, mapsLength, coords, coordsLength, "b/construction ", col, row);
				continue;
			}
			/* basemaps with needs are not (like the images in B_DrawBase) two maps - but one
			 * this is why we check the used flag and continue if it was set already */
			if (B_BuildingGetUsed(used, entry->idx))
				continue;
			B_BuildingSetUsed(used, entry->idx);

			if (!entry->mapPart)
				cgi->Com_Error(ERR_DROP, "MapPart for building '%s' is missing'", entry->id);

			B_AddMap(maps, mapsLength, coords, coordsLength, va("b/%s ", entry->mapPart), col, row);
		}
	}

	return true;
}

/**
 * @brief Check base status for particular buildings as well as capacities.
 * @param[in] building Pointer to building.
 * @return true if a base status has been modified (but do not check capacities)
 */
static bool B_CheckUpdateBuilding (building_t* building)
{
	bool oldValue;

	assert(building);

	/* Status of Miscellenious buildings cannot change. */
	if (building->buildingType == B_MISC)
		return false;

	oldValue = B_GetBuildingStatus(building->base, building->buildingType);
	if (building->buildingStatus == B_STATUS_WORKING
	 && B_CheckBuildingDependencesStatus(building))
		B_SetBuildingStatus(building->base, building->buildingType, true);
	else
		B_SetBuildingStatus(building->base, building->buildingType, false);

	if (B_GetBuildingStatus(building->base, building->buildingType) != oldValue) {
		Com_DPrintf(DEBUG_CLIENT, "Status of building %s is changed to %i.\n",
			building->name, B_GetBuildingStatus(building->base, building->buildingType));
		return true;
	}

	return false;
}

/**
 * @brief Update status of every building when a building has been built/destroyed
 * @param[in] base
 * @param[in] buildingType The building-type that has been built / removed.
 * @param[in] onBuilt true if building has been built, false else
 * @sa B_BuildingDestroy
 * @sa B_UpdateAllBaseBuildingStatus
 * @return true if at least one building status has been modified
 */
static bool B_UpdateStatusBuilding (base_t* base, buildingType_t buildingType, bool onBuilt)
{
	bool test = false;
	bool returnValue = false;
	building_t* building = nullptr;

	/* Construction / destruction may have changed the status of other building
	 * We check that, but only for buildings which needed building */
	while ((building = B_GetNextBuilding(base, building))) {
		const building_t* dependsBuilding = building->dependsBuilding;
		if (dependsBuilding && buildingType == dependsBuilding->buildingType) {
			/* ccs.buildings[base->idx][i] needs built/removed building */
			if (onBuilt && !B_GetBuildingStatus(base, building->buildingType)) {
				/* we can only activate a non operational building */
				if (B_CheckUpdateBuilding(building)) {
					B_FireEvent(building, base, B_ONENABLE);
					test = true;
					returnValue = true;
				}
			} else if (!onBuilt && B_GetBuildingStatus(base, building->buildingType)) {
				/* we can only deactivate an operational building */
				if (B_CheckUpdateBuilding(building)) {
					B_FireEvent(building, base, B_ONDISABLE);
					test = true;
					returnValue = true;
				}
			}
		}
	}
	/* and maybe some updated status have changed status of other building.
	 * So we check again, until nothing changes. (no condition here for check, it's too complex) */
	while (test) {
		test = false;
		building = nullptr;
		while ((building = B_GetNextBuilding(base, building))) {
			if (onBuilt && !B_GetBuildingStatus(base, building->buildingType)) {
				/* we can only activate a non operational building */
				if (B_CheckUpdateBuilding(building)) {
					B_FireEvent(building, base, B_ONENABLE);
					test = true;
				}
			} else if (!onBuilt && B_GetBuildingStatus(base, building->buildingType)) {
				/* we can only deactivate an operational building */
				if (B_CheckUpdateBuilding(building)) {
					B_FireEvent(building, base, B_ONDISABLE);
					test = true;
				}
			}
		}
	}

	return returnValue;
}

/**
 * @brief Update Antimatter Capacity.
 * @param[in] base Pointer to the base
 * @sa B_ResetAllStatusAndCapacities_f
 */
static void B_UpdateAntimatterCap (base_t* base)
{
	const objDef_t* od = INVSH_GetItemByID(ANTIMATTER_ITEM_ID);
	if (od != nullptr)
		CAP_SetCurrent(base, CAP_ANTIMATTER, B_ItemInBase(od, base));
}

/**
 * @brief Recalculate status and capacities of one base
 * @param[in] base Pointer to the base where status and capacities must be recalculated
 * @param[in] firstEnable true if this is the first time the function is called for this base
 */
void B_ResetAllStatusAndCapacities (base_t* base, bool firstEnable)
{
	bool test = true;

	assert(base);

	Com_DPrintf(DEBUG_CLIENT, "Reseting base %s:\n", base->name);

	/* reset all values of hasBuilding[] */
	for (int i = 0; i < MAX_BUILDING_TYPE; i++) {
		const buildingType_t type = (buildingType_t)i;
		if (type != B_MISC)
			B_SetBuildingStatus(base, type, false);
	}
	/* activate all buildings that needs to be activated */
	while (test) {
		building_t* building = nullptr;
		test = false;
		while ((building = B_GetNextBuilding(base, building))) {
			if (!B_GetBuildingStatus(base, building->buildingType)
			 && B_CheckUpdateBuilding(building)) {
				if (firstEnable)
					B_FireEvent(building, base, B_ONENABLE);
				test = true;
			}
		}
	}

	/* Update all capacities of base */
	B_UpdateBaseCapacities(MAX_CAP, base);

	/* calculate capacities.cur for every capacity */

	/* Current CAP_ALIENS (live alien capacity) is managed by AlienContainment class */
	/* Current aircraft capacities should not need recounting */

	if (B_GetBuildingStatus(base, B_GetBuildingTypeByCapacity(CAP_EMPLOYEES)))
		CAP_SetCurrent(base, CAP_EMPLOYEES, E_CountAllHired(base, true));

	if (B_GetBuildingStatus(base, B_GetBuildingTypeByCapacity(CAP_ITEMS)))
		CAP_UpdateStorageCap(base);

	if (B_GetBuildingStatus(base, B_GetBuildingTypeByCapacity(CAP_LABSPACE)))
		CAP_SetCurrent(base, CAP_LABSPACE, RS_CountScientistsInBase(base));

	if (B_GetBuildingStatus(base, B_GetBuildingTypeByCapacity(CAP_WORKSPACE)))
		PR_UpdateProductionCap(base);

	if (B_GetBuildingStatus(base, B_GetBuildingTypeByCapacity(CAP_ANTIMATTER)))
		B_UpdateAntimatterCap(base);

	/* Check that current capacity is possible -- if we changed values in *.ufo */
	for (int i = 0; i < MAX_CAP; i++) {
		const baseCapacities_t cap = (baseCapacities_t)i;
		if (CAP_GetFreeCapacity(base, cap) < 0)
			Com_Printf("B_ResetAllStatusAndCapacities: Warning, capacity of %i is bigger than maximum capacity\n", i);
	}
}

/**
 * @brief Removes a building from the given base
 * @param[in] building The building to remove
 * @note Also updates capacities and sets the hasBuilding[] values in base_t
 * @sa B_BuildingDestroy_f
 */
bool B_BuildingDestroy (building_t* building)
{
	/* Don't allow to destroy a mandatory building. */
	if (building->mandatory)
		return false;

	base_t* base = building->base;

	if (base->map[(int)building->pos[1]][(int)building->pos[0]].building != building) {
		cgi->Com_Error(ERR_DROP, "B_BuildingDestroy: building mismatch at base %i pos %i,%i.",
			base->idx, (int)building->pos[0], (int)building->pos[1]);
	}

	/* Refuse destroying if it hurts coherency - only exception is when the whole base destroys */
	if (!B_IsBuildingDestroyable(building)) {
		return false;
	}

	const buildingType_t buildingType = building->buildingType;
	const building_t* buildingTemplate = building->tpl;
	const bool runDisableCommand = building->buildingStatus == B_STATUS_WORKING;
	building->buildingStatus = B_STATUS_NOT_SET;

	/* Update buildingCurrent */
	if (base->buildingCurrent > building)
		base->buildingCurrent--;
	else if (base->buildingCurrent == building)
		base->buildingCurrent = nullptr;

	int const baseIDX = base->idx;
	building_t* const buildings = ccs.buildings[baseIDX];
	int const idx = building->idx;

	for (int y = building->pos[1]; y < building->pos[1] + building->size[1]; y++)
		for (int x = building->pos[0]; x < building->pos[0] + building->size[0]; x++)
			base->map[y][x].building = nullptr;

	REMOVE_ELEM(buildings, idx, ccs.numBuildings[baseIDX]);

	/* Update the link of other buildings */
	const int cntBldgs = ccs.numBuildings[baseIDX];
	for (int i = 0; i < cntBldgs; i++) {
		building_t* bldg = &buildings[i];
		if (bldg->idx < idx)
			continue;
		bldg->idx--;

		for (int y = bldg->pos[1]; y < bldg->pos[1] + bldg->size[1]; y++)
			for (int x = (int)bldg->pos[0]; x < bldg->pos[0] + bldg->size[0]; x++)
				base->map[y][x].building = bldg;
	}
	building = nullptr;

	/* Don't use the building pointer after this point - it's zeroed. */

	if (buildingType != B_MISC && buildingType != MAX_BUILDING_TYPE) {
		if (B_GetNumberOfBuildingsInBaseByBuildingType(base, buildingType) <= 0) {
			/* there is no more building of this type */
			B_SetBuildingStatus(base, buildingType, false);
			/* check if this has an impact on other buildings */
			B_UpdateStatusBuilding(base, buildingType, false);
			/* we may have changed status of several building: update all capacities */
			B_UpdateBaseCapacities(MAX_CAP, base);
		} else {
			/* there is still at least one other building of the same type in base: just update capacity */
			const baseCapacities_t cap = B_GetCapacityFromBuildingType(buildingType);
			if (cap != MAX_CAP)
				B_UpdateBaseCapacities(cap, base);
		}
	}

	/* call ondisable trigger only if building is not under construction
	 * (we do that after base capacity has been updated) */
	if (runDisableCommand) {
		if (B_FireEvent(buildingTemplate, base, B_ONDISABLE))
			Com_DPrintf(DEBUG_CLIENT, "B_BuildingDestroy: %s %i %i;\n",	buildingTemplate->onDisable, base->idx, buildingType);
	}
	if (B_FireEvent(buildingTemplate, base, B_ONDESTROY))
		Com_DPrintf(DEBUG_CLIENT, "B_BuildingDestroy: %s %i %i;\n",	buildingTemplate->onDestroy, base->idx, buildingType);

	CAP_CheckOverflow();

	cgi->Cmd_ExecuteString("base_init");

	return true;
}

/**
 * @brief Will ensure that aircraft on geoscape are not stored
 * in a base that no longer has any hangar left
 * @param[in] base The base that is going to be destroyed
 * @todo this should be merged into CAP_RemoveAircraftExceedingCapacity
 */
static void B_MoveAircraftOnGeoscapeToOtherBases (const base_t* base)
{
	AIR_ForeachFromBase(aircraft, base) {
		if (!AIR_IsAircraftOnGeoscape(aircraft))
			continue;
		base_t* newbase = nullptr;
		bool moved = false;
		while ((newbase = B_GetNext(newbase)) != nullptr) {
			/* found a new homebase? */
			if (base != newbase && !AIR_CheckMoveIntoNewHomebase(aircraft, newbase)) {
				AIR_MoveAircraftIntoNewHomebase(aircraft, newbase);
				moved = true;
				break;
			}
		}

		if (moved)
			continue;

		/* No base can hold this aircraft */
		UFO_NotifyPhalanxAircraftRemoved(aircraft);
		if (!MapIsWater(GEO_GetColor(aircraft->pos, MAPTYPE_TERRAIN, nullptr))) {
			CP_SpawnRescueMission(aircraft, nullptr);
		} else {
			/* Destroy the aircraft and everything onboard - the aircraft pointer
			 * is no longer valid after this point */
			/** @todo Pilot skills; really kill pilot in this case? */
			AIR_DestroyAircraft(aircraft);
		}
	}
}

/**
 * @brief Destroy a base.
 * @param[in,out] base Pointer to base to be destroyed.
 * @note If you want to sell items or unhire employees, you should do it before
 * calling this function - they are going to be killed / destroyed.
 */
void B_Destroy (base_t* base)
{
	assert(base);
	base->baseStatus = BASE_DESTROYED;

	CP_MissionNotifyBaseDestroyed(base);
	B_MoveAircraftOnGeoscapeToOtherBases(base);

	/* do a reverse loop as buildings are going to be destroyed */
	for (int buildingIdx = ccs.numBuildings[base->idx] - 1; buildingIdx >= 0; buildingIdx--) {
		building_t* building = B_GetBuildingByIDX(base->idx, buildingIdx);
		B_BuildingDestroy(building);
	}

	E_DeleteAllEmployees(base);

	AIR_ForeachFromBase(aircraft, base) {
		AIR_DeleteAircraft(aircraft);
	}

	if (base->alienContainment != nullptr) {
		delete base->alienContainment;
		base->alienContainment = nullptr;
	}

	OBJZERO(base->storage);
	CAP_SetCurrent(base, CAP_ITEMS, 0);

	/** @todo Destroy the base. For this we need to check all the dependencies and references.
	 * Should be only done after putting bases into a linkedList
	 */
}

#ifdef DEBUG
/**
 * @brief Debug command for destroying a base.
 */
static void B_Destroy_f (void)
{
	if (cgi->Cmd_Argc() < 2) {
		Com_Printf("Usage: %s <baseIdx>\n", cgi->Cmd_Argv(0));
		return;
	}

	const int baseIdx = atoi(cgi->Cmd_Argv(1));
	if (baseIdx < 0 || baseIdx >= MAX_BASES) {
		Com_Printf("B_Destroy_f: baseIdx %i is outside bounds\n", baseIdx);
		return;
	}

	base_t* base = B_GetFoundedBaseByIDX(baseIdx);
	if (!base) {
		Com_Printf("B_Destroy_f: Base %i not founded\n", baseIdx);
		return;
	}

	B_Destroy(base);
}
#endif

/**
 * @brief Displays the status of a building for baseview.
 * @note updates the cvar mn_building_status which is used in the building
 * construction menu to display the status of the given building
 * @note also script command function binding for 'building_status'
 */
void B_BuildingStatus (const building_t* building)
{
	assert(building);

	cgi->Cvar_Set("mn_building_status", _("Not set"));

	switch (building->buildingStatus) {
	case B_STATUS_NOT_SET: {
		const int numberOfBuildings = B_GetNumberOfBuildingsInBaseByTemplate(B_GetCurrentSelectedBase(), building->tpl);
		if (numberOfBuildings >= 0)
			cgi->Cvar_Set("mn_building_status", _("Already %i in base"), numberOfBuildings);
		break;
	}
	case B_STATUS_UNDER_CONSTRUCTION:
		cgi->Cvar_Set("mn_building_status", "");
		break;
	case B_STATUS_CONSTRUCTION_FINISHED:
		cgi->Cvar_Set("mn_building_status", _("Construction finished"));
		break;
	case B_STATUS_WORKING:
		if (B_CheckBuildingDependencesStatus(building)) {
			cgi->Cvar_Set("mn_building_status", "%s", _("Working 100%"));
		} else {
			assert(building->dependsBuilding);
			/** @todo shorten text or provide more space in overview popup */
			cgi->Cvar_Set("mn_building_status",_("Not operational, depends on %s"), _(building->dependsBuilding->name));
		}
		break;
	case B_STATUS_DOWN:
		cgi->Cvar_Set("mn_building_status", _("Down"));
		break;
	default:
		break;
	}
}

/**
 * @brief Updates base status for particular buildings as well as capacities.
 * @param[in,out] building Pointer to building.
 * @param[in] status Enum of buildingStatus_t which is status of given building.
 * @note This function checks whether a building has B_STATUS_WORKING status, and
 * then updates base status for particular buildings and base capacities.
 */
static void B_UpdateAllBaseBuildingStatus (building_t* building, buildingStatus_t status)
{
	building->buildingStatus = status;

	/* we update the status of the building (we'll call this building building 1) */
	const bool test = B_CheckUpdateBuilding(building);
	if (test) {
		base_t* base = building->base;
		B_FireEvent(building, base, B_ONENABLE);
		/* now, the status of this building may have changed the status of other building.
		 * We check that, but only for buildings which needed building 1 */
		B_UpdateStatusBuilding(base, building->buildingType, true);
		/* we may have changed status of several building: update all capacities */
		B_UpdateBaseCapacities(MAX_CAP, base);
	} else {
		/* no other status than status of building 1 has been modified
		 * update only status of building 1 */
		const baseCapacities_t cap = B_GetCapacityFromBuildingType(building->buildingType);
		if (cap != MAX_CAP) {
			base_t* base = building->base;
			B_UpdateBaseCapacities(cap, base);
		}
	}
}

/**
 * @brief Build starting building in the first base, and hire employees.
 * @param[in,out] base The base to put the new building into
 * @param[in] buildingTemplate The building template to create a new building with
 * @param[in] hire Hire employees for the building we create from the template
 * @param[in] pos The position on the base grid
 */
static void B_AddBuildingToBasePos (base_t* base, const building_t* buildingTemplate, bool hire, const vec2_t pos)
{
	/* new building in base (not a template) */
	building_t* buildingNew;

	/* fake a click to basemap */
	buildingNew = B_SetBuildingByClick(base, buildingTemplate, (int)pos[1], (int)pos[0]);
	if (!buildingNew)
		return;
	buildingNew->timeStart.day = 0;
	buildingNew->timeStart.sec = 0;
	B_UpdateAllBaseBuildingStatus(buildingNew, B_STATUS_WORKING);
	Com_DPrintf(DEBUG_CLIENT, "Base %i new building: %s at (%.0f:%.0f)\n",
			base->idx, buildingNew->id, buildingNew->pos[0], buildingNew->pos[1]);

	if (hire)
		E_HireForBuilding(base, buildingNew, -1);

	/* now call the onenable trigger */
	if (B_FireEvent(buildingNew, base, B_ONENABLE))
		Com_DPrintf(DEBUG_CLIENT, "B_AddBuildingToBasePos: %s %i;\n", buildingNew->onEnable, base->idx);
}

/**
 * @brief builds a base from template
 * @param[out] base The base to build
 * @param[in] templateName Template used for building. If @c nullptr no template
 * will be used.
 * @param[in] hire If hiring employee needed
 * @note It builds an empty base on nullptr (or empty) templatename
 */
static void B_BuildFromTemplate (base_t* base, const char* templateName, bool hire)
{
	const baseTemplate_t* baseTemplate = B_GetBaseTemplate(templateName);
	int freeSpace = BASE_SIZE * BASE_SIZE;

	assert(base);

	if (baseTemplate) {
		/* find each building in the template */
		for (int i = 0; i < baseTemplate->numBuildings; i++) {
			const baseBuildingTile_t* buildingTile = &baseTemplate->buildings[i];

			if (base->map[buildingTile->posY][buildingTile->posX].building)
				continue;

			vec2_t pos;
			Vector2Set(pos, buildingTile->posX, buildingTile->posY);
			B_AddBuildingToBasePos(base, buildingTile->building, hire, pos);
			freeSpace--;
		}
	}

	/* we need to set up the mandatory buildings */
	for (int i = 0; i < ccs.numBuildingTemplates; i++) {
		building_t* building = &ccs.buildingTemplates[i];
		vec2_t pos;

		if (!building->mandatory || B_GetBuildingStatus(base, building->buildingType))
			continue;

		while (freeSpace > 0 && !B_GetBuildingStatus(base, building->buildingType)) {
			const int x = rand() % BASE_SIZE;
			const int y = rand() % BASE_SIZE;
			Vector2Set(pos, x, y);
			if (base->map[y][x].building)
				continue;
			B_AddBuildingToBasePos(base, building, hire, pos);
			freeSpace--;
		}
		/** @todo if there is no more space for mandatory building, remove a non mandatory one
		 * or build mandatory ones first */
		if (!B_GetBuildingStatus(base, building->buildingType))
			cgi->Com_Error(ERR_DROP, "B_BuildFromTemplate: Cannot build base. No space for it's buildings!");
	}

	/* set building tile positions */
	for (int i = 0; i < BASE_SIZE; i++) {
		for (int j = 0; j < BASE_SIZE; j++) {
			base->map[i][j].posY = i;
			base->map[i][j].posX = j;
		}
	}

	/* Create random blocked fields in the base.
	 * The first base never has blocked fields so we skip it. */
	if (ccs.campaignStats.basesBuilt >= 1) {
		const int j = round((frand() * (MAX_BLOCKEDFIELDS - MIN_BLOCKEDFIELDS)) + MIN_BLOCKEDFIELDS);
		B_AddBlockedTiles(base, j);
	}
}

/**
 * @brief Setup aircraft and equipment for first base. Uses the campaign
 * scriptable equipmentlist.
 * @param[in] campaign The campaign data structure
 * @param[in,out] base The base to set up
 */
void B_SetUpFirstBase (const campaign_t* campaign, base_t* base)
{
	const equipDef_t* ed;

	/* Find the initial equipment definition for current campaign. */
	ed = cgi->INV_GetEquipmentDefinitionByID(campaign->equipment);
	/* Copy it to base storage. */
	base->storage = *ed;

	/* Add aircraft to the first base */
	/** @todo move aircraft to .ufo */
	/* buy two first aircraft and hire pilots for them. */
	if (B_GetBuildingStatus(base, B_HANGAR)) {
		const equipDef_t* equipDef = cgi->INV_GetEquipmentDefinitionByID(campaign->soldierEquipment);
		const char* firebird = cgi->Com_DropShipTypeToShortName(DROPSHIP_FIREBIRD);
		const aircraft_t* firebirdAircraft = AIR_GetAircraft(firebird);
		aircraft_t* aircraft = AIR_NewAircraft(base, firebirdAircraft);
		CP_UpdateCredits(ccs.credits - firebirdAircraft->price);
		if (!E_HireEmployeeByType(base, EMPL_PILOT))
			cgi->Com_Error(ERR_DROP, "B_SetUpFirstBase: Hiring pilot failed.");
		/* refuel initial aicraft instantly */
		aircraft->fuel = aircraft->stats[AIR_STATS_FUELSIZE];
		/* Assign and equip soldiers on Dropships */
		AIR_AssignInitial(aircraft);
		B_InitialEquipment(aircraft, equipDef);
	}
	if (B_GetBuildingStatus(base, B_SMALL_HANGAR)) {
		const char* stiletto = cgi->Com_DropShipTypeToShortName(INTERCEPTOR_STILETTO);
		const aircraft_t* stilettoAircraft = AIR_GetAircraft(stiletto);
		aircraft_t* aircraft = AIR_NewAircraft(base, stilettoAircraft);
		CP_UpdateCredits(ccs.credits - stilettoAircraft->price);
		if (!E_HireEmployeeByType(base, EMPL_PILOT))
			cgi->Com_Error(ERR_DROP, "B_SetUpFirstBase: Hiring pilot failed.");
		/* refuel initial aicraft instantly */
		aircraft->fuel = aircraft->stats[AIR_STATS_FUELSIZE];
		AIM_AutoEquipAircraft(aircraft);
	}
}

/**
 * @brief Counts the actual installation count limit
 * @returns int number of installations can be built
 */
int B_GetInstallationLimit (void)
{
	int limit = 0;
	base_t* base = nullptr;

	/* count working Command Centers */
	while ((base = B_GetNext(base)) != nullptr) {
		if (B_GetBuildingStatus(base, B_COMMAND))
			limit++;
	}

	return limit * MAX_INSTALLATIONS_PER_BASE;
}

/**
 * @brief Set the base name
 * @param[out] base The base to set the name for
 * @param[in] name The name for the base. This might already be in utf-8 as
 * it's the user input from the UI
 */
void B_SetName (base_t* base, const char* name)
{
	Q_strncpyz(base->name, name, sizeof(base->name));
}

/**
 * @brief Build new base, uses template for the first base
 * @param[in] campaign The campaign data structure
 * @param[in] pos Position (on Geoscape) the base built at
 * @param[in] name The name of the new base, this string might already be in utf-8
 */
base_t* B_Build (const campaign_t* campaign, const vec2_t pos, const char* name)
{
	if (!campaign)
		cgi->Com_Error(ERR_DROP, "You can only build a base in an active campaign");

	base_t* base = B_GetFirstUnfoundedBase();
	if (!base)
		cgi->Com_Error(ERR_DROP, "Cannot build more bases");

	B_SetName(base, name);
	Vector2Copy(pos, base->pos);

	base->idx = ccs.campaignStats.basesBuilt;
	base->founded = true;

	/* increase this early because a lot of functions rely on this
	 * value to get the base we are setting up here */
	ccs.numBases++;

	/* reset capacities */
	for (int i = 0; i < MAX_CAP; i++) {
		const baseCapacities_t cap = (baseCapacities_t)i;
		CAP_SetCurrent(base, cap, 0);
	}

	/* setup for first base */
	if (ccs.campaignStats.basesBuilt == 0) {
		if (campaign->firstBaseTemplate[0] == '\0')
			cgi->Com_Error(ERR_DROP, "No base template for setting up the first base given");
		B_BuildFromTemplate(base, campaign->firstBaseTemplate, true);
	} else {
		B_BuildFromTemplate(base, nullptr, true);
	}
	base->baseStatus = BASE_WORKING;

	/* a new base is not discovered (yet) */
	base->alienInterest = BASE_INITIALINTEREST;

	BDEF_InitialiseBaseSlots(base);

	/* Reset Radar range */
	const float level = B_GetMaxBuildingLevel(base, B_RADAR);
	RADAR_Initialise(&base->radar, RADAR_BASERANGE, RADAR_BASETRACKINGRANGE, level, false);
	RADAR_InitialiseUFOs(&base->radar);

	B_ResetAllStatusAndCapacities(base, true);

	PR_UpdateProductionCap(base);

	ccs.campaignStats.basesBuilt++;

	return base;
}

/**
 * @brief Returns the baseTemplate in the global baseTemplate list that has the unique name baseTemplateID.
 * @param[in] baseTemplateID The unique id of the building (baseTemplate_t->name).
 * @return baseTemplate_t If a Template was found it is returned, otherwise->nullptr.
 */
const baseTemplate_t* B_GetBaseTemplate (const char* baseTemplateID)
{
	if (!baseTemplateID)
		return nullptr;

	for (int i = 0; i < ccs.numBaseTemplates; i++)
		if (Q_streq(ccs.baseTemplates[i].id, baseTemplateID))
			return &ccs.baseTemplates[i];

	Com_Printf("Base Template %s not found\n", baseTemplateID);
	return nullptr;
}

/**
 * @brief Check a base cell
 * @return True if the cell is free to build
 */
bool B_MapIsCellFree (const base_t* base, int col, int row)
{
	return col >= 0 && col < BASE_SIZE
	 && row >= 0 && row < BASE_SIZE
	 && B_GetBuildingAt(base, col, row) == nullptr
	 && !B_IsTileBlocked(base, col, row);
}

/**
 * @brief Set the currently selected building.
 * @param[in,out] base The base to place the building in
 * @param[in] buildingTemplate The template of the building to place at the given location
 * @param[in] row Set building to row
 * @param[in] col Set building to col
 * @return building created in base (this is not a building template)
 */
building_t* B_SetBuildingByClick (base_t* base, const building_t* buildingTemplate, int row, int col)
{
#ifdef DEBUG
	if (!base)
		cgi->Com_Error(ERR_DROP, "no current base\n");
	if (!buildingTemplate)
		cgi->Com_Error(ERR_DROP, "no current building\n");
#endif
	if (!CP_CheckCredits(buildingTemplate->fixCosts)) {
		CP_Popup(_("Notice"), _("Not enough credits to build this\n"));
		return nullptr;
	}

	/* template should really be a template */
	/*assert(template == template->tpl);*/

	if (0 <= row && row < BASE_SIZE && 0 <= col && col < BASE_SIZE) {
		/* new building in base (not a template) */
		building_t* buildingNew = B_GetBuildingByIDX(base->idx, ccs.numBuildings[base->idx]);

		/* copy building from template list to base-buildings-list */
		*buildingNew = *buildingTemplate;
		/* self-link to building-list in base */
		buildingNew->idx = B_GetBuildingIDX(base, buildingNew);
		/* Link to the base. */
		buildingNew->base = base;

		if (!B_IsTileBlocked(base, col, row) && B_GetBuildingAt(base, col, row) == nullptr) {
			int y, x;

			if (col + buildingNew->size[0] > BASE_SIZE)
				return nullptr;
			if (row + buildingNew->size[1] > BASE_SIZE)
				return nullptr;
			for (y = row; y < row + buildingNew->size[1]; y++)
				for (x = col; x < col + buildingNew->size[0]; x++)
					if (B_GetBuildingAt(base, x, y) != nullptr || B_IsTileBlocked(base, x, y))
						return nullptr;
			/* No building in this place */

			/* set building position */
			buildingNew->pos[0] = col;
			buildingNew->pos[1] = row;

			/* Refuse adding if it hurts coherency */
			if (base->baseStatus == BASE_WORKING) {
				linkedList_t* neighbours;
				bool coherent = false;

				neighbours = B_GetNeighbours(buildingNew);
				LIST_Foreach(neighbours, building_t, bldg) {
					if (B_IsBuildingBuiltUp(bldg)) {
						coherent = true;
						break;
					}
				}
				cgi->LIST_Delete(&neighbours);

				if (!coherent) {
					CP_Popup(_("Notice"), _("You must build next to existing buildings."));
					return nullptr;
				}
			}

			/* set building position */
			for (y = row; y < row + buildingNew->size[1]; y++)
				for (x = col; x < col + buildingNew->size[0]; x++) {
					assert(!B_IsTileBlocked(base, x, y));
					base->map[y][x].building = buildingNew;
				}

			/* status and build (start) time */
			buildingNew->buildingStatus = B_STATUS_UNDER_CONSTRUCTION;
			buildingNew->timeStart = ccs.date;

			/* pay */
			CP_UpdateCredits(ccs.credits - buildingNew->fixCosts);
			/* Update number of buildings on the base. */
			ccs.numBuildings[base->idx]++;

			B_BuildingStatus(buildingNew);
			B_ResetBuildingCurrent(base);
			cgi->Cmd_ExecuteString("base_init");
			cgi->Cmd_ExecuteString("building_init");
			B_FireEvent(buildingNew, base, B_ONCONSTRUCT);

			return buildingNew;
		}
	}
	return nullptr;
}

#define MAX_BUILDING_INFO_TEXT_LENGTH 512

/**
 * @brief Draws a building.
 * @param[in] building The building to draw
 */
void B_DrawBuilding (const building_t* building)
{
	static char buildingText[MAX_BUILDING_INFO_TEXT_LENGTH];

	/* maybe someone call this command before the buildings are parsed?? */
	if (!building)
		return;

	buildingText[0] = '\0';

	B_BuildingStatus(building);

	Com_sprintf(buildingText, sizeof(buildingText), "%s\n", _(building->name));

	if (building->buildingStatus < B_STATUS_UNDER_CONSTRUCTION && building->fixCosts)
		Com_sprintf(buildingText, sizeof(buildingText), _("Costs:\t%i c\n"), building->fixCosts);

	if (building->buildingStatus == B_STATUS_UNDER_CONSTRUCTION || building->buildingStatus == B_STATUS_NOT_SET)
		Q_strcat(buildingText, sizeof(buildingText), ngettext("%i Day to build\n", "%i Days to build\n", building->buildTime), building->buildTime);

	if (building->varCosts)
		Q_strcat(buildingText, sizeof(buildingText), _("Running costs:\t%i c\n"), building->varCosts);

	if (building->dependsBuilding)
		Q_strcat(buildingText, sizeof(buildingText), _("Needs:\t%s\n"), _(building->dependsBuilding->name));

	if (building->name)
		cgi->Cvar_Set("mn_building_name", "%s", _(building->name));

	if (building->image)
		cgi->Cvar_Set("mn_building_image", "%s", building->image);
	else
		cgi->Cvar_Set("mn_building_image", "base/empty");

	/* link into menu text array */
	cgi->UI_RegisterText(TEXT_BUILDING_INFO, buildingText);
}

/**
 * @brief Counts the number of buildings of a particular type in a base.
 * @param[in] base Which base to count in.
 * @param[in] tpl The template type in the ccs.buildingTemplates list.
 * @return The number of buildings or -1 on error (e.g. base index out of range)
 */
int B_GetNumberOfBuildingsInBaseByTemplate (const base_t* base, const building_t* tpl)
{
	if (!base) {
		Com_Printf("B_GetNumberOfBuildingsInBaseByTemplate: No base given!\n");
		return -1;
	}

	if (!tpl) {
		Com_Printf("B_GetNumberOfBuildingsInBaseByTemplate: no building-type given!\n");
		return -1;
	}

	/* Check if the template really is one. */
	if (tpl != tpl->tpl) {
		Com_Printf("B_GetNumberOfBuildingsInBaseByTemplate: No building-type given as parameter. It's probably a normal building!\n");
		return -1;
	}

	int numberOfBuildings = 0;
	building_t* building = nullptr;
	while ((building = B_GetNextBuilding(base, building))) {
		if (building->tpl == tpl && building->buildingStatus != B_STATUS_NOT_SET)
			numberOfBuildings++;
	}
	return numberOfBuildings;
}

/**
 * @brief Counts the number of buildings of a particular building type in a base.
 * @param[in] base Which base to count in.
 * @param[in] buildingType Building type value.
 * @return The number of buildings or -1 on error (e.g. base index out of range)
 */
int B_GetNumberOfBuildingsInBaseByBuildingType (const base_t* base, const buildingType_t buildingType)
{
	if (!base) {
		Com_Printf("B_GetNumberOfBuildingsInBaseByBuildingType: No base given!\n");
		return -1;
	}

	if (buildingType >= MAX_BUILDING_TYPE) {
		Com_Printf("B_GetNumberOfBuildingsInBaseByBuildingType: no sane building-type given!\n");
		return -1;
	}

	int numberOfBuildings = 0;
	building_t* building = nullptr;
	while ((building = B_GetNextBuildingByType(base, building, buildingType)))
		if (building->buildingStatus != B_STATUS_NOT_SET)
			numberOfBuildings++;

	return numberOfBuildings;
}

/**
 * @brief Gets a building of a given type in the given base
 * @param[in] base The base to search the building in
 * @param[in] buildingType What building-type to get.
 * @param[in] onlyWorking If we're looking only for working buildings
 * @return The building or nullptr if base has no building of that type
 */
const building_t* B_GetBuildingInBaseByType (const base_t* base, buildingType_t buildingType, bool onlyWorking)
{
	/* we maybe only want to get the working building (e.g. it might the
	 * case that we don't have a powerplant and thus the searched building
	 * is not functional) */
	if (onlyWorking && !B_GetBuildingStatus(base, buildingType))
		return nullptr;

	building_t* building = nullptr;
	while ((building = B_GetNextBuildingByType(base, building, buildingType)))
		return building;

	return nullptr;
}

/**
 * @brief Reads a base layout template
 * @param[in] name The script id of the base template
 * @param[in] text The script block to parse
 * @sa CL_ParseScriptFirst
 */
void B_ParseBaseTemplate (const char* name, const char** text)
{
	const char* errhead = "B_ParseBaseTemplate: unexpected end of file (names ";
	const char* token;
	baseTemplate_t* baseTemplate;
	vec2_t pos;
	bool map[BASE_SIZE][BASE_SIZE];
	byte buildingNums[MAX_BUILDINGS];

	/* get token */
	token = Com_Parse(text);

	if (!*text || *token != '{') {
		Com_Printf("B_ParseBaseTemplate: Template \"%s\" without body ignored\n", name);
		return;
	}

	if (ccs.numBaseTemplates >= MAX_BASETEMPLATES)
		cgi->Com_Error(ERR_DROP, "B_ParseBaseTemplate: too many base templates");

	/* create new Template */
	baseTemplate = &ccs.baseTemplates[ccs.numBaseTemplates];
	baseTemplate->id = cgi->PoolStrDup(name, cp_campaignPool, 0);

	/* clear map for checking duplicate positions and buildingNums for checking moreThanOne constraint */
	OBJZERO(map);
	OBJZERO(buildingNums);

	ccs.numBaseTemplates++;

	do {
		/* get the building */
		token = cgi->Com_EParse(text, errhead, baseTemplate->id);
		if (!*text)
			break;
		if (*token == '}')
			break;

		if (!Q_streq(token, "building")) {
			cgi->Com_Error(ERR_DROP, "B_ParseBaseTemplate: \"building\" token expected but \"%s\" found", token);
		}

		linkedList_t* list;
		if (!cgi->Com_ParseList(text, &list)) {
			cgi->Com_Error(ERR_DROP, "B_ParseBaseTemplate: error while reading building tuple");
		}

		if (cgi->LIST_Count(list) != 2) {
			cgi->Com_Error(ERR_DROP, "B_ParseBaseTemplate: building tuple must contains 2 elements (id pos)");
		}

		const char* buildingToken = (char*)list->data;
		const char* positionToken = (char*)list->next->data;

		if (baseTemplate->numBuildings >= MAX_BASEBUILDINGS)
			cgi->Com_Error(ERR_DROP, "B_ParseBaseTemplate: too many buildings");

		/* check if building type is known */
		baseBuildingTile_t* tile = &baseTemplate->buildings[baseTemplate->numBuildings];
		baseTemplate->numBuildings++;

		for (int i = 0; i < ccs.numBuildingTemplates; i++)
			if (Q_streq(ccs.buildingTemplates[i].id, buildingToken)) {
				tile->building = &ccs.buildingTemplates[i];
				if (tile->building->maxCount >= 0 && tile->building->maxCount <= buildingNums[i])
					cgi->Com_Error(ERR_DROP, "B_ParseBaseTemplate: Found more %s than allowed in template %s (%d))", buildingToken, baseTemplate->id, tile->building->maxCount);
				buildingNums[i]++;
				break;
			}

		if (!tile->building)
			cgi->Com_Error(ERR_DROP, "B_ParseBaseTemplate: Could not find building with id %s\n", baseTemplate->id);

		/* get the position */
		cgi->Com_EParseValue(pos, positionToken, V_POS, 0, sizeof(vec2_t));
		tile->posX = pos[0];
		tile->posY = pos[1];
		if (tile->posX < 0 || tile->posX >= BASE_SIZE || tile->posY < 0 || tile->posY >= BASE_SIZE)
			cgi->Com_Error(ERR_DROP, "Invalid template coordinates for building %s in template %s given",
					tile->building->id, baseTemplate->id);

		/* check for buildings on same position */
		if (map[tile->posY][tile->posX])
			cgi->Com_Error(ERR_DROP, "Base template '%s' has ambiguous positions for buildings set.", baseTemplate->id);
		map[tile->posY][tile->posX] = true;

		cgi->LIST_Delete(&list);
	} while (*text);

	/* templates without the must-have buildings can't be used */
	for (int i = 0; i < ccs.numBuildingTemplates; i++) {
		const building_t* building = &ccs.buildingTemplates[i];
		if (building && building->mandatory && !buildingNums[i]) {
			cgi->Com_Error(ERR_DROP, "Every base template needs one '%s'! '%s' has none.", building->id, baseTemplate->id);
		}
	}
}

/**
 * @brief Get the first unfounded base
 * @return first unfounded base or nullptr if every available base slot is already filled
 */
base_t* B_GetFirstUnfoundedBase (void)
{
	for (int baseIdx = 0; baseIdx < MAX_BASES; baseIdx++) {
		base_t* base = B_GetBaseByIDX(baseIdx);
		if (!base->founded)
			return base;
	}

	return nullptr;
}

/**
 * @brief Sets the selected base
 * @param[in] base The base that is going to be selected
 * @sa B_SelectBase
 */
void B_SetCurrentSelectedBase (const base_t* base)
{
	base_t* b = nullptr;
	while ((b = B_GetNext(b)) != nullptr) {
		if (b == base) {
			b->selected = true;
			if (b->aircraftCurrent == nullptr)
				b->aircraftCurrent = AIR_GetFirstFromBase(b);
		} else
			b->selected = false;
	}

	if (base) {
		INS_SetCurrentSelectedInstallation(nullptr);
		cgi->Cvar_Set("mn_base_title", "%s", base->name);
		cgi->Cvar_SetValue("mn_base_status_id", base->baseStatus);
	} else {
		cgi->Cvar_Set("mn_base_title", "");
		cgi->Cvar_Set("mn_base_status_id", "");
	}
}

/**
 * @brief returns the currently selected base
 */
base_t* B_GetCurrentSelectedBase (void)
{
	base_t* base = nullptr;
	while ((base = B_GetNext(base)) != nullptr)
		if (base->selected)
			return base;

	return nullptr;
}

/**
 * @brief Select and opens a base
 * @param[in] base If this is @c nullptr we want to build a new base
 */
void B_SelectBase (const base_t* base)
{
	/* set up a new base */
	if (!base) {
		/* if player hit the "create base" button while creating base mode is enabled
		 * that means that player wants to quit this mode */
		if (ccs.mapAction == MA_NEWBASE) {
			GEO_ResetAction();
			return;
		}

		if (B_GetCount() < MAX_BASES) {
			/* show radar overlay (if not already displayed) */
			if (!GEO_IsRadarOverlayActivated())
				GEO_SetOverlay("radar", 1);
			ccs.mapAction = MA_NEWBASE;
		} else {
			ccs.mapAction = MA_NONE;
		}
	} else {
		Com_DPrintf(DEBUG_CLIENT, "B_SelectBase_f: select base with id %i\n", base->idx);
		ccs.mapAction = MA_NONE;
		cgi->UI_PushWindow("bases");
		B_SetCurrentSelectedBase(base);
	}
}

/**
 * @brief Swaps one skill from character1 to character 2 and vice versa.
 */
static void CL_SwapSkill (character_t* cp1, character_t* cp2, abilityskills_t skill)
{
	int tmp1, tmp2;
	tmp1 = cp1->score.skills[skill];
	tmp2 = cp2->score.skills[skill];
	cp1->score.skills[skill] = tmp2;
	cp2->score.skills[skill] = tmp1;

	tmp1 = cp1->score.initialSkills[skill];
	tmp2 = cp2->score.initialSkills[skill];
	cp1->score.initialSkills[skill] = tmp2;
	cp2->score.initialSkills[skill] = tmp1;

	tmp1 = cp1->score.experience[skill];
	tmp2 = cp2->score.experience[skill];
	cp1->score.experience[skill] = tmp2;
	cp2->score.experience[skill] = tmp1;
}

static void CL_DoSwapSkills (character_t* cp1, character_t* cp2, const abilityskills_t skill)
{
	if (cp1->score.skills[skill] < cp2->score.skills[skill])
		CL_SwapSkill(cp1, cp2, skill);

	switch (skill) {
	case SKILL_CLOSE:
		if (cp1->score.skills[ABILITY_SPEED] < cp2->score.skills[ABILITY_SPEED])
			CL_SwapSkill(cp1, cp2, ABILITY_SPEED);
		break;
#if 0
	case SKILL_HEAVY:
		if (cp1->score.skills[ABILITY_POWER] < cp2->score.skills[ABILITY_POWER])
			CL_SwapSkill(cp1, cp2, ABILITY_POWER);
		break;
#endif
	case SKILL_ASSAULT:
		/* no related basic attribute */
		break;
	case SKILL_SNIPER:
		if (cp1->score.skills[ABILITY_ACCURACY] < cp2->score.skills[ABILITY_ACCURACY])
			CL_SwapSkill(cp1, cp2, ABILITY_ACCURACY);
		break;
	case SKILL_EXPLOSIVE:
		if (cp1->score.skills[ABILITY_MIND] < cp2->score.skills[ABILITY_MIND])
			CL_SwapSkill(cp1, cp2, ABILITY_MIND);
		break;
	default:
		cgi->Com_Error(ERR_DROP, "CL_SwapSkills: illegal skill %i.\n", skill);
	}
}

/**
 * @brief Assembles a skill indicator for the given character and its wore weapons in correlation to the given skill
 * @param[in] chr The character to get the skill indicator for
 * @param[in] skill The skill to get the indicator for
 * @return -1 if the given character does not wear the needed weapons, otherwise a weighted indicator that tell us
 * how good the weapons fit for the given skill.
 * @todo This currently always uses exactly the first two firemodes (see fmode1+fmode2) for calculation. This needs to be adapted to support less (1) or more 3+ firemodes. I think the function will even  break on only one firemode .. never tested it.
 * @todo i think currently also the different ammo/firedef types for each weapon (different weaponr_fd_idx and weaponr_fd_idx values) are ignored.
 */
static int CL_GetSkillIndicator (const character_t* chr, abilityskills_t skill)
{
	const fireDef_t* fdRight = nullptr;
	const fireDef_t* fdHolster = nullptr;
	const Item* rightHand = chr->inv.getRightHandContainer();
	const Item* holster = chr->inv.getHolsterContainer();

	if (rightHand && rightHand->ammoDef() && rightHand->def())
		fdRight = rightHand->getFiredefs();
	if (holster && holster->ammoDef() && holster->def())
		fdHolster = holster->getFiredefs();
	/* disregard left hand, or dual-wielding guys are too good */

	if (fdHolster == nullptr)
		return -1;
	if (fdRight == nullptr)
		return -1;

	const byte fmode1 = 0;
	const byte fmode2 = 1;
	int no = 0;
	if (rightHand != nullptr) {
		const fireDef_t* fd = rightHand->ammoDef()->fd[fdRight->weapFdsIdx];
		no += 2 * (skill == fd[fmode1].weaponSkill);
		no += 2 * (skill == fd[fmode2].weaponSkill);
	}
	if (holster != nullptr && holster->isReloadable()) {
		const fireDef_t* fd = holster->ammoDef()->fd[fdHolster->weapFdsIdx];
		no += (skill == fd[fmode1].weaponSkill);
		no += (skill == fd[fmode2].weaponSkill);
	}
	return no;
}

/**
 * @brief Swaps skills of the initial team of soldiers so that they match inventories
 */
static void CL_SwapSkills (linkedList_t* team)
{
	const int teamSize = cgi->LIST_Count(team);

	for (int i = 0; i < teamSize; i++) {
		/* running the loops below is not enough, we need transitive closure */
		/* I guess num times is enough --- could anybody prove this? */
		/* or perhaps 2 times is enough as long as weapons have 1 skill? */
		for (int x = ABILITY_NUM_TYPES; x < SKILL_NUM_TYPES; x++) {
			const abilityskills_t skill = (abilityskills_t)x;
			LIST_Foreach(team, character_t, cp1) {
				if (cp1__iter == nullptr)
					continue;

				const int no1 = CL_GetSkillIndicator(cp1, skill);
				if (no1 == -1)
					continue;

				LIST_Foreach(cp1__iter->next, character_t, cp2) {
					const int no2 = CL_GetSkillIndicator(cp2, skill);
					if (no2 == -1)
						continue;

					if (no1 > no2 /* more use of this skill */
						 || (no1 && no1 == no2)) { /* or earlier on list */
						CL_DoSwapSkills(cp1, cp2, skill);
					} else if (no1 < no2) {
						CL_DoSwapSkills(cp2, cp1, skill);
					}
				}
			}
		}
	}
}

/**
 * @brief Prepares initial equipment for initial team the beginning of the campaign.
 * @param[in,out] aircraft aircraft on which the soldiers (to equip) are
 * @param[in] ed Initial equipment definition
 */
static void B_InitialEquipment (aircraft_t* aircraft, const equipDef_t* ed)
{
	base_t* homebase;
	linkedList_t* chrListTemp = nullptr;

	assert(aircraft);
	homebase = aircraft->homebase;
	assert(homebase);
	assert(ed);

	LIST_Foreach(aircraft->acTeam, Employee, employee) {
		character_t* chr = &employee->chr;

		/* pack equipment */
		Com_DPrintf(DEBUG_CLIENT, "B_InitialEquipment: Packing initial equipment for %s.\n", chr->name);
		cgi->INV_EquipActor(chr, ed, nullptr, cgi->GAME_GetChrMaxLoad(chr));
		cgi->LIST_AddPointer(&chrListTemp, (void*)chr);
	}

	AIR_MoveEmployeeInventoryIntoStorage(*aircraft, homebase->storage);
	CL_SwapSkills(chrListTemp);
	cgi->LIST_Delete(&chrListTemp);
	CAP_UpdateStorageCap(homebase);
}

/**
 * @brief Sets the baseStatus to BASE_NOT_USED
 * @param[in] base Which base should be reseted?
 * @sa CL_CampaignRemoveMission
 */
void B_BaseResetStatus (base_t* const base)
{
	assert(base);
	base->baseStatus = BASE_NOT_USED;
	if (ccs.mapAction == MA_BASEATTACK)
		ccs.mapAction = MA_NONE;
}

#ifdef DEBUG
/**
 * @brief Just lists all buildings with their data
 * @note called with debug_listbuilding
 * @note Just for debugging purposes - not needed in game
 */
static void B_BuildingList_f (void)
{
	base_t* base = nullptr;
	while ((base = B_GetNext(base)) != nullptr) {
		Com_Printf("\nBase id %i: %s\n", base->idx, base->name);
		/** @todo building count should not depend on base->idx. base->idx will not be an array index! */
		for (int j = 0; j < ccs.numBuildings[base->idx]; j++) {
			const building_t* building = B_GetBuildingByIDX(base->idx, j);

			Com_Printf("...Building: %s #%i - id: %i\n", building->id,
				B_GetNumberOfBuildingsInBaseByTemplate(base, building->tpl), base->idx);
			Com_Printf("...image: %s\n", building->image);
			Com_Printf(".....Status:\n");

			for (int k = 0; k < BASE_SIZE * BASE_SIZE; k++) {
				if (k > 1 && k % BASE_SIZE == 0)
					Com_Printf("\n");
				Com_Printf("%i ", building->buildingStatus);
				if (!building->buildingStatus)
					break;
			}
			Com_Printf("\n");
		}
	}
}

/**
 * @brief Just lists all bases with their data
 * @note called with debug_listbase
 * @note Just for debugging purposes - not needed in game
 */
static void B_BaseList_f (void)
{
	base_t* base = nullptr;
	while ((base = B_GetNext(base)) != nullptr) {
		if (!base->founded) {
			Com_Printf("Base idx %i not founded\n\n", base->idx);
			continue;
		}

		Com_Printf("Base idx %i\n", base->idx);
		Com_Printf("Base name %s\n", base->name);
		Com_Printf("Base founded %i\n", base->founded);
		Com_Printf("Base numMissileBattery %i\n", base->numBatteries);
		Com_Printf("Base numLaserBattery %i\n", base->numLasers);
		Com_Printf("Base radarRange %i\n", base->radar.range);
		Com_Printf("Base trackingRange %i\n", base->radar.trackingRange);
		Com_Printf("Base numSensoredAircraft %i\n", base->radar.numUFOs);
		Com_Printf("Base Alien interest %f\n", base->alienInterest);
		Com_Printf("Base hasBuilding[]:\n");
		Com_Printf("Misc  Lab Quar Stor Work Hosp Hang Cont SHgr UHgr SUHg Powr  cgi->Cmd AMtr Entr Miss Lasr  Rdr Team\n");
		for (int j = 0; j < MAX_BUILDING_TYPE; j++) {
			const buildingType_t type = (buildingType_t)j;
			Com_Printf("  %i  ", B_GetBuildingStatus(base, type));
		}
		Com_Printf("\n");
		Com_Printf("Base pos %.02f:%.02f\n", base->pos[0], base->pos[1]);
		Com_Printf("Base map:\n");
		for (int row = 0; row < BASE_SIZE; row++) {
			if (row > 0)
				Com_Printf("\n");
			for (int col = 0; col < BASE_SIZE; col++)
				Com_Printf("%2i (%3i: %3i)  ", (base->map[row][col].building ? base->map[row][col].building->idx : -1),
					base->map[row][col].posX, base->map[row][col].posY);
		}
		Com_Printf("\n\n");
	}
}
#endif

/**
 * @brief Checks whether the building menu or the pedia entry should be called
 * when you click a building in the baseview
 * @param[in] building The building we have clicked
 */
void B_BuildingOpenAfterClick (const building_t* building)
{
	const base_t* base = building->base;
	if (!B_GetBuildingStatus(base, building->buildingType)) {
		UP_OpenWith(building->pedia);
		return;
	}

	switch (building->buildingType) {
	case B_LAB:
		if (RS_ResearchAllowed(base))
			cgi->UI_PushWindow("research");
		else
			UP_OpenWith(building->pedia);
		break;
	case B_HOSPITAL:
		if (HOS_HospitalAllowed(base))
			cgi->UI_PushWindow("hospital");
		else
			UP_OpenWith(building->pedia);
		break;
	case B_ALIEN_CONTAINMENT:
		if (AC_ContainmentAllowed(base))
			cgi->UI_PushWindow("aliencont");
		else
			UP_OpenWith(building->pedia);
		break;
	case B_QUARTERS:
		if (E_HireAllowed(base))
			cgi->UI_PushWindow("employees");
		else
			UP_OpenWith(building->pedia);
		break;
	case B_WORKSHOP:
		if (PR_ProductionAllowed(base))
			cgi->UI_PushWindow("production");
		else
			UP_OpenWith(building->pedia);
		break;
	case B_DEFENCE_LASER:
	case B_DEFENCE_MISSILE:
		cgi->UI_PushWindow("basedefence");
		break;
	case B_HANGAR:
	case B_SMALL_HANGAR:
		if (!AIR_AircraftAllowed(base)) {
			UP_OpenWith(building->pedia);
		} else if (AIR_BaseHasAircraft(base)) {
			cgi->UI_PushWindow("aircraft_equip");
		} else {
			cgi->UI_PushWindow("buyaircraft");
			/* transfer is only possible when there are at least two bases */
			if (B_GetCount() > 1)
				CP_Popup(_("Note"), _("No aircraft in this base - You first have to purchase or transfer an aircraft\n"));
			else
				CP_Popup(_("Note"), _("No aircraft in this base - You first have to purchase an aircraft\n"));
		}
		break;
	case B_STORAGE:
		if (BS_BuySellAllowed(base))
			cgi->UI_PushWindow("market");
		else
			UP_OpenWith(building->pedia);
		break;
	case B_ANTIMATTER:
		CP_Popup(_("Information"), "%s %d/%d", _("Antimatter (current/max):"), CAP_GetCurrent(base, CAP_ANTIMATTER), CAP_GetMax(base, CAP_ANTIMATTER));
		break;
	default:
		UP_OpenWith(building->pedia);
		break;
	}
}

#ifdef DEBUG
/**
 * @brief Debug function for printing all capacities in given base.
 * @note called with debug_listcapacities
 */
static void B_PrintCapacities_f (void)
{
	if (cgi->Cmd_Argc() < 2) {
		Com_Printf("Usage: %s <baseID>\n", cgi->Cmd_Argv(0));
		return;
	}

	const int baseIdx = atoi(cgi->Cmd_Argv(1));
	if (baseIdx >= B_GetCount()) {
		Com_Printf("invalid baseID (%s)\n", cgi->Cmd_Argv(1));
		return;
	}
	base_t* base = B_GetBaseByIDX(baseIdx);
	for (int i = 0; i < MAX_CAP; i++) {
		const baseCapacities_t cap = (baseCapacities_t)i;
		const buildingType_t buildingType = B_GetBuildingTypeByCapacity(cap);
		if (buildingType >= MAX_BUILDING_TYPE) {
			Com_Printf("B_PrintCapacities_f: Could not find building associated with capacity %i\n", i);
			continue;
		}
		for (int j = 0; j < ccs.numBuildingTemplates; j++) {
			if (ccs.buildingTemplates[j].buildingType != buildingType)
				continue;

			Com_Printf("Building: %s, capacity max: %i, capacity cur: %i\n",
					ccs.buildingTemplates[j].id, CAP_GetMax(base, i), CAP_GetCurrent(base, cap));
			break;
		}
	}
}

/**
 * @brief Finishes the construction of every building in the base
 */
static void B_BuildingConstructionFinished_f (void)
{
	base_t* base = B_GetCurrentSelectedBase();

	if (!base)
		return;

	for (int i = 0; i < ccs.numBuildings[base->idx]; i++) {
		building_t* building = B_GetBuildingByIDX(base->idx, i);
		if (building->buildingStatus != B_STATUS_UNDER_CONSTRUCTION)
			continue;

		B_UpdateAllBaseBuildingStatus(building, B_STATUS_WORKING);
		building->timeStart.day = 0;
		building->timeStart.sec = 0;
		base->buildingCurrent = building;
		B_FireEvent(building, base, B_ONENABLE);
	}
	/* update menu */
	B_SelectBase(base);
}

/**
 * @brief Recalculate status and capacities
 * @note function called with debug_basereset or after loading
 */
static void B_ResetAllStatusAndCapacities_f (void)
{
	base_t* base;

	base = nullptr;
	while ((base = B_GetNext(base)) != nullptr) {
		/* set buildingStatus[] and capacities.max values */
		B_ResetAllStatusAndCapacities(base, false);
	}
}

/**
 * @brief Check base coherency
 */
static void B_CheckCoherency_f (void)
{
	base_t* base;

	if (cgi->Cmd_Argc() >= 2) {
		const int i = atoi(cgi->Cmd_Argv(1));

		if (i < 0 || i >= B_GetCount()) {
			Com_Printf("Usage: %s [baseIdx]\nWithout baseIdx the current base is selected.\n", cgi->Cmd_Argv(0));
			return;
		}
		base = B_GetFoundedBaseByIDX(i);
	} else {
		base = B_GetCurrentSelectedBase();
	}

	if (base)
		Com_Printf("Base '%s' (idx:%i) is %scoherent.\n", base->name, base->idx, (B_IsCoherent(base)) ? "" : "not ");
	else
		Com_Printf("No base selected.\n");
}
#endif

/**
 * @brief Resets console commands.
 */
void B_InitStartup (void)
{
#ifdef DEBUG
	cgi->Cmd_AddCommand("debug_listbase", B_BaseList_f, "Print base information to the game console");
	cgi->Cmd_AddCommand("debug_listbuilding", B_BuildingList_f, "Print building information to the game console");
	cgi->Cmd_AddCommand("debug_listcapacities", B_PrintCapacities_f, "Debug function to show all capacities in given base");
	cgi->Cmd_AddCommand("debug_basereset", B_ResetAllStatusAndCapacities_f, "Reset building status and capacities of all bases");
	cgi->Cmd_AddCommand("debug_destroybase", B_Destroy_f, "Destroy a base");
	cgi->Cmd_AddCommand("debug_buildingfinished", B_BuildingConstructionFinished_f, "Finish construction for every building in the current base");
	cgi->Cmd_AddCommand("debug_baseiscoherent", B_CheckCoherency_f, "Checks if all buildings are connected on a base");
#endif
}

/**
 * @brief Checks whether the construction of a building is finished.
 * Calls the onEnable functions and assign workers, too.
 */
static bool B_CheckBuildingConstruction (building_t* building)
{
	if (building->buildingStatus != B_STATUS_UNDER_CONSTRUCTION)
		return false;

	if (!B_IsBuildingBuiltUp(building))
		return false;

	base_t* base = building->base;
	base->buildingCurrent = building;

	B_UpdateAllBaseBuildingStatus(building, B_STATUS_WORKING);
	if (B_FireEvent(building, base, B_ONENABLE))
		Com_DPrintf(DEBUG_CLIENT, "B_CheckBuildingConstruction: %s %i;\n", building->onEnable, base->idx);

	cgi->Cmd_ExecuteString("building_init");

	return true;
}

/**
 * @brief Updates base data
 * @sa CP_CampaignRun
 * @note called every "day"
 * @sa AIRFIGHT_ProjectileHitsBase
 */
void B_UpdateBaseData (void)
{
	base_t* base = nullptr;
	while ((base = B_GetNext(base)) != nullptr) {
		building_t* building = nullptr;
		while ((building = B_GetNextBuilding(base, building))) {
			if (!B_CheckBuildingConstruction(building))
				continue;

			Com_sprintf(cp_messageBuffer, lengthof(cp_messageBuffer),
					_("Construction of %s building finished in %s."), _(building->name), base->name);
			MSO_CheckAddNewMessage(NT_BUILDING_FINISHED, _("Building finished"), cp_messageBuffer);
		}
	}
}

/**
 * @brief Sell items to the market or add them to base storage.
 * @param[in] aircraft Pointer to an aircraft landing in base.
 * @sa B_AircraftReturnedToHomeBase
 */
static void B_SellOrAddItems (aircraft_t* aircraft)
{
	int numitems = 0;
	int gained = 0;

	assert(aircraft);
	base_t* base = aircraft->homebase;
	assert(base);

	itemsTmp_t* cargo = aircraft->itemcargo;

	for (int i = 0; i < aircraft->itemTypes; i++) {
		const objDef_t* item = cargo[i].item;
		const int amount = cargo[i].amount;
		technology_t* tech = RS_GetTechForItem(item);
		/* If the related technology is NOT researched, don't sell items. */
		if (!RS_IsResearched_ptr(tech)) {
			/* Items not researched cannot be thrown out even if not enough space in storage. */
			B_AddToStorage(base, item, amount);
			if (amount > 0)
				RS_MarkCollected(tech);
			continue;
		} else {
			/* If the related technology is researched, check the autosell option. */
			if (ccs.eMarket.autosell[item->idx]) { /* Sell items if autosell is enabled. */
				BS_SellItem(item, nullptr, amount);
				gained += BS_GetItemSellingPrice(item) * amount;
				numitems += amount;
			} else {
				B_AddToStorage(base, item, amount);
			}
			continue;
		}
	}

	if (numitems > 0) {
		Com_sprintf(cp_messageBuffer, lengthof(cp_messageBuffer), _("By selling %s you gathered %i credits."),
			va(ngettext("%i collected item", "%i collected items", numitems), numitems), gained);
		MS_AddNewMessage(_("Notice"), cp_messageBuffer);
	}

	/* ship no longer has cargo aboard */
	aircraft->itemTypes = 0;

	/* Mark new technologies researchable. */
	RS_MarkResearchable(aircraft->homebase);
	/* Recalculate storage capacity, to fix wrong capacity if a soldier drops something on the ground */
	CAP_UpdateStorageCap(aircraft->homebase);
}

/**
 * @brief Will unload all cargo to the homebase
 * @param[in,out] aircraft The aircraft to dump
 */
void B_DumpAircraftToHomeBase (aircraft_t* aircraft)
{
	/* Don't call cargo functions if aircraft is not a transporter. */
	if (aircraft->type != AIRCRAFT_TRANSPORTER)
		return;

	/* Add aliens to Alien Containment. */
	AL_AddAliens(aircraft);
	/* Sell collected items or add them to storage. */
	B_SellOrAddItems(aircraft);

	/* Now empty alien/item cargo just in case. */
	OBJZERO(aircraft->itemcargo);
}

/**
 * @brief Do anything when dropship returns to base
 * @param[in] aircraft Returning aircraft.
 * @note Place here any stuff, which should be called when Drophip returns to base.
 * @sa AIR_CampaignRun
 */
void B_AircraftReturnedToHomeBase (aircraft_t* aircraft)
{
	/* AA Missiles should miss */
	AIRFIGHT_RemoveProjectileAimingAircraft(aircraft);
	/* Reset UFO sensored on radar */
	RADAR_InitialiseUFOs(&aircraft->radar);
	/* Reload weapons */
	AII_ReloadAircraftWeapons(aircraft);

	B_DumpAircraftToHomeBase(aircraft);
}

/**
 * @brief Check if an item is available on a base
 * @param[in] base Pointer to the base to check at
 * @param[in] item Pointer to the item to check
 */
bool B_BaseHasItem (const base_t* base, const objDef_t* item)
{
	if (item->isVirtual)
		return true;

	return B_ItemInBase(item, base) > 0;
}

/**
 * @brief Check if the item has been collected (i.e it is in the storage) in the given base.
 * @param[in] item The item to check
 * @param[in] base The base to search in.
 * @return amount Number of available items in base
 */
int B_ItemInBase (const objDef_t* item, const base_t* base)
{
	const equipDef_t* ed;

	if (!item)
		return -1;
	if (item->isVirtual)
		return -1;
	if (!base)
		return -1;

	ed = &base->storage;

	if (!ed)
		return -1;

	return ed->numItems[item->idx];
}

/**
 * @brief Updates base capacities.
 * @param[in] cap Enum type of baseCapacities_t.
 * @param[in] base Pointer to the base.
 * @sa B_UpdateAllBaseBuildingStatus
 * @sa B_BuildingDestroy_f
 * @note If hasBuilding is false, the capacity is still increase: if power plant is destroyed and rebuilt, you shouldn't have to hire employees again
 */
void B_UpdateBaseCapacities (baseCapacities_t cap, base_t* base)
{
	switch (cap) {
	case CAP_ALIENS:			/**< Update Aliens capacity in base. */
	case CAP_EMPLOYEES:			/**< Update employees capacity in base. */
	case CAP_LABSPACE:			/**< Update laboratory space capacity in base. */
	case CAP_WORKSPACE:			/**< Update workshop space capacity in base. */
	case CAP_ITEMS:				/**< Update items capacity in base. */
	case CAP_AIRCRAFT_SMALL:	/**< Update aircraft capacity in base. */
	case CAP_AIRCRAFT_BIG:		/**< Update aircraft capacity in base. */
	case CAP_ANTIMATTER:		/**< Update antimatter capacity in base. */
	{
		int buildingTemplateIDX = -1;
		const buildingType_t buildingType = B_GetBuildingTypeByCapacity(cap);
		int capacity = 0;

		/* Reset given capacity in current base. */
		CAP_SetMax(base, cap, 0);
		/* Get building capacity. */
		for (int i = 0; i < ccs.numBuildingTemplates; i++) {
			const building_t* b = &ccs.buildingTemplates[i];
			if (b->buildingType == buildingType) {
				capacity = b->capacity;
				Com_DPrintf(DEBUG_CLIENT, "Building: %s capacity: %i\n", b->id, capacity);
				buildingTemplateIDX = i;
				break;
			}
		}
		/* Finally update capacity. */
		building_t* building = nullptr;
		while ((building = B_GetNextBuildingByType(base, building, buildingType)))
			if (building->buildingStatus >= B_STATUS_CONSTRUCTION_FINISHED)
				CAP_AddMax(base, cap, capacity);

		if (buildingTemplateIDX != -1)
			Com_DPrintf(DEBUG_CLIENT, "B_UpdateBaseCapacities: updated capacity of %s: %i\n",
				ccs.buildingTemplates[buildingTemplateIDX].id, CAP_GetMax(base, cap));

		if (cap == CAP_ALIENS) {
			const capacities_t* alienCap = CAP_Get(base, CAP_ALIENS);
			if (base->alienContainment != nullptr && alienCap->max == 0 && alienCap->cur == 0) {
				delete base->alienContainment;
				base->alienContainment = nullptr;
			} else if (base->alienContainment == nullptr && alienCap->max > 0) {
				base->alienContainment = new AlienContainment(CAP_Get(base, CAP_ALIENS), nullptr);
			}
		}
		break;
	}
	case MAX_CAP:			/**< Update all capacities in base. */
		Com_DPrintf(DEBUG_CLIENT, "B_UpdateBaseCapacities: going to update ALL capacities.\n");
		/* Loop through all capacities and update them. */
		for (int i = 0; i < cap; i++) {
			const baseCapacities_t cap = (baseCapacities_t) i;
			B_UpdateBaseCapacities(cap, base);
		}
		break;
	default:
		cgi->Com_Error(ERR_DROP, "Unknown capacity limit for this base: %i \n", cap);
	}
}

/**
 * @brief Saves the missile and laser slots of a base or sam site.
 * @param[in] weapons Defence weapons array
 * @param[in] numWeapons Number of entries in weapons array
 * @param[out] node XML Node structure, where we write the information to
 */
void B_SaveBaseSlotsXML (const baseWeapon_t* weapons, const int numWeapons, xmlNode_t* node)
{
	for (int i = 0; i < numWeapons; i++) {
		xmlNode_t* sub = cgi->XML_AddNode(node, SAVE_BASES_WEAPON);
		AII_SaveOneSlotXML(sub, &weapons[i].slot, true);
		cgi->XML_AddBool(sub, SAVE_BASES_AUTOFIRE, weapons[i].autofire);
		if (weapons[i].target)
			cgi->XML_AddInt(sub, SAVE_BASES_TARGET, weapons[i].target->idx);
	}
}

/**
 * @brief Saves base storage
 * @param[out] parent XML Node structure, where we write the information to
 * @param[in] equip Storage to save
 */
bool B_SaveStorageXML (xmlNode_t* parent, const equipDef_t& equip)
{
	for (int k = 0; k < cgi->csi->numODs; k++) {
		const objDef_t* od = INVSH_GetItemByIDX(k);
		if (equip.numItems[k] || equip.numItemsLoose[k]) {
			xmlNode_t* node = cgi->XML_AddNode(parent, SAVE_BASES_ITEM);

			cgi->XML_AddString(node, SAVE_BASES_ODS_ID, od->id);
			cgi->XML_AddIntValue(node, SAVE_BASES_NUM, equip.numItems[k]);
			cgi->XML_AddByteValue(node, SAVE_BASES_NUMLOOSE, equip.numItemsLoose[k]);
		}
	}
	return true;
}

/**
 * @brief Save callback for saving in xml format.
 * @param[out] parent XML Node structure, where we write the information to
 */
bool B_SaveXML (xmlNode_t* parent)
{
	xmlNode_t* bases;
	base_t* b;

	bases = cgi->XML_AddNode(parent, SAVE_BASES_BASES);
	b = nullptr;
	while ((b = B_GetNext(b)) != nullptr) {

		if (!b->founded) {
			Com_Printf("B_SaveXML: Base (idx: %i) not founded!\n", b->idx);
			return false;
		}

		cgi->Com_RegisterConstList(saveBaseConstants);

		xmlNode_t* act_base = cgi->XML_AddNode(bases, SAVE_BASES_BASE);
		cgi->XML_AddInt(act_base, SAVE_BASES_IDX, b->idx);
		cgi->XML_AddString(act_base, SAVE_BASES_NAME, b->name);
		cgi->XML_AddPos3(act_base, SAVE_BASES_POS, b->pos);
		cgi->XML_AddString(act_base, SAVE_BASES_BASESTATUS, cgi->Com_GetConstVariable(SAVE_BASESTATUS_NAMESPACE, b->baseStatus));
		cgi->XML_AddFloat(act_base, SAVE_BASES_ALIENINTEREST, b->alienInterest);

		/* building space */
		xmlNode_t* node = cgi->XML_AddNode(act_base, SAVE_BASES_BUILDINGSPACE);
		for (int row = 0; row < BASE_SIZE; row++) {
			for (int column = 0; column < BASE_SIZE; column++) {
				xmlNode_t* snode = cgi->XML_AddNode(node, SAVE_BASES_BUILDING);
				/** @todo save it as vec2t if needed, also it's opposite */
				cgi->XML_AddInt(snode, SAVE_BASES_X, row);
				cgi->XML_AddInt(snode, SAVE_BASES_Y, column);
				if (B_GetBuildingAt(b, column, row))
					cgi->XML_AddInt(snode, SAVE_BASES_BUILDINGINDEX, B_GetBuildingAt(b, column, row)->idx);
				cgi->XML_AddBoolValue(snode, SAVE_BASES_BLOCKED, B_IsTileBlocked(b, column, row));
			}
		}
		/* buildings */
		node = cgi->XML_AddNode(act_base, SAVE_BASES_BUILDINGS);
		building_t* building = nullptr;
		while ((building = B_GetNextBuilding(b, building))) {
			xmlNode_t* snode;

			if (!building->tpl)
				continue;

			snode = cgi->XML_AddNode(node, SAVE_BASES_BUILDING);
			cgi->XML_AddString(snode, SAVE_BASES_BUILDINGTYPE, building->tpl->id);
			cgi->XML_AddInt(snode, SAVE_BASES_BUILDING_PLACE, building->idx);
			cgi->XML_AddString(snode, SAVE_BASES_BUILDINGSTATUS, cgi->Com_GetConstVariable(SAVE_BUILDINGSTATUS_NAMESPACE, building->buildingStatus));
			cgi->XML_AddDate(snode, SAVE_BASES_BUILDINGTIMESTART, building->timeStart.day, building->timeStart.sec);
			cgi->XML_AddInt(snode, SAVE_BASES_BUILDINGBUILDTIME, building->buildTime);
			cgi->XML_AddFloatValue(snode, SAVE_BASES_BUILDINGLEVEL, building->level);
			cgi->XML_AddPos2(snode, SAVE_BASES_POS, building->pos);
		}
		/* base defences */
		node = cgi->XML_AddNode(act_base, SAVE_BASES_BATTERIES);
		B_SaveBaseSlotsXML(b->batteries, b->numBatteries, node);
		node = cgi->XML_AddNode(act_base, SAVE_BASES_LASERS);
		B_SaveBaseSlotsXML(b->lasers, b->numLasers, node);
		/* store equipment */
		node = cgi->XML_AddNode(act_base, SAVE_BASES_STORAGE);
		B_SaveStorageXML(node, b->storage);
		/* radar */
		cgi->XML_AddIntValue(act_base, SAVE_BASES_RADARRANGE, b->radar.range);
		cgi->XML_AddIntValue(act_base, SAVE_BASES_TRACKINGRANGE, b->radar.trackingRange);
		/* alien containment */
		if (b->alienContainment) {
			node = cgi->XML_AddNode(act_base, SAVE_BASES_ALIENCONTAINMENT);
			b->alienContainment->save(node);
		}

		cgi->Com_UnregisterConstList(saveBaseConstants);
	}
	return true;
}

/**
 * @brief Loads the missile and laser slots of a base or sam site.
 * @param[out] weapons Defence weapons array
 * @param[out] max Number of entries in weapons array
 * @param[in] p XML Node structure, where we load the information from
 * @sa B_Load
 * @sa B_SaveBaseSlots
 */
int B_LoadBaseSlotsXML (baseWeapon_t* weapons, int max, xmlNode_t* p)
{
	int i;
	xmlNode_t* s;
	for (i = 0, s = cgi->XML_GetNode(p, SAVE_BASES_WEAPON); s && i < max; i++, s = cgi->XML_GetNextNode(s, p, SAVE_BASES_WEAPON)) {
		const int target = cgi->XML_GetInt(s, SAVE_BASES_TARGET, -1);
		AII_LoadOneSlotXML(s, &weapons[i].slot, true);
		weapons[i].autofire = cgi->XML_GetBool(s, SAVE_BASES_AUTOFIRE, true);
		weapons[i].target = (target >= 0) ? UFO_GetByIDX(target) : nullptr;
	}
	return i;
}

/**
 * @brief Set the capacity stuff for all the bases after loading a savegame
 * @sa B_PostLoadInit
 */
static bool B_PostLoadInitCapacity (void)
{
	base_t* base = nullptr;
	while ((base = B_GetNext(base)) != nullptr)
		B_ResetAllStatusAndCapacities(base, true);

	return true;
}

/**
 * @brief Set the capacity stuff for all the bases after loading a savegame
 * @sa SAV_GameActionsAfterLoad
 */
bool B_PostLoadInit (void)
{
	return B_PostLoadInitCapacity();
}

/**
 * @brief Loads base storage
 * @param[in] parent XML Node structure, where we get the information from
 * @param[out] equip Storage to load
 */
bool B_LoadStorageXML (xmlNode_t* parent, equipDef_t* equip)
{
	for (xmlNode_t* node = cgi->XML_GetNode(parent, SAVE_BASES_ITEM); node; node = cgi->XML_GetNextNode(node, parent, SAVE_BASES_ITEM)) {
		const char* s = cgi->XML_GetString(node, SAVE_BASES_ODS_ID);
		const objDef_t* od = INVSH_GetItemByID(s);
		if (!od) {
			Com_Printf("B_Load: Could not find item '%s'\n", s);
			continue;
		}
		equip->numItems[od->idx] = cgi->XML_GetInt(node, SAVE_BASES_NUM, 0);
		equip->numItemsLoose[od->idx] = cgi->XML_GetInt(node, SAVE_BASES_NUMLOOSE, 0);
	}
	return true;
}

/**
 * @brief Loads base data
 * @param[in] parent XML Node structure, where we get the information from
 */
bool B_LoadXML (xmlNode_t* parent)
{
	int buildingIdx;
	xmlNode_t* bases;

	bases = cgi->XML_GetNode(parent, "bases");
	if (!bases) {
		Com_Printf("Error: Node 'bases' wasn't found in savegame\n");
		return false;
	}

	ccs.numBases = 0;

	cgi->Com_RegisterConstList(saveBaseConstants);
	FOREACH_XMLNODE(base, bases, SAVE_BASES_BASE) {
		const int i = ccs.numBases;
		base_t* const b = B_GetBaseByIDX(i);
		if (b == nullptr)
			break;

		ccs.numBases++;

		b->idx = cgi->XML_GetInt(base, SAVE_BASES_IDX, -1);
		if (b->idx < 0) {
			Com_Printf("Invalid base index %i\n", b->idx);
			cgi->Com_UnregisterConstList(saveBaseConstants);
			return false;
		}
		b->founded = true;
		const char* str = cgi->XML_GetString(base, SAVE_BASES_BASESTATUS);
		if (!cgi->Com_GetConstIntFromNamespace(SAVE_BASESTATUS_NAMESPACE, str, (int*) &b->baseStatus)) {
			Com_Printf("Invalid base status '%s'\n", str);
			cgi->Com_UnregisterConstList(saveBaseConstants);
			return false;
		}

		Q_strncpyz(b->name, cgi->XML_GetString(base, SAVE_BASES_NAME), sizeof(b->name));
		cgi->XML_GetPos3(base, SAVE_BASES_POS, b->pos);
		b->alienInterest = cgi->XML_GetFloat(base, SAVE_BASES_ALIENINTEREST, 0.0);
		b->aircraftCurrent = nullptr;

		/* building space*/
		xmlNode_t* node = cgi->XML_GetNode(base, SAVE_BASES_BUILDINGSPACE);
		FOREACH_XMLNODE(snode, node, SAVE_BASES_BUILDING) {
			/** @todo save it as vec2t if needed, also it's opposite */
			const int x = cgi->XML_GetInt(snode, SAVE_BASES_X, 0);
			const int y = cgi->XML_GetInt(snode, SAVE_BASES_Y, 0);
			baseBuildingTile_t* tile = &b->map[x][y];
			buildingIdx = cgi->XML_GetInt(snode, SAVE_BASES_BUILDINGINDEX, -1);

			tile->posX = y;	/* NOT a typo ! */
			tile->posY = x;
			if (buildingIdx != -1)
				/* The buildings are actually parsed _below_. (See PRE_MAXBUI loop) */
				tile->building = B_GetBuildingByIDX(i, buildingIdx);
			else
				tile->building = nullptr;
			tile->blocked = cgi->XML_GetBool(snode, SAVE_BASES_BLOCKED, false);
			if (tile->blocked && tile->building != nullptr) {
				Com_Printf("inconstent base layout found\n");
				cgi->Com_UnregisterConstList(saveBaseConstants);
				return false;
			}
		}
		/* buildings */
		node = cgi->XML_GetNode(base, SAVE_BASES_BUILDINGS);

		ccs.numBuildings[i] = 0;
		FOREACH_XMLNODE(snode, node, SAVE_BASES_BUILDING) {
			const int buildId = cgi->XML_GetInt(snode, SAVE_BASES_BUILDING_PLACE, MAX_BUILDINGS);
			building_t* building;
			const building_t* buildingTemplate;
			char buildingType[MAX_VAR];

			if (buildId >= MAX_BUILDINGS) {
				Com_Printf("building ID is greater than MAX buildings\n");
				cgi->Com_UnregisterConstList(saveBaseConstants);
				return false;
			}

			Q_strncpyz(buildingType, cgi->XML_GetString(snode, SAVE_BASES_BUILDINGTYPE), sizeof(buildingType));
			if (buildingType[0] == '\0') {
				Com_Printf("No buildingtype set\n");
				cgi->Com_UnregisterConstList(saveBaseConstants);
				return false;
			}

			buildingTemplate = B_GetBuildingTemplate(buildingType);
			if (!buildingTemplate)
				continue;

			ccs.buildings[i][buildId] = *buildingTemplate;
			building = B_GetBuildingByIDX(i, buildId);
			building->idx = B_GetBuildingIDX(b, building);
			if (building->idx != buildId) {
				Com_Printf("building ID doesn't match\n");
				cgi->Com_UnregisterConstList(saveBaseConstants);
				return false;
			}
			building->base = b;

			str = cgi->XML_GetString(snode, SAVE_BASES_BUILDINGSTATUS);
			if (!cgi->Com_GetConstIntFromNamespace(SAVE_BUILDINGSTATUS_NAMESPACE, str, (int*) &building->buildingStatus)) {
				Com_Printf("Invalid building status '%s'\n", str);
				cgi->Com_UnregisterConstList(saveBaseConstants);
				return false;
			}

			cgi->XML_GetDate(snode, SAVE_BASES_BUILDINGTIMESTART, &building->timeStart.day, &building->timeStart.sec);

			building->buildTime = cgi->XML_GetInt(snode, SAVE_BASES_BUILDINGBUILDTIME, 0);
			building->level = cgi->XML_GetFloat(snode, SAVE_BASES_BUILDINGLEVEL, 0);
			cgi->XML_GetPos2(snode, SAVE_BASES_POS, building->pos);
			ccs.numBuildings[i]++;
		}

		BDEF_InitialiseBaseSlots(b);
		/* read missile battery slots */
		node = cgi->XML_GetNode(base, SAVE_BASES_BATTERIES);
		if (node)
			b->numBatteries = B_LoadBaseSlotsXML(b->batteries, MAX_BASE_SLOT, node);
		/* read laser battery slots */
		node = cgi->XML_GetNode(base, SAVE_BASES_LASERS);
		if (node)
			b->numLasers = B_LoadBaseSlotsXML(b->lasers, MAX_BASE_SLOT, node);
		/* read equipment */
		node = cgi->XML_GetNode(base, SAVE_BASES_STORAGE);
		B_LoadStorageXML(node, &(b->storage));
		/* read radar info */
		RADAR_InitialiseUFOs(&b->radar);
		RADAR_Initialise(&b->radar, cgi->XML_GetInt(base, SAVE_BASES_RADARRANGE, 0), cgi->XML_GetInt(base, SAVE_BASES_TRACKINGRANGE, 0), B_GetMaxBuildingLevel(b, B_RADAR), true);

		node = cgi->XML_GetNode(base, SAVE_BASES_ALIENCONTAINMENT);
		if (node) {
			b->alienContainment = new AlienContainment(CAP_Get(b, CAP_ALIENS), nullptr);
			b->alienContainment->load(node);
		}

		/* clear the mess of stray loaded pointers */
		b->bEquipment.init();
	}
	cgi->Com_UnregisterConstList(saveBaseConstants);
	cgi->Cvar_SetValue("mn_base_count", B_GetCount());
	return true;
}

/**
 * @brief Check if an item is stored in storage.
 * @param[in] obj Pointer to the item to check.
 * @return True if item is stored in storage.
 */
bool B_ItemIsStoredInBaseStorage (const objDef_t* obj)
{
	/* antimatter is stored in antimatter storage */
	if (obj->isVirtual || Q_streq(obj->id, ANTIMATTER_ITEM_ID))
		return false;

	return true;
}

/**
 * @brief Add/remove items to/from the storage.
 * @param[in] base The base which storage and capacity should be updated
 * @param[in] obj The item.
 * @param[in] amount Amount to be added to removed
 * @return the added/removed amount
 */
int B_AddToStorage (base_t* base, const objDef_t* obj, int amount)
{
	capacities_t* cap;

	assert(base);
	assert(obj);

	if (!B_ItemIsStoredInBaseStorage(obj))
		return 0;

	cap = CAP_Get(base, CAP_ITEMS);
	if (amount > 0) {
		if (obj->size > 0)
			cap->cur += (amount * obj->size);
		base->storage.numItems[obj->idx] += amount;
	} else if (amount < 0) {
		/* correct amount */
		const int itemInBase = B_ItemInBase(obj, base);
		amount = std::max(amount, -itemInBase);
		if (obj->size > 0)
			cap->cur += (amount * obj->size);
		base->storage.numItems[obj->idx] += amount;

		if (base->storage.numItems[obj->idx] == 0) {
			technology_t* tech = RS_GetTechForItem(obj);
			if (tech->statusResearch == RS_RUNNING && tech->base == base)
				RS_StopResearch(tech);
		}
	}

	return amount;
}

/**
 * @brief returns the amount of antimatter stored in a base
 * @param[in] base Pointer to the base to check
 */
int B_AntimatterInBase (const base_t* base)
{
#ifdef DEBUG
	const objDef_t* od;

	od = INVSH_GetItemByID(ANTIMATTER_ITEM_ID);
	if (od == nullptr)
		cgi->Com_Error(ERR_DROP, "Could not find " ANTIMATTER_ITEM_ID " object definition");

	assert(base);
	assert(B_ItemInBase(od, base) == CAP_GetCurrent(base, CAP_ANTIMATTER));
#endif

	return CAP_GetCurrent(base, CAP_ANTIMATTER);
}

/**
 * @brief Manages antimatter (adding, removing) through Antimatter Storage Facility.
 * @param[in,out] base Pointer to the base.
 * @param[in] amount quantity of antimatter to add/remove
 * @returns amount of antimater in the storage after the action
 */
int B_AddAntimatter (base_t* base, int amount)
{
	const objDef_t* od;
	capacities_t* cap;

	assert(base);

	od = INVSH_GetItemByIDSilent(ANTIMATTER_ITEM_ID);
	if (od == nullptr)
		cgi->Com_Error(ERR_DROP, "Could not find " ANTIMATTER_ITEM_ID " object definition");

	cap = CAP_Get(base, CAP_ANTIMATTER);
	if (amount > 0) {
		cap->cur += amount;
		base->storage.numItems[od->idx] += amount;
	} else if (amount < 0) {
		/* correct amount */
		const int inBase = B_AntimatterInBase(base);
		amount = std::max(amount, -inBase);
		cap->cur += (amount);
		base->storage.numItems[od->idx] += amount;
	}

	return B_AntimatterInBase(base);
}
