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
#include "r_light.h"
#include "r_draw.h"

/*
=============================================================
BRUSH MODELS
=============================================================
*/

#define BACKFACE_EPSILON 0.01


/**
 * @brief Returns true if the specified bounding box is completely culled by the
 * view frustum, false otherwise.
 * @param[in] mins The mins of the bounding box
 * @param[in] maxs The maxs of the bounding box
 */
static qboolean R_CullBox (const vec3_t mins, const vec3_t maxs)
{
	int i;

	if (r_nocull->integer)
		return qfalse;

	for (i = lengthof(r_locals.frustum) - 1; i >= 0; i--)
		if (TR_BoxOnPlaneSide(mins, maxs, &r_locals.frustum[i]) == PSIDE_BACK)
			return qtrue;
	return qfalse;
}


/**
 * @brief Performs a spherical frustum check
 * @param[in] centre The world coordinate that is the center of the sphere
 * @param[in] radius The radius of the sphere to check the frustum for
 * @param[in] clipflags Can be used to skip sides of the frustum planes
 * @return @c true if the sphere is completely outside the frustum, @c false otherwise
 */
qboolean R_CullSphere (const vec3_t centre, const float radius, const unsigned int clipflags)
{
	unsigned int i;
	unsigned int bit;
	const cBspPlane_t *p;

	if (r_nocull->integer)
		return qfalse;

	for (i = lengthof(r_locals.frustum), bit = 1, p = r_locals.frustum; i > 0; i--, bit <<= 1, p++) {
		if (!(clipflags & bit))
			continue;
		if (DotProduct(centre, p->normal) - p->dist <= -radius)
			return qtrue;
	}

	return qfalse;
}

/**
 * @brief Returns true if the specified entity is completely culled by the view
 * frustum, false otherwise.
 * @param[in] e The entity to check
 * @sa R_CullBox
 */
qboolean R_CullBspModel (const entity_t *e)
{
	vec3_t mins, maxs;

	/* no surfaces */
	if (!e->model->bsp.nummodelsurfaces)
		return qtrue;

	if (e->isOriginBrushModel) {
		int i;
		for (i = 0; i < 3; i++) {
			mins[i] = e->origin[i] - e->model->radius;
			maxs[i] = e->origin[i] + e->model->radius;
		}
	} else {
		VectorAdd(e->origin, e->model->mins, mins);
		VectorAdd(e->origin, e->model->maxs, maxs);
	}

	return R_CullBox(mins, maxs);
}

/**
 * @brief Renders all the surfaces that belongs to an inline bsp model entity
 * @param[in] e The inline bsp model entity
 * @param[in] modelorg relative to viewpoint
 */
static void R_DrawBspModelSurfaces (const entity_t *e, const vec3_t modelorg)
{
	int i;
	mBspSurface_t *surf;

	/* temporarily swap the view frame so that the surface drawing
	 * routines pickup only the bsp model's surfaces */
	const int f = r_locals.frame;
	r_locals.frame = -1;

	surf = &e->model->bsp.surfaces[e->model->bsp.firstmodelsurface];

	for (i = 0; i < e->model->bsp.nummodelsurfaces; i++, surf++) {
		/** @todo This leads to unrendered backfaces for e.g. doors */
#if 0
		float dot;
		/* find which side of the surf we are on  */
		if (AXIAL(surf->plane))
			dot = modelorg[surf->plane->type] - surf->plane->dist;
		else
			dot = DotProduct(modelorg, surf->plane->normal) - surf->plane->dist;

		if (((surf->flags & MSURF_PLANEBACK) && (dot < -BACKFACE_EPSILON)) ||
			(!(surf->flags & MSURF_PLANEBACK) && (dot > BACKFACE_EPSILON)))
#endif
			/* visible flag for rendering */
			surf->frame = r_locals.frame;
	}

	R_DrawOpaqueSurfaces(e->model->bsp.opaque_surfaces);

	R_DrawOpaqueWarpSurfaces(e->model->bsp.opaque_warp_surfaces);

	R_DrawAlphaTestSurfaces(e->model->bsp.alpha_test_surfaces);

	R_EnableBlend(qtrue);

	R_DrawMaterialSurfaces(e->model->bsp.material_surfaces);

	R_DrawFlareSurfaces(e->model->bsp.flare_surfaces);

	R_EnableFog(qfalse);

	R_DrawBlendSurfaces(e->model->bsp.blend_surfaces);

	R_DrawBlendWarpSurfaces(e->model->bsp.blend_warp_surfaces);

	R_EnableFog(qtrue);

	R_EnableBlend(qfalse);

	/* undo the swap */
	r_locals.frame = f;
}

/**
 * @brief Draws a brush model
 * @param[in] e The inline bsp model entity
 * @note E.g. a func_breakable or func_door
 */
void R_DrawBrushModel (const entity_t * e)
{
	/* relative to viewpoint */
	vec3_t modelorg;

	/* set the relative origin, accounting for rotation if necessary */
	VectorSubtract(refdef.viewOrigin, e->origin, modelorg);
	if (VectorNotEmpty(e->angles)) {
		vec3_t rotationMatrix[3];
		VectorCreateRotationMatrix(e->angles, rotationMatrix);
		VectorRotatePoint(modelorg, rotationMatrix);
	}

	R_ShiftLights(e->origin);

	glPushMatrix();

	glTranslatef(e->origin[0], e->origin[1], e->origin[2]);
	glRotatef(e->angles[YAW], 0, 0, 1);
	glRotatef(e->angles[PITCH], 0, 1, 0);
	glRotatef(e->angles[ROLL], 1, 0, 0);

	R_DrawBspModelSurfaces(e, modelorg);

	/* show model bounding box */
	if (r_showbox->integer) {
		const model_t *model = e->model;
		R_DrawBoundingBox(model->mins, model->maxs);
	}

	glPopMatrix();

	R_ShiftLights(vec3_origin);
}


/*
=============================================================
WORLD MODEL
=============================================================
*/

/**
 * @brief Developer tool for viewing BSP vertex normals. Only Phong interpolated
 * surfaces show their normals when r_shownormals > 1.
 */
void R_DrawBspNormals (int tile)
{
	int i, j, k;
	const mBspSurface_t *surf;
	const mBspModel_t *bsp;
	const vec4_t color = {1.0, 0.0, 0.0, 1.0};

	if (!r_shownormals->integer)
		return;

	R_EnableTexture(&texunit_diffuse, qfalse);

	R_ResetArrayState();  /* default arrays */

	R_Color(color);

	k = 0;
	bsp = &r_mapTiles[tile]->bsp;
	surf = bsp->surfaces;
	for (i = 0; i < bsp->numsurfaces; i++, surf++) {
		if (surf->frame != r_locals.frame)
			continue; /* not visible */

		if (surf->texinfo->flags & SURF_WARP)
			continue;  /* don't care */

		if (r_shownormals->integer > 1 && !(surf->texinfo->flags & SURF_PHONG))
			continue;  /* don't care */

		/* avoid overflows, draw in batches */
		if (k > r_state.array_size - 512) {
			glDrawArrays(GL_LINES, 0, k / 3);
			k = 0;
		}

		for (j = 0; j < surf->numedges; j++) {
			vec3_t end;
			const GLfloat *vertex = &bsp->verts[(surf->index + j) * 3];
			const GLfloat *normal = &bsp->normals[(surf->index + j) * 3];

			VectorMA(vertex, 12.0, normal, end);

			memcpy(&r_state.vertex_array_3d[k], vertex, sizeof(vec3_t));
			memcpy(&r_state.vertex_array_3d[k + 3], end, sizeof(vec3_t));
			k += sizeof(vec3_t) / sizeof(vec_t) * 2;
			R_ReallocateStateArrays(k);
		}
	}

	glDrawArrays(GL_LINES, 0, k / 3);

	R_EnableTexture(&texunit_diffuse, qtrue);

	R_Color(NULL);
}

/**
 * @brief Recurse down the bsp tree and mark surfaces that are visible (not culled and in front)
 * for being rendered
 * @sa R_DrawWorld
 * @sa R_RecurseWorld
 * @param[in] node The bsp node to check
 * @param[in] tile The maptile (map assembly)
 */
static void R_RecursiveWorldNode (const mBspNode_t * node, int tile)
{
	int i, side, sidebit;
	mBspSurface_t *surf;
	float dot;

	if (node->contents == CONTENTS_SOLID)
		return;					/* solid */

	if (R_CullBox(node->minmaxs, node->minmaxs + 3))
		return;					/* culled out */

	/* if a leaf node, draw stuff */
	if (node->contents > CONTENTS_NODE)
		return;

	/* pathfinding nodes are invalid here */
	assert(node->plane);

	/* node is just a decision point, so go down the appropriate sides
	 * find which side of the node we are on */
	if (r_isometric->integer) {
		dot = -DotProduct(r_locals.forward, node->plane->normal);
	} else if (!AXIAL(node->plane)) {
		dot = DotProduct(refdef.viewOrigin, node->plane->normal) - node->plane->dist;
	} else {
		dot = refdef.viewOrigin[node->plane->type] - node->plane->dist;
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

	surf = r_mapTiles[tile]->bsp.surfaces + node->firstsurface;
	for (i = 0; i < node->numsurfaces; i++, surf++) {
		/* visible (front) side */
		if ((surf->flags & MSURF_PLANEBACK) == sidebit)
			surf->frame = r_locals.frame;
	}

	/* recurse down the back side */
	R_RecursiveWorldNode(node->children[!side], tile);
}

/**
 * @brief Wrapper that recurses the bsp nodes but skip the pathfinding nodes
 * @sa R_GetLevelSurfaceLists
 * @param[in] node The bsp node to check
 * @param[in] tile The maptile (map assembly)
 * @sa R_ModLoadNodes about pathfinding nodes
 */
static void R_RecurseWorld (const mBspNode_t * node, int tile)
{
	/* skip special pathfinding nodes */
	if (node->contents == CONTENTS_PATHFINDING_NODE) {
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

	r_locals.frame++;

	/* avoid overflows, negatives are reserved */
	if (r_locals.frame > 0xffff)
		r_locals.frame = 0;

	if (!r_drawworld->integer)
		return;

	mask = 1 << refdef.worldlevel;

	for (tile = 0; tile < r_numMapTiles; tile++) {
		/* don't draw weaponclip, actorclip and stepon */
		for (i = 0; i <= LEVEL_LASTVISIBLE; i++) {
			mBspHeader_t *header;
			mBspModel_t *bspModel;

			/* check the worldlevel flags */
			if (i && !(i & mask))
				continue;

			bspModel = &r_mapTiles[tile]->bsp;
			header = &bspModel->submodels[i];
			if (!header->numfaces)
				continue;

			R_RecurseWorld(bspModel->nodes + header->headnode, tile);
		}
	}
}
