/**
 * @file
 * @brief grid pathfinding and routing
 */

/*
All original material Copyright (C) 2002-2015 UFO: Alien Invasion.

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

#pragma once

// Should be included here because it is used-in-the-interface, but breaks uforadiant compilation :(
//#include "tracing.h"

/*==============================================================
GLOBAL TYPES
==============================================================*/

/* Decide whether we are doing uni- or bidirectional conclusions from our traces.
 * This constant is used in both a boolean and an integer way,
 * so it must only be set to 0 or 1 ! */
#define RT_IS_BIDIRECTIONAL 0

/*==============================================================
MACROS
==============================================================*/
/**
 * @brief Some macros to access routing table values as described above.
 * @note P/N = positive/negative; X/Y =direction
 */
/* route - Used by Grid_* only  */
/** @note IMPORTANT: actorSize is 1 or greater!!! */

#define RT_CONN_PX(r, actorSize, x, y, z)		(r.getConn(actorSize, x, y, z, 0))
#define RT_CONN_NX(r, actorSize, x, y, z)		(r.getConn(actorSize, x, y, z, 1))
#define RT_CONN_PY(r, actorSize, x, y, z)		(r.getConn(actorSize, x, y, z, 2))
#define RT_CONN_NY(r, actorSize, x, y, z)		(r.getConn(actorSize, x, y, z, 3))

#define RT_CONN_PX_PY(r, actorSize, x, y, z)	(r.getConn(actorSize, x, y, z, 4))
#define RT_CONN_PX_NY(r, actorSize, x, y, z)	(r.getConn(actorSize, x, y, z, 7))
#define RT_CONN_NX_PY(r, actorSize, x, y, z)	(r.getConn(actorSize, x, y, z, 6))
#define RT_CONN_NX_NY(r, actorSize, x, y, z)	(r.getConn(actorSize, x, y, z, 5))

#define RT_STEPUP_PX(r, actorSize, x, y, z)		(r.getStepup(actorSize, x, y, z, 0))
#define RT_STEPUP_NX(r, actorSize, x, y, z)		(r.getStepup(actorSize, x, y, z, 1))
#define RT_STEPUP_PY(r, actorSize, x, y, z)		(r.getStepup(actorSize, x, y, z, 2))
#define RT_STEPUP_NY(r, actorSize, x, y, z)		(r.getStepup(actorSize, x, y, z, 3))

#define RT_STEPUP_PX_PY(r, actorSize, x, y, z)	(r.getStepup(actorSize, x, y, z, 4))
#define RT_STEPUP_PX_NY(r, actorSize, x, y, z)	(r.getStepup(actorSize, x, y, z, 7))
#define RT_STEPUP_NX_PY(r, actorSize, x, y, z)	(r.getStepup(actorSize, x, y, z, 6))
#define RT_STEPUP_NX_NY(r, actorSize, x, y, z)	(r.getStepup(actorSize, x, y, z, 5))


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

extern bool debugTrace;

/*
===============================================================================
GAME RELATED TRACING
===============================================================================
*/


int RT_CheckCell(mapTiles_t* mapTiles, Routing& routing, const int actorSize, const int x, const int y, const int z, const char** list);
void RT_UpdateConnectionColumn(mapTiles_t* mapTiles, Routing& routing, const int actorSize, const int x, const int y, const int dir, const char** list = nullptr, const int minZ = 0, const int maxZ = PATHFINDING_HEIGHT - 1);
bool RT_AllCellsBelowAreFilled(const Routing& routing, const int actorSize, const pos3_t pos);
void RT_GetMapSize(mapTiles_t* mapTiles, AABB& mapBox);
bool RT_CanActorStandHere(const Routing& routing, const int actorSize, const pos3_t pos);


/*
==========================================================
DEBUGGING CODE
==========================================================
*/

#ifdef DEBUG
void RT_DumpWholeMap(mapTiles_t* mapTiles, const Routing& routing);
int RT_DebugSpecial(mapTiles_t* mapTiles, Routing& routing, const int actorSize, const int x, const int y, const int dir, const char** list);
void RT_DebugPathDisplay (Routing& routing, const int actorSize, int x, int y, int z);
#endif
void RT_WriteCSVFiles(const Routing& routing, const char* baseFilename, const GridBox& box);
