/**
 * @file
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

static int CMod_ValidateLump (const lump_t* lump, const char* functionName, size_t elementSize, const char* elementName, int elementMaxCount)
{
	if (!lump)
		Com_Error(ERR_DROP, "%s: No lump given", functionName);
	if (lump->filelen % elementSize)
		Com_Error(ERR_DROP, "%s: funny lump size (%i => " UFO_SIZE_T ")", functionName, lump->filelen, elementSize);
	int count = lump->filelen / elementSize;
	Com_DPrintf(DEBUG_ENGINE, S_COLOR_GREEN "...%s: %i\n", elementName, count);

	if (count < 1)
		Com_Error(ERR_DROP, "Map with no %s", elementName);
	if (count > elementMaxCount)
		Com_Error(ERR_DROP, "Map has too many %s: %i", elementName, count);

	return count;
}

/**
 * @brief Loads brush entities like func_door and func_breakable
 * @param[in] tile Stores the data of the map tile
 * @param[in] base The start of the data loaded from the file.
 * @param[in] lump The lump to load the data from
 * @param[in] shift The shifting vector in case this is a map assemble
 * @sa CM_AddMapTile
 * @sa R_ModLoadSubmodels
 * @sa CM_InlineModel
 */
static void CMod_LoadSubmodels (MapTile& tile, const byte* base, const lump_t* lump, const vec3_t shift)
{
	int count = CMod_ValidateLump(lump, __FUNCTION__, sizeof(dBspModel_t), "models", MAX_MAP_MODELS);

	const dBspModel_t* in = (const dBspModel_t*) (base + lump->fileofs);

	cBspModel_t* out = Mem_PoolAllocTypeN(cBspModel_t, count + 6, com_cmodelSysPool);
	tile.models = out;
	tile.nummodels = count;

	for (int i = 0; i < count; i++, in++, out++) {
		/* Record the shift in case we need to undo it. */
		VectorCopy(shift, out->shift);
		/* spread the mins / maxs by a pixel */
		for (int j = 0; j < 3; j++) {
			out->cbmBox.mins[j] = LittleFloat(in->mins[j]) - 1 + shift[j];
			out->cbmBox.maxs[j] = LittleFloat(in->maxs[j]) + 1 + shift[j];
		}
		out->headnode = LittleLong(in->headnode);
		out->tile = tile.idx; /* backlink to the loaded map tile */
	}
}


/**
 * @param[in] tile Stores the data of the map tile
 * @param[in] base The start of the data loaded from the file.
 * @param[in] lump descriptor of the data block we are working on
 * @sa CM_AddMapTile
 */
static void CMod_LoadSurfaces (MapTile& tile, const byte* base, const lump_t* lump)
{
	int count = CMod_ValidateLump(lump, __FUNCTION__, sizeof(dBspTexinfo_t), "surfaces", MAX_MAP_TEXINFO);

	const dBspTexinfo_t* in = (const dBspTexinfo_t*) (base + lump->fileofs);

	cBspSurface_t* out = Mem_PoolAllocTypeN(cBspSurface_t, count, com_cmodelSysPool);

	tile.surfaces = out;
	tile.numtexinfo = count;

	for (int i = 0; i < count; i++, in++, out++) {
		Q_strncpyz(out->name, in->texture, sizeof(out->name));
		out->surfaceFlags = LittleLong(in->surfaceFlags);
		out->value = LittleLong(in->value);
	}
}


/**
 * @param[in] tile Stores the data of the map tile
 * @param[in] base The start of the data loaded from the file.
 * @param[in] lump descriptor of the data block we are working on
 * @param[in] shift The shifting vector in case this is a map assemble
 * @sa CM_AddMapTile
 * @sa TR_BuildTracingNode_r
 */
static void CMod_LoadNodes (MapTile& tile, const byte* base, const lump_t* lump, const vec3_t shift)
{
	int count = CMod_ValidateLump(lump, __FUNCTION__, sizeof(dBspNode_t), "nodes", MAX_MAP_NODES);

	const dBspNode_t* in = (const dBspNode_t*) (base + lump->fileofs);

	/* add some for the box */
	cBspNode_t* out = Mem_PoolAllocTypeN(cBspNode_t, count + 6, com_cmodelSysPool);

	tile.numnodes = count;
	tile.nodes = out;

	for (int i = 0; i < count; i++, out++, in++) {
		if (LittleLong(in->planenum) == PLANENUM_LEAF)
			out->plane = nullptr;
		else
			out->plane = tile.planes + LittleLong(in->planenum);

		/* in case this is a map assemble */
		int j;
		for (j = 0; j < 3; j++) {
			out->mins[j] = LittleShort(in->mins[j]) + shift[j];
			out->maxs[j] = LittleShort(in->maxs[j]) + shift[j];
		}

		for (j = 0; j < 2; j++) {
			int child = LittleLong(in->children[j]);
			out->children[j] = child;
		}
	}
}

/**
 * @param[in] tile Stores the data of the map tile
 * @param[in] base The start of the data loaded from the file.
 * @param[in] lump descriptor of the data block we are working on
 * @sa CM_AddMapTile
 */
static void CMod_LoadBrushes (MapTile& tile, const byte* base, const lump_t* lump)
{
	int count = CMod_ValidateLump(lump, __FUNCTION__, sizeof(dBspBrush_t), "brushes", MAX_MAP_BRUSHES);

	const dBspBrush_t* in = (const dBspBrush_t*) (base + lump->fileofs);

	/* add some for the box */
	cBspBrush_t* out = Mem_PoolAllocTypeN(cBspBrush_t, count + 1, com_cmodelSysPool);

	tile.numbrushes = count;
	tile.brushes = out;

	for (int i = 0; i < count; i++, out++, in++) {
		out->firstbrushside = LittleLong(in->firstbrushside);
		out->numsides = LittleLong(in->numsides);
		out->brushContentFlags = LittleLong(in->brushContentFlags);
	}
}

/**
 * @param[in] tile Stores the data of the map tile
 * @param[in] base The start of the data loaded from the file.
 * @param[in] lump descriptor of the data block we are working on
 * @sa CM_AddMapTile
 */
static void CMod_LoadLeafs (MapTile& tile, const byte* base, const lump_t* lump)
{
	int count = CMod_ValidateLump(lump, __FUNCTION__, sizeof(dBspLeaf_t), "leafs", MAX_MAP_LEAFS);

	const dBspLeaf_t* in = (const dBspLeaf_t*) (base + lump->fileofs);

	/* add some for the box */
	cBspLeaf_t* out = Mem_PoolAllocTypeN(cBspLeaf_t, count + 1, com_cmodelSysPool);

	tile.numleafs = count;
	tile.leafs = out;

	int i;

	for (i = 0; i < count; i++, in++, out++) {
		out->contentFlags = LittleLong(in->contentFlags);
		out->firstleafbrush = LittleShort(in->firstleafbrush);
		out->numleafbrushes = LittleShort(in->numleafbrushes);
	}

	if (tile.leafs[0].contentFlags != CONTENTS_SOLID)
		Com_Error(ERR_DROP, "Map leaf 0 is not CONTENTS_SOLID");
	tile.emptyleaf = -1;
	for (i = 1; i < tile.numleafs; i++) {
		if (!tile.leafs[i].contentFlags) {
			tile.emptyleaf = i;
			break;
		}
	}
	if (tile.emptyleaf == -1)
		Com_Error(ERR_DROP, "Map does not have an empty leaf");
}

/**
 * @param[in] tile Stores the data of the map tile
 * @param[in] base The start of the data loaded from the file.
 * @param[in] lump descriptor of the data block we are working on
 * @param[in] shift The shifting vector in case this is a map assemble
 * @sa CM_AddMapTile
 * @sa R_ModLoadPlanes
 */
static void CMod_LoadPlanes (MapTile& tile, const byte* base, const lump_t* lump, const vec3_t shift)
{
	int count = CMod_ValidateLump(lump, __FUNCTION__, sizeof(dBspPlane_t), "planes", MAX_MAP_PLANES);

	const dBspPlane_t* in = (const dBspPlane_t*) (base + lump->fileofs);

	/* add some for the box */
	cBspPlane_t* out = Mem_PoolAllocTypeN(cBspPlane_t, count + 12, com_cmodelSysPool);

	tile.numplanes = count;
	tile.planes = out;

	for (int i = 0; i < count; i++, in++, out++) {
		out->dist = LittleFloat(in->dist);
		out->type = LittleLong(in->type);

		/* load normals and shift (map assembly) */
		for (int j = 0; j < 3; j++) {
			out->normal[j] = LittleFloat(in->normal[j]);
			out->dist += out->normal[j] * shift[j];
		}
	}
}

/**
 * @param[in] tile Stores the data of the map tile
 * @param[in] base The start of the data loaded from the file.
 * @param[in] lump descriptor of the data block we are working on
 * @sa CM_AddMapTile
 */
static void CMod_LoadLeafBrushes (MapTile& tile, const byte* base, const lump_t* lump)
{
	int count = CMod_ValidateLump(lump, __FUNCTION__, sizeof(unsigned short), "leafbrushes", MAX_MAP_LEAFBRUSHES);

	const unsigned short* in = (const unsigned short*) (base + lump->fileofs);

	/* add some for the box */
	unsigned short* out = Mem_PoolAllocTypeN(unsigned short, count + 1, com_cmodelSysPool);

	tile.numleafbrushes = count;
	tile.leafbrushes = out;

	for (int i = 0; i < count; i++, in++, out++)
		*out = LittleShort(*in);
}

/**
 * @param[in] tile Stores the data of the map tile
 * @param[in] base The start of the data loaded from the file.
 * @param[in] lump descriptor of the data block we are working on
 * @sa CM_AddMapTile
 */
static void CMod_LoadBrushSides (MapTile& tile, const byte* base, const lump_t* lump)
{
	int count = CMod_ValidateLump(lump, __FUNCTION__, sizeof(dBspBrushSide_t), "brushsides", MAX_MAP_BRUSHSIDES);

	const dBspBrushSide_t* in = (const dBspBrushSide_t*) (base + lump->fileofs);

	/* add some for the box */
	cBspBrushSide_t* out = Mem_PoolAllocTypeN(cBspBrushSide_t, count + 6, com_cmodelSysPool);

	tile.numbrushsides = count;
	tile.brushsides = out;

	for (int i = 0; i < count; i++, in++, out++) {
		const int num = LittleShort(in->planenum);
		const int j = LittleShort(in->texinfo);
		if (j >= tile.numtexinfo)
			Com_Error(ERR_DROP, "Bad brushside texinfo");
		out->plane = &tile.planes[num];
		out->surface = &tile.surfaces[j];
	}
}

/**
 * @param[in] source Source will be set to the end of the compressed data block!
 * @param[in] dataStart where to place the uncompressed data
 * @sa CompressRouting (ufo2map)
 * @sa CMod_LoadRouting
 */
static int CMod_DeCompressRouting (const byte**  source, byte*  dataStart)
{
	byte* data_p = dataStart;
	const byte* src = *source;

	while (*src) {
		int i, c;

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
static void CM_MakeTracingNodes (MapTile& tile)
{
	tnode_t* tnode = tile.tnodes = Mem_PoolAllocTypeN(tnode_t, tile.numnodes + 6, com_cmodelSysPool);

	tile.numtheads = 0;
	tile.numcheads = 0;

	for (int i = 0; i < tile.nummodels; i++) {
		if (tile.models[i].headnode == LEAFNODE || tile.models[i].headnode >= tile.numnodes + 6)
			continue;

		tile.thead[tile.numtheads] = tnode - tile.tnodes;
		tile.theadlevel[tile.numtheads] = i;
		tile.numtheads++;
		assert(tile.numtheads < LEVEL_MAX);

		/* If this level (i) is the last visible level or earlier, then trace it.
		 * Otherwise don't; we have separate checks for entities. */
		if (i < NUM_REGULAR_MODELS)
			TR_BuildTracingNode_r(&tile, &tnode, tile.models[i].headnode, i);
	}
}

/**
 * @param[in] tile Stores the data of the map tile
 * @param[in] base The start of the data loaded from the file.
 * @param[in] mapData The loaded data is stored here.
 * @param[in] name The name of the maptile
 * @param[in] lump Routing lump ... (routing data lump from bsp file)
 * @param[in] sX The x position on the world plane (grid position) - values from -(PATHFINDING_WIDTH/2) up to PATHFINDING_WIDTH/2 are allowed
 * @param[in] sY The y position on the world plane (grid position) - values from -(PATHFINDING_WIDTH/2) up to PATHFINDING_WIDTH/2 are allowed
 * @param[in] sZ The height level on the world plane (grid position) - values from 0 - PATHFINDING_HEIGHT are allowed
 * @sa CM_AddMapTile
 * @todo TEST z-level routing
 */
static void CMod_LoadRouting (MapTile& tile, mapData_t* mapData, const byte* base, const char* name, const lump_t* lump, const int sX, const int sY, const int sZ)
{
	/** @todo this eats a lot of memory - load directory into mapData->map */
	Routing* tempMap = static_cast<Routing*>(Mem_Alloc(sizeof(Routing)));
	const int targetLength = sizeof(tile.wpMins) + sizeof(tile.wpMaxs) + sizeof(Routing);

	double start = time(nullptr);

	if (!lump)
		Com_Error(ERR_DROP, "CMod_LoadRouting: No lump given");

	if (!lump->filelen)
		Com_Error(ERR_DROP, "CMod_LoadRouting: Map has NO routing lump");

	assert((sX > -(PATHFINDING_WIDTH / 2)) && (sX < (PATHFINDING_WIDTH / 2)));
	assert((sY > -(PATHFINDING_WIDTH / 2)) && (sY < (PATHFINDING_WIDTH / 2)));
	assert((sZ >= 0) && (sZ < PATHFINDING_HEIGHT));

	const byte* source = base + lump->fileofs;

	int length = CMod_DeCompressRouting(&source, (byte*)tile.wpMins);
	length += CMod_DeCompressRouting(&source, (byte*)tile.wpMaxs);
	length += CMod_DeCompressRouting(&source, (byte*)tempMap);

	if (length != targetLength)
		Com_Error(ERR_DROP, "CMod_LoadRouting: Map has BAD routing lump; expected %i got %i", targetLength, length);

	/* endian swap possibly necessary */
	for (int i = 0; i < 3; i++) {
		tile.wpMins[i] = LittleLong(tile.wpMins[i]);
		tile.wpMaxs[i] = LittleLong(tile.wpMaxs[i]);
	}

	Com_DPrintf(DEBUG_ROUTING, "Map:%s  Offset:(%i, %i, %i)\n", name, sX, sY, sZ);
	Com_DPrintf(DEBUG_ROUTING, "wpMins:(%i, %i, %i) wpMaxs:(%i, %i, %i)\n", tile.wpMins[0], tile.wpMins[1],
			tile.wpMins[2], tile.wpMaxs[0], tile.wpMaxs[1], tile.wpMaxs[2]);

	/* wpMins and wpMaxs have the map size data from the initial build.
	 * Offset this by the given parameters so the stored values are in real coordinates. */
	tile.wpMins[0] += sX;
	tile.wpMins[1] += sY;
	tile.wpMins[2] += sZ;
	tile.wpMaxs[0] += sX;
	tile.wpMaxs[1] += sY;
	tile.wpMaxs[2] += sZ;

	Com_DPrintf(DEBUG_ROUTING, "Shifted wpMins:(%i, %i, %i) wpMaxs:(%i, %i, %i)\n", tile.wpMins[0],
			tile.wpMins[1], tile.wpMins[2], tile.wpMaxs[0], tile.wpMaxs[1], tile.wpMaxs[2]);

	/* Things that need to be done:
	 * The floor, ceiling, and route data can be copied over from the map.
	 * All data must be regenerated for cells with overlapping content or where new
	 * model data is adjacent to a cell with existing model data. */

	/* Copy the routing information into our master table */
	int minX = std::max(tile.wpMins[0], 0);
	int minY = std::max(tile.wpMins[1], 0);
	int minZ = std::max(tile.wpMins[2], 0);
	int maxX = std::min(tile.wpMaxs[0], PATHFINDING_WIDTH - 1);
	int maxY = std::min(tile.wpMaxs[1], PATHFINDING_WIDTH - 1);
	int maxZ = std::min(tile.wpMaxs[2], PATHFINDING_HEIGHT - 1);

	assert(minX <= maxX);
	assert(minY <= maxY);
	assert(minZ <= maxZ);

	Com_DPrintf(DEBUG_ROUTING, "Tile bounds: (%i, %i, %i) to (%i, %i, %i)\n", minX, minY, minZ, maxX, maxY, maxZ);
	Com_DPrintf(DEBUG_ROUTING, "Source bounds: (%i, %i, %i) to (%i, %i, %i)\n", minX - sX, minY - sY, minZ - sZ,
			maxX - sX, maxY - sY, maxZ - sZ);

	for (actorSizeEnum_t size = 0; size < ACTOR_MAX_SIZE; size++)
		/* Adjust starting x and y by size to catch large actor cell overlap. */
		for (int y = minY - size; y <= maxY; y++)
			for (int x = minX - size; x <= maxX; x++) {
				/* Just incase x or y start negative. */
				if (x < 0 || y < 0)
					continue;
				for (int z = minZ; z <= maxZ; z++) {
					mapData->routing.copyPosData(*tempMap, size + 1, x, y, z, sX, sY, sZ);
				}
				/* Update the reroute table */
				if (!mapData->reroute[size][y][x]) {
					mapData->reroute[size][y][x] = tile.idx + 1;
				} else {
					mapData->reroute[size][y][x] = ROUTING_NOT_REACHABLE;
				}
			}

	Com_DPrintf(DEBUG_ROUTING, "Done copying data.\n");

	double end = time(nullptr);
	Com_DPrintf(DEBUG_ROUTING, "Loaded routing for tile %s in %5.1fs\n", name, end - start);

	Mem_Free(tempMap);
}


/**
 * @note Transforms coordinates and stuff for assembled maps
 * @param[in] mapData The loaded data is stored here.
 * @param[in] tile Stores the data of the map tile
 * @param[in] entityString If not empty, this is added to mapEntityString
 * @param[in] base The start of the data loaded from the file.
 * @param[in] l descriptor of the data block we are working on
 * @param[in] shift The shifting vector in case this is a map assemble
 * loaded map tiles.
 * @sa CM_AddMapTile
 */
static void CMod_LoadEntityString (MapTile& tile, const char* entityString, mapData_t* mapData, const byte* base, const lump_t* l, const vec3_t shift)
{
	if (!l)
		Com_Error(ERR_DROP, "CMod_LoadEntityString: No lump given (entitystring: '%s')", entityString ? entityString : "none");

	if (!l->filelen)
		Com_Error(ERR_DROP, "CMod_LoadEntityString: Map has NO entity lump (offset: %u, length: %u, entitystring: '%s')", l->fileofs, l->filelen, entityString ? entityString : "none");

	if (l->filelen + 1 > MAX_MAP_ENTSTRING)
		Com_Error(ERR_DROP, "CMod_LoadEntityString: Map has too large entity lump (offset: %u, length: %u, entitystring: '%s')", l->fileofs, l->filelen, entityString ? entityString : "none");

	/* merge entitystring information */
	const char* es = (const char*) (base + l->fileofs);
	while (1) {
		cBspModel_t* model = nullptr;
		/* parse the opening brace */
		const char* token = Com_Parse(&es);
		if (!es)
			break;
		if (token[0] != '{')
			Com_Error(ERR_DROP, "CMod_LoadEntityString: found %s when expecting { (offset: %u, length: %u, remaining: '%s')", token, l->fileofs, l->filelen, es);

		/* new entity */
		Q_strcat(mapData->mapEntityString, sizeof(mapData->mapEntityString), "{ ");

		/* go through all the dictionary pairs */
		while (1) {
			/* parse key */
			token = Com_Parse(&es);
			if (token[0] == '}')
				break;
			if (!es)
				Com_Error(ERR_DROP, "CMod_LoadEntityString: EOF without closing brace");

			char keyname[256];
			Q_strncpyz(keyname, token, sizeof(keyname));

			/* parse value */
			token = Com_Parse(&es);
			if (!es)
				Com_Error(ERR_DROP, "CMod_LoadEntityString: EOF without closing brace");

			if (token[0] == '}')
				Com_Error(ERR_DROP, "CMod_LoadEntityString: closing brace without data for keyname %s", keyname);

			/* alter value, if needed */
			if (Q_streq(keyname, "origin")) {
				/* origins are shifted */
				vec3_t v;
				if (sscanf(token, "%f %f %f", &(v[0]), &(v[1]), &(v[2])) != 3)
					Com_Error(ERR_DROP, "CMod_LoadEntityString: invalid origin token '%s' for keyname %s", token, keyname);
				VectorAdd(v, shift, v);
				Q_strcat(mapData->mapEntityString, sizeof(mapData->mapEntityString), "%s \"%f %f %f\" ", keyname, v[0], v[1], v[2]);
				/* If we have a model, then unadjust it's mins and maxs. */
				if (model) {
					VectorSubtract(model->cbmBox.mins, shift, model->cbmBox.mins);
					VectorSubtract(model->cbmBox.maxs, shift, model->cbmBox.maxs);
					model = nullptr; /* reset it, or the next origin will shift it again */
				}
			} else if (Q_streq(keyname, "model") && token[0] == '*') {
				/* adapt inline model number */
				int num = atoi(token + 1);
				/* Get the model */
				model = &tile.models[NUM_REGULAR_MODELS + num - 1];
				/* Now update the model number to reflect prior tiles loaded. */
				num += mapData->numInline;
				Q_strcat(mapData->mapEntityString, sizeof(mapData->mapEntityString), "%s *%i ", keyname, num);
			} else if (Q_streq(keyname, "targetname") || Q_streq(keyname, "target")) {
				Q_strcat(mapData->mapEntityString, sizeof(mapData->mapEntityString), "%s \"%s-%i\" ", keyname, token, tile.idx);
			} else {
				if (Q_streq(keyname, "classname") && Q_streq(token, "worldspawn")) {
					if (Q_strvalid(entityString))
						Q_strcat(mapData->mapEntityString, sizeof(mapData->mapEntityString), "%s", entityString);
				}
				/* just store key and value */
				Q_strcat(mapData->mapEntityString, sizeof(mapData->mapEntityString), "%s \"%s\" ", keyname, token);
			}
		}

		/* finish entity */
		Q_strcat(mapData->mapEntityString, sizeof(mapData->mapEntityString), "} ");
	}
}

/**
 * @brief Loads the lightmap for server side visibility lookup
 * @todo Implement this
 */
static void CMod_LoadLighting (MapTile& tile, const byte* base, const lump_t* l)
{
	if (l->filelen == 0)
		return;

	tile.lightdata = Mem_PoolAllocTypeN(byte, l->filelen, com_cmodelSysPool);
	memcpy(tile.lightdata, base + l->fileofs, l->filelen);
}

/**
 * @brief Set up the planes and nodes so that the six floats of a bounding
 * box can just be stored out and get a proper clipping hull structure.
 */
static void CM_InitBoxHull (MapTile& tile)
{
	tile.box_headnode = tile.numnodes;
	tile.box_planes = &tile.planes[tile.numplanes];
	/* sanity check if you only use one maptile => no map assembly */
	if (tile.idx == 1 && (tile.numnodes + 6 > MAX_MAP_NODES
		|| tile.numbrushes + 1 > MAX_MAP_BRUSHES
		|| tile.numleafbrushes + 1 > MAX_MAP_LEAFBRUSHES
		|| tile.numbrushsides + 6 > MAX_MAP_BRUSHSIDES
		|| tile.numplanes + 12 > MAX_MAP_PLANES))
		Com_Error(ERR_DROP, "Not enough room for box tree");

	tile.box_brush = &tile.brushes[tile.numbrushes];
	tile.box_brush->numsides = 6;
	tile.box_brush->firstbrushside = tile.numbrushsides;
	tile.box_brush->brushContentFlags = CONTENTS_WEAPONCLIP;

	tile.box_leaf = &tile.leafs[tile.numleafs];
	tile.box_leaf->contentFlags = CONTENTS_WEAPONCLIP;
	tile.box_leaf->firstleafbrush = tile.numleafbrushes;
	tile.box_leaf->numleafbrushes = 1;

	tile.leafbrushes[tile.numleafbrushes] = tile.numbrushes;

	/* each side */
	for (int i = 0; i < 6; i++) {
		const int side = i & 1;

		/* brush sides */
		cBspBrushSide_t* s = &tile.brushsides[tile.numbrushsides + i];
		s->plane = tile.planes + (tile.numplanes + i * 2 + side);
		s->surface = &nullSurface;

		/* nodes */
		cBspNode_t* c = &tile.nodes[tile.box_headnode + i];
		c->plane = tile.planes + (tile.numplanes + i * 2);
		c->children[side] = -1 - tile.emptyleaf;
		if (i != 5)
			c->children[side ^ 1] = tile.box_headnode + i + 1;
		else
			c->children[side ^ 1] = LEAFNODE - tile.numleafs;

		/* planes */
		cBspPlane_t* p = &tile.box_planes[i * 2];
		p->type = i >> 1;
		VectorClear(p->normal);
		p->normal[i >> 1] = 1;

		p = &tile.box_planes[i * 2 + 1];
		p->type = PLANE_ANYX + (i >> 1);
		VectorClear(p->normal);
		p->normal[i >> 1] = -1;
	}
}

void CM_LoadBsp (MapTile& tile, const dBspHeader_t& header, const vec3_t shift, const byte* base)
{
	/* load into heap */
	CMod_LoadSurfaces(tile, base, &header.lumps[LUMP_TEXINFO]);
	CMod_LoadLeafs(tile, base, &header.lumps[LUMP_LEAFS]);
	CMod_LoadLeafBrushes(tile, base, &header.lumps[LUMP_LEAFBRUSHES]);
	CMod_LoadPlanes(tile, base, &header.lumps[LUMP_PLANES], shift);
	CMod_LoadBrushes(tile, base, &header.lumps[LUMP_BRUSHES]);
	CMod_LoadBrushSides(tile, base, &header.lumps[LUMP_BRUSHSIDES]);
	CMod_LoadSubmodels(tile, base, &header.lumps[LUMP_MODELS], shift);
	CMod_LoadNodes(tile, base, &header.lumps[LUMP_NODES], shift);
}

/**
 * @brief Adds in a single map tile
 * @param[in] name The (file-)name of the tile to add.
 * @param[in] entityString If not empty, this is added to mapEntityString by CMod_LoadEntityString
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
static void CM_AddMapTile (const char* name, const char* entityString, const bool day, const int sX, const int sY, const byte sZ, mapData_t* mapData, mapTiles_t* mapTiles)
{
	char filename[MAX_QPATH];
	unsigned checksum;
	byte* buf;
	int length;
	dBspHeader_t header;
	/* use for random map assembly for shifting origins and so on */
	vec3_t shift;
	const byte* base;

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

	header = *(dBspHeader_t*) buf;
	BSP_SwapHeader(&header, filename);

	if (header.version != BSPVERSION)
		Com_Error(ERR_DROP, "CM_AddMapTile: %s has wrong version number (%i should be %i)", name, header.version, BSPVERSION);

	base = (const byte*) buf;

	/* init */
	if (mapTiles->numTiles >= MAX_MAPTILES)
		Com_Error(ERR_FATAL, "CM_AddMapTile: too many tiles loaded %i", mapTiles->numTiles);

	MapTile& tile = mapTiles->mapTiles[mapTiles->numTiles];
	OBJZERO(tile);
	tile.idx = mapTiles->numTiles;
	Q_strncpyz(tile.name, name, sizeof(tile.name));

	/* pathfinding and the like must be shifted on the worldplane when we
	 * are assembling a map */
	VectorSet(shift, sX * UNIT_SIZE, sY * UNIT_SIZE, sZ * UNIT_HEIGHT);

	CM_LoadBsp(tile, header, shift, base);
	CMod_LoadEntityString(tile, entityString, mapData, base, &header.lumps[LUMP_ENTITIES], shift);
	if (day)
		CMod_LoadLighting(tile, base, &header.lumps[LUMP_LIGHTING_DAY]);
	else
		CMod_LoadLighting(tile, base, &header.lumps[LUMP_LIGHTING_NIGHT]);

	CM_InitBoxHull(tile);
	CM_MakeTracingNodes(tile);

	mapData->numInline += tile.nummodels - NUM_REGULAR_MODELS;

	CMod_LoadRouting(tile, mapData, base, name, &header.lumps[LUMP_ROUTING], sX, sY, sZ);

	/* now increase the amount of loaded tiles */
	mapTiles->numTiles++;

	/* Now find the map bounds with the updated numTiles. */
	/* calculate new border after merge */
	RT_GetMapSize(mapTiles, mapData->mapBox.mins, mapData->mapBox.maxs);

	FS_FreeFile(buf);

	mapData->mapChecksum += checksum;
}

/**
 * @brief Recalculate the seams of the tiles after an RMA
 */
static void CMod_RerouteMap (mapTiles_t* mapTiles, mapData_t* mapData)
{
	actorSizeEnum_t actorSize;
	int x, y, z, dir;
	int cols = 0;
	double start, end;	/* stopwatch */

	start = time(nullptr);

	GridBox rBox(mapData->mapBox);	/* the box we will actually reroute */
	rBox.clipToMaxBoundaries();

	/* First, close the borders of the map. This is needed once we produce tiles with open borders.
	 * It's done by setting the connection (height) to 0 */
	for (actorSize = 1; actorSize <= ACTOR_MAX_SIZE; actorSize++) {
		for (z = rBox.getMinZ(); z <= rBox.getMaxZ(); z++) {
			for (y = rBox.getMinY(); y <= rBox.getMaxY(); y++) {
				x = rBox.getMinX();
				mapData->routing.setConn(actorSize, x, y, z, 6, 0);	/* NX-PY */
				mapData->routing.setConn(actorSize, x, y, z, 1, 0);	/* NX */
				mapData->routing.setConn(actorSize, x, y, z, 5, 0);	/* NX-NY */
				x = rBox.getMaxX();
				mapData->routing.setConn(actorSize, x, y, z, 4, 0);	/* PX-PY */
				mapData->routing.setConn(actorSize, x, y, z, 0, 0);	/* PX */
				mapData->routing.setConn(actorSize, x, y, z, 7, 0);	/* PX-NY */
			}
			for (x = rBox.getMinX(); x <= rBox.getMaxX(); x++) {
				y = rBox.getMinY();
				mapData->routing.setConn(actorSize, x, y, z, 5, 0);	/* NY-NX */
				mapData->routing.setConn(actorSize, x, y, z, 3, 0);	/* NY */
				mapData->routing.setConn(actorSize, x, y, z, 7, 0);	/* NY-PX */
				y = rBox.getMaxY();
				mapData->routing.setConn(actorSize, x, y, z, 6, 0);	/* PY-NX */
				mapData->routing.setConn(actorSize, x, y, z, 2, 0);	/* PY */
				mapData->routing.setConn(actorSize, x, y, z, 4, 0);	/* PY-PX */
			}
		}
	}

	/* Floor pass */
	for (actorSize = ACTOR_SIZE_INVALID; actorSize < ACTOR_MAX_SIZE; actorSize++) {
		for (y = rBox.getMinY(); y <= rBox.getMaxY(); y++) {
			for (x = rBox.getMinX(); x <= rBox.getMaxX(); x++) {
				if (mapData->reroute[actorSize][y][x] == ROUTING_NOT_REACHABLE) {
					cols++;
					for (z = rBox.getMaxZ(); z >= rBox.getMinZ(); z--) {
						const int newZ = RT_CheckCell(mapTiles, mapData->routing, actorSize + 1, x, y, z, nullptr);
						assert(newZ <= z);
						z = newZ;
					}
				}
			}
		}
	}

	/* Wall pass */
	for (actorSize = ACTOR_SIZE_INVALID; actorSize < ACTOR_MAX_SIZE; actorSize++) {
		for (y = rBox.getMinY(); y <= rBox.getMaxY(); y++) {
			for (x = rBox.getMinX(); x <= rBox.getMaxX(); x++) {
				const byte tile = mapData->reroute[actorSize][y][x];
				if (tile) {
					byte fromTile1 = 0;
					byte fromTile2 = 0;
					byte fromTile3 = 0;
					mapTiles->getTilesAt(x ,y, fromTile1, fromTile2, fromTile3);
					for (dir = 0; dir < CORE_DIRECTIONS; dir++) {
						const int dx = x + dvecs[dir][0];
						const int dy = y + dvecs[dir][1];
						/* Skip if the destination is out of bounds. */
						if (dx < 0 || dx >= PATHFINDING_WIDTH || dy < 0 || dy >= PATHFINDING_WIDTH)
							continue;
#if 0	// this causes problems eg. at baseattack entrance (see bug 5292). Disabled until the underlying problem is found
						byte toTile1 = 0;
						byte toTile2 = 0;
						byte toTile3 = 0;
						mapTiles->getTilesAt(dx ,dy, toTile1, toTile2, toTile3);

						int minZ = 0;
						int maxZ = PATHFINDING_HEIGHT - 1;
						if (!toTile1)
							continue;	/* no tile at destination, so skip this dir */
						if (fromTile1 == toTile1) {		/* same tile */
							if (!fromTile2 && !toTile2)	/* and no stacked tiles */
								continue;				/* so nothing to do */
							else {
								if (fromTile2 == toTile2) {	/* the stacked tiles are also the same */
									mapTiles->getTileOverlap(toTile1, toTile2, minZ, maxZ);
								}
							}
						}
						RT_UpdateConnectionColumn(mapTiles, mapData->routing, actorSize + 1, x, y, dir, nullptr, minZ, maxZ);
#else
						const int tile2 = mapData->reroute[actorSize][dy][dx];
						/* Both cells are present and if either cell is ROUTING_NOT_REACHABLE or if the cells are different. */
						if (tile2 && (tile2 == ROUTING_NOT_REACHABLE || tile2 != tile)) {
							RT_UpdateConnectionColumn(mapTiles, mapData->routing, actorSize + 1, x, y, dir);
						}
#endif // 1
					}
				}
			}
		}
	}
	end = time(nullptr);
	Com_Printf("Rerouted %i cols for RMA in %5.1fs\n", cols, end - start);
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
 * @param[in] entityString An entity string that is used for all map tiles. Might be @c nullptr.
 * @sa CM_AddMapTile
 * @sa R_ModBeginLoading
 */
void CM_LoadMap (const char* tiles, bool day, const char* pos, const char* entityString, mapData_t* mapData, mapTiles_t* mapTiles)
{
	char name[MAX_VAR];
	char base[MAX_QPATH];

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
		const char* token = Com_Parse(&tiles);
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
			for (int i = 0; i < 3; i++) {
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
			CM_AddMapTile(name, entityString, day, sh[0], sh[1], sh[2], mapData, mapTiles);
		} else {
			/* load only a single tile, if no positions are specified */
			CM_AddMapTile(name, entityString, day, 0, 0, 0, mapData, mapTiles);
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
cBspModel_t* CM_InlineModel (const mapTiles_t* mapTiles, const char* name)
{
	/* we only want inline models here */
	if (!name || name[0] != '*')
		Com_Error(ERR_DROP, "CM_InlineModel: bad name: '%s'", name ? name : "");
	/* skip the '*' character and get the inline model number */
	int num = atoi(name + 1) - 1;
	if (num < 0 || num >= MAX_MODELS)
		Com_Error(ERR_DROP, "CM_InlineModel: bad number %i - max inline models are %i", num, MAX_MODELS);

	/* search all the loaded tiles for the given inline model */
	for (int i = 0; i < mapTiles->numTiles; i++) {
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
cBspModel_t* CM_SetInlineModelOrientation (mapTiles_t* mapTiles, const char* name, const vec3_t origin, const vec3_t angles)
{
	cBspModel_t* model = CM_InlineModel(mapTiles, name);
	assert(model);
	VectorCopy(origin, model->origin);
	VectorCopy(angles, model->angles);

	return model;
}

/**
 * @brief This function calculates a model's aabb in world coordinates
 * @param[in] mapTiles List of tiles the current (RMA-)map is composed of
 * @param[in] name The name of the model, must include the '*'
 * @param[out] aabb The aabb to be filled
 */
void CM_GetInlineModelAABB (mapTiles_t* mapTiles, const char* name, AABB& aabb)
{
	cBspModel_t* model = CM_InlineModel(mapTiles, name);
	assert(model);
	CalculateMinsMaxs(model->angles, model->cbmBox, model->origin, aabb);
}

/**
 * @brief Checks how well a position is visible
 * @return a visibility factor. @c 1.0 means fully visible, @c 0.0 means hardly visible because the
 * given position is in the darkness
 */
float CM_GetVisibility (const mapTiles_t* mapTiles, const pos3_t position)
{
	for (int i = 0; i < mapTiles->numTiles; i++) {
		const MapTile& tile = mapTiles->mapTiles[i];
		if (VectorInside(position, tile.wpMins, tile.wpMaxs)) {
			if (tile.lightdata == nullptr)
				return 1.0f;
			/** @todo implement me */
			return 1.0f;
		}
	}

	/* point is outside of any loaded tile */
	Com_Printf("given point %i:%i:%i is not inside of any loaded tile\n", position[0], position[1], position[2]);
	return 1.0f;
}
