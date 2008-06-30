/**
 * @file patches.c
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

#include "radiosity.h"
#include "../../common/tracing.h"

static vec3_t texture_reflectivity[MAX_MAP_TEXINFO];

/* created planes for origin offset */
static int fakeplanes;

/*
===================================================================
TEXTURE LIGHT VALUES
===================================================================
*/

void CalcTextureReflectivity (void)
{
	int i, j, texels = 0;
	char path[1024];
	int color[3];
	byte *pos;
	float r;
	miptex_t *mt;
	qboolean loaded = qfalse;
	const char *gamedir = FS_GameDir();

	/* always set index 0 even if no textures */
	texture_reflectivity[0][0] = 0.5;
	texture_reflectivity[0][1] = 0.5;
	texture_reflectivity[0][2] = 0.5;

	for (i = 0; i < curTile->numtexinfo; i++) {
		/* see if an earlier texinfo already got the value */
		for (j = 0; j < i; j++) {
			if (!strcmp(curTile->texinfo[i].texture, curTile->texinfo[j].texture)) {
				VectorCopy(texture_reflectivity[j], texture_reflectivity[i]);
				break;
			}
		}
		if (j != i)
			continue;

		/* load the tga file */
		Com_sprintf(path, sizeof(path), "%stextures/%s.tga", gamedir, curTile->texinfo[i].texture);
		if (TryLoadTGA(path, &mt) != -1) {
			/* load rgba from the tga */
			texels = mt->width * mt->height;  /* these are already endian-correct */
			color[0] = color[1] = color[2] = 0;

			for (j = 0; j < texels; j++) {
				pos = ((byte *)mt + mt->offsets[0]) + j * 4;
				color[0] += *pos++; /* r */
				color[1] += *pos++; /* g */
				color[2] += *pos++; /* b */
			}
			free(mt);
			loaded = qtrue;
			Sys_FPrintf(SYS_VRB, "...path: %s (%i) - use tga colors: %i:%i:%i\n", path, texels, color[0], color[1], color[2]);
		}

		if (!loaded) {
			/* try jpeg */
			Com_sprintf(path, sizeof(path), "%stextures/%s.jpg", gamedir, curTile->texinfo[i].texture);
			if (TryLoadJPG(path, &mt) != -1) {
				/* load rgba from the tga */
				texels = mt->width * mt->height;  /* these are already endian-correct */
				color[0] = color[1] = color[2] = 0;

				for (j = 0; j < texels; j++) {
					pos = ((byte *)mt + mt->offsets[0]) + j * 4;
					color[0] += *pos++; /* r */
					color[1] += *pos++; /* g */
					color[2] += *pos++; /* b */
				}
				free(mt);
				loaded = qtrue;
				Sys_FPrintf(SYS_VRB, "...path: %s (%i) - use jpeg colors: %i:%i:%i\n", path, texels, color[0], color[1], color[2]);
			}
		}

		if (!loaded) {
			texture_reflectivity[i][0] = 0.5;
			texture_reflectivity[i][1] = 0.5;
			texture_reflectivity[i][2] = 0.5;
		} else {
			for (j = 0; j < 3; j++) {
				r = color[j] / texels / 255.0;
				texture_reflectivity[i][j] = r;
			}
		}
	}
}

/*
=======================================================================
MAKE FACES
=======================================================================
*/

static winding_t *WindingFromFace (dBspFace_t *f)
{
	int i, v;
	dBspVertex_t *dv;
	winding_t *w;

	w = AllocWinding(f->numedges);
	w->numpoints = f->numedges;

	for (i = 0; i < f->numedges; i++) {
		const int se = curTile->surfedges[f->firstedge + i];
		if (se < 0)
			v = curTile->edges[-se].v[1];
		else
			v = curTile->edges[se].v[0];

		dv = &curTile->vertexes[v];
		VectorCopy(dv->point, w->p[i]);
	}

	RemoveColinearPoints(w);

	return w;
}

/**
 * @brief Check for light emited by texture
 * @note Surface lights
 * @sa TexinfoForBrushTexture
 */
static void BaseLightForFace (dBspFace_t *f, vec3_t color)
{
	const dBspTexinfo_t *tx;

	/* check for light emited by texture */
	tx = &curTile->texinfo[f->texinfo];
	if (!(tx->surfaceFlags & SURF_LIGHT) || tx->value == 0) {
		if (tx->surfaceFlags & SURF_LIGHT)
			Com_Printf("Surface light has 0 intensity (%s).\n", tx->texture);
		VectorClear(color);
		return;
	}

	VectorScale(texture_reflectivity[f->texinfo], tx->value, color);
}

static float totalarea;

static void MakePatchForFace (int facenum, winding_t *w)
{
	dBspFace_t *f;
	float area;
	patch_t *patch;
	dBspPlane_t *pl;
	int i;
	vec3_t color;
	dBspLeaf_t *leaf;

	f = &curTile->faces[facenum];

	area = WindingArea(w);
	totalarea += area;

	patch = &patches[num_patches];
	if (num_patches == MAX_PATCHES)
		Sys_Error("num_patches == MAX_PATCHES");
	patch->next = face_patches[facenum];
	face_patches[facenum] = patch;

	patch->winding = w;

	if (f->side)
		patch->plane = &backplanes[f->planenum];
	else
		patch->plane = &curTile->planes[f->planenum];

	/* origin offset faces must create new planes (moving bmodels - e.g. func_door) */
	if (VectorNotEmpty(face_offset[facenum])) {
		if (curTile->numplanes + fakeplanes >= MAX_MAP_PLANES)
			Sys_Error("numplanes + fakeplanes >= MAX_MAP_PLANES");
		pl = &curTile->planes[curTile->numplanes + fakeplanes];
		fakeplanes++;
		*pl = *(patch->plane);
		pl->dist += DotProduct(face_offset[facenum], pl->normal);
		patch->plane = pl;
	}

	WindingCenter(w, patch->origin);
	VectorAdd(patch->origin, patch->plane->normal, patch->origin);
	leaf = Rad_PointInLeaf(patch->origin);

	patch->area = area;
	if (patch->area <= 1)
		patch->area = 1;

	VectorCopy(texture_reflectivity[f->texinfo], patch->reflectivity);

	VectorClear(patch->baselight);
	VectorClear(patch->totallight);

	/* non-bmodel patches can emit light */
	if (facenum < curTile->models[0].numfaces || (curTile->texinfo[f->texinfo].surfaceFlags & SURF_LIGHT)) {
		BaseLightForFace(f, patch->baselight);

		ColorNormalize(patch->reflectivity, color);

		for (i = 0; i < 3; i++)
			patch->baselight[i] *= color[i];

		VectorCopy(patch->baselight, patch->totallight);
	}
	num_patches++;
}

static entity_t *EntityForModel (int modnum)
{
	int i;
	char name[16];

	Com_sprintf(name, sizeof(name), "*%i", modnum);
	/* search the entities for one using modnum */
	for (i = 0; i < num_entities; i++) {
		const char *s = ValueForKey(&entities[i], "model");
		if (!strcmp(s, name))
			return &entities[i];
	}

	/* return the world */
	return &entities[0];
}

/**
 * @brief turn each face into a single patch
 */
void MakePatches (void)
{
	int i, j, k;
	vec3_t origin;

	Sys_FPrintf(SYS_VRB, "%i faces\n", curTile->numfaces);

	for (i = 0; i < curTile->nummodels; i++) {
		const dBspModel_t *mod = &curTile->models[i];
		const entity_t *ent = EntityForModel(i);
		/* bmodels with origin brushes need to be offset into their
		 * in-use position */
		GetVectorForKey(ent, "origin", origin);

		for (j = 0; j < mod->numfaces; j++) {
			const int facenum = mod->firstface + j;
			dBspFace_t *f = &curTile->faces[facenum];
			winding_t *w = WindingFromFace(f);

			/* store the origin in case of moving bmodels (e.g. func_door) */
			VectorCopy(origin, face_offset[facenum]);

			for (k = 0; k < w->numpoints; k++)
				VectorAdd(w->p[k], origin, w->p[k]);
			MakePatchForFace(facenum, w);
		}
	}

	Sys_FPrintf(SYS_VRB, "%i square feet\n", (int)(totalarea / 64));
}

/*
=======================================================================
SUBDIVIDE
=======================================================================
*/

static void FinishSplit (patch_t *patch, patch_t *newp)
{
	VectorCopy(patch->baselight, newp->baselight);
	VectorCopy(patch->totallight, newp->totallight);
	VectorCopy(patch->reflectivity, newp->reflectivity);
	newp->plane = patch->plane;

	patch->area = WindingArea(patch->winding);
	newp->area = WindingArea(newp->winding);

	if (patch->area <= 1)
		patch->area = 1;
	if (newp->area <= 1)
		newp->area = 1;

	WindingCenter(patch->winding, patch->origin);
	VectorAdd(patch->origin, patch->plane->normal, patch->origin);
	Rad_PointInLeaf(patch->origin);

	WindingCenter(newp->winding, newp->origin);
	VectorAdd(newp->origin, newp->plane->normal, newp->origin);
	Rad_PointInLeaf(newp->origin);
}

/**
 *	@brief Chops the patch by a global grid
 */
static void DicePatch (patch_t *patch)
{
	winding_t *w, *o1, *o2;
	vec3_t mins, maxs;
	vec3_t split;
	vec_t dist;
	int i;
	patch_t *newp;

	w = patch->winding;
	WindingBounds(w, mins, maxs);
	for (i = 0; i < 3; i++)
		if (floor((mins[i] + 1) / config.subdiv) < floor((maxs[i] - 1) / config.subdiv))
			break;
	/* no splitting needed */
	if (i == 3)
		return;

	/* split the winding */
	VectorCopy(vec3_origin, split);
	split[i] = 1;
	dist = config.subdiv * (1 + floor((mins[i] + 1) / config.subdiv));
	ClipWindingEpsilon(w, split, dist, ON_EPSILON, &o1, &o2);

	/* create a new patch */
	if (num_patches == MAX_PATCHES)
		Sys_Error("MAX_PATCHES (%i)", num_patches);
	newp = &patches[num_patches];
	num_patches++;

	newp->next = patch->next;
	patch->next = newp;

	patch->winding = o1;
	newp->winding = o2;

	FinishSplit(patch, newp);

	DicePatch(patch);
	DicePatch(newp);
}

/**
 * @brief subdivide patches to a maximum dimension
 */
void SubdividePatches (void)
{
	int i;
	/* because the list will grow in DicePatch */
	const int num = num_patches;

	if (config.subdiv < 1)
		return;

	for (i = 0; i < num; i++)
		DicePatch(&patches[i]);
	Sys_FPrintf(SYS_VRB, "%i patches after subdivision\n", num_patches);
}
