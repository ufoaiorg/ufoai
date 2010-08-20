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

/*==============================================================
CMODEL
==============================================================*/
#include "../common/qfiles.h"
#include "pqueue.h"

void CM_LoadMap(const char *tiles, qboolean day, const char *pos, mapData_t *mapData);
cBspModel_t *CM_InlineModel(const char *name);
void CM_SetInlineModelOrientation(const char *name, const vec3_t origin, const vec3_t angles);

/*==============================================================
CMODEL BOX TRACING
==============================================================*/

/** creates a clipping hull for an arbitrary box */
int CM_HeadnodeForBox(int tile, const vec3_t mins, const vec3_t maxs);
trace_t CM_CompleteBoxTrace(const vec3_t start, const vec3_t end, const vec3_t mins, const vec3_t maxs, int tile, int headnode, int brushmask, int brushrejects, const vec3_t origin, const vec3_t angles);
trace_t CM_HintedTransformedBoxTrace(const int tile, const vec3_t start, const vec3_t end, const vec3_t mins, const vec3_t maxs, const int headnode, const int brushmask, const int brushrejects, const vec3_t origin, const vec3_t angles, const vec3_t rmaShift, const float fraction);
qboolean CM_TestLineWithEnt(const vec3_t start, const vec3_t stop, const int levelmask, const char **entlist);
qboolean CM_TestLineDMWithEnt(const vec3_t start, const vec3_t stop, vec3_t end, const int levelmask, const char **entlist);
trace_t CM_EntCompleteBoxTrace(const vec3_t start, const vec3_t end, const box_t* traceBox, int levelmask, int brushmask, int brushreject, const char **list);
void CM_RecalcRouting(routing_t *map, const char *name, const char **list);

