/**
 * @file src/tools/ufo2map/map.h
 */

/*
All original material Copyright (C) 2002-2011 UFO: Alien Invasion.

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

#ifndef UFO2MAP_TEXTURES_H
#define UFO2MAP_TEXTURES_H

#include "../../shared/mathlib.h"	/* for vec3_t */
#include "../../shared/defines.h"
#include "../../shared/ufotypes.h"

typedef struct {
	char	name[MAX_TEXPATH];
	qboolean	footstepMarked; /**< only print it once to the footsteps file */
	qboolean	materialMarked; /**< only print it once to the material file */
} textureref_t;


extern	textureref_t	textureref[MAX_MAP_TEXTURES];
int	FindMiptex(const char *name);
int TexinfoForBrushTexture(plane_t *plane, brush_texture_t *bt, const vec3_t origin, qboolean isTerrain);

#endif
