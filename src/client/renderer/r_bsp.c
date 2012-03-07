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

#define MAX_BSPS_TO_RENDER 1024

typedef struct bspRenderRef_s {
	const mBspModel_t *bsp;
	vec3_t origin;
	vec3_t angles;
} bspRenderRef_t;

static bspRenderRef_t bspRRefs[MAX_BSPS_TO_RENDER];
static int numBspRRefs;

/**
 * @brief Returns whether if the specified bounding box is completely culled by the
 * view frustum (PSIDE_BACK), completely not culled (PSIDE_FRONT) or split by it (PSIDE_BOTH)
 * @param[in] mins The mins of the bounding box
 * @param[in] maxs The maxs of the bounding box
 */
static int R_CullBox (const vec3_t mins, const vec3_t maxs)
{
	int i;
	int cullState = 0;

	if (r_nocull->integer)
		return PSIDE_FRONT;

	for (i = lengthof(r_locals.frustum) - 1; i >= 0; i--) {
		int planeSide = TR_BoxOnPlaneSide(mins, maxs, &r_locals.frustum[i]);
		if (planeSide == PSIDE_BACK)
			return PSIDE_BACK; /* completely culled away */
		cullState |= planeSide;
	}

	return cullState;
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

	return R_CullBox(mins, maxs) == PSIDE_BACK;
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

			refdef.batchCount++;
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

	refdef.batchCount++;

	R_EnableTexture(&texunit_diffuse, qtrue);

	R_Color(NULL);
}

/**
 * @brief Recurse down the bsp tree and mark all surfaces as visible (if in front)
 * for being rendered
 * @sa R_DrawWorld
 * @sa R_RecurseWorld
 * @sa R_RecursiveWorldNode
 * @param[in] node The bsp node to mark
 * @param[in] tile The maptile (map assembly)
 */
static void R_RecursiveVisibleWorldNode (const mBspNode_t * node, int tile)
{
	int i, sidebit;
	mBspSurface_t *surf;
	float dot;

	/* if a leaf node, nothing to mark */
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
		sidebit = 0;
	} else {
		sidebit = MSURF_PLANEBACK;
	}

	surf = r_mapTiles[tile]->bsp.surfaces + node->firstsurface;
	for (i = 0; i < node->numsurfaces; i++, surf++) {
		/* visible (front) side */
		if ((surf->flags & MSURF_PLANEBACK) == sidebit)
			surf->frame = r_locals.frame;
	}

	/* recurse down the children */
	R_RecursiveVisibleWorldNode(node->children[0], tile);
	R_RecursiveVisibleWorldNode(node->children[1], tile);
}

/**
 * @brief Recurse down the bsp tree and mark surfaces that are visible (not culled and in front)
 * for being rendered
 * @sa R_DrawWorld
 * @sa R_RecurseWorld
 * @sa R_RecursiveVisibleWorldNode
 * @param[in] node The bsp node to check
 * @param[in] tile The maptile (map assembly)
 */
static void R_RecursiveWorldNode (const mBspNode_t * node, int tile)
{
	int i, sidebit;
	int cullState;
	mBspSurface_t *surf;
	float dot;

	/* if a leaf node, nothing to mark */
	if (node->contents > CONTENTS_NODE)
		return;

	cullState = R_CullBox(node->minmaxs, node->minmaxs + 3);

	if (cullState == PSIDE_BACK)
		return;					/* culled out */

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
		sidebit = 0;
	} else {
		sidebit = MSURF_PLANEBACK;
	}

	surf = r_mapTiles[tile]->bsp.surfaces + node->firstsurface;
	for (i = 0; i < node->numsurfaces; i++, surf++) {
		/* visible (front) side */
		if ((surf->flags & MSURF_PLANEBACK) == sidebit)
			surf->frame = r_locals.frame;
	}

	/* recurse down the children */
	if (cullState == PSIDE_FRONT) {
		/* completely inside the frustum - no need to do any further checks */
		R_RecursiveVisibleWorldNode(node->children[0], tile);
		R_RecursiveVisibleWorldNode(node->children[1], tile);
	} else {
		/* partially clipped by frustum - recurse to do finer checks */
		R_RecursiveWorldNode(node->children[0], tile);
		R_RecursiveWorldNode(node->children[1], tile);
	}
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

/*
=============================================================
Deferred rendering
=============================================================
*/

void R_ClearBspRRefs (void)
{
	numBspRRefs = 0;
}

/**
 * @brief Adds bsp render references
 * @note If forceVisibility is set, will mark the surfaces of the given bsp model as visible for this frame.
 * @param[in] model The bsp model to add to the render chain
 * @param[in] origin
 * @param[in] angles
 * @param[in] forceVisibility force model to be fully visible
 */
void R_AddBspRRef (const mBspModel_t *model, const vec3_t origin, const vec3_t angles, const qboolean forceVisibility)
{
	bspRenderRef_t *bspRR;

	if (numBspRRefs >= MAX_BSPS_TO_RENDER) {
		Com_Printf("Cannot add BSP model rendering reference: MAX_BSPS_TO_RENDER exceeded\n");
		return;
	}

	if (!model) {
		Com_Printf("R_AddBspRRef: null model!\n");
		return;
	}

	bspRR = &bspRRefs[numBspRRefs++];

	bspRR->bsp = model;
	VectorCopy(origin, bspRR->origin);
	VectorCopy(angles, bspRR->angles);

	if (forceVisibility) {
		int i;
		mBspSurface_t *surf = &model->surfaces[model->firstmodelsurface];

		for (i = 0; i < model->nummodelsurfaces; i++, surf++) {
			/* visible flag for rendering */
			surf->frame = r_locals.frame;
		}
	}
}

typedef void (*drawSurfaceFunc)(const mBspSurfaces_t *surfs);

/**
 * @param[in] drawFunc The function pointer to the surface draw function
 */
static void R_RenderBspRRefs (drawSurfaceFunc drawFunc, surfaceArrayType_t surfType)
{
	int i;
	for (i = 0; i < numBspRRefs; i++) {
		const bspRenderRef_t const *bspRR = &bspRRefs[i];
		const mBspModel_t const *bsp = bspRR->bsp;

		if (!bsp->sorted_surfaces[surfType]->count)
			continue;

		/* This is required to find the tile (world) bsp model to which arrays belong (submodels do not own arrays, but use world model ones) */
		R_SetArrayState(&r_mapTiles[bsp->maptile]->bsp);

		glPushMatrix();

		glTranslatef(bspRR->origin[0], bspRR->origin[1], bspRR->origin[2]);
		glRotatef(bspRR->angles[YAW], 0, 0, 1);
		glRotatef(bspRR->angles[PITCH], 0, 1, 0);
		glRotatef(bspRR->angles[ROLL], 1, 0, 0);

		drawFunc(bsp->sorted_surfaces[surfType]);

		/** @todo make it work again */
#if 0
		/* show model bounding box */
		if (r_showbox->integer) {
			const model_t *model = bspRR->bsp;
			R_DrawBoundingBox(model->mins, model->maxs);
		}
#endif

		glPopMatrix();
	}

	/* and restore array pointers */
	R_ResetArrayState();
}

/**
 * @brief Draw all simple opaque bsp surfaces with multitexture enabled and light enabled
 */
void R_RenderOpaqueBspRRefs (void)
{
	R_EnableTexture(&texunit_lightmap, qtrue);
	R_EnableLighting(r_state.world_program, qtrue);
	R_EnableWorldLights();

	R_RenderBspRRefs(R_DrawSurfaces, S_OPAQUE);

	R_EnableLighting(NULL, qfalse);
	R_EnableGlowMap(NULL);
	R_EnableTexture(&texunit_lightmap, qfalse);
}

/**
 * @brief Draw all warped opaque bsp surfaces via warp shader
 */
void R_RenderOpaqueWarpBspRRefs (void)
{
	R_EnableWarp(r_state.warp_program, qtrue);

	R_RenderBspRRefs(R_DrawSurfaces, S_OPAQUE_WARP);

	R_EnableWarp(NULL, qfalse);
	R_EnableGlowMap(NULL);
}

void R_RenderAlphaTestBspRRefs (void)
{
	R_EnableAlphaTest(qtrue);
	R_EnableLighting(r_state.world_program, qtrue);
	R_EnableWorldLights();

	R_RenderBspRRefs(R_DrawSurfaces, S_ALPHA_TEST);

	R_EnableLighting(NULL, qfalse);
	R_EnableGlowMap(NULL);
	R_EnableAlphaTest(qfalse);
}

void R_RenderMaterialBspRRefs (void)
{
	R_RenderBspRRefs(R_DrawMaterialSurfaces, S_MATERIAL);
}

void R_RenderFlareBspRRefs (void)
{
	R_RenderBspRRefs(R_DrawFlareSurfaces, S_FLARE);
}

/**
 * @brief Draw all translucent bsp surfaces with multitexture enabled and blend enabled
 */
void R_RenderBlendBspRRefs (void)
{
	assert(r_state.blend_enabled);
	R_EnableTexture(&texunit_lightmap, qtrue);

	R_RenderBspRRefs(R_DrawSurfaces, S_BLEND);

	R_EnableTexture(&texunit_lightmap, qfalse);
}

/**
 * @brief Draw all warped translucent bsp surfaces via warp shader and with blend enabled
 */
void R_RenderBlendWarpBspRRefs (void)
{
	assert(r_state.blend_enabled);
	R_EnableWarp(r_state.warp_program, qtrue);

	R_RenderBspRRefs(R_DrawSurfaces, S_BLEND_WARP);

	R_EnableWarp(NULL, qfalse);
	R_EnableGlowMap(NULL);
}
