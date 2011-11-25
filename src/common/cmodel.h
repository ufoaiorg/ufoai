/**
 * @file cmodel.h
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

#ifndef _COMMON_CMODEL_H
#define _COMMON_CMODEL_H

/*==============================================================
CMODEL
==============================================================*/
#include "tracing.h"

void CM_LoadMap(const char *tiles, qboolean day, const char *pos, mapData_t *mapData, mapTiles_t *mapTiles);
cBspModel_t *CM_InlineModel(const mapTiles_t *mapTiles, const char *name);
cBspModel_t *CM_SetInlineModelOrientation(mapTiles_t *mapTiles, const char *name, const vec3_t origin, const vec3_t angles);
float CM_GetVisibility(const mapTiles_t *mapTiles, const pos3_t position);

/*==============================================================
CMODEL BOX TRACING
==============================================================*/

/** creates a clipping hull for an arbitrary box */
int32_t CM_HeadnodeForBox(mapTile_t *tile, const vec3_t mins, const vec3_t maxs);
trace_t CM_HintedTransformedBoxTrace(mapTile_t *tile, const vec3_t start, const vec3_t end, const vec3_t mins, const vec3_t maxs, const int headnode, const int brushmask, const int brushrejects, const vec3_t origin, const vec3_t angles, const vec3_t rmaShift, const float fraction);
#define CM_TransformedBoxTrace(tile, start, end, mins, maxs, headnode, brushmask, brushreject, origin, angles) CM_HintedTransformedBoxTrace(tile, start, end, mins, maxs, headnode, brushmask, brushreject, origin, angles, vec3_origin, 1.0f);
trace_t CM_EntCompleteBoxTrace(mapTiles_t *mapTiles, const vec3_t start, const vec3_t end, const box_t* traceBox, int levelmask, int brushmask, int brushreject, const char **list);
qboolean CM_EntTestLineDM(mapTiles_t *mapTiles, const vec3_t start, const vec3_t stop, vec3_t end, const int levelmask, const char **entlist);
qboolean CM_EntTestLine(mapTiles_t *mapTiles, const vec3_t start, const vec3_t stop, const int levelmask, const char **entlist);
trace_t CM_CompleteBoxTrace(mapTiles_t *mapTiles, const vec3_t start, const vec3_t end, const vec3_t mins, const vec3_t maxs, int levelmask, int brushmask, int brushreject);
#endif
