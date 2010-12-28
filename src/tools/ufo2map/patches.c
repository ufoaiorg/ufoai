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

#include "lighting.h"
#include "bsp.h"
#include "../../shared/images.h"

static vec3_t texture_reflectivity[MAX_MAP_TEXINFO];
patch_t *face_patches[MAX_MAP_FACES];

/*
===================================================================
TEXTURE LIGHT VALUES
===================================================================
*/

/**
 * @brief Calculates the texture color that is used for light emitting surfaces
 */
void CalcTextureReflectivity (void)
{
	int i, j, texels = 0;
	char path[MAX_QPATH];
	int color[3];
	SDL_Surface *surf;

	/* always set index 0 even if no textures */
	VectorSet(texture_reflectivity[0], 0.5, 0.5, 0.5);

	VectorSet(color, 0, 0, 0);

	for (i = 0; i < curTile->numtexinfo; i++) {
		/* see if an earlier texinfo already got the value */
		for (j = 0; j < i; j++) {
			if (Q_streq(curTile->texinfo[i].texture, curTile->texinfo[j].texture)) {
				VectorCopy(texture_reflectivity[j], texture_reflectivity[i]);
				break;
			}
		}
		if (j != i) /* earlier texinfo found, continue */
			continue;

		/* load the image file */
		Com_sprintf(path, sizeof(path), "textures/%s", curTile->texinfo[i].texture);
		if (!(surf = Img_LoadImage(path))) {
			Verb_Printf(VERB_NORMAL, "Couldn't load %s\n", path);
			VectorSet(texture_reflectivity[i], 0.5, 0.5, 0.5);
			continue;
		}

		/* calculate average color */
		texels = surf->w * surf->h;
		color[0] = color[1] = color[2] = 0;

		for(j = 0; j < texels; j++){
			const byte *pos = (byte *)surf->pixels + j * 4;
			color[0] += *pos++; /* r */
			color[1] += *pos++; /* g */
			color[2] += *pos++; /* b */
		}

		Verb_Printf(VERB_EXTRA, "Loaded %s (%dx%d)\n", curTile->texinfo[i].texture, surf->w, surf->h);

		SDL_FreeSurface(surf);

		for(j = 0; j < 3; j++){
			const float r = color[j] / texels / 255.0;
			texture_reflectivity[i][j] = r;
		}
	}
}


static winding_t *WindingFromFace (const dBspSurface_t *f)
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

static inline qboolean HasLight (const dBspSurface_t *f)
{
	const dBspTexinfo_t *tex;

	tex = &curTile->texinfo[f->texinfo];
	return (tex->surfaceFlags & SURF_LIGHT) && tex->value;
}

/**
 * @brief Check for light emitted by texture
 * @note Surface lights
 * @sa TexinfoForBrushTexture
 */
static inline void EmissiveLight (patch_t *patch)
{
	const dBspTexinfo_t *tex = &curTile->texinfo[patch->face->texinfo];
	const vec_t *ref = texture_reflectivity[patch->face->texinfo];

	VectorScale(ref, tex->value, patch->light);
}

/**
 * @brief Build a patch for a surface that emits light
 * @note This is called in the lighting stage
 * @param fn The face number of the surface that emits the light
 * @param w The winding
 * @sa BuildLights
 */
static void BuildPatch (int fn, winding_t *w)
{
	patch_t *patch;
	dBspPlane_t *plane;

	patch = Mem_AllocType(patch_t);

	face_patches[fn] = patch;

	patch->face = &curTile->faces[fn];
	patch->winding = w;

	/* resolve the normal */
	plane = &curTile->planes[patch->face->planenum];

	if (patch->face->side)
		VectorNegate(plane->normal, patch->normal);
	else
		VectorCopy(plane->normal, patch->normal);

	WindingCenter(w, patch->origin);

	/* nudge the origin out along the normal */
	VectorMA(patch->origin, 2.0, patch->normal, patch->origin);

	patch->area = WindingArea(w);

	if (patch->area < 1.0)  /* clamp area */
		patch->area = 1.0;

	EmissiveLight(patch);  /* surface light */
}

static entity_t *EntityForModel (int modnum)
{
	int i;
	char name[16];

	Com_sprintf(name, sizeof(name), "*%i", modnum);
	/* search the entities for one using modnum */
	for (i = 0; i < num_entities; i++) {
		const char *s = ValueForKey(&entities[i], "model");
		if (Q_streq(s, name))
			return &entities[i];
	}

	/* return the world */
	return &entities[0];
}

/**
 * @brief Create surface fragments for light-emitting surfaces so that light sources
 * may be computed along them.
 */
void BuildPatches (void)
{
	int i;

	OBJZERO(face_patches);

	for (i = 0; i < curTile->nummodels; i++) {
		const dBspModel_t *mod = &curTile->models[i];
		const entity_t *ent = EntityForModel(i);
		vec3_t origin;
		int j;
		/* bmodels with origin brushes (like func_door) need to be offset into their
		 * in-use position */
		GetVectorForKey(ent, "origin", origin);

		for (j = 0; j < mod->numfaces; j++) {
			const int facenum = mod->firstface + j;
			dBspSurface_t *f = &curTile->faces[facenum];
			winding_t *w;
			int k;

			/* store the origin in case of moving bmodels (e.g. func_door) */
			VectorCopy(origin, face_offset[facenum]);

			if (!HasLight(f))  /* no light */
				continue;

			w = WindingFromFace(f);

			for (k = 0; k < w->numpoints; k++)
				VectorAdd(w->p[k], origin, w->p[k]);

			BuildPatch(facenum, w);
		}
	}
}

/*
=======================================================================
SUBDIVIDE
=======================================================================
*/

#define PATCH_SUBDIVIDE 64

static void FinishSubdividePatch (patch_t *patch, patch_t *newp)
{
	VectorCopy(patch->normal, newp->normal);

	VectorCopy(patch->light, newp->light);

	patch->area = WindingArea(patch->winding);

	if (patch->area < 1.0)
		patch->area = 1.0;

	newp->area = WindingArea(newp->winding);

	if (newp->area < 1.0)
		newp->area = 1.0;

	WindingCenter(patch->winding, patch->origin);
	/* nudge the patch origin out along the normal */
	VectorMA(patch->origin, 2.0, patch->normal, patch->origin);

	WindingCenter(newp->winding, newp->origin);
	/* nudge the patch origin out along the normal */
	VectorMA(newp->origin, 2.0, newp->normal, newp->origin);
}

/**
 *	@brief Chops the patch by a global grid
 */
static void SubdividePatch(patch_t *patch)
{
	winding_t *w, *o1, *o2;
	vec3_t mins, maxs;
	vec3_t split;
	vec_t dist;
	int i;
	patch_t *newp;

	w = patch->winding;
	WindingBounds(w, mins, maxs);

	VectorClear(split);

	for (i = 0; i < 3; i++) {
		if (floor((mins[i] + 1) / PATCH_SUBDIVIDE) < floor((maxs[i] - 1) / PATCH_SUBDIVIDE)) {
			split[i] = 1.0;
			break;
		}
	}
	/* no splitting needed */
	if (i == 3)
		return;

	/* split the winding */
	dist = PATCH_SUBDIVIDE * (1 + floor((mins[i] + 1) / PATCH_SUBDIVIDE));
	ClipWindingEpsilon(w, split, dist, ON_EPSILON, &o1, &o2);

	/* create a new patch */
	newp = Mem_AllocType(patch_t);

	newp->next = patch->next;
	patch->next = newp;

	patch->winding = o1;
	newp->winding = o2;

	FinishSubdividePatch(patch, newp);

	SubdividePatch(patch);
	SubdividePatch(newp);
}

/**
 * @brief Iterate all of the head face patches, subdividing them as necessary.
 */
void SubdividePatches (void)
{
	int i;

	for (i = 0; i < MAX_MAP_FACES; i++) {
		patch_t *p = face_patches[i];

		if (p)  /* break it up */
			SubdividePatch(p);
	}
}

/**
 * @brief After light sources have been created, patches may be freed.
 */
void FreePatches (void)
{
	int i;

	for (i = 0; i < MAX_MAP_FACES; i++) {
		patch_t *p = face_patches[i];
		while (p) {
			patch_t *pnext = p->next;
			Mem_Free(p);
			p = pnext;
		}
	}
}
