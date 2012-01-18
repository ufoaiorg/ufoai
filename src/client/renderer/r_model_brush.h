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
	vec3_t normal;
} mBspVertex_t;

typedef struct mBspHeader_s {
	vec3_t mins, maxs;
	vec3_t origin;
	float radius;
	int headnode;
	int visleafs;				/**< not including the solid leaf 0 */
	int firstface, numfaces;	/**< indices in the bsp surfaces list */
} mBspHeader_t;

#define	MSURF_PLANEBACK		1
#define	MSURF_LIGHTMAP		2

typedef struct mBspEdge_s {
	unsigned short v[2];
} mBspEdge_t;

/**
 * Apply texture with a planar texture mapping
 * @note Texture coordinates for a vector v are found with this computation:
 * @code
 * float u = v[0] * uv[0] + v[1] * uv[1] + v[2] * uv[2] + u_offset
 * float v = v[0] * vv[0] + v[1] * vv[1] + v[2] * vv[2] + v_offset
 * @endcode
 */
typedef struct mBspTexInfo_s {
	vec3_t uv;
	float u_offset;
	vec3_t vv;
	float v_offset;
	uint32_t flags;	/**< surfaceflags */
	image_t *image;
} mBspTexInfo_t;

typedef struct mBspFlare_s {
	vec3_t origin;
	float radius;
	const image_t *image;
	vec3_t color;
	float time;
	float alpha;
} mBspFlare_t;

typedef struct mBspSurface_s {
	cBspPlane_t *plane;
	int flags;
	int tile;				/**< index in r_mapTiles (loaded bsp map index) this surface belongs, to */

	int frame;	/**< used to decide whether this surface should be drawn */

	/** look up in model->surfedges[], negative numbers are backwards edges */
	int firstedge;
	int numedges;

	short stmins[2];		/**< st min coordinates */
	short stmaxs[2];			/**< st max coordinates */
	vec2_t stcenter;
	vec2_t stextents;

	vec3_t mins;
	vec3_t maxs;

	vec3_t center;
	vec4_t color;
	vec3_t normal;

	int light_s, light_t;		/**< gl lightmap coordinates */
	int lightmap_scale;

	unsigned int index;

	int tracenum;

	mBspTexInfo_t *texinfo;

	mBspFlare_t *flare;

	int lightmap_texnum;
	int deluxemap_texnum;

	byte *samples;				/**< lightmap samples - only used at loading time */
	byte *lightmap;				/**< finalized lightmap samples, cached for lookups */

	int lightframe;				/**< dynamic lighting frame */
	int lights;					/**< bitmask of dynamic light sources */

	qboolean isOriginBrushModel;	/**< func_door, func_rotating - if this is true the vertices for this surface
									 * won't get shifted in case of an rma - these surfaces are translated by
									 * their entities origin vector (which was given by the CONTENTS_ORIGIN flag
									 * in ufo2map and removed from the bsp later) */
} mBspSurface_t;

/**
 * @brief surfaces are assigned to arrays based on their primary rendering type
 * and then sorted by world texture index to reduce binds
 */
typedef struct mBspSurfaces_s {
	mBspSurface_t **surfaces;
	int count;
} mBspSurfaces_t;

typedef enum {
	S_OPAQUE,
	S_OPAQUE_WARP,
	S_ALPHA_TEST,
	S_BLEND,
	S_BLEND_WARP,
	S_MATERIAL,
	S_FLARE,

	NUM_SURFACES_ARRAYS
} surfaceArrayType_t;

#define opaque_surfaces			sorted_surfaces[S_OPAQUE]
#define opaque_warp_surfaces	sorted_surfaces[S_OPAQUE_WARP]
#define alpha_test_surfaces		sorted_surfaces[S_ALPHA_TEST]
#define blend_surfaces			sorted_surfaces[S_BLEND]
#define blend_warp_surfaces		sorted_surfaces[S_BLEND_WARP]
#define material_surfaces		sorted_surfaces[S_MATERIAL]
#define flare_surfaces			sorted_surfaces[S_FLARE]

#define R_AddSurfaceToArray(array, surf)\
	(array)->surfaces[(array)->count++] = surf

#define CONTENTS_NODE -1
#define CONTENTS_PATHFINDING_NODE -2

/**
 * @note The mBspLeaf_t type shares the same first variables - don't change the order */
typedef struct mBspNode_s {
	/* common with leaf */
	int contents;				/**< -1, to differentiate from leafs */
	float minmaxs[6];			/**< for bounding box culling */

	struct mBspNode_s *parent;

	struct model_s *model;

	/* node specific */
	cBspPlane_t *plane;
	struct mBspNode_s *children[2];

	unsigned short firstsurface;
	unsigned short numsurfaces;
} mBspNode_t;

typedef struct mBspLeaf_s {
	/* common with mBspNode_t */
	int contents;				/**< will be a negative contents number */
	float minmaxs[6];			/**< for bounding box culling */

	mBspNode_t *parent;

	struct model_s *model;
} mBspLeaf_t;

/** @brief brush model */
typedef struct mBspModel_s {
	/* range of surface numbers in this (sub)model */
	int firstmodelsurface, nummodelsurfaces;
	int maptile;		/**< the maptile the surface indices belongs to */

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

	/* vertex arrays */
	float *verts;
	float *texcoords;
	float *lmtexcoords;
	float *tangents;
	float *normals;

	/* vertex buffer objects */
	unsigned int vertex_buffer;
	unsigned int texcoord_buffer;
	unsigned int lmtexcoord_buffer;
	unsigned int tangent_buffer;
	unsigned int normal_buffer;

	byte lightquant;
	byte *lightdata;

	/* sorted surfaces arrays */
	mBspSurfaces_t *sorted_surfaces[NUM_SURFACES_ARRAYS];
} mBspModel_t;

#endif /* R_MODEL_BRUSH_H */
