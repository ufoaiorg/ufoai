/**
 * @file bspbrush.h
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

#ifndef UFO2MAP_BSPBRUSH_H
#define UFO2MAP_BSPBRUSH_H

#include "../../shared/ufotypes.h"	/* for qboolean */
/* the code is not yet ready for this
struct bspbrush_s;
typedef struct bspbrush_s bspbrush_t;
*/

bspbrush_t *BrushFromBounds(vec3_t mins, vec3_t maxs);
bspbrush_t *CopyBrush(const bspbrush_t *brush);
void SplitBrush(const bspbrush_t *brush, int planenum, bspbrush_t **front, bspbrush_t **back);
bspbrush_t *AllocBrush(int numsides);
int	CountBrushList(bspbrush_t *brushes);
void FreeBrush(bspbrush_t *brushes);
void FreeBrushList(bspbrush_t *brushes);
node_t *BuildTree_r(node_t *node, bspbrush_t *brushes);
void BrushlistCalcStats(bspbrush_t *brushlist, vec3_t mins, vec3_t maxs);
tree_t *BuildTree(bspbrush_t *brushlist, vec3_t mins, vec3_t maxs);
void WriteBSPBrushMap(const char *name, const bspbrush_t *list);

#endif /* UFO2MAP_BSPBRUSH_H */
