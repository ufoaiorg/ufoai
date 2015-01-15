/**
 * @file
 * @brief Tracing functions
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

#include "../shared/typedefs.h"

/*
 * ufo2map and ufo have a different view on the data in the bsp tree,
 * so they use different structs and classes.
 */
#if defined(COMPILE_MAP)
  #define TR_TILE_TYPE			dMapTile_t
  #define TR_PLANE_TYPE			dBspPlane_t
#elif defined(COMPILE_UFO)
  #define TR_TILE_TYPE			MapTile
  #define TR_PLANE_TYPE			cBspPlane_t
#else
  #error Either COMPILE_MAP or COMPILE_UFO must be defined in order for tracing.c to work.
#endif
/** @note all the above types are declared in typedefs.h */

/**
 * mask to trace against all the different visible levels (1-8) (resp. (1<<[0-7])
 */
#define TRACE_VISIBLE_LEVELS	0x0FF
#define TRACE_CLIP_LEVELS		0x100
#define TRACE_ALL_LEVELS		0x1FF

/** a trace is returned when a box is swept through the world */
typedef struct trace_s {
	bool allsolid;				/**< if true, plane is not valid */
	bool startsolid;			/**< if true, the initial point was in a solid area */
	float fraction;				/**< distance traveled, 1.0 = didn't hit anything, 0.0 Inside of a brush */
	vec3_t endpos;				/**< final position along line */
	TR_PLANE_TYPE plane;		/**< surface plane at impact - normal is in there */
	cBspSurface_t* surface;		/**< surface hit */
	int planenum;				/**< index of the plane hit, used for map debugging */
	uint32_t contentFlags;		/**< contents on other side of surface hit */
	int32_t leafnum;
	int mapTile;				/**< the map tile we hit something */
	struct le_s* le;			/**< not set by CM_*() functions */
	int entNum;					/**< not set by CM_*() functions */

	inline trace_s () {
		init();
	}
	inline void init() {
		OBJZERO(*this);
	}
} trace_t;

typedef struct mapTiles_s {
	/** @note loaded map tiles with this assembly.  ufo2map has exactly 1. */
	TR_TILE_TYPE mapTiles[MAX_MAPTILES];

	/** @note number of loaded map tiles (map assembly) */
	int numTiles;

	void getTilesAt(int x ,int y, byte& fromTile1, byte& fromTile2, byte& fromTile3);
	void getTileOverlap(const byte tile1, const byte tile2, int& minZ, int& maxZ);
	void printTilesAt(int x ,int y);
} mapTiles_t;

/*==============================================================
BOX AND LINE TRACING
==============================================================*/

/* This attempts to make the box tracing code thread safe. */
typedef struct boxtrace_s {
	vec3_t start, end;
	vec3_t mins, maxs;
	vec3_t absmins, absmaxs;
	vec3_t extents;
	vec3_t offset;

	trace_t trace;
	uint32_t contents;			/**< content flags to match again - MASK_ALL to match everything */
	uint32_t rejects;			/**< content flags that should be rejected in a trace - ignored when MASK_ALL is given as content flags */
	bool ispoint;				/* optimized case */

	TR_TILE_TYPE* tile;			/** The Tile to check (normally 0 - except in assembled maps) */

	void init (TR_TILE_TYPE* _tile, const int contentmask, const int brushreject, const float fraction);
	void setLineAndBox(const Line& line, const AABB& box);
} boxtrace_t;

int TR_BoxOnPlaneSide(const vec3_t mins, const vec3_t maxs, const TR_PLANE_TYPE* plane);

void TR_BuildTracingNode_r(TR_TILE_TYPE* tile, tnode_t** tnode, int32_t nodenum, int level);

#ifdef COMPILE_MAP
trace_t TR_SingleTileBoxTrace(mapTiles_t* mapTiles, const Line& traceLine, const AABB* traceBox, const int levelmask, const int brushmask, const int brushreject);
#endif
int TR_TestLine_r(TR_TILE_TYPE* tile, int32_t nodenum, const vec3_t start, const vec3_t end);
trace_t TR_BoxTrace(boxtrace_t& traceData, const Line& traceLine, const AABB& traceBox, const int headnode, const float fraction);

bool TR_TestLine(mapTiles_t* mapTiles, const vec3_t start, const vec3_t end, const int levelmask);
bool TR_TestLineDM(mapTiles_t* mapTiles, const vec3_t start, const vec3_t end, vec3_t hit, const int levelmask);
trace_t TR_TileBoxTrace(TR_TILE_TYPE* myTile, const Line& traceLine, const AABB& aabb, const int levelmask, const int brushmask, const int brushreject);
