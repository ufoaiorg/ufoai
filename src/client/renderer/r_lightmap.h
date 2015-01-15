/**
 * @file
 * @brief lightmap definitions
 */

/*
All original material Copyright (C) 2002-2015 UFO: Alien Invasion.

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

#include "r_entity.h"

#define LIGHTMAP_DEFAULT_PAGE_SIZE 1024
#define LIGHTMAP_MAX_PAGE_SIZE 4096

#define LIGHTMAP_SAMPLE_SIZE 3 /* RGB */
#define DELUXEMAP_SAMPLE_SIZE 3 /* XYZ */

void R_CreateSurfaceLightmap(mBspSurface_t* surf);
void R_EndBuildingLightmaps(void);
void R_BeginBuildingLightmaps(void);

void R_Trace(const Line& trLine, float size, int contentmask);

typedef struct lightmaps_s {
	GLuint lightmap_texnums[MAX_GL_LIGHTMAPS];
	GLuint deluxemap_texnums[MAX_GL_DELUXEMAPS];

	int lightmap_count;
	int deluxemap_count;

	bool incomplete_atlas;

	int size;				/**< lightmap atlas page (NxN) */

	unsigned* allocated;	/**< current atlas page heightmap */

	byte* sample_buffer;	/**< RGB color buffer for assembling the atlas page */
	byte* direction_buffer;	/**< XYZ direction buffer for assembling the atlas page */
} lightmaps_t;

extern lightmaps_t r_lightmaps;
