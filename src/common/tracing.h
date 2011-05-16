/**
 * @file tracing.h
 * @brief Tracing functions
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

#ifndef COMMON_TRACING_H
#define COMMON_TRACING_H


#include "../shared/typedefs.h"

/*==============================================================
GLOBAL TYPES
==============================================================*/
#if defined(COMPILE_MAP)
  #define TR_TILE_TYPE			dMapTile_t
  #define TR_PLANE_TYPE			dBspPlane_t
  #define TR_PLANE2_TYPE		plane_t
  #define TR_NODE_TYPE			dBspNode_t
  #define TR_LEAF_TYPE			dBspLeaf_t
  #define TR_BRUSH_TYPE			dBspBrush_t
  #define TR_BRUSHSIDE_TYPE		dBspBrushSide_t
#elif defined(COMPILE_UFO)
  #define TR_TILE_TYPE			mapTile_t
  #define TR_PLANE_TYPE			cBspPlane_t
  #define TR_PLANE2_TYPE		cBspPlane_t
  #define TR_NODE_TYPE			cBspNode_t
  #define TR_LEAF_TYPE			cBspLeaf_t
  #define TR_BRUSH_TYPE			cBspBrush_t
  #define TR_BRUSHSIDE_TYPE		cBspBrushSide_t
#else
  #error Either COMPILE_MAP or COMPILE_UFO must be defined in order for tracing.c to work.
#endif

/**
 * mask to trace against all the different visible levels (1-8) (resp. (1<<[0-7])
 */
#define TRACING_ALL_VISIBLE_LEVELS 0x1FF

/** a trace is returned when a box is swept through the world */
typedef struct trace_s {
	qboolean allsolid;			/**< if true, plane is not valid */
	qboolean startsolid;		/**< if true, the initial point was in a solid area */
	float fraction;				/**< distance traveled, 1.0 = didn't hit anything, 0.0 Inside of a brush */
	vec3_t endpos;				/**< final position along line */
	TR_PLANE_TYPE plane;		/**< surface plane at impact - normal is in there */
	cBspSurface_t *surface;	    /**< surface hit */
	int planenum;				/**< index of the plane hit, used for map debugging */
	uint32_t contentFlags;		/**< contents on other side of surface hit */
	int32_t leafnum;
	int mapTile;				/**< the map tile we hit something */
	struct le_s *le;			/**< not set by CM_*() functions */
	struct edict_s *ent;		/**< not set by CM_*() functions */
} trace_t;

/* This attempts to make the box tracing code thread safe. */
typedef struct boxtrace_s {
	vec3_t start, end;
	vec3_t mins, maxs;
	vec3_t absmins, absmaxs;
	vec3_t extents;

	trace_t trace;
	uint32_t contents;			/**< content flags to match again - MASK_ALL to match everything */
	uint32_t rejects;			/**< content flags that should be rejected in a trace - ignored when MASK_ALL is given as content flags */
	qboolean ispoint;			/* optimized case */

	TR_TILE_TYPE *tile;
} boxtrace_t;

typedef struct box_s {
	vec3_t mins, maxs;
} box_t;

typedef struct {
	/** @note loaded map tiles with this assembly.  ufo2map has exactly 1. */
	TR_TILE_TYPE mapTiles[MAX_MAPTILES];

	/** @note number of loaded map tiles (map assembly) */
	int numTiles;
} mapTiles_t;

/*==============================================================
BOX AND LINE TRACING
==============================================================*/

int TR_BoxOnPlaneSide(const vec3_t mins, const vec3_t maxs, const TR_PLANE_TYPE *plane);

void TR_BuildTracingNode_r(TR_TILE_TYPE *tile, tnode_t **tnode, int32_t nodenum, int level);

#ifdef COMPILE_MAP
trace_t TR_SingleTileBoxTrace(mapTiles_t *mapTiles, const vec3_t start, const vec3_t end, const box_t* traceBox, const int levelmask, const int brushmask, const int brushreject);
#endif
int TR_TestLine_r(TR_TILE_TYPE *tile, int32_t nodenum, const vec3_t start, const vec3_t stop);
trace_t TR_BoxTrace(TR_TILE_TYPE *tile, const vec3_t start, const vec3_t end, const vec3_t mins, const vec3_t maxs, const int headnode, const int brushmask, const int brushreject, const float fraction);

qboolean TR_TestLine(mapTiles_t *mapTiles, const vec3_t start, const vec3_t stop, const int levelmask);
qboolean TR_TestLineDM(mapTiles_t* mapTiles, const vec3_t start, const vec3_t stop, vec3_t end, const int levelmask);
trace_t TR_TileBoxTrace(TR_TILE_TYPE *myTile, const vec3_t start, const vec3_t end, const vec3_t mins, const vec3_t maxs, const int levelmask, const int brushmask, const int brushreject);

#endif /* COMMON_TRACING_H */
