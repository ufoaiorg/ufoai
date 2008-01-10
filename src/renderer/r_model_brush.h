/**
 * @file r_model_brush.h
 * @brief brush model loading
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

#ifndef R_MODEL_BRUSH_H
#define R_MODEL_BRUSH_H

/*
==============================================================================
BRUSH MODELS
==============================================================================
*/


/** in memory representation */
typedef struct mBspVertex_s {
	vec3_t position;
} mBspVertex_t;

typedef struct mBspHeader_s {
	vec3_t mins, maxs;
	vec3_t origin;				/**< for sounds or lights */
	float radius;
	int headnode;
	int visleafs;				/**< not including the solid leaf 0 */
	int firstface, numfaces;
} mBspHeader_t;

#define	MSURF_PLANEBACK		1
#define	MSURF_LIGHTMAP		2

typedef struct mBspEdge_s {
	unsigned short v[2];
} mBspEdge_t;

typedef struct mBspTexInfo_s {
	float vecs[2][4];
	int flags;
	int numframes;
	struct mBspTexInfo_s *next;	/**< animation chain */
	image_t *image;
} mBspTexInfo_t;

typedef struct mBspPoly_s {
	int numverts;
	float *verts;	/**< verts */
	float *texcoords; /**< diffuse texcoords */
	float *lmtexcoords; /**< lightmap texcoords */
} mBspPoly_t;

typedef struct mBspSurface_s {
	cBspPlane_t *plane;
	int flags;

	int firstedge;				/**< look up in model->surfedges[], negative numbers */
	int numedges;				/**< are backwards edges */

	short stmins[2];		/**< st min coordinates */
	short stmaxs[2];			/**< st max coordinates */

	vec3_t center;
	vec3_t color;
	vec3_t normal;

	int light_s, light_t;		/**< gl lightmap coordinates */
	byte lquant;

	mBspPoly_t *polys;

	mBspTexInfo_t *texinfo;

	int lightmaptexturenum;
	byte styles[MAXLIGHTMAPS];
	byte *samples;				/**< lightmap samples - only used at loading time */
	byte *lightmap;				/**< finalized lightmap samples, cached for lookups */
} mBspSurface_t;

#define MAX_RENDERER_SURFACES 32768
/* surfaces are assembled according to bsp order into arrays */
typedef struct msurfaces_s {
	mBspSurface_t *surfaces[MAX_RENDERER_SURFACES];
	int count;
} mBspSurfaces_t;

#define NODE_NO_LEAF -1

typedef struct mBspNode_s {
	/* common with leaf */
	int contents;				/**< -1, to differentiate from leafs */
	float minmaxs[6];			/**< for bounding box culling */

	struct mBspNode_s *parent;

	/* node specific */
	cBspPlane_t *plane;
	struct mBspNode_s *children[2];

	unsigned short firstsurface;
	unsigned short numsurfaces;
} mBspNode_t;

typedef struct mBspLeaf_s {
	/* common with node */
	int contents;				/**< will be a negative contents number */

	float minmaxs[6];			/**< for bounding box culling */

	struct mnode_s *parent;
} mBspLeaf_t;

/** @brief brush model */
typedef struct mBspModel_s {
	int firstmodelsurface, nummodelsurfaces;

	int numsubmodels;
	mBspHeader_t *submodels;

	int numplanes;
	cBspPlane_t *planes;

	int numleafs;				/**< number of visible leafs, not counting 0 */
	mBspLeaf_t *leafs;

	int numvertexes;
	mBspVertex_t *vertexes;

	int numedges;
	mBspEdge_t *edges;

	int numnodes;
	int firstnode;
	mBspNode_t *nodes;

	int numtexinfo;
	mBspTexInfo_t *texinfo;

	int numsurfaces;
	mBspSurface_t *surfaces;

	int numsurfedges;
	int *surfedges;

	byte lightquant;
	byte *lightdata;
} mBspModel_t;

void R_CreateSurfaceLightmap(mBspSurface_t * surf);
void R_EndBuildingLightmaps(void);
void R_BeginBuildingLightmaps(void);

#endif /* R_MODEL_BRUSH_H */
