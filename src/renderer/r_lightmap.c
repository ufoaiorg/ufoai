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

	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, r_lightmaps.size, r_lightmaps.size,
		0, GL_RGBA, GL_UNSIGNED_BYTE, r_lightmaps.sample_buffer);

	R_CheckError();

	r_lightmaps.lightmap_texnum++;

	if (r_lightmaps.deluxemap_texnum == MAX_GL_DELUXEMAPS) {
		Com_Printf("R_UploadLightmapBlock: MAX_GL_DELUXEMAPS reached.\n");
		return;
	}

	R_BindTexture(r_lightmaps.deluxemap_texnum);

	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, r_lightmaps.size, r_lightmaps.size,
		0, GL_RGBA, GL_UNSIGNED_BYTE, r_lightmaps.direction_buffer);

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
	int best, best2;

	best = r_lightmaps.size;

	for (i = 0; i < r_lightmaps.size - w; i++) {
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
	int i, j, smax, tmax, size;

	smax = (surf->stmaxs[0] / surf->lightmap_scale) + 1;
	tmax = (surf->stmaxs[1] / surf->lightmap_scale) + 1;

	size = smax * tmax;
	stride -= (smax * 4);

	for (i = 0; i < tmax; i++, sout += stride, dout += stride) {
		for (j = 0; j < smax; j++) {
			sout[0] = 255;
			sout[1] = 255;
			sout[2] = 255;
			sout[3] = 255;

			sout += 4;

			dout[0] = 127;
			dout[1] = 127;
			dout[2] = 127;
			dout[3] = 255;

			dout += 4;
		}
	}

	Vector4Set(surf->color, 1, 1, 1, 1);
}

/**
 * @brief Consume raw lightmap and deluxemap RGB/XYZ data from the surface samples,
 * writing processed lightmap and deluxemap RGBA to the specified destinations.
 * @sa R_BuildDefaultLightmap
 */
static void R_BuildLightmap (mBspSurface_t *surf, byte *sout, byte *dout, int stride)
{
	int smax, tmax;
	unsigned int i, j, size;
	byte *lightmap, *lm, *l, *d;
	float *fb;

	smax = (surf->stmaxs[0] / surf->lightmap_scale) + 1;
	tmax = (surf->stmaxs[1] / surf->lightmap_scale) + 1;
	size = smax * tmax;
	if (size * LIGHTMAP_BYTES > sizeof(r_lightmaps.fbuffer))
		Com_Error(ERR_DROP, "R_BuildLightmap: Surface too large: %d.\n", size);

	lightmap = surf->samples;

	d = r_lightmaps.direction_buffer;
	fb = r_lightmaps.fbuffer;

	/* convert the raw lightmap samples to floating point and scale them */
	for (i = j = 0; i < size; i++, fb += 3, d += 4) {
		fb[0] = surf->samples[j++] * r_modulate->value;
		fb[1] = surf->samples[j++] * r_modulate->value;
		fb[2] = surf->samples[j++] * r_modulate->value;

		/* read in directional samples for deluxe mapping as well */
		d[0] = surf->samples[j++];
		d[1] = surf->samples[j++];
		d[2] = surf->samples[j++];
		d[3] = 255;  /* pad alpha */
	}

	/* put into texture format */
	stride -= (smax * 4);
	fb = r_lightmaps.fbuffer;

	/* first into an RGBA linear block for softening */
	lightmap = (byte *)Mem_PoolAlloc(size * LIGHTMAP_BLOCK_BYTES, vid_lightPool, 0);
	lm = lightmap;

	for (i = 0; i < tmax; i++) {
		for (j = 0; j < smax; j++) {
			int r = Q_ftol(fb[0]);
			int g = Q_ftol(fb[1]);
			int b = Q_ftol(fb[2]);
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

			fb += LIGHTMAP_BYTES;
			lm += LIGHTMAP_BLOCK_BYTES;
		}
	}

	/* apply contrast, resolve average surface color, etc.. */
	R_FilterTexture((unsigned *)lightmap, smax, tmax, it_lightmap);

	/* soften it if it's sufficiently large */
	if (r_soften->integer && size > 128)
		for (i = 0; i < 4; i++) {
			R_SoftenTexture(lightmap, smax, tmax, LIGHTMAP_BLOCK_BYTES);
			R_SoftenTexture(r_lightmaps.direction_buffer, smax, tmax, LIGHTMAP_BLOCK_BYTES);
		}

	/* the final lightmap is uploaded to the card via the strided lightmap
	 * block, and also cached on the surface for fast point lighting lookups */

	surf->lightmap = (byte *)Mem_PoolAlloc(size * LIGHTMAP_BYTES, vid_lightPool, 0);
	l = surf->lightmap;
	lm = lightmap;
	d = r_lightmaps.direction_buffer;

	for (i = 0; i < tmax; i++, sout += stride, dout += stride) {
		for (j = 0; j < smax; j++) {
			/* copy the lightmap to the strided block */
			sout[0] = lm[0];
			sout[1] = lm[1];
			sout[2] = lm[2];
			sout[3] = lm[3];
			sout += LIGHTMAP_BLOCK_BYTES;

			/* and to the surface, discarding alpha */
			l[0] = lm[0];
			l[1] = lm[1];
			l[2] = lm[2];
			l += LIGHTMAP_BYTES;

			lm += LIGHTMAP_BLOCK_BYTES;

			/* lastly copy the deluxemap to the strided block */
			dout[0] = d[0];
			dout[1] = d[1];
			dout[2] = d[2];
			dout[3] = d[3];
			dout += DELUXEMAP_BLOCK_BYTES;

			d += DELUXEMAP_BLOCK_BYTES;
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
	byte *samples, *directions;

	if (!(surf->flags & MSURF_LIGHTMAP))
		return;

	smax = (surf->stmaxs[0] / surf->lightmap_scale) + 1;
	tmax = (surf->stmaxs[1] / surf->lightmap_scale) + 1;

	if (!R_AllocLightmapBlock(smax, tmax, &surf->light_s, &surf->light_t)) {
		/* upload the last block */
		R_UploadLightmapBlock();
		if (!R_AllocLightmapBlock(smax, tmax, &surf->light_s, &surf->light_t))
			Com_Error(ERR_DROP, "Consecutive calls to R_AllocLightmapBlock(%d,%d) failed (lightmap_scale: %i)\n", smax, tmax, surf->lightmap_scale);
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

/** @brief for resolving static lighting for mesh ents */
lightmap_sample_t r_lightmap_sample;

/**
 * @brief for resolving static lighting for mesh ents
 * @todo This is not yet working because we are using some special nodes for
 * pathfinding @sa BuildNodeChildren - and these nodes don't have a plane assigned
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

	/** @todo Due to the pathfinding nodes the lighting is not working
	 * not every node is touched */
	/* special pathfinding node */
	if (!node->plane)
		return qfalse;

	if (node->contents != NODE_NO_LEAF) /* didn't hit anything */
		return qfalse;

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
		sample = (int)(LIGHTMAP_BYTES * ((int)dt * ((surf->stmaxs[0] / surf->lightmap_scale) + 1) + (int)ds));

		/* and normalize it to floating point */
		VectorSet(r_lightmap_sample.color, surf->lightmap[sample + 0] / 255.0,
				surf->lightmap[sample + 1] / 255.0, surf->lightmap[sample + 2] / 255.0);

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
