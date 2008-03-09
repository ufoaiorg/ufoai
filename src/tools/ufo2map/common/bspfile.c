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
#include "cmdlib.h"
#include "bspfile.h"
#include "scriplib.h"

int nummodels;
dBspModel_t dmodels[MAX_MAP_MODELS];

int routedatasize;
byte droutedata[MAX_MAP_ROUTING];

int lightdatasize[2];
byte dlightdata[2][MAX_MAP_LIGHTING];

static int entdatasize;
static char dentdata[MAX_MAP_ENTSTRING];

int numleafs;
dBspLeaf_t dleafs[MAX_MAP_LEAFS];

int numplanes;
dBspPlane_t dplanes[MAX_MAP_PLANES];

int numvertexes;
dBspVertex_t dvertexes[MAX_MAP_VERTS];

int numnodes;
dBspNode_t dnodes[MAX_MAP_NODES];

int numtexinfo;
dBspTexinfo_t texinfo[MAX_MAP_TEXINFO];

int numfaces;
dBspFace_t dfaces[MAX_MAP_FACES];

int numedges;
dBspEdge_t dedges[MAX_MAP_EDGES];

int numleafbrushes;
unsigned short dleafbrushes[MAX_MAP_LEAFBRUSHES];

int numsurfedges;
int dsurfedges[MAX_MAP_SURFEDGES];

int numbrushes;
dBspBrush_t dbrushes[MAX_MAP_BRUSHES];

int numbrushsides;
dBspBrushSide_t dbrushsides[MAX_MAP_BRUSHSIDES];

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
			c = 0;
			while (data + 1 < dend && *data == *(data + 1)) {
				if (c >= SCHAR_MAX) /* must fit into one byte */
					break;
				data++;
				c++;
			}
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
	int i, j;
	dBspModel_t *d;

	/* models	 */
	for (i = 0; i < nummodels; i++) {
		d = &dmodels[i];

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
	for (i = 0; i < numvertexes; i++) {
		for (j = 0; j < 3; j++)
			dvertexes[i].point[j] = LittleFloat(dvertexes[i].point[j]);
	}

	/* planes */
	for (i = 0; i < numplanes; i++) {
		for (j = 0; j < 3; j++)
			dplanes[i].normal[j] = LittleFloat(dplanes[i].normal[j]);
		dplanes[i].dist = LittleFloat(dplanes[i].dist);
		dplanes[i].type = LittleLong(dplanes[i].type);
	}

	/* texinfos */
	for (i = 0; i < numtexinfo; i++) {
		for (j = 0; j < 8; j++)
			texinfo[i].vecs[0][j] = LittleFloat(texinfo[i].vecs[0][j]);
		texinfo[i].surfaceFlags = LittleLong(texinfo[i].surfaceFlags);
		texinfo[i].value = LittleLong(texinfo[i].value);
	}

	/* faces */
	for (i = 0; i < numfaces; i++) {
		dfaces[i].texinfo = LittleShort(dfaces[i].texinfo);
		dfaces[i].planenum = LittleShort(dfaces[i].planenum);
		dfaces[i].side = LittleShort(dfaces[i].side);
		for (j = 0; j < LIGHTMAP_MAX; j++)
			dfaces[i].lightofs[j] = LittleLong(dfaces[i].lightofs[j]);
		dfaces[i].firstedge = LittleLong(dfaces[i].firstedge);
		dfaces[i].numedges = LittleShort(dfaces[i].numedges);
	}

	/* nodes */
	for (i = 0; i < numnodes; i++) {
		dnodes[i].planenum = LittleLong(dnodes[i].planenum);
		for (j = 0; j < 3; j++) {
			dnodes[i].mins[j] = LittleShort(dnodes[i].mins[j]);
			dnodes[i].maxs[j] = LittleShort(dnodes[i].maxs[j]);
		}
		dnodes[i].children[0] = LittleLong(dnodes[i].children[0]);
		dnodes[i].children[1] = LittleLong(dnodes[i].children[1]);
		dnodes[i].firstface = LittleShort(dnodes[i].firstface);
		dnodes[i].numfaces = LittleShort(dnodes[i].numfaces);
	}

	/* leafs */
	for (i = 0; i < numleafs; i++) {
		dleafs[i].contentFlags = LittleLong(dleafs[i].contentFlags);
		dleafs[i].area = LittleShort(dleafs[i].area);
		for (j = 0; j < 3; j++) {
			dleafs[i].mins[j] = LittleShort(dleafs[i].mins[j]);
			dleafs[i].maxs[j] = LittleShort(dleafs[i].maxs[j]);
		}

		dleafs[i].firstleafbrush = LittleShort(dleafs[i].firstleafbrush);
		dleafs[i].numleafbrushes = LittleShort(dleafs[i].numleafbrushes);
	}

	/* leafbrushes */
	for (i = 0; i < numleafbrushes; i++)
		dleafbrushes[i] = LittleShort(dleafbrushes[i]);

	/* surfedges */
	for (i = 0; i < numsurfedges; i++)
		dsurfedges[i] = LittleLong(dsurfedges[i]);

	/* edges */
	for (i = 0; i < numedges; i++) {
		dedges[i].v[0] = LittleShort(dedges[i].v[0]);
		dedges[i].v[1] = LittleShort(dedges[i].v[1]);
	}

	/* brushes */
	for (i = 0; i < numbrushes; i++) {
		dbrushes[i].firstside = LittleLong(dbrushes[i].firstside);
		dbrushes[i].numsides = LittleLong(dbrushes[i].numsides);
		dbrushes[i].contentFlags = LittleLong(dbrushes[i].contentFlags);
	}

	/* brushsides */
	for (i = 0; i < numbrushsides; i++) {
		dbrushsides[i].planenum = LittleShort(dbrushsides[i].planenum);
		dbrushsides[i].texinfo = LittleShort(dbrushsides[i].texinfo);
	}
}


static dBspHeader_t *header;

/**
 * @brief
 */
static int CopyLump (int lump, void *dest, int size)
{
	int length, ofs;

	length = header->lumps[lump].filelen;
	ofs = header->lumps[lump].fileofs;

	if (length == 0)
		return 0;
	if (length % size)
		Sys_Error("LoadBSPFile: odd lump size");

	memcpy(dest, (byte *)header + ofs, length);

	return length / size;
}

/**
 * @brief
 */
void LoadBSPFile (const char *filename)
{
	int i;

	/* load the file header */
	LoadFile(filename, (void **)&header);

	/* swap the header */
	for (i = 0; i < sizeof(dBspHeader_t) / 4; i++)
		((int *)header)[i] = LittleLong(((int *)header)[i]);

	if (header->ident != IDBSPHEADER)
		Sys_Error("%s is not a IBSP file", filename);
	if (header->version != BSPVERSION)
		Sys_Error("%s is version %i, not %i", filename, header->version, BSPVERSION);

	nummodels = CopyLump(LUMP_MODELS, dmodels, sizeof(dBspModel_t));
	numvertexes = CopyLump(LUMP_VERTEXES, dvertexes, sizeof(dBspVertex_t));
	numplanes = CopyLump(LUMP_PLANES, dplanes, sizeof(dBspPlane_t));
	numleafs = CopyLump(LUMP_LEAFS, dleafs, sizeof(dBspLeaf_t));
	numnodes = CopyLump(LUMP_NODES, dnodes, sizeof(dBspNode_t));
	numtexinfo = CopyLump(LUMP_TEXINFO, texinfo, sizeof(dBspTexinfo_t));
	numfaces = CopyLump(LUMP_FACES, dfaces, sizeof(dBspFace_t));
	numleafbrushes = CopyLump(LUMP_LEAFBRUSHES, dleafbrushes, sizeof(dleafbrushes[0]));
	numsurfedges = CopyLump(LUMP_SURFEDGES, dsurfedges, sizeof(dsurfedges[0]));
	numedges = CopyLump(LUMP_EDGES, dedges, sizeof(dBspEdge_t));
	numbrushes = CopyLump(LUMP_BRUSHES, dbrushes, sizeof(dBspBrush_t));
	numbrushsides = CopyLump(LUMP_BRUSHSIDES, dbrushsides, sizeof(dBspBrushSide_t));
	routedatasize = CopyLump(LUMP_ROUTING, droutedata, 1);
	lightdatasize[LIGHTMAP_NIGHT] = CopyLump(LUMP_LIGHTING_NIGHT, dlightdata[LIGHTMAP_NIGHT], 1);
	lightdatasize[LIGHTMAP_DAY] = CopyLump(LUMP_LIGHTING_DAY, dlightdata[LIGHTMAP_DAY], 1);
	entdatasize = CopyLump(LUMP_ENTITIES, dentdata, 1);

	FreeFile(header);		/* everything has been copied out */

	/* swap everything */
	SwapBSPFile();
}

/**
 * @brief
 * @sa WriteBSPFile
 */
static inline void AddLump (qFILE *bspfile, dBspHeader_t *header, int lumpnum, void *data, int len)
{
	lump_t *lump;

	lump = &header->lumps[lumpnum];

	lump->fileofs = LittleLong(ftell(bspfile->f));
	lump->filelen = LittleLong(len);
	/* 4 byte align */
	SafeWrite(bspfile, data, (len + 3) &~ 3);
}

/**
 * @brief Swaps the bsp file in place, so it should not be referenced again
 */
void WriteBSPFile (const char *filename)
{
	qFILE bspfile;
	dBspHeader_t outheader;

	memset(&outheader, 0, sizeof(outheader));
	memset(&bspfile, 0, sizeof(bspfile));

	SwapBSPFile();

	outheader.ident = LittleLong(IDBSPHEADER);
	outheader.version = LittleLong(BSPVERSION);

	SafeOpenWrite(filename, &bspfile);
	SafeWrite(&bspfile, &outheader, sizeof(dBspHeader_t));	/* overwritten later */

	AddLump(&bspfile, &outheader, LUMP_PLANES, dplanes, numplanes * sizeof(dBspPlane_t));
	AddLump(&bspfile, &outheader, LUMP_LEAFS, dleafs, numleafs * sizeof(dBspLeaf_t));
	AddLump(&bspfile, &outheader, LUMP_VERTEXES, dvertexes, numvertexes * sizeof(dBspVertex_t));
	AddLump(&bspfile, &outheader, LUMP_NODES, dnodes, numnodes * sizeof(dBspNode_t));
	AddLump(&bspfile, &outheader, LUMP_TEXINFO, texinfo, numtexinfo * sizeof(dBspTexinfo_t));
	AddLump(&bspfile, &outheader, LUMP_FACES, dfaces, numfaces * sizeof(dBspFace_t));
	AddLump(&bspfile, &outheader, LUMP_BRUSHES, dbrushes, numbrushes * sizeof(dBspBrush_t));
	AddLump(&bspfile, &outheader, LUMP_BRUSHSIDES, dbrushsides, numbrushsides * sizeof(dBspBrushSide_t));
	AddLump(&bspfile, &outheader, LUMP_LEAFBRUSHES, dleafbrushes, numleafbrushes * sizeof(dleafbrushes[0]));
	AddLump(&bspfile, &outheader, LUMP_SURFEDGES, dsurfedges, numsurfedges * sizeof(dsurfedges[0]));
	AddLump(&bspfile, &outheader, LUMP_EDGES, dedges, numedges * sizeof(dBspEdge_t));
	AddLump(&bspfile, &outheader, LUMP_MODELS, dmodels, nummodels * sizeof(dBspModel_t));
	AddLump(&bspfile, &outheader, LUMP_LIGHTING_NIGHT, dlightdata[0], lightdatasize[0]);
	AddLump(&bspfile, &outheader, LUMP_LIGHTING_DAY, dlightdata[1], lightdatasize[1]);
	AddLump(&bspfile, &outheader, LUMP_ROUTING, droutedata, routedatasize);
	AddLump(&bspfile, &outheader, LUMP_ENTITIES, dentdata, entdatasize);

	fseek(bspfile.f, 0, SEEK_SET);
	SafeWrite(&bspfile, &outheader, sizeof(outheader));
	CloseFile(&bspfile);
	SwapBSPFile();
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
	Com_Printf("%5i models            %7i\n", nummodels, (int)(nummodels * sizeof(dBspModel_t)));
	Com_Printf("%5i brushes           %7i\n", numbrushes, (int)(numbrushes * sizeof(dBspBrush_t)));
	Com_Printf("%5i brushsides        %7i\n", numbrushsides, (int)(numbrushsides * sizeof(dBspBrushSide_t)));
	Com_Printf("%5i planes            %7i\n", numplanes, (int)(numplanes * sizeof(dBspPlane_t)));
	Com_Printf("%5i texinfo           %7i\n", numtexinfo, (int)(numtexinfo * sizeof(dBspTexinfo_t)));
	Com_Printf("%5i entdata           %7i\n", num_entities, entdatasize);

	Com_Printf("\n");

	Com_Printf("%5i vertexes          %7i\n", numvertexes, (int)(numvertexes * sizeof(dBspVertex_t)));
	Com_Printf("%5i nodes             %7i\n", numnodes, (int)(numnodes * sizeof(dBspNode_t)));
	Com_Printf("%5i faces             %7i\n", numfaces, (int)(numfaces * sizeof(dBspFace_t)));
	Com_Printf("%5i leafs             %7i\n", numleafs, (int)(numleafs * sizeof(dBspLeaf_t)));
	Com_Printf("%5i leafbrushes       %7i\n", numleafbrushes, (int)(numleafbrushes * sizeof(dleafbrushes[0])));
	Com_Printf("%5i surfedges         %7i\n", numsurfedges, (int)(numsurfedges * sizeof(dsurfedges[0])));
	Com_Printf("%5i edges             %7i\n", numedges, (int)(numedges * sizeof(dBspEdge_t)));
	Com_Printf("night lightdata         %7i\n", lightdatasize[0]);
	Com_Printf("  day lightdata         %7i\n", lightdatasize[1]);
	Com_Printf("      routedata         %7i\n", routedatasize);
}


int num_entities;
entity_t entities[MAX_MAP_ENTITIES];

/**
 * @brief
 */
static void StripTrailing (char *e)
{
	char *s;

	s = e + strlen(e)-1;
	while (s >= e && *s <= 32) {
		*s = 0;
		s--;
	}
}

/**
 * @brief
 */
static inline char *copystring (const char *s)
{
	char *b = (char*)malloc(sizeof(char) * (strlen(s) + 1));
	strcpy(b, s);
	return b;
}

/**
 * @brief
 * @sa ParseEntity
 * @sa ParseMapEntity
 */
epair_t *ParseEpair (void)
{
	epair_t	*e;

	e = malloc(sizeof(epair_t));
	memset(e, 0, sizeof(epair_t));

	if (strlen(parsedToken) >= MAX_KEY - 1)
		Sys_Error("ParseEpar: token too long");
	e->key = copystring(parsedToken);
	GetToken(qfalse);
	if (strlen(parsedToken) >= MAX_VALUE - 1)
		Sys_Error("ParseEpar: token too long");
	e->value = copystring(parsedToken);

	/* strip trailing spaces */
	StripTrailing(e->key);
	StripTrailing(e->value);

	return e;
}


/**
 * @brief
 * @sa ParseEntities
 */
static qboolean ParseEntity (void)
{
	epair_t	 *e;
	entity_t *mapent;

	if (!GetToken(qtrue))
		return qfalse;

	if (strcmp(parsedToken, "{"))
		Sys_Error("ParseEntity: { not found");

	if (num_entities >= MAX_MAP_ENTITIES)
		Sys_Error("num_entities >= MAX_MAP_ENTITIES (%i)", num_entities);

	mapent = &entities[num_entities];
	num_entities++;

	do {
		if (!GetToken(qtrue))
			Sys_Error("ParseEntity: EOF without closing brace");
		if (!strcmp(parsedToken, "}") )
			break;
		e = ParseEpair ();
		e->next = mapent->epairs;
		mapent->epairs = e;
	} while (1);

	return qtrue;
}

/**
 * @brief Parses the dentdata string into entities
 * @sa UnparseEntities
 * @sa ParseEntity
 */
void ParseEntities (void)
{
	num_entities = 0;
	ParseFromMemory(dentdata, entdatasize);

	while (ParseEntity()) {
	}
}


/**
 * @brief Generates the dentdata string from all the entities
 * @sa ParseEntities
 */
void UnparseEntities (void)
{
	char *buf, *end;
	epair_t *ep;
	char line[2048], key[1024], value[1024];
	int i;

	buf = dentdata;
	end = buf;
	*end = 0;

	for (i = 0; i < num_entities; i++) {
		ep = entities[i].epairs;
		if (!ep)
			continue;	/* ent got removed */

		strcat (end,"{\n");
		end += 2;

		for (ep = entities[i].epairs; ep; ep = ep->next) {
			strncpy(key, ep->key, sizeof(key));
			StripTrailing(key);
			strncpy(value, ep->value, sizeof(value));
			StripTrailing(value);

			snprintf(line, sizeof(line), "\"%s\" \"%s\"\n", key, value);
			strcat(end, line);
			end += strlen(line);
		}
		strcat(end,"}\n");
		end += 2;

		if (end > buf + MAX_MAP_ENTSTRING)
			Sys_Error("Entity text too long");
	}
	entdatasize = end - buf + 1;
}

/**
 * @brief
 */
void SetKeyValue (entity_t *ent, const char *key, const char *value)
{
	epair_t *ep;

	for (ep = ent->epairs; ep; ep = ep->next)
		if (!strcmp (ep->key, key) ) {
			free(ep->value);
			ep->value = copystring(value);
			return;
		}
	ep = malloc (sizeof(*ep));
	ep->next = ent->epairs;
	ent->epairs = ep;
	ep->key = copystring(key);
	ep->value = copystring(value);
}

/**
 * @brief
 */
const char *ValueForKey (entity_t *ent, const char *key)
{
	epair_t *ep;

	for (ep = ent->epairs; ep; ep = ep->next)
		if (!strcmp(ep->key, key) )
			return ep->value;
	return "";
}

/**
 * @brief
 */
vec_t FloatForKey (entity_t *ent, const char *key)
{
	const char *k;

	k = ValueForKey(ent, key);
	return atof(k);
}

/**
 * @brief
 */
void GetVectorForKey (entity_t *ent, const char *key, vec3_t vec)
{
	const char *k;
	double v1, v2, v3;

	k = ValueForKey(ent, key);
	/* scanf into doubles, then assign, so it is vec_t size independent */
	v1 = v2 = v3 = 0;
	sscanf(k, "%lf %lf %lf", &v1, &v2, &v3);
	vec[0] = v1;
	vec[1] = v2;
	vec[2] = v3;
}
