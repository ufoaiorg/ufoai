/**
 * @file
 * In video memory, lightmaps are chunked into NxN RGB pages.
 * In the bsp, they are just sequentially stored RGB blocks per surface
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

static void R_UploadLightmapPage (void)
{
#ifdef GL_VERSION_ES_CM_1_0
	const int texFormat = GL_RGB;
#else
	const int texFormat = r_config.gl_compressed_solid_format ? r_config.gl_compressed_solid_format : r_config.gl_solid_format;
#endif
	GLuint texid;
	if (r_lightmaps.lightmap_count >= MAX_GL_LIGHTMAPS) {
		Com_Printf("R_UploadLightmapPage: MAX_GL_LIGHTMAPS reached.\n");
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

	glTexImage2D(GL_TEXTURE_2D, 0, texFormat, r_lightmaps.size, r_lightmaps.size,
		0, GL_RGB, GL_UNSIGNED_BYTE, r_lightmaps.sample_buffer);

	R_CheckError();

	if (r_lightmaps.deluxemap_count >= MAX_GL_DELUXEMAPS) {
		Com_Printf("R_UploadLightmapPage: MAX_GL_DELUXEMAPS reached.\n");
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

	glTexImage2D(GL_TEXTURE_2D, 0, texFormat, r_lightmaps.size, r_lightmaps.size,
		0, GL_RGB, GL_UNSIGNED_BYTE, r_lightmaps.direction_buffer);

	/* clear the allocation heightmap and buffers */
	memset(r_lightmaps.allocated, 0, r_lightmaps.size * sizeof(unsigned));
	memset(r_lightmaps.sample_buffer, 0, r_lightmaps.size * r_lightmaps.size * LIGHTMAP_SAMPLE_SIZE);
	memset(r_lightmaps.direction_buffer, 0, r_lightmaps.size * r_lightmaps.size * DELUXEMAP_SAMPLE_SIZE);

	r_lightmaps.incomplete_atlas = false;
}

/**
 * Find a suitable location to store data in the texture atlas
 * @brief returns a texture number and the position inside it
 */
static bool R_AllocLightmapBlock (int w, int h, int* x, int* y)
{
	if (!r_lightmaps.incomplete_atlas) {
		r_lightmaps.incomplete_atlas = true;
		glGenTextures(1, &r_lightmaps.lightmap_texnums[++r_lightmaps.lightmap_count]);
		glGenTextures(1, &r_lightmaps.deluxemap_texnums[++r_lightmaps.deluxemap_count]);
	}

	/* the height to store the data in the atlas */
	int best = r_lightmaps.size;

	int i;
	for (i = 0; i < r_lightmaps.size - w; i++) {
		int best2 = 0;
		int j;

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
		return false;

	for (i = 0; i < w; i++)
		r_lightmaps.allocated[*x + i] = best + h;

	return true;
}

/**
 * @brief Fullbridght lightmap
 * @sa R_BuildLightmap
 */
static void R_BuildDefaultLightmap (mBspSurface_t* surf, byte* sout, byte* dout, int stride)
{
	const int smax = (surf->stextents[0] / surf->lightmap_scale) + 1;
	const int tmax = (surf->stextents[1] / surf->lightmap_scale) + 1;

	/* Allocatate attached-to-surface cache for fast point lighting lookups */
	byte* l = surf->lightmap = Mem_PoolAllocTypeN(byte, smax * tmax * LIGHTMAP_SAMPLE_SIZE, vid_lightPool);

	for (int t = 0; t < tmax; t++) {
		byte* lmPtr = sout, *dmPtr = dout;
		for (int s = 0; s < smax; s++) {
			/* fill lightmap samples */
			l[0] = lmPtr[0] = 255;
			l[1] = lmPtr[1] = 255;
			l[2] = lmPtr[2] = 255;
			/* fill deluxemap samples */
			dmPtr[0] = 127;
			dmPtr[1] = 127;
			dmPtr[2] = 255;
			/* advance pointers */
			lmPtr += LIGHTMAP_SAMPLE_SIZE;
			dmPtr += DELUXEMAP_SAMPLE_SIZE;
			l += LIGHTMAP_SAMPLE_SIZE;
		}
		sout += stride * LIGHTMAP_SAMPLE_SIZE;
		dout += stride * DELUXEMAP_SAMPLE_SIZE;
	}

	Vector4Set(surf->lightColor, 1.0, 1.0, 1.0, 1.0);
}

/**
 * @brief Consume raw lightmap and deluxemap RGB/XYZ data from the surface samples,
 * and write them into the strided block in the atlas page
 * @sa R_BuildDefaultLightmap
 */
static void R_BuildLightmap (mBspSurface_t* surf, byte* sout, byte* dout, int stride)
{
	const int smax = (surf->stextents[0] / surf->lightmap_scale) + 1;
	const int tmax = (surf->stextents[1] / surf->lightmap_scale) + 1;
	const int area = smax * tmax;

	int color[3] = {0, 0, 0};
	byte* src = surf->samples;

	/* Allocatate attached-to-surface cache for fast point lighting lookups and keep the pointer to fill it  a bit later */
	byte* l = surf->lightmap = Mem_PoolAllocTypeN(byte, area * LIGHTMAP_SAMPLE_SIZE, vid_lightPool);

	for (int t = 0; t < tmax; t++) {
		byte* lmPtr = sout, *dmPtr = dout;
		for (int s = 0; s < smax; s++) {
			/* process lightmap samples and accumulate the average color */
			color[0] += l[0] = lmPtr[0] = src[0];
			color[1] += l[1] = lmPtr[1] = src[1];
			color[2] += l[2] = lmPtr[2] = src[2];
			/* process deluxemap samples */
			dmPtr[0] = src[3];
			dmPtr[1] = src[4];
			dmPtr[2] = src[5];
			/* advance pointers */
			lmPtr += LIGHTMAP_SAMPLE_SIZE;
			dmPtr += DELUXEMAP_SAMPLE_SIZE;
			l += LIGHTMAP_SAMPLE_SIZE;
			src += 6;
		}
		sout += stride * LIGHTMAP_SAMPLE_SIZE;
		dout += stride * DELUXEMAP_SAMPLE_SIZE;
	}

	/* store average lightmap color and surface alpha */
	surf->lightColor[0] = color[0] / (255.0 * area);
	surf->lightColor[1] = color[1] / (255.0 * area);
	surf->lightColor[2] = color[2] / (255.0 * area);

	if (surf->texinfo->flags & (SURF_BLEND33 | SURF_ALPHATEST))
		surf->lightColor[3] = 0.25;
	else if (surf->texinfo->flags & SURF_BLEND66)
		surf->lightColor[3] = 0.50;
	else
		surf->lightColor[3] = 1.0;
}

/**
 * @sa R_ModLoadSurfaces
 */
void R_CreateSurfaceLightmap (mBspSurface_t* surf)
{
	int smax, tmax;
	byte* samples, *directions;

	if (!(surf->flags & MSURF_LIGHTMAP))
		return;

	smax = (surf->stextents[0] / surf->lightmap_scale) + 1;
	tmax = (surf->stextents[1] / surf->lightmap_scale) + 1;

	if (!R_AllocLightmapBlock(smax, tmax, &surf->light_s, &surf->light_t)) {
		/* upload the last page */
		R_UploadLightmapPage();
		if (!R_AllocLightmapBlock(smax, tmax, &surf->light_s, &surf->light_t))
			Com_Error(ERR_DROP, "R_CreateSurfaceLightmap: Consecutive calls to R_AllocLightmapBlock(%d,%d) failed (lightmap_scale: %i, stextents: %f %f)\n",
					smax, tmax, surf->lightmap_scale, surf->stextents[0], surf->stextents[1]);
	}

	surf->lightmap_texnum = r_lightmaps.lightmap_texnums[r_lightmaps.lightmap_count];
	surf->deluxemap_texnum = r_lightmaps.deluxemap_texnums[r_lightmaps.deluxemap_count];

	samples = r_lightmaps.sample_buffer;
	samples += (surf->light_t*  r_lightmaps.size + surf->light_s) * LIGHTMAP_SAMPLE_SIZE;

	directions = r_lightmaps.direction_buffer;
	directions += (surf->light_t*  r_lightmaps.size + surf->light_s) * DELUXEMAP_SAMPLE_SIZE;

	if (!surf->samples)  /* make it fullbright */
		R_BuildDefaultLightmap(surf, samples, directions, r_lightmaps.size);
	else  /* or light it properly */
		R_BuildLightmap(surf, samples, directions, r_lightmaps.size);
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
	static bool gotAllocatedLightmaps = false;

	if (gotAllocatedLightmaps)
		R_DisposeLightmaps();

	gotAllocatedLightmaps = true;

	/* users can tune lightmap size for their card */
	r_lightmaps.size = r_maxlightmap->integer;

	r_lightmaps.allocated        = Mem_PoolAllocTypeN(unsigned, r_lightmaps.size, vid_lightPool);
	r_lightmaps.sample_buffer    = Mem_PoolAllocTypeN(byte, r_lightmaps.size * r_lightmaps.size * LIGHTMAP_SAMPLE_SIZE, vid_lightPool);
	r_lightmaps.direction_buffer = Mem_PoolAllocTypeN(byte, r_lightmaps.size * r_lightmaps.size * DELUXEMAP_SAMPLE_SIZE, vid_lightPool);

	r_lightmaps.lightmap_count = 0;
	r_lightmaps.deluxemap_count = 0;
	r_lightmaps.incomplete_atlas = false;
}

/**
 * @sa R_BeginBuildingLightmaps
 * @sa R_ModBeginLoading
 */
void R_EndBuildingLightmaps (void)
{
	/* upload the pending lightmap page */
	R_UploadLightmapPage();
	Mem_Free(r_lightmaps.allocated);
	Mem_Free(r_lightmaps.sample_buffer);
	Mem_Free(r_lightmaps.direction_buffer);
	r_lightmaps.allocated = nullptr;
	r_lightmaps.sample_buffer = nullptr;
	r_lightmaps.direction_buffer = nullptr;
}


/**
 * @brief Moves the given mins/maxs volume through the world from start to end.
 * @param[in] trLine Start and end vectors of the trace
 * @param[in] size Bounding box size used for tracing
 * @param[in] contentmask Searched content the trace should watch for
 */
void R_Trace (const Line& trLine, float size, int contentmask)
{
	r_locals.tracenum++;

	if (r_locals.tracenum > 0xffff)  /* avoid overflows */
		r_locals.tracenum = 0;

	AABB box;
	box.expand(size);

	refdef.trace = CM_CompleteBoxTrace(refdef.mapTiles, trLine, box, TRACE_ALL_LEVELS, contentmask, 0);
	refdef.traceEntity = nullptr;

	float frac = refdef.trace.fraction;

	/* check bsp models */
	for (int i = 0; i < refdef.numEntities; i++) {
		entity_t* ent = R_GetEntity(i);
		const model_t* m = ent->model;

		if (!m || m->type != mod_bsp_submodel)
			continue;

		trace_t tr = CM_TransformedBoxTrace(refdef.mapTiles->mapTiles[m->bsp.maptile], trLine, box, m->bsp.firstnode,
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
