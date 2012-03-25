/**
 * @file textures.c
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


#include "bsp.h"
#include "textures.h"

static int nummiptex = 0;
textureref_t textureref[MAX_MAP_TEXTURES];

/**
 * @brief
 * @return -1 means that the texture was not found
 * @sa TexinfoForBrushTexture
 * @sa ParseBrush
 */
int FindMiptex (const char *name)
{
	int i;

	/* search through textures that have already been loaded. */
	for (i = 0; i < nummiptex; i++)
		if (Q_streq(name, textureref[i].name)) {
			return i;
		}
	if (nummiptex == MAX_MAP_TEXTURES)
		Sys_Error("MAX_MAP_TEXTURES");
	Q_strncpyz(textureref[i].name, name, sizeof(textureref[i].name));

	return i;
}


static const vec3_t baseaxis[18] =
{
{0,0,1}, {1,0,0}, {0,-1,0},			/* floor */
{0,0,-1}, {1,0,0}, {0,-1,0},		/* ceiling */
{1,0,0}, {0,1,0}, {0,0,-1},			/* west wall */
{-1,0,0}, {0,1,0}, {0,0,-1},		/* east wall */
{0,1,0}, {1,0,0}, {0,0,-1},			/* south wall */
{0,-1,0}, {1,0,0}, {0,0,-1}			/* north wall */
};

static void TextureAxisFromPlane (plane_t *pln, vec3_t xv, vec3_t yv, qboolean isTerrain)
{
	int bestaxis, numaxis, i;
	vec_t best;

	/* Knightmare- terrain support, use floor/ceiling axis only */
	numaxis = (isTerrain) ? 2 : 6;

	best = 0;
	bestaxis = 0;

	for (i = 0; i < numaxis; i++) {
		const vec_t dot = DotProduct(pln->normal, baseaxis[i * 3]);
		if (dot > best) {
			best = dot;
			bestaxis = i;
		}
	}

	VectorCopy(baseaxis[bestaxis * 3 + 1], xv);
	VectorCopy(baseaxis[bestaxis * 3 + 2], yv);
}


/**
 * @sa BaseLightForFace
 */
int TexinfoForBrushTexture (plane_t *plane, brush_texture_t *bt, const vec3_t origin, qboolean isTerrain)
{
	vec3_t vecs[2];
	int sv, tv;
	vec_t ang, sinv, cosv;
	dBspTexinfo_t tx, *tc;
	int i, j, k;
	float shift[2];
	vec3_t scaledOrigin;

	if (!bt->name[0])
		return 0;

	OBJZERO(tx);
	Q_strncpyz(tx.texture, bt->name, sizeof(tx.texture));

	TextureAxisFromPlane(plane, vecs[0], vecs[1], isTerrain);

	/* dot product of a vertex location with the [4] part will produce a
	 * texcoord (s or t depending on the first index) */
	VectorScale(origin, 1.0 / bt->scale[0], scaledOrigin);
	shift[0] = DotProduct(scaledOrigin, vecs[0]);
	VectorScale(origin, 1.0 / bt->scale[1], scaledOrigin);
	shift[1] = DotProduct(scaledOrigin, vecs[1]);

	if (!bt->scale[0])
		bt->scale[0] = 1;
	if (!bt->scale[1])
		bt->scale[1] = 1;

	/* rotate axis */
	if (bt->rotate == 0) {
		sinv = 0;
		cosv = 1;
	} else if (bt->rotate == 90) {
		sinv = 1;
		cosv = 0;
	} else if (bt->rotate == 180) {
		sinv = 0;
		cosv = -1;
	} else if (bt->rotate == 270) {
		sinv = -1;
		cosv = 0;
	} else {
		ang = bt->rotate * torad;
		sinv = sin(ang);
		cosv = cos(ang);
	}

	shift[0] = cosv * shift[0] - sinv * shift[1];
	shift[1] = sinv * shift[0] + cosv * shift[1];

	if (vecs[0][0])
		sv = 0;
	else if (vecs[0][1])
		sv = 1;
	else
		sv = 2;

	if (vecs[1][0])
		tv = 0;
	else if (vecs[1][1])
		tv = 1;
	else
		tv = 2;

	for (i = 0; i < 2; i++) {
		const vec_t ns = cosv * vecs[i][sv] - sinv * vecs[i][tv];
		const vec_t nt = sinv * vecs[i][sv] + cosv * vecs[i][tv];
		vecs[i][sv] = ns;
		vecs[i][tv] = nt;
	}

	for (i = 0; i < 2; i++)
		for (j = 0; j < 3; j++)
			tx.vecs[i][j] = vecs[i][j] / bt->scale[i];

	/* texture offsets */
	tx.vecs[0][3] = bt->shift[0] + shift[0];
	tx.vecs[1][3] = bt->shift[1] + shift[1];

	tx.surfaceFlags = bt->surfaceFlags;
	tx.value = bt->value;

	/* find the texinfo */
	tc = curTile->texinfo;
	for (i = 0; i < curTile->numtexinfo; i++, tc++) {
		if (tc->surfaceFlags != tx.surfaceFlags)
			continue;
		if (tc->value != tx.value)
			continue;
		if (!Q_streq(tc->texture, tx.texture))
			continue;
		for (j = 0; j < 2; j++) {
			for (k = 0; k < 4; k++) {
				if (tc->vecs[j][k] != tx.vecs[j][k])
					goto skip;
			}
		}
		return i;
skip:;
	}
	if (curTile->numtexinfo >= MAX_MAP_TEXINFO)
		Sys_Error("MAX_MAP_TEXINFO overflow");
	*tc = tx;
	curTile->numtexinfo++;

	return i;
}
