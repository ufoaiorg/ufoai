/**
 * @file gl_model_brush.c
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

#include "gl_local.h"

/*
===============================================================================
BRUSHMODEL LOADING
===============================================================================
*/

static byte *mod_base;
static int shift[3];
void GL_EndBuildingLightmaps(void);
void GL_BeginBuildingLightmaps(void);
typedef model_t *model_p;
model_t *loadmodel;

/**
 * @brief FIXME
 */
static void Mod_LoadLighting (lump_t * l)
{
	if (!l->filelen) {
		loadmodel->lightdata = NULL;
		return;
	}
	loadmodel->lightdata = ri.TagMalloc(ri.lightPool, l->filelen, 0);
	loadmodel->lightquant = *(byte *) (mod_base + l->fileofs);
	memcpy(loadmodel->lightdata, mod_base + l->fileofs, l->filelen);
}

/**
 * @brief
 */
static void Mod_LoadVertexes (lump_t * l)
{
	dvertex_t *in;
	mvertex_t *out;
	int i, count;

	in = (void *) (mod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
		ri.Sys_Error(ERR_DROP, "Mod_LoadVertexes: funny lump size in %s", loadmodel->name);
	count = l->filelen / sizeof(*in);
	out = ri.TagMalloc(ri.modelPool, count * sizeof(*out), 0);
	Com_Printf("...verts: %i\n", count);

	loadmodel->vertexes = out;
	loadmodel->numvertexes = count;

	for (i = 0; i < count; i++, in++, out++) {
		out->position[0] = LittleFloat(in->point[0]);
		out->position[1] = LittleFloat(in->point[1]);
		out->position[2] = LittleFloat(in->point[2]);
	}
}

/**
 * @brief
 */
static float RadiusFromBounds (vec3_t mins, vec3_t maxs)
{
	int i;
	vec3_t corner;

	for (i = 0; i < 3; i++)
		corner[i] = fabs(mins[i]) > fabs(maxs[i]) ? fabs(mins[i]) : fabs(maxs[i]);

	return VectorLength(corner);
}


/**
 * @brief
 * @sa CMod_LoadSubmodels
 */
static void Mod_LoadSubmodels (lump_t * l)
{
	dmodel_t *in;
	mmodel_t *out;
	int i, j, count;

	in = (void *) (mod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
		ri.Sys_Error(ERR_DROP, "Mod_LoadSubmodels: funny lump size in %s", loadmodel->name);
	count = l->filelen / sizeof(*in);
	out = ri.TagMalloc(ri.modelPool, count * sizeof(*out), 0);
	Com_Printf("...submodels: %i\n", count);

	loadmodel->submodels = out;
	loadmodel->numsubmodels = count;

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

/**
 * @brief
 */
static void Mod_LoadEdges (lump_t * l)
{
	dedge_t *in;
	medge_t *out;
	int i, count;

	in = (void *) (mod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
		ri.Sys_Error(ERR_DROP, "Mod_LoadEdges: funny lump size in %s", loadmodel->name);
	count = l->filelen / sizeof(*in);
	out = ri.TagMalloc(ri.modelPool, (count + 1) * sizeof(*out), 0);
	Com_Printf("...edges: %i\n", count);

	loadmodel->edges = out;
	loadmodel->numedges = count;

	for (i = 0; i < count; i++, in++, out++) {
		out->v[0] = (unsigned short) LittleShort(in->v[0]);
		out->v[1] = (unsigned short) LittleShort(in->v[1]);
	}
}

/**
 * @brief
 */
static void Mod_LoadTexinfo (lump_t * l)
{
	texinfo_t *in;
	mtexinfo_t *out, *step;
	int i, j, count;
	char name[MAX_QPATH];
	int next;

	in = (void *) (mod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
		ri.Sys_Error(ERR_DROP, "Mod_LoadTexinfo: funny lump size in %s", loadmodel->name);
	count = l->filelen / sizeof(*in);
	out = ri.TagMalloc(ri.modelPool, count * sizeof(*out), 0);
	Com_Printf("...texinfo: %i\n", count);

	loadmodel->texinfo = out;
	loadmodel->numtexinfo = count;

	for (i = 0; i < count; i++, in++, out++) {
		for (j = 0; j < 4; j++) {
			out->vecs[0][j] = LittleFloat(in->vecs[0][j]);
			out->vecs[1][j] = LittleFloat(in->vecs[1][j]);
		}

		out->flags = LittleLong(in->flags);
		next = LittleLong(in->nexttexinfo);
		if (next > 0)
			out->next = loadmodel->texinfo + next;
		else
			out->next = NULL;
		/* exchange the textures with the ones that are needed for base assembly */
		if (r_newrefdef.mapZone && strstr(in->texture, "tex_bases/dummy"))
			Com_sprintf(name, sizeof(name), "textures/tex_bases/%s", r_newrefdef.mapZone);
		else
			Com_sprintf(name, sizeof(name), "textures/%s", in->texture);

		out->image = GL_FindImage(name, it_wall);
		if (!out->image) {
			ri.Con_Printf(PRINT_ALL, "Couldn't load %s\n", name);
			out->image = r_notexture;
		}
	}

	/* count animation frames */
	for (i = 0; i < count; i++) {
		out = &loadmodel->texinfo[i];
		out->numframes = 1;
		for (step = out->next; step && step != out; step = step->next)
			out->numframes++;
	}
}

/**
 * @brief Fills in s->texturemins[] and s->extents[]
 */
static void CalcSurfaceExtents (msurface_t * s)
{
	float mins[2], maxs[2], val;

/* 	vec3_t	pos; */
	int i, j, e;
	mvertex_t *v;
	mtexinfo_t *tex;
	int bmins[2], bmaxs[2];

	mins[0] = mins[1] = 999999;
	maxs[0] = maxs[1] = -99999;

	tex = s->texinfo;

	for (i = 0; i < s->numedges; i++) {
		e = loadmodel->surfedges[s->firstedge + i];
		if (e >= 0)
			v = &loadmodel->vertexes[loadmodel->edges[e].v[0]];
		else
			v = &loadmodel->vertexes[loadmodel->edges[-e].v[1]];

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


void GL_BuildPolygonFromSurface(msurface_t * fa, int shift[3]);
void GL_CreateSurfaceLightmap(msurface_t * surf);

/**
 * @brief
 */
static void Mod_LoadFaces (lump_t * l)
{
	dface_t *in;
	msurface_t *out;
	int i, count, surfnum;
	int planenum, side;
	int ti;

	in = (void *) (mod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
		ri.Sys_Error(ERR_DROP, "Mod_LoadFaces: funny lump size in %s", loadmodel->name);
	count = l->filelen / sizeof(*in);
	out = ri.TagMalloc(ri.modelPool, count * sizeof(*out), 0);
	Com_Printf("...faces: %i\n", count);

	loadmodel->surfaces = out;
	loadmodel->numsurfaces = count;

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

		out->plane = loadmodel->planes + planenum;

		ti = LittleShort(in->texinfo);
		if (ti < 0 || ti >= loadmodel->numtexinfo)
			ri.Sys_Error(ERR_DROP, "Mod_LoadFaces: bad texinfo number");
		out->texinfo = loadmodel->texinfo + ti;

		out->lquant = loadmodel->lightquant;
		CalcSurfaceExtents(out);

		/* lighting info */
		for (i = 0; i < MAXLIGHTMAPS; i++)
			out->styles[i] = in->styles[i];
		i = LittleLong(in->lightofs);
		if (i == -1)
			out->samples = NULL;
		else
			out->samples = loadmodel->lightdata + i;

		/* set the drawing flags */
		if (out->texinfo->flags & SURF_WARP) {
			out->flags |= SURF_DRAWTURB;
			for (i = 0; i < 2; i++) {
				out->extents[i] = 16384;
				out->texturemins[i] = -8192;
			}
			/* cut up polygon for warps */
			GL_SubdivideSurface(out);
		}

		/* create lightmaps and polygons */
		if (!(out->texinfo->flags & (SURF_TRANS33 | SURF_TRANS66 | SURF_WARP)))
			GL_CreateSurfaceLightmap(out);

		if (!(out->texinfo->flags & SURF_WARP))
			GL_BuildPolygonFromSurface(out, shift);
	}
}


/**
 * @brief
 */
static void Mod_SetParent (mnode_t * node, mnode_t * parent)
{
	node->parent = parent;
	if (node->contents != -1)
		return;
	Mod_SetParent(node->children[0], node);
	Mod_SetParent(node->children[1], node);
}

/**
 * @brief
 */
static void Mod_LoadNodes (lump_t * l)
{
	int i, j, count, p;
	dnode_t *in;
	mnode_t *out;

	in = (void *) (mod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
		ri.Sys_Error(ERR_DROP, "Mod_LoadNodes: funny lump size in %s", loadmodel->name);
	count = l->filelen / sizeof(*in);
	out = ri.TagMalloc(ri.modelPool, count * sizeof(*out), 0);
	Com_Printf("...nodes: %i\n", count);

	loadmodel->nodes = out;
	loadmodel->numnodes = count;

	for (i = 0; i < count; i++, in++, out++) {
		for (j = 0; j < 3; j++) {
			out->minmaxs[j] = LittleShort(in->mins[j]) + shift[j];
			out->minmaxs[3 + j] = LittleShort(in->maxs[j]) + shift[j];
		}

		p = LittleLong(in->planenum);
		if (in->planenum == -1)
			out->plane = NULL;
		else
			out->plane = loadmodel->planes + p;

		out->firstsurface = LittleShort(in->firstface);
		out->numsurfaces = LittleShort(in->numfaces);
		/* differentiate from leafs */
		out->contents = -1;

		for (j = 0; j < 2; j++) {
			p = LittleLong(in->children[j]);
			if (p >= 0)
				out->children[j] = loadmodel->nodes + p;
			else
				out->children[j] = (mnode_t *) (loadmodel->leafs + (-1 - p));
		}
	}

	/* sets nodes and leafs */
	Mod_SetParent(loadmodel->nodes, NULL);
}

/**
 * @brief
 */
static void Mod_LoadLeafs (lump_t * l)
{
	dleaf_t *in;
	mleaf_t *out;
	int i, j, count, p;

/*	glpoly_t	*poly; */

	in = (void *) (mod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
		ri.Sys_Error(ERR_DROP, "Mod_LoadLeafs: funny lump size in %s", loadmodel->name);
	count = l->filelen / sizeof(*in);
	out = ri.TagMalloc(ri.modelPool, count * sizeof(*out), 0);
	Com_Printf("...leafs: %i\n", count);

	loadmodel->leafs = out;
	loadmodel->numleafs = count;

	for (i = 0; i < count; i++, in++, out++) {
		for (j = 0; j < 3; j++) {
			out->minmaxs[j] = LittleShort(in->mins[j]) + shift[j];
			out->minmaxs[3 + j] = LittleShort(in->maxs[j]) + shift[j];
		}

		p = LittleLong(in->contents);
		out->contents = p;

		out->cluster = LittleShort(in->cluster);
		out->area = LittleShort(in->area);

		out->firstmarksurface = loadmodel->marksurfaces + LittleShort(in->firstleafface);
		out->nummarksurfaces = LittleShort(in->numleaffaces);
	}
}

/**
 * @brief
 */
static void Mod_LoadMarksurfaces (lump_t * l)
{
	int i, j, count;
	short *in;
	msurface_t **out;

	in = (void *) (mod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
		ri.Sys_Error(ERR_DROP, "Mod_LoadMarksurfaces: funny lump size in %s", loadmodel->name);
	count = l->filelen / sizeof(*in);
	out = ri.TagMalloc(ri.modelPool, count * sizeof(*out), 0);
	Com_Printf("...marksurfaces: %i\n", count);

	loadmodel->marksurfaces = out;
	loadmodel->nummarksurfaces = count;

	for (i = 0; i < count; i++) {
		j = LittleShort(in[i]);
		if (j < 0 || j >= loadmodel->numsurfaces)
			ri.Sys_Error(ERR_DROP, "Mod_ParseMarksurfaces: bad surface number");
		out[i] = loadmodel->surfaces + j;
	}
}

/**
 * @brief
 */
static void Mod_LoadSurfedges (lump_t * l)
{
	int i, count;
	int *in, *out;

	in = (void *) (mod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
		ri.Sys_Error(ERR_DROP, "Mod_LoadSurfedges: funny lump size in %s", loadmodel->name);
	count = l->filelen / sizeof(*in);
	if (count < 1 || count >= MAX_MAP_SURFEDGES)
		ri.Sys_Error(ERR_DROP, "Mod_LoadSurfedges: bad surfedges count in %s: %i", loadmodel->name, count);

	out = ri.TagMalloc(ri.modelPool, count * sizeof(*out), 0);
	Com_Printf("...surface edges: %i\n", count);

	loadmodel->surfedges = out;
	loadmodel->numsurfedges = count;

	for (i = 0; i < count; i++)
		out[i] = LittleLong(in[i]);
}


/**
 * @brief
 */
static void Mod_LoadPlanes (lump_t * l)
{
	int i, j;
	cplane_t *out;
	dplane_t *in;
	int count;
	int bits;

	in = (void *) (mod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
		ri.Sys_Error(ERR_DROP, "Mod_LoadPlanes: funny lump size in %s", loadmodel->name);
	count = l->filelen / sizeof(*in);
	out = ri.TagMalloc(ri.modelPool, count * 2 * sizeof(*out), 0);
	Com_Printf("...planes: %i\n", count);

	loadmodel->planes = out;
	loadmodel->numplanes = count;

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


/**
 * @brief
 */
static void Mod_ShiftTile (void)
{
	mvertex_t *vert;
	cplane_t *plane;
	int i, j;

	/* we can't do this instantly, because of rounding errors on extents calculation */
	/* shift vertexes */
	for (i = 0, vert = loadmodel->vertexes; i < loadmodel->numvertexes; i++, vert++)
		for (j = 0; j < 3; j++)
			vert->position[j] += shift[j];

	/* shift planes */
	for (i = 0, plane = loadmodel->planes; i < loadmodel->numplanes; i++, plane++)
		for (j = 0; j < 3; j++)
			plane->dist += plane->normal[j] * shift[j];
}


/**
 * @brief
 * @sa CM_AddMapTile
 */
static void R_AddMapTile (const char *name, int sX, int sY, int sZ)
{
	int i;
	unsigned *buffer;
	dheader_t *header;
	mmodel_t *bm;

	/* get new model */
	if ((mod_numknown < 0) || (mod_numknown >= MAX_MOD_KNOWN)) {
		ri.Sys_Error(ERR_DROP, "mod_numknown >= MAX_MOD_KNOWN");
		return;					/* never reached. need for code analyst. */
	}

	if ((rNumTiles < 0) || (rNumTiles >= MAX_MAPTILES)) {
		ri.Sys_Error(ERR_DROP, "Too many map tiles");
		return;					/* never reached. need for code analyst. */
	}

	/* alloc model and tile */
	loadmodel = &mod_known[mod_numknown++];
	rTiles[rNumTiles++] = loadmodel;
	memset(loadmodel, 0, sizeof(model_t));
	Com_sprintf(loadmodel->name, sizeof(loadmodel->name), "maps/%s.bsp", name);

	/* load the file */
	ri.FS_LoadFile(loadmodel->name, (void **) &buffer);
	if (!buffer) {
		ri.Sys_Error(ERR_DROP, "R_AddMapTile: %s not found", loadmodel->name);
		return;					/* never reached. need for code analyst. */
	}

	/* init */
	loadmodel->registration_sequence = registration_sequence;
	loadmodel->type = mod_brush;

	/* prepare shifting */
	VectorSet(shift, sX * UNIT_SIZE, sY * UNIT_SIZE, sZ * UNIT_SIZE);

	/* test version */
	header = (dheader_t *) buffer;
	i = LittleLong(header->version);
	if (i != BSPVERSION)
		ri.Sys_Error(ERR_DROP, "R_AddMapTile: %s has wrong version number (%i should be %i)", loadmodel->name, i, BSPVERSION);

	/* swap all the lumps */
	mod_base = (byte *) header;

	for (i = 0; i < (int)sizeof(dheader_t) / 4; i++)
		((int *) header)[i] = LittleLong(((int *) header)[i]);

	/* load into heap */
	Mod_LoadVertexes(&header->lumps[LUMP_VERTEXES]);
	Mod_LoadEdges(&header->lumps[LUMP_EDGES]);
	Mod_LoadSurfedges(&header->lumps[LUMP_SURFEDGES]);
	Mod_LoadLighting(&header->lumps[LUMP_LIGHTING]);
	Mod_LoadPlanes(&header->lumps[LUMP_PLANES]);
	Mod_LoadTexinfo(&header->lumps[LUMP_TEXINFO]);
	Mod_LoadFaces(&header->lumps[LUMP_FACES]);
	Mod_ShiftTile();
	Mod_LoadMarksurfaces(&header->lumps[LUMP_LEAFFACES]);
	Mod_LoadLeafs(&header->lumps[LUMP_LEAFS]);
	Mod_LoadNodes(&header->lumps[LUMP_NODES]);
	Mod_LoadSubmodels(&header->lumps[LUMP_MODELS]);
	/* regular and alternate animation */
	loadmodel->numframes = 2;

	/* set up the submodels, the first 255 submodels */
	/* are the models of the different levels, don't */
	/* care about them */
	for (i = LEVEL_TRACING - 1; i < loadmodel->numsubmodels; i++) {
		model_t *starmod;

		bm = &loadmodel->submodels[i];
		starmod = &mod_inline[numInline++];

		*starmod = *loadmodel;

		starmod->firstmodelsurface = bm->firstface;
		starmod->nummodelsurfaces = bm->numfaces;
		starmod->firstnode = bm->headnode;
		if (starmod->firstnode >= loadmodel->numnodes)
			ri.Sys_Error(ERR_DROP, "Inline model %i has bad firstnode", i);

		FastVectorCopy(bm->maxs, starmod->maxs);
		FastVectorCopy(bm->mins, starmod->mins);
		starmod->radius = bm->radius;

		if (i == 0)
			*loadmodel = *starmod;

		starmod->numleafs = bm->visleafs;
	}
	ri.FS_FreeFile(buffer);
}

/**
 * @brief Specifies the model that will be used as the world
 */
void GL_BeginLoading (char *tiles, char *pos)
{
	char *token;
	char name[MAX_VAR];
	char base[MAX_QPATH];
	int sh[3];
	int i;

	/* next registration sequence */
	registration_sequence++;

	/* fix this, currently needed, slows down loading times */
	Mod_FreeAll();

	/* init */
	GL_BeginBuildingLightmaps();
	numInline = 0;
	rNumTiles = 0;

	/* load tiles */
	while (tiles) {
		/* get tile name */
		token = COM_Parse(&tiles);
		if (!tiles) {
			/* finish */
			GL_EndBuildingLightmaps();
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

		if (pos && pos[0]) {
			/* get position and add a tile */
			for (i = 0; i < 2; i++) {
				token = COM_Parse(&pos);
				if (!pos)
					ri.Sys_Error(ERR_DROP, "GL_BeginLoading: invalid positions\n");
				sh[i] = atoi(token);
			}
			R_AddMapTile(name, sh[0], sh[1], 0);
		} else {
			/* load only a single tile, if no positions are specified */
			R_AddMapTile(name, 0, 0, 0);
			GL_EndBuildingLightmaps();
			return;
		}
	}

	ri.Sys_Error(ERR_DROP, "GL_BeginLoading: invalid tile names\n");
}
