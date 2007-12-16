/**
 * @file r_light.c
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

static int r_dlightframecount;

#define	DLIGHT_CUTOFF	64

/*
=============================================================================
DYNAMIC LIGHTS
=============================================================================
*/

void R_MarkLights (dlight_t * light, int bit, mBspNode_t * node)
{
	cBspPlane_t *splitplane;
	float dist;
	mBspSurface_t *surf;
	int i, sidebit;

	if (node->contents != LEAFNODE)
		return;

	splitplane = node->plane;
	dist = DotProduct(light->origin, splitplane->normal) - splitplane->dist;

	if (dist > light->intensity - DLIGHT_CUTOFF) {
		R_MarkLights(light, bit, node->children[0]);
		return;
	}
	if (dist < -light->intensity + DLIGHT_CUTOFF) {
		R_MarkLights(light, bit, node->children[1]);
		return;
	}

	/* mark the polygons */
	/* FIXME: Go through other rTiles, too */
	surf = rTiles[0]->bsp.surfaces + node->firstsurface;
	for (i = 0; i < node->numsurfaces; i++, surf++) {
		/*Discoloda */
		dist = DotProduct(light->origin, surf->plane->normal) - surf->plane->dist;
		if (dist >= 0)
			sidebit = 0;
		else
			sidebit = SURF_PLANEBACK;

		if ((surf->flags & SURF_PLANEBACK) != sidebit)
			continue;

		if (surf->dlightframe != r_dlightframecount) {
			surf->dlightbits = 0;
			surf->dlightframe = r_dlightframecount;
		}
		surf->dlightbits |= bit;
	}

	R_MarkLights(light, bit, node->children[0]);
	R_MarkLights(light, bit, node->children[1]);
}

/*
=============================================================================
LIGHT SAMPLING
=============================================================================
*/

static float s_blocklights[256 * 256 * 4];

static void R_AddDynamicLights (mBspSurface_t * surf)
{
	int lnum;
	int sd, td;
	float fdist, frad, fminlight;
	vec3_t impact, local;
	int s, t;
	int i;
	int smax, tmax;
	mBspTexInfo_t *tex;
	dlight_t *dl;
	float *pfBL;
	float fsacc, ftacc;

	smax = (surf->extents[0] >> surf->lquant) + 1;
	tmax = (surf->extents[1] >> surf->lquant) + 1;
	tex = surf->texinfo;

	for (lnum = 0; lnum < refdef.num_dlights; lnum++) {
		if (!(surf->dlightbits & (1 << lnum)))
			continue;			/* not lit by this light */

		dl = &refdef.dlights[lnum];
		frad = dl->intensity;
		fdist = DotProduct(dl->origin, surf->plane->normal) - surf->plane->dist;
		frad -= fabs(fdist);
		/* rad is now the highest intensity on the plane */

		fminlight = DLIGHT_CUTOFF;	/* FIXME: make configurable? */
		if (frad < fminlight)
			continue;
		fminlight = frad - fminlight;

		for (i = 0; i < 3; i++)
			impact[i] = dl->origin[i] - surf->plane->normal[i] * fdist;

		local[0] = DotProduct(impact, tex->vecs[0]) + tex->vecs[0][3] - surf->texturemins[0];
		local[1] = DotProduct(impact, tex->vecs[1]) + tex->vecs[1][3] - surf->texturemins[1];

		pfBL = s_blocklights;
		for (t = 0, ftacc = 0; t < tmax; t++, ftacc += 8) {
			td = local[1] - ftacc;
			if (td < 0)
				td = -td;

			for (s = 0, fsacc = 0; s < smax; s++, fsacc += 8, pfBL += 3) {
				sd = Q_ftol(local[0] - fsacc);

				if (sd < 0)
					sd = -sd;

				if (sd > td)
					fdist = sd + (td >> 1);
				else
					fdist = td + (sd >> 1);

				if (fdist < fminlight) {
					pfBL[0] += (frad - fdist) * dl->color[0];
					pfBL[1] += (frad - fdist) * dl->color[1];
					pfBL[2] += (frad - fdist) * dl->color[2];
				}
			}
		}
	}
}

/**
 * @brief Combine and scale multiple lightmaps into the floating format in blocklights
 */
void R_BuildLightMap (mBspSurface_t * surf, byte * dest, int stride)
{
	unsigned int smax, tmax;
	int r, g, b, a, max;
	unsigned int i, j, size;
	byte *lightmap, *lm;
	float scale[4];
	int nummaps;
	float *bl;
	lightstyle_t *style;
	int maps;

	/* no rad processing - for map testing */
	if (!surf->lquant) {
		Com_Printf("R_BuildLightMap - no lightmap\n");
		return;
	}

	if (surf->texinfo->flags & (SURF_TRANS33 | SURF_TRANS66 | SURF_WARP))
		Com_Error(ERR_DROP, "R_BuildLightMap called for non-lit surface");

	smax = (surf->extents[0] >> surf->lquant) + 1;
	tmax = (surf->extents[1] >> surf->lquant) + 1;
	size = smax * tmax;
	if (size > (sizeof(s_blocklights) >> surf->lquant)) {
		Com_Error(ERR_DROP, "Bad s_blocklights size (%i) - should be "UFO_SIZE_T" (lquant: %i) (smax: %i, extents[0]: %i, tmax: %i, extents[1]: %i)\n",
			size, (sizeof(s_blocklights) >> surf->lquant), surf->lquant, smax, surf->extents[0], tmax, surf->extents[1]);
	}

	/* set to full bright if no light data */
	if (!surf->samples) {
		memset(s_blocklights, 255, sizeof(s_blocklights));
		for (maps = 0; maps < MAXLIGHTMAPS && surf->styles[maps] != 255; maps++)
			style = &refdef.lightstyles[surf->styles[maps]];

		goto store;
	}

	/* count the # of maps */
	for (nummaps = 0; nummaps < MAXLIGHTMAPS && surf->styles[nummaps] != 255; nummaps++);

	lightmap = surf->samples;

	/* only add one lightmap */
	if (nummaps != 1)
		memset(s_blocklights, 0, sizeof(s_blocklights[0]) * size * RGB_PIXELSIZE);

	for (maps = 0; maps < MAXLIGHTMAPS && surf->styles[maps] != 255; maps++) {
		bl = s_blocklights;

		VectorScale(refdef.lightstyles[surf->styles[maps]].rgb, r_modulate->value, scale);

		for (i = 0; i < size; i++, bl += RGB_PIXELSIZE) {
			if (maps > 0) {
				bl[0] += lightmap[i * RGB_PIXELSIZE + 0] * scale[0];
				bl[1] += lightmap[i * RGB_PIXELSIZE + 1] * scale[1];
				bl[2] += lightmap[i * RGB_PIXELSIZE + 2] * scale[2];
			} else {
				bl[0] = lightmap[i * RGB_PIXELSIZE + 0] * scale[0];
				bl[1] = lightmap[i * RGB_PIXELSIZE + 1] * scale[1];
				bl[2] = lightmap[i * RGB_PIXELSIZE + 2] * scale[2];
			}
		}
		lightmap += size * RGB_PIXELSIZE;	/* skip to next lightmap */
	}

	/* add all the dynamic lights */
	if (surf->dlightframe == r_framecount)
		R_AddDynamicLights(surf);

	/* put into texture format */
  store:
	stride -= (smax << 2);
	bl = s_blocklights;

	/* first into an rgba linear block for softening */
	lightmap = (byte *)VID_TagAlloc(vid_lightPool, size * 4, 0);
	lm = lightmap;

	for (i = 0; i < tmax; i++) {
		for (j = 0; j < smax; j++) {
			r = Q_ftol(bl[0]);
			g = Q_ftol(bl[1]);
			b = Q_ftol(bl[2]);

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

			/* alpha is ONLY used for the mono lightmap case.  For this reason
			 * we set it to the brightest of the color components so that
			 * things don't get too dim. */
			a = max;

			/* rescale all the color components if the intensity of the greatest
			 * channel exceeds 1.0 */
			if (max > 255) {
				float t = 255.0F / max;

				r = r * t;
				g = g * t;
				b = b * t;
				a = a * t;
			}

			lm[0] = r;
			lm[1] = g;
			lm[2] = b;
			lm[3] = a;

			bl += RGB_PIXELSIZE;
			lm += 4;
		}
	}

	/* soften it if it's sufficiently large */
	if (r_soften->integer && size > 1024)
		for (i = 0; i < 4; i++)
			R_SoftenTexture(lightmap, smax, tmax, 4);

	/* then copy into the final strided lightmap block */
	lm = lightmap;
	for (i = 0; i < tmax; i++, dest += stride) {
		for (j = 0; j < smax; j++) {
			dest[0] = lm[0];
			dest[1] = lm[1];
			dest[2] = lm[2];
			dest[3] = lm[3];

			lm += 4;
			dest += 4;
		}
	}

	VID_MemFree(lightmap);
}

static vec3_t pointcolor;
static cBspPlane_t *lightplane;			/* used as shadow plane */
static vec3_t lightspot;

static int RecursiveLightPoint (model_t* mapTile, mBspNode_t * node, vec3_t start, vec3_t end)
{
	float front, back, frac;
	int side;
	cBspPlane_t *plane;
	vec3_t mid;
	mBspSurface_t *surf;
	int s, t, ds, dt;
	int i;
	mBspTexInfo_t *tex;
	byte *lightmap;
	int maps;
	int r;

	/* didn't hit anything */
	if (node->contents != LEAFNODE)
		return -1;

	/* calculate mid point */
	plane = node->plane;
	if (plane->type < 3) {
		front = start[plane->type] - plane->dist;
		back = end[plane->type] - plane->dist;
	} else {
		front = DotProduct(start, plane->normal) - plane->dist;
		back = DotProduct(end, plane->normal) - plane->dist;
	}
	side = front < 0;

	if ((back < 0) == side)
		return RecursiveLightPoint(mapTile, node->children[side], start, end);

	frac = front / (front - back);
	mid[0] = start[0] + (end[0] - start[0]) * frac;
	mid[1] = start[1] + (end[1] - start[1]) * frac;
	mid[2] = start[2] + (end[2] - start[2]) * frac;

	/* go down front side */
	r = RecursiveLightPoint(mapTile, node->children[side], start, mid);
	if (r >= 0)
		return r;				/* hit something */

	if ((back < 0) == side)
		return -1;				/* didn't hit anything */

	/* check for impact on this node */
	VectorCopy(mid, lightspot);
	lightplane = plane;

	surf = mapTile->bsp.surfaces + node->firstsurface;

	for (i = 0; i < node->numsurfaces; i++, surf++) {
		if (surf->flags & SURF_DRAWTURB)
			continue;			/* no lightmaps */

		tex = surf->texinfo;

		s = Q_ftol(DotProduct(mid, tex->vecs[0]) + tex->vecs[0][3]);
		t = Q_ftol(DotProduct(mid, tex->vecs[1]) + tex->vecs[1][3]);

		if (s < surf->texturemins[0] || t < surf->texturemins[1])
			continue;

		ds = s - surf->texturemins[0];
		dt = t - surf->texturemins[1];

		if (ds > surf->extents[0] || dt > surf->extents[1])
			continue;

		if (!surf->samples)
			return 0;

		ds >>= 4;
		dt >>= 4;

		lightmap = surf->samples;
		VectorClear(pointcolor);
		if (lightmap) {
			vec3_t scale;

			lightmap += 3 * (dt * ((surf->extents[0] >> 4) + 1) + ds);

			for (maps = 0; maps < MAXLIGHTMAPS && surf->styles[maps] != 255; maps++) {
				VectorScale(refdef.lightstyles[surf->styles[maps]].rgb, r_modulate->value, scale);
				for (i = 0; i < 3; i++)
					pointcolor[i] += lightmap[i] * scale[i] * (1.0f/255.0f);

				lightmap += 3 * ((surf->extents[0] >> 4) + 1) * ((surf->extents[1] >> 4) + 1);
			}
		}

		return 1;
	}

	/* go down back side */
	return RecursiveLightPoint(mapTile, node->children[!side], mid, end);
}

void R_LightPoint (vec3_t p, vec3_t color)
{
	vec3_t end;
	float r;
	int lnum, i, num;
	dlight_t *dl;
	float light;
	vec3_t dist;
	float add;

	end[0] = p[0];
	end[1] = p[1];
	end[2] = p[2] - 2048;

	VectorClear(color);

	/* FIXME */
	for (num = 0; num < /*rNumTiles*/ 1; num++) {
		if (!rTiles[num]->bsp.lightdata)
			continue;

		r = RecursiveLightPoint(rTiles[num], rTiles[num]->bsp.nodes, p, end);

		if (r != -1)
			VectorMA(color, 1.1, pointcolor, color);

		/* this catches too bright modulated color */
		for (i = 0; i < 3; i++)
			if (color[i] > 1)
				color[i] = 1;

		/* add dynamic lights */
		light = 0;
		dl = refdef.dlights;
		for (lnum = 0; lnum < refdef.num_dlights; lnum++, dl++) {
			VectorSubtract(currententity->origin, dl->origin, dist);
			add = dl->intensity - VectorLength(dist);
			add /= 64.0f;
			if (add > 0)
				VectorMA(color, add, dl->color, color);
		}
	}
}
