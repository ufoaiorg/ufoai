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
#include "../qcommon/qfiles.h"

#define MAX_MAPTILES	256

extern vec3_t map_min, map_max;

void CM_LoadMap(char *tiles, char *pos, unsigned *checksum);
int CheckBSPFile(const char *filename);
cBspModel_t *CM_InlineModel(const char *name);	/* *0, *1, *2, etc */

int CM_NumClusters(void);
int CM_NumInlineModels(void);
char *CM_EntityString(void);

/*==============================================================
CMODEL BOX TRACING
==============================================================*/

/** creates a clipping hull for an arbitrary box */
int CM_HeadnodeForBox(int tile, vec3_t mins, vec3_t maxs);

trace_t CM_TransformedBoxTrace(vec3_t start, vec3_t end, vec3_t mins, vec3_t maxs, int tile, int headnode, int brushmask, vec3_t origin, vec3_t angles);
trace_t CM_CompleteBoxTrace(vec3_t start, vec3_t end, vec3_t mins, vec3_t maxs, int levelmask, int brushmask);

int CM_TestLine(vec3_t start, vec3_t stop);
int CM_TestLineDM(vec3_t start, vec3_t stop, vec3_t end);


/*==========================================================
GRID ORIENTED MOVEMENT AND SCANNING
==========================================================*/

extern struct routing_s svMap, clMap;

void Grid_RecalcRouting(struct routing_s *map, char *name, char **list);
void Grid_MoveCalc(struct routing_s *map, pos3_t from, int distance, byte ** fb_list, int fb_length);
void Grid_MoveStore(struct routing_s *map);
int Grid_MoveLength(struct routing_s *map, pos3_t to, qboolean stored);
int Grid_MoveNext(struct routing_s *map, pos3_t from);
int Grid_Height(struct routing_s *map, pos3_t pos);
int Grid_Fall(struct routing_s *map, pos3_t pos);
void Grid_PosToVec(struct routing_s *map, pos3_t pos, vec3_t vec);


/*==========================================================
MISC WORLD RELATED
==========================================================*/

float Com_GrenadeTarget(vec3_t from, vec3_t at, float speed, qboolean launched, qboolean rolled, vec3_t v0);
