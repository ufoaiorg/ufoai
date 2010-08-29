/**
 * @file lighting.h
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

#include "common/shared.h"
#include "common/bspfile.h"
#include "common/polylib.h"

#define	MAX_PATCHES	65000			/* larger will cause 32 bit overflows */

typedef struct patch_s {
	dBspSurface_t 		*face;
	winding_t		*winding;

	vec3_t			origin;
	vec3_t			normal;

	float			area;
	vec3_t			light;		/**< emissive surface light */

	struct patch_s	*next;		/**< next in face */
} patch_t;

extern patch_t *face_patches[MAX_MAP_FACES];
/** for rotating bmodels */
extern vec3_t face_offset[MAX_MAP_FACES];

void BuildFacelights(unsigned int facenum);
void FinalLightFace(unsigned int facenum);
void ExportLightmaps(const char *bspFileName);
void BuildLights(void);

void BuildPatches(void);
void SubdividePatches(void);
void CalcTextureReflectivity(void);
void LightWorld(void);
void BuildVertexNormals(void);
void FreePatches(void);
