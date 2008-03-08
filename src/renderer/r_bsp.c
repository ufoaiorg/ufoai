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

#define R_SurfaceToSurfaces(surfs, surf)\
	(surfs)->surfaces[(surfs)->count++] = surf

/*
=============================================================
BRUSH MODELS
=============================================================
*/

#define BACKFACE_EPSILON 0.01

/**
 * @brief
 */
static void R_DrawInlineBrushModel (entity_t *e, vec3_t modelorg)
{
	int i;
	float dot;
	mBspSurface_t *surf;
	mBspSurfaces_t opaque_surfaces, opaque_warp_surfaces;
	mBspSurfaces_t alpha_surfaces, alpha_warp_surfaces;
	mBspSurfaces_t material_surfaces;

	opaque_surfaces.count = opaque_warp_surfaces.count = 0;
	alpha_surfaces.count = alpha_warp_surfaces.count = 0;
	material_surfaces.count = 0;

	surf = &e->model->bsp.surfaces[e->model->bsp.firstmodelsurface];

	for (i = 0; i < e->model->bsp.nummodelsurfaces; i++, surf++) {

		/* find which side of the surf we are on  */
		switch(surf->plane->type){
		case PLANE_X:
			dot = modelorg[0] - surf->plane->dist;
			break;
		case PLANE_Y:
			dot = modelorg[1] - surf->plane->dist;
			break;
		case PLANE_Z:
			dot = modelorg[2] - surf->plane->dist;
			break;
		default:
			dot = DotProduct(modelorg, surf->plane->normal) - surf->plane->dist;
			break;
		}

		if (((surf->flags & MSURF_PLANEBACK) && (dot < -BACKFACE_EPSILON)) ||
			(!(surf->flags & MSURF_PLANEBACK) && (dot > BACKFACE_EPSILON))) {

			/* add to appropriate surface chain */
			if (surf->texinfo->flags & (SURF_TRANS33 | SURF_TRANS66)) {
				if (surf->texinfo->flags & SURF_WARP)
					R_SurfaceToSurfaces(&alpha_warp_surfaces, surf);
				else
					R_SurfaceToSurfaces(&alpha_surfaces, surf);
			} else {
				if (surf->texinfo->flags & SURF_WARP)
					R_SurfaceToSurfaces(&opaque_warp_surfaces, surf);
				else
					R_SurfaceToSurfaces(&opaque_surfaces, surf);
			}

			/* add to the material chain if appropriate */
			if (surf->texinfo->image->material.flags & STAGE_RENDER)
				R_SurfaceToSurfaces(&material_surfaces, surf);
		}
	}

	R_DrawOpaqueSurfaces(&opaque_surfaces);

	R_DrawOpaqueWarpSurfaces(&opaque_warp_surfaces);

	R_EnableBlend(qtrue);

	R_DrawAlphaSurfaces(&alpha_surfaces);

	R_DrawAlphaWarpSurfaces(&alpha_warp_surfaces);

	R_DrawMaterialSurfaces(&material_surfaces);

	R_EnableBlend(qfalse);
}


/**
 * @brief Draws a brush model
 * @note E.g. a func_breakable or func_door
 */
void R_DrawBrushModel (entity_t * e)
{
	/* relative to viewpoint */
	vec3_t modelorg;
	vec3_t mins, maxs;
	int i;
	qboolean rotated;

	if (e->model->bsp.nummodelsurfaces == 0)
		return;

	if (VectorNotEmpty(e->angles)) {
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

	R_DrawInlineBrushModel(e, modelorg);

	/* show model bounding box */
	if (r_showbox->integer) {
		vec4_t bbox[8];
		vec4_t tmp;

		/* compute a full bounding box */
		for (i = 0; i < 8; i++) {
			tmp[0] = (i & 1) ? e->model->mins[0] : e->model->maxs[0];
			tmp[1] = (i & 2) ? e->model->mins[1] : e->model->maxs[1];
			tmp[2] = (i & 4) ? e->model->mins[2] : e->model->maxs[2];
			tmp[3] = 1.0;

			Vector4Copy(tmp, bbox[i]);
		}
		R_EntityDrawlBBox(bbox);
	}

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

	if (r_isometric->integer) {
		dot = -DotProduct(vpn, node->plane->normal);
	} else if (node->plane->type >= 3) {
		dot = DotProduct(refdef.vieworg, node->plane->normal) - node->plane->dist;
	} else {
		dot = refdef.vieworg[node->plane->type] - node->plane->dist;
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
	for (c = node->numsurfaces, surf = r_mapTiles[tile]->bsp.surfaces + node->firstsurface; c; c--, surf++) {
		if ((surf->flags & MSURF_PLANEBACK) != sidebit)
			continue;			/* wrong side */

		/* add to appropriate surfaces list */
		if (surf->texinfo->flags & (SURF_TRANS33 | SURF_TRANS66)) {
			if (surf->texinfo->flags & SURF_WARP)
				R_SurfaceToSurfaces(&r_alpha_warp_surfaces, surf);
			else
				R_SurfaceToSurfaces(&r_alpha_surfaces, surf);
		} else {
			if (surf->texinfo->flags & SURF_WARP)
				R_SurfaceToSurfaces(&r_opaque_warp_surfaces, surf);
			else
				R_SurfaceToSurfaces(&r_opaque_surfaces, surf);
		}

		/* add to the material list if appropriate */
		if (surf->texinfo->image->material.flags & STAGE_RENDER)
			R_SurfaceToSurfaces(&r_material_surfaces, surf);
	}

	/* recurse down the back side */
	R_RecursiveWorldNode(node->children[!side], tile);
}

/**
 * @sa R_GetLevelSurfaceLists
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
void R_GetLevelSurfaceLists (void)
{
	int i, tile, mask;

	/* reset surface chains and regenerate them
	 * even reset them when RDF_NOWORLDMODEL is set - otherwise
	 * there still might be some surfaces in none-world-mode */
	r_opaque_surfaces.count = r_opaque_warp_surfaces.count = 0;
	r_alpha_surfaces.count = r_alpha_warp_surfaces.count = 0;
	r_material_surfaces.count = 0;

	if (refdef.rdflags & RDF_NOWORLDMODEL)
		return;

	if (!r_drawworld->integer)
		return;

	mask = 1 << refdef.worldlevel;

	for (tile = 0; tile < r_numMapTiles; tile++) {
		/* don't draw weapon-, actorclip and stepon */
		for (i = 0; i <= LEVEL_LASTVISIBLE; i++) {
			/* check the worldlevel flags */
			if (i && !(i & mask))
				continue;

			if (!r_mapTiles[tile]->bsp.submodels[i].numfaces)
				continue;

			R_RecurseWorld(r_mapTiles[tile]->bsp.nodes + r_mapTiles[tile]->bsp.submodels[i].headnode, tile);
		}
		if (r_drawspecialbrushes->integer) {
			for (; i < LEVEL_MAX; i++) {
				if (!r_mapTiles[tile]->bsp.submodels[i].numfaces)
					continue;

				R_RecurseWorld(r_mapTiles[tile]->bsp.nodes + r_mapTiles[tile]->bsp.submodels[i].headnode, tile);
			}
		}
	}
}
