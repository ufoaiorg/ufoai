/**
 * @file grid.h
 * @brief Battlescape grid functions
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

#ifndef GRID_H_
#define GRID_H_

/*==========================================================
GRID ORIENTED MOVEMENT AND SCANNING
==========================================================*/

void Grid_RecalcRouting(mapTiles_t *mapTiles, routing_t *map, const char *name, const char **list);
void Grid_MoveCalc(const routing_t *map, const actorSizeEnum_t actorSize, pathing_t *path, const pos3_t from, byte crouchingSstate, int distance, byte ** fb_list, int fb_length);
void Grid_MoveStore(pathing_t *path);
pos_t Grid_MoveLength(const pathing_t *path, const pos3_t to, byte crouchingState, qboolean stored);
int Grid_MoveNext(const pathing_t *path, const pos3_t toPos, byte crouchingState);
int Grid_Height(const routing_t *map, const actorSizeEnum_t actorSize, const pos3_t pos);
unsigned int Grid_Ceiling(const routing_t *map, const actorSizeEnum_t actorSize, const pos3_t pos);
int Grid_Floor(const routing_t *map, const actorSizeEnum_t actorSize, const pos3_t pos);
pos_t Grid_StepUp(const routing_t *map, const actorSizeEnum_t actorSize, const pos3_t pos, const int dir);
int Grid_GetTUsForDirection(const int dir, const int crouched);
int Grid_Filled(const routing_t *map, const actorSizeEnum_t actorSize, const pos3_t pos);
pos_t Grid_Fall(const routing_t *map, const actorSizeEnum_t actorSize, const pos3_t pos);
void Grid_PosToVec(const routing_t *map, const actorSizeEnum_t actorSize, const pos3_t pos, vec3_t vec);

#endif /* GRID_H_ */
