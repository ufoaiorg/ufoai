/**
 * @file cmodels.c
 * @brief model loading and grid oriented movement and scanning
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

#include "qcommon.h"
#define	ON_EPSILON	0.1
/* TODO: What is PH? */
#define PH		(UNIT_HEIGHT-12)
#define QUANT	4
#define GRENADE_ALPHAFAC	0.7
#define GRENADE_MINALPHA	M_PI/6
#define GRENADE_MAXALPHA	M_PI*7/16
#define QUANT		4

/* 1/32 epsilon to keep floating point happy */
#define	DIST_EPSILON	(0.03125)

typedef struct {
	cplane_t *plane;
	vec3_t mins, maxs;
	int children[2];			/* negative numbers are leafs */
} cnode_t;

typedef struct {
	cplane_t *plane;
	mapsurface_t *surface;
} cbrushside_t;

typedef struct {
	int contents;
	unsigned short firstleafbrush;
	unsigned short numleafbrushes;
} cleaf_t;

typedef struct {
	int contents;
	int numsides;
	int firstbrushside;
	int checkcount;				/* to avoid repeated testings */
} cbrush_t;

typedef struct tnode_s {
	int type;
	vec3_t normal;
	float dist;
	int children[2];
	int pad;
} tnode_t;

typedef struct chead_s {
	int cnode;
	int level;
} chead_t;

typedef struct {
	char name[MAX_QPATH];

	void *extraData;
	int extraDataSize;

	int numbrushsides;
	cbrushside_t *brushsides;

	int numtexinfo;
	mapsurface_t *surfaces;

	int numplanes;
	cplane_t *planes;

	int numnodes;
	cnode_t *nodes;

	int numleafs;
	cleaf_t *leafs;
	int emptyleaf, solidleaf;

	int numleafbrushes;
	unsigned short *leafbrushes;

	int numcmodels;
	cmodel_t *cmodels;

	int numbrushes;
	cbrush_t *brushes;

	/* tracing box */
	cplane_t *box_planes;
	int box_headnode;
	cbrush_t *box_brush;
	cleaf_t *box_leaf;

	/* line tracing */
	tnode_t *tnodes;
	int numtheads;
	int thead[258];

	int numcheads;
	chead_t cheads[MAX_MAP_NODES];
} mapTile_t;

typedef struct routing_s {
	byte route[8][256][256];
	byte fall[256][256];
	byte step[256][256];

	byte area[HEIGHT][WIDTH][WIDTH];
	byte areaStored[HEIGHT][WIDTH][WIDTH];

	/* forbidden list */
	byte **fblist;
	int fblength;
} routing_t;

/* extern */
int c_pointcontents;
int c_traces, c_brush_traces;
char map_entitystring[MAX_MAP_ENTSTRING];
vec3_t map_min, map_max;
struct routing_s svMap, clMap;

/* static */
static mapsurface_t nullsurface;
static mapTile_t mapTiles[MAX_MAPTILES];
static int numTiles = 0;
static mapTile_t *curTile;
static int checkcount;
static int numInline;
static byte sh_low;
static byte sh_big;
static char **inlineList;
static int cur_level;
static vec3_t tr_end;
static byte *cmod_base;
static vec3_t shift;
static byte filled[256][256];

static const vec3_t v_dup = { 0, 0, PH - UNIT_HEIGHT / 2 };
static const vec3_t v_dwn = { 0, 0, -UNIT_HEIGHT / 2 };
static const vec3_t testvec[5] = { {-UNIT_SIZE / 2 + 5, -UNIT_SIZE / 2 + 5, 0}, {UNIT_SIZE / 2 - 5, UNIT_SIZE / 2 - 5, 0}, {-UNIT_SIZE / 2 + 5, UNIT_SIZE / 2 - 5, 0}, {UNIT_SIZE / 2 - 5, -UNIT_SIZE / 2 + 5, 0}, {0, 0, 0} };

static int leaf_count, leaf_maxcount;
static int *leaf_list;
static float *leaf_mins, *leaf_maxs;
static int leaf_topnode;

static vec3_t trace_start, trace_end;
static vec3_t trace_mins, trace_maxs;
static vec3_t trace_extents;

static trace_t trace_trace;
static int trace_contents;
static qboolean trace_ispoint;			/* optimized case */
static tnode_t *tnode_p;
static byte stf;
static byte tfList[HEIGHT][WIDTH][WIDTH];
static byte tf;

void CM_MakeTnodes(void);
void CM_InitBoxHull(void);


/*
===============================================================================
MAP LOADING
===============================================================================
*/


/**
 * @brief
 * @param[in] l
 * @sa CM_AddMapTile
 */
static void CMod_LoadSubmodels(lump_t * l)
{
	dmodel_t *in;
	cmodel_t *out;
	int i, j, count;

	in = (void *) (cmod_base + l->fileofs);
	if (l->filelen % sizeof(dmodel_t))
		Com_Error(ERR_DROP, "CMod_LoadSubmodels: funny lump size (%i => %i", l->filelen, sizeof(dmodel_t));
	count = l->filelen / sizeof(dmodel_t);

	if (count < 1)
		Com_Error(ERR_DROP, "Map with no models");
	if (count > MAX_MAP_MODELS)
		Com_Error(ERR_DROP, "Map has too many models");

	out = Hunk_Alloc(count * sizeof(cmodel_t));
	curTile->cmodels = out;
	curTile->numcmodels = count;

	for (i = 0; i < count; i++, in++, out++) {
		out = &curTile->cmodels[i];

		/* spread the mins / maxs by a pixel */
		for (j = 0; j < 3; j++) {
			out->mins[j] = LittleFloat(in->mins[j]) - 1 + shift[j];
			out->maxs[j] = LittleFloat(in->maxs[j]) + 1 + shift[j];
			out->origin[j] = LittleFloat(in->origin[j]);
		}
		out->headnode = LittleLong(in->headnode);
		out->tile = curTile - mapTiles;
	}
}


/**
 * @brief
 * @param[in] l
 * @sa CM_AddMapTile
 */
static void CMod_LoadSurfaces(lump_t * l)
{
	texinfo_t *in;
	mapsurface_t *out;
	int i, count;

	in = (void *) (cmod_base + l->fileofs);
	if (l->filelen % sizeof(texinfo_t))
		Com_Error(ERR_DROP, "CMod_LoadSurfaces: funny lump size: %i", l->filelen);
	count = l->filelen / sizeof(texinfo_t);

	if (count < 1)
		Com_Error(ERR_DROP, "Map with no surfaces");
	if (count > MAX_MAP_TEXINFO)
		Com_Error(ERR_DROP, "Map has too many surfaces");

	out = Hunk_Alloc(count * sizeof(mapsurface_t));

	curTile->surfaces = out;
	curTile->numtexinfo = count;

	for (i = 0; i < count; i++, in++, out++) {
		Q_strncpyz(out->c.name, in->texture, sizeof(out->c.name));
		Q_strncpyz(out->rname, in->texture, sizeof(out->rname));
		out->c.flags = LittleLong(in->flags);
		out->c.value = LittleLong(in->value);
	}
}


/**
 * @brief
 * @param[in] l
 * @sa CM_AddMapTile
 */
static void CMod_LoadNodes(lump_t * l)
{
	dnode_t *in;
	int child;
	cnode_t *out;
	int i, j, count;

	in = (void *) (cmod_base + l->fileofs);
	if (l->filelen % sizeof(dnode_t))
		Com_Error(ERR_DROP, "CMod_LoadNodes: funny lump size: %i", l->filelen);
	count = l->filelen / sizeof(dnode_t);

	if (count < 1)
		Com_Error(ERR_DROP, "Map has no nodes");
	if (count > MAX_MAP_NODES)
		Com_Error(ERR_DROP, "Map has too many nodes");

	/* add some for the box */
	out = Hunk_Alloc((count + 6) * sizeof(cnode_t));

	curTile->numnodes = count;
	curTile->nodes = out;

	for (i = 0; i < count; i++, out++, in++) {
		if (in->planenum == -1)
			out->plane = NULL;
		else
			out->plane = curTile->planes + LittleLong(in->planenum);

		VectorAdd(in->mins, shift, out->mins);
		VectorAdd(in->maxs, shift, out->maxs);

		for (j = 0; j < 2; j++) {
			child = LittleLong(in->children[j]);
			out->children[j] = child;
		}
	}

}

/**
 * @brief
 * @param[in] l
 * @sa CM_AddMapTile
 */
static void CMod_LoadBrushes(lump_t * l)
{
	dbrush_t *in;
	cbrush_t *out;
	int i, count;

	in = (void *) (cmod_base + l->fileofs);
	if (l->filelen % sizeof(dbrush_t))
		Com_Error(ERR_DROP, "CMod_LoadBrushes: funny lump size: %i", l->filelen);
	count = l->filelen / sizeof(dbrush_t);

	if (count > MAX_MAP_BRUSHES)
		Com_Error(ERR_DROP, "Map has too many brushes");

	/* add some for the box */
	out = Hunk_Alloc((count + 1) * sizeof(cbrush_t));

	curTile->numbrushes = count;
	curTile->brushes = out;

	for (i = 0; i < count; i++, out++, in++) {
		out->firstbrushside = LittleLong(in->firstside);
		out->numsides = LittleLong(in->numsides);
		out->contents = LittleLong(in->contents);
	}
}

/**
 * @brief
 * @param[in] l
 * @sa CM_AddMapTile
 */
static void CMod_LoadLeafs(lump_t * l)
{
	int i;
	cleaf_t *out;
	dleaf_t *in;
	int count;

	in = (void *) (cmod_base + l->fileofs);
	if (l->filelen % sizeof(dleaf_t))
		Com_Error(ERR_DROP, "CMod_LoadLeafs: funny lump size: %i", l->filelen);
	count = l->filelen / sizeof(dleaf_t);

	if (count < 1)
		Com_Error(ERR_DROP, "Map with no leafs");
	/* need to save space for box planes */
	if (count > MAX_MAP_PLANES)
		Com_Error(ERR_DROP, "Map has too many planes");

	/* add some for the box */
	out = Hunk_Alloc((count + 1) * sizeof(cleaf_t));

	curTile->numleafs = count;
	curTile->leafs = out;

	for (i = 0; i < count; i++, in++, out++) {
		out->contents = LittleLong(in->contents);
		out->firstleafbrush = LittleShort(in->firstleafbrush);
		out->numleafbrushes = LittleShort(in->numleafbrushes);
	}

	if (curTile->leafs[0].contents != CONTENTS_SOLID)
		Com_Error(ERR_DROP, "Map leaf 0 is not CONTENTS_SOLID");
	curTile->solidleaf = 0;
	curTile->emptyleaf = -1;
	for (i = 1; i < curTile->numleafs; i++) {
		if (!curTile->leafs[i].contents) {
			curTile->emptyleaf = i;
			break;
		}
	}
	if (curTile->emptyleaf == -1)
		Com_Error(ERR_DROP, "Map does not have an empty leaf");
}

/**
 * @brief
 * @param[in] l
 * @sa CM_AddMapTile
 */
static void CMod_LoadPlanes(lump_t * l)
{
	int i, j;
	cplane_t *out;
	dplane_t *in;
	int count;
	int bits;

	in = (void *) (cmod_base + l->fileofs);
	if (l->filelen % sizeof(dplane_t))
		Com_Error(ERR_DROP, "CMod_LoadPlanes: funny lump size: %i", l->filelen);
	count = l->filelen / sizeof(dplane_t);

	if (count < 1)
		Com_Error(ERR_DROP, "Map with no planes");
	/* need to save space for box planes */
	if (count > MAX_MAP_PLANES)
		Com_Error(ERR_DROP, "Map has too many planes");

	/* add some for the box */
	out = Hunk_Alloc((count + 12) * sizeof(cplane_t));

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

		/* shift */
		for (j = 0; j < 3; j++)
			out->dist += out->normal[j] * shift[j];
	}
}

/**
 * @brief
 * @param[in] l
 * @sa CM_AddMapTile
 */
static void CMod_LoadLeafBrushes(lump_t * l)
{
	int i;
	unsigned short *out;
	unsigned short *in;
	int count;

	in = (void *) (cmod_base + l->fileofs);
	if (l->filelen % sizeof(unsigned short))
		Com_Error(ERR_DROP, "CMod_LoadLeafBrushes: funny lump size: %i", l->filelen);
	count = l->filelen / sizeof(unsigned short);

	/* add some for the box */
	out = Hunk_Alloc((count + 1) * sizeof(unsigned short));

	if (count < 1)
		Com_Error(ERR_DROP, "Map with no planes");
	/* need to save space for box planes */
	if (count > MAX_MAP_LEAFBRUSHES)
		Com_Error(ERR_DROP, "Map has too many leafbrushes");

	curTile->numleafbrushes = count;
	curTile->leafbrushes = out;

	for (i = 0; i < count; i++, in++, out++)
		*out = LittleShort(*in);
}

/**
 * @brief
 * @param[in] l
 * @sa CM_AddMapTile
 */
static void CMod_LoadBrushSides(lump_t * l)
{
	int i, j;
	cbrushside_t *out;
	dbrushside_t *in;
	int count;
	int num;

	in = (void *) (cmod_base + l->fileofs);
	if (l->filelen % sizeof(dbrushside_t))
		Com_Error(ERR_DROP, "CMod_LoadBrushSides: funny lump size: %i", l->filelen);
	count = l->filelen / sizeof(dbrushside_t);

	/* need to save space for box planes */
	if (count > MAX_MAP_BRUSHSIDES)
		Com_Error(ERR_DROP, "Map has too many planes");

	/* add some for the box */
	out = Hunk_Alloc((count + 6) * sizeof(cbrushside_t));

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
 * @brief Source will be set to the end of the compressed data block!
 */
static int Cmod_DeCompressRouting(byte ** source, byte * dataStart)
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
int CheckBSPFile(char *filename)
{
	int i;
	int header[2];
	FILE *file = NULL;
	char name[MAX_QPATH];

	/* load the file */
	Com_sprintf(name, MAX_QPATH, "maps/%s.bsp", filename);

	FS_FOpenFile(name, &file);
	if (!file)
		return 1;

	FS_Read(header, sizeof(header), file);

	FS_FCloseFile(file);

	for (i = 0; i < 2; i++)
		header[i] = LittleLong(header[i]);

	if (header[0] != IDBSPHEADER)
		return 2;
	if (header[1] != BSPVERSION)
		return 3;

	/* valid BSP-File */
	return 0;
}

/**
 * @brief
 * @param[in] start
 * @param[in] stop
 * @sa CM_TestLine
 * @sa CM_InlineModel
 * @sa CM_TransformedBoxTrace
 */
int CM_EntTestLine(vec3_t start, vec3_t stop)
{
	trace_t trace;
	cmodel_t *model;
	char **name;

	/* trace against world first */
	if (CM_TestLine(start, stop))
		return 1;
	if (!inlineList)
		return 0;

	for (name = inlineList; *name; name++) {
		if (**name != '*')
			continue;
		model = CM_InlineModel(*name);
		if (!model)
			continue;
/*		Com_Printf("CM_EntTestLine call function\n"); */
		trace = CM_TransformedBoxTrace(start, stop, vec3_origin, vec3_origin, model->tile, model->headnode, MASK_ALL, model->origin, vec3_origin);
		if (trace.startsolid || trace.fraction < 1.0)
			return 1;
	}

	/* not blocked */
	return 0;
}


/**
 * @brief
 * @param[in] start
 * @param[in] stop
 * @param[in] end
 * @sa CM_TestLineDM
 * @sa CM_TransformedBoxTrace
 */
int CM_EntTestLineDM(vec3_t start, vec3_t stop, vec3_t end)
{
	trace_t trace;
	cmodel_t *model;
	char **name;
	int blocked;

	/* trace against world first */
	blocked = CM_TestLineDM(start, stop, end);
	if (!inlineList)
		return blocked;

	for (name = inlineList; *name; name++) {
		if (**name != '*')
			continue;
		model = CM_InlineModel(*name);
		if (!model)
			continue;
/*		Com_Printf("CM_EntTestLineDM call function\n"); */
		trace = CM_TransformedBoxTrace(start, end, vec3_origin, vec3_origin, model->tile, model->headnode, MASK_ALL, model->origin, vec3_origin);
		if (trace.startsolid) {
			VectorCopy(start, end);
			return 1;
		}
		if (trace.fraction < 1.0) {
			blocked = 1;
			VectorCopy(trace.endpos, end);
		}
	}

	/* return result */
	return blocked;
}


/**
 * @brief
 * @param[in] map
 * @param[in] x
 * @param[in] y
 * @param[in] z
 * @param[in] dir
 * @param[in] fill
 */
qboolean CM_TestConnection(routing_t * map, int x, int y, int z, int dir, qboolean fill)
{
	vec3_t start, end;
	pos3_t pos;
	int h, sh, ax, ay, az;

	/* totally blocked unit */
	if ((fill && (filled[y][x] & (1 << z))) || (map->fall[y][x] == 0xFF))
		return qfalse;

	/* get step height and trace vectors */
	sh = (map->step[y][x] & (1 << z)) ? sh_big : sh_low;
	h = map->route[z][y][x] & 0x0F;
	VectorSet(pos, x, y, z);
	PosToVec(pos, start);
	start[2] += h * 4;
	VectorSet(end, start[0] + 32 * dvecs[dir][0], start[1] + 32 * dvecs[dir][1], start[2]);

	ax = x + dvecs[dir][0];
	ay = y + dvecs[dir][1];
	az = z + (h + sh) / 0x10;
	h = ((map->route[z][y][x] & 0x0F) + sh) % 0x10;

	/* assume blocked */
	map->route[z][y][x] &= ~((0x10 << dir) & UCHAR_MAX);

	/* test filled */
	if (fill && (filled[ay][ax] & (1 << az)))
		return qfalse;

	/* test height */
	if ((map->route[az][ay][ax] & 0x0F) > h)
		return qfalse;

	/* center check */
	if (CM_EntTestLine(start, end))
		return qfalse;

	/* lower check */
	start[2] = end[2] -= UNIT_HEIGHT / 2 - sh * 4 - 2;
	if (CM_EntTestLine(start, end))
		return qfalse;

	/* no wall */
	map->route[z][y][x] |= (0x10 << dir) & UCHAR_MAX;
	return qtrue;
}


/**
 * @brief
 * @param[in] map
 * @param[in] x
 * @param[in] y
 * @param[in] z
 */
void CM_CheckUnit(routing_t * map, int x, int y, int z)
{
	vec3_t start, end;
	vec3_t tend, tvs, tve;
	pos3_t pos;
	float height;
	int i;

	/* reset flags */
	filled[y][x] &= ~(1 << z);
	map->fall[y][x] &= ~(1 << z);

	/* prepare fall down check */
	VectorSet(pos, x, y, z);
	PosToVec(pos, end);
	VectorCopy(end, start);
	start[2] -= UNIT_HEIGHT / 2 - 4;
	end[2] -= UNIT_HEIGHT / 2 + 4;

	/* test for fall down */
	if (CM_EntTestLine(start, end)) {
		PosToVec(pos, end);
		VectorAdd(end, v_dup, start);
		VectorAdd(end, v_dwn, end);
		height = 0;

		/* test for ground with a "middled" height */
		for (i = 0; i < 5; i++) {
			VectorAdd(start, testvec[i], tvs);
			VectorAdd(end, testvec[i], tve);
			CM_EntTestLineDM(tvs, tve, tend);
			height += tend[2];

			/* stop if it's totally blocked somewhere */
			/* and try a higher starting point */
			if (VectorCompare(tvs, tend))
				break;
		}

		/* tend[0] & [1] are correct (testvec[4]) */
		height += tend[2];
		tend[2] = height / 6.0;

		if (i == 5 && !VectorCompare(start, tend)) {
			/* found a possibly valid ground */
			height = PH - (start[2] - tend[2]);
			end[2] = start[2] + height;

			if (!CM_EntTestLineDM(start, end, tend))
				map->route[z][y][x] = ((height + QUANT / 2) / QUANT < 0) ? 0 : (height + QUANT / 2) / QUANT;
			else
				filled[y][x] |= 1 << z;	/* don't enter */
		} else {
			/* elevated a lot */
			end[2] = start[2];
			start[2] += UNIT_HEIGHT - PH;
			height = 0;

			/* test for ground with a "middled" height */
			for (i = 0; i < 5; i++) {
				VectorAdd(start, testvec[i], tvs);
				VectorAdd(end, testvec[i], tve);
				CM_EntTestLineDM(tvs, tve, tend);
				height += tend[2];
			}
			/* tend[0] & [1] are correct (testvec[4]) */
			height += tend[2];
			tend[2] = height / 6.0;

			if (VectorCompare(start, tend)) {
				filled[y][x] |= 1 << z;	/* don't enter */
			} else {
				/* found a possibly valid elevated ground */
				end[2] = start[2] + PH - (start[2] - tend[2]);
				height = UNIT_HEIGHT - (start[2] - tend[2]);

/*				printf( "%i %i\n", (int)height, (int)(start[2]-tend[2]) ); */

				if (!CM_EntTestLineDM(start, end, tend))
					map->route[z][y][x] = ((height + QUANT / 2) / QUANT < 0) ? 0 : (height + QUANT / 2) / QUANT;
				else
					filled[y][x] |= 1 << z;	/* don't enter */
			}
		}
	} else {
		/* fall down */
		map->route[z][y][x] = 0;
		map->fall[y][x] |= 1 << z;
	}
}


/**
 * @brief
 * @param[in] map
 */
void CMod_GetMapSize(routing_t * map)
{
	vec3_t offset = { 100, 100, 100 };
	pos3_t min, max;
	int x, y;

	VectorSet(min, 255, 255, 0);
	VectorSet(max, 0, 0, 0);

	/* get border */
	for (y = 0; y < 256; y++)
		for (x = 0; x < 256; x++)
			if (map->fall[y][x] != 0xFF) {
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
 * @brief
 * @param[in] l
 * @param[in] sX
 * @param[in] sY
 * @param[in] sZ
 * @sa CM_AddMapTile
 */
void CMod_LoadRouting(lump_t * l, int sX, int sY, int sZ)
{
	static byte temp_route[8][256][256];
	static byte temp_fall[256][256];
	static byte temp_step[256][256];
	byte *source;
	int length;
	int x, y, z;
	int maxX, maxY;
	int i, ax, ay;

	inlineList = NULL;

	if (!l->filelen)
		Com_Error(ERR_DROP, "Map has NO routing lump");

	source = cmod_base + l->fileofs;
	sh_low = *source++;
	sh_big = *source++;
	length = Cmod_DeCompressRouting(&source, &temp_route[0][0][0]);
	length += Cmod_DeCompressRouting(&source, &temp_fall[0][0]);
	length += Cmod_DeCompressRouting(&source, &temp_step[0][0]);

	if (length != 256 * 256 * 10)
		Com_Error(ERR_DROP, "Map has BAD routing lump");

	/* shift and merge the routing information */
	maxX = sX > 0 ? 256 - sX : 256;
	maxY = sY > 0 ? 256 - sY : 256;
	sZ = 0;

	for (y = sY < 0 ? -sY : 0; y < maxY; y++)
		for (x = sX < 0 ? -sX : 0; x < maxX; x++)
			if (temp_fall[y][x] != 0xFF) {
				/* add new quant */
				clMap.fall[y + sY][x + sX] = temp_fall[y][x];
				clMap.step[y + sY][x + sX] = temp_step[y][x];

				/* copy routing info */
				for (z = 0; z < 8; z++)
					clMap.route[z][y + sY][x + sX] = temp_route[z][y][x];

				/* check border connections */
				for (i = 0; i < 8; i++) {
					/* test for border */
					ax = x + dvecs[i][0];
					ay = y + dvecs[i][1];
					if (temp_fall[ay][ax] != 0xFF)
						continue;

					/* check for walls */
					for (z = 0; z < 8; z++) {
						CM_TestConnection(&clMap, x + sX, y + sY, z, i, qfalse);
						CM_TestConnection(&clMap, ax + sX, ay + sY, z, i ^ 1, qfalse);
					}
				}
			}

	/* calculate new border */
	CMod_GetMapSize(&clMap);

/*	Com_Printf( "route: (%i %i) fall: %i step: %i\n", */
/*		(int)map->route[0][0][0], (int)map->route[1][0][0], (int)map->fall[0][0], (int)map->step[0][0] ); */
}


/**
 * @brief
 * @param[in] l
 * @sa CM_AddMapTile
 */
void CMod_LoadEntityString(lump_t * l)
{
	char *com_token;
	char *es;
	char keyname[256];
	vec3_t v;
	int num;

	if (l->filelen + 1 > MAX_MAP_ENTSTRING)
		Com_Error(ERR_DROP, "Map has too large entity lump");

	/* marge entitystring information */
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
#if 0
	/* copy new entitystring */
	memcpy(map_entitystringpos, cmod_base + l->fileofs, l->filelen);

	/* update length */
	map_entitystringpos += l->filelen - 1;
#endif
}



/**
 * @brief Frees a map tile
 * @param[in] tile
 */
void CM_FreeTile(mapTile_t * tile)
{
	if (tile->extraData) {
		Hunk_Free(tile->extraData);
		tile->extraData = NULL;
	}
}


/**
 * @brief Adds in a single map tile
 * @sa CM_LoadMap
 * FIXME: here might be the map memory leak - every new map eats more and more memory
 */
static unsigned CM_AddMapTile(char *name, int sX, int sY, int sZ)
{
	char filename[MAX_QPATH];
	unsigned checksum;
	unsigned *buf;
	int i, length;
	dheader_t header;

	/* load the file */
	Com_sprintf(filename, MAX_QPATH, "maps/%s.bsp", name);
	length = FS_LoadFile(filename, (void **) &buf);
	if (!buf)
		Com_Error(ERR_DROP, "Couldn't load %s", filename);

	checksum = LittleLong(Com_BlockChecksum(buf, length));

	header = *(dheader_t *) buf;
	for (i = 0; i < sizeof(dheader_t) / 4; i++)
		((int *) &header)[i] = LittleLong(((int *) &header)[i]);

	if (header.version != BSPVERSION)
		Com_Error(ERR_DROP, "CM_AddMapTile: %s has wrong version number (%i should be %i)", name, header.version, BSPVERSION);

	cmod_base = (byte *) buf;

	/* init */
	if (numTiles >= MAX_MAPTILES)
		Com_Error(ERR_FATAL, "CM_AddMapTile: too many tiles loaded\n");

	curTile = &mapTiles[numTiles++];
	memset(curTile, 0, sizeof(mapTile_t));
	curTile->extraData = Hunk_Begin(0x400000);
	Q_strncpyz(curTile->name, name, MAX_QPATH);

	VectorSet(shift, sX * UNIT_SIZE, sY * UNIT_SIZE, sZ * UNIT_SIZE);

	/* load into heap */
	CMod_LoadSurfaces(&header.lumps[LUMP_TEXINFO]);
	CMod_LoadLeafs(&header.lumps[LUMP_LEAFS]);
	CMod_LoadLeafBrushes(&header.lumps[LUMP_LEAFBRUSHES]);
	CMod_LoadPlanes(&header.lumps[LUMP_PLANES]);
	CMod_LoadBrushes(&header.lumps[LUMP_BRUSHES]);
	CMod_LoadBrushSides(&header.lumps[LUMP_BRUSHSIDES]);
	CMod_LoadSubmodels(&header.lumps[LUMP_MODELS]);
	CMod_LoadNodes(&header.lumps[LUMP_NODES]);
	CMod_LoadEntityString(&header.lumps[LUMP_ENTITIES]);

	CM_InitBoxHull();
	CM_MakeTnodes();

	CMod_LoadRouting(&header.lumps[LUMP_ROUTING], sX, sY, sZ);
	memcpy(&svMap, &clMap, sizeof(routing_t));

	FS_FreeFile(buf);

	numInline += curTile->numcmodels - 258;

	curTile->extraDataSize = Hunk_End();

	return checksum;
}


/**
 * @brief Loads in the map and all submodels
 * @sa CM_AddMapTile
 */
void CM_LoadMap(char *tiles, char *pos)
{
	char *token;
	char name[MAX_VAR];
	char base[MAX_QPATH];
	int sh[3];
	int i;

	/* free old stuff */
	for (i = 0; i < numTiles; i++)
		CM_FreeTile(&mapTiles[i]);

	/* init */
	c_pointcontents = c_traces = c_brush_traces = numInline = numTiles = 0;
	map_entitystring[0] = base[0] = 0;

	memset(&(clMap.fall[0][0]), 0xFF, 256 * 256);
	memset(&(clMap.step[0][0]), 0, 256 * 256);
	memset(&(clMap.route[0][0][0]), 0, 256 * 256 * 8);

	/* load tiles */
	while (tiles) {
		/* get tile name */
		token = COM_Parse(&tiles);
		if (!tiles)
			return;

		/* get base path */
		if (token[0] == '-') {
			Q_strncpyz(base, token + 1, MAX_QPATH);
			continue;
		}

		/* get tile name */
		Com_DPrintf("CM_AddMapTile: token: %s\n", token);
		if (token[0] == '+')
			Com_sprintf(name, MAX_VAR, "%s%s", base, token + 1);
		else
			Q_strncpyz(name, token, MAX_VAR);

		if (pos && pos[0]) {
			/* get position and add a tile */
			for (i = 0; i < 2; i++) {
				token = COM_Parse(&pos);
				if (!pos)
					Com_Error(ERR_DROP, "CM_LoadMap: invalid positions\n");
				sh[i] = atoi(token);
			}
			CM_AddMapTile(name, sh[0], sh[1], 0);
		} else {
			/* load only a single tile, if no positions are specified */
			CM_AddMapTile(name, 0, 0, 0);
			return;
		}
	}

	Com_Error(ERR_DROP, "CM_LoadMap: invalid tile names\n");
}

/**
 * @brief
 * @param[in] name
 */
cmodel_t *CM_InlineModel(char *name)
{
	int i, num;

	if (!name || name[0] != '*')
		Com_Error(ERR_DROP, "CM_InlineModel: bad name");
	num = atoi(name + 1) - 1;
	if (num < 0 || num >= curTile->numcmodels)
		Com_Error(ERR_DROP, "CM_InlineModel: bad number");

	for (i = 0; i < numTiles; i++)
		if (num >= mapTiles[i].numcmodels - 258)
			num -= mapTiles[i].numcmodels - 258;
		else
			return &mapTiles[i].cmodels[258 + num];

	Com_Error(ERR_DROP, "CM_InlineModel: impossible error ;)");
	return NULL;
}

/**
 * @brief
 */
int CM_NumInlineModels(void)
{
	return numInline;
}

/**
 * @brief
 */
char *CM_EntityString(void)
{
	return map_entitystring;
}

/**
 * @brief
 * @param[in] leafnum
 */
int CM_LeafContents(int leafnum)
{
	if (leafnum < 0 || leafnum >= curTile->numleafs)
		Com_Error(ERR_DROP, "CM_LeafContents: bad number");
	return curTile->leafs[leafnum].contents;
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
void CM_InitBoxHull(void)
{
	int i;
	int side;
	cnode_t *c;
	cplane_t *p;
	cbrushside_t *s;

	curTile->box_headnode = curTile->numnodes;
	curTile->box_planes = &curTile->planes[curTile->numplanes];

	curTile->box_brush = &curTile->brushes[curTile->numbrushes];
	curTile->box_brush->numsides = 6;
	curTile->box_brush->firstbrushside = curTile->numbrushsides;
	curTile->box_brush->contents = CONTENTS_WEAPONCLIP;

	curTile->box_leaf = &curTile->leafs[curTile->numleafs];
	curTile->box_leaf->contents = CONTENTS_WEAPONCLIP;
	curTile->box_leaf->firstleafbrush = curTile->numleafbrushes;
	curTile->box_leaf->numleafbrushes = 1;

	curTile->leafbrushes[curTile->numleafbrushes] = curTile->numbrushes;

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
int CM_HeadnodeForBox(int tile, vec3_t mins, vec3_t maxs)
{
	curTile = &mapTiles[tile];

	curTile->box_planes[0].dist = maxs[0];
	curTile->box_planes[1].dist = -maxs[0];
	curTile->box_planes[2].dist = mins[0];
	curTile->box_planes[3].dist = -mins[0];
	curTile->box_planes[4].dist = maxs[1];
	curTile->box_planes[5].dist = -maxs[1];
	curTile->box_planes[6].dist = mins[1];
	curTile->box_planes[7].dist = -mins[1];
	curTile->box_planes[8].dist = maxs[2];
	curTile->box_planes[9].dist = -maxs[2];
	curTile->box_planes[10].dist = mins[2];
	curTile->box_planes[11].dist = -mins[2];

	return curTile->box_headnode;
}


/**
 * @brief
 * @param[in] p
 * @param[in] num
 */
int CM_PointLeafnum_r(vec3_t p, int num)
{
	float d;
	cnode_t *node;
	cplane_t *plane;

	while (num >= 0) {
		node = curTile->nodes + num;
		plane = node->plane;

		if (plane->type < 3)
			d = p[plane->type] - plane->dist;
		else
			d = DotProduct(plane->normal, p) - plane->dist;
		if (d < 0)
			num = node->children[1];
		else
			num = node->children[0];
	}

	c_pointcontents++;			/* optimize counter */

	return -1 - num;
}

/**
 * @brief
 * @param[in] p
 * @sa CM_PointLeafnum_r
 */
int CM_PointLeafnum(vec3_t p)
{
	if (!curTile->numplanes)
		return 0;				/* sound may call this without map loaded */
	return CM_PointLeafnum_r(p, 0);
}


/**
 * @brief Fills in a list of all the leafs touched
 * call with topnode set to the headnode, returns with topnode
 * set to the first node that splits the box
 */
void CM_BoxLeafnums_r(int nodenum)
{
	cplane_t *plane;
	cnode_t *node;
	int s;

	while (1) {
		if (nodenum < 0) {
			if (leaf_count >= leaf_maxcount) {
/*				Com_Printf ("CM_BoxLeafnums_r: overflow\n"); */
				return;
			}
			leaf_list[leaf_count++] = -1 - nodenum;
			return;
		}

		node = &curTile->nodes[nodenum];
		plane = node->plane;
/*		s = BoxOnPlaneSide (leaf_mins, leaf_maxs, plane); */
		s = BOX_ON_PLANE_SIDE(leaf_mins, leaf_maxs, plane);
		if (s == 1)
			nodenum = node->children[0];
		else if (s == 2)
			nodenum = node->children[1];
		else {					/* go down both */
			if (leaf_topnode == -1)
				leaf_topnode = nodenum;
			CM_BoxLeafnums_r(node->children[0]);
			nodenum = node->children[1];
		}
	}
}

/**
 * @brief
 * @param
 * @sa
 */
int CM_BoxLeafnums_headnode(vec3_t mins, vec3_t maxs, int *list, int listsize, int headnode, int *topnode)
{
	leaf_list = list;
	leaf_count = 0;
	leaf_maxcount = listsize;
	leaf_mins = mins;
	leaf_maxs = maxs;

	leaf_topnode = -1;

	CM_BoxLeafnums_r(headnode);

	if (topnode)
		*topnode = leaf_topnode;

	return leaf_count;
}

/**
 * @brief
 * @param
 * @sa
 */
int CM_BoxLeafnums(vec3_t mins, vec3_t maxs, int *list, int listsize, int *topnode)
{
	return CM_BoxLeafnums_headnode(mins, maxs, list, listsize, curTile->cmodels[0].headnode, topnode);
}

/**
 * @brief
 * @param
 * @sa
 */
int CM_PointContents(vec3_t p, int headnode)
{
	int l;

	if (!curTile || !curTile->numnodes)	/* map not loaded */
		return 0;

	l = CM_PointLeafnum_r(p, headnode);

	return curTile->leafs[l].contents;
}

/**
 * @brief Handles offseting and rotation of the end points for moving and rotating entities
 */
int CM_TransformedPointContents(vec3_t p, int headnode, vec3_t origin, vec3_t angles)
{
	vec3_t p_l;
	vec3_t temp;
	vec3_t forward, right, up;
	int l;

	/* subtract origin offset */
	VectorSubtract(p, origin, p_l);

	/* rotate start and end into the models frame of reference */
	if (headnode != curTile->box_headnode && (angles[0] || angles[1] || angles[2])) {
		AngleVectors(angles, forward, right, up);

		VectorCopy(p_l, temp);
		p_l[0] = DotProduct(temp, forward);
		p_l[1] = -DotProduct(temp, right);
		p_l[2] = DotProduct(temp, up);
	}

	l = CM_PointLeafnum_r(p_l, headnode);

	return curTile->leafs[l].contents;
}


/*======================================================================= */


/**
 * @brief
 * @param
 * @sa
 */
void CM_ClipBoxToBrush(vec3_t mins, vec3_t maxs, vec3_t p1, vec3_t p2, trace_t * trace, cbrush_t * brush)
{
	int i, j;
	cplane_t *plane, *clipplane;
	float dist;
	float enterfrac, leavefrac;
	vec3_t ofs;
	float d1, d2;
	qboolean getout, startout;
	float f;
	cbrushside_t *side, *leadside;

	enterfrac = -1;
	leavefrac = 1;
	clipplane = NULL;

	if (!brush->numsides)
		return;

	c_brush_traces++;

	getout = qfalse;
	startout = qfalse;
	leadside = NULL;

	for (i = 0; i < brush->numsides; i++) {
		side = &curTile->brushsides[brush->firstbrushside + i];
		plane = side->plane;

		/* FIXME: special case for axial */
		if (!trace_ispoint) {	/* general box case */
			/* push the plane out apropriately for mins/maxs */
			/* FIXME: use signbits into 8 way lookup for each mins/maxs */
			for (j = 0; j < 3; j++) {
				if (plane->normal[j] < 0)
					ofs[j] = maxs[j];
				else
					ofs[j] = mins[j];
			}
			dist = DotProduct(ofs, plane->normal);
			dist = plane->dist - dist;
		} else {				/* special point case */
			dist = plane->dist;
		}

		d1 = DotProduct(p1, plane->normal) - dist;
		d2 = DotProduct(p2, plane->normal) - dist;

		if (d2 > 0)
			getout = qtrue;		/* endpoint is not in solid */
		if (d1 > 0)
			startout = qtrue;

		/* if completely in front of face, no intersection */
		if (d1 > 0 && d2 >= d1)
			return;

		if (d1 <= 0 && d2 <= 0)
			continue;

		/* crosses face */
		if (d1 > d2) {			/* enter */
			f = (d1 - DIST_EPSILON) / (d1 - d2);
			if (f > enterfrac) {
				enterfrac = f;
				clipplane = plane;
				leadside = side;
			}
		} else {				/* leave */
			f = (d1 + DIST_EPSILON) / (d1 - d2);
			if (f < leavefrac)
				leavefrac = f;
		}
	}

	if (!startout) {			/* original point was inside brush */
		trace->startsolid = qtrue;
		if (!getout)
			trace->allsolid = qtrue;
		return;
	}
	if (enterfrac < leavefrac) {
		if (enterfrac > -1 && enterfrac < trace->fraction) {
			if (enterfrac < 0)
				enterfrac = 0;
			trace->fraction = enterfrac;
			trace->plane = *clipplane;
			trace->surface = &(leadside->surface->c);
			trace->contents = brush->contents;
		}
	}
}

/**
 * @brief
 * @param
 * @sa CM_TraceToLeaf
 */
void CM_TestBoxInBrush(vec3_t mins, vec3_t maxs, vec3_t p1, trace_t * trace, cbrush_t * brush)
{
	int i, j;
	cplane_t *plane;
	float dist;
	vec3_t ofs;
	float d1;
	cbrushside_t *side;

	if (!brush->numsides)
		return;

	for (i = 0; i < brush->numsides; i++) {
		side = &curTile->brushsides[brush->firstbrushside + i];
		plane = side->plane;

		/* FIXME: special case for axial */
		/* general box case */
		/* push the plane out apropriately for mins/maxs */
		/* FIXME: use signbits into 8 way lookup for each mins/maxs */
		for (j = 0; j < 3; j++) {
			if (plane->normal[j] < 0)
				ofs[j] = maxs[j];
			else
				ofs[j] = mins[j];
		}
		dist = DotProduct(ofs, plane->normal);
		dist = plane->dist - dist;

		d1 = DotProduct(p1, plane->normal) - dist;

		/* if completely in front of face, no intersection */
		if (d1 > 0)
			return;

	}

	/* inside this brush */
	trace->startsolid = trace->allsolid = qtrue;
	trace->fraction = 0;
	trace->contents = brush->contents;
}


/**
 * @brief
 * @param[in] leafnum
 * @sa CM_ClipBoxToBrush
 */
void CM_TraceToLeaf(int leafnum)
{
	int k;
	int brushnum;
	cleaf_t *leaf;
	cbrush_t *b;

	leaf = &curTile->leafs[leafnum];
	if (!(leaf->contents & trace_contents))
		return;
	/* trace line against all brushes in the leaf */
	for (k = 0; k < leaf->numleafbrushes; k++) {
		brushnum = curTile->leafbrushes[leaf->firstleafbrush + k];
		b = &curTile->brushes[brushnum];
		if (b->checkcount == checkcount)
			continue;			/* already checked this brush in another leaf */
		b->checkcount = checkcount;

		if (!(b->contents & trace_contents))
			continue;
		CM_ClipBoxToBrush(trace_mins, trace_maxs, trace_start, trace_end, &trace_trace, b);
		if (!trace_trace.fraction)
			return;
	}

}


/**
 * @brief
 * @param[in] leafnum
 * @sa CM_TestBoxInBrush
 */
void CM_TestInLeaf(int leafnum)
{
	int k;
	int brushnum;
	cleaf_t *leaf;
	cbrush_t *b;

	leaf = &curTile->leafs[leafnum];
	if (!(leaf->contents & trace_contents))
		return;
	/* trace line against all brushes in the leaf */
	for (k = 0; k < leaf->numleafbrushes; k++) {
		brushnum = curTile->leafbrushes[leaf->firstleafbrush + k];
		b = &curTile->brushes[brushnum];
		if (b->checkcount == checkcount)
			continue;			/* already checked this brush in another leaf */
		b->checkcount = checkcount;

		if (!(b->contents & trace_contents))
			continue;
		CM_TestBoxInBrush(trace_mins, trace_maxs, trace_start, &trace_trace, b);
		if (!trace_trace.fraction)
			return;
	}

}


/**
 * @brief
 * @param[in] num
 * @param[in] p1f
 * @param[in] p2f
 * @param[in] p1
 * @param[in] p2
 */
void CM_RecursiveHullCheck(int num, float p1f, float p2f, vec3_t p1, vec3_t p2)
{
	cnode_t *node;
	cplane_t *plane;
	float t1, t2, offset;
	float frac, frac2;
	float idist;
	int i;
	vec3_t mid;
	int side;
	float midf;

	if (trace_trace.fraction <= p1f)
		return;					/* already hit something nearer */

	/* if < 0, we are in a leaf node */
	if (num < 0) {
		CM_TraceToLeaf(-1 - num);
		return;
	}

	/* find the point distances to the seperating plane */
	/* and the offset for the size of the box */
	node = curTile->nodes + num;
	plane = node->plane;

	if (plane->type < 3) {
		t1 = p1[plane->type] - plane->dist;
		t2 = p2[plane->type] - plane->dist;
		offset = trace_extents[plane->type];
	} else {
		t1 = DotProduct(plane->normal, p1) - plane->dist;
		t2 = DotProduct(plane->normal, p2) - plane->dist;
		if (trace_ispoint)
			offset = 0;
		else
			offset = fabs(trace_extents[0] * plane->normal[0]) + fabs(trace_extents[1] * plane->normal[1]) + fabs(trace_extents[2] * plane->normal[2]);
	}


#if 0
	CM_RecursiveHullCheck(node->children[0], p1f, p2f, p1, p2);
	CM_RecursiveHullCheck(node->children[1], p1f, p2f, p1, p2);
	return;
#endif

	/* see which sides we need to consider */
	if (t1 >= offset && t2 >= offset) {
		CM_RecursiveHullCheck(node->children[0], p1f, p2f, p1, p2);
		return;
	}
	if (t1 < -offset && t2 < -offset) {
		CM_RecursiveHullCheck(node->children[1], p1f, p2f, p1, p2);
		return;
	}

	/* put the crosspoint DIST_EPSILON pixels on the near side */
	if (t1 < t2) {
		idist = 1.0 / (t1 - t2);
		side = 1;
		frac2 = (t1 + offset + DIST_EPSILON) * idist;
		frac = (t1 - offset + DIST_EPSILON) * idist;
	} else if (t1 > t2) {
		idist = 1.0 / (t1 - t2);
		side = 0;
		frac2 = (t1 - offset - DIST_EPSILON) * idist;
		frac = (t1 + offset + DIST_EPSILON) * idist;
	} else {
		side = 0;
		frac = 1;
		frac2 = 0;
	}

	/* move up to the node */
	if (frac < 0)
		frac = 0;
	if (frac > 1)
		frac = 1;

	midf = p1f + (p2f - p1f) * frac;
	for (i = 0; i < 3; i++)
		mid[i] = p1[i] + frac * (p2[i] - p1[i]);

	CM_RecursiveHullCheck(node->children[side], p1f, midf, p1, mid);


	/* go past the node */
	if (frac2 < 0)
		frac2 = 0;
	if (frac2 > 1)
		frac2 = 1;

	midf = p1f + (p2f - p1f) * frac2;
	for (i = 0; i < 3; i++)
		mid[i] = p1[i] + frac2 * (p2[i] - p1[i]);

	CM_RecursiveHullCheck(node->children[side ^ 1], midf, p2f, mid, p2);
}



/*====================================================================== */

/**
 * @brief
 * @param[in] start
 * @param[in] end
 * @param[in] mins
 * @param[in] max
 * @param[in] tile
 * @param[in] headnode
 * @param[in] brushmask
 * @sa CM_TransformedBoxTrace
 * @sa CM_CompleteBoxTrace
 */
trace_t CM_BoxTrace(vec3_t start, vec3_t end, vec3_t mins, vec3_t maxs, int tile, int headnode, int brushmask)
{
	int i;

	checkcount++;				/* for multi-check avoidance */
	c_traces++;					/* for statistics, may be zeroed */

	/* init */
	curTile = &mapTiles[tile];

	/* fill in a default trace */
	memset(&trace_trace, 0, sizeof(trace_trace));
	trace_trace.fraction = 1;
	trace_trace.surface = &(nullsurface.c);

	if (!curTile->numnodes)		/* map not loaded */
		return trace_trace;

	trace_contents = brushmask;
	VectorCopy(start, trace_start);
	VectorCopy(end, trace_end);
	VectorCopy(mins, trace_mins);
	VectorCopy(maxs, trace_maxs);

	/* check for position test special case */
	if (start[0] == end[0] && start[1] == end[1] && start[2] == end[2]) {
		int leafs[1024];
		int i, numleafs;
		vec3_t c1, c2;
		int topnode;

		VectorAdd(start, mins, c1);
		VectorAdd(start, maxs, c2);
		for (i = 0; i < 3; i++) {
			c1[i] -= 1;
			c2[i] += 1;
		}

		numleafs = CM_BoxLeafnums_headnode(c1, c2, leafs, 1024, headnode, &topnode);
		for (i = 0; i < numleafs; i++) {
			CM_TestInLeaf(leafs[i]);
			if (trace_trace.allsolid)
				break;
		}
		VectorCopy(start, trace_trace.endpos);
		return trace_trace;
	}

	/* check for point special case */
	if (mins[0] == 0 && mins[1] == 0 && mins[2] == 0 && maxs[0] == 0 && maxs[1] == 0 && maxs[2] == 0) {
		trace_ispoint = qtrue;
		VectorClear(trace_extents);
	} else {
		trace_ispoint = qfalse;
		trace_extents[0] = -mins[0] > maxs[0] ? -mins[0] : maxs[0];
		trace_extents[1] = -mins[1] > maxs[1] ? -mins[1] : maxs[1];
		trace_extents[2] = -mins[2] > maxs[2] ? -mins[2] : maxs[2];
	}

	/* general sweeping through world */
	CM_RecursiveHullCheck(headnode, 0, 1, start, end);

	if (trace_trace.fraction == 1) {
		VectorCopy(end, trace_trace.endpos);
	} else {
		for (i = 0; i < 3; i++)
			trace_trace.endpos[i] = start[i] + trace_trace.fraction * (end[i] - start[i]);
	}
	return trace_trace;
}

#ifdef _MSC_VER
#pragma optimize( "", off )
#endif

/**
 * @brief Handles offseting and rotation of the end points for moving and rotating entities
 * @sa CM_BoxTrace
 */
trace_t CM_TransformedBoxTrace(vec3_t start, vec3_t end, vec3_t mins, vec3_t maxs, int tile, int headnode, int brushmask, vec3_t origin, vec3_t angles)
{
	trace_t trace;
	vec3_t start_l, end_l;
	vec3_t a;
	vec3_t forward, right, up;
	vec3_t temp;
	qboolean rotated;

	if (tile >= MAX_MAPTILES) {
		Com_Printf("CM_TransformedBoxTrace: too many tiles loaded\n");
		tile = 0;
	}

	/* init */
	curTile = &mapTiles[tile];

	/* subtract origin offset */
	VectorSubtract(start, origin, start_l);
	VectorSubtract(end, origin, end_l);

	/* rotate start and end into the models frame of reference */
	if (headnode != curTile->box_headnode && (angles[0] || angles[1] || angles[2]))
		rotated = qtrue;
	else
		rotated = qfalse;

	if (rotated) {
		AngleVectors(angles, forward, right, up);

		VectorCopy(start_l, temp);
		start_l[0] = DotProduct(temp, forward);
		start_l[1] = -DotProduct(temp, right);
		start_l[2] = DotProduct(temp, up);

		VectorCopy(end_l, temp);
		end_l[0] = DotProduct(temp, forward);
		end_l[1] = -DotProduct(temp, right);
		end_l[2] = DotProduct(temp, up);
	}

	/* sweep the box through the model */
	trace = CM_BoxTrace(start_l, end_l, mins, maxs, tile, headnode, brushmask);

	if (rotated && trace.fraction != 1.0) {
		/* FIXME: figure out how to do this with existing angles */
		VectorNegate(angles, a);
		AngleVectors(a, forward, right, up);

		VectorCopy(trace.plane.normal, temp);
		trace.plane.normal[0] = DotProduct(temp, forward);
		trace.plane.normal[1] = -DotProduct(temp, right);
		trace.plane.normal[2] = DotProduct(temp, up);
	}

	trace.endpos[0] = start[0] + trace.fraction * (end[0] - start[0]);
	trace.endpos[1] = start[1] + trace.fraction * (end[1] - start[1]);
	trace.endpos[2] = start[2] + trace.fraction * (end[2] - start[2]);

	return trace;
}

#ifdef _MSC_VER
#pragma optimize( "", on )
#endif



/**
 * @brief Handles all 255 level specific submodels too
 */
trace_t CM_CompleteBoxTrace(vec3_t start, vec3_t end, vec3_t mins, vec3_t maxs, int levelmask, int brushmask)
{
	trace_t newtr, tr;
	int tile, i;
	chead_t *h;

	tr.fraction = 2.0f;

	for (tile = 0; tile < numTiles; tile++) {
		curTile = &mapTiles[tile];
		for (i = 0, h = curTile->cheads; i < curTile->numcheads; i++, h++) {
			if (h->level && levelmask && !(h->level & levelmask))
				continue;

			newtr = CM_BoxTrace(start, end, mins, maxs, tile, h->cnode, brushmask);

			/* memorize the trace with the minimal fraction */
			if (newtr.fraction == 0.0)
				return newtr;
			if (newtr.fraction < tr.fraction)
				tr = newtr;
		}
	}
	return tr;
}



/* ===================================================================== */

/**
 * @brief Converts the disk node structure into the efficient tracing structure
 */
void MakeTnode(int nodenum)
{
	tnode_t *t;
	cplane_t *plane;
	int i;
	cnode_t *node;

	t = tnode_p++;

	node = curTile->nodes + nodenum;
	plane = node->plane;

	t->type = plane->type;
	VectorCopy(plane->normal, t->normal);
	t->dist = plane->dist;

	for (i = 0; i < 2; i++) {
		if (node->children[i] < 0) {
			if (curTile->leafs[-(node->children[i]) - 1].contents & CONTENTS_SOLID)
				t->children[i] = 1 | (1 << 31);
			else
				t->children[i] = (1 << 31);
		} else {
			t->children[i] = tnode_p - curTile->tnodes;
			MakeTnode(node->children[i]);
		}
	}
}


/**
 * @brief
 * @param
 * @sa
 */
void BuildTnode_r(int node)
{
	if (!curTile->nodes[node].plane) {
		cnode_t *n;
		tnode_t *t;
		vec3_t c0maxs, c1mins;
		int i;

		n = &curTile->nodes[node];

		/* alloc new node */
		t = tnode_p++;

		if (n->children[0] < 0 || n->children[1] < 0)
			Com_Error(ERR_DROP, "Unexpected leaf");

		VectorCopy(curTile->nodes[n->children[0]].maxs, c0maxs);
		VectorCopy(curTile->nodes[n->children[1]].mins, c1mins);

		/*  printf( "(%i %i : %i %i) (%i %i : %i %i)\n", */
		/*      (int)dnodes[n->children[0]].mins[0], (int)dnodes[n->children[0]].mins[1], (int)dnodes[n->children[0]].maxs[0], (int)dnodes[n->children[0]].maxs[1], */
		/*      (int)dnodes[n->children[1]].mins[0], (int)dnodes[n->children[1]].mins[1], (int)dnodes[n->children[1]].maxs[0], (int)dnodes[n->children[1]].maxs[1] ); */

		for (i = 0; i < 2; i++)
			if (c0maxs[i] <= c1mins[i]) {
				/* create a separation plane */
				t->type = i;
				t->normal[0] = i;
				t->normal[1] = i ^ 1;
				t->normal[2] = 0;
				t->dist = (c0maxs[i] + c1mins[i]) / 2;

				t->children[1] = tnode_p - curTile->tnodes;
				BuildTnode_r(n->children[0]);
				t->children[0] = tnode_p - curTile->tnodes;
				BuildTnode_r(n->children[1]);
				return;
			}

		/* can't construct such a separation plane */
		t->type = PLANE_NONE;

		for (i = 0; i < 2; i++) {
			t->children[i] = tnode_p - curTile->tnodes;
			BuildTnode_r(n->children[i]);
		}
	} else {
		/* don't include weapon clip in standard tracing */
		if (cur_level <= 256) {
			curTile->cheads[curTile->numcheads].cnode = node;
			curTile->cheads[curTile->numcheads].level = cur_level;
			curTile->numcheads++;
		}

		MakeTnode(node);
	}
}


/**
 * @brief Loads the node structure out of a .bsp file to be used for light occlusion
 * @sa BuildTnode_r
 */
void CM_MakeTnodes(void)
{
	int i;

	/* 32 byte align the structs */
	curTile->tnodes = Hunk_Alloc((curTile->numnodes + 1) * sizeof(tnode_t));
	curTile->tnodes = (tnode_t *) (((size_t) curTile->tnodes + 31) & ~31);
	tnode_p = curTile->tnodes;

	curTile->numtheads = 0;
	curTile->numcheads = 0;

	for (i = 0; i < 258; i++) {
		if (curTile->cmodels[i].headnode == -1)
			continue;

		curTile->thead[curTile->numtheads] = tnode_p - curTile->tnodes;
		curTile->numtheads++;

		cur_level = i;

		BuildTnode_r(curTile->cmodels[i].headnode);
/*		MakeTnode (map_models[i].headnode); */
	}
}


/*========================================================== */

/**
 * @brief
 * @param[in] node Node index
 * @param[in] start
 * @param[in] stop
 * @sa TestLineDist_r
 * @sa CM_TestLine
 */
int TestLine_r(int node, vec3_t start, vec3_t stop)
{
	tnode_t *tnode;
	float front, back;
	vec3_t mid;
	float frac;
	int side;
	int r;

	/* leaf node */
	if (node & (1 << 31))
		return node & ~(1 << 31);

	tnode = &curTile->tnodes[node];
	assert(tnode);
	switch (tnode->type) {
	case PLANE_X:
		front = start[0] - tnode->dist;
		back = stop[0] - tnode->dist;
		break;
	case PLANE_Y:
		front = start[1] - tnode->dist;
		back = stop[1] - tnode->dist;
		break;
	case PLANE_Z:
		front = start[2] - tnode->dist;
		back = stop[2] - tnode->dist;
		break;
	case PLANE_NONE:
		r = TestLine_r(tnode->children[0], start, stop);
		if (r)
			return r;
		return TestLine_r(tnode->children[1], start, stop);
		break;
	default:
		front = (start[0] * tnode->normal[0] + start[1] * tnode->normal[1] + start[2] * tnode->normal[2]) - tnode->dist;
		back = (stop[0] * tnode->normal[0] + stop[1] * tnode->normal[1] + stop[2] * tnode->normal[2]) - tnode->dist;
		break;
	}

	if (front >= -ON_EPSILON && back >= -ON_EPSILON)
		return TestLine_r(tnode->children[0], start, stop);

	if (front < ON_EPSILON && back < ON_EPSILON)
		return TestLine_r(tnode->children[1], start, stop);

	side = front < 0;

	frac = front / (front - back);

	mid[0] = start[0] + (stop[0] - start[0]) * frac;
	mid[1] = start[1] + (stop[1] - start[1]) * frac;
	mid[2] = start[2] + (stop[2] - start[2]) * frac;

	r = TestLine_r(tnode->children[side], start, mid);
	if (r)
		return r;
	return TestLine_r(tnode->children[!side], mid, stop);
}

/**
 * @brief
 * @param[in] node Node index
 * @param[in] start
 * @param[in] stop
 * @sa TestLine_r
 * @sa CM_TestLineDM
 */
int TestLineDist_r(int node, vec3_t start, vec3_t stop)
{
	tnode_t *tnode;
	float front, back;
	vec3_t mid;
	float frac;
	int side;
	int r;

	if (node & (1 << 31)) {
		r = node & ~(1 << 31);
		if (r)
			VectorCopy(start, tr_end);
		return r;				/* leaf node */
	}

	tnode = &curTile->tnodes[node];
	assert(tnode);
	switch (tnode->type) {
	case PLANE_X:
		front = start[0] - tnode->dist;
		back = stop[0] - tnode->dist;
		break;
	case PLANE_Y:
		front = start[1] - tnode->dist;
		back = stop[1] - tnode->dist;
		break;
	case PLANE_Z:
		front = start[2] - tnode->dist;
		back = stop[2] - tnode->dist;
		break;
	case PLANE_NONE:
		r = TestLineDist_r(tnode->children[0], start, stop);
		if (r)
			VectorCopy(tr_end, mid);
		side = TestLineDist_r(tnode->children[1], start, stop);
		if (side && r) {
			if (VectorNearer(mid, tr_end, start)) {
				VectorCopy(mid, tr_end);
				return r;
			} else {
				return side;
			}
		}

		if (r) {
			VectorCopy(mid, tr_end);
			return r;
		}

		return side;

		break;
	default:
		front = (start[0] * tnode->normal[0] + start[1] * tnode->normal[1] + start[2] * tnode->normal[2]) - tnode->dist;
		back = (stop[0] * tnode->normal[0] + stop[1] * tnode->normal[1] + stop[2] * tnode->normal[2]) - tnode->dist;
		break;
	}

	if (front >= -ON_EPSILON && back >= -ON_EPSILON)
		return TestLineDist_r(tnode->children[0], start, stop);

	if (front < ON_EPSILON && back < ON_EPSILON)
		return TestLineDist_r(tnode->children[1], start, stop);

	side = front < 0;

	frac = front / (front - back);

	mid[0] = start[0] + (stop[0] - start[0]) * frac;
	mid[1] = start[1] + (stop[1] - start[1]) * frac;
	mid[2] = start[2] + (stop[2] - start[2]) * frac;

	r = TestLineDist_r(tnode->children[side], start, mid);
	if (r)
		return r;
	return TestLineDist_r(tnode->children[!side], mid, stop);
}

/**
 * @brief
 * @param[in] start
 * @param[in] stop
 * @sa TestLine_r
 */
int CM_TestLine(vec3_t start, vec3_t stop)
{
	int tile, i;

	for (tile = 0; tile < numTiles; tile++) {
		curTile = &mapTiles[tile];
		for (i = 0; i < curTile->numtheads; i++) {
			if (TestLine_r(curTile->thead[i], start, stop))
				return 1;
		}
	}
	return 0;
}

/**
 * @brief
 * @param[in] start
 * @param[in] stop
 * @param[in] end
 * @sa CM_TestLine
 */
int CM_TestLineDM(vec3_t start, vec3_t stop, vec3_t end)
{
	int tile, i;

	VectorCopy(stop, end);

	for (tile = 0; tile < numTiles; tile++) {
		curTile = &mapTiles[tile];
		for (i = 0; i < curTile->numtheads; i++) {
			if (TestLineDist_r(curTile->thead[i], start, end))
				if (VectorNearer(tr_end, end, start))
					VectorCopy(tr_end, end);
		}
	}

	if (VectorCompare(end, stop))
		return 0;
	else
		return 1;
}

/*
==========================================================
  GRID ORIENTED MOVEMENT AND SCANNING
==========================================================
*/

/**
 * @brief
 * @param
 * @sa Grid_MoveMark
 */
qboolean Grid_CheckForbidden(struct routing_s * map, int x, int y, int z)
{
	byte **p;
	int i;

	for (i = 0, p = map->fblist; i < map->fblength; i++, p++)
		if (x == (*p)[0] && y == (*p)[1] && z == (*p)[2])
			return qtrue;
	return qfalse;
}


/**
 * @brief
 * @param
 * @sa Grid_CheckForbidden
 */
void Grid_MoveMark(struct routing_s *map, int x, int y, int z, int dv, int h, int ol)
{
	int nx, ny, sh, l;
	int dx, dy;

	/* range check */
	l = dv > 3 ? ol + 3 : ol + 2;
	dx = dvecs[dv][0];
	dy = dvecs[dv][1];
	nx = x + dx;
	ny = y + dy;
	if (nx < 0 || nx >= WIDTH || ny < 0 || ny >= WIDTH)
		return;

	/* connection checks */
	if (dx > 0 && !(map->route[z][y][x] & 0x10))
		return;
	if (dx < 0 && !(map->route[z][y][x] & 0x20))
		return;
	if (dy > 0 && !(map->route[z][y][x] & 0x40))
		return;
	if (dy < 0 && !(map->route[z][y][x] & 0x80))
		return;
	if (dv > 3 && !((map->route[z][y + dy][x] & (dx > 0 ? 0x10 : 0x20)) && (map->route[z][y][x + dx] & (dy > 0 ? 0x40 : 0x80))
	  && !Grid_CheckForbidden(map, x, y + dy, z) && !Grid_CheckForbidden(map, x + dx, y, z)))
		return;

	/* height checks */
	sh = (map->step[y][x] & (1 << z)) ? sh_big : sh_low;
	z += (h + sh) / 0x10;
	while (map->fall[ny][nx] & (1 << z))
		z--;

	/* can it be better than ever? */
	if (map->area[z][ny][nx] < l)
		return;

	/* test for forbidden areas */
	if (Grid_CheckForbidden(map, nx, ny, z))
		return;

	/* store move */
	map->area[z][ny][nx] = l;
	tfList[z][ny][nx] = stf;
}


/**
 * @brief
 * @param
 * @sa Grid_MoveMark
 */
static void Grid_MoveMarkRoute(struct routing_s *map, int xl, int yl, int xh, int yh)
{
	int x, y, z, h;
	int dv, l;

	for (z = 0; z < HEIGHT; z++)
		for (y = yl; y <= yh; y++)
			for (x = xl; x <= xh; x++)
				if (tfList[z][y][x] == tf) {
					/* reset test flags */
					tfList[z][y][x] = 0;
					l = map->area[z][y][x];
					h = map->route[z][y][x] & 0xF;

					/* check for end */
					if (l + 3 >= MAX_MOVELENGTH)
						continue;

					/* test the next connections */
					for (dv = 0; dv < 8; dv++)
						Grid_MoveMark(map, x, y, z, dv, h, l);
				}
}

/**
 * @brief
 * @param[in] map
 * @param[in] from
 * @param[in] distance
 * @param[in] fb_list Forbidden list
 * @param[in] fb_length Length of forbidden list
 * @sa Grid_MoveMarkRoute
 * @sa G_MoveCalc
 */
void Grid_MoveCalc(struct routing_s *map, pos3_t from, int distance, byte ** fb_list, int fb_length)
{
	int xl, xh, yl, yh;
	int i;

	/* reset move data */
	memset(map->area, 0xFF, WIDTH * WIDTH * HEIGHT);
	memset(tfList, 0, WIDTH * WIDTH * HEIGHT);
	map->fblist = fb_list;
	map->fblength = fb_length;

	xl = xh = from[0];
	yl = yh = from[1];

	if (distance > MAX_ROUTE)
		distance = MAX_ROUTE;

	/* first step */
	tf = 1;
	stf = 2;
	map->area[from[2]][yl][xl] = 0;
	tfList[from[2]][yl][xl] = 1;

	for (i = 0; i < distance; i++) {
		/* go on checking */
		if (xl > 0)
			xl--;
		if (yl > 0)
			yl--;
		if (xh < WIDTH - 1)
			xh++;
		if (yh < WIDTH - 1)
			yh++;

		Grid_MoveMarkRoute(map, xl, yl, xh, yh);

		/* swap test flag */
		stf = tf;
		tf ^= 3;
	}
}


/**
 * @brief Caches the calculated move
 * @param
 * @sa AI_ActorThink
 */
void Grid_MoveStore(struct routing_s *map)
{
	memcpy(map->areaStored, map->area, WIDTH * WIDTH * HEIGHT);
}


/**
 * @brief
 * @param[in] map
 * @param[in] to
 * @param[in] stored
 * @return 0xFF if the move isn't possible
 * @return length of move otherwise
 */
int Grid_MoveLength(struct routing_s *map, pos3_t to, qboolean stored)
{
	if (!stored)
		return map->area[to[2]][to[1]][to[0]];
	else
		return map->areaStored[to[2]][to[1]][to[0]];
}


/**
 * @brief
 * @param[in] map
 * @param[in] pos
 * @param[in] sz
 * @param[in] l
 * @sa Grid_MoveNext
 */
int Grid_MoveCheck(struct routing_s *map, pos3_t pos, int sz, int l)
{
	int x, y, sh;
	int dv, dx, dy;
	int z;

	for (dv = 0; dv < 8; dv++) {
		dx = -dvecs[dv][0];
		dy = -dvecs[dv][1];

		/* range check */
		if (dx > 0 && pos[0] >= WIDTH - 1)
			continue;
		if (dy > 0 && pos[1] >= WIDTH - 1)
			continue;
		if (dx < 0 && pos[0] <= 0)
			continue;
		if (dy < 0 && pos[1] <= 0)
			continue;

		/* distance table check */
		x = pos[0] - dx;
		y = pos[1] - dy;
		z = sz;

		if (map->area[z][y][x] != (l - 2) - (dv > 3))
			continue;

		/* connection checks */
		if (dx > 0 && !(map->route[z][y][x] & 0x10))
			continue;
		if (dx < 0 && !(map->route[z][y][x] & 0x20))
			continue;
		if (dy > 0 && !(map->route[z][y][x] & 0x40))
			continue;
		if (dy < 0 && !(map->route[z][y][x] & 0x80))
			continue;
		if (dv > 3 && !((map->route[z][y + dy][x] & (dx > 0 ? 0x10 : 0x20)) && (map->route[z][y][x + dx] & (dy > 0 ? 0x40 : 0x80))))
			continue;

		/* height checks */
		sh = (map->step[y][x] & (1 << z)) ? sh_big : sh_low;
		z += ((map->route[z][y][x] & 0xF) + sh) / 0x10;
		while (map->fall[pos[1]][pos[0]] & (1 << z))
			z--;

		/*  Com_Printf( "pos: (%i %i %i) (x,y,z): (%i %i %i)\n", pos[0], pos[1], pos[2], x, y, z ); */

		if (pos[2] == z)
			break;				/* found it! */

		/* not found... try next dv */
	}

	return dv;
}


/**
 * @brief
 * @param[in] map
 * @param[in] from
 * @sa Grid_MoveCheck
 */
int Grid_MoveNext(struct routing_s *map, pos3_t from)
{
	int l, x, y, z, dv;

	l = map->area[from[2]][from[1]][from[0]];

	/* finished */
	if (!l)
		return 0xFF;

	/* initialize tests */
	x = from[0];
	y = from[1];

	/* do tests */
	for (z = 0; z < HEIGHT; z++) {
		/* suppose it's possible at that height */
		dv = Grid_MoveCheck(map, from, z, l);
		if (dv < 8)
			return dv | (z << 3);
	}

	/* shouldn't happen */
	Com_Printf("failed...\n");
	return 0xFF;
}


/**
 * @brief
 * @param
 * @sa
 */
int Grid_Height(struct routing_s *map, pos3_t pos)
{
	return (map->route[pos[2]][pos[1]][pos[0]] & 0x0F) * QUANT;
}


/**
 * @brief
 * @param
 * @sa
 */
int Grid_Fall(struct routing_s *map, pos3_t pos)
{
	int z = pos[2];

	while (z > 0 && map->fall[pos[1]][pos[0]] & (1 << z))
		z--;
	return z;
}

/**
 * @brief
 * @sa Grid_Height
 */
void Grid_PosToVec(struct routing_s *map, pos3_t pos, vec3_t vec)
{
	PosToVec(pos, vec);
	vec[2] += Grid_Height(map, pos);
}

/**
 * @brief
 * @sa CM_InlineModel
 * @sa CM_CheckUnit
 * @sa CM_TestConnection
 */
void Grid_RecalcRouting(struct routing_s *map, char *name, char **list)
{
	cmodel_t *model;
	pos3_t min, max;
	int x, y, z;
	int i;

	/* get inline model, if it is one */
	if (*name != '*')
		return;
	model = CM_InlineModel(name);
	if (!model)
		return;
	inlineList = list;

	/* get dimensions */
	VecToPos(model->mins, min);
	VecToPos(model->maxs, max);

	memset(filled, 0, WIDTH * WIDTH);

	max[0] = max[0] < 253 ? max[0] + 2 : 255;
	max[1] = max[1] < 253 ? max[1] + 2 : 255;
	max[2] = max[2] < 5 ? max[2] + 2 : 7;
	for (i = 0; i < 3; i++)
		min[i] = min[i] > 2 ? min[i] - 2 : 0;

/*	Com_Printf( "routing: (%i %i %i) (%i %i %i)\n", */
/*		(int)min[0], (int)min[1], (int)min[2], */
/*		(int)max[0], (int)max[1], (int)max[2] ); */

	/* check unit heights */
	for (z = min[2]; z < max[2]; z++)
		for (y = min[1]; y < max[1]; y++)
			for (x = min[0]; x < max[0]; x++)
				CM_CheckUnit(map, x, y, z);

	/* check connections */
	for (z = min[2]; z < max[2]; z++)
		for (y = min[1]; y < max[1]; y++)
			for (x = min[0]; x < max[0]; x++)
				for (i = 0; i < 4; i++)
					CM_TestConnection(map, x, y, z, i, qtrue);

	inlineList = NULL;
}


/*
==============================================================================
TARGETING FUNCTIONS
==============================================================================
*/


/**
 * Grenade Aiming Maths
 * --------------------------------------------------------------
 *
 * There are two possibilities when aiming: either we can reach the target at
 * maximum speed or we can't. If we can reach it would would like to reach it with
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
 * to take the derivative of f (which I suggest doing an algebra system!).
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

/**
 * @brief
 */
float Com_GrenadeTarget(vec3_t from, vec3_t at, float speed, vec3_t v0)
{
	vec3_t delta;
	float d, h, g, v, alpha, vx, vy;
	float k, gd2, len;

	/* calculate target distance and height */
	h = at[2] - from[2];
	VectorSubtract(at, from, delta);
	delta[2] = 0;
	d = VectorLength(delta);

	/* check that it's not degenerate */
	if (d == 0){
		return 0;
	}

	/* firstly try to get as flat a trajectory as possible (fire at max velocity) */
	g = GRAVITY; v = speed;
	gd2 = g*d*d;
	len = sqrt(h*h + d*d);
	k = (v*v*h + gd2) / (v*v*len);

	/* check whether the shot's possible */
	if (k >= -1 && k <= 1){
		/* it is possible, so calculate the angle */
		alpha = 0.5 * (atan2(d, -h) - acos(k));
	}else{
		/* it's not possible, so calculate the minimum possible velocity that would make it possible */
		alpha = 0.5 * atan2(d, -h);
		v = sqrt(gd2 / (len - h));
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
