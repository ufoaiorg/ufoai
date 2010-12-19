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
#include "../shared/parse.h"

/** @note holds all entity data as a single parsable string */
static char mapEntityString[MAX_MAP_ENTSTRING];

/**
 * @note The vectors are from 0 up to 2*MAX_WORLD_WIDTH - but not negative
 * @note holds the smallest bounding box that will contain the map
 * @sa CL_ClampCamToMap
 * @sa CL_OutsideMap
 * @sa CMod_GetMapSize
 * @sa SV_ClearWorld
 */
vec3_t mapMin, mapMax;

/** @brief server and client routing table */
routing_t svMap[ACTOR_MAX_SIZE], clMap[ACTOR_MAX_SIZE]; /**< A routing_t per size */
pathing_t svPathMap; /**< This is where the data for TUS used to move and actor locations go */

/** @note holds the number of inline entities, e.g. ET_DOOR */
static int numInline;

/** @note a list with all inline models (like func_breakable) */
static const char **inlineList;

/** @note a pointer to the bsp file model data */
static byte *cModelBase;

/** @note this is the position of the current actor- so the actor can stand in the cell it is in when pathfinding */
static pos3_t excludeFromForbiddenList;

/** @note this is a zeroed surface structure */
static cBspSurface_t nullSurface;

/** @note these are the TUs used to intentionally move in a given direction.  Falling not included. */
static const int TUsUsed[] = {
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
	TU_MOVE_STRAIGHT * TU_FLYING_MOVING_FACTOR, /* FLY LEVEL & E  */
	TU_MOVE_STRAIGHT * TU_FLYING_MOVING_FACTOR, /* FLY LEVEL & W  */
	TU_MOVE_STRAIGHT * TU_FLYING_MOVING_FACTOR, /* FLY LEVEL & N  */
	TU_MOVE_STRAIGHT * TU_FLYING_MOVING_FACTOR, /* FLY LEVEL & S  */
	TU_MOVE_DIAGONAL * TU_FLYING_MOVING_FACTOR, /* FLY LEVEL & NE */
	TU_MOVE_DIAGONAL * TU_FLYING_MOVING_FACTOR, /* FLY LEVEL & SW */
	TU_MOVE_DIAGONAL * TU_FLYING_MOVING_FACTOR, /* FLY LEVEL & NW */
	TU_MOVE_DIAGONAL * TU_FLYING_MOVING_FACTOR, /* FLY LEVEL & SE */
	TU_MOVE_STRAIGHT * TU_FLYING_MOVING_FACTOR, /* FLY DOWN & E  */
	TU_MOVE_STRAIGHT * TU_FLYING_MOVING_FACTOR, /* FLY DOWN & W  */
	TU_MOVE_STRAIGHT * TU_FLYING_MOVING_FACTOR, /* FLY DOWN & N  */
	TU_MOVE_STRAIGHT * TU_FLYING_MOVING_FACTOR, /* FLY DOWN & S  */
	TU_MOVE_DIAGONAL * TU_FLYING_MOVING_FACTOR, /* FLY DOWN & NE */
	TU_MOVE_DIAGONAL * TU_FLYING_MOVING_FACTOR, /* FLY DOWN & SW */
	TU_MOVE_DIAGONAL * TU_FLYING_MOVING_FACTOR, /* FLY DOWN & NW */
	TU_MOVE_DIAGONAL * TU_FLYING_MOVING_FACTOR  /* FLY DOWN & SE */
};
CASSERT(lengthof(TUsUsed) == PATHFINDING_DIRECTIONS);

/** @brief Used to track where rerouting needs to occur. */
static byte reroute[ACTOR_MAX_SIZE][PATHFINDING_WIDTH][PATHFINDING_WIDTH];

static void CM_MakeTracingNodes(void);
static void CM_InitBoxHull(void);

static void CM_SetInlineList (const char **list)
{
	inlineList = list;
}

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
static void CMod_LoadSubmodels (const lump_t * l, const vec3_t shift)
{
	const dBspModel_t *in;
	cBspModel_t *out;
	int i, j, count;

	if (!l)
		Com_Error(ERR_DROP, "CMod_LoadSubmodels: No lump given");

	in = (const void *) (cModelBase + l->fileofs);
	if (l->filelen % sizeof(*in))
		Com_Error(ERR_DROP, "CMod_LoadSubmodels: funny lump size (%i => "UFO_SIZE_T")", l->filelen, sizeof(*in));
	count = l->filelen / sizeof(*in);
	Com_DPrintf(DEBUG_ENGINE, COLORED_GREEN "...submodels: %i\n", count);

	if (count < 1)
		Com_Error(ERR_DROP, "Map with no models");
	if (count > MAX_MAP_MODELS)
		Com_Error(ERR_DROP, "Map has too many models: %i", count);

	out = Mem_PoolAlloc((count + 6) * sizeof(*out), com_cmodelSysPool, 0);
	curTile->models = out;
	curTile->nummodels = count;

	for (i = 0; i < count; i++, in++, out++) {
		out = &curTile->models[i];

		/* Record the shift in case we need to undo it. */
		VectorCopy(shift, out->shift);
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
 * @param[in] l descriptor of the data block we are working on
 * @sa CM_AddMapTile
 */
static void CMod_LoadSurfaces (const lump_t * l)
{
	const dBspTexinfo_t *in;
	cBspSurface_t *out;
	int i, count;

	if (!l)
		Com_Error(ERR_DROP, "CMod_LoadSurfaces: No lump given");

	in = (const void *) (cModelBase + l->fileofs);
	if (l->filelen % sizeof(*in))
		Com_Error(ERR_DROP, "CMod_LoadSurfaces: funny lump size: %i", l->filelen);
	count = l->filelen / sizeof(*in);
	Com_DPrintf(DEBUG_ENGINE, COLORED_GREEN "...surfaces: %i\n", count);

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
 * @param[in] l descriptor of the data block we are working on
 * @param[in] shift The shifting vector in case this is a map assemble
 * @sa CM_AddMapTile
 * @sa TR_BuildTracingNode_r
 */
static void CMod_LoadNodes (const lump_t * l, const vec3_t shift)
{
	const dBspNode_t *in;
	int child;
	cBspNode_t *out;
	int i, j, count;

	if (!l)
		Com_Error(ERR_DROP, "CMod_LoadNodes: No lump given");

	in = (const void *) (cModelBase + l->fileofs);
	if (l->filelen % sizeof(*in))
		Com_Error(ERR_DROP, "CMod_LoadNodes: funny lump size: %i", l->filelen);
	count = l->filelen / sizeof(*in);
	Com_DPrintf(DEBUG_ENGINE, COLORED_GREEN "...nodes: %i\n", count);

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
 * @param[in] l descriptor of the data block we are working on
 * @sa CM_AddMapTile
 */
static void CMod_LoadBrushes (const lump_t * l)
{
	const dBspBrush_t *in;
	cBspBrush_t *out;
	int i, count;

	if (!l)
		Com_Error(ERR_DROP, "CMod_LoadBrushes: No lump given");

	in = (const void *) (cModelBase + l->fileofs);
	if (l->filelen % sizeof(*in))
		Com_Error(ERR_DROP, "CMod_LoadBrushes: funny lump size: %i", l->filelen);
	count = l->filelen / sizeof(*in);
	Com_DPrintf(DEBUG_ENGINE, COLORED_GREEN "...brushes: %i\n", count);

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
 * @param[in] l descriptor of the data block we are working on
 * @sa CM_AddMapTile
 */
static void CMod_LoadLeafs (const lump_t * l)
{
	int i;
	cBspLeaf_t *out;
	const dBspLeaf_t *in;
	int count;

	if (!l)
		Com_Error(ERR_DROP, "CMod_LoadLeafs: No lump given");

	in = (const void *) (cModelBase + l->fileofs);
	if (l->filelen % sizeof(*in))
		Com_Error(ERR_DROP, "CMod_LoadLeafs: funny lump size: %i", l->filelen);
	count = l->filelen / sizeof(*in);
	Com_DPrintf(DEBUG_ENGINE, COLORED_GREEN "...leafs: %i\n", count);

	if (count < 1)
		Com_Error(ERR_DROP, "Map with no leafs");
	/* need to save space for box planes */
	if (count > MAX_MAP_LEAFS)
		Com_Error(ERR_DROP, "Map has too many leafs");

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
 * @param[in] l descriptor of the data block we are working on
 * @param[in] shift The shifting vector in case this is a map assemble
 * @sa CM_AddMapTile
 * @sa R_ModLoadPlanes
 */
static void CMod_LoadPlanes (const lump_t * l, const vec3_t shift)
{
	int i, j;
	cBspPlane_t *out;
	const dBspPlane_t *in;
	int count;

	if (!l)
		Com_Error(ERR_DROP, "CMod_LoadPlanes: No lump given");

	in = (const void *) (cModelBase + l->fileofs);
	if (l->filelen % sizeof(*in))
		Com_Error(ERR_DROP, "CMod_LoadPlanes: funny lump size: %i", l->filelen);
	count = l->filelen / sizeof(*in);
	Com_DPrintf(DEBUG_ENGINE, COLORED_GREEN "...planes: %i\n", count);

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
		out->dist = LittleFloat(in->dist);
		out->type = LittleLong(in->type);

		/* load normals and shift (map assembly) */
		for (j = 0; j < 3; j++) {
			out->normal[j] = LittleFloat(in->normal[j]);
			out->dist += out->normal[j] * shift[j];
		}
	}
}

/**
 * @param[in] l descriptor of the data block we are working on
 * @sa CM_AddMapTile
 */
static void CMod_LoadLeafBrushes (const lump_t * l)
{
	int i;
	unsigned short *out;
	const unsigned short *in;
	int count;

	if (!l)
		Com_Error(ERR_DROP, "CMod_LoadLeafBrushes: No lump given");

	in = (const unsigned short *) (cModelBase + l->fileofs);
	if (l->filelen % sizeof(unsigned short))
		Com_Error(ERR_DROP, "CMod_LoadLeafBrushes: funny lump size: %i", l->filelen);
	count = l->filelen / sizeof(unsigned short);
	Com_DPrintf(DEBUG_ENGINE, COLORED_GREEN "...leafbrushes: %i\n", count);

	/* add some for the box */
	out = (unsigned short *)Mem_PoolAlloc((count + 1) * sizeof(*out), com_cmodelSysPool, 0);

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
 * @param[in] l descriptor of the data block we are working on
 * @sa CM_AddMapTile
 */
static void CMod_LoadBrushSides (const lump_t * l)
{
	int i;
	cBspBrushSide_t *out;
	const dBspBrushSide_t *in;
	int count;

	if (!l)
		Com_Error(ERR_DROP, "CMod_LoadBrushSides: No lump given");

	in = (const void *) (cModelBase + l->fileofs);
	if (l->filelen % sizeof(dBspBrushSide_t))
		Com_Error(ERR_DROP, "CMod_LoadBrushSides: funny lump size: %i", l->filelen);
	count = l->filelen / sizeof(dBspBrushSide_t);
	Com_DPrintf(DEBUG_ENGINE, COLORED_GREEN "...brushsides: %i\n", count);

	/* need to save space for box planes */
	if (count > MAX_MAP_BRUSHSIDES)
		Com_Error(ERR_DROP, "Map has too many brushsides");

	/* add some for the box */
	out = Mem_PoolAlloc((count + 6) * sizeof(*out), com_cmodelSysPool, 0);

	curTile->numbrushsides = count;
	curTile->brushsides = out;

	for (i = 0; i < count; i++, in++, out++) {
		const int num = LittleShort(in->planenum);
		const int j = LittleShort(in->texinfo);
		if (j >= curTile->numtexinfo)
			Com_Error(ERR_DROP, "Bad brushside texinfo");
		out->plane = &curTile->planes[num];
		out->surface = &curTile->surfaces[j];
	}
}

/**
 * @param[in] source Source will be set to the end of the compressed data block!
 * @param[in] dataStart where to place the uncompressed data
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
		if (curTile->models[i].headnode == LEAFNODE || curTile->models[i].headnode >= curTile->numnodes + 6)
			continue;

		curTile->thead[curTile->numtheads] = tnode_p - curTile->tnodes;
		curTile->theadlevel[curTile->numtheads] = i;
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
 * @brief Calculates the bounding box for the given bsp model
 * @param[in] model The model to calculate the bbox for
 * @param[out] mins The maxs of the bbox
 * @param[out] maxs The mins of the bbox
 */
static void CM_CalculateBoundingBox (const cBspModel_t* model, vec3_t mins, vec3_t maxs)
{
	/* Quickly calculate the bounds of this model to see if they can overlap. */
	VectorAdd(model->origin, model->mins, mins);
	VectorAdd(model->origin, model->maxs, maxs);
	if (VectorNotEmpty(model->angles)) {
		vec3_t acenter, aoffset;
		const float offset = max(max(fabs(mins[0] - maxs[0]), fabs(mins[1] - maxs[1])), fabs(mins[2] - maxs[2])) / 2.0;
		VectorCenterFromMinsMaxs(mins, maxs, acenter);
		VectorSet(aoffset, offset, offset, offset);
		VectorAdd(acenter, aoffset, maxs);
		VectorSubtract(acenter, aoffset, mins);
	}
}

/**
 * @brief A quick test if the trace might hit the inline model
 * @param[in] start The position to start the trace.
 * @param[in] stop The position where the trace ends.
 * @param[in] model The entity to check
 * @return qtrue - the line isn't anywhere near the model
 */
static qboolean CM_LineMissesModel (const vec3_t start, const vec3_t stop, const cBspModel_t *model)
{
	vec3_t amins, amaxs;
	CM_CalculateBoundingBox(model, amins, amaxs);
	/* If the bounds of the extents box and the line do not overlap, then skip tracing this model. */
	if ((start[0] > amaxs[0] && stop[0] > amaxs[0])
		|| (start[1] > amaxs[1] && stop[1] > amaxs[1])
		|| (start[2] > amaxs[2] && stop[2] > amaxs[2])
		|| (start[0] < amins[0] && stop[0] < amins[0])
		|| (start[1] < amins[1] && stop[1] < amins[1])
		|| (start[2] < amins[2] && stop[2] < amins[2]))
		return qtrue;

	return qfalse;
}

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
		if (*name[0] != '*')
			Com_Error(ERR_DROP, "name in the inlineList is no inline model: '%s'", *name);
		model = CM_InlineModel(*name);
		assert(model);
		if (model->headnode >= mapTiles[model->tile].numnodes + 6)
			continue;

		/* check if we can safely exclude that the trace can hit the model */
		if (CM_LineMissesModel(start, stop, model))
			continue;

		trace = CM_HintedTransformedBoxTrace(model->tile, start, stop, vec3_origin, vec3_origin, model->headnode, MASK_VISIBILILITY, 0, model->origin, model->angles, model->shift, 1.0);
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
	CM_SetInlineList(entlist);
	/* do the line test */
	hit = CM_EntTestLine(start, stop, levelmask);
	/* zero the list */
	CM_SetInlineList(NULL);

	return hit;
}


/**
 * @brief Checks traces against the world and all inline models
 * @param[in] start The position to start the trace.
 * @param[in] stop The position where the trace ends.
 * @param[out] end The position where the trace hits something
 * @param[in] levelmask The bsp level that is used for tracing in (see @c TL_FLAG_*)
 * @param[in] entlist of entities that might be on this line
 * @sa CM_EntTestLineDM
 * @return qtrue - hit something
 * @return qfalse - hit nothing
 */
qboolean CM_TestLineDMWithEnt (const vec3_t start, const vec3_t stop, vec3_t end, const int levelmask, const char **entlist)
{
	qboolean hit;

	/* set the list of entities to check */
	CM_SetInlineList(entlist);
	/* do the line test */
	hit = CM_EntTestLineDM(start, stop, end, levelmask);
	/* zero the list */
	CM_SetInlineList(NULL);

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
	float fraction = 2.0f;

	/* trace against world first */
	blocked = TR_TestLineDM(start, stop, end, levelmask);
	if (!inlineList)
		return blocked;

	for (name = inlineList; *name; name++) {
		/* check whether this is really an inline model */
		if (*name[0] != '*') {
			/* Let's see what the data looks like... */
			Com_Error(ERR_DROP, "name in the inlineList is no inline model: '%s' (inlines: %p, name: %p)",
					*name, (void*)inlineList, (void*)name);
		}
		model = CM_InlineModel(*name);
		assert(model);
		if (model->headnode >= mapTiles[model->tile].numnodes + 6)
			continue;

		/* check if we can safely exclude that the trace can hit the model */
		if (CM_LineMissesModel(start, stop, model))
			continue;

		trace = CM_HintedTransformedBoxTrace(model->tile, start, end, vec3_origin, vec3_origin, model->headnode, MASK_ALL, 0, model->origin, model->angles, vec3_origin, fraction);
		/* if we started the trace in a wall */
		if (trace.startsolid) {
			VectorCopy(start, end);
			return qtrue;
		}
		/* trace not finishd */
		if (trace.fraction < fraction) {
			blocked = qtrue;
			fraction = trace.fraction;
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
trace_t CM_HintedTransformedBoxTrace (const int tile, const vec3_t start, const vec3_t end, const vec3_t mins, const vec3_t maxs, const int headnode, const int brushmask, const int brushrejects, const vec3_t origin, const vec3_t angles, const vec3_t rmaShift, const float fraction)
{
	return TR_HintedTransformedBoxTrace(&mapTiles[tile], start, end, mins, maxs, headnode, brushmask, brushrejects, origin, angles, rmaShift, fraction);
}

/**
 * @brief Performs box traces against the world and all inline models, gives the hit position back
 * @param[in] start The position to start the trace.
 * @param[in] end The position where the trace ends.
 * @param[in] traceBox The minimum/maximum extents of the collision box that is projected.
 * @param[in] levelmask A mask of the game levels to trace against. Mask 0x100 filters clips.
 * @param[in] brushmask Any brush detected must at least have one of these contents.
 * @param[in] brushreject Any brush detected with any of these contents will be ignored.
 * @return a trace_t with the information of the closest brush intersected.
 * @sa TR_CompleteBoxTrace
 * @sa CM_HintedTransformedBoxTrace
 */
trace_t CM_EntCompleteBoxTrace (const vec3_t start, const vec3_t end, const box_t* traceBox, int levelmask, int brushmask, int brushreject)
{
	trace_t trace, newtr;
	cBspModel_t *model;
	const char **name;
	vec3_t bmins, bmaxs;

	/* trace against world first */
	trace = TR_CompleteBoxTrace(start, end, traceBox->mins, traceBox->maxs, levelmask, brushmask, brushreject);
	if (!inlineList || trace.fraction == 0.0)
		return trace;

	/* Find the original bounding box for the tracing line. */
	VectorSet(bmins, min(start[0], end[0]), min(start[1], end[1]), min(start[2], end[2]));
	VectorSet(bmaxs, max(start[0], end[0]), max(start[1], end[1]), max(start[2], end[2]));
	/* Now increase the bounding box by mins and maxs in both directions. */
	VectorAdd(bmins, traceBox->mins, bmins);
	VectorAdd(bmaxs, traceBox->maxs, bmaxs);
	/* Now bmins and bmaxs specify the whole volume to be traced through. */

	for (name = inlineList; *name; name++) {
		vec3_t amins, amaxs;

		/* check whether this is really an inline model */
		if (*name[0] != '*')
			Com_Error(ERR_DROP, "name in the inlineList is no inline model: '%s'", *name);
		model = CM_InlineModel(*name);
		assert(model);
		if (model->headnode >= mapTiles[model->tile].numnodes + 6)
			continue;

		/* Quickly calculate the bounds of this model to see if they can overlap. */
		CM_CalculateBoundingBox(model, amins, amaxs);

		/* If the bounds of the extents box and the line do not overlap, then skip tracing this model. */
		if (bmins[0] > amaxs[0]
		 || bmins[1] > amaxs[1]
		 || bmins[2] > amaxs[2]
		 || bmaxs[0] < amins[0]
		 || bmaxs[1] < amins[1]
		 || bmaxs[2] < amins[2])
			continue;

		newtr = CM_HintedTransformedBoxTrace(model->tile, start, end, traceBox->mins, traceBox->maxs, model->headnode, brushmask, brushreject, model->origin, model->angles, model->shift, trace.fraction);

		/* memorize the trace with the minimal fraction */
		if (newtr.fraction == 0.0)
			return newtr;
		if (newtr.fraction < trace.fraction)
			trace = newtr;
	}
	return trace;
}


/*
===============================================================================
GAME RELATED TRACING
===============================================================================
*/


/**
 * @param[in] name The name of the maptile
 * @param[in] l Routing lump ... (routing data lump from bsp file)
 * @param[in] sX The x position on the world plane (grid position) - values from -(PATHFINDING_WIDTH/2) up to PATHFINDING_WIDTH/2 are allowed
 * @param[in] sY The y position on the world plane (grid position) - values from -(PATHFINDING_WIDTH/2) up to PATHFINDING_WIDTH/2 are allowed
 * @param[in] sZ The height level on the world plane (grid position) - values from 0 - PATHFINDING_HEIGHT are allowed
 * @sa CM_AddMapTile
 * @todo TEST z-level routing
 */
static void CMod_LoadRouting (const char *name, const lump_t * l, int sX, int sY, int sZ)
{
	static routing_t tempMap[ACTOR_MAX_SIZE];
	byte *source;
	int length;
	int x, y, z, size, dir;
	int minX, minY, minZ;
	int maxX, maxY, maxZ;
	unsigned int i;
	double start, end;
	const int targetLength = sizeof(curTile->wpMins) + sizeof(curTile->wpMaxs) + sizeof(tempMap);

	CM_SetInlineList(NULL);

	start = time(NULL);

	if (!l)
		Com_Error(ERR_DROP, "CMod_LoadRouting: No lump given");

	if (!l->filelen)
		Com_Error(ERR_DROP, "CMod_LoadRouting: Map has NO routing lump");

	assert((sX > -(PATHFINDING_WIDTH / 2)) && (sX < (PATHFINDING_WIDTH / 2)));
	assert((sY > -(PATHFINDING_WIDTH / 2)) && (sY < (PATHFINDING_WIDTH / 2)));
	assert((sZ >= 0) && (sZ < PATHFINDING_HEIGHT));

	source = cModelBase + l->fileofs;

	i = CMod_DeCompressRouting(&source, (byte*)curTile->wpMins);
	length = i;
	i = CMod_DeCompressRouting(&source, (byte*)curTile->wpMaxs);
	length += i;
	i = CMod_DeCompressRouting(&source, (byte*)tempMap);
	length += i;

	if (length != targetLength)
		Com_Error(ERR_DROP, "CMod_LoadRouting: Map has BAD routing lump; expected %i got %i", targetLength, length);

	/* endian swap possibly necessary */
	for (i = 0; i < 3; i++) {
		curTile->wpMins[i] = LittleLong(curTile->wpMins[i]);
		curTile->wpMaxs[i] = LittleLong(curTile->wpMaxs[i]);
	}

	Com_DPrintf(DEBUG_ROUTING, "Map:%s  Offset:(%i, %i, %i)\n", name, sX, sY, sZ);
	Com_DPrintf(DEBUG_ROUTING, "wpMins:(%i, %i, %i) wpMaxs:(%i, %i, %i)\n", curTile->wpMins[0], curTile->wpMins[1], curTile->wpMins[2], curTile->wpMaxs[0], curTile->wpMaxs[1], curTile->wpMaxs[2]);

	/* wpMins and wpMaxs have the map size data from the initial build.
	 * Offset this by the given parameters so the stored values are in real coordinates. */
	curTile->wpMins[0] += sX;
	curTile->wpMins[1] += sY;
	curTile->wpMins[2] += sZ;
	curTile->wpMaxs[0] += sX;
	curTile->wpMaxs[1] += sY;
	curTile->wpMaxs[2] += sZ;

	Com_DPrintf(DEBUG_ROUTING, "Shifted wpMins:(%i, %i, %i) wpMaxs:(%i, %i, %i)\n", curTile->wpMins[0], curTile->wpMins[1], curTile->wpMins[2], curTile->wpMaxs[0], curTile->wpMaxs[1], curTile->wpMaxs[2]);

	/* Things that need to be done:
	 * The floor, ceiling, and route data can be copied over from the map.
	 * All data must be regenerated for cells with overlapping content or where new
	 * model data is adjacent to a cell with existing model data. */

	/* Copy the routing information into our master table */
	minX = max(curTile->wpMins[0], 0);
	minY = max(curTile->wpMins[1], 0);
	minZ = max(curTile->wpMins[2], 0);
	maxX = min(curTile->wpMaxs[0], PATHFINDING_WIDTH - 1);
	maxY = min(curTile->wpMaxs[1], PATHFINDING_WIDTH - 1);
	maxZ = min(curTile->wpMaxs[2], PATHFINDING_HEIGHT - 1);

	assert(minX <= maxX);
	assert(minY <= maxY);
	assert(minZ <= maxZ);

	Com_DPrintf(DEBUG_ROUTING, "Tile bounds: (%i, %i, %i) to (%i, %i, %i)\n", minX, minY, minZ, maxX, maxY, maxZ);
	Com_DPrintf(DEBUG_ROUTING, "Source bounds: (%i, %i, %i) to (%i, %i, %i)\n", minX - sX, minY - sY, minZ - sZ, maxX - sX, maxY - sY, maxZ - sZ);

	for (size = 0; size < ACTOR_MAX_SIZE; size++)
		/* Adjust starting x and y by size to catch large actor cell overlap. */
		for (y = minY - size; y <= maxY; y++)
			for (x = minX - size; x <= maxX; x++) {
				/* Just incase x or y start negative. */
				if (x < 0 || y < 0)
					continue;
				for (z = minZ; z <= maxZ; z++) {
					clMap[size].floor[z][y][x] = tempMap[size].floor[z - sZ][y - sY][x - sX];
					clMap[size].ceil[z][y][x] = tempMap[size].ceil[z - sZ][y - sY][x - sX];
					for (dir = 0; dir < CORE_DIRECTIONS; dir++) {
						clMap[size].route[z][y][x][dir] = tempMap[size].route[z - sZ][y - sY][x - sX][dir];
						clMap[size].stepup[z][y][x][dir] = tempMap[size].stepup[z - sZ][y - sY][x - sX][dir];
					}
				}
				/* Update the reroute table */
				if (!reroute[size][y][x]) {
					reroute[size][y][x] = numTiles + 1;
				} else {
					reroute[size][y][x] = ROUTING_NOT_REACHABLE;
				}
			}

	Com_DPrintf(DEBUG_ROUTING, "Done copying data.\n");

	end = time(NULL);
	Com_DPrintf(DEBUG_ROUTING, "Loaded routing for tile %s in %5.1fs\n", name, end - start);
}


/**
 * @note Transforms coordinates and stuff for assembled maps
 * @param[in] l descriptor of the data block we are working on
 * @param[in] shift The shifting vector in case this is a map assemble
 * @param[in] tileIndex Used to make targetname and target unique over all
 * loaded map tiles.
 * @sa CM_AddMapTile
 */
static void CMod_LoadEntityString (lump_t * l, vec3_t shift, int tileIndex)
{
	const char *token;
	const char *es;
	char keyname[256];
	vec3_t v;
	cBspModel_t *model = NULL;

	if (!l)
		Com_Error(ERR_DROP, "CMod_LoadEntityString: No lump given");

	if (!l->filelen)
		Com_Error(ERR_DROP, "CMod_LoadEntityString: Map has NO entity lump");

	if (l->filelen + 1 > MAX_MAP_ENTSTRING)
		Com_Error(ERR_DROP, "CMod_LoadEntityString: Map has too large entity lump");

	/* merge entitystring information */
	es = (const char *) (cModelBase + l->fileofs);
	while (1) {
		/* parse the opening brace */
		token = Com_Parse(&es);
		if (!es)
			break;
		if (token[0] != '{')
			Com_Error(ERR_DROP, "CMod_LoadEntityString: found %s when expecting {", token);

		/* new entity */
		Q_strcat(mapEntityString, "{ ", MAX_MAP_ENTSTRING);

		/* go through all the dictionary pairs */
		while (1) {
			/* parse key */
			token = Com_Parse(&es);
			if (token[0] == '}')
				break;
			if (!es)
				Com_Error(ERR_DROP, "CMod_LoadEntityString: EOF without closing brace");

			Q_strncpyz(keyname, token, sizeof(keyname));

			/* parse value */
			token = Com_Parse(&es);
			if (!es)
				Com_Error(ERR_DROP, "CMod_LoadEntityString: EOF without closing brace");

			if (token[0] == '}')
				Com_Error(ERR_DROP, "CMod_LoadEntityString: closing brace without data");

			/* alter value, if needed */
			if (!strcmp(keyname, "origin")) {
				/* origins are shifted */
				sscanf(token, "%f %f %f", &(v[0]), &(v[1]), &(v[2]));
				VectorAdd(v, shift, v);
				Q_strcat(mapEntityString, va("%s \"%f %f %f\" ", keyname, v[0], v[1], v[2]), MAX_MAP_ENTSTRING);
				/* If we have a model, then unadjust it's mins and maxs. */
				if (model) {
					VectorSubtract(model->mins, shift, model->mins);
					VectorSubtract(model->maxs, shift, model->maxs);
					model = NULL; /* reset it, or the next origin will shift it again */
				}
			} else if (!strcmp(keyname, "model") && token[0] == '*') {
				/* adapt inline model number */
				int num = atoi(token + 1);
				/* Get the model */
				model = &curTile->models[NUM_REGULAR_MODELS + num - 1];
				/* Now update the model number to reflect prior tiles loaded. */
				num += numInline;
				Q_strcat(mapEntityString, va("%s *%i ", keyname, num), MAX_MAP_ENTSTRING);
			} else if (!strcmp(keyname, "targetname") || !strcmp(keyname, "target")) {
				Q_strcat(mapEntityString, va("%s \"%s-%i\" ", keyname, token, tileIndex), MAX_MAP_ENTSTRING);
			} else {
				/* just store key and value */
				Q_strcat(mapEntityString, va("%s \"%s\" ", keyname, token), MAX_MAP_ENTSTRING);
			}
		}

		/* finish entity */
		Q_strcat(mapEntityString, "} ", MAX_MAP_ENTSTRING);
	}
}

/**
 * @brief Loads the lightmap for server side visibility lookup
 * @todo Implement this
 */
static void CMod_LoadLighting (const lump_t * l)
{
#if 0
	curTile->lightdata = Mem_PoolAlloc(l->filelen, com_cmodelSysPool, 0);
	memcpy(curTile->lightdata, cModelBase + l->fileofs, l->filelen);
#endif
}

/**
 * @brief Adds in a single map tile
 * @param[in] name The (file-)name of the tile to add.
 * @param[in] day whether the lighting for day or night should be loaded.
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
static unsigned CM_AddMapTile (const char *name, qboolean day, int sX, int sY, byte sZ)
{
	char filename[MAX_QPATH];
	unsigned checksum;
	byte *buf;
	unsigned int i;
	int length;
	dBspHeader_t header;
	/* use for random map assembly for shifting origins and so on */
	vec3_t shift;

	Com_DPrintf(DEBUG_ENGINE, "CM_AddMapTile: %s at %i,%i,%i\n", name, sX, sY, sZ);
	assert(name);
	assert(name[0]);
	assert((sX > -(PATHFINDING_WIDTH / 2)) && (sX < (PATHFINDING_WIDTH / 2)));
	assert((sY > -(PATHFINDING_WIDTH / 2)) && (sY < (PATHFINDING_WIDTH / 2)));
	assert(sZ < PATHFINDING_HEIGHT);

	/* load the file */
	Com_sprintf(filename, sizeof(filename), "maps/%s.bsp", name);
	length = FS_LoadFile(filename, &buf);
	if (!buf)
		Com_Error(ERR_DROP, "Couldn't load %s", filename);

	checksum = LittleLong(Com_BlockChecksum(buf, length));

	header = *(dBspHeader_t *) buf;
	for (i = 0; i < sizeof(header) / 4; i++)
		((int *) &header)[i] = LittleLong(((int *) &header)[i]);

	if (header.version != BSPVERSION)
		Com_Error(ERR_DROP, "CM_AddMapTile: %s has wrong version number (%i should be %i)", name, header.version, BSPVERSION);

	cModelBase = (byte *) buf;

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
	CMod_LoadSurfaces(&header.lumps[LUMP_TEXINFO]);
	CMod_LoadLeafs(&header.lumps[LUMP_LEAFS]);
	CMod_LoadLeafBrushes(&header.lumps[LUMP_LEAFBRUSHES]);
	CMod_LoadPlanes(&header.lumps[LUMP_PLANES], shift);
	CMod_LoadBrushes(&header.lumps[LUMP_BRUSHES]);
	CMod_LoadBrushSides(&header.lumps[LUMP_BRUSHSIDES]);
	CMod_LoadSubmodels(&header.lumps[LUMP_MODELS], shift);
	CMod_LoadNodes(&header.lumps[LUMP_NODES], shift);
	CMod_LoadEntityString(&header.lumps[LUMP_ENTITIES], shift, numTiles);
	if (day)
		CMod_LoadLighting(&header.lumps[LUMP_LIGHTING_DAY]);
	else
		CMod_LoadLighting(&header.lumps[LUMP_LIGHTING_NIGHT]);

	CM_InitBoxHull();
	CM_MakeTracingNodes();

	/* Lets test if curTile is changed */
	assert(curTile == &mapTiles[numTiles]);

	/* CMod_LoadRouting plays with curTile and numTiles, so let set
	 * these to the right values now */
	numInline += curTile->nummodels - NUM_REGULAR_MODELS;

	CMod_LoadRouting(name, &header.lumps[LUMP_ROUTING], sX, sY, sZ);

	/* now increase the amount of loaded tiles */
	numTiles++;

	/* Now find the map bounds with the updated numTiles. */
	/* calculate new border after merge */
	RT_GetMapSize(mapMin, mapMax);

	FS_FreeFile(buf);

	return checksum;
}

static void CMod_RerouteMap (void)
{
	actorSizeEnum_t size;
	int x, y, z, dir;
	int i;
	pos3_t mins, maxs;
	double start, end;

	start = time(NULL);

	VecToPos(mapMin, mins);
	VecToPos(mapMax, maxs);

	/* fit min/max into the world size */
	maxs[0] = min(maxs[0], PATHFINDING_WIDTH - 1);
	maxs[1] = min(maxs[1], PATHFINDING_WIDTH - 1);
	maxs[2] = min(maxs[2], PATHFINDING_HEIGHT - 1);
	for (i = 0; i < 3; i++)
		mins[i] = max(mins[i], 0);

	Com_DPrintf(DEBUG_PATHING, "rerouting (%i %i %i) (%i %i %i)\n",
		(int)mins[0], (int)mins[1], (int)mins[2],
		(int)maxs[0], (int)maxs[1], (int)maxs[2]);

	/* Floor pass */
	for (size = ACTOR_SIZE_INVALID; size < ACTOR_MAX_SIZE; size++) {
		for (y = mins[1]; y <= maxs[1]; y++) {
			for (x = mins[0]; x <= maxs[0]; x++) {
				if (reroute[size][y][x] == ROUTING_NOT_REACHABLE) {
					/* Com_Printf("Tracing floor (%i %i s:%i)\n", x, y, size); */
					for (z = maxs[2]; z >= mins[2]; z--) {
						const int newZ = RT_CheckCell(clMap, size + 1, x, y, z);
						assert(newZ <= z);
						z = newZ;
					}
				}
			}
		}
	}

	/* Wall pass */
	/** @todo A temporary hack- if we decrease ACTOR_MAX_SIZE we need to bump the BSPVERSION again.
	 * I'm just commenting out the CORRECT code for now. */
	/* for (size = ACTOR_SIZE_INVALID; size < ACTOR_MAX_SIZE; size++) { */
	for (size = ACTOR_SIZE_INVALID; size < 1; size++) {
		for (y = mins[1]; y <= maxs[1]; y++) {
			for (x = mins[0]; x <= maxs[0]; x++) {
				const byte tile = reroute[size][y][x];
				if (tile) {
					/** @note The new R_UpdateConnection updates both directions at the same time,
					 * so we only need to check every other direction. */
					/* Not needed until inverse tracing works again.
					 * for (dir = 0; dir < CORE_DIRECTIONS; dir += 2) { */
					for (dir = 0; dir < CORE_DIRECTIONS; dir++) {
						const int dx = x + dvecs[dir][0];
						const int dy = y + dvecs[dir][1];
						int tile2; /**< Can't const: need to check bounds first. */
						/* Skip if the destination is out of bounds. */
						if (dx < 0 || dx >= PATHFINDING_WIDTH || dy < 0 || dy >= PATHFINDING_WIDTH)
							continue;
						tile2 = reroute[size][dy][dx];
						/* Both cells are present and if either cell is ROUTING_NOT_REACHABLE or if the cells are different. */
						if (tile2 && (tile2 == ROUTING_NOT_REACHABLE || tile2 != tile)) {
							/** @note This update MUST go from the bottom (0) to the top (7) of the model.
							 * RT_UpdateConnection expects it and breaks otherwise. */
							/* Com_Printf("Tracing passage (%i %i s:%i d:%i)\n", x, y, size, dir); */
							RT_UpdateConnectionColumn(clMap, size + 1, x, y, dir);
						}
					}
				}
			}
		}
	}
	end = time(NULL);
	Com_Printf("Rerouted for RMA in %5.1fs\n", end - start);
}

/**
 * @brief Loads in the map and all submodels
 * @note This function loads the collision data from the bsp file. For
 * rendering @c R_ModBeginLoading is used.
 * @param[in] tiles	Map name(s) relative to base/maps or random map assembly string
 * @param[in] day Use the day (@c true) or the night (@c false) version of the map
 * @param[in] pos In case you gave more than one tile (Random map assembly [rma]) you also
 * have to provide the positions where those tiles should be placed at.
 * @param[out] mapchecksum The checksum of the bsp file to check for in multiplayer games
 * @sa CM_AddMapTile
 * @sa R_ModBeginLoading
 * @note Make sure that mapchecksum was set to 0 before you call this function
 */
void CM_LoadMap (const char *tiles, qboolean day, const char *pos, unsigned *mapchecksum)
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
	mapEntityString[0] = base[0] = 0;

	memset(clMap, 0, sizeof(clMap));

	/* Reset the reroute table */
	memset(reroute, 0, sizeof(reroute));

	if (pos && *pos)
		Com_Printf("CM_LoadMap: \"%s\" \"%s\"\n", tiles, pos);

	/* load tiles */
	while (tiles) {
		/* get tile name */
		token = Com_Parse(&tiles);
		if (!tiles) {
			CMod_RerouteMap();
			/* Copy the server map from the client */
			memcpy(&svMap, &clMap, sizeof(svMap));
			return;
		}

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
				token = Com_Parse(&pos);
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
			*mapchecksum += CM_AddMapTile(name, day, sh[0], sh[1], sh[2]);
		} else {
			/* load only a single tile, if no positions are specified */
			*mapchecksum = CM_AddMapTile(name, day, 0, 0, 0);
			/* Copy the server map from the client */
			memcpy(&svMap, &clMap, sizeof(svMap));
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
	int i, num;

	/* we only want inline models here */
	if (!name || name[0] != '*')
		Com_Error(ERR_DROP, "CM_InlineModel: bad name: '%s'", name ? name : "");
	/* skip the '*' character and get the inline model number */
	num = atoi(name + 1) - 1;
	if (num < 0 || num >= MAX_MODELS)
		Com_Error(ERR_DROP, "CM_InlineModel: bad number %i - max inline models are %i", num, MAX_MODELS);

	/* search all the loaded tiles for the given inline model */
	for (i = 0; i < numTiles; i++) {
		const int models = mapTiles[i].nummodels - NUM_REGULAR_MODELS;
		assert(models >= 0);

		if (num >= models)
			num -= models;
		else
			return &mapTiles[i].models[NUM_REGULAR_MODELS + num];
	}

	Com_Error(ERR_DROP, "CM_InlineModel: Error cannot find model '%s'\n", name);
}

/**
 * @brief This function updates a model's orientation
 * @param[in] name The name of the model, must include the '*'
 * @param[in] origin The new origin for the model
 * @param[in] angles The new facing angles for the model
 * @note This is used whenever a model's orientation changes, e.g. for func_doors and func_rotating models
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
	return mapEntityString;
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
		const int side = i & 1;
		cBspNode_t *c;
		cBspPlane_t *p;
		cBspBrushSide_t *s;

		/* brush sides */
		s = &curTile->brushsides[curTile->numbrushsides + i];
		s->plane = curTile->planes + (curTile->numplanes + i * 2 + side);
		s->surface = &nullSurface;

		/* nodes */
		c = &curTile->nodes[curTile->box_headnode + i];
		c->plane = curTile->planes + (curTile->numplanes + i * 2);
		c->children[side] = -1 - curTile->emptyleaf;
		if (i != 5)
			c->children[side ^ 1] = curTile->box_headnode + i + 1;
		else
			c->children[side ^ 1] = LEAFNODE - curTile->numleafs;

		/* planes */
		p = &curTile->box_planes[i * 2];
		p->type = i >> 1;
		VectorClear(p->normal);
		p->normal[i >> 1] = 1;

		p = &curTile->box_planes[i * 2 + 1];
		p->type = PLANE_ANYX + (i >> 1);
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
	assert(tile < numTiles && tile >= 0);
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
void Grid_DumpWholeClientMap_f (void)
{
	RT_DumpWholeMap(&clMap[0]);
	RT_DumpWholeMap(&clMap[1]);
}


/**
 * @brief  Dumps contents of the entire server map to console for inspection.
 * @sa CL_InitLocal
 */
void Grid_DumpWholeServerMap_f (void)
{
	RT_DumpWholeMap(&svMap[0]);
	RT_DumpWholeMap(&svMap[1]);
}


/**
 * @brief  Dumps contents of the entire client routing table to CSV file.
 * @sa CL_InitLocal
 */
void Grid_DumpClientRoutes_f (void)
{
	ipos3_t wpMins, wpMaxs;
	VecToPos(mapMin, wpMins);
	VecToPos(mapMax, wpMaxs);
	RT_WriteCSVFiles(clMap, "ufoaiclient", wpMins, wpMaxs);
}


/**
 * @brief  Dumps contents of the entire server routing table to CSV file.
 * @sa CL_InitLocal
 */
void Grid_DumpServerRoutes_f (void)
{
	ipos3_t wpMins, wpMaxs;
	VecToPos(mapMin, wpMins);
	VecToPos(mapMax, wpMaxs);
	RT_WriteCSVFiles(svMap, "ufoaiserver", wpMins, wpMaxs);
}


/**
* @brief Checks one field (square) on the grid of the given routing data (i.e. the map).
 * @param[in] map Routing data/map.
 * @param[in] actorSize width of the actor in cells
 * @param[in] path Pointer to client or server side pathing table (clPathMap, svPathMap)
 * @param[in] x Field in x direction
 * @param[in] y Field in y direction
 * @param[in] z Field in z direction
 * @sa Grid_MoveMark
 * @sa G_BuildForbiddenList
 * @sa CL_BuildForbiddenList
 * @return qtrue if one can't walk there (i.e. the field [and attached fields for e.g. 2x2 units] is/are blocked by entries in
 * the forbidden list) otherwise false.
 */
static qboolean Grid_CheckForbidden (const routing_t *map, const actorSizeEnum_t actorSize, pathing_t *path, int x, int y, int z)
{
	pos_t **p;
	int i;
	actorSizeEnum_t size;
	int fx, fy, fz; /**< Holding variables for the forbidden x and y */
	byte *forbiddenSize;

	for (i = 0, p = path->fblist; i < path->fblength / 2; i++, p += 2) {
		/* Skip initial position. */
		if (VectorCompare((*p), excludeFromForbiddenList)) {
			/* Com_DPrintf(DEBUG_PATHING, "Grid_CheckForbidden: skipping %i|%i|%i\n", (*p)[0], (*p)[1], (*p)[2]); */
			continue;
		}

		forbiddenSize = *(p + 1);
		memcpy(&size, forbiddenSize, sizeof(size));
		fx = (*p)[0];
		fy = (*p)[1];
		fz = (*p)[2];

		/* Com_DPrintf(DEBUG_PATHING, "Grid_CheckForbidden: comparing (%i, %i, %i) * %i to (%i, %i, %i) * %i \n", x, y, z, actorSize, fx, fy, fz, size); */

		if (fx + size <= x || x + actorSize <= fx)
			continue; /* x bounds do not intersect */
		if (fy + size <= y || y + actorSize <= fy)
			continue; /* y bounds do not intersect */
		if (z == fz) {
			Com_DPrintf(DEBUG_PATHING, "Grid_CheckForbidden: Collision (%i, %i, %i) * %i and (%i, %i, %i) * %i \n", x, y, z, actorSize, fx, fy, fz, size);
			return qtrue; /* confirmed intersection */
		}
	}
	return qfalse;
}

void Grid_DumpDVTable (const pathing_t *path)
{
	int px, py, pz, cr;
	pos3_t mins, maxs;

	VecToPos(mapMin, mins);
	VecToPos(mapMax, maxs);

	Com_Printf("Bounds: (%i %i %i) to (%i %i %i)\n", mins[0], mins[1], mins[2], maxs[0], maxs[1], maxs[2]);
	for (cr = 0; cr < ACTOR_MAX_STATES; cr++) {
		for (pz = mins[2]; pz <= maxs[2]; pz++) {
			Com_Printf("\ncr:%i z:%i\n", cr, pz);
			for (py = maxs[1]; py >= mins[1]; py--) {
				for (px = mins[0]; px <= maxs[0]; px++) {
					const int dv = RT_AREA_FROM(path, px, py, pz, cr);
					const int oz = getDVz(dv);
					const int dir = getDVdir(dv);
					Com_Printf("%3i %2i %1i,", RT_AREA(path, px, py, pz, cr), dir, oz);
				}
				Com_Printf("\n");
			}
		}
	}
}

static void Grid_SetMoveData (pathing_t *path, const int x, const int y, const int z, const int c, const byte length, const int dir, const int ox, const int oy, const int oz, const int oc, priorityQueue_t *pqueue)
{
	pos4_t dummy;

	RT_AREA_TEST(path, x, y, z, c);
	RT_AREA(path, x, y, z, c) = length;	/**< Store TUs for this square. */
	RT_AREA_FROM(path, x, y, z, c) = makeDV(dir, oz); /**< Store origination information for this square. */
	{
		pos3_t pos, test;
		int crouch = c;
		VectorSet(pos, ox, oy, oz);
		VectorSet(test, x, y, z);
		PosSubDV(test, crouch, RT_AREA_FROM(path, x, y, z, c));
		if (!VectorCompare(test, pos) || crouch != oc) {
			Com_Printf("Grid_SetMoveData: Created faulty DV table.\nx:%i y:%i z:%i c:%i\ndir:%i\nnx:%i ny:%i nz:%i nc:%i\ntx:%i ty:%i tz:%i tc:%i\n",
				ox, oy, oz, oc, dir, x, y, z, c, test[0], test[1], test[2], crouch);

			Grid_DumpDVTable(path);
			Com_Error(ERR_DROP, "Grid_SetMoveData: Created faulty DV table.");
		}
	}
	Vector4Set(dummy, x, y, z, c);
	/** @todo add heuristic for A* algorithm */
	PQueuePush(pqueue, dummy, length);
}

/**
 * @param[in] map Pointer to client or server side routing table (clMap, svMap)
 * @param[in] actorSize Give the field size of the actor (e.g. for 2x2 units) to check linked fields as well.
 * @param[in,out] path Pointer to client or server side pathing table (clMap, svMap)
 * @param[in] pos Current location in the map.
 * @param[in] crouchingState Whether the actor is currently crouching, 1 is yes, 0 is no.
 * @param[in] dir Direction vector index (see DIRECTIONS and dvecs)
 * @param[in,out] pqueue Priority queue (heap) to insert the now reached tiles for reconsidering
 * @sa Grid_CheckForbidden
 */
void Grid_MoveMark (const routing_t *map, const actorSizeEnum_t actorSize, pathing_t *path, const pos3_t pos, byte crouchingState, const int dir, priorityQueue_t *pqueue)
{
	int x, y, z;
	int nx, ny, nz;
	int dx, dy, dz;
	byte l, ol;
	int passageHeight, actorStepupHeight;

	/** @todo flier should return true if the actor can fly. */
	const qboolean flier = qfalse; /**< This can be keyed into whether an actor can fly or not to allow flying */

	/** @todo has_ladder_support should return true if
	 *  1) There is a ladder in the new cell in the specified direction or
	 *  2) There is a ladder in any direction in the cell below the new cell and no ladder in the new cell itself. */
	const qboolean hasLadderSupport = qfalse; /**< Indicates if there is a ladder present providing support. */

	/** @todo has_ladder_climb should return true if
	 *  1) There is a ladder in the new cell in the specified direction. */
	const qboolean hasLadderClimb = qfalse; /**< Indicates if there is a ladder present providing ability to climb. */

	/** @todo falling_height should be replaced with an arbitrary max falling height based on the actor. */
	const int fallingHeight = PATHFINDING_MAX_FALL;/**<This is the maximum height that an actor can fall. */

	/**
	 * @note This value is worthless if it is between CORE_DIRECTIONS and FLYING_DIRECTIONS:
	 * These are defined actions or climbing.
	 */
	const int coreDir = dir % CORE_DIRECTIONS;/**< The compass direction of this move if less than CORE_DIRECTIONS or at least FLYING_DIRECTIONS */

	/** @note This is the actor's height in QUANT units. */
	const int actorHeight = ModelCeilingToQuant((float)(crouchingState ? PLAYER_CROUCHING_HEIGHT : PLAYER_STANDING_HEIGHT)); /**< The actor's height */

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

	RT_AREA_TEST(path, x, y, z, crouchingState);
	ol = RT_AREA(path, x, y, z, crouchingState);

	Com_DPrintf(DEBUG_PATHING, "Grid_MoveMark: (%i %i %i) s:%i dir:%i c:%i ol:%i\n", x, y, z, actorSize, dir, crouchingState, ol);

	/* We cannot fly and crouch at the same time. This will also cause an actor to stand to fly. */
	if (crouchingState && dir >= FLYING_DIRECTIONS) {
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
	l = Grid_GetTUsForDirection(dir);

	/* If crouching then multiply by the crouching factor. */
	if (crouchingState == 1)
		l *= TU_CROUCH_MOVING_FACTOR;

	/* Now add the TUs needed to get to the originating cell. */
	l += ol;

	/* If this is a crouching or crouching move, then process that motion. */
	if (dir == DIRECTION_STAND_UP || dir == DIRECTION_CROUCH) {
		/* Can't stand up if standing. */
		if (dir == DIRECTION_STAND_UP && crouchingState == 0) {
			Com_DPrintf(DEBUG_PATHING, "Grid_MoveMark: Can't stand while standing.\n");
			return;
		}
		/* Can't stand up if there's not enough head room. */
		if (dir == DIRECTION_STAND_UP && QuantToModel(Grid_Height(map, actorSize, pos)) >= PLAYER_STANDING_HEIGHT) {
			Com_DPrintf(DEBUG_PATHING, "Grid_MoveMark: Can't stand under a short ceiling.\n");
			return;
		}
		/* Can't get down if crouching. */
		if (dir == DIRECTION_CROUCH && crouchingState == 1) {
			Com_DPrintf(DEBUG_PATHING, "Grid_MoveMark: Can't crouch while crouching.\n");
			return;
		}

		/* Since we can toggle crouching, then do so. */
		crouchingState ^= 1;

		/* Is this a better move into this cell? */
		RT_AREA_TEST(path, x, y, z, crouchingState);
		if (RT_AREA(path, x, y, z, crouchingState) <= l) {
			Com_DPrintf(DEBUG_PATHING, "Grid_MoveMark: Toggling crouch is not optimum. %i %i\n", RT_AREA(path, x, y, z, crouchingState), l);
			return;
		}

		/* Store move. */
		if (pqueue)
			Grid_SetMoveData(path, x, y, z, crouchingState, l, dir, x, y, z, crouchingState ^ 1, pqueue);

		Com_DPrintf(DEBUG_PATHING, "Grid_MoveMark: Set move to (%i %i %i) c:%i to %i.\n", x, y, z, crouchingState, l);
		/* We are done, exit now. */
		return;
	}

	dx = dvecs[dir][0];	/**< Get the difference value for x for this direction. (can be pos or neg) */
	dy = dvecs[dir][1];	/**< Get the difference value for y for this direction. (can be pos or neg) */
	dz = dvecs[dir][2];	/**< Get the difference value for z for this direction. (can be pos or neg) */
	nx = x + dx;		/**< "new" x value = starting x value + difference from chosen direction */
	ny = y + dy;		/**< "new" y value = starting y value + difference from chosen direction */
	nz = z + dz;		/**< "new" z value = starting z value + difference from chosen direction */

	/* Connection checks.  If we cannot move in the desired direction, then bail. */
	/* Range check of new values (all sizes) */
	if (nx < 0 || nx > PATHFINDING_WIDTH - actorSize
	 || ny < 0 || ny > PATHFINDING_WIDTH - actorSize
	 || nz < 0 || nz > PATHFINDING_HEIGHT) {
		return;
	}

	Com_DPrintf(DEBUG_PATHING, "Grid_MoveMark: (%i %i %i) s:%i to (%i %i %i)\n", x, y, z, actorSize, nx, ny, nz);

	/* If there is no passageway (or rather lack of a wall) to the desired cell, then return. */
	/** @todo actor_height is currently the fixed height of a 1x1 actor.  This needs to be adjusted
	 *  to the actor's actual height, including crouching. */
	/* If the flier is moving up or down diagonally, then passage height will also adjust */
	if (dir >= FLYING_DIRECTIONS) {
		int neededHeight;
		if (dz > 0) {
			/* If the actor is moving up, check the passage at the current cell.
			 * The minimum height is the actor's height plus the distance from the current floor to the top of the cell. */
			neededHeight = actorHeight + CELL_HEIGHT - max(0, RT_FLOOR(map, actorSize, x, y, z));
			RT_CONN_TEST(map, actorSize, x, y, z, coreDir);
			passageHeight = RT_CONN(map, actorSize, x, y, z, coreDir);
		} else if (dz < 0) {
			/* If the actor is moving down, check from the destination cell back. *
			 * The minimum height is the actor's height plus the distance from the destination floor to the top of the cell. */
			neededHeight = actorHeight + CELL_HEIGHT - max(0, RT_FLOOR(map, actorSize, nx, ny, nz));
			RT_CONN_TEST(map, actorSize, nx, ny, nz, coreDir ^ 1);
			passageHeight = RT_CONN(map, actorSize, nx, ny, nz, coreDir ^ 1);
		} else {
			neededHeight = actorHeight;
			RT_CONN_TEST(map, actorSize, x, y, z, coreDir);
			passageHeight = RT_CONN(map, actorSize, x, y, z, coreDir);
		}
		if (passageHeight < neededHeight) {
			Com_DPrintf(DEBUG_PATHING, "Grid_MoveMark: Passage is not tall enough. passage:%i actor:%i\n", passageHeight, actorHeight);
			return;
		}
	} else if (dir < CORE_DIRECTIONS) {
		/**
		 * @note Fliers use this code only when they are walking.
		 * @brief First test for opening height availablilty.  Then test for stepup compatibility.
		 * Last test for fall.
		 */

		const int stepup = RT_STEPUP(map, actorSize, x, y, z, dir); /**< The stepup needed to get to/through the passage */
		const int stepupHeight = stepup & ~(PATHFINDING_BIG_STEPDOWN | PATHFINDING_BIG_STEPUP); /**< The actual stepup height without the level flags */
		int heightChange;

		/** @todo actor_stepup_height should be replaced with an arbitrary max stepup height based on the actor. */
		actorStepupHeight = PATHFINDING_MAX_STEPUP;

		/* This is the standard passage height for all units trying to move horizontally. */
		RT_CONN_TEST(map, actorSize, x, y, z, coreDir);
		passageHeight = RT_CONN(map, actorSize, x, y, z, coreDir);
		if (passageHeight < actorHeight) {
			Com_DPrintf(DEBUG_PATHING, "Grid_MoveMark: Passage is not tall enough. passage:%i actor:%i\n", passageHeight, actorHeight);
			return;
		}

		/* If we are moving horizontally, use the stepup requirement of the floors.
		 * The new z coordinate may need to be adjusted from stepup.
		 * Also, actor_stepup_height must be at least the cell's positive stepup value to move that direction. */
		/* If the actor cannot reach stepup, then we can't go this way. */
		if (actorStepupHeight < stepupHeight) {
			Com_DPrintf(DEBUG_PATHING, "Grid_MoveMark: Actor cannot stepup high enough. passage:%i actor:%i\n", stepupHeight, actorStepupHeight);
			return;
		}
		if ((stepup & PATHFINDING_BIG_STEPUP) && nz < PATHFINDING_HEIGHT - 1) {
			Com_DPrintf(DEBUG_PATHING, "Grid_MoveMark: Stepping up into higher cell.\n");
			nz++;
			/**
			 * @note If you need to know about how pathfinding works,  you need to understand the
			 * following brief.  It may cause nausea, but is an important concept.
			 *
			 * @brief OK, now some crazy tests:
			 * Because of the grid based nature of this game, each cell can have at most only ONE
			 * floor that can be stood upon.  If an actor can walk down a slope that is in the
			 * same level, and actor should be able to walk on (and not fall into) the slope that
			 * decends a game level.  BUT it is possible for an actor to be able to crawl under a
			 * floor that can be stood on, with this opening being in the same cell as the floor.
			 * SO to prevent any conflicts, we will move down a floor under the following conditions:
			 * - The STEPDOWN flag is set
			 * - The floor in the immediately adjacent cell is lower than the current floor, but not
			 *   more than CELL_HEIGHT units (in QUANT units) below the current floor.
			 * - The actor's stepup value is at least the inverse stepup value.  This is the stepup
			 *   FROM the cell we are moving towards back into the cell we are starting in.  This
			 *    ensures that the actor can actually WALK BACK.
			 * If the actor does not have a high enough stepup but meets all the other requirements to
			 * descend the level, the actor will move into a fall state, provided that there is no
			 * floor in the adjacent cell.
			 *
			 * This will prevent actors from walking under a floor in the same cell in order to fall
			 * to the floor beneath.  They will need to be able to step down into the cell or will
			 * not be able to use the opening.
			 */
		} else if ((stepup & PATHFINDING_BIG_STEPDOWN) && nz > 0
			&& actorStepupHeight >= (RT_STEPUP(map, actorSize, nx, ny, nz - 1, dir ^ 1) & ~(PATHFINDING_BIG_STEPDOWN | PATHFINDING_BIG_STEPUP))) {
			Com_DPrintf(DEBUG_PATHING, "Grid_MoveMark: Not stepping down into lower cell.\n");
			nz--;
		} else {
			Com_DPrintf(DEBUG_PATHING, "Grid_MoveMark: Not stepping up or down.\n");
		}
		heightChange = RT_FLOOR(map, actorSize, nx, ny, nz) - RT_FLOOR(map, actorSize, x, y, z) + (nz - z) * CELL_HEIGHT;

		/* If the actor tries to fall more than falling_height, then prohibit the move. */
		if (heightChange < -fallingHeight && !hasLadderSupport) {
			Com_DPrintf(DEBUG_PATHING, "Grid_MoveMark: Too far a drop without a ladder. change:%i maxfall:%i\n", heightChange, -fallingHeight);
			return;
		}

		/* If we are walking normally, we can fall if we move into a cell that does not
		 * have its STEPDOWN flag set and has a negative floor:
		 * Set heightChange to 0.
		 * The actor enters the cell.
		 * The actor will be forced to fall (dir 13) from the destination cell to the cell below. */
		if (RT_FLOOR(map, actorSize, nx, ny, nz) < 0) {
			/* We cannot fall if STEPDOWN is defined. */
			if (stepup & PATHFINDING_BIG_STEPDOWN) {
				Com_DPrintf(DEBUG_PATHING, "Grid_MoveMark: There is stepdown from here.\n");
				return;
			}
			/* We cannot fall if there is an entity below the cell we want to move to. */
			if (Grid_CheckForbidden(map, actorSize, path, nx, ny, nz - 1)) {
				Com_DPrintf(DEBUG_PATHING, "Grid_MoveMark: The fall destination is occupied.\n");
				return;
			}
			Com_DPrintf(DEBUG_PATHING, "Grid_MoveMark: Preparing for a fall. change:%i fall:%i\n", heightChange, -actorStepupHeight);
			heightChange = 0;
			nz--;
		}

	}
	/* else there is no movement that uses passages. */
	/* If we are falling, the height difference is the floor value. */
	else if (dir == DIRECTION_FALL) {
		if (flier) {
			/* Fliers cannot fall intentionally. */
			Com_DPrintf(DEBUG_PATHING, "Grid_MoveMark: Fliers can't fall.\n");
			return;
		} else if (RT_FLOOR(map, actorSize, x, y, z) >= 0) {
			/* We cannot fall if there is a floor in this cell. */
			Com_DPrintf(DEBUG_PATHING, "Grid_MoveMark: Can't fall while supported. floor:%i\n", RT_FLOOR(map, actorSize, x, y, z));
			return;
		} else if (hasLadderSupport) {
			/* The actor can't fall if there is ladder support. */
			Com_DPrintf(DEBUG_PATHING, "Grid_MoveMark: Can't fall because of ladder.\n");
			return;
		}
	} else if (dir == DIRECTION_CLIMB_UP) {
		if (flier && QuantToModel(RT_CEILING(map, actorSize, x, y, z)) < UNIT_HEIGHT * 2 - PLAYER_HEIGHT) { /* up */
			Com_DPrintf(DEBUG_PATHING, "Grid_MoveMark: Not enough headroom to fly up. passage:%i actor:%i\n", QuantToModel(RT_CEILING(map, actorSize, x, y, z)), UNIT_HEIGHT * 2 - PLAYER_HEIGHT);
			return;
		}
		/* If the actor is not a flyer and tries to move up, there must be a ladder. */
		if (dir == DIRECTION_CLIMB_UP && !hasLadderClimb) {
			Com_DPrintf(DEBUG_PATHING, "Grid_MoveMark: Can't climb up without a ladder.\n");
			return;
		}
	} else if (dir == DIRECTION_CLIMB_DOWN) {
		if (flier) {
			if (RT_FLOOR(map, actorSize, x, y, z) >= 0 ) { /* down */
				Com_DPrintf(DEBUG_PATHING, "Grid_MoveMark: Can't fly down through a floor. floor:%i\n", RT_FLOOR(map, actorSize, x, y, z));
				return;
			}
		} else {
			/* If the actor is not a flyer and tries to move down, there must be a ladder. */
			if (!hasLadderClimb) {
				Com_DPrintf(DEBUG_PATHING, "Grid_MoveMark: Can't climb down without a ladder.\n");
				return;
			}
		}
	}

	/* OK, at this point we are certain of a few things:
	 * There is not a wall obstructing access to the destination cell.
	 * If the actor is not a flier, the actor will not rise more than actor_stepup_height or fall more than
	 *    falling_height, unless climbing.
	 *
	 * If the actor is a flier, as long as there is a passage, it can be moved through.
	 * There are no floor difference restrictions for fliers, only obstructions. */

	/* nz can't move out of bounds */
	if (nz < 0)
		nz = 0;
	if (nz >= PATHFINDING_HEIGHT)
		nz = PATHFINDING_HEIGHT - 1;

	/* Is this a better move into this cell? */
	RT_AREA_TEST(path, nx, ny, nz, crouchingState);
	if (RT_AREA(path, nx, ny, nz, crouchingState) <= l) {
		Com_DPrintf(DEBUG_PATHING, "Grid_MoveMark: This move is not optimum. %i %i\n", RT_AREA(path, nx, ny, nz, crouchingState), l);
		return;
	}

	/* Test for forbidden (by other entities) areas. */
	if (Grid_CheckForbidden(map, actorSize, path, nx, ny, nz)) {
		Com_DPrintf(DEBUG_PATHING, "Grid_MoveMark: That spot is occupied.\n");
		return;
	}

	/* Store move. */
	if (pqueue) {
		Grid_SetMoveData(path, nx, ny, nz, crouchingState, l, dir, x, y, z, crouchingState, pqueue);
	}
	Com_DPrintf(DEBUG_PATHING, "Grid_MoveMark: Set move to (%i %i %i) c:%i to %i. srcfloor:%i\n", nx, ny, nz, crouchingState, l, RT_FLOOR(map, actorSize, x, y, z));
}


/**
 * @brief Recalculate the pathing table for the given actor(-position)
 * @param[in] map Pointer to client or server side routing table (clMap, svMap)
 * @param[in] actorSize The size of thing to calc the move for (e.g. size=2 means 2x2).
 * The plan is to have the 'origin' in 2x2 units in the bottom-left (towards the lower coordinates) corner of the 2x2 square.
 * @param[in,out] path Pointer to client or server side pathing table (clMap, svMap)
 * @param[in] from The position to start the calculation from.
 * @param[in] distance The maximum TUs away from 'from' to calculate move-information for
 * @param[in] crouchingState Whether the actor is currently crouching, 1 is yes, 0 is no.
 * @param[in] fb_list Forbidden list (entities are standing at those points)
 * @param[in] fb_length Length of forbidden list
 * @sa Grid_MoveMark
 * @sa G_MoveCalc
 * @sa CL_ConditionalMoveCalc
 */
void Grid_MoveCalc (const routing_t *map, const actorSizeEnum_t actorSize, pathing_t *path, const pos3_t from, byte crouchingState, int distance, byte ** fb_list, int fb_length)
{
	int dir;
	int count;
	priorityQueue_t pqueue;
	pos4_t epos; /**< Extended position; includes crouching state */
	pos3_t pos;

	/* reset move data */
	memset(path->area, ROUTING_NOT_REACHABLE, PATHFINDING_WIDTH * PATHFINDING_WIDTH * PATHFINDING_HEIGHT * ACTOR_MAX_STATES);
	memset(path->areaFrom, ROUTING_NOT_REACHABLE, PATHFINDING_WIDTH * PATHFINDING_WIDTH * PATHFINDING_HEIGHT * ACTOR_MAX_STATES);
	path->fblist = fb_list;
	path->fblength = fb_length;

	if (distance > MAX_ROUTE + 3)	/* +3 is added to calc at least one square (diagonal) more */
		distance = MAX_ROUTE + 3;	/* and later show one step beyond the walkable path in red */

	VectorCopy(from, excludeFromForbiddenList); /**< Prepare exclusion of starting-location (i.e. this should be ent-pos or le-pos) in Grid_CheckForbidden */

	PQueueInitialise(&pqueue, 1024);
	Vector4Set(epos, from[0], from[1], from[2], crouchingState);
	PQueuePush(&pqueue, epos, 0);

	/* Confirm bounds */
	assert((from[2]) < PATHFINDING_HEIGHT);
	assert(crouchingState == 0 || crouchingState == 1);	/* s.a. ACTOR_MAX_STATES */

	RT_AREA(path, from[0], from[1], from[2], crouchingState) = 0;

	Com_DPrintf(DEBUG_PATHING, "Grid_MoveCalc: Start at (%i %i %i) c:%i\n", from[0], from[1], from[2], crouchingState);

	count = 0;
	while (!PQueueIsEmpty(&pqueue)) {
		PQueuePop(&pqueue, epos);
		VectorCopy(epos, pos);
		count++;

		/* for A*
		if pos = goal
			return pos
		*/
		/**< if reaching that square already took too many TUs,
		 * don't bother to reach new squares *from* there. */
		if (RT_AREA(path, pos[0], pos[1], pos[2], crouchingState) >= distance)
			continue;

		for (dir = 0; dir < PATHFINDING_DIRECTIONS; dir++) {
			Grid_MoveMark(map, actorSize, path, pos, epos[3], dir, &pqueue);
		}
	}
	/* Com_Printf("Loop: %i", count); */
	PQueueFree(&pqueue);

	Com_DPrintf(DEBUG_PATHING, "Grid_MoveCalc: Done\n\n");
}

/**
 * @brief Caches the calculated move
 * @param[in] path Pointer to client or server side pathing table (clPathMap, svPathMap)
 * @sa AI_ActorThink
 */
void Grid_MoveStore (pathing_t *path)
{
	memcpy(path->areaStored, path->area, sizeof(path->areaStored));
}


/**
 * @brief Return the needed TUs to walk to a given position
 * @param[in] path Pointer to client or server side pathing table (clPathMap, svPathMap)
 * @param[in] to Position to walk to
 * @param[in] crouchingState Whether the actor is currently crouching, 1 is yes, 0 is no.
 * @param[in] stored Use the stored mask (the cached move) of the routing data
 * @return ROUTING_NOT_REACHABLE if the move isn't possible
 * @return length of move otherwise (TUs)
 */
pos_t Grid_MoveLength (const pathing_t *path, const pos3_t to, byte crouchingState, qboolean stored)
{
#ifdef PARANOID
	if (to[2] >= PATHFINDING_HEIGHT) {
		Com_DPrintf(DEBUG_PATHING, "Grid_MoveLength: WARNING to[2] = %i(>= HEIGHT)\n", to[2]);
		return ROUTING_NOT_REACHABLE;
	}
#endif
	/* Confirm bounds */
	assert(to[2] < PATHFINDING_HEIGHT);
	assert(crouchingState == 0 || crouchingState == 1);	/* s.a. ACTOR_MAX_STATES */

	if (!stored)
		return RT_AREA(path, to[0], to[1], to[2], crouchingState);
	else
		return RT_SAREA(path, to[0], to[1], to[2], crouchingState);
}


/**
 * @brief Get the direction to use to move to a position (used to reconstruct the path)
 * @param[in] path Pointer to client or server side pathing table (le->PathMap, svPathMap)
 * @param[in] toPos The desired location
 * @param[in] crouchingState Whether the actor is currently crouching, 1 is yes, 0 is no.
 * @return a direction index (see dvecs and DIRECTIONS)
 * @sa Grid_MoveCheck
 */
int Grid_MoveNext (const pathing_t *path, const pos3_t toPos, byte crouchingState)
{
	const pos_t l = RT_AREA(path, toPos[0], toPos[1], toPos[2], crouchingState); /**< Get TUs for this square */

	/* Check to see if the TUs needed to move here are greater than 0 and less then ROUTING_NOT_REACHABLE */
	if (!l || l == ROUTING_NOT_REACHABLE) {
		/* ROUTING_UNREACHABLE means, not possible/reachable */
		return ROUTING_UNREACHABLE;
	}

	/* Return the information indicating how the actor got to this cell */
	return RT_AREA_FROM(path, toPos[0], toPos[1], toPos[2], crouchingState);
}


/**
 * @brief Returns the height of the floor in a cell.
 * @param[in] map Pointer to client or server side routing table (clMap, svMap)
 * @param[in] actorSize width of the actor in cells
 * @param[in] pos Position in the map to check the height
 * @return The actual model height of the cell's ceiling.
 */
unsigned int Grid_Ceiling (const routing_t *map, const actorSizeEnum_t actorSize, const pos3_t pos)
{
	/* max 8 levels */
	if (pos[2] >= PATHFINDING_HEIGHT) {
		Com_Printf("Grid_Height: Warning: z level is bigger than %i: %i\n",
			(PATHFINDING_HEIGHT - 1), pos[2]);
	}
	return QuantToModel(RT_CEILING(map, actorSize, pos[0], pos[1], pos[2] & 7));
}


/**
 * @brief Returns the height of the floor in a cell.
 * @param[in] map Pointer to client or server side routing table (clMap, svMap)
 * @param[in] actorSize width of the actor in cells
 * @param[in] pos Position in the map to check the height
 * @return The actual model height of the cell's ceiling.
 */
int Grid_Height (const routing_t *map, const actorSizeEnum_t actorSize, const pos3_t pos)
{
	/* max 8 levels */
	if (pos[2] >= PATHFINDING_HEIGHT) {
		Com_Printf("Grid_Height: Warning: z level is bigger than %i: %i\n",
			(PATHFINDING_HEIGHT - 1), pos[2]);
	}
	return QuantToModel(RT_CEILING(map, actorSize, pos[0], pos[1], pos[2] & (PATHFINDING_HEIGHT - 1))
		- RT_FLOOR(map, actorSize, pos[0], pos[1], pos[2] & (PATHFINDING_HEIGHT - 1)));
}


/**
 * @brief Returns the height of the floor in a cell.
 * @param[in] map Pointer to client or server side routing table (clMap, svMap)
 * @param[in] actorSize width of the actor in cells
 * @param[in] pos Position in the map to check the height
 * @return The actual model height of the cell's floor.
 */
int Grid_Floor (const routing_t *map, const actorSizeEnum_t actorSize, const pos3_t pos)
{
	/* max 8 levels */
	if (pos[2] >= PATHFINDING_HEIGHT) {
		Com_Printf("Grid_Floor: Warning: z level is bigger than %i: %i\n",
			(PATHFINDING_HEIGHT - 1), pos[2]);
	}
	return QuantToModel(RT_FLOOR(map, actorSize, pos[0], pos[1], pos[2] & (PATHFINDING_HEIGHT - 1)));
}


/**
 * @brief Returns the maximum height of an obstruction that an actor can travel over.
 * @param[in] map Pointer to client or server side routing table (clMap, svMap)
 * @param[in] actorSize width of the actor in cells
 * @param[in] pos Position in the map to check the height
 * @param[in] dir the direction in which we are moving
 * @return The actual model height increase needed to move into an adjacent cell.
 */
pos_t Grid_StepUp (const routing_t *map, const actorSizeEnum_t actorSize, const pos3_t pos, const int dir)
{
	/* max 8 levels */
	if (pos[2] >= PATHFINDING_HEIGHT) {
		Com_Printf("Grid_StepUp: Warning: z level is bigger than 7: %i\n", pos[2]);
	}
	return QuantToModel(RT_STEPUP(map, actorSize, pos[0], pos[1], pos[2] & (PATHFINDING_HEIGHT - 1), dir));
}


/**
 * @brief Returns the amounts of TUs that are needed to perform one step into the given direction.
 * @param[in] dir the direction in which we are moving
 * @return The TUs needed to move there.
 */
int Grid_GetTUsForDirection (int dir)
{
	assert(dir >= 0 && dir < PATHFINDING_DIRECTIONS);
	return TUsUsed[dir];
}


/**
 * @brief Returns non-zero if the cell is filled (solid) and cannot be entered.
 * @param[in] map Pointer to client or server side routing table (clMap, svMap)
 * @param[in] actorSize width of the actor in cells
 * @param[in] pos Position in the map to check for filling
 * @return 0 if the cell is vacant (of the world model), non-zero otherwise.
 */
int Grid_Filled (const routing_t *map, const actorSizeEnum_t actorSize, const pos3_t pos)
{
	/* max 8 levels */
	assert(pos[2] < PATHFINDING_HEIGHT);
	return RT_FILLED(map, pos[0], pos[1], pos[2], actorSize);
}


/**
 * @brief Calculated the new height level when something falls down from a certain position.
 * @param[in] map Pointer to client or server side routing table (clMap, svMap)
 * @param[in] pos Position in the map to start the fall (starting height is the z-value in this position)
 * @param[in] actorSize Give the field size of the actor (e.g. for 2x2 units) to check linked fields as well.
 * @return New z (height) value.
 * @return 0xFF if an error occurred.
 */
pos_t Grid_Fall (const routing_t *map, const actorSizeEnum_t actorSize, const pos3_t pos)
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

	/* Easy math- get the floor, integer divide by CELL_HEIGHT, add to z.
	 * If z < 0, we are going down.
	 * If z >= CELL_HEIGHT, we are going up.
	 * If 0 <= z <= CELL_HEIGHT, then z / 16 = 0, no change. */
	base = RT_FLOOR(map, actorSize, pos[0], pos[1], z);
	/* Hack to deal with negative numbers- otherwise rounds toward 0 instead of down. */
	diff = base < 0 ? (base - (CELL_HEIGHT - 1)) / CELL_HEIGHT : base / CELL_HEIGHT;
	z += diff;
	/* The tracing code will set locations without a floor to -1.  Compensate for that. */
	if (z < 0)
		z = 0;
	else if (z >= PATHFINDING_HEIGHT)
		z = PATHFINDING_HEIGHT - 1;
	return z;
}

/**
 * @brief Converts a grid position to world coordinates
 * @sa Grid_Height
 * @param[in] map The routing map
 * @param[in] actorSize width of the actor in cells
 * @param[in] pos The grid position
 * @param[out] vec The world vector
 */
void Grid_PosToVec (const routing_t *map, const actorSizeEnum_t actorSize, const pos3_t pos, vec3_t vec)
{
	SizedPosToVec(pos, actorSize, vec);
#ifdef PARANOID
	if (pos[2] >= PATHFINDING_HEIGHT)
		Com_Printf("Grid_PosToVec: Warning - z level bigger than 7 (%i - source: %.02f)\n", pos[2], vec[2]);
#endif
	/* Clamp the floor value between 0 and UNIT_HEIGHT */
	vec[2] += max(0, min(UNIT_HEIGHT, Grid_Floor(map, actorSize, pos)));
}


/**
 * @brief This function recalculates the routing in the box bounded by min and max.
 * @sa CMod_LoadRouting
 * @sa Grid_RecalcRouting
 * @param[in] map The routing map (either server or client map)
 * @param[in] min The lower extents of the box to recalc routing for
 * @param[in] max The upper extents of the box to recalc routing for
 */
void Grid_RecalcBoxRouting (routing_t *map, const pos3_t min, const pos3_t max)
{
	int x, y, z, actorSize, dir;

	Com_DPrintf(DEBUG_PATHING, "rerouting (%i %i %i) (%i %i %i)\n",
		(int)min[0], (int)min[1], (int)min[2],
		(int)max[0], (int)max[1], (int)max[2]);

	/* Com_Printf("Before:\n"); */
	/* Grid_DumpMap(map, (int)min[0], (int)min[1], (int)min[2], (int)max[0], (int)max[1], (int)max[2]); */

	/* check unit heights */
	/*for (actorSize = 1; actorSize <= ACTOR_MAX_SIZE; actorSize++) {*/
	for (actorSize = 1; actorSize <= 1; actorSize++) {
		const int maxY = max[1] + actorSize;
		const int maxX = max[0] + actorSize;
		/* Offset the initial X and Y to compensate for larger actors when needed. */
		for (y = max(min[1] - actorSize + 1, 0); y < maxY; y++) {
			for (x = max(min[0] - actorSize + 1, 0); x < maxX; x++) {
				/** @note RT_CheckCell goes from top (7) to bottom (0) */
				for (z = max[2]; z >= 0; z--) {
					const int newZ = RT_CheckCell(map, actorSize, x, y, z);
					assert(newZ <= z);
					z = newZ;
				}
			}
		}
	}

#if 0
	Grid_DumpMap(map, (int)min[0], (int)min[1], (int)min[2], (int)max[0], (int)max[1], (int)max[2]);
#endif

	/* check connections */
	/** @todo A temporary hack- if we decrease ACTOR_MAX_SIZE we need to bump the BSPVERSION again.
	 * I'm just commenting out the CORRECT code for now. */
	/*for (actorSize = 1; actorSize <= ACTOR_MAX_SIZE; actorSize++) { */
	for (actorSize = 1; actorSize <= 1; actorSize++) {
		const int minX = max(min[0] - actorSize, 0);
		const int minY = max(min[1] - actorSize, 0);
		const int maxX = min(max[0] + actorSize, PATHFINDING_WIDTH - 1);
		const int maxY = min(max[1] + actorSize, PATHFINDING_WIDTH - 1);
		/* Offset the initial X and Y to compensate for larger actors when needed.
		 * Also sweep further out to catch the walls back into our box. */
		for (y = minY; y <= maxY; y++) {
			for (x = minX; x <= maxX; x++) {
				for (dir = 0; dir < CORE_DIRECTIONS; dir++) {
					/** @note The new version of RT_UpdateConnectionColumn can work bidirectional, so we can
					 * trace every other dir, unless we are on the edge. */
#if RT_IS_BIDIRECTIONAL == 1
					if ((dir & 1) && x != minX && x != maxX && y != minY && y != maxY)
						continue;
#endif
					RT_UpdateConnectionColumn(map, actorSize, x, y, dir);
				}
			}
		}
	}

#if 0
	Com_Printf("After:\n");
	Grid_DumpMap(map, (int)min[0], (int)min[1], (int)min[2], (int)max[0], (int)max[1], (int)max[2]);
#endif
}


/**
 * @brief This function recalculates the routing surrounding the entity name.
 * @sa CM_InlineModel
 * @sa CM_CheckUnit
 * @sa CM_UpdateConnection
 * @sa CMod_LoadSubmodels
 * @sa Grid_RecalcBoxRouting
 * @param[in] map The routing map (either server or client map)
 * @param[in] name Name of the inline model to compute the mins/maxs for
 * @param[in] list The local models list (a local model has a name starting with * followed by the model number)
 */
void Grid_RecalcRouting (routing_t *map, const char *name, const char **list)
{
	cBspModel_t *model;
	pos3_t min, max;
	unsigned int i;
	double start, end;

	start = time(NULL);

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

	Com_DPrintf(DEBUG_PATHING, "Model:%s origin(%f,%f,%f) angles(%f,%f,%f) mins(%f,%f,%f) maxs(%f,%f,%f)\n", name,
		model->origin[0], model->origin[1], model->origin[2],
		model->angles[0], model->angles[1], model->angles[2],
		model->mins[0], model->mins[1], model->mins[2],
		model->maxs[0], model->maxs[1], model->maxs[2]);

	CM_SetInlineList(list);

	/* get the target model's dimensions */
	if (VectorNotEmpty(model->angles)) {
		vec3_t minVec, maxVec;
		vec3_t centerVec, halfVec, newCenterVec;
		vec3_t m[3];

		/* Find the center of the extents. */
		VectorCenterFromMinsMaxs(model->mins, model->maxs, centerVec);

		/* Find the half height and half width of the extents. */
		VectorSubtract(model->maxs, centerVec, halfVec);

		/* Rotate the center about the origin. */
		VectorCreateRotationMatrix(model->angles, m);
		VectorRotate(m, centerVec, newCenterVec);

		/* Set minVec and maxVec to bound around newCenterVec at halfVec size. */
		VectorSubtract(newCenterVec, halfVec, minVec);
		VectorAdd(newCenterVec, halfVec, maxVec);

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
	max[0] = min(max[0] + 1, PATHFINDING_WIDTH - 1);
	max[1] = min(max[1] + 1, PATHFINDING_WIDTH - 1);
	max[2] = min(max[2] + 1, PATHFINDING_HEIGHT - 1);
	for (i = 0; i < 3; i++)
		min[i] = max(min[i] - 1, 0);

	/* We now have the dimensions, call the generic rerouting function. */
	Grid_RecalcBoxRouting(map, min, max);

	/* Reset the inlineList variable */
	CM_SetInlineList(NULL);

	end = time(NULL);
	Com_DPrintf(DEBUG_ROUTING, "Retracing for model %s between (%i, %i, %i) and (%i, %i %i) in %5.1fs\n",
			name, min[0], min[1], min[2], max[0], max[1], max[2], end - start);
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
 *
 * @brief Calculates parabola-type shot.
 * @param[in] from Starting position for calculations.
 * @param[in] at Ending position for calculations.
 * @param[in] speed Launch velocity.
 * @param[in] launched Set to true for grenade launchers.
 * @param[in] rolled Set to true for "roll" type shoot.
 * @param[in,out] v0 The velocity vector
 * @todo refactor and move me
 * @todo Com_GrenadeTarget() is called from CL_TargetingGrenade() with speed
 * param as (fireDef_s) fd->range (gi.GrenadeTarget, too), while it is being used here for speed
 * calculations - a bug or just misleading documentation?
 * @sa CL_TargetingGrenade
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
