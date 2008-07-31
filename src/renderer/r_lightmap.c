/**
 * @file r_lightmap.c
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

/* in video memory, lightmaps are chunked into rgba blocks */
#define LIGHTMAP_BUFFER_SIZE \
	(LIGHTMAP_BLOCK_WIDTH * LIGHTMAP_BLOCK_HEIGHT * LIGHTMAP_BLOCK_BYTES)

/* in the bsp, they are just rgb, and we work with floats */
#define LIGHTMAP_FBUFFER_SIZE \
	(LIGHTMAP_WIDTH * LIGHTMAP_HEIGHT * LIGHTMAP_BYTES)

typedef struct lightmaps_s {
	GLuint texnum;

	unsigned allocated[LIGHTMAP_BLOCK_WIDTH];

	byte buffer[LIGHTMAP_BUFFER_SIZE];
	float fbuffer[LIGHTMAP_FBUFFER_SIZE];
} lightmaps_t;

static lightmaps_t r_lightmaps;

static void R_UploadLightmapBlock (void)
{
	if (r_lightmaps.texnum == MAX_GL_LIGHTMAPS) {
		Com_Printf("R_UploadLightmapBlock: MAX_GL_LIGHTMAPS reached.\n");
		return;
	}

	R_BindTexture(TEXNUM_LIGHTMAPS + r_lightmaps.texnum);

	qglTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	qglTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

	qglTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, r_maxlightmap->integer, r_maxlightmap->integer,
		0, GL_RGBA, GL_UNSIGNED_BYTE, r_lightmaps.buffer);

	R_CheckError();

	/* clear the allocation block */
	memset(r_lightmaps.allocated, 0, sizeof(r_lightmaps.allocated));

	r_lightmaps.texnum++;
}

/**
 * @brief returns a texture number and the position inside it
 */
static qboolean R_AllocLightmapBlock (int w, int h, int *x, int *y)
{
	int i, j;
	int best, best2;

	best = r_maxlightmap->integer;

	for (i = 0; i < r_maxlightmap->integer - w; i++) {
		best2 = 0;

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

	if (best + h > r_maxlightmap->integer)
		return qfalse;

	for (i = 0; i < w; i++)
		r_lightmaps.allocated[*x + i] = best + h;

	return qtrue;
}

/**
 * @brief Fullbridght lightmap
 * @sa R_BuildLightmap
 */
static void R_BuildDefaultLightmap (mBspSurface_t *surf, byte *dest, int stride)
{
	int i, j, smax, tmax, size;

	smax = (surf->stmaxs[0] / surf->lightmap_scale) + 1;
	tmax = (surf->stmaxs[1] / surf->lightmap_scale) + 1;

	size = smax * tmax;
	stride -= (smax * 4);

	for (i = 0; i < tmax; i++, dest += stride) {
		for (j = 0; j < smax; j++) {
			dest[0] = 255;
			dest[1] = 255;
			dest[2] = 255;
			dest[3] = 255;

			dest += 4;
		}
	}

	VectorSet(surf->color, 1, 1, 1);
}

/**
 * @brief Combine and scale multiple lightmaps into the floating format in blocklights
 * @sa R_BuildDefaultLightmap
 */
static void R_BuildLightmap (mBspSurface_t * surf, byte * dest, int stride)
{
	int smax, tmax;
	unsigned int i, j, size;
	byte *lightmap, *lm, *l;
	float *bl;

	smax = (surf->stmaxs[0] / surf->lightmap_scale) + 1;
	tmax = (surf->stmaxs[1] / surf->lightmap_scale) + 1;
	size = smax * tmax;
	if (size * LIGHTMAP_BYTES > (sizeof(r_lightmaps.fbuffer)))
		Com_Error(ERR_DROP, "R_BuildLightmap: Surface too large: %d.\n", size);

	lightmap = surf->samples;

	bl = r_lightmaps.fbuffer;

	for (i = 0; i < size; i++, bl += LIGHTMAP_BYTES) {
		bl[0] = lightmap[i * LIGHTMAP_BYTES + 0] * r_modulate->value;
		bl[1] = lightmap[i * LIGHTMAP_BYTES + 1] * r_modulate->value;
		bl[2] = lightmap[i * LIGHTMAP_BYTES + 2] * r_modulate->value;
	}

	/* put into texture format */
	stride -= (smax * 4);
	bl = r_lightmaps.fbuffer;

	/* first into an rgba linear block for softening */
	lightmap = (byte *)Mem_PoolAlloc(size * LIGHTMAP_BLOCK_BYTES, vid_lightPool, 0);
	lm = lightmap;

	for (i = 0; i < tmax; i++) {
		for (j = 0; j < smax; j++) {
			int r = Q_ftol(bl[0]);
			int g = Q_ftol(bl[1]);
			int b = Q_ftol(bl[2]);
			int max;

			/* catch negative lights */
			if (r < 0)
				r = 0;
			if (g < 0)
				g = 0;
			if (b < 0)
				b = 0;

			/* determine the brightest of the three color components */
			if (r > g)
				max = r;
			else
				max = g;
			if (b > max)
				max = b;

			/* rescale all the color components if the intensity of the greatest
			 * channel exceeds 255 */
			if (max > 255) {
				float t = 255.0F / max;

				r *= t;
				g *= t;
				b *= t;
			}

			lm[0] = r;
			lm[1] = g;
			lm[2] = b;
			lm[3] = 255; /* pad alpha */

			bl += LIGHTMAP_BYTES;
			lm += LIGHTMAP_BLOCK_BYTES;
		}
	}

	/* apply contrast, resolve average surface color, etc.. */
	R_FilterTexture((unsigned *)lightmap, smax, tmax, it_lightmap);

	/* soften it if it's sufficiently large */
	if (r_soften->integer && size > 128)
		for (i = 0; i < 4; i++)
			R_SoftenTexture(lightmap, smax, tmax, LIGHTMAP_BLOCK_BYTES);

	/* the final lightmap is uploaded to the card via the strided lightmap
	 * block, and also cached on the surface for fast point lighting lookups */

	surf->lightmap = (byte *)Mem_PoolAlloc(size * LIGHTMAP_BYTES, vid_lightPool, 0);
	l = surf->lightmap;
	lm = lightmap;
	for (i = 0; i < tmax; i++, dest += stride) {
		for (j = 0; j < smax; j++) {
			/* copy to the strided block */
			dest[0] = lm[0];
			dest[1] = lm[1];
			dest[2] = lm[2];
			dest[3] = lm[3];
			dest += LIGHTMAP_BLOCK_BYTES;

			/* and to the surface */
			l[0] = lm[0];
			l[1] = lm[1];
			l[2] = lm[2];
			l += LIGHTMAP_BYTES;

			lm += LIGHTMAP_BLOCK_BYTES;
		}
	}

	Mem_Free(lightmap);
}

/**
 * @sa R_ModLoadSurfaces
 */
void R_CreateSurfaceLightmap (mBspSurface_t * surf)
{
	int smax, tmax;
	byte *base;

	if (!(surf->flags & MSURF_LIGHTMAP))
		return;

	smax = (surf->stmaxs[0] / surf->lightmap_scale) + 1;
	tmax = (surf->stmaxs[1] / surf->lightmap_scale) + 1;

	if (!R_AllocLightmapBlock(smax, tmax, &surf->light_s, &surf->light_t)) {
		R_UploadLightmapBlock();
		if (!R_AllocLightmapBlock(smax, tmax, &surf->light_s, &surf->light_t))
			Com_Error(ERR_DROP, "Consecutive calls to R_AllocLightmapBlock(%d,%d) failed (lightmap_scale: %i)\n", smax, tmax, surf->lightmap_scale);
	}

	surf->lightmaptexturenum = TEXNUM_LIGHTMAPS + r_lightmaps.texnum;

	base = r_lightmaps.buffer;
	base += (surf->light_t * r_maxlightmap->integer + surf->light_s) * LIGHTMAP_BLOCK_BYTES;

	if (!surf->samples)  /* make it fullbright */
		R_BuildDefaultLightmap(surf, base, r_maxlightmap->integer * LIGHTMAP_BLOCK_BYTES);
	else  /* or light it properly */
		R_BuildLightmap(surf, base, r_maxlightmap->integer * LIGHTMAP_BLOCK_BYTES);
}

/**
 * @sa R_ModBeginLoading
 * @sa R_EndBuildingLightmaps
 */
void R_BeginBuildingLightmaps (void)
{
	memset(r_lightmaps.allocated, 0, sizeof(r_lightmaps.allocated));

	r_lightmaps.texnum = 0;
}

/**
 * @sa R_BeginBuildingLightmaps
 * @sa R_ModBeginLoading
 */
void R_EndBuildingLightmaps (void)
{
	R_UploadLightmapBlock();
	Com_DPrintf(DEBUG_RENDERER, "lightmaps: %i\n", r_lightmaps.texnum);
}

/** @brief for resolving static lighting for mesh ents */
lightmap_sample_t r_lightmap_sample;

/**
 * @brief for resolving static lighting for mesh ents
 */
static qboolean R_LightPoint_ (const model_t *mapTile, const mBspNode_t *node, vec3_t start, vec3_t end)
{
	float front, back;
	int i, side, sample;
	float s, t, ds, dt;
	vec3_t mid;
	mBspSurface_t *surf;
	mBspTexInfo_t *tex;

begin:

	if (node->contents != NODE_NO_LEAF) /* didn't hit anything */
		return qfalse;

	/* "special" pathfinding node */
	/** @todo This prevents the light point code from working properly */
	if (!node->plane) {
		side = 0;
		if (R_LightPoint_(mapTile, node->children[side], start, end))
			return qtrue;

		return R_LightPoint_(mapTile, node->children[!side], start, end);
	}

	VectorCopy(end, mid);

	/* optimize for axially aligned planes */
	switch (node->plane->type) {
	case PLANE_X:
	case PLANE_Y:
		node = node->children[start[node->plane->type] < node->plane->dist];
		goto begin;
	case PLANE_Z:
		side = start[2] < node->plane->dist;
		if ((end[2] < node->plane->dist) == side) {
			node = node->children[side];
			goto begin;
		}
		mid[2] = node->plane->dist;
		break;
	default:
		/* compute partial dot product to determine sidedness */
		back = front = start[0] * node->plane->normal[0] + start[1] * node->plane->normal[1];
		front += start[2] * node->plane->normal[2];
		back += end[2] * node->plane->normal[2];
		side = front < node->plane->dist;
		if ((back < node->plane->dist) == side) {
			node = node->children[side];
			goto begin;
		}
		mid[2] = start[2] + (end[2] - start[2]) * (front - node->plane->dist) / (front - back);
		break;
	}

	/* go down front side */
	if (R_LightPoint_(mapTile, node->children[side], start, mid))
		return qtrue;

	/* check for impact on this node */
	surf = mapTile->bsp.surfaces + node->firstsurface;
	for (i = 0; i < node->numsurfaces; i++, surf++) {
		if (!(surf->flags & MSURF_LIGHTMAP))
			continue;  /* no lightmap */

		if (surf->texinfo->flags & SURF_ALPHATEST)
			continue;

		tex = surf->texinfo;

		s = DotProduct(mid, tex->vecs[0]) + tex->vecs[0][3];
		t = DotProduct(mid, tex->vecs[1]) + tex->vecs[1][3];

		if (s < surf->stmins[0] || t < surf->stmins[1])
			continue;

		ds = s - surf->stmins[0];
		dt = t - surf->stmins[1];

		if (ds > surf->stmaxs[0] || dt > surf->stmaxs[1])
			continue;

		/* we've hit, so find the sample */
		VectorCopy(mid, r_lightmap_sample.point);

		ds /= surf->lightmap_scale;
		dt /= surf->lightmap_scale;

		/* resolve the lightmap sample at intersection */
		/** @todo What about r_modulate? */
		VectorSet(r_lightmap_sample.color, surf->lightmap[sample + 0] / 255.0,
				surf->lightmap[sample + 1] / 255.0, surf->lightmap[sample + 2] / 255.0);

		/* and normalize it to floating point */
		VectorScale(surf->lightmap + sample, r_modulate->value * (1.0 / 255), r_lightmap_sample.color);

		return qtrue;
	}

	/* go down back side */
	return R_LightPoint_(mapTile, node->children[!side], mid, end);
}

/**
 * @sa R_LightPoint_
 */
void R_LightPoint (const vec3_t p)
{
	lightmap_sample_t lms;
	vec3_t start, end, dest;
	float dist, d;
	int j;

	/* fullbright */
	VectorClear(r_lightmap_sample.point);
	VectorSet(r_lightmap_sample.color, 1.0, 1.0, 1.0);

	if (!r_mapTiles[0]->bsp.lightdata)
		return;

	/* there is lightdata */
	VectorSet(r_lightmap_sample.color, 0.5, 0.5, 0.5);
	lms = r_lightmap_sample;

	/* dest is the absolute lowest we'll trace to */
	VectorCopy(p, dest);
	dest[2] -= 256;

	dist = 999999;

	VectorCopy(p, start);	/* while start and end are transformed according */
	VectorCopy(dest, end);	/* to the entity being traced */

	/* check world */
	for (j = 0; j < r_numMapTiles; j++) {
		const mBspNode_t *node = r_mapTiles[j]->bsp.nodes;
		if (!r_mapTiles[j]->bsp.lightdata) {
			Com_Printf("R_LightPoint: No light data in maptile %i (%s)\n", j, r_mapTiles[j]->name);
			continue;
		}

		if (!node->plane) {
			Com_Printf("R_LightPoint: Called with pathfinding node\n");
			continue;
		}

		if (!R_LightPoint_(r_mapTiles[j], node, start, end))
			continue;

		if ((d = start[2] - r_lightmap_sample.point[2]) < dist) {  /* hit */
			dist = start[2] - r_lightmap_sample.point[2];
			lms = r_lightmap_sample;
		}
	}



	/* check inline bsp models */
	for (j = 0; j < r_numEntities; j++) {
		const entity_t *ent = R_GetEntity(j);
		model_t *m = ent->model;
		mBspModel_t *bsp = &m->bsp;

		if (!m || m->type != mod_bsp_submodel)
			continue;

		VectorSubtract(p, ent->origin, start);
		VectorSubtract(dest, ent->origin, end);

		if (!R_LightPoint_(r_mapTiles[bsp->maptile], &r_mapTiles[bsp->maptile]->bsp.nodes[m->bsp.firstnode], start, end))
			continue;

		if ((d = start[2] - r_lightmap_sample.point[2]) < dist) {  /* hit */
			lms = r_lightmap_sample;
			dist = d;

			VectorAdd(lms.point, ent->origin, lms.point);
		}
	}

	/* copy the closest result to the shared instance */
	r_lightmap_sample = lms;
}
