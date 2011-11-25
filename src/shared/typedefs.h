#ifndef SHARED_TYPEDEFS_H
#define SHARED_TYPEDEFS_H

#include "defines.h"
#include "mathlib.h"
#include "../common/filesys.h"

/*
==============================================================
MAP CONFIGURATION
==============================================================
*/

typedef int ipos_t;
typedef ipos_t ipos3_t[3];

/**
 * @brief plane_t structure
 */
typedef struct cBspPlane_s {
	vec3_t normal;
	float dist;
	byte type;					/**< for fast side tests */
} cBspPlane_t;

typedef struct cBspModel_s {
	vec3_t mins, maxs;				/**< the absolute mins and maxs values of the bmodel */
	vec3_t origin, angles, shift;		/**< used to orient doors and rotating entities */
	int32_t headnode;
	/** @note Not used by ufo2map */
	int tile;					/**< which tile in assembly */
	/** @note Used only by ufo2map */
	int firstface, numfaces;	/**< submodels just draw faces without walking the bsp tree */
} cBspModel_t;

/** @sa dBspTexinfo_t */
typedef struct cBspSurface_s {
	char name[MAX_QPATH];	/**< the texture name */
	uint32_t surfaceFlags;	/**< surface flags SURF_* */
	uint32_t value;				/**< currently not used except in loading CMod_LoadSurfaces */
	byte *lightmap;			/**< lightmap samples for server side visibility lookup */
} cBspSurface_t;

typedef struct cBspNode_s {
	cBspPlane_t *plane;
	vec3_t mins, maxs;
	int32_t children[2];			/**< negative numbers are leafs */
} cBspNode_t;

typedef struct cBspBrushSide_s {
	cBspPlane_t *plane;
	cBspSurface_t *surface;
} cBspBrushSide_t;

typedef struct cBspLeaf_s {
	uint32_t contentFlags;
	unsigned short firstleafbrush;
	unsigned short numleafbrushes;
} cBspLeaf_t;

typedef struct cBspBrush_s {
	uint32_t contentFlags;			/**< the CONTENTS_* mask */
	int numsides;				/**< number of sides for this models - start to count from firstbrushside */
	int firstbrushside;			/**< first brush in the list of this model */
	int checkcount;				/**< to avoid repeated testings */
} cBspBrush_t;

/**
 * @brief Data for line tracing (?)
 */
typedef struct tnode_s {
	int type;
	vec3_t normal;
	float dist;
	int32_t children[2];
	int pad;
} tnode_t;

typedef struct chead_s {
	int cnode;
	int level;
} cBspHead_t;

/**
 * @brief Stores the data of a map tile
 */
typedef struct {
	char name[MAX_QPATH];
	int idx;

	int numbrushsides;
	cBspBrushSide_t *brushsides;

	int numtexinfo;
	cBspSurface_t *surfaces;

	int numplanes;
	cBspPlane_t *planes; /* numplanes + 12 for box hull */

	int numnodes;
	cBspNode_t *nodes; /* numnodes + 6 for box hull */

	int numleafs;
	cBspLeaf_t *leafs;
	int emptyleaf;

	int numleafbrushes;
	unsigned short *leafbrushes;

	int nummodels;
	cBspModel_t *models;

	int numbrushes;
	cBspBrush_t *brushes;

	/* tracing box */
	cBspPlane_t *box_planes;
	int box_headnode;
	cBspBrush_t *box_brush;
	cBspLeaf_t *box_leaf;

	/* line tracing */
	tnode_t *tnodes;
	int numtheads;
	intptr_t thead[LEVEL_MAX];
	int theadlevel[LEVEL_MAX];

	int numcheads;
	cBspHead_t cheads[MAX_MAP_NODES];

	ipos3_t wpMins;
	ipos3_t wpMaxs;

	byte lightquant;
	byte *lightdata;
} mapTile_t;

/**
 * @brief Pathfinding routing structure and tile layout
 * @note Comments strongly WIP!
 *
 * ROUTE
 * 	Information stored in "route"
 *
 * 	connections (see Grid_MoveCheck)
 * 	mask				description
 * 	0x10	0001 0000	connection to +x	(height ignored?)
 * 	0x20	0010 0000	connection to -x	(height ignored?)
 * 	0x40	0100 0000	connection to +y	(height ignored?)
 * 	0x80	1000 0000	connection to -y	(height ignored?)
 *
 * 	See "h = map->route[z][y][x] & 0x0F;" and "if (map->route[az][ay][ax] & 0x0F) > h)" in CM_UpdateConnection
 * 	0x0F	0000 1111	some height info?
 *
 * FALL
 * 	Information about how much you'll fall down from x,y position?
 *	I THINK as long as a bit is set you will fall down ...
 *	See "while (map->fall[ny][nx] & (1 << z)) z--;" in Grid_MoveMark
 *
 * STEP
 *
 *	0000 0000
 *	Access with "step[y][x] & (1 << z)"
 *	Guess:
 *		Each bit if set to 0 if a unit can step on it (e.g. ground or chair) or it's 1 if there is a wall or similar (i.e. it's blocked).
 * 		GERD THINKS it shows stairs and step-on stuff
 *		Search for "sh = (map->step[y][x] & (1 << z)) ? sh_big : sh_low;" and similar.
 *		"sh" seems to mean "step height"
 *
 * AREA
 *	The needed TUs to walk to a given position. (See Grid_MoveLength)
 *
 * AREASTORED
 * 	The stored mask (the cached move) of the routing data. (See Grid_MoveLength)
 *
 * TILE LAYOUT AND PATHING
 *  Maps are comprised of tiles.  Each tile has a number of levels corresponding to entities in game.
 *  All static entities in the tile are located in levels 0-255, with the main world located in 0.
 *  Levels 256-258 are reserved, see LEVEL_* constants in src/shared/shared.h.  Non-static entities
 *  (ET_BREAKABLE and ET_ROTATING, ET_DOOR, etc.) are contained in levels 259 and above. These entities'
 *  models are named *##, beginning from 1, and each corresponds to level LEVEL_MAX - 1 + ##.
 *
 *  The code that handles the pathing has separate checks for the static and non-static levels in a tile.
 *  The static levels have their bounds precalculated by CM_MakeTracingNodes and stored in tile->theads.
 *  The other levels are checked in the fly when Grid_CheckUnit is called.
 *
 */
typedef struct routing_s {
	byte stepup[PATHFINDING_HEIGHT][PATHFINDING_WIDTH][PATHFINDING_WIDTH][CORE_DIRECTIONS];
	byte route[PATHFINDING_HEIGHT][PATHFINDING_WIDTH][PATHFINDING_WIDTH][CORE_DIRECTIONS];
	signed char floor[PATHFINDING_HEIGHT][PATHFINDING_WIDTH][PATHFINDING_WIDTH];
	byte ceil[PATHFINDING_HEIGHT][PATHFINDING_WIDTH][PATHFINDING_WIDTH];
} routing_t;

typedef struct mapData_s {
	/** @note holds all entity data as a single parsable string */
	char mapEntityString[MAX_MAP_ENTSTRING];

	/** @note holds the number of inline entities, e.g. ET_DOOR */
	int numInline;

	unsigned mapChecksum;

	/** @brief Used to track where rerouting needs to occur.
	 * @todo not threadsafe */
	byte reroute[ACTOR_MAX_SIZE][PATHFINDING_WIDTH][PATHFINDING_WIDTH];

	routing_t map[ACTOR_MAX_SIZE];	/**< routing table */

	/**
	 * @note The vectors are from 0 up to 2*MAX_WORLD_WIDTH - but not negative
	 * @note holds the smallest bounding box that will contain the map
	 * @sa CL_ClampCamToMap
	 * @sa CL_OutsideMap
	 * @sa CMod_GetMapSize
	 * @sa SV_ClearWorld
	 */
	vec3_t mapMin, mapMax;
} mapData_t;

typedef struct pathing_s {
	/* TUs needed to move to this cell for the current actor */
	byte area[ACTOR_MAX_STATES][PATHFINDING_HEIGHT][PATHFINDING_WIDTH][PATHFINDING_WIDTH];
	byte areaStored[ACTOR_MAX_STATES][PATHFINDING_HEIGHT][PATHFINDING_WIDTH][PATHFINDING_WIDTH];

	/* Indicates where the actor would have moved from to get to the cell */
	dvec_t areaFrom[ACTOR_MAX_STATES][PATHFINDING_HEIGHT][PATHFINDING_WIDTH][PATHFINDING_WIDTH];

	/* forbidden list */
	pos_t **fblist;	/**< pointer to forbidden list (entities are standing here) */
	int fblength;	/**< length of forbidden list (amount of entries) */
} pathing_t;

typedef struct {
	vec3_t mins, maxs;
	vec3_t origin;			/**< for sounds or lights */
	int32_t headnode;
	int firstface, numfaces;	/**< submodels just draw faces without walking the bsp tree */
} dBspModel_t;

typedef struct {
	float point[3];
} dBspVertex_t;

typedef struct {
	vec3_t normal;
} dBspNormal_t;

/**
 * @note 0-2 are axial planes
 */
typedef struct {
	vec3_t normal;	/**< normal vector */
	float dist;		/**< distance from origin */
	int type;		/**< PLANE_X - PLANE_ANYZ */
} dBspPlane_t;

typedef struct {
	int32_t planenum;			/**< index into the planes array */
	int32_t children[2];		/**< negative numbers are -(leafs + 1), not nodes */
	short mins[3];				/**< for frustum culling */
	short maxs[3];				/**< for frustum culling */
	unsigned short firstface;	/**< index into the faces array */
	unsigned short numfaces;	/**< counting both sides */
} dBspNode_t;

/** @sa cBspSurface_t */
typedef struct texinfo_s {
	float vecs[2][4];				/**< [s/t][xyz offset] */
	uint32_t surfaceFlags;			/**< miptex flags + overrides */
	uint32_t value;					/**< light emission, etc */
	char texture[32];			/**< texture name */
} dBspTexinfo_t;

/**
 * @note note that edge 0 is never used, because negative edge nums are used for
 * counterclockwise use of the edge in a face
 */
typedef struct {
	unsigned short v[2];		/**< vertex indices */
} dBspEdge_t;

typedef struct {
	uint16_t planenum;	/**< planenum is used in lighting stage, but not in ufo */
	short side;

	int firstedge;				/**< we must support > 64k edges */
	short numedges;
	short texinfo;				/**< index in the global texinfo array */

	/** lighting info */
	int lightofs[LIGHTMAP_MAX];				/**< start of [surfsize] samples */
} dBspSurface_t;

/** @brief convex region of space in the BSP tree */
typedef struct {
	uint32_t contentFlags;				/**< OR of all brushes */

	short area;

	short mins[3];				/**< for frustum culling */
	short maxs[3];				/**< for frustum culling */

	uint16_t firstleafbrush;
	uint16_t numleafbrushes;
} dBspLeaf_t;

typedef struct {
	uint16_t planenum;			/**< facing out of the leaf
								 * index info the planes array for this side */
	short texinfo;
} dBspBrushSide_t;

typedef struct {
	int firstbrushside;
	int numsides;
	uint32_t contentFlags;				/**< OR of all brushes */
} dBspBrush_t;

typedef struct {
	/* tracing box */
	dBspPlane_t *box_planes;
	int box_headnode;
	dBspBrush_t *box_brush;
	dBspLeaf_t *box_leaf;

	/* line tracing */
	tnode_t *tnodes;
	int numtheads;
	int thead[LEVEL_MAX];
	int theadlevel[LEVEL_MAX];

    /* Used by TR_TileBoxTrace */
	int numcheads;
	cBspHead_t cheads[MAX_MAP_NODES];

	/* ---- */
	int				entdatasize;
	char			entdata[MAX_MAP_ENTSTRING];

	int				routedatasize;
	byte			routedata[MAX_MAP_ROUTING];

	int				lightdatasize[LIGHTMAP_MAX];
	byte			lightdata[LIGHTMAP_MAX][MAX_MAP_LIGHTING];

	int				nummodels;
	dBspModel_t		models[MAX_MAP_MODELS];

	int				numleafs;
	dBspLeaf_t		leafs[MAX_MAP_LEAFS];
	int				emptyleaf;

	int				numplanes;
	dBspPlane_t		planes[MAX_MAP_PLANES];

	int				numnormals;
	dBspNormal_t	normals[MAX_MAP_VERTS];

	int				numvertexes;
	dBspVertex_t	vertexes[MAX_MAP_VERTS];

	int				numnodes;
	dBspNode_t		nodes[MAX_MAP_NODES];

	int				numtexinfo;
	dBspTexinfo_t	texinfo[MAX_MAP_TEXINFO];

	int				numfaces;
	dBspSurface_t		faces[MAX_MAP_FACES];

	int				numedges;
	dBspEdge_t		edges[MAX_MAP_EDGES];

	int				numleafbrushes;
	unsigned short	leafbrushes[MAX_MAP_LEAFBRUSHES];

	/**
	 * references the edges array
	 * positive or negative values are possible.
	 * the absolute value is the index into the edges array
	 * positive: the edge is defined from the first to the second vertex
	 * negative: the second to the first vertex
	 */
	int				numsurfedges;
	int				surfedges[MAX_MAP_SURFEDGES];

	int				numbrushes;
	dBspBrush_t		dbrushes[MAX_MAP_BRUSHES];
	cBspBrush_t		brushes[MAX_MAP_BRUSHES];

	int				numbrushsides;
	dBspBrushSide_t	brushsides[MAX_MAP_BRUSHSIDES];
} dMapTile_t;

#endif /* SHARED_TYPEDEFS_H */
