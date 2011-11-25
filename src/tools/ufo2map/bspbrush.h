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

#include "../../shared/mathlib.h"	/* for vec3_t */

#if 0
/* the code is not yet ready for this */
struct bspbrush_s;
typedef struct bspbrush_s bspbrush_t;
#else
typedef struct bspbrush_s {
	struct bspbrush_s	*next;
	vec3_t	mins, maxs;
	int		side;		/**< side of node during construction */
	int		testside;
	struct mapbrush_s	*original;
	int		numsides;
	side_t	sides[6];			/**< variably sized */
} bspbrush_t;
#endif

bspbrush_t *BrushFromBounds(vec3_t mins, vec3_t maxs);
bspbrush_t *CopyBrush(const bspbrush_t *brush);
side_t *SelectSplitSide(bspbrush_t *brushes, bspbrush_t *volume);
void SplitBrushList(bspbrush_t *brushes, uint16_t planenum, bspbrush_t **front, bspbrush_t **back);
void SplitBrush(const bspbrush_t *brush, uint16_t planenum, bspbrush_t **front, bspbrush_t **back);
bspbrush_t *AllocBrush(int numsides);
int	CountBrushList(bspbrush_t *brushes);
void FreeBrush(bspbrush_t *brushes);
void FreeBrushList(bspbrush_t *brushes);
uint32_t BrushListCalcContents(bspbrush_t *brushlist);
void BrushlistCalcStats(bspbrush_t *brushlist, vec3_t mins, vec3_t maxs);

#endif /* UFO2MAP_BSPBRUSH_H */
