/**
 * @file r_lightmap.c
 * In video memory, lightmaps are chunked into NxN RGBA blocks.
 * In the bsp, they are just RGB, and we retrieve them using floating point for precision.
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
#include "r_error.h"
#include "r_entity.h"
#include "r_lightmap.h"

lightmaps_t r_lightmaps;

static void R_UploadLightmapBlock (void)
{
	if (r_lightmaps.lightmap_texnum == MAX_GL_LIGHTMAPS) {
		Com_Printf("R_UploadLightmapBlock: MAX_GL_LIGHTMAPS reached.\n");
		return;
	}

	R_BindTexture(r_lightmaps.lightmap_texnum);

	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, r_lightmaps.size, r_lightmaps.size,
		0, GL_RGB, GL_UNSIGNED_BYTE, r_lightmaps.sample_buffer);

	R_CheckError();

	r_lightmaps.lightmap_texnum++;

	if (r_lightmaps.deluxemap_texnum == MAX_GL_DELUXEMAPS) {
		Com_Printf("R_UploadLightmapBlock: MAX_GL_DELUXEMAPS reached.\n");
		return;
	}

	R_BindTexture(r_lightmaps.deluxemap_texnum);

	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, r_lightmaps.size, r_lightmaps.size,
		0, GL_RGB, GL_UNSIGNED_BYTE, r_lightmaps.direction_buffer);

	r_lightmaps.deluxemap_texnum++;

	/* clear the allocation block and buffers */
	memset(r_lightmaps.allocated, 0, r_lightmaps.size * sizeof(unsigned));
	memset(r_lightmaps.sample_buffer, 0, r_lightmaps.size * r_lightmaps.size * sizeof(unsigned));
	memset(r_lightmaps.direction_buffer, 0, r_lightmaps.size * r_lightmaps.size * sizeof(unsigned));
}

/**
 * @brief returns a texture number and the position inside it
 */
static qboolean R_AllocLightmapBlock (int w, int h, int *x, int *y)
{
	int i, j;
	int best;

	best = r_lightmaps.size;

	for (i = 0; i < r_lightmaps.size - w; i++) {
		int best2 = 0;

		for (j = 0; j < w; j++) {
			if (r_lightmaps.allocated[i + j] >= best)
				break;
			if (r_lightmaps.allocated[i + j] > best2)
				best2 = r_lightmaps.allocated[i + j];
		}
		/* this is a valid spot */
		if (j == w) {
			*x = i;
			*y = best = best2;
		}
	}

	if (best + h > r_lightmaps.size)
		return qfalse;

	for (i = 0; i < w; i++)
		r_lightmaps.allocated[*x + i] = best + h;

	return qtrue;
}

/**
 * @brief Fullbridght lightmap
 * @sa R_BuildLightmap
 */
static void R_BuildDefaultLightmap (mBspSurface_t *surf, byte *sout, byte *dout, int stride)
{
	int i, j;

	const int smax = (surf->stextents[0] / surf->lightmap_scale) + 1;
	const int tmax = (surf->stextents[1] / surf->lightmap_scale) + 1;

	/* this works because the block bytes for the deluxemap are the same as for the lightmap */
	stride -= (smax * LIGHTMAP_BLOCK_BYTES);

	for (i = 0; i < tmax; i++, sout += stride, dout += stride) {
		for (j = 0; j < smax; j++) {
			sout[0] = 255;
			sout[1] = 255;
			sout[2] = 255;

			sout += LIGHTMAP_BLOCK_BYTES;

			dout[0] = 127;
			dout[1] = 127;
			dout[2] = 255;

			dout += DELUXEMAP_BLOCK_BYTES;
		}
	}

	Vector4Set(surf->color, 1.0, 1.0, 1.0, 1.0);
}

/**
 * @brief Consume raw lightmap and deluxemap RGB/XYZ data from the surface samples,
 * writing processed lightmap and deluxemap RGBA to the specified destinations.
 * @sa R_BuildDefaultLightmap
 */
static void R_BuildLightmap (mBspSurface_t *surf, byte *sout, byte *dout, int stride)
{
	unsigned int i, j;
	byte *lightmap, *lm, *l, *deluxemap, *dm;

	const int smax = (surf->stextents[0] / surf->lightmap_scale) + 1;
	const int tmax = (surf->stextents[1] / surf->lightmap_scale) + 1;
	const int size = smax * tmax;
	stride -= (smax * LIGHTMAP_BLOCK_BYTES);

	lightmap = (byte *)Mem_PoolAlloc(size * LIGHTMAP_BLOCK_BYTES, vid_lightPool, 0);
	lm = lightmap;

	deluxemap = (byte *)Mem_PoolAlloc(size * DELUXEMAP_BLOCK_BYTES, vid_lightPool, 0);
	dm = deluxemap;

	/* convert the raw lightmap samples to floating point and scale them */
	for (i = j = 0; i < size; i++, lm += LIGHTMAP_BLOCK_BYTES, dm += DELUXEMAP_BLOCK_BYTES) {
		lm[0] = surf->samples[j++];
		lm[1] = surf->samples[j++];
		lm[2] = surf->samples[j++];

		/* read in directional samples for deluxe mapping as well */
		dm[0] = surf->samples[j++];
		dm[1] = surf->samples[j++];
		dm[2] = surf->samples[j++];
	}

	/* apply modulate, contrast, resolve average surface color, etc.. */
	R_FilterTexture(lightmap, smax, tmax, it_lightmap, LIGHTMAP_BLOCK_BYTES);

	if (surf->texinfo->flags & (SURF_BLEND33 | SURF_ALPHATEST))
		surf->color[3] = 0.25;
	else if (surf->texinfo->flags & SURF_BLEND66)
		surf->color[3] = 0.50;
	else
		surf->color[3] = 1.0;

	/* soften it if it's sufficiently large */
	if (r_soften->integer && size > 128)
		for (i = 0; i < 4; i++) {
			R_SoftenTexture(lightmap, smax, tmax, LIGHTMAP_BLOCK_BYTES);
			R_SoftenTexture(deluxemap, smax, tmax, DELUXEMAP_BLOCK_BYTES);
		}

	/* the final lightmap is uploaded to the card via the strided lightmap
	 * block, and also cached on the surface for fast point lighting lookups */

	surf->lightmap = (byte *)Mem_PoolAlloc(size * LIGHTMAP_BYTES, vid_lightPool, 0);
	l = surf->lightmap;
	lm = lightmap;
	dm = deluxemap;

	for (i = 0; i < tmax; i++, sout += stride, dout += stride) {
		for (j = 0; j < smax; j++) {
			/* copy the lightmap to the strided block */
			sout[0] = lm[0];
			sout[1] = lm[1];
			sout[2] = lm[2];
			sout += LIGHTMAP_BLOCK_BYTES;

			/* and to the surface, discarding alpha */
			l[0] = lm[0];
			l[1] = lm[1];
			l[2] = lm[2];
			l += LIGHTMAP_BYTES;

			lm += LIGHTMAP_BLOCK_BYTES;

			/* lastly copy the deluxemap to the strided block */
			dout[0] = dm[0];
			dout[1] = dm[1];
			dout[2] = dm[2];
			dout += DELUXEMAP_BLOCK_BYTES;

			dm += DELUXEMAP_BLOCK_BYTES;
		}
	}

	Mem_Free(lightmap);
	Mem_Free(deluxemap);
}

/**
 * @sa R_ModLoadSurfaces
 */
void R_CreateSurfaceLightmap (mBspSurface_t * surf)
{
	int smax, tmax;
	byte *samples, *directions;

	if (!(surf->flags & MSURF_LIGHTMAP))
		return;

	smax = (surf->stextents[0] / surf->lightmap_scale) + 1;
	tmax = (surf->stextents[1] / surf->lightmap_scale) + 1;

	if (!R_AllocLightmapBlock(smax, tmax, &surf->light_s, &surf->light_t)) {
		/* upload the last block */
		R_UploadLightmapBlock();
		if (!R_AllocLightmapBlock(smax, tmax, &surf->light_s, &surf->light_t))
			Com_Error(ERR_DROP, "R_CreateSurfaceLightmap: Consecutive calls to R_AllocLightmapBlock(%d,%d) failed (lightmap_scale: %i, stextends: %f %f)\n",
					smax, tmax, surf->lightmap_scale, surf->stextents[0], surf->stextents[1]);
	}

	surf->lightmap_texnum = r_lightmaps.lightmap_texnum;
	surf->deluxemap_texnum = r_lightmaps.deluxemap_texnum;

	samples = r_lightmaps.sample_buffer;
	samples += (surf->light_t * r_lightmaps.size + surf->light_s) * LIGHTMAP_BLOCK_BYTES;

	directions = r_lightmaps.direction_buffer;
	directions += (surf->light_t * r_lightmaps.size + surf->light_s) * DELUXEMAP_BLOCK_BYTES;

	if (!surf->samples)  /* make it fullbright */
		R_BuildDefaultLightmap(surf, samples, directions, r_lightmaps.size * LIGHTMAP_BLOCK_BYTES);
	else  /* or light it properly */
		R_BuildLightmap(surf, samples, directions, r_lightmaps.size * LIGHTMAP_BLOCK_BYTES);
}

/**
 * @sa R_ModBeginLoading
 * @sa R_EndBuildingLightmaps
 */
void R_BeginBuildingLightmaps (void)
{
	/* users can tune lightmap size for their card */
	r_lightmaps.size = r_maxlightmap->integer;

	r_lightmaps.allocated = (unsigned *)Mem_PoolAlloc(r_lightmaps.size *
		sizeof(unsigned), vid_lightPool, 0);

	r_lightmaps.sample_buffer = (byte *)Mem_PoolAlloc(
		r_lightmaps.size * r_lightmaps.size * sizeof(unsigned), vid_lightPool, 0);

	r_lightmaps.direction_buffer = (byte *)Mem_PoolAlloc(
		r_lightmaps.size * r_lightmaps.size * sizeof(unsigned), vid_lightPool, 0);

	r_lightmaps.lightmap_texnum = TEXNUM_LIGHTMAPS;
	r_lightmaps.deluxemap_texnum = TEXNUM_DELUXEMAPS;
}

/**
 * @sa R_BeginBuildingLightmaps
 * @sa R_ModBeginLoading
 */
void R_EndBuildingLightmaps (void)
{
	/* upload the pending lightmap block */
	R_UploadLightmapBlock();
}


/**
 * @brief Moves the given mins/maxs volume through the world from start to end.
 * @param[in] start Start vector to start the trace from
 * @param[in] end End vector to stop the trace at
 * @param[in] size Bounding box size used for tracing
 * @param[in] contentmask Searched content the trace should watch for
 */
static void R_Trace (vec3_t start, vec3_t end, float size, int contentmask)
{
	vec3_t mins, maxs;
	float frac;
	trace_t tr;
	int i;

	r_locals.tracenum++;

	if (r_locals.tracenum > 0xffff)  /* avoid overflows */
		r_locals.tracenum = 0;

	VectorSet(mins, -size, -size, -size);
	VectorSet(maxs, size, size, size);

	refdef.trace = TR_CompleteBoxTrace(start, end, mins, maxs, TRACING_ALL_VISIBLE_LEVELS, contentmask, 0);
	refdef.traceEntity = NULL;

	frac = refdef.trace.fraction;

	/* check bsp models */
	for (i = 0; i < r_numEntities; i++) {
		entity_t *ent = R_GetEntity(i);
		const model_t *m = ent->model;

		if (!m || m->type != mod_bsp_submodel)
			continue;

		tr = TR_TransformedBoxTrace(&mapTiles[m->bsp.maptile], start, end, mins, maxs, m->bsp.firstnode,
				contentmask, 0, ent->origin, ent->angles);

		if (tr.fraction < frac) {
			refdef.trace = tr;
			refdef.traceEntity = ent;

			frac = tr.fraction;
		}
	}

	assert(refdef.trace.mapTile >= 0);
	assert(refdef.trace.mapTile < r_numMapTiles);
}


/**
 * @brief Clip to all surfaces within the specified range, accumulating static lighting
 * color to the specified vector in the event of an intersection.
 * @sa R_RecurseSetParent
 */
static qboolean R_LightPointColor_ (const int tile, const int firstsurface, const int numsurfaces, const vec3_t point, vec3_t color)
{
	mBspSurface_t *surf;
	int i, width, sample;
	float s, t;

	/* resolve the surface that was impacted */
	surf = r_mapTiles[tile]->bsp.surfaces + firstsurface;

	for (i = 0; i < numsurfaces; i++, surf++) {
		const mBspTexInfo_t *tex = surf->texinfo;

		if (!(surf->flags & MSURF_LIGHTMAP))
			continue;	/* no lightmap */

#if 0
		/** @todo Texture names don't match - because image_t holds the full path */
		if (strcmp(refdef.trace.surface->name, tex->image->name))
			continue;	/* wrong material */
#endif

		if (!VectorCompare(refdef.trace.plane.normal, surf->plane->normal))
			continue;	/* facing the wrong way */

		if (surf->tracenum == r_locals.tracenum)
			continue;	/* already checked this trace */

		surf->tracenum = r_locals.tracenum;

		s = DotProduct(point, tex->uv) + tex->u_offset - surf->stmins[0];
		t = DotProduct(point, tex->vv) + tex->v_offset - surf->stmins[1];

		if ((s < 0.0 || s > surf->stextents[0]) || (t < 0.0 || t > surf->stextents[1]))
			continue;

		/* we've hit, resolve the texture coordinates */
		s /= surf->lightmap_scale;
		t /= surf->lightmap_scale;

		/* resolve the lightmap at intersection */
		width = (surf->stextents[0] / surf->lightmap_scale) + 1;
		sample = (int)(3 * ((int)t * width + (int)s));

		/* and convert it to floating point */
		VectorScale((&surf->lightmap[sample]), 1.0 / 255.0, color);
		return qtrue;
	}

	return qfalse;
}

#define STATIC_LIGHTING_MIN_COMPONENT 0.05
#define STATIC_LIGHTING_MIN_SUM 0.75

/*
 * R_LightPointClamp
 *
 * Clamps and scales the newly resolved sample color into an acceptable range.
 */
static void R_LightPointClamp (static_lighting_t *lighting)
{
	qboolean clamped;
	float f;
	int i;

	clamped = qfalse;

	for (i = 0; i < 3; i++) {  /* clamp it */
		if (lighting->colors[0][i] < STATIC_LIGHTING_MIN_COMPONENT) {
			lighting->colors[0][i] = STATIC_LIGHTING_MIN_COMPONENT;
			clamped = qtrue;
		}
	}

	/* scale it into acceptable range */
	while (VectorSum(lighting->colors[0]) < STATIC_LIGHTING_MIN_SUM) {
		VectorScale(lighting->colors[0], 1.25, lighting->colors[0]);
		clamped = qtrue;
	}

	if (!clamped)  /* the color was fine, leave it be */
		return;

	/* pale it out some to minimize scaling artifacts */
	f = VectorSum(lighting->colors[0]) / 3.0;

	for (i = 0; i < 3; i++)
		lighting->colors[0][i] = (lighting->colors[0][i] + f) / 2.0;
}


/**
 * @brief Resolve the lighting color at the point of impact from the downward trace.
 * If the trace did not intersect a surface, use the level's ambient color.
 */
static void R_LightPointColor (static_lighting_t *lighting)
{
	/* shuffle the samples */
	VectorCopy(lighting->colors[0], lighting->colors[1]);

	/* hit something */
	if (refdef.trace.leafnum) {
		VectorSet(lighting->colors[0], 1.0, 1.0, 1.0);
		/* resolve the lighting sample */
		if (refdef.traceEntity) {
			const entity_t *entity = refdef.traceEntity;
			const model_t *model = entity->model;
			const mBspModel_t *bspModel = &model->bsp;
			/* clip to all surfaces of the bsp entity */
			if (r_mapTiles[bspModel->maptile]->bsp.lightdata) {
				VectorSubtract(refdef.trace.endpos,
						refdef.traceEntity->origin, refdef.trace.endpos);

				R_LightPointColor_(bspModel->maptile, bspModel->firstmodelsurface,
						bspModel->nummodelsurfaces, refdef.trace.endpos, lighting->colors[0]);
			}
		} else if (r_mapTiles[refdef.trace.mapTile]->bsp.lightdata) {
			/* general case is to recurse up the nodes */
			mBspLeaf_t *leaf = &r_mapTiles[refdef.trace.mapTile]->bsp.leafs[refdef.trace.leafnum];
			mBspNode_t *node = leaf->parent;

			while (node) {
				if (R_LightPointColor_(refdef.trace.mapTile, node->firstsurface, node->numsurfaces, refdef.trace.endpos, lighting->colors[0]))
					break;

				node = node->parent;
			}
		}
	}

	R_LightPointClamp(lighting);
}

/**
 * @brief Resolves the closest static light source, populating the lighting's position
 * element. This facilitates directional shading in the fragment program.
 */
static void R_LightPointPosition (static_lighting_t *lighting)
{
	mBspLight_t *l;
	float best;
	int i;

	if (!r_state.lighting_enabled)  /* don't bother */
		return;

	/* shuffle the samples */
	VectorCopy(lighting->positions[0], lighting->positions[1]);

	best = -99999.0;

	assert(refdef.trace.mapTile >= 0);
	assert(refdef.trace.mapTile < r_numMapTiles);

	l = r_mapTiles[refdef.trace.mapTile]->bsp.bsplights;
	for (i = 0; i < r_mapTiles[refdef.trace.mapTile]->bsp.numbsplights; i++, l++) {
		float light;
		vec3_t delta;
		VectorSubtract(l->org, lighting->origin, delta);
		light = l->radius - VectorLength(delta);

		if (light > best) {  /* it's close, but is it in sight */
			R_Trace(l->org, lighting->origin, 0.0, CONTENTS_SOLID);
			if (refdef.trace.fraction < 1.0)
				continue;

			best = light;
			VectorCopy(l->org, lighting->positions[0]);
		}
	}
}


#define STATIC_LIGHTING_INTERVAL 0.25

/**
 * @brief Interpolate color and position information
 */
static void R_LightPointLerp (static_lighting_t *lighting)
{
	float lerp;

	/* only one sample */
	if (VectorCompare(lighting->colors[1], vec3_origin)) {
		VectorCopy(lighting->colors[0], lighting->color);
		VectorCopy(lighting->positions[0], lighting->position);
		return;
	}

	/* calculate the lerp fraction */
	lerp = (refdef.time - lighting->time) / STATIC_LIGHTING_INTERVAL;

	/* and lerp the colors */
	VectorMix(lighting->colors[1], lighting->colors[0], lerp, lighting->color);

	/* and the positions */
	VectorMix(lighting->positions[1], lighting->positions[0], lerp, lighting->position);
}

/**
 * @brief Resolve static lighting information for the specified point.  Linear
 * interpolation is used to blend the previous lighting information, provided
 * it is recent.
 * @sa R_LightPointColor_
 */
void R_LightPoint (const vec3_t point, static_lighting_t *lighting)
{
	vec3_t start, end;
	float delta;

	lighting->dirty = qfalse;  /* mark it clean */

	/* new level check */
	if (lighting->time > refdef.time)
		lighting->time = 0.0;

	/* copy the origin */
	VectorCopy(point, lighting->origin);

	/* do the trace */
	VectorCopy(lighting->origin, start);
	VectorCopy(lighting->origin, end);
	end[2] -= 256.0;

	R_Trace(start, end, 0.0, MASK_SOLID);

	/* resolve the shadow origin and direction */
	if (refdef.trace.leafnum) { /* hit something */
		VectorCopy(refdef.trace.endpos, lighting->point);
		VectorCopy(refdef.trace.plane.normal, lighting->normal);
	} else {  /* clear it */
		VectorClear(lighting->point);
		VectorClear(lighting->normal);
	}

	/* resolve the lighting sample, using linear interpolation */
	delta = refdef.time - lighting->time;

	/* just lerp */
	if (lighting->time && delta < STATIC_LIGHTING_INTERVAL) {
		R_LightPointLerp(lighting);
		return;
	}

	/* bump the time */
	lighting->time = refdef.time;

	/* resolve the lighting color */
	R_LightPointColor(lighting);

	/* resolve the static light source position */
	R_LightPointPosition(lighting);

	/* interpolate the lighting samples */
	R_LightPointLerp(lighting);
}
