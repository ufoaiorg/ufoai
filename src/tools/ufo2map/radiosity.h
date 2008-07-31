/**
 * @file radiosity.h
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
#include "common/cmdlib.h"
#include "common/bspfile.h"
#include "common/polylib.h"
#include "common/imagelib.h"

#ifdef _WIN32
#include <windows.h>
#endif

typedef enum {
	emit_surface,
	emit_point,
	emit_spotlight
} emittype_t;

typedef struct directlight_s {
	struct directlight_s *next;
	emittype_t	type;

	float		intensity;
	vec3_t		origin;
	vec3_t		color;
	vec3_t		normal;		/**< for surfaces and spotlights */
	float		stopdot;	/**< for spotlights */
} directlight_t;


/**
 * @note the sum of all tranfer->transfer values for a given patch
 * should equal exactly 0x10000, showing that all radiance
 * reaches other patches
 */
typedef struct {
	unsigned short	patch;
	unsigned short	transfer;
} transfer_t;


#define	MAX_PATCHES	65000			/* larger will cause 32 bit overflows */

typedef struct patch_s {
	winding_t	*winding;
	struct patch_s		*next;		/**< next in face */
	int			numtransfers;
	transfer_t	*transfers;

	vec3_t		origin;
	dBspPlane_t	*plane;

	vec3_t		totallight;			/**< accumulated by radiosity
									 * does NOT include light
									 * accounted for by direct lighting */
	float		area;

	/** illuminance * reflectivity = radiosity */
	vec3_t		reflectivity;
	vec3_t		baselight;			/**< emissivity only */

	/** each style 0 lightmap sample in the patch will be
	 * added up to get the average illuminance of the entire patch */
	vec3_t		samplelight;
	int			samples;		/**< for averaging direct light */
} patch_t;

extern patch_t *face_patches[MAX_MAP_FACES];
extern entity_t *face_entity[MAX_MAP_FACES];
extern patch_t patches[MAX_PATCHES];
extern unsigned num_patches;

/*============================================== */

void LinkPlaneFaces(void);
void BuildFacelights(unsigned int facenum);
void FinalLightFace(unsigned int facenum);
void CreateDirectLights(void);

dBspLeaf_t *Rad_PointInLeaf(const vec3_t point);

extern dBspPlane_t backplanes[MAX_MAP_PLANES];

void MakePatches(void);
void SubdividePatches(const int num);
void CalcTextureReflectivity(void);
void RadWorld(void);
void BuildVertexNormals(void);
