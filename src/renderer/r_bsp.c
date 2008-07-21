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

/*
=============================================================
BRUSH MODELS
=============================================================
*/

#define BACKFACE_EPSILON 0.01

/**
 * @brief
 */
static void R_DrawInlineBrushModel (const entity_t *e, const vec3_t modelorg)
{
	int i;
	float dot;
	mBspSurface_t *surf;

	surf = &e->model->bsp.surfaces[e->model->bsp.firstmodelsurface];

	for (i = 0; i < e->model->bsp.nummodelsurfaces; i++, surf++) {
		/* find which side of the surf we are on  */
		switch (surf->plane->type) {
		case PLANE_X:
		case PLANE_Y:
		case PLANE_Z:
			dot = modelorg[surf->plane->type] - surf->plane->dist;
			break;
		default:
			dot = DotProduct(modelorg, surf->plane->normal) - surf->plane->dist;
			break;
		}

		if (((surf->flags & MSURF_PLANEBACK) && (dot < -BACKFACE_EPSILON)) ||
			(!(surf->flags & MSURF_PLANEBACK) && (dot > BACKFACE_EPSILON)))
			/* visible flag for rendering */
			surf->levelflagToRenderIn = (1 << refdef.worldlevel);
	}

	R_DrawOpaqueSurfaces(e->model->bsp.opaque_surfaces);

	R_DrawOpaqueWarpSurfaces(e->model->bsp.opaque_warp_surfaces);

	R_EnableBlend(qtrue);

	R_DrawBlendSurfaces(e->model->bsp.blend_surfaces);

	R_DrawBlendWarpSurfaces(e->model->bsp.blend_warp_surfaces);

	R_DrawAlphaTestSurfaces(e->model->bsp.alpha_test_surfaces);

	R_DrawMaterialSurfaces(e->model->bsp.material_surfaces);

	R_EnableBlend(qfalse);
}


/**
 * @brief Draws a brush model
 * @note E.g. a func_breakable or func_door
 */
void R_DrawBrushModel (const entity_t * e)
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

	VectorCopy(refdef.vieworg, modelorg);
	if (rotated) {
		vec3_t temp;
		vec3_t forward, right, up;

		VectorCopy(modelorg, temp);
		AngleVectors(e->angles, forward, right, up);
		modelorg[0] = DotProduct(temp, forward);
		modelorg[1] = -DotProduct(temp, right);
		modelorg[2] = DotProduct(temp, up);
	}
	VectorSubtract(modelorg, e->origin, modelorg);

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
		R_EntityDrawBBox(bbox);
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
		dot = -DotProduct(r_vpn, node->plane->normal);
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
		if ((surf->flags & MSURF_PLANEBACK) == sidebit)
			surf->levelflagToRenderIn = (1 << refdef.worldlevel);
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

	if (refdef.rdflags & RDF_NOWORLDMODEL)
		return;

	if (!r_drawworld->integer)
		return;

	mask = 1 << refdef.worldlevel;

	for (tile = 0; tile < r_numMapTiles; tile++) {
		/* don't draw weaponclip, actorclip and stepon */
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
				/** @todo numfaces and headnode might get screwed up in some cases (segfault) */
				if (r_mapTiles[tile]->bsp.submodels[i].numfaces <= 0)
					continue;

				R_RecurseWorld(r_mapTiles[tile]->bsp.nodes + r_mapTiles[tile]->bsp.submodels[i].headnode, tile);
			}
		}
	}
}
