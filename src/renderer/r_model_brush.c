/**
 * @file r_model_brush.c
 * @brief brush model loading
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

#include "r_local.h"

/*
===============================================================================
BRUSHMODEL LOADING
===============================================================================
*/

static byte *mod_base;
static int shift[3];
typedef model_t *model_p;
model_t *loadmodel;

/**
 * @brief FIXME
 */
static void R_ModLoadLighting (lump_t * l)
{
	if (!l->filelen) {
		loadmodel->bsp.lightdata = NULL;
		return;
	}
	loadmodel->bsp.lightdata = VID_TagAlloc(vid_lightPool, l->filelen, 0);
	loadmodel->bsp.lightquant = *(byte *) (mod_base + l->fileofs);
	memcpy(loadmodel->bsp.lightdata, mod_base + l->fileofs, l->filelen);
}

static void R_ModLoadVertexes (lump_t * l)
{
	dvertex_t *in;
	mBspVertex_t *out;
	int i, count;

	in = (void *) (mod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
		Sys_Error("R_ModLoadVertexes: funny lump size in %s", loadmodel->name);
	count = l->filelen / sizeof(*in);
	out = VID_TagAlloc(vid_modelPool, count * sizeof(*out), 0);
	Com_DPrintf(DEBUG_RENDERER, "...verts: %i\n", count);

	loadmodel->bsp.vertexes = out;
	loadmodel->bsp.numvertexes = count;

	for (i = 0; i < count; i++, in++, out++) {
		out->position[0] = LittleFloat(in->point[0]);
		out->position[1] = LittleFloat(in->point[1]);
		out->position[2] = LittleFloat(in->point[2]);
	}
}

static float RadiusFromBounds (vec3_t mins, vec3_t maxs)
{
	int i;
	vec3_t corner;

	for (i = 0; i < 3; i++)
		corner[i] = fabs(mins[i]) > fabs(maxs[i]) ? fabs(mins[i]) : fabs(maxs[i]);

	return VectorLength(corner);
}


/**
 * @sa CR_ModLoadSubmodels
 */
static void R_ModLoadSubmodels (lump_t * l)
{
	dmodel_t *in;
	mBspHeader_t *out;
	int i, j, count;

	in = (void *) (mod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
		Sys_Error("R_ModLoadSubmodels: funny lump size in %s", loadmodel->name);
	count = l->filelen / sizeof(*in);
	out = VID_TagAlloc(vid_modelPool, count * sizeof(*out), 0);
	Com_DPrintf(DEBUG_RENDERER, "...submodels: %i\n", count);

	loadmodel->bsp.submodels = out;
	loadmodel->bsp.numsubmodels = count;

	for (i = 0; i < count; i++, in++, out++) {
		/* spread the mins / maxs by a pixel */
		for (j = 0; j < 3; j++) {
			out->mins[j] = LittleFloat(in->mins[j]) - 1 + shift[j];
			out->maxs[j] = LittleFloat(in->maxs[j]) + 1 + shift[j];
			out->origin[j] = LittleFloat(in->origin[j]) + shift[j];
		}
		out->radius = RadiusFromBounds(out->mins, out->maxs);
		out->headnode = LittleLong(in->headnode);
		out->firstface = LittleLong(in->firstface);
		out->numfaces = LittleLong(in->numfaces);
	}
}

static void R_ModLoadEdges (lump_t * l)
{
	dedge_t *in;
	mBspEdge_t *out;
	int i, count;

	in = (void *) (mod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
		Sys_Error("R_ModLoadEdges: funny lump size in %s", loadmodel->name);
	count = l->filelen / sizeof(*in);
	out = VID_TagAlloc(vid_modelPool, (count + 1) * sizeof(*out), 0);
	Com_DPrintf(DEBUG_RENDERER, "...edges: %i\n", count);

	loadmodel->bsp.edges = out;
	loadmodel->bsp.numedges = count;

	for (i = 0; i < count; i++, in++, out++) {
		out->v[0] = (unsigned short) LittleShort(in->v[0]);
		out->v[1] = (unsigned short) LittleShort(in->v[1]);
	}
}

/**
 * @sa CP_StartMissionMap
 */
static void R_ModLoadTexinfo (lump_t * l)
{
	texinfo_t *in;
	mBspTexInfo_t *out, *step;
	int i, j, count;
	char name[MAX_QPATH];
	int next;

	in = (void *) (mod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
		Sys_Error("R_ModLoadTexinfo: funny lump size in %s", loadmodel->name);
	count = l->filelen / sizeof(*in);
	out = VID_TagAlloc(vid_modelPool, count * sizeof(*out), 0);
	Com_DPrintf(DEBUG_RENDERER, "...texinfo: %i\n", count);

	loadmodel->bsp.texinfo = out;
	loadmodel->bsp.numtexinfo = count;

	for (i = 0; i < count; i++, in++, out++) {
		for (j = 0; j < 4; j++) {
			out->vecs[0][j] = LittleFloat(in->vecs[0][j]);
			out->vecs[1][j] = LittleFloat(in->vecs[1][j]);
		}

		out->flags = LittleLong(in->surfaceFlags);
		next = LittleLong(in->nexttexinfo);
		if (next > 0)
			out->next = loadmodel->bsp.texinfo + next;
		else
			out->next = NULL;
		/* exchange the textures with the ones that are needed for base assembly */
		if (refdef.mapZone && strstr(in->texture, "tex_terrain/dummy"))
			Com_sprintf(name, sizeof(name), "textures/tex_terrain/%s", refdef.mapZone);
		else
			Com_sprintf(name, sizeof(name), "textures/%s", in->texture);

		out->image = R_FindImage(name, it_wall);
		if (!out->image) {
			Com_Printf("Couldn't load %s\n", name);
			out->image = r_notexture;
		}
	}

	/* count animation frames */
	for (i = 0; i < count; i++) {
		out = &loadmodel->bsp.texinfo[i];
		out->numframes = 1;
		for (step = out->next; step && step != out; step = step->next)
			out->numframes++;
	}
}

/**
 * @brief Fills in s->texturemins[] and s->extents[]
 */
static void CalcSurfaceExtents (mBspSurface_t * s)
{
	float mins[2], maxs[2], val;

/* 	vec3_t	pos; */
	int i, j, e;
	mBspVertex_t *v;
	mBspTexInfo_t *tex;
	int bmins[2], bmaxs[2];

	mins[0] = mins[1] = 999999;
	maxs[0] = maxs[1] = -99999;

	tex = s->texinfo;

	for (i = 0; i < s->numedges; i++) {
		e = loadmodel->bsp.surfedges[s->firstedge + i];
		if (e >= 0)
			v = &loadmodel->bsp.vertexes[loadmodel->bsp.edges[e].v[0]];
		else
			v = &loadmodel->bsp.vertexes[loadmodel->bsp.edges[-e].v[1]];

		for (j = 0; j < 2; j++) {
			val = v->position[0] * tex->vecs[j][0] + v->position[1] * tex->vecs[j][1] + v->position[2] * tex->vecs[j][2] + tex->vecs[j][3];
			if (val < mins[j])
				mins[j] = val;
			if (val > maxs[j])
				maxs[j] = val;
		}
	}

	for (i = 0; i < 2; i++) {
		/* tiny rounding hack, not sure if it works */
		bmins[i] = floor(mins[i] / (1 << s->lquant));
		bmaxs[i] = ceil(maxs[i] / (1 << s->lquant));

		s->texturemins[i] = bmins[i] << s->lquant;
		s->extents[i] = (bmaxs[i] - bmins[i]) << s->lquant;
	}
}

static void R_ModLoadFaces (lump_t * l)
{
	dface_t *in;
	mBspSurface_t *out;
	int i, count, surfnum;
	int planenum, side;
	int ti;

	in = (void *) (mod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
		Sys_Error("R_ModLoadFaces: funny lump size in %s", loadmodel->name);
	count = l->filelen / sizeof(*in);
	out = VID_TagAlloc(vid_modelPool, count * sizeof(*out), 0);
	Com_DPrintf(DEBUG_RENDERER, "...faces: %i\n", count);

	loadmodel->bsp.surfaces = out;
	loadmodel->bsp.numsurfaces = count;

	currentmodel = loadmodel;

	for (surfnum = 0; surfnum < count; surfnum++, in++, out++) {
		out->firstedge = LittleLong(in->firstedge);
		out->numedges = LittleShort(in->numedges);
		out->flags = 0;
		out->polys = NULL;

		planenum = LittleShort(in->planenum);
		side = LittleShort(in->side);
		if (side)
			out->flags |= SURF_PLANEBACK;

		out->plane = loadmodel->bsp.planes + planenum;

		ti = LittleShort(in->texinfo);
		if (ti < 0 || ti >= loadmodel->bsp.numtexinfo)
			Sys_Error("R_ModLoadFaces: bad texinfo number");
		out->texinfo = loadmodel->bsp.texinfo + ti;

		out->lquant = loadmodel->bsp.lightquant;
		CalcSurfaceExtents(out);

		/* lighting info */
		for (i = 0; i < MAXLIGHTMAPS; i++)
			out->styles[i] = in->styles[i];
		i = LittleLong(in->lightofs);
		if (i == -1)
			out->samples = NULL;
		else
			out->samples = loadmodel->bsp.lightdata + i;

		/* set the drawing flags */
		if (out->texinfo->flags & SURF_WARP) {
			out->flags |= SURF_DRAWTURB;
			for (i = 0; i < 2; i++) {
				out->extents[i] = 16384;
				out->texturemins[i] = -8192;
			}
			/* cut up polygon for warps */
			R_SubdivideSurface(out);
		}

		/* create lightmaps and polygons */
		if (!(out->texinfo->flags & (SURF_TRANS33 | SURF_TRANS66 | SURF_WARP)))
			R_CreateSurfaceLightmap(out);

		if (!(out->texinfo->flags & SURF_WARP))
			R_BuildPolygonFromSurface(out, shift);
	}
}


static void R_ModSetParent (mBspNode_t * node, mBspNode_t * parent)
{
	node->parent = parent;
	if (node->contents != NODE_NO_LEAF)
		return;
	R_ModSetParent(node->children[0], node);
	R_ModSetParent(node->children[1], node);
}

static void R_ModLoadNodes (lump_t * l)
{
	int i, j, count, p;
	dnode_t *in;
	mBspNode_t *out;

	in = (void *) (mod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
		Sys_Error("R_ModLoadNodes: funny lump size in %s", loadmodel->name);
	count = l->filelen / sizeof(*in);
	out = VID_TagAlloc(vid_modelPool, count * sizeof(*out), 0);
	Com_DPrintf(DEBUG_RENDERER, "...nodes: %i\n", count);

	loadmodel->bsp.nodes = out;
	loadmodel->bsp.numnodes = count;

	for (i = 0; i < count; i++, in++, out++) {
		for (j = 0; j < 3; j++) {
			out->minmaxs[j] = LittleShort(in->mins[j]) + shift[j];
			out->minmaxs[3 + j] = LittleShort(in->maxs[j]) + shift[j];
		}

		p = LittleLong(in->planenum);
		if (in->planenum == -1)
			out->plane = NULL;
		else
			out->plane = loadmodel->bsp.planes + p;

		out->firstsurface = LittleShort(in->firstface);
		out->numsurfaces = LittleShort(in->numfaces);
		/* differentiate from leafs */
		out->contents = NODE_NO_LEAF;

		for (j = 0; j < 2; j++) {
			p = LittleLong(in->children[j]);
			if (p >= 0)
				out->children[j] = loadmodel->bsp.nodes + p;
			else
				out->children[j] = (mBspNode_t *) (loadmodel->bsp.leafs + (-1 - p));
		}
	}

	/* sets nodes and leafs */
	R_ModSetParent(loadmodel->bsp.nodes, NULL);
}

static void R_ModLoadLeafs (lump_t * l)
{
	dleaf_t *in;
	mBspLeaf_t *out;
	int i, j, count, p;

	in = (void *) (mod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
		Sys_Error("R_ModLoadLeafs: funny lump size in %s", loadmodel->name);
	count = l->filelen / sizeof(*in);
	out = VID_TagAlloc(vid_modelPool, count * sizeof(*out), 0);
	Com_DPrintf(DEBUG_RENDERER, "...leafs: %i\n", count);

	loadmodel->bsp.leafs = out;
	loadmodel->bsp.numleafs = count;

	for (i = 0; i < count; i++, in++, out++) {
		for (j = 0; j < 3; j++) {
			out->minmaxs[j] = LittleShort(in->mins[j]) + shift[j];
			out->minmaxs[3 + j] = LittleShort(in->maxs[j]) + shift[j];
		}

		p = LittleLong(in->contentFlags);
		out->contents = p;

		out->cluster = LittleShort(in->cluster);
		out->area = LittleShort(in->area);

		out->firstmarksurface = loadmodel->bsp.marksurfaces + LittleShort(in->firstleafface);
		out->nummarksurfaces = LittleShort(in->numleaffaces);
	}
}

static void R_ModLoadMarksurfaces (lump_t * l)
{
	int i, j, count;
	short *in;
	mBspSurface_t **out;

	in = (void *) (mod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
		Sys_Error("R_ModLoadMarksurfaces: funny lump size in %s", loadmodel->name);
	count = l->filelen / sizeof(*in);
	out = VID_TagAlloc(vid_modelPool, count * sizeof(*out), 0);
	Com_DPrintf(DEBUG_RENDERER, "...marksurfaces: %i\n", count);

	loadmodel->bsp.marksurfaces = out;
	loadmodel->bsp.nummarksurfaces = count;

	for (i = 0; i < count; i++) {
		j = LittleShort(in[i]);
		if (j < 0 || j >= loadmodel->bsp.numsurfaces)
			Sys_Error("R_ModParseMarksurfaces: bad surface number");
		out[i] = loadmodel->bsp.surfaces + j;
	}
}

static void R_ModLoadSurfedges (lump_t * l)
{
	int i, count;
	int *in, *out;

	in = (void *) (mod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
		Sys_Error("R_ModLoadSurfedges: funny lump size in %s", loadmodel->name);
	count = l->filelen / sizeof(*in);
	if (count < 1 || count >= MAX_MAP_SURFEDGES)
		Sys_Error("R_ModLoadSurfedges: bad surfedges count in %s: %i", loadmodel->name, count);

	out = VID_TagAlloc(vid_modelPool, count * sizeof(*out), 0);
	Com_DPrintf(DEBUG_RENDERER, "...surface edges: %i\n", count);

	loadmodel->bsp.surfedges = out;
	loadmodel->bsp.numsurfedges = count;

	for (i = 0; i < count; i++)
		out[i] = LittleLong(in[i]);
}

/**
 * @sa CMod_LoadPlanes
 */
static void R_ModLoadPlanes (lump_t * l)
{
	int i, j;
	cBspPlane_t *out;
	dplane_t *in;
	int count;
	int bits;

	in = (void *) (mod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
		Sys_Error("R_ModLoadPlanes: funny lump size in %s", loadmodel->name);
	count = l->filelen / sizeof(*in);
	out = VID_TagAlloc(vid_modelPool, count * 2 * sizeof(*out), 0);
	Com_DPrintf(DEBUG_RENDERER, "...planes: %i\n", count);

	loadmodel->bsp.planes = out;
	loadmodel->bsp.numplanes = count;

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
	}
}


static void R_ModShiftTile (void)
{
	mBspVertex_t *vert;
	cBspPlane_t *plane;
	int i, j;

	/* we can't do this instantly, because of rounding errors on extents calculation */
	/* shift vertexes */
	for (i = 0, vert = loadmodel->bsp.vertexes; i < loadmodel->bsp.numvertexes; i++, vert++)
		for (j = 0; j < 3; j++)
			vert->position[j] += shift[j];

	/* shift planes */
	for (i = 0, plane = loadmodel->bsp.planes; i < loadmodel->bsp.numplanes; i++, plane++)
		for (j = 0; j < 3; j++)
			plane->dist += plane->normal[j] * shift[j];
}


/**
 * @sa CM_AddMapTile
 */
static void R_ModAddMapTile (const char *name, int sX, int sY, int sZ)
{
	int i;
	unsigned *buffer;
	dheader_t *header;
	mBspHeader_t *bm;

	/* get new model */
	if ((mod_numknown < 0) || (mod_numknown >= MAX_MOD_KNOWN))
		Sys_Error("R_ModAddMapTile: mod_numknown >= MAX_MOD_KNOWN");

	if ((rNumTiles < 0) || (rNumTiles >= MAX_MAPTILES))
		Sys_Error("R_ModAddMapTile: Too many map tiles");

	/* alloc model and tile */
	loadmodel = &mod_known[mod_numknown++];
	rTiles[rNumTiles++] = loadmodel;
	memset(loadmodel, 0, sizeof(model_t));
	Com_sprintf(loadmodel->name, sizeof(loadmodel->name), "maps/%s.bsp", name);

	/* load the file */
	FS_LoadFile(loadmodel->name, (byte **) &buffer);
	if (!buffer)
		Sys_Error("R_ModAddMapTile: %s not found", loadmodel->name);

	/* init */
	loadmodel->type = mod_brush;

	/* prepare shifting */
	VectorSet(shift, sX * UNIT_SIZE, sY * UNIT_SIZE, sZ * UNIT_SIZE);

	/* test version */
	header = (dheader_t *) buffer;
	i = LittleLong(header->version);
	if (i != BSPVERSION)
		Sys_Error("R_ModAddMapTile: %s has wrong version number (%i should be %i)", loadmodel->name, i, BSPVERSION);

	/* swap all the lumps */
	mod_base = (byte *) header;

	for (i = 0; i < (int)sizeof(dheader_t) / 4; i++)
		((int *) header)[i] = LittleLong(((int *) header)[i]);

	/* load into heap */
	R_ModLoadVertexes(&header->lumps[LUMP_VERTEXES]);
	R_ModLoadEdges(&header->lumps[LUMP_EDGES]);
	R_ModLoadSurfedges(&header->lumps[LUMP_SURFEDGES]);
	R_ModLoadLighting(&header->lumps[LUMP_LIGHTING]);
	R_ModLoadPlanes(&header->lumps[LUMP_PLANES]);
	R_ModLoadTexinfo(&header->lumps[LUMP_TEXINFO]);
	R_ModLoadFaces(&header->lumps[LUMP_FACES]);
	R_ModShiftTile();
	R_ModLoadMarksurfaces(&header->lumps[LUMP_LEAFFACES]);
	R_ModLoadLeafs(&header->lumps[LUMP_LEAFS]);
	R_ModLoadNodes(&header->lumps[LUMP_NODES]);
	R_ModLoadSubmodels(&header->lumps[LUMP_MODELS]);
	/* regular and alternate animation */
	loadmodel->numframes = 2;

	/* set up the submodels, the first 255 submodels
	 * are the models of the different levels, don't
	 * care about them */
	for (i = LEVEL_TRACING - 1; i < loadmodel->bsp.numsubmodels; i++) {
		model_t *starmod;

		bm = &loadmodel->bsp.submodels[i];
		starmod = &mod_inline[numInline++];

		*starmod = *loadmodel;

		starmod->bsp.firstmodelsurface = bm->firstface;
		starmod->bsp.nummodelsurfaces = bm->numfaces;
		starmod->bsp.firstnode = bm->headnode;
		if (starmod->bsp.firstnode >= loadmodel->bsp.numnodes)
			Sys_Error("R_ModAddMapTile: Inline model %i has bad firstnode", i);

		VectorCopy(bm->maxs, starmod->maxs);
		VectorCopy(bm->mins, starmod->mins);
		starmod->radius = bm->radius;

		if (i == 0)
			*loadmodel = *starmod;

		starmod->bsp.numleafs = bm->visleafs;
	}
	FS_FreeFile(buffer);
}

/**
 * @brief Specifies the model that will be used as the world
 * @sa R_ModEndLoading
 */
void R_ModBeginLoading (const char *tiles, const char *pos)
{
	const char *token;
	char name[MAX_VAR];
	char base[MAX_QPATH];
	int sh[3];
	int i;

	/* next registration sequence */
	registration_sequence++;

	/* fix this, currently needed, slows down loading times */
	R_ShutdownModels();

	/* init */
	R_BeginBuildingLightmaps();
	numInline = 0;
	rNumTiles = 0;

	/* load tiles */
	while (tiles) {
		/* get tile name */
		token = COM_Parse(&tiles);
		if (!tiles) {
			/* finish */
			R_EndBuildingLightmaps();
			return;
		}

		/* get base path */
		if (token[0] == '-') {
			Q_strncpyz(base, token + 1, sizeof(base));
			continue;
		}

		/* get tile name */
		if (token[0] == '+')
			Com_sprintf(name, MAX_VAR, "%s%s", base, token + 1);
		else
			Q_strncpyz(name, token, sizeof(name));

		R_LoadMaterials(name);

		if (pos && pos[0]) {
			/* get position and add a tile */
			for (i = 0; i < 2; i++) {
				token = COM_Parse(&pos);
				if (!pos)
					Sys_Error("R_ModBeginLoading: invalid positions\n");
				sh[i] = atoi(token);
			}
			R_ModAddMapTile(name, sh[0], sh[1], 0);
		} else {
			/* load only a single tile, if no positions are specified */
			R_ModAddMapTile(name, 0, 0, 0);
			R_EndBuildingLightmaps();
			return;
		}
	}

	Sys_Error("R_ModBeginLoading: invalid tile names\n");
}
