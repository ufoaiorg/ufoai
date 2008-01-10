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
#include "r_lightmap.h"

/*
===============================================================================
BRUSHMODEL LOADING
===============================================================================
*/

static byte *mod_base;
static int shift[3];
static model_t *loadmodel;

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
	dBspVertex_t *in;
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
		corner[i] = fabsf(mins[i]) > fabsf(maxs[i]) ? fabsf(mins[i]) : fabsf(maxs[i]);

	return VectorLength(corner);
}


/**
 * @sa CR_ModLoadSubmodels
 */
static void R_ModLoadSubmodels (lump_t * l)
{
	dBspModel_t *in;
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
	dBspEdge_t *in;
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
	dBspTexinfo_t *in;
	mBspTexInfo_t *out;
	int i, j, count;
	char name[MAX_QPATH];

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

		/* exchange the textures with the ones that are needed for base assembly */
		if (refdef.mapZone && strstr(in->texture, "tex_terrain/dummy"))
			Com_sprintf(name, sizeof(name), "textures/tex_terrain/%s", refdef.mapZone);
		else
			Com_sprintf(name, sizeof(name), "textures/%s", in->texture);

		out->image = R_FindImage(name, it_wall);
	}
}

/**
 * @brief Fills in s->stmins[] and s->stmaxs[]
 */
static void R_SetSurfaceExtents (mBspSurface_t *surf, model_t* mod)
{
	vec3_t mins, maxs;
	vec2_t stmins, stmaxs;
	int bmins[2], bmaxs[2];
	float val;
	int i, j, e;
	mBspVertex_t *v;
	mBspTexInfo_t *tex;

	VectorSet(mins, 999999, 999999, 999999);
	VectorSet(maxs, -999999, -999999, -999999);

	stmins[0] = stmins[1] = 999999;
	stmaxs[0] = stmaxs[1] = -99999;

	tex = surf->texinfo;

	for (i = 0; i < surf->numedges; i++) {
		e = mod->bsp.surfedges[surf->firstedge + i];
		if (e >= 0)
			v = &mod->bsp.vertexes[mod->bsp.edges[e].v[0]];
		else
			v = &mod->bsp.vertexes[mod->bsp.edges[-e].v[1]];

		for (j = 0; j < 3; j++) {  /* calculate mins, maxs */
			if (v->position[j] > maxs[j])
				maxs[j] = v->position[j];
			if (v->position[j] < mins[j])
				mins[j] = v->position[j];
		}

		for (j = 0; j < 2; j++) {  /* calculate stmins, stmaxs */
			val = v->position[0] * tex->vecs[j][0] +
				  v->position[1] * tex->vecs[j][1] +
				  v->position[2] * tex->vecs[j][2] +
				  tex->vecs[j][3];
			if (val < stmins[j])
				stmins[j] = val;
			if (val > stmaxs[j])
				stmaxs[j] = val;
		}
	}

	for (i = 0; i < 3; i++)
		surf->center[i] = (mins[i] + maxs[i]) / 2;

	for (i = 0; i < 2; i++) {
		/* tiny rounding hack, not sure if it works */
		bmins[i] = floor(stmins[i] / (1 << surf->lquant));
		bmaxs[i] = ceil(stmaxs[i] / (1 << surf->lquant));

		surf->stmins[i] = bmins[i] << surf->lquant;
		surf->stmaxs[i] = (bmaxs[i] - bmins[i]) << surf->lquant;
	}
}

static void R_ModLoadSurfaces (lump_t * l)
{
	dBspFace_t *in;
	mBspSurface_t *out;
	int i, count, surfnum;
	int planenum, side;
	int ti;

	in = (void *) (mod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
		Sys_Error("R_ModLoadSurfaces: funny lump size in %s", loadmodel->name);
	count = l->filelen / sizeof(*in);
	out = VID_TagAlloc(vid_modelPool, count * sizeof(*out), 0);
	Com_DPrintf(DEBUG_RENDERER, "...faces: %i\n", count);

	loadmodel->bsp.surfaces = out;
	loadmodel->bsp.numsurfaces = count;

	for (surfnum = 0; surfnum < count; surfnum++, in++, out++) {
		out->firstedge = LittleLong(in->firstedge);
		out->numedges = LittleShort(in->numedges);

		/* resolve plane */
		planenum = LittleShort(in->planenum);
		out->plane = loadmodel->bsp.planes + planenum;

		/* and sideness */
		side = LittleShort(in->side);
		if (side) {
			out->flags |= MSURF_PLANEBACK;
			VectorNegate(out->plane->normal, out->normal);
		} else {
			VectorCopy(out->plane->normal, out->normal);
		}

		ti = LittleShort(in->texinfo);
		if (ti < 0 || ti >= loadmodel->bsp.numtexinfo)
			Sys_Error("R_ModLoadSurfaces: bad texinfo number");
		out->texinfo = loadmodel->bsp.texinfo + ti;

		out->lquant = loadmodel->bsp.lightquant;

		/* and size, texcoords, etc */
		R_SetSurfaceExtents(out, loadmodel);

		/* lastly lighting info */
		for (i = 0; i < MAXLIGHTMAPS; i++)
			out->styles[i] = in->styles[i];

		i = LittleLong(in->lightofs);
		if (i == -1 || (out->texinfo->flags & (SURF_WARP)))
			out->samples = NULL;
		else {
			out->samples = loadmodel->bsp.lightdata + i;
			out->flags |= MSURF_LIGHTMAP;
		}

		R_CreateSurfaceLightmap(out);

		out->tile = r_numMapTiles - 1;
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
	dBspNode_t *in;
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
	dBspLeaf_t *in;
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
	dBspPlane_t *in;
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

static void R_LoadBspVertexArrays (void)
{
	int i, j, index;
	int vertind, coordind;
	float *vec, s, t;
	mBspEdge_t *edge;
	mBspSurface_t *surf;

	vertind = coordind = 0;
	surf = loadmodel->bsp.surfaces;

	for (i = 0; i < loadmodel->bsp.numsurfaces; i++, surf++) {
		surf->index = vertind / 3;

		for (j = 0; j < surf->numedges; j++) {
			index = loadmodel->bsp.surfedges[surf->firstedge + j];

			/* vertex */
			if (index > 0) {  /* negative indices to differentiate which end of the edge */
				edge = &loadmodel->bsp.edges[index];
				vec = loadmodel->bsp.vertexes[edge->v[0]].position;
			} else {
				edge = &loadmodel->bsp.edges[-index];
				vec = loadmodel->bsp.vertexes[edge->v[1]].position;
			}

			memcpy(&r_state.vertex_array_3d[vertind], vec, sizeof(vec3_t));

			/* texture coordinates */
			s = DotProduct(vec, surf->texinfo->vecs[0]) + surf->texinfo->vecs[0][3];
			s /= surf->texinfo->image->width;

			t = DotProduct(vec, surf->texinfo->vecs[1]) + surf->texinfo->vecs[1][3];
			t /= surf->texinfo->image->height;

			r_state.texcoord_array[coordind + 0] = s;
			r_state.texcoord_array[coordind + 1] = t;

			if (surf->flags & MSURF_LIGHTMAP) {  /* lightmap coordinates */
				s = DotProduct(vec, surf->texinfo->vecs[0]) + surf->texinfo->vecs[0][3];
				s -= surf->stmins[0];
				s += surf->light_s << surf->lquant;
				s += 1 << (surf->lquant - 1);
				s /= LIGHTMAP_BLOCK_WIDTH << surf->lquant;

				t = DotProduct(vec, surf->texinfo->vecs[1]) + surf->texinfo->vecs[1][3];
				t -= surf->stmins[1];
				t += surf->light_t << surf->lquant;
				t += 1 << (surf->lquant - 1);
				t /= LIGHTMAP_BLOCK_HEIGHT << surf->lquant;
			}

			r_state.color_array[coordind + 0] = s;
			r_state.color_array[coordind + 1] = t;

			/* normal vectors */
			memcpy(&r_state.normal_array[vertind], surf->normal, sizeof(vec3_t));

			vertind += 3;
			coordind += 2;
		}
	}

	/* populate the vertex arrays */
	loadmodel->bsp.verts = (GLfloat *)VID_TagAlloc(vid_modelPool, vertind * sizeof(GLfloat), 0);
	memcpy(loadmodel->bsp.verts, r_state.vertex_array_3d, vertind * sizeof(GLfloat));

	loadmodel->bsp.texcoords = (GLfloat *)VID_TagAlloc(vid_modelPool, coordind * sizeof(GLfloat), 0);
	memcpy(loadmodel->bsp.texcoords, r_state.texcoord_array, coordind * sizeof(GLfloat));

	loadmodel->bsp.lmtexcoords = (GLfloat *)VID_TagAlloc(vid_modelPool, coordind * sizeof(GLfloat), 0);
	memcpy(loadmodel->bsp.lmtexcoords, r_state.color_array, coordind * sizeof(GLfloat));

	loadmodel->bsp.normals = (GLfloat *)VID_TagAlloc(vid_modelPool, vertind * sizeof(GLfloat), 0);
	memcpy(loadmodel->bsp.normals, r_state.normal_array, vertind * sizeof(GLfloat));

	/* and also the vertex buffer objects */
	qglGenBuffers(1, &loadmodel->bsp.vertex_buffer);
	qglBindBuffer(GL_ARRAY_BUFFER, loadmodel->bsp.vertex_buffer);
	qglBufferData(GL_ARRAY_BUFFER, vertind * sizeof(GLfloat),
			r_state.vertex_array_3d, GL_STATIC_DRAW);

	qglGenBuffers(1, &loadmodel->bsp.texcoord_buffer);
	qglBindBuffer(GL_ARRAY_BUFFER, loadmodel->bsp.texcoord_buffer);
	qglBufferData(GL_ARRAY_BUFFER, coordind * sizeof(GLfloat),
			r_state.texcoord_array, GL_STATIC_DRAW);

	qglGenBuffers(1, &loadmodel->bsp.lmtexcoord_buffer);
	qglBindBuffer(GL_ARRAY_BUFFER, loadmodel->bsp.lmtexcoord_buffer);
	qglBufferData(GL_ARRAY_BUFFER, coordind * sizeof(GLfloat),
			r_state.color_array, GL_STATIC_DRAW);

	qglGenBuffers(1, &loadmodel->bsp.normal_buffer);
	qglBindBuffer(GL_ARRAY_BUFFER, loadmodel->bsp.normal_buffer);
	qglBufferData(GL_ARRAY_BUFFER, vertind * sizeof(GLfloat),
			r_state.normal_array, GL_STATIC_DRAW);

	qglBindBuffer(GL_ARRAY_BUFFER, 0);
}

/**
 * @sa CM_AddMapTile
 */
static void R_ModAddMapTile (const char *name, int sX, int sY, int sZ)
{
	int i;
	unsigned *buffer;
	dBspHeader_t *header;
	mBspHeader_t *bm;

	/* get new model */
	if ((r_numModels < 0) || (r_numModels >= MAX_MOD_KNOWN))
		Sys_Error("R_ModAddMapTile: r_numModels >= MAX_MOD_KNOWN");

	if ((r_numMapTiles < 0) || (r_numMapTiles >= MAX_MAPTILES))
		Sys_Error("R_ModAddMapTile: Too many map tiles");

	/* alloc model and tile */
	loadmodel = &r_models[r_numModels++];
	r_mapTiles[r_numMapTiles++] = loadmodel;
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
	header = (dBspHeader_t *) buffer;
	i = LittleLong(header->version);
	if (i != BSPVERSION)
		Sys_Error("R_ModAddMapTile: %s has wrong version number (%i should be %i)", loadmodel->name, i, BSPVERSION);

	/* swap all the lumps */
	mod_base = (byte *) header;

	for (i = 0; i < (int)sizeof(dBspHeader_t) / 4; i++)
		((int *) header)[i] = LittleLong(((int *) header)[i]);

	/* load into heap */
	Com_DPrintf(DEBUG_RENDERER, "...load vertexes\n");
	R_ModLoadVertexes(&header->lumps[LUMP_VERTEXES]);
	Com_DPrintf(DEBUG_RENDERER, "...load edges\n");
	R_ModLoadEdges(&header->lumps[LUMP_EDGES]);
	Com_DPrintf(DEBUG_RENDERER, "...load surfedges\n");
	R_ModLoadSurfedges(&header->lumps[LUMP_SURFEDGES]);
	Com_DPrintf(DEBUG_RENDERER, "...load lighting\n");
	R_ModLoadLighting(&header->lumps[LUMP_LIGHTING]);
	Com_DPrintf(DEBUG_RENDERER, "...load planes\n");
	R_ModLoadPlanes(&header->lumps[LUMP_PLANES]);
	Com_DPrintf(DEBUG_RENDERER, "...load texinfo\n");
	R_ModLoadTexinfo(&header->lumps[LUMP_TEXINFO]);
	Com_DPrintf(DEBUG_RENDERER, "...load surfaces\n");
	R_ModLoadSurfaces(&header->lumps[LUMP_FACES]);
	R_ModShiftTile();
	Com_DPrintf(DEBUG_RENDERER, "...load leafs\n");
	R_ModLoadLeafs(&header->lumps[LUMP_LEAFS]);
	Com_DPrintf(DEBUG_RENDERER, "...load nodes\n");
	R_ModLoadNodes(&header->lumps[LUMP_NODES]);
	Com_DPrintf(DEBUG_RENDERER, "...load submodels\n");
	R_ModLoadSubmodels(&header->lumps[LUMP_MODELS]);

	/* set up the submodels, the first 255 submodels
	 * are the models of the different levels, don't
	 * care about them */
	for (i = LEVEL_TRACING - 1; i < loadmodel->bsp.numsubmodels; i++) {
		model_t *starmod;

		bm = &loadmodel->bsp.submodels[i];
		starmod = &r_modelsInline[r_numModelsInline++];

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

	R_LoadBspVertexArrays();

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
	r_numModelsInline = 0;
	r_numMapTiles = 0;

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
