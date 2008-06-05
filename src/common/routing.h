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
#define R_CONN_PX(map,x,y,z) ((map)->route[(z)][(y)][(x)] & 0x10)
#define R_CONN_NX(map,x,y,z) ((map)->route[(z)][(y)][(x)] & 0x20)
#define R_CONN_PY(map,x,y,z) ((map)->route[(z)][(y)][(x)] & 0x40)
#define R_CONN_NY(map,x,y,z) ((map)->route[(z)][(y)][(x)] & 0x80)

/* height - Used by Grid_* and updates */
#define R_HEIGHT(map,x,y,z)  ((map)->route[(z)][(y)][(x)] & 0x0F)

/* filled - Used by Grid_* and updates */
#define R_FILLED(map,x,y,z) ((map)->step[(y)][(x)] & (1 << (z)))

/* step - Used by Grid_* and updates  */
#define R_STEP(map,x,y,z) ((map)->step[(y)][(x)] & (1 << (z)))

/* fall - Used by Grid_* and updates  */
#define R_FALL(map,x,y,z) ((map)->fall[(y)][(x)] & (1 << (z)))

/* area - Used by Grid_* only */
#define R_AREA(map,x,y,z) ((map)->area[(z)][(y)][(x)])
#define R_SAREA(map,x,y,z) ((map)->areaStored[(z)][(y)][(x)])



/*
===============================================================================
SHARED EXTERNS (cmodel.c and ufo2map/routing.c)
===============================================================================
*/

/** @note The old value for the normal step up (will become obselete) */
byte sh_low;

/** @note The old value for the STEPON flagged step up (will become obselete) */
byte sh_big;


/*
===============================================================================
GAME RELATED TRACING
===============================================================================
*/


void RT_CheckUnit (routing_t * map, int x, int y, pos_t z);
void RT_UpdateConnection (routing_t * map, int x, int y, byte z, qboolean fill);

/*
==========================================================
  GRID ORIENTED MOVEMENT AND SCANNING
==========================================================
*/


void Grid_DumpMap (struct routing_s *map, int lx, int ly, int lz, int hx, int hy, int hz);
void Grid_DumpWholeMap (routing_t *map);
