/**
 * @file routing.h
 * @brief grid pathfinding and routing
 */

/*
All original materal Copyright (C) 2002-2007 UFO: Alien Invasion team.

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


/*==============================================================
GLOBAL TYPES
==============================================================*/
#if defined(COMPILE_MAP)
  #define RT_TESTLINE  			TR_TestLine
  #define RT_TESTLINEDM			TR_TestLineDM

#elif defined(COMPILE_UFO)
  #define RT_TESTLINE  			CM_EntTestLine
  #define RT_TESTLINEDM			CM_EntTestLineDM

#else
  #error Either COMPILE_MAP or COMPILE_UFO must be defined in order for tracing.c to work.
#endif


/*==============================================================
MACROS
==============================================================*/
/**
 * @brief Some macros to access routing_t values as described above.
 * @note P/N = positive/negative; X/Y =direction
 */
/* route - Used by Grid_* only  */
#ifdef COMPILE_MAP
#define R_CONN_PX(map,x,y,z) ((map)->route[(z)][(y)][(x)] & 0x10)
#define R_CONN_NX(map,x,y,z) ((map)->route[(z)][(y)][(x)] & 0x20)
#define R_CONN_PY(map,x,y,z) ((map)->route[(z)][(y)][(x)] & 0x40)
#define R_CONN_NY(map,x,y,z) ((map)->route[(z)][(y)][(x)] & 0x80)

#define R_CONN_PX_PY(map,x,y,z) (R_CONN_PX(map,x,y,z) && R_CONN_PY(map,x,y,z) && R_CONN_PY(map,x+1,y,z) && R_CONN_PX(map,x,y+1,z))
#define R_CONN_PX_NY(map,x,y,z) (R_CONN_PX(map,x,y,z) && R_CONN_NY(map,x,y,z) && R_CONN_NY(map,x+1,y,z) && R_CONN_PX(map,x,y-1,z))
#define R_CONN_NX_PY(map,x,y,z) (R_CONN_NX(map,x,y,z) && R_CONN_PY(map,x,y,z) && R_CONN_PY(map,x-1,y,z) && R_CONN_NX(map,x,y+1,z))
#define R_CONN_NX_NY(map,x,y,z) (R_CONN_NX(map,x,y,z) && R_CONN_NY(map,x,y,z) && R_CONN_NY(map,x-1,y,z) && R_CONN_NX(map,x,y-1,z))

/* height - Used by Grid_* and updates */
#define R_HEIGHT(map,x,y,z)  ((map)->route[(z)][(y)][(x)] & 0x0F)

/* filled - Used by Grid_* and updates */
#define R_FILLED(map,x,y,z) ((map)->step[(y)][(x)] & (1 << (z)))

/* step - Used by Grid_* and updates  */
#define R_STEP(map,x,y,z) ((map)->step[(y)][(x)] & (1 << (z)))

/* fall - Used by Grid_* and updates  */
#define R_FALL(map,x,y,z) ((map)->fall[(y)][(x)] & (1 << (z)))

/* area - Used by Grid_* only */
#define R_AREA(path,x,y,z,state) ((path)->area[(state)][(z)][(y)][(x)])
#define R_SAREA(path,x,y,z,state) ((path)->areaStored[(state)][(z)][(y)][(x)])
#endif


/** @note IMPORTANT: actor_size is 1 or greater!!! */
/*
#define RT_CONN(map, actor_size, x, y, z, dir)		map[(assert(0 == 1), assert(actor_size >= 1), assert(actor_size <= ACTOR_MAX_SIZE), actor_size - 1)]\
													.route[assert(z >= 0), assert(z < PATHFINDING_HEIGHT), z]\
													[assert(y >= 0), assert(y < PATHFINDING_WIDTH), y]\
													[assert(x >= 0), assert(x < PATHFINDING_WIDTH), x]\
													[assert(dir >= 0), assert(dir < CORE_DIRECTIONS), dir]
*/
#define RT_CONN(map, actor_size, x, y, z, dir)		map[(actor_size) - 1].route[(z)][(y)][(x)][(dir)]
#define RT_CONN_TEST(map, actor_size, x, y, z, dir)		assert((actor_size) >= 1); assert((actor_size) <= ACTOR_MAX_SIZE); \
														assert((z) >= 0); assert((z) < PATHFINDING_HEIGHT);\
														assert((y) >= 0); assert((y) < PATHFINDING_WIDTH);\
														assert((x) >= 0); assert((x) < PATHFINDING_WIDTH);\
														assert((dir) >= 0); assert((dir) < CORE_DIRECTIONS);

#define RT_CONN_PX(map, actor_size, x, y, z)		(RT_CONN(map, actor_size, x, y, z, 0))
#define RT_CONN_NX(map, actor_size, x, y, z)		(RT_CONN(map, actor_size, x, y, z, 1))
#define RT_CONN_PY(map, actor_size, x, y, z)		(RT_CONN(map, actor_size, x, y, z, 2))
#define RT_CONN_NY(map, actor_size, x, y, z)		(RT_CONN(map, actor_size, x, y, z, 3))

#define RT_CONN_PX_PY(map, actor_size, x, y, z)	    (RT_CONN(map, actor_size, x, y, z, 4))
#define RT_CONN_PX_NY(map, actor_size, x, y, z)	    (RT_CONN(map, actor_size, x, y, z, 7))
#define RT_CONN_NX_PY(map, actor_size, x, y, z)	    (RT_CONN(map, actor_size, x, y, z, 6))
#define RT_CONN_NX_NY(map, actor_size, x, y, z)	    (RT_CONN(map, actor_size, x, y, z, 5))

#define RT_FLOOR(map, actor_size, x, y, z)		    map[(actor_size) - 1].floor[(z)][(y)][(x)]
#define RT_CEILING(map, actor_size, x, y, z)		map[(actor_size) - 1].ceil[(z)][(y)][(x)]
#define RT_FILLED(map, actor_size, x, y, z)		    (RT_CEILING(map, actor_size, x, y, z) - RT_FLOOR(map, actor_size, x, y, z) < PATHFINDING_MIN_OPENING)

/* area - Used by Grid_* only */
#define RT_AREA(path, x, y, z, state)               ((path)->area[(state)][(z)][(y)][(x)])
#define RT_AREA_FROM(path, x, y, z, state)          ((path)->areaFrom[(state)][(z)][(y)][(x)])
#define RT_SAREA(path, x, y, z, state)              ((path)->areaStored[(state)][(z)][(y)][(x)])
#define RT_AREA_TEST(path, x, y, z, state)			assert((z) >= 0); assert((z) < PATHFINDING_HEIGHT);\
														assert((y) >= 0); assert((y) < PATHFINDING_WIDTH);\
														assert((x) >= 0); assert((x) < PATHFINDING_WIDTH);\
														assert((state) >= 0); assert((state) < ACTOR_MAX_STATES);


/**
 * @brief SizedPosToVect locates the center of an actor based on size and position. */
#define SizedPosToVec(p, actor_size, v) ( \
	v[0] = ((int)p[0] - 128) * UNIT_SIZE   + (UNIT_SIZE * actor_size)  / 2, \
	v[1] = ((int)p[1] - 128) * UNIT_SIZE   + (UNIT_SIZE * actor_size)  / 2, \
	v[2] =  (int)p[2]        * UNIT_HEIGHT + UNIT_HEIGHT / 2  \
)


/*
===============================================================================
SHARED EXTERNS (cmodel.c and ufo2map/routing.c)
===============================================================================
*/

/** @note The old value for the normal step up (will become obselete) */
byte sh_low;


/** @note The old value for the STEPON flagged step up (will become obselete) */
byte sh_big;


extern crossPoints_t brushesHit;


/*
===============================================================================
GAME RELATED TRACING
===============================================================================
*/


void RT_CheckUnit (old_routing_t * map, int x, int y, int z);
int RT_NewCheckCell (routing_t * map, const int actor_size, const int x, const int y, const int z);
void RT_UpdateConnection (old_routing_t * map, int x, int y, int z, qboolean fill);
int RT_NewUpdateConnection (routing_t * map, const int actor_size, const int x, const int y, const int z, const int dir);

/*
==========================================================
  GRID ORIENTED MOVEMENT AND SCANNING
==========================================================
*/


void Grid_DumpMap (struct routing_s *map, int size, int lx, int ly, int lz, int hx, int hy, int hz);
void Grid_DumpWholeMap (routing_t *map);
