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
	const int samples = r_config.gl_compressed_solid_format ? r_config.gl_compressed_solid_format : r_config.gl_solid_format;
	GLuint texid;
	if (r_lightmaps.lightmap_count >= MAX_GL_LIGHTMAPS) {
		Com_Printf("R_UploadLightmapBlock: MAX_GL_LIGHTMAPS reached.\n");
		return;
	}

	if (!r_lightmaps.incomplete_atlas) {
		glGenTextures(1, &texid);
		r_lightmaps.lightmap_texnums[r_lightmaps.lightmap_count++] = texid;
	} else {
		texid = r_lightmaps.lightmap_texnums[r_lightmaps.lightmap_count];
	}

	R_BindTexture(texid);

	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

	glTexImage2D(GL_TEXTURE_2D, 0, samples, r_lightmaps.size, r_lightmaps.size,
		0, GL_RGB, GL_UNSIGNED_BYTE, r_lightmaps.sample_buffer);

	R_CheckError();

	if (r_lightmaps.deluxemap_count >= MAX_GL_DELUXEMAPS) {
		Com_Printf("R_UploadLightmapBlock: MAX_GL_DELUXEMAPS reached.\n");
		return;
	}

	if (!r_lightmaps.incomplete_atlas) {
		glGenTextures(1, &texid);
		r_lightmaps.deluxemap_texnums[r_lightmaps.deluxemap_count++] = texid;
	} else {
		texid = r_lightmaps.deluxemap_texnums[r_lightmaps.deluxemap_count];
	}

	R_BindTexture(texid);

	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

	glTexImage2D(GL_TEXTURE_2D, 0, samples, r_lightmaps.size, r_lightmaps.size,
		0, GL_RGB, GL_UNSIGNED_BYTE, r_lightmaps.direction_buffer);

	/* clear the allocation block and buffers */
	memset(r_lightmaps.allocated, 0, r_lightmaps.size * sizeof(unsigned));
	memset(r_lightmaps.sample_buffer, 0, r_lightmaps.size * r_lightmaps.size * sizeof(unsigned));
	memset(r_lightmaps.direction_buffer, 0, r_lightmaps.size * r_lightmaps.size * sizeof(unsigned));

	r_lightmaps.incomplete_atlas = qfalse;
}

/**
 * @brief returns a texture number and the position inside it
 */
static qboolean R_AllocLightmapBlock (int w, int h, int *x, int *y)
{
	int i, j;
	int best;

	if (!r_lightmaps.incomplete_atlas) {
		r_lightmaps.incomplete_atlas = qtrue;
		glGenTextures(1, &r_lightmaps.lightmap_texnums[++r_lightmaps.lightmap_count]);
		glGenTextures(1, &r_lightmaps.deluxemap_texnums[++r_lightmaps.deluxemap_count]);
	}

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
 * @brief Compute the surface's average color.
 */
static void R_GetAverageSurfaceColor (const byte *in, int width, int height, vec3_t color, int bpp)
{
	unsigned col[3];
	const byte *p = in;
	const byte *end = p + width * height * bpp;
	int i;

	VectorClear(col);

	for (; p != end; p += bpp) {
		int j;
		for (j = 0; j < 3; j++) {
			/* accumulate color */
			col[j] += p[j];
		}
	}

	/* average accumulated colors */
	for (i = 0; i < 3; i++)
		col[i] /= (width * height);

	for (i = 0; i < 3; i++)
		color[i] = col[i] / 255.0;
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
	R_GetAverageSurfaceColor(lightmap, smax, tmax, surf->color, LIGHTMAP_BLOCK_BYTES);

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

	surf->lightmap_texnum = r_lightmaps.lightmap_texnums[r_lightmaps.lightmap_count];
	surf->deluxemap_texnum = r_lightmaps.deluxemap_texnums[r_lightmaps.deluxemap_count];

	samples = r_lightmaps.sample_buffer;
	samples += (surf->light_t * r_lightmaps.size + surf->light_s) * LIGHTMAP_BLOCK_BYTES;

	directions = r_lightmaps.direction_buffer;
	directions += (surf->light_t * r_lightmaps.size + surf->light_s) * DELUXEMAP_BLOCK_BYTES;

	if (!surf->samples)  /* make it fullbright */
		R_BuildDefaultLightmap(surf, samples, directions, r_lightmaps.size * LIGHTMAP_BLOCK_BYTES);
	else  /* or light it properly */
		R_BuildLightmap(surf, samples, directions, r_lightmaps.size * LIGHTMAP_BLOCK_BYTES);
}

static void R_DisposeLightmaps (void)
{
	if (r_lightmaps.lightmap_count) {
		glDeleteTextures(r_lightmaps.lightmap_count, r_lightmaps.lightmap_texnums);
		r_lightmaps.lightmap_count = 0;
	}
	if (r_lightmaps.deluxemap_count) {
		glDeleteTextures(r_lightmaps.deluxemap_count, r_lightmaps.deluxemap_texnums);
		r_lightmaps.deluxemap_count = 0;
	}
}

/**
 * @sa R_ModBeginLoading
 * @sa R_EndBuildingLightmaps
 */
void R_BeginBuildingLightmaps (void)
{
	static qboolean gotAllocatedLightmaps = qfalse;

	if (gotAllocatedLightmaps)
		R_DisposeLightmaps();

	gotAllocatedLightmaps = qtrue;

	/* users can tune lightmap size for their card */
	r_lightmaps.size = r_maxlightmap->integer;

	r_lightmaps.allocated = (unsigned *)Mem_PoolAlloc(r_lightmaps.size *
		sizeof(unsigned), vid_lightPool, 0);

	r_lightmaps.sample_buffer = (byte *)Mem_PoolAlloc(
		r_lightmaps.size * r_lightmaps.size * sizeof(unsigned), vid_lightPool, 0);

	r_lightmaps.direction_buffer = (byte *)Mem_PoolAlloc(
		r_lightmaps.size * r_lightmaps.size * sizeof(unsigned), vid_lightPool, 0);

	r_lightmaps.lightmap_count = 0;
	r_lightmaps.deluxemap_count = 0;
	r_lightmaps.incomplete_atlas = qfalse;
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
void R_Trace (const vec3_t start, const vec3_t end, float size, int contentmask)
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

	refdef.trace = CM_CompleteBoxTrace(refdef.mapTiles, start, end, mins, maxs, TRACING_ALL_VISIBLE_LEVELS, contentmask, 0);
	refdef.traceEntity = NULL;

	frac = refdef.trace.fraction;

	/* check bsp models */
	for (i = 0; i < refdef.numEntities; i++) {
		entity_t *ent = R_GetEntity(i);
		const model_t *m = ent->model;

		if (!m || m->type != mod_bsp_submodel)
			continue;

		tr = CM_TransformedBoxTrace(&(refdef.mapTiles->mapTiles[m->bsp.maptile]), start, end, mins, maxs, m->bsp.firstnode,
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
