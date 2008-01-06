/**
 * @file r_bsp.c
 * @brief BSP model code
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
#include "r_material.h"

static vec3_t modelorg;			/* relative to viewpoint */

/*
=============================================================
BRUSH MODELS
=============================================================
*/

#define BACKFACE_EPSILON 0.01

/**
 * @brief
 */
static void R_DrawInlineBrushModel (entity_t *e)
{
	int i;
	cBspPlane_t *plane;
	float dot;
	mBspSurface_t *surf;
	mBspSurface_t *opaque_surfaces, *opaque_warp_surfaces;
	mBspSurface_t *alpha_surfaces, *alpha_warp_surfaces;
	mBspSurface_t *material_surfaces;

	opaque_surfaces = opaque_warp_surfaces = NULL;
	alpha_surfaces = alpha_warp_surfaces = NULL;

	material_surfaces = NULL;

	surf = &e->model->bsp.surfaces[e->model->bsp.firstmodelsurface];

	for (i = 0; i < e->model->bsp.nummodelsurfaces; i++, surf++) {
		/* find which side of the node we are on */
		plane = surf->plane;

		dot = DotProduct(modelorg, plane->normal) - plane->dist;

		if (((surf->flags & MSURF_PLANEBACK) && (dot < -BACKFACE_EPSILON)) ||
			(!(surf->flags & MSURF_PLANEBACK) && (dot > BACKFACE_EPSILON))) {

			/* add to appropriate surface chain */
			if (surf->texinfo->flags & (SURF_TRANS33 | SURF_TRANS66)) {
				if (surf->texinfo->flags & SURF_WARP) {
					surf->next = alpha_warp_surfaces;
					alpha_warp_surfaces = surf;
				} else {
					surf->next = alpha_surfaces;
					alpha_surfaces = surf;
				}
			} else {
				if (surf->texinfo->flags & SURF_WARP) {
					surf->next = opaque_warp_surfaces;
					opaque_warp_surfaces = surf;
				} else {
					surf->next = opaque_surfaces;
					opaque_surfaces = surf;
				}
			}

			/* add to the material chain if appropriate */
			if (surf->texinfo->image->material.flags & STAGE_RENDER) {
				surf->materialchain = material_surfaces;
				material_surfaces = surf;
			}
		}
	}

	R_DrawOpaqueSurfaces(opaque_surfaces);

	R_DrawOpaqueWarpSurfaces(opaque_warp_surfaces);

	R_EnableBlend(qtrue);

	R_DrawAlphaSurfaces(alpha_surfaces);

	R_DrawAlphaWarpSurfaces(alpha_warp_surfaces);

	R_DrawMaterialSurfaces(material_surfaces);

	R_EnableBlend(qfalse);
}


/**
 * @brief Draws a brush model
 * @note E.g. a func_breakable or func_door
 */
void R_DrawBrushModel (entity_t * e)
{
	vec3_t mins, maxs;
	int i;
	qboolean rotated;

	if (e->model->bsp.nummodelsurfaces == 0)
		return;

	if (e->angles[0] || e->angles[1] || e->angles[2]) {
		rotated = qtrue;
		for (i = 0; i < 3; i++) {
			mins[i] = e->origin[i] - e->model->radius;
			maxs[i] = e->origin[i] + e->model->radius;
		}
	} else {
		rotated = qfalse;
		VectorAdd(e->origin, e->model->mins, mins);
		VectorAdd(e->origin, e->model->maxs, maxs);
	}

	if (R_CullBox(mins, maxs))
		return;

	VectorSubtract(refdef.vieworg, e->origin, modelorg);
	if (rotated) {
		vec3_t temp;
		vec3_t forward, right, up;

		VectorCopy(modelorg, temp);
		AngleVectors(e->angles, forward, right, up);
		modelorg[0] = DotProduct(temp, forward);
		modelorg[1] = -DotProduct(temp, right);
		modelorg[2] = DotProduct(temp, up);
	}

	qglPushMatrix();
	qglMultMatrixf(e->transform.matrix);

	R_DrawInlineBrushModel(e);

	qglPopMatrix();
}


/*
=============================================================
WORLD MODEL
=============================================================
*/

/**
 * @sa R_DrawWorld
 * @sa R_RecurseWorld
 * @param[in] tile The maptile (map assembly)
 */
static void R_RecursiveWorldNode (mBspNode_t * node, int tile)
{
	int c, side, sidebit;
	cBspPlane_t *plane;
	mBspSurface_t *surf;
	float dot;

	if (node->contents == CONTENTS_SOLID)
		return;					/* solid */

	if (R_CullBox(node->minmaxs, node->minmaxs + 3))
		return;

	/* if a leaf node, draw stuff */
	if (node->contents != LEAFNODE)
		return;

	/* node is just a decision point, so go down the apropriate sides
	 * find which side of the node we are on */
	plane = node->plane;

	if (r_isometric->integer) {
		dot = -DotProduct(vpn, plane->normal);
	} else if (plane->type >= 3) {
		dot = DotProduct(modelorg, plane->normal) - plane->dist;
	} else {
		dot = modelorg[plane->type] - plane->dist;
	}

	if (dot >= 0) {
		side = 0;
		sidebit = 0;
	} else {
		side = 1;
		sidebit = MSURF_PLANEBACK;
	}

	/* recurse down the children, front side first */
	R_RecursiveWorldNode(node->children[side], tile);

	/* draw stuff */
	for (c = node->numsurfaces, surf = rTiles[tile]->bsp.surfaces + node->firstsurface; c; c--, surf++) {
		if ((surf->flags & MSURF_PLANEBACK) != sidebit)
			continue;			/* wrong side */

		/* add to appropriate surface chain */
		if (surf->texinfo->flags & (SURF_TRANS33 | SURF_TRANS66)) {
			if (surf->texinfo->flags & SURF_WARP) {
				surf->next = r_alpha_warp_surfaces;
				r_alpha_warp_surfaces = surf;
			} else {
				surf->next = r_alpha_surfaces;
				r_alpha_surfaces = surf;
			}
		} else {
			if (surf->texinfo->flags & SURF_WARP) {
				surf->next = r_opaque_warp_surfaces;
				r_opaque_warp_surfaces = surf;
			} else {
				surf->next = r_opaque_surfaces;
				r_opaque_surfaces = surf;
			}
		}

		/* add to the material chain if appropriate */
		if (surf->texinfo->image->material.flags & STAGE_RENDER) {
			surf->materialchain = r_material_surfaces;
			r_material_surfaces = surf;
		}
	}

	/* recurse down the back side */
	R_RecursiveWorldNode(node->children[!side], tile);
}

/**
 * @sa R_GetLevelSurfaceChains
 * @param[in] tile The maptile (map assembly)
 */
static void R_RecurseWorld (mBspNode_t * node, int tile)
{
	if (!node->plane) {
		R_RecurseWorld(node->children[0], tile);
		R_RecurseWorld(node->children[1], tile);
	} else {
		R_RecursiveWorldNode(node, tile);
	}
}


/**
 * @brief Fills the surface chains for the current worldlevel and hide other levels
 * @sa cvar cl_worldlevel
 */
void R_GetLevelSurfaceChains (void)
{
	int i, tile, mask;

	/* reset surface chains and regenerate them
	 * even reset them when RDF_NOWORLDMODEL is set - otherwise
	 * there still might be some surfaces in none-world-mode */
	r_opaque_surfaces = r_opaque_warp_surfaces = NULL;
	r_alpha_surfaces = r_alpha_warp_surfaces = NULL;
	r_material_surfaces = NULL;

	if (refdef.rdflags & RDF_NOWORLDMODEL)
		return;

	if (!r_drawworld->integer)
		return;

	mask = 1 << refdef.worldlevel;

	VectorCopy(refdef.vieworg, modelorg);

	for (tile = 0; tile < rNumTiles; tile++) {
		/* don't draw weapon-, actorclip and stepon */
		/* @note Change this to 258 to see the actorclip brushes in-game */
		for (i = 0; i <= LEVEL_LASTVISIBLE; i++) {
			/* check the worldlevel flags */
			if (i && !(i & mask))
				continue;

			if (!rTiles[tile]->bsp.submodels[i].numfaces)
				continue;

			R_RecurseWorld(rTiles[tile]->bsp.nodes + rTiles[tile]->bsp.submodels[i].headnode, tile);
		}
	}
}

void R_CreateSurfacePoly (mBspSurface_t *surf, int shift[3], model_t *mod)
{
	int i, index, nv;
	mBspEdge_t *edge;
	float *vec, *vertsPos;
	float s, t;

	surf->polys = (mBspPoly_t *)VID_TagAlloc(vid_modelPool, sizeof(mBspPoly_t), 0);
	surf->polys->numverts = nv = surf->numedges;

	surf->polys->verts = vertsPos = (float *)VID_TagAlloc(vid_modelPool, nv * sizeof(vec3_t), 0);
	surf->polys->texcoords = (float *)VID_TagAlloc(vid_modelPool, nv * sizeof(vec2_t), 0);

	if (surf->flags & MSURF_LIGHTMAP)  /* allocate for lightmap coords */
		surf->polys->lmtexcoords = (float *)VID_TagAlloc(vid_modelPool, nv * sizeof(vec2_t), 0);

	for (i = 0; i < nv; i++) {
		index = mod->bsp.surfedges[surf->firstedge + i];

		/* vertex */
		if (index > 0) {  /* negative indices to differentiate which end of the edge */
			edge = &mod->bsp.edges[index];
			vec = mod->bsp.vertexes[edge->v[0]].position;
		} else {
			edge = &mod->bsp.edges[-index];
			vec = mod->bsp.vertexes[edge->v[1]].position;
		}

		/* shifting the position for maptiles */
		VectorAdd(vec, shift, vertsPos);
		vertsPos += 3;

		/* texture coordinates */
		s = DotProduct(vec, surf->texinfo->vecs[0]) + surf->texinfo->vecs[0][3];
		s /= surf->texinfo->image->width;

		t = DotProduct(vec, surf->texinfo->vecs[1]) + surf->texinfo->vecs[1][3];
		t /= surf->texinfo->image->height;

		surf->polys->texcoords[i * 2 + 0] = s;
		surf->polys->texcoords[i * 2 + 1] = t;

		if (!(surf->flags & MSURF_LIGHTMAP))
			continue;

		/* lightmap coordinates */
		s = DotProduct(vec, surf->texinfo->vecs[0]) + surf->texinfo->vecs[0][3];
		s -= surf->stmins[0];
		s += surf->light_s << surf->lquant;
		s += 1 << (surf->lquant - 1);
		s /= LIGHTMAP_BLOCK_WIDTH << surf->lquant;

		t = DotProduct(vec, surf->texinfo->vecs[1]) + surf->texinfo->vecs[1][3];
		t -= surf->stmins[1];
		t += surf->light_t << surf->lquant;
		t += 1 << (surf->lquant - 1);
		t /= LIGHTMAP_BLOCK_HEIGHT << surf->lquant;

		surf->polys->lmtexcoords[i * 2 + 0] = s;
		surf->polys->lmtexcoords[i * 2 + 1] = t;
	}
}
