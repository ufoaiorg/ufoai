/**
 * @file check.h
 * @brief Performs check on a loaded mapfile, and makes changes
 * that can be saved back to the source map.
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

#ifndef UFO2MAP_CHECK_CHECK_H
#define UFO2MAP_CHECK_CHECK_H

#include "../map.h"

/** @sa Check_FindCompositeSides */
typedef struct compositeSide_s {
	struct side_s **memberSides;
	int numMembers;
} compositeSide_t;

/** an array of composite mapbrush sides.
 *  @sa Check_FindCompositeSides ensures the array is populated
 *  @note a composite must have at least 2 members. composites should not be duplicated,
 *  hence the largest possible number is the number of sides divided by two.
 */
compositeSide_t compositeSides[MAX_MAP_SIDES / 2];
int numCompositeSides;

void CheckTexturesBasedOnFlags(void);
void CheckFlagsBasedOnTextures(void);
void CheckLevelFlags(void);
void CheckFillLevelFlags(void);
void CheckBrushes(void);
void CheckNodraws(void);
void CheckMixedFaceContents(void);
void CheckMapMicro(void);
void Check_BrushIntersection(void);
void FixErrors(void);
void DisplayContentFlags(const int flags);
void SetImpliedFlags (side_t *side, brush_texture_t *tex, const mapbrush_t *brush);
void CheckPropagateParserContentFlags(mapbrush_t *b);
void Check_ContainedBrushes(void);
void CheckZFighting(void);

#endif
