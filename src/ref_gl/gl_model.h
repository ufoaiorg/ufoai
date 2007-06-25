/**
 * @file gl_model.h
 * @brief Brush model header file
 * @note d*_t structures are on-disk representations
 * @note m*_t structures are in-memory
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


#define	SIDE_FRONT	0
#define	SIDE_BACK	1
#define	SIDE_ON		2


#define	SURF_PLANEBACK		2
#define SURF_DRAWTURB		0x10
#define SURF_DRAWBACKGROUND	0x40
#define SURF_UNDERWATER		0x80

/* !!! if this is changed, it must be changed in asm_draw.h too !!! */
typedef struct mBspEdge_s {
	unsigned short v[2];
	unsigned int cachededgeoffset;
} mBspEdge_t;

typedef struct mBspTexInfo_s {
	float vecs[2][4];
	int flags;
	int numframes;
	struct mBspTexInfo_s *next;	/**< animation chain */
	image_t *image;
} mBspTexInfo_t;

#define	VERTEXSIZE	7

typedef struct mBspPoly_s {
	struct mBspPoly_s *next;
	struct mBspPoly_s *chain;
	int numverts;
	float verts[4][VERTEXSIZE];	/**< variable sized (xyz s1t1 s2t2) */
} mBspPoly_t;

typedef struct mBspSurface_s {
	cBspPlane_t *plane;
	int flags;

	int firstedge;				/**< look up in model->surfedges[], negative numbers */
	int numedges;				/**< are backwards edges */

	short texturemins[2];
	short extents[2];

	int light_s, light_t;		/**< gl lightmap coordinates */
	int dlight_s, dlight_t;		/**< gl lightmap coordinates for dynamic lightmaps */
	byte lquant;

	mBspPoly_t *polys;			/* multiple if warped */
	struct mBspSurface_s *texturechain;
	struct mBspSurface_s *lightmapchain;

	mBspTexInfo_t *texinfo;

	/* lighting info */
	int dlightframe;
	int dlightbits;

	int lightmaptexturenum;
	byte styles[MAXLIGHTMAPS];
	float cached_light[MAXLIGHTMAPS];	/**< values currently used in lightmap */
	byte *samples;				/**< [numstyles*surfsize] */
} mBspSurface_t;

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

	/** leaf specific */
	int cluster;
	int area;

	mBspSurface_t **firstmarksurface;
	int nummarksurfaces;
} mBspLeaf_t;

typedef struct mnormals_s {
	vec3_t ijk;
} mnormals_t;

#define MAX_ANIMS		128
#define MAX_ANIMNAME	16

typedef struct manim_s {
	char name[MAX_ANIMNAME];
	int from, to;
	int time;
} manim_t;

/*=================================================================== */

/* Whole model */

/**
 * @brief All supported model formats
 * @sa mod_extensions
 */
typedef enum {mod_bad, mod_brush, mod_sprite, mod_alias_md2, mod_alias_md3, mod_obj} modtype_t;

typedef struct {
	int n[3];
} neighbors_t;

typedef struct model_s {
	/** the name needs to be the first entry in the struct */
	char name[MAX_QPATH];

	int registration_sequence;

	modtype_t type;	/**< model type */
	int numframes;

	int flags;

	/** volume occupied by the model graphics */
	vec3_t mins, maxs;
	float radius;

	/** solid volume for clipping */
	qboolean clipbox;
	vec3_t clipmins, clipmaxs;

	/** brush model */
	int firstmodelsurface, nummodelsurfaces;
	int lightmap;				/**< only for submodels */

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

	int nummarksurfaces;
	mBspSurface_t **marksurfaces;

	int numnormals;			/**< number of normal vectors */
	mnormals_t *normals;

	byte lightquant;
	byte *lightdata;

	/** for alias models and skins */
	image_t *skins[MD2_MAX_SKINS];

	void *extraData;

	/** tag data */
	char tagname[MAX_QPATH];
	void *tagdata;

	qboolean noshadow;
	/** animation data */
	char animname[MAX_QPATH];
	int numanims;
	manim_t *animdata;
	neighbors_t *neighbors;
} model_t;

/*============================================================================ */

#define MAX_MOD_KNOWN   512

void Mod_ClearAll(void);
void Mod_Modellist_f(void);
void Mod_FreeAll(void);
void Mod_DrawModelBBox(vec4_t bbox[8], entity_t *e);

#include "gl_model_alias.h"
#include "gl_model_brush.h"
#include "gl_model_md2.h"
#include "gl_model_md3.h"
#include "gl_model_sp2.h"

extern model_t mod_known[MAX_MOD_KNOWN];
extern int mod_numknown;

extern model_t mod_inline[MAX_MOD_KNOWN];
extern int numInline;

