/**
 * @file common/bsp.c
 * @brief bsp model loading
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
#include "qfiles.h"
#include "tracing.h"
#include "routing.h"
#include "../shared/parse.h"

/** @note this is a zeroed surface structure */
static cBspSurface_t nullSurface;

/*
===============================================================================
MAP LOADING
===============================================================================
*/

/**
 * @brief Loads brush entities like func_door and func_breakable
 * @param[in] tile Stores the data of the map tile
 * @param[in] base The start of the data loaded from the file.
 * @param[in] l The lump to load the data from
 * @param[in] shift The shifting vector in case this is a map assemble
 * @sa CM_AddMapTile
 * @sa R_ModLoadSubmodels
 * @sa CM_InlineModel
 */
static void CMod_LoadSubmodels (mapTile_t *tile, const byte *base, const lump_t * l, const vec3_t shift)
{
	const dBspModel_t *in;
	cBspModel_t *out;
	int i, j, count;

	if (!l)
		Com_Error(ERR_DROP, "CMod_LoadSubmodels: No lump given");

	in = (const dBspModel_t *) (base + l->fileofs);
	if (l->filelen % sizeof(*in))
		Com_Error(ERR_DROP, "CMod_LoadSubmodels: funny lump size (%i => "UFO_SIZE_T")", l->filelen, sizeof(*in));
	count = l->filelen / sizeof(*in);
	Com_DPrintf(DEBUG_ENGINE, S_COLOR_GREEN "...submodels: %i\n", count);

	if (count < 1)
		Com_Error(ERR_DROP, "Map with no models");
	if (count > MAX_MAP_MODELS)
		Com_Error(ERR_DROP, "Map has too many models: %i", count);

	out = (cBspModel_t *)Mem_PoolAlloc((count + 6) * sizeof(*out), com_cmodelSysPool, 0);
	tile->models = out;
	tile->nummodels = count;

	for (i = 0; i < count; i++, in++, out++) {
		out = &tile->models[i];

		/* Record the shift in case we need to undo it. */
		VectorCopy(shift, out->shift);
		/* spread the mins / maxs by a pixel */
		for (j = 0; j < 3; j++) {
			out->mins[j] = LittleFloat(in->mins[j]) - 1 + shift[j];
			out->maxs[j] = LittleFloat(in->maxs[j]) + 1 + shift[j];
		}
		out->headnode = LittleLong(in->headnode);
		out->tile = tile->idx; /* backlink to the loaded map tile */
	}
}


/**
 * @param[in] tile Stores the data of the map tile
 * @param[in] base The start of the data loaded from the file.
 * @param[in] l descriptor of the data block we are working on
 * @sa CM_AddMapTile
 */
static void CMod_LoadSurfaces (mapTile_t *tile, const byte *base, const lump_t * l)
{
	const dBspTexinfo_t *in;
	cBspSurface_t *out;
	int i, count;

	if (!l)
		Com_Error(ERR_DROP, "CMod_LoadSurfaces: No lump given");

	in = (const dBspTexinfo_t *) (base + l->fileofs);
	if (l->filelen % sizeof(*in))
		Com_Error(ERR_DROP, "CMod_LoadSurfaces: funny lump size: %i", l->filelen);
	count = l->filelen / sizeof(*in);
	Com_DPrintf(DEBUG_ENGINE, S_COLOR_GREEN "...surfaces: %i\n", count);

	if (count < 1)
		Com_Error(ERR_DROP, "Map with no surfaces");
	if (count > MAX_MAP_TEXINFO)
		Com_Error(ERR_DROP, "Map has too many surfaces");

	out = (cBspSurface_t *)Mem_PoolAlloc(count * sizeof(*out), com_cmodelSysPool, 0);

	tile->surfaces = out;
	tile->numtexinfo = count;

	for (i = 0; i < count; i++, in++, out++) {
		Q_strncpyz(out->name, in->texture, sizeof(out->name));
		out->surfaceFlags = LittleLong(in->surfaceFlags);
		out->value = LittleLong(in->value);
	}
}


/**
 * @param[in] tile Stores the data of the map tile
 * @param[in] base The start of the data loaded from the file.
 * @param[in] l descriptor of the data block we are working on
 * @param[in] shift The shifting vector in case this is a map assemble
 * @sa CM_AddMapTile
 * @sa TR_BuildTracingNode_r
 */
static void CMod_LoadNodes (mapTile_t *tile, const byte *base, const lump_t * l, const vec3_t shift)
{
	const dBspNode_t *in;
	int child;
	cBspNode_t *out;
	int i, j, count;

	if (!l)
		Com_Error(ERR_DROP, "CMod_LoadNodes: No lump given");

	in = (const dBspNode_t *) (base + l->fileofs);
	if (l->filelen % sizeof(*in))
		Com_Error(ERR_DROP, "CMod_LoadNodes: funny lump size: %i", l->filelen);
	count = l->filelen / sizeof(*in);
	Com_DPrintf(DEBUG_ENGINE, S_COLOR_GREEN "...nodes: %i\n", count);

	if (count < 1)
		Com_Error(ERR_DROP, "Map has no nodes");
	if (count > MAX_MAP_NODES)
		Com_Error(ERR_DROP, "Map has too many nodes");

	/* add some for the box */
	out = (cBspNode_t *)Mem_PoolAlloc((count + 6) * sizeof(*out), com_cmodelSysPool, 0);

	tile->numnodes = count;
	tile->nodes = out;

	for (i = 0; i < count; i++, out++, in++) {
		if (LittleLong(in->planenum) == PLANENUM_LEAF)
			out->plane = NULL;
		else
			out->plane = tile->planes + LittleLong(in->planenum);

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
 * @param[in] tile Stores the data of the map tile
 * @param[in] base The start of the data loaded from the file.
 * @param[in] l descriptor of the data block we are working on
 * @sa CM_AddMapTile
 */
static void CMod_LoadBrushes (mapTile_t *tile, const byte *base, const lump_t * l)
{
	const dBspBrush_t *in;
	cBspBrush_t *out;
	int i, count;

	if (!l)
		Com_Error(ERR_DROP, "CMod_LoadBrushes: No lump given");

	in = (const dBspBrush_t *) (base + l->fileofs);
	if (l->filelen % sizeof(*in))
		Com_Error(ERR_DROP, "CMod_LoadBrushes: funny lump size: %i", l->filelen);
	count = l->filelen / sizeof(*in);
	Com_DPrintf(DEBUG_ENGINE, S_COLOR_GREEN "...brushes: %i\n", count);

	if (count > MAX_MAP_BRUSHES)
		Com_Error(ERR_DROP, "Map has too many brushes");

	/* add some for the box */
	out = (cBspBrush_t *)Mem_PoolAlloc((count + 1) * sizeof(*out), com_cmodelSysPool, 0);

	tile->numbrushes = count;
	tile->brushes = out;

	for (i = 0; i < count; i++, out++, in++) {
		out->firstbrushside = LittleLong(in->firstbrushside);
		out->numsides = LittleLong(in->numsides);
		out->contentFlags = LittleLong(in->contentFlags);
	}
}

/**
 * @param[in] tile Stores the data of the map tile
 * @param[in] base The start of the data loaded from the file.
 * @param[in] l descriptor of the data block we are working on
 * @sa CM_AddMapTile
 */
static void CMod_LoadLeafs (mapTile_t *tile, const byte *base, const lump_t * l)
{
	int i;
	cBspLeaf_t *out;
	const dBspLeaf_t *in;
	int count;

	if (!l)
		Com_Error(ERR_DROP, "CMod_LoadLeafs: No lump given");

	in = (const dBspLeaf_t *) (base + l->fileofs);
	if (l->filelen % sizeof(*in))
		Com_Error(ERR_DROP, "CMod_LoadLeafs: funny lump size: %i", l->filelen);
	count = l->filelen / sizeof(*in);
	Com_DPrintf(DEBUG_ENGINE, S_COLOR_GREEN "...leafs: %i\n", count);

	if (count < 1)
		Com_Error(ERR_DROP, "Map with no leafs");
	/* need to save space for box planes */
	if (count > MAX_MAP_LEAFS)
		Com_Error(ERR_DROP, "Map has too many leafs");

	/* add some for the box */
	out = (cBspLeaf_t *)Mem_PoolAlloc((count + 1) * sizeof(*out), com_cmodelSysPool, 0);

	tile->numleafs = count;
	tile->leafs = out;

	for (i = 0; i < count; i++, in++, out++) {
		out->contentFlags = LittleLong(in->contentFlags);
		out->firstleafbrush = LittleShort(in->firstleafbrush);
		out->numleafbrushes = LittleShort(in->numleafbrushes);
	}

	if (tile->leafs[0].contentFlags != CONTENTS_SOLID)
		Com_Error(ERR_DROP, "Map leaf 0 is not CONTENTS_SOLID");
	tile->emptyleaf = -1;
	for (i = 1; i < tile->numleafs; i++) {
		if (!tile->leafs[i].contentFlags) {
			tile->emptyleaf = i;
			break;
		}
	}
	if (tile->emptyleaf == -1)
		Com_Error(ERR_DROP, "Map does not have an empty leaf");
}

/**
 * @param[in] tile Stores the data of the map tile
 * @param[in] base The start of the data loaded from the file.
 * @param[in] l descriptor of the data block we are working on
 * @param[in] shift The shifting vector in case this is a map assemble
 * @sa CM_AddMapTile
 * @sa R_ModLoadPlanes
 */
static void CMod_LoadPlanes (mapTile_t *tile, const byte *base, const lump_t * l, const vec3_t shift)
{
	int i, j;
	cBspPlane_t *out;
	const dBspPlane_t *in;
	int count;

	if (!l)
		Com_Error(ERR_DROP, "CMod_LoadPlanes: No lump given");

	in = (const dBspPlane_t *) (base + l->fileofs);
	if (l->filelen % sizeof(*in))
		Com_Error(ERR_DROP, "CMod_LoadPlanes: funny lump size: %i", l->filelen);
	count = l->filelen / sizeof(*in);
	Com_DPrintf(DEBUG_ENGINE, S_COLOR_GREEN "...planes: %i\n", count);

	if (count < 1)
		Com_Error(ERR_DROP, "Map with no planes");
	/* need to save space for box planes */
	if (count > MAX_MAP_PLANES)
		Com_Error(ERR_DROP, "Map has too many planes");

	/* add some for the box */
	out = (cBspPlane_t *)Mem_PoolAlloc((count + 12) * sizeof(*out), com_cmodelSysPool, 0);

	tile->numplanes = count;
	tile->planes = out;

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
 * @param[in] tile Stores the data of the map tile
 * @param[in] base The start of the data loaded from the file.
 * @param[in] l descriptor of the data block we are working on
 * @sa CM_AddMapTile
 */
static void CMod_LoadLeafBrushes (mapTile_t *tile, const byte *base, const lump_t * l)
{
	int i;
	unsigned short *out;
	const unsigned short *in;
	int count;

	if (!l)
		Com_Error(ERR_DROP, "CMod_LoadLeafBrushes: No lump given");

	in = (const unsigned short *) (base + l->fileofs);
	if (l->filelen % sizeof(unsigned short))
		Com_Error(ERR_DROP, "CMod_LoadLeafBrushes: funny lump size: %i", l->filelen);
	count = l->filelen / sizeof(unsigned short);
	Com_DPrintf(DEBUG_ENGINE, S_COLOR_GREEN "...leafbrushes: %i\n", count);

	/* add some for the box */
	out = (unsigned short *)Mem_PoolAlloc((count + 1) * sizeof(*out), com_cmodelSysPool, 0);

	if (count < 1)
		Com_Error(ERR_DROP, "Map with no planes");
	/* need to save space for box planes */
	if (count >= MAX_MAP_LEAFBRUSHES)
		Com_Error(ERR_DROP, "Map has too many leafbrushes");

	tile->numleafbrushes = count;
	tile->leafbrushes = out;

	for (i = 0; i < count; i++, in++, out++)
		*out = LittleShort(*in);
}

/**
 * @param[in] tile Stores the data of the map tile
 * @param[in] base The start of the data loaded from the file.
 * @param[in] l descriptor of the data block we are working on
 * @sa CM_AddMapTile
 */
static void CMod_LoadBrushSides (mapTile_t *tile, const byte *base, const lump_t * l)
{
	int i;
	cBspBrushSide_t *out;
	const dBspBrushSide_t *in;
	int count;

	if (!l)
		Com_Error(ERR_DROP, "CMod_LoadBrushSides: No lump given");

	in = (const dBspBrushSide_t *) (base + l->fileofs);
	if (l->filelen % sizeof(dBspBrushSide_t))
		Com_Error(ERR_DROP, "CMod_LoadBrushSides: funny lump size: %i", l->filelen);
	count = l->filelen / sizeof(dBspBrushSide_t);
	Com_DPrintf(DEBUG_ENGINE, S_COLOR_GREEN "...brushsides: %i\n", count);

	/* need to save space for box planes */
	if (count > MAX_MAP_BRUSHSIDES)
		Com_Error(ERR_DROP, "Map has too many brushsides");

	/* add some for the box */
	out = (cBspBrushSide_t *)Mem_PoolAlloc((count + 6) * sizeof(*out), com_cmodelSysPool, 0);

	tile->numbrushsides = count;
	tile->brushsides = out;

	for (i = 0; i < count; i++, in++, out++) {
		const int num = LittleShort(in->planenum);
		const int j = LittleShort(in->texinfo);
		if (j >= tile->numtexinfo)
			Com_Error(ERR_DROP, "Bad brushside texinfo");
		out->plane = &tile->planes[num];
		out->surface = &tile->surfaces[j];
	}
}

/**
 * @param[in] source Source will be set to the end of the compressed data block!
 * @param[in] dataStart where to place the uncompressed data
 * @sa CompressRouting (ufo2map)
 * @sa CMod_LoadRouting
 */
static int CMod_DeCompressRouting (const byte ** source, byte * dataStart)
{
	int i, c;
	byte *data_p;
	const byte *src;

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
 * @note tile->tnodes is expected to have enough memory malloc'ed for the function to work.
 * @sa BuildTracingNode_r
 */
static void CM_MakeTracingNodes (mapTile_t *tile)
{
	int i;
	tnode_t *tnode;

	tnode = tile->tnodes = (tnode_t *)Mem_PoolAlloc((tile->numnodes + 6) * sizeof(*tnode), com_cmodelSysPool, 0);

	tile->numtheads = 0;
	tile->numcheads = 0;

	for (i = 0; i < tile->nummodels; i++) {
		if (tile->models[i].headnode == LEAFNODE || tile->models[i].headnode >= tile->numnodes + 6)
			continue;

		tile->thead[tile->numtheads] = tnode - tile->tnodes;
		tile->theadlevel[tile->numtheads] = i;
		tile->numtheads++;
		assert(tile->numtheads < LEVEL_MAX);

		/* If this level (i) is the last visible level or earlier, then trace it.
		 * Otherwise don't; we have separate checks for entities. */
		if (i < NUM_REGULAR_MODELS)
			TR_BuildTracingNode_r(tile, &tnode, tile->models[i].headnode, i);
	}
}

/**
 * @param[in] tile Stores the data of the map tile
 * @param[in] base The start of the data loaded from the file.
 * @param[in] mapData The loaded data is stored here.
 * @param[in] name The name of the maptile
 * @param[in] l Routing lump ... (routing data lump from bsp file)
 * @param[in] sX The x position on the world plane (grid position) - values from -(PATHFINDING_WIDTH/2) up to PATHFINDING_WIDTH/2 are allowed
 * @param[in] sY The y position on the world plane (grid position) - values from -(PATHFINDING_WIDTH/2) up to PATHFINDING_WIDTH/2 are allowed
 * @param[in] sZ The height level on the world plane (grid position) - values from 0 - PATHFINDING_HEIGHT are allowed
 * @sa CM_AddMapTile
 * @todo TEST z-level routing
 */
static void CMod_LoadRouting (mapTile_t *tile, mapData_t *mapData, const byte *base, const char *name, const lump_t * l, const int sX, const int sY, const int sZ)
{
	/** @todo this eats a lot of memory - load directory into mapData->map */
	static routing_t tempMap[ACTOR_MAX_SIZE];
	const byte *source;
	int length;
	int x, y, z, size, dir;
	int minX, minY, minZ;
	int maxX, maxY, maxZ;
	unsigned int i;
	double start, end;
	const int targetLength = sizeof(tile->wpMins) + sizeof(tile->wpMaxs) + sizeof(tempMap);

	start = time(NULL);

	if (!l)
		Com_Error(ERR_DROP, "CMod_LoadRouting: No lump given");

	if (!l->filelen)
		Com_Error(ERR_DROP, "CMod_LoadRouting: Map has NO routing lump");

	assert((sX > -(PATHFINDING_WIDTH / 2)) && (sX < (PATHFINDING_WIDTH / 2)));
	assert((sY > -(PATHFINDING_WIDTH / 2)) && (sY < (PATHFINDING_WIDTH / 2)));
	assert((sZ >= 0) && (sZ < PATHFINDING_HEIGHT));

	source = base + l->fileofs;

	i = CMod_DeCompressRouting(&source, (byte*)tile->wpMins);
	length = i;
	i = CMod_DeCompressRouting(&source, (byte*)tile->wpMaxs);
	length += i;
	i = CMod_DeCompressRouting(&source, (byte*)tempMap);
	length += i;

	if (length != targetLength)
		Com_Error(ERR_DROP, "CMod_LoadRouting: Map has BAD routing lump; expected %i got %i", targetLength, length);

	/* endian swap possibly necessary */
	for (i = 0; i < 3; i++) {
		tile->wpMins[i] = LittleLong(tile->wpMins[i]);
		tile->wpMaxs[i] = LittleLong(tile->wpMaxs[i]);
	}

	Com_DPrintf(DEBUG_ROUTING, "Map:%s  Offset:(%i, %i, %i)\n", name, sX, sY, sZ);
	Com_DPrintf(DEBUG_ROUTING, "wpMins:(%i, %i, %i) wpMaxs:(%i, %i, %i)\n", tile->wpMins[0], tile->wpMins[1],
			tile->wpMins[2], tile->wpMaxs[0], tile->wpMaxs[1], tile->wpMaxs[2]);

	/* wpMins and wpMaxs have the map size data from the initial build.
	 * Offset this by the given parameters so the stored values are in real coordinates. */
	tile->wpMins[0] += sX;
	tile->wpMins[1] += sY;
	tile->wpMins[2] += sZ;
	tile->wpMaxs[0] += sX;
	tile->wpMaxs[1] += sY;
	tile->wpMaxs[2] += sZ;

	Com_DPrintf(DEBUG_ROUTING, "Shifted wpMins:(%i, %i, %i) wpMaxs:(%i, %i, %i)\n", tile->wpMins[0],
			tile->wpMins[1], tile->wpMins[2], tile->wpMaxs[0], tile->wpMaxs[1], tile->wpMaxs[2]);

	/* Things that need to be done:
	 * The floor, ceiling, and route data can be copied over from the map.
	 * All data must be regenerated for cells with overlapping content or where new
	 * model data is adjacent to a cell with existing model data. */

	/* Copy the routing information into our master table */
	minX = max(tile->wpMins[0], 0);
	minY = max(tile->wpMins[1], 0);
	minZ = max(tile->wpMins[2], 0);
	maxX = min(tile->wpMaxs[0], PATHFINDING_WIDTH - 1);
	maxY = min(tile->wpMaxs[1], PATHFINDING_WIDTH - 1);
	maxZ = min(tile->wpMaxs[2], PATHFINDING_HEIGHT - 1);

	assert(minX <= maxX);
	assert(minY <= maxY);
	assert(minZ <= maxZ);

	Com_DPrintf(DEBUG_ROUTING, "Tile bounds: (%i, %i, %i) to (%i, %i, %i)\n", minX, minY, minZ, maxX, maxY, maxZ);
	Com_DPrintf(DEBUG_ROUTING, "Source bounds: (%i, %i, %i) to (%i, %i, %i)\n", minX - sX, minY - sY, minZ - sZ,
			maxX - sX, maxY - sY, maxZ - sZ);

	for (size = 0; size < ACTOR_MAX_SIZE; size++)
		/* Adjust starting x and y by size to catch large actor cell overlap. */
		for (y = minY - size; y <= maxY; y++)
			for (x = minX - size; x <= maxX; x++) {
				/* Just incase x or y start negative. */
				if (x < 0 || y < 0)
					continue;
				for (z = minZ; z <= maxZ; z++) {
					mapData->map[size].floor[z][y][x] = tempMap[size].floor[z - sZ][y - sY][x - sX];
					mapData->map[size].ceil[z][y][x] = tempMap[size].ceil[z - sZ][y - sY][x - sX];
					for (dir = 0; dir < CORE_DIRECTIONS; dir++) {
						mapData->map[size].route[z][y][x][dir] = tempMap[size].route[z - sZ][y - sY][x - sX][dir];
						mapData->map[size].stepup[z][y][x][dir] = tempMap[size].stepup[z - sZ][y - sY][x - sX][dir];
					}
				}
				/* Update the reroute table */
				if (!mapData->reroute[size][y][x]) {
					mapData->reroute[size][y][x] = tile->idx + 1;
				} else {
					mapData->reroute[size][y][x] = ROUTING_NOT_REACHABLE;
				}
			}

	Com_DPrintf(DEBUG_ROUTING, "Done copying data.\n");

	end = time(NULL);
	Com_DPrintf(DEBUG_ROUTING, "Loaded routing for tile %s in %5.1fs\n", name, end - start);
}


/**
 * @note Transforms coordinates and stuff for assembled maps
 * @param[in] mapData The loaded data is stored here.
 * @param[in] tile Stores the data of the map tile
 * @param[in] base The start of the data loaded from the file.
 * @param[in] l descriptor of the data block we are working on
 * @param[in] shift The shifting vector in case this is a map assemble
 * loaded map tiles.
 * @sa CM_AddMapTile
 */
static void CMod_LoadEntityString (mapTile_t *tile, mapData_t *mapData, const byte *base, const lump_t * l, const vec3_t shift)
{
	const char *token;
	const char *es;
	char keyname[256];
	vec3_t v;

	if (!l)
		Com_Error(ERR_DROP, "CMod_LoadEntityString: No lump given");

	if (!l->filelen)
		Com_Error(ERR_DROP, "CMod_LoadEntityString: Map has NO entity lump");

	if (l->filelen + 1 > MAX_MAP_ENTSTRING)
		Com_Error(ERR_DROP, "CMod_LoadEntityString: Map has too large entity lump");

	/* merge entitystring information */
	es = (const char *) (base + l->fileofs);
	while (1) {
		cBspModel_t *model = NULL;
		/* parse the opening brace */
		token = Com_Parse(&es);
		if (!es)
			break;
		if (token[0] != '{')
			Com_Error(ERR_DROP, "CMod_LoadEntityString: found %s when expecting {", token);

		/* new entity */
		Q_strcat(mapData->mapEntityString, "{ ", sizeof(mapData->mapEntityString));

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
			if (Q_streq(keyname, "origin")) {
				/* origins are shifted */
				sscanf(token, "%f %f %f", &(v[0]), &(v[1]), &(v[2]));
				VectorAdd(v, shift, v);
				Q_strcat(mapData->mapEntityString, va("%s \"%f %f %f\" ", keyname, v[0], v[1], v[2]), sizeof(mapData->mapEntityString));
				/* If we have a model, then unadjust it's mins and maxs. */
				if (model) {
					VectorSubtract(model->mins, shift, model->mins);
					VectorSubtract(model->maxs, shift, model->maxs);
					model = NULL; /* reset it, or the next origin will shift it again */
				}
			} else if (Q_streq(keyname, "model") && token[0] == '*') {
				/* adapt inline model number */
				int num = atoi(token + 1);
				/* Get the model */
				model = &tile->models[NUM_REGULAR_MODELS + num - 1];
				/* Now update the model number to reflect prior tiles loaded. */
				num += mapData->numInline;
				Q_strcat(mapData->mapEntityString, va("%s *%i ", keyname, num), sizeof(mapData->mapEntityString));
			} else if (Q_streq(keyname, "targetname") || Q_streq(keyname, "target")) {
				Q_strcat(mapData->mapEntityString, va("%s \"%s-%i\" ", keyname, token, tile->idx), sizeof(mapData->mapEntityString));
			} else {
				/* just store key and value */
				Q_strcat(mapData->mapEntityString, va("%s \"%s\" ", keyname, token), sizeof(mapData->mapEntityString));
			}
		}

		/* finish entity */
		Q_strcat(mapData->mapEntityString, "} ", sizeof(mapData->mapEntityString));
	}
}

/**
 * @brief Loads the lightmap for server side visibility lookup
 * @todo Implement this
 */
static void CMod_LoadLighting (mapTile_t *tile, const byte *base, const lump_t * l)
{
	if (l->filelen == 0)
		return;

	tile->lightdata = Mem_PoolAlloc(l->filelen, com_cmodelSysPool, 0);
	memcpy(tile->lightdata, base + l->fileofs, l->filelen);
}

/**
 * @brief Set up the planes and nodes so that the six floats of a bounding
 * box can just be stored out and get a proper clipping hull structure.
 */
static void CM_InitBoxHull (mapTile_t *tile)
{
	int i;

	tile->box_headnode = tile->numnodes;
	tile->box_planes = &tile->planes[tile->numplanes];
	/* sanity check if you only use one maptile => no map assembly */
	if (tile->idx == 1 && (tile->numnodes + 6 > MAX_MAP_NODES
		|| tile->numbrushes + 1 > MAX_MAP_BRUSHES
		|| tile->numleafbrushes + 1 > MAX_MAP_LEAFBRUSHES
		|| tile->numbrushsides + 6 > MAX_MAP_BRUSHSIDES
		|| tile->numplanes + 12 > MAX_MAP_PLANES))
		Com_Error(ERR_DROP, "Not enough room for box tree");

	tile->box_brush = &tile->brushes[tile->numbrushes];
	tile->box_brush->numsides = 6;
	tile->box_brush->firstbrushside = tile->numbrushsides;
	tile->box_brush->contentFlags = CONTENTS_WEAPONCLIP;

	tile->box_leaf = &tile->leafs[tile->numleafs];
	tile->box_leaf->contentFlags = CONTENTS_WEAPONCLIP;
	tile->box_leaf->firstleafbrush = tile->numleafbrushes;
	tile->box_leaf->numleafbrushes = 1;

	tile->leafbrushes[tile->numleafbrushes] = tile->numbrushes;

	/* each side */
	for (i = 0; i < 6; i++) {
		const int side = i & 1;
		cBspNode_t *c;
		cBspPlane_t *p;
		cBspBrushSide_t *s;

		/* brush sides */
		s = &tile->brushsides[tile->numbrushsides + i];
		s->plane = tile->planes + (tile->numplanes + i * 2 + side);
		s->surface = &nullSurface;

		/* nodes */
		c = &tile->nodes[tile->box_headnode + i];
		c->plane = tile->planes + (tile->numplanes + i * 2);
		c->children[side] = -1 - tile->emptyleaf;
		if (i != 5)
			c->children[side ^ 1] = tile->box_headnode + i + 1;
		else
			c->children[side ^ 1] = LEAFNODE - tile->numleafs;

		/* planes */
		p = &tile->box_planes[i * 2];
		p->type = i >> 1;
		VectorClear(p->normal);
		p->normal[i >> 1] = 1;

		p = &tile->box_planes[i * 2 + 1];
		p->type = PLANE_ANYX + (i >> 1);
		VectorClear(p->normal);
		p->normal[i >> 1] = -1;
	}
}

/**
 * @brief Adds in a single map tile
 * @param[in] name The (file-)name of the tile to add.
 * @param[in] day whether the lighting for day or night should be loaded.
 * @param[in] sX The x position on the world plane (grid position) - values from -(PATHFINDING_WIDTH / 2) up to PATHFINDING_WIDTH / 2 are allowed
 * @param[in] sY The y position on the world plane (grid position) - values from -(PATHFINDING_WIDTH / 2) up to PATHFINDING_WIDTH / 2 are allowed
 * @param[in] sZ The height level on the world plane (grid position) - values from 0 up to PATHFINDING_HEIGHT are allowed
 * @param[in] mapData The loaded data is stored here.
 * @param[in] mapTiles List of tiles the current (RMA-)map is composed of
 * @note The sX and sY values are grid positions - max. grid size is PATHFINDING_WIDTH - unit size is
 * UNIT_SIZE => ends up at 2*MAX_WORLD_WIDTH (the worldplace size - or [-MAX_WORLD_WIDTH, MAX_WORLD_WIDTH])
 * @return The checksum of the maptile
 * @return 0 on error
 * @sa CM_LoadMap
 * @sa R_ModAddMapTile
 */
static void CM_AddMapTile (const char *name, const qboolean day, const int sX, const int sY, const byte sZ, mapData_t *mapData, mapTiles_t *mapTiles)
{
	char filename[MAX_QPATH];
	unsigned checksum;
	byte *buf;
	int length;
	dBspHeader_t header;
	/* use for random map assembly for shifting origins and so on */
	vec3_t shift;
	const byte *base;
	mapTile_t *tile;

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
	BSP_SwapHeader(&header, filename);

	if (header.version != BSPVERSION)
		Com_Error(ERR_DROP, "CM_AddMapTile: %s has wrong version number (%i should be %i)", name, header.version, BSPVERSION);

	base = (const byte *) buf;

	/* init */
	if (mapTiles->numTiles >= MAX_MAPTILES)
		Com_Error(ERR_FATAL, "CM_AddMapTile: too many tiles loaded %i", mapTiles->numTiles);

	tile = &(mapTiles->mapTiles[mapTiles->numTiles]);
	OBJZERO(*tile);
	tile->idx = mapTiles->numTiles;
	Q_strncpyz(tile->name, name, sizeof(tile->name));

	/* pathfinding and the like must be shifted on the worldplane when we
	 * are assembling a map */
	VectorSet(shift, sX * UNIT_SIZE, sY * UNIT_SIZE, sZ * UNIT_HEIGHT);

	/* load into heap */
	CMod_LoadSurfaces(tile, base, &header.lumps[LUMP_TEXINFO]);
	CMod_LoadLeafs(tile, base, &header.lumps[LUMP_LEAFS]);
	CMod_LoadLeafBrushes(tile, base, &header.lumps[LUMP_LEAFBRUSHES]);
	CMod_LoadPlanes(tile, base, &header.lumps[LUMP_PLANES], shift);
	CMod_LoadBrushes(tile, base, &header.lumps[LUMP_BRUSHES]);
	CMod_LoadBrushSides(tile, base, &header.lumps[LUMP_BRUSHSIDES]);
	CMod_LoadSubmodels(tile, base, &header.lumps[LUMP_MODELS], shift);
	CMod_LoadNodes(tile, base, &header.lumps[LUMP_NODES], shift);
	CMod_LoadEntityString(tile, mapData, base, &header.lumps[LUMP_ENTITIES], shift);
	if (day)
		CMod_LoadLighting(tile, base, &header.lumps[LUMP_LIGHTING_DAY]);
	else
		CMod_LoadLighting(tile, base, &header.lumps[LUMP_LIGHTING_NIGHT]);

	CM_InitBoxHull(tile);
	CM_MakeTracingNodes(tile);

	mapData->numInline += tile->nummodels - NUM_REGULAR_MODELS;

	CMod_LoadRouting(tile, mapData, base, name, &header.lumps[LUMP_ROUTING], sX, sY, sZ);

	/* now increase the amount of loaded tiles */
	mapTiles->numTiles++;

	/* Now find the map bounds with the updated numTiles. */
	/* calculate new border after merge */
	RT_GetMapSize(mapTiles, mapData->mapMin, mapData->mapMax);

	FS_FreeFile(buf);

	mapData->mapChecksum += checksum;
}

static void CMod_RerouteMap (mapTiles_t *mapTiles, mapData_t *mapData)
{
	actorSizeEnum_t size;
	int x, y, z, dir;
	int i;
	pos3_t mins, maxs;
	double start, end;

	start = time(NULL);

	VecToPos(mapData->mapMin, mins);
	VecToPos(mapData->mapMax, maxs);

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
				if (mapData->reroute[size][y][x] == ROUTING_NOT_REACHABLE) {
					/* Com_Printf("Tracing floor (%i %i s:%i)\n", x, y, size); */
					for (z = maxs[2]; z >= mins[2]; z--) {
						const int newZ = RT_CheckCell(mapTiles, mapData->map, size + 1, x, y, z, NULL);
						assert(newZ <= z);
						z = newZ;
					}
				}
			}
		}
	}

	/* Wall pass */
	for (size = ACTOR_SIZE_INVALID; size < ACTOR_MAX_SIZE; size++) {
		for (y = mins[1]; y <= maxs[1]; y++) {
			for (x = mins[0]; x <= maxs[0]; x++) {
				const byte tile = mapData->reroute[size][y][x];
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
						tile2 = mapData->reroute[size][dy][dx];
						/* Both cells are present and if either cell is ROUTING_NOT_REACHABLE or if the cells are different. */
						if (tile2 && (tile2 == ROUTING_NOT_REACHABLE || tile2 != tile)) {
							/** @note This update MUST go from the bottom (0) to the top (7) of the model.
							 * RT_UpdateConnection expects it and breaks otherwise. */
							/* Com_Printf("Tracing passage (%i %i s:%i d:%i)\n", x, y, size, dir); */
							RT_UpdateConnectionColumn(mapTiles, mapData->map, size + 1, x, y, dir, NULL);
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
 * @param[out] mapData The loaded data is stored here.
 * @param[in] mapTiles List of tiles the current (RMA-)map is composed of
 * @sa CM_AddMapTile
 * @sa R_ModBeginLoading
 */
void CM_LoadMap (const char *tiles, qboolean day, const char *pos, mapData_t *mapData, mapTiles_t *mapTiles)
{
	const char *token;
	char name[MAX_VAR];
	char base[MAX_QPATH];
	int i;

	Mem_FreePool(com_cmodelSysPool);

	/* init */
	base[0] = 0;

	/* Reset the map related data */
	OBJZERO(*mapData);
	OBJZERO(*mapTiles);

	if (pos && *pos)
		Com_Printf("CM_LoadMap: \"%s\" \"%s\"\n", tiles, pos);

	/* load tiles */
	while (tiles) {
		/* get tile name */
		token = Com_Parse(&tiles);
		if (!tiles) {
			CMod_RerouteMap(mapTiles, mapData);
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
			ipos3_t sh;
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
			CM_AddMapTile(name, day, sh[0], sh[1], sh[2], mapData, mapTiles);
		} else {
			/* load only a single tile, if no positions are specified */
			CM_AddMapTile(name, day, 0, 0, 0, mapData, mapTiles);
			return;
		}
	}

	Com_Error(ERR_DROP, "CM_LoadMap: invalid tile names");
}

/**
 * @brief Searches all inline models and return the cBspModel_t pointer for the
 * given modelnumber or -name
 * @param[in] mapTiles List of tiles the current (RMA-)map is composed of
 * @param[in] name The modelnumber (e.g. "*2") for inline brush models [bmodels]
 * @note Inline bmodels are e.g. the brushes that are assoziated with a func_breakable or func_door
 */
cBspModel_t *CM_InlineModel (const mapTiles_t *mapTiles, const char *name)
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
	for (i = 0; i < mapTiles->numTiles; i++) {
		const int models = mapTiles->mapTiles[i].nummodels - NUM_REGULAR_MODELS;
		assert(models >= 0);

		if (num >= models)
			num -= models;
		else
			return &(mapTiles->mapTiles[i].models[NUM_REGULAR_MODELS + num]);
	}

	Com_Error(ERR_DROP, "CM_InlineModel: Error cannot find model '%s'\n", name);
}

/**
 * @brief This function updates a model's orientation
 * @param[in] mapTiles List of tiles the current (RMA-)map is composed of
 * @param[in] name The name of the model, must include the '*'
 * @param[in] origin The new origin for the model
 * @param[in] angles The new facing angles for the model
 * @note This is used whenever a model's orientation changes, e.g. for func_doors and func_rotating models
 * @sa LE_DoorAction
 * @sa G_ClientUseEdict
 */
cBspModel_t * CM_SetInlineModelOrientation (mapTiles_t *mapTiles, const char *name, const vec3_t origin, const vec3_t angles)
{
	cBspModel_t *model = CM_InlineModel(mapTiles, name);
	assert(model);
	VectorCopy(origin, model->origin);
	VectorCopy(angles, model->angles);

	return model;
}

/**
 * @brief Checks how well a position is visible
 * @return a visibility factor. @c 1.0 means fully visible, @c 0.0 means hardly visible because the
 * given position is in the darkness
 */
float CM_GetVisibility (const mapTiles_t *mapTiles, const pos3_t position)
{
	int i;

	for (i = 0; i < mapTiles->numTiles; i++) {
		const mapTile_t *tile = &mapTiles->mapTiles[i];
		if (VectorInside(position, tile->wpMins, tile->wpMaxs)) {
			if (tile->lightdata == NULL)
				return 1.0f;
			/** @todo implement me */
			return 1.0f;
		}
	}

	/* point is outside of any loaded tile */
	Com_Printf("given point %i:%i:%i is not inside of any loaded tile\n", position[0], position[1], position[2]);
	return 1.0f;
}
