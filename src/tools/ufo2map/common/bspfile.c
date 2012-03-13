/**
 * @file bspfile.c
 * @brief
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

#include "shared.h"
#include "bspfile.h"
#include "scriplib.h"
#include "../bsp.h"
#include <errno.h>

/**
 * @brief Compress the routing data of a map
 * @sa CMod_DeCompressRouting
 * @sa CMod_LoadRouting
 */
byte *CompressRouting (byte *dataStart, byte *destStart, int l)
{
	int c;
	byte val;
	byte *data, *dend, *dest_p, *count_p;

	dest_p = destStart;
	data = dataStart;
	dend = dataStart + l;

	while (data < dend) {
		if (data + 1 < dend && *data == *(data + 1)) {
			/* repetitions */
			val = *data++;
			data++; /* Advance data again.  The first two bytes are identical!!! */
			c = 0; /* This means 2 bytes are the same.  Total bytes the same is 2 + c */
			/* Loop while the piece of data being looked at equals val */
			while (data + 1 < dend && val == *(data)) {
				if (c >= SCHAR_MAX) /* must fit into one byte */
					break;
				data++;
				c++;
			}
			count_p = dest_p;
			*dest_p++ = (byte)(c) | 0x80;
			*dest_p++ = val;
		} else {
			/* identities */
			count_p = dest_p++;
			c = 0;
			while ((data + 1 < dend && *data != *(data + 1)) || data == dend - 1) {
				if (c >= SCHAR_MAX) /* must fit into one byte */
					break;
				*dest_p++ = *data++;
				c++;
			}
			*count_p = (byte)c;
		}
	}

	/* terminate compressed data */
	*dest_p++ = 0;

	return dest_p;
}

/**
 * @brief Byte swaps all data in a bsp file.
 */
static void SwapBSPFile (void)
{
	int i, j, k;

	/* models */
	for (i = 0; i < curTile->nummodels; i++) {
		dBspModel_t *d = &curTile->models[i];

		d->firstface = LittleLong(d->firstface);
		d->numfaces = LittleLong(d->numfaces);
		d->headnode = LittleLong(d->headnode);

		for (j = 0; j < 3; j++) {
			d->mins[j] = LittleFloat(d->mins[j]);
			d->maxs[j] = LittleFloat(d->maxs[j]);
			d->origin[j] = LittleFloat(d->origin[j]);
		}
	}

	/* vertexes */
	for (i = 0; i < curTile->numvertexes; i++) {
		dBspVertex_t *vertexes = &curTile->vertexes[i];
		for (j = 0; j < 3; j++)
			vertexes->point[j] = LittleFloat(vertexes->point[j]);
	}

	/* planes */
	for (i = 0; i < curTile->numplanes; i++) {
		dBspPlane_t *plane = &curTile->planes[i];
		for (j = 0; j < 3; j++)
			plane->normal[j] = LittleFloat(plane->normal[j]);
		plane->dist = LittleFloat(plane->dist);
		plane->type = LittleLong(plane->type);
	}

	/* texinfos */
	for (i = 0; i < curTile->numtexinfo; i++) {
		dBspTexinfo_t *texinfo = &curTile->texinfo[i];
		for (j = 0; j < 2; j++)
			for (k = 0; k < 4; k++)
				texinfo->vecs[j][k] = LittleFloat(texinfo->vecs[j][k]);
		texinfo->surfaceFlags = LittleLong(texinfo->surfaceFlags);
		texinfo->value = LittleLong(texinfo->value);
	}

	/* faces */
	for (i = 0; i < curTile->numfaces; i++) {
		dBspSurface_t *face = &curTile->faces[i];
		face->texinfo = LittleShort(face->texinfo);
		face->planenum = LittleShort(face->planenum);
		face->side = LittleShort(face->side);
		for (j = 0; j < LIGHTMAP_MAX; j++)
			face->lightofs[j] = LittleLong(face->lightofs[j]);
		face->firstedge = LittleLong(face->firstedge);
		face->numedges = LittleShort(face->numedges);
	}

	/* nodes */
	for (i = 0; i < curTile->numnodes; i++) {
		dBspNode_t *node = &curTile->nodes[i];
		/* planenum might be -1 here - special case for pathfinding nodes */
		node->planenum = LittleLong(node->planenum);
		for (j = 0; j < 3; j++) {
			node->mins[j] = LittleShort(node->mins[j]);
			node->maxs[j] = LittleShort(node->maxs[j]);
		}
		node->children[0] = LittleLong(node->children[0]);
		node->children[1] = LittleLong(node->children[1]);
		node->firstface = LittleShort(node->firstface);
		node->numfaces = LittleShort(node->numfaces);
	}

	/* leafs */
	for (i = 0; i < curTile->numleafs; i++) {
		dBspLeaf_t *leaf = &curTile->leafs[i];
		leaf->contentFlags = LittleLong(leaf->contentFlags);
		leaf->area = LittleShort(leaf->area);
		for (j = 0; j < 3; j++) {
			leaf->mins[j] = LittleShort(leaf->mins[j]);
			leaf->maxs[j] = LittleShort(leaf->maxs[j]);
		}

		leaf->firstleafbrush = LittleShort(leaf->firstleafbrush);
		leaf->numleafbrushes = LittleShort(leaf->numleafbrushes);
	}

	/* leafbrushes */
	for (i = 0; i < curTile->numleafbrushes; i++)
		curTile->leafbrushes[i] = LittleShort(curTile->leafbrushes[i]);

	/* surfedges */
	for (i = 0; i < curTile->numsurfedges; i++)
		curTile->surfedges[i] = LittleLong(curTile->surfedges[i]);

	/* edges */
	for (i = 0; i < curTile->numedges; i++) {
		dBspEdge_t *edge = &curTile->edges[i];
		edge->v[0] = LittleShort(edge->v[0]);
		edge->v[1] = LittleShort(edge->v[1]);
	}

	/* dbrushes */
	for (i = 0; i < curTile->numbrushes; i++) {
		dBspBrush_t *dbrush = &curTile->dbrushes[i];
		dbrush->firstbrushside = LittleLong(dbrush->firstbrushside);
		dbrush->numsides = LittleLong(dbrush->numsides);
		dbrush->contentFlags = LittleLong(dbrush->contentFlags);
	}

	/* brushes */
	for (i = 0; i < curTile->numbrushes; i++) {
		cBspBrush_t *cbrush = &curTile->brushes[i];
		cbrush->firstbrushside = LittleLong(cbrush->firstbrushside);
		cbrush->numsides = LittleLong(cbrush->numsides);
		cbrush->contentFlags = LittleLong(cbrush->contentFlags);
	}

	/* brushsides */
	for (i = 0; i < curTile->numbrushsides; i++) {
		dBspBrushSide_t *brushSide = &curTile->brushsides[i];
		brushSide->planenum = LittleShort(brushSide->planenum);
		brushSide->texinfo = LittleShort(brushSide->texinfo);
	}
}

static uint32_t CopyLump (const dBspHeader_t *header, int lumpIdx, void *dest, size_t size)
{
	const lump_t *lump = &header->lumps[lumpIdx];
	const uint32_t length = lump->filelen;
	const uint32_t ofs = lump->fileofs;

	if (length == 0)
		return 0;
	if (length % size)
		Sys_Error("LoadBSPFile: odd lump size");

	memcpy(dest, (const byte *)header + ofs, length);

	return length / size;
}

/**
 * @sa WriteBSPFile
 */
dMapTile_t *LoadBSPFile (const char *filename)
{
	int size;
	unsigned int i;
	dBspHeader_t *header;

	/* Create this shortcut to mapTiles[0] */
	curTile = &mapTiles.mapTiles[0];
	/* Set the number of tiles to 1. */
	mapTiles.numTiles = 1;

	/* load the file header */
	size = FS_LoadFile(filename, (byte **)&header);
	if (size == -1)
		Sys_Error("'%s' doesn't exist", filename);

	/* swap the header */
	BSP_SwapHeader(header, filename);

	if (header->ident != IDBSPHEADER)
		Sys_Error("%s is not a IBSP file", filename);
	if (header->version != BSPVERSION)
		Sys_Error("%s is version %i, not %i", filename, header->version, BSPVERSION);

	curTile->nummodels = CopyLump(header, LUMP_MODELS, curTile->models, sizeof(dBspModel_t));
	curTile->numvertexes = CopyLump(header, LUMP_VERTEXES, curTile->vertexes, sizeof(dBspVertex_t));
	curTile->numplanes = CopyLump(header, LUMP_PLANES, curTile->planes, sizeof(dBspPlane_t));
	curTile->numleafs = CopyLump(header, LUMP_LEAFS, curTile->leafs, sizeof(dBspLeaf_t));
	curTile->numnormals = CopyLump(header, LUMP_NORMALS, curTile->normals, sizeof(dBspNormal_t));
	curTile->numnodes = CopyLump(header, LUMP_NODES, curTile->nodes, sizeof(dBspNode_t));
	curTile->numtexinfo = CopyLump(header, LUMP_TEXINFO, curTile->texinfo, sizeof(dBspTexinfo_t));
	curTile->numfaces = CopyLump(header, LUMP_FACES, curTile->faces, sizeof(dBspSurface_t));
	curTile->numleafbrushes = CopyLump(header, LUMP_LEAFBRUSHES, curTile->leafbrushes, sizeof(curTile->leafbrushes[0]));
	curTile->numsurfedges = CopyLump(header, LUMP_SURFEDGES, curTile->surfedges, sizeof(curTile->surfedges[0]));
	curTile->numedges = CopyLump(header, LUMP_EDGES, curTile->edges, sizeof(dBspEdge_t));
	curTile->numbrushes = CopyLump(header, LUMP_BRUSHES, curTile->dbrushes, sizeof(dBspBrush_t));
	curTile->numbrushsides = CopyLump(header, LUMP_BRUSHSIDES, curTile->brushsides, sizeof(dBspBrushSide_t));
	curTile->routedatasize = CopyLump(header, LUMP_ROUTING, curTile->routedata, 1);
	curTile->lightdatasize[LIGHTMAP_NIGHT] = CopyLump(header, LUMP_LIGHTING_NIGHT, curTile->lightdata[LIGHTMAP_NIGHT], 1);
	curTile->lightdatasize[LIGHTMAP_DAY] = CopyLump(header, LUMP_LIGHTING_DAY, curTile->lightdata[LIGHTMAP_DAY], 1);
	curTile->entdatasize = CopyLump(header, LUMP_ENTITIES, curTile->entdata, 1);

	/* Because the tracing functions use cBspBrush_t and not dBspBrush_t,
	 * copy data from curTile->dbrushes into curTile->cbrushes */
	OBJZERO(curTile->brushes);
	for (i = 0; i < curTile->numbrushes; i++) {
		dBspBrush_t *dbrush = &curTile->dbrushes[i];
		cBspBrush_t *brush = &curTile->brushes[i];
		brush->firstbrushside = dbrush->firstbrushside;
		brush->numsides = dbrush->numsides;
		brush->contentFlags = dbrush->contentFlags;
	}

	/* everything has been copied out */
	FS_FreeFile(header);

	/* swap everything */
	SwapBSPFile();

	return curTile;
}

/**
 * @sa WriteBSPFile
 * @todo Implement this without the ftell stuff - don't write the bsp file twice
 */
static void AddLump (qFILE *bspfile, dBspHeader_t *header, int lumpnum, void *data, int len)
{
	lump_t *lump;
	long offset;

	lump = &header->lumps[lumpnum];

	offset = ftell(bspfile->f);
	if (offset == -1) {
		Sys_Error("Overflow in AddLump for lump %i (%s) %s", lumpnum, bspfile->name, strerror(errno));
	}
	lump->fileofs = LittleLong(offset);
	lump->filelen = LittleLong(len);
	/* 4 byte align */
	FS_Write(data, (len + 3) &~ 3, bspfile);
}

/**
 * @brief Swaps the bsp file in place, so it should not be referenced again
 * @sa LoadBSPFile
 */
long WriteBSPFile (const char *filename)
{
	qFILE bspfile;
	dBspHeader_t outheader;
	long size;

	OBJZERO(outheader);
	OBJZERO(bspfile);

	SwapBSPFile();

	outheader.ident = LittleLong(IDBSPHEADER);
	outheader.version = LittleLong(BSPVERSION);

	FS_OpenFile(filename, &bspfile, FILE_WRITE);
	if (!bspfile.f)
		Sys_Error("Could not write bsp file");
	FS_Write(&outheader, sizeof(outheader), &bspfile);	/* overwritten later */

	AddLump(&bspfile, &outheader, LUMP_PLANES, curTile->planes, curTile->numplanes * sizeof(dBspPlane_t));
	AddLump(&bspfile, &outheader, LUMP_LEAFS, curTile->leafs, curTile->numleafs * sizeof(dBspLeaf_t));
	AddLump(&bspfile, &outheader, LUMP_VERTEXES, curTile->vertexes, curTile->numvertexes * sizeof(dBspVertex_t));
	AddLump(&bspfile, &outheader, LUMP_NORMALS, curTile->normals, curTile->numnormals * sizeof(dBspNormal_t));
	AddLump(&bspfile, &outheader, LUMP_NODES, curTile->nodes, curTile->numnodes * sizeof(dBspNode_t));
	AddLump(&bspfile, &outheader, LUMP_TEXINFO, curTile->texinfo, curTile->numtexinfo * sizeof(dBspTexinfo_t));
	AddLump(&bspfile, &outheader, LUMP_FACES, curTile->faces, curTile->numfaces * sizeof(dBspSurface_t));
	AddLump(&bspfile, &outheader, LUMP_BRUSHES, curTile->dbrushes, curTile->numbrushes * sizeof(dBspBrush_t));
	AddLump(&bspfile, &outheader, LUMP_BRUSHSIDES, curTile->brushsides, curTile->numbrushsides * sizeof(dBspBrushSide_t));
	AddLump(&bspfile, &outheader, LUMP_LEAFBRUSHES, curTile->leafbrushes, curTile->numleafbrushes * sizeof(curTile->leafbrushes[0]));
	AddLump(&bspfile, &outheader, LUMP_SURFEDGES, curTile->surfedges, curTile->numsurfedges * sizeof(curTile->surfedges[0]));
	AddLump(&bspfile, &outheader, LUMP_EDGES, curTile->edges, curTile->numedges * sizeof(dBspEdge_t));
	AddLump(&bspfile, &outheader, LUMP_MODELS, curTile->models, curTile->nummodels * sizeof(dBspModel_t));
	AddLump(&bspfile, &outheader, LUMP_LIGHTING_NIGHT, curTile->lightdata[0], curTile->lightdatasize[0]);
	AddLump(&bspfile, &outheader, LUMP_LIGHTING_DAY, curTile->lightdata[1], curTile->lightdatasize[1]);
	AddLump(&bspfile, &outheader, LUMP_ROUTING, curTile->routedata, curTile->routedatasize);
	AddLump(&bspfile, &outheader, LUMP_ENTITIES, curTile->entdata, curTile->entdatasize);
	size = ftell(bspfile.f);

	fseek(bspfile.f, 0L, SEEK_SET);
	FS_Write(&outheader, sizeof(outheader), &bspfile);
	FS_CloseFile(&bspfile);

	SwapBSPFile();

	return size;
}

/**
 * @brief Dumps info about current file
 */
void PrintBSPFileSizes (void)
{
	if (!num_entities)
		ParseEntities();

	Com_Printf("amount type        size in bytes\n");
	Com_Printf("================================\n");
	Com_Printf("%5i models            %7i\n", curTile->nummodels, (int)(curTile->nummodels * sizeof(cBspModel_t)));
	Com_Printf("%5i brushes           %7i\n", curTile->numbrushes, (int)(curTile->numbrushes * sizeof(dBspBrush_t)));
	Com_Printf("%5i brushsides        %7i\n", curTile->numbrushsides, (int)(curTile->numbrushsides * sizeof(dBspBrushSide_t)));
	Com_Printf("%5i planes            %7i\n", curTile->numplanes, (int)(curTile->numplanes * sizeof(dBspPlane_t)));
	Com_Printf("%5i texinfo           %7i\n", curTile->numtexinfo, (int)(curTile->numtexinfo * sizeof(dBspTexinfo_t)));
	Com_Printf("%5i entdata           %7i\n", num_entities, curTile->entdatasize);

	Com_Printf("\n");

	Com_Printf("%5i normales          %7i\n", curTile->numnormals, (int)(curTile->numnormals * sizeof(dBspNormal_t)));
	Com_Printf("%5i vertexes          %7i\n", curTile->numvertexes, (int)(curTile->numvertexes * sizeof(dBspVertex_t)));
	Com_Printf("%5i nodes             %7i\n", curTile->numnodes, (int)(curTile->numnodes * sizeof(dBspNode_t)));
	Com_Printf("%5i faces             %7i\n", curTile->numfaces, (int)(curTile->numfaces * sizeof(dBspSurface_t)));
	Com_Printf("%5i leafs             %7i\n", curTile->numleafs, (int)(curTile->numleafs * sizeof(dBspLeaf_t)));
	Com_Printf("%5i leafbrushes       %7i\n", curTile->numleafbrushes, (int)(curTile->numleafbrushes * sizeof(curTile->leafbrushes[0])));
	Com_Printf("%5i surfedges         %7i\n", curTile->numsurfedges, (int)(curTile->numsurfedges * sizeof(curTile->surfedges[0])));
	Com_Printf("%5i edges             %7i\n", curTile->numedges, (int)(curTile->numedges * sizeof(dBspEdge_t)));
	Com_Printf("night lightdata         %7i\n", curTile->lightdatasize[0]);
	Com_Printf("  day lightdata         %7i\n", curTile->lightdatasize[1]);
	Com_Printf("      routedata         %7i\n", curTile->routedatasize);
}


int num_entities;
entity_t entities[MAX_MAP_ENTITIES];

/**
 * @brief Removes trailing whitespaces from the given string
 * @param[in,out] str The string to clean up
 * @note Whitespaces are converted to \\0
 */
static void StripTrailingWhitespaces (char *str)
{
	char *s;

	s = str + strlen(str) - 1;
	while (s >= str && *s <= ' ') {
		*s = '\0';
		s--;
	}
}

/**
 * @brief Parses one key and value for an entity from the current tokens
 * @sa parsedToken
 * @sa GetToken
 * @sa ParseEntity
 * @sa ParseMapEntity
 */
epair_t *ParseEpair (void)
{
	epair_t	*e;

	e = Mem_AllocType(epair_t);

	if (strlen(parsedToken) >= MAX_KEY - 1)
		Sys_Error("ParseEpar: token too long");
	e->key = Mem_StrDup(parsedToken);
	GetToken(qfalse);
	if (strlen(parsedToken) >= MAX_VALUE - 1)
		Sys_Error("ParseEpar: token too long");
	e->value = Mem_StrDup(parsedToken);

	/* strip trailing spaces */
	StripTrailingWhitespaces(e->key);
	StripTrailingWhitespaces(e->value);

	return e;
}


/**
 * @sa ParseEntities
 */
static entity_t* ParseEntity (void)
{
	entity_t *mapent;

	if (!GetToken(qtrue))
		return NULL;

	if (parsedToken[0] != '{')
		Sys_Error("ParseEntity: { not found");

	if (num_entities >= MAX_MAP_ENTITIES)
		Sys_Error("num_entities >= MAX_MAP_ENTITIES (%i)", num_entities);

	mapent = &entities[num_entities];
	num_entities++;

	do {
		if (!GetToken(qtrue))
			Sys_Error("ParseEntity: EOF without closing brace");
		if (*parsedToken == '}') {
			break;
		} else {
			epair_t *e = ParseEpair();
			e->next = mapent->epairs;
			mapent->epairs = e;
		}
	} while (1);

	return mapent;
}

/**
 * @brief Parses the curTile->entdata string into entities
 * @sa UnparseEntities
 * @sa ParseEntity
 */
void ParseEntities (void)
{
	num_entities = 0;
	ParseFromMemory(curTile->entdata, curTile->entdatasize);

	while (ParseEntity() != NULL) {
	}
}


/**
 * @brief Generates the curTile->entdata string from all the entities
 * @sa ParseEntities
 */
const char *UnparseEntities (void)
{
	char line[2048], key[1024], value[1024];
	int i;

	curTile->entdata[0] = '\0';

	for (i = 0; i < num_entities; i++) {
		const epair_t *ep = entities[i].epairs;
		if (!ep)
			continue;	/* ent got removed */

		Q_strcat(curTile->entdata, "{\n", sizeof(curTile->entdata));

		for (ep = entities[i].epairs; ep; ep = ep->next) {
			Q_strncpyz(key, ep->key, sizeof(key));
			StripTrailingWhitespaces(key);
			Q_strncpyz(value, ep->value, sizeof(value));
			StripTrailingWhitespaces(value);

			Com_sprintf(line, sizeof(line), "\"%s\" \"%s\"\n", key, value);
			Q_strcat(curTile->entdata, line, sizeof(curTile->entdata));
		}
		Q_strcat(curTile->entdata, "}\n", sizeof(curTile->entdata));
	}
	curTile->entdatasize = strlen(curTile->entdata);

	return curTile->entdata;
}

void SetKeyValue (entity_t *ent, const char *key, const char *value)
{
	epair_t *ep;

	for (ep = ent->epairs; ep; ep = ep->next)
		if (Q_streq(ep->key, key)) {
			Mem_Free(ep->value);
			ep->value = Mem_StrDup(value);
			return;
		}
	ep = (epair_t *)Mem_Alloc(sizeof(*ep));
	ep->next = ent->epairs;
	ent->epairs = ep;
	ep->key = Mem_StrDup(key);
	ep->value = Mem_StrDup(value);
}

const char *ValueForKey (const entity_t *ent, const char *key)
{
	const epair_t *ep;

	for (ep = ent->epairs; ep; ep = ep->next)
		if (Q_streq(ep->key, key))
			return ep->value;
	return "";
}

vec_t FloatForKey (const entity_t *ent, const char *key)
{
	const char *k;

	k = ValueForKey(ent, key);
	return atof(k);
}

/**
 * @brief Converts a string into a @c vec3_t
 */
void GetVectorFromString (const char *value, vec3_t vec)
{
	if (value[0] != '\0') {
		double v1, v2, v3;

		/* scanf into doubles, then assign, so it is vec_t size independent */
		v1 = v2 = v3 = 0;
		if (sscanf(value, "%lf %lf %lf", &v1, &v2, &v3) != 3)
			Sys_Error("invalid vector statement given: '%s'", value);
		VectorSet(vec, v1, v2, v3);
	} else
		VectorClear(vec);
}

/**
 * @brief Converts the value of a entity parameter into a @c vec3_t
 */
void GetVectorForKey (const entity_t *ent, const char *key, vec3_t vec)
{
	const char *k = ValueForKey(ent, key);
	GetVectorFromString(k, vec);
}
