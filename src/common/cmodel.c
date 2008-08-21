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

/**
 * @note The vectors are from 0 up to 2*MAX_WORLD_WIDTH - but not negative
 * @note holds the smallest bounding box that will contain the map
 * @sa CL_ClampCamToMap
 * @sa CL_OutsideMap
 * @sa CMod_GetMapSize
 * @sa SV_ClearWorld
 */
vec3_t map_min, map_max;

/** @brief server and client routing table */
/* routing_t svMap, clMap; */
routing_t svMap[ACTOR_MAX_SIZE], clMap[ACTOR_MAX_SIZE]; /* A routing_t per size */
pathing_t svPathMap, clPathMap; /* This is where the data for TUS used to move and actor locations go */

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

/** @note these are the TUs used to intentionally move in a given direction.  Falling not included. */
/*                                            E  W  N  S NE SW NW SE UP DN  ST         KN        -- FA -- -- (Fliers up             )(Fliers down           ) */
const int TUs_used[PATHFINDING_DIRECTIONS] = {
	TU_MOVE_STRAIGHT, /* E  */
	TU_MOVE_STRAIGHT, /* W  */
	TU_MOVE_STRAIGHT, /* N  */
	TU_MOVE_STRAIGHT, /* S  */
	TU_MOVE_DIAGONAL, /* NE */
	TU_MOVE_DIAGONAL, /* SW */
	TU_MOVE_DIAGONAL, /* NW */
	TU_MOVE_DIAGONAL, /* SE */
	TU_MOVE_CLIMB,    /* UP     */
	TU_MOVE_CLIMB,    /* DOWN   */
	TU_CROUCH,        /* STAND  */
	TU_CROUCH,        /* CROUCH */
	0,				  /* ???    */
	TU_MOVE_FALL,	  /* FALL   */
	0,				  /* ???    */
	0,				  /* ???    */
	TU_MOVE_STRAIGHT * TU_FLYING_MOVING_FACTOR, /* FLY UP & E  */
	TU_MOVE_STRAIGHT * TU_FLYING_MOVING_FACTOR, /* FLY UP & W  */
	TU_MOVE_STRAIGHT * TU_FLYING_MOVING_FACTOR, /* FLY UP & N  */
	TU_MOVE_STRAIGHT * TU_FLYING_MOVING_FACTOR, /* FLY UP & S  */
	TU_MOVE_DIAGONAL * TU_FLYING_MOVING_FACTOR, /* FLY UP & NE */
	TU_MOVE_DIAGONAL * TU_FLYING_MOVING_FACTOR, /* FLY UP & SW */
	TU_MOVE_DIAGONAL * TU_FLYING_MOVING_FACTOR, /* FLY UP & NW */
	TU_MOVE_DIAGONAL * TU_FLYING_MOVING_FACTOR, /* FLY UP & SE */
	TU_MOVE_STRAIGHT * TU_FLYING_MOVING_FACTOR, /* FLY DOWN & E  */
	TU_MOVE_STRAIGHT * TU_FLYING_MOVING_FACTOR, /* FLY DOWN & W  */
	TU_MOVE_STRAIGHT * TU_FLYING_MOVING_FACTOR, /* FLY DOWN & N  */
	TU_MOVE_STRAIGHT * TU_FLYING_MOVING_FACTOR, /* FLY DOWN & S  */
	TU_MOVE_DIAGONAL * TU_FLYING_MOVING_FACTOR, /* FLY DOWN & NE */
	TU_MOVE_DIAGONAL * TU_FLYING_MOVING_FACTOR, /* FLY DOWN & SW */
	TU_MOVE_DIAGONAL * TU_FLYING_MOVING_FACTOR, /* FLY DOWN & NW */
	TU_MOVE_DIAGONAL * TU_FLYING_MOVING_FACTOR  /* FLY DOWN & SE */
	};

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
		out->surfaceFlags = LittleLong(in->surfaceFlags);
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
			/* Remember that the total bytes that are the same is c + 2 */
			for (i = 0; i < c + 2; i++)
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

	FS_OpenFile(name, &file);
	if (!file.f && !file.z)
		return 1;

	FS_Read(header, sizeof(header), &file);

	FS_CloseFile(&file);

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
 * @param[in] levelmask
 * @sa TR_TestLine
 * @sa CM_InlineModel
 * @sa TR_TransformedBoxTrace
 * @return qtrue - hit something
 * @return qfalse - hit nothing
 */
qboolean CM_EntTestLine (const vec3_t start, const vec3_t stop, const int levelmask)
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
		trace = CM_TransformedBoxTrace(start, stop, vec3_origin, vec3_origin, model->tile, model->headnode, MASK_ALL, 0, model->origin, model->angles);
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
 * @param[in] levelmask
 * @param[in] entlist of entities that might be on this line
 * @sa CM_EntTestLine
 * @return qtrue - hit something
 * @return qfalse - hit nothing
 */
qboolean CM_TestLineWithEnt (const vec3_t start, const vec3_t stop, const int levelmask, const char **entlist)
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
 * @param[in] levelmask
 * @sa TR_TestLineDM
 * @sa TR_TransformedBoxTrace
 */
qboolean CM_EntTestLineDM (const vec3_t start, const vec3_t stop, vec3_t end, const int levelmask)
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
		trace = CM_TransformedBoxTrace(start, end, vec3_origin, vec3_origin, model->tile, model->headnode, MASK_ALL, 0, model->origin, model->angles);
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
trace_t CM_TransformedBoxTrace (const vec3_t start, const vec3_t end, const vec3_t mins, const vec3_t maxs, int tile, int headnode, int brushmask, int brushrejects, const vec3_t origin, const vec3_t angles)
{
	return TR_TransformedBoxTrace(start, end, mins, maxs, &mapTiles[tile], headnode, brushmask, brushrejects, origin, angles);
}


/*
===============================================================================
GAME RELATED TRACING
===============================================================================
*/


/**
 * @brief Calculate the map size via model data and store grid size
 * in map_min and map_max. This is done with every new map load
 * @sa map_min
 * @sa map_max
 * @sa CMod_LoadRouting
 */
static void CMod_GetMapSize (void)
{
	const vec3_t offset = {MAP_SIZE_OFFSET, MAP_SIZE_OFFSET, MAP_SIZE_OFFSET};
	pos3_t start, end, test;
	vec3_t mins, maxs, normal, origin;
	int i;
	trace_t trace;

	/* Initialize start, end, and normal */
	VectorSet(start, 0, 0, 0);
	VectorSet(end, PATHFINDING_WIDTH - 1, PATHFINDING_WIDTH - 1, PATHFINDING_HEIGHT - 1);
	VectorSet(normal, UNIT_SIZE / 2, UNIT_SIZE / 2, UNIT_HEIGHT / 2);
	VectorCopy(vec3_origin, origin);

	for (i = 0; i < 3; i++) {
		/* Lower positive boundary */
		while (end[i]>start[i]) {
			/* Adjust ceiling */
			VectorCopy(start, test);
			test[i] = end[i] - 1; /* test is now one floor lower than end */
			/* Prep boundary box */
			PosToVec(test, mins);
			VectorSubtract(mins, normal, mins);
			PosToVec(end, maxs);
			VectorAdd(maxs, normal, maxs);
			/* Test for stuff in a small box, if there is something then exit while */
			trace = TR_CompleteBoxTrace(origin, origin, mins, maxs, 0x1FF, MASK_ALL, 0);
			if (trace.fraction < 1.0)
				break;
			/* There is nothing, lower the boundary. */
			end[i]--;
		}

		/* Raise negative boundary */
		while (end[i]>start[i]) {
			/* Adjust ceiling */
			VectorCopy(end, test);
			test[i] = start[i] + 1; /* test is now one floor lower than end */
			/* Prep boundary box */
			PosToVec(start, mins);
			VectorSubtract(mins, normal, mins);
			PosToVec(test, maxs);
			VectorAdd(maxs, normal, maxs);
			/* Test for stuff in a small box, if there is something then exit while */
			trace = TR_CompleteBoxTrace(origin, origin, mins, maxs, 0x1FF, MASK_ALL, 0);
			if (trace.fraction < 1.0)
				break;
			/* There is nothing, raise the boundary. */
			start[i]++;
		}
	}

	/* convert to vectors */
	PosToVec(start, map_min);
	PosToVec(end, map_max);

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
static void CMod_LoadRouting (const char *name, lump_t * l, int sX, int sY, int sZ)
{
	static byte reroute[PATHFINDING_WIDTH][PATHFINDING_WIDTH];

	static vec3_t wpMins, wpMaxs;
	static routing_t temp_map[ACTOR_MAX_SIZE];
	byte *source;
	int length;
	int x, y, z, size, dir;
	int x2, y2, z2;
	int maxX, maxY, maxZ;
	int new_z;
	unsigned int i;
#ifdef DEBUG
	/* count all reachable fields of the map for debugging */
	qboolean route_available = qfalse;
#endif
	const int targetLength = sizeof(wpMins) + sizeof(wpMaxs) + sizeof(temp_map);

	inlineList = NULL;

	if (!l)
		Com_Error(ERR_DROP, "CMod_LoadRouting: No lump given");

	if (!l->filelen)
		Com_Error(ERR_DROP, "CMod_LoadRouting: Map has NO routing lump");

	assert((sX > -(PATHFINDING_WIDTH / 2)) && (sX < (PATHFINDING_WIDTH / 2)));
	assert((sY > -(PATHFINDING_WIDTH / 2)) && (sY < (PATHFINDING_WIDTH / 2)));
	assert((sZ >= 0) && (sZ < PATHFINDING_HEIGHT));


	/* calculate existing border to determine overlay */
	CMod_GetMapSize();

	/* routing must be redone for overlapping tiles and borders */
	memset(reroute, 0, sizeof(reroute));

	source = cmod_base + l->fileofs;

	i = CMod_DeCompressRouting(&source, (byte*)wpMins);
	/* Com_Printf("wpMins: %i %i\n", i, sizeof(wpMins)); */
	length = i;
	i = CMod_DeCompressRouting(&source, (byte*)wpMaxs);
	/* Com_Printf("wpMaxs: %i %i\n", i, sizeof(wpMaxs)); */
	length += i;
	i = CMod_DeCompressRouting(&source, (byte*)temp_map);
	/* Com_Printf("temp_map: %i %i\n", i, sizeof(temp_map)); */
	length += i;

	if (length != targetLength)
		Com_Error(ERR_DROP, "CMod_LoadRouting: Map has BAD routing lump; expected %i got %i", targetLength, length);


	/* wpMin and wpMax have the map size data from the initial build.
	 * Crop this if any part of the model is shifted off the playing field.
	 */
	if (wpMins[0] + sX < 0) wpMins[0] = -sX;
	if (wpMins[1] + sY < 0) wpMins[1] = -sY;
	if (wpMins[2] + sZ < 0) wpMins[2] = -sZ;
	if (wpMaxs[0] + sX >= PATHFINDING_WIDTH) wpMaxs[0] = PATHFINDING_WIDTH - sX - 1;
	if (wpMaxs[1] + sY >= PATHFINDING_WIDTH) wpMaxs[1] = PATHFINDING_WIDTH - sY - 1;
	if (wpMaxs[2] + sZ >= PATHFINDING_HEIGHT) wpMaxs[2] = PATHFINDING_HEIGHT - sZ - 1;

	/* Things that need to be done:
	 * The floor, ceiling, and route data can be copied over from the map.
	 * All data must be regenerated for cells with overlapping content or where new model data is adjacent
	 *   to a cell with existing model data.
	 */

	/* Copy the routing information into our master table */
	maxX = sX > 0 ? PATHFINDING_WIDTH - sX : PATHFINDING_WIDTH;
	maxY = sY > 0 ? PATHFINDING_WIDTH - sY : PATHFINDING_WIDTH;
	maxZ = sZ > 0 ? PATHFINDING_HEIGHT - sZ : PATHFINDING_HEIGHT;

	for (size = 0; size < ACTOR_MAX_SIZE; size++)
		for (z = sZ < 0 ? -sZ : 0; z < maxZ; z++)
			for (y = sY < 0 ? -sY : 0; y < maxY; y++)
				for (x = sX < 0 ? -sX : 0; x < maxX; x++) {
					clMap[size].floor[z + sZ][y + sY][x + sX] = temp_map[size].floor[z][y][x];
					clMap[size].ceil[z + sZ][y + sY][x + sX] = temp_map[size].ceil[z][y][x];
					for (dir = 0; dir < CORE_DIRECTIONS; dir++)
						clMap[size].route[z + sZ][y + sY][x + sX][dir] = temp_map[size].route[z][y][x][dir];
					}

	/* look for adjacent and overlapping data to regenerate */
	for (size = 1; size <= ACTOR_MAX_SIZE; size++) {
		z2 = min(min(wpMaxs[2], map_max[2]) + 1, PATHFINDING_HEIGHT - 1);
		y2 = min(min(wpMaxs[1], map_max[1]) + 1, PATHFINDING_WIDTH - 1) - size;
		x2 = min(min(wpMaxs[0], map_max[0]) + 1, PATHFINDING_WIDTH - 1) - size;
		for (y = max(max(wpMins[1] - 1, map_min[1] - 1), 0); y <= y2; y++)
			for (x = max(max(wpMins[0] - 1, map_min[0] - 1), 0); x <= x2; x++) {
				/* z is done last because the tracing code can skip z checks. */
				for (z = max(max(wpMins[2] - 1, map_min[2] - 1), 0); z <= z2; y++) {
					new_z = RT_CheckCell(clMap, size, x, y, z);
					/* new_z should never be below z. */
					assert(new_z >= z);
					/* Adjust z if this check adjusted multiple cells. */
					z += new_z - z -1;
#ifdef DEBUG
					route_available = qtrue;
#endif
				}
				/* Check for walls separate- z can also jump here */
				for (dir = 0; dir < CORE_DIRECTIONS; dir++)
					/* z is done last because the tracing code can skip z checks. */
					for (z = max(max(wpMins[2] - 1, map_min[2] - 1), 0); z <= z2; y++) {
						new_z = RT_UpdateConnection(clMap, size, x, y, z, dir);
						/* new_z should never be below z. */
						assert(new_z >= z);
						/* Adjust z if this check adjusted multiple cells. */
						z += new_z - z -1;
#ifdef DEBUG
						route_available = qtrue;
#endif
					}
			}
	}

#ifdef DEBUG
	if (!route_available && numTiles > 1)
		Com_Error(ERR_DROP, "CMod_LoadRouting: Map '%s' has NO reachable field", curTile->name);
#endif

	/* calculate new border after merge */
	CMod_GetMapSize();

/*	Com_Printf("route: (%i %i) fall: %i step: %i\n", */
/*		(int)map->route[0][0][0], (int)map->route[1][0][0], (int)map->fall[0][0], (int)map->step[0][0]); */
	Com_Printf("Loaded Routing\n");
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
 * @note The sX and sY values are grid positions - max. grid size is PATHFINDING_WIDTH - unit size is
 * UNIT_SIZE => ends up at 2*MAX_WORLD_WIDTH (the worldplace size - or [-MAX_WORLD_WIDTH, MAX_WORLD_WIDTH])
 * @return The checksum of the maptile
 * @return 0 on error
 * @sa CM_LoadMap
 * @sa R_ModAddMapTile
 */
static unsigned CM_AddMapTile (const char *name, int sX, int sY, byte sZ)
{
	char filename[MAX_QPATH];
	unsigned checksum;
	byte *buf;
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
	length = FS_LoadFile(filename, &buf);
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
	memset(curTile, 0, sizeof(*curTile));
	Q_strncpyz(curTile->name, name, sizeof(curTile->name));

	/* pathfinding and the like must be shifted on the worldplane when we
	 * are assembling a map */
	VectorSet(shift, sX * UNIT_SIZE, sY * UNIT_SIZE, sZ * UNIT_HEIGHT);

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

	CMod_LoadRouting(name, &header.lumps[LUMP_ROUTING], sX, sY, sZ);
	memcpy(&svMap, &clMap, sizeof(svMap));

	FS_FreeFile(buf);

	return checksum;
}


/**
 * @brief Loads in the map and all submodels
 * @note This function loads the collision data from the bsp file. For
 * rendering @c R_ModBeginLoading is used.
 * @sa CM_AddMapTile
 * @sa R_ModBeginLoading
 * @note Make sure that mapchecksum was set to 0 before you call this function
 */
void CM_LoadMap (const char *tiles, const char *pos, unsigned *mapchecksum)
{
	const char *token;
	char name[MAX_VAR];
	char base[MAX_QPATH];
	ipos3_t sh;
	int i;

	Mem_FreePool(com_cmodelSysPool);

	assert(mapchecksum);
	assert(*mapchecksum == 0);

	/* init */
	c_traces = c_brush_traces = numInline = numTiles = 0;
	map_entitystring[0] = base[0] = 0;

	memset(clMap, 0, sizeof(clMap));

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
			if (sh[0] <= -(PATHFINDING_WIDTH / 2) || sh[0] >= PATHFINDING_WIDTH / 2)
				Com_Error(ERR_DROP, "CM_LoadMap: invalid x position given: %i\n", sh[0]);
			if (sh[1] <= -(PATHFINDING_WIDTH / 2) || sh[1] >= PATHFINDING_WIDTH / 2)
				Com_Error(ERR_DROP, "CM_LoadMap: invalid y position given: %i\n", sh[1]);
			if (sh[2] >= PATHFINDING_HEIGHT)
				Com_Error(ERR_DROP, "CM_LoadMap: invalid z position given: %i\n", sh[2]);
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
 * @param[in] name The modelnumber (e.g. "*2") for inline brush models [bmodels]
 * @note Inline bmodels are e.g. the brushes that are assoziated with a func_breakable or func_door
 */
cBspModel_t *CM_InlineModel (const char *name)
{
	int i, num, models;

	/* we only want inline models here */
	if (!name || name[0] != '*')
		Com_Error(ERR_DROP, "CM_InlineModel: bad name: '%s'", name ? name : "");
	/* skip the '*' character and get the inline model number */
	num = atoi(name + 1) - 1;
	if (num < 0 || num >= MAX_MODELS)
		Com_Error(ERR_DROP, "CM_InlineModel: bad number %i - max inline models are %i", num, MAX_MODELS);

	/* search all the loaded tiles for the given inline model */
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
 * @sa G_ClientUseEdict
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
	Grid_DumpWholeMap(&clMap[0]);
	Grid_DumpWholeMap(&clMap[1]);
}


/**
 * @brief  Dumps contents of the entire client map to console for inspection.
 * @sa CL_InitLocal
 */
void Grid_DumpWholeServerMap (void)
{
	Grid_DumpWholeMap(&svMap[0]);
	Grid_DumpWholeMap(&svMap[1]);
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
static qboolean Grid_CheckForbidden (struct routing_s *map, const int actor_size, struct pathing_s *path, int x, int y, int z)
{
	pos_t **p;
	int i, size;
	int fx, fy, fz; /**< Holding variables for the forbidden x and y */
	byte *forbidden_size;

	for (i = 0, p = path->fblist; i < path->fblength / 2; i++, p += 2) {
		/* Skip initial position. */
		if (VectorCompare((*p), exclude_from_forbiddenlist)) {
			/* Com_DPrintf(DEBUG_PATHING, "Grid_CheckForbidden: skipping %i|%i|%i\n", (*p)[0], (*p)[1], (*p)[2]); */
			continue;
		}

		forbidden_size = *(p + 1);
		memcpy(&size, forbidden_size, sizeof(size));
		fx = (*p)[0];
		fy = (*p)[1];
		fz = (*p)[2];

		/* Com_DPrintf(DEBUG_PATHING, "Grid_CheckForbidden: comparing (%i, %i, %i) * %i to (%i, %i, %i) * %i \n", x, y, z, actor_size, fx, fy, fz, size); */

		if (fx + size <= x || x + actor_size <= fx)
			continue; /* x bounds do not intersect */
		if (fy + size <= y || y + actor_size <= fy)
			continue; /* y bounds do not intersect */
		if (z == fz) {
			Com_DPrintf(DEBUG_PATHING, "Grid_CheckForbidden: Collision (%i, %i, %i) * %i and (%i, %i, %i) * %i \n", x, y, z, actor_size, fx, fy, fz, size);
			return qtrue; /* confirmed intersection */
		}
	}
	return qfalse;
}

/**
 * @param[in] map Pointer to client or server side routing table (clMap, svMap)
 * @param[in] actor_size Give the field size of the actor (e.g. for 2x2 units) to check linked fields as well.
 * @param[in,out] path Pointer to client or server side pathing table (clMap, svMap)
 * @param[in] pos Current location in the map.
 * @param[in] crouching_state Whether the actor is currently crouching, 1 is yes, 0 is no.
 * @param[in] dir Direction vector index (see DIRECTIONS and dvecs)
 * @param[in,out] pqueue Priority queue (heap) to insert the now reached tiles for reconsidering
 * @sa Grid_CheckForbidden
 * @todo Add height/fall checks for actor size (2x2).
 */
void Grid_MoveMark (struct routing_s *map, const int actor_size, struct pathing_s *path, pos3_t pos, int crouching_state, const int dir, priorityQueue_t *pqueue)
{
	int x, y, z;
	byte ol;
	int nx, ny, nz;
	int dx, dy, dz;
	pos4_t dummy;
	byte l;
	qboolean flier = qfalse; /**< This can be keyed into whether an actor can fly or not to allow flying */
	qboolean has_ladder_support = qfalse; /**< Indicates if there is a ladder present providing support. */
	qboolean has_ladder_climb = qfalse; /**< Indicates if there is a ladder present providing ability to climb. */
	int actor_height, passage_height, height_change, stepup_height, falling_height;
	int core_dir;

	/* Ensure that dir is in bounds. */
	if (dir < 0 || dir >= PATHFINDING_DIRECTIONS)
		return;

	/* Directions 12, 14, and 15 are currently undefined. */
	if (dir == 12 || dir == 14 || dir == 15)
		return;

	/* IMPORTANT: only fliers can use directions higher than NON_FLYING_DIRECTIONS. */
	if (!flier && dir >= FLYING_DIRECTIONS) {
		Com_DPrintf(DEBUG_PATHING, "Grid_MoveMark: Non-fliers can't fly.\n");
		return;
	}

	x = pos[0];
	y = pos[1];
	z = pos[2];

	RT_AREA_TEST(path, x, y, z, crouching_state);
	ol = RT_AREA(path, x, y, z, crouching_state);

	Com_DPrintf(DEBUG_PATHING, "Grid_MoveMark: (%i %i %i) s:%i dir:%i c:%i ol:%i\n", x, y, z, actor_size, dir, crouching_state, ol);


	/* We cannot fly and crouch at the same time. This will also cause an actor to stand to fly. */
	if (crouching_state && dir >= FLYING_DIRECTIONS) {
		Com_DPrintf(DEBUG_PATHING, "Grid_MoveMark: Can't fly while crouching.\n");
		return;
	}

	if (ol >= MAX_MOVELENGTH && ol != ROUTING_NOT_REACHABLE) {
		Com_DPrintf(DEBUG_PATHING, "Grid_MoveMark: Exiting because the TUS needed to move here are already too large. %i %i\n", ol , MAX_MOVELENGTH);
		return;
	}

#ifdef PARANOID
	if (z >= PATHFINDING_HEIGHT) {
		Com_DPrintf(DEBUG_PATHING, "Grid_MoveMark: WARNING z = %i(>= HEIGHT %i)\n", z, PATHFINDING_HEIGHT);
		return;
	}
#endif

	/* Find the number of TUs used to move in this direction. */
	l = TUs_used[dir];

	/* If crouching then multiply by the crouching factor. */
	if (crouching_state == 1)
		l *= TU_CROUCH_MOVING_FACTOR;

	/* Now add the TUs needed to get to the originating cell. */
	l += ol;

	/* If this is a crouching or crouching move, then process that motion. */
	if (dir == DIRECTION_STAND_UP || dir == DIRECTION_CROUCH) {
		/* Can't stand up if standing. */
		if (dvecs[dir][3] > 0 && crouching_state == 0) {
			Com_DPrintf(DEBUG_PATHING, "Grid_MoveMark: Can't stand while standing.\n");
			return;
		}
		/* Can't get down if crouching. */
		if (dvecs[dir][3] < 0 && crouching_state == 1) {
			Com_DPrintf(DEBUG_PATHING, "Grid_MoveMark: Can't crouch while crouching.\n");
			return;
		}

		/* Since we can toggle crouching, then do so. */
		crouching_state ^= 1;

		/* Is this a better move into this cell? */
		RT_AREA_TEST(path, x, y, z, crouching_state);
		if (RT_AREA(path, x, y, z, crouching_state) <= l) {
			Com_DPrintf(DEBUG_PATHING, "Grid_MoveMark: Toggling crouch is not optimum. %i %i\n", RT_AREA(path, x, y, z, crouching_state), l);
			return;
		}

		/* Store move. */
		RT_AREA(path, x, y, z, crouching_state) = l;	/**< Store TUs for this square. */
		RT_AREA_FROM(path, x, y, z, crouching_state) = z | (dir << 3); /**< Store origination information for this square. */
		Vector4Set(dummy, x, y, z, crouching_state);
		PQueuePush(pqueue, dummy, l); /** @todo add heuristic for A* algorithm */
		Com_DPrintf(DEBUG_PATHING, "Grid_MoveMark: Set move to (%i %i %i) c:%i to %i.\n", x, y, z, crouching_state, l);
		/* We are done, exit now. */
		return;
	}

	dx = dvecs[dir][0];	/**< Get the difference value for x for this direction. (can be pos or neg) */
	dy = dvecs[dir][1];	/**< Get the difference value for y for this direction. (can be pos or neg) */
	dz = dvecs[dir][2];	/**< Get the difference value for z for this direction. (can be pos or neg) */
	nx = x + dx;		/**< "new" x value = starting x value + difference from choosen direction */
	ny = y + dy;		/**< "new" y value = starting y value + difference from choosen direction */
	nz = z + dz;		/**< "new" z value = starting z value + difference from choosen direction */

	/* Connection checks.  If we cannot move in the desired direction, then bail. */
	/* Range check of new values (all sizes) */
	if (nx < 0 || nx > PATHFINDING_WIDTH - actor_size
		|| ny < 0 || ny > PATHFINDING_WIDTH - actor_size
		|| nz < 0 || nz > PATHFINDING_HEIGHT) {
		return;
		}

	/* This value is worthless if it is CORE_DIRECTIONS through FLYING_DIRECTIONS:
	 * these are actions or climbing.
	 */
	core_dir = dir % CORE_DIRECTIONS;

	/* If there is no passageway (or rather lack of a wall) to the desired cell, then return. */
	/** @todo actor_height is currently the height of a standing 1x1 actor.  This needs to be adjusted
	 *  to the actor's actual height, including crouching. */
	/* If the flier is moving up or down diagonally, then passage height will also adjust */
	if (dir >= FLYING_DIRECTIONS){
		if (dz > 0) {
			/* If the actor is moving up, check the passage at the current cell.
			 * The minimum height is the actor's height plus the distance from the current floor to the top of the cell. */
			actor_height = (UNIT_HEIGHT + PLAYER_HEIGHT) / QUANT - max(0, RT_FLOOR(map, actor_size, x, y, z));
			RT_CONN_TEST(map, actor_size, x, y, z, core_dir);
			passage_height = RT_CONN(map, actor_size, x, y, z, core_dir);
		} else {
			/* If the actor is moving down, check from the destination cell back. *
			 * The minimum height is the actor's height plus the distance from the destination floor to the top of the cell. */
			actor_height = (UNIT_HEIGHT + PLAYER_HEIGHT) / QUANT - max(0, RT_FLOOR(map, actor_size, nx, ny, nz));
			RT_CONN_TEST(map, actor_size, nx, ny, nz, core_dir ^ 1);
			passage_height = RT_CONN(map, actor_size, nx, ny, nz, core_dir ^ 1);
		}
		if (passage_height < actor_height) {
			Com_DPrintf(DEBUG_PATHING, "Grid_MoveMark: Passage is not tall enough. passage:%i actor:%i\n", passage_height, actor_height);
			return;
		}
	} else if (dir < CORE_DIRECTIONS) {
		/* This is the standard passage height for all units trying to move horizontally. */
		actor_height = PLAYER_HEIGHT / QUANT;
		RT_CONN_TEST(map, actor_size, x, y, z, core_dir);
		passage_height = RT_CONN(map, actor_size, x, y, z, core_dir);
		if (passage_height < actor_height) {
			Com_DPrintf(DEBUG_PATHING, "Grid_MoveMark: Passage is not tall enough. passage:%i actor:%i\n", passage_height, actor_height);
			return;
		}
	}
	/* else there is no movement that uses passages. */


	/* If we are moving horizontally, get the height difference of the floors. */
	if (dir < CORE_DIRECTIONS)
		height_change = RT_FLOOR(map, actor_size, nx, ny, nz) - RT_FLOOR(map, actor_size, x, y, z);
	/* If we are falling, the height difference is the floor value. */
	if (dir == DIRECTION_FALL)
		height_change = RT_FLOOR(map, actor_size, x, y, z);

	if (!flier) {
		/* If the destination cell is higher than this actor can walk, then return.
		 * Fliers will ignore this rule.  They only need the passage to exist.
		 */
		/** @todo stepup_height should be replaced with an arbitrary max stepup height based on the actor. */
		stepup_height = PATHFINDING_MIN_STEPUP;
		if (height_change > stepup_height) {
			Com_DPrintf(DEBUG_PATHING, "Grid_MoveMark: Can't step up high enough. change:%i stepup:%i\n", height_change, stepup_height);
			return;
		}

		/* If the actor cannot fly and tries to fall more than falling_height, then prohibit the move. */
		/** @todo falling_height should be replaced with an arbitrary max falling height based on the actor. */
		falling_height = PATHFINDING_MAX_FALL;
		/** @todo has_ladder_support should return true if
		 *  1) There is a ladder in the new cell in the specified direction or
		 *  2) There is a ladder in any direction in the cell below the new cell and no ladder in the new cell itself.
		 */
		has_ladder_support = qfalse;
		if (height_change < -falling_height && !has_ladder_support) {
			Com_DPrintf(DEBUG_PATHING, "Grid_MoveMark: Too far a drop without a ladder. change:%i maxfall:%i\n", height_change, -falling_height);
			return;
		}

		/* The actor can't fall it there is ladder support. */
		if (dir == DIRECTION_FALL && has_ladder_support){
			Com_DPrintf(DEBUG_PATHING, "Grid_MoveMark: Can't fall because of ladder.\n");
			return;
		}

		/** @todo has_ladder_climb should return true if
		 *  1) There is a ladder in the new cell in the specified direction.
		 */
		has_ladder_climb = qfalse;
		/* If the actor is not a flyer and tries to move up, there must be a ladder. */
		if (dir == DIRECTION_CLIMB_UP && !has_ladder_climb) {
			Com_DPrintf(DEBUG_PATHING, "Grid_MoveMark: Can't climb up without a ladder.\n");
			return;
		}

		/* If the actor is not a flyer and tries to move down, there must be a ladder. */
		if (dir == DIRECTION_CLIMB_DOWN && !has_ladder_climb) {
			Com_DPrintf(DEBUG_PATHING, "Grid_MoveMark: Can't climb down without a ladder.\n");
			return;
		}

		/* If we are walking normally, we cannot fall a distance further than stepup_height, so we initiate a fall:
		 * Set height_change to 0.
		 * The actor enters the cell.
		 * The actor will be forced to fall (dir 13) from the destination cell to the cell below.
		 */
		if (dir < CORE_DIRECTIONS && height_change < -stepup_height) {
			/* We cannot fall if there is an entity below the cell we want to move to. */
			if (Grid_CheckForbidden(map, actor_size, path, nx, ny, nz - 1)) {
				Com_DPrintf(DEBUG_PATHING, "Grid_MoveMark: The fall destination is occupied.\n");
				return;
			}
			Com_DPrintf(DEBUG_PATHING, "Grid_MoveMark: Preparing for a fall. change:%i fall:%i\n", height_change, -stepup_height);
			height_change = 0;
		}

		/* We cannot fall if there is a floor in this cell. */
		if (dir == DIRECTION_FALL && RT_FLOOR(map, actor_size, x, y, z) >= 0) {
			Com_DPrintf(DEBUG_PATHING, "Grid_MoveMark: Can't fall while supported. floor:%i\n", RT_FLOOR(map, actor_size, x, y, z));
			return;
		}
	} else {
		/* Fliers cannot fall intentionally. */
		if (dir == DIRECTION_FALL) {
			Com_DPrintf(DEBUG_PATHING, "Grid_MoveMark: Fliers can't fall.\n");
			return;
		}
		/* This is the last check for fliers. All passages are OK if we are here, but the actor
		 * might be moving straight up or down.  Ensure there is an opening for this actor to
		 * move in the desired direction. */
		if (dir == DIRECTION_CLIMB_UP && RT_CEILING(map, actor_size, x, y, z) * QUANT < UNIT_HEIGHT * 2 - PLAYER_HEIGHT) { /* up */
			Com_DPrintf(DEBUG_PATHING, "Grid_MoveMark: Not enough headroom to fly up. passage:%i actor:%i\n", RT_CEILING(map, actor_size, x, y, z) * QUANT, UNIT_HEIGHT * 2 - PLAYER_HEIGHT);
			return;
		}
		if (dir == DIRECTION_CLIMB_DOWN && RT_FLOOR(map, actor_size, x, y, z) >= 0 ) { /* down */
			Com_DPrintf(DEBUG_PATHING, "Grid_MoveMark: Can't fly down through a floor. floor:%i\n", RT_FLOOR(map, actor_size, x, y, z));
			return;
		}
	}

	/* OK, at this point we are certain of a few things:
	 * There is not a wall obstructing access to the destination cell.
	 * If the actor is not a flier, the actor will not rise more than stepup_height or fall more than
	 *    falling_height, unless climbing.
	 *
	 * If the actor is a flier, as long as there is a passage, it can be moved through.
	 * There are no floor difference restrictions for fliers, only obstructions.
	 */

	/* If we are moving horizontally, the new z coordinate may need to be adjusted from stepup. */
	if (dir < CORE_DIRECTIONS && abs(height_change) <= stepup_height) {
		const int new_floor = RT_FLOOR(map, actor_size, x, y, z) + height_change;
		/** @note offset by 15 if negative to force nz down */
		const int delta = new_floor < 0 ? (new_floor - 15) / 16 : new_floor / 16;
		/* Com_DPrintf(DEBUG_PATHING, "Grid_MoveMark: Adjusting hight. floor:%i new_floor:%i delta:%i\n", RT_FLOOR(map, actor_size, x, y, z), new_floor, delta); */
		nz += delta;
	}
	/* nz can't move outof bounds */
	if (nz < 0)
		nz = 0;
	if (nz >= PATHFINDING_HEIGHT)
		nz = PATHFINDING_HEIGHT - 1;

	/* Is this a better move into this cell? */
	RT_AREA_TEST(path, nx, ny, nz, crouching_state);
	if (RT_AREA(path, nx, ny, nz, crouching_state) <= l) {
		Com_DPrintf(DEBUG_PATHING, "Grid_MoveMark: This move is not optimum. %i %i\n", RT_AREA(path, nx, ny, nz, crouching_state), l);
		return;
	}

	/* Test for forbidden (by other entities) areas. */
	if (Grid_CheckForbidden(map, actor_size, path, nx, ny, nz)) {
		Com_DPrintf(DEBUG_PATHING, "Grid_MoveMark: That spot is occupied.\n");
		return;
	}

	/* Store move. */
	if (pqueue) {
		RT_AREA(path, nx, ny, nz, crouching_state) = l;	/**< Store TUs for this square. */
		RT_AREA_FROM(path, nx, ny, nz, crouching_state) = z | (dir << 3); /**< Store origination information for this square. */
		Vector4Set(dummy, nx, ny, nz, crouching_state);
		/** @todo add heuristic for A* algorithm */
		PQueuePush(pqueue, dummy, l);
	}
	Com_DPrintf(DEBUG_PATHING, "Grid_MoveMark: Set move to (%i %i %i) c:%i to %i. srcfloor:%i change:%i\n", nx, ny, nz, crouching_state, l, RT_FLOOR(map, actor_size, x, y, z), height_change);
}

/**
 * @param[in] map Pointer to client or server side routing table (clMap, svMap)
 * @param[in] actor_size The size of thing to calc the move for (e.g. size=2 means 2x2).
 * The plan is to have the 'origin' in 2x2 units in the bottom-left (towards the lower coordinates) corner of the 2x2 square.
 * @param[in,out] path Pointer to client or server side pathing table (clMap, svMap)
 * @param[in] from The position to start the calculation from.
 * @param[in] distance to calculate move-information for - currently unused
 * @param[in] crouching_state Whether the actor is currently crouching, 1 is yes, 0 is no.
 * @param[in] fb_list Forbidden list (entities are standing at those points)
 * @param[in] fb_length Length of forbidden list
 * @sa Grid_MoveMark
 * @sa G_MoveCalc
 * @sa CL_ConditionalMoveCalc
 */
void Grid_MoveCalc (struct routing_s *map, const int actor_size, struct pathing_s *path, pos3_t from, int crouching_state, int distance, byte ** fb_list, int fb_length)
{
	int dir;
	int count;
	priorityQueue_t pqueue;
	pos4_t epos; /**< Extended position; includes crouching state */
	pos3_t pos;

	/* reset move data */
	/* ROUTING_NOT_REACHABLE means, not reachable */
	memset(path->area, ROUTING_NOT_REACHABLE, PATHFINDING_WIDTH * PATHFINDING_WIDTH * PATHFINDING_HEIGHT * ACTOR_MAX_STATES);
	path->fblist = fb_list;
	path->fblength = fb_length;

	if (distance > MAX_ROUTE)
		distance = MAX_ROUTE;

	VectorCopy(from, exclude_from_forbiddenlist); /**< Prepare exclusion of starting-location (i.e. this should be ent-pos or le-pos) in Grid_CheckForbidden */

	PQueueInitialise(&pqueue, 1024);
	Vector4Set(epos, from[0], from[1], from[2], crouching_state);
	PQueuePush(&pqueue, epos, 0);
	RT_AREA_TEST(path, from[0], from[1], from[2], crouching_state);
	RT_AREA(path, from[0], from[1], from[2], crouching_state) = 0;

	Com_DPrintf(DEBUG_PATHING, "Grid_MoveCalc: Start at (%i %i %i) c:%i\n", from[0], from[1], from[2], crouching_state);

	count = 0;
	while (!PQueueIsEmpty(&pqueue)) {
		PQueuePop(&pqueue, epos);
		VectorCopy(epos, pos);
		count++;

		/* for A*
		if pos = goal
			return pos
		*/
		for (dir = 0; dir < PATHFINDING_DIRECTIONS; dir++) {
			Grid_MoveMark(map, actor_size, path, pos, epos[3], dir, &pqueue);
		}

	}
	/* Com_Printf("Loop: %i", count); */
	PQueueFree(&pqueue);

	Com_DPrintf(DEBUG_PATHING, "Grid_MoveCalc: Done\n\n");
}

/**
 * @brief Caches the calculated move
 * @param[in] map Routing data
 * @sa AI_ActorThink
 */
void Grid_MoveStore (struct pathing_s *path)
{
	memcpy(path->areaStored, path->area, sizeof(path->areaStored));
}


/**
 * @brief Return the needed TUs to walk to a given position
 * @param[in] map Routing data
 * @param[in] to Position to walk to
 * @param[in] stored Use the stored mask (the cached move) of the routing data
 * @return ROUTING_NOT_REACHABLE if the move isn't possible
 * @return length of move otherwise (TUs)
 */
pos_t Grid_MoveLength (struct pathing_s *path, pos3_t to, int crouching_state, qboolean stored)
{
#ifdef PARANOID
	if (to[2] >= PATHFINDING_HEIGHT) {
		Com_DPrintf(DEBUG_PATHING, "Grid_MoveLength: WARNING to[2] = %i(>= HEIGHT)\n", to[2]);
		return ROUTING_NOT_REACHABLE;
	}
#endif
	RT_AREA_TEST(path, to[0], to[1], to[2], crouching_state);

	if (!stored)
		return RT_AREA(path, to[0], to[1], to[2], crouching_state);
	else
		return RT_SAREA(path, to[0], to[1], to[2], crouching_state);
}


/**
 * @brief The next stored move direction
 * @param[in] map Pointer to client or server side routing table (clMap, svMap)
 * @param[in] from
 * @return (Guess: a direction index (see dvecs and DIRECTIONS))
 * @sa Grid_MoveCheck
 */
int Grid_MoveNext (struct routing_s *map, const int actor_size, struct pathing_s *path, pos3_t from, int crouching_state)
{
	const pos_t l = RT_AREA(path, from[0], from[1], from[2], crouching_state); /**< Get TUs for this square */

	/* Check to see if the TUs needed to move here are greater than 0 and less then ROUTING_NOT_REACHABLE */
	if (!l && l != ROUTING_NOT_REACHABLE) {
		/* ROUTING_UNREACHABLE means, not possible/reachable */
		return ROUTING_UNREACHABLE;
	}

	/* Return the information indicating how the actor got to this cell */
	return RT_AREA_FROM(path, from[0], from[1], from[2], crouching_state);
}


/**
 * @brief Returns the height of the floor in a cell.
 * @param[in] map Pointer to client or server side routing table (clMap, svMap)
 * @param[in] pos Position in the map to check the height
 * @return The actual model height of the cell's ceiling.
 */
unsigned int Grid_Ceiling (struct routing_s *map, const int actor_size, const pos3_t pos)
{
	/* max 8 levels */
	if (pos[2] >= PATHFINDING_HEIGHT) {
		Com_Printf("Grid_Height: Warning: z level is bigger than %i: %i\n",
			(PATHFINDING_HEIGHT - 1), pos[2]);
	}
	return RT_CEILING(map, actor_size, pos[0], pos[1], pos[2] & 7) * QUANT;
}


/**
 * @brief Returns the height of the floor in a cell.
 * @param[in] map Pointer to client or server side routing table (clMap, svMap)
 * @param[in] pos Position in the map to check the height
 * @return The actual model height of the cell's ceiling.
 */
int Grid_Height (struct routing_s *map, const int actor_size, const pos3_t pos)
{
	/* max 8 levels */
	if (pos[2] >= PATHFINDING_HEIGHT) {
		Com_Printf("Grid_Height: Warning: z level is bigger than %i: %i\n",
			(PATHFINDING_HEIGHT - 1), pos[2]);
	}
	return (RT_CEILING(map, actor_size, pos[0], pos[1], pos[2] & 7) - RT_FLOOR(map, actor_size, pos[0], pos[1], pos[2] & 7)) * QUANT;
}


/**
 * @brief Returns the height of the floor in a cell.
 * @param[in] map Pointer to client or server side routing table (clMap, svMap)
 * @param[in] pos Position in the map to check the height
 * @return The actual model height of the cell's floor.
 */
int Grid_Floor (struct routing_s *map, const int actor_size, const pos3_t pos)
{
	/* max 8 levels */
	if (pos[2] >= PATHFINDING_HEIGHT) {
		Com_Printf("Grid_Floor: Warning: z level is bigger than %i: %i\n",
			(PATHFINDING_HEIGHT - 1), pos[2]);
	}
	return RT_FLOOR(map, actor_size, pos[0], pos[1], pos[2] & 7) * QUANT;
}


/**
 * @brief Returns the maximum height of an obstruction that an actor can travel over.
 * @param[in] map Pointer to client or server side routing table (clMap, svMap)
 * @param[in] pos Position in the map to check the height
 * @return The actual model height increse allowed to move into an adjacent cell.
 */
pos_t Grid_StepUp (struct routing_s *map, const int actor_size, const pos3_t pos)
{
	/* max 8 levels */
	if (pos[2] >= PATHFINDING_HEIGHT) {
		Com_Printf("Grid_StepUp: Warning: z level is bigger than 7: %i\n", pos[2]);
	}
	return PATHFINDING_MIN_STEPUP;
}


/**
 * @brief Returns the maximum height of an obstruction that an actor can travel over.
 * @param[in] map Pointer to client or server side routing table (clMap, svMap)
 * @param[in] pos Position in the map to check the height
 * @return The actual model height increse allowed to move into an adjacent cell.
 */
int Grid_TUsUsed (int dir)
{
	assert(dir >= 0 && dir <PATHFINDING_DIRECTIONS);
	return TUs_used[dir];
}


/**
 * @brief Returns non-zero if the cell is filled (solid) and cannot be entered.
 * @param[in] map Pointer to client or server side routing table (clMap, svMap)
 * @param[in] pos Position in the map to check for filling
 * @return 0 if the cell is vacant (of the world model), non-zero otherwise.
 */
int Grid_Filled (struct routing_s *map, const int actor_size, pos3_t pos)
{
	/* max 8 levels */
	if (pos[2] >= PATHFINDING_HEIGHT) {
		const int maxHeight = PATHFINDING_HEIGHT - 1;
		Com_Printf("Grid_Filled: Warning: z level is bigger than %i: %i\n", pos[2], maxHeight);
		pos[2] &= maxHeight;
	}
	return RT_FILLED(map, pos[0], pos[1], pos[2], actor_size);
}


/**
 * @brief Calculated the new height level when something falls down from a certain position.
 * @param[in] map Pointer to client or server side routing table (clMap, svMap)
 * @param[in] pos Position in the map to start the fall (starting height is the z-value in this position)
 * @param[in] actor_size Give the field size of the actor (e.g. for 2x2 units) to check linked fields as well.
 * @return New z (height) value.
 * @return 0xFF if an error occured.
 */
pos_t Grid_Fall (struct routing_s *map, const int actor_size, pos3_t pos)
{
	int z = pos[2], base, diff;
	qboolean flier = qfalse; /** @todo if an actor can fly, then set this to true. */

	/* Is z off the map? */
	if (z >= PATHFINDING_HEIGHT) {
		Com_DPrintf(DEBUG_PATHING, "Grid_Fall: z (height) out of bounds): z=%i max=%i\n", z, PATHFINDING_HEIGHT);
		return 0xFF;
	}

	/* If we can fly, then obviously we won't fall. */
	if (flier)
		return z;

    /*
     * Easy math- get the floor, integer divide by 16, add to z.
     * If z < 0, we are going down.
     * If z >= 16, we are going up.
     * If 0 <= z <= 16, then z / 16 = 0, no change.
     */
	base = RT_FLOOR(map, actor_size, pos[0], pos[1], z);
    diff = base < 0 ? (base - 15) / 16 : base / 16;
    z += diff;
    assert(z >= 0 && z < PATHFINDING_HEIGHT);
	return z;
}

/**
 * @brief Converts a grid position to world coordinates
 * @sa Grid_Height
 * @param[in] map The routing map
 * @param[in] pos The grid position
 * @param[out] vec The world vector
 */
void Grid_PosToVec (struct routing_s *map, const int actor_size, pos3_t pos, vec3_t vec)
{
	SizedPosToVec(pos, actor_size, vec);
#ifdef PARANOID
	if (pos[2] >= PATHFINDING_HEIGHT)
		Com_Printf("Grid_PosToVec: Warning - z level bigger than 7 (%i - source: %.02f)\n", pos[2], vec[2]);
#endif
    /* Clamp the floor value between 0 and UNIT_HEIGHT */
	vec[2] += max(0, min(UNIT_HEIGHT, Grid_Floor(map, actor_size, pos)));
}

/**
 * @sa CM_InlineModel
 * @sa CM_CheckUnit
 * @sa CM_UpdateConnection
 * @sa CMod_LoadSubmodels
 * @param[in] map The routing map (either server or client map)
 * @param[in] name Name of the inline model to compute the mins/maxs for
 * @param[in] list The local models list (a local model has a name starting with * followed by the model number)
 */

void Grid_RecalcRouting (struct routing_s *map, const char *name, const char **list)
{
	cBspModel_t *model;
	pos3_t min, max;
	int x, y, z, actor_size, dir;
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
	for (actor_size = 1; actor_size <= ACTOR_MAX_SIZE; actor_size++)
		for (y = min[1]; y < max[1] - actor_size; y++)
			for (x = min[0]; x < max[0] - actor_size; x++)
				for (z = max[2]; z >= min[2]; z--)
					z = RT_CheckCell(map, actor_size, x, y, z);

#if 0
	Grid_DumpMap(map, (int)min[0], (int)min[1], (int)min[2], (int)max[0], (int)max[1], (int)max[2]);
#endif

	/* check connections */
	for (actor_size = 1; actor_size <= ACTOR_MAX_SIZE; actor_size++)
		for (y = min[1]; y < max[1] - actor_size; y++)
			for (x = min[0]; x < max[0] - actor_size; x++)
				for (dir = 0; dir < CORE_DIRECTIONS; dir ++)
					for (z = min[2]; z < max[2]; z++)
						z = RT_UpdateConnection(map, actor_size, x, y, z, dir);

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
 * and if we substitute back into our equation for v and we get
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
 * @param[out] v0 The velocity vector
 */
float Com_GrenadeTarget (const vec3_t from, const vec3_t at, float speed, qboolean launched, qboolean rolled, vec3_t v0)
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
