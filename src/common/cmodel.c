/**
 * @file cmodel.c
 * @brief model loading and grid oriented movement and scanning
 * @note collision detection code (server side)
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

#include "common.h"
#include "pqueue.h"
#include "tracing.h"
#include "routing.h"

/** @note holds all entity data as a single parsable string */
char map_entitystring[MAX_MAP_ENTSTRING];

/** @note holds the smallest bounding box that will contain the map
 *  @sa CL_ClampCamToMap (cl_input.c)
 *  @sa CL_OutsideMap (cl_le.c)
 *  @sa CMod_GetMapSize (cmodel.c)
 *  @sa SV_ClearWorld (sv_world.c)
 */
vec3_t map_min, map_max;

/** @brief server and client routing table */
routing_t svMap, clMap;

/** @note holds the number of inline entities, e.g. ET_DOOR */
static int numInline;

/** @note The old value for the normal step up (will become obselete) */
byte sh_low;

/** @note The old value for the STEPON flagged step up (will become obselete) */
byte sh_big;

/** @note a list with all inline models (like func_breakable) */
static const char **inlineList;

/** @note (???) a pointer to the part of the lump that defines inline model data */
static byte *cmod_base;

/** @note this is the position of the current actor- so the actor can stand in the cell it is in when pathfinding */
static pos3_t exclude_from_forbiddenlist;

/** @note this is a zeroed surface structure */
static cBspSurface_t nullsurface;

static void CM_MakeTracingNodes(void);
static void CM_InitBoxHull(void);

/*
===============================================================================
MAP LOADING
===============================================================================
*/


/**
 * @brief Loads brush entities like func_door and func_breakable
 * @param[in] l The lump to load the data from
 * @param[in] shift The shifting vector in case this is a map assemble
 * @sa CM_AddMapTile
 * @sa R_ModLoadSubmodels
 * @sa CM_InlineModel
 */
static void CMod_LoadSubmodels (lump_t * l, vec3_t shift)
{
	dBspModel_t *in;
	cBspModel_t *out;
	int i, j, count;

	if (!l)
		Com_Error(ERR_DROP, "CMod_LoadSubmodels: No lump given");

	in = (void *) (cmod_base + l->fileofs);
	if (l->filelen % sizeof(dBspModel_t))
		Com_Error(ERR_DROP, "CMod_LoadSubmodels: funny lump size (%i => "UFO_SIZE_T")", l->filelen, sizeof(dBspModel_t));
	count = l->filelen / sizeof(dBspModel_t);
	Com_DPrintf(DEBUG_ENGINE, "%c...submodels: %i\n", COLORED_GREEN, count);

	if (count < 1)
		Com_Error(ERR_DROP, "Map with no models");
	if (count > MAX_MAP_MODELS)
		Com_Error(ERR_DROP, "Map has too many models: %i", count);

	out = Mem_PoolAlloc((count + 6) * sizeof(*out), com_cmodelSysPool, 0);
	curTile->models = out;
	curTile->nummodels = count;

	for (i = 0; i < count; i++, in++, out++) {
		out = &curTile->models[i];

		/* spread the mins / maxs by a pixel */
		for (j = 0; j < 3; j++) {
			out->mins[j] = LittleFloat(in->mins[j]) - 1 + shift[j];
			out->maxs[j] = LittleFloat(in->maxs[j]) + 1 + shift[j];
		}
		out->headnode = LittleLong(in->headnode);
		out->tile = numTiles; /* backlink to the loaded map tile */
	}
}


/**
 * @param[in] shift The shifting vector in case this is a map assemble
 * @sa CM_AddMapTile
 */
static void CMod_LoadSurfaces (lump_t * l, vec3_t shift)
{
	dBspTexinfo_t *in;
	cBspSurface_t *out;
	int i, count;

	if (!l)
		Com_Error(ERR_DROP, "CMod_LoadSurfaces: No lump given");

	in = (void *) (cmod_base + l->fileofs);
	if (l->filelen % sizeof(dBspTexinfo_t))
		Com_Error(ERR_DROP, "CMod_LoadSurfaces: funny lump size: %i", l->filelen);
	count = l->filelen / sizeof(dBspTexinfo_t);
	Com_DPrintf(DEBUG_ENGINE, "%c...surfaces: %i\n", COLORED_GREEN, count);

	if (count < 1)
		Com_Error(ERR_DROP, "Map with no surfaces");
	if (count > MAX_MAP_TEXINFO)
		Com_Error(ERR_DROP, "Map has too many surfaces");

	out = Mem_PoolAlloc(count * sizeof(*out), com_cmodelSysPool, 0);

	curTile->surfaces = out;
	curTile->numtexinfo = count;

	for (i = 0; i < count; i++, in++, out++) {
		Q_strncpyz(out->name, in->texture, sizeof(out->name));
		out->flags = LittleLong(in->surfaceFlags);
		out->value = LittleLong(in->value);
	}
}


/**
 * @param[in] shift The shifting vector in case this is a map assemble
 * @sa CM_AddMapTile
 */
static void CMod_LoadNodes (lump_t * l, vec3_t shift)
{
	dBspNode_t *in;
	int child;
	cBspNode_t *out;
	int i, j, count;

	if (!l)
		Com_Error(ERR_DROP, "CMod_LoadNodes: No lump given");

	in = (void *) (cmod_base + l->fileofs);
	if (l->filelen % sizeof(dBspNode_t))
		Com_Error(ERR_DROP, "CMod_LoadNodes: funny lump size: %i", l->filelen);
	count = l->filelen / sizeof(dBspNode_t);
	Com_DPrintf(DEBUG_ENGINE, "%c...nodes: %i\n", COLORED_GREEN, count);

	if (count < 1)
		Com_Error(ERR_DROP, "Map has no nodes");
	if (count > MAX_MAP_NODES)
		Com_Error(ERR_DROP, "Map has too many nodes");

	/* add some for the box */
	out = Mem_PoolAlloc((count + 6) * sizeof(*out), com_cmodelSysPool, 0);

	curTile->numnodes = count;
	curTile->nodes = out;

	for (i = 0; i < count; i++, out++, in++) {
		if (LittleLong(in->planenum) == PLANENUM_LEAF)
			out->plane = NULL;
		else
			out->plane = curTile->planes + LittleLong(in->planenum);

		/* in case this is a map assemble */
		for (j = 0; j < 3; j++) {
			out->mins[j] = LittleShort(in->mins[j]) + shift[j];
			out->maxs[j] = LittleShort(in->maxs[j]) + shift[j];
		}

		for (j = 0; j < 2; j++) {
			child = LittleLong(in->children[j]);
			out->children[j] = child;
		}
	}
}

/**
 * @param[in] shift The shifting vector in case this is a map assemble
 * @sa CM_AddMapTile
 */
static void CMod_LoadBrushes (lump_t * l, vec3_t shift)
{
	dBspBrush_t *in;
	cBspBrush_t *out;
	int i, count;

	if (!l)
		Com_Error(ERR_DROP, "CMod_LoadBrushes: No lump given");

	in = (void *) (cmod_base + l->fileofs);
	if (l->filelen % sizeof(dBspBrush_t))
		Com_Error(ERR_DROP, "CMod_LoadBrushes: funny lump size: %i", l->filelen);
	count = l->filelen / sizeof(dBspBrush_t);
	Com_DPrintf(DEBUG_ENGINE, "%c...brushes: %i\n", COLORED_GREEN, count);

	if (count > MAX_MAP_BRUSHES)
		Com_Error(ERR_DROP, "Map has too many brushes");

	/* add some for the box */
	out = Mem_PoolAlloc((count + 1) * sizeof(*out), com_cmodelSysPool, 0);

	curTile->numbrushes = count;
	curTile->brushes = out;

	for (i = 0; i < count; i++, out++, in++) {
		out->firstbrushside = LittleLong(in->firstbrushside);
		out->numsides = LittleLong(in->numsides);
		out->contentFlags = LittleLong(in->contentFlags);
	}
}

/**
 * @param[in] shift The shifting vector in case this is a map assemble
 * @sa CM_AddMapTile
 */
static void CMod_LoadLeafs (lump_t * l, vec3_t shift)
{
	int i;
	cBspLeaf_t *out;
	dBspLeaf_t *in;
	int count;

	if (!l)
		Com_Error(ERR_DROP, "CMod_LoadLeafs: No lump given");

	in = (void *) (cmod_base + l->fileofs);
	if (l->filelen % sizeof(dBspLeaf_t))
		Com_Error(ERR_DROP, "CMod_LoadLeafs: funny lump size: %i", l->filelen);
	count = l->filelen / sizeof(dBspLeaf_t);
	Com_DPrintf(DEBUG_ENGINE, "%c...leafs: %i\n", COLORED_GREEN, count);

	if (count < 1)
		Com_Error(ERR_DROP, "Map with no leafs");
	/* need to save space for box planes */
	if (count > MAX_MAP_PLANES)
		Com_Error(ERR_DROP, "Map has too many planes");

	/* add some for the box */
	out = Mem_PoolAlloc((count + 1) * sizeof(*out), com_cmodelSysPool, 0);

	curTile->numleafs = count;
	curTile->leafs = out;

	for (i = 0; i < count; i++, in++, out++) {
		out->contentFlags = LittleLong(in->contentFlags);
		out->firstleafbrush = LittleShort(in->firstleafbrush);
		out->numleafbrushes = LittleShort(in->numleafbrushes);
	}

	if (curTile->leafs[0].contentFlags != CONTENTS_SOLID)
		Com_Error(ERR_DROP, "Map leaf 0 is not CONTENTS_SOLID");
	curTile->emptyleaf = -1;
	for (i = 1; i < curTile->numleafs; i++) {
		if (!curTile->leafs[i].contentFlags) {
			curTile->emptyleaf = i;
			break;
		}
	}
	if (curTile->emptyleaf == -1)
		Com_Error(ERR_DROP, "Map does not have an empty leaf");
}

/**
 * @param[in] shift The shifting vector in case this is a map assemble
 * @sa CM_AddMapTile
 * @sa R_ModLoadPlanes
 */
static void CMod_LoadPlanes (lump_t * l, vec3_t shift)
{
	int i, j;
	cBspPlane_t *out;
	dBspPlane_t *in;
	int count;
	int bits;

	if (!l)
		Com_Error(ERR_DROP, "CMod_LoadPlanes: No lump given");

	in = (void *) (cmod_base + l->fileofs);
	if (l->filelen % sizeof(dBspPlane_t))
		Com_Error(ERR_DROP, "CMod_LoadPlanes: funny lump size: %i", l->filelen);
	count = l->filelen / sizeof(dBspPlane_t);
	Com_DPrintf(DEBUG_ENGINE, "%c...planes: %i\n", COLORED_GREEN, count);

	if (count < 1)
		Com_Error(ERR_DROP, "Map with no planes");
	/* need to save space for box planes */
	if (count > MAX_MAP_PLANES)
		Com_Error(ERR_DROP, "Map has too many planes");

	/* add some for the box */
	out = Mem_PoolAlloc((count + 12) * sizeof(*out), com_cmodelSysPool, 0);

	curTile->numplanes = count;
	curTile->planes = out;

	for (i = 0; i < count; i++, in++, out++) {
		bits = 0;
		for (j = 0; j < 3; j++) {
			out->normal[j] = LittleFloat(in->normal[j]);
			if (out->normal[j] < 0)
				bits |= 1 << j;
		}

		out->dist = LittleFloat(in->dist);
		out->type = LittleLong(in->type);
		out->signbits = bits;

		/* shift (map assembly) */
		for (j = 0; j < 3; j++)
			out->dist += out->normal[j] * shift[j];
	}
}

/**
 * @param[in] shift The shifting vector in case this is a map assemble
 * @sa CM_AddMapTile
 */
static void CMod_LoadLeafBrushes (lump_t * l, vec3_t shift)
{
	int i;
	unsigned short *out;
	unsigned short *in;
	int count;

	if (!l)
		Com_Error(ERR_DROP, "CMod_LoadLeafBrushes: No lump given");

	in = (void *) (cmod_base + l->fileofs);
	if (l->filelen % sizeof(unsigned short))
		Com_Error(ERR_DROP, "CMod_LoadLeafBrushes: funny lump size: %i", l->filelen);
	count = l->filelen / sizeof(unsigned short);
	Com_DPrintf(DEBUG_ENGINE, "%c...leafbrushes: %i\n", COLORED_GREEN, count);

	/* add some for the box */
	out = Mem_PoolAlloc((count + 1) * sizeof(*out), com_cmodelSysPool, 0);

	if (count < 1)
		Com_Error(ERR_DROP, "Map with no planes");
	/* need to save space for box planes */
	if (count >= MAX_MAP_LEAFBRUSHES)
		Com_Error(ERR_DROP, "Map has too many leafbrushes");

	curTile->numleafbrushes = count;
	curTile->leafbrushes = out;

	for (i = 0; i < count; i++, in++, out++)
		*out = LittleShort(*in);
}

/**
 * @param[in] shift The shifting vector in case this is a map assemble
 * @sa CM_AddMapTile
 */
static void CMod_LoadBrushSides (lump_t * l, vec3_t shift)
{
	int i, j;
	cBspBrushSide_t *out;
	dBspBrushSide_t *in;
	int count;
	int num;

	if (!l)
		Com_Error(ERR_DROP, "CMod_LoadBrushSides: No lump given");

	in = (void *) (cmod_base + l->fileofs);
	if (l->filelen % sizeof(dBspBrushSide_t))
		Com_Error(ERR_DROP, "CMod_LoadBrushSides: funny lump size: %i", l->filelen);
	count = l->filelen / sizeof(dBspBrushSide_t);
	Com_DPrintf(DEBUG_ENGINE, "%c...brushsides: %i\n", COLORED_GREEN, count);

	/* need to save space for box planes */
	if (count > MAX_MAP_BRUSHSIDES)
		Com_Error(ERR_DROP, "Map has too many brushsides");

	/* add some for the box */
	out = Mem_PoolAlloc((count + 6) * sizeof(*out), com_cmodelSysPool, 0);

	curTile->numbrushsides = count;
	curTile->brushsides = out;

	for (i = 0; i < count; i++, in++, out++) {
		num = LittleShort(in->planenum);
		out->plane = &curTile->planes[num];
		j = LittleShort(in->texinfo);
		if (j >= curTile->numtexinfo)
			Com_Error(ERR_DROP, "Bad brushside texinfo");
		out->surface = &curTile->surfaces[j];
	}
}

/**
 * @param[in] source Source will be set to the end of the compressed data block!
 * @sa CompressRouting (ufo2map)
 * @sa CMod_LoadRouting
 */
static int CMod_DeCompressRouting (byte ** source, byte * dataStart)
{
	int i, c;
	byte *data_p;
	byte *src;

	data_p = dataStart;
	src = *source;

	while (*src) {
		if (*src & 0x80) {
			/* repetitions */
			c = *src++ & ~0x80;
			for (i = 0; i < c + 1; i++)
				*data_p++ = *src;
			src++;
		} else {
			/* identities */
			c = *src++;
			for (i = 0; i < c; i++)
				*data_p++ = *src++;
		}
	}

	src++;
	*source = src;

	return data_p - dataStart;
}

/**
 * @brief Checks for valid BSP-file
 *
 * @param[in] filename BSP-file to check
 *
 * @return 0 if valid
 * @return 1 could not open file
 * @return 2 if magic number is bad
 * @return 3 if version of bsp-file is bad
 */
int CheckBSPFile (const char *filename)
{
	int i;
	int header[2];
	qFILE file;
	char name[MAX_QPATH];

	/* load the file */
	Com_sprintf(name, MAX_QPATH, "maps/%s.bsp", filename);

	FS_FOpenFile(name, &file);
	if (!file.f && !file.z)
		return 1;

	FS_Read(header, sizeof(header), &file);

	FS_FCloseFile(&file);

	for (i = 0; i < 2; i++)
		header[i] = LittleLong(header[i]);

	if (header[0] != IDBSPHEADER)
		return 2;
	if (header[1] != BSPVERSION)
		return 3;

	/* valid BSP-File */
	return 0;
}

/*
===============================================================================
TRACING NODES
===============================================================================
*/

/**
 * @brief Use the bsp node structure to reconstruct efficient tracing structures
 * that are used for fast visibility and pathfinding checks
 * @note curTile->tnodes is expected to have enough memory malloc'ed for the function to work.
 * @sa BuildTracingNode_r
 */
static void CM_MakeTracingNodes (void)
{
	int i;

	curTile->tnodes = Mem_PoolAlloc((curTile->numnodes + 6) * sizeof(tnode_t), com_cmodelSysPool, 0);
	tnode_p = curTile->tnodes;

	curTile->numtheads = 0;
	curTile->numcheads = 0;

	for (i = 0; i < curTile->nummodels; i++) {
		if (curTile->models[i].headnode == -1 || curTile->models[i].headnode >= curTile->numnodes + 6)
			continue;

		curTile->thead[curTile->numtheads] = tnode_p - curTile->tnodes;
		curTile->numtheads++;
		assert(curTile->numtheads < LEVEL_MAX);

		/* If this level (i) is the last visible level or earlier, then trace it.
		 * Otherwise don't; we have separate checks for entities. */
		if (i < NUM_REGULAR_MODELS)
			TR_BuildTracingNode_r(curTile->models[i].headnode, i);
	}
}


/*
===============================================================================
GAME RELATED TRACING USING ENTITIES
===============================================================================
*/


/**
 * @brief Checks traces against the world and all inline models
 * @param[in] start The position to start the trace.
 * @param[in] stop The position where the trace ends.
 * @sa TR_TestLine
 * @sa CM_InlineModel
 * @sa TR_TransformedBoxTrace
 * @return qtrue - hit something
 * @return qfalse - hit nothing
 */
qboolean CM_EntTestLine (vec3_t start, vec3_t stop, const int levelmask)
{
	trace_t trace;
	cBspModel_t *model;
	const char **name;

	/* trace against world first */
	if (TR_TestLine(start, stop, levelmask))
		/* We hit the world, so we didn't make it anyway... */
		return qtrue;

	/* no local models, so we made it. */
	if (!inlineList)
		return qfalse;

	for (name = inlineList; *name; name++) {
		/* check whether this is really an inline model */
		assert(**name == '*');
		model = CM_InlineModel(*name);
		assert(model);
		if (model->headnode >= mapTiles[model->tile].numnodes + 6)
			continue;
		trace = CM_TransformedBoxTrace(start, stop, vec3_origin, vec3_origin, model->tile, model->headnode, MASK_ALL, model->origin, model->angles);
		/* if we started the trace in a wall */
		/* or the trace is not finished */
		if (trace.startsolid || trace.fraction < 1.0)
			return qtrue;
	}

	/* not blocked */
	return qfalse;
}


/**
 * @brief Checks traces against the world and all inline models
 * @param[in] start The position to start the trace.
 * @param[in] stop The position where the trace ends.
 * @param[in] list of entities that might be on this line
 * @sa CM_EntTestLine
 * @return qtrue - hit something
 * @return qfalse - hit nothing
 */
qboolean CM_TestLineWithEnt (vec3_t start, vec3_t stop, const int levelmask, const char **entlist)
{
	qboolean hit;

	/* set the list of entities to check */
	inlineList = entlist;
	/* do the line test */
	hit = CM_EntTestLine(start, stop, levelmask);
	/* zero the list */
	inlineList = NULL;

	return hit;
}


/**
 * @brief Checks traces against the world and all inline models, gives the hit position back
 * @param[in] start The position to start the trace.
 * @param[in] stop The position where the trace ends.
 * @param[out] end The position where the line hits a object or the stop position if nothing is in the line
 * @sa TR_TestLineDM
 * @sa TR_TransformedBoxTrace
 */
qboolean CM_EntTestLineDM (vec3_t start, vec3_t stop, vec3_t end, const int levelmask)
{
	trace_t trace;
	cBspModel_t *model;
	const char **name;
	int blocked;

	/* trace against world first */
	blocked = TR_TestLineDM(start, stop, end, levelmask);
	if (!inlineList)
		return blocked;

	for (name = inlineList; *name; name++) {
		/* check whether this is really an inline model */
		assert(**name == '*');
		model = CM_InlineModel(*name);
		assert(model);
		if (model->headnode >= mapTiles[model->tile].numnodes + 6)
			continue;
		trace = CM_TransformedBoxTrace(start, end, vec3_origin, vec3_origin, model->tile, model->headnode, MASK_ALL, model->origin, model->angles);
		/* if we started the trace in a wall */
		if (trace.startsolid) {
			VectorCopy(start, end);
			return qtrue;
		}
		/* trace not finishd */
		if (trace.fraction < 1.0) {
			blocked = qtrue;
			VectorCopy(trace.endpos, end);
		}
	}

	/* return result */
	return blocked;
}


/**
 * @brief Wrapper for TR_TransformedBoxTrace that accepts a tile number,
 * @sa TR_TransformedBoxTrace
 */
inline trace_t CM_TransformedBoxTrace (vec3_t start, vec3_t end, const vec3_t mins, const vec3_t maxs, int tile, int headnode, int brushmask, const vec3_t origin, const vec3_t angles)
{
	return TR_TransformedBoxTrace(start, end, mins, maxs, &mapTiles[tile], headnode, brushmask, origin, angles);
}


/*
===============================================================================
GAME RELATED TRACING
===============================================================================
*/


/**
 * @brief Calculate the map size via routing data and store grid size
 * in map_min and map_max. This is done with every new map load
 * @sa map_min
 * @sa map_max
 * @sa CMod_LoadRouting
 */
static void CMod_GetMapSize (routing_t * map)
{
	const vec3_t offset = {100, 100, 100};
	pos3_t min, max;
	int x, y;

	assert(map);

	VectorSet(min, PATHFINDING_WIDTH - 1, PATHFINDING_WIDTH - 1, 0);
	VectorSet(max, 0, 0, 0);

	/* get border */
	for (y = 0; y < PATHFINDING_WIDTH; y++)
		for (x = 0; x < PATHFINDING_WIDTH; x++)
			/* ROUTING_NOT_REACHABLE means that we can't walk there */
			if (map->fall[y][x] != ROUTING_NOT_REACHABLE) {
				if (x < min[0])
					min[0] = x;
				if (y < min[1])
					min[1] = y;
				if (x > max[0])
					max[0] = x;
				if (y > max[1])
					max[1] = y;
			}

	/* convert to vectors */
	PosToVec(min, map_min);
	PosToVec(max, map_max);

	/* tiny offset */
	VectorAdd(map_min, offset, map_min);
	VectorSubtract(map_max, offset, map_max);
}


/**
 * @param[in] l Routing lump ... (routing data lump from bsp file)
 * @param[in] sX The x position on the world plane (grid position) - values from -(PATHFINDING_WIDTH/2) up to PATHFINDING_WIDTH/2 are allowed
 * @param[in] sY The y position on the world plane (grid position) - values from -(PATHFINDING_WIDTH/2) up to PATHFINDING_WIDTH/2 are allowed
 * @param[in] sZ The height level on the world plane (grid position) - values from 0 - PATHFINDING_HEIGHT are allowed
 * @sa CM_AddMapTile
 * @todo Fix z-level routing
 */
static void CMod_LoadRouting (lump_t * l, int sX, int sY, int sZ)
{
	static byte temp_route[PATHFINDING_HEIGHT][PATHFINDING_WIDTH][PATHFINDING_WIDTH];
	static byte temp_fall[PATHFINDING_WIDTH][PATHFINDING_WIDTH];
	static byte temp_step[PATHFINDING_WIDTH][PATHFINDING_WIDTH];
	static byte route_again[PATHFINDING_WIDTH][PATHFINDING_WIDTH];
	byte *source;
	int length;
	int x, y;
	pos_t z;
	int maxX, maxY;
	int ax, ay;
	unsigned int i;
	qboolean overwrite;
#ifdef DEBUG
	/* count all reachable fields of the map for debugging */
	unsigned long route_count = 0;
#endif

	inlineList = NULL;

	if (!l)
		Com_Error(ERR_DROP, "CMod_LoadRouting: No lump given");

	if (!l->filelen)
		Com_Error(ERR_DROP, "CMod_LoadRouting: Map has NO routing lump");

	assert((sX > -(PATHFINDING_WIDTH / 2)) && (sX < (PATHFINDING_WIDTH / 2)));
	assert((sY > -(PATHFINDING_WIDTH / 2)) && (sY < (PATHFINDING_WIDTH / 2)));
	assert((sZ >= 0) && (sZ < PATHFINDING_HEIGHT));

	/* routing must be redone for overlapping tiles and borders */
	memset(&(route_again[0][0]), 0, PATHFINDING_WIDTH * PATHFINDING_WIDTH);

	source = cmod_base + l->fileofs;
	sh_low = *source++;
	sh_big = *source++;
	length = CMod_DeCompressRouting(&source, &temp_route[0][0][0]);
	length += CMod_DeCompressRouting(&source, &temp_fall[0][0]);
	length += CMod_DeCompressRouting(&source, &temp_step[0][0]);

	/* 10 => HEIGHT (8), level 257 (1), level 258 (1) */
	if (length != PATHFINDING_WIDTH * PATHFINDING_WIDTH * 10)
		Com_Error(ERR_DROP, "CMod_LoadRouting: Map has BAD routing lump");

	/* shift and merge the routing information
	 * in case of map assemble (random maps) */
	maxX = sX > 0 ? PATHFINDING_WIDTH - sX : PATHFINDING_WIDTH;
	maxY = sY > 0 ? PATHFINDING_WIDTH - sY : PATHFINDING_WIDTH;
	/* no z movement */
	sZ = 0;

	/* find borders and overlapping parts for reroute */
	for (y = sY < 0 ? -sY : 0; y < maxY; y++)
		for (x = sX < 0 ? -sX : 0; x < maxX; x++)
			if (temp_fall[y][x] != ROUTING_NOT_REACHABLE) {
				if (clMap.fall[y + sY][x + sX] != ROUTING_NOT_REACHABLE) {
					/* reroute all directions for overlapping locations*/
					route_again[y][x] = 0xf0;
				} else {
					/* check borders */
					for (i = 0; i < BASE_DIRECTIONS; i++) {
						ax = x + dvecs[i][0];
						ay = y + dvecs[i][1];

						/* reroute if a border is found */
						if (ax < 0 || ax >= PATHFINDING_WIDTH || ay < 0 || ay >= PATHFINDING_WIDTH ||
							temp_fall[ay][ax] == ROUTING_NOT_REACHABLE)
								route_again[y][x] |= ((0x10 << i) & UCHAR_MAX);
					}
				}
#ifdef DEBUG
				route_count++;
#endif
			}

#ifdef DEBUG
	if (!route_count)
		Com_Error(ERR_DROP, "CMod_LoadRouting: Map '%s' has NO reachable field", curTile->name);
#endif

	/* copy or combine the routing information */
	for (y = sY < 0 ? -sY : 0; y < maxY; y++)
		for (x = sX < 0 ? -sX : 0; x < maxX; x++)
			if (temp_fall[y][x] != ROUTING_NOT_REACHABLE) {
				overwrite = (clMap.fall[y + sY][x + sX] == ROUTING_NOT_REACHABLE);

				/* add or combine new quant */
				if (overwrite) {
					clMap.fall[y + sY][x + sX] = temp_fall[y][x];
					clMap.step[y + sY][x + sX] = temp_step[y][x];
				} else {
					clMap.fall[y + sY][x + sX] &= temp_fall[y][x];
					clMap.step[y + sY][x + sX] |= temp_step[y][x];
				}

				/* copy or combine routing info */
				for (z = 0; z < PATHFINDING_HEIGHT; z++) {
					if (overwrite) {
						clMap.route[z][y + sY][x + sX] = temp_route[z][y][x];
					} else if ((clMap.route[z][y + sY][x + sX] & 0x0f) == (temp_route[z][y][x] & 0x0f)) {
						/* Combine the routing info if the height is equal */
						clMap.route[z][y + sY][x + sX] |= temp_route[z][y][x];
					} else if ((clMap.route[z][y + sY][x + sX] & 0x0f) < (temp_route[z][y][x] & 0x0f)) {
						/* or use the heigher one */
						clMap.route[z][y + sY][x + sX] = temp_route[z][y][x];
					}
				}
			}

	/* reroute borders and overlapping */
	for (y = sY < 0 ? -sY : 0; y < maxY; y++)
		for (x = sX < 0 ? -sX : 0; x < maxX; x++)
			if (route_again[y][x])
				for (i = 0; i < BASE_DIRECTIONS; i++) {
					if (route_again[y][x] & (0x10 << i)) {
						ax = x + dvecs[i][0];
						ay = y + dvecs[i][1];

						/* reroute */
						for (z = 0; z < PATHFINDING_HEIGHT; z++) {
							RT_UpdateConnection(&clMap, x + sX, y + sY, z, qfalse);
							RT_UpdateConnection(&clMap, ax + sX, ay + sY, z, qfalse);
						}
					}
				}

	/* calculate new border */
	CMod_GetMapSize(&clMap);

/*	Com_Printf("route: (%i %i) fall: %i step: %i\n", */
/*		(int)map->route[0][0][0], (int)map->route[1][0][0], (int)map->fall[0][0], (int)map->step[0][0]); */
}


/**
 * @note Transforms coordinates and stuff for assembled maps
 * @param[in] shift The shifting vector in case this is a map assemble
 * @sa CM_AddMapTile
 */
static void CMod_LoadEntityString (lump_t * l, vec3_t shift)
{
	const char *com_token;
	const char *es;
	char keyname[256];
	vec3_t v;
	int num;

	if (!l)
		Com_Error(ERR_DROP, "CMod_LoadEntityString: No lump given");

	if (!l->filelen)
		Com_Error(ERR_DROP, "CMod_LoadEntityString: Map has NO entity lump");

	if (l->filelen + 1 > MAX_MAP_ENTSTRING)
		Com_Error(ERR_DROP, "CMod_LoadEntityString: Map has too large entity lump");

	/* merge entitystring information */
	es = (char *) (cmod_base + l->fileofs);
	while (1) {
		/* parse the opening brace */
		com_token = COM_Parse(&es);
		if (!es)
			break;
		if (com_token[0] != '{')
			Com_Error(ERR_DROP, "CMod_LoadEntityString: found %s when expecting {", com_token);

		/* new entity */
		Q_strcat(map_entitystring, "{ ", MAX_MAP_ENTSTRING);

		/* go through all the dictionary pairs */
		while (1) {
			/* parse key */
			com_token = COM_Parse(&es);
			if (com_token[0] == '}')
				break;
			if (!es)
				Com_Error(ERR_DROP, "CMod_LoadEntityString: EOF without closing brace");

			Q_strncpyz(keyname, com_token, sizeof(keyname));

			/* parse value */
			com_token = COM_Parse(&es);
			if (!es)
				Com_Error(ERR_DROP, "CMod_LoadEntityString: EOF without closing brace");

			if (com_token[0] == '}')
				Com_Error(ERR_DROP, "CMod_LoadEntityString: closing brace without data");

			/* alter value, if needed */
			if (!Q_strncmp(keyname, "origin", sizeof(keyname))) {
				/* origins are shifted */
				sscanf(com_token, "%f %f %f", &(v[0]), &(v[1]), &(v[2]));
				VectorAdd(v, shift, v);
				Q_strcat(map_entitystring, va("%s \"%i %i %i\" ", keyname, (int) v[0], (int) v[1], (int) v[2]), MAX_MAP_ENTSTRING);
			} else if (!Q_strncmp(keyname, "model", sizeof(keyname)) && com_token[0] == '*') {
				/* adapt inline model number */
				num = atoi(com_token + 1);
				num += numInline;
				Q_strcat(map_entitystring, va("%s *%i ", keyname, num), MAX_MAP_ENTSTRING);
			} else {
				/* just store key and value */
				Q_strcat(map_entitystring, va("%s \"%s\" ", keyname, com_token), MAX_MAP_ENTSTRING);
			}
		}

		/* finish entity */
		Q_strcat(map_entitystring, "} ", MAX_MAP_ENTSTRING);
	}
}


/**
 * @brief Adds in a single map tile
 * @param[in] name The (file-)name of the tile to add.
 * @param[in] sX The x position on the world plane (grid position) - values from -(PATHFINDING_WIDTH / 2) up to PATHFINDING_WIDTH / 2 are allowed
 * @param[in] sY The y position on the world plane (grid position) - values from -(PATHFINDING_WIDTH / 2) up to PATHFINDING_WIDTH / 2 are allowed
 * @param[in] sZ The height level on the world plane (grid position) - values from 0 up to PATHFINDING_HEIGHT are allowed
 * @note The sX and sY values are grid positions - max. grid size is 256 - unit size is
 * 32 => ends up at 8192 (the worldplace size - or [-4096, 4096])
 * @return The checksum of the maptile
 * @return 0 on error
 * @sa CM_LoadMap
 * @sa R_ModAddMapTile
 */
static unsigned CM_AddMapTile (const char *name, int sX, int sY, byte sZ)
{
	char filename[MAX_QPATH];
	unsigned checksum;
	unsigned *buf;
	unsigned int i;
	int length;
	dBspHeader_t header;
	/* use for random map assembly for shifting origins and so on */
	vec3_t shift = {0.0f, 0.0f, 0.0f};

	Com_DPrintf(DEBUG_ENGINE, "CM_AddMapTile: %s at %i,%i,%i\n", name, sX, sY, sZ);
	assert(name);
	assert(name[0]);
	assert((sX > -(PATHFINDING_WIDTH / 2)) && (sX < (PATHFINDING_WIDTH / 2)));
	assert((sY > -(PATHFINDING_WIDTH / 2)) && (sY < (PATHFINDING_WIDTH / 2)));
	assert(sZ < PATHFINDING_HEIGHT);

	/* load the file */
	Com_sprintf(filename, MAX_QPATH, "maps/%s.bsp", name);
	length = FS_LoadFile(filename, (byte **) &buf);
	if (!buf)
		Com_Error(ERR_DROP, "Couldn't load %s", filename);

	checksum = LittleLong(Com_BlockChecksum(buf, length));

	header = *(dBspHeader_t *) buf;
	for (i = 0; i < sizeof(dBspHeader_t) / 4; i++)
		((int *) &header)[i] = LittleLong(((int *) &header)[i]);

	if (header.version != BSPVERSION)
		Com_Error(ERR_DROP, "CM_AddMapTile: %s has wrong version number (%i should be %i)", name, header.version, BSPVERSION);

	cmod_base = (byte *) buf;

	/* init */
	if (numTiles >= MAX_MAPTILES)
		Com_Error(ERR_FATAL, "CM_AddMapTile: too many tiles loaded %i", numTiles);

	curTile = &mapTiles[numTiles];
	memset(curTile, 0, sizeof(mapTile_t));
	Q_strncpyz(curTile->name, name, sizeof(curTile->name));

	/* pathfinding and the like must be shifted on the worldplane when we
	 * are assembling a map */
	VectorSet(shift, sX * UNIT_SIZE, sY * UNIT_SIZE, sZ * UNIT_SIZE);

	/* load into heap */
	CMod_LoadSurfaces(&header.lumps[LUMP_TEXINFO], shift);
	CMod_LoadLeafs(&header.lumps[LUMP_LEAFS], shift);
	CMod_LoadLeafBrushes(&header.lumps[LUMP_LEAFBRUSHES], shift);
	CMod_LoadPlanes(&header.lumps[LUMP_PLANES], shift);
	CMod_LoadBrushes(&header.lumps[LUMP_BRUSHES], shift);
	CMod_LoadBrushSides(&header.lumps[LUMP_BRUSHSIDES], shift);
	CMod_LoadSubmodels(&header.lumps[LUMP_MODELS], shift);
	CMod_LoadNodes(&header.lumps[LUMP_NODES], shift);
	CMod_LoadEntityString(&header.lumps[LUMP_ENTITIES], shift);

	CM_InitBoxHull();
	CM_MakeTracingNodes();

	/* Lets test if curTile is changed */
	assert(curTile == &mapTiles[numTiles]);

	/* CMod_LoadRouting plays with curTile and numTiles, so lets set */
	/* these to the right values now */
	numInline += curTile->nummodels - NUM_REGULAR_MODELS;

	/* now increase the amount of loaded tiles */
	numTiles++;

	CMod_LoadRouting(&header.lumps[LUMP_ROUTING], sX, sY, sZ);
	memcpy(&svMap, &clMap, sizeof(routing_t));

	FS_FreeFile(buf);

	return checksum;
}


/**
 * @brief Loads in the map and all submodels
 * @sa CM_AddMapTile
 * @note Make sure that mapchecksum was set to 0 before you call this function
 */
void CM_LoadMap (const char *tiles, const char *pos, unsigned *mapchecksum)
{
	const char *token;
	char name[MAX_VAR];
	char base[MAX_QPATH];
	int sh[3];
	int i;

	Mem_FreePool(com_cmodelSysPool);

	assert(mapchecksum);
	assert(*mapchecksum == 0);

	/* init */
	c_traces = c_brush_traces = numInline = numTiles = 0;
	map_entitystring[0] = base[0] = 0;

	memset(&(clMap.fall[0][0]), ROUTING_NOT_REACHABLE, PATHFINDING_WIDTH * PATHFINDING_WIDTH);
	memset(&(clMap.step[0][0]), 0, PATHFINDING_WIDTH * PATHFINDING_WIDTH);
	memset(&(clMap.route[0][0][0]), 0, PATHFINDING_WIDTH * PATHFINDING_WIDTH * PATHFINDING_HEIGHT);

	if (pos && *pos)
		Com_Printf("CM_LoadMap: \"%s\" \"%s\"\n", tiles, pos);

	/* load tiles */
	while (tiles) {
		/* get tile name */
		token = COM_Parse(&tiles);
		if (!tiles)
			return;

		/* get base path */
		if (token[0] == '-') {
			Q_strncpyz(base, token + 1, sizeof(base));
			continue;
		}

		/* get tile name */
		Com_DPrintf(DEBUG_ENGINE, "CM_LoadMap: token: %s\n", token);
		if (token[0] == '+')
			Com_sprintf(name, sizeof(name), "%s%s", base, token + 1);
		else
			Q_strncpyz(name, token, sizeof(name));

		if (pos && pos[0]) {
			/* get position and add a tile */
			for (i = 0; i < 3; i++) {
				token = COM_Parse(&pos);
				if (!pos)
					Com_Error(ERR_DROP, "CM_LoadMap: invalid positions");
				sh[i] = atoi(token);
			}
			*mapchecksum += CM_AddMapTile(name, sh[0], sh[1], sh[2]);
		} else {
			/* load only a single tile, if no positions are specified */
			*mapchecksum = CM_AddMapTile(name, 0, 0, 0);
			return;
		}
	}

	Com_Error(ERR_DROP, "CM_LoadMap: invalid tile names");
}

/**
 * @brief Searches all inline models and return the cBspModel_t pointer for the
 * given modelnumber or -name
 * @param[in] name The modelnumber (e.g. "*2") or the modelname
 */
cBspModel_t *CM_InlineModel (const char *name)
{
	int i, num, models;

	if (!name || name[0] != '*')
		Com_Error(ERR_DROP, "CM_InlineModel: bad name: '%s'", name ? name : "");
	num = atoi(name + 1) - 1;
	if (num < 0 || num >= MAX_MODELS)
		Com_Error(ERR_DROP, "CM_InlineModel: bad number %i - max inline models are %i", num, MAX_MODELS);

	for (i = 0; i < numTiles; i++) {
		models = mapTiles[i].nummodels - NUM_REGULAR_MODELS;
		assert(models >= 0);

		if (num >= models)
			num -= models;
		else
			return &mapTiles[i].models[NUM_REGULAR_MODELS + num];
	}

	Com_Error(ERR_DROP, "CM_InlineModel: Error cannot find model '%s'\n", name);
	return NULL;
}

/**
 * @brief This function updates a model's orientation
 * @param[in] name The name of the model, must include the '*'
 * @param[in] origin The new origin for the model
 * @param[in] angles The new facing angles for the model
 * @note This is used whenever a model's orientation changes, eg doors and rotating models
 * @sa LE_DoorAction
 * @sa G_ClientUseDoor
 */
void CM_SetInlineModelOrientation (const char *name, const vec3_t origin, const vec3_t angles)
{
	cBspModel_t *model;
	model = CM_InlineModel(name);
	assert(model);
	VectorCopy(origin, model->origin);
	VectorCopy(angles, model->angles);
}

int CM_NumInlineModels (void)
{
	return numInline;
}

/**
 * @return The entitystring for all the loaded maps
 * @note Every map assembly will attach their entities here
 * @sa CM_LoadMap
 * @sa G_SpawnEntities
 * @sa SV_SpawnServer
 */
char *CM_EntityString (void)
{
	return map_entitystring;
}

/*
===============================================================================
BOX TRACING
===============================================================================
*/


/**
 * @brief Set up the planes and nodes so that the six floats of a bounding
 * box can just be stored out and get a proper clipping hull structure.
 */
static void CM_InitBoxHull (void)
{
	int i;
	int side;
	cBspNode_t *c;
	cBspPlane_t *p;
	cBspBrushSide_t *s;

	curTile->box_headnode = curTile->numnodes;
	curTile->box_planes = &curTile->planes[curTile->numplanes];
	/* sanity check if you only use one maptile => no map assembly */
	if (numTiles == 1 && (curTile->numnodes + 6 > MAX_MAP_NODES
		|| curTile->numbrushes + 1 > MAX_MAP_BRUSHES
		|| curTile->numleafbrushes + 1 > MAX_MAP_LEAFBRUSHES
		|| curTile->numbrushsides + 6 > MAX_MAP_BRUSHSIDES
		|| curTile->numplanes + 12 > MAX_MAP_PLANES))
		Com_Error(ERR_DROP, "Not enough room for box tree");

	curTile->box_brush = &curTile->brushes[curTile->numbrushes];
	curTile->box_brush->numsides = 6;
	curTile->box_brush->firstbrushside = curTile->numbrushsides;
	curTile->box_brush->contentFlags = CONTENTS_WEAPONCLIP;

	curTile->box_leaf = &curTile->leafs[curTile->numleafs];
	curTile->box_leaf->contentFlags = CONTENTS_WEAPONCLIP;
	curTile->box_leaf->firstleafbrush = curTile->numleafbrushes;
	curTile->box_leaf->numleafbrushes = 1;

	curTile->leafbrushes[curTile->numleafbrushes] = curTile->numbrushes;

	/* each side */
	for (i = 0; i < 6; i++) {
		side = i & 1;

		/* brush sides */
		s = &curTile->brushsides[curTile->numbrushsides + i];
		s->plane = curTile->planes + (curTile->numplanes + i * 2 + side);
		s->surface = &nullsurface;

		/* nodes */
		c = &curTile->nodes[curTile->box_headnode + i];
		c->plane = curTile->planes + (curTile->numplanes + i * 2);
		c->children[side] = -1 - curTile->emptyleaf;
		if (i != 5)
			c->children[side ^ 1] = curTile->box_headnode + i + 1;
		else
			c->children[side ^ 1] = -1 - curTile->numleafs;

		/* planes */
		p = &curTile->box_planes[i * 2];
		p->type = i >> 1;
		p->signbits = 0;
		VectorClear(p->normal);
		p->normal[i >> 1] = 1;

		p = &curTile->box_planes[i * 2 + 1];
		p->type = 3 + (i >> 1);
		p->signbits = 0;
		VectorClear(p->normal);
		p->normal[i >> 1] = -1;
	}
}


/**
 * @brief To keep everything totally uniform, bounding boxes are turned into small
 * BSP trees instead of being compared directly.
 */
int CM_HeadnodeForBox (int tile, const vec3_t mins, const vec3_t maxs)
{
	assert((tile < numTiles) && (tile >= 0));
	return TR_HeadnodeForBox(&mapTiles[tile], mins, maxs);
}


/*
==========================================================
  GRID ORIENTED MOVEMENT AND SCANNING
==========================================================
*/


/**
 * @brief  Dumps contents of the entire client map to console for inspection.
 * @sa CL_InitLocal
 */
void Grid_DumpWholeClientMap (void)
{
	Grid_DumpWholeMap(&clMap);
}


/**
 * @brief  Dumps contents of the entire client map to console for inspection.
 * @sa CL_InitLocal
 */
void Grid_DumpWholeServerMap (void)
{
	Grid_DumpWholeMap(&svMap);
}


/**
* @brief Checks one field (square) on the grid of the given routing data (i.e. the map).
 * @param[in] map Routing data/map.
 * @param[in] x Field in x direction
 * @param[in] y Field in y direction
 * @param[in] z Field in z direction
 * @sa Grid_MoveMark
 * @sa G_BuildForbiddenList
 * @sa CL_BuildForbiddenList
 * @return qtrue if one can't walk there (i.e. the field [and attached fields for e.g. 2x2 units] is/are blocked by entries in the forbidden list) otherwise false.
 */
static qboolean Grid_CheckForbidden (struct routing_s * map, int x, int y, byte z)
{
	pos_t **p;
	int i, size;
	byte *forbidden_size;

	for (i = 0, p = map->fblist; i < map->fblength / 2; i++, p += 2) {
		/* Skip initial position. */
		if (VectorCompare((*p), exclude_from_forbiddenlist)) {
			/* Com_DPrintf(DEBUG_ENGINE, "Grid_CheckForbidden: skipping %i|%i|%i\n", (*p)[0], (*p)[1], (*p)[2]); */
			continue;
		}

		forbidden_size = *(p + 1);
		memcpy(&size, forbidden_size, sizeof(int));
		switch (size) {
		case ACTOR_SIZE_NORMAL:
			if (x == (*p)[0] && y == (*p)[1] && z == (*p)[2])
				return qtrue;
			break;
		case ACTOR_SIZE_2x2:
			if (x == (*p)[0] && y == (*p)[1] && z == (*p)[2]) {
				return qtrue;
			} else if ((x == (*p)[0] + 1) && (y == (*p)[1]) && (z == (*p)[2])) {
				return qtrue;
			} else if ((x == (*p)[0]) && (y == (*p)[1] + 1) && (z == (*p)[2])) {
				return qtrue;
			} else if ((x == (*p)[0] + 1) && (y == (*p)[1] + 1) && (z == (*p)[2])) {
				return qtrue;
			}
			break;
		default:
			Com_Error(ERR_DROP, "Grid_CheckForbidden: unknown forbidden-size: %i", size);
		}
	}
	return qfalse;
}

/**
 * @brief All positions of a 2x2 unit (at the new location).
 * @note 2*int needed for range-check because pos3_t does not support negative values and has a smaller range.
 * There are 2 ints (x and y position) for all 4 squares a 2x2 unit blocks.
 * @sa poslist in Grid_MoveMark
 */
static int poslistNew[4][2];

/**
 * @param[in|out] map Pointer to client or server side routing table (clMap, svMap)
 * @param[in] pos Current location in the map.
 * @param[in] dir Direction vector index (see DIRECTIONS and dvecs)
 * @param[in] actor_size Give the field size of the actor (e.g. for 2x2 units) to check linked fields as well.
 * @param[in|out] pqueue Priority queue (heap) to insert the now reached tiles for reconsidering
 * @sa Grid_CheckForbidden
 * @todo Add height/fall checks for actor size (2x2).
 */
static void Grid_MoveMark (struct routing_s *map, pos3_t pos, int dir, int actor_size, PQUEUE *pqueue)
{
	byte x, y, z;
	int h;
	byte ol;
	int nx, ny;
	pos_t sh;
	int dx, dy;
	pos3_t dummy;
	pos_t l;
	pos3_t poslist[4];	/**< All positions of a 2x2 unit (at the original location). @sa poslistNew */

	x = pos[0];
	y = pos[1];
	z = pos[2];
	h = R_HEIGHT(map, x, y, z);
	ol = R_AREA(map, x, y, z);

	if (ol >= 250)
		return;

#ifdef PARANOID
	if (z >= PATHFINDING_HEIGHT) {
		Com_DPrintf(DEBUG_ENGINE, "Grid_MoveMark: WARNING z = %i(>= HEIGHT %i)\n", z, PATHFINDING_HEIGHT);
		return;
	}
	/*
	if ((l + TU_MOVE_STRAIGHT) >= MAX_MOVELENGTH)) {
		Com_DPrintf(DEBUG_ENGINE, "Grid_MoveMark: maximum movelength reached %i (max %i)\n", l, MAX_MOVELENGTH);
		return;
	}
	*/
#endif

	l = dir > 3	/**< Add TUs to old length/TUs depending on straight or diagonal move. */
		? ol + TU_MOVE_DIAGONAL
		: ol + TU_MOVE_STRAIGHT;

	dx = dvecs[dir][0];	/**< Get the difference value for x for this direction. (can be pos or neg) */
	dy = dvecs[dir][1];	/**< Get the difference value for y for this direction. (can be pos or neg) */
	nx = x + dx;		/**< "new" x value = starting x value + difference from choosen direction */
	ny = y + dy;		/**< "new" y value = starting y value + difference from choosen direction */


	/* Connection checks  (Guess: Abort if any of the 'straight' directions are not connected to the original position.) */
	/** @todo But why is it also checked for (dir > 3) directions? Might be a coding-trick; if so it's not really easy to read -> better way?. */
	/** @todo I think this and the next few lines need to be adapted to support 2x2 units. */
	switch (actor_size) {
	case ACTOR_SIZE_NORMAL:
		/* Range check of new values (1x1) */
		if (nx < 0 || nx >= PATHFINDING_WIDTH || ny < 0 || ny >= PATHFINDING_WIDTH)
			return;

		/* Connection checks for 1x1 actor. */
		/* We need to check the 4 surrounding "straight" fields (the 4 diagonal ones are skipped here) */
		if (dx > 0 && !R_CONN_PX(map, x, y, z))
			return;
		if (dx < 0 && !R_CONN_NX(map, x, y, z))
			return;
		if (dy > 0 && !R_CONN_PY(map, x, y, z))
			return;
		if (dy < 0 && !R_CONN_NY(map, x, y, z))
			return;

		/* Check diagonal connection */
		/* If direction vector index is set to a diagonal offset check if we can move there through connected "straight" squares. */
		if (dir > 3 &&
			!( (dx > 0 ? R_CONN_PX(map, x,    y+dy, z) : R_CONN_NX(map, x,    y+dy, z))
			&& (dy > 0 ? R_CONN_PY(map, x+dx, y,    z) : R_CONN_NY(map, x+dx, y,    z))
			&& !Grid_CheckForbidden(map, x,    y+dy, z)
			&& !Grid_CheckForbidden(map, x+dx, y,    z)))
			return;
		break;
	case ACTOR_SIZE_2x2:
		/* Range check of new values (2x2) */
		Vector2Set(poslistNew[0], nx, ny);
		Vector2Set(poslistNew[1], nx + 1, ny);
		Vector2Set(poslistNew[2], nx,     ny + 1);
		Vector2Set(poslistNew[3], nx + 1, ny + 1);

		if (poslistNew[0][0] < 0 || poslistNew[0][0] >= PATHFINDING_WIDTH || poslistNew[0][1] < 0 || poslistNew[0][1] >= PATHFINDING_WIDTH
		 || poslistNew[1][0] < 0 || poslistNew[1][0] >= PATHFINDING_WIDTH || poslistNew[1][1] < 0 || poslistNew[1][1] >= PATHFINDING_WIDTH
		 || poslistNew[2][0] < 0 || poslistNew[2][0] >= PATHFINDING_WIDTH || poslistNew[2][1] < 0 || poslistNew[2][1] >= PATHFINDING_WIDTH
		 || poslistNew[3][0] < 0 || poslistNew[3][0] >= PATHFINDING_WIDTH || poslistNew[3][1] < 0 || poslistNew[3][1] >= PATHFINDING_WIDTH)
			return;

		/* Connection checks for actor (2x2) */
		/* We need to check the 8 surrounding "straight" fields (2 in each direction; the 4 diagonal ones are skipped here) */
		VectorSet(poslist[0], x, y, z);					/* has NX and NY */
		VectorSet(poslist[1], x + 1, y,     z);	/* has PX and NY */
		VectorSet(poslist[2], x,     y + 1, z);	/* has NX and PY */
		VectorSet(poslist[3], x + 1, y + 1, z);	/* has PX and PY */

		if (dy == 0) {
			/* Straight movement along the x axis */
			if (dx > 0 &&
				(!R_CONN_PX(map, poslist[1][0], poslist[1][1], z)
				|| !R_CONN_PX(map, poslist[3][0], poslist[3][1], z)))
				return;
			if (dx < 0 &&
				(!R_CONN_NX(map, poslist[0][0], poslist[0][1], z)
				|| !R_CONN_NX(map, poslist[2][0], poslist[2][1], z)))
				return;
		}
		if (dx == 0) {
			/* Straight movement along the y axis */
			if (dy > 0 &&
				(!R_CONN_PY(map, poslist[2][0], poslist[2][1], z)
				|| !R_CONN_PY(map, poslist[3][0], poslist[3][1], z)))
				return;
			if (dy < 0 &&
				(!R_CONN_NY(map, poslist[0][0], poslist[0][1], z)
				|| !R_CONN_NY(map, poslist[1][0], poslist[1][1], z)))
				return;
		}

		/**
		 * Check diagonal connection for a 2x2 unit.
		 * Previously this was not needed because we skiped it in Grid_MoveMarkRoute :)
		 */
		if (dir > 3) {
			/*
			 * Diagonal movement
			 * Check the 3 new squares if they are connected (for the choosen direction).
			 * The last two checks (..CONN_..) are for the "corner" square,
			 * I'm not sure if the second one of them is needed,
			 * but I've included it anyway to be sure.
			 */
			if (dx > 0 && dy > 0) {
				/* Movement x+ y+ -> we check connections starting from poslist[3] (x+, y+) */
				if (
				!( R_CONN_PY(map, poslist[3][0], poslist[3][1], poslist[3][2])
				&& R_CONN_PX(map, poslist[3][0], poslist[3][1], poslist[3][2])
				&& R_CONN_PY(map, poslist[3][0]+1, poslist[3][1], poslist[3][2])
				&& R_CONN_PX(map, poslist[3][0], poslist[3][1]+1, poslist[3][2])))
					return;
			}
			if (dx > 0 && dy < 0) {
				/* Movement x+ y- -> we check connections starting from poslist[1] (x+, y) */
				if (
				!( R_CONN_NY(map, poslist[1][0], poslist[1][1], poslist[1][2])
				&& R_CONN_PX(map, poslist[1][0], poslist[1][1], poslist[1][2])
				&& R_CONN_NY(map, poslist[1][0]+1, poslist[1][1], poslist[1][2])
				&& R_CONN_PX(map, poslist[1][0], poslist[1][1]-1, poslist[1][2])))
					return;
			}
			if (dx < 0 && dy < 0) {
				/* Movement x- y- -> we check connections starting from poslist[0] (x, y) */
				if (
				!( R_CONN_NY(map, poslist[0][0], poslist[0][1], poslist[0][2])
				&& R_CONN_NX(map, poslist[0][0], poslist[0][1], poslist[0][2])
				&& R_CONN_NY(map, poslist[0][0]-1, poslist[0][1], poslist[0][2])
				&& R_CONN_NX(map, poslist[0][0], poslist[0][1]-1, poslist[0][2])))
					return;
			}
			if (dx < 0 && dy > 0) {
				/* Movement x- y+ -> we check connections starting from poslist[2] (x, y+) */
				if (
				!( R_CONN_PY(map, poslist[2][0], poslist[2][1], poslist[2][2])
				&& R_CONN_NX(map, poslist[2][0], poslist[2][1], poslist[2][2])
				&& R_CONN_PY(map, poslist[2][0]-1, poslist[2][1], poslist[2][2])
				&& R_CONN_NX(map, poslist[2][0], poslist[2][1]+1, poslist[2][2])))
					return;
			}
			/* No checkforbidden tests needed here because all 3 affected fields
			 * are checked later on anyway. */

		}

		/* Check if there is an obstacle (i.e. a small part of a wall) between one of the 4 fields of the target location.
		 * i.e. Interconnections. */
		/**
		 * @todo MAJOR Problem!
		 * This would still not prevent a single wooden post to stand exactly into the middle of the 2x2 unit!
		 * How are we supposed to check that?
		 * Example: There are some large doors in the warehouses in the harbour map with a post-like part of the wall in hte middle.
		 */
		if (
		!( R_CONN_PX(map, poslistNew[0][0], poslist[0][1], z)	/* Go from x0/y0 forward into +x direction */
		&& R_CONN_PY(map, poslistNew[0][0], poslist[0][1], z)	/* Go from x0/y0 forward into +x direction */
		&& R_CONN_NX(map, poslistNew[3][0], poslist[3][1], z)	/* Go from px/py back into -x direction */
		&& R_CONN_NY(map, poslistNew[3][0], poslist[3][1], z)))	/* Go from px/py back into -y direction */
			return;

		break;
	default:
		Com_Error(ERR_DROP, "Grid_MoveMark: unknown actor-size: %i", actor_size);
	}

	/* Height checks. */
	/** @todo I think this and the next few lines need to be adapted to support 2x2 units.
	 * If we ever encounter 2x2 units falling through tiny holes this might be the reason :)
	 */
	sh = R_STEP(map, x, y, z) ? sh_big : sh_low;
	z += (h + sh) / 0x10;

	VectorSet(dummy, nx, ny, z);
	z = Grid_Fall(map, dummy, actor_size);

	/* Can it be better than ever? */
	if (R_AREA(map, nx, ny, z) <= l)
		return;

	/* Test for forbidden (by other entities) areas. */
	switch (actor_size) {
	case ACTOR_SIZE_NORMAL:
		if (Grid_CheckForbidden(map, nx, ny, z))
			return;
		break;
	case ACTOR_SIZE_2x2:
		if (Grid_CheckForbidden(map, poslistNew[0][0], poslistNew[0][1], z)
		|| Grid_CheckForbidden(map, poslistNew[1][0], poslistNew[1][1], z)
		|| Grid_CheckForbidden(map, poslistNew[2][0], poslistNew[2][1], z)
		|| Grid_CheckForbidden(map, poslistNew[3][0], poslistNew[3][1], z))
			return;
		break;
	default:
		Com_Error(ERR_DROP, "Grid_MoveMark: (2) unknown actor-size: %i", actor_size);
	}

	/* Store move. */
	R_AREA(map, nx, ny, z) = l;	/* *< Store TUs for this square. */
	VectorSet(dummy, nx, ny, z);
	PQueuePush(pqueue, dummy, l); /* TODO add heuristic for A* algorithm */
}

/**
 * @param[in|out] map Pointer to client or server side routing table (clMap, svMap)
 * @param[in] from The position to start the calculation from.
 * @param[in] actor_size The size of thing to calc the move for (e.g. size=2 means 2x2).
 * The plan is to have the 'origin' in 2x2 units in the upper-left (towards the lower coordinates) corner of the 2x2 square.
 * @param[in] distance to calculate move-information for - currently unused
 * @param[in] fb_list Forbidden list (entities are standing at those points)
 * @param[in] fb_length Length of forbidden list
 * @sa Grid_MoveMark
 * @sa G_MoveCalc
 * @sa CL_ConditionalMoveCalc
 */
void Grid_MoveCalc (struct routing_s *map, pos3_t from, int actor_size, int distance, byte ** fb_list, int fb_length)
{
	int dir;
	int count;
	PQUEUE pqueue;
	pos3_t pos;
	/* reset move data */
	/* ROUTING_NOT_REACHABLE means, not reachable */
	memset(map->area, ROUTING_NOT_REACHABLE, PATHFINDING_WIDTH * PATHFINDING_WIDTH * PATHFINDING_HEIGHT);
	map->fblist = fb_list;
	map->fblength = fb_length;

	if (distance > MAX_ROUTE)
		distance = MAX_ROUTE;

	VectorCopy(from, exclude_from_forbiddenlist); /**< Prepare exclusion of starting-location (i.e. this should be ent-pos or le-pos) in Grid_CheckForbidden */

	PQueueInitialise(&pqueue, 1024);
	PQueuePush(&pqueue, from, 0);
	R_AREA(map, from[0], from[1], from[2]) = 0;

	count = 0;
	while (!PQueueIsEmpty(&pqueue)) {
		PQueuePop(&pqueue, pos);
		count++;

		/* for A*
		if pos = goal
			return pos
		*/
		for (dir = 0; dir < DIRECTIONS; dir++) {
			Grid_MoveMark(map, pos, dir, actor_size, &pqueue);
		}

	}
	/* Com_Printf("Loop: %i", count); */
	PQueueFree(&pqueue);
}

/**
 * @brief Caches the calculated move
 * @param[in] map Routing data
 * @sa AI_ActorThink
 */
void Grid_MoveStore (struct routing_s *map)
{
	memcpy(map->areaStored, map->area, sizeof(map->areaStored));
}


/**
 * @brief Return the needed TUs to walk to a given position
 * @param[in] map Routing data
 * @param[in] to Position to walk to
 * @param[in] stored Use the stored mask (the cached move) of the routing data
 * @return ROUTING_NOT_REACHABLE if the move isn't possible
 * @return length of move otherwise (TUs)
 */
pos_t Grid_MoveLength (struct routing_s *map, pos3_t to, qboolean stored)
{
#ifdef PARANOID
	if (to[2] >= PATHFINDING_HEIGHT) {
		Com_DPrintf(DEBUG_ENGINE, "Grid_MoveLength: WARNING to[2] = %i(>= HEIGHT)\n", to[2]);
		return ROUTING_NOT_REACHABLE;
	}
#endif

	if (!stored)
		return R_AREA(map, to[0], to[1], to[2]);
	else
		return R_SAREA(map, to[0], to[1], to[2]);
}


/**
 * @param[in] map Pointer to client or server side routing table (clMap, svMap)
 * @sa Grid_MoveNext
 * @param[in] map The routing table of the current loaded map
 * @param[in] pos
 * @param[in] sz
 * @param[in] l
 * @todo add 2x2 unit support
 */
static byte Grid_MoveCheck (struct routing_s *map, pos3_t pos, pos_t sz, byte l)
{
	int x, y, sh;
	byte dir; /**< Direction vector index */
	int dx, dy;
	pos_t z;
	pos3_t dummy;

#ifdef PARANOID
	if (sz >= PATHFINDING_HEIGHT) {
		Com_DPrintf(DEBUG_ENGINE, "Grid_MoveCheck: WARNING sz = %i(>= HEIGHT %i)\n", sz, PATHFINDING_HEIGHT);
		return 0xFF;
	}
#endif

	for (dir = 0; dir < DIRECTIONS; dir++) {
		/** @todo Why do we use the negative values here? */
		dx = -dvecs[dir][0];
		dy = -dvecs[dir][1];

		/* range check */
		if (dx > 0 && pos[0] >= PATHFINDING_WIDTH - 1)
			continue;
		if (dy > 0 && pos[1] >= PATHFINDING_WIDTH - 1)
			continue;
		if (dx < 0 && pos[0] <= 0)
			continue;
		if (dy < 0 && pos[1] <= 0)
			continue;

		/* distance table check */
		x = pos[0] - dx;
		y = pos[1] - dy;
		z = sz;

		/**
		 * This code verifies the TE table. If the table does not match the movement calculation,
		 * we skip this invalid comparison.
		*/
		if (R_AREA(map, x, y, z) != l - ((dir > 3) ? TU_MOVE_DIAGONAL : TU_MOVE_STRAIGHT))
			continue;

		/* connection checks */
		if (dx > 0 && !R_CONN_PX(map, x, y, z))
			continue;
		if (dx < 0 && !R_CONN_NX(map, x, y, z))
			continue;
		if (dy > 0 && !R_CONN_PY(map, x, y, z))
			continue;
		if (dy < 0 && !R_CONN_NY(map, x, y, z))
			continue;
		if (dir > 3 &&
			!( (dx > 0 ? R_CONN_PX(map, x,    y+dy, z) : R_CONN_NX(map, x,    y+dy, z))
			&& (dy > 0 ? R_CONN_PY(map, x+dx, y,    z) : R_CONN_NY(map, x+dx, y,    z)))
			)
			continue;

		/* height checks */
		sh = R_STEP(map, x, y, z) ? sh_big : sh_low;
		z += (R_HEIGHT(map, x, y, z) + sh) / 0x10;

		VectorSet(dummy, pos[0], pos[1], z);
		z = Grid_Fall(map, dummy, ACTOR_SIZE_NORMAL); /** @todo 2x2 units support? */

		/*  Com_Printf("pos: (%i %i %i) (x,y,z): (%i %i %i)\n", pos[0], pos[1], pos[2], x, y, z); */

		if (pos[2] == z)
			break;				/* found it! */

		/* not found... try next dir */
	}

	return dir;
}


/**
 * @brief The next stored move direction
 * @param[in] map Pointer to client or server side routing table (clMap, svMap)
 * @param[in] from
 * @return (Guess: a direction index (see dvecs and DIRECTIONS))
 * @sa Grid_MoveCheck
 */
pos_t Grid_MoveNext (struct routing_s *map, pos3_t from)
{
	byte dv;
	pos_t l, z;

	l = R_AREA(map, from[0], from[1], from[2]); /**< Get TUs for this square */

	/* finished */
	if (!l) {
		/* ROUTING_NOT_REACHABLE means, not possible/reachable */
		return ROUTING_NOT_REACHABLE;
	}

	/* do tests */
	for (z = 0; z < PATHFINDING_HEIGHT; z++) {
		/* suppose it's possible at that height */
		dv = Grid_MoveCheck(map, from, z, l);
		if (dv < DIRECTIONS)
			return dv | (z << 3);
	}

	/* shouldn't happen */
	Com_Printf("Grid_MoveNext failed...\n");
	/* ROUTING_NOT_REACHABLE means, not possible/reachable */
	return ROUTING_NOT_REACHABLE;
}


/**
 * @brief Returns the height of the floor in a cell.
 * @param[in] map Pointer to client or server side routing table (clMap, svMap)
 * @param[in] pos Position in the map to check the height
 * @return The actual model height of the cell's floor.
 */
pos_t Grid_Height (struct routing_s *map, pos3_t pos)
{
	/* max 8 levels */
	if (pos[2] >= PATHFINDING_HEIGHT) {
		Com_Printf("Grid_Height: Warning: z level is bigger than 7: %i\n", pos[2]);
		pos[2] &= 7;
	}
	return R_HEIGHT(map, pos[0], pos[1], pos[2]) * QUANT;
}


/**
 * @brief Returns the maximum height of an obstruction that an actor can travel over.
 * @param[in] map Pointer to client or server side routing table (clMap, svMap)
 * @param[in] pos Position in the map to check the height
 * @return The actual model height increse allowed to move into an adjacent cell.
 */
pos_t Grid_StepUp (struct routing_s *map, pos3_t pos)
{
	/* max 8 levels */
	if (pos[2] >= PATHFINDING_HEIGHT) {
		Com_Printf("Grid_StepUp: Warning: z level is bigger than 7: %i\n", pos[2]);
		pos[2] &= 7;
	}
	return R_STEP(map, pos[0], pos[1], pos[2]) ? sh_big : sh_low;
}


/**
 * @brief Returns non-zero if the cell is filled (solid) and cannot be entered.
 * @param[in] map Pointer to client or server side routing table (clMap, svMap)
 * @param[in] pos Position in the map to check for filling
 * @return 0 if the cell is vacant (of the world model), non-zero otherwise.
 */
int Grid_Filled (struct routing_s *map, pos3_t pos)
{
	/* max 8 levels */
	if (pos[2] >= PATHFINDING_HEIGHT) {
		Com_Printf("Grid_Filled: Warning: z level is bigger than 7: %i\n", pos[2]);
		pos[2] &= 7;
	}
	return R_FILLED(map, pos[0], pos[1], pos[2]);
}


/**
 * @brief Calculated the new height level when something falls down from a certain position.
 * @param[in] map Pointer to client or server side routing table (clMap, svMap)
 * @param[in] pos Position in the map to start the fall (starting height is the z-value in this position)
 * @param[in] actor_size Give the field size of the actor (e.g. for 2x2 units) to check linked fields as well.
 * @return New z (height) value.
 * @return 0xFF if an error occured.
 */
pos_t Grid_Fall (struct routing_s *map, pos3_t pos, int actor_size)
{
	pos_t z = pos[2];

	/* is z off-map? */
	if (z >= PATHFINDING_HEIGHT) {
		Com_DPrintf(DEBUG_ENGINE, "Grid_Fall: z (height) out of bounds): z=%i max=%i\n", z, PATHFINDING_HEIGHT);
		return 0xFF;
	}

/*	Com_Printf("%i:%i:%i\n", pos[0], pos[1], pos[2]); DEBUG */

	/**
	 * This fixes a problem with height positions
	 * set to 255 in Grid_Height and Grid_PosToVec (->floating soldiers/aliens)
	 */
	if (z == 0)
		return 0;

	/* Reduce z if we can fall down - depending on actor size we also check the atatched squares. */
	switch (actor_size) {
	case ACTOR_SIZE_NORMAL:
		while (z > 0 && R_FALL(map, pos[0], pos[1], z)) {
			z--;
		}
		break;
	case ACTOR_SIZE_2x2:
		while (z > 0
			&& R_FALL(map, pos[0], pos[1], z)
			&& R_FALL(map, pos[0]+1, pos[1], z)
			&& R_FALL(map, pos[0], pos[1]+1, z)
			&& R_FALL(map, pos[0]+1, pos[1]+1, z)) {
			z--;
		}
		break;
	default:
		Com_Error(ERR_DROP, "Grid_Fall: unknown actor-size: %i", actor_size);
	}

	return z;
}

/**
 * @sa Grid_Height
 */
void Grid_PosToVec (struct routing_s *map, pos3_t pos, vec3_t vec)
{
	PosToVec(pos, vec);
#ifdef PARANOID
	if (pos[2] >= PATHFINDING_HEIGHT)
		Com_Printf("Grid_PosToVec: Warning - z level bigger than 7 (%i - source: %.02f)\n", pos[2], vec[2]);
#endif
	vec[2] += Grid_Height(map, pos);
}

/**
 * @sa CM_InlineModel
 * @sa CM_CheckUnit
 * @sa CM_UpdateConnection
 * @sa CMod_LoadSubmodels
 * @param[in] map The routing map (either server or client map)
 * @param[in] name Name of the inline model to compute the mins/maxs for
 * @param[in] origin Offset of the inline model (used by ET_DOOR and ET_ROTATING)
 * @param[in] angles Rotation of the inline model
 * @param[in] list The local models list (a local model has a name starting with * followed by the model number)
 */

void Grid_RecalcRouting (struct routing_s *map, const char *name, const char **list)
{
	cBspModel_t *model;
	pos3_t min, max;
	int x, y;
	pos_t z;
	unsigned int i;

	assert(list);

	/* get inline model, if it is one */
	if (*name != '*') {
		Com_Printf("Called Grid_RecalcRouting with no inline model\n");
		return;
	}
	model = CM_InlineModel(name);
	if (!model) {
		Com_Printf("Called Grid_RecalcRouting with invalid inline model name '%s'\n", name);
		return;
	}
	inlineList = list;

	/* get dimensions */
	if (VectorNotEmpty(model->angles)) {
		vec3_t minVec, maxVec;
		float maxF, v;

		maxF = 0;
		for (i = 0; i < 3; i++) {
			v = fabsf(model->mins[i]);
			if (v > maxF)
				maxF = v;
			v = fabsf(model->maxs[i]);
			if (v > maxF)
				maxF = v;
		}
		for (i = 0; i < 3; i++) {
			minVec[i] = -maxF;
			maxVec[i] = maxF;
		}
		/* Now offset by origin then convert to position (Doors do not have 0 origins) */
		VectorAdd(minVec, model->origin, minVec);
		VecToPos(minVec, min);
		VectorAdd(maxVec, model->origin, maxVec);
		VecToPos(maxVec, max);
	} else {  /* normal */
		vec3_t temp;
		/* Now offset by origin then convert to position (Doors do not have 0 origins) */
		VectorAdd(model->mins, model->origin, temp);
		VecToPos(temp, min);
		VectorAdd(model->maxs, model->origin, temp);
		VecToPos(temp, max);
	}

	memset(map->filled, 0, PATHFINDING_WIDTH * PATHFINDING_WIDTH);

	/* fit min/max into the world size */
	max[0] = min(max[0] + 2, PATHFINDING_WIDTH - 1);
	max[1] = min(max[1] + 2, PATHFINDING_WIDTH - 1);
	max[2] = min(max[2] + 2, PATHFINDING_HEIGHT - 1);
	for (i = 0; i < 3; i++)
		min[i] = max(min[i] - 2, 0);

#if 0
	Com_Printf("routing: %s (%i %i %i) (%i %i %i)\n", name,
		(int)min[0], (int)min[1], (int)min[2],
		(int)max[0], (int)max[1], (int)max[2]);
#endif

#if 0
	Com_Printf("Before:\n");
	Grid_DumpMap(map, (int)min[0], (int)min[1], (int)min[2], (int)max[0], (int)max[1], (int)max[2]);
#endif

	/* check unit heights */
	for (z = min[2]; z < max[2]; z++)
		for (y = min[1]; y < max[1]; y++)
			for (x = min[0]; x < max[0]; x++)
				RT_CheckUnit(map, x, y, z);

#if 0
	Grid_DumpMap(map, (int)min[0], (int)min[1], (int)min[2], (int)max[0], (int)max[1], (int)max[2]);
#endif

	/* check connections */
	for (z = min[2]; z < max[2]; z++)
		for (y = min[1]; y < max[1]; y++)
			for (x = min[0]; x < max[0]; x++)
				RT_UpdateConnection(map, x, y, z, qtrue);

#if 0
	Com_Printf("After:\n");
	Grid_DumpMap(map, (int)min[0], (int)min[1], (int)min[2], (int)max[0], (int)max[1], (int)max[2]);
#endif

	inlineList = NULL;
}


/*
==============================================================================
TARGETING FUNCTIONS
==============================================================================
*/

/**
 * @note Grenade Aiming Maths
 * --------------------------------------------------------------
 *
 * There are two possibilities when aiming: either we can reach the target at
 * maximum speed or we can't. If we can reach it we would like to reach it with
 * as flat a trajectory as possible. To do this we calculate the angle to hit the
 * target with the projectile traveling at the maximum allowed velocity.
 *
 * However, if we can't reach it then we'd like the aiming curve to use the smallest
 * possible velocity that would have reached the target.
 *
 * let d  = horizontal distance to target
 *     h  = vertical distance to target
 *     g  = gravity
 *     v  = launch velocity
 *     vx = horizontal component of launch velocity
 *     vy = vertical component of launch velocity
 *     alpha = launch angle
 *     t  = time
 *
 * Then using the laws of linear motion and some trig
 *
 * d = vx * t
 * h = vy * t - 1/2 * g * t^2
 * vx = v * cos(alpha)
 * vy = v * sin(alpha)
 *
 * and some trig identities
 *
 * 2*cos^2(x) = 1 + cos(2*x)
 * 2*sin(x)*cos(x) = sin(2*x)
 * a*cos(x) + b*sin(x) = sqrt(a^2 + b^2) * cos(x - atan2(b,a))
 *
 * it is possible to show that:
 *
 * alpha = 0.5 * (atan2(d, -h) - theta)
 *
 * where
 *               /    2       2  \
 *              |    v h + g d    |
 *              |  -------------- |
 * theta = acos |        ________ |
 *              |   2   / 2    2  |
 *               \ v  \/ h  + d  /
 *
 *
 * Thus we can calculate the desired aiming angle for any given velocity.
 *
 * With some more rearrangement we can show:
 *
 *               ________________________
 *              /           2
 *             /         g d
 *  v =       / ------------------------
 *           /   ________
 *          /   / 2    2
 *        \/  \/ h  + d   cos(theta) - h
 *
 * Which we can also write as:
 *                _________________
 *               /        a
 * f(theta) =   / ----------------
 *            \/  b cos(theta) - c
 *
 * where
 *  a = g*d^2
 *  b = sqrt(h*h+d*d)
 *  c = h
 *
 * We can imagine a graph of launch velocity versus launch angle. When the angle is near 0 degrees (i.e. totally flat)
 * more and more velocity is needed. Similarly as the angle gets closer and closer to 90 degrees.
 *
 * Somewhere in the middle is the minimum velocity that we could possibly hit the target with and the 'optimum' angle
 * to fire at. Note that when h = 0 the optimum angle is 45 degrees. We want to find the minimum velocity so we need
 * to take the derivative of f (which I suggest doing with an algebra system!).
 *
 * f'(theta) =  a * b * sin(theta) / junk
 *
 * the `junk` is unimportant because we're just going to set f'(theta) = 0 and multiply it out anyway.
 *
 * 0 = a * b * sin(theta)
 *
 * Neither a nor b can be 0 as long as d does not equal 0 (which is a degenerate case). Thus if we solve for theta
 * we get 'sin(theta) = 0', thus 'theta = 0'. If we recall that:
 *
 *  alpha = 0.5 * (atan2(d, -h) - theta)
 *
 * then we get our 'optimum' firing angle alpha as
 *
 * alpha = 1/2 * atan2(d, -h)
 *
 * and if we subsitute back into our equation for v and we get
 *
 *               _______________
 *              /        2
 *             /      g d
 *  vmin =    / ---------------
 *           /   ________
 *          /   / 2    2
 *        \/  \/ h  + d   - h
 *
 * as the minimum launch velocity for that angle.
 */
float Com_GrenadeTarget (vec3_t from, vec3_t at, float speed, qboolean launched, qboolean rolled, vec3_t v0)
{
	const float rollAngle = 3.0; /* angle to throw at for rolling, in degrees. */
	vec3_t delta;
	float d, h, g, v, alpha, theta, vx, vy;
	float k, gd2, len;

	/* calculate target distance and height */
	h = at[2] - from[2];
	VectorSubtract(at, from, delta);
	delta[2] = 0;
	d = VectorLength(delta);

	/* check that it's not degenerate */
	if (d == 0) {
		return 0;
	}

	/* precalculate some useful values */
	g = GRAVITY;
	gd2 = g * d * d;
	len = sqrt(h * h + d * d);

	/* are we rolling? */
	if (rolled) {
		alpha = rollAngle * torad;
		theta = atan2(d, -h) - 2 * alpha;
		k = gd2 / (len * cos(theta) - h);
		if (k <= 0)	/* impossible shot at any velocity */
			return 0;
		v = sqrt(k);
	} else {
		/* firstly try with the maximum speed possible */
		v = speed;
		k = (v * v * h + gd2) / (v * v * len);

		/* check whether the shot's possible */
		if (launched && k >= -1 && k <= 1) {
			/* it is possible, so calculate the angle */
			alpha = 0.5 * (atan2(d, -h) - acos(k));
		} else {
			/* calculate the minimum possible velocity that would make it possible */
			alpha = 0.5 * atan2(d, -h);
			v = sqrt(gd2 / (len - h));
		}
	}

	/* calculate velocities */
	vx = v * cos(alpha);
	vy = v * sin(alpha);
	VectorNormalize(delta);
	VectorScale(delta, vx, v0);
	v0[2] = vy;

	/* prevent any rounding errors */
	VectorNormalize(v0);
	VectorScale(v0, v - DIST_EPSILON, v0);

	/* return time */
	return d / vx;
}
