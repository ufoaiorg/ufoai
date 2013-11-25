/**
 * @file
 * @brief Common model code header (for bsp and others)
 */

/*
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

/*==============================================================
CMODEL
==============================================================*/
#include "tracing.h"
#include "qfiles.h"

void CM_LoadMap(const char* tiles, bool day, const char* pos, const char* entityString, mapData_t* mapData, mapTiles_t* mapTiles);
cBspModel_t* CM_InlineModel(const mapTiles_t* mapTiles, const char* name);
cBspModel_t* CM_SetInlineModelOrientation(mapTiles_t* mapTiles, const char* name, const vec3_t origin, const vec3_t angles);
void CM_GetInlineModelAABB(mapTiles_t* mapTiles, const char* name, AABB& aabb);
float CM_GetVisibility(const mapTiles_t* mapTiles, const pos3_t position);
void CM_LoadBsp(MapTile& tile, const dBspHeader_t& header, const vec3_t shift, const byte* base);

/*==============================================================
CMODEL BOX TRACING
==============================================================*/

/** creates a clipping hull for an arbitrary box */
int32_t CM_HeadnodeForBox(MapTile& tile, const AABB& box);
trace_t CM_HintedTransformedBoxTrace(MapTile& tile, const Line& traceLine, const AABB& traceBox, const int headnode, const int brushmask, const int brushrejects, const vec3_t origin, const vec3_t angles, const vec3_t rmaShift, const float fraction);
#define CM_TransformedBoxTrace(tile, line, box, headnode, brushmask, brushreject, origin, angles) CM_HintedTransformedBoxTrace(tile, line, box, headnode, brushmask, brushreject, origin, angles, vec3_origin, 1.0f);
trace_t CM_EntCompleteBoxTrace(mapTiles_t* mapTiles, const Line& traceLine, const AABB* traceBox, int levelmask, int brushmask, int brushreject, const char** list);
bool CM_EntTestLineDM(mapTiles_t* mapTiles, const Line& trLine, vec3_t hit, const int levelmask, const char** entlist);
bool CM_EntTestLine(mapTiles_t* mapTiles, const Line& traceLine, const int levelmask, const char** entlist);
trace_t CM_CompleteBoxTrace(mapTiles_t* mapTiles, const Line& trLine, const AABB& box, int levelmask, int brushmask, int brushreject);
