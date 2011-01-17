/**
 * @file cp_base.c
 * @brief Handles everything that is located in or accessed trough a base.
 * @note Basemanagement functions prefix: B_
 * @note See "base/ufos/basemanagement.ufo" for the underlying content.
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

#include "../client.h" /* cl, cls */
#include "../cl_inventory.h" /* INV_GetEquipmentDefinitionByID */
#include "../ui/ui_main.h"
#include "../ui/ui_popup.h"
#include "../../shared/parse.h"
#include "cp_campaign.h"
#include "cp_mapfightequip.h"
#include "cp_capacity.h"
#include "cp_aircraft.h"
#include "cp_missions.h"
#include "cp_map.h"
#include "cp_popup.h"
#include "cp_radar.h"
#include "cp_time.h"
#include "cp_base_callbacks.h"
#include "cp_ufo.h"
#include "save/save_base.h"

#define B_GetBuildingByIDX(baseIdx, buildingIdx) (&ccs.buildings[(baseIdx)][(buildingIdx)])
#define B_GetBuildingIDX(base, building) ((ptrdiff_t)((building) - ccs.buildings[base->idx]))
#define B_GetBaseIDX(base) ((ptrdiff_t)((base) - ccs.bases))

static void B_PackInitialEquipment(aircraft_t *aircraft, const equipDef_t *ed);

typedef enum tileStatus_e {
	TILE_EMPTY,			/**< tile cannot be accessed (blocked / no building) */
	TILE_NODE,			/**< tile can be accessed but doesn't connect other tiles (unfinished building) */
	TILE_ACTIVENODE		/**< tile can be accessed and connect other tiles (free space / finished building) */
} tileStatus_t;

typedef tileStatus_t (*tileCondition_t) (const baseBuildingTile_t *);

/**
 * @brief Condition Function for B_BuildTileGraph for Non Blocked base tiles
 * @param[in] tile Pointer to the base Building Tile to check
 */
static tileStatus_t B_NonBlockedTileCondition (const baseBuildingTile_t *tile)
{
	return tile->blocked ? TILE_EMPTY : TILE_ACTIVENODE;
}

/**
 * @brief Condition Function for B_BuildTileGraph for Built base tiles
 * @param[in] tile Pointer to the base Building Tile to check
 */
static tileStatus_t B_BuiltTileCondition (const baseBuildingTile_t *tile)
{
	const building_t *building = tile->building;
	if (!building)
		return TILE_EMPTY;
	return B_IsBuildingBuiltUp(building) ? TILE_ACTIVENODE : TILE_NODE;
}

/**
 * @brief Builds up the traversal graph for base Tiles
 * @param[in] base Pointer to the base to build for
 * @param[in] condition Boolean Function reference to decide if the node is exist
 * @param[out] graph Linked list array of the traversal graph
 * @note graph should be a BASE_SIZE * BASE_SIZE sized linkedList_t* array
 */
static void B_BuildTileGraph (base_t *base, tileCondition_t condition, linkedList_t *graph[])
{
	int y;

	memset(graph, 0, sizeof(graph) * BASE_SIZE * BASE_SIZE);
	for (y = 0; y < BASE_SIZE; y++) {
		int x;
		for (x = 0; x < BASE_SIZE; x++) {
			baseBuildingTile_t *tile = &base->map[y][x];
			int listNum = y * BASE_SIZE + x;

			if (condition(tile) == TILE_EMPTY)
				continue;

			/* self */
			LIST_AddPointer(&graph[listNum], (void*)tile);

			if (condition(tile) == TILE_NODE)
				continue;
			/* west */
			if (x - 1 >= 0 && condition(&base->map[y][x - 1])) {
				if (!LIST_GetPointer(graph[listNum], &base->map[y][x - 1]))
					LIST_AddPointer(&graph[listNum], (void*)&base->map[y][x - 1]);
				if (!LIST_GetPointer(graph[listNum - 1], tile))
					LIST_AddPointer(&graph[listNum - 1], (void*)tile);
			}
			/* east */
			if (x + 1 < BASE_SIZE && condition(&base->map[y][x + 1])) {
				if (!LIST_GetPointer(graph[listNum], &base->map[y][x + 1]))
					LIST_AddPointer(&graph[listNum], (void*)&base->map[y][x + 1]);
				if (!LIST_GetPointer(graph[listNum + 1], tile))
					LIST_AddPointer(&graph[listNum + 1], (void*)tile);
			}
			/* north */
			if (y - 1 >= 0 && condition(&base->map[y - 1][x])) {
				if (!LIST_GetPointer(graph[listNum], &base->map[y - 1][x]))
					LIST_AddPointer(&graph[listNum], (void*)&base->map[y - 1][x]);
				if (!LIST_GetPointer(graph[listNum - BASE_SIZE], tile))
					LIST_AddPointer(&graph[listNum - BASE_SIZE], (void*)tile);
			}
			/* south */
			if (y + 1 < BASE_SIZE && condition(&base->map[y + 1][x])) {
				if (!LIST_GetPointer(graph[listNum], &base->map[y + 1][x]))
					LIST_AddPointer(&graph[listNum], (void*)&base->map[y + 1][x]);
				if (!LIST_GetPointer(graph[listNum + BASE_SIZE], tile))
					LIST_AddPointer(&graph[listNum + BASE_SIZE], (void*)tile);
			}
		}
	}
}

/**
 * @brief Add tile to base Tiles traversal graph
 * @param[in] base Pointer to the base to build for
 * @param[in] condition Boolean Function reference to decide if the node is exist
 * @param[in,out] graph Linked list array of the traversal graph
 * @param[in] tile Pointer to the new buildingtile
 * @note graph should be a BASE_SIZE * BASE_SIZE sized linkedList_t* array
 */
static void B_TileGraphAdd (base_t *base, tileCondition_t condition, linkedList_t *graph[], baseBuildingTile_t *tile)
{
	int x = tile->posX;
	int y = tile->posY;
	int listNum = y * BASE_SIZE + x;

	/* self */
	LIST_AddPointer(&graph[listNum], (void*)tile);

	/* west */
	if (x - 1 >= 0 && condition(&base->map[y][x - 1]) == TILE_ACTIVENODE) {
		if (!LIST_GetPointer(graph[listNum], &base->map[y][x - 1]))
			LIST_AddPointer(&graph[listNum], (void*)&base->map[y][x - 1]);
		if (!LIST_GetPointer(graph[listNum - 1], tile))
			LIST_AddPointer(&graph[listNum - 1], (void*)tile);
	}
	/* east */
	if (x + 1 < BASE_SIZE && condition(&base->map[y][x + 1]) == TILE_ACTIVENODE) {
		if (!LIST_GetPointer(graph[listNum], &base->map[y][x + 1]))
			LIST_AddPointer(&graph[listNum], (void*)&base->map[y][x + 1]);
		if (!LIST_GetPointer(graph[listNum + 1], tile))
			LIST_AddPointer(&graph[listNum + 1], (void*)tile);
	}
	/* north */
	if (y - 1 >= 0 && condition(&base->map[y - 1][x]) == TILE_ACTIVENODE) {
		if (!LIST_GetPointer(graph[listNum], &base->map[y - 1][x]))
			LIST_AddPointer(&graph[listNum], (void*)&base->map[y - 1][x]);
		if (!LIST_GetPointer(graph[listNum - BASE_SIZE], tile))
			LIST_AddPointer(&graph[listNum - BASE_SIZE], (void*)tile);
	}
	/* south */
	if (y + 1 < BASE_SIZE && condition(&base->map[y + 1][x]) == TILE_ACTIVENODE) {
		if (!LIST_GetPointer(graph[listNum], &base->map[y + 1][x]))
			LIST_AddPointer(&graph[listNum], (void*)&base->map[y + 1][x]);
		if (!LIST_GetPointer(graph[listNum + BASE_SIZE], tile))
			LIST_AddPointer(&graph[listNum + BASE_SIZE], (void*)tile);
	}
#if 0

	if (!LIST_IsEmpty(graph[listNum]))
		return;
	/* self */
	LIST_AddPointer(&graph[listNum], (void*)tile);
	/* west */
	if (x - 1 >= 0 && LIST_Count(graph[listNum - 1]) > 1) {
		LIST_AddPointer(&graph[listNum], (void*)&base->map[y][x - 1]);
		LIST_AddPointer(&graph[listNum - 1], (void*)tile);
	}
	/* east */
	if (x + 1 < BASE_SIZE && LIST_Count(graph[listNum + 1]) > 1) {
		LIST_AddPointer(&graph[listNum], (void*)&base->map[y][x + 1]);
		LIST_AddPointer(&graph[listNum + 1], (void*)tile);
	}
	/* north */
	if (y - 1 >= 0 && LIST_Count(graph[listNum - BASE_SIZE]) > 1) {
		LIST_AddPointer(&graph[listNum], (void*)&base->map[y - 1][x]);
		LIST_AddPointer(&graph[listNum - BASE_SIZE], (void*)tile);
	}
	/* south */
	if (y + 1 < BASE_SIZE && LIST_Count(graph[listNum + BASE_SIZE]) > 1) {
		LIST_AddPointer(&graph[listNum], (void*)&base->map[y + 1][x]);
		LIST_AddPointer(&graph[listNum + BASE_SIZE], (void*)tile);
	}
#endif
}

/**
 * @brief Remove tile from base Tiles traversal graph
 * @param[in] B_TileCondition Boolean Function reference to decide if the node is exist
 * @param[in,out] graph Linked list array of the traversal graph
 * @param[in] tile Pointer to the buildingtile to remove
 * @note graph should be a BASE_SIZE * BASE_SIZE sized linkedList_t* array
 */
static void B_TileGraphRemove (linkedList_t *graph[], baseBuildingTile_t *tile)
{
	int x = tile->posX;
	int y = tile->posY;
	int listNum = y * BASE_SIZE + x;

	if (LIST_IsEmpty(graph[listNum]))
		return;
	/* west */
	if (x - 1 >= 0 && !LIST_IsEmpty(graph[listNum - 1])) {
		LIST_Remove(&graph[listNum - 1], (void*)tile);
	}
	/* east */
	if (x + 1 < BASE_SIZE && !LIST_IsEmpty(graph[listNum + 1])) {
		LIST_Remove(&graph[listNum + 1], (void*)tile);
	}
	/* north */
	if (y - 1 >= 0 && !LIST_IsEmpty(graph[listNum - BASE_SIZE])) {
		LIST_Remove(&graph[listNum - BASE_SIZE], (void*)tile);
	}
	/* south */
	if (y + 1 < BASE_SIZE && !LIST_IsEmpty(graph[listNum + BASE_SIZE])) {
		LIST_Remove(&graph[listNum + BASE_SIZE], (void*)tile);
	}
	/* delete self list */
	LIST_Delete(&graph[listNum]);
}

/**
 * @brief Check if the building tile graph coherent
 * @param[in,out] graph Linked list array of the traversal graph
 * @note graph must be a BASE_SIZE * BASE_SIZE sized linkedList_t* array
 * @note this function destroy the tileGraph while working
 */
static qboolean B_IsCoherent (linkedList_t *graph[])
{
	linkedList_t *queue = NULL;
	qboolean coherent = qtrue;
	int i;

	/* add first node to the queue */
	for (i = 0; i < BASE_SIZE * BASE_SIZE; i++) {
		if (!LIST_IsEmpty(graph[i])) {
			LIST_AddPointer(&queue, graph[i]->data);
			break;
		}
	}

	/* iterate through accessible nodes */
	while (!LIST_IsEmpty(queue)) {
		baseBuildingTile_t *tile = (baseBuildingTile_t*)LIST_GetByIdx(queue, 0);
		baseBuildingTile_t *neighbour;

		LIST_RemoveEntry(&queue, queue);
		Com_DPrintf(DEBUG_CLIENT, "Base tile graph traversal at %2i, %2i\n", tile->posX, tile->posY);
		LIST_Foreach(graph[tile->posY * BASE_SIZE + tile->posX], baseBuildingTile_t, neighbour) {
			if (LIST_GetPointer(queue, neighbour))
				continue;
			LIST_AddPointer(&queue, neighbour);
		}
		/* Remove the neighbourhood list */
		LIST_Delete(&graph[tile->posY * BASE_SIZE + tile->posX]);
	}

	/* if there is still a single non-empty list one or mode node was unaccessible */
	for (i = 0; i < BASE_SIZE * BASE_SIZE; i++) {
		if (!LIST_IsEmpty(graph[i])) {
			coherent = qfalse;
			LIST_Delete(&graph[i]);
		}
	}
	LIST_Delete(&queue);

	return coherent;
}

/**
 * @brief Returns if a base building is destroyable
 * @param[in] building Pointer to the building to check
 * @todo buildings being built are not in the graph and it is possible to remove the only connection to them
 */
qboolean B_IsBuildingDestroyable (building_t *building)
{
	base_t *base = building->base;

	if (base->baseStatus != BASE_DESTROYED) {
		linkedList_t *tileGraph[BASE_SIZE * BASE_SIZE];

		B_BuildTileGraph(base, B_BuiltTileCondition, tileGraph);
		B_TileGraphRemove(tileGraph, &base->map[(int)building->pos[1]][(int)building->pos[0]]);
		if (building->needs)
			B_TileGraphRemove(tileGraph, &base->map[(int)building->pos[1]][((int)building->pos[0]) + 1]);
		return B_IsCoherent(tileGraph);
	}
	return qtrue;
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
 * @param[in] lastBase Pointer of the base to iterate from. call with NULL to get the first one.
 */
base_t *B_GetNext (base_t *lastBase)
{
	base_t* endOfBases = &ccs.bases[B_GetCount()];
	base_t* base;

	if (!B_GetCount())
		return NULL;

	if (!lastBase)
		return ccs.bases;
	assert(lastBase >= ccs.bases);
	assert(lastBase < endOfBases);

	base = lastBase;

	base++;
	if (base >= endOfBases)
		return NULL;
	else
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
		return NULL;

	return &ccs.bases[baseIdx];
}

/**
 * @brief Array bound check for the base index.
 * @param[in] baseIdx Index to check
 * @return Pointer to the base corresponding to baseIdx if base is founded, NULL else.
 */
base_t* B_GetFoundedBaseByIDX (int baseIdx)
{
	if (baseIdx >= B_GetCount())
		return NULL;

	return B_GetBaseByIDX(baseIdx);
}

/**
 * @brief Iterates through buildings in a base
 * @param[in] base Pointer to the base which buildings asked
 * @param[in] lastBuilding Pointer to the building iterate from. Call with NULL to get the first one.
 */
building_t* B_GetNextBuilding (const base_t *base, building_t *lastBuilding)
{
	building_t *endOfBuildings = B_GetBuildingByIDX(base->idx, ccs.numBuildings[base->idx]);
	building_t *building;

	if (!ccs.numBuildings[base->idx])
		return NULL;

	if (!lastBuilding)
		return ccs.buildings[base->idx];
	assert(lastBuilding >= ccs.buildings[base->idx]);
	assert(lastBuilding < endOfBuildings);

	building = lastBuilding;

	building++;
	if (building >= endOfBuildings)
		return NULL;
	else
		return building;
}

/**
 * @brief Iterates throught buildings of a type in a base
 * @param[in] base Pointer to the base which buildings asked
 * @param[in] lastBuilding Pointer to the building iterate from. Call with NULL to get the first one.
 * @param[in] buildingType Type of the buildings to search
 * @sa buildingType_t
 */
building_t* B_GetNextBuildingByType (const base_t *base, building_t *lastBuilding, buildingType_t buildingType)
{
	building_t *building = lastBuilding;

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
 * count of that type with the status you are searching - might also be NULL
 * if you are not interested in this value
 * @note If you are searching for a quarter (e.g.) you should perform a
 * <code>if (hasBuilding[B_QUARTERS])</code> check - this should speed things up a lot
 * @return true if building with status was found
 */
qboolean B_CheckBuildingTypeStatus (const base_t* const base, buildingType_t type, buildingStatus_t status, int *cnt)
{
	int cntlocal = 0;
	building_t *building = NULL;

	while ((building = B_GetNextBuildingByType(base, building, type))) {
		if (building->buildingStatus == status) {
			cntlocal++;
			/* don't count any further - the caller doesn't want to know the value */
			if (!cnt)
				return qtrue;
		}
	}

	/* set the cnt pointer if the caller wants to know this value */
	if (cnt)
		*cnt = cntlocal;

	return cntlocal ? qtrue : qfalse;
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
qboolean B_GetBuildingStatus (const base_t* const base, const buildingType_t buildingType)
{
	assert(base);

	if (buildingType == B_MISC)
		return qtrue;
	else if (buildingType < MAX_BUILDING_TYPE)
		return base->hasBuilding[buildingType];
	else {
		Com_Printf("B_GetBuildingStatus: Building-type %i does not exist.\n", buildingType);
		return qfalse;
	}
}


/**
 * @brief Set status associated to a building
 * @param[in] base Base to check
 * @param[in] buildingType value of building->buildingType
 * @param[in] newStatus New value of the status
 * @sa B_GetBuildingStatus
 */
void B_SetBuildingStatus (base_t* const base, const buildingType_t buildingType, qboolean newStatus)
{
	assert(base);

	if (buildingType == B_MISC)
		Com_Printf("B_SetBuildingStatus: No status is associated to B_MISC type of building.\n");
	else if (buildingType < MAX_BUILDING_TYPE) {
		base->hasBuilding[buildingType] = newStatus;
		Com_DPrintf(DEBUG_CLIENT, "B_SetBuildingStatus: set status for %i to %i\n", buildingType, newStatus);
	} else
		Com_Printf("B_SetBuildingStatus: Type of building %i does not exist\n", buildingType);
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
		base->buildingCurrent = NULL;
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
		building_t *building = NULL;
		while ((building = B_GetNextBuildingByType(base, building, type)))
			if (building->buildingStatus == B_STATUS_WORKING)
				max = max(building->level, max);
	}

	return max;
}

/**
 * @brief Perform the base assembling in case of an alien attack
 * @param[in,out] base The base to assemble
 * @return @c true if the assembly was successful, @c false if it failed
 * @todo Search a empty field and add a alien craft there
 * @todo If a building is still under construction, it will be assembled as a finished part.
 * Otherwise we need mapparts for all the maps under construction.
 */
qboolean B_AssembleMap (const base_t *base)
{
	int row, col;
	char maps[2048];
	char coords[2048];
	qboolean used[MAX_BUILDINGS];

	if (!base) {
		Com_Printf("B_AssembleMap: No base to assemble\n");
		return qfalse;
	}

	maps[0] = '\0';
	coords[0] = '\0';

	/* reset the used flag */
	OBJZERO(used);

	for (row = 0; row < BASE_SIZE; row++) {
		for (col = 0; col < BASE_SIZE; col++) {
			const building_t *entry = base->map[row][col].building;
			if (entry && B_IsBuildingBuiltUp(entry)) {
				/* basemaps with needs are not (like the images in B_DrawBase) two maps - but one
				 * this is why we check the used flag and continue if it was set already */
				if (entry->needs) {
					if (B_BuildingGetUsed(used, entry->idx))
						continue;
					B_BuildingSetUsed(used, entry->idx);
				}

				if (!entry->mapPart)
					Com_Error(ERR_DROP, "MapPart for building '%s' is missing'", entry->id);

				Q_strcat(maps, va("b/%s ", entry->mapPart), sizeof(maps));
			} else if (entry && ccs.date.day > entry->timeStart) {
				Q_strcat(maps, "b/construction ", sizeof(maps));
			} else {
				Q_strcat(maps, "b/empty ", sizeof(maps));
			}

			Q_strcat(coords, va("%i %i %i ", col * BASE_TILE_UNITS, (BASE_SIZE - row - 1) * BASE_TILE_UNITS, 0), sizeof(coords));
		}
	}

	/* set maxlevel for base attacks */
	cl.mapMaxLevel = BASE_MAX_WORLDLEVEL;

	SAV_QuickSave();

	Cbuf_AddText(va("map %s \"%s\" \"%s\"\n", (MAP_IsNight(base->pos) ? "night" : "day"), maps, coords));

	return qtrue;
}

/**
 * @brief Check base status for particular buildings as well as capacities.
 * @param[in] building Pointer to building.
 * @note This function checks  base status for particular buildings and base capacities.
 * @return qtrue if a base status has been modified (but do not check capacities)
 */
static qboolean B_CheckUpdateBuilding (building_t* building)
{
	qboolean oldValue;

	assert(building);

	/* Status of Miscellenious buildings cannot change. */
	if (building->buildingType == B_MISC)
		return qfalse;

	oldValue = B_GetBuildingStatus(building->base, building->buildingType);
	if (building->buildingStatus == B_STATUS_WORKING
	 && B_CheckBuildingDependencesStatus(building))
		B_SetBuildingStatus(building->base, building->buildingType, qtrue);
	else
		B_SetBuildingStatus(building->base, building->buildingType, qfalse);

	if (B_GetBuildingStatus(building->base, building->buildingType) != oldValue) {
		Com_DPrintf(DEBUG_CLIENT, "Status of building %s is changed to %i.\n",
			building->name, B_GetBuildingStatus(building->base, building->buildingType));
		return qtrue;
	}

	return qfalse;
}

/**
 * @brief Actions to perform when a type of buildings goes from qfalse to qtrue.
 * @note This function is not only called when a building is enabled for the first time in base
 * @note but also when one of its dependencies is destroyed and then rebuilt
 * @param[in] type Type of building that has been modified from qfalse to qtrue
 * @param[in] base Pointer to base with given building.
 * @sa B_UpdateOneBaseBuildingStatusOnDisable
 * @sa onConstruct trigger
 */
static void B_UpdateOneBaseBuildingStatusOnEnable (buildingType_t type, base_t* base)
{
	switch (type) {
	case B_RADAR:
		Cmd_ExecuteString(va("update_base_radar_coverage %i", base->idx));
		break;
	case B_COMMAND:
		Cmd_ExecuteString("mn_installation_update_max_count");
		break;
	default:
		break;
	}
}

/**
 * @brief Actions to perform when a type of buildings goes from functional to non-functional.
 * @param[in] type Type of building which hasBuilding value has been modified from qtrue to qfalse
 * @param[in] base Pointer to base with given building.
 * @note That does not mean that a building of this type has been destroyed: maybe one of its dependencies
 * has been destroyed: don't use onDestroy trigger.
 * @sa B_UpdateOneBaseBuildingStatusOnEnable
 * @sa onDestroy trigger
 */
static void B_UpdateOneBaseBuildingStatusOnDisable (buildingType_t type, base_t* base)
{
	switch (type) {
	case B_ALIEN_CONTAINMENT:
		/* if an alien containment is not functional, aliens die... */
		AC_KillAll(base);
		break;
	case B_RADAR:
		Cmd_ExecuteString(va("update_base_radar_coverage %i", base->idx));
		break;
	case B_COMMAND:
		Cmd_ExecuteString("mn_installation_update_max_count");
		break;
	default:
		break;
	}
}

/**
 * @brief Update status of every building when a building has been built/destroyed
 * @param[in] base
 * @param[in] buildingType The building-type that has been built / removed.
 * @param[in] onBuilt qtrue if building has been built, qfalse else
 * @sa B_BuildingDestroy
 * @sa B_UpdateAllBaseBuildingStatus
 * @return qtrue if at least one building status has been modified
 */
static qboolean B_UpdateStatusBuilding (base_t* base, buildingType_t buildingType, qboolean onBuilt)
{
	qboolean test = qfalse;
	qboolean returnValue = qfalse;
	building_t *building = NULL;

	/* Construction / destruction may have changed the status of other building
	 * We check that, but only for buildings which needed building */
	while ((building = B_GetNextBuilding(base, building))) {
		const building_t *dependsBuilding = building->dependsBuilding;
		if (dependsBuilding && buildingType == dependsBuilding->buildingType) {
			/* ccs.buildings[base->idx][i] needs built/removed building */
			if (onBuilt && !B_GetBuildingStatus(base, building->buildingType)) {
				/* we can only activate a non operationnal building */
				if (B_CheckUpdateBuilding(building)) {
					B_UpdateOneBaseBuildingStatusOnEnable(building->buildingType, base);
					test = qtrue;
					returnValue = qtrue;
				}
			} else if (!onBuilt && B_GetBuildingStatus(base, building->buildingType)) {
				/* we can only deactivate an operationnal building */
				if (B_CheckUpdateBuilding(building)) {
					B_UpdateOneBaseBuildingStatusOnDisable(building->buildingType, base);
					test = qtrue;
					returnValue = qtrue;
				}
			}
		}
	}
	/* and maybe some updated status have changed status of other building.
	 * So we check again, until nothing changes. (no condition here for check, it's too complex) */
	while (test) {
		test = qfalse;
		building = NULL;
		while ((building = B_GetNextBuilding(base, building))) {
			if (onBuilt && !B_GetBuildingStatus(base, building->buildingType)) {
				/* we can only activate a non operationnal building */
				if (B_CheckUpdateBuilding(building)) {
					B_UpdateOneBaseBuildingStatusOnEnable(building->buildingType, base);
					test = qtrue;
				}
			} else if (!onBuilt && B_GetBuildingStatus(base, building->buildingType)) {
				/* we can only deactivate an operationnal building */
				if (B_CheckUpdateBuilding(building)) {
					B_UpdateOneBaseBuildingStatusOnDisable(building->buildingType, base);
					test = qtrue;
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
static void B_UpdateAntimatterCap (base_t *base)
{
	const objDef_t *od = INVSH_GetItemByID(ANTIMATTER_TECH_ID);
	if (od != NULL)
		base->capacities[CAP_ANTIMATTER].cur = B_ItemInBase(od, base);
}

/**
 * @brief Recalculate status and capacities of one base
 * @param[in] base Pointer to the base where status and capacities must be recalculated
 * @param[in] firstEnable qtrue if this is the first time the function is called for this base
 * @sa B_UpdateOneBaseBuildingStatusOnEnable
 */
void B_ResetAllStatusAndCapacities (base_t *base, qboolean firstEnable)
{
	int i;
	qboolean test = qtrue;

	assert(base);

	Com_DPrintf(DEBUG_CLIENT, "Reseting base %s:\n", base->name);

	/* reset all values of hasBuilding[] */
	for (i = 0; i < MAX_BUILDING_TYPE; i++) {
		if (i != B_MISC)
			B_SetBuildingStatus(base, i, qfalse);
	}
	/* activate all buildings that needs to be activated */
	while (test) {
		building_t *building = NULL;
		test = qfalse;
		while ((building = B_GetNextBuilding(base, building))) {
			if (!B_GetBuildingStatus(base, building->buildingType)
			 && B_CheckUpdateBuilding(building)) {
				if (firstEnable)
					B_UpdateOneBaseBuildingStatusOnEnable(building->buildingType, base);
				test = qtrue;
			}
		}
	}

	/* Update all capacities of base */
	B_UpdateBaseCapacities(MAX_CAP, base);

	/* calculate capacities.cur for every capacity */
	if (B_GetBuildingStatus(base, B_GetBuildingTypeByCapacity(CAP_ALIENS)))
		base->capacities[CAP_ALIENS].cur = AL_CountInBase(base);

	if (B_GetBuildingStatus(base, B_GetBuildingTypeByCapacity(CAP_AIRCRAFT_SMALL)) ||
		B_GetBuildingStatus(base, B_GetBuildingTypeByCapacity(CAP_AIRCRAFT_BIG)))
		AIR_UpdateHangarCapForAll(base);

	if (B_GetBuildingStatus(base, B_GetBuildingTypeByCapacity(CAP_EMPLOYEES)))
		base->capacities[CAP_EMPLOYEES].cur = E_CountAllHired(base);

	if (B_GetBuildingStatus(base, B_GetBuildingTypeByCapacity(CAP_ITEMS)))
		B_UpdateStorageCap(base);

	if (B_GetBuildingStatus(base, B_GetBuildingTypeByCapacity(CAP_LABSPACE)))
		base->capacities[CAP_LABSPACE].cur = RS_CountScientistsInBase(base);

	if (B_GetBuildingStatus(base, B_GetBuildingTypeByCapacity(CAP_WORKSPACE)))
		PR_UpdateProductionCap(base);

	if (B_GetBuildingStatus(base, B_GetBuildingTypeByCapacity(CAP_ANTIMATTER)))
		B_UpdateAntimatterCap(base);

	/* Check that current capacity is possible -- if we changed values in *.ufo */
	for (i = 0; i < MAX_CAP; i++)
		if (base->capacities[i].cur > base->capacities[i].max)
			Com_Printf("B_ResetAllStatusAndCapacities: Warning, capacity of %i is bigger than maximum capacity\n", i);
}

/**
 * @brief Removes a building from the given base
 * @param[in] building The building to remove
 * @note Also updates capacities and sets the hasBuilding[] values in base_t
 * @sa B_BuildingDestroy_f
 */
qboolean B_BuildingDestroy (building_t* building)
{
	const buildingType_t buildingType = building->buildingType;
	const building_t const *buildingTemplate = building->tpl;
	const qboolean onDestroyCommand = (building->onDestroy[0] != '\0') && (building->buildingStatus == B_STATUS_WORKING);
	base_t *base = building->base;

	/* Don't allow to destroy a mandatory building. */
	if (building->mandatory)
		return qfalse;

	if (!base->map[(int)building->pos[1]][(int)building->pos[0]].building
	 || base->map[(int)building->pos[1]][(int)building->pos[0]].building != building) {
		Com_Error(ERR_DROP, "B_BuildingDestroy: building mismatch at base %i pos %i,%i.",
			base->idx, (int)building->pos[0], (int)building->pos[1]);
	}

	/* Refuse destroying if it hurts coherency - only exception is when the whole base destroys */
	if (!B_IsBuildingDestroyable(building)) {
		return qfalse;
	}

	/* Remove the building from the base map */
	if (building->needs) {
		/* "Child" building is always right to the "parent" building". */
		base->map[(int)building->pos[1]][((int)building->pos[0]) + 1].building = NULL;
	}
	base->map[(int)building->pos[1]][(int)building->pos[0]].building = NULL;

	building->buildingStatus = B_STATUS_NOT_SET;

	/* Update buildingCurrent */
	if (base->buildingCurrent > building)
		base->buildingCurrent--;
	else if (base->buildingCurrent == building)
		base->buildingCurrent = NULL;

	{
		int const baseIDX = base->idx;
		building_t* const buildings = ccs.buildings[baseIDX];
		int const idx = building->idx;
		int cntBldgs;
		int i;

		REMOVE_ELEM(buildings, idx, ccs.numBuildings[baseIDX]);

		/* Update the link of other buildings */
		cntBldgs = ccs.numBuildings[baseIDX];
		for (i = 0; i < cntBldgs; i++)
			if (buildings[i].idx >= idx) {
				buildings[i].idx--;
				base->map[(int)buildings[i].pos[1]][(int)buildings[i].pos[0]].building = &buildings[i];
				if (buildings[i].needs)
					base->map[(int)buildings[i].pos[1]][(int)buildings[i].pos[0] + 1].building = &buildings[i];
			}

		building = NULL;
	}
	/** @note Don't use the building pointer after this point - it's zeroed. */

	if (buildingType != B_MISC && buildingType != MAX_BUILDING_TYPE) {
		if (B_GetNumberOfBuildingsInBaseByBuildingType(base, buildingType) <= 0) {
			/* there is no more building of this type */
			B_SetBuildingStatus(base, buildingType, qfalse);
			/* check if this has an impact on other buildings */
			B_UpdateStatusBuilding(base, buildingType, qfalse);
			/* we may have changed status of several building: update all capacities */
			B_UpdateBaseCapacities(MAX_CAP, base);
		} else {
			/* there is still at least one other building of the same type in base: just update capacity */
			const baseCapacities_t cap = B_GetCapacityFromBuildingType(buildingType);
			if (cap != MAX_CAP)
				B_UpdateBaseCapacities(cap, base);
		}
	}

	Cmd_ExecuteString("base_init");

	/* call ondestroy trigger only if building is not under construction
	 * (we do that after base capacity has been updated) */
	if (onDestroyCommand) {
		Com_DPrintf(DEBUG_CLIENT, "B_BuildingDestroy: %s %i %i;\n",
			buildingTemplate->onDestroy, base->idx, buildingType);
		Cmd_ExecuteString(va("%s %i %i", buildingTemplate->onDestroy, base->idx, buildingType));
	}

	return qtrue;
}

/**
 * @brief Will ensure that aircraft on geoscape are not stored
 * in a base that no longer has any hangar left
 * @param[in] base The base that is going to be destroyed
 * @todo this should be merged into B_RemoveAircraftExceedingCapacity
 */
static void B_MoveAircraftOnGeoscapeToOtherBases (const base_t *base)
{
	aircraft_t *aircraft;
	aircraft_t *prevAircraft = NULL;

	AIR_ForeachFromBase(aircraft, base) {
		if (AIR_IsAircraftOnGeoscape(aircraft)) {
			base_t *newbase = NULL;
			qboolean moved = qfalse;
			while ((newbase = B_GetNext(newbase)) != NULL) {
				/* found a new homebase? */
				if (base != newbase && AIR_MoveAircraftIntoNewHomebase(aircraft, newbase)) {
					moved = qtrue;
					break;
				}
			}
			if (!moved) {
				/* No base can hold this aircraft */
				UFO_NotifyPhalanxAircraftRemoved(aircraft);
				if (!MapIsWater(MAP_GetColor(aircraft->pos, MAPTYPE_TERRAIN))) {
					CP_SpawnRescueMission(aircraft, NULL);
				} else {
					/* Destroy the aircraft and everything onboard - the aircraft pointer
					 * is no longer valid after this point */
					AIR_DestroyAircraft(aircraft);
				}
				aircraft = prevAircraft;
			}
		}
		prevAircraft = aircraft;
	}
}

/**
 * @brief Destroy a base.
 * @param[in] base Pointer to base to be destroyed.
 * @note If you want to sell items or unhire employees, you should do it before
 * calling this function - they are going to be killed / destroyed.
 */
void B_Destroy (base_t *base)
{
	int buildingIdx;

	assert(base);
	base->baseStatus = BASE_DESTROYED;

	CP_MissionNotifyBaseDestroyed(base);
	B_MoveAircraftOnGeoscapeToOtherBases(base);

	/* do a reverse loop as buildings are going to be destroyed */
	for (buildingIdx = ccs.numBuildings[base->idx] - 1; buildingIdx >= 0; buildingIdx--) {
		building_t *building = B_GetBuildingByIDX(base->idx, buildingIdx);
		B_BuildingDestroy(building);
	}

	E_DeleteAllEmployees(base);

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
	int baseIdx;
	base_t *base;

	if (Cmd_Argc() < 2) {
		Com_Printf("Usage: %s <baseIdx>\n", Cmd_Argv(0));
		return;
	}

	baseIdx = atoi(Cmd_Argv(1));

	if (baseIdx < 0 || baseIdx >= MAX_BASES) {
		Com_Printf("B_Destroy_f: baseIdx %i is outside bounds\n", baseIdx);
		return;
	}

	base = B_GetFoundedBaseByIDX(baseIdx);
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
	int numberOfBuildings = 0;

	assert(building);

	Cvar_Set("mn_building_status", _("Not set"));

	switch (building->buildingStatus) {
	case B_STATUS_NOT_SET:
		numberOfBuildings = B_GetNumberOfBuildingsInBaseByTemplate(B_GetCurrentSelectedBase(), building->tpl);
		if (numberOfBuildings >= 0)
			Cvar_Set("mn_building_status", va(_("Already %i in base"), numberOfBuildings));
		break;
	case B_STATUS_UNDER_CONSTRUCTION:
		Cvar_Set("mn_building_status", "");
		break;
	case B_STATUS_CONSTRUCTION_FINISHED:
		Cvar_Set("mn_building_status", _("Construction finished"));
		break;
	case B_STATUS_WORKING:
		if (B_CheckBuildingDependencesStatus(building)) {
			Cvar_Set("mn_building_status", _("Working 100%"));
		} else {
			assert (building->dependsBuilding);
			/** @todo shorten text or provide more space in overview popup */
			Cvar_Set("mn_building_status", va("%s %s", _("Not operational, depends on"), _(building->dependsBuilding->name)));
		}
		break;
	case B_STATUS_DOWN:
		Cvar_Set("mn_building_status", _("Down"));
		break;
	default:
		break;
	}
}

/**
 * @brief Updates base status for particular buildings as well as capacities.
 * @param[in] building Pointer to building.
 * @param[in] status Enum of buildingStatus_t which is status of given building.
 * @note This function checks whether a building has B_STATUS_WORKING status, and
 * then updates base status for particular buildings and base capacities.
 */
static void B_UpdateAllBaseBuildingStatus (building_t* building, buildingStatus_t status)
{
	qboolean test;
	buildingStatus_t oldStatus;
	base_t* base = building->base;

	assert(base);

	oldStatus = building->buildingStatus;
	building->buildingStatus = status;

	/* we update the status of the building (we'll call this building building 1) */
	test = B_CheckUpdateBuilding(building);
	if (test)
		B_UpdateOneBaseBuildingStatusOnEnable(building->buildingType, base);

	/* now, the status of this building may have changed the status of other building.
	 * We check that, but only for buildings which needed building 1 */
	if (test) {
		B_UpdateStatusBuilding(base, building->buildingType, qtrue);
		/* we may have changed status of several building: update all capacities */
		B_UpdateBaseCapacities(MAX_CAP, base);
	} else {
		/* no other status than status of building 1 has been modified
		 * update only status of building 1 */
		const int cap = B_GetCapacityFromBuildingType(building->buildingType);
		if (cap != MAX_CAP)
			B_UpdateBaseCapacities(cap, base);
	}

	/** @todo this should be an user option defined in Game Options. */
	if (oldStatus == B_STATUS_UNDER_CONSTRUCTION && (status == B_STATUS_CONSTRUCTION_FINISHED || status == B_STATUS_WORKING)) {
		if (B_CheckBuildingDependencesStatus(building))
			CL_GameTimeStop();
	} else {
		CL_GameTimeStop();
	}
}

/**
 * @brief Build starting building in the first base, and hire employees.
 * @param[in,out] base The base to put the new building into
 * @param[in] buildingTemplate The building template to create a new building with
 * @param[in] hire Hire employees for the building we create from the template
 * @param[in] pos The position on the base grid
 */
static void B_AddBuildingToBasePos (base_t *base, const building_t const *buildingTemplate, qboolean hire, const vec2_t pos)
{
	/* new building in base (not a template) */
	building_t *buildingNew;

	/* fake a click to basemap */
	buildingNew = B_SetBuildingByClick(base, buildingTemplate, (int)pos[1], (int)pos[0]);
	if (!buildingNew)
		return;
	buildingNew->timeStart = 0;
	B_UpdateAllBaseBuildingStatus(buildingNew, B_STATUS_WORKING);
	Com_DPrintf(DEBUG_CLIENT, "Base %i new building: %s at (%.0f:%.0f)\n",
			base->idx, buildingNew->id, buildingNew->pos[0], buildingNew->pos[1]);

	if (hire)
		E_HireForBuilding(base, buildingNew, -1);

	/* now call the onconstruct trigger */
	if (buildingNew->onConstruct[0] != '\0') {
		Com_DPrintf(DEBUG_CLIENT, "B_AddBuildingToBasePos: %s %i;\n",
			buildingNew->onConstruct, base->idx);
		Cmd_ExecuteString(va("%s %i", buildingNew->onConstruct, base->idx));
	}
}

/**
 * @brief Prepares initial equipment for first base at the beginning of the campaign.
 * @param[in] base Pointer to first base.
 * @param[in] assignInitialAircraft aircraft on which the soldiers (to equip) are
 * @param[in] ed Initial equipment definition
 * @param[in] edTarget storage to put items to
 * @sa B_BuildBase_f
 * @todo Make sure all equipment including soldiers equipment is added to capacity.cur.
 */
static void B_InitialEquipment (base_t *base, aircraft_t *assignInitialAircraft, const equipDef_t *ed, equipDef_t *edTarget)
{
	int i, price = 0;

	assert(base);
	assert(edTarget);

	/* Initial soldiers and their equipment. */
	if (assignInitialAircraft) {
		B_PackInitialEquipment(assignInitialAircraft, ed);
	} else {
		for (i = 0; i < csi.numODs; i++)
			edTarget->numItems[i] += ed->numItems[i] / 5;
	}

	/* Pay for the initial equipment as well as update storage capacity. */
	for (i = 0; i < csi.numODs; i++) {
		const objDef_t *od = INVSH_GetItemByIDX(i);
		price += edTarget->numItems[i] * od->price;
		base->capacities[CAP_ITEMS].cur += edTarget->numItems[i] * od->size;
	}

	/* Finally update credits. */
	CL_UpdateCredits(ccs.credits - price);
}

/**
 * @brief builds a base from template
 * @param[out] base The base to build
 * @param[in] templateName Templated used for building. If @c NULL no template
 * will be used.
 * @param[in] hire If hiring employee needed
 * @note It builds an empty base on NULL (or empty) templatename
 */
static void B_BuildFromTemplate (base_t *base, const char *templateName, qboolean hire)
{
	const baseTemplate_t *baseTemplate = B_GetBaseTemplate(templateName);
	int freeSpace = BASE_SIZE * BASE_SIZE;
	int i;

	assert(base);

	for (i = 0; i < MAX_CAP; i++)
		base->capacities[i].cur = 0;

	if (baseTemplate) {
		/* find each building in the template */
		for (i = 0; i < baseTemplate->numBuildings; i++) {
			const baseBuildingTile_t *buildingTile = &baseTemplate->buildings[i];

			if (!base->map[buildingTile->posY][buildingTile->posX].building) {
				vec2_t pos;
				Vector2Set(pos, buildingTile->posX, buildingTile->posY);
				B_AddBuildingToBasePos(base, buildingTile->building, hire, pos);
				freeSpace--;
			}
		}
	}

	/* we need to set up the mandatory buildings */
	for (i = 0; i < ccs.numBuildingTemplates; i++) {
		building_t* building = &ccs.buildingTemplates[i];
		vec2_t pos;

		if (!building->mandatory || B_GetBuildingStatus(base, building->buildingType))
			continue;

		while (freeSpace > 0 && !B_GetBuildingStatus(base, building->buildingType)) {
			int x = rand() % BASE_SIZE;
			int y = rand() % BASE_SIZE;
			Vector2Set(pos, x, y);
			if (!base->map[y][x].building) {
				B_AddBuildingToBasePos(base, building, hire, pos);
				freeSpace--;
			}
		}
		/** @todo if there is no more space for mandatory building, remove a non mandatory one
		 * or build mandatory ones first */
		if (!B_GetBuildingStatus(base, building->buildingType))
			Com_Error(ERR_DROP, "B_BuildFromTemplate: Cannot build base. No space for it's buildings!");
	}

	/* set building tile positions */
	for (i = 0; i < BASE_SIZE; i++) {
		int j;
		for (j = 0; j < BASE_SIZE; j++) {
			base->map[i][j].posY = i;
			base->map[i][j].posX = j;
		}
	}

	/* Create random blocked fields in the base.
	 * The first base never has blocked fields so we skip it. */
	if (ccs.campaignStats.basesBuilt >= 1) {
		linkedList_t *tileGraph[BASE_SIZE * BASE_SIZE];
		const int j = round((frand() * (MAX_BLOCKEDFIELDS - MIN_BLOCKEDFIELDS)) + MIN_BLOCKEDFIELDS);

		for (i = 0; i < j;) {
			const int x = rand() % (BASE_SIZE);
			const int y = rand() % (BASE_SIZE);
			baseBuildingTile_t *mapPtr = &base->map[y][x];

			if (mapPtr->building || mapPtr->blocked)
				continue;

			/* refuse if it hurts base coherency */
			B_BuildTileGraph(base, B_NonBlockedTileCondition, tileGraph);
			B_TileGraphRemove(tileGraph, mapPtr);
			if (!B_IsCoherent(tileGraph)) {
				Com_DPrintf(DEBUG_CLIENT, "No blocked tile should be put at %i, %i\n", x, y);
				continue;
			}

			mapPtr->blocked = qtrue;
			i++;
			freeSpace--;
		}
	}
}

/**
 * @brief Setup buildings and equipment for first base. Uses the campaign
 * scriptable first base template to place the buildings in the base.
 * @param[in] campaign The campaign data structure
 * @param[in,out] base The base to set up
 * @sa B_SetUpBase
 */
static void B_SetUpFirstBase (const campaign_t *campaign, base_t* base)
{
	const equipDef_t *ed;

	if (campaign->firstBaseTemplate[0] == '\0')
		Com_Error(ERR_DROP, "No base template for setting up the first base given");

	RS_MarkResearchable(qtrue, base);
	BS_InitMarket(campaign);
	E_InitialEmployees(campaign);

	/* Find the initial equipment definition for current campaign. */
	ed = INV_GetEquipmentDefinitionByID(campaign->equipment);
	/* Copy it to base storage. */
	base->storage = *ed;

	B_BuildFromTemplate(base, campaign->firstBaseTemplate, qtrue);
	/* Add aircraft to the first base */
	/** @todo move aircraft to .ufo */
	/* buy two first aircraft and hire pilots for them. */
	if (B_GetBuildingStatus(base, B_HANGAR)) {
		const equipDef_t *equipDef = INV_GetEquipmentDefinitionByID(campaign->soldierEquipment);
		const char *firebird = Com_DropShipTypeToShortName(DROPSHIP_FIREBIRD);
		const aircraft_t *firebirdAircraft = AIR_GetAircraft(firebird);
		aircraft_t *aircraft = AIR_NewAircraft(base, firebirdAircraft);
		CL_UpdateCredits(ccs.credits - firebirdAircraft->price);
		if (!E_HireEmployeeByType(base, EMPL_PILOT))
			Com_Error(ERR_DROP, "B_SetUpFirstBase: Hiring pilot failed.");
		/* Assign and equip soldiers on Dropships */
		AIR_AssignInitial(aircraft);
		B_InitialEquipment(base, aircraft, equipDef, &base->storage);
	}
	if (B_GetBuildingStatus(base, B_SMALL_HANGAR)) {
		const char *stiletto = Com_DropShipTypeToShortName(INTERCEPTOR_STILETTO);
		const aircraft_t *stilettoAircraft = AIR_GetAircraft(stiletto);
		aircraft_t *aircraft = AIR_NewAircraft(base, stilettoAircraft);
		CL_UpdateCredits(ccs.credits - stilettoAircraft->price);
		if (!E_HireEmployeeByType(base, EMPL_PILOT))
			Com_Error(ERR_DROP, "B_SetUpFirstBase: Hiring pilot failed.");
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
	base_t *base = NULL;

	/* count working Command Centers */
	while ((base = B_GetNext(base)) != NULL) {
		if (B_GetBuildingStatus(base, B_COMMAND))
			limit++;
	}

	return min(MAX_INSTALLATIONS, limit * MAX_INSTALLATIONS_PER_BASE);
}

/**
 * @brief Set the base name
 * @param[out] base The base to set the name for
 * @param[in] name The name for the base. This might already be in utf-8 as
 * it's the user input from the UI
 */
void B_SetName (base_t *base, const char *name)
{
	Q_strncpyz(base->name, name, sizeof(base->name));
}

/**
 * @brief Setup new base, uses template for the first base
 * @param[in] campaign The campaign data structure
 * @param[in,out] base The base to set up
 * @param[in] pos Position (on Geoscape) the base built at
 * @param[in] name The name of the new base, this string might already be in utf-8
 * @sa B_NewBase
 * @sa B_SetUpFirstBase
 */
void B_SetUpBase (const campaign_t *campaign, base_t* base, const vec2_t pos, const char *name)
{
	const int newBaseAlienInterest = 1.0f;
	float level;

	B_SetName(base, name);
	Vector2Copy(pos, base->pos);

	base->idx = B_GetBaseIDX(base);
	base->founded = qtrue;
	base->baseStatus = BASE_WORKING;

	/* increase this early because a lot of functions rely on this
	 * value to get the base we are setting up here */
	ccs.numBases++;

	/* setup for first base */
	if (ccs.campaignStats.basesBuilt == 0)
		B_SetUpFirstBase(campaign, base);
	else
		B_BuildFromTemplate(base, NULL, qtrue);

	/* a new base is not discovered (yet) */
	base->alienInterest = newBaseAlienInterest;

	BDEF_InitialiseBaseSlots(base);

	/* Reset Radar range */
	level = B_GetMaxBuildingLevel(base, B_RADAR);
	RADAR_Initialise(&base->radar, RADAR_BASERANGE, RADAR_BASETRACKINGRANGE, level, qfalse);
	RADAR_InitialiseUFOs(&base->radar);

	B_ResetAllStatusAndCapacities(base, qtrue);
	AL_FillInContainment(base);
	PR_UpdateProductionCap(base);

	ccs.campaignStats.basesBuilt++;
	ccs.mapAction = MA_NONE;
}

/**
 * @brief Returns the baseTemplate in the global baseTemplate list that has the unique name baseTemplateID.
 * @param[in] baseTemplateID The unique id of the building (baseTemplate_t->name).
 * @return baseTemplate_t If a Template was found it is returned, otherwise->NULL.
 */
const baseTemplate_t *B_GetBaseTemplate (const char *baseTemplateID)
{
	int i = 0;

	if (!baseTemplateID)
		return NULL;

	for (i = 0; i < ccs.numBaseTemplates; i++)
		if (Q_streq(ccs.baseTemplates[i].id, baseTemplateID))
			return &ccs.baseTemplates[i];

	Com_Printf("Base Template %s not found\n", baseTemplateID);
	return NULL;
}

/**
 * @brief Set the currently selected building.
 * @param[in,out] base The base to place the building in
 * @param[in] buildingTemplate The template of the building to place at the given location
 * @param[in] row Set building to row
 * @param[in] col Set building to col
 * @return building created in base (this is not a building template)
 */
building_t* B_SetBuildingByClick (base_t *base, const building_t const *buildingTemplate, int row, int col)
{
#ifdef DEBUG
	if (!base)
		Com_Error(ERR_DROP, "no current base\n");
	if (!buildingTemplate)
		Com_Error(ERR_DROP, "no current building\n");
#endif
	if (!CP_CheckCredits(buildingTemplate->fixCosts)) {
		UI_Popup(_("Notice"), _("Not enough credits to build this\n"));
		return NULL;
	}

	/* template should really be a template */
	/*assert(template == template->tpl);*/

	if (0 <= row && row < BASE_SIZE && 0 <= col && col < BASE_SIZE) {
		/* new building in base (not a template) */
		building_t *buildingNew = B_GetBuildingByIDX(base->idx, ccs.numBuildings[base->idx]);

		/* copy building from template list to base-buildings-list */
		*buildingNew = *buildingTemplate;
		/* self-link to building-list in base */
		buildingNew->idx = B_GetBuildingIDX(base, buildingNew);
		/* Link to the base. */
		buildingNew->base = base;
		/* status and build (start) time */
		buildingNew->buildingStatus = B_STATUS_UNDER_CONSTRUCTION;
		buildingNew->timeStart = ccs.date.day;

		if (!base->map[row][col].blocked && !base->map[row][col].building) {
			baseBuildingTile_t tile;
			baseBuildingTile_t tileNeeded;
			linkedList_t *tileGraph[BASE_SIZE * BASE_SIZE];

			OBJZERO(tile);
			OBJZERO(tileNeeded);

			/* No building in this place */
			if (buildingNew->needs) {
				if (col + 1 == BASE_SIZE) {
					if (base->map[row][col - 1].building
					 || base->map[row][col - 1].blocked) {
						Com_DPrintf(DEBUG_CLIENT, "Can't place this building here - the second part overlapped with another building or invalid field\n");
						return NULL;
					}
					col--;
				} else if (base->map[row][col + 1].building || base->map[row][col + 1].blocked) {
					if (base->map[row][col - 1].building
					 || base->map[row][col - 1].blocked
					 || !col) {
						Com_DPrintf(DEBUG_CLIENT, "Can't place this building here - the second part overlapped with another building or invalid field\n");
						return NULL;
					}
					col--;
				}
				/* needed building tile for coherency check */
				tileNeeded.building = buildingNew;
				tileNeeded.posX = col + 1;
				tileNeeded.posY = row;
			}

			/* Refuse adding if it hurts coherency */
			/** @todo this first check about unmodified base is just to keep compatibility with savegames. Should be removed */
			B_BuildTileGraph(base, B_BuiltTileCondition, tileGraph);
			if (B_IsCoherent(tileGraph)) {
				B_BuildTileGraph(base, B_BuiltTileCondition, tileGraph);
				tile.building = buildingNew;
				tile.posX = col;
				tile.posY = row;
				B_TileGraphAdd(base, B_BuiltTileCondition, tileGraph, &tile);
				if (buildingNew->needs)
					B_TileGraphAdd(base, B_BuiltTileCondition, tileGraph, &tileNeeded);
				if (!B_IsCoherent(tileGraph)) {
					UI_Popup(_("Notice"), _("You must build next to existing buildings."));
					return qfalse;
				}
			}

			/* set building position */
			if (buildingNew->needs) {
				base->map[row][col + 1].building = buildingNew;
			}
			base->map[row][col].building = buildingNew;
			buildingNew->pos[0] = col;
			buildingNew->pos[1] = row;

			/* pay */
			CL_UpdateCredits(ccs.credits - buildingNew->fixCosts);
			/* Update number of buildings on the base. */
			ccs.numBuildings[base->idx]++;

			B_BuildingStatus(buildingNew);
			B_ResetBuildingCurrent(base);
			Cmd_ExecuteString("base_init");
			Cmd_ExecuteString("building_init");

			return buildingNew;
		}
	}
	return NULL;
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

	if (building->buildingStatus == B_STATUS_UNDER_CONSTRUCTION)
		Q_strcat(buildingText, va(ngettext("%i Day to build\n", "%i Days to build\n", building->buildTime), building->buildTime), sizeof(buildingText));

	if (building->varCosts)
		Q_strcat(buildingText, va(_("Running costs:\t%i c\n"), building->varCosts), sizeof(buildingText));

	if (building->dependsBuilding)
		Q_strcat(buildingText, va(_("Needs:\t%s\n"), _(building->dependsBuilding->name)), sizeof(buildingText));

	if (building->name)
		Cvar_Set("mn_building_name", _(building->name));

	if (building->image)
		Cvar_Set("mn_building_image", building->image);
	else
		Cvar_Set("mn_building_image", "base/empty");

	/* link into menu text array */
	UI_RegisterText(TEXT_BUILDING_INFO, buildingText);
}

/**
 * @brief Counts the number of buildings of a particular type in a base.
 * @param[in] base Which base to count in.
 * @param[in] tpl The template type in the ccs.buildingTemplates list.
 * @return The number of buildings or -1 on error (e.g. base index out of range)
 */
int B_GetNumberOfBuildingsInBaseByTemplate (const base_t *base, const building_t *tpl)
{
	int numberOfBuildings = 0;
	building_t *building = NULL;

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
int B_GetNumberOfBuildingsInBaseByBuildingType (const base_t *base, const buildingType_t buildingType)
{
	int numberOfBuildings = 0;
	building_t *building = NULL;

	if (!base) {
		Com_Printf("B_GetNumberOfBuildingsInBaseByBuildingType: No base given!\n");
		return -1;
	}

	if (buildingType >= MAX_BUILDING_TYPE) {
		Com_Printf("B_GetNumberOfBuildingsInBaseByBuildingType: no sane building-type given!\n");
		return -1;
	}

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
 * @return The building or NULL if base has no building of that type
 */
const building_t *B_GetBuildingInBaseByType (const base_t* base, buildingType_t buildingType, qboolean onlyWorking)
{
	building_t *building = NULL;

	/* we maybe only want to get the working building (e.g. it might the
	 * case that we don't have a powerplant and thus the searched building
	 * is not functional) */
	if (onlyWorking && !B_GetBuildingStatus(base, buildingType))
		return NULL;

	while ((building = B_GetNextBuildingByType(base, building, buildingType)))
		return building;

	return NULL;
}

/**
 * @brief Reads a base layout template
 * @param[in] name The script id of the base template
 * @param[in] text The script block to parse
 * @sa CL_ParseScriptFirst
 */
void B_ParseBaseTemplate (const char *name, const char **text)
{
	const char *errhead = "B_ParseBaseTemplate: unexpected end of file (names ";
	const char *token;
	baseTemplate_t* baseTemplate;
	baseBuildingTile_t* tile;
	vec2_t pos;
	qboolean map[BASE_SIZE][BASE_SIZE];
	byte buildingNums[MAX_BUILDINGS];
	int i;

	/* get token */
	token = Com_Parse(text);

	if (!*text || *token != '{') {
		Com_Printf("B_ParseBaseTemplate: Template \"%s\" without body ignored\n", name);
		return;
	}

	if (ccs.numBaseTemplates >= MAX_BASETEMPLATES)
		Com_Error(ERR_DROP, "B_ParseBaseTemplate: too many base templates");

	/* create new Template */
	baseTemplate = &ccs.baseTemplates[ccs.numBaseTemplates];
	baseTemplate->id = Mem_PoolStrDup(name, cp_campaignPool, 0);

	/* clear map for checking duplicate positions and buildingNums for checking moreThanOne constraint */
	OBJZERO(map);
	OBJZERO(buildingNums);

	ccs.numBaseTemplates++;

	do {
		/* get the building */
		token = Com_EParse(text, errhead, baseTemplate->id);
		if (!*text)
			break;
		if (*token == '}')
			break;

		if (baseTemplate->numBuildings >= MAX_BASEBUILDINGS)
			Com_Error(ERR_DROP, "B_ParseBaseTemplate: too many buildings");

		/* check if building type is known */
		tile = &baseTemplate->buildings[baseTemplate->numBuildings];
		baseTemplate->numBuildings++;

		for (i = 0; i < ccs.numBuildingTemplates; i++)
			if (Q_streq(ccs.buildingTemplates[i].id, token)) {
				tile->building = &ccs.buildingTemplates[i];
				if (tile->building->maxCount >= 0 && tile->building->maxCount <= buildingNums[i])
					Com_Error(ERR_DROP, "B_ParseBaseTemplate: Found more %s than allowed in template %s (%d))", token, baseTemplate->id, tile->building->maxCount);
				buildingNums[i]++;
				break;
			}

		if (!tile->building)
			Com_Error(ERR_DROP, "B_ParseBaseTemplate: Could not find building with id %s\n", baseTemplate->id);

		/* get the position */
		token = Com_EParse(text, errhead, baseTemplate->id);
		if (!*text)
			break;
		if (*token == '}')
			break;

		Com_EParseValue(pos, token, V_POS, 0, sizeof(vec2_t));
		tile->posX = pos[0];
		tile->posY = pos[1];
		if (tile->posX < 0 || tile->posX >= BASE_SIZE || tile->posY < 0 || tile->posY >= BASE_SIZE)
			Com_Error(ERR_DROP, "Invalid template coordinates for building %s in template %s given",
					tile->building->id, baseTemplate->id);

		/* check for buildings on same position */
		if (map[tile->posY][tile->posX])
			Com_Error(ERR_DROP, "Base template '%s' has ambiguous positions for buildings set.", baseTemplate->id);
		map[tile->posY][tile->posX] = qtrue;
	} while (*text);

	/* templates without the must-have buildings can't be used */
	for (i = 0; i < ccs.numBuildingTemplates; i++) {
		const building_t *building = &ccs.buildingTemplates[i];
		if (building && building->mandatory && !buildingNums[i]) {
			Com_Error(ERR_DROP, "Every base template needs one '%s'! '%s' has none.", building->id, baseTemplate->id);
		}
	}
}

/**
 * @brief Get the first unfounded base
 * @return first unfounded base or NULL if every available base slot is already filled
 */
base_t *B_GetFirstUnfoundedBase (void)
{
	int baseIdx;

	for (baseIdx = 0; baseIdx < MAX_BASES; baseIdx++) {
		base_t *base = B_GetBaseByIDX(baseIdx);
		if (!base->founded)
			return base;
	}

	return NULL;
}

/**
 * @brief Sets the selected base
 * @param[in] base The base that is going to be selected
 * @sa B_SelectBase
 */
void B_SetCurrentSelectedBase (const base_t *base)
{
	base_t *b = NULL;
	while ((b = B_GetNext(b)) != NULL) {
		if (b == base) {
			b->selected = qtrue;
			if (b->aircraftCurrent == NULL)
				b->aircraftCurrent = AIR_GetFirstFromBase(b);
		} else
			b->selected = qfalse;
	}

	if (base) {
		INS_SetCurrentSelectedInstallation(NULL);
		Cvar_Set("mn_base_title", base->name);
		Cvar_SetValue("mn_base_status_id", base->baseStatus);
	} else {
		Cvar_Set("mn_base_title", "");
		Cvar_Set("mn_base_status_id", "");
	}
}

/**
 * @brief returns the currently selected base
 */
base_t *B_GetCurrentSelectedBase (void)
{
	base_t *base = NULL;
	while ((base = B_GetNext(base)) != NULL)
		if (base->selected)
			return base;

	return NULL;
}

/**
 * @brief Select and opens a base
 * @param[in] base If this is @c NULL we want to build a new base
 */
void B_SelectBase (const base_t *base)
{
	/* set up a new base */
	if (!base) {
		/* if player hit the "create base" button while creating base mode is enabled
		 * that means that player wants to quit this mode */
		if (ccs.mapAction == MA_NEWBASE) {
			MAP_ResetAction();
			return;
		}

		if (B_GetCount() < MAX_BASES) {
			/* show radar overlay (if not already displayed) */
			if (!MAP_IsRadarOverlayActivated())
				MAP_SetOverlay("radar");
			ccs.mapAction = MA_NEWBASE;
		} else {
			ccs.mapAction = MA_NONE;
		}
	} else {
		Com_DPrintf(DEBUG_CLIENT, "B_SelectBase_f: select base with id %i\n", base->idx);
		ccs.mapAction = MA_NONE;
		UI_PushWindow("bases", NULL, NULL);
		B_SetCurrentSelectedBase(base);
	}
}

/**
 * @brief Swaps one skill from character1 to character 2 and vice versa.
 */
static void CL_SwapSkill (character_t *cp1, character_t *cp2, abilityskills_t skill)
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

static void CL_DoSwapSkills (character_t *cp1, character_t *cp2, const int skill)
{
	if (cp1->score.skills[skill] < cp2->score.skills[skill])
		CL_SwapSkill(cp1, cp2, skill);

	switch (skill) {
	case SKILL_CLOSE:
		if (cp1->score.skills[ABILITY_SPEED] < cp2->score.skills[ABILITY_SPEED])
			CL_SwapSkill(cp1, cp2, ABILITY_SPEED);
		break;
	case SKILL_HEAVY:
		if (cp1->score.skills[ABILITY_POWER] < cp2->score.skills[ABILITY_POWER])
			CL_SwapSkill(cp1, cp2, ABILITY_POWER);
		break;
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
		Com_Error(ERR_DROP, "CL_SwapSkills: illegal skill %i.\n", skill);
	}
}

/**
 * @brief Swaps skills of the initial team of soldiers so that they match inventories
 * @todo This currently always uses exactly the first two firemodes (see fmode1+fmode2) for calculation. This needs to be adapted to support less (1) or more 3+ firemodes. I think the function will even  break on only one firemode .. never tested it.
 * @todo i think currently also the different ammo/firedef types for each weapon (different weaponr_fd_idx and weaponr_fd_idx values) are ignored.
 */
static void CL_SwapSkills (chrList_t *team)
{
	int i, j, k, skill;
	const byte fmode1 = 0;
	const byte fmode2 = 1;

	i = team->num;
	while (i--) {
		/* running the loops below is not enough, we need transitive closure */
		/* I guess num times is enough --- could anybody prove this? */
		/* or perhaps 2 times is enough as long as weapons have 1 skill? */
		for (skill = ABILITY_NUM_TYPES; skill < SKILL_NUM_TYPES; skill++) {
			for (j = 0; j < team->num - 1; j++) {
				character_t *cp1 = team->chr[j];
				const fireDef_t *fdRightArray = NULL;
				const fireDef_t *fdHolsterArray = NULL;

				if (RIGHT(cp1) && RIGHT(cp1)->item.m && RIGHT(cp1)->item.t)
					fdRightArray = FIRESH_FiredefForWeapon(&RIGHT(cp1)->item);
				if (HOLSTER(cp1) && HOLSTER(cp1)->item.m && HOLSTER(cp1)->item.t)
					fdHolsterArray = FIRESH_FiredefForWeapon(&HOLSTER(cp1)->item);
				/* disregard left hand, or dual-wielding guys are too good */

				if (fdHolsterArray != NULL && fdRightArray != NULL) {
					const int no1 = 2 * (RIGHT(cp1) && skill == RIGHT(cp1)->item.m->fd[fdRightArray->weapFdsIdx][fmode1].weaponSkill)
						+ 2 * (RIGHT(cp1) && skill == RIGHT(cp1)->item.m->fd[fdRightArray->weapFdsIdx][fmode2].weaponSkill)
						+ (HOLSTER(cp1) && HOLSTER(cp1)->item.t->reload
						   && skill == HOLSTER(cp1)->item.m->fd[fdHolsterArray->weapFdsIdx][fmode1].weaponSkill)
						+ (HOLSTER(cp1) && HOLSTER(cp1)->item.t->reload
						   && skill == HOLSTER(cp1)->item.m->fd[fdHolsterArray->weapFdsIdx][fmode2].weaponSkill);

					for (k = j + 1; k < team->num; k++) {
						character_t *cp2 = team->chr[k];
						fdRightArray = NULL;
						fdHolsterArray = NULL;

						if (RIGHT(cp2) && RIGHT(cp2)->item.m && RIGHT(cp2)->item.t)
							fdRightArray = FIRESH_FiredefForWeapon(&RIGHT(cp2)->item);
						if (HOLSTER(cp2) && HOLSTER(cp2)->item.m && HOLSTER(cp2)->item.t)
							fdHolsterArray = FIRESH_FiredefForWeapon(&HOLSTER(cp2)->item);

						if (fdHolsterArray != NULL && fdRightArray != NULL) {
							const int no2 = 2 * (RIGHT(cp2) && skill == RIGHT(cp2)->item.m->fd[fdRightArray->weapFdsIdx][fmode1].weaponSkill)
								+ 2 * (RIGHT(cp2) && skill == RIGHT(cp2)->item.m->fd[fdRightArray->weapFdsIdx][fmode2].weaponSkill)
								+ (HOLSTER(cp2) && HOLSTER(cp2)->item.t->reload
								   && skill == HOLSTER(cp2)->item.m->fd[fdHolsterArray->weapFdsIdx][fmode1].weaponSkill)
								+ (HOLSTER(cp2) && HOLSTER(cp2)->item.t->reload
								   && skill == HOLSTER(cp2)->item.m->fd[fdHolsterArray->weapFdsIdx][fmode2].weaponSkill);

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
	}
}

/**
 * @brief Assigns initial soldier equipment for the first base
 * @todo Move this function to a better place - has nothing to do with bases anymore
 */
static void B_PackInitialEquipment (aircraft_t *aircraft, const equipDef_t *ed)
{
	base_t *base = aircraft->homebase;
	chrList_t chrListTemp;
	employee_t *employee;

	if (!aircraft)
		return;

	chrListTemp.num = 0;
	LIST_Foreach(aircraft->acTeam, employee_t, employee) {
		character_t *chr = &employee->chr;
		/* pack equipment */
		Com_DPrintf(DEBUG_CLIENT, "B_PackInitialEquipment: Packing initial equipment for %s.\n", chr->name);
		cls.i.EquipActor(&cls.i, &chr->i, ed, chr->teamDef);
		chrListTemp.chr[chrListTemp.num] = chr;
		chrListTemp.num++;
	}

	if (base) {
		AIR_MoveEmployeeInventoryIntoStorage(aircraft, &base->storage);
		B_UpdateStorageCap(base);
	}
	CL_SwapSkills(&chrListTemp);
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
	base_t *base;

	base = NULL;
	while ((base = B_GetNext(base)) != NULL) {
		int j;

		Com_Printf("\nBase id %i: %s\n", base->idx, base->name);
		/** @todo building count should not depend on base->idx. base->idx will not be an array index! */
		for (j = 0; j < ccs.numBuildings[base->idx]; j++) {
			const building_t *building = B_GetBuildingByIDX(base->idx, j);
			int k;

			Com_Printf("...Building: %s #%i - id: %i\n", building->id,
				B_GetNumberOfBuildingsInBaseByTemplate(base, building->tpl), base->idx);
			Com_Printf("...image: %s\n", building->image);
			Com_Printf(".....Status:\n");

			for (k = 0; k < BASE_SIZE * BASE_SIZE; k++) {
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
	int row, col, j;
	base_t *base;

	base = NULL;
	while ((base = B_GetNext(base)) != NULL) {
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
		Com_Printf("Misc  Lab Quar Stor Work Hosp Hang Cont SHgr UHgr SUHg Powr  Cmd AMtr Entr Miss Lasr  Rdr Team\n");
		for (j = 0; j < MAX_BUILDING_TYPE; j++)
			Com_Printf("  %i  ", B_GetBuildingStatus(base, j));
		Com_Printf("Base pos %.02f:%.02f\n", base->pos[0], base->pos[1]);
		Com_Printf("Base map:\n");
		for (row = 0; row < BASE_SIZE; row++) {
			if (row > 0)
				Com_Printf("\n");
			for (col = 0; col < BASE_SIZE; col++)
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
void B_BuildingOpenAfterClick (const building_t *building)
{
	const base_t *base = building->base;
	if (!B_GetBuildingStatus(base, building->buildingType)) {
		UP_OpenWith(building->pedia);
	} else {
		switch (building->buildingType) {
		case B_LAB:
			if (RS_ResearchAllowed(base))
				UI_PushWindow("research", NULL, NULL);
			else
				UP_OpenWith(building->pedia);
			break;
		case B_HOSPITAL:
			if (HOS_HospitalAllowed(base))
				UI_PushWindow("hospital", NULL, NULL);
			else
				UP_OpenWith(building->pedia);
			break;
		case B_ALIEN_CONTAINMENT:
			if (AC_ContainmentAllowed(base))
				UI_PushWindow("aliencont", NULL, NULL);
			else
				UP_OpenWith(building->pedia);
			break;
		case B_QUARTERS:
			if (E_HireAllowed(base))
				UI_PushWindow("employees", NULL, NULL);
			else
				UP_OpenWith(building->pedia);
			break;
		case B_WORKSHOP:
			if (PR_ProductionAllowed(base))
				UI_PushWindow("production", NULL, NULL);
			else
				UP_OpenWith(building->pedia);
			break;
		case B_DEFENCE_LASER:
		case B_DEFENCE_MISSILE:
			UI_PushWindow("basedefence", NULL, NULL);
			break;
		case B_HANGAR:
		case B_SMALL_HANGAR:
			if (!AIR_AircraftAllowed(base)) {
				UP_OpenWith(building->pedia);
			} else if (AIR_BaseHasAircraft(base)) {
				UI_PushWindow("aircraft", NULL, NULL);
			} else {
				UI_PushWindow("buyaircraft", NULL, NULL);
				/* transfer is only possible when there are at least two bases */
				if (B_GetCount() > 1)
					UI_Popup(_("Note"), _("No aircraft in this base - You first have to purchase or transfer an aircraft\n"));
				else
					UI_Popup(_("Note"), _("No aircraft in this base - You first have to purchase an aircraft\n"));
			}
			break;
		case B_STORAGE:
			if (BS_BuySellAllowed(base))
				UI_PushWindow("market", NULL, NULL);
			else
				UP_OpenWith(building->pedia);
			break;
		case B_ANTIMATTER:
			Com_sprintf(popupText, sizeof(popupText), "%s %d/%d", _("Antimatter (current/max):"), base->capacities[CAP_ANTIMATTER].cur, base->capacities[CAP_ANTIMATTER].max);
			UI_Popup(_("Information"), popupText);
			break;
		default:
			UP_OpenWith(building->pedia);
			break;
		}
	}
}

#ifdef DEBUG
/**
 * @brief Debug function for printing all capacities in given base.
 * @note called with debug_listcapacities
 */
static void B_PrintCapacities_f (void)
{
	int i, j;
	base_t *base;

	if (Cmd_Argc() < 2) {
		Com_Printf("Usage: %s <baseID>\n", Cmd_Argv(0));
		return;
	}

	i = atoi(Cmd_Argv(1));
	if (i >= B_GetCount()) {
		Com_Printf("invalid baseID (%s)\n", Cmd_Argv(1));
		return;
	}
	base = B_GetBaseByIDX(i);
	for (i = 0; i < MAX_CAP; i++) {
		const buildingType_t buildingType = B_GetBuildingTypeByCapacity(i);
		if (buildingType >= MAX_BUILDING_TYPE)
			Com_Printf("B_PrintCapacities_f: Could not find building associated with capacity %i\n", i);
		else {
			for (j = 0; j < ccs.numBuildingTemplates; j++) {
				if (ccs.buildingTemplates[j].buildingType == buildingType)
					break;
			}
			Com_Printf("Building: %s, capacity max: %i, capacity cur: %i\n",
			ccs.buildingTemplates[j].id, base->capacities[i].max, base->capacities[i].cur);
		}
	}
}

/**
 * @brief Finishes the construction of every building in the base
 */
static void B_BuildingConstructionFinished_f (void)
{
	int i;
	base_t *base = B_GetCurrentSelectedBase();

	if (!base)
		return;

	for (i = 0; i < ccs.numBuildings[base->idx]; i++) {
		building_t *building = B_GetBuildingByIDX(base->idx, i);

		if (building->buildingStatus == B_STATUS_UNDER_CONSTRUCTION) {
			B_UpdateAllBaseBuildingStatus(building, B_STATUS_WORKING);
			building->timeStart = 0;

			if (building->onConstruct[0] != '\0') {
				base->buildingCurrent = building;
				Cbuf_AddText(va("%s %i;", building->onConstruct, base->idx));
			}
		}
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
	base_t *base;

	base = NULL;
	while ((base = B_GetNext(base)) != NULL) {
		/* set buildingStatus[] and capacities.max values */
		B_ResetAllStatusAndCapacities(base, qfalse);
	}
}

/**
 * @brief Check base coherency
 */
static void B_CheckCoherency_f (void)
{
	base_t *base;

	if (Cmd_Argc() >= 2) {
		int i = atoi(Cmd_Argv(1));

		if (i < 0 || i >= B_GetCount()) {
			Com_Printf("Usage: %s [baseIdx]\nWithout baseIdx the current base is selected.\n", Cmd_Argv(0));
			return;
		}
		base = B_GetFoundedBaseByIDX(i);
	} else {
		base = B_GetCurrentSelectedBase();
	}

	if (base) {
		linkedList_t *bGraph[BASE_SIZE * BASE_SIZE];

		B_BuildTileGraph(base, B_BuiltTileCondition, bGraph);
		Com_Printf("Base '%s' (idx:%i) is %scoherent.\n", base->name, base->idx, (B_IsCoherent(bGraph)) ? "" : "not ");
	} else {
		Com_Printf("No base selected.\n");
	}
}
#endif

/**
 * @brief Resets console commands.
 * @sa UI_InitStartup
 */
void B_InitStartup (void)
{
#ifdef DEBUG
	Cmd_AddCommand("debug_listbase", B_BaseList_f, "Print base information to the game console");
	Cmd_AddCommand("debug_listbuilding", B_BuildingList_f, "Print building information to the game console");
	Cmd_AddCommand("debug_listcapacities", B_PrintCapacities_f, "Debug function to show all capacities in given base");
	Cmd_AddCommand("debug_basereset", B_ResetAllStatusAndCapacities_f, "Reset building status and capacities of all bases");
	Cmd_AddCommand("debug_destroybase", B_Destroy_f, "Destroy a base");
	Cmd_AddCommand("debug_buildingfinished", B_BuildingConstructionFinished_f, "Finish construction for every building in the current base");
	Cmd_AddCommand("debug_baseiscoherent", B_CheckCoherency_f, "Checks if all buildings are connected on a base");
#endif
}

/**
 * @brief Checks whether the construction of a building is finished.
 * Calls the onConstruct functions and assign workers, too.
 */
static int B_CheckBuildingConstruction (building_t *building)
{
	int newBuilding = 0;

	if (building->buildingStatus == B_STATUS_UNDER_CONSTRUCTION) {
		if (B_IsBuildingBuiltUp(building)) {
			B_UpdateAllBaseBuildingStatus(building, B_STATUS_WORKING);

			if (building->onConstruct[0] != '\0') {
				base_t *base = building->base;
				base->buildingCurrent = building;
				Com_DPrintf(DEBUG_CLIENT, "B_CheckBuildingConstruction: %s %i;\n", building->onConstruct, base->idx);
				Cbuf_AddText(va("%s %i;", building->onConstruct, base->idx));
			}

			newBuilding++;
		}
	}

	if (newBuilding)
		Cmd_ExecuteString("building_init");

	return newBuilding;
}

/**
 * @brief Updates base data
 * @sa CL_CampaignRun
 * @note called every "day"
 * @sa AIRFIGHT_ProjectileHitsBase
 */
void B_UpdateBaseData (void)
{
	base_t *base = NULL;
	while ((base = B_GetNext(base)) != NULL) {
		building_t *building = NULL;
		while ((building = B_GetNextBuilding(base, building))) {
			if (B_CheckBuildingConstruction(building)) {
				Com_sprintf(cp_messageBuffer, lengthof(cp_messageBuffer),
						_("Construction of %s building finished in %s."), _(building->name), base->name);
				MS_AddNewMessage(_("Building finished"), cp_messageBuffer, qfalse, MSG_CONSTRUCTION, NULL);
			}
		}
	}
}

/**
 * @brief Sell items to the market or add them to base storage.
 * @param[in] aircraft Pointer to an aircraft landing in base.
 * @sa B_AircraftReturnedToHomeBase
 */
static void B_SellOrAddItems (aircraft_t *aircraft)
{
	int i;
	int numitems = 0;
	int gained = 0;
	int forcedsold = 0;
	int forcedgained = 0;
	itemsTmp_t *cargo;
	base_t *base;

	assert(aircraft);
	base = aircraft->homebase;
	assert(base);

	cargo = aircraft->itemcargo;

	for (i = 0; i < aircraft->itemTypes; i++) {
		const objDef_t *item = cargo[i].item;
		const int amount = cargo[i].amount;
		technology_t *tech = RS_GetTechForItem(item);
		/* If the related technology is NOT researched, don't sell items. */
		if (!RS_IsResearched_ptr(tech)) {
			/* Items not researched cannot be thrown out even if not enough space in storage. */
			B_UpdateStorageAndCapacity(base, item, amount, qfalse, qtrue);
			if (amount > 0)
				RS_MarkCollected(tech);
			continue;
		} else {
			/* If the related technology is researched, check the autosell option. */
			if (ccs.eMarket.autosell[item->idx]) { /* Sell items if autosell is enabled. */
				BS_AddItemToMarket(item, amount);
				gained += (item->price * amount);
				numitems += amount;
			} else {
				int j;
				/* Check whether there is enough space for adding this item.
				 * If yes - add. If not - sell. */
				for (j = 0; j < amount; j++) {
					if (!B_UpdateStorageAndCapacity(base, item, 1, qfalse, qfalse)) {
						/* Not enough space, sell item. */
						BS_AddItemToMarket(item, 1);
						forcedgained += item->price;
						forcedsold++;
					}
				}
			}
			continue;
		}
	}

	if (numitems > 0) {
		Com_sprintf(cp_messageBuffer, lengthof(cp_messageBuffer), _("By selling %s you gathered %i credits."),
			va(ngettext("%i collected item", "%i collected items", numitems), numitems), gained);
		MS_AddNewMessage(_("Notice"), cp_messageBuffer, qfalse, MSG_STANDARD, NULL);
	}
	if (forcedsold > 0) {
		Com_sprintf(cp_messageBuffer, lengthof(cp_messageBuffer), _("Not enough storage space in %s. %s"),
			base->name, va(ngettext("%i item was sold for %i credits.", "%i items were sold for %i credits.", forcedsold), forcedsold, forcedgained));
		MS_AddNewMessage(_("Notice"), cp_messageBuffer, qfalse, MSG_STANDARD, NULL);
	}
	CL_UpdateCredits(ccs.credits + gained + forcedgained);

	/* ship no longer has cargo aboard */
	aircraft->itemTypes = 0;

	/* Mark new technologies researchable. */
	RS_MarkResearchable(qfalse, aircraft->homebase);
	/* Recalculate storage capacity, to fix wrong capacity if a soldier drops something on the ground */
	B_UpdateStorageCap(aircraft->homebase);
}

/**
 * @brief Will unload all cargo to the homebase
 * @param[in,out] aircraft The aircraft to dump
 */
void B_DumpAircraftToHomeBase (aircraft_t *aircraft)
{
	aliensTmp_t *cargo;

	/* Don't call cargo functions if aircraft is not a transporter. */
	if (aircraft->type != AIRCRAFT_TRANSPORTER)
		return;

	/* Add aliens to Alien Containment. */
	AL_AddAliens(aircraft);
	/* Sell collected items or add them to storage. */
	B_SellOrAddItems(aircraft);

	/* Now empty alien/item cargo just in case. */
	cargo = AL_GetAircraftAlienCargo(aircraft);
	OBJZERO(*cargo);
	OBJZERO(aircraft->itemcargo);
	AL_SetAircraftAlienCargoTypes(aircraft, 0);
}

/**
 * @brief Do anything when dropship returns to base
 * @param[in] aircraft Returning aircraft.
 * @note Place here any stuff, which should be called when Drophip returns to base.
 * @sa CL_CampaignRunAircraft
 */
void B_AircraftReturnedToHomeBase (aircraft_t* aircraft)
{
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
qboolean B_BaseHasItem (const base_t *base, const objDef_t *item)
{
	if (item->isVirtual)
		return qtrue;

	return B_ItemInBase(item, base) > 0;
}

/**
 * @brief Check if the item has been collected (i.e it is in the storage) in the given base.
 * @param[in] item The item to check
 * @param[in] base The base to search in.
 * @return amount Number of available items in base
 */
int B_ItemInBase (const objDef_t *item, const base_t *base)
{
	const equipDef_t *ed;

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
 * @note If hasBuilding is qfalse, the capacity is still increase: if power plant is destroyed and rebuilt, you shouldn't have to hire employees again
 */
void B_UpdateBaseCapacities (baseCapacities_t cap, base_t *base)
{
	int i, capacity = 0, buildingTemplateIDX = -1;
	buildingType_t buildingType;
	building_t *building;

	/* Get building type. */
	buildingType = B_GetBuildingTypeByCapacity(cap);

	switch (cap) {
	case CAP_ALIENS:		/**< Update Aliens capacity in base. */
	case CAP_EMPLOYEES:		/**< Update employees capacity in base. */
	case CAP_LABSPACE:		/**< Update laboratory space capacity in base. */
	case CAP_WORKSPACE:		/**< Update workshop space capacity in base. */
	case CAP_ITEMS:			/**< Update items capacity in base. */
	case CAP_AIRCRAFT_SMALL:	/**< Update aircraft capacity in base. */
	case CAP_AIRCRAFT_BIG:		/**< Update aircraft capacity in base. */
	case CAP_ANTIMATTER:		/**< Update antimatter capacity in base. */
		/* Reset given capacity in current base. */
		base->capacities[cap].max = 0;
		/* Get building capacity. */
		for (i = 0; i < ccs.numBuildingTemplates; i++) {
			const building_t *b = &ccs.buildingTemplates[i];
			if (b->buildingType == buildingType) {
				capacity = b->capacity;
				Com_DPrintf(DEBUG_CLIENT, "Building: %s capacity: %i\n", b->id, capacity);
				buildingTemplateIDX = i;
				break;
			}
		}
		/* Finally update capacity. */
		building = NULL;
		while ((building = B_GetNextBuildingByType(base, building, buildingType)))
			if (building->buildingStatus >= B_STATUS_CONSTRUCTION_FINISHED)
				base->capacities[cap].max += capacity;

		if (buildingTemplateIDX != -1)
			Com_DPrintf(DEBUG_CLIENT, "B_UpdateBaseCapacities: updated capacity of %s: %i\n",
				ccs.buildingTemplates[buildingTemplateIDX].id, base->capacities[cap].max);
		break;
	case MAX_CAP:			/**< Update all capacities in base. */
		Com_DPrintf(DEBUG_CLIENT, "B_UpdateBaseCapacities: going to update ALL capacities.\n");
		/* Loop through all capacities and update them. */
		for (i = 0; i < cap; i++) {
			B_UpdateBaseCapacities(i, base);
		}
		break;
	default:
		Com_Error(ERR_DROP, "Unknown capacity limit for this base: %i \n", cap);
	}
}

/**
 * @brief Saves the missile and laser slots of a base or sam site.
 * @param[in] weapons Defence weapons array
 * @param[in] numWeapons Number of entries in weapons array
 * @param[out] node XML Node structure, where we write the information to
 */
void B_SaveBaseSlotsXML (const baseWeapon_t *weapons, const int numWeapons, mxml_node_t *node)
{
	int i;

	for (i = 0; i < numWeapons; i++) {
		mxml_node_t *sub = mxml_AddNode(node, SAVE_BASES_WEAPON);
		AII_SaveOneSlotXML(sub, &weapons[i].slot, qtrue);
		mxml_AddBoolValue(sub, SAVE_BASES_AUTOFIRE, weapons[i].autofire);
		if (weapons[i].target)
			mxml_AddInt(sub, SAVE_BASES_TARGET, weapons[i].target->idx);
	}
}

/**
 * @brief Saves base storage
 * @param[out] parent XML Node structure, where we write the information to
 * @param[in] equip Storage to save
 */
qboolean B_SaveStorageXML (mxml_node_t *parent, const equipDef_t equip)
{
	int k;
	for (k = 0; k < csi.numODs; k++) {
		const objDef_t *od = INVSH_GetItemByIDX(k);
		if (equip.numItems[k] || equip.numItemsLoose[k]) {
			mxml_node_t *node = mxml_AddNode(parent, SAVE_BASES_ITEM);

			mxml_AddString(node, SAVE_BASES_ODS_ID, od->id);
			mxml_AddIntValue(node, SAVE_BASES_NUM, equip.numItems[k]);
			mxml_AddByteValue(node, SAVE_BASES_NUMLOOSE, equip.numItemsLoose[k]);
		}
	}
	return qtrue;
}
/**
 * @brief Save callback for saving in xml format.
 * @param[out] parent XML Node structure, where we write the information to
 */
qboolean B_SaveXML (mxml_node_t *parent)
{
	mxml_node_t * bases;
	base_t *b;

	bases = mxml_AddNode(parent, SAVE_BASES_BASES);
	b = NULL;
	while ((b = B_GetNext(b)) != NULL) {
		int k;
		mxml_node_t * act_base, *node;
		building_t *building;

		if (!b->founded) {
			Com_Printf("B_SaveXML: Base (idx: %i) not founded!\n", b->idx);
			return qfalse;
		}

		Com_RegisterConstList(saveBaseConstants);

		act_base = mxml_AddNode(bases, SAVE_BASES_BASE);
		mxml_AddString(act_base, SAVE_BASES_NAME, b->name);
		mxml_AddPos3(act_base, SAVE_BASES_POS, b->pos);
		mxml_AddString(act_base, SAVE_BASES_BASESTATUS, Com_GetConstVariable(SAVE_BASESTATUS_NAMESPACE, b->baseStatus));
		mxml_AddFloat(act_base, SAVE_BASES_ALIENINTEREST, b->alienInterest);
		if (b->aircraftCurrent)
			mxml_AddInt(act_base, SAVE_BASES_CURRENTAIRCRAFTIDX, AIR_GetAircraftIDXInBase(b->aircraftCurrent));

		/* building space */
		node = mxml_AddNode(act_base, SAVE_BASES_BUILDINGSPACE);
		for (k = 0; k < BASE_SIZE; k++) {
			int l;
			for (l = 0; l < BASE_SIZE; l++) {
				mxml_node_t * snode = mxml_AddNode(node, SAVE_BASES_BUILDING);
				/** @todo save it as vec2t if needed, also it's opposite */
				mxml_AddInt(snode, SAVE_BASES_X, k);
				mxml_AddInt(snode, SAVE_BASES_Y, l);
				if (b->map[k][l].building)
					mxml_AddInt(snode, SAVE_BASES_BUILDINGINDEX, b->map[k][l].building->idx);
				mxml_AddBoolValue(snode, SAVE_BASES_BLOCKED, b->map[k][l].blocked);
			}
		}
		/* buildings */
		node = mxml_AddNode(act_base, SAVE_BASES_BUILDINGS);
		building = NULL;
		while ((building = B_GetNextBuilding(b, building))) {
			mxml_node_t * snode;

			if (!building->tpl)
				continue;

			snode = mxml_AddNode(node, SAVE_BASES_BUILDING);
			mxml_AddString(snode, SAVE_BASES_BUILDINGTYPE, building->tpl->id);
			mxml_AddInt(snode, SAVE_BASES_BUILDING_PLACE, building->idx);
			mxml_AddString(snode, SAVE_BASES_BUILDINGSTATUS, Com_GetConstVariable(SAVE_BUILDINGSTATUS_NAMESPACE, building->buildingStatus));
			mxml_AddInt(snode, SAVE_BASES_BUILDINGTIMESTART, building->timeStart);
			mxml_AddInt(snode, SAVE_BASES_BUILDINGBUILDTIME, building->buildTime);
			mxml_AddFloat(snode, SAVE_BASES_BUILDINGLEVEL, building->level);
			mxml_AddPos2(snode, SAVE_BASES_POS, building->pos);
		}
		/* base defences */
		node = mxml_AddNode(act_base, SAVE_BASES_BATTERIES);
		B_SaveBaseSlotsXML(b->batteries, b->numBatteries, node);
		node = mxml_AddNode(act_base, SAVE_BASES_LASERS);
		B_SaveBaseSlotsXML(b->lasers, b->numLasers, node);
		/* store equipment */
		node = mxml_AddNode(act_base, SAVE_BASES_STORAGE);
		B_SaveStorageXML(node, b->storage);
		/* radar */
		mxml_AddIntValue(act_base, SAVE_BASES_RADARRANGE, b->radar.range);
		mxml_AddIntValue(act_base, SAVE_BASES_TRACKINGRANGE, b->radar.trackingRange);

		Com_UnregisterConstList(saveBaseConstants);
	}
	return qtrue;
}

/**
 * @brief Loads the missile and laser slots of a base or sam site.
 * @param[out] weapons Defence weapons array
 * @param[out] max Number of entries in weapons array
 * @param[in] p XML Node structure, where we load the information from
 * @sa B_Load
 * @sa B_SaveBaseSlots
 */
int B_LoadBaseSlotsXML (baseWeapon_t* weapons, int max, mxml_node_t *p)
{
	int i;
	mxml_node_t *s;
	for (i = 0, s = mxml_GetNode(p, SAVE_BASES_WEAPON); s && i < max; i++, s = mxml_GetNextNode(s, p, SAVE_BASES_WEAPON)) {
		const int target = mxml_GetInt(s, SAVE_BASES_TARGET, -1);
		AII_LoadOneSlotXML(s, &weapons[i].slot, qtrue);
		weapons[i].autofire = mxml_GetBool(s, SAVE_BASES_AUTOFIRE, qtrue);
		weapons[i].target = (target >= 0) ? UFO_GetByIDX(target) : NULL;
	}
	return i;
}

/**
 * @brief Set the capacity stuff for all the bases after loading a savegame
 * @sa B_PostLoadInit
 */
static qboolean B_PostLoadInitCapacity (void)
{
	base_t *base = NULL;
	while ((base = B_GetNext(base)) != NULL)
		B_ResetAllStatusAndCapacities(base, qtrue);

	return qtrue;
}

/**
 * @brief Set the capacity stuff for all the bases after loading a savegame
 * @sa SAV_GameActionsAfterLoad
 */
qboolean  B_PostLoadInit (void)
{
	return B_PostLoadInitCapacity();
}

/**
 * @brief Loads base storage
 * @param[in] parent XML Node structure, where we get the information from
 * @param[out] equip Storage to load
 */
qboolean B_LoadStorageXML (mxml_node_t *parent, equipDef_t *equip)
{
	mxml_node_t *node;
	for (node = mxml_GetNode(parent, SAVE_BASES_ITEM); node; node = mxml_GetNextNode(node, parent, SAVE_BASES_ITEM)) {
		const char *s = mxml_GetString(node, SAVE_BASES_ODS_ID);
		objDef_t *od = INVSH_GetItemByID(s);

		if (!od) {
			Com_Printf("B_Load: Could not find item '%s'\n", s);
		} else {
			equip->numItems[od->idx] = mxml_GetInt(node, SAVE_BASES_NUM, 0);
			equip->numItemsLoose[od->idx] = mxml_GetInt(node, SAVE_BASES_NUMLOOSE, 0);
		}
	}
	return qtrue;
}

/**
 * @brief Loads base data
 * @param[in] parent XML Node structure, where we get the information from
 */
qboolean B_LoadXML (mxml_node_t *parent)
{
	int i;
	int buildingIdx;
	mxml_node_t *bases, *base;

	bases = mxml_GetNode(parent, "bases");
	if (!bases) {
		Com_Printf("Error: Node 'bases' wasn't found in savegame\n");
		return qfalse;
	}

	ccs.numBases = 0;

	Com_RegisterConstList(saveBaseConstants);
	for (base = mxml_GetNode(bases, SAVE_BASES_BASE), i = 0; i < MAX_BASES && base; i++, base = mxml_GetNextNode(base, bases, SAVE_BASES_BASE)) {
		mxml_node_t * node, * snode;
		int aircraftIdxInBase;
		base_t *const b = B_GetBaseByIDX(i);
		const char *str = mxml_GetString(base, SAVE_BASES_BASESTATUS);
		int j;

		ccs.numBases++;

		b->idx = B_GetBaseIDX(b);
		b->founded = qtrue;
		if (!Com_GetConstIntFromNamespace(SAVE_BASESTATUS_NAMESPACE, str, (int*) &b->baseStatus)) {
			Com_Printf("Invalid base status '%s'\n", str);
			Com_UnregisterConstList(saveBaseConstants);
			return qfalse;
		}

		Q_strncpyz(b->name, mxml_GetString(base, SAVE_BASES_NAME), sizeof(b->name));
		mxml_GetPos3(base, SAVE_BASES_POS, b->pos);
		b->alienInterest = mxml_GetFloat(base, SAVE_BASES_ALIENINTEREST, 0.0);

		aircraftIdxInBase = mxml_GetInt(base, SAVE_BASES_CURRENTAIRCRAFTIDX, AIRCRAFT_INBASE_INVALID);
		if (aircraftIdxInBase != AIRCRAFT_INBASE_INVALID) {
			/* aircraft are not yet loaded! */
			/** @todo this must be set after the aircraft are loaded */
			/*b->aircraftCurrent = &b->aircraft[aircraftIdxInBase];*/
			b->aircraftCurrent = NULL;
		} else {
			b->aircraftCurrent = NULL;
		}

		/* building space*/
		node = mxml_GetNode(base, SAVE_BASES_BUILDINGSPACE);
		for (snode = mxml_GetNode(node, SAVE_BASES_BUILDING); snode; snode = mxml_GetNextNode(snode, node, SAVE_BASES_BUILDING)) {
			/** @todo save it as vec2t if needed, also it's opposite */
			const int k = mxml_GetInt(snode, SAVE_BASES_X, 0);
			const int l = mxml_GetInt(snode, SAVE_BASES_Y, 0);
			baseBuildingTile_t* tile = &b->map[k][l];
			buildingIdx = mxml_GetInt(snode, SAVE_BASES_BUILDINGINDEX, -1);

			tile->posX = l;
			tile->posY = k;
			if (buildingIdx != -1)
				/* The buildings are actually parsed _below_. (See PRE_MAXBUI loop) */
				tile->building = B_GetBuildingByIDX(i, buildingIdx);
			else
				tile->building = NULL;
			tile->blocked = mxml_GetBool(snode, SAVE_BASES_BLOCKED, qfalse);
		}
		/* buildings */
		node = mxml_GetNode(base, SAVE_BASES_BUILDINGS);
		for (j = 0, snode = mxml_GetNode(node, SAVE_BASES_BUILDING); snode; snode = mxml_GetNextNode(snode, node, SAVE_BASES_BUILDING), j++) {
			const int buildId = mxml_GetInt(snode, SAVE_BASES_BUILDING_PLACE, MAX_BUILDINGS);
			building_t *building;
			char buildingType[MAX_VAR];

			if (buildId >= MAX_BUILDINGS) {
				Com_Printf("building ID is greater than MAX buildings\n");
				Com_UnregisterConstList(saveBaseConstants);
				return qfalse;
			}

			Q_strncpyz(buildingType, mxml_GetString(snode, SAVE_BASES_BUILDINGTYPE), sizeof(buildingType));
			if (buildingType[0] == '\0') {
				Com_Printf("No buildingtype set\n");
				Com_UnregisterConstList(saveBaseConstants);
				return qfalse;
			}

			building = B_GetBuildingTemplate(buildingType);
			if (!building)
				continue;

			ccs.buildings[i][buildId] = *building;
			building = B_GetBuildingByIDX(i, buildId);
			building->idx = B_GetBuildingIDX(b, building);
			if (building->idx != buildId) {
				Com_Printf("building ID doesn't match\n");
				Com_UnregisterConstList(saveBaseConstants);
				return qfalse;
			}
			building->base = b;

			str = mxml_GetString(snode, SAVE_BASES_BUILDINGSTATUS);
			if (!Com_GetConstIntFromNamespace(SAVE_BUILDINGSTATUS_NAMESPACE, str, (int*) &building->buildingStatus)) {
				Com_Printf("Invalid building status '%s'\n", str);
				Com_UnregisterConstList(saveBaseConstants);
				return qfalse;
			}

			building->timeStart = mxml_GetInt(snode, SAVE_BASES_BUILDINGTIMESTART, 0);
			building->buildTime = mxml_GetInt(snode, SAVE_BASES_BUILDINGBUILDTIME, 0);
			building->level = mxml_GetFloat(snode, SAVE_BASES_BUILDINGLEVEL, 0);
			mxml_GetPos2(snode, SAVE_BASES_POS, building->pos);
		}
		ccs.numBuildings[i] = j;

		BDEF_InitialiseBaseSlots(b);
		/* read missile battery slots */
		node = mxml_GetNode(base, SAVE_BASES_BATTERIES);
		if (node)
			b->numBatteries = B_LoadBaseSlotsXML(b->batteries, MAX_BASE_SLOT, node);
		/* read laser battery slots */
		node = mxml_GetNode(base, SAVE_BASES_LASERS);
		if (node)
			b->numLasers = B_LoadBaseSlotsXML(b->lasers, MAX_BASE_SLOT, node);
		/* read equipment */
		node = mxml_GetNode(base, SAVE_BASES_STORAGE);
		B_LoadStorageXML(node, &(b->storage));
		/* read radar info */
		RADAR_InitialiseUFOs(&b->radar);
		RADAR_Initialise(&b->radar, mxml_GetInt(base, SAVE_BASES_RADARRANGE, 0), mxml_GetInt(base, SAVE_BASES_TRACKINGRANGE, 0), B_GetMaxBuildingLevel(b, B_RADAR), qtrue);

		/** @todo can't we use something like I_DestroyInventory here? */
		/* clear the mess of stray loaded pointers */
		OBJZERO(b->bEquipment);
	}
	Com_UnregisterConstList(saveBaseConstants);
	Cvar_SetValue("mn_base_count", B_GetCount());
	return qtrue;
}

/**
 * @brief Check if an item is stored in storage.
 * @param[in] obj Pointer to the item to check.
 * @return True if item is stored in storage.
 */
qboolean B_ItemIsStoredInBaseStorage (const objDef_t *obj)
{
	/* antimatter is stored in antimatter storage */
	if (obj->isVirtual || Q_streq(obj->id, ANTIMATTER_TECH_ID))
		return qfalse;

	return qtrue;
}

/**
 * @brief Add/remove items to/from the storage.
 * @param[in] base The base which storage and capacity should be updated
 * @param[in] obj The item.
 * @param[in] amount Amount to be added to removed
 * @return the added/removed amount
 * @note The main difference between B_AddToStorage and B_UpdateStorageAndCapacity is that
 * B_AddToStorage adds/removes as many items as possible if adding/removing all not possible
 * also B_AddToStorage don't have a reset method
 */
int B_AddToStorage (base_t* base, const objDef_t *obj, int amount)
{
	capacities_t *cap;

	assert(base);
	assert(obj);

	if (!B_ItemIsStoredInBaseStorage(obj))
		return 0;

	cap = CAP_Get(base, CAP_ITEMS);
	if (amount > 0) {
		if (obj->size > 0) {
			const int freeSpace = cap->max - cap->cur;
			/* correct amount and update capacity */
			amount = min(amount, freeSpace / obj->size);
			cap->cur += (amount * obj->size);
		}
		base->storage.numItems[obj->idx] += amount;
	} else if (amount < 0) {
		/* correct amount */
		amount = max(amount, -B_ItemInBase(obj, base));
		if (obj->size > 0)
			cap->cur += (amount * obj->size);
		base->storage.numItems[obj->idx] += amount;
	}

	return amount;
}

/**
 * @brief Update the storage amount and the capacities for the storages in the base
 * @param[in] base The base which storage and capacity should be updated
 * @param[in] obj The item.
 * @param[in] amount Amount to be added to removed
 * @param[in] reset Set this to true (amount is not needed) if you want to reset the
 * storage amount and capacities (e.g. in case of a base ransack)
 * @param[in] ignorecap qtrue if we won't check freespace but will just add items.
 * @sa CL_BaseRansacked
 */
qboolean B_UpdateStorageAndCapacity (base_t* base, const objDef_t *obj, int amount, qboolean reset, qboolean ignorecap)
{
	capacities_t *cap;

	assert(base);
	assert(obj);

	if (obj->isVirtual)
		return qtrue;

	cap = CAP_Get(base, CAP_ITEMS);
	if (reset) {
		base->storage.numItems[obj->idx] = 0;
		base->storage.numItemsLoose[obj->idx] = 0;
		cap->cur = 0;
	} else {
		if (!B_ItemIsStoredInBaseStorage(obj)) {
			Com_DPrintf(DEBUG_CLIENT, "B_UpdateStorageAndCapacity: Item '%s' is not stored in storage: skip\n", obj->id);
			return qfalse;
		}

		if (!ignorecap && amount > 0) {
			/* Only add items if there is enough room in storage */
			if (cap->max - cap->cur < (obj->size * amount)) {
				Com_DPrintf(DEBUG_CLIENT, "B_UpdateStorageAndCapacity: Not enough storage space (item: %s, amount: %i)\n", obj->id, amount);
				return qfalse;
			}
		}

		base->storage.numItems[obj->idx] += amount;
		if (obj->size > 0)
			cap->cur += (amount * obj->size);

		if (cap->cur < 0) {
			Com_Printf("B_UpdateStorageAndCapacity: current storage capacity is negative (%i): reset to 0\n", cap->cur);
			cap->cur = 0;
		}

		if (base->storage.numItems[obj->idx] < 0) {
			Com_Printf("B_UpdateStorageAndCapacity: current number of item '%s' is negative: reset to 0\n", obj->id);
			base->storage.numItems[obj->idx] = 0;
		}
	}

	return qtrue;
}

/**
 * @brief Checks the parsed buildings for errors
 * @return false if there are errors - true otherwise
 */
qboolean B_ScriptSanityCheck (void)
{
	int i, error = 0;
	building_t* b;

	for (i = 0, b = ccs.buildingTemplates; i < ccs.numBuildingTemplates; i++, b++) {
		if (!b->mapPart && b->visible) {
			error++;
			Com_Printf("...... no mappart for building '%s' given\n", b->id);
		}
		if (!b->name) {
			error++;
			Com_Printf("...... no name for building '%s' given\n", b->id);
		}
		if (!b->image) {
			error++;
			Com_Printf("...... no image for building '%s' given\n", b->id);
		}
		if (!b->pedia) {
			error++;
			Com_Printf("...... no pedia link for building '%s' given\n", b->id);
		} else if (!RS_GetTechByID(b->pedia)) {
			error++;
			Com_Printf("...... could not get pedia entry tech (%s) for building '%s'\n", b->pedia, b->id);
		}
	}
	if (!error)
		return qtrue;
	else
		return qfalse;
}

/**
 * @brief returns the amount of antimatter stored in a base
 * @param[in] base Pointer to the base to check
 */
int B_AntimatterInBase (const base_t *base)
{
#ifdef DEBUG
	objDef_t *od;

	od = INVSH_GetItemByID(ANTIMATTER_TECH_ID);
	if (od == NULL)
		Com_Error(ERR_DROP, "Could not find "ANTIMATTER_TECH_ID" object definition");

	assert(base);
	assert(B_ItemInBase(od, base) == base->capacities[CAP_ANTIMATTER].cur);
#endif

	return base->capacities[CAP_ANTIMATTER].cur;
}

/**
 * @brief Manages antimatter (adding, removing) through Antimatter Storage Facility.
 * @param[in,out] base Pointer to the base.
 * @param[in] amount quantity of antimatter to add/remove (> 0 even if antimatter is removed)
 * @param[in] add True if we are adding antimatter, false when removing.
 * @note This function should be called whenever we add or remove antimatter from Antimatter Storage Facility.
 * @note Call with amount = 0 if you want to remove ALL antimatter from given base.
 */
void B_ManageAntimatter (base_t *base, int amount, qboolean add)
{
	objDef_t *od;
	capacities_t *cap;

	assert(base);

	if (add && !B_GetBuildingStatus(base, B_ANTIMATTER)) {
		Com_sprintf(cp_messageBuffer, lengthof(cp_messageBuffer),
			_("%s does not have Antimatter Storage Facility. %i units of antimatter got removed."),
			base->name, amount);
		MS_AddNewMessage(_("Notice"), cp_messageBuffer, qfalse, MSG_STANDARD, NULL);
		return;
	}

	od = INVSH_GetItemByIDSilent(ANTIMATTER_TECH_ID);
	if (od == NULL)
		Com_Error(ERR_DROP, "Could not find "ANTIMATTER_TECH_ID" object definition");

	cap = CAP_Get(base, CAP_ANTIMATTER);
	if (add) {	/* Adding. */
		const int a = min(amount, cap->max - cap->cur);
		base->storage.numItems[od->idx] += a;
		cap->cur += a;
	} else {	/* Removing. */
		if (amount == 0) {
			cap->cur = 0;
			base->storage.numItems[od->idx] = 0;
		} else {
			const int a = min(amount, cap->cur);
			cap->cur -= a;
			base->storage.numItems[od->idx] -= a;
		}
	}
}
