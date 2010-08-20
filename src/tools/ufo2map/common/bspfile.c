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

	/* models	 */
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
		for (j = 0; j < 3; j++)
			curTile->vertexes[i].point[j] = LittleFloat(curTile->vertexes[i].point[j]);
	}

	/* planes */
	for (i = 0; i < curTile->numplanes; i++) {
		for (j = 0; j < 3; j++)
			curTile->planes[i].normal[j] = LittleFloat(curTile->planes[i].normal[j]);
		curTile->planes[i].dist = LittleFloat(curTile->planes[i].dist);
		curTile->planes[i].type = LittleLong(curTile->planes[i].type);
	}

	/* texinfos */
	for (i = 0; i < curTile->numtexinfo; i++) {
		for (j = 0; j < 2; j++)
			for (k = 0; k < 4; k++)
				curTile->texinfo[i].vecs[j][k] = LittleFloat(curTile->texinfo[i].vecs[j][k]);
		curTile->texinfo[i].surfaceFlags = LittleLong(curTile->texinfo[i].surfaceFlags);
		curTile->texinfo[i].value = LittleLong(curTile->texinfo[i].value);
	}

	/* faces */
	for (i = 0; i < curTile->numfaces; i++) {
		curTile->faces[i].texinfo = LittleShort(curTile->faces[i].texinfo);
		curTile->faces[i].planenum = LittleShort(curTile->faces[i].planenum);
		curTile->faces[i].side = LittleShort(curTile->faces[i].side);
		for (j = 0; j < LIGHTMAP_MAX; j++)
			curTile->faces[i].lightofs[j] = LittleLong(curTile->faces[i].lightofs[j]);
		curTile->faces[i].firstedge = LittleLong(curTile->faces[i].firstedge);
		curTile->faces[i].numedges = LittleShort(curTile->faces[i].numedges);
	}

	/* nodes */
	for (i = 0; i < curTile->numnodes; i++) {
		/* planenum might be -1 here - special case for pathfinding nodes */
		curTile->nodes[i].planenum = LittleLong(curTile->nodes[i].planenum);
		for (j = 0; j < 3; j++) {
			curTile->nodes[i].mins[j] = LittleShort(curTile->nodes[i].mins[j]);
			curTile->nodes[i].maxs[j] = LittleShort(curTile->nodes[i].maxs[j]);
		}
		curTile->nodes[i].children[0] = LittleLong(curTile->nodes[i].children[0]);
		curTile->nodes[i].children[1] = LittleLong(curTile->nodes[i].children[1]);
		curTile->nodes[i].firstface = LittleShort(curTile->nodes[i].firstface);
		curTile->nodes[i].numfaces = LittleShort(curTile->nodes[i].numfaces);
	}

	/* leafs */
	for (i = 0; i < curTile->numleafs; i++) {
		curTile->leafs[i].contentFlags = LittleLong(curTile->leafs[i].contentFlags);
		curTile->leafs[i].area = LittleShort(curTile->leafs[i].area);
		for (j = 0; j < 3; j++) {
			curTile->leafs[i].mins[j] = LittleShort(curTile->leafs[i].mins[j]);
			curTile->leafs[i].maxs[j] = LittleShort(curTile->leafs[i].maxs[j]);
		}

		curTile->leafs[i].firstleafbrush = LittleShort(curTile->leafs[i].firstleafbrush);
		curTile->leafs[i].numleafbrushes = LittleShort(curTile->leafs[i].numleafbrushes);
	}

	/* leafbrushes */
	for (i = 0; i < curTile->numleafbrushes; i++)
		curTile->leafbrushes[i] = LittleShort(curTile->leafbrushes[i]);

	/* surfedges */
	for (i = 0; i < curTile->numsurfedges; i++)
		curTile->surfedges[i] = LittleLong(curTile->surfedges[i]);

	/* edges */
	for (i = 0; i < curTile->numedges; i++) {
		curTile->edges[i].v[0] = LittleShort(curTile->edges[i].v[0]);
		curTile->edges[i].v[1] = LittleShort(curTile->edges[i].v[1]);
	}

	/* dbrushes */
	for (i = 0; i < curTile->numbrushes; i++) {
		curTile->dbrushes[i].firstbrushside = LittleLong(curTile->dbrushes[i].firstbrushside);
		curTile->dbrushes[i].numsides = LittleLong(curTile->dbrushes[i].numsides);
		curTile->dbrushes[i].contentFlags = LittleLong(curTile->dbrushes[i].contentFlags);
	}

	/* brushes */
	for (i = 0; i < curTile->numbrushes; i++) {
		curTile->brushes[i].firstbrushside = LittleLong(curTile->brushes[i].firstbrushside);
		curTile->brushes[i].numsides = LittleLong(curTile->brushes[i].numsides);
		curTile->brushes[i].contentFlags = LittleLong(curTile->brushes[i].contentFlags);
	}

	/* brushsides */
	for (i = 0; i < curTile->numbrushsides; i++) {
		curTile->brushsides[i].planenum = LittleShort(curTile->brushsides[i].planenum);
		curTile->brushsides[i].texinfo = LittleShort(curTile->brushsides[i].texinfo);
	}
}


static dBspHeader_t *header;

static int CopyLump (int lump, void *dest, int size)
{
	const int length = header->lumps[lump].filelen;
	const int ofs = header->lumps[lump].fileofs;

	if (length == 0)
		return 0;
	if (length % size)
		Sys_Error("LoadBSPFile: odd lump size");

	memcpy(dest, (byte *)header + ofs, length);

	return length / size;
}

/**
 * @sa WriteBSPFile
 */
void LoadBSPFile (const char *filename)
{
	int i, size;

	/* Create this shortcut to mapTiles[0] */
	curTile = &mapTiles.mapTiles[0];
	/* Set the number of tiles to 1. */
	mapTiles.numTiles = 1;

	/* load the file header */
	size = FS_LoadFile(filename, (byte **)&header);
	if (size == -1)
		Sys_Error("'%s' doesn't exist", filename);

	/* swap the header */
	for (i = 0; i < sizeof(dBspHeader_t) / 4; i++)
		((int *)header)[i] = LittleLong(((int *)header)[i]);

	if (header->ident != IDBSPHEADER)
		Sys_Error("%s is not a IBSP file", filename);
	if (header->version != BSPVERSION)
		Sys_Error("%s is version %i, not %i", filename, header->version, BSPVERSION);

	curTile->nummodels = CopyLump(LUMP_MODELS, curTile->models, sizeof(dBspModel_t));
	curTile->numvertexes = CopyLump(LUMP_VERTEXES, curTile->vertexes, sizeof(dBspVertex_t));
	curTile->numplanes = CopyLump(LUMP_PLANES, curTile->planes, sizeof(dBspPlane_t));
	curTile->numleafs = CopyLump(LUMP_LEAFS, curTile->leafs, sizeof(dBspLeaf_t));
	curTile->numnormals = CopyLump(LUMP_NORMALS, curTile->normals, sizeof(dBspNormal_t));
	curTile->numnodes = CopyLump(LUMP_NODES, curTile->nodes, sizeof(dBspNode_t));
	curTile->numtexinfo = CopyLump(LUMP_TEXINFO, curTile->texinfo, sizeof(dBspTexinfo_t));
	curTile->numfaces = CopyLump(LUMP_FACES, curTile->faces, sizeof(dBspSurface_t));
	curTile->numleafbrushes = CopyLump(LUMP_LEAFBRUSHES, curTile->leafbrushes, sizeof(curTile->leafbrushes[0]));
	curTile->numsurfedges = CopyLump(LUMP_SURFEDGES, curTile->surfedges, sizeof(curTile->surfedges[0]));
	curTile->numedges = CopyLump(LUMP_EDGES, curTile->edges, sizeof(dBspEdge_t));
	curTile->numbrushes = CopyLump(LUMP_BRUSHES, curTile->dbrushes, sizeof(dBspBrush_t));
	curTile->numbrushsides = CopyLump(LUMP_BRUSHSIDES, curTile->brushsides, sizeof(dBspBrushSide_t));
	curTile->routedatasize = CopyLump(LUMP_ROUTING, curTile->routedata, 1);
	curTile->lightdatasize[LIGHTMAP_NIGHT] = CopyLump(LUMP_LIGHTING_NIGHT, curTile->lightdata[LIGHTMAP_NIGHT], 1);
	curTile->lightdatasize[LIGHTMAP_DAY] = CopyLump(LUMP_LIGHTING_DAY, curTile->lightdata[LIGHTMAP_DAY], 1);
	curTile->entdatasize = CopyLump(LUMP_ENTITIES, curTile->entdata, 1);

	/* Because the tracing functions use cBspBrush_t and not dBspBrush_t,
	 * copy data from curTile->dbrushes into curTile->cbrushes */
	memset(curTile->brushes, 0, MAX_MAP_BRUSHES * sizeof(cBspBrush_t));
	for (i = 0; i < curTile->numbrushes; i++) {
		curTile->brushes[i].firstbrushside = curTile->dbrushes[i].firstbrushside;
		curTile->brushes[i].numsides = curTile->dbrushes[i].numsides;
		curTile->brushes[i].contentFlags = curTile->dbrushes[i].contentFlags;
	}

	/* everything has been copied out */
	FS_FreeFile(header);

	/* swap everything */
	SwapBSPFile();
}

/**
 * @sa WriteBSPFile
 * @todo Implement this without the ftell stuff - don't write the bsp file twice
 */
static inline void AddLump (qFILE *bspfile, dBspHeader_t *header, int lumpnum, void *data, int len)
{
	lump_t *lump;

	lump = &header->lumps[lumpnum];

	lump->fileofs = LittleLong(ftell(bspfile->f));
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

	memset(&outheader, 0, sizeof(outheader));
	memset(&bspfile, 0, sizeof(bspfile));

	SwapBSPFile();

	outheader.ident = LittleLong(IDBSPHEADER);
	outheader.version = LittleLong(BSPVERSION);

	FS_OpenFile(filename, &bspfile, FILE_WRITE);
	if (!bspfile.f)
		Sys_Error("Could not write bsp file");
	FS_Write(&outheader, sizeof(dBspHeader_t), &bspfile);	/* overwritten later */

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

	Com_Printf("amout type         size in bytes\n");
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

	e = Mem_Alloc(sizeof(*e));

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
	entity_t *ent;
	int subdivide;

	num_entities = 0;
	ParseFromMemory(curTile->entdata, curTile->entdatasize);

	while ((ent = ParseEntity()) != NULL) {
	}

	subdivide = atoi(ValueForKey(&entities[0], "subdivide"));
	if (subdivide >= 256 && subdivide <= 2048) {
		Verb_Printf(VERB_EXTRA, "Using subdivide %d from worldspawn.\n", subdivide);
		config.subdivideSize = subdivide;
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
		if (!strcmp(ep->key, key)) {
			Mem_Free(ep->value);
			ep->value = Mem_StrDup(value);
			return;
		}
	ep = Mem_Alloc(sizeof(*ep));
	ep->next = ent->epairs;
	ent->epairs = ep;
	ep->key = Mem_StrDup(key);
	ep->value = Mem_StrDup(value);
}

const char *ValueForKey (const entity_t *ent, const char *key)
{
	const epair_t *ep;

	for (ep = ent->epairs; ep; ep = ep->next)
		if (!strcmp(ep->key, key))
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
		sscanf(value, "%lf %lf %lf", &v1, &v2, &v3);
		vec[0] = v1;
		vec[1] = v2;
		vec[2] = v3;
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
