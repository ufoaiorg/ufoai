/**
 * @file routing.h
 * @brief grid pathfinding and routing
 */

/*
All original material Copyright (C) 2002-2011 UFO: Alien Invasion.

Copyright (C) 1997-2001 Id Software, Inc.

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

#ifndef COMMON_ROUTING_H
#define COMMON_ROUTING_H

/*==============================================================
GLOBAL TYPES
==============================================================*/
#if defined(COMPILE_MAP)
  #define RT_COMPLETEBOXTRACE_SIZE(mapTiles,s,e,b,list)		TR_SingleTileBoxTrace((mapTiles), (s),(e),(b),TRACING_ALL_VISIBLE_LEVELS, MASK_ALL, 0)
  #define RT_COMPLETEBOXTRACE_PASSAGE(mapTiles,s,e,b,list)	TR_SingleTileBoxTrace((mapTiles), (s),(e),(b),TRACING_ALL_VISIBLE_LEVELS, MASK_IMPASSABLE, MASK_PASSABLE)

#elif defined(COMPILE_UFO)
  #define RT_COMPLETEBOXTRACE_SIZE(mapTiles,s,e,b,list)		CM_EntCompleteBoxTrace((mapTiles), (s),(e),(b),TRACING_ALL_VISIBLE_LEVELS, MASK_ALL, 0, (list))
  #define RT_COMPLETEBOXTRACE_PASSAGE(mapTiles,s,e,b,list)	CM_EntCompleteBoxTrace((mapTiles), (s),(e),(b),TRACING_ALL_VISIBLE_LEVELS, MASK_IMPASSABLE, MASK_PASSABLE, (list))

#else
  #error Either COMPILE_MAP or COMPILE_UFO must be defined in order for tracing.c to work.
#endif

/* Decide whether we are doing uni- or bidirectional conclusions from our traces.
 * This constant is used in both a boolean and an integer way,
 * so it must only be set to 0 or 1 ! */
#define RT_IS_BIDIRECTIONAL 0

/*==============================================================
MACROS
==============================================================*/
/**
 * @brief Some macros to access routing_t values as described above.
 * @note P/N = positive/negative; X/Y =direction
 */
/* route - Used by Grid_* only  */
/** @note IMPORTANT: actorSize is 1 or greater!!! */
#define RT_CONN(map, actorSize, x, y, z, dir)			map[(actorSize) - 1].route[(z)][(y)][(x)][(dir)]
#define RT_CONN_POS(map, actorSize, p, dir)				(int)(map[(actorSize) - 1].route[(p)[2]][(p)[1]][(p)[0]][(dir)])
#define RT_CONN_TEST(map, actorSize, x, y, z, dir)		assert((actorSize) > ACTOR_SIZE_INVALID); assert((actorSize) <= ACTOR_MAX_SIZE); \
															assert((z) >= 0); assert((z) < PATHFINDING_HEIGHT);\
															assert((y) >= 0); assert((y) < PATHFINDING_WIDTH);\
															assert((x) >= 0); assert((x) < PATHFINDING_WIDTH);\
															assert((dir) >= 0); assert((dir) < CORE_DIRECTIONS);
#define RT_CONN_TEST_POS(map, actorSize, p, dir)		assert((actorSize) > ACTOR_SIZE_INVALID); assert((actorSize) <= ACTOR_MAX_SIZE); \
															assert((p)[2] < PATHFINDING_HEIGHT);\
															assert((dir) >= 0); assert((dir) < CORE_DIRECTIONS);
															/* assuming p is a pos3_t, we don't need to check fo p[n] >= 0 here because it's unsigned.
															 * also we don't need to check against PATHFINDING_WIDTH because it's always greater than a 'byte' type. */

#define RT_CONN_PX(map, actorSize, x, y, z)		(RT_CONN(map, actorSize, x, y, z, 0))
#define RT_CONN_NX(map, actorSize, x, y, z)		(RT_CONN(map, actorSize, x, y, z, 1))
#define RT_CONN_PY(map, actorSize, x, y, z)		(RT_CONN(map, actorSize, x, y, z, 2))
#define RT_CONN_NY(map, actorSize, x, y, z)		(RT_CONN(map, actorSize, x, y, z, 3))

#define RT_CONN_PX_PY(map, actorSize, x, y, z)	(RT_CONN(map, actorSize, x, y, z, 4))
#define RT_CONN_PX_NY(map, actorSize, x, y, z)	(RT_CONN(map, actorSize, x, y, z, 7))
#define RT_CONN_NX_PY(map, actorSize, x, y, z)	(RT_CONN(map, actorSize, x, y, z, 6))
#define RT_CONN_NX_NY(map, actorSize, x, y, z)	(RT_CONN(map, actorSize, x, y, z, 5))

#define RT_STEPUP(map, actorSize, x, y, z, dir)	map[(actorSize) - 1].stepup[(z)][(y)][(x)][(dir)]
#define RT_STEPUP_POS(map, actorSize, p, dir)	map[(actorSize) - 1].stepup[(p)[2]][(p)[1]][(p)[0]][(dir)]

#define RT_STEPUP_PX(map, actorSize, x, y, z)		(RT_STEPUP(map, actorSize, x, y, z, 0))
#define RT_STEPUP_NX(map, actorSize, x, y, z)		(RT_STEPUP(map, actorSize, x, y, z, 1))
#define RT_STEPUP_PY(map, actorSize, x, y, z)		(RT_STEPUP(map, actorSize, x, y, z, 2))
#define RT_STEPUP_NY(map, actorSize, x, y, z)		(RT_STEPUP(map, actorSize, x, y, z, 3))

#define RT_STEPUP_PX_PY(map, actorSize, x, y, z)	(RT_STEPUP(map, actorSize, x, y, z, 4))
#define RT_STEPUP_PX_NY(map, actorSize, x, y, z)	(RT_STEPUP(map, actorSize, x, y, z, 7))
#define RT_STEPUP_NX_PY(map, actorSize, x, y, z)	(RT_STEPUP(map, actorSize, x, y, z, 6))
#define RT_STEPUP_NX_NY(map, actorSize, x, y, z)	(RT_STEPUP(map, actorSize, x, y, z, 5))

#define RT_FLOOR(map, actorSize, x, y, z)			map[(actorSize) - 1].floor[(z)][(y)][(x)]
#define RT_FLOOR_POS(map, actorSize, p)				map[(actorSize) - 1].floor[(p)[2]][(p)[1]][(p)[0]]
#define RT_CEILING(map, actorSize, x, y, z)			map[(actorSize) - 1].ceil[(z)][(y)][(x)]
#define RT_CEILING_POS(map, actorSize, p)			map[(actorSize) - 1].ceil[(p)[2]][(p)[1]][(p)[0]]
#define RT_FILLED(map, actorSize, x, y, z)			(RT_CEILING(map, actorSize, x, y, z) - RT_FLOOR(map, actorSize, x, y, z) < PATHFINDING_MIN_OPENING)

/* area - Used by Grid_* only */
#define RT_AREA(path, x, y, z, state)				((path)->area[(state)][(z)][(y)][(x)])
#define RT_AREA_POS(path, p, state)					((path)->area[(state)][(p)[2]][(p)[1]][(p)[0]])
#define RT_AREA_FROM(path, x, y, z, state)			((path)->areaFrom[(state)][(z)][(y)][(x)])
#define RT_AREA_FROM_POS(path, p, state)			((path)->areaFrom[(state)][(p)[2]][(p)[1]][(p)[0]])
#define RT_SAREA(path, x, y, z, state)				((path)->areaStored[(state)][(z)][(y)][(x)])
#define RT_AREA_TEST(path, x, y, z, state)			assert((z) >= 0); assert((z) < PATHFINDING_HEIGHT);\
														assert((y) >= 0); assert((y) < PATHFINDING_WIDTH);\
														assert((x) >= 0); assert((x) < PATHFINDING_WIDTH);\
														assert((state) == 0 || (state) == 1);
#define RT_AREA_TEST_POS(path, p, state)			assert((p)[2] < PATHFINDING_HEIGHT);\
														assert((state) == 0 || (state) == 1);
														/* assuming p is a pos3_t, we don't need to check fo p[n] >= 0 here because it's unsigned.
														 * also we don't need to check against PATHFINDING_WIDTH because it's always greater than a 'byte' type. */


/** @brief  These macros are meant to correctly convert from model units to QUANT units and back. */
/* Surfaces used as floors are rounded up. */
#define ModelFloorToQuant(x)	(ceil((x) / QUANT))
/* Surfaces used as ceilings are rounded down. */
#define ModelCeilingToQuant(x)	(floor((x) / QUANT))
/* Going from QUANT units back to model units returns the approximation of the QUANT unit. */
#define QuantToModel(x)			((x) * QUANT)

/**
 * @brief SizedPosToVect locates the center of an actor based on size and position.
 */
#define SizedPosToVec(p, actorSize, v) { \
	assert(actorSize > ACTOR_SIZE_INVALID); \
	assert(actorSize <= ACTOR_MAX_SIZE); \
	v[0] = ((int)p[0] - 128) * UNIT_SIZE   + (UNIT_SIZE * actorSize)  / 2; \
	v[1] = ((int)p[1] - 128) * UNIT_SIZE   + (UNIT_SIZE * actorSize)  / 2; \
	v[2] =  (int)p[2]        * UNIT_HEIGHT + UNIT_HEIGHT / 2;  \
}


/*
===============================================================================
SHARED EXTERNS (cmodel.c and ufo2map/routing.c)
===============================================================================
*/

extern qboolean debugTrace;

/*
===============================================================================
GAME RELATED TRACING
===============================================================================
*/


int RT_CheckCell(mapTiles_t *mapTiles, routing_t * map, const int actorSize, const int x, const int y, const int z, const char **list);
void RT_UpdateConnectionColumn(mapTiles_t *mapTiles, routing_t * map, const int actorSize, const int x, const int y, const int dir, const char **list);
qboolean RT_AllCellsBelowAreFilled(const routing_t * map, const int actorSize, const pos3_t pos);
void RT_GetMapSize(mapTiles_t *mapTiles, vec3_t map_min, vec3_t map_max);


/*
==========================================================
DEBUGGING CODE
==========================================================
*/

#ifdef DEBUG
void RT_DumpWholeMap(mapTiles_t *mapTiles, const routing_t *map);
int RT_DebugSpecial(mapTiles_t *mapTiles, routing_t * map, const int actorSize, const int x, const int y, const int dir, const char **list);
#endif
void RT_WriteCSVFiles(const routing_t *map, const char* baseFilename, const ipos3_t mins, const ipos3_t maxs);

#endif
