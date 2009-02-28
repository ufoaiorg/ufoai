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

void CheckTexturesBasedOnFlags(void);
void CheckFlagsBasedOnTextures(void);
void CheckLevelFlags(void);
void CheckFillLevelFlags(void);
void CheckBrushes(void);
void CheckEntities(void);
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
void Check_Free(void);
void FreeWindings(void);
mapbrush_t **Check_ExtraBrushesForWorldspawn (int *numBrushes);
