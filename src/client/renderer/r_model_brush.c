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
#include "../../shared/parse.h"
#include "r_light.h"

/*
===============================================================================
BRUSHMODEL LOADING
===============================================================================
*/

/**
 * @brief The model base pointer - bases for the lump offsets
 */
static const byte *mod_base;
/**
 * @brief The shift array is used for random map assemblies (RMA) to shift
 * the mins/maxs and stuff like that
 */
static ipos3_t shift;
/**
 * @brief The currently loaded world model for the actual tile
 * @sa r_mapTiles
 */
static model_t *r_worldmodel;

/**
 * @brief Load the lightmap data
 */
static void R_ModLoadLighting (const lump_t *l)
{
	/* map has no lightmap */
	if (l->filelen == 0)
		return;

	r_worldmodel->bsp.lightdata = (byte *)Mem_PoolAlloc(l->filelen, vid_lightPool, 0);
	r_worldmodel->bsp.lightquant = *(const byte *) (mod_base + l->fileofs);
	memcpy(r_worldmodel->bsp.lightdata, mod_base + l->fileofs, l->filelen);
}

static void R_ModLoadVertexes (const lump_t *l)
{
	const dBspVertex_t *in;
	mBspVertex_t *out;
	int i, count;

	in = (const dBspVertex_t *) (mod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
		Com_Error(ERR_DROP, "R_ModLoadVertexes: funny lump size in %s", r_worldmodel->name);
	count = l->filelen / sizeof(*in);
	out = (mBspVertex_t *)Mem_PoolAlloc(count * sizeof(*out), vid_modelPool, 0);
	Com_DPrintf(DEBUG_RENDERER, "...verts: %i\n", count);

	r_worldmodel->bsp.vertexes = out;
	r_worldmodel->bsp.numvertexes = count;

	for (i = 0; i < count; i++, in++, out++) {
		out->position[0] = LittleFloat(in->point[0]);
		out->position[1] = LittleFloat(in->point[1]);
		out->position[2] = LittleFloat(in->point[2]);
	}
}

static void R_ModLoadNormals (const lump_t *l)
{
	const dBspNormal_t *in;
	mBspVertex_t *out;
	int i, count;

	in = (const dBspNormal_t *)(mod_base + l->fileofs);
	if (l->filelen % sizeof(*in)) {
		Com_Error(ERR_DROP, "R_LoadNormals: Funny lump size in %s.", r_worldmodel->name);
	}
	count = l->filelen / sizeof(*in);

	if (count != r_worldmodel->bsp.numvertexes) {  /* ensure sane normals count */
		Com_Error(ERR_DROP, "R_LoadNormals: unexpected normals count in %s: (%d != %d).",
				r_worldmodel->name, count, r_worldmodel->bsp.numvertexes);
	}

	out = r_worldmodel->bsp.vertexes;

	for (i = 0; i < count; i++, in++, out++) {
		out->normal[0] = LittleFloat(in->normal[0]);
		out->normal[1] = LittleFloat(in->normal[1]);
		out->normal[2] = LittleFloat(in->normal[2]);
	}
}

static inline float R_RadiusFromBounds (const vec3_t mins, const vec3_t maxs)
{
	int i;
	vec3_t corner;

	for (i = 0; i < 3; i++)
		corner[i] = fabsf(mins[i]) > fabsf(maxs[i]) ? fabsf(mins[i]) : fabsf(maxs[i]);

	return VectorLength(corner);
}


/**
 * @brief Loads brush entities like func_door and func_breakable
 * @sa CMod_LoadSubmodels
 */
static void R_ModLoadSubmodels (const lump_t *l)
{
	const dBspModel_t *in;
	mBspHeader_t *out;
	int i, j, count;

	in = (const dBspModel_t *) (mod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
		Com_Error(ERR_DROP, "R_ModLoadSubmodels: funny lump size in %s", r_worldmodel->name);
	count = l->filelen / sizeof(*in);
	out = (mBspHeader_t *)Mem_PoolAlloc(count * sizeof(*out), vid_modelPool, 0);
	Com_DPrintf(DEBUG_RENDERER, "...submodels: %i\n", count);

	r_worldmodel->bsp.submodels = out;
	r_worldmodel->bsp.numsubmodels = count;

	for (i = 0; i < count; i++, in++, out++) {
		/* spread the mins / maxs by a pixel */
		for (j = 0; j < 3; j++) {
			out->mins[j] = LittleFloat(in->mins[j]) - 1.0f + (float)shift[j];
			out->maxs[j] = LittleFloat(in->maxs[j]) + 1.0f + (float)shift[j];
			out->origin[j] = LittleFloat(in->origin[j]) + (float)shift[j];
		}
		out->radius = R_RadiusFromBounds(out->mins, out->maxs);
		out->headnode = LittleLong(in->headnode);
		out->firstface = LittleLong(in->firstface);
		out->numfaces = LittleLong(in->numfaces);
	}
}

static void R_ModLoadEdges (const lump_t *l)
{
	const dBspEdge_t *in;
	mBspEdge_t *out;
	int i, count;

	in = (const dBspEdge_t *) (mod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
		Com_Error(ERR_DROP, "R_ModLoadEdges: funny lump size in %s", r_worldmodel->name);
	count = l->filelen / sizeof(*in);
	out = (mBspEdge_t *)Mem_PoolAlloc((count + 1) * sizeof(*out), vid_modelPool, 0);
	Com_DPrintf(DEBUG_RENDERER, "...edges: %i\n", count);

	r_worldmodel->bsp.edges = out;
	r_worldmodel->bsp.numedges = count;

	for (i = 0; i < count; i++, in++, out++) {
		out->v[0] = (unsigned short) LittleShort(in->v[0]);
		out->v[1] = (unsigned short) LittleShort(in->v[1]);
	}
}

/**
 * @sa CP_StartMissionMap
 */
static void R_ModLoadTexinfo (const lump_t *l)
{
	const dBspTexinfo_t *in;
	mBspTexInfo_t *out;
	int i, j, count;
	char name[MAX_QPATH];

	in = (const dBspTexinfo_t *) (mod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
		Com_Error(ERR_DROP, "R_ModLoadTexinfo: funny lump size in %s", r_worldmodel->name);
	count = l->filelen / sizeof(*in);
	out = (mBspTexInfo_t *)Mem_PoolAlloc(count * sizeof(*out), vid_modelPool, 0);
	Com_DPrintf(DEBUG_RENDERER, "...texinfo: %i\n", count);

	r_worldmodel->bsp.texinfo = out;
	r_worldmodel->bsp.numtexinfo = count;

	for (i = 0; i < count; i++, in++, out++) {
		for (j = 0; j < 3; j++) {
			out->uv[j] = LittleFloat(in->vecs[0][j]);
			out->vv[j] = LittleFloat(in->vecs[1][j]);
		}
		out->u_offset = LittleFloat(in->vecs[0][3]);
		out->v_offset = LittleFloat(in->vecs[1][3]);

		out->flags = LittleLong(in->surfaceFlags);

		/* exchange the textures with the ones that are needed for base assembly */
		if (refdef.mapZone && strstr(in->texture, "tex_terrain/dummy"))
			Com_sprintf(name, sizeof(name), "textures/tex_terrain/%s", refdef.mapZone);
		else
			Com_sprintf(name, sizeof(name), "textures/%s", in->texture);

		out->image = R_FindImage(name, it_world);
	}
}

/**
 * @brief Fills in s->stmins[] and s->stmaxs[]
 */
static void R_SetSurfaceExtents (mBspSurface_t *surf, const model_t* mod)
{
	vec3_t mins, maxs;
	vec2_t stmins, stmaxs;
	int i, j;
	const mBspTexInfo_t *tex;

	VectorSet(mins, 999999, 999999, 999999);
	VectorSet(maxs, -999999, -999999, -999999);

	Vector2Set(stmins, 999999, 999999);
	Vector2Set(stmaxs, -999999, -999999);

	tex = surf->texinfo;

	for (i = 0; i < surf->numedges; i++) {
		const int e = mod->bsp.surfedges[surf->firstedge + i];
		const mBspVertex_t *v;
		vec3_t position;
		if (e >= 0)
			v = &mod->bsp.vertexes[mod->bsp.edges[e].v[0]];
		else
			v = &mod->bsp.vertexes[mod->bsp.edges[-e].v[1]];

		VectorCopy(v->position, position);

		for (j = 0; j < 3; j++) {  /* calculate mins, maxs */
			position[j] += (float)shift[j];
			if (position[j] > maxs[j])
				maxs[j] = position[j];
			if (position[j] < mins[j])
				mins[j] = position[j];
		}

		{  /* calculate stmins, stmaxs */
			const float valS = DotProduct(v->position, tex->uv) + tex->u_offset;
			const float valT = DotProduct(v->position, tex->vv) + tex->v_offset;
			stmins[0] = min(valS, stmins[0]);
			stmaxs[0] = max(valS, stmaxs[0]);
			stmins[1] = min(valT, stmins[1]);
			stmaxs[1] = max(valT, stmaxs[1]);
		}
	}

	VectorCopy(mins, surf->mins);
	VectorCopy(maxs, surf->maxs);
	VectorCenterFromMinsMaxs(surf->mins, surf->maxs, surf->center);

	for (i = 0; i < 2; i++) {
		const int bmins = floor(stmins[i] / surf->lightmap_scale);
		const int bmaxs = ceil(stmaxs[i] / surf->lightmap_scale);

		surf->stmins[i] = bmins * surf->lightmap_scale;
		surf->stmaxs[i] = bmaxs * surf->lightmap_scale;

		surf->stcenter[i] = (surf->stmaxs[i] + surf->stmins[i]) / 2.0;
		surf->stextents[i] = (bmaxs - bmins) * surf->lightmap_scale;
	}
}

static void R_ModLoadSurfaces (qboolean day, const lump_t *l)
{
	const dBspSurface_t *in;
	mBspSurface_t *out;
	int count, surfnum;

	in = (const dBspSurface_t *) (mod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
		Com_Error(ERR_DROP, "R_ModLoadSurfaces: funny lump size in %s", r_worldmodel->name);
	count = l->filelen / sizeof(*in);
	out = (mBspSurface_t *)Mem_PoolAlloc(count * sizeof(*out), vid_modelPool, 0);
	Com_DPrintf(DEBUG_RENDERER, "...faces: %i\n", count);

	r_worldmodel->bsp.surfaces = out;
	r_worldmodel->bsp.numsurfaces = count;

	for (surfnum = 0; surfnum < count; surfnum++, in++, out++) {
		uint16_t planenum;
		int16_t side;
		int ti;
		int i;

		out->firstedge = LittleLong(in->firstedge);
		out->numedges = LittleShort(in->numedges);

		/* resolve plane */
		planenum = LittleShort(in->planenum);
		out->plane = r_worldmodel->bsp.planes + planenum;

		/* and sideness */
		side = LittleShort(in->side);
		if (side) {
			out->flags |= MSURF_PLANEBACK;
			VectorNegate(out->plane->normal, out->normal);
		} else {
			VectorCopy(out->plane->normal, out->normal);
		}

		ti = LittleShort(in->texinfo);
		if (ti < 0 || ti >= r_worldmodel->bsp.numtexinfo)
			Com_Error(ERR_DROP, "R_ModLoadSurfaces: bad texinfo number");
		out->texinfo = r_worldmodel->bsp.texinfo + ti;

		out->lightmap_scale = (1 << r_worldmodel->bsp.lightquant);

		/* and size, texcoords, etc */
		R_SetSurfaceExtents(out, r_worldmodel);

		if (!(out->texinfo->flags & SURF_WARP))
			out->flags |= MSURF_LIGHTMAP;

		/* lastly lighting info */
		if (day)
			i = LittleLong(in->lightofs[LIGHTMAP_DAY]);
		else
			i = LittleLong(in->lightofs[LIGHTMAP_NIGHT]);

		if (i == -1)
			out->samples = NULL;
		else
			out->samples = r_worldmodel->bsp.lightdata + i;

		/* create lightmaps */
		R_CreateSurfaceLightmap(out);

		out->tile = r_numMapTiles - 1;
	}
}

/**
 * @sa TR_BuildTracingNode_r
 * @sa R_RecurseSetParent
 */
static void R_ModLoadNodes (const lump_t *l)
{
	int i, j, count;
	const dBspNode_t *in;
	mBspNode_t *out;
	mBspNode_t *parent = NULL;

	in = (const dBspNode_t *) (mod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
		Com_Error(ERR_DROP, "R_ModLoadNodes: funny lump size in %s", r_worldmodel->name);
	count = l->filelen / sizeof(*in);
	out = (mBspNode_t *)Mem_PoolAlloc(count * sizeof(*out), vid_modelPool, 0);
	Com_DPrintf(DEBUG_RENDERER, "...nodes: %i\n", count);

	r_worldmodel->bsp.nodes = out;
	r_worldmodel->bsp.numnodes = count;

	for (i = 0; i < count; i++, in++, out++) {
		const int p = LittleLong(in->planenum);

		/* skip special pathfinding nodes - they have a negative index */
		if (p == PLANENUM_LEAF) {
			/* in case of "special" pathfinding nodes (they don't have a plane)
			 * we have to set this to NULL */
			out->plane = NULL;
			out->contents = CONTENTS_PATHFINDING_NODE;
			parent = NULL;
		} else {
			out->plane = r_worldmodel->bsp.planes + p;
			/* differentiate from leafs */
			out->contents = CONTENTS_NODE;
			parent = out;
		}

		for (j = 0; j < 3; j++) {
			out->minmaxs[j] = LittleShort(in->mins[j]) + (float)shift[j];
			out->minmaxs[3 + j] = LittleShort(in->maxs[j]) + (float)shift[j];
		}

		out->firstsurface = LittleShort(in->firstface);
		out->numsurfaces = LittleShort(in->numfaces);

		for (j = 0; j < 2; j++) {
			const int p2 = LittleLong(in->children[j]);
			if (p2 > LEAFNODE) {
				assert(p2 < r_worldmodel->bsp.numnodes);
				out->children[j] = r_worldmodel->bsp.nodes + p2;
			} else {
				assert((LEAFNODE - p2) < r_worldmodel->bsp.numleafs);
				out->children[j] = (mBspNode_t *) (r_worldmodel->bsp.leafs + (LEAFNODE - p2));
			}
			out->children[j]->parent = parent;
		}
	}
}

static void R_ModLoadLeafs (const lump_t *l)
{
	const dBspLeaf_t *in;
	mBspLeaf_t *out;
	int i, j, count;

	in = (const dBspLeaf_t *) (mod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
		Com_Error(ERR_DROP, "R_ModLoadLeafs: funny lump size in %s", r_worldmodel->name);
	count = l->filelen / sizeof(*in);
	out = (mBspLeaf_t *)Mem_PoolAlloc(count * sizeof(*out), vid_modelPool, 0);
	Com_DPrintf(DEBUG_RENDERER, "...leafs: %i\n", count);

	r_worldmodel->bsp.leafs = out;
	r_worldmodel->bsp.numleafs = count;

	for (i = 0; i < count; i++, in++, out++) {
		for (j = 0; j < 3; j++) {
			out->minmaxs[j] = LittleShort(in->mins[j]) + (float)shift[j];
			out->minmaxs[3 + j] = LittleShort(in->maxs[j]) + (float)shift[j];
		}

		out->contents = LittleLong(in->contentFlags);
	}
}

static void R_ModLoadSurfedges (const lump_t *l)
{
	int i, count;
	const int *in;
	int *out;

	in = (const int *) (mod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
		Com_Error(ERR_DROP, "R_ModLoadSurfedges: funny lump size in %s", r_worldmodel->name);
	count = l->filelen / sizeof(*in);
	if (count < 1 || count >= MAX_MAP_SURFEDGES)
		Com_Error(ERR_DROP, "R_ModLoadSurfedges: bad surfedges count in %s: %i", r_worldmodel->name, count);

	out = (int *) Mem_PoolAlloc(count * sizeof(*out), vid_modelPool, 0);
	Com_DPrintf(DEBUG_RENDERER, "...surface edges: %i\n", count);

	r_worldmodel->bsp.surfedges = out;
	r_worldmodel->bsp.numsurfedges = count;

	for (i = 0; i < count; i++)
		out[i] = LittleLong(in[i]);
}

/**
 * @sa CMod_LoadPlanes
 */
static void R_ModLoadPlanes (const lump_t *l)
{
	int i, j;
	cBspPlane_t *out;
	const dBspPlane_t *in;
	int count;

	in = (const dBspPlane_t *) (mod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
		Com_Error(ERR_DROP, "R_ModLoadPlanes: funny lump size in %s", r_worldmodel->name);
	count = l->filelen / sizeof(*in);
	out = (cBspPlane_t *)Mem_PoolAlloc(count * 2 * sizeof(*out), vid_modelPool, 0);
	Com_DPrintf(DEBUG_RENDERER, "...planes: %i\n", count);

	r_worldmodel->bsp.planes = out;
	r_worldmodel->bsp.numplanes = count;

	for (i = 0; i < count; i++, in++, out++) {
		for (j = 0; j < 3; j++)
			out->normal[j] = LittleFloat(in->normal[j]);
		out->dist = LittleFloat(in->dist);
		out->type = LittleLong(in->type);
	}
}

/**
 * @brief Shift the verts for map assemblies
 * @note This is needed because you want to place a bsp file to a given
 * position on the grid - see R_ModAddMapTile for the shift vector calculation
 * This vector differs for every map - and depends on the grid position the bsp
 * map tile is placed in the world
 * @note Call this after the buffers were generated in R_LoadBspVertexArrays
 * @sa R_LoadBspVertexArrays
 */
static void R_ModShiftTile (void)
{
	mBspVertex_t *vert;
	cBspPlane_t *plane;
	int i, j;

	/* we can't do this instantly, because of rounding errors on extents calculation */
	/* shift vertexes */
	for (i = 0, vert = r_worldmodel->bsp.vertexes; i < r_worldmodel->bsp.numvertexes; i++, vert++)
		for (j = 0; j < 3; j++)
			vert->position[j] += shift[j];

	/* shift planes */
	for (i = 0, plane = r_worldmodel->bsp.planes; i < r_worldmodel->bsp.numplanes; i++, plane++)
		for (j = 0; j < 3; j++)
			plane->dist += plane->normal[j] * shift[j];
}

/**
 * @brief Puts the map data into buffers
 * @sa R_ModAddMapTile
 * @note Shift the verts after the texcoords for diffuse and lightmap are loaded
 * @sa R_ModShiftTile
 * @todo Don't use the buffers from r_state here - they might overflow
 * @todo Decrease MAX_GL_ARRAY_LENGTH to 32768 again when this is fixed
 */
static void R_LoadBspVertexArrays (model_t *mod)
{
	int i, j;
	int vertind, coordind, tangind;
	float *vecShifted;
	float soff, toff, s, t;
	float *point, *sdir, *tdir;
	vec4_t tangent;
	vec3_t binormal;
	mBspSurface_t *surf;
	mBspVertex_t *vert;
	int vertexcount;

	vertind = coordind = tangind = vertexcount = 0;

	for (i = 0, surf = mod->bsp.surfaces; i < mod->bsp.numsurfaces; i++, surf++)
		for (j = 0; j < surf->numedges; j++)
			vertexcount++;

	surf = mod->bsp.surfaces;

	/* allocate the vertex arrays */
	mod->bsp.texcoords = (GLfloat *)Mem_PoolAlloc(vertexcount * 2 * sizeof(GLfloat), vid_modelPool, 0);
	mod->bsp.lmtexcoords = (GLfloat *)Mem_PoolAlloc(vertexcount * 2 * sizeof(GLfloat), vid_modelPool, 0);
	mod->bsp.verts = (GLfloat *)Mem_PoolAlloc(vertexcount * 3 * sizeof(GLfloat), vid_modelPool, 0);
	mod->bsp.normals = (GLfloat *)Mem_PoolAlloc(vertexcount * 3 * sizeof(GLfloat), vid_modelPool, 0);
	mod->bsp.tangents = (GLfloat *)Mem_PoolAlloc(vertexcount * 4 * sizeof(GLfloat), vid_modelPool, 0);

	for (i = 0; i < mod->bsp.numsurfaces; i++, surf++) {
		surf->index = vertind / 3;

		for (j = 0; j < surf->numedges; j++) {
			const float *normal;
			const int index = mod->bsp.surfedges[surf->firstedge + j];

			/* vertex */
			if (index > 0) {  /* negative indices to differentiate which end of the edge */
				const mBspEdge_t *edge = &mod->bsp.edges[index];
				vert = &mod->bsp.vertexes[edge->v[0]];
			} else {
				const mBspEdge_t *edge = &mod->bsp.edges[-index];
				vert = &mod->bsp.vertexes[edge->v[1]];
			}

			point = vert->position;

			/* shift it for assembled maps */
			vecShifted = &mod->bsp.verts[vertind];
			/* origin (func_door, func_rotating) bmodels must not have shifted vertices,
			 * they are translated by their entity origin value */
			if (surf->isOriginBrushModel)
				VectorCopy(point, vecShifted);
			else
				VectorAdd(point, shift, vecShifted);

			/* texture directional vectors and offsets */
			sdir = surf->texinfo->uv;
			soff = surf->texinfo->u_offset;

			tdir = surf->texinfo->vv;
			toff = surf->texinfo->v_offset;

			/* texture coordinates */
			s = DotProduct(point, sdir) + soff;
			s /= surf->texinfo->image->width;

			t = DotProduct(point, tdir) + toff;
			t /= surf->texinfo->image->height;

			mod->bsp.texcoords[coordind + 0] = s;
			mod->bsp.texcoords[coordind + 1] = t;

			if (surf->flags & MSURF_LIGHTMAP) {  /* lightmap coordinates */
				s = DotProduct(point, sdir) + soff;
				s -= surf->stmins[0];
				s += surf->light_s * surf->lightmap_scale;
				s += surf->lightmap_scale / 2.0;
				s /= r_lightmaps.size * surf->lightmap_scale;

				t = DotProduct(point, tdir) + toff;
				t -= surf->stmins[1];
				t += surf->light_t * surf->lightmap_scale;
				t += surf->lightmap_scale / 2.0;
				t /= r_lightmaps.size * surf->lightmap_scale;
			}

			mod->bsp.lmtexcoords[coordind + 0] = s;
			mod->bsp.lmtexcoords[coordind + 1] = t;

			/* normal vectors */
			if ((surf->texinfo->flags & SURF_PHONG) && VectorNotEmpty(vert->normal))
				normal = vert->normal; /* phong shaded */
			else
				normal = surf->normal; /* per plane */

			memcpy(&mod->bsp.normals[vertind], normal, sizeof(vec3_t));

			/* tangent vector */
			TangentVectors(normal, sdir, tdir, tangent, binormal);
			memcpy(&mod->bsp.tangents[tangind], tangent, sizeof(vec4_t));

			vertind += 3;
			coordind += 2;
			tangind += 4;
		}
	}

	R_ReallocateStateArrays(vertind / 3);

	if (qglBindBuffer) {
		/* and also the vertex buffer objects */
		qglGenBuffers(1, &mod->bsp.vertex_buffer);
		qglBindBuffer(GL_ARRAY_BUFFER, mod->bsp.vertex_buffer);
		qglBufferData(GL_ARRAY_BUFFER, vertind * sizeof(GLfloat), mod->bsp.verts, GL_STATIC_DRAW);

		qglGenBuffers(1, &mod->bsp.texcoord_buffer);
		qglBindBuffer(GL_ARRAY_BUFFER, mod->bsp.texcoord_buffer);
		qglBufferData(GL_ARRAY_BUFFER, coordind * sizeof(GLfloat), mod->bsp.texcoords, GL_STATIC_DRAW);

		qglGenBuffers(1, &mod->bsp.lmtexcoord_buffer);
		qglBindBuffer(GL_ARRAY_BUFFER, mod->bsp.lmtexcoord_buffer);
		qglBufferData(GL_ARRAY_BUFFER, coordind * sizeof(GLfloat), mod->bsp.lmtexcoords, GL_STATIC_DRAW);

		qglGenBuffers(1, &mod->bsp.normal_buffer);
		qglBindBuffer(GL_ARRAY_BUFFER, mod->bsp.normal_buffer);
		qglBufferData(GL_ARRAY_BUFFER, vertind * sizeof(GLfloat), mod->bsp.normals, GL_STATIC_DRAW);

		qglGenBuffers(1, &mod->bsp.tangent_buffer);
		qglBindBuffer(GL_ARRAY_BUFFER, mod->bsp.tangent_buffer);
		qglBufferData(GL_ARRAY_BUFFER, tangind * sizeof(GLfloat), mod->bsp.tangents, GL_STATIC_DRAW);

		qglBindBuffer(GL_ARRAY_BUFFER, 0);
	}
}

static void R_SortSurfacesArrays_ (mBspSurfaces_t *surfs, mBspSurfaces_t **r_sorted_surfaces)
{
	int i, j;

	for (i = 0; i < surfs->count; i++) {
		const int texindex = R_GetImageIndex(surfs->surfaces[i]->texinfo->image);
		if (texindex < 0 || texindex >= MAX_GL_TEXTURES)
			Com_Error(ERR_FATAL, "R_SortSurfacesArrays: bogus image pointer");
		R_AddSurfaceToArray(r_sorted_surfaces[texindex], surfs->surfaces[i]);
	}

	surfs->count = 0;

	for (i = 0; i < r_numImages; i++) {
		mBspSurfaces_t *sorted = r_sorted_surfaces[i];
		if (sorted && sorted->count) {
			for (j = 0; j < sorted->count; j++)
				R_AddSurfaceToArray(surfs, sorted->surfaces[j]);

			sorted->count = 0;
		}
	}
}

/**
 * @brief Reorders all surfaces arrays for the specified model, grouping the surface
 * pointers by texture.  This dramatically reduces glBindTexture calls.
 */
static void R_SortSurfacesArrays (const model_t *mod)
{
	const mBspSurface_t *surf, *s;
	int i, ns;
	mBspSurfaces_t **r_sorted_surfaces = (mBspSurfaces_t **) Mem_Alloc(r_numImages * sizeof(mBspSurfaces_t *));

	/* resolve the start surface and total surface count */
	s = &mod->bsp.surfaces[mod->bsp.firstmodelsurface];
	ns = mod->bsp.nummodelsurfaces;

	/* allocate the per-texture surfaces arrays and determine counts */
	for (i = 0, surf = s; i < ns; i++, surf++) {
		int index = R_GetImageIndex(surf->texinfo->image);
		mBspSurfaces_t *surfs = r_sorted_surfaces[index];
		if (!surfs) {  /* allocate it */
			surfs = (mBspSurfaces_t *)Mem_PoolAlloc(sizeof(*surfs), vid_modelPool, 0);
			r_sorted_surfaces[index] = surfs;
		}

		surfs->count++;
	}

	/* allocate the surfaces pointers based on counts */
	for (i = 0; i < r_numImages; i++) {
		mBspSurfaces_t *surfs = r_sorted_surfaces[i];
		if (surfs) {
			surfs->surfaces = (mBspSurface_t **)Mem_PoolAlloc(sizeof(mBspSurface_t *) * surfs->count, vid_modelPool, 0);
			surfs->count = 0;
		}
	}

	/* sort the model's surfaces arrays into the per-texture arrays */
	for (i = 0; i < NUM_SURFACES_ARRAYS; i++) {
		if (mod->bsp.sorted_surfaces[i]->count) {
			R_SortSurfacesArrays_(mod->bsp.sorted_surfaces[i], r_sorted_surfaces);
			Com_DPrintf(DEBUG_RENDERER, "%i: #%i surfaces\n", i, mod->bsp.sorted_surfaces[i]->count);
		}
	}

	/* free the per-texture surfaces arrays */
	for (i = 0; i < r_numImages; i++) {
		mBspSurfaces_t *surfs = r_sorted_surfaces[i];
		if (surfs) {
			if (surfs->surfaces)
				Mem_Free(surfs->surfaces);
			Mem_Free(surfs);
		}
	}

	Mem_Free(r_sorted_surfaces);
}

static void R_LoadSurfacesArrays_ (model_t *mod)
{
	mBspSurface_t *surf, *s;
	int i, ns;

	/* allocate the surfaces array structures */
	/** @todo only one allocation should be used here - allocate the whole array with one Mem_PoolAlloc call */
	for (i = 0; i < NUM_SURFACES_ARRAYS; i++)
		mod->bsp.sorted_surfaces[i] = (mBspSurfaces_t *)Mem_PoolAlloc(sizeof(mBspSurfaces_t), vid_modelPool, 0);

	/* resolve the start surface and total surface count */
	s = &mod->bsp.surfaces[mod->bsp.firstmodelsurface];
	ns = mod->bsp.nummodelsurfaces;

	/* determine the maximum counts for each rendered type in order to
	 * allocate only what is necessary for the specified model */
	for (i = 0, surf = s; i < ns; i++, surf++) {
		const mBspTexInfo_t *texinfo = surf->texinfo;
		const material_t *material = &texinfo->image->material;
		if (texinfo->flags & (SURF_BLEND33 | SURF_BLEND66)) {
			if (texinfo->flags & SURF_WARP)
				mod->bsp.blend_warp_surfaces->count++;
			else
				mod->bsp.blend_surfaces->count++;
		} else {
			if (texinfo->flags & SURF_WARP)
				mod->bsp.opaque_warp_surfaces->count++;
			else if (texinfo->flags & SURF_ALPHATEST)
				mod->bsp.alpha_test_surfaces->count++;
			else
				mod->bsp.opaque_surfaces->count++;
		}

		if (material->flags & STAGE_RENDER)
			mod->bsp.material_surfaces->count++;

		if (material->flags & STAGE_FLARE)
			mod->bsp.flare_surfaces->count++;
	}

	/* allocate the surfaces pointers based on the counts */
	for (i = 0; i < NUM_SURFACES_ARRAYS; i++) {
		mBspSurfaces_t *surfaces = mod->bsp.sorted_surfaces[i];
		if (surfaces->count) {
			surfaces->surfaces = (mBspSurface_t **)Mem_PoolAlloc(sizeof(*surfaces) * surfaces->count, vid_modelPool, 0);
			surfaces->count = 0;
		}
	}

	/* iterate the surfaces again, populating the allocated arrays based
	 * on primary render type */
	for (i = 0, surf = s; i < ns; i++, surf++) {
		const mBspTexInfo_t *texinfo = surf->texinfo;
		const material_t *material = &texinfo->image->material;
		if (texinfo->flags & (SURF_BLEND33 | SURF_BLEND66)) {
			if (texinfo->flags & SURF_WARP)
				R_AddSurfaceToArray(mod->bsp.blend_warp_surfaces, surf);
			else
				R_AddSurfaceToArray(mod->bsp.blend_surfaces, surf);
		} else {
			if (texinfo->flags & SURF_WARP)
				R_AddSurfaceToArray(mod->bsp.opaque_warp_surfaces, surf);
			else if (texinfo->flags & SURF_ALPHATEST)
				R_AddSurfaceToArray(mod->bsp.alpha_test_surfaces, surf);
			else
				R_AddSurfaceToArray(mod->bsp.opaque_surfaces, surf);
		}

		if (material->flags & STAGE_RENDER)
			R_AddSurfaceToArray(mod->bsp.material_surfaces, surf);

		if (material->flags & STAGE_FLARE)
			R_AddSurfaceToArray(mod->bsp.flare_surfaces, surf);
	}

	/* now sort them by texture */
	R_SortSurfacesArrays(mod);
}

static void R_LoadSurfacesArrays (void)
{
	int i;

	for (i = 0; i < r_numMapTiles; i++)
		R_LoadSurfacesArrays_(r_mapTiles[i]);

	for (i = 0; i < r_numModelsInline; i++)
		R_LoadSurfacesArrays_(&r_modelsInline[i]);
}

/**
 * @sa R_SetParent
 */
static void R_SetModel (mBspNode_t *node, model_t *mod)
{
	node->model = mod;

	if (node->contents > CONTENTS_NODE)
		return;

	R_SetModel(node->children[0], mod);
	R_SetModel(node->children[1], mod);
}


/**
 * @sa R_RecurseSetParent
 */
static void R_RecursiveSetModel (mBspNode_t *node, model_t *mod)
{
	/* skip special pathfinding nodes */
	if (node->contents == CONTENTS_PATHFINDING_NODE) {
		R_RecursiveSetModel(node->children[0], mod);
		R_RecursiveSetModel(node->children[1], mod);
	} else {
		R_SetModel(node, mod);
	}
}

/**
 * @brief Sets up bmodels (brush models) like doors and breakable objects
 */
static void R_SetupSubmodels (void)
{
	int i, j;

	/* set up the submodels, the first 255 submodels are the models of the
	 * different levels, don't care about them */
	for (i = NUM_REGULAR_MODELS; i < r_worldmodel->bsp.numsubmodels; i++) {
		model_t *mod = &r_modelsInline[r_numModelsInline];
		const mBspHeader_t *sub = &r_worldmodel->bsp.submodels[i];

		/* copy most info from world */
		*mod = *r_worldmodel;
		mod->type = mod_bsp_submodel;

		Com_sprintf(mod->name, sizeof(mod->name), "*%d", i);

		/* copy the rest from the submodel */
		VectorCopy(sub->maxs, mod->maxs);
		VectorCopy(sub->mins, mod->mins);
		mod->radius = sub->radius;

		mod->bsp.firstnode = sub->headnode;
		mod->bsp.nodes = &r_worldmodel->bsp.nodes[mod->bsp.firstnode];
		mod->bsp.maptile = r_numMapTiles - 1;
		if (mod->bsp.firstnode >= r_worldmodel->bsp.numnodes)
			Com_Error(ERR_DROP, "R_SetupSubmodels: Inline model %i has bad firstnode", i);

		R_RecursiveSetModel(mod->bsp.nodes, mod);

		mod->bsp.firstmodelsurface = sub->firstface;
		mod->bsp.nummodelsurfaces = sub->numfaces;

		/* submodel vertices of the surfaces must not be shifted in case of rmas */
		for (j = mod->bsp.firstmodelsurface; j < mod->bsp.firstmodelsurface + mod->bsp.nummodelsurfaces; j++) {
			mod->bsp.surfaces[j].isOriginBrushModel = (mod->bsp.surfaces[j].texinfo->flags & SURF_ORIGIN);
		}

		/*mod->bsp.numleafs = sub->visleafs;*/
		r_numModelsInline++;
	}
}

/**
 @brief Sets up surface range for the world model
 @note Depends on ufo2map to store all world model surfaces before submodel ones
 */
static void R_SetupWorldModel (void)
{
	int surfCount = r_worldmodel->bsp.numsurfaces;
	/* first NUM_REGULAR_MODELS submodels are the models of the different levels, don't care about them */
	int i = NUM_REGULAR_MODELS;

#ifdef DEBUG
	/* validate surface allocation by submodels by checking the surface array range they are using */
	/* start with inverted range to simplify code */
	int first = surfCount, last = -1; /* @note range is [,) */
	for (; i < r_worldmodel->bsp.numsubmodels; i++) {
		const mBspHeader_t *sub = &r_worldmodel->bsp.submodels[i];
		int firstFace = sub->firstface;
		int lastFace = firstFace + sub->numfaces;

		if (lastFace <= firstFace)
			continue; /* empty submodel, ignore it */

		if (last >= 0 && (firstFace > last || lastFace < first))
			Com_Printf("Warning: submodels may not combine to contigous range in the surface array, some world geometry could be missing as a result (submodel %i)\n", i);

		if (firstFace < first)
			first = firstFace;

		if (lastFace > last)
			last = lastFace;

		Com_DPrintf(DEBUG_RENDERER, "Submodel %i, range %i..%i\n", i, firstFace, lastFace - 1);
	}
	surfCount = first;

	if (last >= 0 && last != r_worldmodel->bsp.numsurfaces)
		Com_Printf("Warning: %i surfaces are lost, some world geometry could be missing as a result\n", r_worldmodel->bsp.numsurfaces - last);
#else
	/* take a shortcut: assume that first submodel surfaces begin exactly after world model ones */
	if (i < r_worldmodel->bsp.numsubmodels)
		surfCount = r_worldmodel->bsp.submodels[i].firstface;
#endif
	Com_DPrintf(DEBUG_RENDERER, "World model, range 0..%i\n", surfCount - 1);

	r_worldmodel->bsp.firstmodelsurface = 0;
	r_worldmodel->bsp.nummodelsurfaces = surfCount;
}

/**
 * @sa CM_AddMapTile
 * @sa R_ModBeginLoading
 * @param[in] name The name of the map. Relative to maps/ and without extension
 * @param[in] day Load the day lightmap
 * @param[in] sX Shift x grid units
 * @param[in] sY Shift y grid units
 * @param[in] sZ Shift z grid units
 * @sa UNIT_SIZE
 */
static void R_ModAddMapTile (const char *name, qboolean day, int sX, int sY, int sZ)
{
	int i;
	byte *buffer;
	dBspHeader_t *header;
	const int lightingLump = day ? LUMP_LIGHTING_DAY : LUMP_LIGHTING_NIGHT;

	if (r_numMapTiles < 0 || r_numMapTiles >= MAX_MAPTILES)
		Com_Error(ERR_DROP, "R_ModAddMapTile: Too many map tiles");

	/* alloc model and tile */
	r_worldmodel = R_AllocModelSlot();
	r_mapTiles[r_numMapTiles++] = r_worldmodel;
	OBJZERO(*r_worldmodel);
	Com_sprintf(r_worldmodel->name, sizeof(r_worldmodel->name), "maps/%s.bsp", name);

	/* load the file */
	FS_LoadFile(r_worldmodel->name, &buffer);
	if (!buffer)
		Com_Error(ERR_DROP, "R_ModAddMapTile: %s not found", r_worldmodel->name);

	/* init */
	r_worldmodel->type = mod_bsp;
	r_worldmodel->bsp.maptile = r_numMapTiles - 1;

	/* prepare shifting */
	VectorSet(shift, sX * UNIT_SIZE, sY * UNIT_SIZE, sZ * UNIT_SIZE);

	/* test version */
	header = (dBspHeader_t *) buffer;
	i = LittleLong(header->version);
	if (i != BSPVERSION)
		Com_Error(ERR_DROP, "R_ModAddMapTile: %s has wrong version number (%i should be %i)", r_worldmodel->name, i, BSPVERSION);

	/* swap all the lumps */
	mod_base = (byte *) header;

	BSP_SwapHeader(header, r_worldmodel->name);

	/* load into heap */
	R_ModLoadVertexes(&header->lumps[LUMP_VERTEXES]);
	R_ModLoadNormals(&header->lumps[LUMP_NORMALS]);
	R_ModLoadEdges(&header->lumps[LUMP_EDGES]);
	R_ModLoadSurfedges(&header->lumps[LUMP_SURFEDGES]);
	R_ModLoadLighting(&header->lumps[lightingLump]);
	R_ModLoadPlanes(&header->lumps[LUMP_PLANES]);
	R_ModLoadTexinfo(&header->lumps[LUMP_TEXINFO]);
	R_ModLoadSurfaces(day, &header->lumps[LUMP_FACES]);
	R_ModLoadLeafs(&header->lumps[LUMP_LEAFS]);
	R_ModLoadNodes(&header->lumps[LUMP_NODES]);
	R_ModLoadSubmodels(&header->lumps[LUMP_MODELS]);

	R_SetupSubmodels();
	R_SetupWorldModel();

	R_LoadBspVertexArrays(r_worldmodel);

	/* in case of random map assembly shift some vectors */
	if (VectorNotEmpty(shift))
		R_ModShiftTile();

	FS_FreeFile(buffer);
}

static void R_ModEndLoading (const char *mapName)
{
	R_EndBuildingLightmaps();
	R_LoadMaterials(mapName);
	R_LoadSurfacesArrays();
}

/**
 * @brief Specifies the model that will be used as the world
 * @param[in] tiles The tiles string can be only one map or a list of space
 * seperated map tiles for random assembly. In case of random assembly we also
 * need the @c pos string. Every tile needs an entry in the @c pos string, too.
 * @param[in] day Load the day lightmap
 * @param[in] pos In case of a random map assembly this is the string that holds
 * the world grid positions of the tiles. The positions are x, y and z values.
 * They are just written one after another for every tile in the @c tiles string
 * and every of the three components must exists for every tile.
 * @param[in] mapName The mapname that the get from the server (used to identify
 * the correct name for the materials in case of a random assembly).
 * @sa R_ModAddMapTile
 * @sa CM_LoadMap
 * @note This function is called for listen servers, too. This loads the bsp
 * struct for rendering it. The @c CM_LoadMap code only loads the collision
 * and pathfinding stuff.
 * @sa UI_BuildRadarImageList
 */
void R_ModBeginLoading (const char *tiles, qboolean day, const char *pos, const char *mapName)
{
	char name[MAX_VAR];
	char base[MAX_QPATH];
	ipos3_t sh;
	int i;

	assert(mapName);

	/* clear any lights leftover in the active list from previous maps */
	R_ClearStaticLights();

	/* init */
	R_BeginBuildingLightmaps();
	r_numModelsInline = 0;
	r_numMapTiles = 0;

	/* load tiles */
	while (tiles) {
		/* get tile name */
		const char *token = Com_Parse(&tiles);
		if (!tiles) {
			/* finish */
			R_ModEndLoading(mapName);
			return;
		}

		/* get base path */
		if (token[0] == '-') {
			Q_strncpyz(base, token + 1, sizeof(base));
			continue;
		}

		/* get tile name */
		if (token[0] == '+')
			Com_sprintf(name, sizeof(name), "%s%s", base, token + 1);
		else
			Q_strncpyz(name, token, sizeof(name));

		if (pos && pos[0]) {
			/* get grid position and add a tile */
			for (i = 0; i < 3; i++) {
				token = Com_Parse(&pos);
				if (!pos)
					Com_Error(ERR_DROP, "R_ModBeginLoading: invalid positions\n");
				sh[i] = atoi(token);
			}
			if (sh[0] <= -(PATHFINDING_WIDTH / 2) || sh[0] >= PATHFINDING_WIDTH / 2)
				Com_Error(ERR_DROP, "R_ModBeginLoading: invalid x position given: %i\n", sh[0]);
			if (sh[1] <= -(PATHFINDING_WIDTH / 2) || sh[1] >= PATHFINDING_WIDTH / 2)
				Com_Error(ERR_DROP, "R_ModBeginLoading: invalid y position given: %i\n", sh[1]);
			if (sh[2] >= PATHFINDING_HEIGHT)
				Com_Error(ERR_DROP, "R_ModBeginLoading: invalid z position given: %i\n", sh[2]);
			R_ModAddMapTile(name, day, sh[0], sh[1], sh[2]);
		} else {
			/* load only a single tile, if no positions are specified */
			R_ModAddMapTile(name, day, 0, 0, 0);
			R_ModEndLoading(mapName);
			return;
		}
	}

	Com_Error(ERR_DROP, "R_ModBeginLoading: invalid tile names\n");
}

/**
 * @todo fix this for threaded renderer mode
 */
void R_ModReloadSurfacesArrays (void)
{
	int i, j;

	for (i = 0; i < r_numMapTiles; i++) {
		model_t *mod = r_mapTiles[i];
		const size_t size = lengthof(mod->bsp.sorted_surfaces);
		for (j = 0; j < size; j++)
			if (mod->bsp.sorted_surfaces[j]) {
				Mem_Free(mod->bsp.sorted_surfaces[j]);
				mod->bsp.sorted_surfaces[j] = NULL;
			}
	}
	R_LoadSurfacesArrays();
}
